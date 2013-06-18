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

#include "tclInt.h"

/*
 * Use our own ISDIGIT to avoid linking to libc on windows
 */

#define ISDIGIT(c) (((unsigned)((c)-'0')) <= 9)

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
    int exact,
    const char *tclversion,
    int magic)
{
    Interp *iPtr = (Interp *) interp;
    const char *actualVersion = NULL;
    const TclStubs *stubsPtr = iPtr->stubTable;
    ClientData pkgData = NULL;

    /*
     * We can't optimize this check by caching tclStubsPtr because that
     * prevents apps from being able to load/unload Tcl dynamically multiple
     * times. [Bug 615304]
     */

    if (!stubsPtr || (stubsPtr->magic != TCL_STUB_MAGIC)) {
	iPtr->legacyResult = "interpreter uses an incompatible stubs mechanism";
	iPtr->legacyFreeProc = 0; /* TCL_STATIC */
	return NULL;
    }

    actualVersion = stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 0, &pkgData);
    if (actualVersion == NULL) {
	return NULL;
    }
    if (exact&1) {
	const char *p = version;
	int count = 0;

	while (*p) {
	    count += !ISDIGIT(*p++);
	}
	if (count == 1) {
	    const char *q = actualVersion;

	    p = version;
	    while (*p && (*p == *q)) {
		p++; q++;
	    }
	    if (*p || ISDIGIT(*q)) {
		/* Construct error message */
		stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
		return NULL;
	    }
	} else {
	    actualVersion = stubsPtr->tcl_PkgRequireEx(interp, "Tcl", version, 1, NULL);
	    if (actualVersion == NULL) {
		return NULL;
	    }
	}
    }

    if (stubsPtr->reserved77) {
	/* We are running Tcl 8. */
	TclInitStubTable(((char *)&pkgData)-TclOffset(TclStubInfoType, stubs));
    } else {
	/* We are running Tcl 9. */
	TclInitStubTable(((char *)&stubsPtr)-TclOffset(TclStubInfoType, stubs));
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
