/*
 * tclUnixTime.c --
 *
 *	Contains Unix specific versions of Tcl functions that obtain time
 *	values from the operating system.
 *
 * Copyright © 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#if defined(TCL_WIDE_CLICKS) && defined(MAC_OSX_TCL)
#include <mach/mach_time.h>
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclpGetSeconds --
 *
 *	This procedure returns the number of seconds from the epoch. On most
 *	Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of seconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long long
TclpGetSeconds(void)
{
    return (unsigned long long) time(NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetMicroseconds --
 *
 *	This procedure returns the number of microseconds from the epoch.
 *	On most Unix systems the epoch is Midnight Jan 1, 1970 GMT.
 *
 * Results:
 *	Number of microseconds from the epoch.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

long long
TclpGetMicroseconds(void)
{
    Tcl_Time time;

    Tcl_GetTime(&time);
    return time.sec * 1000000 + time.usec;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetClicks --
 *
 *	This procedure returns a value that represents the highest resolution
 *	clock available on the system. There are no guarantees on what the
 *	resolution will be. In Tcl we will call this value a "click". The
 *	start time is also system dependent.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long long
TclpGetClicks(void)
{
    unsigned long long now;

#ifdef NO_GETTOD

    /*
     * A semi-NativeGetTime, specialized to clicks.
     */

    struct tms dummy;

    now = (unsigned long long) times(&dummy);
#else /* !NO_GETTOD */
    Tcl_Time time;

    Tcl_GetTime(&time);
    now = ((unsigned long long)(time.sec)*1000000ULL) +
	    (unsigned long long)(time.usec);
#endif /* NO_GETTOD */

    return now;
}
#ifdef TCL_WIDE_CLICKS

/*
 *----------------------------------------------------------------------
 *
 * TclpGetWideClicks --
 *
 *	This procedure returns a WideInt value that represents the highest
 *	resolution clock available on the system. There are no guarantees on
 *	what the resolution will be. In Tcl we will call this value a "click".
 *	The start time is also system dependent.
 *
 * Results:
 *	Number of WideInt clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

long long
TclpGetWideClicks(void)
{
    long long now;

#ifdef MAC_OSX_TCL
    now = (long long) (mach_absolute_time() & INT64_MAX);
#else
#error Wide high-resolution clicks not implemented on this platform
#endif /* MAC_OSX_TCL */

    return now;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpWideClicksToNanoseconds --
 *
 *	This procedure converts click values from the TclpGetWideClicks native
 *	resolution to nanosecond resolution.
 *
 * Results:
 *	Number of nanoseconds from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

double
TclpWideClicksToNanoseconds(
    long long clicks)
{
    double nsec;

#ifdef MAC_OSX_TCL
    static mach_timebase_info_data_t tb;
	static uint64_t maxClicksForUInt64;

    if (!tb.denom) {
	mach_timebase_info(&tb);
	maxClicksForUInt64 = UINT64_MAX / tb.numer;
    }
    if ((uint64_t) clicks < maxClicksForUInt64) {
	nsec = ((uint64_t) clicks) * tb.numer / tb.denom;
    } else {
	nsec = ((long double) (uint64_t) clicks) * tb.numer / tb.denom;
    }
#else
#error Wide high-resolution clicks not implemented on this platform
#endif /* MAC_OSX_TCL */

    return nsec;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpWideClickInMicrosec --
 *
 *	This procedure return scale to convert click values from the
 *	TclpGetWideClicks native resolution to microsecond resolution
 *	and back.
 *
 * Results:
 *	1 click in microseconds as double.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

double
TclpWideClickInMicrosec(void)
{
#ifdef MAC_OSX_TCL
    static int initialized = 0;
    static double scale = 0.0;

    if (!initialized) {
	mach_timebase_info_data_t tb;

	mach_timebase_info(&tb);
	/* value of tb.numer / tb.denom = 1 click in nanoseconds */
	scale = ((double) tb.numer) / tb.denom / 1000;
	initialized = 1;
    }
    return scale;
#else
#error Wide high-resolution clicks not implemented on this platform
#endif /* MAC_OSX_TCL */
}
#endif /* TCL_WIDE_CLICKS */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetMonotonicTime --
 *
 *	Gets the current monotonic time in microseconds.
 *	In the Windows case, this is the time elapsed since system
 *	startup.
 *	The current resolution is 10 to 16 milli-seconds. The implementation
 *	may be enhanced for more resolution.
 *	Thanks to Christian Werner for the implementation.
 *
 * Results:
 *	Returns the monotonic time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

long long
Tcl_GetMonotonicTime()		/* Location to store time information. */
{
    long long microSeconds;
#ifdef HAVE_CLOCK_GETTIME
    int ret;
    struct timespec ts;
    static int useMonoClock = -1;

    if (useMonoClock) {
	ret = (clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	if (useMonoClock < 0) {
	    useMonoClock = ret;
	    if (!ret) {
		(void) clock_gettime(CLOCK_REALTIME, &ts);
	    }
	} else if (!ret) {
	    Tcl_Panic("clock_gettime(CLOCK_MONOTONIC) failed");
	}
    } else {
	(void) clock_gettime(CLOCK_REALTIME, &ts);
	ret = 0;
    }
    microSeconds = (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#else
    struct timeval tv;

    (void) gettimeofday(&tv, NULL);
    microSeconds = (long long)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
    return microSeconds;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetTime --
 *
 *	Gets the current system time in seconds and microseconds since the
 *	beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 *	This function is hooked, allowing users to specify their own virtual
 *	system time.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetTime(
    Tcl_Time *timePtr)
{
    struct timeval tv;

    (void) gettimeofday(&tv, NULL);
    timePtr->sec = tv.tv_sec;
    timePtr->usec = tv.tv_usec;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
