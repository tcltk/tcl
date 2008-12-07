/* reserved for Pat Thoyts and tcludp */
#include "tclWinInt.h"
#include "tclWinsockCore.h"

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
    return NULL;
}
