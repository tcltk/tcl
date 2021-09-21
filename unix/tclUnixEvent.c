/*
 * tclUnixEvent.c --
 *
 *	This file implements Unix specific event related routines.
 *
 * Copyright © 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#ifndef HAVE_COREFOUNDATION	/* Darwin/Mac OS X CoreFoundation notifier is
				 * in tclMacOSXNotify.c */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(
    int ms)			/* Number of milliseconds to sleep. */
{
    struct timeval delay;
    Tcl_Time before, after, vdelay;

    /*
     * The only trick here is that select appears to return early under some
     * conditions, so we have to check to make sure that the right amount of
     * time really has elapsed.  If it's too early, go back to sleep again.
     */

    Tcl_GetTime(&before);
    after = before + ms * 1000;
    while (1) {
	/*
	 * TIP #233: Scale from virtual time to real-time for select.
	 */

	vdelay = after - before;

	if (vdelay != 0) {
	    TclScaleTime(&vdelay);
	}

	delay.tv_sec  = vdelay / 1000000;
	delay.tv_usec = vdelay % 1000000;

	/*
	 * Special note: must convert delay.tv_sec to int before comparing to
	 * zero, since delay.tv_usec is unsigned on some platforms.
	 */

	if ((((int) delay.tv_sec) < 0)
		|| ((delay.tv_usec == 0) && (delay.tv_sec == 0))) {
	    break;
	}
	(void) select(0, (SELECT_MASK *) 0, (SELECT_MASK *) 0,
		(SELECT_MASK *) 0, &delay);
	Tcl_GetTime(&before);
    }
}

#else
TCL_MAC_EMPTY_FILE(unix_tclUnixEvent_c)
#endif /* HAVE_COREFOUNDATION */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
