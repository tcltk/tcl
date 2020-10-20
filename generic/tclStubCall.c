/*
 * tclStubCall.c --
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
    "_Tcl_InitSubsystems",
    "_Tcl_FindExecutable",
    "_Tcl_SetPanicProc",
    "_TclZipfs_AppHook",
    "_Tcl_StaticPackage",
    "_Tcl_MainEx",
    "_Tcl_MainExW"
};

MODULE_SCOPE const void *nullVersionProc(void) {
	return NULL;
}

static const char CANNOTCALL[] = "Cannot call %s from stubbed extension\n";
static const char CANNOTFIND[] = "Cannot find %s: %s\n";

MODULE_SCOPE void *
TclStubCall(int index, void *arg1)
{
    static void *stubFn[] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
    const char *(*versionProc)(void) = (const char *(*)(void))nullVersionProc;

    if (tclStubsHandle == (void *)-1) {
	if (index == 2 && arg1 != NULL) {
	    ((Tcl_PanicProc *)arg1)(CANNOTCALL, PROCNAME[index] + 1);
	} else {
	    fprintf(stderr, CANNOTCALL, PROCNAME[index] + 1);
	    abort();
	}
    }
    if (!stubFn[index]) {
	if (!tclStubsHandle) {
	    tclStubsHandle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	    if (!tclStubsHandle) {
		if (index == 2 && arg1 != NULL) {
		    ((Tcl_PanicProc *)arg1)(CANNOTFIND, TCL_DLL_FILE, dlerror());
		} else {
		    fprintf(stderr, CANNOTFIND, TCL_DLL_FILE, dlerror());
		    abort();
		}
	    }
	}
	stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index] + 1);
	if (!stubFn[index]) {
		stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index]);
	}
	if (stubFn[index]) {
	    versionProc = ((const char *(*)(void))stubFn[index]);
	}
    }
    return versionProc;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
