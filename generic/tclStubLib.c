/*
 * tclStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tcl.
 *
 * Copyright © 1998-1999 Scriptics Corporation.
 * Copyright © 1998 Paul Duffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

MODULE_SCOPE const TclStubs *tclStubsPtr;
MODULE_SCOPE const TclPlatStubs *tclPlatStubsPtr;
MODULE_SCOPE const TclIntStubs *tclIntStubsPtr;
MODULE_SCOPE const TclIntPlatStubs *tclIntPlatStubsPtr;
MODULE_SCOPE void *tclStubsHandle;

const TclStubs *tclStubsPtr = NULL;
const TclPlatStubs *tclPlatStubsPtr = NULL;
const TclIntStubs *tclIntStubsPtr = NULL;
const TclIntPlatStubs *tclIntPlatStubsPtr = NULL;
void *tclStubsHandle = NULL;

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
    int magic)
{
    Interp *iPtr = (Interp *)interp;
    const char *actualVersion = NULL;
    void *pkgData = NULL;
    const TclStubs *stubsPtr = iPtr->stubTable;
    const char *tclName = (((exact&0xFF00) >= 0x900) ? "tcl" : "Tcl");

#undef TCL_STUB_MAGIC /* We need the TCL_STUB_MAGIC from Tcl 8.x here */
#define TCL_STUB_MAGIC		((int) 0xFCA3BACF)
    /*
     * We can't optimize this check by caching tclStubsPtr because that
     * prevents apps from being able to load/unload Tcl dynamically multiple
     * times. [Bug 615304]
     */

    if (!stubsPtr || (stubsPtr->magic != (((exact&0xFF00) >= 0x900) ? magic : TCL_STUB_MAGIC))) {
	iPtr->legacyResult = "interpreter uses an incompatible stubs mechanism";
	iPtr->legacyFreeProc = 0; /* TCL_STATIC */
	return NULL;
    }

    actualVersion = stubsPtr->tcl_PkgRequireEx(interp, tclName, version, 0, &pkgData);
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
		stubsPtr->tcl_PkgRequireEx(interp, tclName, version, 1, NULL);
		return NULL;
	    }
	} else {
	    actualVersion = stubsPtr->tcl_PkgRequireEx(interp, tclName, version, 1, NULL);
	    if (actualVersion == NULL) {
		return NULL;
	    }
	}
    }
    if (((exact&0xFF00) < 0x900)) {
	/* We are running Tcl 8.x */
	stubsPtr = (TclStubs *)pkgData;
    }
    if (tclStubsHandle == NULL) {
	tclStubsHandle = INT2PTR(-1);
    }
    tclStubsPtr = stubsPtr;

    if (stubsPtr->hooks) {
	tclPlatStubsPtr = stubsPtr->hooks->tclPlatStubs;
	tclIntStubsPtr = stubsPtr->hooks->tclIntStubs;
	tclIntPlatStubsPtr = stubsPtr->hooks->tclIntPlatStubs;
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
