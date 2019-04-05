/* must be compiled without stubs */
#undef USE_TCL_STUBS
#include <tcl.h>

Tcl_Interp * Tcl_CreateInterpWithStubs(const char *version, int exact)
{
    Tcl_Interp *interp = Tcl_CreateInterp();

    if (Tcl_Init(interp) == TCL_ERROR
        || Tcl_InitStubs(interp, version, exact) == NULL
        || Tcl_PkgRequire(interp, "Tcl", version, exact) == NULL) {
	return NULL;
    }   
    return interp;
}
