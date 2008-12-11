/* reserved for Pat Thoyts and tcludp */
#include "tclWinInt.h"
#include "tclWinsockCore.h"

/* ISO hack for dumb VC++ */
#ifdef _MSC_VER
#define   snprintf	_snprintf
#endif

static Tcl_NetCreateClientProc OpenUdpClientChannel;

WS2ProtocolData udpAnyProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN),
    AF_UNSPEC,
    OpenUdpClientChannel,
    NULL,
    DecodeIpSockaddr,
    NULL, /* resolver */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

WS2ProtocolData udp4ProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN),
    AF_INET,
    OpenUdpClientChannel,
    NULL,
    DecodeIpSockaddr,
    NULL, /* resolver */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

WS2ProtocolData udp6ProtoData = {
    AF_INET6,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN6),
    AF_INET6,
    OpenUdpClientChannel,
    NULL,
    DecodeIpSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateUdpSocket(Tcl_Interp *interp,
				const char *port, const char *host,
				const char *myaddr,
				const char *myport, int afhint);


Tcl_Channel
OpenUdpClientChannel(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    const char *port,		/* Port (number|service) to open. */
    const char *host,		/* Host on which to open port. */
    const char *myaddr,		/* Client-side address. */
    const char *myport,		/* Client-side port (number|service).*/
    int async,			/* If nonzero, should connect
				 * client socket asynchronously. */
    int afhint)
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];


    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateUdpSocket(interp, port, host, myaddr, myport, afhint);
    if (infoPtr == NULL) {
	return NULL;
    }
    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "sock%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpPacketChannelType, channelName,
	    (ClientData) infoPtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-translation",
	    "binary") == TCL_ERROR) {
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

SocketInfo *
CreateUdpSocket(
	Tcl_Interp *interp,
	const char *port,
	const char *host,
	const char *myaddr,
	const char *myport,
	int afhint)
{
    u_long flag = 1;		/* Indicates nonblocking mode. */
    int asyncConnect = 0;	/* Will be 1 if async connect is
				 * in progress. */
    LPADDRINFO hostaddr;	/* Socket address for peer */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr = NULL;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD reuse;
    BOOL code;
    int i;
    WS2ProtocolData *pdata;
    ADDRINFO hints;
    LPADDRINFO addr;
    WSAPROTOCOL_INFO wpi;
    ThreadSpecificData *tsdPtr = InitSockets();

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = afhint;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (host == NULL && !strcmp(port, "0")) {
	/* Win2K hack.  Ask for port 1, then set to 0 so getaddrinfo() doesn't bomb. */
	if (! CreateSocketAddress(host, "1", &hints, &hostaddr)) {
	    goto error1;
	}
	addr = hostaddr;
	while (addr) {
	    if (addr->ai_family == AF_INET) {
		((LPSOCKADDR_IN)addr->ai_addr)->sin_port = INADDR_ANY;
	    } else {
		IN6ADDR_SETANY((LPSOCKADDR_IN6) addr->ai_addr);
	    }
	    addr = addr->ai_next;
	}
    } else {
	if (! CreateSocketAddress(host, port, &hints, &hostaddr)) {
	    goto error1;
	}
    }
    addr = hostaddr;

    if (myaddr != NULL || myport != NULL) {
	if (!CreateSocketAddress(myaddr, myport, addr, &mysockaddr)) {
	    goto error2;
	}
    } else if (1) {
	/* Win2K hack.  Ask for port 1, then set to 0 so getaddrinfo() doesn't bomb. */
	if (!CreateSocketAddress(NULL, "1", addr, &mysockaddr)) {
	    goto error2;
	}
	if (mysockaddr->ai_family == AF_INET) {
	    ((LPSOCKADDR_IN)mysockaddr->ai_addr)->sin_port = INADDR_ANY;
	} else {
	    IN6ADDR_SETANY((LPSOCKADDR_IN6) mysockaddr->ai_addr);
	}
    }


    switch (addr->ai_family) {
    case AF_INET:
	pdata = &udp4ProtoData; break;
    case AF_INET6:
	pdata = &udp6ProtoData; break;
    default:
	Tcl_Panic("very bad protocol family returned from getaddrinfo()");
    }

    code = FindProtocolInfo(pdata->af, pdata->type, pdata->protocol,
	    0 /*XP1_QOS_SUPPORTED*/, &wpi);
    if (code == FALSE) {
	goto error2;
    }

    sock = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
	    FROM_PROTOCOL_INFO, &wpi, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
	goto error2;
    }

    IocpInitProtocolData(sock, pdata);

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    /*
     * Turn off the internal send buffing.  We get more speed and are
     * more efficient by reducing memcpy calls as the stack will use
     * our overlapped buffers directly.
     */

    i = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
	    (const char *) &i, sizeof(int)) == SOCKET_ERROR) {
	goto error2;
    }

#if 1
    /*
     * Allow us to hijack, or be hijacked
     */
    reuse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
	    (const char *) &reuse, sizeof(DWORD)) == SOCKET_ERROR) {
	goto error2;
    }
#endif

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = pdata;

    /* Info needed to get back to this thread. */
    infoPtr->tsdHome = tsdPtr;

    /*
     * bind to a local address.  ConnectEx needs this.
     */

    if (bind(sock, mysockaddr->ai_addr,
	    mysockaddr->ai_addrlen) == SOCKET_ERROR) {
	FreeSocketAddress(mysockaddr);
	goto error2;
    }
    FreeSocketAddress(mysockaddr);

#if 0
    /*
     * Associate remote for filtering.
     */

    code = connect(sock, addr->ai_addr, addr->ai_addrlen);
    FreeSocketAddress(hostaddr);
    if (code == SOCKET_ERROR) {
	goto error1;
    }
#endif

    /*
     * Associate the socket and its SocketInfo struct to the
     * completion port.  This implies an automatic set to
     * non-blocking.  We emulate blocking to the Tcl side.
     */
    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
	    (ULONG_PTR)infoPtr, 0) == NULL) {
	WSASetLastError(GetLastError());
	goto error1;
    }

    infoPtr->llPendingRecv = IocpLLCreate();

    /* post IOCP_INITIAL_RECV_COUNT recvs. */
    for(i=0; i < IOCP_INITIAL_RECV_COUNT ;i++) {
	bufPtr = GetBufferObj(infoPtr,
		(infoPtr->recvMode == IOCP_RECVMODE_ZERO_BYTE ? 0 : IOCP_RECV_BUFSIZE));
	if (PostOverlappedRecv(infoPtr, bufPtr, 0, 0)) {
	    FreeBufferObj(bufPtr);
	    goto error1;
	}
    }
#if 0
    {
	int ret;
	QOS clientQos;
	DWORD dwBytes;

	ZeroMemory(&clientQos, sizeof(QOS));
	clientQos.SendingFlowspec = flowspec_g711;
	clientQos.ReceivingFlowspec =  flowspec_notraffic;
	clientQos.ProviderSpecific.buf = NULL;
	clientQos.ProviderSpecific.len = 0;

	/*clientQos.SendingFlowspec.ServiceType |= 
		SERVICE_NO_QOS_SIGNALING;
	clientQos.ReceivingFlowspec.ServiceType |= 
		SERVICE_NO_QOS_SIGNALING;*/

	ret = WSAIoctl(sock, SIO_SET_QOS, &clientQos, 
	    sizeof(clientQos), NULL, 0, &dwBytes, NULL, NULL);
    }
    bufPtr = GetBufferObj(infoPtr, QOS_BUFFER_SZ);
    PostOverlappedQOS(infoPtr, bufPtr);
#endif

    return infoPtr;

error2:
    FreeSocketAddress(hostaddr);
error1:
    TclWinConvertWSAError(WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_PosixError(interp), NULL);
    }
    FreeSocketInfo(infoPtr);
    return NULL;
}
