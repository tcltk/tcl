/* 
 * tclWinSock.c --
 *
 *	This file contains Windows-specific socket related code.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinSock.c,v 1.9 1999/04/22 20:28:02 redman Exp $
 */

#include "tclWinInt.h"

/*
 * The following variable is used to tell whether this module has been
 * initialized.
 */

static int initialized = 0;

static int  hostnameInitialized = 0;
static char hostname[255];	/* This buffer should be big enough for
                                 * hostname plus domain name. */
static int useThreads = 0;

static HANDLE   socketThread;  /* Thread used with WSAEventSelect to check
				  sockets for fileevents on NT (not used for
				  Win95 or Win98) */
static WSAEVENT socketEvent;   /* Event triggered by WSAEventSelect, for all
				  sockets, NT only */
static WSAEVENT killEvent;     /* Event used to kill off the socketThread, NT
				  only */

static CRITICAL_SECTION socketCS; /* Critical Section used even when building
				     without threads */
TCL_DECLARE_MUTEX(socketMutex)    /* Mutex used when built with multithreading
				   */ 

    
/*
 * The following structure contains pointers to all of the WinSock API entry
 * points used by Tcl.  It is initialized by InitSockets.  Since we
 * dynamically load Winsock.dll on demand, we must use this function table
 * to refer to functions in the socket API.
 */

static struct {
    HINSTANCE hInstance;	/* Handle to WinSock library. */
    SOCKET (PASCAL FAR *accept)(SOCKET s, struct sockaddr FAR *addr,
	    int FAR *addrlen);
    int (PASCAL FAR *bind)(SOCKET s, const struct sockaddr FAR *addr,
	    int namelen);
    int (PASCAL FAR *closesocket)(SOCKET s);
    int (PASCAL FAR *connect)(SOCKET s, const struct sockaddr FAR *name,
	    int namelen);
    int (PASCAL FAR *ioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
    int (PASCAL FAR *getsockopt)(SOCKET s, int level, int optname,
	    char FAR * optval, int FAR *optlen);
    u_short (PASCAL FAR *htons)(u_short hostshort);
    unsigned long (PASCAL FAR *inet_addr)(const char FAR * cp);
    char FAR * (PASCAL FAR *inet_ntoa)(struct in_addr in);
    int (PASCAL FAR *listen)(SOCKET s, int backlog);
    u_short (PASCAL FAR *ntohs)(u_short netshort);
    int (PASCAL FAR *recv)(SOCKET s, char FAR * buf, int len, int flags);
    int (PASCAL FAR *select)(int nfds, fd_set FAR * readfds,
	    fd_set FAR * writefds, fd_set FAR * exceptfds,
	    const struct timeval FAR * tiemout);
    int (PASCAL FAR *send)(SOCKET s, const char FAR * buf, int len, int flags);
    int (PASCAL FAR *setsockopt)(SOCKET s, int level, int optname,
	    const char FAR * optval, int optlen);
    int (PASCAL FAR *shutdown)(SOCKET s, int how);
    SOCKET (PASCAL FAR *socket)(int af, int type, int protocol);
    struct hostent FAR * (PASCAL FAR *gethostbyname)(const char FAR * name);
    struct hostent FAR * (PASCAL FAR *gethostbyaddr)(const char FAR *addr,
            int addrlen, int addrtype);
    int (PASCAL FAR *gethostname)(char FAR * name, int namelen);
    int (PASCAL FAR *getpeername)(SOCKET sock, struct sockaddr FAR *name,
            int FAR *namelen);
    struct servent FAR * (PASCAL FAR *getservbyname)(const char FAR * name,
	    const char FAR * proto);
    int (PASCAL FAR *getsockname)(SOCKET sock, struct sockaddr FAR *name,
            int FAR *namelen);
    int (PASCAL FAR *WSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
    int (PASCAL FAR *WSACleanup)(void);
    int (PASCAL FAR *WSAGetLastError)(void);
    int (PASCAL FAR *WSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg,
	    long lEvent);

    /*
     * The following are used for the WinSock 2.0 for NT implementation
     */

    WSAEVENT (PASCAL FAR *WSACreateEvent)(void);
    BOOL (PASCAL FAR *WSACloseEvent)(WSAEVENT event);
    int (PASCAL FAR *WSAEventSelect)(SOCKET s, WSAEVENT event, long lEvent);
    BOOL (PASCAL FAR *WSAResetEvent)(WSAEVENT event);
    BOOL (PASCAL FAR *WSASetEvent)(WSAEVENT event);
    int (PASCAL FAR *WSAEnumNetworkEvents)(SOCKET s, WSAEVENT event,
	    LPWSANETWORKEVENTS events);
    DWORD (PASCAL FAR *WSAWaitForMultipleEvents)(DWORD cEvents,
	    const WSAEVENT FAR *events, 
	    BOOL fWaitAll, DWORD dwTimeOUT, BOOL fAlertable); 
} winSock;

/*
 * The following defines declare the messages used on socket windows.
 */

#define SOCKET_MESSAGE	WM_USER+1

/*
 * The following structure is used to store the data associated with
 * each socket.
 */

typedef struct SocketInfo {
    Tcl_Channel channel;	   /* Channel associated with this socket. */
    SOCKET socket;		   /* Windows SOCKET handle. */
    int flags;			   /* Bit field comprised of the flags
				    * described below.  */
    int watchEvents;		   /* OR'ed combination of FD_READ, FD_WRITE,
                                    * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				    * indicate which events are interesting. */
    int readyEvents;		   /* OR'ed combination of FD_READ, FD_WRITE,
                                    * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				    * indicate which events have occurred. Not
				    * used for the WinSock 2.0 for NT
				    * implementation. */
    int selectEvents;		   /* OR'ed combination of FD_READ, FD_WRITE,
                                    * FD_CLOSE, FD_ACCEPT and FD_CONNECT that
				    * indicate which events are currently
				    * being selected. */
    Tcl_TcpAcceptProc *acceptProc; /* Proc to call on accept. */
    ClientData acceptProcData;	   /* The data for the accept proc. */
    int lastError;		   /* Error code from last message. */
    struct SocketInfo *nextPtr;	   /* The next socket on the global socket
				    * list. */
} SocketInfo;

/*
 * The following structure is what is added to the Tcl event queue when
 * a socket event occurs.
 */

typedef struct SocketEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    SOCKET socket;		/* Socket descriptor that is ready.  Used
				 * to find the SocketInfo structure for
				 * the file (can't point directly to the
				 * SocketInfo structure because it could
				 * go away while the event is queued). */
} SocketEvent;

/*
 * This defines the minimum buffersize maintained by the kernel.
 */

#define TCP_BUFFER_SIZE 4096

/*
 * The following macros may be used to set the flags field of
 * a SocketInfo structure.
 */

#define SOCKET_ASYNC		(1<<0)	/* The socket is in blocking mode. */
#define SOCKET_EOF		(1<<1)	/* A zero read happened on
					 * the socket. */
#define SOCKET_ASYNC_CONNECT	(1<<2)	/* This socket uses async connect. */
#define SOCKET_PENDING		(1<<3)	/* A message has been sent
					 * for this socket */

typedef struct ThreadSpecificData {
    SocketInfo *socketList;     /* List of all sockets in the thread */

    /*
     * Data need for the Win95/Win98 implementation:
     */
    
    HWND hwnd;			/* Handle to window for socket messages. */

    /*
     * Data need for the WinNT implementation:
     */
    
    HANDLE wakeEvent;           /* Event to wake up the socket thread */
    Tcl_ThreadId threadId;     /* ID of the thread */

    struct ThreadSpecificData *next; /* Next entry in the thread list */
} ThreadSpecificData;

ThreadSpecificData *firstWaitingThread; /* List of threads */

static Tcl_ThreadDataKey dataKey;

/*
 * Static functions defined in this file.
 */

static SocketInfo *	CreateSocket _ANSI_ARGS_((Tcl_Interp *interp,
			    int port, char *host, int server, char *myaddr,
			    int myport, int async));
static int		CreateSocketAddress _ANSI_ARGS_(
			    (struct sockaddr_in *sockaddrPtr,
			    char *host, int port));
static void		InitSockets _ANSI_ARGS_((void));
static SocketInfo *	NewSocketInfo _ANSI_ARGS_((SOCKET socket));
static void		SocketCheckProc _ANSI_ARGS_((ClientData clientData,
			    int flags));
static int		SocketEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));
static void		SocketExitHandler _ANSI_ARGS_((ClientData clientData));
static LRESULT CALLBACK	SocketProc _ANSI_ARGS_((HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam));
static DWORD WINAPI     SocketThread(LPVOID arg);
static void		SocketSetupProc _ANSI_ARGS_((ClientData clientData,
			    int flags));
static void		SocketThreadExitHandler _ANSI_ARGS_((ClientData clientData));
static int		SocketsEnabled _ANSI_ARGS_((void));
static void		TcpAccept _ANSI_ARGS_((SocketInfo *infoPtr));
static int		TcpBlockProc _ANSI_ARGS_((ClientData instanceData,
			    int mode));
static int		TcpCloseProc _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_Interp *interp));
static int		TcpGetOptionProc _ANSI_ARGS_((ClientData instanceData,
		            Tcl_Interp *interp, char *optionName,
			    Tcl_DString *optionValue));
static int		TcpInputProc _ANSI_ARGS_((ClientData instanceData,
	            	    char *buf, int toRead, int *errorCode));
static int		TcpOutputProc _ANSI_ARGS_((ClientData instanceData,
	            	    char *buf, int toWrite, int *errorCode));
static void		TcpWatchProc _ANSI_ARGS_((ClientData instanceData,
		            int mask));
static int		TcpGetHandleProc _ANSI_ARGS_((ClientData instanceData,
		            int direction, ClientData *handlePtr));
static int		WaitForSocketEvent _ANSI_ARGS_((SocketInfo *infoPtr,
		            int events, int *errorCodePtr));

/*
 * This structure describes the channel type structure for TCP socket
 * based IO.
 */

static Tcl_ChannelType tcpChannelType = {
    "tcp",		/* Type name. */
    TcpBlockProc,	/* Set socket into blocking/non-blocking mode. */
    TcpCloseProc,	/* Close proc. */
    TcpInputProc,	/* Input proc. */
    TcpOutputProc,	/* Output proc. */
    NULL,		/* Seek proc. */
    NULL,		/* Set option proc. */
    TcpGetOptionProc,	/* Get option proc. */
    TcpWatchProc,	/* Initialize notifier to watch this channel. */
    TcpGetHandleProc,	/* Get an OS handle from channel. */
};

/*
 * Define version of Winsock required by Tcl.
 */

#define WSA_VERSION_REQD MAKEWORD(1,1)


/*
 *----------------------------------------------------------------------
 *
 * GetSocketState --
 *
 *	Retrieve the state of a given socket from the winsock API
 *      using WSAEnumNetWorkEvents.  Modify the state appropriately
 *      when the winsock version is wrong.
 *
 *	Assumes Mutex is held.
 *
 * Results:
 *	State of the socket in FD_* masks.
 *
 * Side effects:
 *	Modifies the infoPtr values to reflect the current state.
 *
 *----------------------------------------------------------------------
 */

int GetSocketState(SocketInfo *infoPtr)
{
    WSANETWORKEVENTS netEvents;
    int event = 0;

    if (!useThreads) {
	return infoPtr->readyEvents;
    }
    
    EnterCriticalSection(&socketCS);
    if ((*winSock.WSAEnumNetworkEvents)(infoPtr->socket,
	    socketEvent, &netEvents) == 0) {
	
	event = netEvents.lNetworkEvents;
	
	if (event & FD_CLOSE) {
	    event &= ~(FD_WRITE|FD_ACCEPT);
	}
	if (event & FD_CONNECT) {
	    /*
	     * The socket is now connected, so clear the
	     * async connect flag.
	     */
	    
	    infoPtr->flags &= ~(SOCKET_ASYNC_CONNECT);
	    
	} 
	if(infoPtr->flags & SOCKET_ASYNC_CONNECT) {
	    infoPtr->flags &= ~(SOCKET_ASYNC_CONNECT);
	    event |= FD_WRITE;
	}
    }
    LeaveCriticalSection(&socketCS);

    return event;
}

/*
 *----------------------------------------------------------------------
 *
 * InitSockets --
 *
 *	Initialize the socket module.  Attempts to load the wsock32.dll
 *	library and set up the winSock function table.  If successful,
 *	registers the event window for the socket notifier code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Dynamically loads wsock32.dll, and registers a new window
 *	class and creates a window for use in asynchronous socket
 *	notification.
 *
 *----------------------------------------------------------------------
 */

static void
InitSockets()
{
    WSADATA wsaData;
    DWORD id;
    OSVERSIONINFO info;
    static WNDCLASSA class;
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    Tcl_MutexLock(&socketMutex);
    if (! initialized) {
	
	InitializeCriticalSection(&socketCS);
	initialized = 1;
	Tcl_CreateExitHandler(SocketExitHandler, (ClientData) NULL);
    
	/*
	 * Find out if we're running on Win32s.
	 */
    
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&info);
    
	/*
	 * Check to see if Sockets are supported on this system.
	 *
	 * For WinNT, use the WinSock 2.0 API to avoid creating extra windows
	 * to handle socket events.
	 *
	 * Not all versions of Win95 have WinSock 2.0, don't count on it being
	 * there.  Also, until someone can get the code to work on Win98 or
	 * Win95 with WinSock 2.0, use the window-based version for those
	 * OS's.  Major socket users (servers) will probably be on NT anyway.
	 */
    
	if ((info.dwPlatformId == VER_PLATFORM_WIN32_NT)&&
		(SearchPathA(NULL, "ws2_32", ".dll", 0, NULL, NULL) != 0) ) {
	    useThreads = 1;
	    winSock.hInstance = LoadLibraryA("ws2_32.dll");
	} else if (SearchPathA(NULL, "wsock32", ".dll", 0, NULL, NULL) != 0) {
	    winSock.hInstance = LoadLibraryA("wsock32.dll");
	} else {
	    winSock.hInstance = NULL;
	}
    
	/*
	 * Initialize the function table.
	 */
    
	if (!SocketsEnabled()) {
	    Tcl_MutexUnlock(&socketMutex);
	    return;
	}
    
	winSock.accept = (SOCKET (PASCAL FAR *)(SOCKET s,
		struct sockaddr FAR *addr, int FAR *addrlen))
	    GetProcAddress(winSock.hInstance, "accept");
	winSock.bind = (int (PASCAL FAR *)(SOCKET s,
		const struct sockaddr FAR *addr, int namelen))
	    GetProcAddress(winSock.hInstance, "bind");
	winSock.closesocket = (int (PASCAL FAR *)(SOCKET s))
	    GetProcAddress(winSock.hInstance, "closesocket");
	winSock.connect = (int (PASCAL FAR *)(SOCKET s,
		const struct sockaddr FAR *name, int namelen))
	    GetProcAddress(winSock.hInstance, "connect");
	winSock.ioctlsocket = (int (PASCAL FAR *)(SOCKET s, long cmd,
		u_long FAR *argp))
	    GetProcAddress(winSock.hInstance, "ioctlsocket");
	winSock.getsockopt = (int (PASCAL FAR *)(SOCKET s,
		int level, int optname, char FAR * optval, int FAR *optlen))
	    GetProcAddress(winSock.hInstance, "getsockopt");
	winSock.htons = (u_short (PASCAL FAR *)(u_short hostshort))
	    GetProcAddress(winSock.hInstance, "htons");
	winSock.inet_addr = (unsigned long (PASCAL FAR *)(const char FAR *cp))
	    GetProcAddress(winSock.hInstance, "inet_addr");
	winSock.inet_ntoa = (char FAR * (PASCAL FAR *)(struct in_addr in))
	    GetProcAddress(winSock.hInstance, "inet_ntoa");
	winSock.listen = (int (PASCAL FAR *)(SOCKET s, int backlog))
	    GetProcAddress(winSock.hInstance, "listen");
	winSock.ntohs = (u_short (PASCAL FAR *)(u_short netshort))
	    GetProcAddress(winSock.hInstance, "ntohs");
	winSock.recv = (int (PASCAL FAR *)(SOCKET s, char FAR * buf,
		int len, int flags)) GetProcAddress(winSock.hInstance, "recv");
	winSock.select = (int (PASCAL FAR *)(int nfds, fd_set FAR * readfds,
		fd_set FAR * writefds, fd_set FAR * exceptfds,
		const struct timeval FAR * tiemout))
	    GetProcAddress(winSock.hInstance, "select");
	winSock.send = (int (PASCAL FAR *)(SOCKET s, const char FAR * buf,
		int len, int flags)) GetProcAddress(winSock.hInstance, "send");
	winSock.setsockopt = (int (PASCAL FAR *)(SOCKET s, int level,
		int optname, const char FAR * optval, int optlen))
	    GetProcAddress(winSock.hInstance, "setsockopt");
	winSock.shutdown = (int (PASCAL FAR *)(SOCKET s, int how))
	    GetProcAddress(winSock.hInstance, "shutdown");
	winSock.socket = (SOCKET (PASCAL FAR *)(int af, int type,
		int protocol)) GetProcAddress(winSock.hInstance, "socket");
	winSock.gethostbyaddr = (struct hostent FAR * (PASCAL FAR *)
		(const char FAR *addr, int addrlen, int addrtype))
	    GetProcAddress(winSock.hInstance, "gethostbyaddr");
	winSock.gethostbyname = (struct hostent FAR * (PASCAL FAR *)
		(const char FAR *name))
	    GetProcAddress(winSock.hInstance, "gethostbyname");
	winSock.gethostname = (int (PASCAL FAR *)(char FAR * name,
		int namelen)) GetProcAddress(winSock.hInstance, "gethostname");
	winSock.getpeername = (int (PASCAL FAR *)(SOCKET sock,
		struct sockaddr FAR *name, int FAR *namelen))
	    GetProcAddress(winSock.hInstance, "getpeername");
	winSock.getservbyname = (struct servent FAR * (PASCAL FAR *)
		(const char FAR * name, const char FAR * proto))
	    GetProcAddress(winSock.hInstance, "getservbyname");
	winSock.getsockname = (int (PASCAL FAR *)(SOCKET sock,
		struct sockaddr FAR *name, int FAR *namelen))
	    GetProcAddress(winSock.hInstance, "getsockname");
	winSock.WSAStartup = (int (PASCAL FAR *)(WORD wVersionRequired,
		LPWSADATA lpWSAData)) GetProcAddress(winSock.hInstance,
			"WSAStartup");
	winSock.WSACleanup = (int (PASCAL FAR *)(void))
	    GetProcAddress(winSock.hInstance, "WSACleanup");
	winSock.WSAGetLastError = (int (PASCAL FAR *)(void))
	    GetProcAddress(winSock.hInstance, "WSAGetLastError");
	winSock.WSAAsyncSelect = (int (PASCAL FAR *)(SOCKET s, HWND hWnd,
		u_int wMsg, long lEvent))
	    GetProcAddress(winSock.hInstance, "WSAAsyncSelect");

	if (useThreads) {
	    winSock.WSACreateEvent = (WSAEVENT (PASCAL FAR *)(void))
		GetProcAddress(winSock.hInstance, "WSACreateEvent");
	    winSock.WSACloseEvent = (BOOL (PASCAL FAR *)(WSAEVENT event))
		GetProcAddress(winSock.hInstance, "WSACloseEvent");
	    winSock.WSAEventSelect = (int (PASCAL FAR *)(SOCKET s,
		    WSAEVENT event,
		    long lEvent))
		GetProcAddress(winSock.hInstance, "WSAEventSelect");
	    winSock.WSAResetEvent = (BOOL (PASCAL FAR *)(WSAEVENT event))
		GetProcAddress(winSock.hInstance, "WSAResetEvent");
	    winSock.WSASetEvent = (BOOL (PASCAL FAR *)(WSAEVENT event))
		GetProcAddress(winSock.hInstance, "WSASetEvent");
	    winSock.WSAEnumNetworkEvents = (int (PASCAL FAR *)(SOCKET s,
		    WSAEVENT event,
		    LPWSANETWORKEVENTS events))
		GetProcAddress(winSock.hInstance, "WSAEnumNetworkEvents");
	    winSock.WSAWaitForMultipleEvents =
		(DWORD (PASCAL FAR *)(DWORD cEvents,
			const WSAEVENT FAR *events, 
			BOOL fWaitAll, DWORD dwTimeOUT, BOOL fAlertable))
		GetProcAddress(winSock.hInstance, "WSAWaitForMultipleEvents");
	}	    
	
	/*
	 * Now check that all fields are properly initialized. If not, return
	 * zero to indicate that we failed to initialize properly.
	 */
    
	if ((winSock.hInstance == NULL) ||
		(winSock.accept == NULL) ||
		(winSock.bind == NULL) ||
		(winSock.closesocket == NULL) ||
		(winSock.connect == NULL) ||
		(winSock.ioctlsocket == NULL) ||
		(winSock.getsockopt == NULL) ||
		(winSock.htons == NULL) ||
		(winSock.inet_addr == NULL) ||
		(winSock.inet_ntoa == NULL) ||
		(winSock.listen == NULL) ||
		(winSock.ntohs == NULL) ||
		(winSock.recv == NULL) ||
		(winSock.select == NULL) ||
		(winSock.send == NULL) ||
		(winSock.setsockopt == NULL) ||
		(winSock.socket == NULL) ||
		(winSock.gethostbyname == NULL) ||
		(winSock.gethostbyaddr == NULL) ||
		(winSock.gethostname == NULL) ||
		(winSock.getpeername == NULL) ||
		(winSock.getservbyname == NULL) ||
		(winSock.getsockname == NULL) ||
		(winSock.WSAStartup == NULL) ||
		(winSock.WSACleanup == NULL) ||
		(winSock.WSAGetLastError == NULL) ||
		(winSock.WSAAsyncSelect == NULL)) {
	    goto unloadLibrary;
	} else if (useThreads &&
		((winSock.WSACreateEvent == NULL) ||
			(winSock.WSACloseEvent == NULL) ||
			(winSock.WSAGetLastError == NULL) ||
			(winSock.WSAEventSelect == NULL) ||
			(winSock.WSAResetEvent == NULL) ||
			(winSock.WSASetEvent == NULL) ||
			(winSock.WSAEnumNetworkEvents == NULL) ||
			(winSock.WSAWaitForMultipleEvents == NULL))) {
	    /*
	     * WinSock 2.0 not correctly installed, use 1.x
	     */
	    
	    useThreads = 0;
	}
	
	/*
	 * Create the async notification window with a new class.  We
	 * must create a new class to avoid a Windows 95 bug that causes
	 * us to get the wrong message number for socket events if the
	 * message window is a subclass of a static control.
	 */
	if (!useThreads) {
	    class.style = 0;
	    class.cbClsExtra = 0;
	    class.cbWndExtra = 0;
	    class.hInstance = TclWinGetTclInstance();
	    class.hbrBackground = NULL;
	    class.lpszMenuName = NULL;
	    class.lpszClassName = "TclSocket";
	    class.lpfnWndProc = SocketProc;
	    class.hIcon = NULL;
	    class.hCursor = NULL;
	    
	    if (!RegisterClassA(&class)) {
		TclWinConvertError(GetLastError());
		(*winSock.WSACleanup)();
		goto unloadLibrary;
	    }
	}
	
	/*
	 * Initialize the winsock library and check the version number.
	 */
	
	if ((*winSock.WSAStartup)(WSA_VERSION_REQD, &wsaData) != 0) {
		goto unloadLibrary;
	}
	if (wsaData.wVersion != WSA_VERSION_REQD) {
	    (*winSock.WSACleanup)();
	    goto unloadLibrary;
	}

    	if (useThreads) {
	    socketEvent = (*winSock.WSACreateEvent)();
	    killEvent = (*winSock.WSACreateEvent)();
	    socketThread = CreateThread(NULL, 8000, SocketThread,
		    NULL, 0, &id);
	    SetThreadPriority(socketThread, THREAD_PRIORITY_HIGHEST);
	}	
    }
    Tcl_MutexUnlock(&socketMutex);

    /*
     * Check for per-thread initialization.
     */

    EnterCriticalSection(&socketCS);
    if (tsdPtr == NULL) {
	tsdPtr = TCL_TSD_INIT(&dataKey);
	tsdPtr->socketList = NULL;
    
	if (useThreads) {
	    tsdPtr->wakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	} else {
	    tsdPtr->hwnd = CreateWindowA("TclSocket", "TclSocket", 
		    WS_TILED, 0, 0, 0, 0, NULL, NULL, class.hInstance, NULL);
	    if (tsdPtr->hwnd == NULL) {
		FreeLibrary(winSock.hInstance);
		winSock.hInstance = NULL;
		LeaveCriticalSection(&socketCS);
		return;
	    }
	}
	    
	Tcl_CreateEventSource(SocketSetupProc, SocketCheckProc, NULL);
	Tcl_CreateThreadExitHandler(SocketThreadExitHandler, NULL);

	tsdPtr->threadId = Tcl_GetCurrentThread();
	tsdPtr->next = firstWaitingThread;
	firstWaitingThread = tsdPtr;

    }
    LeaveCriticalSection(&socketCS);
    return;

unloadLibrary:
    FreeLibrary(winSock.hInstance);
    winSock.hInstance = NULL;
    Tcl_MutexUnlock(&socketMutex);
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketsEnabled --
 *
 *	Check that the WinSock DLL is loaded and ready.
 *
 * Results:
 *	1 if it is.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static int
SocketsEnabled()
{
    int enabled;
    EnterCriticalSection(&socketCS);
    enabled = (winSock.hInstance != NULL);
    LeaveCriticalSection(&socketCS);
    return enabled;
}


/*
 *----------------------------------------------------------------------
 *
 * SocketExitHandler --
 *
 *	Callback invoked during exit clean up to delete the socket
 *	communication window and to release the WinSock DLL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static void
SocketExitHandler(clientData)
    ClientData clientData;              /* Not used. */
{
    EnterCriticalSection(&socketCS);
    if (useThreads && (socketThread != NULL)) {
	LeaveCriticalSection(&socketCS);
	(*winSock.WSASetEvent)(killEvent);
	WaitForSingleObject(socketThread, INFINITE);
	EnterCriticalSection(&socketCS);
	CloseHandle(socketThread);
	(*winSock.WSACloseEvent)(killEvent);
	(*winSock.WSACloseEvent)(socketEvent);
    }
    if (winSock.hInstance) {
	if (!useThreads) {
	    UnregisterClassA("TclSocket", TclWinGetTclInstance());
	}
	(*winSock.WSACleanup)();
	FreeLibrary(winSock.hInstance);
	winSock.hInstance = NULL;
    }
    initialized = 0;
    hostnameInitialized = 0;
    LeaveCriticalSection(&socketCS);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketThreadExitHandler --
 *
 *	Callback invoked during thread clean up to delete the socket
 *	event source.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Delete the event source.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static void
SocketThreadExitHandler(clientData)
    ClientData clientData;              /* Not used. */
{
    ThreadSpecificData *tsdPtr, *nextPtr;

    tsdPtr = (ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    if (useThreads) {
	EnterCriticalSection(&socketCS);
	CloseHandle(tsdPtr->wakeEvent);
	
	if (firstWaitingThread == tsdPtr) {
	    firstWaitingThread = tsdPtr->next;
	} else {
	    for (nextPtr = firstWaitingThread;
		 nextPtr != NULL;
		 nextPtr = nextPtr->next) {
		
		if (nextPtr->next == tsdPtr) {
		    nextPtr->next = tsdPtr->next;
		    break;
		}
	    }
	}
	LeaveCriticalSection(&socketCS);
    } else {
	DestroyWindow(tsdPtr->hwnd);
    }

    Tcl_DeleteEventSource(SocketSetupProc, SocketCheckProc, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpHasSockets --
 *
 *	This function determines whether sockets are available on the
 *	current system and returns an error in interp if they are not.
 *	Note that interp may be NULL.
 *
 * Results:
 *	Returns TCL_OK if the system supports sockets, or TCL_ERROR with
 *	an error in interp.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpHasSockets(interp)
    Tcl_Interp *interp;
{
    InitSockets();

    if (SocketsEnabled()) {
	return TCL_OK;
    }
    if (interp != NULL) {
	Tcl_AppendResult(interp, "sockets are not available on this system",
		NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketSetupProc --
 *
 *	This procedure is invoked before Tcl_DoOneEvent blocks waiting
 *	for an event.
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
SocketSetupProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    SocketInfo *infoPtr;
    Tcl_Time blockTime = { 0, 0 };
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    int readyEvents;
    
    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Check to see if there is a ready socket.  If so, poll.
     */

    EnterCriticalSection(&socketCS);
    for (infoPtr = tsdPtr->socketList; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	readyEvents = GetSocketState(infoPtr);
	if (readyEvents & infoPtr->watchEvents) {
	    Tcl_SetMaxBlockTime(&blockTime);
	    break;
	}
    }
    LeaveCriticalSection(&socketCS);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketCheckProc --
 *
 *	This procedure is called by Tcl_DoOneEvent to check the socket
 *	event source for events. 
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
SocketCheckProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    int readyEvents;
    SocketInfo *infoPtr;
    SocketEvent *evPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Queue events for any ready sockets that don't already have events
     * queued (caused by persistent states that won't generate WinSock
     * events).
     */

    EnterCriticalSection(&socketCS);
    for (infoPtr = tsdPtr->socketList; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	readyEvents = GetSocketState(infoPtr);
	if ((readyEvents & infoPtr->watchEvents)
		&& !(infoPtr->flags & SOCKET_PENDING)) {
	    infoPtr->flags |= SOCKET_PENDING;
	    evPtr = (SocketEvent *) ckalloc(sizeof(SocketEvent));
	    evPtr->header.proc = SocketEventProc;
	    evPtr->socket = infoPtr->socket;
	    LeaveCriticalSection(&socketCS);
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	    EnterCriticalSection(&socketCS);
	}
    }
    LeaveCriticalSection(&socketCS);
}

/*
 *----------------------------------------------------------------------
 *
 * SocketEventProc --
 *
 *	This procedure is called by Tcl_ServiceEvent when a socket event
 *	reaches the front of the event queue.  This procedure is
 *	responsible for notifying the generic channel code.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the channel callback procedures do.
 *
 *----------------------------------------------------------------------
 */

static int
SocketEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SocketInfo *infoPtr;
    SocketEvent *eventPtr = (SocketEvent *) evPtr;
    int mask = 0;
    int events;
    int readyEvents;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Find the specified socket on the socket list.
     */

    EnterCriticalSection(&socketCS);
    for (infoPtr = tsdPtr->socketList; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->socket == eventPtr->socket) {
	    break;
	}
    }
    LeaveCriticalSection(&socketCS);

    /*
     * Discard events that have gone stale.
     */

    if (!infoPtr) {
	return 1;
    }

    infoPtr->flags &= ~SOCKET_PENDING;

    /*
     * Handle connection requests directly.
     */

    readyEvents = GetSocketState(infoPtr);
    if (readyEvents & FD_ACCEPT) {
	TcpAccept(infoPtr);
	return 1;
    }


    /*
     * Mask off unwanted events and compute the read/write mask so 
     * we can notify the channel.
     */

    events = readyEvents & infoPtr->watchEvents;

    if (events & FD_CLOSE) {
	/*
	 * If the socket was closed and the channel is still interested
	 * in read events, then we need to ensure that we keep polling
	 * for this event until someone does something with the channel.
	 * Note that we do this before calling Tcl_NotifyChannel so we don't
	 * have to watch out for the channel being deleted out from under
	 * us.  This may cause a redundant trip through the event loop, but
	 * it's simpler than trying to do unwind protection.
	 */

	Tcl_Time blockTime = { 0, 0 };
	Tcl_SetMaxBlockTime(&blockTime);
	mask |= TCL_READABLE;
    } else if (events & FD_READ) {
	fd_set readFds;
	struct timeval timeout;

	/*
	 * We must check to see if data is really available, since someone
	 * could have consumed the data in the meantime.  Turn off async
	 * notification so select will work correctly.	If the socket is
	 * still readable, notify the channel driver, otherwise reset the
	 * async select handler and keep waiting.
	 */

	if (!useThreads) {
	    (void) (*winSock.WSAAsyncSelect)(infoPtr->socket,
		    tsdPtr->hwnd, 0, 0);
	}
	
	FD_ZERO(&readFds);
	FD_SET(infoPtr->socket, &readFds);
	timeout.tv_usec = 0;
	timeout.tv_sec = 0;
 
	if ((*winSock.select)(0, &readFds, NULL, NULL, &timeout) != 0) {
	    mask |= TCL_READABLE;
	} else if (!useThreads) {
	    (void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		    SOCKET_MESSAGE, infoPtr->selectEvents);
	}
	infoPtr->readyEvents &= ~(FD_READ);
    }
    if (events & (FD_WRITE | FD_CONNECT)) {
	mask |= TCL_WRITABLE;
    }

    if (mask) {
	Tcl_NotifyChannel(infoPtr->channel, mask);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpBlockProc --
 *
 *	Sets a socket into blocking or non-blocking mode.
 *
 * Results:
 *	0 if successful, errno if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpBlockProc(instanceData, mode)
    ClientData	instanceData;	/* The socket to block/un-block. */
    int mode;			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    if (mode == TCL_MODE_NONBLOCKING) {
	infoPtr->flags |= SOCKET_ASYNC;
    } else {
	infoPtr->flags &= ~(SOCKET_ASYNC);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpCloseProc --
 *
 *	This procedure is called by the generic IO level to perform
 *	channel type specific cleanup on a socket based channel
 *	when the channel is closed.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the socket.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static int
TcpCloseProc(instanceData, interp)
    ClientData instanceData;	/* The socket to close. */
    Tcl_Interp *interp;		/* Unused. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    SocketInfo **nextPtrPtr;
    int errorCode = 0;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (SocketsEnabled()) {
        
	/*
         * Clean up the OS socket handle.  The default Windows setting
	 * for a socket is SO_DONTLINGER, which does a graceful shutdown
	 * in the background.
         */
    
        if ((*winSock.closesocket)(infoPtr->socket) == SOCKET_ERROR) {
            TclWinConvertWSAError((*winSock.WSAGetLastError)());
            errorCode = Tcl_GetErrno();
        }
    }

    /*
     * Remove the socket from socketList.
     */

    EnterCriticalSection(&socketCS);
    for (nextPtrPtr = &(tsdPtr->socketList); (*nextPtrPtr) != NULL;
	 nextPtrPtr = &((*nextPtrPtr)->nextPtr)) {
	if ((*nextPtrPtr) == infoPtr) {
	    (*nextPtrPtr) = infoPtr->nextPtr;
	    break;
	}
    }
    LeaveCriticalSection(&socketCS);

    ckfree((char *) infoPtr);
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * NewSocketInfo --
 *
 *	This function allocates and initializes a new SocketInfo
 *	structure.
 *
 * Results:
 *	Returns a newly allocated SocketInfo.
 *
 * Side effects:
 *	Adds the socket to the global socket list.
 *
 *----------------------------------------------------------------------
 */

static SocketInfo *
NewSocketInfo(socket)
    SOCKET socket;
{
    SocketInfo *infoPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    infoPtr = (SocketInfo *) ckalloc((unsigned) sizeof(SocketInfo));
    infoPtr->socket = socket;
    infoPtr->flags = 0;
    infoPtr->watchEvents = 0;
    infoPtr->readyEvents = 0;
    infoPtr->selectEvents = 0;
    infoPtr->acceptProc = NULL;
    infoPtr->lastError = 0;

    EnterCriticalSection(&socketCS);
    infoPtr->nextPtr = tsdPtr->socketList;
    tsdPtr->socketList = infoPtr;
    LeaveCriticalSection(&socketCS);

    return infoPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocket --
 *
 *	This function opens a new socket and initializes the
 *	SocketInfo structure.
 *
 * Results:
 *	Returns a new SocketInfo, or NULL with an error in interp.
 *
 * Side effects:
 *	Adds a new socket to the socketList.
 *
 *----------------------------------------------------------------------
 */

static SocketInfo *
CreateSocket(interp, port, host, server, myaddr, myport, async)
    Tcl_Interp *interp;		/* For error reporting; can be NULL. */
    int port;			/* Port number to open. */
    char *host;			/* Name of host on which to open port. */
    int server;			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    char *myaddr;		/* Optional client-side address */
    int myport;			/* Optional client-side port */
    int async;			/* If nonzero, connect client socket
                                 * asynchronously. */
{
    u_long flag = 1;			/* Indicates nonblocking mode. */
    int asyncConnect = 0;		/* Will be 1 if async connect is
                                         * in progress. */
    struct sockaddr_in sockaddr;	/* Socket address */
    struct sockaddr_in mysockaddr;	/* Socket address for client */
    SOCKET sock;
    SocketInfo *infoPtr;		/* The returned value. */
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
        return NULL;
    }

    if (! CreateSocketAddress(&sockaddr, host, port)) {
	goto error;
    }
    if ((myaddr != NULL || myport != 0) &&
	    ! CreateSocketAddress(&mysockaddr, myaddr, myport)) {
	goto error;
    }

    sock = (*winSock.socket)(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
	goto error;
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation( (HANDLE) sock, HANDLE_FLAG_INHERIT, 0 );
	
    /*
     * Set kernel space buffering
     */

    TclSockMinimumBuffers(sock, TCP_BUFFER_SIZE);

    if (server) {
	/*
	 * Bind to the specified port.  Note that we must not call setsockopt
	 * with SO_REUSEADDR because Microsoft allows addresses to be reused
	 * even if they are still in use.
         *
         * Bind should not be affected by the socket having already been
         * set into nonblocking mode. If there is trouble, this is one place
         * to look for bugs.
	 */
    
	if ((*winSock.bind)(sock, (struct sockaddr *) &sockaddr,
		sizeof(sockaddr)) == SOCKET_ERROR) {
            goto error;
        }

        /*
         * Set the maximum number of pending connect requests to the
         * max value allowed on each platform (Win32 and Win32s may be
         * different, and there may be differences between TCP/IP stacks).
         */
        
	if ((*winSock.listen)(sock, SOMAXCONN) == SOCKET_ERROR) {
	    goto error;
	}

	/*
	 * Add this socket to the global list of sockets.
	 */

	infoPtr = NewSocketInfo(sock);

	/*
	 * Set up the select mask for connection request events.
	 */

	EnterCriticalSection(&socketCS);
	infoPtr->selectEvents = FD_ACCEPT;
	infoPtr->watchEvents |= FD_ACCEPT;
	LeaveCriticalSection(&socketCS);

    } else {

        /*
         * Try to bind to a local port, if specified.
         */
        
	if (myaddr != NULL || myport != 0) { 
	    if ((*winSock.bind)(sock, (struct sockaddr *) &mysockaddr,
		    sizeof(struct sockaddr)) == SOCKET_ERROR) {
		goto error;
	    }
	}            
    
	/*
	 * Set the socket into nonblocking mode if the connect should be
	 * done in the background.
	 */
    
	if (async) {
	    if ((*winSock.ioctlsocket)(sock, FIONBIO, &flag) == SOCKET_ERROR) {
		goto error;
	    }
	}

	/*
	 * Attempt to connect to the remote socket.
	 */

	if ((*winSock.connect)(sock, (struct sockaddr *) &sockaddr,
		sizeof(sockaddr)) == SOCKET_ERROR) {
            TclWinConvertWSAError((*winSock.WSAGetLastError)());
	    if (Tcl_GetErrno() != EWOULDBLOCK) {
		goto error;
	    }

	    /*
	     * The connection is progressing in the background.
	     */

	    asyncConnect = 1;
        }

	/*
	 * Add this socket to the global list of sockets.
	 */

	infoPtr = NewSocketInfo(sock);

	/*
	 * Set up the select mask for read/write events.  If the connect
	 * attempt has not completed, include connect events.
	 */

	EnterCriticalSection(&socketCS);
	infoPtr->selectEvents = FD_READ | FD_WRITE | FD_CLOSE;
	if (asyncConnect) {
	    infoPtr->flags |= SOCKET_ASYNC_CONNECT;
	    infoPtr->selectEvents |= FD_CONNECT;
	}
	LeaveCriticalSection(&socketCS);
    }

    /*
     * Register for interest in events in the select mask.  Note that this
     * automatically places the socket into non-blocking mode.
     */

    if (useThreads) {
	(void) (*winSock.WSAEventSelect)(infoPtr->socket, socketEvent,
		infoPtr->selectEvents);
    } else {
	(*winSock.ioctlsocket)(sock, FIONBIO, &flag);
	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		SOCKET_MESSAGE, infoPtr->selectEvents);
    }
    
    return infoPtr;

error:
    TclWinConvertWSAError((*winSock.WSAGetLastError)());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), (char *) NULL);
    }
    if (sock != INVALID_SOCKET) {
	(*winSock.closesocket)(sock);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a sockaddr structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *sockaddrPtr structure.
 *
 *----------------------------------------------------------------------
 */

static int
CreateSocketAddress(sockaddrPtr, host, port)
    struct sockaddr_in *sockaddrPtr;	/* Socket address */
    char *host;				/* Host.  NULL implies INADDR_ANY */
    int port;				/* Port number */
{
    struct hostent *hostent;		/* Host database entry */
    struct in_addr addr;		/* For 64/32 bit madness */

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
        Tcl_SetErrno(EFAULT);
        return 0;
    }

    (void) memset((char *) sockaddrPtr, '\0', sizeof(struct sockaddr_in));
    sockaddrPtr->sin_family = AF_INET;
    sockaddrPtr->sin_port = (*winSock.htons)((short) (port & 0xFFFF));
    if (host == NULL) {
	addr.s_addr = INADDR_ANY;
    } else {
        addr.s_addr = (*winSock.inet_addr)(host);
        if (addr.s_addr == INADDR_NONE) {
            hostent = (*winSock.gethostbyname)(host);
            if (hostent != NULL) {
                memcpy((char *) &addr,
                        (char *) hostent->h_addr_list[0],
                        (size_t) hostent->h_length);
            } else {
#ifdef	EHOSTUNREACH
                Tcl_SetErrno(EHOSTUNREACH);
#else
#ifdef ENXIO
                Tcl_SetErrno(ENXIO);
#endif
#endif
		return 0;	/* Error. */
	    }
	}
    }

    /*
     * NOTE: On 64 bit machines the assignment below is rumored to not
     * do the right thing. Please report errors related to this if you
     * observe incorrect behavior on 64 bit machines such as DEC Alphas.
     * Should we modify this code to do an explicit memcpy?
     */

    sockaddrPtr->sin_addr.s_addr = addr.s_addr;
    return 1;	/* Success. */
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForSocketEvent --
 *
 *	Waits until one of the specified events occurs on a socket.
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
WaitForSocketEvent(infoPtr, events, errorCodePtr)
    SocketInfo *infoPtr;	/* Information about this socket. */
    int events;			/* Events to look for. */
    int *errorCodePtr;		/* Where to store errors? */
{
    MSG msg;
    int readyEvents;
    int result = 1;
    int oldMode;
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    /*
     * Be sure to disable event servicing so we are truly modal.
     */
    
    oldMode = Tcl_SetServiceMode(TCL_SERVICE_NONE);
	
    if (useThreads) {

	while (1) {
	    readyEvents = GetSocketState(infoPtr);
	    if (infoPtr->lastError) {
		*errorCodePtr = infoPtr->lastError;
		result = 0;
		break;
	    } else if (readyEvents & events) {
		break;
	    } else if (infoPtr->flags & SOCKET_ASYNC) {
		*errorCodePtr = EWOULDBLOCK;
		result = 0;
		break;
	    }
	    WaitForSingleObject(tsdPtr->wakeEvent, 100);
//	    Tcl_ServiceAll();
	}
    } else {
	
	/*
	 * Reset WSAAsyncSelect so we have a fresh set of events pending.
	 */

	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd, 0, 0);
	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		SOCKET_MESSAGE, infoPtr->selectEvents);

	while (1) {
	    /*
	     * Process all outstanding messages on the socket window.
	     */
	    
	    while (PeekMessage(&msg, tsdPtr->hwnd, 0, 0, PM_REMOVE)) {
		DispatchMessage(&msg);
	    }
	    
	    if (infoPtr->lastError) {
		*errorCodePtr = infoPtr->lastError;
		result = 0;
		break;
	    } else if (infoPtr->readyEvents & events) {
		break;
	    } else if (infoPtr->flags & SOCKET_ASYNC) {
		*errorCodePtr = EWOULDBLOCK;
		result = 0;
		break;
	    }
	    
	    /*
	     * Wait until something happens.
	     */
	    
	    WaitMessage();
	}
    }
    (void) Tcl_SetServiceMode(oldMode);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpClient --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpClient(interp, port, host, myaddr, myport, async)
    Tcl_Interp *interp;			/* For error reporting; can be NULL. */
    int port;				/* Port number to open. */
    char *host;				/* Host on which to open port. */
    char *myaddr;			/* Client-side address */
    int myport;				/* Client-side port */
    int async;				/* If nonzero, should connect
                                         * client socket asynchronously. */
{
    SocketInfo *infoPtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    if (TclpHasSockets(interp) != TCL_OK) {
	return NULL;
    }

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateSocket(interp, port, host, 0, myaddr, myport, async);
    if (infoPtr == NULL) {
	return NULL;
    }

    wsprintfA(channelName, "sock%d", infoPtr->socket);

    infoPtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) infoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }
    return infoPtr->channel;
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
 * NOTE: Code contributed by Mark Diekhans (markd@grizzly.com)
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeTcpClientChannel(sock)
    ClientData sock;		/* The socket to wrap up into a channel. */
{
    SocketInfo *infoPtr;
    char channelName[16 + TCL_INTEGER_SPACE];
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    if (TclpHasSockets(NULL) != TCL_OK) {
	return NULL;
    }

    /*
     * Set kernel space buffering and non-blocking.
     */

    TclSockMinimumBuffers((SOCKET) sock, TCP_BUFFER_SIZE);

    infoPtr = NewSocketInfo((SOCKET) sock);

    if (infoPtr == NULL) {
	return NULL;
    }
    
    /*
     * Start watching for read/write events on the socket.
     */

    infoPtr->selectEvents = FD_READ | FD_CLOSE | FD_WRITE;

    if (useThreads) {
	(void) (*winSock.WSAEventSelect)(infoPtr->socket, socketEvent,
		infoPtr->selectEvents);
    } else {
	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		SOCKET_MESSAGE, infoPtr->selectEvents);
    }
    
    wsprintfA(channelName, "sock%d", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) infoPtr, (TCL_READABLE | TCL_WRITABLE));
    Tcl_SetChannelOption(NULL, infoPtr->channel, "-translation", "auto crlf");
    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpServer --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed.  An error message is returned
 *	in the interpreter on failure.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpServer(interp, port, host, acceptProc, acceptProcData)
    Tcl_Interp *interp;			/* For error reporting - may be
                                         * NULL. */
    int port;				/* Port number to open. */
    char *host;				/* Name of local host. */
    Tcl_TcpAcceptProc *acceptProc;	/* Callback for accepting connections
                                         * from new clients. */
    ClientData acceptProcData;		/* Data for the callback. */
{
    SocketInfo *infoPtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    if (TclpHasSockets(interp) != TCL_OK) {
	return NULL;
    }

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateSocket(interp, port, host, 1, NULL, 0, 0);
    if (infoPtr == NULL) {
	return NULL;
    }

    infoPtr->acceptProc = acceptProc;
    infoPtr->acceptProcData = acceptProcData;

    wsprintfA(channelName, "sock%d", infoPtr->socket);

    infoPtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) infoPtr, 0);
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }

    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAccept --
 *	Accept a TCP socket connection.  This is called by
 *	SocketEventProc and it in turns calls the registered accept
 *	procedure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Invokes the accept proc which may invoke arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

static void
TcpAccept(infoPtr)
    SocketInfo *infoPtr;	/* Socket to accept. */
{
    SOCKET newSocket;
    SocketInfo *newInfoPtr;
    struct sockaddr_in addr;
    int len;
    char channelName[16 + TCL_INTEGER_SPACE];
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    /*
     * Accept the incoming connection request.
     */

    len = sizeof(struct sockaddr_in);
    newSocket = (*winSock.accept)(infoPtr->socket, (struct sockaddr *)&addr,
	    &len);

    /*
     * Clear the ready mask so we can detect the next connection request.
     * Note that connection requests are level triggered, so if there is
     * a request already pending, a new event will be generated.
     */

    infoPtr->readyEvents &= ~(FD_ACCEPT);
    
    if (newSocket == INVALID_SOCKET) {
        return;
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation( (HANDLE) newSocket, HANDLE_FLAG_INHERIT, 0 );
	
    /*
     * Add this socket to the global list of sockets.
     */

    newInfoPtr = NewSocketInfo(newSocket);

    /*
     * Select on read/write events and create the channel.
     */

    newInfoPtr->selectEvents = (FD_READ | FD_WRITE | FD_CLOSE);
    if (useThreads) {
	(void) (*winSock.WSAEventSelect)(newInfoPtr->socket, socketEvent, 
		newInfoPtr->selectEvents);
    } else {
	(void) (*winSock.WSAAsyncSelect)(newInfoPtr->socket, tsdPtr->hwnd, 
		SOCKET_MESSAGE, newInfoPtr->selectEvents);
    }
    
    wsprintfA(channelName, "sock%d", newInfoPtr->socket);
    newInfoPtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) newInfoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(NULL, newInfoPtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, newInfoPtr->channel);
        return;
    }
    if (Tcl_SetChannelOption(NULL, newInfoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, newInfoPtr->channel);
        return;
    }

    /*
     * Invoke the accept callback procedure.
     */

    if (infoPtr->acceptProc != NULL) {
	(infoPtr->acceptProc) (infoPtr->acceptProcData, newInfoPtr->channel,
		(*winSock.inet_ntoa)(addr.sin_addr),
		(*winSock.ntohs)(addr.sin_port));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpInputProc --
 *
 *	This procedure is called by the generic IO level to read data from
 *	a socket based channel.
 *
 * Results:
 *	The number of bytes read or -1 on error.
 *
 * Side effects:
 *	Consumes input from the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpInputProc(instanceData, buf, toRead, errorCodePtr)
    ClientData instanceData;		/* The socket state. */
    char *buf;				/* Where to store data. */
    int toRead;				/* Maximum number of bytes to read. */
    int *errorCodePtr;			/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int bytesRead;
    int error;
    int readyEvents;
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);
    
    *errorCodePtr = 0;

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
        *errorCodePtr = EFAULT;
        return -1;
    }

    /*
     * First check to see if EOF was already detected, to prevent
     * calling the socket stack after the first time EOF is detected.
     */

    if (infoPtr->flags & SOCKET_EOF) {
	return 0;
    }

    /*
     * Check to see if the socket is connected before trying to read.
     */
    infoPtr->selectEvents |= FD_READ | FD_CLOSE;

    if (useThreads) {
	(void) (*winSock.WSAEventSelect)(infoPtr->socket, socketEvent,
		infoPtr->selectEvents);
    }	
    
    if ((infoPtr->flags & SOCKET_ASYNC_CONNECT)
	    && ! WaitForSocketEvent(infoPtr,  FD_CONNECT, errorCodePtr)) {
	return -1;
    }
    
    /*
     * No EOF, and it is connected, so try to read more from the socket.
     * Note that we clear the FD_READ bit because read events are level
     * triggered so a new event will be generated if there is still data
     * available to be read.  We have to simulate blocking behavior here
     * since we are always using non-blocking sockets.
     */

    while (1) {

	if (!useThreads) {
	    (void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		    0, 0);
	}
	bytesRead = (*winSock.recv)(infoPtr->socket, buf, toRead, 0);

	infoPtr->readyEvents &= ~(FD_READ);
	
	/*
	 * Check for end-of-file condition or successful read.
	 */
  
	if (bytesRead == 0) {
	    infoPtr->flags |= SOCKET_EOF;
	}
	if (bytesRead != SOCKET_ERROR) {
	    break;
	}
  
	/*
	 * If an error occurs after the FD_CLOSE has arrived,
	 * then ignore the error and report an EOF.
	 */
	readyEvents = GetSocketState(infoPtr);
	
	if (readyEvents & FD_CLOSE) {
	    infoPtr->flags |= SOCKET_EOF;
	    bytesRead = 0;
	    break;
	}
  
	/*
	 * Check for error condition or underflow in non-blocking case.
	 */
  
	error = (*winSock.WSAGetLastError)();
	if ((error != 0) && ((infoPtr->flags & SOCKET_ASYNC)
		|| (error != WSAEWOULDBLOCK))) {
	    TclWinConvertWSAError(error);
	    *errorCodePtr = Tcl_GetErrno();
	    bytesRead = -1;
	    break;
	}

	/*
	 * In the blocking case, wait until the file becomes readable
	 * or closed and try again.
	 */

	if (!WaitForSocketEvent(infoPtr, FD_READ|FD_CLOSE, errorCodePtr)) {
	    bytesRead = -1;
	    break;
  	}
    }

    if (!useThreads) {
	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		SOCKET_MESSAGE, infoPtr->selectEvents);
    }
    return bytesRead;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpOutputProc --
 *
 *	This procedure is called by the generic IO level to write data
 *	to a socket based channel.
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
TcpOutputProc(instanceData, buf, toWrite, errorCodePtr)
    ClientData instanceData;		/* The socket state. */
    char *buf;				/* Where to get data. */
    int toWrite;			/* Maximum number of bytes to write. */
    int *errorCodePtr;			/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int bytesWritten;
    int error;
    ThreadSpecificData *tsdPtr = 
	(ThreadSpecificData *)TclThreadDataKeyGet(&dataKey);

    *errorCodePtr = 0;

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (! SocketsEnabled()) {
        *errorCodePtr = EFAULT;
        return -1;
    }

    /*
     * Check to see if the socket is connected before trying to write.
     */
    
    if ((infoPtr->flags & SOCKET_ASYNC_CONNECT)
	    && ! WaitForSocketEvent(infoPtr,  FD_CONNECT, errorCodePtr)) {
	return -1;
    }

    while (1) {
	if (!useThreads) {
	    (void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		    0, 0);
	}
	
	bytesWritten = (*winSock.send)(infoPtr->socket, buf, toWrite, 0);
	if (bytesWritten != SOCKET_ERROR) {
	    if (!useThreads && (infoPtr->watchEvents & FD_WRITE)) {
		/*
		 * Since Windows won't generate a new write event until we hit
		 * an overflow condition, we need to force the event loop to
		 * poll until the condition changes.
		 */

		Tcl_Time blockTime = { 0, 0 };
		Tcl_SetMaxBlockTime(&blockTime);
	    }
	    break;
	}
	
	/*
	 * Check for error condition or overflow.  In the event of overflow, we
	 * need to clear the FD_WRITE flag so we can detect the next writable
	 * event.  Note that Windows only sends a new writable event after a
	 * send fails with WSAEWOULDBLOCK.
	 */

	error = (*winSock.WSAGetLastError)();

	if (error == WSAEWOULDBLOCK) {
	    infoPtr->readyEvents &= ~(FD_WRITE);
	    if (infoPtr->flags & SOCKET_ASYNC) {
		*errorCodePtr = EWOULDBLOCK;
		bytesWritten = -1;
		break;
	    } 
	} else {
	    TclWinConvertWSAError(error);
	    *errorCodePtr = Tcl_GetErrno();
	    bytesWritten = -1;
	    break;
	}

	/*
	 * In the blocking case, wait until the file becomes writable
	 * or closed and try again.
	 */

	if (!WaitForSocketEvent(infoPtr, FD_WRITE|FD_CLOSE, errorCodePtr)) {
	    bytesWritten = -1;
	    break;
	}
    }

    if (!useThreads) {
	(void) (*winSock.WSAAsyncSelect)(infoPtr->socket, tsdPtr->hwnd,
		SOCKET_MESSAGE, infoPtr->selectEvents);
    }
    return bytesWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetOptionProc --
 *
 *	Computes an option value for a TCP socket based channel, or a
 *	list of all options and their values.
 *
 *	Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *	A standard Tcl result. The value of the specified option or a
 *	list of all options and	their values is returned in the
 *	supplied DString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetOptionProc(instanceData, interp, optionName, dsPtr)
    ClientData instanceData;		/* Socket state. */
    Tcl_Interp *interp;                 /* For error reporting - can be NULL */
    char *optionName;			/* Name of the option to
                                         * retrieve the value for, or
                                         * NULL to get all options and
                                         * their values. */
    Tcl_DString *dsPtr;			/* Where to store the computed
                                         * value; initialized by caller. */
{
    SocketInfo *infoPtr;
    struct sockaddr_in sockname;
    struct sockaddr_in peername;
    struct hostent *hostEntPtr;
    SOCKET sock;
    int size = sizeof(struct sockaddr_in);
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];

    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
	if (interp) {
	    Tcl_AppendResult(interp, "winsock is not initialized", NULL);
	}
        return TCL_ERROR;
    }
    
    infoPtr = (SocketInfo *) instanceData;
    sock = (int) infoPtr->socket;
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if ((len > 1) && (optionName[1] == 'e') &&
	    (strncmp(optionName, "-error", len) == 0)) {
	int optlen;
	int err, ret;
    
	optlen = sizeof(int);
	ret = TclWinGetSockOpt(sock, SOL_SOCKET, SO_ERROR,
		(char *)&err, &optlen);
	if (ret == SOCKET_ERROR) {
	    err = (*winSock.WSAGetLastError)();
	}
	if (err) {
	    TclWinConvertWSAError(err);
	    Tcl_DStringAppend(dsPtr, Tcl_ErrnoMsg(Tcl_GetErrno()), -1);
	}
	return TCL_OK;
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 'p') &&
                    (strncmp(optionName, "-peername", len) == 0))) {
        if ((*winSock.getpeername)(sock, (struct sockaddr *) &peername, &size)
                == 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-peername");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    (*winSock.inet_ntoa)(peername.sin_addr));
            hostEntPtr = (*winSock.gethostbyaddr)(
                (char *) &(peername.sin_addr), sizeof(peername.sin_addr),
                AF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        (*winSock.inet_ntoa)(peername.sin_addr));
            }
	    TclFormatInt(buf, (*winSock.ntohs)(peername.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        } else {
            /*
             * getpeername failed - but if we were asked for all the options
             * (len==0), don't flag an error at that point because it could
             * be an fconfigure request on a server socket. (which have
             * no peer). {copied from unix/tclUnixChan.c}
             */
            if (len) {
		TclWinConvertWSAError((*winSock.WSAGetLastError)());
                if (interp) {
                    Tcl_AppendResult(interp, "can't get peername: ",
                                     Tcl_PosixError(interp),
                                     (char *) NULL);
                }
                return TCL_ERROR;
            }
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if ((*winSock.getsockname)(sock, (struct sockaddr *) &sockname, &size)
                == 0) {
            if (len == 0) {
                Tcl_DStringAppendElement(dsPtr, "-sockname");
                Tcl_DStringStartSublist(dsPtr);
            }
            Tcl_DStringAppendElement(dsPtr,
                    (*winSock.inet_ntoa)(sockname.sin_addr));
            hostEntPtr = (*winSock.gethostbyaddr)(
                (char *) &(sockname.sin_addr), sizeof(peername.sin_addr),
                AF_INET);
            if (hostEntPtr != (struct hostent *) NULL) {
                Tcl_DStringAppendElement(dsPtr, hostEntPtr->h_name);
            } else {
                Tcl_DStringAppendElement(dsPtr,
                        (*winSock.inet_ntoa)(sockname.sin_addr));
            }
            TclFormatInt(buf, (*winSock.ntohs)(sockname.sin_port));
            Tcl_DStringAppendElement(dsPtr, buf);
            if (len == 0) {
                Tcl_DStringEndSublist(dsPtr);
            } else {
                return TCL_OK;
            }
        } else {
	    if (interp) {
		TclWinConvertWSAError((*winSock.WSAGetLastError)());
		Tcl_AppendResult(interp, "can't get sockname: ",
				 Tcl_PosixError(interp),
				 (char *) NULL);
	    }
	    return TCL_ERROR;
	}
    }

    if (len > 0) {
        return Tcl_BadChannelOption(interp, optionName, "peername sockname");
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpWatchProc --
 *
 *	Informs the channel driver of the events that the generic
 *	channel code wishes to receive on this socket.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May cause the notifier to poll if any of the specified 
 *	conditions are already true.
 *
 *----------------------------------------------------------------------
 */

static void
TcpWatchProc(instanceData, mask)
    ClientData instanceData;		/* The socket state. */
    int mask;				/* Events of interest; an OR-ed
                                         * combination of TCL_READABLE,
                                         * TCL_WRITABLE and TCL_EXCEPTION. */
{
    int readyEvents;
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    
    /*
     * Update the watch events mask.
     */

    EnterCriticalSection(&socketCS);
    infoPtr->watchEvents = 0;
    if (mask & TCL_READABLE) {
	infoPtr->watchEvents |= (FD_READ|FD_CLOSE|FD_ACCEPT);
    }
    if (mask & TCL_WRITABLE) {
	infoPtr->watchEvents |= (FD_WRITE|FD_CONNECT);
    }
    LeaveCriticalSection(&socketCS);

    /*
     * If there are any conditions already set, then tell the notifier to poll
     * rather than block.
     */

    readyEvents = GetSocketState(infoPtr);
    if (readyEvents & infoPtr->watchEvents) {
	Tcl_Time blockTime = { 0, 0 };
	Tcl_SetMaxBlockTime(&blockTime);
    }		
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve an OS handle from inside
 *	a TCP socket based channel.
 *
 * Results:
 *	Returns TCL_OK with the socket in handlePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetHandleProc(instanceData, direction, handlePtr)
    ClientData instanceData;	/* The socket state. */
    int direction;		/* Not used. */
    ClientData *handlePtr;	/* Where to store the handle.  */
{
    SocketInfo *statePtr = (SocketInfo *) instanceData;

    *handlePtr = (ClientData) statePtr->socket;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketThread --
 *
 *
 * Results:
 *	0 on success.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
SocketThread(LPVOID arg)
{
    SocketInfo *infoPtr;
    WSAEVENT events[2];
    ThreadSpecificData *tsdPtr;
    DWORD result;
    
    /*
     * Find the specified socket on the socket list and update its
     * eventState flag.
     */

    events[0] = killEvent;
    events[1] = socketEvent;
    
    while (1) {

	result = (*winSock.WSAWaitForMultipleEvents)(2, events, FALSE,
		WSA_INFINITE, FALSE);

	switch (result) {
	    case (WSA_WAIT_EVENT_0 +1):
		/*
		 * A socket event has fired, determine which socket(s) it was
		 * and which thread(s) it came from.
		 */

		EnterCriticalSection(&socketCS);
		for (tsdPtr = firstWaitingThread; tsdPtr != NULL;
		     tsdPtr = tsdPtr->next) {
		    
		    for (infoPtr = tsdPtr->socketList; infoPtr != NULL; 
			 infoPtr = infoPtr->nextPtr) {
			
			if (GetSocketState(infoPtr) & infoPtr->watchEvents) {
			    Tcl_ThreadAlert(tsdPtr->threadId);
			    SetEvent(tsdPtr->wakeEvent);

			    /*
			     * Only process one at a time for a given thread,
			     * so break here.
			     */
			    
			    break;
			}
		    }
		}
		LeaveCriticalSection(&socketCS);
		(*winSock.WSAResetEvent)(socketEvent);
		break;

	    case WSA_WAIT_EVENT_0:
		/*
		 * This thread is supposed to kill itself off
		 */

		return 0;
		break;
		
	    default:
		/*
		 * The thread has detected a fatal error
		 */
		
		return 1;
		break;
	}
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SocketProc --
 *
 *	This function is called when WSAAsyncSelect has been used
 *	to register interest in a socket event, and the event has
 *	occurred.
 *
 * Results:
 *	0 on success.
 *
 * Side effects:
 *	The flags for the given socket are updated to reflect the
 *	event that occured.
 *
 *----------------------------------------------------------------------
 */

static LRESULT CALLBACK
SocketProc(hwnd, message, wParam, lParam)
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
{
    int event, error;
    SOCKET socket;
    SocketInfo *infoPtr;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (message != SOCKET_MESSAGE) {
	return DefWindowProc(hwnd, message, wParam, lParam);
    }

    event = WSAGETSELECTEVENT(lParam);
    error = WSAGETSELECTERROR(lParam);
    socket = (SOCKET) wParam;

    /*
     * Find the specified socket on the socket list and update its
     * eventState flag.
     */

    for (infoPtr = tsdPtr->socketList; infoPtr != NULL; 
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->socket == socket) {
	    /*
	     * Update the socket state.
	     */

	    if (event & FD_CLOSE) {
		infoPtr->readyEvents &= ~(FD_WRITE|FD_ACCEPT);
	    }
	    if (event & FD_CONNECT) {
		/*
		 * The socket is now connected, so clear the async connect
		 * flag.
		 */

		infoPtr->flags &= ~(SOCKET_ASYNC_CONNECT);

		/*
		 * Remember any error that occurred so we can report
		 * connection failures.
		 */

		if (error != ERROR_SUCCESS) {
		    TclWinConvertWSAError(error);
		    infoPtr->lastError = Tcl_GetErrno();
		}

	    } 
	    if(infoPtr->flags & SOCKET_ASYNC_CONNECT) {
		infoPtr->flags &= ~(SOCKET_ASYNC_CONNECT);
		if (error != ERROR_SUCCESS) {
		    TclWinConvertWSAError(error);
		    infoPtr->lastError = Tcl_GetErrno();
		}
		infoPtr->readyEvents |= FD_WRITE;
	    }
	    infoPtr->readyEvents |= event;
	    break;
	}
    }

    /*
     * Flush the Tcl event queue before returning to the event loop.
     */

    Tcl_ServiceAll();

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Returns the name of the local host.
 *
 * Results:
 *	A string containing the network name for this machine, or
 *	an empty string if we can't figure out the name.  The caller 
 *	must not modify or free this string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetHostName()
{
    DWORD length;
    WCHAR wbuf[MAX_COMPUTERNAME_LENGTH + 1];

    Tcl_MutexLock(&socketMutex);
    if (hostnameInitialized) {
	Tcl_MutexUnlock(&socketMutex);
        return hostname;
    }

    if (TclpHasSockets(NULL) == TCL_OK) {
	/*
	 * INTL: bug
	 */

	if ((*winSock.gethostname)(hostname, sizeof(hostname)) == 0) {
	    hostnameInitialized = 1;
	    Tcl_MutexUnlock(&socketMutex);
	    return hostname;
	}
    }
    length = sizeof(hostname);
    if ((*tclWinProcs->getComputerNameProc)(wbuf, &length) != 0) {
	/*
	 * Convert string from native to UTF then change to lowercase.
	 */

	Tcl_DString ds;

	lstrcpynA(hostname, Tcl_WinTCharToUtf((TCHAR *) wbuf, -1, &ds),
		sizeof(hostname));
	Tcl_DStringFree(&ds);
	Tcl_UtfToLower(hostname);
    } else {
	hostname[0] = '\0';
    }
    hostnameInitialized = 1;
    Tcl_MutexUnlock(&socketMutex);
    return hostname;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinGetSockOpt, et al. --
 *
 *	These functions are wrappers that let us bind the WinSock
 *	API dynamically so we can run on systems that don't have
 *	the wsock32.dll.  We need wrappers for these interfaces
 *	because they are called from the generic Tcl code.
 *
 * Results:
 *	As defined for each function.
 *
 * Side effects:
 *	As defined for each function.
 *
 *----------------------------------------------------------------------
 */

int
TclWinGetSockOpt(SOCKET s, int level, int optname, char * optval,
	int FAR *optlen)
{
    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
        return SOCKET_ERROR;
    }
    
    return (*winSock.getsockopt)(s, level, optname, optval, optlen);
}

int
TclWinSetSockOpt(SOCKET s, int level, int optname, const char * optval,
	int optlen)
{
    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */
    if (!SocketsEnabled()) {
        return SOCKET_ERROR;
    }

    return (*winSock.setsockopt)(s, level, optname, optval, optlen);
}

u_short
TclWinNToHS(u_short netshort)
{
    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */

    if (!SocketsEnabled()) {
        return (u_short) -1;
    }

    return (*winSock.ntohs)(netshort);
}

struct servent *
TclWinGetServByName(const char * name, const char * proto)
{
    /*
     * Check that WinSock is initialized; do not call it if not, to
     * prevent system crashes. This can happen at exit time if the exit
     * handler for WinSock ran before other exit handlers that want to
     * use sockets.
     */
    if (!SocketsEnabled()) {
        return (struct servent *) NULL;
    }

    return (*winSock.getservbyname)(name, proto);
}
