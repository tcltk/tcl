/*
 * tclStubStaticPackage.c --
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#ifndef _WIN32
#   include <dlfcn.h>
#else
#   define dlopen(a,b) (void *)LoadLibrary(TEXT(a))
#   define dlsym(a,b) (void *)GetProcAddress((HANDLE)(a),b)
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

static const char PROCNAME[] = "_Tcl_StaticPackage";

MODULE_SCOPE const char *
TclStubStaticPackage(Tcl_Interp *interp,
	const char *pkgName,
	Tcl_PackageInitProc *initProc,
	Tcl_PackageInitProc *safeInitProc)
{
    static const char *(*stubFn)(Tcl_Interp *, const char *, Tcl_PackageInitProc *, Tcl_PackageInitProc *) = NULL;
    static const char *version = NULL;

    if (tclStubsHandle == (void *)-1) {
	fprintf(stderr, "Cannot call %s from stubbed extension\n", PROCNAME + 1);
	abort();
    }
    if (!stubFn) {
	if (!tclStubsHandle) {
	    tclStubsHandle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	    if (!tclStubsHandle) {
		tclStubsPtr->tcl_Panic("Cannot find " TCL_DLL_FILE ": %s\n", dlerror());
	    }
	}
	stubFn = dlsym(tclStubsHandle, PROCNAME + 1);
	if (!stubFn) {
		stubFn = dlsym(tclStubsHandle, PROCNAME);
	}
	if (stubFn) {
	    version = stubFn(interp, pkgName, initProc, safeInitProc);
	}
    }
    return version;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
