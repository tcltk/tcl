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
	"_TclZipfs_AppHook"
};

MODULE_SCOPE const char *
TclStubCall(int index, void *arg1, void *arg2)
{
    static void *stubFn[] = {NULL,NULL,NULL,NULL};
    static const char *version = NULL;

    if (tclStubsHandle == (void *)-1) {
	if (index == 2 && arg1 != NULL) {
	    ((Tcl_PanicProc *)arg1)("Cannot call %s from stubbed extension\n", PROCNAME[index] + 1);
	} else {
	    fprintf(stderr, "Cannot call %s from stubbed extension\n", PROCNAME[index] + 1);
	    abort();
	}
    }
    if (!stubFn[index]) {
	if (!tclStubsHandle) {
	    tclStubsHandle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	    if (!tclStubsHandle) {
		if (index == 2 && arg1 != NULL) {
		    ((Tcl_PanicProc *)arg1)("Cannot find " TCL_DLL_FILE ": %s\n", dlerror());
		} else {
		    fprintf(stderr, "Cannot find " TCL_DLL_FILE ": %s\n", dlerror());
		    abort();
		}
	    }
	}
	stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index] + 1);
	if (!stubFn[index]) {
		stubFn[index] = dlsym(tclStubsHandle, PROCNAME[index]);
	}
	if (stubFn[index]) {
	    version = ((const char *(*)(void *,void *))stubFn[index])(arg1, arg2);
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
