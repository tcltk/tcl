/*
 * $Id: tclOOStubLib.c,v 1.1.2.2 2008/05/31 21:02:06 dgp Exp $
 * ORIGINAL SOURCE: tk/generic/tkStubLib.c, version 1.9 2004/03/17
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tcl.h"

#define USE_TCLOO_STUBS 1
#include "tclOO.h"
#include "tclOOInt.h"

const TclOOStubs *tclOOStubsPtr;
const TclOOIntStubs *tclOOIntStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * TclOOInitializeStubs --
 *	Load the tclOO package, initialize stub table pointer. Do not call
 *	this function directly, use Tcl_OOInitStubs() macro instead.
 *
 * Results:
 *	The actual version of the package that satisfies the request, or NULL
 *	to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointer.
 *
 */

const char *TclOOInitializeStubs(
    Tcl_Interp *interp, const char *version, int epoch, int revision)
{
    int exact = 0;
    const char *packageName = "TclOO";
    const char *errMsg = NULL;
    ClientData clientData = NULL;
    const char *actualVersion =
	    Tcl_PkgRequireEx(interp, packageName,version, exact, &clientData);
    struct TclOOStubAPI *stubsAPIPtr = clientData;

    if (stubsAPIPtr == NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Error loading ", packageName, " package; ",
		"package not present or incomplete", NULL);
	return NULL;
    } else {
	TclOOStubs *stubsPtr = stubsAPIPtr->stubsPtr;
	TclOOIntStubs *intStubsPtr = stubsAPIPtr->intStubsPtr;

	if (!actualVersion) {
	    return NULL;
	}

	if (!stubsPtr || !intStubsPtr) {
	    errMsg = "missing stub table pointer";
	    goto error;
	}
	if (stubsPtr->epoch != epoch || intStubsPtr->epoch != epoch) {
	    errMsg = "epoch number mismatch";
	    goto error;
	}
	if (stubsPtr->revision<revision || intStubsPtr->revision<revision) {
	    errMsg = "require later revision";
	    goto error;
	}

	tclOOStubsPtr = stubsPtr;
	tclOOIntStubsPtr = intStubsPtr;
	return actualVersion;

    error:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Error loading ", packageName, " package",
		" (requested version '", version, "', loaded version '",
		actualVersion, "'): ", errMsg, NULL);
	return NULL;
    }
}
