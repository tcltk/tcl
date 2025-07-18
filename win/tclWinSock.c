/*
 * tclWinSock.c --
 *
 *	This file contains Windows-specific socket related code.
 *
 * Copyright © 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -----------------------------------------------------------------------
 * The order and naming of functions in this file should minimize
 * the file diff to tclUnixSock.c.
 * -----------------------------------------------------------------------
 *
 * General information on how this module works.
 *
 * - Each Tcl-thread with its sockets maintains an internal window to receive
 *   socket messages from the OS.
 *
 * - To ensure that message reception is always running this window is
 *   actually owned and handled by an internal thread. This we call the
 *   co-thread of Tcl's thread.
 *
 * - The whole structure is set up by InitSockets() which is called for each
 *   Tcl thread. The implementation of the co-thread is in SocketThread(),
 *   and the messages are handled by SocketProc(). The connection between
 *   both is not directly visible, it is done through a Win32 window class.
 *   This class is initialized by InitSockets() as well, and used in the
 *   creation of the message receiver windows.
 *
 * - An important thing to note is that *both* thread and co-thread have
 *   access to the list of sockets maintained in the private TSD data of the
 *   thread. The co-thread was given access to it upon creation through the
 *   new thread's client-data.
 *
 *   Because of this dual access the TSD data contains an OS mutex, the
 *   "socketListLock", to mediate exclusion between thread and co-thread.
 *
 *   The co-thread's access is all in SocketProc(). The thread's access is
 *   through SocketEventProc() (1) and the functions called by it.
 *
 *   (Ad 1) This is the handler function for all queued socket events, which
 *          all the OS messages are translated to through the EventSource (2)
 *          driven by the OS messages.
 *
 *   (Ad 2) The main functions for this are SocketSetupProc() and
 *          SocketCheckProc().
 */

#include "tclWinInt.h"

#ifdef _MSC_VER
#   pragma comment (lib, "ws2_32")
#endif

/*
 * Helper macros to make parts of this file clearer. The macros do exactly
 * what they say on the tin. :-) They also only ever refer to their arguments
 * once, and so can be used without regard to side effects.
 */

#define SET_BITS(var, bits)	((var) |= (bits))
#define CLEAR_BITS(var, bits)	((var) &= ~(bits))
#define GOT_BITS(var, bits)	(((var) & (bits)) != 0)

/* "sock" + a pointer in hex + \0 */
#define SOCK_CHAN_LENGTH	(16 + TCL_INTEGER_SPACE)

/*
 * The following variable is used to tell whether this module has been
 * initialized.  If 1, initialization of sockets was successful, if -1 then
 * socket initialization failed (WSAStartup failed).
 */

static int initialized = 0;
static const WCHAR className[] = L"TclSocket";
TCL_DECLARE_MUTEX(socketMutex)

/*
 * The following defines declare the messages used on socket windows.
 */
enum TclSocketMessages {
    SOCKET_MESSAGE = WM_USER+1,	/* Sent by OS: something happened. */
    SOCKET_SELECT = WM_USER+2,	/* Adjust select mask. */
    SOCKET_TERMINATE = WM_USER+3/* Stop worker thread. */
};

/*
 * Operations used with a SOCKET_SELECT message.
 */
enum SocketSelectOperations {
    SELECT = TRUE,		/* Add socket to select. */
    UNSELECT = FALSE		/* Remove socket from select. */
};

/*
 * This is needed to comply with the strict aliasing rules of GCC, but it also
 * simplifies casting between the different sockaddr types.
 */

typedef union {
    struct sockaddr sa;
    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    struct sockaddr_storage sas;
} address;

#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL IN6_ADDR_EQUAL
#endif

/*
 * This structure describes per-instance state of a tcp-based channel.
 */

typedef struct TcpState TcpState;

typedef struct TcpFdList {
    TcpState *statePtr;
    SOCKET fd;
    struct TcpFdList *next;
} TcpFdList;

struct TcpState {
    Tcl_Channel channel;	/* Channel associated with this socket. */
    int flags;			/* Bit field comprised of the flags described
				 * below. */
    struct TcpFdList *sockets;	/* Windows SOCKET handle. */
    int watchEvents;		/* OR'ed combination of FD_READ, FD_WRITE,
				 * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				 * indicate which events are interesting. */
    volatile int readyEvents;	/* OR'ed combination of FD_READ, FD_WRITE,
				 * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				 * indicate which events have occurred.
				 * Set by notifier thread, access must be
				 * protected by semaphore */
    int selectEvents;		/* OR'ed combination of FD_READ, FD_WRITE,
				 * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				 * indicate which events are currently being
				 * selected. */
    volatile int acceptEventCount;
				/* Count of the current number of FD_ACCEPTs
				 * that have arrived and not yet processed.
				 * Set by notifier thread, access must be
				 * protected by semaphore */
    Tcl_TcpAcceptProc *acceptProc;
				/* Proc to call on accept. */
    void *acceptProcData;	/* The data for the accept proc. */

    /*
     * Only needed for client sockets
     */

    struct addrinfo *addrlist;	/* Addresses to connect to. */
    struct addrinfo *addr;	/* Iterator over addrlist. */
    struct addrinfo *myaddrlist;/* Local address. */
    struct addrinfo *myaddr;	/* Iterator over myaddrlist. */
    int connectError;		/* Cache status of async socket. */
    int cachedBlocking;		/* Cache blocking mode of async socket. */
    volatile int notifierConnectError;
				/* Async connect error set by notifier thread.
				 * This error is still a windows error code.
				 * Access must be protected by semaphore */
    struct TcpState *nextPtr;	/* The next socket on the per-thread socket
				 * list. */
};

/*
 * These bits may be OR'ed together into the "flags" field of a TcpState
 * structure.
 */

enum TcpStateFlags {
    TCP_NONBLOCKING = 1<<0,	/* Socket with non-blocking I/O. */
    TCP_ASYNC_CONNECT = 1<<1,	/* Async connect in progress. */
    SOCKET_EOF = 1<<2,		/* A zero read happened on the socket. */
    SOCKET_PENDING = 1<<3,	/* A message has been sent for this socket */
    TCP_ASYNC_PENDING = 1<<4,	/* TcpConnect was called to process an async
				 * connect. This flag indicates that reentry is
				 * still pending. */
    TCP_ASYNC_FAILED = 1<<5,	/* An async connect finally failed. */

    TCP_ASYNC_TEST_MODE = 1<<8	/* Async testing activated.  Do not
				 * automatically continue connection
				 * process */
};

/*
 * The following structure is what is added to the Tcl event queue when a
 * socket event occurs.
 */

typedef struct {
    Tcl_Event header;		/* Information that is standard for all
				 * events. */
    SOCKET socket;		/* Socket descriptor that is ready. Used to
				 * find the TcpState structure for the file
				 * (can't point directly to the TcpState
				 * structure because it could go away while
				 * the event is queued). */
} SocketEvent;

/*
 * This defines the minimum buffersize maintained by the kernel.
 */

#define TCP_BUFFER_SIZE 4096

/*
 * Per (main) thread data, holding list of things being waited upon and the
 * various handles to things doing the waiting/notification.
 */
typedef struct {
    HWND hwnd;			/* Handle to window for socket messages. */
    HANDLE socketThread;	/* Thread handling the window */
    Tcl_ThreadId threadId;	/* Parent thread. */
    HANDLE readyEvent;		/* Event indicating that a socket event is
				 * ready. Also used to indicate that the
				 * socketThread has been initialized and has
				 * started. */
    HANDLE socketListLock;	/* Win32 Event to lock the socketList */
    TcpState *pendingTcpState;	/* This socket is opened but not jet in the
				 * list. This value is also checked by
				 * the event structure. */
    TcpState *socketList;	/* Every open socket in this thread has an
				 * entry on this list. */
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;
static WNDCLASSW windowClass;

/*
 * Static routines for this file:
 */

static int		TcpConnect(Tcl_Interp *interp,
			    TcpState *state);
static void		InitSocketWindowClass(void);
static TcpState *	NewSocketInfo(SOCKET socket);
static void		SocketExitHandler(void *clientData);
static LRESULT CALLBACK	SocketProc(HWND hwnd, UINT message, WPARAM wParam,
			    LPARAM lParam);
static void		TcpAccept(TcpFdList *fds, SOCKET newSocket, address addr);
static int		WaitForConnect(TcpState *statePtr, int *errorCodePtr);
static int		WaitForSocketEvent(TcpState *statePtr, int events,
			    int *errorCodePtr);
static void		AddSocketInfoFd(TcpState *statePtr, SOCKET socket);
static int		FindFDInList(TcpState *statePtr, SOCKET socket);
static DWORD WINAPI	SocketThread(LPVOID arg);
static void		TcpThreadActionProc(void *instanceData,
			    int action);
static int		TcpCloseProc(void *, Tcl_Interp *);

static Tcl_EventCheckProc	SocketCheckProc;
static Tcl_EventProc		SocketEventProc;
static Tcl_EventSetupProc	SocketSetupProc;
static Tcl_DriverBlockModeProc	TcpBlockModeProc;
static Tcl_DriverClose2Proc	TcpClose2Proc;
static Tcl_DriverSetOptionProc	TcpSetOptionProc;
static Tcl_DriverGetOptionProc	TcpGetOptionProc;
static Tcl_DriverInputProc	TcpInputProc;
static Tcl_DriverOutputProc	TcpOutputProc;
static Tcl_DriverWatchProc	TcpWatchProc;
static Tcl_DriverGetHandleProc	TcpGetHandleProc;

/*
 * This structure describes the channel type structure for TCP socket
 * based IO:
 */

static const Tcl_ChannelType tcpChannelType = {
    "tcp",
    TCL_CHANNEL_VERSION_5,
    NULL,			/* Deprecated. */
    TcpInputProc,
    TcpOutputProc,
    NULL,			/* Deprecated. */
    TcpSetOptionProc,
    TcpGetOptionProc,
    TcpWatchProc,
    TcpGetHandleProc,
    TcpClose2Proc,
    TcpBlockModeProc,
    NULL,			/* Flush proc. */
    NULL,			/* Bubbled event handler proc. */
    NULL,			/* Seek proc. */
    TcpThreadActionProc,
    NULL			/* Truncate proc. */
};

/*
 * The following variable holds the network name of this host.
 */

static TclInitProcessGlobalValueProc InitializeHostName;
static ProcessGlobalValue hostName =
	{0, 0, NULL, NULL, InitializeHostName, NULL, NULL};

/*
 *----------------------------------------------------------------------
 *
 * SendSelectMessage --
 *
 *	Simple wrapper round the SendMessage syscall with a SOCKET_SELECT
 *	message to add a bit of type safety.
 *
 *----------------------------------------------------------------------
 */
static inline void
SendSelectMessage(
    ThreadSpecificData *tsdPtr,	/* Reference to this thread's worker. */
    int operation,		/* Whether to add or remove from the mask. */
    TcpState *payload)		/* What socket to add/remove. */
{
    SendMessageW(tsdPtr->hwnd, SOCKET_SELECT, (WPARAM) operation,
	    (LPARAM) payload);
}

/*
 *----------------------------------------------------------------------
 *
 * InitializeHostName --
 *
 *	This routine sets the process global value of the name of the local
 *	host on which the process is running.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
InitializeHostName(
    char **valuePtr,
    size_t *lengthPtr,
    Tcl_Encoding *encodingPtr)
{
    WCHAR wbuf[256];
    DWORD length = sizeof(wbuf) / sizeof(WCHAR);
    Tcl_DString ds;

    Tcl_DStringInit(&ds);
    if (GetComputerNameExW(ComputerNamePhysicalDnsFullyQualified, wbuf, &length) != 0) {
	/*
	 * Convert string from WCHAR to utf-8, then change to lowercase,
	 * then to system encoding.
	 */
	Tcl_DString inDs;

	Tcl_DStringInit(&inDs);
	Tcl_UtfToLower(Tcl_WCharToUtfDString(wbuf, TCL_INDEX_NONE, &inDs));
	Tcl_UtfToExternalDStringEx(NULL, NULL, Tcl_DStringValue(&inDs),
		TCL_INDEX_NONE, TCL_ENCODING_PROFILE_TCL8, &ds, NULL);
	Tcl_DStringFree(&inDs);
    } else {
	TclInitSockets();
	/*
	 * The buffer size of 256 is recommended by the MSDN page that
	 * documents gethostname() as being always adequate.
	 */

	Tcl_DStringInit(&ds);
	Tcl_DStringSetLength(&ds, 256);
	gethostname(Tcl_DStringValue(&ds), (int)Tcl_DStringLength(&ds));
	Tcl_DStringSetLength(&ds, strlen(Tcl_DStringValue(&ds)));
    }

    *encodingPtr = Tcl_GetEncoding(NULL, NULL);
    *lengthPtr = Tcl_DStringLength(&ds);
    *valuePtr = (char *)Tcl_Alloc(*lengthPtr + 1);
    memcpy(*valuePtr, Tcl_DStringValue(&ds), *lengthPtr + 1);
    Tcl_DStringFree(&ds);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Returns the name of the local host.
 *
 * Results:
 *	A string containing the network name for this machine, or an empty
 *	string if we can't figure out the name. The caller must not modify or
 *	free this string.
 *
 * Side effects:
 *	Caches the name to return for future calls.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_GetHostName(void)
{
    return Tcl_GetString(TclGetProcessGlobalValue(&hostName));
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitSockets --
 *
 *	Initialization of sockets for the thread. Also creates message
 *	handling window class for the process if needed.
 *
 * Results:
 *	Nothing. Panics on failure.
 *
 * Side effects:
 *	If not already prepared, initializes the TSD structure and socket
 *	message handling thread associated to the calling thread for the
 *	subsystem of the driver.
 *
 *----------------------------------------------------------------------
 */

void
TclInitSockets(void)
{
    /* Then Per thread initialization. */
    DWORD id;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    if (tsdPtr != NULL) {
	return;
    }

    InitSocketWindowClass();

    /*
     * OK, this thread has never done anything with sockets before.  Construct
     * a worker thread to handle asynchronous events related to sockets
     * assigned to _this_ thread.
     */

    tsdPtr = TCL_TSD_INIT(&dataKey);
    tsdPtr->pendingTcpState = NULL;
    tsdPtr->socketList	= NULL;
    tsdPtr->hwnd	= NULL;
    tsdPtr->threadId	= Tcl_GetCurrentThread();
    tsdPtr->readyEvent	= CreateEventW(NULL, FALSE, FALSE, NULL);
    if (tsdPtr->readyEvent == NULL) {
	goto initFailure;
    }
    tsdPtr->socketListLock = CreateEventW(NULL, FALSE, TRUE, NULL);
    if (tsdPtr->socketListLock == NULL) {
	goto initFailure;
    }
    tsdPtr->socketThread = CreateThread(NULL, 256, SocketThread, tsdPtr, 0,
	    &id);
    if (tsdPtr->socketThread == NULL) {
	goto initFailure;
    }

    SetThreadPriority(tsdPtr->socketThread, THREAD_PRIORITY_HIGHEST);

    /*
     * Wait for the thread to signal when the window has been created and if
     * it is ready to go.
     */

    WaitForSingleObject(tsdPtr->readyEvent, INFINITE);

    if (tsdPtr->hwnd != NULL) {
	Tcl_CreateEventSource(SocketSetupProc, SocketCheckProc, NULL);
	return;
    }

  initFailure:
    Tcl_Panic("InitSockets failed");
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeSockets --
 *
 *	This function is called from Tcl_FinalizeThread to finalize the
 *	platform specific socket subsystem. Also, it may be called from within
 *	this module to cleanup the state if unable to initialize the sockets
 *	subsystem.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the event source and destroys the socket thread.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeSockets(void)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    /*
     * Careful! This is a finalizer!
     */

    if (tsdPtr == NULL) {
	return;
    }

    if (tsdPtr->socketThread != NULL) {
	if (tsdPtr->hwnd != NULL) {
	    PostMessageW(tsdPtr->hwnd, SOCKET_TERMINATE, 0, 0);

	    /*
	     * Wait for the thread to exit. This ensures that we are
	     * completely cleaned up before we leave this function.
	     */

	    WaitForSingleObject(tsdPtr->socketThread, INFINITE);
	    tsdPtr->hwnd = NULL;
	}
	CloseHandle(tsdPtr->socketThread);
	tsdPtr->socketThread = NULL;
    }
    if (tsdPtr->readyEvent != NULL) {
	CloseHandle(tsdPtr->readyEvent);
	tsdPtr->readyEvent = NULL;
    }
    if (tsdPtr->socketListLock != NULL) {
	CloseHandle(tsdPtr->socketListLock);
	tsdPtr->socketListLock = NULL;
    }
    Tcl_DeleteEventSource(SocketSetupProc, SocketCheckProc, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TcpBlockModeProc --
 *
 *	This function is invoked by the generic IO level to set blocking and
 *	nonblocking mode on a TCP socket based channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or nonblocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
TcpBlockModeProc(
    void *instanceData,		/* Socket state. */
    int mode)			/* The mode to set. Can be one of
				 * TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    TcpState *statePtr = (TcpState *)instanceData;

    if (mode == TCL_MODE_NONBLOCKING) {
	SET_BITS(statePtr->flags, TCP_NONBLOCKING);
    } else {
	CLEAR_BITS(statePtr->flags, TCP_NONBLOCKING);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForConnect --
 *
 *	Check the state of an async connect process. If a connection attempt
 *	terminated, process it, which may finalize it or may start the next
 *	attempt. If a connect error occures, it is saved in
 *	statePtr->connectError to be reported by 'fconfigure -error'.
 *
 *	There are two modes of operation, defined by errorCodePtr:
 *	 *  non-NULL: Called by explicite read/write command. Block if socket
 *	    is blocking.
 *	    May return two error codes:
 *	     *	EWOULDBLOCK: if connect is still in progress
 *	     *	ENOTCONN: if connect failed. This would be the error message
 *		of a recv or sendto syscall so this is emulated here.
 *	 *  Null: Called by a background operation. Do not block and don't
 *	    return any error code.
 *
 * Results:
 *	0 if the connection has completed, -1 if still in progress or there is
 *	an error.
 *
 * Side effects:
 *	Processes socket events off the system queue. May process
 *	asynchronous connect.
 *
 *----------------------------------------------------------------------
 */

static int
WaitForConnect(
    TcpState *statePtr,		/* State of the socket. */
    int *errorCodePtr)		/* Where to store errors? A passed
				 * null-pointer activates background mode. */
{
    int result;
    int oldMode;

    /*
     * Check if an async connect failed already and error reporting is
     * demanded, return the error ENOTCONN.
     */

    if (errorCodePtr != NULL && GOT_BITS(statePtr->flags, TCP_ASYNC_FAILED)) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    /*
     * Check if an async connect is running. If not return ok
     */

    if (!GOT_BITS(statePtr->flags, TCP_ASYNC_CONNECT)) {
	return 0;
    }

    /*
     * In socket test mode do not continue with the connect
     * Exceptions are:
     * - Call by recv/send and blocking socket
     *   (errorCodePtr != NULL && !GOT_BITS(flags, TCP_NONBLOCKING))
     * - Call by the event queue (errorCodePtr == NULL)
     */

    if (GOT_BITS(statePtr->flags, TCP_ASYNC_TEST_MODE)
	    && errorCodePtr != NULL
	    && GOT_BITS(statePtr->flags, TCP_NONBLOCKING)) {
	*errorCodePtr = EWOULDBLOCK;
	return -1;
    }

    /*
     * Be sure to disable event servicing so we are truly modal.
     */

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_NONE);

    /*
     * Loop in the blocking case until the connect signal is present
     */

    while (1) {
	/*
	 * Get the statePtr lock.
	 */

	ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
		TclThreadDataKeyGet(&dataKey);
	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	/*
	 * Check for connect event.
	 */

	if (GOT_BITS(statePtr->readyEvents, FD_CONNECT)) {
	    /*
	     * Consume the connect event.
	     */

	    CLEAR_BITS(statePtr->readyEvents, FD_CONNECT);

	    /*
	     * For blocking sockets and foreground processing, disable async
	     * connect as we continue now synchronously.
	     */

	    if (errorCodePtr != NULL &&
		    !GOT_BITS(statePtr->flags, TCP_NONBLOCKING)) {
		CLEAR_BITS(statePtr->flags, TCP_ASYNC_CONNECT);
	    }

	    /*
	     * Free list lock.
	     */

	    SetEvent(tsdPtr->socketListLock);

	    /*
	     * Continue connect. If switched to synchronous connect, the
	     * connect is terminated.
	     */

	    result = TcpConnect(NULL, statePtr);

	    /*
	     * Restore event service mode.
	     */

	    (void) Tcl_SetServiceMode(oldMode);

	    /*
	     * Check for Successful connect or async connect restart
	     */

	    if (result == TCL_OK) {
		/*
		 * Check for async connect restart (not possible for
		 * foreground blocking operation)
		 */

		if (GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)) {
		    if (errorCodePtr != NULL) {
			*errorCodePtr = EWOULDBLOCK;
		    }
		    return -1;
		}
		return 0;
	    }

	    /*
	     * Connect finally failed. For foreground operation return
	     * ENOTCONN.
	     */

	    if (errorCodePtr != NULL) {
		*errorCodePtr = ENOTCONN;
	    }
	    return -1;
	}

	/*
	 * Free list lock.
	 */

	SetEvent(tsdPtr->socketListLock);

	/*
	 * Background operation returns with no action as there was no connect
	 * event
	 */

	if (errorCodePtr == NULL) {
	    return -1;
	}

	/*
	 * A non blocking socket waiting for an asynchronous connect
	 * returns directly the error EWOULDBLOCK
	 */

	if (GOT_BITS(statePtr->flags, TCP_NONBLOCKING)) {
	    *errorCodePtr = EWOULDBLOCK;
	    return -1;
	}

	/*
	 * Wait until something happens.
	 */

	WaitForSingleObject(tsdPtr->readyEvent, INFINITE);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpInputProc --
 *
 *	This function is invoked by the generic IO level to read input from a
 *	TCP socket based channel.
 *
 * Results:
 *	The number of bytes read is returned or -1 on error. An output
 *	argument contains the POSIX error code on error, or zero if no error
 *	occurred.
 *
 * Side effects:
 *	Reads input from the input device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
TcpInputProc(
    void *instanceData,		/* Socket state. */
    char *buf,			/* Where to store data read. */
    int bufSize,		/* How much space is available in the
				 * buffer? */
    int *errorCodePtr)		/* Where to store error code. */
{
    TcpState *statePtr = (TcpState *)instanceData;
    int bytesRead;
    DWORD error;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    *errorCodePtr = 0;

    /*
     * First check to see if EOF was already detected, to prevent calling the
     * socket stack after the first time EOF is detected.
     */

    if (GOT_BITS(statePtr->flags, SOCKET_EOF)) {
	return 0;
    }

    /*
     * Check if there is an async connect running.
     * For blocking sockets terminate connect, otherwise do one step.
     * For a non blocking socket return EWOULDBLOCK if connect not terminated
     */

    if (WaitForConnect(statePtr, errorCodePtr) != 0) {
	return -1;
    }

    /*
     * No EOF, and it is connected, so try to read more from the socket. Note
     * that we clear the FD_READ bit because read events are level triggered
     * so a new event will be generated if there is still data available to be
     * read. We have to simulate blocking behavior here since we are always
     * using non-blocking sockets.
     */

    while (1) {
	SendSelectMessage(tsdPtr, UNSELECT, statePtr);

	/*
	 * Single fd operation: this proc is only called for a connected
	 * socket.
	 */

	bytesRead = recv(statePtr->sockets->fd, buf, bufSize, 0);
	CLEAR_BITS(statePtr->readyEvents, FD_READ);

	/*
	 * Check for end-of-file condition or successful read.
	 */

	if (bytesRead == 0) {
	    SET_BITS(statePtr->flags, SOCKET_EOF);
	}
	if (bytesRead != SOCKET_ERROR) {
	    break;
	}

	/*
	 * If an error occurs after the FD_CLOSE has arrived, then ignore the
	 * error and report an EOF.
	 */

	if (GOT_BITS(statePtr->readyEvents, FD_CLOSE)) {
	    SET_BITS(statePtr->flags, SOCKET_EOF);
	    bytesRead = 0;
	    break;
	}

	error = WSAGetLastError();

	/*
	 * If an RST comes, then ignore the error and report an EOF just like
	 * on Unix.
	 */

	if (error == WSAECONNRESET) {
	    SET_BITS(statePtr->flags, SOCKET_EOF);
	    bytesRead = 0;
	    break;
	}

	/*
	 * Check for error condition or underflow in non-blocking case.
	 */

	if (GOT_BITS(statePtr->flags, TCP_NONBLOCKING)
		|| (error != WSAEWOULDBLOCK)) {
	    Tcl_WinConvertError(error);
	    *errorCodePtr = Tcl_GetErrno();
	    bytesRead = -1;
	    break;
	}

	/*
	 * In the blocking case, wait until the file becomes readable or
	 * closed and try again.
	 */

	if (!WaitForSocketEvent(statePtr, FD_READ|FD_CLOSE, errorCodePtr)) {
	    bytesRead = -1;
	    break;
	}
    }

    SendSelectMessage(tsdPtr, SELECT, statePtr);

    return bytesRead;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpOutputProc --
 *
 *	This function is called by the generic IO level to write data to a
 *	socket based channel.
 *
 * Results:
 *	The number of bytes written or -1 on failure.
 *
 * Side effects:
 *	Produces output on the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpOutputProc(
    void *instanceData,		/* Socket state. */
    const char *buf,		/* The data buffer. */
    int toWrite,		/* How many bytes to write? */
    int *errorCodePtr)		/* Where to store error code. */
{
    TcpState *statePtr = (TcpState *)instanceData;
    int written;
    DWORD error;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    *errorCodePtr = 0;

    /*
     * Check if there is an async connect running.
     * For blocking sockets terminate connect, otherwise do one step.
     * For a non blocking socket return EWOULDBLOCK if connect not terminated
     */

    if (WaitForConnect(statePtr, errorCodePtr) != 0) {
	return -1;
    }

    while (1) {
	SendSelectMessage(tsdPtr, UNSELECT, statePtr);

	/*
	 * Single fd operation: this proc is only called for a connected
	 * socket.
	 */

	written = send(statePtr->sockets->fd, buf, toWrite, 0);
	if (written != SOCKET_ERROR) {
	    /*
	     * Since Windows won't generate a new write event until we hit an
	     * overflow condition, we need to force the event loop to poll
	     * until the condition changes.
	     */

	    if (GOT_BITS(statePtr->watchEvents, FD_WRITE)) {
		Tcl_Time blockTime = { 0, 0 };

		Tcl_SetMaxBlockTime(&blockTime);
	    }
	    break;
	}

	/*
	 * Check for error condition or overflow. In the event of overflow, we
	 * need to clear the FD_WRITE flag so we can detect the next writable
	 * event. Note that Windows only sends a new writable event after a
	 * send fails with WSAEWOULDBLOCK.
	 */

	error = WSAGetLastError();
	if (error == WSAEWOULDBLOCK) {
	    CLEAR_BITS(statePtr->readyEvents, FD_WRITE);
	    if (GOT_BITS(statePtr->flags, TCP_NONBLOCKING)) {
		*errorCodePtr = EWOULDBLOCK;
		written = -1;
		break;
	    }
	} else {
	    Tcl_WinConvertError(error);
	    *errorCodePtr = Tcl_GetErrno();
	    written = -1;
	    break;
	}

	/*
	 * In the blocking case, wait until the file becomes writable or
	 * closed and try again.
	 */

	if (!WaitForSocketEvent(statePtr, FD_WRITE|FD_CLOSE, errorCodePtr)) {
	    written = -1;
	    break;
	}
    }

    SendSelectMessage(tsdPtr, SELECT, statePtr);

    return written;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpCloseProc --
 *
 *	This function is called by the generic IO level to perform channel
 *	type specific cleanup on a socket based channel when the channel is
 *	closed.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpCloseProc(
    void *instanceData,		/* The socket to close. */
    TCL_UNUSED(Tcl_Interp *))
{
    TcpState *statePtr = (TcpState *)instanceData;
    /* TIP #218 */
    int errorCode = 0;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /*
     * Clean up the OS socket handle. The default Windows setting for a
     * socket is SO_DONTLINGER, which does a graceful shutdown in the
     * background.
     */

    while (statePtr->sockets != NULL) {
	TcpFdList *thisfd = statePtr->sockets;

	statePtr->sockets = thisfd->next;
	if (closesocket(thisfd->fd) == SOCKET_ERROR) {
	    Tcl_WinConvertError((DWORD) WSAGetLastError());
	    errorCode = Tcl_GetErrno();
	}
	Tcl_Free(thisfd);
    }

    if (statePtr->addrlist != NULL) {
	freeaddrinfo(statePtr->addrlist);
    }
    if (statePtr->myaddrlist != NULL) {
	freeaddrinfo(statePtr->myaddrlist);
    }

    /*
     * Clear an eventual tsd info list pointer.
     *
     * This may be called, if an async socket connect fails or is closed
     * between connect and thread action callback.
     */

    if (tsdPtr->pendingTcpState != NULL
	    && tsdPtr->pendingTcpState == statePtr) {
	/*
	 * Get infoPtr lock, because this concerns the notifier thread.
	 */

	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	tsdPtr->pendingTcpState = NULL;

	/*
	 * Free list lock.
	 */

	SetEvent(tsdPtr->socketListLock);
    }

    /*
     * TIP #218. Removed the code removing the structure from the global
     * socket list. This is now done by the thread action callbacks, and only
     * there. This happens before this code is called. We can free without
     * fear of damaging the list.
     */

    Tcl_Free(statePtr);
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpClose2Proc --
 *
 *	This function is called by the generic IO level to perform the channel
 *	type specific part of a half-close: namely, a shutdown() on a socket.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Shuts down one side of the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpClose2Proc(
    void *instanceData,		/* The socket to close. */
    Tcl_Interp *interp,		/* For error reporting. */
    int flags)			/* Flags that indicate which side to close. */
{
    TcpState *statePtr = (TcpState *)instanceData;
    int readError = 0;
    int writeError = 0;

    /*
     * Shutdown the OS socket handle.
     */

    if ((flags & (TCL_CLOSE_READ | TCL_CLOSE_WRITE)) == 0) {
	return TcpCloseProc(instanceData, interp);
    }

    /*
     * Single fd operation: Tcl_OpenTcpServer() does not set TCL_READABLE or
     * TCL_WRITABLE so this should never be called for a server socket.
     */

    if ((flags & TCL_CLOSE_READ)
	    && (shutdown(statePtr->sockets->fd, SD_RECEIVE) == SOCKET_ERROR)) {
	Tcl_WinConvertError((DWORD) WSAGetLastError());
	readError = Tcl_GetErrno();
    }
    if ((flags & TCL_CLOSE_WRITE)
	    && (shutdown(statePtr->sockets->fd, SD_SEND) == SOCKET_ERROR)) {
	Tcl_WinConvertError((DWORD) WSAGetLastError());
	writeError = Tcl_GetErrno();
    }
    return (readError != 0) ? readError : writeError;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpSetOptionProc --
 *
 *	Sets Tcp channel specific options.
 *
 * Results:
 *	None, unless an error happens.
 *
 * Side effects:
 *	Changes attributes of the socket at the system level.
 *
 *----------------------------------------------------------------------
 */

static int
TcpSetOptionProc(
    void *instanceData,		/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    const char *optionName,	/* Name of the option to set. */
    const char *value)		/* New value for option. */
{
    TcpState *statePtr = (TcpState *)instanceData;
    SOCKET sock;
    size_t len = 0;

    if (optionName != NULL) {
	len = strlen(optionName);
    }

    sock = statePtr->sockets->fd;

    if ((len > 1) && (optionName[1] == 'k') &&
	    (strncmp(optionName, "-keepalive", len) == 0)) {
	BOOL boolVar;
	int rtn;

	if (Tcl_GetBoolean(interp, value, &boolVar) != TCL_OK) {
	    return TCL_ERROR;
	}
	rtn = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
		(const char *) &boolVar, sizeof(boolVar));
	if (rtn != 0) {
	    Tcl_WinConvertError(WSAGetLastError());
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't set socket option: %s",
			Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    if ((len > 1) && (optionName[1] == 'n') &&
	    (strncmp(optionName, "-nodelay", len) == 0)) {
	BOOL boolVar;
	int rtn;

	if (Tcl_GetBoolean(interp, value, &boolVar) != TCL_OK) {
	    return TCL_ERROR;
	}
	rtn = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(const char *) &boolVar, sizeof(boolVar));
	if (rtn != 0) {
	    Tcl_WinConvertError(WSAGetLastError());
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"couldn't set socket option: %s",
			Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    return Tcl_BadChannelOption(interp, optionName, "keepalive nodelay");
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetOptionProc --
 *
 *	Computes an option value for a TCP socket based channel, or a list of
 *	all options and their values.
 *
 *	Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *	A standard Tcl result. The value of the specified option or a list of
 *	all options and their values is returned in the supplied DString. Sets
 *	Error message if needed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetOptionProc(
    void *instanceData,		/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    const char *optionName,	/* Name of the option to retrieve the value
				 * for, or NULL to get all options and their
				 * values. */
    Tcl_DString *dsPtr)		/* Where to store the computed value;
				 * initialized by caller. */
{
    TcpState *statePtr = (TcpState *)instanceData;
    char host[NI_MAXHOST], port[NI_MAXSERV];
    SOCKET sock;
    size_t len = 0;
    int reverseDNS = 0;
#define SUPPRESS_RDNS_VAR "::tcl::unsupported::noReverseDNS"
#define HAVE_OPTION(option) \
	((len > 1) && (optionName[1] == option[1]) && \
	    (strncmp(optionName, option, len) == 0))

    /*
     * Go one step in async connect
     *
     * If any error is thrown save it as background error to report eventually
     * below.
     */

    if (!GOT_BITS(statePtr->flags, TCP_ASYNC_TEST_MODE)) {
	WaitForConnect(statePtr, NULL);
    }

    sock = statePtr->sockets->fd;
    if (optionName != NULL) {
	len = strlen(optionName);
    }

    if (HAVE_OPTION("-error")) {
	/*
	 * Do not return any errors if async connect is running.
	 */

	if (!GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)) {
	    if (GOT_BITS(statePtr->flags, TCP_ASYNC_FAILED)) {
		/*
		 * In case of a failed async connect, eventually report the
		 * connect error only once.  Do not report the system error,
		 * as this comes again and again.
		 */

		if (statePtr->connectError != 0) {
		    Tcl_DStringAppend(dsPtr,
			    Tcl_ErrnoMsg(statePtr->connectError),
			    TCL_INDEX_NONE);
		    statePtr->connectError = 0;
		}
	    } else {
		/*
		 * Report an eventual last error of the socket system.
		 */

		int optlen;
		int ret;
		DWORD err;

		/*
		 * Populate the err variable with a POSIX error
		 */

		optlen = sizeof(int);
		ret = getsockopt(sock, SOL_SOCKET, SO_ERROR,
			(char *)&err, &optlen);

		/*
		 * The error was not returned directly but should be taken
		 * from WSA.
		 */

		if (ret == SOCKET_ERROR) {
		    err = WSAGetLastError();
		}

		/*
		 * Return error message.
		 */

		if (err) {
		    Tcl_WinConvertError(err);
		    Tcl_DStringAppend(dsPtr, Tcl_ErrnoMsg(Tcl_GetErrno()),
			    TCL_INDEX_NONE);
		}
	    }
	}
	return TCL_OK;
    }

    if (HAVE_OPTION("-connecting")) {
	Tcl_DStringAppend(dsPtr,
		GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)
		? "1" : "0", TCL_INDEX_NONE);
	return TCL_OK;
    }

    if (interp != NULL
	    && Tcl_GetVar(interp, SUPPRESS_RDNS_VAR, 0) != NULL) {
	reverseDNS = NI_NUMERICHOST;
    }

    if ((len == 0) || HAVE_OPTION("-peername")) {
	address peername;
	socklen_t size = sizeof(peername);

	if (GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)) {
	    /*
	     * In async connect output an empty string
	     */

	    if (len == 0) {
		Tcl_DStringAppendElement(dsPtr, "-peername");
		Tcl_DStringAppendElement(dsPtr, "");
	    } else {
		return TCL_OK;
	    }
	} else if (getpeername(sock, (LPSOCKADDR) &(peername.sa),
		&size) == 0) {
	    /*
	     * Peername fetch succeeded - output list
	     */

	    if (len == 0) {
		Tcl_DStringAppendElement(dsPtr, "-peername");
		Tcl_DStringStartSublist(dsPtr);
	    }

	    getnameinfo(&(peername.sa), size, host, sizeof(host),
		    NULL, 0, NI_NUMERICHOST);
	    Tcl_DStringAppendElement(dsPtr, host);
	    getnameinfo(&(peername.sa), size, host, sizeof(host),
		    port, sizeof(port), reverseDNS | NI_NUMERICSERV);
	    Tcl_DStringAppendElement(dsPtr, host);
	    Tcl_DStringAppendElement(dsPtr, port);
	    if (len == 0) {
		Tcl_DStringEndSublist(dsPtr);
	    } else {
		return TCL_OK;
	    }
	} else {
	    /*
	     * getpeername failed - but if we were asked for all the options
	     * (len==0), don't flag an error at that point because it could be
	     * an fconfigure request on a server socket (such sockets have no
	     * peer). {Copied from unix/tclUnixChan.c}
	     */

	    if (len) {
		Tcl_WinConvertError((DWORD) WSAGetLastError());
		if (interp) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "can't get peername: %s",
			    Tcl_PosixError(interp)));
		}
		return TCL_ERROR;
	    }
	}
    }

    if ((len == 0) || HAVE_OPTION("-sockname")) {
	TcpFdList *fds;
	address sockname;
	socklen_t size;
	int found = 0;

	if (len == 0) {
	    Tcl_DStringAppendElement(dsPtr, "-sockname");
	    Tcl_DStringStartSublist(dsPtr);
	}
	if (GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)) {
	    /*
	     * In async connect output an empty string
	     */

	    found = 1;
	} else {
	    for (fds = statePtr->sockets; fds != NULL; fds = fds->next) {
		sock = fds->fd;
		size = sizeof(sockname);
		if (getsockname(sock, &(sockname.sa), &size) >= 0) {
		    int flags = reverseDNS;

		    found = 1;
		    getnameinfo(&sockname.sa, size, host, sizeof(host),
			    NULL, 0, NI_NUMERICHOST);
		    Tcl_DStringAppendElement(dsPtr, host);

		    /*
		     * We don't want to resolve INADDR_ANY and sin6addr_any;
		     * they can sometimes cause problems (and never have a
		     * name).
		     */

		    flags |= NI_NUMERICSERV;
		    if (sockname.sa.sa_family == AF_INET) {
			if (sockname.sa4.sin_addr.s_addr == INADDR_ANY) {
			    flags |= NI_NUMERICHOST;
			}
		    } else if (sockname.sa.sa_family == AF_INET6) {
			if ((IN6_ARE_ADDR_EQUAL(&sockname.sa6.sin6_addr,
				    &in6addr_any)) ||
				(IN6_IS_ADDR_V4MAPPED(&sockname.sa6.sin6_addr)
				&& sockname.sa6.sin6_addr.s6_addr[12] == 0
				&& sockname.sa6.sin6_addr.s6_addr[13] == 0
				&& sockname.sa6.sin6_addr.s6_addr[14] == 0
				&& sockname.sa6.sin6_addr.s6_addr[15] == 0)) {
			    flags |= NI_NUMERICHOST;
			}
		    }
		    getnameinfo(&sockname.sa, size, host, sizeof(host),
			    port, sizeof(port), flags);
		    Tcl_DStringAppendElement(dsPtr, host);
		    Tcl_DStringAppendElement(dsPtr, port);
		}
	    }
	}
	if (found) {
	    if (len) {
		return TCL_OK;
	    }
	    Tcl_DStringEndSublist(dsPtr);
	} else {
	    if (interp) {
		Tcl_WinConvertError((DWORD) WSAGetLastError());
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"can't get sockname: %s", Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}
    }

    if ((len == 0) || HAVE_OPTION("-keepalive")) {
	int optlen;
	BOOL opt = FALSE;

	if (len == 0) {
	    sock = statePtr->sockets->fd;
	    Tcl_DStringAppendElement(dsPtr, "-keepalive");
	}
	optlen = sizeof(BOOL);
	getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt, &optlen);
	Tcl_DStringAppendElement(dsPtr, opt ? "1" : "0");
	if (len > 0) {
	    return TCL_OK;
	}
    }

    if ((len == 0) || HAVE_OPTION("-nodelay")) {
	int optlen;
	BOOL opt = FALSE;

	if (len == 0) {
	    sock = statePtr->sockets->fd;
	    Tcl_DStringAppendElement(dsPtr, "-nodelay");
	}
	optlen = sizeof(BOOL);
	getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, &optlen);
	Tcl_DStringAppendElement(dsPtr, opt ? "1" : "0");
	if (len > 0) {
	    return TCL_OK;
	}
    }

    if (len > 0) {
	return Tcl_BadChannelOption(interp, optionName,
		"connecting keepalive nodelay peername sockname");
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpWatchProc --
 *
 *	Informs the channel driver of the events that the generic channel code
 *	wishes to receive on this socket.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May cause the notifier to poll if any of the specified conditions are
 *	already true.
 *
 *----------------------------------------------------------------------
 */

static void
TcpWatchProc(
    void *instanceData,		/* The socket state. */
    int mask)			/* Events of interest; an OR-ed combination of
				 * TCL_READABLE, TCL_WRITABLE and
				 * TCL_EXCEPTION. */
{
    TcpState *statePtr = (TcpState *)instanceData;

    /*
     * Update the watch events mask. Only if the socket is not a server
     * socket. [Bug 557878]
     */

    if (!statePtr->acceptProc) {
	statePtr->watchEvents = 0;
	if (GOT_BITS(mask, TCL_READABLE)) {
	    SET_BITS(statePtr->watchEvents, FD_READ | FD_CLOSE);
	}
	if (GOT_BITS(mask, TCL_WRITABLE)) {
	    SET_BITS(statePtr->watchEvents, FD_WRITE | FD_CLOSE);
	}

	/*
	 * If there are any conditions already set, then tell the notifier to
	 * poll rather than block.
	 */

	if (statePtr->readyEvents & statePtr->watchEvents) {
	    Tcl_Time blockTime = { 0, 0 };

	    Tcl_SetMaxBlockTime(&blockTime);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from inside a
 *	TCP socket based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if there is no
 *	handle for the specified direction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetHandleProc(
    void *instanceData,		/* The socket state. */
    TCL_UNUSED(int) /*direction*/,
    void **handlePtr)		/* Where to store the handle. */
{
    TcpState *statePtr = (TcpState *)instanceData;

    *handlePtr = INT2PTR(statePtr->sockets->fd);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpConnect --
 *
 *	This function opens a new socket in client mode.
 *
 *	This might be called in 3 circumstances:
 *	-   By a regular socket command
 *	-   By the event handler to continue an asynchronously connect
 *	-   By a blocking socket function (gets/puts) to terminate the
 *	    connect synchronously
 *
 * Results:
 *	TCL_OK, if the socket was successfully connected or an asynchronous
 *	connection is in progress. If an error occurs, TCL_ERROR is returned
 *	and an error message is left in interp.
 *
 * Side effects:
 *	Opens a socket.
 *
 * Remarks:
 *	A single host name may resolve to more than one IP address, e.g. for
 *	an IPv4/IPv6 dual stack host. For handling asynchronously connecting
 *	sockets in the background for such hosts, this function can act as a
 *	coroutine. On the first call, it sets up the control variables for the
 *	two nested loops over the local and remote addresses. Once the first
 *	connection attempt is in progress, it sets up itself as a writable
 *	event handler for that socket, and returns. When the callback occurs,
 *	control is transferred to the "reenter" label, right after the initial
 *	return and the loops resume as if they had never been interrupted.
 *	For synchronously connecting sockets, the loops work the usual way.
 *
 *----------------------------------------------------------------------
 */

static int
TcpConnect(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    TcpState *statePtr)
{
    DWORD error;
    int async_connect = GOT_BITS(statePtr->flags, TCP_ASYNC_CONNECT);
				/* We are started with async connect and the
				 * connect notification was not yet
				 * received. */
    int async_callback = GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING);
				/* We were called by the event procedure and
				 * continue our loop. */
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    if (async_callback) {
	goto reenter;
    }

    for (statePtr->addr = statePtr->addrlist; statePtr->addr != NULL;
	    statePtr->addr = statePtr->addr->ai_next) {
	for (statePtr->myaddr = statePtr->myaddrlist;
		statePtr->myaddr != NULL;
		statePtr->myaddr = statePtr->myaddr->ai_next) {
	    /*
	     * No need to try combinations of local and remote addresses
	     * of different families.
	     */

	    if (statePtr->myaddr->ai_family != statePtr->addr->ai_family) {
		continue;
	    }

	    /*
	     * Close the socket if it is still open from the last unsuccessful
	     * iteration.
	     */

	    if (statePtr->sockets->fd != INVALID_SOCKET) {
		closesocket(statePtr->sockets->fd);
	    }

	    /*
	     * Get statePtr lock.
	     */

	    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	    /*
	     * Reset last error from last try
	     */

	    statePtr->notifierConnectError = 0;
	    Tcl_SetErrno(0);

	    statePtr->sockets->fd = socket(statePtr->myaddr->ai_family,
		    SOCK_STREAM, 0);

	    /*
	     * Free list lock.
	     */

	    SetEvent(tsdPtr->socketListLock);

	    /*
	     * Continue on socket creation error.
	     */

	    if (statePtr->sockets->fd == INVALID_SOCKET) {
		Tcl_WinConvertError((DWORD) WSAGetLastError());
		continue;
	    }

	    /*
	     * Win-NT has a misfeature that sockets are inherited in child
	     * processes by default. Turn off the inherit bit.
	     */

	    SetHandleInformation((HANDLE) statePtr->sockets->fd,
		    HANDLE_FLAG_INHERIT, 0);

	    /*
	     * Set kernel space buffering
	     */

	    TclSockMinimumBuffers((void *)statePtr->sockets->fd,
		    TCP_BUFFER_SIZE);

	    /*
	     * Try to bind to a local port.
	     */

	    if (bind(statePtr->sockets->fd, statePtr->myaddr->ai_addr,
		    (socklen_t)statePtr->myaddr->ai_addrlen) == SOCKET_ERROR) {
		Tcl_WinConvertError((DWORD) WSAGetLastError());
		continue;
	    }

	    /*
	     * For asynchronous connect set the socket in nonblocking mode
	     * and activate connect notification
	     */

	    if (async_connect) {
		TcpState *statePtr2;
		int in_socket_list = 0;

		/*
		 * Get statePtr lock.
		 */

		WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

		/*
		 * Bugfig for 336441ed59 to not ignore notifications until the
		 * infoPtr is in the list.
		 * Check if my statePtr is already in the tsdPtr->socketList
		 * It is set after this call by TcpThreadActionProc and is set
		 * on a second round.
		 *
		 * If not, we buffer my statePtr in the tsd memory so it is
		 * not lost by the event procedure
		 */

		for (statePtr2 = tsdPtr->socketList; statePtr2 != NULL;
			statePtr2 = statePtr2->nextPtr) {
		    if (statePtr2 == statePtr) {
			in_socket_list = 1;
			break;
		    }
		}
		if (!in_socket_list) {
		    tsdPtr->pendingTcpState = statePtr;
		}

		/*
		 * Set connect mask to connect events
		 *
		 * This is activated by a SOCKET_SELECT message to the
		 * notifier thread.
		 */

		SET_BITS(statePtr->selectEvents, FD_CONNECT);

		/*
		 * Free list lock.
		 */

		SetEvent(tsdPtr->socketListLock);

		/*
		 * Activate accept notification.
		 */

		SendSelectMessage(tsdPtr, SELECT, statePtr);
	    }

	    /*
	     * Attempt to connect to the remote socket.
	     */

	    connect(statePtr->sockets->fd, statePtr->addr->ai_addr,
		    (socklen_t)statePtr->addr->ai_addrlen);

	    error = WSAGetLastError();
	    Tcl_WinConvertError(error);

	    if (async_connect && error == WSAEWOULDBLOCK) {
		/*
		 * Asynchronous connect
		 *
		 * Remember that we jump back behind this next round
		 */

		SET_BITS(statePtr->flags, TCP_ASYNC_PENDING);
		return TCL_OK;

	    reenter:
		/*
		 * Re-entry point for async connect after connect event or
		 * blocking operation
		 *
		 * Clear the reenter flag
		 */

		CLEAR_BITS(statePtr->flags, TCP_ASYNC_PENDING);

		/*
		 * Get statePtr lock.
		 */

		WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

		/*
		 * Get signaled connect error.
		 */

		Tcl_WinConvertError((DWORD) statePtr->notifierConnectError);

		/*
		 * Clear eventual connect flag.
		 */

		CLEAR_BITS(statePtr->selectEvents, FD_CONNECT);

		/*
		 * Free list lock.
		 */

		SetEvent(tsdPtr->socketListLock);
	    }

	    /*
	     * Clear the tsd socket list pointer if we did not wait for
	     * the FD_CONNECT asynchronously
	     */

	    tsdPtr->pendingTcpState = NULL;

	    if (Tcl_GetErrno() == 0) {
		goto out;
	    }
	}
    }

  out:
    /*
     * Socket connected or connection failed
     */

    /*
     * Async connect terminated
     */

    CLEAR_BITS(statePtr->flags, TCP_ASYNC_CONNECT);

    if (Tcl_GetErrno() == 0) {
	/*
	 * Successfully connected
	 *
	 * Set up the select mask for read/write events.
	 */

	statePtr->selectEvents = FD_READ | FD_WRITE | FD_CLOSE;

	/*
	 * Register for interest in events in the select mask. Note that this
	 * automatically places the socket into non-blocking mode.
	 */

	SendSelectMessage(tsdPtr, SELECT, statePtr);
    } else {
	/*
	 * Connect failed
	 *
	 * For async connect schedule a writable event to report the fail.
	 */

	if (async_callback) {
	    /*
	     * Set up the select mask for read/write events.
	     */

	    statePtr->selectEvents = FD_WRITE|FD_READ;

	    /*
	     * Get statePtr lock.
	     */

	    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	    /*
	     * Signal ready readable and writable events.
	     */

	    SET_BITS(statePtr->readyEvents, FD_WRITE | FD_READ);

	    /*
	     * Flag error to event routine.
	     */

	    SET_BITS(statePtr->flags, TCP_ASYNC_FAILED);

	    /*
	     * Save connect error to be reported by 'fconfigure -error'.
	     */

	    statePtr->connectError = Tcl_GetErrno();

	    /*
	     * Free list lock.
	     */

	    SetEvent(tsdPtr->socketListLock);
	}

	/*
	 * Error message on synchronous connect
	 */

	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "couldn't open socket: %s", Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpClient --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed. An error message is returned in the
 *	interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpClient(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    int port,			/* Port number to open. */
    const char *host,		/* Host on which to open port. */
    const char *myaddr,		/* Client-side address */
    int myport,			/* Client-side port */
    int async)			/* If nonzero, attempt to do an asynchronous
				 * connect. Otherwise we do a blocking
				 * connect. */
{
    TcpState *statePtr;
    const char *errorMsg = NULL;
    struct addrinfo *addrlist = NULL, *myaddrlist = NULL;
    char channelName[SOCK_CHAN_LENGTH];

    TclInitSockets();

    /*
     * Do the name lookups for the local and remote addresses.
     */

    if (!TclCreateSocketAddress(interp, &addrlist, host, port, 0, &errorMsg)
	    || !TclCreateSocketAddress(interp, &myaddrlist, myaddr, myport, 1,
		    &errorMsg)) {
	if (addrlist != NULL) {
	    freeaddrinfo(addrlist);
	}
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "couldn't open socket: %s", errorMsg));
	}
	return NULL;
    }

    statePtr = NewSocketInfo(INVALID_SOCKET);
    statePtr->addrlist = addrlist;
    statePtr->myaddrlist = myaddrlist;
    if (async) {
	SET_BITS(statePtr->flags, TCP_ASYNC_CONNECT);
    }

    /*
     * Create a new client socket and wrap it in a channel.
     */
    if (TcpConnect(interp, statePtr) != TCL_OK) {
	TcpCloseProc(statePtr, NULL);
	return NULL;
    }

    TclWinGenerateChannelName(channelName, "sock", statePtr);
    statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    statePtr, (TCL_READABLE | TCL_WRITABLE));
    if (TCL_ERROR == Tcl_SetChannelOption(NULL, statePtr->channel,
	    "-translation", "auto crlf")) {
	Tcl_CloseEx(NULL, statePtr->channel, 0);
	return NULL;
    } else if (TCL_ERROR == Tcl_SetChannelOption(NULL, statePtr->channel,
	    "-eofchar", "")) {
	Tcl_CloseEx(NULL, statePtr->channel, 0);
	return NULL;
    }
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeTcpClientChannel --
 *
 *	Creates a Tcl_Channel from an existing client TCP socket.
 *
 * Results:
 *	The Tcl_Channel wrapped around the preexisting TCP socket.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeTcpClientChannel(
    void *sock)			/* The socket to wrap up into a channel. */
{
    TclInitSockets();

    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    /*
     * Set kernel space buffering and non-blocking.
     */

    TclSockMinimumBuffers(sock, TCP_BUFFER_SIZE);

    TcpState *statePtr = NewSocketInfo((SOCKET) sock);

    /*
     * Start watching for read/write events on the socket.
     */

    statePtr->selectEvents = FD_READ | FD_CLOSE | FD_WRITE;
    SendSelectMessage(tsdPtr, SELECT, statePtr);

    char channelName[SOCK_CHAN_LENGTH];
    TclWinGenerateChannelName(channelName, "sock", statePtr);
    statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    statePtr, (TCL_READABLE | TCL_WRITABLE));
    Tcl_SetChannelOption(NULL, statePtr->channel, "-translation", "auto crlf");
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpServerEx --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed. If an error occurred, an error message
 *	is left in the interp's result if interp is not NULL.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpServerEx(
    Tcl_Interp *interp,		/* For error reporting - may be NULL. */
    const char *service,	/* Port number to open. */
    const char *myHost,		/* Name of local host. */
    unsigned int flags,		/* Flags. */
    int backlog,		/* Length of OS listen backlog queue, or -1
				 * for default. */
    Tcl_TcpAcceptProc *acceptProc,
				/* Callback for accepting connections from new
				 * clients. */
    void *acceptProcData)	/* Data for the callback. */
{
    SOCKET sock = INVALID_SOCKET;
    unsigned short chosenport = 0;
    struct addrinfo *addrlist = NULL;
    struct addrinfo *addrPtr;	/* Socket address to listen on. */
    TcpState *statePtr = NULL;	/* The returned value. */
    char channelName[SOCK_CHAN_LENGTH];
    u_long flag = 1;		/* Indicates nonblocking mode. */
    const char *errorMsg = NULL;
    int optvalue, port;

    TclInitSockets();

    /*
     * Construct the addresses for each end of the socket.
     */

    if (TclSockGetPort(interp, service, "tcp", &port) != TCL_OK) {
	errorMsg = "invalid port number";
	goto error;
    }

    if (!TclCreateSocketAddress(interp, &addrlist, myHost, port, 1,
	    &errorMsg)) {
	goto error;
    }

    for (addrPtr = addrlist; addrPtr != NULL; addrPtr = addrPtr->ai_next) {
	sock = socket(addrPtr->ai_family, addrPtr->ai_socktype,
		addrPtr->ai_protocol);
	if (sock == INVALID_SOCKET) {
	    Tcl_WinConvertError((DWORD) WSAGetLastError());
	    continue;
	}

	/*
	 * Win-NT has a misfeature that sockets are inherited in child
	 * processes by default. Turn off the inherit bit.
	 */

	SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0);

	/*
	 * Set kernel space buffering
	 */

	TclSockMinimumBuffers((void *)sock, TCP_BUFFER_SIZE);

	/*
	 * Make sure we use the same port when opening two server sockets
	 * for IPv4 and IPv6.
	 *
	 * As sockaddr_in6 uses the same offset and size for the port
	 * member as sockaddr_in, we can handle both through the IPv4 API.
	 */

	if (port == 0 && chosenport != 0) {
	    ((struct sockaddr_in *) addrPtr->ai_addr)->sin_port =
		htons(chosenport);
	}

	/*
	 * The SO_REUSEADDR option on Windows behaves like SO_REUSEPORT on
	 * unix systems.
	 */

	if (GOT_BITS(flags, TCL_TCPSERVER_REUSEPORT)) {
	    optvalue = 1;
	    (void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &optvalue, sizeof(optvalue));
	}

	/*
	 * Bind to the specified port.
	 *
	 * Bind should not be affected by the socket having already been
	 * set into nonblocking mode. If there is trouble, this is one
	 * place to look for bugs.
	 */

	if (bind(sock, addrPtr->ai_addr,
		(socklen_t)addrPtr->ai_addrlen) == SOCKET_ERROR) {
	    Tcl_WinConvertError((DWORD) WSAGetLastError());
	    closesocket(sock);
	    sock = INVALID_SOCKET; /* Bug [40b1814b93] */
	    continue;
	}
	if (port == 0 && chosenport == 0) {
	    address sockname;
	    socklen_t namelen = sizeof(sockname);

	    /*
	     * Synchronize port numbers when binding to port 0 of multiple
	     * addresses.
	     */

	    if (getsockname(sock, &sockname.sa, &namelen) >= 0) {
		chosenport = ntohs(sockname.sa4.sin_port);
	    }
	}

	/*
	 * Set the maximum number of pending connect requests to the max
	 * value allowed on each platform (Win32 and Win32s may be
	 * different, and there may be differences between TCP/IP stacks).
	 */

	if (backlog < 0) {
	    backlog = SOMAXCONN;
	}
	if (listen(sock, backlog) == SOCKET_ERROR) {
	    Tcl_WinConvertError((DWORD) WSAGetLastError());
	    closesocket(sock);
	    sock = INVALID_SOCKET; /* Bug [40b1814b93] */
	    continue;
	}

	if (statePtr == NULL) {
	    /*
	     * Add this socket to the global list of sockets.
	     */

	    statePtr = NewSocketInfo(sock);
	} else {
	    AddSocketInfoFd(statePtr, sock);
	}
    }

  error:
    if (addrlist != NULL) {
	freeaddrinfo(addrlist);
    }

    if (statePtr != NULL) {
	ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
		TclThreadDataKeyGet(&dataKey);

	statePtr->acceptProc = acceptProc;
	statePtr->acceptProcData = acceptProcData;
	TclWinGenerateChannelName(channelName, "sock", statePtr);
	statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
		statePtr, 0);
	/*
	 * Set up the select mask for connection request events.
	 */

	statePtr->selectEvents = FD_ACCEPT;

	/*
	 * Register for interest in events in the select mask. Note that this
	 * automatically places the socket into non-blocking mode.
	 */

	ioctlsocket(sock, (long) FIONBIO, &flag);
	SendSelectMessage(tsdPtr, SELECT, statePtr);
	if (Tcl_SetChannelOption(interp, statePtr->channel, "-eofchar", "")
		== TCL_ERROR) {
	    Tcl_CloseEx(NULL, statePtr->channel, 0);
	    return NULL;
	}
	return statePtr->channel;
    }

    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"couldn't open socket: %s",
		(errorMsg ? errorMsg : Tcl_PosixError(interp))));
    }

    if (sock != INVALID_SOCKET) {
	closesocket(sock);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAccept --
 *	Accept a TCP socket connection.	 This is called by the event loop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new connection socket. Calls the registered callback for the
 *	connection acceptance mechanism.
 *
 *----------------------------------------------------------------------
 */

static void
TcpAccept(
    TcpFdList *fds,		/* Server socket that accepted newSocket. */
    SOCKET newSocket,		/* Newly accepted socket. */
    address addr)		/* Address of new socket. */
{
    TcpState *newInfoPtr;
    TcpState *statePtr = fds->statePtr;
    int len = sizeof(addr);
    char channelName[SOCK_CHAN_LENGTH];
    char host[NI_MAXHOST], port[NI_MAXSERV];
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    /*
     * Win-NT has a misfeature that sockets are inherited in child processes
     * by default. Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE) newSocket, HANDLE_FLAG_INHERIT, 0);

    /*
     * Add this socket to the global list of sockets.
     */

    newInfoPtr = NewSocketInfo(newSocket);

    /*
     * Select on read/write events and create the channel.
     */

    newInfoPtr->selectEvents = (FD_READ | FD_WRITE | FD_CLOSE);
    SendSelectMessage(tsdPtr, SELECT, newInfoPtr);

    TclWinGenerateChannelName(channelName, "sock", newInfoPtr);
    newInfoPtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    newInfoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(NULL, newInfoPtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_CloseEx(NULL, newInfoPtr->channel, 0);
	return;
    }
    if (Tcl_SetChannelOption(NULL, newInfoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
	Tcl_CloseEx(NULL, newInfoPtr->channel, 0);
	return;
    }

    /*
     * Invoke the accept callback function.
     */

    if (statePtr->acceptProc != NULL) {
	getnameinfo(&(addr.sa), len, host, sizeof(host), port, sizeof(port),
		NI_NUMERICHOST|NI_NUMERICSERV);
	statePtr->acceptProc(statePtr->acceptProcData, newInfoPtr->channel,
		host, atoi(port));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitSocketWindowClass --
 *
 *	Registers the event window class for the socket notifier code.
 *	Caller must not hold socket mutex lock.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Register a new window class.
 *
 *----------------------------------------------------------------------
 */

static void
InitSocketWindowClass(void)
{
    if (initialized) {
	return;
    }
    Tcl_MutexLock(&socketMutex);
    if (!initialized) {
	initialized = 1;
	TclCreateLateExitHandler(SocketExitHandler, NULL);

	/*
	 * Create the async notification window with a new class. We must
	 * create a new class to avoid a Windows 95 bug that causes us to get
	 * the wrong message number for socket events if the message window is
	 * a subclass of a static control.
	 */

	windowClass.style = 0;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = (HINSTANCE)TclWinGetTclInstance();
	windowClass.hbrBackground = NULL;
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = className;
	windowClass.lpfnWndProc = SocketProc;
	windowClass.hIcon = NULL;
	windowClass.hCursor = NULL;

	if (!RegisterClassW(&windowClass)) {
	    Tcl_WinConvertError(GetLastError());
	    goto initFailure;
	}
    }
    Tcl_MutexUnlock(&socketMutex);
    return;

  initFailure:
    Tcl_MutexUnlock(&socketMutex); /* Probably pointless before panicing */
    Tcl_Panic("InitSockets failed");
}

/*
 *----------------------------------------------------------------------
 *
 * SocketExitHandler --
 *
 *	Callback invoked during exit clean up to delete the socket
 *	communication window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SocketExitHandler(
    TCL_UNUSED(void *))
{
    Tcl_MutexLock(&socketMutex);

    /*
     * Make sure the socket event handling window is cleaned-up for, at
     * most, this thread.
     */

    TclpFinalizeSockets();
    UnregisterClassW(className, (HINSTANCE)TclWinGetTclInstance());
    initialized = 0;
    Tcl_MutexUnlock(&socketMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketSetupProc --
 *
 *	This function is invoked before Tcl_DoOneEvent blocks waiting for an
 *	event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adjusts the block time if needed.
 *
 *----------------------------------------------------------------------
 */

void
SocketSetupProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    TcpState *statePtr;
    Tcl_Time blockTime = { 0, 0 };
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!GOT_BITS(flags, TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Check to see if there is a ready socket.	 If so, poll.
     */
    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
    for (statePtr = tsdPtr->socketList; statePtr != NULL;
	    statePtr = statePtr->nextPtr) {
	if (GOT_BITS(statePtr->readyEvents,
		statePtr->watchEvents | FD_CONNECT | FD_ACCEPT)) {
	    Tcl_SetMaxBlockTime(&blockTime);
	    break;
	}
    }
    SetEvent(tsdPtr->socketListLock);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketCheckProc --
 *
 *	This function is called by Tcl_DoOneEvent to check the socket event
 *	source for events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event.
 *
 *----------------------------------------------------------------------
 */

static void
SocketCheckProc(
    TCL_UNUSED(void *),
    int flags)			/* Event flags as passed to Tcl_DoOneEvent. */
{
    TcpState *statePtr;
    SocketEvent *evPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!GOT_BITS(flags, TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * Queue events for any ready sockets that don't already have events
     * queued (caused by persistent states that won't generate WinSock
     * events).
     */

    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
    for (statePtr = tsdPtr->socketList; statePtr != NULL;
	    statePtr = statePtr->nextPtr) {
	if (GOT_BITS(statePtr->readyEvents,
		statePtr->watchEvents | FD_CONNECT | FD_ACCEPT)
		&& !GOT_BITS(statePtr->flags, SOCKET_PENDING)) {
	    SET_BITS(statePtr->flags, SOCKET_PENDING);
	    evPtr = (SocketEvent *)Tcl_Alloc(sizeof(SocketEvent));
	    evPtr->header.proc = SocketEventProc;
	    evPtr->socket = statePtr->sockets->fd;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }
    SetEvent(tsdPtr->socketListLock);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketEventProc --
 *
 *	This function is called by Tcl_ServiceEvent when a socket event
 *	reaches the front of the event queue. This function is responsible for
 *	notifying the generic channel code.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed from
 *	the queue. Returns 0 if the event was not handled, meaning it should
 *	stay on the queue. The only time the event isn't handled is if the
 *	TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the channel callback functions do.
 *
 *----------------------------------------------------------------------
 */

static int
SocketEventProc(
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to handle,
				 * such as TCL_FILE_EVENTS. */
{
    TcpState *statePtr;
    SocketEvent *eventPtr = (SocketEvent *) evPtr;
    int mask = 0, events;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    TcpFdList *fds;
    SOCKET newSocket;
    address addr;
    int len;

    if (!GOT_BITS(flags, TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Find the specified socket on the socket list.
     */

    WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
    for (statePtr = tsdPtr->socketList; statePtr != NULL;
	    statePtr = statePtr->nextPtr) {
	if (statePtr->sockets->fd == eventPtr->socket) {
	    break;
	}
    }

    /*
     * Discard events that have gone stale.
     */

    if (!statePtr) {
	SetEvent(tsdPtr->socketListLock);
	return 1;
    }

    /*
     * Clear flag that (this) event is pending
     */

    CLEAR_BITS(statePtr->flags, SOCKET_PENDING);

    /*
     * Continue async connect if pending and ready
     */

    if (GOT_BITS(statePtr->readyEvents, FD_CONNECT)) {
	if (GOT_BITS(statePtr->flags, TCP_ASYNC_PENDING)) {
	    /*
	     * Do one step and save eventual connect error
	     */

	    SetEvent(tsdPtr->socketListLock);
	    WaitForConnect(statePtr,NULL);
	} else {
	    /*
	     * No async connect reenter pending. Just clear event.
	     */

	    CLEAR_BITS(statePtr->readyEvents, FD_CONNECT);
	    SetEvent(tsdPtr->socketListLock);
	}
	return 1;
    }

    /*
     * Handle connection requests directly.
     */

    if (GOT_BITS(statePtr->readyEvents, FD_ACCEPT)) {
	for (fds = statePtr->sockets; fds != NULL; fds = fds->next) {
	    /*
	     * Accept the incoming connection request.
	     */

	    len = sizeof(address);
	    newSocket = accept(fds->fd, &(addr.sa), &len);

	    /*
	     * On Tcl server sockets with multiple OS fds we loop over the fds
	     * trying an accept() on each, so we expect INVALID_SOCKET.  There
	     * are also other network stack conditions that can result in
	     * FD_ACCEPT but a subsequent failure on accept() by the time we
	     * get around to it.
	     *
	     * Access to sockets (acceptEventCount, readyEvents) in socketList
	     * is still protected by the lock (prevents reintroduction of
	     * SF Tcl Bug 3056775.
	     */

	    if (newSocket == INVALID_SOCKET) {
		/* int err = WSAGetLastError(); */
		continue;
	    }

	    /*
	     * It is possible that more than one FD_ACCEPT has been sent, so
	     * an extra count must be kept. Decrement the count, and reset the
	     * readyEvent bit if the count is no longer > 0.
	     */

	    statePtr->acceptEventCount--;

	    if (statePtr->acceptEventCount <= 0) {
		CLEAR_BITS(statePtr->readyEvents, FD_ACCEPT);
	    }

	    SetEvent(tsdPtr->socketListLock);

	    /*
	     * Caution: TcpAccept() has the side-effect of evaluating the
	     * server accept script (via AcceptCallbackProc() in tclIOCmd.c),
	     * which can close the server socket and invalidate statePtr and
	     * fds. If TcpAccept() accepts a socket we must return immediately
	     * and let SocketCheckProc queue additional FD_ACCEPT events.
	     */

	    TcpAccept(fds, newSocket, addr);
	    return 1;
	}

	/*
	 * Loop terminated with no sockets accepted; clear the ready mask so
	 * we can detect the next connection request. Note that connection
	 * requests are level triggered, so if there is a request already
	 * pending, a new event will be generated.
	 */

	statePtr->acceptEventCount = 0;
	CLEAR_BITS(statePtr->readyEvents, FD_ACCEPT);

	SetEvent(tsdPtr->socketListLock);
	return 1;
    }

    SetEvent(tsdPtr->socketListLock);

    /*
     * Mask off unwanted events and compute the read/write mask so we can
     * notify the channel.
     */

    events = statePtr->readyEvents & statePtr->watchEvents;

    if (GOT_BITS(events, FD_CLOSE)) {
	/*
	 * If the socket was closed and the channel is still interested in
	 * read events, then we need to ensure that we keep polling for this
	 * event until someone does something with the channel. Note that we
	 * do this before calling Tcl_NotifyChannel so we don't have to watch
	 * out for the channel being deleted out from under us. This may cause
	 * a redundant trip through the event loop, but it's simpler than
	 * trying to do unwind protection.
	 */

	Tcl_Time blockTime = { 0, 0 };

	Tcl_SetMaxBlockTime(&blockTime);
	SET_BITS(mask, TCL_READABLE | TCL_WRITABLE);
    } else if (GOT_BITS(events, FD_READ)) {
	/*
	 * Throw the readable event if an async connect failed.
	 */

	if (GOT_BITS(statePtr->flags, TCP_ASYNC_FAILED)) {
	    SET_BITS(mask, TCL_READABLE);
	} else {
	    fd_set readFds;
	    struct timeval timeout;

	    /*
	     * We must check to see if data is really available, since someone
	     * could have consumed the data in the meantime. Turn off async
	     * notification so select will work correctly. If the socket is
	     * still readable, notify the channel driver, otherwise reset the
	     * async select handler and keep waiting.
	     */

	    SendSelectMessage(tsdPtr, UNSELECT, statePtr);

	    FD_ZERO(&readFds);
	    FD_SET(statePtr->sockets->fd, &readFds);
	    timeout.tv_usec = 0;
	    timeout.tv_sec = 0;

	    if (select(0, &readFds, NULL, NULL, &timeout) != 0) {
		SET_BITS(mask, TCL_READABLE);
	    } else {
		CLEAR_BITS(statePtr->readyEvents, FD_READ);
		SendSelectMessage(tsdPtr, SELECT, statePtr);
	    }
	}
    }

    /*
     * writable event
     */

    if (GOT_BITS(events, FD_WRITE)) {
	SET_BITS(mask, TCL_WRITABLE);
    }

    /*
     * Call registered event procedures
     */

    if (mask) {
	Tcl_NotifyChannel(statePtr->channel, mask);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * AddSocketInfoFd --
 *
 *	This function adds a SOCKET file descriptor to the 'sockets' linked
 *	list of a TcpState structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None, except for allocation of memory.
 *
 *----------------------------------------------------------------------
 */

static void
AddSocketInfoFd(
    TcpState *statePtr,
    SOCKET socket)
{
    TcpFdList *fds = statePtr->sockets;

    if (fds == NULL) {
	/*
	 * Add the first FD.
	 */

	statePtr->sockets = (TcpFdList *)Tcl_Alloc(sizeof(TcpFdList));
	fds = statePtr->sockets;
    } else {
	/*
	 * Find end of list and append FD.
	 */

	while (fds->next != NULL) {
	    fds = fds->next;
	}

	fds->next = (TcpFdList *)Tcl_Alloc(sizeof(TcpFdList));
	fds = fds->next;
    }

    /*
     * Populate new FD.
     */

    fds->fd = socket;
    fds->statePtr = statePtr;
    fds->next = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * NewSocketInfo --
 *
 *	This function allocates and initializes a new TcpState structure.
 *
 * Results:
 *	Returns a newly allocated TcpState.
 *
 * Side effects:
 *	None, except for allocation of memory.
 *
 *----------------------------------------------------------------------
 */

static TcpState *
NewSocketInfo(
    SOCKET socket)
{
    TcpState *statePtr = (TcpState *)Tcl_Alloc(sizeof(TcpState));

    memset(statePtr, 0, sizeof(TcpState));

    /*
     * TIP #218. Removed the code inserting the new structure into the global
     * list. This is now handled in the thread action callbacks, and only
     * there.
     */

    AddSocketInfoFd(statePtr, socket);

    return statePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForSocketEvent --
 *
 *	Waits until one of the specified events occurs on a socket.
 *	For event FD_CONNECT use WaitForConnect.
 *
 * Results:
 *	Returns 1 on success or 0 on failure, with an error code in
 *	errorCodePtr.
 *
 * Side effects:
 *	Processes socket events off the system queue.
 *
 *----------------------------------------------------------------------
 */

static int
WaitForSocketEvent(
    TcpState *statePtr,		/* Information about this socket. */
    int events,			/* Events to look for. May be one of
				 * FD_READ or FD_WRITE. */
    int *errorCodePtr)		/* Where to store errors? */
{
    int result = 1;
    int oldMode;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    TclThreadDataKeyGet(&dataKey);

    /*
     * Be sure to disable event servicing so we are truly modal.
     */

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_NONE);

    /*
     * Reset WSAAsyncSelect so we have a fresh set of events pending.
     */

    SendSelectMessage(tsdPtr, UNSELECT, statePtr);
    SendSelectMessage(tsdPtr, SELECT, statePtr);

    while (1) {
	int event_found;

	/*
	 * Get statePtr lock.
	 */

	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	/*
	 * Check if event occurred.
	 */

	event_found = GOT_BITS(statePtr->readyEvents, events);

	/*
	 * Free list lock.
	 */

	SetEvent(tsdPtr->socketListLock);

	/*
	 * Exit loop if event occurred.
	 */

	if (event_found) {
	    break;
	}

	/*
	 * Exit loop if event did not occur but this is a non-blocking channel
	 */

	if (statePtr->flags & TCP_NONBLOCKING) {
	    *errorCodePtr = EWOULDBLOCK;
	    result = 0;
	    break;
	}

	/*
	 * Wait until something happens.
	 */

	WaitForSingleObject(tsdPtr->readyEvent, INFINITE);
    }

    (void) Tcl_SetServiceMode(oldMode);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketThread --
 *
 *	Helper thread used to manage the socket event handling window.
 *
 * Results:
 *	1 if unable to create socket event window, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
SocketThread(
    LPVOID arg)
{
    MSG msg;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)arg;

    /*
     * Create a dummy window receiving socket events.
     */

    tsdPtr->hwnd = CreateWindowW(className, className, WS_TILED, 0, 0, 0, 0,
	    NULL, NULL, windowClass.hInstance, arg);

    /*
     * Signalize thread creator that we are done creating the window.
     */

    SetEvent(tsdPtr->readyEvent);

    /*
     * If unable to create the window, exit this thread immediately.
     */

    if (tsdPtr->hwnd == NULL) {
	return 1;
    }

    /*
     * Process all messages on the socket window until WM_QUIT. This threads
     * exits only when instructed to do so by the call to
     * PostMessageW(SOCKET_TERMINATE) in TclpFinalizeSockets().
     */

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
	DispatchMessageW(&msg);
    }

    /*
     * This releases waiters on thread exit in TclpFinalizeSockets()
     */

    SetEvent(tsdPtr->readyEvent);

    return (DWORD)msg.wParam;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketProc --
 *
 *	This function is called when WSAAsyncSelect has been used to register
 *	interest in a socket event, and the event has occurred.
 *
 * Results:
 *	0 on success.
 *
 * Side effects:
 *	The flags for the given socket are updated to reflect the event that
 *	occurred.
 *
 *----------------------------------------------------------------------
 */

static LRESULT CALLBACK
SocketProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    int event, error;
    SOCKET socket;
    TcpState *statePtr;
    int info_found = 0;
    TcpFdList *fds = NULL;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
#ifdef _WIN64
	    GetWindowLongPtrW(hwnd, GWLP_USERDATA);
#else
	    GetWindowLongW(hwnd, GWL_USERDATA);
#endif

    switch (message) {
    default:
	return DefWindowProcW(hwnd, message, wParam, lParam);
	break;

    case WM_CREATE:
	/*
	 * Store the initial tsdPtr, it's from a different thread, so it's not
	 * directly accessible, but needed.
	 */

#ifdef _WIN64
	SetWindowLongPtrW(hwnd, GWLP_USERDATA,
		(LONG_PTR) ((LPCREATESTRUCT)lParam)->lpCreateParams);
#else
	SetWindowLongW(hwnd, GWL_USERDATA,
		(LONG) ((LPCREATESTRUCT)lParam)->lpCreateParams);
#endif
	break;

    case WM_DESTROY:
	PostQuitMessage(0);
	break;

    case SOCKET_MESSAGE:
	event = WSAGETSELECTEVENT(lParam);
	error = WSAGETSELECTERROR(lParam);
	socket = (SOCKET) wParam;

	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);

	/*
	 * Find the specified socket on the socket list and update its
	 * eventState flag.
	 */

	for (statePtr = tsdPtr->socketList; statePtr != NULL;
		statePtr = statePtr->nextPtr) {
	    if (FindFDInList(statePtr, socket)) {
		info_found = 1;
		break;
	    }
	}

	/*
	 * Check if there is a pending info structure not jet in the list.
	 */

	if (!info_found
		&& tsdPtr->pendingTcpState != NULL
		&& FindFDInList(tsdPtr->pendingTcpState, socket)) {
	    statePtr = tsdPtr->pendingTcpState;
	    info_found = 1;
	}
	if (info_found) {
	    /*
	     * Update the socket state.
	     *
	     * A count of FD_ACCEPTS is stored, so if an FD_CLOSE event
	     * happens, then clear the FD_ACCEPT count. Otherwise, increment
	     * the count if the current event is an FD_ACCEPT.
	     */

	    if (GOT_BITS(event, FD_CLOSE)) {
		statePtr->acceptEventCount = 0;
		CLEAR_BITS(statePtr->readyEvents, FD_WRITE | FD_ACCEPT);
	    } else if (GOT_BITS(event, FD_ACCEPT)) {
		statePtr->acceptEventCount++;
	    }

	    if (GOT_BITS(event, FD_CONNECT)) {
		/*
		 * Remember any error that occurred so we can report
		 * connection failures.
		 */

		if (error != ERROR_SUCCESS) {
		    statePtr->notifierConnectError = error;
		}
	    }

	    /*
	     * Inform main thread about signaled events
	     */

	    SET_BITS(statePtr->readyEvents, event);

	    /*
	     * Wake up the Main Thread.
	     */

	    SetEvent(tsdPtr->readyEvent);
	    Tcl_ThreadAlert(tsdPtr->threadId);
	}
	SetEvent(tsdPtr->socketListLock);
	break;

    case SOCKET_SELECT:
	statePtr = (TcpState *) lParam;
	for (fds = statePtr->sockets; fds != NULL; fds = fds->next) {
	    if (wParam == SELECT) {
		WSAAsyncSelect(fds->fd, hwnd,
			SOCKET_MESSAGE, statePtr->selectEvents);
	    } else {
		/*
		 * Clear the selection mask
		 */

		WSAAsyncSelect(fds->fd, hwnd, 0, 0);
	    }
	}
	break;

    case SOCKET_TERMINATE:
	DestroyWindow(hwnd);
	break;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FindFDInList --
 *
 *	Return true, if the given file descriptor is contained in the
 *	file descriptor list.
 *
 * Results:
 *	true if found.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static int
FindFDInList(
    TcpState *statePtr,
    SOCKET socket)
{
    TcpFdList *fds;
    for (fds = statePtr->sockets; fds != NULL; fds = fds->next) {
	if (fds->fd == socket) {
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpThreadActionProc --
 *
 *	Insert or remove any thread local refs to this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes thread local list of valid channels.
 *
 *----------------------------------------------------------------------
 */

static void
TcpThreadActionProc(
    void *instanceData,
    int action)
{
    ThreadSpecificData *tsdPtr;
    TcpState *statePtr = (TcpState *)instanceData;
    int notifyCmd;

    if (action == TCL_CHANNEL_THREAD_INSERT) {
	/*
	 * Ensure that socket subsystem is initialized in this thread, or else
	 * sockets will not work.
	 */

	TclInitSockets();

	tsdPtr = TCL_TSD_INIT(&dataKey);

	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
	statePtr->nextPtr = tsdPtr->socketList;
	tsdPtr->socketList = statePtr;

	if (statePtr == tsdPtr->pendingTcpState) {
	    tsdPtr->pendingTcpState = NULL;
	}

	SetEvent(tsdPtr->socketListLock);

	notifyCmd = SELECT;
    } else {
	TcpState **nextPtrPtr;
	int removed = 0;

	tsdPtr = TCL_TSD_INIT(&dataKey);

	/*
	 * TIP #218, Bugfix: All access to socketList has to be protected by
	 * the lock.
	 */

	WaitForSingleObject(tsdPtr->socketListLock, INFINITE);
	for (nextPtrPtr = &(tsdPtr->socketList); (*nextPtrPtr) != NULL;
		nextPtrPtr = &((*nextPtrPtr)->nextPtr)) {
	    if ((*nextPtrPtr) == statePtr) {
		(*nextPtrPtr) = statePtr->nextPtr;
		removed = 1;
		break;
	    }
	}
	SetEvent(tsdPtr->socketListLock);

	/*
	 * This could happen if the channel was created in one thread and then
	 * moved to another without updating the thread local data in each
	 * thread.
	 */

	if (!removed) {
	    Tcl_Panic("file info ptr not on thread channel list");
	}

	notifyCmd = UNSELECT;
    }

    /*
     * Ensure that, or stop, notifications for the socket occur in this
     * thread.
     */

    SendSelectMessage(tsdPtr, notifyCmd, statePtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 */
