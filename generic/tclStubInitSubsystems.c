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
#   define dlerror() ""
#endif

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

static const char PROCNAME[] = "_Tcl_InitSubsystems";

MODULE_SCOPE const char *
TclStubInitSubsystems(void)
{
    static const char *(*initSubsystems)(void) = NULL;
    static const char *version = NULL;

    if (!initSubsystems) {
	void *handle = dlopen(TCL_DLL_FILE, RTLD_NOW|RTLD_LOCAL);
	if (!handle) {
	    fprintf(stderr, "Cannot find " TCL_DLL_FILE ": %s\n", dlerror());
	    abort();
	}
	initSubsystems = dlsym(handle, PROCNAME + 1);
	if (!initSubsystems) {
		initSubsystems = dlsym(handle, PROCNAME);
	}
	if (initSubsystems) {
	    version = initSubsystems();
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
