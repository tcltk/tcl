/*
 * tclDate.h --
 *
 *	This header file handles common usage of clock primitives
 *	between tclDate.c (yacc) and tclClock.c.
 *
 * Copyright (c) 2014 Serg G. Brester (aka sebres)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLCLOCK_H
#define _TCLCLOCK_H

/*
 * Structure contains return parsed fields.
 */

typedef struct DateInfo {

    time_t dateYear;
    time_t dateMonth;
    time_t dateDay;
    int dateHaveDate;

    time_t dateHour;
    time_t dateMinutes;
    time_t dateSeconds;
    int dateMeridian;
    int dateHaveTime;

    time_t dateTimezone;
    int dateDSTmode;
    int dateHaveZone;

    time_t dateRelMonth;
    time_t dateRelDay;
    time_t dateRelSeconds;
    int dateHaveRel;

    time_t dateMonthOrdinal;
    int dateHaveOrdinalMonth;

    time_t dateDayOrdinal;
    time_t dateDayNumber;
    int dateHaveDay;

    const char *dateStart;
    const char *dateInput;
    time_t *dateRelPointer;

    int dateDigitCount;

    Tcl_Obj* messages;	    /* Error messages */
    const char* separatrix; /* String separating messages */
} DateInfo;


enum {CL_INVALIDATE = (signed int)0x80000000};
/*
 * Structure containing the command arguments supplied to [clock format] and [clock scan]
 */

typedef struct ClockFmtScnCmdArgs {
    Tcl_Obj *formatObj;	    /* Format */
    Tcl_Obj *localeObj;	    /* Name of the locale where the time will be expressed. */
    Tcl_Obj *timezoneObj;   /* Default time zone in which the time will be expressed */
    Tcl_Obj *baseObj;	    /* Base (scan only) */
} ClockFmtScnCmdArgs;

/*
 * Structure containing the fields used in [clock format] and [clock scan]
 */

typedef struct TclDateFields {
    Tcl_WideInt seconds;	/* Time expressed in seconds from the Posix
				 * epoch */
    Tcl_WideInt localSeconds;	/* Local time expressed in nominal seconds
				 * from the Posix epoch */
    int secondOfDay;		/* Seconds of day (in-between time only calculation) */
    int tzOffset;		/* Time zone offset in seconds east of
				 * Greenwich */
    Tcl_Obj *tzName;		/* Time zone name (if set the refCount is incremented) */
    Tcl_Obj *tzData;		/* Time zone data object (internally referenced) */
    int julianDay;		/* Julian Day Number in local time zone */
    enum {BCE=1, CE=0} era;	/* Era */
    int gregorian;		/* Flag == 1 if the date is Gregorian */
    int year;			/* Year of the era */
    int dayOfYear;		/* Day of the year (1 January == 1) */
    int month;			/* Month number */
    int dayOfMonth;		/* Day of the month */
    int iso8601Year;		/* ISO8601 week-based year */
    int iso8601Week;		/* ISO8601 week number */
    int dayOfWeek;		/* Day of the week */
} TclDateFields;

/*
 * Meridian: am, pm, or 24-hour style.
 */

typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;

/*
 * Clock scan and format facilities.
 */

#define CLOCK_FMT_SCN_STORAGE_GC_SIZE 32

#define CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE 12

typedef enum _CLCKTOK_TYPE {
   CTOKT_EOB=0, CTOKT_DIGIT, CTOKT_SPACE
} CLCKTOK_TYPE;

typedef struct ClockFmtScnStorage ClockFmtScnStorage;

typedef struct ClockFormatToken {
    CLCKTOK_TYPE      type;
} ClockFormatToken;

typedef struct ClockScanToken {
    unsigned short int	type;
    unsigned short int	minSize;
    unsigned short int	maxSize;
    unsigned short int	offs;
} ClockScanToken;

typedef struct ClockFmtScnStorage {
    int			 objRefCount;	/* Reference count shared across threads */
    ClockScanToken     **scnTok;
    unsigned int	 scnTokC;
    ClockFormatToken   **fmtTok;
    unsigned int	 fmtTokC;
#if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0
    ClockFmtScnStorage	*nextPtr;
    ClockFmtScnStorage	*prevPtr;
#endif
/*  +Tcl_HashEntry    hashEntry		/* ClockFmtScnStorage is a derivate of Tcl_HashEntry,
					 * stored by offset +sizeof(self) */
} ClockFmtScnStorage;

typedef struct ClockLitStorage {
    int		      dummy;
} ClockLitStorage;

/*
 * Prototypes of module functions.
 */

MODULE_SCOPE time_t ToSeconds(time_t Hours, time_t Minutes,
			    time_t Seconds, MERIDIAN Meridian);

MODULE_SCOPE int    TclClockFreeScan(Tcl_Interp *interp, DateInfo *info);

/* tclClockFmt.c module declarations */

MODULE_SCOPE ClockFmtScnStorage * 
		    Tcl_GetClockFrmScnFromObj(Tcl_Interp *interp,
			Tcl_Obj *objPtr);

MODULE_SCOPE int    ClockScan(ClientData clientData, Tcl_Interp *interp,
			TclDateFields *date, Tcl_Obj *strObj, 
			ClockFmtScnCmdArgs *opts);

/*
 * Other externals.
 */

MODULE_SCOPE unsigned long TclEnvEpoch; /* Epoch of the tcl environment 
					 * (if changed with tcl-env). */

#endif /* _TCLCLOCK_H */
