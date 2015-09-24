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
 * Tcl_InitSubsystems --
 *
 *	Load the Tcl core dynamically, either version "8.6" or "8.5"
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
Tcl_InitSubsystems(
	TCL_NORETURN1 Tcl_PanicProc *panicProc)
{
	static struct info {
		const TclStubs *stubs;
		char version[255];
	} info = {NULL, ""};
    const char *(*initSubsystems)(TCL_NORETURN1 Tcl_PanicProc *);
    int a,b,c,d;

    if (!info.version[0]) {
	void *handle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	if (!handle) {
	    handle = dlopen(TCL_PREV_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	}
	if (!handle) {
	    if (panicProc) {
		panicProc("Cannot find " TCL_DLL_FILE " neither " TCL_PREV_DLL_FILE);
	    } else {
		fprintf(stderr, "Cannot find " TCL_DLL_FILE " neither " TCL_PREV_DLL_FILE);
		abort();
	    }
	    return NULL;
	}
	initSubsystems = dlsym(handle, "Tcl_InitSubsystems");
	if (!initSubsystems) {
	    initSubsystems = dlsym(handle, "_Tcl_InitSubsystems");
	}
	if (initSubsystems) {
	    /* If the core has TIP #414, use it. */
	    const char *version = initSubsystems(panicProc);
	    info.stubs = ((const TclStubs **)version)[-1];
	    strcpy(info.version+1, version+1);
	    info.version[0] = version[0];
	} else {
	    const TclStubs *stubs;
	    const char *(*setPanicProc)(Tcl_PanicProc *);
	    Tcl_Interp *interp, *(*createInterp)(void);

	    setPanicProc = dlsym(handle, "Tcl_SetPanicProc");
	    if (!setPanicProc) {
		setPanicProc = dlsym(handle, "_Tcl_SetPanicProc");
	    }
	    createInterp = dlsym(handle, "Tcl_CreateInterp");
	    if (!createInterp) {
		createInterp = dlsym(handle, "_Tcl_CreateInterp");
	    }
	    setPanicProc(panicProc);
	    interp = createInterp();
	    stubs = ((Interp *) interp)->stubTable;
	    stubs->tcl_DeleteInterp(interp);
	    stubs->tcl_GetVersion(&a, &b, &c, &d);
	    info.stubs = stubs;
	    if (a>9) {
		sprintf(info.version+1, "%d.%d%c%d", a%10, b, "ab."[d], c);
		info.version[0] = '0' + (a/10);
	    } else {
		sprintf(info.version+1, ".%d%c%d", b, "ab."[d], c);
		info.version[0] = '0' + a;
	    }
	}
    }
    return info.version;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
