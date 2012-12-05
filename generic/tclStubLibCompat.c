/*
 * tclStubLibCompat.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tcl.
 *
 * Copyright (c) 2012 Jan Nijtmans
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * Small wrapper, which allows Tcl8 extensions to use the same stub
 * library as Tcl 9.
 */

#include "tclInt.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitStubs --
 *
 *	Tries to initialise the stub table pointers and ensures that the
 *	correct version of Tcl is loaded.
 *
 * Results:
 *	The actual version of Tcl that satisfies the request, or NULL to
 *	indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */
#undef Tcl_InitStubs

MODULE_SCOPE const char *
Tcl_InitStubs(
    Tcl_Interp *interp,
    const char *version,
    int exact)
{
    /* Use the hardcoded TCL_VERSION and Tcl8 magic value here. */
    return TclInitStubs(interp, version, exact, "8.6", (int) 0xFCA3BACF);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */

