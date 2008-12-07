
/* 
 * Thread safe linked-list management state bitmasks.
 */
#define IOCP_LL_NOLOCK		(1<<0)
#define IOCP_LL_NODESTROY	(1<<1)

/* Linked-List node object. */
struct _ListNode;
struct _List;
typedef struct _ListNode {
    struct _ListNode *next;	/* node in back */
    struct _ListNode *prev;	/* node in back */
    struct _List *ll;		/* parent linked-list */
    LPVOID lpItem;		/* storage item */
} LLNODE, *LPLLNODE;

/* Linked-List object. */
typedef struct _List {
    struct _ListNode *front;	/* head of list */
    struct _ListNode *back;	/* tail of list */
    LONG lCount;		/* nodes contained */
    CRITICAL_SECTION lock;	/* accessor lock */
    HANDLE haveData;		/* event when data is added to an empty list */
} LLIST, *LPLLIST;

/* forward reference */
struct SocketInfo;

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
    SIZE_T used;	    /* Length of the buffer used (post operation) */
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

#define QOS_BUFFER_SZ 16000

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

/* forward references. */
struct WS2ProtocolData;
typedef struct ThreadSpecificData {
    Tcl_ThreadId threadId;
    LPLLIST readySockets;
    LPLLIST deadSockets;
} ThreadSpecificData;
extern Tcl_ThreadDataKey dataKey;

#include <pshpack4.h>

typedef struct SocketInfo {
    Tcl_Channel channel;	    /* Tcl channel for this socket. */
    SOCKET socket;		    /* Windows SOCKET handle. */
    DWORD flags;		    /* info about this socket. */

    /* we need 32-bit alignment for these: */
    volatile LONG markedReady;	    /* indicates we are on the readySockets list.
				     * Access must be protected within the
				     * tsdPtr->readySockets->lock critical
				     * section. */
    volatile LONG outstandingOps;   /* Count of all overlapped operations. */
    volatile LONG outstandingSends; /* Count of overlapped WSASend() operations. */
    volatile LONG outstandingSendCap; /* Limit of outstanding overlapped WSASend
				     * operations allowed. */
    volatile LONG outstandingAccepts; /* Count of overlapped AcceptEx() operations. */
    volatile LONG outstandingAcceptCap; /* Limit of outstanding overlapped AcceptEx
				     * operations allowed. */
    volatile LONG outstandingRecvs; /* Count of overlapped WSARecv() operations. */
    volatile LONG outstandingRecvCap; /* Limit of outstanding overlapped WSARecv
				     * operations allowed. */
    SIZE_T outstandingRecvBufferCap; /* limit to how many unread buffers
				     * in llPendingRecv are allowed. */
    int needRecvRestart;	    /* When the buffer cap is hit, this is set. */

    enum IocpRecvMode recvMode;	    /* mode in use */

    int watchMask;		    /* events we are interested in. */
    struct WS2ProtocolData *proto;  /* Network protocol info. */

    CRITICAL_SECTION tsdLock;	    /* accessor for the TSD block */
    ThreadSpecificData *tsdHome;    /* TSD block for getting back to our
				     * origin. */
    /* For listening sockets: */
    LPLLIST readyAccepts;	    /* Ready accepts() in queue. */
    Tcl_SocketAcceptProc *acceptProc;  /* Proc to call on accept. */
    ClientData acceptProcData;	    /* The data for the accept proc. */

    /* Neutral SOCKADDR data: */
    LPSOCKADDR localAddr;	    /* Local sockaddr. */
    LPSOCKADDR remoteAddr;	    /* Remote sockaddr. */

    DWORD lastError;		    /* Error code from last operation. */
    LPLLIST llPendingRecv;	    /* Our pending recv list. */
    LLNODE node;		    /* linked list node for the readySockts list. */
} SocketInfo;

#include <poppack.h>

typedef Tcl_Obj * (Tcl_NetDecodeAddrProc) (SocketInfo *info, LPSOCKADDR addr);

/* name resolver */
#define TCL_NET_RESOLVER_QUERY	    0
#define TCL_NET_RESOLVER_REGISTER   1
#define TCL_NET_RESOLVER_UNREGISTER 2

typedef int (Tcl_NetResolverProc)(int command, Tcl_Obj *question,
	Tcl_Obj *argument, Tcl_Obj **answers);
typedef Tcl_Channel (Tcl_NetCreateClientProc)(Tcl_Interp *interp, const char *port,
	    const char *host, const char *myaddr, const char *myport,
	    int async, int afhint);
typedef Tcl_Channel (Tcl_NetCreateServerProc)(Tcl_Interp *interp, const char *port,
		const char *myhost, Tcl_SocketAcceptProc *acceptProc,
		ClientData callbackData, int afhint);
/*
 * Specific protocol information is stored here and shared to all
 * SocketInfo objects of that type.
 */
typedef struct WS2ProtocolData {
    int af;		    /* Address family. */
    int	type;		    /* Address type. */
    int	protocol;	    /* Protocol type. */
    size_t addrLen;	    /* Length of protocol specific SOCKADDR */
    int afhint;		    /* Address family hint (for getaddrinfo(). */
    Tcl_NetCreateClientProc	*CreateClient;
    Tcl_NetCreateServerProc	*CreateServer;
    Tcl_NetDecodeAddrProc	*DecodeSockAddr;
    Tcl_NetResolverProc		*Resolver;

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



typedef struct AcceptInfo {
    SOCKADDR_STORAGE local;
    int localLen;
    SOCKADDR_STORAGE remote;
    int remoteLen;
    SocketInfo *clientInfo;
    LLNODE node;
} AcceptInfo;

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

/*
 * ----------------------------------------------------------------------
 * Some stuff that needs to be switches or fconfigures, but aren't yet.
 * ----------------------------------------------------------------------
 */

/*
 * Default number of overlapped AcceptEx calls to place on a new
 * listening socket.  Base minimum allowed, found by experimentation.
 * This is the base pool count.  Don't go below this number or the
 * listening socket will start returning errors quite easily.
 *
 * Use the -backlog fconfigure on the listening socket to set the
 * pool size.  Each overlapped AcceptEx call will reserve ~500 bytes
 * of the non-paged memory pool.  Larger IS better, if you don't mind
 * the memory in reserve.  Choose a good sized -backlog such as 500
 * if you want it "bunker buster proof".  The NP pool is a global
 * resource for all processes and is said to have a limit around 1/4 of
 * the physical memory.  500 overlapped AccepEx calls * ~500 bytes = ~250K
 * of reserved NP pool memory.  Only use such high -backlog sizes
 * for VERY heavy load servers that you want to handle SYN attacks
 * gracefully.
 */

/* This is the -backlog fconfigure's default value. */
#define IOCP_ACCEPT_CAP		    20

/*
 * We do not want an initial recv() with a new connection.  Use of this
 * feature would require a watchdog thread for doing clean-up of mid-state
 * (connected, but no data yet) connections and is just asking to be a DoS
 * hole..  Not only that, but only works on protocols where the client
 * speaks first.  HTTPD may be one, but many others are not.
 *
 *  !DO NOT USE THIS FEATURE!  Turn it off.
 */
#define IOCP_ACCEPT_BUFSIZE	    0

/*
 * Initial count of how many WSARecv(From) calls to place on a connected
 * socket.  The actual count can grow automatically based on burst
 * activity to the cap count (See the recursion used in PostOverlappedRecv
 * for details).
 */

#define IOCP_INITIAL_RECV_COUNT	    1
/* This is the -recvburst fconfigure's default value. */
#define IOCP_RECV_CAP		    20

/*
 * How large do we want the receive buffers?  Use multiples of the
 * page size only (4096) as windows will lock this on page boundaries
 * anyways.
 */

#define IOCP_RECV_BUFSIZE	    4096

/*
 * Initial (default) cap on send concurrency.  This is the -sendcap
 * fconfigure's default value.
 */

#define IOCP_SEND_CAP		    20

extern Tcl_ChannelType IocpChannelType;
extern CompletionPortInfo IocpSubSystem;

extern void IocpInitProtocolData (SOCKET sock, WS2ProtocolData *pdata);
extern int CreateSocketAddress (const char *addr, const char *port,
	LPADDRINFO inhints, LPADDRINFO *result);
extern void FreeSocketAddress(LPADDRINFO addrinfo);
extern BOOL FindProtocolInfo(int af, int type, int protocol, DWORD flags,
	WSAPROTOCOL_INFO *pinfo);
extern DWORD PostOverlappedAccept (SocketInfo *infoPtr,
	BufferInfo *acceptobj, int useBurst);
extern DWORD PostOverlappedRecv (SocketInfo *infoPtr,
	BufferInfo *recvobj, int useBurst, int ForcePostOnError);
extern DWORD PostOverlappedQOS (SocketInfo *infoPtr, BufferInfo *bufPtr);
extern void FreeBufferObj(BufferInfo *obj);
extern BufferInfo * GetBufferObj (SocketInfo *infoPtr, SIZE_T buflen);
extern SocketInfo * NewSocketInfo (SOCKET socket);
extern void FreeSocketInfo (SocketInfo *infoPtr);

/* thread safe linked list procedures */
extern LPLLIST IocpLLCreate (void);
extern BOOL IocpLLDestroy (LPLLIST ll);
extern LPLLNODE IocpLLPushBack (LPLLIST ll, LPVOID lpItem, LPLLNODE pnode,
	DWORD dwState);
extern LPLLNODE IocpLLPushFront (LPLLIST ll, LPVOID lpItem, LPLLNODE pnode,
	DWORD dwState);
extern BOOL IocpLLPop (LPLLNODE pnode, DWORD dwState);
extern BOOL IocpLLPopAll (LPLLIST ll, LPLLNODE snode, DWORD dwState);
extern LPVOID IocpLLPopBack (LPLLIST ll, DWORD dwState, DWORD timeout);
extern LPVOID IocpLLPopFront (LPLLIST ll, DWORD dwState, DWORD timeout);
extern BOOL IocpLLIsNotEmpty (LPLLIST ll);
extern BOOL IocpLLNodeDestroy (LPLLNODE node);
extern SIZE_T IocpLLGetCount (LPLLIST ll);

/* private memory stuff */
extern __inline LPVOID	IocpAlloc (SIZE_T size);
extern __inline LPVOID  IocpReAlloc (LPVOID block, SIZE_T size);
extern __inline BOOL	IocpFree (LPVOID block);
extern __inline LPVOID	IocpNPPAlloc (SIZE_T size);
extern __inline LPVOID  IocpNPPReAlloc (LPVOID block, SIZE_T size);
extern __inline BOOL	IocpNPPFree (LPVOID block);

extern Tcl_NetDecodeAddrProc DecodeIpSockaddr;
extern WS2ProtocolData tcpAnyProtoData;
extern WS2ProtocolData tcp4ProtoData;
extern WS2ProtocolData tcp6ProtoData;
extern WS2ProtocolData udpAnyProtoData;
extern WS2ProtocolData udp4ProtoData;
extern WS2ProtocolData udp4ProtoData;
extern WS2ProtocolData bthProtoData;
extern WS2ProtocolData irdaProtoData;

extern ThreadSpecificData *InitSockets(void);

