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
#   define dlopen(a,b) (void *)LoadLibraryA(a)
#   define dlsym(a,b) (void *)GetProcAddress((HANDLE)(a),b)
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitSubsystems --
 *
 *	Initialize the stub table, using the structure pointed at
 *	by the "version" argument.
 *
 * Results:
 *	Outputs the value of the "version" argument.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

static TclStubInfoType info;

MODULE_SCOPE const char *
Tcl_InitSubsystems(
	Tcl_PanicProc *panicProc)
{
    void *handle = dlopen(TCL_LIB_FILE, RTLD_NOW|RTLD_LOCAL);
    const char *(*initSubsystems)(Tcl_PanicProc *);
    const char *(*setPanicProc)(Tcl_PanicProc *);
    Tcl_Interp *interp, *(*createInterp)(void);
    int a,b,c,d;

    if (!handle) {
	if (panicProc) {
	    panicProc("Cannot find Tcl core");
	} else {
	    fprintf(stderr, "Cannot find Tcl core");
	    abort();
	}
	return NULL;
    }
    initSubsystems = dlsym(handle, "Tcl_InitSubsystems");
    if (!initSubsystems) {
	initSubsystems = dlsym(handle, "_Tcl_InitSubsystems");
    }
    if (initSubsystems) {
	return initSubsystems(panicProc);
    }
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
    info.stubs = ((Interp *) interp)->stubTable;
    info.stubs->tcl_DeleteInterp(interp);
    info.stubs->tcl_GetVersion(&a, &b, &c, &d);
    sprintf(info.version, "%d.%d.%d", a, b, c);
    return info.version;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
