struct SocketInfo;


extern Tcl_ThreadDataKey dataKey;

extern Tcl_ChannelType IocpChannelType;


#define QOS_BUFFER_SZ 16000




/* forward reference. */
struct WS2ProtocolData;




typedef struct AcceptInfo {
    SOCKADDR_STORAGE local;
    int localLen;
    SOCKADDR_STORAGE remote;
    int remoteLen;
    SocketInfo *clientInfo;
    LLNODE node;
} AcceptInfo;


extern CompletionPortInfo IocpSubSystem;

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
 * speaks first.  HTTP may be one, but many others are not.
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
