/*
 * tclUnixEvent.c --
 *
 *	This file implements Unix specific event related routines.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#ifndef HAVE_COREFOUNDATION	/* Darwin/Mac OS X CoreFoundation notifier is
				 * in tclMacOSXNotify.c */

/* declaration of TclpSleep() from tclUnixNotfy.c */
void	TclpSleep(const Tcl_Time *timePtr);

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
    Tcl_Time delay;

    delay.sec = ms/1000;
    delay.usec = (ms%1000)*1000;
    TclpSleep(&delay);
}

#endif /* HAVE_COREFOUNDATION */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
