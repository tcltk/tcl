#include "tclWinInt.h"
#include "tclWinsockCore.h"

/* ISO hack for dumb VC++ */
#ifdef _MSC_VER
#define   snprintf	_snprintf
#endif



static SocketInfo *	CreateTcpSocket(Tcl_Interp *interp,
				const char *port, const char *host,
				int server, const char *myaddr,
				const char *myport, int async, int afhint);
static int		DoIpResolve (int hint, Tcl_Obj *question,
				Tcl_Obj **answers);
static int		isIp (Tcl_Obj *name);
static Tcl_NetCreateClientProc OpenTcpClientChannel;
static Tcl_NetCreateServerProc OpenTcpServerChannel;

#if 0
const FLOWSPEC flowspec_notraffic = {
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    SERVICETYPE_NOTRAFFIC,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED
};

const FLOWSPEC flowspec_g711 = {
    8500,
    680,
    17000,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    SERVICETYPE_CONTROLLEDLOAD,
    340,
    340
};

const FLOWSPEC flowspec_guaranteed = {
    17000,
    1260,
    34000,
    QOS_NOT_SPECIFIED,
    QOS_NOT_SPECIFIED,
    SERVICETYPE_GUARANTEED,
    340,
    340
};
#endif

WS2ProtocolData tcpAnyProtoData = {
    AF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    sizeof(SOCKADDR_IN),
    AF_UNSPEC,
    OpenTcpClientChannel,
    OpenTcpServerChannel,
    DecodeIpSockaddr,
    ResolveIp,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

WS2ProtocolData tcp4ProtoData = {
    AF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
    sizeof(SOCKADDR_IN),
    AF_INET,
    OpenTcpClientChannel,
    OpenTcpServerChannel,
    DecodeIpSockaddr,
    ResolveIp,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

WS2ProtocolData tcp6ProtoData = {
    AF_INET6,
    SOCK_STREAM,
    IPPROTO_TCP,
    sizeof(SOCKADDR_IN6),
    AF_INET6,
    OpenTcpClientChannel,
    OpenTcpServerChannel,
    DecodeIpSockaddr,
    ResolveIp,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


Tcl_Obj *isIpRE_IPv4 = NULL;
Tcl_Obj *isIpRE_IPv6 = NULL;
Tcl_Obj *isIpRE_IPv6Comp = NULL;
Tcl_Obj *isIpRE_4in6 = NULL;
Tcl_Obj *isIpRE_4in6Comp = NULL;



/*
 *----------------------------------------------------------------------
 *
 * DecodeIpSockaddr --
 *
 *	Decodes the info from the sockaddr_in.
 *
 * Results:
 *	A Tcl_Obj* as a list in the form {IP, name, port}.  name will
 *	be empty, if the resolve fails.  The caller must free the
 *	Tcl_Obj* when done.
 *
 * Side effects:
 *	Reverse resolve may block for an unknown amount of time.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
DecodeIpSockaddr (SocketInfo *info, LPSOCKADDR addr, int noLookup)
{
    char name[NI_MAXHOST];
    Tcl_Obj *result = Tcl_NewObj();

    /*
     * Get the numeric IP string.
     */
    if (getnameinfo(addr, info->proto->addrLen, name, NI_MAXHOST, NULL,
	    0, NI_NUMERICHOST) == NO_ERROR) {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(name, -1));
    } else {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj("", -1));
    }

    /*
     * Get the reverse resolved name through DNS from the IP.
     * This may block for an unknown amount of time.  Defaults
     * to a numeric if DNS can not resolve to a name.
     */
    if (noLookup) {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj("", -1));
    } else {
	if (getnameinfo(addr, info->proto->addrLen, name, NI_MAXHOST, NULL,
		0, 0) == NO_ERROR) {
	    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(name, -1));
	} else {
	    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj("", -1));
	}
    }

    /*
     * Get port numeric string.  Defaults to the port number if the
     * service name is unknown.
     */
    if (getnameinfo(addr, info->proto->addrLen, NULL, 0, name,
	    NI_MAXSERV, NI_NUMERICSERV) == NO_ERROR) {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(name, -1));
    } else {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj("", -1));
    }

    return result;
}

int
ResolveIp(
    Tcl_Interp *interp,	    /* for error reporting, maybe NULL. */
    int command,	    /* the command to perform. */
    int hint,		    /* adress family hint for getaddrinfo. */
    Tcl_Obj *question,	    /* the query to request. */
    Tcl_Obj **answers)
{
    switch (command) {
	case TCL_NET_RESOLVER_QUERY:
	    if (DoIpResolve(hint, question, answers) != TCL_OK) {
		goto error;
	    }
	    break;
	case TCL_NET_RESOLVER_REGISTER:
	case TCL_NET_RESOLVER_UNREGISTER:
	    WSASetLastError(WSAEOPNOTSUPP);
	    goto error;
    }
    return TCL_OK;

error:
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't resolve: ",
		Tcl_WinError(interp, WSAGetLastError()), NULL);
    }
    return TCL_ERROR;
}

int
DoIpResolve (int hint, Tcl_Obj *question, Tcl_Obj **answers)
{
    struct addrinfo hints;
    struct addrinfo *hostaddr, *addr;
    int result, type, len;
    CONST char *utf8Chars;
    Tcl_DString dnsTxt;
    Tcl_Encoding dnsEnc;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags  = 0;
    hints.ai_family = hint;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;

    /* RFC3490 */
    dnsEnc = Tcl_GetEncoding(NULL, "ascii");
    utf8Chars = Tcl_GetStringFromObj(question, &len);
    Tcl_UtfToExternalDString(dnsEnc, utf8Chars, len, &dnsTxt);

    if ((result = getaddrinfo(Tcl_DStringValue(&dnsTxt), NULL, &hints,
	    &hostaddr)) != 0) {
	goto error1;
    }

    if (*answers == NULL) {
	*answers = Tcl_NewObj();
    }

    if (isIp(question)) {
	/* question was a numeric IP, return a hostname. */
	type = NI_NAMEREQD;
    } else {
	/* question was a hostname, return a numeric IP. */
	type = NI_NUMERICHOST;
    }

    addr = hostaddr;
    while (addr != NULL) {
	char hostStr[NI_MAXHOST];
	int err;

	err = getnameinfo(addr->ai_addr, addr->ai_addrlen, hostStr,
		NI_MAXHOST, NULL, 0, type);

	if (err == 0) {
	    Tcl_ExternalToUtfDString(dnsEnc, hostStr, -1, &dnsTxt);
	    Tcl_ListObjAppendElement(NULL, *answers,
		    Tcl_NewStringObj(Tcl_DStringValue(&dnsTxt),
		    Tcl_DStringLength(&dnsTxt)));
	} else {
	    goto error2;
	}
	addr = addr->ai_next;
    }

    return TCL_OK;

error2:
    freeaddrinfo(hostaddr);
error1:
    Tcl_DStringFree(&dnsTxt);
    return TCL_ERROR;
}

int
isIp (Tcl_Obj *name)
{
    if (isIpRE_IPv4 == NULL) {
	/* lazy init */
	/* TODO: add a cleanup procedure through an exit handler or something */
	isIpRE_IPv4 = Tcl_NewStringObj("^((25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
	Tcl_IncrRefCount(isIpRE_IPv4);
	isIpRE_IPv6 = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}:){7}[[:xdigit:]]{1,4})$", -1);
	Tcl_IncrRefCount(isIpRE_IPv6);
	isIpRE_IPv6Comp = Tcl_NewStringObj("^((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)$", -1);
	Tcl_IncrRefCount(isIpRE_IPv6Comp);
	isIpRE_4in6 = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}:){6,6})(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
	Tcl_IncrRefCount(isIpRE_4in6);
	isIpRE_4in6Comp = Tcl_NewStringObj("^(((?:[[:xdigit:]]{1,4}(?::[[:xdigit:]]{1,4})*)?)::((?:[[:xdigit:]]{1,4}:)*)(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|[0-1]?\\d?\\d)){3})$", -1);
	Tcl_IncrRefCount(isIpRE_4in6Comp);
    }
    if (
	Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv4) ||
	Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6) ||
	Tcl_RegExpMatchObj(NULL, name, isIpRE_IPv6Comp) ||
	Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6) ||
	Tcl_RegExpMatchObj(NULL, name, isIpRE_4in6Comp))
    {
	return 1;
    }
    return 0;
}


Tcl_Channel
OpenTcpClientChannel(
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

    infoPtr = CreateTcpSocket(interp, port, host, 0, myaddr, myport, async, afhint);
    if (infoPtr == NULL) {
	return NULL;
    }
    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "sock%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpStreamChannelType, channelName,
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

Tcl_Channel
OpenTcpServerChannel(
    Tcl_Interp *interp,		/* For error reporting, may be NULL. */
    const char *port,		/* Port (number|service) to open. */
    const char *host,		/* Name of host for binding. */
    Tcl_SocketAcceptProc *acceptProc,
				/* Callback for accepting connections
				 * from new clients. */
    ClientData acceptProcData,	/* Data for the callback. */
    int afhint)
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateTcpSocket(interp, port, host, 1 /*server*/, NULL, 0, 0, afhint);
    if (infoPtr == NULL) {
	return NULL;
    }

    infoPtr->acceptProc = acceptProc;
    infoPtr->acceptProcData = acceptProcData;
    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "sock%lu", infoPtr->socket);
    infoPtr->channel = Tcl_CreateChannel(&IocpStreamChannelType, channelName,
	    (ClientData) infoPtr, 0);
    if (Tcl_SetChannelOption(interp, infoPtr->channel, "-eofchar", "")
	    == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
        return (Tcl_Channel) NULL;
    }

    return infoPtr->channel;
}

static SocketInfo *
CreateTcpSocket(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    const char *port,		/* Port to open. */
    const char *host,		/* Name of host on which to open port. */
    int server,			/* 1 if socket should be a server socket,
				 * else 0 for a client socket. */
    const char *myaddr,		/* Optional client-side address */
    const char *myport,		/* Optional client-side port */
    int async,			/* If nonzero, connect client socket
				 * asynchronously. */
    int afhint)
{
    u_long flag = 1;		/* Indicates nonblocking mode. */
    int asyncConnect = 0;	/* Will be 1 if async connect is
				 * in progress. */
    LPADDRINFO hostaddr;	/* Socket address */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr = NULL;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD bytes;
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
	if (!CreateSocketAddress(host, port, &hints, &hostaddr)) {
	    goto error1;
	}
    }
    addr = hostaddr;

    if (myaddr != NULL || myport != NULL) {
	if (!CreateSocketAddress(myaddr, myport, addr, &mysockaddr)) {
	    goto error2;
	}
    } else if (!server) {
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
	pdata = &tcp4ProtoData; break;
    case AF_INET6:
	pdata = &tcp6ProtoData; break;
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

    infoPtr = NewSocketInfo(sock);
    infoPtr->proto = pdata;

    /* Info needed to get back to this thread. */
    infoPtr->tsdHome = tsdPtr;

    if (server) {

	/*
	 * Associate the socket and its SocketInfo struct to the completion
	 * port.  Implies an automatic set to non-blocking.
	 */
	if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		(ULONG_PTR)infoPtr, 0) == NULL) {
	    WSASetLastError(GetLastError());
	    goto error2;
	}

	/*
	 * Bind to the specified port.  Note that we must not call
	 * setsockopt with SO_REUSEADDR because Microsoft allows
	 * addresses to be reused even if they are still in use.
         *
         * Bind should not be affected by the socket having already been
         * set into nonblocking mode. If there is trouble, this is one
	 * place to look for bugs.
	 */
    
	if (bind(sock, addr->ai_addr,
		addr->ai_addrlen) == SOCKET_ERROR) {
            goto error2;
        }

	FreeSocketAddress(hostaddr);

        /*
         * Set the maximum number of pending connect requests to the
         * max value allowed on each platform (Win32 and Win32s may be
         * different, and there may be differences between TCP/IP stacks).
         */
        
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
	    goto error1;
	}

	/* create the queue for holding ready ones */
	infoPtr->readyAccepts = IocpLLCreate();

	/* post default IOCP_INITIAL_ACCEPT_COUNT accepts. */
        for(i=0; i < IOCP_ACCEPT_CAP ;i++) {
	    BufferInfo *bufPtr;
	    bufPtr = GetBufferObj(infoPtr, 0);
	    if (PostOverlappedAccept(infoPtr, bufPtr, 0) != NO_ERROR) {
		/* Oh no, the AcceptEx failed. */
		FreeBufferObj(bufPtr);
		goto error1;
	    }
        }

    } else {

        /*
         * bind to a local address.  ConnectEx needs this.
         */

	if (bind(sock, mysockaddr->ai_addr,
		mysockaddr->ai_addrlen) == SOCKET_ERROR) {
	    FreeSocketAddress(mysockaddr);
	    goto error2;
	}
	FreeSocketAddress(mysockaddr);

	/*
	 * Attempt to connect to the remote.
	 */

	if (async) {
	    bufPtr = GetBufferObj(infoPtr, 0);
	    bufPtr->operation = OP_CONNECT;

	    /*
	     * Associate the socket and its SocketInfo struct to the
	     * completion port.  Implies an automatic set to
	     * non-blocking.
	     */
	    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
		    (ULONG_PTR)infoPtr, 0) == NULL) {
		WSASetLastError(GetLastError());
		goto error2;
	    }

	    InterlockedIncrement(&infoPtr->outstandingOps);

	    code = pdata->_ConnectEx(sock, addr->ai_addr,
		    addr->ai_addrlen, NULL, 0, &bytes, &bufPtr->ol);

	    FreeSocketAddress(hostaddr);

	    if (code == FALSE) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
		    InterlockedDecrement(&infoPtr->outstandingOps);
		    FreeBufferObj(bufPtr);
		    goto error1;
		}
	    }
	} else {
	    code = connect(sock, addr->ai_addr, addr->ai_addrlen);
	    FreeSocketAddress(hostaddr);
	    if (code == SOCKET_ERROR) {
		goto error1;
	    }

	    /* Associate the socket and its SocketInfo struct to the
	     * completion port.  Implies an automatic set to
	     * non-blocking. */
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
	}
    }

    return infoPtr;

error2:
    FreeSocketAddress(hostaddr);
error1:
    FreeSocketInfo(infoPtr);
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_WinError(interp, WSAGetLastError()), NULL);
    }
    return NULL;
}
