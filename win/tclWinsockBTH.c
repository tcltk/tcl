/* reserved for Bluetooth */

#include "tclWinInt.h"
#include "tclWinsockCore.h"
#include <ws2bth.h>
#include <Bthsdpdef.h>
#include <BluetoothAPIs.h>

static WS2ProtocolData bthProtoData = {
    AF_BTH,
    SOCK_STREAM,
    BTHPROTO_RFCOMM,
    sizeof(SOCKADDR_BTH),
    DecodeBthSockaddr,
    ,	/* resolver */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

