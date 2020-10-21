#include "tcl.h"
#include <stdio.h>

int main() {
    const char *version = Tcl_SetPanicProc(Tcl_ConsolePanic);

    if (version != NULL) {
    	printf("OK. version = %s\n", version);
    }
    return 0;
}
