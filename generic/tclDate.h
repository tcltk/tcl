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

    Tcl_Obj* messages;		/* Error messages */
    const char* separatrix;	/* String separating messages */

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
} DateInfo;


/*
 * Meridian: am, pm, or 24-hour style.
 */

typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;

/*
 * Prototypes of module functions.
 */

MODULE_SCOPE time_t ToSeconds(time_t Hours, time_t Minutes,
			    time_t Seconds, MERIDIAN Meridian);

MODULE_SCOPE int TclClockFreeScan(Tcl_Interp *interp, DateInfo *info);

#endif /* _TCLCLOCK_H */
