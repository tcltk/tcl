/* reserved for Pat Thoyts and tcludp...  This needs big factoral work! */

static WS2ProtocolData udp4ProtoData = {
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN),
    DecodeIpSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static WS2ProtocolData udp6ProtoData = {
    AF_INET6,
    SOCK_DGRAM,
    IPPROTO_UDP,
    sizeof(SOCKADDR_IN6),
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
				CONST char *port, CONST char *host,
				CONST char *myaddr, CONST char *myport);

/*
 *----------------------------------------------------------------------
 *
 * Iocp_OpenUdp4Client --
 *
 *	Opens a UDP socket and creates a channel around it.
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
Iocp_OpenUdpSocket (
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    const char *port,		/* Port (number|service) to open. */
    const char *host,		/* Host on which to open port. */
    const char *myaddr,		/* Client-side address */
    const char *myport)		/* Client-side port (number|service).*/
{
    SocketInfo *infoPtr;
    char channelName[4 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    infoPtr = CreateUdpSocket(interp, port, host, myaddr, myport);
    if (infoPtr == NULL) {
	return NULL;
    }

    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "iocp%d", infoPtr->socket);

    infoPtr->channel = Tcl_CreateChannel(&IocpChannelType, channelName,
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
    // Had to add this!
    if (Tcl_SetChannelOption(NULL, infoPtr->channel, "-blocking", "0")
	    == TCL_ERROR) {
	Tcl_Close((Tcl_Interp *) NULL, infoPtr->channel);
	return (Tcl_Channel) NULL;
    }
    return infoPtr->channel;
}


#if 0
static SocketInfo *
CreateUdpSocket (
    Tcl_Interp *interp,
    const char *port,
    const char *host,
    const char *myaddr,
    const char *myport)
{
    LPADDRINFO hostaddr;	/* Socket address */
    LPADDRINFO mysockaddr;	/* Socket address for client */
    SOCKET sock = INVALID_SOCKET;
    SocketInfo *infoPtr;	/* The returned value. */
    BufferInfo *bufPtr;		/* The returned value. */
    DWORD bytes, WSAerr;
    BOOL code;
    int i;
    WS2ProtocolData *pdata;
    ADDRINFO hints;
    LPADDRINFO addr;
    WSAPROTOCOL_INFO wpi;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);


    ZeroMemory(&hints, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    /* discover both/either ipv6 and ipv4. */
    if (host == NULL && !strcmp(port, "0")) {
	/* Win2K hack.  Ask for port 1, then set to 0 so getaddrinfo() doesn't bomb. */
	if (! CreateSocketAddress(host, "1", &hints, &hostaddr)) {
	    goto error1;
	}
	addr = hostaddr;
	while (addr) {
	    if (addr->ai_family == AF_INET) {
		((LPSOCKADDR_IN)addr->ai_addr)->sin_port = 0;
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
    /* 
     * If we have more than one and being passive (bind() for a listen()),
     * choose ipv4.
     */
    if (addr->ai_next && host == NULL) {
	while (addr->ai_family != AF_INET && addr->ai_next) {
	    addr = addr->ai_next;
	}
    }

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
	    ((LPSOCKADDR_IN)mysockaddr->ai_addr)->sin_port = 0;
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
	goto error;
    }

    /*
     * Win-NT has a misfeature that sockets are inherited in child
     * processes by default.  Turn off the inherit bit.
     */

    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    infoPtr = NewSocketInfo(sock);
    IocpInitProtocolData(socket, pdata);
    infoPtr->proto = &udp4ProtoData;


    /* Info needed to get back to this thread. */
    infoPtr->tsdHome = tsdPtr;


    /* Associate the socket and its SocketInfo struct to the completion
     * port.  Implies an automatic set to non-blocking. */
    if (CreateIoCompletionPort((HANDLE)sock, IocpSubSystem.port,
	    (ULONG_PTR)infoPtr, 0) == NULL) {
	winSock.WSASetLastError(GetLastError());
	goto error;
    }

    if (server) {
	/*
	 * Bind to the specified port.  Note that we must not call
	 * setsockopt with SO_REUSEADDR because Microsoft allows
	 * addresses to be reused even if they are still in use.
         *
         * Bind should not be affected by the socket having already been
         * set into nonblocking mode. If there is trouble, this is one
	 * place to look for bugs.
	 */
    
	if (winSock.bind(sock, hostaddr->ai_addr,
		hostaddr->ai_addrlen) == SOCKET_ERROR) {
            goto error;
        }

	FreeSocketAddress(hostaddr);

	/* create the queue for holding ready receives. */
	infoPtr->llPendingRecv = IocpLLCreate();

	bufPtr = GetBufferObj(infoPtr, 256);
	if (PostOverlappedRecv(infoPtr, bufPtr) != NO_ERROR) {
	    goto error;
        }

    } else {

        /*
         * Try to bind to a local port, if specified.
         */

	if (myaddr != NULL || myport != 0) { 
	    if (winSock.bind(sock, mysockaddr->ai_addr,
		    mysockaddr->ai_addrlen) == SOCKET_ERROR) {
		goto error;
	    }
	    FreeSocketAddress(mysockaddr);
	}            

	/*
	 * Attempt to connect to the remote.
	 */

	if (async) {
	    bufPtr = GetBufferObj(infoPtr, 0);
	    bufPtr->operation = OP_CONNECT;

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
    SetLastError(WSAGetLastError());
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open socket: ",
		Tcl_WinError(interp), NULL);
    }
    FreeSocketInfo(infoPtr);
    return NULL;
}
#else
static SocketInfo *
CreateUdpSocket (
    Tcl_Interp *interp,
    CONST char *port,
    CONST char *host,
    CONST char *myaddr,
    CONST char *myport)
{
    return NULL;
}
#endif