/*
 * tclTomMathStubLib.c --
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

/*
 * We need to ensure that we use the stub macros so that this file contains no
 * references to any of the stub functions. This will make it possible to
 * build an extension that references Tcl_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#define USE_TCL_STUBS

#include "tclInt.h"

MODULE_SCOPE const TclTomMathStubs *tclTomMathStubsPtr;

const TclTomMathStubs *tclTomMathStubsPtr = NULL;


/*
 *----------------------------------------------------------------------
 *
 * TclTomMathInitStubs --
 *
 *	Initializes the Stubs table for Tcl's subset of libtommath
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * This procedure should not be called directly, but rather through
 * the TclTomMath_InitStubs macro, to insure that the Stubs table
 * matches the header files used in compilation.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE const char *
TclTomMathInitializeStubs(
    Tcl_Interp *interp,		/* Tcl interpreter */
    const char *version,	/* Tcl version needed */
    int epoch,			/* Stubs table epoch from the header files */
    int revision)		/* Stubs table revision number from the
				 * header files */
{
    int exact = 0;
    const char *packageName = "tcl::tommath";
    const char *errMsg = NULL;
    ClientData pkgClientData = NULL;
    const char *actualVersion =
	Tcl_PkgRequireEx(interp, packageName, version, exact, &pkgClientData);
    const TclTomMathStubs *stubsPtr = pkgClientData;

    if (actualVersion == NULL) {
	return NULL;
    }
    if (pkgClientData == NULL) {
	errMsg = "missing stub table pointer";
    } else if ((stubsPtr->tclBN_epoch)() != epoch) {
	errMsg = "epoch number mismatch";
    } else if ((stubsPtr->tclBN_revision)() != revision) {
	errMsg = "requires a later revision";
    } else {
	tclTomMathStubsPtr = stubsPtr;
	return actualVersion;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "error loading %s (requested version %s, actual version %s): %s",
	    packageName, version, actualVersion, errMsg));
    return NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
