/*
 * tclWinSockCore.c --
 *
 *	This file contains Windows-specific and protocol agnostic
 *	socket related code.  The default method uses overlapped-I/O
 *	with completion port notification.
 *
 *	No fallback exists, yet, to support non-NT based systems.
 *
 * Copyright (c) 2008 David Gravereaux <davygrvy@pobox.com>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinsockCore.c,v 1.1.2.2 2008/12/06 03:39:00 davygrvy Exp $
 */

#include "tclWinInt.h"
#include "tclWinsockCore.h"

#ifdef _MSC_VER
#   pragma comment (lib, "ws2_32")
#endif

/*
 * Support for control over sockets' KEEPALIVE and NODELAY behavior is
 * currently disabled.
 */

#undef TCL_FEATURE_KEEPALIVE_NAGLE

/*
 * The following declare the winsock loading error codes.
 */

#define TCL_WSE_NOTFOUND		1
#define TCL_WSE_CANTDOONEPOINTZERO	2
#define TCL_WSE_NOTALL11FUNCS		3
#define TCL_WSE_NOTALL2XFUNCS		4
#define TCL_WSE_CANTSTARTHANDLERTHREAD	5
static DWORD winsockLoadErr	= 0;

/* some globals defined. */
int initialized			= 0;

/*
 * Only one subsystem is ever created, but we group it within a
 * struct just to be organized.
 */
typedef struct CompletionPortInfo {
    HANDLE port;	    /* The completion port handle. */
    HANDLE heap;	    /* General private heap used for objects 
			     * that don't need to interact with Tcl
			     * directly. */
    HANDLE NPPheap;	    /* Special private heap for all data that
			     * will be set with special attributes for
			     * the non-paged pool (WSABUF and
			     * OVERLAPPED) */
    HANDLE thread;	    /* The single thread for handling the
			     * completion routine for the entire
			     * process. */
} CompletionPortInfo;

CompletionPortInfo IocpSubSystem;

/* Stats being collected */
LONG StatOpenSockets		= 0;
LONG StatFailedAcceptExCalls	= 0;
LONG StatGeneralBytesInUse	= 0;
LONG StatSpecialBytesInUse	= 0;
LONG StatFailedReplacementAcceptExCalls	= 0;

/* file-scope globals */
GUID AcceptExGuid		= WSAID_ACCEPTEX;
GUID GetAcceptExSockaddrsGuid	= WSAID_GETACCEPTEXSOCKADDRS;
GUID ConnectExGuid		= WSAID_CONNECTEX;
GUID DisconnectExGuid		= WSAID_DISCONNECTEX;
GUID TransmitFileGuid		= WSAID_TRANSMITFILE;
GUID TransmitPacketsGuid	= WSAID_TRANSMITPACKETS;
GUID WSARecvMsgGuid		= WSAID_WSARECVMSG;
Tcl_ThreadDataKey dataKey;

/* local prototypes */
static DWORD			InitializeIocpSubSystem();
static Tcl_ExitProc		IocpExitHandler;
static Tcl_ExitProc		IocpThreadExitHandler;
static Tcl_EventSetupProc	IocpEventSetupProc;
static Tcl_EventCheckProc	IocpEventCheckProc;
static Tcl_EventProc		IocpEventProc;
static Tcl_EventDeleteProc	IocpRemovePendingEvents;
static Tcl_EventDeleteProc	IocpRemoveAllPendingEvents;

static Tcl_DriverCloseProc	IocpCloseProc;
static Tcl_DriverInputProc	IocpInputProc;
static Tcl_DriverOutputProc	IocpOutputProc;
static Tcl_DriverSetOptionProc	IocpSetOptionProc;
static Tcl_DriverGetOptionProc	IocpGetOptionProc;
static Tcl_DriverWatchProc	IocpWatchProc;
static Tcl_DriverGetHandleProc	IocpGetHandleProc;
static Tcl_DriverBlockModeProc	IocpBlockProc;
static Tcl_DriverThreadActionProc IocpThreadActionProc;

static void		IocpZapTclNotifier (SocketInfo *infoPtr);
static void		IocpAlertToTclNewAccept (SocketInfo *infoPtr,
			    SocketInfo *newClient);
static void		IocpAcceptOne (SocketInfo *infoPtr);
static void		IocpPushRecvAlertToTcl(SocketInfo *infoPtr,
			    BufferInfo *bufPtr);
static DWORD		PostOverlappedSend (SocketInfo *infoPtr,
			    BufferInfo *bufPtr);
static DWORD		PostOverlappedDisconnect (SocketInfo *infoPtr,
			    BufferInfo *bufPtr);
static DWORD WINAPI	CompletionThreadProc (LPVOID lpParam);
static void		HandleIo(register SocketInfo *infoPtr,
			    register BufferInfo *bufPtr,
			    HANDLE compPort, DWORD bytes, DWORD error,
			    DWORD flags);
static void		RepostRecvs (SocketInfo *infoPtr,
			    int chanBufSize);
static int		FilterSingleOpRecvBuf (SocketInfo *infoPtr,
			    BufferInfo *bufPtr, int bytesRead);
static int		FilterPartialRecvBufMerge (SocketInfo *infoPtr,
			    BufferInfo *bufPtr, int *bytesRead,
			    int toRead, char *bufPos);
static int		DoRecvBufMerge (SocketInfo *infoPtr,
			    BufferInfo *bufPtr, int *bytesRead,
			    int toRead, char **bufPos, int *gotError);


/* special hack jobs! */
static BOOL PASCAL	OurConnectEx(SOCKET s,
			    const struct sockaddr* name, int namelen,
			    PVOID lpSendBuffer, DWORD dwSendDataLength,
			    LPDWORD lpdwBytesSent,
			    LPOVERLAPPED lpOverlapped);
static BOOL PASCAL	OurDisconnectEx(SOCKET hSocket,
			    LPOVERLAPPED lpOverlapped, DWORD dwFlags,
			    DWORD reserved);

/*
 * This structure describes the channel type structure for TCP socket
 * based I/O using the native overlapped interface along with completion
 * ports for maximum efficiency so that most operation are done entirely
 * in kernel-mode.
 */

Tcl_ChannelType IocpChannelType = {
    "sock",		    /* Type name. */
    TCL_CHANNEL_VERSION_5,
    IocpCloseProc,	    /* Close proc. */
    IocpInputProc,	    /* Input proc. */
    IocpOutputProc,	    /* Output proc. */
    NULL,		    /* Seek proc. */
    IocpSetOptionProc,	    /* Set option proc. */
    IocpGetOptionProc,	    /* Get option proc. */
    IocpWatchProc,	    /* Set up notifier to watch this channel. */
    IocpGetHandleProc,	    /* Get an OS handle from channel. */
    NULL,		    /* close2proc. */
    IocpBlockProc,	    /* Set socket into (non-)blocking mode. */
    NULL,		    /* flush proc. */
    NULL,		    /* handler proc. */
    NULL,		    /* wide seek */
    IocpThreadActionProc,   /* TIP #218. */
    NULL		    /* truncate */
};


typedef struct SocketEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    SocketInfo *infoPtr;
} SocketEvent;


typedef Tcl_Obj * (FN_DECODEADDR) (SocketInfo *info, LPSOCKADDR addr);


/*
 * Specific protocol information is stored here and shared to all
 * SocketInfo objects of that type.
 */
typedef struct WS2ProtocolData {
    int af;		    /* Address family. */
    int	type;		    /* Address type. */
    int	protocol;	    /* protocol type. */
    size_t addrLen;	    /* length of protocol specific SOCKADDR */
    FN_DECODEADDR		*DecodeSockAddr;

    /* LSP specific extension functions */
    LPFN_ACCEPTEX		_AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS	_GetAcceptExSockaddrs;
    LPFN_CONNECTEX		_ConnectEx;
    LPFN_DISCONNECTEX		_DisconnectEx;
    LPFN_TRANSMITFILE		_TransmitFile;
    /* The only caveat of using this TransmitFile extension API is that
       on Windows NT Workstation or Windows 2000 Professional only two
       requests will be processed at a time. You must be running on
       Windows NT or Windows 2000 Server, Windows 2000 Advanced Server,
       or Windows 2000 Data Center to get full usage of this specialized
       API. */
    LPFN_TRANSMITPACKETS	_TransmitPackets;
    LPFN_WSARECVMSG		_WSARecvMsg;
} WS2ProtocolData;

/*
 * The following structure is used to store the data associated with
 * each socket.
 */

#define IOCP_EOF	    (1<<0)
#define IOCP_CLOSING	    (1<<1)
#define IOCP_ASYNC	    (1<<2)
#define IOCP_CLOSABLE	    (1<<3)

enum IocpRecvMode {
    IOCP_RECVMODE_ZERO_BYTE,
    IOCP_RECVMODE_FLOW_CTRL,
    IOCP_RECVMODE_BURST_DETECT
};

/*
 * This is our per I/O buffer. It contains a WSAOVERLAPPED structure as well
 * as other necessary information for handling an IO operation on a socket.
 */

typedef struct _BufferInfo {
    WSAOVERLAPPED ol;
    struct SocketInfo *parent;
    SOCKET socket;	    /* Used for AcceptEx client socket */
    DWORD WSAerr;	    /* Any error that occured for this operation. */
    BYTE *buf;		    /* Buffer for recv/send/AcceptEx */
    BYTE *last;		    /* buffer position if last read operation was a partial copy */
    SIZE_T buflen;	    /* Length of the buffer */
    SIZE_T used;	    /* Length of the buffer used (after operation) */
#   define OP_ACCEPT	0   /* AcceptEx() */
#   define OP_READ	1   /* WSARecv()/WSARecvFrom() */
#   define OP_WRITE	2   /* WSASend()/WSASendTo() */
#   define OP_CONNECT	3   /* ConnectEx() */
#   define OP_DISCONNECT 4  /* DisconnectEx() */
#   define OP_QOS	5   /* Quality of Service notices. */
#   define OP_TRANSMIT	6   /* TransmitFile() */
#   define OP_IOCTL	7   /* WSAIoctl() */
#   define OP_LOOKUP	8   /* TODO: For future use, WSANSIoctl()?? */
    int operation;	    /* Type of operation issued */
    LPSOCKADDR addr;	    /* addr storage space for WSARecvFrom/WSASendTo. */
    LLNODE node;	    /* linked list node */
} BufferInfo;

typedef struct ThreadSpecificData {
    Tcl_ThreadId threadId;
    LPLLIST readySockets;
    LPLLIST deadSockets;
} ThreadSpecificData;


/* =================================================================== */
/* ============= Initailization and shutdown procedures ============== */



ThreadSpecificData *
InitSockets(void)
{
    WSADATA wsaData;
    OSVERSIONINFO os;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    /* global/once init */
    if (!initialized) {
	initialized = 1;

#ifdef STATIC_BUILD
	iocpModule = TclWinGetTclInstance();
#endif

	/*
	 * Initialize the winsock library and check the interface
	 * version number.  We ask for the 2.2 interface, but
	 * don't accept less than 1.1.
	 */

#define WSA_VER_MIN_MAJOR   1
#define WSA_VER_MIN_MINOR   1
#define WSA_VERSION_REQUESTED    MAKEWORD(2,2)

	if ((winsockLoadErr = WSAStartup(WSA_VERSION_REQUESTED,
		&wsaData)) != 0) {
	    goto unloadLibrary;
	}

//	wVersionLoaded = wsaData.wVersion;

	/*
	 * Note the byte positions are swapped for the comparison, so
	 * that 0x0002 (2.0, MAKEWORD(2,0)) doesn't look less than 0x0101
	 * (1.1).  We want the comparison to be 0x0200 < 0x0101.
	 */
	if (MAKEWORD(HIBYTE(wsaData.wVersion), LOBYTE(wsaData.wVersion))
		< MAKEWORD(WSA_VER_MIN_MINOR, WSA_VER_MIN_MAJOR)) {
	    winsockLoadErr = TCL_WSE_CANTDOONEPOINTZERO;
	    WSACleanup();
	    goto unloadLibrary;
	}

#undef WSA_VERSION_REQUESTED
#undef WSA_VER_MIN_MAJOR
#undef WSA_VER_MIN_MINOR


	/*
	 * Assert our Tcl_ChannelType struct to the true version this core
	 * can accept, but not above the version of our design.
	 */

	IocpChannelType.version =
		IocpGetTclMaxChannelVer(IocpChannelType.version);

	switch ((int)IocpChannelType.version) {
	    case TCL_CHANNEL_VERSION_1:
		/* Oldest Tcl_ChannelType struct. */
		IocpChannelType.version =
			(Tcl_ChannelTypeVersion) IocpBlockProc;
		break;
	    case TCL_CHANNEL_VERSION_3:
		/* We don't have a wideseekProc, so back down one more
		 * to v2. */
		IocpChannelType.version = TCL_CHANNEL_VERSION_2;
		break;
	    default:
		/* No other Tcl_ChannelType struct maniputions are known at
		 * this time. */
		break;
	}

	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&os);

	if (InitializeIocpSubSystem() != NO_ERROR) {
	    goto unloadLibrary;
	}
    }

    /* per thread init */
    if (tsdPtr->threadId == 0) {
	Tcl_CreateEventSource(IocpEventSetupProc, IocpEventCheckProc, NULL);
	Tcl_CreateThreadExitHandler(IocpThreadExitHandler, tsdPtr);
	tsdPtr->threadId = Tcl_GetCurrentThread();
	tsdPtr->readySockets = IocpLLCreate();
    }

    return tsdPtr;

unloadLibrary:
    initialized = 0;
    return NULL;
}

Tcl_ChannelTypeVersion
IocpGetTclMaxChannelVer (Tcl_ChannelTypeVersion maxAllowed)
{
    Tcl_ChannelType fake;

    if (maxAllowed == (Tcl_ChannelTypeVersion) 0x1) {
	return maxAllowed;
    }

    /*
     * Stubs slot empty.  Must be TCL_CHANNEL_VERSION_1
     */
    if (Tcl_ChannelVersion == NULL) {
	return TCL_CHANNEL_VERSION_1;
    }
    
    /* Tcl_ChannelVersion only touches the ->version field. */
    fake.version = maxAllowed;

    while (Tcl_ChannelVersion(&fake) == TCL_CHANNEL_VERSION_1) {
	fake.version = (Tcl_ChannelTypeVersion)((int)fake.version - 1);
	if (fake.version == (Tcl_ChannelTypeVersion) 0x1) break;
    }
    return fake.version;
}

int
HasSockets(Tcl_Interp *interp)
{
    ThreadSpecificData *blob;

    blob = InitSockets();

    if (blob != NULL) {
	return TCL_OK;
    }
    if (interp != NULL) {
	switch (winsockLoadErr) {
	    case TCL_WSE_NOTFOUND:
		Tcl_AppendResult(interp,
			"Windows Sockets are not available on this "
			"system.  A suitable winsock DLL could not be "
			"located.", NULL);
		break;
	    case TCL_WSE_CANTDOONEPOINTZERO:
		Tcl_AppendResult(interp,
			"The Windows Sockets version loaded by "
			"WSAStartup is under 1.1 and is not "
			"accepatable.", NULL);
		break;
	    case TCL_WSE_NOTALL11FUNCS:
		Tcl_AppendResult(interp,
			"The Windows Sockets library didn't have all the"
			" needed exports for the 1.1 interface.", NULL);
		break;
	    case TCL_WSE_NOTALL2XFUNCS:
		Tcl_AppendResult(interp,
			"The Windows Sockets library didn't have all the"
			" needed exports for the 2.x interface.", NULL);
		break;
	    case TCL_WSE_CANTSTARTHANDLERTHREAD:
		Tcl_AppendResult(interp,
			"The worker thread to service the completion port "
			"was unable to start.", NULL);
		break;
	    default:
		SetLastError(winsockLoadErr);
		Tcl_AppendResult(interp, "can't start Iocpsock: ",
			Tcl_WinError(interp), NULL);
		break;
	}
    }
    return TCL_ERROR;
}

static DWORD
InitializeIocpSubSystem ()
{
#define IOCP_HEAP_START_SIZE	(si.dwPageSize*64)  /* about 256k */
    DWORD error = NO_ERROR;
    SYSTEM_INFO si;

    GetSystemInfo(&si);

    /* Create the completion port. */
    IocpSubSystem.port = CreateIoCompletionPort(
	    INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)NULL, 0);
    if (IocpSubSystem.port == NULL) {
	error = GetLastError();
	goto done;
    }

    /* Create the general private memory heap. */
    IocpSubSystem.heap = HeapCreate(0, IOCP_HEAP_START_SIZE, 0);
    if (IocpSubSystem.heap == NULL) {
	error = GetLastError();
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    /* Create the special private memory heap. */
    IocpSubSystem.NPPheap = HeapCreate(0, IOCP_HEAP_START_SIZE, 0);
    if (IocpSubSystem.NPPheap == NULL) {
	error = GetLastError();
	HeapDestroy(IocpSubSystem.heap);
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    /* Create the thread to service the completion port. */
    IocpSubSystem.thread = CreateThread(NULL, 0, CompletionThreadProc,
	    &IocpSubSystem, 0, NULL);
    if (IocpSubSystem.thread == NULL) {
	error = TCL_WSE_CANTSTARTHANDLERTHREAD;
	HeapDestroy(IocpSubSystem.heap);
	HeapDestroy(IocpSubSystem.NPPheap);
	CloseHandle(IocpSubSystem.port);
	goto done;
    }

    Tcl_CreateExitHandler(IocpExitHandler, NULL);

done:
    return error;
#undef IOCP_HEAP_START_SIZE
}

void
IocpExitHandler (ClientData clientData)
{
    DWORD wait;

    if (initialized) {

	Tcl_DeleteEvents(IocpRemoveAllPendingEvents, NULL);

	/* Cause the waiting I/O handler threads to exit. */
	PostQueuedCompletionStatus(IocpSubSystem.port, 0, 0, 0);

	/* Wait for our completion thread to exit. */
	wait = WaitForSingleObject(IocpSubSystem.thread, 400);
	if (wait == WAIT_TIMEOUT) {
	    TerminateThread(IocpSubSystem.thread, 0x666);
	}
	CloseHandle(IocpSubSystem.thread);

	/* Close the completion port object. */
	CloseHandle(IocpSubSystem.port);

	/* Tear down the private memory heaps. */
	HeapDestroy(IocpSubSystem.heap);
	HeapDestroy(IocpSubSystem.NPPheap);

	initialized = 0;
	WSACleanup();
    }
}

void
IocpThreadExitHandler (ClientData clientData)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) clientData;

    Tcl_DeleteEventSource(IocpEventSetupProc, IocpEventCheckProc, NULL);
    if (initialized) {
	IocpLLPopAll(tsdPtr->readySockets, NULL, IOCP_LL_NODESTROY);
	IocpLLDestroy(tsdPtr->readySockets);
	tsdPtr->readySockets = NULL;
    }
}


/* =================================================================== */
/* ==================== Tcl_Event*Proc procedures ==================== */


/*
 *-----------------------------------------------------------------------
 * IocpEventSetupProc --
 *
 *  Happens before the event loop is to wait in the notifier.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpEventSetupProc (
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = InitSockets();
    Tcl_Time blockTime = {0, 0};

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }

    /*
     * If any ready events exist now, don't let the notifier go into it's
     * wait state.  This function call is very inexpensive.
     */

    if (IocpLLIsNotEmpty(tsdPtr->readySockets) ||
		0 /*TODO: IocpLLIsNotEmpty(tsdPtr->deadSockets)*/) {
	Tcl_SetMaxBlockTime(&blockTime);
    }
}

/*
 *-----------------------------------------------------------------------
 * IocpEventCheckProc --
 *
 *  Happens after the notifier has waited.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpEventCheckProc (
    ClientData clientData,
    int flags)
{
    ThreadSpecificData *tsdPtr = InitSockets();
    SocketInfo *infoPtr;
    SocketEvent *evPtr;
    int evCount;

    if (!(flags & TCL_FILE_EVENTS)) {
	/* Don't be greedy. */
	return;
    }

    /*
     * Sockets that are EOF, but not yet closed, are considered readable.
     * Because Tcl historically requires that EOF channels shall still
     * fire readable and writable events until closed and our alert
     * semantics are such that we'll never get repeat notifications after
     * EOF, we place this poll condition here.
     */

    /* TODO: evCount = IocpLLGetCount(tsdPtr->deadSockets); */

    /*
     * Do we have any jobs to queue?  Take a snapshot of the count as
     * of now.
     */

    evCount = IocpLLGetCount(tsdPtr->readySockets);

    while (evCount--) {
	EnterCriticalSection(&tsdPtr->readySockets->lock);
	infoPtr = IocpLLPopFront(tsdPtr->readySockets,
		IOCP_LL_NOLOCK | IOCP_LL_NODESTROY, 0);
	/*
	 * Flop the markedReady toggle.  This is used to improve event
	 * loop efficiency to avoid unneccesary events being queued into
	 * the readySockets list.
	 */
	if (infoPtr) InterlockedExchange(&infoPtr->markedReady, 0);
	LeaveCriticalSection(&tsdPtr->readySockets->lock);

	/*
	 * Safety check. Somehow the count of what is and what actually
	 * is, is less (!?)..  whatever...  
	 */
	if (!infoPtr) continue;

	/*
	 * The socket isn't ready to be serviced.  accept() in the Tcl
	 * layer hasn't happened yet while reads on the new socket are
	 * coming in or the socket is in the middle of doing an async
	 * close.
	 */
	if (infoPtr->channel == NULL) {
	    continue;
	}

	evPtr = (SocketEvent *) ckalloc(sizeof(SocketEvent));
	evPtr->header.proc = IocpEventProc;
	evPtr->infoPtr = infoPtr;
	Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
    }
}

/*
 *-----------------------------------------------------------------------
 * IocpEventProc --
 *
 *  Tcl's event loop is now servicing this.
 *
 *-----------------------------------------------------------------------
 */
static int
IocpEventProc (
    Tcl_Event *evPtr,		/* Event to service. */
    int flags)			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    SocketInfo *infoPtr = ((SocketEvent *)evPtr)->infoPtr;
    int readyMask = 0;

    if (!(flags & TCL_FILE_EVENTS)) {
	/* Don't be greedy. */
	return 0;
    }

    /*
     * If an accept is ready, pop one only.  There might be more,
     * but this would be greedy with regards to the event loop.
     */
    if (infoPtr->readyAccepts != NULL) {
	IocpAcceptOne(infoPtr);
	return 1;
    }

    /*
     * If there is at least one entry on the infoPtr->llPendingRecv list,
     * and the watch mask is set to notify for readable events, the channel
     * is readable.
     */
    if (infoPtr->watchMask & TCL_READABLE &&
	    IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	readyMask |= TCL_READABLE;
    }

    /*
     * If the watch mask is set to notify for writable events, and
     * outstanding sends are less than the resource cap allowed for
     * this socket, the channel is writable.
     */
    if (infoPtr->watchMask & TCL_WRITABLE && infoPtr->llPendingRecv
	    && infoPtr->outstandingSends < infoPtr->outstandingSendCap) {
	readyMask |= TCL_WRITABLE;
    }

    if (readyMask) {
	Tcl_NotifyChannel(infoPtr->channel, readyMask);
    } else {
	/* This was a useless queue.  I want to know why! */
	__asm nop;
    }
    return 1;
}

/*
 *-----------------------------------------------------------------------
 * IocpAcceptOne --
 *
 *  Accept one connection from the listening socket.  Repost to the
 *  readySockets list if more are available.  By doing it this way,
 *  incoming connections aren't greedy.
 *
 *-----------------------------------------------------------------------
 */
static void
IocpAcceptOne (SocketInfo *infoPtr)
{
    char channelName[4 + TCL_INTEGER_SPACE];
    AcceptInfo *acptInfo;
    int objc, port;
    Tcl_Obj **objv, *AddrInfo;

    acptInfo = IocpLLPopFront(infoPtr->readyAccepts, IOCP_LL_NODESTROY, 0);

    if (acptInfo == NULL) {
	/* Don't barf if the counts don't match. */
	return;
    }

    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "iocp%lu", acptInfo->clientInfo->socket);
    acptInfo->clientInfo->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
	    (ClientData) acptInfo->clientInfo, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	goto error;
    }
    if (Tcl_SetChannelOption(NULL, acptInfo->clientInfo->channel, "-eofchar", "")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, acptInfo->clientInfo->channel);
	goto error;
    }

    /*
     * Invoke the accept callback procedure.
     *
     * TODO: move this to the protocol specific files so the type cast,
     * conversion and invokation signature are proper for the address
     * type.
     */

    AddrInfo = acptInfo->clientInfo->proto->DecodeSockAddr(
	    acptInfo->clientInfo, acptInfo->clientInfo->remoteAddr);
    Tcl_ListObjGetElements(NULL, AddrInfo, &objc, &objv);
    Tcl_GetIntFromObj(NULL, objv[2], &port);
    if (infoPtr->acceptProc != NULL) {
	(infoPtr->acceptProc) (infoPtr->acceptProcData,
		acptInfo->clientInfo->channel,
		Tcl_GetString(objv[0]) /* IP string */,
		port);
    }

    Tcl_DecrRefCount(AddrInfo);

error:
    /* TODO: return error info to the trace routine. */

    IocpFree(acptInfo);

    /* Requeue another for the next checkProc iteration if another
     * readyAccepts exists. */
    EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
    if (IocpLLIsNotEmpty(infoPtr->readyAccepts)) {
	/*
	 * Flop the markedReady toggle.  This is used to improve event
	 * loop efficiency to avoid unneccesary events being queued into
	 * the readySockets list.
	 */
	if (!InterlockedExchange(&infoPtr->markedReady, 1)) {
	    /* No entry on the ready list.  Insert one. */
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,
		    &infoPtr->node, IOCP_LL_NOLOCK);
	}
    }
    LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
    return;
}

static int
IocpRemovePendingEvents (Tcl_Event *ev, ClientData cData)
{
    SocketInfo *infoPtr = (SocketInfo *) cData;
    SocketEvent *sev = (SocketEvent *) ev;

    if (ev->proc == IocpEventProc && sev->infoPtr == infoPtr) {
	return 1;
    } else {
	return 0;
    }
}

static int
IocpRemoveAllPendingEvents (Tcl_Event *ev, ClientData cData)
{
    if (ev->proc == IocpEventProc) {
	return 1;
    } else {
	return 0;
    }
}

/* =================================================================== */
/* ==================== Tcl_Driver*Proc procedures =================== */

static int
IocpCloseProc (
    ClientData instanceData,	/* The socket to close. */
    Tcl_Interp *interp)		/* Unused. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    int errorCode = 0;
    BufferInfo *bufPtr;

    /* TODO: Tcl convention says a blocking device should wait for
    all data and return an error code (if any). A non-blocking
    device should hard-close.. what to do here? */

    /*
     * The core wants to close channels after the exit handler!
     * Our heap is gone!
     */
    if (initialized) {

	/* Artificially increment the count. */
	InterlockedIncrement(&infoPtr->outstandingOps);

	/* Flip the bit so no new stuff can ever come in again. */
	InterlockedExchange(&infoPtr->markedReady, 1);

	/* Setting this means all returning operations will get
	 * trashed and no new operations are allowed. */
	infoPtr->flags |= IOCP_CLOSING;

	/* Tcl now doesn't recognize us anymore, so don't let this
	 * dangle. */
	infoPtr->channel = NULL;

	/* Remove ourselves from the readySockets list. */
	IocpLLPop(&infoPtr->node, IOCP_LL_NODESTROY);

	/* Remove all events queued in the event loop for this socket. */
	Tcl_DeleteEvents(IocpRemovePendingEvents, infoPtr);

	if (!infoPtr->acceptProc) {
	    /* Queue this client socket up for auto-destroy. */
	    bufPtr = GetBufferObj(infoPtr, 0);
	    PostOverlappedDisconnect(infoPtr, bufPtr);
	} else {
	    SOCKET temp;
	    /* Close this listening socket directly. */
	    infoPtr->flags |= IOCP_CLOSABLE;
	    InterlockedDecrement(&infoPtr->outstandingOps);
	    temp = infoPtr->socket;
	    infoPtr->socket = INVALID_SOCKET;
	    /* Cause all pending AcceptEx calls to return with WSA_OPERATION_ABORTED */
	    closesocket(temp);
	}
    }

    return errorCode;
}

static int
IocpInputProc (
    ClientData instanceData,	/* The socket state. */
    char *buf,			/* Where to store data. */
    int toRead,			/* Maximum number of bytes to read. */
    int *errorCodePtr)		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    char *bufPos = buf;
    int bytesRead = 0;
    DWORD timeout;
    BufferInfo *bufPtr;
    int done, gotError = 0;

    *errorCodePtr = 0;

    if (infoPtr->flags & IOCP_EOF) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    /* If we are async, don't block on the queue. */
    timeout = (infoPtr->flags & IOCP_ASYNC ? 0 : INFINITE);

    /* Merge in as much as toRead will allow. */

    if ((!(infoPtr->flags & IOCP_ASYNC))
	    || IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {

	while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv,
		IOCP_LL_NODESTROY, timeout)) != NULL) {

	    if (FilterSingleOpRecvBuf(infoPtr, bufPtr, bytesRead)) {
		break;
	    }
	    if (FilterPartialRecvBufMerge(infoPtr, bufPtr, &bytesRead,
		    toRead, bufPos)) {
		break;
	    }
	    done = DoRecvBufMerge(infoPtr, bufPtr, &bytesRead,
		    toRead, &bufPos, &gotError);
	    if (gotError) goto error;
	    if (done) break;
	    FreeBufferObj(bufPtr);
	    /* When blocking, only read one. */
	    if (!(infoPtr->flags & IOCP_ASYNC)) break;
	}
	RepostRecvs(infoPtr, toRead);

    } else {
	/* If there's nothing to get, return EWOULDBLOCK. */
	*errorCodePtr = EWOULDBLOCK;
	bytesRead = -1;
    }

    return bytesRead;

error:
    *errorCodePtr = Tcl_GetErrno();
    return -1;
}

static int
FilterPartialRecvBufMerge (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    int *bytesRead,
    int toRead,
    char *bufPos)
{
    BYTE *buffer;
    SIZE_T howMuch = toRead - *bytesRead;

    if ((*bytesRead + (int) bufPtr->used) > toRead) {

	/*
	 * Socket WSABUF is larger than the channel buffer space.  We need
	 * to do a partial copy to the channel buffer and repost the
	 * BufferInfo* back onto the linkedlist for the next read()
	 * operation.
	 */
	if (bufPtr->last) {
	    buffer = bufPtr->last;
	} else {
	    buffer = bufPtr->buf;
	}
	memcpy(bufPos, buffer, howMuch);
	bufPtr->used -= howMuch;
	bufPtr->last = buffer + howMuch;
	*bytesRead += howMuch;
	IocpLLPushFront(infoPtr->llPendingRecv, bufPtr,
		&bufPtr->node, 0);
	return 1;
    }
    return 0;
}

static int
FilterSingleOpRecvBuf (SocketInfo *infoPtr, BufferInfo *bufPtr, int bytesRead)
{
    if (bufPtr->used == 0 && bufPtr->buflen != 0 && bytesRead) {
	
	/*
	 * We have a new EOF or error, yet some bytes have already been
	 * written to the channel buffer within this read() operation.
	 * Push the bufPtr back onto the linkedlist for later. We need
	 * the EOF (or error) as a single operation.
	 */
	IocpLLPushFront(infoPtr->llPendingRecv, bufPtr,
		&bufPtr->node, 0);
	return 1;
    }
    return 0;
}

static int
DoRecvBufMerge (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    int *bytesRead,
    int toRead,
    char **bufPos,
    int *gotError)
{
    *gotError = 0;

    if (bufPtr->WSAerr != NO_ERROR) {
	IocpWinConvertWSAError(bufPtr->WSAerr);
	FreeBufferObj(bufPtr);
	*gotError = 1;
	return 1;
    } else {
	if (bufPtr->used == 0) {
	    if (bufPtr->buflen != 0) {
		/* got official EOF */
		infoPtr->flags |= IOCP_EOF;
		/* A read value of zero indicates EOF to the generic layer. */
		*bytesRead = 0;
		FreeBufferObj(bufPtr);
		return 1;
	    } else {
		WSABUF buffer;
		DWORD NumberOfBytesRecvd, Flags;

		/*
		 * Got the zero-byte recv alert. Do a non-blocking
		 * and non-posting WSARecv using the channel buffer
		 * directly.
		 */

		if (infoPtr->lastError != NO_ERROR) {

		    /*
		     * A write-side error occured.  Just return EOF,
		     * ignore this bufPtr error and don't call WSARecv.
		     */
		    *bytesRead = 0;
		    infoPtr->flags |= IOCP_EOF;
		    FreeBufferObj(bufPtr);
		    return 1;

		} else {
		    buffer.len = toRead;
		    buffer.buf = *bufPos;
		    Flags = 0;

		    if (WSARecv(infoPtr->socket, &buffer, 1,
			    &NumberOfBytesRecvd, &Flags, 0L, 0L)) {
			IocpWinConvertWSAError(WSAGetLastError());
			*gotError = 1;
			FreeBufferObj(bufPtr);
			return 1;
		    }
		    *bytesRead = NumberOfBytesRecvd;
		    if (NumberOfBytesRecvd == 0) {
			/* got official EOF */
			infoPtr->flags |= IOCP_EOF;
			FreeBufferObj(bufPtr);
			return 1;
		    }
		}
	    }
	} else {
	    BYTE *buffer;
	    if (bufPtr->last) {
		buffer = bufPtr->last;
	    } else {
		buffer = bufPtr->buf;
	    }
	    memcpy(*bufPos, buffer, bufPtr->used);
	    bytesRead += bufPtr->used;
	    *bufPos += bufPtr->used;
	}
    }
    return 0;
}

static void
RepostRecvs (SocketInfo *infoPtr, int chanBufSize)
{
    BufferInfo *newBufPtr;

    /* No more overlapped WSARecv calls needed? */
    if (infoPtr->flags & IOCP_EOF) return;

    if (infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE
	    || infoPtr->recvMode == IOCP_RECVMODE_FLOW_CTRL) {
	newBufPtr = GetBufferObj(infoPtr,
		(infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE ? 0 : chanBufSize));

	/*
	 * If an immediate error occurs, we can not return it to Tcl now
	 * as this is essentially the "next" packet of delivery. We have
	 * to force post it to the completion port to "read" it later.
	 */
	if (PostOverlappedRecv(infoPtr, newBufPtr, 0 /*useBurst*/,
		1 /*forcepost*/) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    } else if (infoPtr->recvMode == IOCP_RECVMODE_BURST_DETECT && infoPtr->needRecvRestart
	    && IocpLLGetCount(infoPtr->llPendingRecv) < infoPtr->outstandingRecvBufferCap) {
	newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);

	/*
	 * If an immediate error occurs, we can not return it to Tcl now
	 * as this is essentially the "next" packet of delivery. We have
	 * to force post it to the completion port to "read" it later.
	 */
	if (PostOverlappedRecv(infoPtr, newBufPtr, 1 /*useBurst*/,
		1 /*forcepost*/) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
	infoPtr->needRecvRestart = 0;
    }
}

static int
IocpOutputProc (
    ClientData instanceData,	/* The socket state. */
    CONST char *buf,		/* Where to get data. */
    int toWrite,		/* Maximum number of bytes to write. */
    int *errorCodePtr)		/* Where to store error codes. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;
    BufferInfo *bufPtr;
    DWORD result;

    *errorCodePtr = 0;


    if (TclInExit() || infoPtr->flags & IOCP_EOF
	    || infoPtr->flags & IOCP_CLOSING) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    /*
     * Check for a background error on the last operations.
     */

    if (infoPtr->lastError) {
	IocpWinConvertWSAError(infoPtr->lastError);
	goto error;
    }

    bufPtr = GetBufferObj(infoPtr, toWrite);
    memcpy(bufPtr->buf, buf, toWrite);
    result = PostOverlappedSend(infoPtr, bufPtr);
    if (result == WSAENOBUFS) {
	/* Would have been over the sendcap restriction. */
	FreeBufferObj(bufPtr);
	*errorCodePtr = EWOULDBLOCK;
	return -1;
    } else if (result != NO_ERROR) {
	/* Don't FreeBufferObj(), as it is already queued to the cp, too */
	infoPtr->lastError = result;
	IocpWinConvertWSAError(result);
	goto error;
    }

    return toWrite;

error:
    *errorCodePtr = Tcl_GetErrno();
    return -1;
}

static int
IocpSetOptionProc (
    ClientData instanceData,	/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    CONST char *optionName,	/* Name of the option to set. */
    CONST char *value)		/* New value for option. */
{
    SocketInfo *infoPtr;
    SOCKET sock;
    BOOL val = FALSE;
    int Integer, rtn;

    infoPtr = (SocketInfo *) instanceData;
    sock = infoPtr->socket;

    if (!stricmp(optionName, "-keepalive")) {
	if (Tcl_GetBoolean(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer) val = TRUE;
	rtn = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
		(const char *) &val, sizeof(BOOL));
	if (rtn != 0) {
	    if (interp) {
		SetLastError(WSAGetLastError());
		Tcl_AppendResult(interp, "couldn't set keepalive socket option: ",
			Tcl_WinError(interp), NULL);
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;

    } else if (!stricmp(optionName, "-nagle")) {
	if (Tcl_GetBoolean(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Integer) val = TRUE;
	rtn = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(const char *) &val, sizeof(BOOL));
	if (rtn != 0) {
	    if (interp) {
		SetLastError(WSAGetLastError());
		Tcl_AppendResult(interp, "couldn't set nagle socket option: ",
			Tcl_WinError(interp),	NULL);
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;

    } else if (strcmp(optionName, "-backlog") == 0 && infoPtr->acceptProc) {
	int i, error = TCL_OK;
	if (Tcl_GetInt(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer < IOCP_ACCEPT_CAP) {
	    if (interp) {
		char buf[TCL_INTEGER_SPACE];
		TclFormatInt(buf, IOCP_ACCEPT_CAP);
		Tcl_AppendResult(interp,
			"only a positive integer not less than ", buf,
			" is recommended", NULL);
	    }
	    error = TCL_ERROR;
	    if (Integer < 1) {
		return TCL_ERROR;
	    }
	    /* Let an unacceptably low -backlog get set anyways */
	}
	infoPtr->outstandingAcceptCap = Integer;
	/* Now post them in, if outstandingAcceptCap is now larger. */
        for (
	    i = infoPtr->outstandingAccepts;
	    i < infoPtr->outstandingAcceptCap;
	    i++
	) {
	    BufferInfo *bufPtr;
	    bufPtr = GetBufferObj(infoPtr, 0);
	    if (PostOverlappedAccept(infoPtr, bufPtr, 0) != NO_ERROR) {
		/* Oh no, the AcceptEx failed. */
		FreeBufferObj(bufPtr); break;
		/* TODO: add error reporting */
	    }
        }
	return error;

    } else if (strcmp(optionName, "-sendcap") == 0) {
	if (Tcl_GetInt(interp, value, &Integer) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Integer < 1) {
	    if (interp) {
		Tcl_AppendResult(interp,
		"only a positive integer greater than zero is allowed",
			NULL);
	    }
	    return TCL_ERROR;
	}
	InterlockedExchange(&infoPtr->outstandingSendCap, Integer);
	return TCL_OK;

    } else if (strcmp(optionName, "-recvmode") == 0) {
	CONST char **argv;
	int argc, recvCap, bufferCap;

        if (Tcl_SplitList(interp, value, &argc, &argv) == TCL_ERROR) {
            return TCL_ERROR;
        }

	if (strcmp(argv[0], "zero-byte") == 0) {
	    infoPtr->recvMode = IOCP_RECVMODE_ZERO_BYTE;
	    InterlockedExchange(&infoPtr->outstandingRecvCap, 1);
	    InterlockedExchange(&infoPtr->outstandingRecvBufferCap, 1);
	} else if (strcmp(argv[0], "flow-controlled") == 0) {
	    infoPtr->recvMode = IOCP_RECVMODE_FLOW_CTRL;
	    InterlockedExchange(&infoPtr->outstandingRecvCap, 1);
	    InterlockedExchange(&infoPtr->outstandingRecvBufferCap, 1);
	} else if (strcmp(argv[0], "burst-detection") == 0) {
	    if (argc == 3) {
		if (Tcl_GetInt(interp, argv[1], &recvCap) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (Tcl_GetInt(interp, argv[2], &bufferCap) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (recvCap < 1) {
		    if (interp) {
			Tcl_AppendResult(interp,
			    "only a positive integer greater than zero is "
			    "allowed", NULL);
		    }
		    return TCL_ERROR;
		}
		infoPtr->recvMode = IOCP_RECVMODE_BURST_DETECT;
		InterlockedExchange(&infoPtr->outstandingRecvCap, recvCap);
		InterlockedExchange(&infoPtr->outstandingRecvBufferCap, bufferCap);
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp,
			"burst-detection must be followed by an integer for "
			"the concurrency limit and another integer for the "
			"buffer limit count as a list.", NULL);
		}
		return TCL_ERROR;
	    }
	} else {
	    if (interp) {
		Tcl_AppendResult(interp,
		    "unknown option for -recvmode: must be one of "
		    "zero-byte, flow-controlled or {burst-detection "
		    "<WSARecv_limit> <buffer_limit>}.", NULL);
	    }
	    return TCL_ERROR;
	}

	return TCL_OK;
    }

/* TODO: pass this also to a protocol specific option routine. */

    if (infoPtr->acceptProc) {
	return Tcl_BadChannelOption(interp, optionName,
		"keepalive nagle backlog sendcap recvmode");
    } else {
	return Tcl_BadChannelOption(interp, optionName,
		"keepalive nagle sendcap recvmode");
    }
}

static int
IocpGetOptionProc (
    ClientData instanceData,	/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL */
    CONST char *optionName,	/* Name of the option to
				 * retrieve the value for, or
				 * NULL to get all options and
				 * their values. */
    Tcl_DString *dsPtr)		/* Where to store the computed
				 * value; initialized by caller. */
{
    SocketInfo *infoPtr;
    SOCKET sock;
    int size;
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];
    Tcl_Obj *AddrInfo;
    int objc, i;
    Tcl_Obj **objv;

    
    infoPtr = (SocketInfo *) instanceData;
    sock = infoPtr->socket;
    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if (len > 1) {
	if ((optionName[1] == 'e') &&
	    (strncmp(optionName, "-error", len) == 0)) {
	    if (infoPtr->lastError != NO_ERROR) {
		IocpWinConvertWSAError(infoPtr->lastError);
		Tcl_DStringAppend(dsPtr, Tcl_ErrnoMsg(Tcl_GetErrno()), -1);
	    }
	    return TCL_OK;
#if _DEBUG   /* for debugging only */
	} else if (strncmp(optionName, "-ops", len) == 0) {
	    TclFormatInt(buf, infoPtr->outstandingOps);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    return TCL_OK;
	} else if (strncmp(optionName, "-ready", len) == 0) {
	    EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	    TclFormatInt(buf, infoPtr->markedReady);
	    LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    return TCL_OK;
	} else if (strncmp(optionName, "-readable", len) == 0) {
	    if (infoPtr->llPendingRecv != NULL) {
		EnterCriticalSection(&infoPtr->llPendingRecv->lock);
		TclFormatInt(buf, infoPtr->llPendingRecv->lCount);
		LeaveCriticalSection(&infoPtr->llPendingRecv->lock);
		Tcl_DStringAppendElement(dsPtr, buf);
		return TCL_OK;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp,
			"A listening socket is not readable, ever.", NULL);
		}
		return TCL_ERROR;
	    }
	} else if (strncmp(optionName, "-readyaccepts", len) == 0) {
	    if (infoPtr->readyAccepts != NULL) {
		EnterCriticalSection(&infoPtr->readyAccepts->lock);
		TclFormatInt(buf, infoPtr->readyAccepts->lCount);
		LeaveCriticalSection(&infoPtr->readyAccepts->lock);
		Tcl_DStringAppendElement(dsPtr, buf);
		return TCL_OK;
	    } else {
		if (interp) {
		    Tcl_AppendResult(interp, "Not a listening socket.",
			    NULL);
		}
		return TCL_ERROR;
	    }
#endif
	}
    }

    if ((infoPtr->readyAccepts == NULL) /* not a listening socket */
	    && ((len == 0) || ((len > 1) && (optionName[1] == 'p') &&
            (strncmp(optionName, "-peername", len) == 0))))
    {
        if (infoPtr->remoteAddr == NULL) {
	    size = infoPtr->proto->addrLen;
	    infoPtr->remoteAddr = IocpAlloc(size);
	    if (getpeername(sock, infoPtr->remoteAddr, &size)
		    == SOCKET_ERROR) {
		/*
		 * getpeername failed - but if we were asked for all the
		 * options (len==0), don't flag an error at that point
		 * because it could be an fconfigure request on a server
		 * socket. (which have no peer). {copied from
		 * unix/tclUnixChan.c}
		 */
		if (len) {
		    if (interp) {
			SetLastError(WSAGetLastError());
			Tcl_AppendResult(interp, "getpeername() failed: ",
				Tcl_WinError(interp), NULL);
		    }
		    return TCL_ERROR;
		}
	    }
	}
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-peername");
            Tcl_DStringStartSublist(dsPtr);
        }

	AddrInfo = infoPtr->proto->DecodeSockAddr(infoPtr, infoPtr->remoteAddr);
	Tcl_ListObjGetElements(NULL, AddrInfo, &objc, &objv);

	/* Append data as per the protocol type. */
	for (i = 0; i < objc; i++) {
	    Tcl_DStringAppendElement(dsPtr, Tcl_GetString(objv[i]));
	}
	Tcl_DecrRefCount(AddrInfo);

        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if ((len == 0) ||
            ((len > 1) && (optionName[1] == 's') &&
                    (strncmp(optionName, "-sockname", len) == 0))) {
        if (infoPtr->localAddr == NULL) {
	    size = infoPtr->proto->addrLen;
	    infoPtr->localAddr = IocpAlloc(size);
	    if (getsockname(sock, infoPtr->localAddr, &size)
		    == SOCKET_ERROR) {
		if (interp) {
		    SetLastError(WSAGetLastError());
		    Tcl_AppendResult(interp, "getsockname() failed: ",
			    Tcl_WinError(interp), NULL);
		}
		return TCL_ERROR;
	    }
	}
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-sockname");
            Tcl_DStringStartSublist(dsPtr);
        }

	AddrInfo = infoPtr->proto->DecodeSockAddr(infoPtr, infoPtr->localAddr);
	Tcl_ListObjGetElements(NULL, AddrInfo, &objc, &objv);

	/* Append data as per the protocol type. */
	for (i = 0; i < objc; i++) {
	    Tcl_DStringAppendElement(dsPtr, Tcl_GetString(objv[i]));
	}
	Tcl_DecrRefCount(AddrInfo);

        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if (len == 0 || !strncmp(optionName, "-keepalive", len)) {
	int optlen;
	BOOL opt = FALSE;
    
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-keepalive");
        }
	optlen = sizeof(BOOL);
	getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt,
		&optlen);
	if (opt) {
	    Tcl_DStringAppendElement(dsPtr, "1");
	} else {
	    Tcl_DStringAppendElement(dsPtr, "0");
	}
	if (len > 0) return TCL_OK;
    }

    if (len == 0 || !strncmp(optionName, "-nagle", len)) {
	int optlen;
	BOOL opt = FALSE;
    
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-nagle");
        }
	optlen = sizeof(BOOL);
	getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt,
		&optlen);
	if (opt) {
	    Tcl_DStringAppendElement(dsPtr, "0");
	} else {
	    Tcl_DStringAppendElement(dsPtr, "1");
	}
	if (len > 0) return TCL_OK;
    }

    if (infoPtr->acceptProc) {
	if (len == 0 || !strncmp(optionName, "-backlog", len)) {
	    if (len == 0) {
		Tcl_DStringAppendElement(dsPtr, "-backlog");
		Tcl_DStringStartSublist(dsPtr);
	    }
	    TclFormatInt(buf, infoPtr->outstandingAcceptCap);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    TclFormatInt(buf, infoPtr->outstandingAccepts);
	    Tcl_DStringAppendElement(dsPtr, buf);
	    if (len == 0) {
		Tcl_DStringEndSublist(dsPtr);
	    } else {
		return TCL_OK;
	    }
	}
    }

    if (len == 0 || !strncmp(optionName, "-sendcap", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-sendcap");
            Tcl_DStringStartSublist(dsPtr);
        }
	TclFormatInt(buf, infoPtr->outstandingSendCap);
	Tcl_DStringAppendElement(dsPtr, buf);
	TclFormatInt(buf, infoPtr->outstandingSends);
	Tcl_DStringAppendElement(dsPtr, buf);
        if (len == 0) {
            Tcl_DStringEndSublist(dsPtr);
        } else {
            return TCL_OK;
        }
    }

    if (len == 0 || !strncmp(optionName, "-recvmode", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-recvmode");
        }
	switch (infoPtr->recvMode) {
	    case IOCP_RECVMODE_ZERO_BYTE:
		Tcl_DStringAppendElement(dsPtr, "zero-byte");
		break;
	    case IOCP_RECVMODE_FLOW_CTRL:
		Tcl_DStringAppendElement(dsPtr, "flow-controlled");
		break;
	    case IOCP_RECVMODE_BURST_DETECT:
		if (len == 0) {
		    Tcl_DStringStartSublist(dsPtr);
		}
		Tcl_DStringAppendElement(dsPtr, "burst-detection");
		TclFormatInt(buf, infoPtr->outstandingRecvCap);
		Tcl_DStringAppendElement(dsPtr, buf);
		TclFormatInt(buf, infoPtr->outstandingRecvs);
		Tcl_DStringAppendElement(dsPtr, buf);
		if (len == 0) {
		    Tcl_DStringEndSublist(dsPtr);
		}
		break;
	    default:
		Tcl_Panic("improper enumerator in IocpGetOptionProc");
	}
        if (len != 0) {
            return TCL_OK;
        }
    }

    if (len > 0) {
	if (infoPtr->acceptProc) {
	    return Tcl_BadChannelOption(interp, optionName,
		"peername sockname keepalive nagle backlog sendcap recvmode");
	} else {
	    return Tcl_BadChannelOption(interp, optionName,
		"peername sockname keepalive nagle sendcap recvmode");
	}
    }

    return TCL_OK;
}

static void
IocpWatchProc (
    ClientData instanceData,	/* The socket state. */
    int mask)			/* Events of interest; an OR-ed
				 * combination of TCL_READABLE,
				 * TCL_WRITABLE and TCL_EXCEPTION. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    if (!infoPtr->acceptProc) {
        infoPtr->watchMask = mask;
	if (!mask) {
	    return;
	}
	if (mask & TCL_READABLE
		&& IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
	    /* Instance is readable, validate the instance is on the
	     * ready list. */
	    IocpZapTclNotifier(infoPtr);
	} else if (mask & TCL_WRITABLE && infoPtr->llPendingRecv
		&& infoPtr->outstandingSends < infoPtr->outstandingSendCap) {
	    /* Instance is writable, validate the instance is on the
	     * ready list. */
	    IocpZapTclNotifier(infoPtr);
	}
    }
}

static int
IocpBlockProc (
    ClientData instanceData,	/* The socket state. */
    int mode)			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    if (!initialized) return 0;

    if (mode == TCL_MODE_NONBLOCKING) {
	infoPtr->flags |= IOCP_ASYNC;
    } else {
	infoPtr->flags &= ~(IOCP_ASYNC);
    }
    return 0;
}

static int
IocpGetHandleProc (
    ClientData instanceData,	/* The socket state. */
    int direction,		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr)	/* Where to store the handle.  */
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    *handlePtr = (ClientData) infoPtr->socket;
    return TCL_OK;
}

#ifdef TCL_CHANNEL_VERSION_4
static void
IocpThreadActionProc (ClientData instanceData, int action)
{
    SocketInfo *infoPtr = (SocketInfo *) instanceData;

    /* This lock is to prevent IocpZapTclNotifier() from accessing
     * infoPtr->tsdHome */
    EnterCriticalSection(&infoPtr->tsdLock);
    switch (action) {
    case TCL_CHANNEL_THREAD_INSERT:
	infoPtr->tsdHome = InitSockets();
	break;
    case TCL_CHANNEL_THREAD_REMOVE:
	/* Unable to turn off reading, therefore don't notify
	 * anyone during the move. */
	infoPtr->tsdHome = NULL;
	break;
    }
    LeaveCriticalSection(&infoPtr->tsdLock);
}
#endif

/* =================================================================== */
/* ============== Lo-level buffer and state manipulation ============= */

SocketInfo *
NewSocketInfo (SOCKET socket)
{
    SocketInfo *infoPtr;

    /* collect stats */
    InterlockedIncrement(&StatOpenSockets);

    infoPtr = IocpAlloc(sizeof(SocketInfo));
    infoPtr->channel = NULL;
    infoPtr->socket = socket;
    infoPtr->flags = 0;		    /* assume initial blocking state */
    infoPtr->markedReady = 0;
    infoPtr->outstandingOps = 0;	/* Total operations pending */
    infoPtr->outstandingSends = 0;
    infoPtr->outstandingSendCap = IOCP_SEND_CAP;
    infoPtr->outstandingAccepts = 0;
    infoPtr->outstandingAcceptCap = IOCP_ACCEPT_CAP;
    infoPtr->outstandingRecvs = 0;
    infoPtr->outstandingRecvCap = IOCP_RECV_CAP;
    infoPtr->needRecvRestart = 0;
    InitializeCriticalSectionAndSpinCount(&infoPtr->tsdLock, 400);
    infoPtr->recvMode = IOCP_RECVMODE_ZERO_BYTE;
    infoPtr->watchMask = 0;
    infoPtr->readyAccepts = NULL;
    infoPtr->acceptProc = NULL;
    infoPtr->localAddr = NULL;	    /* Local sockaddr. */
    infoPtr->remoteAddr = NULL;	    /* Remote sockaddr. */
    infoPtr->lastError = NO_ERROR;
    infoPtr->node.ll = NULL;
    return infoPtr;
}

void
FreeSocketInfo (SocketInfo *infoPtr)
{
    BufferInfo *bufPtr;

    if (!infoPtr) return;

    /* Remove us from the readySockets list, if on it. */
    IocpLLPop(&infoPtr->node, IOCP_LL_NODESTROY);

    /* Just in case... */
    if (infoPtr->socket != INVALID_SOCKET) {
	closesocket(infoPtr->socket);
    }

    /* collect stats */
    InterlockedDecrement(&StatOpenSockets);

    DeleteCriticalSection(&infoPtr->tsdLock);

    if (infoPtr->localAddr) {
	IocpFree(infoPtr->localAddr);
    }
    if (infoPtr->remoteAddr) {
	IocpFree(infoPtr->remoteAddr);
    }

    if (infoPtr->readyAccepts) {
	AcceptInfo *acptInfo;
	while ((acptInfo = IocpLLPopFront(infoPtr->readyAccepts,
		IOCP_LL_NODESTROY, 0)) != NULL) {
	    /* Recursion, but can't be a server socket.. So this is safe. */
	    FreeSocketInfo(acptInfo->clientInfo);
	    IocpFree(acptInfo);
	}
	IocpLLDestroy(infoPtr->readyAccepts);
    }
    if (infoPtr->llPendingRecv) {
	while ((bufPtr = IocpLLPopFront(infoPtr->llPendingRecv,
		IOCP_LL_NODESTROY, 0)) != NULL) {
	    FreeBufferObj(bufPtr);
	}
	IocpLLDestroy(infoPtr->llPendingRecv);
    }
    IocpFree(infoPtr);
}

BufferInfo *
GetBufferObj (SocketInfo *infoPtr, SIZE_T buflen)
{
    BufferInfo *bufPtr;

    /* Allocate the object. */
    bufPtr = IocpNPPAlloc(sizeof(BufferInfo));
    if (bufPtr == NULL) {
	return NULL;
    }
    /* Allocate the buffer. */
    bufPtr->buf = IocpNPPAlloc(sizeof(BYTE)*buflen);
    if (bufPtr->buf == NULL) {
	IocpNPPFree(bufPtr);
	return NULL;
    }
    bufPtr->socket = INVALID_SOCKET;
    bufPtr->last = NULL;
    bufPtr->buflen = buflen;
    bufPtr->addr = NULL;
    bufPtr->WSAerr = NO_ERROR;
    bufPtr->parent = infoPtr;
    bufPtr->node.ll = NULL;
    return bufPtr;
}

void
FreeBufferObj (BufferInfo *bufPtr)
{
    /* Pop itself off any linked-list it may be on. */
    IocpLLPop(&bufPtr->node, IOCP_LL_NODESTROY);
    /* If we have a socket for AcceptEx(), close it. */
    if (bufPtr->socket != INVALID_SOCKET) {
	closesocket(bufPtr->socket);
    }
    IocpNPPFree(bufPtr->buf);
    IocpNPPFree(bufPtr);
}

SocketInfo *
NewAcceptSockInfo (SOCKET socket, SocketInfo *infoPtr)
{
    SocketInfo *newInfoPtr;

    newInfoPtr = NewSocketInfo(socket);
    if (newInfoPtr == NULL) {
	return NULL;
    }

    /* Initialize some members (partial cloning of the parent). */
    newInfoPtr->proto = infoPtr->proto;
    newInfoPtr->tsdHome = infoPtr->tsdHome;
    newInfoPtr->llPendingRecv = IocpLLCreate();
    InterlockedExchange(&newInfoPtr->outstandingSendCap,
	    infoPtr->outstandingSendCap);
    InterlockedExchange(&newInfoPtr->outstandingRecvCap,
	    infoPtr->outstandingRecvCap);
    newInfoPtr->recvMode = infoPtr->recvMode;

    return newInfoPtr;
}

/*
 *----------------------------------------------------------------------
 * IocpZapTclNotifier --
 *
 *	Wake the notifier.  Only zap the notifier when the notifier is
 *	waiting and this request has not already been made previously.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None known.
 *
 *----------------------------------------------------------------------
 */

static void
IocpZapTclNotifier (SocketInfo *infoPtr)
{
    EnterCriticalSection(&infoPtr->tsdLock);
    /*
     * If we are in the middle of a thread transfer on the channel,
     * infoPtr->tsdHome will be NULL.
     */
    if (infoPtr->tsdHome) {
	EnterCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	if (!InterlockedExchange(&infoPtr->markedReady, 1)) {
	    /* No entry on the ready list.  Insert one. */
	    IocpLLPushBack(infoPtr->tsdHome->readySockets, infoPtr,
		    &infoPtr->node, IOCP_LL_NOLOCK);
	}
	LeaveCriticalSection(&infoPtr->tsdHome->readySockets->lock);
	/* This is safe to call from any thread. */
	Tcl_ThreadAlert(infoPtr->tsdHome->threadId);
    }
    LeaveCriticalSection(&infoPtr->tsdLock);
}

/* takes buffer ownership */

static void
IocpAlertToTclNewAccept (
    SocketInfo *infoPtr,
    SocketInfo *newClient)
{
    AcceptInfo *acptInfo;

    acptInfo = IocpAlloc(sizeof(AcceptInfo));
    if (acptInfo == NULL) {
	return;
    }

    memcpy(&acptInfo->local, newClient->localAddr,
	    newClient->proto->addrLen);
    acptInfo->localLen = newClient->proto->addrLen;
    memcpy(&acptInfo->remote, newClient->remoteAddr,
	    newClient->proto->addrLen);
    acptInfo->remoteLen = newClient->proto->addrLen;
    acptInfo->clientInfo = newClient;

    /*
     * Queue this accept's data into the listening channel's info block.
     */

    IocpLLPushBack(infoPtr->readyAccepts, acptInfo, &acptInfo->node, 0);

    /*
     * Let IocpCheckProc() know this channel has an accept ready
     * that needs servicing.
     */
    IocpZapTclNotifier(infoPtr);
}

static void
IocpPushRecvAlertToTcl(SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    /* (takes buffer ownership) */
    IocpLLPushBack(infoPtr->llPendingRecv, bufPtr,
	    &bufPtr->node, 0);

    /*
     * Let IocpCheckProc() know this new channel has a ready
     * event (a recv) that needs servicing.  That is, if Tcl
     * is interested in knowing about it.
     */
    if (infoPtr->watchMask & TCL_READABLE) {
	IocpZapTclNotifier(infoPtr);
    }
}

DWORD
PostOverlappedAccept (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    int useBurst)
{
    DWORD bytes, WSAerr;
    int rc;
    SIZE_T buflen, addr_storage;

    if (infoPtr->flags & IOCP_CLOSING) return WSAENOTCONN;

    /* Recursion limit */
    if (InterlockedIncrement(&infoPtr->outstandingAccepts)
	    > infoPtr->outstandingAcceptCap) {
	InterlockedDecrement(&infoPtr->outstandingAccepts);
	/* Best choice I could think of for an error value. */
	return WSAENOBUFS;
    }

    bufPtr->operation = OP_ACCEPT;
    buflen = bufPtr->buflen;
    addr_storage = infoPtr->proto->addrLen + 16;

    /*
     * Create a ready client socket of the same type for a future
     * incoming connection.
     */

    bufPtr->socket = WSASocket(infoPtr->proto->af,
	    infoPtr->proto->type, infoPtr->proto->protocol, NULL, 0,
	    WSA_FLAG_OVERLAPPED);

    if (bufPtr->socket == INVALID_SOCKET) {
	return WSAENOTSOCK;
    }

    /*
     * Realloc for the extra needed addr storage space.
     */
    bufPtr->buf = IocpNPPReAlloc(bufPtr->buf, bufPtr->buflen +
	    (addr_storage * 2));
    bufPtr->buflen += (addr_storage * 2);

    /*
     * Increment the outstanding overlapped count for this socket and put
     * the buffer on the pending accepts list.  We need to do this before
     * the operation because it might complete instead of posting.
     */
    
    InterlockedIncrement(&infoPtr->outstandingOps);

    /*
     * Use the special function for overlapped accept() that is provided
     * by the LSP of this socket type.
     */

    rc = infoPtr->proto->_AcceptEx(infoPtr->socket, bufPtr->socket,
	    bufPtr->buf, bufPtr->buflen - (addr_storage * 2),
	    addr_storage, addr_storage, &bytes, &bufPtr->ol);

    if (rc == FALSE) {
	if ((WSAerr = WSAGetLastError()) != WSA_IO_PENDING) {
	    InterlockedDecrement(&infoPtr->outstandingOps);
	    InterlockedDecrement(&infoPtr->outstandingAccepts);
	    bufPtr->WSAerr = WSAerr;
	    return WSAerr;
	}
    } else if (useBurst) {
	/*
	 * Tested under extreme listening socket abuse it was found that
	 * this logic condition is never met.  AcceptEx _never_ completes
	 * immediately.  It always returns WSA_IO_PENDING.  Too bad,
	 * as this was looking like a good way to detect and handle burst
	 * conditions.
	 */

	BufferInfo *newBufPtr;

	/*
	 * The AcceptEx() has completed now, and was posted to the port.
	 * Keep giving more AcceptEx() calls to drain the internal
	 * backlog.  Why should we wait for the time when the completion
	 * routine is run if we know the listening socket can take another
	 * right now?  IOW, keep recursing until the WSA_IO_PENDING 
	 * condition is achieved.
	 */

	newBufPtr = GetBufferObj(infoPtr, buflen);
	if (PostOverlappedAccept(infoPtr, newBufPtr, 1) != NO_ERROR) {
	    FreeBufferObj(newBufPtr);
	}
    }

    return NO_ERROR;
}

/*
 * A NO_ERROR return indicates the WSARecv operation is pending, or 
 * if an error occurs that the bufPtr was posted to the port.
 */

DWORD
PostOverlappedRecv (
    SocketInfo *infoPtr,
    BufferInfo *bufPtr,
    int useBurst,
    int ForcePostOnError)
{
    WSABUF wbuf;
    DWORD bytes = 0, flags, WSAerr;
    int rc;

    bufPtr->WSAerr = NO_ERROR;

    if (infoPtr->flags & IOCP_EOF || infoPtr->flags & IOCP_CLOSING)
	    return WSAENOTCONN;

    /* Recursion limit */
    if (InterlockedIncrement(&infoPtr->outstandingRecvs)
	    > infoPtr->outstandingRecvCap) {
	InterlockedDecrement(&infoPtr->outstandingRecvs);
	/* Best choice I could think of for an error value. */
	return WSAENOBUFS;
    }

    bufPtr->operation = OP_READ;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;
    flags = 0;

    /*
     * Increment the outstanding overlapped count for this socket.
     */

    InterlockedIncrement(&infoPtr->outstandingOps);

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = WSARecv(infoPtr->socket, &wbuf, 1, &bytes, &flags,
		&bufPtr->ol, NULL);
    } else {
	rc = WSARecvFrom(infoPtr->socket, &wbuf, 1, &bytes,
		&flags, bufPtr->addr, &infoPtr->proto->addrLen,
		&bufPtr->ol, NULL);
    }

    /*
     * There are three states that can happen here:
     *
     * 1) WSARecv returns zero when the operation has completed
     *	  immediately and the completion is queued to the port (behind
     *	  us now).
     * 2) WSARecv returns SOCKET_ERROR with WSAGetLastError() returning
     *	  WSA_IO_PENDING to indicate the operation was succesfully
     *	  initiated and will complete at a later time (and possibly
     *	  complete with an error or not).
     * 3) WSARecv returns SOCKET_ERROR with WSAGetLastError() returning
     *	  any other WSAGetLastError() code to indicate the operation was
     *	  NOT succesfully initiated and completion will NOT occur.
     */

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = WSAGetLastError()) != WSA_IO_PENDING) {
	    bufPtr->WSAerr = WSAerr;
	    if (ForcePostOnError) {
		PostQueuedCompletionStatus(IocpSubSystem.port, 0,
			(ULONG_PTR) infoPtr, &bufPtr->ol);
		/* We can not process the error now, but is posted, so don't return an error. */
		return NO_ERROR;
	    } else {
		InterlockedDecrement(&infoPtr->outstandingOps);
		InterlockedDecrement(&infoPtr->outstandingRecvs);
		/* return the error. */
		return WSAerr;
	    }
	}
    } else if (bytes > 0 && useBurst) {
	BufferInfo *newBufPtr;

	/*
	 * The WSARecv(From) has completed now, *AND* is posted to the
	 * port.  Keep giving more WSARecv(From) calls to drain the
	 * internal buffer (AFD.sys).  Why should we wait for the time
	 * when the completion routine is run if we know the connected
	 * socket can take another right now?  IOW, keep recursing until
	 * the WSA_IO_PENDING condition is achieved.
	 * 
	 * The only drawback to this is the amount of outstanding calls
	 * will increase.  There is no method for coming out of a burst
	 * condition to return the count to normal.  This shouldn't be
	 * an issue with short lived sockets -- only ones with a long
	 * lifetime.
	 */

	newBufPtr = GetBufferObj(infoPtr, wbuf.len);
	if (PostOverlappedRecv(infoPtr, newBufPtr, 1 /*useBurst */, 1 /*forcepost*/)) {
	    /*
	     * simple states for burstCap and !connected shall be
	     * ignored and the buffer recycled.
	     */
	    FreeBufferObj(newBufPtr);
	}
    }

    return NO_ERROR;
}

DWORD
PostOverlappedSend (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    WSABUF wbuf;
    DWORD bytes = 0, WSAerr;
    int rc;

    if (infoPtr->flags & IOCP_EOF || infoPtr->flags & IOCP_CLOSING)
	    return WSAENOTCONN;

    bufPtr->operation = OP_WRITE;
    wbuf.buf = bufPtr->buf;
    wbuf.len = bufPtr->buflen;

    /* Recursion limit */
    if (InterlockedIncrement(&infoPtr->outstandingSends)
	    > infoPtr->outstandingSendCap) {
	InterlockedDecrement(&infoPtr->outstandingSends);
	/* Best choice I could think of for an error value. */
	return WSAENOBUFS;
    }

    /*
     * Increment the outstanding overlapped counts for this socket.
     */

    InterlockedIncrement(&infoPtr->outstandingOps);

    if (infoPtr->proto->type == SOCK_STREAM) {
	rc = WSASend(infoPtr->socket, &wbuf, 1, &bytes, 0,
		&bufPtr->ol, NULL);
    } else {
	rc = WSASendTo(infoPtr->socket, &wbuf, 1, &bytes, 0,
                bufPtr->addr, infoPtr->proto->addrLen,
		&bufPtr->ol, NULL);
    }

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = WSAGetLastError()) != WSA_IO_PENDING) {
	    bufPtr->WSAerr = WSAerr;

	    /*
	     * Eventhough we know about the error now, post this to the
	     * port manually, too.  We need to force EOF on the read side
	     * as the generic layer needs a little help to know that both
	     * sides of our bidirectional channel are now dead because of
	     * this write side error.
	     */

	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	    return WSAerr;
	}
    } else {
	/*
	 * The WSASend/To completed now and is queued to the port.
	 */
	__asm nop;
    }

    return NO_ERROR;
}

static DWORD
PostOverlappedDisconnect (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    BOOL rc;
    DWORD WSAerr;

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&infoPtr->outstandingOps);

    bufPtr->operation = OP_DISCONNECT;

    rc = infoPtr->proto->_DisconnectEx(infoPtr->socket, &bufPtr->ol,
	    0 /*TF_REUSE_SOCKET*/, 0);

    if (rc == FALSE) {
	if ((WSAerr = WSAGetLastError()) != WSA_IO_PENDING) {
	    bufPtr->WSAerr = WSAerr;

	    /*
	     * Eventhough we know about the error now, post this to the
	     * port manually, anyways.
	     */

	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	    return NO_ERROR;
	}
    } else {
	/*
	 * The DisconnectEx completed now and is queued to the port.
	 */
	__asm nop;
    }

    return NO_ERROR;
}

DWORD
PostOverlappedQOS (SocketInfo *infoPtr, BufferInfo *bufPtr)
{
    int rc;
    DWORD bytes = 0, WSAerr;

    /*
     * Increment the outstanding overlapped count for this socket.
     */
    InterlockedIncrement(&infoPtr->outstandingOps);
    bufPtr->operation = OP_QOS;

    rc = WSAIoctl(infoPtr->socket, SIO_GET_QOS, NULL, 0,
	    bufPtr->buf, bufPtr->buflen, &bytes, &bufPtr->ol, NULL);

    if (rc == SOCKET_ERROR) {
	if ((WSAerr = WSAGetLastError()) != WSA_IO_PENDING) {
	    /*
	     * Eventhough we know about the error now, post this to the
	     * port manually, anyways.
	     */

	    bufPtr->WSAerr = WSAerr;
	    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
		(ULONG_PTR) infoPtr, &bufPtr->ol);
	}
    } else {
	/*
	 * The WSAIoctl completed now and is queued to the port.
	 */
	__asm nop;
    }

    return NO_ERROR;
}


/* =================================================================== */
/* ================== Lo-level Completion handler ==================== */


/*
 *----------------------------------------------------------------------
 * CompletionThread --
 *
 *	The "main" for the I/O handling thread.  Only one thread is used.
 *
 * Results:
 *
 *	None.  Returns when the completion port is sent a completion
 *	notification with a NULL key by the exit handler
 *	(IocpExitHandler).
 *
 * Side effects:
 *
 *	Without direct interaction from Tcl, incoming accepts will be
 *	accepted and receives, received.  The results of the operations
 *	are posted and tcl will service them when the event loop is
 *	ready to.  Winsock is never left "dangling" on operations.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
CompletionThreadProc (LPVOID lpParam)
{
    CompletionPortInfo *cpinfo = (CompletionPortInfo *)lpParam;
    SocketInfo *infoPtr;
    BufferInfo *bufPtr;
    OVERLAPPED *ol;
    DWORD bytes, flags, WSAerr, error = NO_ERROR;
    BOOL ok;

#ifdef _DEBUG
#else
    __try {
#endif
again:
	WSAerr = NO_ERROR;
	flags = 0;

	ok = GetQueuedCompletionStatus(cpinfo->port, &bytes,
		(PULONG_PTR) &infoPtr, &ol, INFINITE);

	if (ok && !infoPtr) {
	    /* A NULL key indicates closure time for this thread. */
#ifdef _DEBUG
	    return error;
#else
	    __leave;
#endif
	}

	/*
	 * Use the pointer to the overlapped structure and derive from it
	 * the top of the parent BufferInfo structure it sits in.  If the
	 * position of the overlapped structure moves around within the
	 * BufferInfo structure declaration, this logic does _not_ need
	 * to be modified.
	 */

	bufPtr = CONTAINING_RECORD(ol, BufferInfo, ol);

	if (!ok) {
	    /*
	     * If GetQueuedCompletionStatus() returned a failure on
	     * the operation, call WSAGetOverlappedResult() to
	     * translate the error into a Winsock error code.
	     */

	    ok = WSAGetOverlappedResult(infoPtr->socket,
		    ol, &bytes, FALSE, &flags);

	    if (!ok) {
		WSAerr = WSAGetLastError();
	    }
	}

	/* Go handle the IO operation. */
	HandleIo(infoPtr, bufPtr, cpinfo->port, bytes, WSAerr, flags);
	goto again;
#ifdef _DEBUG
#else
    }
    __except (error = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
	Tcl_Panic("Big ERROR!  IOCP Completion thread died with exception"
		" code: %#x\n", error);
    }

    return error;
#endif
}

/*
 *----------------------------------------------------------------------
 * HandleIo --
 *
 *	Has all the logic for what to do with a socket "event".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Either deletes the buffer handed to it, or processes it.
 *
 *----------------------------------------------------------------------
 */

static void
HandleIo (
    register SocketInfo *infoPtr,
    register BufferInfo *bufPtr,
    HANDLE CompPort,
    DWORD bytes,
    DWORD WSAerr,
    DWORD flags)
{
    register SocketInfo *newInfoPtr;
    register BufferInfo *newBufPtr;
    register int i;
    SOCKADDR *local, *remote;
    SIZE_T addr_storage;
    int localLen, remoteLen;


    if (WSAerr == WSA_OPERATION_ABORTED) {
	/* Reclaim cancelled overlapped buffer objects. */
        FreeBufferObj(bufPtr);
	goto done;
    }

    bufPtr->used = bytes;
    /* An error stored in the buffer object takes precedence. */
    if (bufPtr->WSAerr == NO_ERROR) {
	bufPtr->WSAerr = WSAerr;
    }

    switch (bufPtr->operation) {
    case OP_ACCEPT:

	/*
	 * Decrement the count of pending accepts from the total.
	 */

	InterlockedDecrement(&infoPtr->outstandingAccepts);

	if (bufPtr->WSAerr == NO_ERROR) {
	    addr_storage = infoPtr->proto->addrLen + 16;

	    /*
	     * Get the address information from the decoder routine
	     * specific to this socket's LSP.
	     */

	    infoPtr->proto->_GetAcceptExSockaddrs(bufPtr->buf,
		    bufPtr->buflen - (addr_storage * 2), addr_storage,
		    addr_storage, &local, &localLen, &remote,
		    &remoteLen);

	    setsockopt(bufPtr->socket, SOL_SOCKET,
		    SO_UPDATE_ACCEPT_CONTEXT, (char *)&infoPtr->socket,
		    sizeof(SOCKET));

	    /*
	     * Get a new SocketInfo for the new client connection.
	     */

	    newInfoPtr = NewAcceptSockInfo(bufPtr->socket, infoPtr);

	    /*
	     * Set the socket to invalid in the buffer so it won't be
	     * closed when the buffer is reclaimed.
	     */

	    bufPtr->socket = INVALID_SOCKET;

	    /*
	     * Save the remote and local SOCKADDRs to its SocketInfo
	     * struct.
	     */

	    newInfoPtr->localAddr = IocpAlloc(localLen);
	    memcpy(newInfoPtr->localAddr, local, localLen);
	    newInfoPtr->remoteAddr = IocpAlloc(remoteLen);
	    memcpy(newInfoPtr->remoteAddr, remote, remoteLen);

	    /* Associate the new socket to our completion port. */
	    CreateIoCompletionPort((HANDLE) newInfoPtr->socket, CompPort,
		    (ULONG_PTR) newInfoPtr, 0);

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(newInfoPtr,
			(infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE ? 0
			: IOCP_RECV_BUFSIZE));
		if ((WSAerr = PostOverlappedRecv(newInfoPtr, newBufPtr,
			0 /*useburst*/, 0 /*forcepost*/)) != NO_ERROR) {
		    /*
		     * The new connection is not valid.  Do not alert
		     * Tcl about this new dud connection.  Clean it
		     * up ourselves.
		     */
		    newInfoPtr->flags |= IOCP_CLOSING;
		    PostOverlappedDisconnect(newInfoPtr, newBufPtr);
		    goto replace;
		}
	    }

	    /* Alert Tcl to this new connection. */
	    IocpAlertToTclNewAccept(infoPtr, newInfoPtr);

	    if (bytes > 0) {
		/* Only if we asked for AcceptEx to give us an initial
		 * recv with the accept. */
		IocpPushRecvAlertToTcl(newInfoPtr, bufPtr);
	    } else {
		/* No data received from AcceptEx(). */
		FreeBufferObj(bufPtr);
	    }

	} else if (bufPtr->WSAerr == WSA_OPERATION_ABORTED ||
		bufPtr->WSAerr == WSAENOTSOCK) {
	    /* The operation was cancelled.  The listening socket must
	     * be closing.  Do NOT replace this returning AcceptEx. */
	    FreeBufferObj(bufPtr);
	    break;

	} else if (bufPtr->WSAerr == WSAENOBUFS) {
	    /*
	     * No more space! The pending limit has been reached.
	     * Decrement the asking cap by one.
	     */

	    InterlockedDecrement(&infoPtr->outstandingAcceptCap);
	    FreeBufferObj(bufPtr);
	    break;

	} else {
	    /*
	     * Possible spoofed SYN flood in progress. We WANT this
	     * returning AcceptEx that had an error to be replaced.
	     * An AcceptEx can fail with the same errors as ConnectEx,
	     * believe it or not!  Some of the errors sampled are:
	     *
	     * WSAEHOSTUNREACH, WSAETIMEDOUT, WSAENETUNREACH
	     */
	    InterlockedIncrement(&StatFailedAcceptExCalls);
	    FreeBufferObj(bufPtr);
	}

replace:
	/*
	 * Post another new AcceptEx() to replace this one that just
	 * completed.
	 */

	newBufPtr = GetBufferObj(infoPtr, 0);
	if (PostOverlappedAccept(infoPtr, newBufPtr, 0) != NO_ERROR) {

	    /*
	     * Oh no, the AcceptEx failed.  There is no way to return an
	     * error for this condition.  Tcl has no failure aspect for
	     * listening sockets.  This Just shouldn't have happened.
	     */
	    FreeBufferObj(newBufPtr);
	    InterlockedIncrement(&StatFailedReplacementAcceptExCalls);
	}
	break;

    case OP_READ:

	/* Decrement the count of pending recvs from the total. */
	InterlockedDecrement(&infoPtr->outstandingRecvs);

	if (bytes > 0) {
	    /*
	     * Don't replace this buffer if we hit the cap.  Not until the
	     * buffers get consumed will the receiving be restarted.
	     */

	    if (infoPtr->recvMode == IOCP_RECVMODE_BURST_DETECT) {
		if (IocpLLGetCount(infoPtr->llPendingRecv)
			< infoPtr->outstandingRecvBufferCap) {

		    /*
		     * Create a new buffer object to replace the one that
		     * just came in, but use the hard-coded size of
		     * IOCP_RECV_BUFSIZE.
		     */

		    newBufPtr = GetBufferObj(infoPtr, IOCP_RECV_BUFSIZE);
		    if (PostOverlappedRecv(infoPtr, newBufPtr,
			    1 /*useburst*/, 1 /*forcepost*/) != NO_ERROR) {
			FreeBufferObj(newBufPtr);
		    }
		} else {
		    infoPtr->needRecvRestart = 1;
		}
	    }

	} else if (infoPtr->flags & IOCP_CLOSING) {
	    infoPtr->flags |= IOCP_CLOSABLE;
	    FreeBufferObj(bufPtr);
	    break;

	} else if (bufPtr->WSAerr == WSAENOBUFS) {
	    /*
	     * No more space! The pending limit has been reached.
	     * Decrement the asking cap by one.
	     */

	    InterlockedDecrement(&infoPtr->outstandingRecvCap);
	    FreeBufferObj(bufPtr);
	    break;
	}

	/* Takes buffer ownership. */
	IocpPushRecvAlertToTcl(infoPtr, bufPtr);

	break;

    case OP_WRITE:

	/*
	 * Decrement the count of pending sends from the total.
	 */

	InterlockedDecrement(&infoPtr->outstandingSends);

	if (infoPtr->flags & IOCP_CLOSING) {
	    FreeBufferObj(bufPtr);
	    break;
	}

	if (bufPtr->WSAerr != NO_ERROR && bufPtr->WSAerr != WSAENOBUFS
		&& infoPtr->llPendingRecv) {

	    infoPtr->lastError = bufPtr->WSAerr;

	    /* Errors are writable events too. */
	    IocpZapTclNotifier(infoPtr);


	} else if (infoPtr->watchMask & TCL_WRITABLE &&
		infoPtr->outstandingSends < infoPtr->outstandingSendCap) {

	    if (bufPtr->WSAerr == WSAENOBUFS) {
		/*
		 * No more space! The pending limit has been reached.
		 * Decrement the asking cap by one.
		 */

		InterlockedDecrement(&infoPtr->outstandingSendCap);
	    } else {
		/* can write more. */
		IocpZapTclNotifier(infoPtr);
	    }
	}
	FreeBufferObj(bufPtr);
	break;

    case OP_CONNECT:

	infoPtr->llPendingRecv = IocpLLCreate();

	if (bufPtr->WSAerr != NO_ERROR) {
	    infoPtr->lastError = bufPtr->WSAerr;
	    newBufPtr = GetBufferObj(infoPtr, 1);
	    newBufPtr->WSAerr = bufPtr->WSAerr;
	    /* Force EOF. */
	    IocpPushRecvAlertToTcl(infoPtr, newBufPtr);
	} else {
	    setsockopt(infoPtr->socket, SOL_SOCKET,
		    SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	    /* post IOCP_INITIAL_RECV_COUNT recvs. */
	    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
		newBufPtr = GetBufferObj(infoPtr,
			(infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE ? 0
			: IOCP_RECV_BUFSIZE));
		if (PostOverlappedRecv(infoPtr, newBufPtr, 0 /*useburst*/,
			1 /*forcepost*/) != NO_ERROR) {
		    FreeBufferObj(newBufPtr);
		    break;
		}
	    }
	    IocpZapTclNotifier(infoPtr);
	}
	FreeBufferObj(bufPtr);
	break;

    case OP_DISCONNECT:
	/* remove the extra ref count. */
	InterlockedDecrement(&infoPtr->outstandingOps);
	infoPtr->flags |= IOCP_CLOSABLE;
	FreeBufferObj(bufPtr);
	break;

    case OP_QOS: {
	/* TODO: make this do something useful. */
	QOS *stuff = (QOS *) bufPtr->buf;
	FreeBufferObj(bufPtr);
	break;
	}

    case OP_TRANSMIT:
    case OP_LOOKUP:
    case OP_IOCTL:
	/* For future use. */
	break;
    }

done:
    if (InterlockedDecrement(&infoPtr->outstandingOps) <= 0
	    && infoPtr->flags & IOCP_CLOSABLE) {
	/* This is the last operation. */
	FreeSocketInfo(infoPtr);
    }
}


/* =================================================================== */
/* ====================== Private memory heap ======================== */

/* general pool */


__inline LPVOID
IocpAlloc (SIZE_T size)
{
    LPVOID p;
    p = HeapAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, size);
    if (p) InterlockedExchangeAdd(&StatGeneralBytesInUse, size);
    return p;
}

__inline LPVOID
IocpReAlloc (LPVOID block, SIZE_T size)
{
    LPVOID p;
    SIZE_T oldSize;
    oldSize = HeapSize(IocpSubSystem.heap, 0, block);
    p = HeapReAlloc(IocpSubSystem.heap, HEAP_ZERO_MEMORY, block, size);
    if (p) InterlockedExchangeAdd(&StatGeneralBytesInUse, ((LONG)size - oldSize));
    return p;
}

__inline BOOL
IocpFree (LPVOID block)
{
    BOOL code;
    SIZE_T oldSize;
    oldSize = HeapSize(IocpSubSystem.heap, 0, block);
    code = HeapFree(IocpSubSystem.heap, 0, block);
    if (code) InterlockedExchangeAdd(&StatGeneralBytesInUse, -((LONG)oldSize));
    return code;
}

/* special pool */

__inline LPVOID
IocpNPPAlloc (SIZE_T size)
{
    LPVOID p;
    p = HeapAlloc(IocpSubSystem.NPPheap, HEAP_ZERO_MEMORY, size);
    if (p) InterlockedExchangeAdd(&StatSpecialBytesInUse, size);
    return p;
}

__inline LPVOID
IocpNPPReAlloc (LPVOID block, SIZE_T size)
{
    LPVOID p;
    SIZE_T oldSize;
    oldSize = HeapSize(IocpSubSystem.NPPheap, 0, block);
    p = HeapReAlloc(IocpSubSystem.NPPheap, HEAP_ZERO_MEMORY, block, size);
    if (p) InterlockedExchangeAdd(&StatSpecialBytesInUse, ((LONG)size - oldSize));
    return p;
}

__inline BOOL
IocpNPPFree (LPVOID block)
{
    BOOL code;
    SIZE_T oldSize;
    oldSize = HeapSize(IocpSubSystem.NPPheap, 0, block);
    code = HeapFree(IocpSubSystem.NPPheap, 0, block);
    if (code) InterlockedExchangeAdd(&StatSpecialBytesInUse, -((LONG)oldSize));
    return code;
}


/* =================================================================== */
/* =================== Protocol neutral procedures =================== */


/*
 *-----------------------------------------------------------------------
 *
 * IocpInitProtocolData --
 *
 *	This function initializes the WS2ProtocolData struct.
 *
 * Results:
 *	nothing.
 *
 * Side effects:
 *	Fills in the WS2ProtocolData structure, if uninitialized.
 *
 *-----------------------------------------------------------------------
 */

void
IocpInitProtocolData (SOCKET sock, WS2ProtocolData *pdata)
{
    DWORD bytes;

    /* is it already cached? */
    if (pdata->_AcceptEx == NULL) {
	/* Get the LSP specific functions. */
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&AcceptExGuid, sizeof(GUID),
		&pdata->_AcceptEx,
		sizeof(pdata->_AcceptEx),
		&bytes, NULL, NULL);
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GetAcceptExSockaddrsGuid, sizeof(GUID),
		&pdata->_GetAcceptExSockaddrs,
		sizeof(pdata->_GetAcceptExSockaddrs),
		&bytes, NULL, NULL);
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&ConnectExGuid, sizeof(GUID),
		&pdata->_ConnectEx,
		sizeof(pdata->_ConnectEx),
		&bytes, NULL, NULL);
	if (pdata->_ConnectEx == NULL) {
	    /* Use our lame Win2K/NT4 emulation for this. */
	    pdata->_ConnectEx = OurConnectEx;
	}
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&DisconnectExGuid, sizeof(GUID),
		&pdata->_DisconnectEx,
		sizeof(pdata->_DisconnectEx),
		&bytes, NULL, NULL);
	if (pdata->_DisconnectEx == NULL) {
	    /* Use our lame Win2K/NT4 emulation for this. */
	    pdata->_DisconnectEx = OurDisconnectEx;
	}
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&TransmitFileGuid, sizeof(GUID),
		&pdata->_TransmitFile,
		sizeof(pdata->_TransmitFile),
		&bytes, NULL, NULL);
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&TransmitPacketsGuid, sizeof(GUID),
		&pdata->_TransmitPackets,
		sizeof(pdata->_TransmitPackets),
		&bytes, NULL, NULL);
	if (pdata->_TransmitPackets == NULL) {
	    /* There is no Win2K/NT4 emulation for this. */
	    pdata->_TransmitPackets = NULL;
	}
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&WSARecvMsgGuid, sizeof(GUID),
		&pdata->_WSARecvMsg,
		sizeof(pdata->_WSARecvMsg),
		&bytes, NULL, NULL);
	if (pdata->_WSARecvMsg == NULL) {
	    /* There is no Win2K/NT4 emulation for this. */
	    pdata->_WSARecvMsg = NULL;
	}
    }
}

/*
 *-----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a ADDRINFO structure for a host and
 *	port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to
 *	an IP address.
 *
 * Side effects:
 *	Fills in the *ADDRINFO structure.
 *
 *-----------------------------------------------------------------------
 */

int
CreateSocketAddress (
     const char *addr,
     const char *port,
     LPADDRINFO inhints,
     LPADDRINFO *paddrinfo)
{
    ADDRINFO hints;
    LPADDRINFO phints;
    int result;

    if (inhints != NULL) {
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);
	hints.ai_family = inhints->ai_family;
	hints.ai_socktype = inhints->ai_socktype;
	hints.ai_protocol = inhints->ai_protocol;
	phints = &hints;
    } else {
	phints = NULL;
    }

    result = getaddrinfo(addr, port, phints, paddrinfo);

    if (result != 0) {
	/* an older platSDK needed this; the current doesn't.
	WSASetLastError(result); */
	return 0;
    }
    return 1;
}

void
FreeSocketAddress (LPADDRINFO addrinfo)
{
    freeaddrinfo(addrinfo);
}

/*
 *----------------------------------------------------------------------
 *
 * FindProtocolInfo --
 *
 *	This function searches the Winsock catalog for a provider of the
 *	given address family, socket type, protocol and flags. The flags
 *	field is a bitwise OR of all the attributes that you request
 *	such as multipoint or QOS support.
 *
 * Results:
 *	TRUE if pinfo was set or FALSE for an error.
 *
 * Side effects:
 *	None known.
 *
 *----------------------------------------------------------------------
 */

BOOL FindProtocolInfo(int af, int type, 
    int protocol, DWORD flags, WSAPROTOCOL_INFO *pinfo)
{
    DWORD protosz = 0, nprotos, i;
    WSAPROTOCOL_INFO *buf = NULL;
    int ret;

    /*
     * Find out the size of the buffer needed to enumerate
     * all entries.
     */
    ret = WSAEnumProtocols(NULL, NULL, &protosz);
    if (ret != SOCKET_ERROR) {
        return FALSE;  
    }
    /* Allocate the necessary buffer */
    buf = (WSAPROTOCOL_INFO *) IocpAlloc(protosz);
    if (!buf) {
        return FALSE;
    }
    nprotos = protosz / sizeof(WSAPROTOCOL_INFO);
    /* Make the real call */
    ret = WSAEnumProtocols(NULL, buf, &protosz);
    if (ret == SOCKET_ERROR) {
        IocpFree(buf);
        return FALSE;
    }
    /*
     * Search throught the catalog entries returned for the 
     * requested attributes.
     */
    for(i=0; i < nprotos ;i++) {
        if ((buf[i].iAddressFamily == af) &&
		(buf[i].iSocketType == type) &&
		(buf[i].iProtocol == protocol)) {
            if ((buf[i].dwServiceFlags1 & flags) == flags) {
                memcpy(pinfo, &buf[i], sizeof(WSAPROTOCOL_INFO));
                IocpFree(buf);
                return TRUE;
            }
        }
    }
    /* LSP with flag combination not found.. */
    WSASetLastError(WSAEOPNOTSUPP);
    IocpFree(buf);
    return FALSE;
}


/* =================================================================== */
/* ========================= Error mappings ========================== */

/*
 * The following table contains the mapping from WinSock errors to
 * errno errors.
 */

static int wsaErrorTable1[] = {
    EWOULDBLOCK,	/* WSAEWOULDBLOCK */
    EINPROGRESS,	/* WSAEINPROGRESS */
    EALREADY,		/* WSAEALREADY */
    ENOTSOCK,		/* WSAENOTSOCK */
    EDESTADDRREQ,	/* WSAEDESTADDRREQ */
    EMSGSIZE,		/* WSAEMSGSIZE */
    EPROTOTYPE,		/* WSAEPROTOTYPE */
    ENOPROTOOPT,	/* WSAENOPROTOOPT */
    EPROTONOSUPPORT,	/* WSAEPROTONOSUPPORT */
    ESOCKTNOSUPPORT,	/* WSAESOCKTNOSUPPORT */
    EOPNOTSUPP,		/* WSAEOPNOTSUPP */
    EPFNOSUPPORT,	/* WSAEPFNOSUPPORT */
    EAFNOSUPPORT,	/* WSAEAFNOSUPPORT */
    EADDRINUSE,		/* WSAEADDRINUSE */
    EADDRNOTAVAIL,	/* WSAEADDRNOTAVAIL */
    ENETDOWN,		/* WSAENETDOWN */
    ENETUNREACH,	/* WSAENETUNREACH */
    ENETRESET,		/* WSAENETRESET */
    ECONNABORTED,	/* WSAECONNABORTED */
    ECONNRESET,		/* WSAECONNRESET */
    ENOBUFS,		/* WSAENOBUFS */
    EISCONN,		/* WSAEISCONN */
    ENOTCONN,		/* WSAENOTCONN */
    ESHUTDOWN,		/* WSAESHUTDOWN */
    ETOOMANYREFS,	/* WSAETOOMANYREFS */
    ETIMEDOUT,		/* WSAETIMEDOUT */
    ECONNREFUSED,	/* WSAECONNREFUSED */
    ELOOP,		/* WSAELOOP */
    ENAMETOOLONG,	/* WSAENAMETOOLONG */
    EHOSTDOWN,		/* WSAEHOSTDOWN */
    EHOSTUNREACH,	/* WSAEHOSTUNREACH */
    ENOTEMPTY,		/* WSAENOTEMPTY */
    EAGAIN,		/* WSAEPROCLIM */
    EUSERS,		/* WSAEUSERS */
    EDQUOT,		/* WSAEDQUOT */
    ESTALE,		/* WSAESTALE */
    EREMOTE,		/* WSAEREMOTE */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation, yet.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable2[] = {
    EINVAL,		/* WSASYSNOTREADY	    WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable. */
    EINVAL,		/* WSAVERNOTSUPPORTED	    The Windows Sockets version requested is not supported. */
    EINVAL,		/* WSANOTINITIALISED	    Either the application has not called WSAStartup, or WSAStartup failed. */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    ENOTCONN,		/* WSAEDISCON		    Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence. */
    EINVAL,		/* WSAENOMORE		    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSAECANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEINVALIDPROCTABLE	    The procedure call table is invalid. */
    EINVAL,		/* WSAEINVALIDPROVIDER	    The requested service provider is invalid. */
    EINVAL,		/* WSAEPROVIDERFAILEDINIT   The requested service provider could not be loaded or initialized. */
    EINVAL,		/* WSASYSCALLFAILURE	    A system call that should never fail has failed. */
    EINVAL,		/* WSASERVICE_NOT_FOUND	    No such service is known. The service cannot be found in the specified name space. */
    EINVAL,		/* WSATYPE_NOT_FOUND	    The specified class was not found. */
    EINVAL,		/* WSA_E_NO_MORE	    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSA_E_CANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEREFUSED		    A database query failed because it was actively refused. */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation, yet.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable3[] = {
    EINVAL,	/* WSAHOST_NOT_FOUND,	Authoritative Answer: Host not found */
    EINVAL,	/* WSATRY_AGAIN,	Non-Authoritative: Host not found, or SERVERFAIL */
    EINVAL,	/* WSANO_RECOVERY,	Non-recoverable errors, FORMERR, REFUSED, NOTIMP */
    EINVAL,	/* WSANO_DATA,		Valid name, no data record of requested type */
    EINVAL,	/* WSA_QOS_RECEIVERS,		at least one Reserve has arrived */
    EINVAL,	/* WSA_QOS_SENDERS,		at least one Path has arrived */
    EINVAL,	/* WSA_QOS_NO_SENDERS,		there are no senders */
    EINVAL,	/* WSA_QOS_NO_RECEIVERS,	there are no receivers */
    EINVAL,	/* WSA_QOS_REQUEST_CONFIRMED,	Reserve has been confirmed */
    EINVAL,	/* WSA_QOS_ADMISSION_FAILURE,	error due to lack of resources */
    EINVAL,	/* WSA_QOS_POLICY_FAILURE,	rejected for administrative reasons - bad credentials */
    EINVAL,	/* WSA_QOS_BAD_STYLE,		unknown or conflicting style */
    EINVAL,	/* WSA_QOS_BAD_OBJECT,		problem with some part of the filterspec or providerspecific buffer in general */
    EINVAL,	/* WSA_QOS_TRAFFIC_CTRL_ERROR,	problem with some part of the flowspec */
    EINVAL,	/* WSA_QOS_GENERIC_ERROR,	general error */
    EINVAL,	/* WSA_QOS_ESERVICETYPE,	invalid service type in flowspec */
    EINVAL,	/* WSA_QOS_EFLOWSPEC,		invalid flowspec */
    EINVAL,	/* WSA_QOS_EPROVSPECBUF,	invalid provider specific buffer */
    EINVAL,	/* WSA_QOS_EFILTERSTYLE,	invalid filter style */
    EINVAL,	/* WSA_QOS_EFILTERTYPE,		invalid filter type */
    EINVAL,	/* WSA_QOS_EFILTERCOUNT,	incorrect number of filters */
    EINVAL,	/* WSA_QOS_EOBJLENGTH,		invalid object length */
    EINVAL,	/* WSA_QOS_EFLOWCOUNT,		incorrect number of flows */
    EINVAL,	/* WSA_QOS_EUNKOWNPSOBJ,	unknown object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPOLICYOBJ,		invalid policy object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EFLOWDESC,		invalid flow descriptor in the list */
    EINVAL,	/* WSA_QOS_EPSFLOWSPEC,		inconsistent flow spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPSFILTERSPEC,	invalid filter spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESDMODEOBJ,		invalid shape discard mode object in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESHAPERATEOBJ,	invalid shaping rate object in provider specific buffer */
    EINVAL,	/* WSA_QOS_RESERVED_PETYPE,	reserved policy element in provider specific buffer */
};

/*
 *----------------------------------------------------------------------
 *
 * IocpWinConvertWSAError --
 *
 *	This routine converts a WinSock error into an errno value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the errno global variable.
 *
 *----------------------------------------------------------------------
 */

void
IocpWinConvertWSAError(
    DWORD errCode)	/* Win32 WSA error code. */		
{
    if ((errCode >= WSAEWOULDBLOCK) && (errCode <= WSAEREMOTE)) {
	Tcl_SetErrno(wsaErrorTable1[errCode - WSAEWOULDBLOCK]);
    } else if ((errCode >= WSASYSNOTREADY) && (errCode <= WSAEREFUSED)) {
	Tcl_SetErrno(wsaErrorTable2[errCode - WSASYSNOTREADY]);
    } else if ((errCode >= WSAHOST_NOT_FOUND) && (errCode <= WSA_QOS_RESERVED_PETYPE)) {
	Tcl_SetErrno(wsaErrorTable3[errCode - WSAHOST_NOT_FOUND]);
    } else {
	Tcl_SetErrno(EINVAL);
    }
}


/* =================================================================== */
/* ========================= Bad hack jobs! ========================== */


typedef struct {
    SOCKET s;
    LPSOCKADDR name;
    int namelen;
    PVOID lpSendBuffer;
    LPOVERLAPPED lpOverlapped;

} ConnectJob;

DWORD WINAPI
ConnectThread (LPVOID lpParam)
{
    int code;
    ConnectJob *job = lpParam;
    BufferInfo *bufPtr;

    bufPtr = CONTAINING_RECORD(job->lpOverlapped, BufferInfo, ol);
    code = connect(job->s, job->name, job->namelen);
    if (code == SOCKET_ERROR) {
	bufPtr->WSAerr = WSAGetLastError();
    }
    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
	    (ULONG_PTR) bufPtr->parent, job->lpOverlapped);
    IocpFree(job->name);
    IocpFree(job);
    return 0;
}

BOOL PASCAL
OurConnectEx (
    SOCKET s,
    const struct sockaddr* name,
    int namelen,
    PVOID lpSendBuffer,
    DWORD dwSendDataLength,
    LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped)
{
    ConnectJob *job;
    HANDLE thread;
    DWORD dummy;

    // 1) Create a thread and have the thread do the work.
    //    Return thread start status.
    // 2) thread will do a blocking connect() and possible send()
    //    should lpSendBuffer not be NULL and dwSendDataLength greater
    //    than zero.
    // 3) Notify the completion port with PostQueuedCompletionStatus().
    //    We don't exactly know the port associated to us, so assume the
    //    one we ALWAYS use (insider knowledge of ourselves).
    // 4) exit thread.

    job = IocpAlloc(sizeof(ConnectJob));
    job->s = s;
    job->name = IocpAlloc(namelen);
    memcpy(job->name, name, namelen);
    job->namelen = namelen;
//    job->lpSendBuffer = lpSendBuffer;    Not supported here.
    job->lpOverlapped = lpOverlapped;

    thread = CreateThread(NULL, 256, ConnectThread, job, 0, &dummy);

    if (thread) {
	/* remove local reference so the thread cleans up after it exits. */
	CloseHandle(thread);
	WSASetLastError(WSA_IO_PENDING);
    } else {
	WSASetLastError(GetLastError());
    }
    return FALSE;
}

BOOL PASCAL
OurDisconnectEx (
    SOCKET hSocket,
    LPOVERLAPPED lpOverlapped,
    DWORD dwFlags,
    DWORD reserved)
{
    BufferInfo *bufPtr;
    bufPtr = CONTAINING_RECORD(lpOverlapped, BufferInfo, ol);
    WSASendDisconnect(hSocket, NULL);
    PostQueuedCompletionStatus(IocpSubSystem.port, 0,
	    (ULONG_PTR) bufPtr->parent, lpOverlapped);
    WSASetLastError(WSA_IO_PENDING);
    return FALSE;
}

/*
 * compare and swap functions
 */

#if !defined(_MSC_VER)
static inline char CAS (volatile void * addr, volatile void * value, void * newvalue) 
{
    register char ret;
    __asm__ __volatile__ (
	"# CAS \n\t"
	"lock ; cmpxchg %2, (%1) \n\t"
	"sete %0                 \n\t"
	:"=a" (ret)
	:"c" (addr), "d" (newvalue), "a" (value)
    );
    return ret;
}

static inline char CAS2 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char ret;
    __asm__ __volatile__ (
	"# CAS2 \n\t"
	"lock ;  cmpxchg8b (%1) \n\t"
	"sete %0                \n\t"
	:"=a" (ret)
	:"D" (addr), "d" (v2), "a" (v1), "b" (n1), "c" (n2)
    );
    return ret;
}
#else
static __inline char CAS (volatile void * addr, volatile void * value, void * newvalue) 
{
    register char c;
    __asm {
	push	ebx
	push	esi
	mov	esi, addr
	mov	eax, value
	mov	ebx, newvalue
	lock	cmpxchg dword ptr [esi], ebx
	sete	c
	pop	esi
	pop	ebx
    }
    return c;
}

static __inline char CAS2 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char c;
    __asm {
	push	ebx
	push	ecx
	push	edx
	push	esi
	mov	esi, addr
	mov	eax, v1
	mov	ebx, n1
	mov	ecx, n2
	mov	edx, v2
	lock    cmpxchg8b qword ptr [esi]
	sete	c
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
    }
    return c;
}
#endif


/* Bitmask macros. */
#define mask_a( mask, val ) if ( ( mask & val ) != val ) { mask |= val; }
#define mask_d( mask, val ) if ( ( mask & val ) == val ) { mask &= ~(val); }
#define mask_y( mask, val ) ( mask & val ) == val
#define mask_n( mask, val ) ( mask & val ) != val


/*
 *----------------------------------------------------------------------
 *
 * IocpLLCreate --
 *
 *	Creates a linked list.
 *
 * Results:
 *	pointer to the new one or NULL for error.
 *
 * Side effects:
 *	None known.
 *
 *----------------------------------------------------------------------
 */

LPLLIST
IocpLLCreate (void)
{   
    LPLLIST ll;
    
    /* Alloc a linked list. */
    if (!(ll = IocpAlloc(sizeof(LLIST)))) {
	return NULL;
    }
    if (!InitializeCriticalSectionAndSpinCount(&ll->lock, 4000)) {
	IocpFree(ll);
	return NULL;
    }
    ll->haveData = CreateEvent(NULL, TRUE, FALSE, NULL);  /* manual reset */
    if (ll->haveData == INVALID_HANDLE_VALUE) {
	DeleteCriticalSection(&ll->lock);
	IocpFree(ll);
	return NULL;
    }
    ll->back = ll->front = 0L;
    ll->lCount = 0;
    return ll;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLDestroy --
 *
 *	Destroys a linked list.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	Nodes aren't destroyed.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLDestroy (
    LPLLIST ll)
{
    if (!ll) {
	return FALSE;
    }
    DeleteCriticalSection(&ll->lock);
    CloseHandle(ll->haveData);
    return IocpFree(ll);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the end of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushBack(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode,
    DWORD dwState)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->back->next = pnode;
	tmp = ll->back;
	ll->back = pnode;
	ll->back->prev = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    SetEvent(ll->haveData);
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the front of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushFront(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode,
    DWORD dwState)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->front->prev = pnode;
	tmp = ll->front;
	ll->front = pnode;
	ll->front->next = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    SetEvent(ll->haveData);
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopAll --
 *
 *	Removes all items from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node(s) with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPopAll(
    LPLLIST ll,
    LPLLNODE snode,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (snode && snode->ll) {
	ll = snode->ll;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && ! ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    if (snode) {
	tmp1 = snode;
    }
    while(tmp1) {
	tmp2 = tmp1->next;
	/* Delete (or blank) the node and decrement the counter. */
        if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	    IocpLLNodeDestroy(tmp1);
	} else {
	    tmp1->ll = NULL;
	    tmp1->next = NULL; 
            tmp1->prev = NULL;
	}
        ll->lCount--;
	tmp1 = tmp2;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}


BOOL 
IocpLLPopAllCompare(
    LPLLIST ll,
    LPVOID lpItem,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    while(tmp1) {
	tmp2 = tmp1->next;
	if (tmp1->lpItem == lpItem) {
	    IocpLLPop(tmp1, IOCP_LL_NOLOCK | dwState);
	}
	tmp1 = tmp2;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPop --
 *
 *	Removes an item from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPop(
    LPLLNODE node,
    DWORD dwState)
{
    LPLLIST ll;
    LPLLNODE prev, next;

    //Ready the node
    if (!node || !node->ll) {
	return FALSE;
    }
    ll = node->ll;
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    prev = node->prev;
    next = node->next;

    /* Check for only item. */
    if (!prev & !next) {
	ll->front = NULL;
	ll->back = NULL;
    /* Check for front of list. */
    } else if (!prev && next) {
	next->prev = NULL;
	ll->front = next;
    /* Check for back of list. */
    } else if (prev && !next) {
	prev->next = NULL;
	ll->back = prev;
    /* Check for middle of list. */
    } else if (prev && next) {
	next->prev = prev;
	prev->next = next;
    }

    /* Delete the node when IOCP_LL_NODESTROY is not specified. */
    if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	IocpLLNodeDestroy(node);
    } else {
	node->ll = NULL;
	node->next = NULL; 
        node->prev = NULL;
    }
    ll->lCount--;
    if (ll->lCount <= 0) {
	ll->front = NULL;
	ll->back = NULL;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLNodeDestroy --
 *
 *	Destroys a node.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	memory returns to the system.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLNodeDestroy (LPLLNODE node)
{
    return IocpFree(node);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopBack --
 *
 *	Removes the item at the back of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopBack(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID data;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->lCount) {
	if (timeout) {
	    DWORD dwWait;
	    ResetEvent(ll->haveData);
	    LeaveCriticalSection(&ll->lock);
	    dwWait = WaitForSingleObject(ll->haveData, timeout);
	    if (dwWait == WAIT_OBJECT_0) {
		/* wait succedded, fall through and remove one. */
		EnterCriticalSection(&ll->lock);
	    } else {
		/* wait failed */
		return NULL;
	    }
	} else {
	    LeaveCriticalSection(&ll->lock);
	    return NULL;
	}
    }
    tmp = ll->back;
    data = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopFront --
 *
 *	Removes the item at the front of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopFront(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID data;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->lCount) {
	if (timeout) {
	    DWORD dwWait;
	    ResetEvent(ll->haveData);
	    LeaveCriticalSection(&ll->lock);
	    dwWait = WaitForSingleObject(ll->haveData, timeout);
	    if (dwWait == WAIT_OBJECT_0) {
		/* wait succedded, fall through and remove one. */
		EnterCriticalSection(&ll->lock);
	    } else {
		/* wait failed */
		return NULL;
	    }
	} else {
	    LeaveCriticalSection(&ll->lock);
	    return NULL;
	}
    }
    tmp = ll->front;
    data = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLIsNotEmpty --
 *
 *	self explanatory.
 *
 * Results:
 *	Boolean for if the linked-list has entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLIsNotEmpty (LPLLIST ll)
{
    BOOL b;
    if (!ll) {
	return FALSE;
    }
    EnterCriticalSection(&ll->lock);
    b = (ll->lCount != 0);
    LeaveCriticalSection(&ll->lock);
    return b;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLGetCount --
 *
 *	How many nodes are on the list?
 *
 * Results:
 *	Count of entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

SIZE_T
IocpLLGetCount (LPLLIST ll)
{
    SIZE_T c;
    if (!ll) {
	return 0;
    }
    EnterCriticalSection(&ll->lock);
    c = ll->lCount;
    LeaveCriticalSection(&ll->lock);
    return c;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
