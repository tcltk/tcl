/*
 * tclWinTime.c --
 *
 *	Contains Windows specific versions of Tcl functions that obtain time
 *	values from the operating system.
 *
 * Copyright 1995-1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#define SECSPERDAY	(60L * 60L * 24L)
#define SECSPERYEAR	(SECSPERDAY * 365L)
#define SECSPER4YEAR	(SECSPERYEAR * 4L + SECSPERDAY)

/*
 * The following arrays contain the day of year for the last day of each
 * month, where index 1 is January.
 */

static const int normalDays[] = {
    -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364
};

static const int leapDays[] = {
    -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

typedef struct ThreadSpecificData {
    char tzName[64];		/* Time zone name */
    struct tm tm;		/* time information */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/*
 * The following structure used for calculating virtual time. Virtual
 * time is always equal to:
 *    virtTimeBase + (currentPerfCounter - perfCounter)
 *			* 10000000 / nominalFreq
 */
typedef struct TimeCalibInfo {
    LONGLONG perfCounter;	/* QPC value of last calibrated virtual time */
    Tcl_WideInt virtTimeBase;	/* Last virtual time base (in 100-ns) */
    Tcl_WideInt monoTimeBase;	/* Last monotonic time base (in 100-ns) */
    Tcl_WideInt sysTime;	/* Last real system time (in 100-ns),
				   truncated to VT_SYSTMR_DIST (100ms) */
} TimeCalibInfo;

/* Milliseconds <-> 100-ns ticks */
#define MsToT100ns(ms)   ((ms) * 10000)
#define T100nsToMs(ms)   ((ms) / 10000)
/* Microseconds <-> 100-ns ticks */
#define UsToT100ns(ms)   ((ms) * 10)
#define T100nsToUs(ms)   ((ms) / 10)


/*
 * Use factor 1000 for the frequencies of QPC if it ascertained in Hz:
 *   frequency = nominal frequency / 1000
 *   native perf-counter = original perf-counter * 1000
 */
#ifndef TCL_VT_FREQ_FACTOR
#   define TCL_VT_FREQ_FACTOR 1
#endif

/* Distance in ms to obtain system timer (avoids unneeded syscalls). */
#define VT_SYSTMR_MIN_DIST	50
/* Resolution distance of the system-timer in milliseconds, 
 * should be greater as real resolution (normally 15.6ms) to make more 
 * accurate approximated part of virtual time */
#define VT_SYSTMR_DIST		250
/* Max discrepancy of virtual time to system time. Time can slow drift
 * to the drift distance (+/-5ms), if reached this distance relative
 * current system time.
 * Note: it should be greater as real timer-resolution (> 15.6ms). */
#define VT_MAX_DISCREPANCY	20
/* Max virtual time drift to shorten current distance */
#define VT_MAX_DRIFT_TIME	4

/*
 * Data for managing high-resolution timers (virtual time).
 */

typedef struct TimeInfo {
    CRITICAL_SECTION cs;	/* Mutex guarding this structure. */
    int initialized;		/* Flag == 1 if this structure is
				 * initialized. */
    int perfCounterAvailable;	/* Flag == 1 if the hardware has a performance
				 * counter. */
    LONGLONG nominalFreq;	/* Nominal frequency of the system performance
				 * counter, that is, the value returned from
				 * QueryPerformanceFrequency. */
#if TCL_VT_FREQ_FACTOR
    int	freqFactor;		/* Frequency factor (1 - KHz, 1000 - Hz) */
#endif
    LARGE_INTEGER posixEpoch;	/* Posix epoch expressed as 100-ns ticks since
				 * the windows epoch. */
    TimeCalibInfo lastCI;	/* Last virtual timer-data updated in the
				 * calibration process. */
    volatile LONG lastCIEpoch;	/* Calibration epoch (increased each 100ms) */
    
    size_t lastUsedTime;	/* Last known (caller) offset to time base
				 * (used to avoid back-drifts after calibrate) */
} TimeInfo;

static TimeInfo timeInfo = {
    { NULL, 0, 0, NULL, NULL, 0 },
    0,
    0,
    (LONGLONG) 0,
#if TCL_VT_FREQ_FACTOR
    1, /* for frequency in KHz */
#endif
#ifdef HAVE_CAST_TO_UNION
    (LARGE_INTEGER) (Tcl_WideInt) 0,
#else
    {0, 0},
#endif
    {
	(LONGLONG) 0,
	(Tcl_WideInt) 0,
	(Tcl_WideInt) 0,
    },
    (LONG) 0,
    (Tcl_WideInt) 0
};

/*
 * Scale to convert wide click values from the TclpGetWideClicks native
 * resolution to microsecond resolution and back.
 */
static struct {
    int initialized;		/* 1 if initialized, 0 otherwise */
    int perfCounter;		/* 1 if performance counter usable for wide clicks */
    double microsecsScale;	/* Denominator scale between clock / microsecs */
} wideClick = {0, 0.0};


/*
 * Declarations for functions defined later in this file.
 */

static struct tm *	ComputeGMT(const time_t *tp);
static void		NativeScaleTime(Tcl_Time* timebuf,
			    ClientData clientData);
static Tcl_WideInt	NativeGetMicroseconds(int monotonic);
static void		NativeGetTime(Tcl_Time* timebuf,
			    ClientData clientData);

/*
 * TIP #233 (Virtualized Time): Data for the time hooks, if any.
 */

Tcl_GetTimeProc *tclGetTimeProcPtr = NativeGetTime;
Tcl_ScaleTimeProc *tclScaleTimeProcPtr = NativeScaleTime;
ClientData tclTimeClientData = NULL;

/*
 *----------------------------------------------------------------------
 *
 * NativePerformanceCounter --
 *
 *	Used instead of QueryPerformanceCounter to consider frequency factor.
 *
 * Results:
 *	Returns QPC corresponding current frequency factor.
 *
 *----------------------------------------------------------------------
 */
static inline LONGLONG
NativePerformanceCounter(void) {
    LARGE_INTEGER curCounter;
    QueryPerformanceCounter(&curCounter);
#if TCL_VT_FREQ_FACTOR
    if (timeInfo.freqFactor == 1) {
    	return curCounter.QuadPart; /* no factor */
    }
    /* defactoring counter */
    return curCounter.QuadPart / timeInfo.freqFactor;
#else
    return curCounter.QuadPart; /* no factor configured */
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * NativeCalc100NsOffs --
 *
 *	Calculate the current system time in 100-ns ticks since some base,
 *	for current performance counter (curCounter), using given calibrated values.
 *
 *	offs = (curCounter - lastCI.perfCounter) * 10000000 / nominalFreq
 *
 *	vt = lastCI.virtTimeBase + offs
 *	mt = lastCI.monoTimeBase + offs
 *
 * Results:
 *	Returns the wide integer with number of 100-ns ticks from the epoch.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static inline Tcl_WideInt
NativeCalc100NsOffs(
    LONGLONG ciPerfCounter,
    LONGLONG curCounter
) {
    curCounter -= ciPerfCounter; /* current distance */
    if (!curCounter) {
    	return 0; /* virtual time without offset */
    }
    /* virtual time with offset */
    return curCounter * 10000000 / timeInfo.nominalFreq;
}

/*
 * Representing the number of 100-nanosecond intervals since posix epoch.
 */
static inline Tcl_WideInt
GetSystemTimeAsVirtual(void)
{
    FILETIME curSysTime;	/* Current system time. */
    LARGE_INTEGER curFileTime;

    /* 100-ns ticks since since Jan 1, 1601 (UTC) */
    GetSystemTimeAsFileTime(&curSysTime);
    curFileTime.LowPart = curSysTime.dwLowDateTime;
    curFileTime.HighPart = curSysTime.dwHighDateTime;
    return (Tcl_WideInt)(curFileTime.QuadPart - timeInfo.posixEpoch.QuadPart);
}

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

unsigned long
TclpGetSeconds(void)
{
    /* Try to use high resolution timer */
    if (tclGetTimeProcPtr == NativeGetTime) {
	return NativeGetMicroseconds(0) / 1000000;
    } else {
	Tcl_Time t;

	tclGetTimeProcPtr(&t, tclTimeClientData);	/* Tcl_GetTime inlined. */
	return t.sec;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetClicks --
 *
 *	This procedure returns a value that represents the highest resolution
 *	clock available on the system. There are no guarantees on what the
 *	resolution will be. In Tcl we will call this value a "click". The
 *	start time is also system dependant.
 *
 * Results:
 *	Number of clicks from some start time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned long
TclpGetClicks(void)
{
    /* Try to use high resolution timer */
    if (tclGetTimeProcPtr == NativeGetTime) {
	return (unsigned long)NativeGetMicroseconds(1);
    } else {
	/*
	* Use the Tcl_GetTime abstraction to get the time in microseconds, as
	* nearly as we can, and return it.
	*/

	Tcl_Time now;		/* Current Tcl time */

	tclGetTimeProcPtr(&now, tclTimeClientData);	/* Tcl_GetTime inlined */
	return (unsigned long)(now.sec * 1000000) + now.usec;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetWideClicks --
 *
 *	This procedure returns a WideInt value that represents the highest
 *	resolution clock in microseconds available on the system.
 *
 * Results:
 *	Number of microseconds (from some start time).
 *
 * Side effects:
 *	This should be used for time-delta resp. for measurement purposes
 *	only, because on some platforms can return microseconds from some
 *	start time (not from the epoch).
 *
 *----------------------------------------------------------------------
 */

Tcl_WideInt
TclpGetWideClicks(void)
{
    LARGE_INTEGER curCounter;

    if (!wideClick.initialized) {
	LARGE_INTEGER perfCounterFreq;

	/*
	 * The frequency of the performance counter is fixed at system boot and
	 * is consistent across all processors. Therefore, the frequency need 
	 * only be queried upon application initialization.
	 */
	if (QueryPerformanceFrequency(&perfCounterFreq)) {
	    wideClick.perfCounter = 1;
	    wideClick.microsecsScale = 1000000.0 / perfCounterFreq.QuadPart;
	} else {
	    /* fallback using microseconds */
	    wideClick.perfCounter = 0;
	    wideClick.microsecsScale = 1;
	}
	
	wideClick.initialized = 1;
    }
    if (wideClick.perfCounter) {
	if (QueryPerformanceCounter(&curCounter)) {
	    return (Tcl_WideInt)curCounter.QuadPart;
	}
	/* fallback using microseconds */
	wideClick.perfCounter = 0;
	wideClick.microsecsScale = 1;
	return TclpGetMicroseconds();
    } else {
    	return TclpGetMicroseconds();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpWideClickInMicrosec --
 *
 *	This procedure return scale to convert wide click values from the 
 *	TclpGetWideClicks native resolution to microsecond resolution
 *	and back.
 *
 * Results:
 * 	1 click in microseconds as double.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

double
TclpWideClickInMicrosec(void)
{
    if (!wideClick.initialized) {
    	(void)TclpGetWideClicks();	/* initialize */
    }
    return wideClick.microsecsScale;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetMicroseconds --
 *
 *	This procedure returns a WideInt value that represents the highest
 *	resolution clock in microseconds available on the system.
 *
 * Results:
 *	Number of microseconds (from the epoch).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_WideInt 
TclpGetMicroseconds(void)
{
    /* Use high resolution timer if possible */
    if (tclGetTimeProcPtr == NativeGetTime) {
    	return NativeGetMicroseconds(0);
    } else {
	/*
	 * Use the Tcl_GetTime abstraction to get the time in microseconds, as
	 * nearly as we can, and return it.
	 */

	Tcl_Time now;

	tclGetTimeProcPtr(&now, tclTimeClientData);	/* Tcl_GetTime inlined */
	return TCL_TIME_TO_USEC(now);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetMicroseconds --
 *
 *	This procedure returns a WideInt value that represents the highest
 *	resolution clock in microseconds available on the system.
 *
 * Results:
 *	Number of microseconds (from the epoch).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_WideInt 
TclpGetUTimeMonotonic(void)
{
    /* Use high resolution timer if possible */
    if (tclGetTimeProcPtr == NativeGetTime) {
    	return NativeGetMicroseconds(1); /* monotonic based time */
    } else {
	/*
	 * Use the Tcl_GetTime abstraction to get the time in microseconds, as
	 * nearly as we can, and return it.
	 */

	Tcl_Time now;

	tclGetTimeProcPtr(&now, tclTimeClientData);	/* Tcl_GetTime inlined */
	return TCL_TIME_TO_USEC(now);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTimeZone --
 *
 *	Determines the current timezone. The method varies wildly between
 *	different Platform implementations, so its hidden in this function.
 *
 * Results:
 *	Minutes west of GMT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpGetTimeZone(
    unsigned long currentTime)
{
    int timeZone;

    tzset();
    timeZone = timezone / 60;

    return timeZone;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetTime --
 *
 *	Gets the current system time in seconds and microseconds since the
 *	beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	On the first call, initializes a set of static variables to keep track
 *	of the base value of the performance counter, the corresponding wall
 *	clock (obtained through ftime) and the frequency of the performance
 *	counter. Also spins a thread whose function is to wake up periodically
 *	and monitor these values, adjusting them as necessary to correct for
 *	drift in the performance counter's oscillator.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_GetTime(
    Tcl_Time *timePtr)		/* Location to store time information. */
{
    /* Try to use high resolution timer */
    if ( tclGetTimeProcPtr == NativeGetTime) {
    	Tcl_WideInt now = NativeGetMicroseconds(0);
	timePtr->sec = (long) (now / 1000000);
	timePtr->usec = (unsigned long) (now % 1000000);
    } else {
    	tclGetTimeProcPtr(timePtr, tclTimeClientData);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NativeScaleTime --
 *
 *	TIP #233: Scale from virtual time to the real-time. For native scaling
 *	the relationship is 1:1 and nothing has to be done.
 *
 * Results:
 *	Scales the time in timePtr.
 *
 * Side effects:
 *	See above.
 *
 *----------------------------------------------------------------------
 */

static void
NativeScaleTime(
    Tcl_Time *timePtr,
    ClientData clientData)
{
    /*
     * Native scale is 1:1. Nothing is done.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * TclpScaleUTime --
 *
 *	This procedure scales number of microseconds if expected.
 *
 * Results:
 *	Number of microseconds scaled using tclScaleTimeProcPtr.
 *
 *----------------------------------------------------------------------
 */
void
TclpScaleUTime(
    Tcl_WideInt *usec)
{
    /* Native scale is 1:1. */
    if (tclScaleTimeProcPtr != NativeScaleTime) {
	return;
    } else {
	Tcl_Time scTime;
	scTime.sec = *usec / 1000000;
	scTime.usec = *usec % 1000000;
	tclScaleTimeProcPtr(&scTime, tclTimeClientData);
	*usec = ((Tcl_WideInt)scTime.sec) * 1000000 + scTime.usec;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NativeGetMicroseconds --
 *
 *	Gets the current system time in microseconds since the beginning
 *	of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the wide integer with number of microseconds from the epoch, or
 *	0 if high resolution timer is not available.
 *
 * Side effects:
 *	On the first call, initializes a set of static variables to keep track
 *	of the base value of the performance counter, the corresponding wall
 *	clock (obtained through ftime) and the frequency of the performance
 *	counter. Also spins a thread whose function is to wake up periodically
 *	and monitor these values, adjusting them as necessary to correct for
 *	drift in the performance counter's oscillator.
 *
 *----------------------------------------------------------------------
 */

static Tcl_WideInt
NativeGetMicroseconds(
    int monotonic)
{
    static size_t nomObtainSTPerfCntrDist = 0;
				/* Nominal distance in perf-counter ticks to
				 * obtain system timer (avoids unneeded syscalls). */
    Tcl_WideInt curTime;	/* Current time in 100-ns ticks since epoch */

    /*
     * Initialize static storage on the first trip through.
     *
     * Note: Outer check for 'initialized' is a performance win since it
     * avoids an extra mutex lock in the common case.
     */

    if (!timeInfo.initialized) {
    	LARGE_INTEGER nominalFreq;
	TclpInitLock();
	if (!timeInfo.initialized) {

	    timeInfo.posixEpoch.LowPart = 0xD53E8000;
	    timeInfo.posixEpoch.HighPart = 0x019DB1DE;

	    if ((timeInfo.perfCounterAvailable =
		    QueryPerformanceFrequency(&nominalFreq))
	    ) {
		timeInfo.nominalFreq = nominalFreq.QuadPart;

		/*
		 * We devide by timeInfo.nominalFreq in several places.
		 */
		if (timeInfo.nominalFreq == 0) {
		    timeInfo.perfCounterAvailable = FALSE;
		}
#if TCL_VT_FREQ_FACTOR
		/* Some systems having frequency in Hz, so save the factor here */
		if (timeInfo.nominalFreq >= 1000000000 
			&& (timeInfo.nominalFreq % 1000) == 0) {
		    /* assume that frequency in Hz, factor used only for tolerance */
		    timeInfo.freqFactor = 1000;
		    timeInfo.nominalFreq /= timeInfo.freqFactor;
		}
#endif
		/* Distance in perf-counter ticks for VT_SYSTMR_MIN_DIST (ms) */
		nomObtainSTPerfCntrDist = (size_t)
			(timeInfo.nominalFreq * MsToT100ns(VT_SYSTMR_MIN_DIST))
			    / 10000000;
	    }

	    /*
	     * Some hardware abstraction layers use the CPU clock in place of
	     * the real-time clock as a performance counter reference. This
	     * results in:
	     *    - inconsistent results among the processors on
	     *      multi-processor systems.
	     *    - unpredictable changes in performance counter frequency on
	     *      "gearshift" processors such as Transmeta and SpeedStep.
	     *
	     * There seems to be no way to test whether the performance
	     * counter is reliable, but a useful heuristic is that if its
	     * frequency is 1.193182 MHz or 3.579545 MHz, it's derived from a
	     * colorburst crystal and is therefore the RTC rather than the
	     * TSC.
	     *
	     * A sloppier but serviceable heuristic is that the RTC crystal is
	     * normally less than 15 MHz while the TSC crystal is virtually
	     * assured to be greater than 100 MHz. Since Win98SE appears to
	     * fiddle with the definition of the perf counter frequency
	     * (perhaps in an attempt to calibrate the clock?), we use the
	     * latter rule rather than an exact match.
	     *
	     * We also assume (perhaps questionably) that the vendors have
	     * gotten their act together on Win64, so bypass all this rubbish
	     * on that platform.
	     */

#if !defined(_WIN64)
	    if (timeInfo.perfCounterAvailable
		    /*
		     * The following lines would do an exact match on crystal
		     * frequency:
		     * && timeInfo.nominalFreq != 1193182
		     * && timeInfo.nominalFreq != 3579545
		     */
		    && timeInfo.nominalFreq > 15000000){
		/*
		 * As an exception, if every logical processor on the system
		 * is on the same chip, we use the performance counter anyway,
		 * presuming that everyone's TSC is locked to the same
		 * oscillator.
		 */

		SYSTEM_INFO systemInfo;
		unsigned int regs[4];

		GetSystemInfo(&systemInfo);

		if (TclWinCPUID(0, regs) == TCL_OK
			&& regs[1] == 0x756e6547	/* "Genu" */
			&& regs[3] == 0x49656e69	/* "ineI" */
			&& regs[2] == 0x6c65746e	/* "ntel" */
			&& TclWinCPUID(1, regs) == TCL_OK
			&& (( ((regs[0]&0x00000F00) == 0xF00) /* Pentium 4 */
			   || ((regs[0]&0x00000F00) == 0x600) ) /* or compatible (VM) */
			&& ((regs[0] & 0x0FF00000)	/* Extended family (bits 20-27) */
			|| (regs[3] & 0x10000000)))	/* Hyperthread (bit 28) */
			|| (((regs[1]&0x00FF0000) >> 16) >= 2 /* CPU count */
			    || systemInfo.dwNumberOfProcessors >= 2)) {
		    timeInfo.perfCounterAvailable = TRUE;
		} else {
		    timeInfo.perfCounterAvailable = FALSE;
		}

	    }
#endif /* above code is Win32 only */

	    /*
	     * If the performance counter is available, initialize
	     */

	    if (timeInfo.perfCounterAvailable) {
		InitializeCriticalSection(&timeInfo.cs);

		timeInfo.lastCI.perfCounter = NativePerformanceCounter();
		/* base of the real-time (and last known system time) */
		timeInfo.lastCI.sysTime =
		    timeInfo.lastCI.virtTimeBase = GetSystemTimeAsVirtual();

		/* base of the monotonic time */
		timeInfo.lastCI.monoTimeBase = NativeCalc100NsOffs(
			0, timeInfo.lastCI.perfCounter);
	    }
	    timeInfo.initialized = TRUE;
	}
	TclpInitUnlock();
    }

    if (timeInfo.perfCounterAvailable) {

	static LONGLONG lastObtainSTPerfCntr = 0; 
				/* Last perf-counter system timer was obtained. */
	TimeCalibInfo ci;	/* Copy of common base/offset used to calc VT. */
	volatile LONG ciEpoch;	/* Epoch of "ci", protecting this structure. */
	Tcl_WideInt sysTime, trSysTime;
				/* System time and truncated (rounded) time. */
	LONGLONG curCounter;	/* Current value of native QPC. */

	/*
	 * Try to acquire data without lock (same epoch at end of copy process).
	 */
	ciEpoch = timeInfo.lastCIEpoch;
	memcpy(&ci, &timeInfo.lastCI, sizeof(ci));
	/*
	 * Lock on demand and hold time section locked as short as possible.
	 */
	if (InterlockedCompareExchange(&timeInfo.lastCIEpoch, 
				ciEpoch, ciEpoch) != ciEpoch) {
	    EnterCriticalSection(&timeInfo.cs);
	    if (ciEpoch != timeInfo.lastCIEpoch) {
		memcpy(&ci, &timeInfo.lastCI, sizeof(ci));
		ciEpoch = timeInfo.lastCIEpoch;
	    }
	    LeaveCriticalSection(&timeInfo.cs);
	}

	/* Query current performance counter. */
	curCounter = NativePerformanceCounter();
	
	/* Avoid doing unneeded syscall too often */
	if ( curCounter >= lastObtainSTPerfCntr
	  && curCounter < lastObtainSTPerfCntr + nomObtainSTPerfCntrDist
	) {
	    goto calcVT; /* don't check system time (curCounter precise enough) */
	}
	lastObtainSTPerfCntr = curCounter;

	/* Query non-precise system time */
	sysTime = GetSystemTimeAsVirtual();
	/*
	 * Truncate non-precise part of the system time (to VT_SYSTMR_DIST ms)
	 */
	trSysTime = sysTime;
	trSysTime /= MsToT100ns(VT_SYSTMR_DIST); /* VT_SYSTMR_DIST ms (in 100ns)*/
	trSysTime *= MsToT100ns(VT_SYSTMR_DIST);

	/*
	 * If rounded system time is changed - recalibrate offsets/base values
	 */ 
	if (ci.sysTime != trSysTime) { /* next interval VT_SYSTMR_DIST ms */
	  EnterCriticalSection(&timeInfo.cs);
	  if (ci.sysTime != trSysTime) { /* again in lock (done in other thread) */

	    /*
	     * Recalibration / Adjustment of base values.
	     */

	    Tcl_WideInt vt0, vt1;	/* Desired virtual time */
	    Tcl_WideInt tdiff;		/* Time difference to the system time */
	    Tcl_WideInt lastTime;	/* Used to compare with last known time */

	    /* New desired virtual time using current base values */
	    vt1 = vt0 = ci.virtTimeBase
		+ NativeCalc100NsOffs(ci.perfCounter, curCounter);

	    tdiff = vt0 - sysTime;
	    /* If we can adjust offsets (not a jump to new system time) */
	    if (MsToT100ns(-800) < tdiff && tdiff < MsToT100ns(800)) {

		/* Allow small drift if discrepancy larger as expected */
		if (tdiff <= MsToT100ns(-VT_MAX_DISCREPANCY)) {
		    vt0 += MsToT100ns(VT_MAX_DRIFT_TIME);
		}
		else
		if (tdiff <= MsToT100ns(-VT_MAX_DRIFT_TIME)) {
		    vt0 -= tdiff / 2; /* small drift forwards */
		}
		else
		if (tdiff >= MsToT100ns(VT_MAX_DISCREPANCY)) {
		    vt0 -= MsToT100ns(VT_MAX_DRIFT_TIME);
		}
		
		/*
		 * Be sure the clock ticks never backwards (avoid backwards 
		 * time-drifts). If time-reset (< 800ms) just use curent time
		 * (avoid time correction in such case).
		 */
		if ( (lastTime = (ci.virtTimeBase + timeInfo.lastUsedTime))
		  && (lastTime -= vt0) > 0 /* offset to vt0 */
		  && lastTime < MsToT100ns(800) /* bypass time-switch (drifts only) */
		) {
		    vt0 += lastTime; /* hold on the time a bit */
		}

		/* difference for addjustment of monotonic base */
		tdiff = vt0 - vt1;

	    } else {
		/* 
		 * The time-jump (reset or initial), we should use system time
		 * instead of virtual to recalibrate offsets (let the time jump).
		 */
		vt0 = sysTime;
		tdiff = 0;
	    }

	    /*
	     * Now adjust monotonic time base, note this time should absolutely
	     * never ticks backwards (relative the last known monotonic time).
	     */
	    ci.monoTimeBase += NativeCalc100NsOffs(ci.perfCounter, curCounter);
	    ci.monoTimeBase += tdiff;
	    lastTime = (timeInfo.lastCI.monoTimeBase + timeInfo.lastUsedTime);
	    if (ci.monoTimeBase < lastTime) {
		ci.monoTimeBase = lastTime; /* freeze monotonic time a bit */
	    }

	    /* 
	     * Adjustment of current base for virtual time. This will also
	     * prevent too large counter difference (resp. max distance ~ 100ms).
	     */
	    ci.virtTimeBase = vt0;
	    ci.perfCounter = curCounter;
	    ci.sysTime = trSysTime;
	    /* base adjusted, so reset also last known offset */
	    timeInfo.lastUsedTime = 0;

	    /* Update global structure lastCI with new values */
	    memcpy(&timeInfo.lastCI, &ci, sizeof(ci));
	    /* Increase epoch, to inform all other threads about new data */
	    InterlockedIncrement(&timeInfo.lastCIEpoch);
  
	  } /* end lock */
	  LeaveCriticalSection(&timeInfo.cs);
	} /* common info lastCI contains actual data */
	
    calcVT:

	/* Calculate actual time-offset using performance counter */
	curTime = NativeCalc100NsOffs(ci.perfCounter, curCounter);
	/* Save last used time (offset) */
	timeInfo.lastUsedTime = (size_t)curTime;
	if (monotonic) {
	    /* Use monotonic time base */
	    curTime += ci.monoTimeBase;
	} else {
	    /* Use real-time base */
	    curTime += ci.virtTimeBase;
	}
	/* Return virtual time */
	return T100nsToUs(curTime); /* 100-ns to microseconds */
    }

    /*
     * High resolution timer is not available.
     */

    curTime = GetSystemTimeAsVirtual(); /* in 100-ns ticks */
    return T100nsToUs(curTime); /* 100-ns to microseconds */
}

/*
 *----------------------------------------------------------------------
 *
 * NativeGetTime --
 *
 *	TIP #233: Gets the current system time in seconds and microseconds
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time in timePtr.
 *
 * Side effects:
 *	See NativeGetMicroseconds for more information.
 *
 *----------------------------------------------------------------------
 */

static void
NativeGetTime(
    Tcl_Time *timePtr,
    ClientData clientData)
{
    Tcl_WideInt now;

    now = NativeGetMicroseconds(0);
    timePtr->sec = (long) (now / 1000000);
    timePtr->usec = (unsigned long) (now % 1000000);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetTZName --
 *
 *	Gets the current timezone string.
 *
 * Results:
 *	Returns a pointer to a static string, or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclpGetTZName(
    int dst)
{
    int len;
    char *zone, *p;
    TIME_ZONE_INFORMATION tz;
    Tcl_Encoding encoding;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    char *name = tsdPtr->tzName;

    /*
     * tzset() under Borland doesn't seem to set up tzname[] at all.
     * tzset() under MSVC has the following weird observed behavior:
     *	 First time we call "clock format [clock seconds] -format %Z -gmt 1"
     *	 we get "GMT", but on all subsequent calls we get the current time
     *	 ezone string, even though env(TZ) is GMT and the variable _timezone
     *	 is 0.
     */

    name[0] = '\0';

    zone = getenv("TZ");
    if (zone != NULL) {
	/*
	 * TZ is of form "NST-4:30NDT", where "NST" would be the name of the
	 * standard time zone for this area, "-4:30" is the offset from GMT in
	 * hours, and "NDT is the name of the daylight savings time zone in
	 * this area. The offset and DST strings are optional.
	 */

	len = strlen(zone);
	if (len > 3) {
	    len = 3;
	}
	if (dst != 0) {
	    /*
	     * Skip the offset string and get the DST string.
	     */

	    p = zone + len;
	    p += strspn(p, "+-:0123456789");
	    if (*p != '\0') {
		zone = p;
		len = strlen(zone);
		if (len > 3) {
		    len = 3;
		}
	    }
	}
	Tcl_ExternalToUtf(NULL, NULL, zone, len, 0, NULL, name,
		sizeof(tsdPtr->tzName), NULL, NULL, NULL);
    }
    if (name[0] == '\0') {
	if (GetTimeZoneInformation(&tz) == TIME_ZONE_ID_UNKNOWN) {
	    /*
	     * MSDN: On NT this is returned if DST is not used in the current
	     * TZ
	     */

	    dst = 0;
	}
	encoding = Tcl_GetEncoding(NULL, "unicode");
	Tcl_ExternalToUtf(NULL, encoding,
		(char *) ((dst) ? tz.DaylightName : tz.StandardName), -1,
		0, NULL, name, sizeof(tsdPtr->tzName), NULL, NULL, NULL);
	Tcl_FreeEncoding(encoding);
    }
    return name;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetDate --
 *
 *	This function converts between seconds and struct tm. If useGMT is
 *	true, then the returned date will be in Greenwich Mean Time (GMT).
 *	Otherwise, it will be in the local time zone.
 *
 * Results:
 *	Returns a static tm structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

struct tm *
TclpGetDate(
    CONST time_t *t,
    int useGMT)
{
    struct tm *tmPtr;
    time_t time;

    if (!useGMT) {
#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#	undef timezone /* prevent conflict with timezone() function */
	long timezone = 0;
#endif

	tzset();

	/*
	 * If we are in the valid range, let the C run-time library handle it.
	 * Otherwise we need to fake it. Note that this algorithm ignores
	 * daylight savings time before the epoch.
	 */

	/*
	 * Hm, Borland's localtime manages to return NULL under certain
	 * circumstances (e.g. wintime.test, test 1.2). Nobody tests for this,
	 * since 'localtime' isn't supposed to do this, possibly leading to
	 * crashes.
	 *
	 * Patch: We only call this function if we are at least one day into
	 * the epoch, else we handle it ourselves (like we do for times < 0).
	 * H. Giese, June 2003
	 */

#ifdef __BORLANDC__
#define LOCALTIME_VALIDITY_BOUNDARY	SECSPERDAY
#else
#define LOCALTIME_VALIDITY_BOUNDARY	0
#endif

	if (*t >= LOCALTIME_VALIDITY_BOUNDARY) {
	    return TclpLocaltime(t);
	}

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
	_get_timezone(&timezone);
#endif

	time = *t - timezone;

	/*
	 * If we aren't near to overflowing the long, just add the bias and
	 * use the normal calculation. Otherwise we will need to adjust the
	 * result at the end.
	 */

	if (*t < (LONG_MAX - 2*SECSPERDAY) && *t > (LONG_MIN + 2*SECSPERDAY)) {
	    tmPtr = ComputeGMT(&time);
	} else {
	    tmPtr = ComputeGMT(t);

	    tzset();

	    /*
	     * Add the bias directly to the tm structure to avoid overflow.
	     * Propagate seconds overflow into minutes, hours and days.
	     */

	    time = tmPtr->tm_sec - timezone;
	    tmPtr->tm_sec = (int)(time % 60);
	    if (tmPtr->tm_sec < 0) {
		tmPtr->tm_sec += 60;
		time -= 60;
	    }

	    time = tmPtr->tm_min + time/60;
	    tmPtr->tm_min = (int)(time % 60);
	    if (tmPtr->tm_min < 0) {
		tmPtr->tm_min += 60;
		time -= 60;
	    }

	    time = tmPtr->tm_hour + time/60;
	    tmPtr->tm_hour = (int)(time % 24);
	    if (tmPtr->tm_hour < 0) {
		tmPtr->tm_hour += 24;
		time -= 24;
	    }

	    time /= 24;
	    tmPtr->tm_mday += (int)time;
	    tmPtr->tm_yday += (int)time;
	    tmPtr->tm_wday = (tmPtr->tm_wday + (int)time) % 7;
	}
    } else {
	tmPtr = ComputeGMT(t);
    }
    return tmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeGMT --
 *
 *	This function computes GMT given the number of seconds since the epoch
 *	(midnight Jan 1 1970).
 *
 * Results:
 *	Returns a (per thread) statically allocated struct tm.
 *
 * Side effects:
 *	Updates the values of the static struct tm.
 *
 *----------------------------------------------------------------------
 */

static struct tm *
ComputeGMT(
    const time_t *tp)
{
    struct tm *tmPtr;
    long tmp, rem;
    int isLeap;
    const int *days;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    tmPtr = &tsdPtr->tm;

    /*
     * Compute the 4 year span containing the specified time.
     */

    tmp = (long)(*tp / SECSPER4YEAR);
    rem = (long)(*tp % SECSPER4YEAR);

    /*
     * Correct for weird mod semantics so the remainder is always positive.
     */

    if (rem < 0) {
	tmp--;
	rem += SECSPER4YEAR;
    }

    /*
     * Compute the year after 1900 by taking the 4 year span and adjusting for
     * the remainder. This works because 2000 is a leap year, and 1900/2100
     * are out of the range.
     */

    tmp = (tmp * 4) + 70;
    isLeap = 0;
    if (rem >= SECSPERYEAR) {			  /* 1971, etc. */
	tmp++;
	rem -= SECSPERYEAR;
	if (rem >= SECSPERYEAR) {		  /* 1972, etc. */
	    tmp++;
	    rem -= SECSPERYEAR;
	    if (rem >= SECSPERYEAR + SECSPERDAY) { /* 1973, etc. */
		tmp++;
		rem -= SECSPERYEAR + SECSPERDAY;
	    } else {
		isLeap = 1;
	    }
	}
    }
    tmPtr->tm_year = tmp;

    /*
     * Compute the day of year and leave the seconds in the current day in the
     * remainder.
     */

    tmPtr->tm_yday = rem / SECSPERDAY;
    rem %= SECSPERDAY;

    /*
     * Compute the time of day.
     */

    tmPtr->tm_hour = rem / 3600;
    rem %= 3600;
    tmPtr->tm_min = rem / 60;
    tmPtr->tm_sec = rem % 60;

    /*
     * Compute the month and day of month.
     */

    days = (isLeap) ? leapDays : normalDays;
    for (tmp = 1; days[tmp] < tmPtr->tm_yday; tmp++) {
	/* empty body */
    }
    tmPtr->tm_mon = --tmp;
    tmPtr->tm_mday = tmPtr->tm_yday - days[tmp];

    /*
     * Compute day of week.  Epoch started on a Thursday.
     */

    tmPtr->tm_wday = (long)(*tp / SECSPERDAY) + 4;
    if ((*tp % SECSPERDAY) < 0) {
	tmPtr->tm_wday--;
    }
    tmPtr->tm_wday %= 7;
    if (tmPtr->tm_wday < 0) {
	tmPtr->tm_wday += 7;
    }

    return tmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGmtime --
 *
 *	Wrapper around the 'gmtime' library function to make it thread safe.
 *
 * Results:
 *	Returns a pointer to a 'struct tm' in thread-specific data.
 *
 * Side effects:
 *	Invokes gmtime or gmtime_r as appropriate.
 *
 *----------------------------------------------------------------------
 */

struct tm *
TclpGmtime(
    CONST time_t *timePtr)	/* Pointer to the number of seconds since the
				 * local system's epoch */
{
    /*
     * The MS implementation of gmtime is thread safe because it returns the
     * time in a block of thread-local storage, and Windows does not provide a
     * Posix gmtime_r function.
     */

    return gmtime(timePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpLocaltime --
 *
 *	Wrapper around the 'localtime' library function to make it thread
 *	safe.
 *
 * Results:
 *	Returns a pointer to a 'struct tm' in thread-specific data.
 *
 * Side effects:
 *	Invokes localtime or localtime_r as appropriate.
 *
 *----------------------------------------------------------------------
 */

struct tm *
TclpLocaltime(
    CONST time_t *timePtr)	/* Pointer to the number of seconds since the
				 * local system's epoch */

{
    /*
     * The MS implementation of localtime is thread safe because it returns
     * the time in a block of thread-local storage, and Windows does not
     * provide a Posix localtime_r function.
     */

    return localtime(timePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetTimeProc --
 *
 *	TIP #233 (Virtualized Time): Registers two handlers for the
 *	virtualization of Tcl's access to time information.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remembers the handlers, alters core behaviour.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetTimeProc(
    Tcl_GetTimeProc *getProc,
    Tcl_ScaleTimeProc *scaleProc,
    ClientData clientData)
{
    tclGetTimeProcPtr = getProc;
    tclScaleTimeProcPtr = scaleProc;
    tclTimeClientData = clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_QueryTimeProc --
 *
 *	TIP #233 (Virtualized Time): Query which time handlers are registered.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_QueryTimeProc(
    Tcl_GetTimeProc **getProc,
    Tcl_ScaleTimeProc **scaleProc,
    ClientData *clientData)
{
    if (getProc) {
	*getProc = tclGetTimeProcPtr;
    }
    if (scaleProc) {
	*scaleProc = tclScaleTimeProcPtr;
    }
    if (clientData) {
	*clientData = tclTimeClientData;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
