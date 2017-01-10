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
 * Structure containing the fields used in [clock format] and [clock scan]
 */

typedef struct TclDateFields {
    Tcl_WideInt seconds;	/* Time expressed in seconds from the Posix
				 * epoch */
    Tcl_WideInt localSeconds;	/* Local time expressed in nominal seconds
				 * from the Posix epoch */
    time_t tzOffset;		/* Time zone offset in seconds east of
				 * Greenwich */
    time_t julianDay;		/* Julian Day Number in local time zone */
    enum {BCE=1, CE=0} era;	/* Era */
    time_t gregorian;		/* Flag == 1 if the date is Gregorian */
    time_t year;		/* Year of the era */
    time_t dayOfYear;		/* Day of the year (1 January == 1) */
    time_t month;		/* Month number */
    time_t dayOfMonth;		/* Day of the month */
    time_t iso8601Year;		/* ISO8601 week-based year */
    time_t iso8601Week;		/* ISO8601 week number */
    time_t dayOfWeek;		/* Day of the week */
    time_t hour;		/* Hours of day (in-between time only calculation) */
    time_t minutes;		/* Minutes of day (in-between time only calculation) */
    time_t secondOfDay;		/* Seconds of day (in-between time only calculation) */

    Tcl_Obj *tzName;		/* Time zone name (if set the refCount is incremented) */
    Tcl_Obj *tzData;		/* Time zone data object (internally referenced) */
} TclDateFields;

#define ClockCacheableDateFieldsSize \
    TclOffset(TclDateFields, tzName)

/*
 * Structure contains return parsed fields.
 */

typedef struct DateInfo {
    const char *dateStart;
    const char *dateInput;

    TclDateFields date;

    time_t dateHaveDate;

    time_t dateMeridian;
    time_t dateHaveTime;

    time_t dateTimezone;
    time_t dateDSTmode;
    time_t dateHaveZone;

    time_t dateRelMonth;
    time_t dateRelDay;
    time_t dateRelSeconds;
    time_t dateHaveRel;

    time_t dateMonthOrdinalIncr;
    time_t dateMonthOrdinal;
    time_t dateHaveOrdinalMonth;

    time_t dateDayOrdinal;
    time_t dateDayNumber;
    time_t dateHaveDay;

    time_t *dateRelPointer;

    time_t dateDigitCount;

    Tcl_Obj* messages;	    /* Error messages */
    const char* separatrix; /* String separating messages */
} DateInfo;

#define yydate	    (info->date)  /* Date fields used for converting */

#define yyDay	    (info->date.dayOfMonth)
#define yyMonth	    (info->date.month)
#define yyYear	    (info->date.year)

#define yyHour	    (info->date.hour)
#define yyMinutes   (info->date.minutes)
#define yySeconds   (info->date.secondOfDay)

#define yyDSTmode   (info->dateDSTmode)
#define yyDayOrdinal	(info->dateDayOrdinal)
#define yyDayNumber (info->dateDayNumber)
#define yyMonthOrdinalIncr  (info->dateMonthOrdinalIncr)
#define yyMonthOrdinal	(info->dateMonthOrdinal)
#define yyHaveDate  (info->dateHaveDate)
#define yyHaveDay   (info->dateHaveDay)
#define yyHaveOrdinalMonth (info->dateHaveOrdinalMonth)
#define yyHaveRel   (info->dateHaveRel)
#define yyHaveTime  (info->dateHaveTime)
#define yyHaveZone  (info->dateHaveZone)
#define yyTimezone  (info->dateTimezone)
#define yyMeridian  (info->dateMeridian)
#define yyRelMonth  (info->dateRelMonth)
#define yyRelDay    (info->dateRelDay)
#define yyRelSeconds	(info->dateRelSeconds)
#define yyRelPointer	(info->dateRelPointer)
#define yyInput	    (info->dateInput)
#define yyDigitCount	(info->dateDigitCount)


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

typedef struct ClockScanToken ClockScanToken;


typedef int ClockScanTokenProc(
	DateInfo *info,
	ClockScanToken *tok);


#define CLF_DATE (1 << 2)
#define CLF_TIME (1 << 3)

typedef enum _CLCKTOK_TYPE {
   CTOKT_DIGIT = 1, CTOKT_SPACE, CTOKT_WORD
} CLCKTOK_TYPE;

typedef struct ClockFmtScnStorage ClockFmtScnStorage;

typedef struct ClockFormatToken {
    CLCKTOK_TYPE      type;
} ClockFormatToken;

typedef struct ClockScanTokenMap {
    unsigned short int	type;
    unsigned short int	flags;
    unsigned short int	minSize;
    unsigned short int	maxSize;
    unsigned short int	offs;
    ClockScanTokenProc *parser;
} ClockScanTokenMap;

typedef struct ClockScanToken {
    ClockScanTokenMap  *map;
    unsigned int	lookAhead;
    struct {
	const char *start;
	const char *end;
    } tokWord;
} ClockScanToken;

typedef struct ClockFmtScnStorage {
    int			 objRefCount;	/* Reference count shared across threads */
    ClockScanToken	*scnTok;
    unsigned int	 scnTokC;
    ClockFormatToken	*fmtTok;
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
			register DateInfo *info,
			Tcl_Obj *strObj, ClockFmtScnCmdArgs *opts);

/*
 * Other externals.
 */

MODULE_SCOPE unsigned long TclEnvEpoch; /* Epoch of the tcl environment 
					 * (if changed with tcl-env). */

#endif /* _TCLCLOCK_H */
