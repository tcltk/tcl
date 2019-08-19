/*
 * tclStubLibDl.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tcl.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 1998 Paul Duffin.
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
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetPanicProc --
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

MODULE_SCOPE const char *
Tcl_SetPanicProc(
	TCL_NORETURN1 Tcl_PanicProc *panicProc)
{
    static const char *(*setPanicProc)(TCL_NORETURN1 Tcl_PanicProc *) = NULL;
    static const char *version = NULL;

    if (!setPanicProc) {
	void *handle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	if (!handle) {
	    if (panicProc) {
		panicProc("Cannot find " TCL_DLL_FILE);
	    } else {
		fprintf(stderr, "Cannot find " TCL_DLL_FILE);
		abort();
	    }
	    return NULL;
	}
	setPanicProc = dlsym(handle, "Tcl_SetPanicProc");
	if (!setPanicProc) {
		setPanicProc = dlsym(handle, "_Tcl_SetPanicProc");
	}
	if (setPanicProc) {
	    version = setPanicProc(panicProc);
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
