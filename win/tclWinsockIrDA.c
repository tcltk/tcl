/* reserved for IrDA's IrLAP/LSAP sockets protocol */

#include "tclWinInt.h"
#include "tclWinsockCore.h"
#include <af_irda.h>

static WS2ProtocolData irdaProtoData = {
    AF_IRDA,
    SOCK_STREAM,
    0,
    sizeof(SOCKADDR_IRDA),
    DecodeIrdaSockaddr,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static SocketInfo *	CreateIrdaSocket(Tcl_Interp *interp,
				CONST char *port, CONST char *host,
				int server, CONST char *myaddr,
				CONST char *myport, int async);


Tcl_Obj *
DecodeIrdaSockaddr (SocketInfo *info, LPSOCKADDR addr)
{
    char formatedId[12];
    Tcl_Obj *result = Tcl_NewObj();
    SOCKADDR_IRDA *irdaaddr = (SOCKADDR_IRDA *) addr;

    /* Device ID. */
    sprintf(formatedId, "%02x-%02x-%02x-%02x",
	    irdaaddr->irdaDeviceID[0], irdaaddr->irdaDeviceID[1],
	    irdaaddr->irdaDeviceID[2], irdaaddr->irdaDeviceID[3]);
    Tcl_ListObjAppendElement(NULL, result,
	    Tcl_NewStringObj(formatedId, 11));

    /* Service Name (probably not in UTF-8). */
    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(
	    irdaaddr->irdaServiceName, -1));

    return result;
}
