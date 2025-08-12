/*
 * tclWinTime.c --
 *
 *	Contains Windows specific versions of Tcl functions that obtain time
 *	values from the operating system.
 *
 * Copyright © 1995-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * Number of samples over which to estimate the performance counter.
 */

#define SAMPLES		64

/*
 * Data for managing high-resolution timers.
 */

typedef struct {
    CRITICAL_SECTION cs;	/* Mutex guarding this structure. */
    bool initialized;		/* True if this structure is initialized. */
    bool perfCounterAvailable;	/* True if the hardware has a performance
				 * counter. */
    DWORD calibrationInterv;	/* Calibration interval in seconds (start 1
				 * sec) */
    HANDLE calibrationThread;	/* Handle to the thread that keeps the virtual
				 * clock calibrated. */
    HANDLE readyEvent;		/* System event used to trigger the requesting
				 * thread when the clock calibration procedure
				 * is initialized for the first time. */
    HANDLE exitEvent;		/* Event to signal out of an exit handler to
				 * tell the calibration loop to terminate. */
    LARGE_INTEGER nominalFreq;	/* Nominal frequency of the system performance
				 * counter, that is, the value returned from
				 * QueryPerformanceFrequency. */
    /*
     * The following values are used for calculating virtual time. Virtual
     * time is always equal to:
     *    lastFileTime + (current perf counter - lastCounter)
     *				* 10000000 / curCounterFreq
     * and lastFileTime and lastCounter are updated any time that virtual time
     * is returned to a caller.
     */

    ULARGE_INTEGER fileTimeLastCall;
    LARGE_INTEGER perfCounterLastCall;
    LARGE_INTEGER curCounterFreq;
    LARGE_INTEGER posixEpoch;	/* Posix epoch expressed as 100-ns ticks since
				 * the windows epoch. */

    /*
     * Data used in developing the estimate of performance counter frequency
     */

    unsigned long long fileTimeSample[SAMPLES];
				/* Last 64 samples of system time. */
    long long perfCounterSample[SAMPLES];
				/* Last 64 samples of performance counter. */
    int sampleNo;		/* Current sample number. */
} TimeInfo;

static TimeInfo timeInfo = {
    { NULL, 0, 0, NULL, NULL, 0 },
    false,
    false,
    1,
    (HANDLE) NULL,
    (HANDLE) NULL,
    (HANDLE) NULL,
#if defined(HAVE_CAST_TO_UNION) && !defined(__cplusplus)
    (LARGE_INTEGER) (long long) 0,
    (ULARGE_INTEGER) (DWORDLONG) 0,
    (LARGE_INTEGER) (long long) 0,
    (LARGE_INTEGER) (long long) 0,
    (LARGE_INTEGER) (long long) 0,
#else
    {{0, 0}},
    {{0, 0}},
    {{0, 0}},
    {{0, 0}},
    {{0, 0}},
#endif
    { 0 },
    { 0 },
    0
};

/*
 * Scale to convert wide click values from the TclpGetWideClicks native
 * resolution to microsecond resolution and back.
 */
static struct {
    bool initialized;		/* true if initialized, false otherwise */
    bool perfCounter;		/* true if performance counter usable for wide
				 * clicks */
    double microsecsScale;	/* Denominator scale between clock / microsecs */
} wideClick = {0, 0, 0.0};

/*
 * Declarations for functions defined later in this file.
 */

static void		StopCalibration(void *clientData);
static DWORD WINAPI	CalibrationThread(LPVOID arg);
static void		UpdateTimeEachSecond(void);
static void		ResetCounterSamples(unsigned long long fileTime,
			    long long perfCounter, long long perfFreq);
static long long	AccumulateSample(long long perfCounter,
			    unsigned long long fileTime);
static void		NativeScaleTime(Tcl_Time* timebuf,
			    void *clientData);
static long long	NativeGetMicroseconds(void);
static void		NativeGetTime(Tcl_Time* timebuf,
			    void *clientData);

/*
 * TIP #233 (Virtualized Time): Data for the time hooks, if any.
 */

Tcl_GetTimeProc *tclGetTimeProcPtr = NativeGetTime;
Tcl_ScaleTimeProc *tclScaleTimeProcPtr = NativeScaleTime;
void *tclTimeClientData = NULL;

/*
 * Inlined version of Tcl_GetTime.
 */

static inline void
GetTime(
    Tcl_Time *timePtr)
{
    tclGetTimeProcPtr(timePtr, tclTimeClientData);
}

static inline int
IsTimeNative(void)
{
    return tclGetTimeProcPtr == NativeGetTime;
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

unsigned long long
TclpGetSeconds(void)
{
    long long usecSincePosixEpoch;

    /*
     * Try to use high resolution timer
     */

    if (IsTimeNative() && (usecSincePosixEpoch = NativeGetMicroseconds())) {
	return usecSincePosixEpoch / 1000000;
    } else {
	Tcl_Time t;

	GetTime(&t);
	return (unsigned long long)t.sec;
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
    long long usecSincePosixEpoch;

    /*
     * Try to use high resolution timer.
     */

    if (IsTimeNative() && (usecSincePosixEpoch = NativeGetMicroseconds())) {
	return (Tcl_WideUInt) usecSincePosixEpoch;
    } else {
	/*
	* Use the Tcl_GetTime abstraction to get the time in microseconds, as
	* nearly as we can, and return it.
	*/

	Tcl_Time now;		/* Current Tcl time */

	GetTime(&now);
	return ((unsigned long long)(now.sec)*1000000ULL) +
		(unsigned long long)(now.usec);
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

long long
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
	    wideClick.perfCounter = true;
	    wideClick.microsecsScale = 1000000.0 / (double)perfCounterFreq.QuadPart;
	} else {
	    /* fallback using microseconds */
	    wideClick.perfCounter = false;
	    wideClick.microsecsScale = 1;
	}

	wideClick.initialized = true;
    }
    if (wideClick.perfCounter) {
	if (QueryPerformanceCounter(&curCounter)) {
	    return (long long)curCounter.QuadPart;
	}
	/* fallback using microseconds */
	wideClick.perfCounter = false;
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
    if (!wideClick.initialized) {
	(void) TclpGetWideClicks();	/* initialize */
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

long long
TclpGetMicroseconds(void)
{
    long long usecSincePosixEpoch;

    /*
     * Try to use high resolution timer.
     */

    if (IsTimeNative() && (usecSincePosixEpoch = NativeGetMicroseconds())) {
	return usecSincePosixEpoch;
    } else {
	/*
	 * Use the Tcl_GetTime abstraction to get the time in microseconds, as
	 * nearly as we can, and return it.
	 */

	Tcl_Time now;

	GetTime(&now);
	return now.sec * 1000000 + now.usec;
    }
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
    long long usecSincePosixEpoch;

    /*
     * Try to use high resolution timer.
     */

    if (IsTimeNative() && (usecSincePosixEpoch = NativeGetMicroseconds())) {
	timePtr->sec = usecSincePosixEpoch / 1000000;
	timePtr->usec = (long)(usecSincePosixEpoch % 1000000);
    } else {
	GetTime(timePtr);
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
    TCL_UNUSED(Tcl_Time *),
    TCL_UNUSED(void *))
{
    /*
     * Native scale is 1:1. Nothing is done.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * IsPerfCounterAvailable --
 *
 *	Tests whether the performance counter is available, which is a gnarly
 *	problem on 32-bit systems. Also retrieves the nominal frequency of the
 *	performance counter.
 *
 * Results:
 *	true if the counter is available, false if not.
 *
 * Side effects:
 *	Updates fields of the timeInfo global. Make sure you hold the lock
 *	before calling this.
 *
 *----------------------------------------------------------------------
 */

static inline bool
IsPerfCounterAvailable(void)
{
    timeInfo.perfCounterAvailable =
	    QueryPerformanceFrequency(&timeInfo.nominalFreq);

    /*
     * Some hardware abstraction layers use the CPU clock in place of the
     * real-time clock as a performance counter reference. This results in:
     *    - inconsistent results among the processors on multi-processor
     *      systems.
     *    - unpredictable changes in performance counter frequency on
     *      "gearshift" processors such as Transmeta and SpeedStep.
     *
     * There seems to be no way to test whether the performance counter is
     * reliable, but a useful heuristic is that if its frequency is 1.193182
     * MHz or 3.579545 MHz, it's derived from a colorburst crystal and is
     * therefore the RTC rather than the TSC.
     *
     * A sloppier but serviceable heuristic is that the RTC crystal is
     * normally less than 15 MHz while the TSC crystal is virtually assured to
     * be greater than 100 MHz. Since Win98SE appears to fiddle with the
     * definition of the perf counter frequency (perhaps in an attempt to
     * calibrate the clock?), we use the latter rule rather than an exact
     * match.
     *
     * We also assume (perhaps questionably) that the vendors have gotten
     * their act together on Win64, so bypass all this rubbish on that
     * platform.
     */

#if !defined(_WIN64)
    if (timeInfo.perfCounterAvailable &&
	    /*
	     * The following lines would do an exact match on crystal
	     * frequency:
	     *
	     *	timeInfo.nominalFreq.QuadPart != (long long) 1193182 &&
	     *	timeInfo.nominalFreq.QuadPart != (long long) 3579545 &&
	     */
	    timeInfo.nominalFreq.QuadPart > (long long) 15000000) {
	/*
	 * As an exception, if every logical processor on the system is on the
	 * same chip, we use the performance counter anyway, presuming that
	 * everyone's TSC is locked to the same oscillator.
	 */

	SYSTEM_INFO systemInfo;
	int regs[4];

	GetSystemInfo(&systemInfo);
	if (TclWinCPUID(0, regs) == TCL_OK
		&& regs[1] == 0x756E6547	/* "Genu" */
		&& regs[3] == 0x49656E69	/* "ineI" */
		&& regs[2] == 0x6C65746E	/* "ntel" */
		&& TclWinCPUID(1, regs) == TCL_OK
		&& ((regs[0]&0x00000F00) == 0x00000F00 /* Pentium 4 */
		|| ((regs[0] & 0x00F00000)	/* Extended family */
		&& (regs[3] & 0x10000000)))	/* Hyperthread */
		&& (((regs[1]&0x00FF0000) >> 16)/* CPU count */
			== (int)systemInfo.dwNumberOfProcessors)) {
	    timeInfo.perfCounterAvailable = true;
	} else {
	    timeInfo.perfCounterAvailable = false;
	}
    }
#endif /* above code is Win32 only */

    return timeInfo.perfCounterAvailable;
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

static inline long long
NativeCalc100NsTicks(
    ULONGLONG fileTimeLastCall,
    LONGLONG perfCounterLastCall,
    LONGLONG curCounterFreq,
    LONGLONG curCounter)
{
    return fileTimeLastCall +
	    ((curCounter - perfCounterLastCall) * 10000000 / curCounterFreq);
}

static long long
NativeGetMicroseconds(void)
{
    /*
     * Initialize static storage on the first trip through.
     *
     * Note: Outer check for 'initialized' is a performance win since it
     * avoids an extra mutex lock in the common case.
     */

    if (!timeInfo.initialized) {
	TclpInitLock();
	if (!timeInfo.initialized) {
	    timeInfo.posixEpoch.LowPart = 0xD53E8000;
	    timeInfo.posixEpoch.HighPart = 0x019DB1DE;

	    /*
	     * If the performance counter is available, start a thread to
	     * calibrate it.
	     */

	    if (IsPerfCounterAvailable()) {
		DWORD id;

		InitializeCriticalSection(&timeInfo.cs);
		timeInfo.readyEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
		timeInfo.exitEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
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
		Tcl_CreateExitHandler(StopCalibration, NULL);
	    }
	    timeInfo.initialized = TRUE;
	}
	TclpInitUnlock();
    }

    if (timeInfo.perfCounterAvailable && timeInfo.curCounterFreq.QuadPart!=0) {
	/*
	 * Query the performance counter and use it to calculate the current
	 * time.
	 */

	ULONGLONG fileTimeLastCall;
	LONGLONG perfCounterLastCall, curCounterFreq;
				/* Copy with current data of calibration
				 * cycle. */
	LARGE_INTEGER curCounter;
				/* Current performance counter. */

	QueryPerformanceCounter(&curCounter);

	/*
	 * Hold time section locked as short as possible
	 */

	EnterCriticalSection(&timeInfo.cs);

	fileTimeLastCall = timeInfo.fileTimeLastCall.QuadPart;
	perfCounterLastCall = timeInfo.perfCounterLastCall.QuadPart;
	curCounterFreq = timeInfo.curCounterFreq.QuadPart;

	LeaveCriticalSection(&timeInfo.cs);

	/*
	 * If calibration cycle occurred after we get curCounter
	 */

	if (curCounter.QuadPart <= perfCounterLastCall) {
	    /*
	     * Calibrated file-time is saved from Posix in 100-ns ticks
	     */

	    return fileTimeLastCall / 10;
	}

	/*
	 * If it appears to be more than 1.1 seconds since the last trip
	 * through the calibration loop, the performance counter may have
	 * jumped forward. (See MSDN Knowledge Base article Q274323 for a
	 * description of the hardware problem that makes this test
	 * necessary.) If the counter jumps, we don't want to use it directly.
	 * Instead, we must return system time. Eventually, the calibration
	 * loop should recover.
	 */

	if (curCounter.QuadPart - perfCounterLastCall <
		11 * curCounterFreq * timeInfo.calibrationInterv / 10) {
	    /*
	     * Calibrated file-time is saved from Posix in 100-ns ticks.
	     */

	    return NativeCalc100NsTicks(fileTimeLastCall,
		    perfCounterLastCall, curCounterFreq,
		    curCounter.QuadPart) / 10;
	}
    }

    /*
     * High resolution timer is not available.
     */

    return 0;
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
    TCL_UNUSED(void *))
{
    long long usecSincePosixEpoch;

    /*
     * Try to use high resolution timer.
     */

    usecSincePosixEpoch = NativeGetMicroseconds();
    if (usecSincePosixEpoch) {
	timePtr->sec = usecSincePosixEpoch / 1000000;
	timePtr->usec = (long)(usecSincePosixEpoch % 1000000);
    } else {
	/*
	 * High resolution timer is not available. Just use ftime.
	 */

	struct _timeb t;

	_ftime(&t);
	timePtr->sec = t.time;
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
    TCL_UNUSED(void *))
{
    SetEvent(timeInfo.exitEvent);

    /*
     * If Tcl_Finalize was called from DllMain, the calibration thread is in a
     * paused state so we need to timeout and continue.
     */

    WaitForSingleObject(timeInfo.calibrationThread, 100);
    CloseHandle(timeInfo.exitEvent);
    CloseHandle(timeInfo.calibrationThread);
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
    TCL_UNUSED(LPVOID))
{
    FILETIME curFileTime;
    DWORD waitResult;

    /*
     * Get initial system time and performance counter.
     */

    GetSystemTimeAsFileTime(&curFileTime);
    QueryPerformanceCounter(&timeInfo.perfCounterLastCall);
    QueryPerformanceFrequency(&timeInfo.curCounterFreq);
    timeInfo.fileTimeLastCall.LowPart = curFileTime.dwLowDateTime;
    timeInfo.fileTimeLastCall.HighPart = curFileTime.dwHighDateTime;

    /*
     * Calibrated file-time will be saved from Posix in 100-ns ticks.
     */

    timeInfo.fileTimeLastCall.QuadPart -= timeInfo.posixEpoch.QuadPart;

    ResetCounterSamples(timeInfo.fileTimeLastCall.QuadPart,
	    timeInfo.perfCounterLastCall.QuadPart,
	    timeInfo.curCounterFreq.QuadPart);

    /*
     * Wake up the calling thread. When it wakes up, it will release the
     * initialization lock.
     */

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
    }

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
    LARGE_INTEGER curPerfCounter;
				/* Current value returned from
				 * QueryPerformanceCounter. */
    FILETIME curSysTime;	/* Current system time. */
    static LARGE_INTEGER lastFileTime;
				/* File time of the previous calibration */
    LARGE_INTEGER curFileTime;	/* File time at the time this callback was
				 * scheduled. */
    long long estFreq;		/* Estimated perf counter frequency. */
    long long vt0;		/* Tcl time right now. */
    long long vt1;		/* Tcl time one second from now. */
    long long tdiff;		/* Difference between system clock and Tcl
				 * time. */
    long long driftFreq;	/* Frequency needed to drift virtual time into
				 * step over 1 second. */

    /*
     * Sample performance counter and system time (from Posix epoch).
     */

    GetSystemTimeAsFileTime(&curSysTime);
    curFileTime.LowPart = curSysTime.dwLowDateTime;
    curFileTime.HighPart = curSysTime.dwHighDateTime;
    curFileTime.QuadPart -= timeInfo.posixEpoch.QuadPart;

    /*
     * If calibration still not needed (check for possible time switch)
     */

    if (curFileTime.QuadPart > lastFileTime.QuadPart && curFileTime.QuadPart <
	    lastFileTime.QuadPart + (timeInfo.calibrationInterv * 10000000)) {
	/*
	 * Look again in next one second.
	 */

	return;
    }
    QueryPerformanceCounter(&curPerfCounter);

    lastFileTime.QuadPart = curFileTime.QuadPart;

    /*
     * We divide by timeInfo.curCounterFreq.QuadPart in several places. That
     * value should always be positive on a correctly functioning system. But
     * it is good to be defensive about such matters. So if something goes
     * wrong and the value does goes to zero, we clear the
     * timeInfo.perfCounterAvailable in order to cause the calibration thread
     * to shut itself down, then return without additional processing.
     */

    if (timeInfo.curCounterFreq.QuadPart == 0){
	timeInfo.perfCounterAvailable = false;
	return;
    }

    /*
     * Several things may have gone wrong here that have to be checked for.
     *  (1) The performance counter may have jumped.
     *  (2) The system clock may have been reset.
     *
     * In either case, we'll need to reinitialize the circular buffer with
     * samples relative to the current system time and the NOMINAL performance
     * frequency (not the actual, because the actual has probably run slow in
     * the first case). Our estimated frequency will be the nominal frequency.
     *
     * Store the current sample into the circular buffer of samples, and
     * estimate the performance counter frequency.
     */

    estFreq = AccumulateSample(curPerfCounter.QuadPart,
	    (unsigned long long) curFileTime.QuadPart);

    /*
     * We want to adjust things so that time appears to be continuous.
     * Virtual file time, right now, is
     *
     * vt0 = 10000000 * (curPerfCounter - perfCounterLastCall)
     *	     / curCounterFreq
     *	     + fileTimeLastCall
     *
     * Ideally, we would like to drift the clock into place over a period of 2
     * sec, so that virtual time 2 sec from now will be
     *
     * vt1 = 20000000 + curFileTime
     *
     * The frequency that we need to use to drift the counter back into place
     * is estFreq * 20000000 / (vt1 - vt0)
     */

    vt0 = NativeCalc100NsTicks(timeInfo.fileTimeLastCall.QuadPart,
	    timeInfo.perfCounterLastCall.QuadPart,
	    timeInfo.curCounterFreq.QuadPart, curPerfCounter.QuadPart);

    /*
     * If we've gotten more than a second away from system time, then drifting
     * the clock is going to be pretty hopeless. Just let it jump. Otherwise,
     * compute the drift frequency and fill in everything.
     */

    tdiff = vt0 - curFileTime.QuadPart;
    if (tdiff > 10000000 || tdiff < -10000000) {
	/*
	 * Jump to current system time, use curent estimated frequency.
	 */

	vt0 = curFileTime.QuadPart;
    } else {
	/*
	 * Calculate new frequency and estimate drift to the next second.
	 */

	vt1 = 20000000 + curFileTime.QuadPart;
	driftFreq = (estFreq * 20000000 / (vt1 - vt0));

	/*
	 * Avoid too large drifts (only half of the current difference), that
	 * allows also be more accurate (aspire to the smallest tdiff), so
	 * then we can prolong calibration interval by tdiff < 100000
	 */

	driftFreq = timeInfo.curCounterFreq.QuadPart +
		(driftFreq - timeInfo.curCounterFreq.QuadPart) / 2;

	/*
	 * Average between estimated, 2 current and 5 drifted frequencies,
	 * (do the soft drifting as possible)
	 */

	estFreq = (estFreq + 2 * timeInfo.curCounterFreq.QuadPart +
		5 * driftFreq) / 8;
    }

    /*
     * Avoid too large discrepancy from nominal frequency.
     */

    if (estFreq > 1003 * timeInfo.nominalFreq.QuadPart / 1000) {
	estFreq = 1003 * timeInfo.nominalFreq.QuadPart / 1000;
	vt0 = curFileTime.QuadPart;
    } else if (estFreq < 997 * timeInfo.nominalFreq.QuadPart / 1000) {
	estFreq = 997 * timeInfo.nominalFreq.QuadPart / 1000;
	vt0 = curFileTime.QuadPart;
    } else if (vt0 != curFileTime.QuadPart) {
	/*
	 * Be sure the clock ticks never backwards (avoid it by negative
	 * drifting). Just compare native time (in 100-ns) before and
	 * hereafter using new calibrated values) and do a small adjustment
	 * (short time freeze).
	 */

	LARGE_INTEGER newPerfCounter;
	long long nt0, nt1;

	QueryPerformanceCounter(&newPerfCounter);
	nt0 = NativeCalc100NsTicks(timeInfo.fileTimeLastCall.QuadPart,
		timeInfo.perfCounterLastCall.QuadPart,
		timeInfo.curCounterFreq.QuadPart, newPerfCounter.QuadPart);
	nt1 = NativeCalc100NsTicks(vt0,
		curPerfCounter.QuadPart, estFreq, newPerfCounter.QuadPart);
	if (nt0 > nt1) {
	    /*
	     * Drifted backwards, try to compensate with new base.
	     *
	     * First adjust with a micro jump (short frozen time is
	     * acceptable).
	     */

	    vt0 += nt0 - nt1;

	    /*
	     * If drift unavoidable (e. g. we had a time switch), then reset
	     * it.
	     */

	    vt1 = vt0 - curFileTime.QuadPart;
	    if (vt1 > 10000000 || vt1 < -10000000) {
		/*
		 * Larger jump resp. shift relative new file-time.
		 */

		vt0 = curFileTime.QuadPart;
	    }
	}
    }

    /*
     * In lock commit new values to timeInfo (hold lock as short as possible)
     */

    EnterCriticalSection(&timeInfo.cs);

    /*
     * Grow calibration interval up to 10 seconds (if still precise enough)
     */

    if (tdiff < -100000 || tdiff > 100000) {
	/*
	 * Too long drift. Reset calibration interval to 1000 second.
	 */

	timeInfo.calibrationInterv = 1;
    } else if (timeInfo.calibrationInterv < 10) {
	timeInfo.calibrationInterv++;
    }

    timeInfo.fileTimeLastCall.QuadPart = vt0;
    timeInfo.curCounterFreq.QuadPart = estFreq;
    timeInfo.perfCounterLastCall.QuadPart = curPerfCounter.QuadPart;

    LeaveCriticalSection(&timeInfo.cs);
}

/*
 *----------------------------------------------------------------------
 *
 * ResetCounterSamples --
 *
 *	Fills the sample arrays in 'timeInfo' with dummy values that will
 *	yield the current performance counter and frequency.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The array of samples is filled in so that it appears that there are
 *	SAMPLES samples at one-second intervals, separated by precisely the
 *	given frequency.
 *
 *----------------------------------------------------------------------
 */

static void
ResetCounterSamples(
    unsigned long long fileTime,/* Current file time */
    long long perfCounter,	/* Current performance counter */
    long long perfFreq)		/* Target performance frequency */
{
    for (int i = SAMPLES - 1 ; i >= 0 ; --i) {
	timeInfo.perfCounterSample[i] = perfCounter;
	timeInfo.fileTimeSample[i] = fileTime;
	perfCounter -= perfFreq;
	fileTime -= 10000000;
    }
    timeInfo.sampleNo = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * AccumulateSample --
 *
 *	Updates the circular buffer of performance counter and system time
 *	samples with a new data point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The new data point replaces the oldest point in the circular buffer,
 *	and the descriptive statistics are updated to accumulate the new
 *	point.
 *
 * Several things may have gone wrong here that have to be checked for.
 *  (1) The performance counter may have jumped.
 *  (2) The system clock may have been reset.
 *
 * In either case, we'll need to reinitialize the circular buffer with samples
 * relative to the current system time and the NOMINAL performance frequency
 * (not the actual, because the actual has probably run slow in the first
 * case).
 */

static long long
AccumulateSample(
    long long perfCounter,
    unsigned long long fileTime)
{
    unsigned long long workFTSample;
				/* File time sample being removed from or
				 * added to the circular buffer. */
    long long workPCSample;	/* Performance counter sample being removed
				 * from or added to the circular buffer. */
    unsigned long long lastFTSample;
				/* Last file time sample recorded */
    long long lastPCSample;	/* Last performance counter sample recorded */
    long long FTdiff;		/* Difference between last FT and current */
    long long PCdiff;		/* Difference between last PC and current */
    long long estFreq;		/* Estimated performance counter frequency */

    /*
     * Test for jumps and reset the samples if we have one.
     */

    if (timeInfo.sampleNo == 0) {
	lastPCSample =
		timeInfo.perfCounterSample[timeInfo.sampleNo + SAMPLES - 1];
	lastFTSample =
		timeInfo.fileTimeSample[timeInfo.sampleNo + SAMPLES - 1];
    } else {
	lastPCSample = timeInfo.perfCounterSample[timeInfo.sampleNo - 1];
	lastFTSample = timeInfo.fileTimeSample[timeInfo.sampleNo - 1];
    }

    PCdiff = perfCounter - lastPCSample;
    FTdiff = fileTime - lastFTSample;
    if (PCdiff < timeInfo.nominalFreq.QuadPart * 9 / 10
	    || PCdiff > timeInfo.nominalFreq.QuadPart * 11 / 10
	    || FTdiff < 9000000 || FTdiff > 11000000) {
	ResetCounterSamples(fileTime, perfCounter,
		timeInfo.nominalFreq.QuadPart);
	return timeInfo.nominalFreq.QuadPart;
    } else {
	/*
	 * Estimate the frequency.
	 */

	workPCSample = timeInfo.perfCounterSample[timeInfo.sampleNo];
	workFTSample = timeInfo.fileTimeSample[timeInfo.sampleNo];
	estFreq = 10000000 * (perfCounter - workPCSample)
		/ (fileTime - workFTSample);
	timeInfo.perfCounterSample[timeInfo.sampleNo] = perfCounter;
	timeInfo.fileTimeSample[timeInfo.sampleNo] = (long long) fileTime;

	/*
	 * Advance the sample number.
	 */

	if (++timeInfo.sampleNo >= SAMPLES) {
	    timeInfo.sampleNo = 0;
	}

	return estFreq;
    }
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
    void *clientData)
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
    void **clientData)
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
