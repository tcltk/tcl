/*
 * tclStubMainEx.c --
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#ifndef _WIN32
#   include <dlfcn.h>
#else
#   define dlopen(a,b) (void *)LoadLibraryW(JOIN(L,a))
#   define dlsym(a,b) (void *)GetProcAddress((HMODULE)(a),b)
#   define dlerror() ""
#endif

MODULE_SCOPE void *tclStubsHandle;

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitSubsystems --
 *
 *	Load the Tcl core dynamically, version "9.0" (or higher, in future versions)
 *
 * Results:
 *	Outputs the value of the "version" argument.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

static const char PROCNAME[][24] = {
	"_Tcl_MainEx",
	"_Tcl_MainExW"
};

MODULE_SCOPE void
TclStubMainEx(int index, int argc, const void *argv,
	Tcl_AppInitProc *appInitProc, Tcl_Interp *interp)
{
    static void *stubFn[] = {NULL,NULL};

    if (!stubFn[index]) {
	if (tclStubsHandle == (void *)-1) {
	    fprintf(stderr, "Cannot call %s from stubbed extension\n", PROCNAME[index] + 1);
	    abort();
	}
	if (!tclStubsHandle) {
	    tclStubsHandle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	    if (!tclStubsHandle) {
		tclStubsPtr->tcl_Panic("Cannot find " TCL_DLL_FILE ": %s\n", dlerror());
	    }
	}
	stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index] + 1);
	if (!stubFn[index]) {
		stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index]);
	}
	if (stubFn[index]) {
	    ((void(*)(int, const void *, Tcl_AppInitProc *, Tcl_Interp *))stubFn[index])(argc, argv, appInitProc, interp);
	}
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
