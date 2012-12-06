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
    Tcl_Interp *interp,
    int magic)
{
    Interp *iPtr = (Interp *) interp;
    if (iPtr->stubTable && iPtr->stubTable->magic == magic
	    && iPtr->stubTable->magic == TCL_STUB_MAGIC) {
	return iPtr->stubTable;
    }
    iPtr->result = (char *) "interpreter uses an incompatible stubs mechanism";
    iPtr->freeProc = TCL_STATIC;
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
 * TclInitStubs --
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
    const char *tclversion,
    int magic)
{
    const char *p;
    char *q;
    char major[TCL_INTEGER_SPACE];
    const char *actualVersion = NULL;
    ClientData pkgData = NULL;
    Interp *iPtr = (Interp *) interp;

    /*
     * We can't optimize this check by caching tclStubsPtr because that
     * prevents apps from being able to load/unload Tcl dynamically multiple
     * times. [Bug 615304]
     */

    tclStubsPtr = HasStubSupport(interp, magic);
    if (!tclStubsPtr) {
	return NULL;
    }

    /*
     * Check that the [load]ing interp and [load]ed extension were compiled
     * against headers from the same major version of Tcl.  If not, they
     * will not agree on the layout of the stubs and will crash.  Report
     * the error instead of crashing.
     */

    p = tclversion;
    q = major;
    while (isDigit(*p)) {
	*q++ = *p++;
	if (q-major > TCL_INTEGER_SPACE) {
	    iPtr->result = (char *) "major version overflow";
	    iPtr->freeProc = TCL_STATIC;
	    return NULL;
	}
    }
    *q = '\0';

    if (NULL == Tcl_PkgRequireEx(interp, "Tcl", major, 0, NULL)) {
	return NULL;
    }

    /*
     * Check satisfaction of the requirement requested by the caller.
     */

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
