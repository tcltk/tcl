/*
 * tclDate.h --
 *
 *	This header file handles common usage of clock primitives
 *	between tclDate.c (yacc), tclClock.c and tclClockFmt.c.
 *
 * Copyright (c) 2014 Serg G. Brester (aka sebres)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLCLOCK_H
#define _TCLCLOCK_H

/*
 * Constants
 */

#define JULIAN_DAY_POSIX_EPOCH		2440588
#define GREGORIAN_CHANGE_DATE		2361222
#define SECONDS_PER_DAY			86400
#define JULIAN_SEC_POSIX_EPOCH	      (((Tcl_WideInt) JULIAN_DAY_POSIX_EPOCH) \
					* SECONDS_PER_DAY)
#define FOUR_CENTURIES			146097	/* days */
#define JDAY_1_JAN_1_CE_JULIAN		1721424
#define JDAY_1_JAN_1_CE_GREGORIAN	1721426
#define ONE_CENTURY_GREGORIAN		36524	/* days */
#define FOUR_YEARS			1461	/* days */
#define ONE_YEAR			365	/* days */

#define RODDENBERRY			1946	/* Another epoch (Hi, Jeff!) */


#define CLF_OPTIONAL	       (1 << 0) /* token is non mandatory */
#define CLF_POSIXSEC	       (1 << 1)
#define CLF_LOCALSEC	       (1 << 2)
#define CLF_JULIANDAY	       (1 << 3)
#define CLF_TIME	       (1 << 4)
#define CLF_CENTURY	       (1 << 6)
#define CLF_DAYOFMONTH	       (1 << 7)
#define CLF_DAYOFYEAR	       (1 << 8)
#define CLF_MONTH	       (1 << 9)
#define CLF_YEAR	       (1 << 10)
#define CLF_ISO8601YEAR	       (1 << 12)
#define CLF_ISO8601	       (1 << 13)
#define CLF_ISO8601CENTURY     (1 << 14)
#define CLF_SIGNED	       (1 << 15)
/* On demand (lazy) assemble flags */
#define CLF_ASSEMBLE_DATE      (1 << 28) /* assemble year, month, etc. using julianDay */
#define CLF_ASSEMBLE_JULIANDAY (1 << 29) /* assemble julianDay using year, month, etc. */
#define CLF_ASSEMBLE_SECONDS   (1 << 30) /* assemble localSeconds (and seconds at end) */

#define CLF_DATE	       (CLF_JULIANDAY | CLF_DAYOFMONTH | CLF_DAYOFYEAR | \
				CLF_MONTH | CLF_YEAR | CLF_ISO8601YEAR | CLF_ISO8601)

/*
 * Enumeration of the string literals used in [clock]
 */

typedef enum ClockLiteral {
    LIT__NIL,
    LIT__DEFAULT_FORMAT,
    LIT_SYSTEM,		LIT_CURRENT,		LIT_C,
    LIT_BCE,		LIT_CE,
    LIT_DAYOFMONTH,	LIT_DAYOFWEEK,		LIT_DAYOFYEAR,
    LIT_ERA,		LIT_GMT,		LIT_GREGORIAN,
    LIT_INTEGER_VALUE_TOO_LARGE,
    LIT_ISO8601WEEK,	LIT_ISO8601YEAR,
    LIT_JULIANDAY,	LIT_LOCALSECONDS,
    LIT_MONTH,
    LIT_SECONDS,	LIT_TZNAME,		LIT_TZOFFSET,
    LIT_YEAR,
    LIT_TZDATA,
    LIT_GETSYSTEMTIMEZONE,
    LIT_SETUPTIMEZONE,
    LIT_MCGET,
    LIT_GETSYSTEMLOCALE, LIT_GETCURRENTLOCALE,
    LIT_LOCALIZE_FORMAT,
    LIT__END
} ClockLiteral;

#define CLOCK_LITERAL_ARRAY(litarr) static const char *const litarr[] = { \
    "", \
    "%a %b %d %H:%M:%S %Z %Y", \
    "system",		"current",		"C", \
    "BCE",		"CE", \
    "dayOfMonth",	"dayOfWeek",		"dayOfYear", \
    "era",		":GMT",			"gregorian", \
    "integer value too large to represent", \
    "iso8601Week",	"iso8601Year", \
    "julianDay",	"localSeconds", \
    "month", \
    "seconds",		"tzName",		"tzOffset", \
    "year", \
    "::tcl::clock::TZData", \
    "::tcl::clock::GetSystemTimeZone", \
    "::tcl::clock::SetupTimeZone", \
    "::tcl::clock::mcget", \
    "::tcl::clock::GetSystemLocale", "::tcl::clock::mclocale", \
    "::tcl::clock::LocalizeFormat" \
}

/*
 * Enumeration of the msgcat literals used in [clock]
 */

typedef enum ClockMsgCtLiteral {
    MCLIT__NIL, /* placeholder */
    MCLIT_MONTHS_FULL,	MCLIT_MONTHS_ABBREV,  MCLIT_MONTHS_COMB,
    MCLIT_DAYS_OF_WEEK_FULL,  MCLIT_DAYS_OF_WEEK_ABBREV,  MCLIT_DAYS_OF_WEEK_COMB,
    MCLIT_AM,  MCLIT_PM, 
    MCLIT_LOCALE_ERAS,
    MCLIT_BCE,	 MCLIT_CE,
    MCLIT_BCE2,	 MCLIT_CE2,
    MCLIT_BCE3,	 MCLIT_CE3,
    MCLIT_LOCALE_NUMERALS,
    MCLIT__END
} ClockMsgCtLiteral;

#define CLOCK_LOCALE_LITERAL_ARRAY(litarr, pref) static const char *const litarr[] = { \
    pref "", \
    pref "MONTHS_FULL", pref "MONTHS_ABBREV", pref "MONTHS_COMB", \
    pref "DAYS_OF_WEEK_FULL", pref "DAYS_OF_WEEK_ABBREV", pref "DAYS_OF_WEEK_COMB", \
    pref "AM", pref "PM", \
    pref "LOCALE_ERAS", \
    pref "BCE",	   pref "CE", \
    pref "b.c.e.", pref "c.e.", \
    pref "b.c.",   pref "a.d.", \
    pref "LOCALE_NUMERALS", \
}

/*
 * Structure containing the fields used in [clock format] and [clock scan]
 */

typedef struct TclDateFields {

    /* Cacheable fields:	 */

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

    /* Non cacheable fields:	 */

    Tcl_Obj *tzName;		/* Name (or corresponding DST-abbreviation) of the
				 * time zone, if set the refCount is incremented */
} TclDateFields;

#define ClockCacheableDateFieldsSize \
    TclOffset(TclDateFields, tzName)

/*
 * Structure contains return parsed fields.
 */

typedef struct DateInfo {
    const char *dateStart;
    const char *dateInput;
    const char *dateEnd;

    TclDateFields date;

    int		flags;

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

    time_t dateCentury;

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

inline void
ClockInitDateInfo(DateInfo *info) {
    memset(info, 0, sizeof(DateInfo));
}

/*
 * Structure containing the command arguments supplied to [clock format] and [clock scan]
 */

#define CLF_EXTENDED	(1 << 4)
#define CLF_STRICT	(1 << 8)
#define CLF_LOCALE_USED (1 << 15)

typedef struct ClockFmtScnCmdArgs {
    ClientData clientData;  /* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp;	    /* Tcl interpreter */

    Tcl_Obj *formatObj;	    /* Format */
    Tcl_Obj *localeObj;	    /* Name of the locale where the time will be expressed. */
    Tcl_Obj *timezoneObj;   /* Default time zone in which the time will be expressed */
    Tcl_Obj *baseObj;	    /* Base (scan only) */
    int	     flags;	    /* Flags control scanning */

    Tcl_Obj *mcDictObj;	    /* Current dictionary of tcl::clock package for given localeObj*/
} ClockFmtScnCmdArgs;

/*
 * Structure containing the client data for [clock]
 */

typedef struct ClockClientData {
    int refCount;		/* Number of live references. */
    Tcl_Obj **literals;		/* Pool of object literals (common, locale independent). */
    Tcl_Obj **mcLiterals;	/* Msgcat object literals with mc-keys for search with locale. */
    Tcl_Obj **mcLitIdxs;	/* Msgcat object indices prefixed with _IDX_,
				 * used for quick dictionary search */

    /* Cache for current clock parameters, imparted via "configure" */
    unsigned long LastTZEpoch;
    int currentYearCentury;
    int yearOfCenturySwitch;
    Tcl_Obj *SystemTimeZone;
    Tcl_Obj *SystemSetupTZData;
    Tcl_Obj *GMTSetupTimeZone;
    Tcl_Obj *GMTSetupTZData;
    Tcl_Obj *AnySetupTimeZone;
    Tcl_Obj *AnySetupTZData;
    Tcl_Obj *LastUnnormSetupTimeZone;
    Tcl_Obj *LastSetupTimeZone;
    Tcl_Obj *LastSetupTZData;

    Tcl_Obj *CurrentLocale;
    Tcl_Obj *CurrentLocaleDict;
    Tcl_Obj *LastUnnormUsedLocale;
    Tcl_Obj *LastUsedLocale;
    Tcl_Obj *LastUsedLocaleDict;

    /* Cache for last base (last-second fast convert if base/tz not changed) */
    struct {
	Tcl_Obj *timezoneObj;
	TclDateFields Date;
    } lastBase;
    /* Las-period cache for fast UTC2Local conversion */
    struct {
	/* keys */
	Tcl_Obj	   *timezoneObj;
	int	    changeover;
	Tcl_WideInt seconds;
	Tcl_WideInt rangesVal[2];   /* Bounds for cached time zone offset */
	/* values */
	time_t	    tzOffset;
	Tcl_Obj	   *tzName;
    } UTC2Local;
    /* Las-period cache for fast Local2UTC conversion */
    struct {
	/* keys */
	Tcl_Obj	   *timezoneObj;
	int	    changeover;
	Tcl_WideInt localSeconds;
	Tcl_WideInt rangesVal[2];   /* Bounds for cached time zone offset */
	/* values */
	time_t	    tzOffset;
    } Local2UTC;
} ClockClientData;

#define ClockDefaultYearCentury 2000
#define ClockDefaultCenturySwitch 38

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

#define CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE 2

typedef struct ClockScanToken ClockScanToken;


typedef int ClockScanTokenProc(
	ClockFmtScnCmdArgs *opts,
	DateInfo *info,
	ClockScanToken *tok);


typedef enum _CLCKTOK_TYPE {
   CTOKT_DIGIT = 1, CTOKT_PARSER, CTOKT_SPACE, CTOKT_WORD, CTOKT_CHAR,
   CFMTT_INT, CFMTT_WIDE, CFMTT_PROC
} CLCKTOK_TYPE;

typedef struct ClockScanTokenMap {
    unsigned short int	type;
    unsigned short int	flags;
    unsigned short int	clearFlags;
    unsigned short int	minSize;
    unsigned short int	maxSize;
    unsigned short int	offs;
    ClockScanTokenProc *parser;
    void	       *data;
} ClockScanTokenMap;

typedef struct ClockScanToken {
    ClockScanTokenMap  *map;
    unsigned short int	lookAhead;
    unsigned short int	endDistance;
    struct {
	const char *start;
	const char *end;
    } tokWord;
} ClockScanToken;


#define MIN_FMT_RESULT_BLOCK_ALLOC 200

typedef struct DateFormat {
    char *resMem;
    char *resEnd;
    char *output;

    TclDateFields date;

    Tcl_Obj *localeEra;
} DateFormat;

#define CLFMT_INCR	    (1 << 3)
#define CLFMT_DECR	    (1 << 4)
#define CLFMT_CALC	    (1 << 5)
#define CLFMT_LOCALE_INDX   (1 << 8)

typedef struct ClockFormatToken ClockFormatToken;

typedef int ClockFormatTokenProc(
	ClockFmtScnCmdArgs *opts,
	DateFormat *dateFmt,
	ClockFormatToken *tok,
	int *val);

typedef struct ClockFormatTokenMap {
    unsigned short int	type;
    const char	       *tostr;
    unsigned short int	width;
    unsigned short int	flags;
    unsigned short int	divider;
    unsigned short int	divmod;
    unsigned short int	offs;
    ClockFormatTokenProc *fmtproc;
    void	       *data;
} ClockFormatTokenMap;
typedef struct ClockFormatToken {
    ClockFormatTokenMap *map;
    struct {
	const char *start;
	const char *end;
    } tokWord;
} ClockFormatToken;


typedef struct ClockFmtScnStorage ClockFmtScnStorage;

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

/*
 * Prototypes of module functions.
 */

MODULE_SCOPE time_t ToSeconds(time_t Hours, time_t Minutes,
			    time_t Seconds, MERIDIAN Meridian);
MODULE_SCOPE int    IsGregorianLeapYear(TclDateFields *);
MODULE_SCOPE void
		    GetJulianDayFromEraYearWeekDay(
			    TclDateFields *fields, int changeover);
MODULE_SCOPE void
		    GetJulianDayFromEraYearMonthDay(
			    TclDateFields *fields, int changeover);
MODULE_SCOPE void
		    GetJulianDayFromEraYearDay(
			    TclDateFields *fields, int changeover);
MODULE_SCOPE int    ConvertUTCToLocal(ClientData clientData, Tcl_Interp *,
			    TclDateFields *, Tcl_Obj *timezoneObj, int);
MODULE_SCOPE Tcl_Obj *
		    LookupLastTransition(Tcl_Interp *, Tcl_WideInt,
			    int, Tcl_Obj *const *, Tcl_WideInt rangesVal[2]);

MODULE_SCOPE int    TclClockFreeScan(Tcl_Interp *interp, DateInfo *info);

/* tclClock.c module declarations */

MODULE_SCOPE Tcl_Obj *
		    ClockSetupTimeZone(ClientData clientData,
		Tcl_Interp *interp, Tcl_Obj *timezoneObj);

MODULE_SCOPE Tcl_Obj *
		    ClockMCDict(ClockFmtScnCmdArgs *opts);
MODULE_SCOPE Tcl_Obj *
		    ClockMCGet(ClockFmtScnCmdArgs *opts, int mcKey);
MODULE_SCOPE Tcl_Obj *
		    ClockMCGetIdx(ClockFmtScnCmdArgs *opts, int mcKey);
MODULE_SCOPE int    ClockMCSetIdx(ClockFmtScnCmdArgs *opts, int mcKey, 
			Tcl_Obj *valObj);

/* tclClockFmt.c module declarations */

MODULE_SCOPE Tcl_Obj* 
		    ClockFrmObjGetLocFmtKey(Tcl_Interp *interp,
			Tcl_Obj *objPtr);

MODULE_SCOPE ClockFmtScnStorage * 
		    Tcl_GetClockFrmScnFromObj(Tcl_Interp *interp,
			Tcl_Obj *objPtr);
MODULE_SCOPE Tcl_Obj *
		    ClockLocalizeFormat(ClockFmtScnCmdArgs *opts);

MODULE_SCOPE int    ClockScan(register DateInfo *info,
			Tcl_Obj *strObj, ClockFmtScnCmdArgs *opts);

MODULE_SCOPE int    ClockFormat(register DateFormat *dateFmt, 
			ClockFmtScnCmdArgs *opts);

MODULE_SCOPE void   ClockFrmScnClearCaches(void);

/*
 * Other externals.
 */

MODULE_SCOPE unsigned long TclEnvEpoch; /* Epoch of the tcl environment 
					 * (if changed with tcl-env). */

#endif /* _TCLCLOCK_H */
