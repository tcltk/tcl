/* reserved for all things Bluetooth */

#include "tclWinInt.h"
#include "tclWinsockCore.h"

#include <ws2bth.h>
#include <Bthsdpdef.h>
#include <BluetoothAPIs.h>

static Tcl_NetCreateClientProc OpenBthClientChannel;
static Tcl_NetCreateServerProc OpenBthServerChannel;
static Tcl_NetDecodeAddrProc DecodeBthSockaddr;
static Tcl_NetResolverProc ResolveBth;

WS2ProtocolData bthProtoData = {
    AF_BTH,
    SOCK_STREAM,
    BTHPROTO_RFCOMM,
    sizeof(SOCKADDR_BTH),
    0,
    OpenBthClientChannel,
    OpenBthServerChannel,
    DecodeBthSockaddr,
    ResolveBth,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

Tcl_Obj *
DecodeBthSockaddr (
    SocketInfo *info,
    LPSOCKADDR addr)
{
    Tcl_Obj *result;
    SOCKADDR_BTH *bthaddr = (SOCKADDR_BTH *) addr;
    DWORD len = 1024;
    char buffer[1024];

    // this is a general call for "display purposes"
    // until a better solution is found.

    if (WSAAddressToString((LPSOCKADDR)bthaddr, bthProtoData.addrLen,
	    NULL, buffer, &len) == SOCKET_ERROR) {
	// error'd
	return NULL;
    }
    // TODO: probably not in utf-8
    result = Tcl_NewStringObj(buffer, len);
    return result;
}

int
ResolveBth (
    int command,
    Tcl_Obj *question,
    Tcl_Obj *argument,
    Tcl_Obj **answers)
{
    // TODO
    return TCL_ERROR;
}

Tcl_Channel
OpenBthClientChannel(
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

Tcl_Channel
OpenBthServerChannel(
    Tcl_Interp *interp,		/* For error reporting, may be NULL. */
    const char *port,		/* Port (number|service) to open. */
    const char *host,		/* Name of host for binding. */
    Tcl_SocketAcceptProc *acceptProc,
				/* Callback for accepting connections
				 * from new clients. */
    ClientData acceptProcData,	/* Data for the callback. */
    int afhint)
{
    return NULL;
}