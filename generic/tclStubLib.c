/*
 * tclStubLib.c --
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

MODULE_SCOPE const TclStubs *tclStubsPtr;
MODULE_SCOPE const TclPlatStubs *tclPlatStubsPtr;
MODULE_SCOPE const TclIntStubs *tclIntStubsPtr;
MODULE_SCOPE const TclIntPlatStubs *tclIntPlatStubsPtr;

const TclStubs *tclStubsPtr = NULL;
const TclPlatStubs *tclPlatStubsPtr = NULL;
const TclIntStubs *tclIntStubsPtr = NULL;
const TclIntPlatStubs *tclIntPlatStubsPtr = NULL;

static const TclStubs *
HasStubSupport(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    if (iPtr->stubTable && (iPtr->stubTable->magic == TCL_STUB_MAGIC)) {
	return iPtr->stubTable;
    }

    /*
     * Either interp has no stubTable field, or its magic number has been
     * changed, indicating a release of Tcl that no longer supports the
     * stubs mechanism with which the extension has been prepared.  This
     * either means interp comes from Tcl releases 7.5 - 8.0, when [load]
     * of extensions was possible, but stubs were not yet in use, or it means
     * interp come from some future release of Tcl where it has been necessary
     * to stop supporting this particular stubs mechanism.  In either case,
     * we can count on the fields legacyResult and legacyFreeProc existing
     * (since they persist to maintain the struct offset fo stubTable; see
     * tclInt.h comments.), and we can hope that [load] or any sensible
     * successor will be able to reach into them to report the mismatch error
     * message sensibly.
     *
     * For maximum compat support, even if only for the sake of reporting
     * clean errors, rather than crashing, we assume the TCL_STUB_MAGIC
     * value is changed only when absolutely necessary.  So long as the first
     * slot in the stub table holds (some function compatible with) the routine
     * Tcl_PkgRequireEx(), that routine can take care of verifying the version
     * compatibility testing with all deployed stubs-enabled extensions.  That is,
     * there is no need to change the value of TCL_STUB_MAGIC when transitioning
     * from the Tcl 8 stubs table to the Tcl 9 stubs table, so long as they 
     * share just their first slot in common.
     */

    iPtr->legacyResult
	    = (char *) "interpreter uses an incompatible stubs mechanism";
    iPtr->legacyFreeProc = TCL_STATIC;
    return NULL;
}

/*
 * Use our own isdigit to avoid linking to libc on windows
 */

static int isDigit(const int c)
{
    return (c >= '0' && c <= '9');
}

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

MODULE_SCOPE const char *
TclInitStubs(
    Tcl_Interp *interp,
    const char *version,
    int exact,
    int major,
    int magic)
{
    Interp *iPtr = (Interp *) interp;
    const char *actualVersion = NULL;
    ClientData pkgData = NULL;
    const char *p, *q;

    /*
     * Detect whether the extension and the stubs library were built
     * against Tcl header files declaring use of incompatible stubs
     * mechanisms.  Even within the same mechanism, also detect if
     * the header files are from different major versions.  Either
     * is seriously broken.  An extension and its stubs library ought
     * to share compatible headers, if not the same one.
     */

    if (magic != TCL_STUB_MAGIC || major != TCL_MAJOR_VERSION) {
	iPtr->legacyResult =
	    (char *) "extension linked to incompatible stubs library";
	iPtr->legacyFreeProc = TCL_STATIC;
	return NULL;
    }

    /*
     * Detect whether an extension compiled against a Tcl header file
     * of one major version is requesting to use a stubs table of a
     * different major version.  According to our compat rules, that's
     * a request that cannot succeed.  Different major versions imply
     * incompatible stub tables.
     */

    p = version;
    q = TCL_VERSION;
    while (isDigit(*p)) {
	if (*p++ != *q++) {
	    goto badVersion;
	}
    }
    if (isDigit(*q)) {
    badVersion:
	iPtr->legacyResult =
	    (char *) "extension passed bad version argument to stubs library";
	iPtr->legacyFreeProc = TCL_STATIC;
	return NULL;
    }

    /*
     * We can't optimize this check by caching tclStubsPtr because that
     * prevents apps from being able to load/unload Tcl dynamically multiple
     * times. [Bug 615304]
     */

    tclStubsPtr = HasStubSupport(interp);
    if (!tclStubsPtr) {
	return NULL;
    }

    actualVersion = Tcl_PkgRequireEx(interp, "Tcl", version, 0, &pkgData);
    if (actualVersion == NULL) {
	return NULL;
    }
    if (exact) {
	int count = 0;

	p = version;
	while (*p) {
	    count += !isDigit(*p++);
	}
	if (count == 1) {
	    q = actualVersion;

	    p = version;
	    while (*p && (*p == *q)) {
		p++; q++;
	    }
	    if (*p) {
		/* Construct error message */
		Tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
		return NULL;
	    }
	} else {
	    actualVersion = Tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
	    if (actualVersion == NULL) {
		return NULL;
	    }
	}
    }
    tclStubsPtr = (TclStubs *) pkgData;

    if (tclStubsPtr->hooks) {
	tclPlatStubsPtr = tclStubsPtr->hooks->tclPlatStubs;
	tclIntStubsPtr = tclStubsPtr->hooks->tclIntStubs;
	tclIntPlatStubsPtr = tclStubsPtr->hooks->tclIntPlatStubs;
    } else {
	tclPlatStubsPtr = NULL;
	tclIntStubsPtr = NULL;
	tclIntPlatStubsPtr = NULL;
    }

    return actualVersion;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
