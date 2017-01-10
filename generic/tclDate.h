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


typedef struct ClockFormatToken	  ClockFormatToken;
typedef struct ClockScanToken	  ClockScanToken;
typedef struct ClockFmtScnStorage ClockFmtScnStorage;

typedef struct ClockFormatToken {
    ClockFormatToken *nextTok;
} ClockFormatToken;

typedef struct ClockScanToken {
    ClockScanToken   *nextTok;
} ClockScanToken;

typedef struct ClockFmtScnStorage {
    int		      objRefCount;	/* Reference count shared across threads */
    ClockScanToken   *firstScnTok;
    ClockFormatToken *firstFmtTok;
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


/*
 * Other externals.
 */

MODULE_SCOPE unsigned long TclEnvEpoch; /* Epoch of the tcl environment 
					 * (if changed with tcl-env). */

#endif /* _TCLCLOCK_H */
