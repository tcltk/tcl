/* 
 * tclClock.c --
 *
 *	Contains the time and date related commands.  This code
 *	is derived from the time and date facilities of TclX,
 *	by Mark Diekhans and Karl Lehenbauer.
 *
 * Copyright 1991-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 2004 by Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclClock.c,v 1.29 2004/08/18 19:58:59 kennykb Exp $
 */

#include "tclInt.h"

/*
 * Windows has mktime.  The configurators do not check.
 */

#ifdef WIN32
#define HAVE_MKTIME
#endif

/*
 * Thread specific data block holding a 'struct tm' for the 'gmtime'
 * and 'localtime' library calls.
 */

static Tcl_ThreadDataKey tmKey;

/*
 * Mutex protecting 'gmtime', 'localtime' and 'mktime' calls
 * and the statics in the date parsing code.
 */

TCL_DECLARE_MUTEX(clockMutex)

/*
 * Function prototypes for local procedures in this file:
 */

static struct tm* ThreadSafeLocalTime _ANSI_ARGS_(( CONST time_t* ));
static void TzsetIfNecessary _ANSI_ARGS_(( void ));

/*
 *-------------------------------------------------------------------------
 *
 * TclClockLocaltimeObjCmd --
 *
 *	Tcl command that extracts local time using the C library to do
 *	it.
 *
 * Usage:
 *	::tcl::clock::Localtime <tick>
 *
 * Parameters:
 *	<tick> -- A count of seconds from the Posix epoch.
 *
 * Results:
 *	Returns a standard Tcl result.  The object result is a Tcl
 *	list containing the year, month, day, hour, minute, and second
 *	fields of the local time.  It may return an error if the
 *	argument exceeds the arithmetic range representable by
 *	'time_t'.
 *
 * Side effects:
 *	None.
 *
 * This function is used as a call of last resort if the current time
 * zone cannot be determined from environment variables TZ or TCL_TZ.
 * It attempts to use the 'localtime' library function to extract the
 * time and return it that way.  This method suffers from Y2038 problems
 * on most platforms.  It also provides no portable way to get the
 * name of the time zone.
 *
 *-------------------------------------------------------------------------
 */

int
TclClockLocaltimeObjCmd( ClientData clientData,
				/* Unused */
			 Tcl_Interp* interp,
				/* Tcl interpreter */
			 int objc,
				/* Parameter count */
			 Tcl_Obj* CONST* objv )
				/* Parameter vector */
{
    Tcl_WideInt tick;		/* Time to convert */
    time_t tock;
    struct tm* timeVal;		/* Time after conversion */

    Tcl_Obj* returnVec[ 6 ];

    /* Check args */

    if ( objc != 2 ) {
	Tcl_WrongNumArgs( interp, 1, objv, "seconds" );
	return TCL_ERROR;
    }
    if ( Tcl_GetWideIntFromObj( interp, objv[1], &tick ) != TCL_OK ) {
	return TCL_ERROR;
    }

    /* Convert the time, checking for overflow */

    tock = (time_t) tick;
    if ( (Tcl_WideInt) tock != tick ) {
	Tcl_SetObjResult
	    ( interp, 
	      Tcl_NewStringObj("number too large to represent as a Posix time", 
			       -1) );
	Tcl_SetErrorCode( interp, "CLOCK", "argTooLarge", (char*) NULL );
	return TCL_ERROR;
    }
    TzsetIfNecessary();
    timeVal = ThreadSafeLocalTime( &tock );

    /* Package the results */

    returnVec[0] = Tcl_NewIntObj( timeVal->tm_year + 1900 );
    returnVec[1] = Tcl_NewIntObj( timeVal->tm_mon + 1);
    returnVec[2] = Tcl_NewIntObj( timeVal->tm_mday );
    returnVec[3] = Tcl_NewIntObj( timeVal->tm_hour );
    returnVec[4] = Tcl_NewIntObj( timeVal->tm_min );
    returnVec[5] = Tcl_NewIntObj( timeVal->tm_sec );
    Tcl_SetObjResult( interp, Tcl_NewListObj( 6, returnVec ) );
    return TCL_OK;

}

/*
 *----------------------------------------------------------------------
 *
 * ThreadSafeLocalTime --
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

static struct tm *
ThreadSafeLocalTime(timePtr)
    CONST time_t *timePtr;	/* Pointer to the number of seconds
				 * since the local system's epoch
				 */

{
    /*
     * Get a thread-local buffer to hold the returned time.
     */

    struct tm *tmPtr = (struct tm *)
	    Tcl_GetThreadData(&tmKey, (int) sizeof(struct tm));
#ifdef HAVE_LOCALTIME_R
    localtime_r(timePtr, tmPtr);
#else
    Tcl_MutexLock(&clockMutex);
    memcpy((VOID *) tmPtr, (VOID *) localtime(timePtr), sizeof(struct tm));
    Tcl_MutexUnlock(&clockMutex);
#endif    
    return tmPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclClockMktimeObjCmd --
 *
 *	Determine seconds from the epoch, given the fields of a local
 *	time.
 *
 * Usage:
 *	mktime <year> <month> <day> <hour> <minute> <second>
 *
 * Parameters:
 *	year -- Calendar year
 *	month -- Calendar month
 *	day -- Calendar day
 *	hour -- Hour of day (00-23)
 *	minute -- Minute of hour
 *	second -- Second of minute
 *
 * Results:
 *	Returns the given local time.
 *
 * Errors:
 *	Returns an error if the 'mktime' function does not exist in the
 *	C library, or if the given time cannot be converted.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclClockMktimeObjCmd(  ClientData clientData,
		       			/* Unused */
		       Tcl_Interp* interp,
		       			/* Tcl interpreter */
		       int objc,
		       			/* Parameter count */
		       Tcl_Obj* CONST* objv )
     					/* Parameter vector */
{
    int i;
    struct tm toConvert;	/* Time to be converted */
    time_t convertedTime;	/* Time converted from mktime */

#ifndef HAVE_MKTIME
    Tcl_SetObjResult( interp,
		      Tcl_NewStringObj( "cannot determine local time", -1 ) );
    return TCL_ERROR;
#else

    /* Convert parameters */

    if ( objc != 7 ) {
	Tcl_WrongNumArgs( interp, 1, objv,
			  "year month day hour minute second" );
	return TCL_ERROR;
    }
    if ( Tcl_GetIntFromObj( interp, objv[1], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_year = i - 1900;
    if ( Tcl_GetIntFromObj( interp, objv[2], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_mon = i;
    if ( Tcl_GetIntFromObj( interp, objv[3], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_mday = i;
    if ( Tcl_GetIntFromObj( interp, objv[4], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_hour = i;
    if ( Tcl_GetIntFromObj( interp, objv[5], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_min = i;
    if ( Tcl_GetIntFromObj( interp, objv[6], &i ) != TCL_OK ) {
	return TCL_ERROR;
    }
    toConvert.tm_sec = i;
    toConvert.tm_isdst = -1;
    toConvert.tm_wday = 0;
    toConvert.tm_yday = 0;

    /* Convert the time.  It is rumored that mktime is not thread
     * safe on some platforms. */

    TzsetIfNecessary();
    Tcl_MutexLock( &clockMutex );
    convertedTime = mktime( &toConvert );
    Tcl_MutexUnlock( &clockMutex );

    /* Return the converted time, or an error if conversion fails */

    if ( convertedTime == -1 ) {
	Tcl_SetObjResult
	    ( interp,
	      Tcl_NewStringObj( "time value too large/small to represent", 
				-1 ) );
	return TCL_ERROR;
    } else {
	Tcl_SetObjResult( interp,
			  Tcl_NewWideIntObj( (Tcl_WideInt) convertedTime ) );
	return TCL_OK;
    }

#endif

}


/*----------------------------------------------------------------------
 *
 * TclClockClicksObjCmd --
 *
 *	Returns a high-resolution counter.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock clicks' Tcl command.  Refer
 * to the user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
TclClockClicksObjCmd( clientData, interp, objc, objv )
    ClientData clientData;	/* Client data is unused */
    Tcl_Interp* interp;		/* Tcl interpreter */
    int objc;			/* Parameter count */
    Tcl_Obj* CONST* objv;	/* Parameter values */
{
    static CONST char *clicksSwitches[] = {
	"-milliseconds", "-microseconds", (char*) NULL
    };
    enum ClicksSwitch {
	CLICKS_MILLIS,   CLICKS_MICROS,   CLICKS_NATIVE
    };
    int index = CLICKS_NATIVE;
    Tcl_Time now;

    switch ( objc ) {
        case 1:
	    break;
        case 2:
	    if ( Tcl_GetIndexFromObj( interp, objv[1], clicksSwitches,
				      "option", 0, &index) != TCL_OK ) {
		return TCL_ERROR;
	    }
	    break;
        default:
	    Tcl_WrongNumArgs( interp, 1, objv, "?option?" );
	    return TCL_ERROR;
    }

    switch ( index ) {
        case CLICKS_MILLIS:
	    Tcl_GetTime( &now );
	    Tcl_SetObjResult( interp,
			      Tcl_NewWideIntObj( (Tcl_WideInt) now.sec * 1000
						 + now.usec / 1000 ) );
	    break;
        case CLICKS_NATIVE:
#if 0
	    /* 
	     * The following code will be used once this is incorporated
	     * into Tcl.  But TEA bugs prevent it for right now. :(
	     * So we fall through this case and return the microseconds
	     * instead.
	     */
	    Tcl_SetObjResult( interp,
			      Tcl_NewWideIntObj( (Tcl_WideInt) TclpGetClicks() ) );
	    break;
#endif
        case CLICKS_MICROS:
	    Tcl_GetTime( &now );
	    Tcl_SetObjResult( interp,
			      Tcl_NewWideIntObj( ( (Tcl_WideInt) now.sec 
						   * 1000000 )
						 + now.usec ) );
	    break;
    }

    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * TclClockMillisecondsObjCmd -
 *
 *	Returns a count of milliseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock milliseconds' Tcl command.  Refer
 * to the user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
TclClockMillisecondsObjCmd( clientData, interp, objc, objv )
    ClientData clientData;	/* Client data is unused */
    Tcl_Interp* interp;		/* Tcl interpreter */
    int objc;			/* Parameter count */
    Tcl_Obj* CONST* objv;	/* Parameter values */
{
    Tcl_Time now;
    if ( objc != 1 ) {
	Tcl_WrongNumArgs( interp, 1, objv, "" );
	return TCL_ERROR;
    }
    Tcl_GetTime( &now );
    Tcl_SetObjResult( interp,
		      Tcl_NewWideIntObj( (Tcl_WideInt) now.sec * 1000
					 + now.usec / 1000 ) );
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * TclClockMicrosecondsObjCmd -
 *
 *	Returns a count of microseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock microseconds' Tcl command.  Refer
 * to the user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
TclClockMicrosecondsObjCmd( clientData, interp, objc, objv )
    ClientData clientData;	/* Client data is unused */
    Tcl_Interp* interp;		/* Tcl interpreter */
    int objc;			/* Parameter count */
    Tcl_Obj* CONST* objv;	/* Parameter values */
{
    Tcl_Time now;
    if ( objc != 1 ) {
	Tcl_WrongNumArgs( interp, 1, objv, "" );
	return TCL_ERROR;
    }
    Tcl_GetTime( &now );
    Tcl_SetObjResult( interp,
		      Tcl_NewWideIntObj( ( (Tcl_WideInt) now.sec * 1000000 )
					 + now.usec ) );
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * TclClockSecondsObjCmd -
 *
 *	Returns a count of microseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock seconds' Tcl command.  Refer
 * to the user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
TclClockSecondsObjCmd( clientData, interp, objc, objv )
    ClientData clientData;	/* Client data is unused */
    Tcl_Interp* interp;		/* Tcl interpreter */
    int objc;			/* Parameter count */
    Tcl_Obj* CONST* objv;	/* Parameter values */
{
    Tcl_Time now;
    if ( objc != 1 ) {
	Tcl_WrongNumArgs( interp, 1, objv, "" );
	return TCL_ERROR;
    }
    Tcl_GetTime( &now );
    Tcl_SetObjResult( interp, Tcl_NewWideIntObj( (Tcl_WideInt) now.sec ) );
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TzsetIfNecessary --
 *
 *	Calls the tzset() library function if the contents of the TZ
 *	environment variable has changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Calls tzset.
 *
 *----------------------------------------------------------------------
 */

static void
TzsetIfNecessary()
{
    static char* tzWas = NULL;	/* Previous value of TZ, protected by
				 * clockMutex. */
    CONST char* tzIsNow;	/* Current value of TZ */

    Tcl_MutexLock( &clockMutex );
    tzIsNow = getenv( "TZ" );
    if ( tzIsNow != NULL
	 && ( tzWas == NULL || strcmp( tzIsNow, tzWas ) != 0 ) ) {
	tzset();
	if ( tzWas != NULL ) {
	    ckfree( tzWas );
	}
	tzWas = ckalloc( strlen( tzIsNow ) + 1 );
	strcpy( tzWas, tzIsNow );
    } else if ( tzIsNow == NULL && tzWas != NULL ) {
	tzset();
	ckfree( tzWas );
	tzWas = NULL;
    }
    Tcl_MutexUnlock( &clockMutex );
}

/*
 *-------------------------------------------------------------------------
 *
 * TclClockOldscanObjCmd --
 *
 *	Implements the legacy 'clock scan' Tcl command when no '-format'
 *	option is supplied.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * This function implements the 'clock scan' Tcl command when no
 * -format group is present. Refer to the user documentation to see
 * what it does.
 *
 *-------------------------------------------------------------------------
 */

int
TclClockOldscanObjCmd( ClientData clientData, /* unused */
		       Tcl_Interp* interp, /* Tcl interpreter */
		       int objc, /* Parameter count */
		       Tcl_Obj *CONST * objv /* Parameter vector */
		       )
{
    int index;
    Tcl_Obj *CONST *objPtr;
    char *scanStr;
    Tcl_Obj *baseObjPtr = NULL;
    int useGMT = 0;
    unsigned long baseClock;
    long clockVal;
    long zone;
    Tcl_Obj *resultPtr;
    int dummy;

    static CONST char *scanSwitches[] = {
	"-base", "-gmt", (char *) NULL
    };

    if ((objc < 2) || (objc > 6)) {
    wrongScanArgs:
	Tcl_WrongNumArgs(interp, 2, objv,
			 "dateString ?-base clockValue? ?-gmt boolean?");
	return TCL_ERROR;
    }
    objPtr = objv+2;
    objc -= 2;
    while (objc > 1) {
	if (Tcl_GetIndexFromObj(interp, objPtr[0], scanSwitches,
				"switch", 0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case 0:		/* -base */
	    baseObjPtr = objPtr[1];
	    break;
	case 1:		/* -gmt */
	    if (Tcl_GetBooleanFromObj(interp, objPtr[1],
				      &useGMT) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	}
	objPtr += 2;
	objc -= 2;
    }
    if (objc != 0) {
	goto wrongScanArgs;
    }
    
    if (baseObjPtr != NULL) {
	if (Tcl_GetLongFromObj(interp, baseObjPtr,
			       (long*) &baseClock) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	baseClock = TclpGetSeconds();
    }
    
    if (useGMT) {
	zone = -50000; /* Force GMT */
    } else {
	zone = TclpGetTimeZone((unsigned long) baseClock);
    }
    
    scanStr = Tcl_GetStringFromObj(objv[1], &dummy);
    Tcl_MutexLock(&clockMutex);
    if (TclGetDate(scanStr, (unsigned long) baseClock, zone,
		   &clockVal) < 0) {
	Tcl_MutexUnlock(&clockMutex);
	resultPtr = Tcl_NewObj();
	Tcl_AppendStringsToObj(resultPtr,
			       "unable to convert date-time string \"",
			       scanStr, "\"", (char *) NULL);
	Tcl_SetObjResult( interp, resultPtr );
	return TCL_ERROR;
    }
    Tcl_MutexUnlock(&clockMutex);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj( (Tcl_WideInt) clockVal ) );
    return TCL_OK;

}
