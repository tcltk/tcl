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
 * The following values are used for calculating virtual time. Virtual
 * time is always equal to:
 *    virtTime + (currentPerfCounter - perfCounter + cntrVariance)
 *			* 10000000 / nominalFreq
 */
typedef struct TimeCalibInfo {
    ULONGLONG fileTime;		/* Last real time (in 100-ns) */
    ULONGLONG virtTime;		/* Last virtual time (in 100-ns) */
    LONGLONG perfCounter;	/* QPC value of last calibration time */
    LONGLONG cntrVariance;	/* Current calculated deviation (compensation) */
    Tcl_WideInt calibNextTime;	/* Next time of calibration (in 100-ns ticks) */
} TimeCalibInfo;

/*
 * Data for managing high-resolution timers.
 */

typedef struct TimeInfo {
    CRITICAL_SECTION cs;	/* Mutex guarding this structure. */
    int initialized;		/* Flag == 1 if this structure is
				 * initialized. */
    int perfCounterAvailable;	/* Flag == 1 if the hardware has a performance
				 * counter. */
    HANDLE calibrationThread;	/* Handle to the thread that keeps the virtual
				 * clock calibrated. */
    HANDLE readyEvent;		/* System event used to trigger the requesting
				 * thread when the clock calibration procedure
				 * is initialized for the first time. */
    HANDLE exitEvent; 		/* Event to signal out of an exit handler to
				 * tell the calibration loop to terminate. */
    LONGLONG nominalFreq;	/* Nominal frequency of the system performance
				 * counter, that is, the value returned from
				 * QueryPerformanceFrequency. */
    int	freqFactor;		/* Frequency factor (1 - KHz, 1000 - Hz) */
    LARGE_INTEGER posixEpoch;	/* Posix epoch expressed as 100-ns ticks since
				 * the windows epoch. */
    /*
     * The following values are used for calculating virtual time. Virtual
     * time is always equal to:
     *    virtTime + ( (currentPerfCounter - perfCounter) * 10000000
     *				+ cntrVariance) / nominalFreq
     */

    TimeCalibInfo lastCC;	/* Last data updated in calibration cycle */
    volatile LONG calibEpoch;	/* Calibration epoch */
    
    Tcl_WideInt lastUsedTime;	/* Last known (caller) virtual time in 100-ns
				 * (used to avoid drifts after calibrate) */

} TimeInfo;

static TimeInfo timeInfo = {
    { NULL, 0, 0, NULL, NULL, 0 },
    0,
    0,
    (HANDLE) NULL,
    (HANDLE) NULL,
    (HANDLE) NULL,
    (LONGLONG) 0,
    1, /* for frequency in KHz */
#ifdef HAVE_CAST_TO_UNION
    (LARGE_INTEGER) (Tcl_WideInt) 0,
#else
    {0, 0},
#endif
    {
	(Tcl_WideInt) 0,
	(ULONGLONG) 0,
	(ULONGLONG) 0,
	(LONGLONG) 0,
	(LONGLONG) 0
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
static void		StopCalibration(ClientData clientData);
static DWORD WINAPI	CalibrationThread(LPVOID arg);
static void 		UpdateTimeEachSecond(void);
static void		NativeScaleTime(Tcl_Time* timebuf,
			    ClientData clientData);
static Tcl_WideInt	NativeGetMicroseconds(void);
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
    return (curCounter.QuadPart / timeInfo.freqFactor);
}

/*
 *----------------------------------------------------------------------
 *
 * NativeCalc100NsTicks --
 *
 *	Calculate the current system time in 100-ns ticks since posix epoch,
 *	for current performance counter (curCounter), using given calibrated values.
 *
 *	vt = lastCC.virtTime 
 *		+ ( (curPerfCounter - lastCC.perfCounter) * 10000000
 * 			+ lastCC.cntrVariance ) / nominalFreq
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
NativeCalc100NsTicks(
    ULONGLONG ccVirtTime,
    LONGLONG ccPerfCounter,
    LONGLONG ccCntrVariance,
    LONGLONG curCounter
) {
    return ccVirtTime + ( (curCounter - ccPerfCounter) * 10000000
				+ ccCntrVariance ) / timeInfo.nominalFreq;
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
    Tcl_WideInt usecSincePosixEpoch;

    /* Try to use high resolution timer */
    if ( tclGetTimeProcPtr == NativeGetTime
      && (usecSincePosixEpoch = NativeGetMicroseconds())
    ) {
	return usecSincePosixEpoch / 1000000;
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
    Tcl_WideInt usecSincePosixEpoch;

    /* Try to use high resolution timer */
    if ( tclGetTimeProcPtr == NativeGetTime
      && (usecSincePosixEpoch = NativeGetMicroseconds())
    ) {
	return (unsigned long)usecSincePosixEpoch;
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
    static Tcl_WideInt prevUS = 0;
    static Tcl_WideInt fileTimeLastCall, perfCounterLastCall, curCntrVariance;
    static LONGLONG prevPerfCounter;
    LONGLONG newPerfCounter;

    Tcl_WideInt usecSincePosixEpoch;

    /* Try to use high resolution timer */
    if (tclGetTimeProcPtr == NativeGetTime) {
	if ( !(usecSincePosixEpoch = NativeGetMicroseconds()) ) {
	    usecSincePosixEpoch = GetSystemTimeAsVirtual() / 10; /* in 100-ns */
	    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!no-native-ms!!!!!!!!!!!\n");
	}
    } else {
	/*
	* Use the Tcl_GetTime abstraction to get the time in microseconds, as
	* nearly as we can, and return it.
	*/

	Tcl_Time now;

	tclGetTimeProcPtr(&now, tclTimeClientData);	/* Tcl_GetTime inlined */
	usecSincePosixEpoch = (((Tcl_WideInt)now.sec) * 1000000) + now.usec;
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!no-native-ms!!!!!!!!!!!\n");
    }

	newPerfCounter = NativePerformanceCounter();

    	if (prevUS && usecSincePosixEpoch < prevUS) {
    	    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!time-backwards!!!! pre-struct: %I64d, %I64d, %I64d, %I64d == %I64d \n", fileTimeLastCall, perfCounterLastCall, prevPerfCounter, curCntrVariance,
    	    	NativeCalc100NsTicks(fileTimeLastCall,
		perfCounterLastCall, curCntrVariance,
		prevPerfCounter));
    	    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!time-backwards!!!! new-struct: %I64d, %I64d, %I64d, %I64d == %I64d \n", timeInfo.lastCC.virtTime, timeInfo.lastCC.perfCounter, newPerfCounter, timeInfo.lastCC.cntrVariance,
    	    	NativeCalc100NsTicks(timeInfo.lastCC.virtTime,
		timeInfo.lastCC.perfCounter, timeInfo.lastCC.cntrVariance,
		newPerfCounter));
	    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!time-backwards!!!! prev: %I64d - now: %I64d (%I64d usec)\n", prevUS, usecSincePosixEpoch, usecSincePosixEpoch - prevUS);
	    Tcl_Panic("Time running backwards!!!");
    	}
    	prevUS = usecSincePosixEpoch;
	fileTimeLastCall = timeInfo.lastCC.virtTime;
	perfCounterLastCall = timeInfo.lastCC.perfCounter;
	curCntrVariance = timeInfo.lastCC.cntrVariance;
	prevPerfCounter = newPerfCounter;

	return usecSincePosixEpoch;
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
    Tcl_WideInt usecSincePosixEpoch;

    /* Try to use high resolution timer */
    if ( tclGetTimeProcPtr == NativeGetTime
      && (usecSincePosixEpoch = NativeGetMicroseconds())
    ) {
	timePtr->sec = (long) (usecSincePosixEpoch / 1000000);
	timePtr->usec = (unsigned long) (usecSincePosixEpoch % 1000000);
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
NativeGetMicroseconds(void)
{
    Tcl_WideInt curTime;	/* Current time in 100-ns ticks since epoch */
    Tcl_WideInt lastTime;	/* Used to compare with last known time */

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
		/* Some systems having frequency in Hz, so save the factor here */
		if (timeInfo.nominalFreq >= 1000000000 
			&& (timeInfo.nominalFreq % 1000) == 0) {
		    timeInfo.nominalFreq /= 1000;
		    /* assume that frequency in Hz, factor used only for tolerance */
		    timeInfo.freqFactor = 1000;
		}
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
		if (TclWinCPUID(1, regs) == TCL_OK) {
		printf("********* system pen: %d, hyperthread: %d, cpu-count: %d\n, cpu-num: %d\n", 
			(regs[0]&0x00000F00) == 0x00000F00 /* Pentium 4 */
			,((regs[0] & 0x0FF00000)	/* Extended family (bits 20-27) */
			&& (regs[3] & 0x10000000)),
			((regs[1]&0x00FF0000) >> 16)/* CPU count */,
			systemInfo.dwNumberOfProcessors
		);}

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
		printf("********* available %d\n", timeInfo.perfCounterAvailable);

	    }
#endif /* above code is Win32 only */

	    /*
	     * If the performance counter is available, start a thread to
	     * calibrate it.
	     */

	    if (timeInfo.perfCounterAvailable) {
		DWORD id;

		InitializeCriticalSection(&timeInfo.cs);
		timeInfo.readyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		timeInfo.exitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		timeInfo.calibrationThread = CreateThread(NULL, 256,
			CalibrationThread, (LPVOID) NULL, 0, &id);
		SetThreadPriority(timeInfo.calibrationThread,
			THREAD_PRIORITY_HIGHEST);

		/*
		 * Wait for the thread just launched to start running, and
		 * create an exit handler that kills it so that it doesn't
		 * outlive unloading tclXX.dll
		 */

		WaitForSingleObject(timeInfo.readyEvent, INFINITE);
		CloseHandle(timeInfo.readyEvent);
		Tcl_CreateExitHandler(StopCalibration, (ClientData) NULL);
	    }
	    timeInfo.initialized = TRUE;
	}
	TclpInitUnlock();
    }

    if (timeInfo.perfCounterAvailable) {

	/* Copies with current data of calibration cycle */
	static TimeCalibInfo commonCC;
	static volatile LONG calibEpoch;
	TimeCalibInfo cc;
	volatile LONG ccEpoch;

	LONGLONG curCounter;	/* Current performance counter. */

	/*
	 * Try to acquire data without lock (same epoch at end of copy process).
	 */
	do {
	    ccEpoch = calibEpoch;
	    memcpy(&cc, &commonCC, sizeof(cc));
	    /*
	     * Hold time section locked as short as possible
	     */
	    if (InterlockedCompareExchange(&timeInfo.calibEpoch, 
				calibEpoch, calibEpoch) != calibEpoch) {
		EnterCriticalSection(&timeInfo.cs);
		if (calibEpoch != timeInfo.calibEpoch) {
		    memcpy(&commonCC, &timeInfo.lastCC, sizeof(commonCC));
		}
		calibEpoch = timeInfo.calibEpoch;
		LeaveCriticalSection(&timeInfo.cs);
	    }

	} while (InterlockedCompareExchange(&timeInfo.calibEpoch, 
			ccEpoch, ccEpoch) != ccEpoch);

	/*
	 * Query the performance counter and use it to calculate the current
	 * time.
	 */
	curCounter = NativePerformanceCounter();

	/* Calibrated file-time is saved from posix in 100-ns ticks */
	curTime = NativeCalc100NsTicks(cc.virtTime,
		cc.perfCounter, cc.cntrVariance, curCounter);

	/* Be sure the clock ticks never backwards (avoid backwards time-drifts) */
	if ( (lastTime = timeInfo.lastUsedTime)
	  && lastTime > curTime
	  && lastTime - curTime < 1000000 /* bypass time-switch (drifts only) */
	) {
	    curTime = timeInfo.lastUsedTime;
	}

	/*
	 * If it appears to be more than 1 seconds since the last trip
	 * through the calibration loop, the performance counter may have
	 * jumped forward. (See MSDN Knowledge Base article Q274323 for a
	 * description of the hardware problem that makes this test
	 * necessary.) If the counter jumps, we don't want to use it directly.
	 * Instead, we must return system time. Eventually, the calibration
	 * loop should recover.
	 */

	if (curTime < cc.calibNextTime + 10000000 /* 1 sec (in 100-ns ticks). */) {
	    /* save last used time */
	    timeInfo.lastUsedTime = curTime;
	    return curTime / 10;
	}
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!calibration-error!!!! cur: %I64d - call: %I64d (%I64d) -- prev: %I64d - now: %I64d (%I64d)\n", curTime, cc.calibNextTime, cc.calibNextTime - curTime, cc.perfCounter, curCounter, curCounter - cc.perfCounter);
    }

    /*
     * High resolution timer is not available.
     */
    curTime = GetSystemTimeAsVirtual(); /* in 100-ns ticks */
    /* Be sure the clock ticks never backwards (avoid backwards time-drifts) */
    if ( (lastTime = timeInfo.lastUsedTime)
      && lastTime > curTime
      && lastTime - curTime < 1000000 /* bypass time-switch (drifts only) */
    ) {
	curTime = timeInfo.lastUsedTime;
    }
    timeInfo.lastUsedTime = curTime;
    return curTime / 10;
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
    Tcl_WideInt usecSincePosixEpoch;

    /*
     * Try to use high resolution timer.
     */
    if ( (usecSincePosixEpoch = NativeGetMicroseconds()) ) {
	timePtr->sec = (long) (usecSincePosixEpoch / 1000000);
	timePtr->usec = (unsigned long) (usecSincePosixEpoch % 1000000);
    } else {
	/*
	* High resolution timer is not available. Just use ftime.
	*/

	struct _timeb t;

	_ftime(&t);
	timePtr->sec = (long)t.time;
	timePtr->usec = t.millitm * 1000;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StopCalibration --
 *
 *	Turns off the calibration thread in preparation for exiting the
 *	process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the 'exitEvent' event in the 'timeInfo' structure to ask the
 *	thread in question to exit, and waits for it to do so.
 *
 *----------------------------------------------------------------------
 */

void TclWinResetTimerResolution(void);

static void
StopCalibration(
    ClientData unused)		/* Client data is unused */
{
    SetEvent(timeInfo.exitEvent);

    /*
     * If Tcl_Finalize was called from DllMain, the calibration thread is in a
     * paused state so we need to timeout and continue.
     */

    WaitForSingleObject(timeInfo.calibrationThread, 100);
    CloseHandle(timeInfo.exitEvent);
    CloseHandle(timeInfo.calibrationThread);

    /*
     * Reset timer resolution (shutdown case)
     */
    (void)TclWinResetTimerResolution();
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
 * CalibrationThread --
 *
 *	Thread that manages calibration of the hi-resolution time derived from
 *	the performance counter, to keep it synchronized with the system
 *	clock.
 *
 * Parameters:
 *	arg - Client data from the CreateThread call. This parameter points to
 *	      the static TimeInfo structure.
 *
 * Return value:
 *	None. This thread embeds an infinite loop.
 *
 * Side effects:
 *	At an interval of 1s, this thread performs virtual time discipline.
 *
 * Note: When this thread is entered, TclpInitLock has been called to
 * safeguard the static storage. There is therefore no synchronization in the
 * body of this procedure.
 *
 *----------------------------------------------------------------------
 */

static DWORD WINAPI
CalibrationThread(
    LPVOID arg)
{
    DWORD waitResult;

    /*
     * Get initial system time and performance counter.
     */

    timeInfo.lastCC.perfCounter = NativePerformanceCounter();
    timeInfo.lastCC.fileTime = timeInfo.lastCC.virtTime = GetSystemTimeAsVirtual();

    /*
     * Calibrate first time and wake up the calling thread. 
     * When it wakes up, it will release the initialization lock.
     */

    if (timeInfo.perfCounterAvailable) {
	UpdateTimeEachSecond();
    }

    SetEvent(timeInfo.readyEvent);

    /*
     * Run the calibration once a second.
     */

    while (timeInfo.perfCounterAvailable) {
	/*
	 * If the exitEvent is set, break out of the loop.
	 */

	waitResult = WaitForSingleObjectEx(timeInfo.exitEvent, 1000, FALSE);
	if (waitResult == WAIT_OBJECT_0) {
	    break;
	}
	UpdateTimeEachSecond();

	/*
	* Reset timer resolution if expected (check waiter count once per second)
	*/
	(void)TclWinResetTimerResolution();
    }

    /* lint */
    return (DWORD) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateTimeEachSecond --
 *
 *	Callback from the waitable timer in the clock calibration thread that
 *	updates system time.
 *
 * Parameters:
 *	info - Pointer to the static TimeInfo structure
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Performs virtual time calibration discipline.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateTimeEachSecond(void)
{
    LONGLONG curPerfCounter;
				/* Current value returned from
				 * NativePerformanceCounter. */
    static int calibrationInterv = 10000000;
				/* Calibration interval in 100-ns ticks (starts from 1s) */
    Tcl_WideInt curFileTime;	/* File time at the time this callback was
				 * scheduled. */
    LONGLONG	estVariance,	/* Estimated variance to compensate ipmact of */
		driftVariance;	/* deviations of perfomance counters. */
    Tcl_WideInt vt0;		/* Tcl time right now. */
    Tcl_WideInt vt1;		/* Interim virtual time used during adjustments */
    Tcl_WideInt tdiff,		/* Difference between system clock and Tcl time. */
		lastDiff;	/* Difference of last calibration. */

    /*
     * Sample system time (from posix epoch) and performance counter.
     */

    curFileTime = GetSystemTimeAsVirtual();
    curPerfCounter = NativePerformanceCounter();
    printf("-------------calibration start, prev-struct: %I64d, %I64d, %I64d / %I64d, pc-diff: %I64d\n", timeInfo.lastCC.fileTime, timeInfo.lastCC.perfCounter, timeInfo.lastCC.cntrVariance, timeInfo.nominalFreq, curPerfCounter - timeInfo.lastCC.perfCounter);

    /* 
     * Current virtual time (using average between last fileTime and virtTime):
     * vt0 = (lastCC.fileTime + lastCC.virtTime) / 2
     *		+ ( (curPerfCounter - lastCC.perfCounter) * 10000000 
     *			+ lastCC.cntrVariance) / nominalFreq
     */
    vt0 = NativeCalc100NsTicks(
	    (timeInfo.lastCC.fileTime/2 + timeInfo.lastCC.virtTime/2),
	    timeInfo.lastCC.perfCounter, timeInfo.lastCC.cntrVariance,
	    curPerfCounter);

    /* Differences between virtual and real-time */
    tdiff = vt0 - curFileTime;
    lastDiff = timeInfo.lastCC.virtTime - timeInfo.lastCC.fileTime;
    if (tdiff >= 10000000 || tdiff <= -10000000) {
	printf("---!!!!!!!---calibration ERR, tdiff %I64d\n", tdiff);
    }
    /* 
     * If calibration still not needed (check for possible time-switch). Note, that
     * NativeGetMicroseconds checks calibNextTime also, be sure it does not overflow.
     * Calibrate immediately if we've too large discrepancy to the real-time (15.6 ms).
     */
#if 1
    if ( curFileTime < timeInfo.lastCC.calibNextTime - (10000000/2) /* 0.5 sec (in 100-ns ticks). */
      && timeInfo.lastCC.calibNextTime - curFileTime < 10 * 10000000 /* max. 10 seconds in-between (time-switch?) */
      && tdiff > -10000 && tdiff < 10000 /* very small discrepancy (1ms) */
    ) {
    	/* again in next one second */
    	printf("-------------calibration end, tdiff %I64d, *** not needed. (next in: %I64d) ------\n", tdiff, curFileTime, timeInfo.lastCC.calibNextTime, timeInfo.lastCC.calibNextTime - curFileTime);
	lastDiff = tdiff;
	return;
    }
#endif

    /*
     * Several things may have gone wrong here that have to be checked for.
     *  (1) The performance counter may have jumped.
     *  (2) The system clock may have been reset.
     */

    /*
     * We want to adjust things so that time appears to be continuous.
     * Virtual file time, right now, is vt0.
     *
     * Ideally, we would like to drift the clock into place over a period of 2
     * sec, so that virtual time 2 sec from now will be
     *
     * vt1 = 10000000 + curFileTime
     *
     * The frequency that we need to use to drift the counter back into place
     * is estFreq * 10000000 / (vt1 - vt0)
     *
     * If we've gotten more than a second away from system time, then drifting
     * the clock is going to be pretty hopeless. Just let it jump. Otherwise,
     * compute the drift frequency and fill in everything.
     */

    if (tdiff > 10000000 || tdiff < -10000000) {
    	/* More as a second difference, so could be a time-switch (reset)
    	/* jump to current system time, use curent estimated frequency */
    	vt0 = curFileTime;
    	timeInfo.lastUsedTime = 0; /* reset last used time */
    	estVariance = 0;
    } else {

	/*
	 * Estimate current frequency corresponding current time / counter.
	 */

	if ((curFileTime - timeInfo.lastCC.fileTime) > (10000000 / 2)) {
	    estVariance = (curFileTime - timeInfo.lastCC.fileTime) 
		* timeInfo.nominalFreq
		- (curPerfCounter- timeInfo.lastCC.perfCounter) * 10000000;
	} else {
	    estVariance = timeInfo.lastCC.cntrVariance;
	}

	/* 
	 * Minimize influence of estVariance if tdiff falls (in relation to
	 * last difference), with dual falling speed. This indicates better
	 * choice of lastCC.cntrVariance.
	 */
	if (tdiff > 0 && tdiff < lastDiff / 2 || tdiff < 0 && tdiff > lastDiff / 2) {
	    //printf("-----***-----calibration minimize %I64d, %I64d\n", estFreq, lastDiff);
	    estVariance = (estVariance + timeInfo.lastCC.cntrVariance) / 2;
	    //printf("-----***-----calibration minimize %I64d, %I64d\n", estFreq, tdiff);
	}
	printf("------**-----calibration estimated, tdiff: %I64d, ** %s ** cntrDiff:%I64d\n", tdiff, (estVariance > timeInfo.lastCC.cntrVariance) ? "^^^" : "vvv", (curPerfCounter - timeInfo.lastCC.perfCounter));
	printf("------**-----calibration estimated %I64d, %I64d, %I64d, diff: %I64d\n", curFileTime, curPerfCounter, estVariance, estVariance - timeInfo.lastCC.cntrVariance);

#if 1
    	/*
    	 * Calculate new estimate drift variance to the next second
    	 */

	vt1 = vt0 - timeInfo.lastCC.virtTime;
	if (vt1 > (10000000 / 2)) {
	    
	    LONGLONG driftPerfCounter = curPerfCounter + 
		    + (curPerfCounter - timeInfo.lastCC.perfCounter)
			/ vt1 * (vt1 + 10000000);

	    vt1 = NativeCalc100NsTicks(curFileTime,
		curPerfCounter, estVariance,
		driftPerfCounter);
	    driftVariance = (vt1 - vt0) * timeInfo.nominalFreq
		//- estVariance
		- (driftPerfCounter - curPerfCounter) * 10000000;
	} else {
	    driftVariance = estVariance * 2;
	}
	/* 
	 * Avoid too large drifts (only half of the current difference),
	 * that allows also be more accurate (aspire to the smallest tdiff),
	 * so then we can prolong calibration interval in such cases.
	 */
	driftVariance = timeInfo.lastCC.cntrVariance +
		(driftVariance - timeInfo.lastCC.cntrVariance) / 2;

	printf("------**-----calibration cntrVariance: %I64d\n", timeInfo.lastCC.cntrVariance);
	printf("------**-----calibration estVariance:  %I64d\n", estVariance);
	printf("------**-----calibration driftVariance:%I64d\n", driftVariance);

	/* 
	 * Average between estimated, 2 current and 5 drifted frequencies,
	 * (do the soft drifting as possible).
	 * Minimize influence if tdiff falls (in relation to last difference)
	 */
#if 0
	if (tdiff > 0 && tdiff < lastDiff / 2 || tdiff < 0 && tdiff > lastDiff / 2) {
	    estFreq = (1 * estFreq + 2 * timeInfo.lastCC.counterFreq + 5 * driftFreq) / 8;
	} else {
	    estFreq = (3 * estFreq + 3 * timeInfo.lastCC.counterFreq + 2 * driftFreq) / 8;
	}
#else
	estVariance = (estVariance + timeInfo.lastCC.cntrVariance + driftVariance) / 3;
#endif

#else
	estVariance = (estVariance + timeInfo.lastCC.cntrVariance) / 2;
#endif
    }
    
    /* If possible backwards time-drifts (larger divider now) */
    vt1 = 0;
    if (estVariance < timeInfo.lastCC.cntrVariance) {
	Tcl_WideInt nt0, nt1;

	/* 
	 * Calculate the time using new calibration values (and compare with old),
	 * to avoid possible backwards drifts (adjust current base time).
	 * This should affect at least next 10 ticks.
	 */
	vt1 = curPerfCounter + 10;
	/*
	 * Be sure the clock ticks never backwards (avoid it by negative drifting)
	 * just compare native time (in 100-ns) before and hereafter using 
	 * previous/new calibrated values) and do a small adjustment
	 */
	nt0 = NativeCalc100NsTicks(timeInfo.lastCC.virtTime,
		timeInfo.lastCC.perfCounter, timeInfo.lastCC.cntrVariance,
		vt1);
	nt1 = NativeCalc100NsTicks(vt0,
		curPerfCounter, estVariance,
		vt1);
	vt1 = (nt0 - nt1); /* old time - new time */
	if (vt1 > 0 && vt1 < 10000000 /* bypass time-switch */) {
	    /* base time should jump forwards (the same virtual time using current values) */
	    vt0 += vt1;
	    tdiff += vt1;
	    //////////////////////////////////////////estFreq = 10000000 * (vt0 - timeInfo.lastCC.perfCounter) / vt1;
	}
    }

    /* if still precise enough, grow calibration interval up to 10 seconds */
    if (tdiff < -100000 || tdiff > 100000 /* 10-ms */) {
	/* too long drift - reset calibration interval to 1 second */
	calibrationInterv = 10000000;
    } else if (calibrationInterv < 10*10000000) {
	calibrationInterv += 10000000;
    }

    /* In lock commit new values to timeInfo (hold lock as short as possible) */
    EnterCriticalSection(&timeInfo.cs);

    timeInfo.lastCC.perfCounter = curPerfCounter;
    timeInfo.lastCC.fileTime = curFileTime;
    timeInfo.lastCC.virtTime = vt0;
    timeInfo.lastCC.cntrVariance = estVariance;
    timeInfo.lastCC.calibNextTime = curFileTime + calibrationInterv;

    InterlockedIncrement(&timeInfo.calibEpoch);

    LeaveCriticalSection(&timeInfo.cs);
#if 1
    //printf("-------------calibration adj -- nt1:%I64d - nt0:%I64d: adj: %I64d\n", nt1, nt0, vt1);
    printf("-------------calibration end, tdiff %I64d, jump -- vt:%I64d - st:%I64d: %I64d, adj: %I64d\n", tdiff, 
    	vt0, curFileTime, (vt0 - curFileTime), vt1);
    printf("-------------calibration end  ,  new-struct: %I64d, %I64d, %I64d / %I64d\n", timeInfo.lastCC.virtTime, timeInfo.lastCC.perfCounter, timeInfo.lastCC.cntrVariance, timeInfo.nominalFreq);
#endif
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
