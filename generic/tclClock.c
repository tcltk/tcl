/*
 * tclClock.c --
 *
 *	Contains the time and date related commands. This code is derived from
 *	the time and date facilities of TclX, by Mark Diekhans and Karl
 *	Lehenbauer.
 *
 * Copyright 1991-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 2004 by Kevin B. Kenny. All rights reserved.
 * Copyright (c) 2015 by Sergey G. Brester aka sebres. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclStrIdxTree.h"
#include "tclDate.h"
#include "tclCompile.h"

/*
 * Windows has mktime. The configurators do not check.
 */

#ifdef _WIN32
#define HAVE_MKTIME 1
#endif

/*
 * Table of the days in each month, leap and common years
 */

static const int hath[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};
static const int daysInPriorMonths[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

/*
 * Enumeration of the string literals used in [clock]
 */

CLOCK_LITERAL_ARRAY(Literals);

/* Msgcat literals for exact match (mcKey) */
CLOCK_LOCALE_LITERAL_ARRAY(MsgCtLiterals, "");
/* Msgcat index literals prefixed with _IDX_, used for quick dictionary search */
CLOCK_LOCALE_LITERAL_ARRAY(MsgCtLitIdxs, "_IDX_");

static const char *const eras[] = { "CE", "BCE", NULL };

/*
 * Thread specific data block holding a 'struct tm' for the 'gmtime' and
 * 'localtime' library calls.
 */

static Tcl_ThreadDataKey tmKey;

/*
 * Mutex protecting 'gmtime', 'localtime' and 'mktime' calls and the statics
 * in the date parsing code.
 */

TCL_DECLARE_MUTEX(clockMutex)

/*
 * Function prototypes for local procedures in this file:
 */

static int		ConvertUTCToLocalUsingTable(Tcl_Interp *,
			    TclDateFields *, int, Tcl_Obj *const[],
			    Tcl_WideInt rangesVal[2]);
static int		ConvertUTCToLocalUsingC(Tcl_Interp *,
			    TclDateFields *, int);
static int		ConvertLocalToUTC(ClientData clientData, Tcl_Interp *,
			    TclDateFields *, Tcl_Obj *timezoneObj, int);
static int		ConvertLocalToUTCUsingTable(Tcl_Interp *,
			    TclDateFields *, int, Tcl_Obj *const[],
			    Tcl_WideInt rangesVal[2]);
static int		ConvertLocalToUTCUsingC(Tcl_Interp *,
			    TclDateFields *, int);
static int		ClockConfigureObjCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static void		GetYearWeekDay(TclDateFields *, int);
static void		GetGregorianEraYearDay(TclDateFields *, int);
static void		GetMonthDay(TclDateFields *);
static int		WeekdayOnOrBefore(int, int);
static int		ClockClicksObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockConvertlocaltoutcObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);

static int		ClockGetDateFields(ClientData clientData, 
			    Tcl_Interp *interp, TclDateFields *fields, 
			    Tcl_Obj *timezoneObj, int changeover);
static int		ClockGetdatefieldsObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockGetjuliandayfromerayearmonthdayObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockGetjuliandayfromerayearweekdayObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockGetenvObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockMicrosecondsObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockMillisecondsObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockSecondsObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockFormatObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockScanObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ClockScanCommit(
			    ClientData clientData, register DateInfo *info,
			    register ClockFmtScnCmdArgs *opts);
static int		ClockFreeScan(
			    register DateInfo *info, 
			    Tcl_Obj *strObj, ClockFmtScnCmdArgs *opts);
static int		ClockCalcRelTime(
			    register DateInfo *info, ClockFmtScnCmdArgs *opts);
static int		ClockAddObjCmd(
			    ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static struct tm *	ThreadSafeLocalTime(const time_t *);
static unsigned long	TzsetGetEpoch(void);
static void		TzsetIfNecessary(void);
static void		ClockDeleteCmdProc(ClientData);

/*
 * Structure containing description of "native" clock commands to create.
 */

struct ClockCommand {
    const char *name;		/* The tail of the command name. The full name
				 * is "::tcl::clock::<name>". When NULL marks
				 * the end of the table. */
    Tcl_ObjCmdProc *objCmdProc; /* Function that implements the command. This
				 * will always have the ClockClientData sent
				 * to it, but may well ignore this data. */
    CompileProc *compileProc;	/* The compiler for the command. */
    ClientData clientData;	/* Any clientData to give the command (if NULL
    				 * a reference to ClockClientData will be sent) */
};

static const struct ClockCommand clockCommands[] = {
    {"add",		ClockAddObjCmd,		TclCompileBasicMin1ArgCmd, NULL},
    {"clicks",		ClockClicksObjCmd,	TclCompileClockClicksCmd,  NULL},
    {"format",		ClockFormatObjCmd,	TclCompileBasicMin1ArgCmd, NULL},
    {"getenv",		ClockGetenvObjCmd,	TclCompileBasicMin1ArgCmd, NULL},
    {"microseconds",	ClockMicrosecondsObjCmd,TclCompileClockReadingCmd, INT2PTR(1)},
    {"milliseconds",	ClockMillisecondsObjCmd,TclCompileClockReadingCmd, INT2PTR(2)},
    {"scan",		ClockScanObjCmd,	TclCompileBasicMin1ArgCmd, NULL},
    {"seconds",		ClockSecondsObjCmd,	TclCompileClockReadingCmd, INT2PTR(3)},
    {"configure",	  ClockConfigureObjCmd,			NULL, NULL},
    {"Oldscan",		  TclClockOldscanObjCmd,		NULL, NULL},
    {"ConvertLocalToUTC", ClockConvertlocaltoutcObjCmd,		NULL, NULL},
    {"GetDateFields",	  ClockGetdatefieldsObjCmd,		NULL, NULL},
    {"GetJulianDayFromEraYearMonthDay",
		ClockGetjuliandayfromerayearmonthdayObjCmd,	NULL, NULL},
    {"GetJulianDayFromEraYearWeekDay",
		ClockGetjuliandayfromerayearweekdayObjCmd,	NULL, NULL},
    {NULL, NULL, NULL, NULL}
};

/*
 *----------------------------------------------------------------------
 *
 * TclClockInit --
 *
 *	Registers the 'clock' subcommands with the Tcl interpreter and
 *	initializes its client data (which consists mostly of constant
 *	Tcl_Obj's that it is too much trouble to keep recreating).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Installs the commands and creates the client data
 *
 *----------------------------------------------------------------------
 */

void
TclClockInit(
    Tcl_Interp *interp)		/* Tcl interpreter */
{
    const struct ClockCommand *clockCmdPtr;
    char cmdName[50];		/* Buffer large enough to hold the string
				 *::tcl::clock::GetJulianDayFromEraYearMonthDay
				 * plus a terminating NUL. */
    Command         *cmdPtr;
    ClockClientData *data;
    int i;

    /*
     * Safe interps get [::clock] as alias to a master, so do not need their
     * own copies of the support routines.
     */

    if (Tcl_IsSafe(interp)) {
	return;
    }

    /*
     * Create the client data, which is a refcounted literal pool.
     */

    data = ckalloc(sizeof(ClockClientData));
    data->refCount = 0;
    data->literals = ckalloc(LIT__END * sizeof(Tcl_Obj*));
    for (i = 0; i < LIT__END; ++i) {
	Tcl_InitObjRef(data->literals[i], Tcl_NewStringObj(Literals[i], -1));
    }
    data->mcLiterals = NULL;
    data->mcLitIdxs = NULL;
    data->LastTZEpoch = 0;
    data->currentYearCentury = ClockDefaultYearCentury;
    data->yearOfCenturySwitch = ClockDefaultCenturySwitch;
    data->SystemTimeZone = NULL;
    data->SystemSetupTZData = NULL;
    data->GMTSetupTimeZone = NULL;
    data->GMTSetupTZData = NULL;
    data->AnySetupTimeZone = NULL;
    data->AnySetupTZData = NULL;
    data->LastUnnormSetupTimeZone = NULL;
    data->LastSetupTimeZone = NULL;
    data->LastSetupTZData = NULL;

    data->CurrentLocale = NULL;
    data->CurrentLocaleDict = NULL;
    data->LastUnnormUsedLocale = NULL;
    data->LastUsedLocale = NULL;
    data->LastUsedLocaleDict = NULL;

    data->lastBase.timezoneObj = NULL;
    data->UTC2Local.timezoneObj = NULL;
    data->UTC2Local.tzName = NULL;
    data->Local2UTC.timezoneObj = NULL;

    /*
     * Install the commands.
     */

#define TCL_CLOCK_PREFIX_LEN 14 /* == strlen("::tcl::clock::") */
    memcpy(cmdName, "::tcl::clock::", TCL_CLOCK_PREFIX_LEN);
    for (clockCmdPtr=clockCommands ; clockCmdPtr->name!=NULL ; clockCmdPtr++) {
    	ClientData clientData;

	strcpy(cmdName + TCL_CLOCK_PREFIX_LEN, clockCmdPtr->name);
	if (!(clientData = clockCmdPtr->clientData)) {
	    clientData = data;
	    data->refCount++;
	}
	cmdPtr = (Command *)Tcl_CreateObjCommand(interp, cmdName, 
		clockCmdPtr->objCmdProc, clientData,
		clockCmdPtr->clientData ? NULL : ClockDeleteCmdProc);
	cmdPtr->compileProc = clockCmdPtr->compileProc ? 
		clockCmdPtr->compileProc : TclCompileBasicMin0ArgCmd;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ClockConfigureClear --
 *
 *	Clean up cached resp. run-time storages used in clock commands.
 *
 *	Shared usage for clean-up (ClockDeleteCmdProc) and "configure -clear".
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ClockConfigureClear(
    ClockClientData *data)
{
    ClockFrmScnClearCaches();

    data->LastTZEpoch = 0;
    Tcl_UnsetObjRef(data->SystemTimeZone);
    Tcl_UnsetObjRef(data->SystemSetupTZData);
    Tcl_UnsetObjRef(data->GMTSetupTimeZone);
    Tcl_UnsetObjRef(data->GMTSetupTZData);
    Tcl_UnsetObjRef(data->AnySetupTimeZone);
    Tcl_UnsetObjRef(data->AnySetupTZData);
    Tcl_UnsetObjRef(data->LastUnnormSetupTimeZone);
    Tcl_UnsetObjRef(data->LastSetupTimeZone);
    Tcl_UnsetObjRef(data->LastSetupTZData);

    Tcl_UnsetObjRef(data->CurrentLocale);
    Tcl_UnsetObjRef(data->CurrentLocaleDict);
    Tcl_UnsetObjRef(data->LastUnnormUsedLocale);
    Tcl_UnsetObjRef(data->LastUsedLocale);
    Tcl_UnsetObjRef(data->LastUsedLocaleDict);

    Tcl_UnsetObjRef(data->lastBase.timezoneObj);
    Tcl_UnsetObjRef(data->UTC2Local.timezoneObj);
    Tcl_UnsetObjRef(data->UTC2Local.tzName);
    Tcl_UnsetObjRef(data->Local2UTC.timezoneObj);
}

/*
 *----------------------------------------------------------------------
 *
 * ClockDeleteCmdProc --
 *
 *	Remove a reference to the clock client data, and clean up memory
 *	when it's all gone.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
ClockDeleteCmdProc(
    ClientData clientData)	/* Opaque pointer to the client data */
{
    ClockClientData *data = clientData;
    int i;

    if (data->refCount-- <= 1) {
	for (i = 0; i < LIT__END; ++i) {
	    Tcl_DecrRefCount(data->literals[i]);
	}
	if (data->mcLiterals != NULL) {
	    for (i = 0; i < MCLIT__END; ++i) {
		Tcl_DecrRefCount(data->mcLiterals[i]);
	    }
	    data->mcLiterals = NULL;
	}
	if (data->mcLitIdxs != NULL) {
	    for (i = 0; i < MCLIT__END; ++i) {
		Tcl_DecrRefCount(data->mcLitIdxs[i]);
	    }
	    data->mcLitIdxs = NULL;
	}

	ClockConfigureClear(data);

	ckfree(data->literals);
	ckfree(data);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NormTimezoneObj --
 *
 *	Normalizes the timezone object (used for caching puposes).
 *
 *	If already cached time zone could be found, returns this
 *	object (last setup or last used, system (current) or gmt).
 *
 * Results:
 *	Normalized tcl object pointer.
 *
 *----------------------------------------------------------------------
 */

static inline Tcl_Obj *
NormTimezoneObj(
    ClockClientData *dataPtr,  /* Client data containing literal pool */
    Tcl_Obj *timezoneObj)
{
    const char *tz;
    if ( timezoneObj == dataPtr->LastUnnormSetupTimeZone
      && dataPtr->LastSetupTimeZone != NULL 
    ) {
	return dataPtr->LastSetupTimeZone;
    }
    if ( timezoneObj == dataPtr->LastSetupTimeZone
      || timezoneObj == dataPtr->literals[LIT_GMT]
      || timezoneObj == dataPtr->SystemTimeZone
      || timezoneObj == dataPtr->AnySetupTimeZone
    ) {
	return timezoneObj;
    }

    tz = TclGetString(timezoneObj);
    if (dataPtr->AnySetupTimeZone != NULL &&
	(timezoneObj == dataPtr->AnySetupTimeZone
	   || strcmp(tz, TclGetString(dataPtr->AnySetupTimeZone)) == 0
	)
    ) {
	timezoneObj = dataPtr->AnySetupTimeZone;
    }
    else
    if (dataPtr->SystemTimeZone != NULL &&
	(timezoneObj == dataPtr->SystemTimeZone
	   || strcmp(tz, TclGetString(dataPtr->SystemTimeZone)) == 0
	)
    ) {
	timezoneObj = dataPtr->SystemTimeZone;
    }
    else
    if (
	strcmp(tz, Literals[LIT_GMT]) == 0
    ) {
	timezoneObj = dataPtr->literals[LIT_GMT];
    }
    return timezoneObj;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetSystemLocale --
 *
 *	Returns system locale.
 *
 *	Executes ::tcl::clock::GetSystemLocale in given interpreter.
 *
 * Results:
 *	Returns system locale tcl object.
 *
 *----------------------------------------------------------------------
 */

static inline Tcl_Obj *
ClockGetSystemLocale(
    ClockClientData *dataPtr,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp)		/* Tcl interpreter */
{
    if (Tcl_EvalObjv(interp, 1, &dataPtr->literals[LIT_GETSYSTEMLOCALE], 0) != TCL_OK) {
	return NULL;
    }

    return Tcl_GetObjResult(interp);
}
/*
 *----------------------------------------------------------------------
 *
 * ClockGetCurrentLocale --
 *
 *	Returns current locale.
 *
 *	Executes ::tcl::clock::mclocale in given interpreter.
 *
 * Results:
 *	Returns current locale tcl object.
 *
 *----------------------------------------------------------------------
 */

static inline Tcl_Obj *
ClockGetCurrentLocale(
    ClockClientData *dataPtr,	/* Client data containing literal pool */
    Tcl_Interp *interp)		/* Tcl interpreter */
{   
    if (Tcl_EvalObjv(interp, 1, &dataPtr->literals[LIT_GETCURRENTLOCALE], 0) != TCL_OK) {
	return NULL;
    }

    Tcl_SetObjRef(dataPtr->CurrentLocale, Tcl_GetObjResult(interp));
    Tcl_UnsetObjRef(dataPtr->CurrentLocaleDict);

    return dataPtr->CurrentLocale;
}

/*
 *----------------------------------------------------------------------
 *
 * NormLocaleObj --
 *
 *	Normalizes the locale object (used for caching puposes).
 *
 *	If already cached locale could be found, returns this
 *	object (current, system (OS) or last used locales).
 *
 * Results:
 *	Normalized tcl object pointer.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
NormLocaleObj(
    ClockClientData *dataPtr,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj    *localeObj,
    Tcl_Obj   **mcDictObj)
{
    const char *loc;
    if ( localeObj == NULL || localeObj == dataPtr->CurrentLocale
      || localeObj == dataPtr->literals[LIT_C] 
      || localeObj == dataPtr->literals[LIT_CURRENT]
    ) {
	if (dataPtr->CurrentLocale == NULL) {
	    ClockGetCurrentLocale(dataPtr, interp);
	}
	*mcDictObj = dataPtr->CurrentLocaleDict;
	return dataPtr->CurrentLocale;
    }
    if ( localeObj == dataPtr->LastUsedLocale
      || localeObj == dataPtr->LastUnnormUsedLocale
    ) {
	*mcDictObj = dataPtr->LastUsedLocaleDict;
	return dataPtr->LastUsedLocale;
    }

    loc = TclGetString(localeObj);
    if ( dataPtr->CurrentLocale != NULL
      && ( localeObj == dataPtr->CurrentLocale
       || (localeObj->length == dataPtr->CurrentLocale->length
	  && strcmp(loc, TclGetString(dataPtr->CurrentLocale)) == 0
	)
      )
    ) {
	*mcDictObj = dataPtr->CurrentLocaleDict;
	localeObj = dataPtr->CurrentLocale;
    }
    else
    if ( dataPtr->LastUsedLocale != NULL
      && ( localeObj == dataPtr->LastUsedLocale
       || (localeObj->length == dataPtr->LastUsedLocale->length
	  && strcmp(loc, TclGetString(dataPtr->LastUsedLocale)) == 0
	)
      )
    ) {
	*mcDictObj = dataPtr->LastUsedLocaleDict;
	Tcl_SetObjRef(dataPtr->LastUnnormUsedLocale, localeObj);
	localeObj = dataPtr->LastUsedLocale;
    }
    else
    if (
	 (localeObj->length == 1 /* C */
	   && strncasecmp(loc, Literals[LIT_C], localeObj->length) == 0)
      || (localeObj->length == 7 /* current */
	   && strncasecmp(loc, Literals[LIT_CURRENT], localeObj->length) == 0)
    ) {
	if (dataPtr->CurrentLocale == NULL) {
	    ClockGetCurrentLocale(dataPtr, interp);
	}
	*mcDictObj = dataPtr->CurrentLocaleDict;
	localeObj = dataPtr->CurrentLocale;
    } 
    else 
    if (
	 (localeObj->length == 6 /* system */
	   && strncasecmp(loc, Literals[LIT_SYSTEM], localeObj->length) == 0)
    ) {
	Tcl_SetObjRef(dataPtr->LastUnnormUsedLocale, localeObj);
	localeObj = ClockGetSystemLocale(dataPtr, interp);
	Tcl_SetObjRef(dataPtr->LastUsedLocale, localeObj);
	*mcDictObj = NULL;
    } 
    else 
    {
	*mcDictObj = NULL;
    }
    return localeObj;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockMCDict --
 *
 *	Retrieves a localized storage dictionary object for the given 
 *	locale object.
 *
 *	This corresponds with call `::tcl::clock::mcget locale`.
 *	Cached representation stored in options (for further access).
 *
 * Results:
 *	Tcl-object contains smart reference to msgcat dictionary.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_Obj *
ClockMCDict(ClockFmtScnCmdArgs *opts)
{
    ClockClientData *dataPtr = opts->clientData;

    /* if dict not yet retrieved */
    if (opts->mcDictObj == NULL) {

	/* if locale was not yet used */
	if ( !(opts->flags & CLF_LOCALE_USED) ) {
	    
	    opts->localeObj = NormLocaleObj(opts->clientData, opts->interp,
		opts->localeObj, &opts->mcDictObj);
	    
	    if (opts->localeObj == NULL) {
		Tcl_SetResult(opts->interp,
		    (char*)"locale not specified and no default locale set", TCL_STATIC);
		Tcl_SetErrorCode(opts->interp, "CLOCK", "badOption", NULL);
		return NULL;
	    }
	    opts->flags |= CLF_LOCALE_USED;

	    /* check locale literals already available (on demand creation) */
	    if (dataPtr->mcLiterals == NULL) {
		int i;
		dataPtr->mcLiterals = ckalloc(MCLIT__END * sizeof(Tcl_Obj*));
		for (i = 0; i < MCLIT__END; ++i) {
		    Tcl_InitObjRef(dataPtr->mcLiterals[i], 
			Tcl_NewStringObj(MsgCtLiterals[i], -1));
		}
	    }
	}

	if (opts->mcDictObj == NULL) {
	    Tcl_Obj *callargs[2];
	    /* get msgcat dictionary - ::tcl::clock::mcget locale */
	    callargs[0] = dataPtr->literals[LIT_MCGET];
	    callargs[1] = opts->localeObj;

	    if (Tcl_EvalObjv(opts->interp, 2, callargs, 0) != TCL_OK) {
		return NULL;
	    }

	    opts->mcDictObj = Tcl_GetObjResult(opts->interp);
	    /* be sure that object reference not increases (dict changeable) */
	    if (opts->mcDictObj->refCount > 0) {
		/* smart reference (shared dict as object with no ref-counter) */
		opts->mcDictObj = Tcl_DictObjSmartRef(opts->interp, opts->mcDictObj);
	    }
	    if ( opts->localeObj == dataPtr->CurrentLocale ) {
		Tcl_SetObjRef(dataPtr->CurrentLocaleDict, opts->mcDictObj);
	    } else if ( opts->localeObj == dataPtr->LastUsedLocale ) {
		Tcl_SetObjRef(dataPtr->LastUsedLocaleDict, opts->mcDictObj);
	    } else {
		Tcl_SetObjRef(dataPtr->LastUsedLocale, opts->localeObj);
		Tcl_UnsetObjRef(dataPtr->LastUnnormUsedLocale);
		Tcl_SetObjRef(dataPtr->LastUsedLocaleDict, opts->mcDictObj);
	    }
	    Tcl_ResetResult(opts->interp);
	}
    }

    return opts->mcDictObj;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockMCGet --
 *
 *	Retrieves a msgcat value for the given literal integer mcKey
 *	from localized storage (corresponding given locale object)
 *	by mcLiterals[mcKey] (e. g. MONTHS_FULL).
 *
 * Results:
 *	Tcl-object contains localized value.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_Obj *
ClockMCGet(
    ClockFmtScnCmdArgs *opts, 
    int mcKey)
{
    ClockClientData *dataPtr = opts->clientData;

    Tcl_Obj *valObj = NULL;

    if (opts->mcDictObj == NULL) {
	ClockMCDict(opts);
	if (opts->mcDictObj == NULL)
	    return NULL;
    }

    Tcl_DictObjGet(opts->interp, opts->mcDictObj, 
	dataPtr->mcLiterals[mcKey], &valObj);

    return valObj; /* or NULL in obscure case if Tcl_DictObjGet failed */
}

/*
 *----------------------------------------------------------------------
 *
 * ClockMCGetIdx --
 *
 *	Retrieves an indexed msgcat value for the given literal integer mcKey
 *	from localized storage (corresponding given locale object)
 *	by mcLitIdxs[mcKey] (e. g. _IDX_MONTHS_FULL).
 *
 * Results:
 *	Tcl-object contains localized indexed value.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_Obj *
ClockMCGetIdx(
    ClockFmtScnCmdArgs *opts, 
    int mcKey)
{
    ClockClientData *dataPtr = opts->clientData;

    Tcl_Obj *valObj = NULL;

    if (opts->mcDictObj == NULL) {
	ClockMCDict(opts);
	if (opts->mcDictObj == NULL)
	    return NULL;
    }

    /* try to get indices object */
    if (dataPtr->mcLitIdxs == NULL) {
	return NULL;
    }
    
    if (Tcl_DictObjGet(NULL, opts->mcDictObj, 
	dataPtr->mcLitIdxs[mcKey], &valObj) != TCL_OK
    ) {
	return NULL;
    }

    return valObj;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockMCSetIdx --
 *
 *	Sets an indexed msgcat value for the given literal integer mcKey
 *	in localized storage (corresponding given locale object)
 *	by mcLitIdxs[mcKey] (e. g. _IDX_MONTHS_FULL).
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
ClockMCSetIdx(
    ClockFmtScnCmdArgs *opts, 
    int mcKey, Tcl_Obj *valObj)
{
    ClockClientData *dataPtr = opts->clientData;

    if (opts->mcDictObj == NULL) {
	ClockMCDict(opts);
	if (opts->mcDictObj == NULL)
	    return TCL_ERROR;
    }

    /* if literal storage for indices not yet created */
    if (dataPtr->mcLitIdxs == NULL) {
	int i;
	dataPtr->mcLitIdxs = ckalloc(MCLIT__END * sizeof(Tcl_Obj*));
	for (i = 0; i < MCLIT__END; ++i) {
	    Tcl_InitObjRef(dataPtr->mcLitIdxs[i], 
		Tcl_NewStringObj(MsgCtLitIdxs[i], -1));
	}
    }

    return Tcl_DictObjPut(opts->interp, opts->mcDictObj, 
	    dataPtr->mcLitIdxs[mcKey], valObj);
}

/*
 *----------------------------------------------------------------------
 *
 * ClockConfigureObjCmd --
 *
 *	This function is invoked to process the Tcl "clock configure" command.
 *
 * Usage:
 *	::tcl::clock::configure ?-option ?value??
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ClockConfigureObjCmd(
    ClientData clientData,  /* Client data containing literal pool */
    Tcl_Interp *interp,	    /* Tcl interpreter */
    int objc,		    /* Parameter count */
    Tcl_Obj *const objv[])  /* Parameter vector */
{
    ClockClientData *dataPtr = clientData;
    
    static const char *const options[] = {
	"-system-tz",	  "-setup-tz",	  "-default-locale",
	"-clear",
	"-year-century",  "-century-switch",
	NULL
    };
    enum optionInd {
	CLOCK_SYSTEM_TZ,  CLOCK_SETUP_TZ, CLOCK_CURRENT_LOCALE,
	CLOCK_CLEAR_CACHE,
	CLOCK_YEAR_CENTURY, CLOCK_CENTURY_SWITCH,
	CLOCK_SETUP_GMT, CLOCK_SETUP_NOP
    };
    int optionIndex;		/* Index of an option. */
    int i;

    for (i = 1; i < objc; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i++], options, 
	    "option", 0, &optionIndex) != TCL_OK) {
	    Tcl_SetErrorCode(interp, "CLOCK", "badOption",
		    Tcl_GetString(objv[i-1]), NULL);
	    return TCL_ERROR;
	}
	switch (optionIndex) {
	case CLOCK_SYSTEM_TZ:
	if (1) {
	    /* validate current tz-epoch */
	    unsigned long lastTZEpoch = TzsetGetEpoch();
	    if (i < objc) {
		if (dataPtr->SystemTimeZone != objv[i]) {
		    Tcl_SetObjRef(dataPtr->SystemTimeZone, objv[i]);
		    Tcl_UnsetObjRef(dataPtr->SystemSetupTZData);
		}
		dataPtr->LastTZEpoch = lastTZEpoch;
	    } 
	    if (i+1 >= objc && dataPtr->SystemTimeZone != NULL 
			 && dataPtr->LastTZEpoch == lastTZEpoch) {
		Tcl_SetObjResult(interp, dataPtr->SystemTimeZone);
	    }
	}
	break;
	case CLOCK_SETUP_TZ:
	    if (i < objc) {
		/* differentiate GMT and system zones, because used often */
		Tcl_Obj *timezoneObj = NormTimezoneObj(dataPtr, objv[i]);
		Tcl_SetObjRef(dataPtr->LastUnnormSetupTimeZone, objv[i]);
		if (dataPtr->LastSetupTimeZone != timezoneObj) {
		    Tcl_SetObjRef(dataPtr->LastSetupTimeZone, timezoneObj);
		    Tcl_UnsetObjRef(dataPtr->LastSetupTZData);
		}
		if (timezoneObj == dataPtr->literals[LIT_GMT]) {
		    optionIndex = CLOCK_SETUP_GMT;
		} else if (timezoneObj == dataPtr->SystemTimeZone) {
		    optionIndex = CLOCK_SETUP_NOP;
		}
		switch (optionIndex) {
		case CLOCK_SETUP_GMT:
		    if (i < objc) {
			if (dataPtr->GMTSetupTimeZone != timezoneObj) {
			    Tcl_SetObjRef(dataPtr->GMTSetupTimeZone, timezoneObj);
			    Tcl_UnsetObjRef(dataPtr->GMTSetupTZData);
			}
		    }
		break;
		case CLOCK_SETUP_TZ:
		    if (i < objc) {
			if (dataPtr->AnySetupTimeZone != timezoneObj) {
			    Tcl_SetObjRef(dataPtr->AnySetupTimeZone, timezoneObj);
			    Tcl_UnsetObjRef(dataPtr->AnySetupTZData);
			}
		    } 
		break;
		}
	    }
	    if (i+1 >= objc && dataPtr->LastSetupTimeZone != NULL) {
		Tcl_SetObjResult(interp, dataPtr->LastSetupTimeZone);
	    }
	break;
	case CLOCK_CURRENT_LOCALE:
	    if (i < objc) {
		if (dataPtr->CurrentLocale != objv[i]) {
		    Tcl_SetObjRef(dataPtr->CurrentLocale, objv[i]);
		    Tcl_UnsetObjRef(dataPtr->CurrentLocaleDict);
		}
	    }
	    if (i+1 >= objc && dataPtr->CurrentLocale != NULL) {
		Tcl_SetObjResult(interp, dataPtr->CurrentLocale);
	    }
	break;
	case CLOCK_YEAR_CENTURY:
	    if (i < objc) {
		int year;
		if (TclGetIntFromObj(interp, objv[i], &year) != TCL_OK) {
		    return TCL_ERROR;
		}
		dataPtr->currentYearCentury = year;
		if (i+1 >= objc) {
		    Tcl_SetObjResult(interp, objv[i]);
		}
		continue;
	    }
	    if (i+1 >= objc) {
		Tcl_SetObjResult(interp, 
		    Tcl_NewIntObj(dataPtr->currentYearCentury));
	    }
	break;
	case CLOCK_CENTURY_SWITCH:
	    if (i < objc) {
		int year;
		if (TclGetIntFromObj(interp, objv[i], &year) != TCL_OK) {
		    return TCL_ERROR;
		}
		dataPtr->yearOfCenturySwitch = year;
		Tcl_SetObjResult(interp, objv[i]);
		continue;
	    }
	    if (i+1 >= objc) {
		Tcl_SetObjResult(interp, 
		    Tcl_NewIntObj(dataPtr->yearOfCenturySwitch));
	    }
	break;
	case CLOCK_CLEAR_CACHE:
	    ClockConfigureClear(dataPtr);
	break;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetTZData --
 *
 *	Retrieves tzdata table for given normalized timezone.
 *
 * Results:
 *	Returns a tcl object with tzdata.
 *
 * Side effects:
 *	The tzdata can be cached in ClockClientData structure.
 *
 *----------------------------------------------------------------------
 */

static inline Tcl_Obj *
ClockGetTZData(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *timezoneObj)	/* Name of the timezone */
{
    ClockClientData *dataPtr = clientData;
    Tcl_Obj **literals = dataPtr->literals;
    Tcl_Obj *ret, **out = NULL;

    /* if cached (if already setup this one) */
    if ( dataPtr->LastSetupTZData != NULL
      && ( timezoneObj == dataPtr->LastSetupTimeZone
	|| timezoneObj == dataPtr->LastUnnormSetupTimeZone
      )
    ) {
	return dataPtr->LastSetupTZData;
    }

    /* differentiate GMT and system zones, because used often */
    /* simple caching, because almost used the tz-data of last timezone
     */
    if (timezoneObj == dataPtr->SystemTimeZone) {
	if (dataPtr->SystemSetupTZData != NULL) {
	    return dataPtr->SystemSetupTZData;
	}
	out = &dataPtr->SystemSetupTZData;
    }
    else 
    if (timezoneObj == dataPtr->GMTSetupTimeZone) {
	if (dataPtr->GMTSetupTZData != NULL) {
	    return dataPtr->GMTSetupTZData;
	}
	out = &dataPtr->GMTSetupTZData;
    }
    else
    if (timezoneObj == dataPtr->AnySetupTimeZone) {
	if (dataPtr->AnySetupTZData != NULL) {
	    return dataPtr->AnySetupTZData;
	}
	out = &dataPtr->AnySetupTZData;
    }

    ret = Tcl_ObjGetVar2(interp, literals[LIT_TZDATA],
	timezoneObj, TCL_LEAVE_ERR_MSG);

    /* cache using corresponding slot and as last used */
    if (out != NULL) {
	Tcl_SetObjRef(*out, ret);
    }
    Tcl_SetObjRef(dataPtr->LastSetupTZData, ret);
    if (dataPtr->LastSetupTimeZone != timezoneObj) {
	Tcl_SetObjRef(dataPtr->LastSetupTimeZone, timezoneObj);
	Tcl_UnsetObjRef(dataPtr->LastUnnormSetupTimeZone);
    }
    return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetSystemTimeZone --
 *
 *	Returns system (current) timezone.
 *
 *	If system zone not yet cached, it executes ::tcl::clock::GetSystemTimeZone
 *	in given interpreter and caches its result.
 *
 * Results:
 *	Returns normalized timezone object.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ClockGetSystemTimeZone(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp)		/* Tcl interpreter */
{
    ClockClientData *dataPtr = clientData;
    Tcl_Obj **literals;

    /* if known (cached and same epoch) - return now */
    if (dataPtr->SystemTimeZone != NULL 
	  && dataPtr->LastTZEpoch == TzsetGetEpoch()) {
	return dataPtr->SystemTimeZone;
    }

    Tcl_UnsetObjRef(dataPtr->SystemTimeZone);
    Tcl_UnsetObjRef(dataPtr->SystemSetupTZData);

    literals = dataPtr->literals;

    if (Tcl_EvalObjv(interp, 1, &literals[LIT_GETSYSTEMTIMEZONE], 0) != TCL_OK) {
	return NULL;
    }
    if (dataPtr->SystemTimeZone == NULL) {
	Tcl_SetObjRef(dataPtr->SystemTimeZone, Tcl_GetObjResult(interp));
    }
    return dataPtr->SystemTimeZone;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockSetupTimeZone --
 *
 *	Sets up the timezone. Loads tzdata, etc.
 *
 * Results:
 *	Returns normalized timezone object.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_Obj *
ClockSetupTimeZone(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *timezoneObj)
{
    ClockClientData *dataPtr = clientData;
    Tcl_Obj **literals = dataPtr->literals;
    Tcl_Obj *callargs[2];

    /* if cached (if already setup this one) */
    if ( dataPtr->LastSetupTimeZone != NULL
      && ( timezoneObj == dataPtr->LastSetupTimeZone
	|| timezoneObj == dataPtr->LastUnnormSetupTimeZone
      )
    ) {
	return dataPtr->LastSetupTimeZone;
    }

    /* differentiate GMT and system zones, because used often and already set */
    timezoneObj = NormTimezoneObj(dataPtr, timezoneObj);
    if (   timezoneObj == dataPtr->GMTSetupTimeZone
	|| timezoneObj == dataPtr->SystemTimeZone
	|| timezoneObj == dataPtr->AnySetupTimeZone
    ) {
	return timezoneObj;
    }

    callargs[0] = literals[LIT_SETUPTIMEZONE];
    callargs[1] = timezoneObj;

    if (Tcl_EvalObjv(interp, 2, callargs, 0) == TCL_OK) {
	return dataPtr->LastSetupTimeZone;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockFormatNumericTimeZone --
 *
 *	Formats a time zone as +hhmmss
 *
 * Parameters:
 *	z - Time zone in seconds east of Greenwich
 *
 * Results:
 *	Returns the time zone object (formatted in a numeric form)
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj * 
ClockFormatNumericTimeZone(int z) {
    char sign = '+';
    int h, m;
    if ( z < 0 ) {
	z = -z;
	sign = '-';
    }
    h = z / 3600;
    z %= 3600;
    m = z / 60;
    z %= 60;
    if (z != 0) {
	return Tcl_ObjPrintf("%c%02d%02d%02d", sign, h, m, z);
    }
    return Tcl_ObjPrintf("%c%02d%02d", sign, h, m);
}

/*
 *----------------------------------------------------------------------
 *
 * ClockConvertlocaltoutcObjCmd --
 *
 *	Tcl command that converts a UTC time to a local time by whatever means
 *	is available.
 *
 * Usage:
 *	::tcl::clock::ConvertUTCToLocal dictionary timezone changeover
 *
 * Parameters:
 *	dict - Dictionary containing a 'localSeconds' entry.
 *	timezone - Time zone
 *	changeover - Julian Day of the adoption of the Gregorian calendar.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	On success, sets the interpreter result to the given dictionary
 *	augmented with a 'seconds' field giving the UTC time. On failure,
 *	leaves an error message in the interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
ClockConvertlocaltoutcObjCmd(
    ClientData clientData,	/* Client data */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter vector */
{
    ClockClientData *data = clientData;
    Tcl_Obj *const *literals = data->literals;
    Tcl_Obj *secondsObj;
    Tcl_Obj *dict;
    int changeover;
    TclDateFields fields;
    int created = 0;
    int status;

    fields.tzName = NULL;
    /*
     * Check params and convert time.
     */

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "dict timezone changeover");
	return TCL_ERROR;
    }
    dict = objv[1];
    if (Tcl_DictObjGet(interp, dict, literals[LIT_LOCALSECONDS],
	    &secondsObj)!= TCL_OK) {
	return TCL_ERROR;
    }
    if (secondsObj == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("key \"localseconds\" not "
		"found in dictionary", -1));
	return TCL_ERROR;
    }
    if ((TclGetWideIntFromObj(interp, secondsObj,
	    &fields.localSeconds) != TCL_OK)
	|| (TclGetIntFromObj(interp, objv[3], &changeover) != TCL_OK)
	|| ConvertLocalToUTC(clientData, interp, &fields, objv[2], changeover)) {
	return TCL_ERROR;
    }

    /*
     * Copy-on-write; set the 'seconds' field in the dictionary and place the
     * modified dictionary in the interpreter result.
     */

    if (Tcl_IsShared(dict)) {
	dict = Tcl_DuplicateObj(dict);
	created = 1;
	Tcl_IncrRefCount(dict);
    }
    status = Tcl_DictObjPut(interp, dict, literals[LIT_SECONDS],
	    Tcl_NewWideIntObj(fields.seconds));
    if (status == TCL_OK) {
	Tcl_SetObjResult(interp, dict);
    }
    if (created) {
	Tcl_DecrRefCount(dict);
    }
    return status;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetdatefieldsObjCmd --
 *
 *	Tcl command that determines the values that [clock format] will use in
 *	formatting a date, and populates a dictionary with them.
 *
 * Usage:
 *	::tcl::clock::GetDateFields seconds timezone changeover
 *
 * Parameters:
 *	seconds - Time expressed in seconds from the Posix epoch.
 *	timezone - Time zone in which time is to be expressed.
 *	changeover - Julian Day Number at which the current locale adopted
 *		     the Gregorian calendar
 *
 * Results:
 *	Returns a dictonary populated with the fields:
 *		seconds - Seconds from the Posix epoch
 *		localSeconds - Nominal seconds from the Posix epoch in the
 *			       local time zone.
 *		tzOffset - Time zone offset in seconds east of Greenwich
 *		tzName - Time zone name
 *		julianDay - Julian Day Number in the local time zone
 *
 *----------------------------------------------------------------------
 */

int
ClockGetdatefieldsObjCmd(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter vector */
{
    TclDateFields fields;
    Tcl_Obj *dict;
    ClockClientData *data = clientData;
    Tcl_Obj *const *literals = data->literals;
    int changeover;

    fields.tzName = NULL;

    /*
     * Check params.
     */

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "seconds timezone changeover");
	return TCL_ERROR;
    }
    if (TclGetWideIntFromObj(interp, objv[1], &fields.seconds) != TCL_OK
	    || TclGetIntFromObj(interp, objv[3], &changeover) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * fields.seconds could be an unsigned number that overflowed. Make sure
     * that it isn't.
     */

    if (objv[1]->typePtr == &tclBignumType) {
	Tcl_SetObjResult(interp, literals[LIT_INTEGER_VALUE_TOO_LARGE]);
	return TCL_ERROR;
    }

    /* Extract fields */

    if (ClockGetDateFields(clientData, interp, &fields, objv[2], 
	  changeover) != TCL_OK) {
	return TCL_ERROR;
    }

    /* Make dict of fields */

    dict = Tcl_NewDictObj();
    Tcl_DictObjPut(NULL, dict, literals[LIT_LOCALSECONDS],
	    Tcl_NewWideIntObj(fields.localSeconds));
    Tcl_DictObjPut(NULL, dict, literals[LIT_SECONDS],
	    Tcl_NewWideIntObj(fields.seconds));
    Tcl_DictObjPut(NULL, dict, literals[LIT_TZNAME], fields.tzName);
    Tcl_DecrRefCount(fields.tzName);
    Tcl_DictObjPut(NULL, dict, literals[LIT_TZOFFSET],
	    Tcl_NewIntObj(fields.tzOffset));
    Tcl_DictObjPut(NULL, dict, literals[LIT_JULIANDAY],
	    Tcl_NewIntObj(fields.julianDay));
    Tcl_DictObjPut(NULL, dict, literals[LIT_GREGORIAN],
	    Tcl_NewIntObj(fields.gregorian));
    Tcl_DictObjPut(NULL, dict, literals[LIT_ERA],
	    literals[fields.era ? LIT_BCE : LIT_CE]);
    Tcl_DictObjPut(NULL, dict, literals[LIT_YEAR],
	    Tcl_NewIntObj(fields.year));
    Tcl_DictObjPut(NULL, dict, literals[LIT_DAYOFYEAR],
	    Tcl_NewIntObj(fields.dayOfYear));
    Tcl_DictObjPut(NULL, dict, literals[LIT_MONTH],
	    Tcl_NewIntObj(fields.month));
    Tcl_DictObjPut(NULL, dict, literals[LIT_DAYOFMONTH],
	    Tcl_NewIntObj(fields.dayOfMonth));
    Tcl_DictObjPut(NULL, dict, literals[LIT_ISO8601YEAR],
	    Tcl_NewIntObj(fields.iso8601Year));
    Tcl_DictObjPut(NULL, dict, literals[LIT_ISO8601WEEK],
	    Tcl_NewIntObj(fields.iso8601Week));
    Tcl_DictObjPut(NULL, dict, literals[LIT_DAYOFWEEK],
	    Tcl_NewIntObj(fields.dayOfWeek));
    Tcl_SetObjResult(interp, dict);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetDateFields --
 *
 *	Converts given UTC time (seconds in a TclDateFields structure) 
 *	to local time and determines the values that clock routines will 
 *	use in scanning or formatting a date.
 *
 * Results:
 *	Date-time values are stored in structure "fields".
 *	Returns a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */

int
ClockGetDateFields(
    ClientData clientData,	/* Client data of the interpreter */
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Pointer to result fields, where
				 * fields->seconds contains date to extract */
    Tcl_Obj *timezoneObj,	/* Time zone object or NULL for gmt */
    int changeover)		/* Julian Day Number */
{
    /*
     * Convert UTC time to local.
     */

    if (ConvertUTCToLocal(clientData, interp, fields, timezoneObj,
	    changeover) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Extract Julian day.
     */

    fields->julianDay = (int) ((fields->localSeconds + JULIAN_SEC_POSIX_EPOCH)
	    / SECONDS_PER_DAY);

    /*
     * Convert to Julian or Gregorian calendar.
     */

    GetGregorianEraYearDay(fields, changeover);
    GetMonthDay(fields);
    GetYearWeekDay(fields, changeover);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetjuliandayfromerayearmonthdayObjCmd --
 *
 *	Tcl command that converts a time from era-year-month-day to a Julian
 *	Day Number.
 *
 * Parameters:
 *	dict - Dictionary that contains 'era', 'year', 'month' and
 *	       'dayOfMonth' keys.
 *	changeover - Julian Day of changeover to the Gregorian calendar
 *
 * Results:
 *	Result is either TCL_OK, with the interpreter result being the
 *	dictionary augmented with a 'julianDay' key, or TCL_ERROR,
 *	with the result being an error message.
 *
 *----------------------------------------------------------------------
 */

static int
FetchEraField(
    Tcl_Interp *interp,
    Tcl_Obj *dict,
    Tcl_Obj *key,
    int *storePtr)
{
    Tcl_Obj *value = NULL;

    if (Tcl_DictObjGet(interp, dict, key, &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if (value == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"expected key(s) not found in dictionary", -1));
	return TCL_ERROR;
    }
    return Tcl_GetIndexFromObj(interp, value, eras, "era", TCL_EXACT, storePtr);
}

static int
FetchIntField(
    Tcl_Interp *interp,
    Tcl_Obj *dict,
    Tcl_Obj *key,
    int *storePtr)
{
    Tcl_Obj *value = NULL;

    if (Tcl_DictObjGet(interp, dict, key, &value) != TCL_OK) {
	return TCL_ERROR;
    }
    if (value == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"expected key(s) not found in dictionary", -1));
	return TCL_ERROR;
    }
    return TclGetIntFromObj(interp, value, storePtr);
}

static int
ClockGetjuliandayfromerayearmonthdayObjCmd(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter vector */
{
    TclDateFields fields;
    Tcl_Obj *dict;
    ClockClientData *data = clientData;
    Tcl_Obj *const *literals = data->literals;
    int changeover;
    int copied = 0;
    int status;
    int era = 0;

    fields.tzName = NULL;

    /*
     * Check params.
     */

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dict changeover");
	return TCL_ERROR;
    }
    dict = objv[1];
    if (FetchEraField(interp, dict, literals[LIT_ERA], &era) != TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_YEAR], &fields.year)
		!= TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_MONTH], &fields.month)
		!= TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_DAYOFMONTH],
		&fields.dayOfMonth) != TCL_OK
	    || TclGetIntFromObj(interp, objv[2], &changeover) != TCL_OK) {
	return TCL_ERROR;
    }
    fields.era = era;

    /*
     * Get Julian day.
     */

    GetJulianDayFromEraYearMonthDay(&fields, changeover);

    /*
     * Store Julian day in the dictionary - copy on write.
     */

    if (Tcl_IsShared(dict)) {
	dict = Tcl_DuplicateObj(dict);
	Tcl_IncrRefCount(dict);
	copied = 1;
    }
    status = Tcl_DictObjPut(interp, dict, literals[LIT_JULIANDAY],
	    Tcl_NewIntObj(fields.julianDay));
    if (status == TCL_OK) {
	Tcl_SetObjResult(interp, dict);
    }
    if (copied) {
	Tcl_DecrRefCount(dict);
    }
    return status;
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetjuliandayfromerayearweekdayObjCmd --
 *
 *	Tcl command that converts a time from the ISO calendar to a Julian Day
 *	Number.
 *
 * Parameters:
 *	dict - Dictionary that contains 'era', 'iso8601Year', 'iso8601Week'
 *	       and 'dayOfWeek' keys.
 *	changeover - Julian Day of changeover to the Gregorian calendar
 *
 * Results:
 *	Result is either TCL_OK, with the interpreter result being the
 *	dictionary augmented with a 'julianDay' key, or TCL_ERROR, with the
 *	result being an error message.
 *
 *----------------------------------------------------------------------
 */

static int
ClockGetjuliandayfromerayearweekdayObjCmd(
    ClientData clientData,	/* Opaque pointer to literal pool, etc. */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter vector */
{
    TclDateFields fields;
    Tcl_Obj *dict;
    ClockClientData *data = clientData;
    Tcl_Obj *const *literals = data->literals;
    int changeover;
    int copied = 0;
    int status;
    int era = 0;

    fields.tzName = NULL;

    /*
     * Check params.
     */

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "dict changeover");
	return TCL_ERROR;
    }
    dict = objv[1];
    if (FetchEraField(interp, dict, literals[LIT_ERA], &era) != TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_ISO8601YEAR],
		&fields.iso8601Year) != TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_ISO8601WEEK],
		&fields.iso8601Week) != TCL_OK
	    || FetchIntField(interp, dict, literals[LIT_DAYOFWEEK],
		&fields.dayOfWeek) != TCL_OK
	    || TclGetIntFromObj(interp, objv[2], &changeover) != TCL_OK) {
	return TCL_ERROR;
    }
    fields.era = era;

    /*
     * Get Julian day.
     */

    GetJulianDayFromEraYearWeekDay(&fields, changeover);

    /*
     * Store Julian day in the dictionary - copy on write.
     */

    if (Tcl_IsShared(dict)) {
	dict = Tcl_DuplicateObj(dict);
	Tcl_IncrRefCount(dict);
	copied = 1;
    }
    status = Tcl_DictObjPut(interp, dict, literals[LIT_JULIANDAY],
	    Tcl_NewIntObj(fields.julianDay));
    if (status == TCL_OK) {
	Tcl_SetObjResult(interp, dict);
    }
    if (copied) {
	Tcl_DecrRefCount(dict);
    }
    return status;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertLocalToUTC --
 *
 *	Converts a time (in a TclDateFields structure) from the local wall
 *	clock to UTC.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Populates the 'seconds' field if successful; stores an error message
 *	in the interpreter result on failure.
 *
 *----------------------------------------------------------------------
 */

static int
ConvertLocalToUTC(
    ClientData clientData,	/* Client data of the interpreter */
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Fields of the time */
    Tcl_Obj *timezoneObj,	/* Time zone */
    int changeover)		/* Julian Day of the Gregorian transition */
{
    ClockClientData *dataPtr = clientData;
    Tcl_Obj *tzdata;		/* Time zone data */
    int rowc;			/* Number of rows in tzdata */
    Tcl_Obj **rowv;		/* Pointers to the rows */
    Tcl_WideInt seconds;

    /* fast phase-out for shared GMT-object (don't need to convert UTC 2 UTC) */
    if (timezoneObj == dataPtr->GMTSetupTimeZone && dataPtr->GMTSetupTimeZone != NULL) {
	fields->seconds = fields->localSeconds;
	fields->tzOffset = 0;
	return TCL_OK;
    }

    /*
     * Check cacheable conversion could be used
     * (last-period Local2UTC cache within the same TZ)
     */
    seconds = fields->localSeconds - dataPtr->Local2UTC.tzOffset;
    if ( timezoneObj == dataPtr->Local2UTC.timezoneObj
      && ( fields->localSeconds == dataPtr->Local2UTC.localSeconds
	|| ( seconds >= dataPtr->Local2UTC.rangesVal[0]
	  && seconds <	dataPtr->Local2UTC.rangesVal[1])
      )
      && changeover == dataPtr->Local2UTC.changeover
    ) {
	/* the same time zone and offset (UTC time inside the last minute) */
	fields->tzOffset = dataPtr->Local2UTC.tzOffset;
	fields->seconds = seconds;
	return TCL_OK;
    }

    /*
     * Check cacheable back-conversion could be used
     * (last-period UTC2Local cache within the same TZ)
     */
    seconds = fields->localSeconds - dataPtr->UTC2Local.tzOffset;
    if ( timezoneObj == dataPtr->UTC2Local.timezoneObj
      && ( seconds == dataPtr->UTC2Local.seconds
	|| ( seconds >= dataPtr->UTC2Local.rangesVal[0]
	  && seconds <	dataPtr->UTC2Local.rangesVal[1])
      )
      && changeover == dataPtr->UTC2Local.changeover
    ) {
	/* the same time zone and offset (UTC time inside the last minute) */
	fields->tzOffset = dataPtr->UTC2Local.tzOffset;
	fields->seconds = seconds;
	return TCL_OK;
    }

    /*
     * Unpack the tz data.
     */

    tzdata = ClockGetTZData(clientData, interp, timezoneObj);
    if (tzdata == NULL) {
	return TCL_ERROR;
    }

    if (TclListObjGetElements(interp, tzdata, &rowc, &rowv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Special case: If the time zone is :localtime, the tzdata will be empty.
     * Use 'mktime' to convert the time to local
     */

    if (rowc == 0) {
	dataPtr->Local2UTC.rangesVal[0] = 0;
	dataPtr->Local2UTC.rangesVal[1] = 0;

	if (ConvertLocalToUTCUsingC(interp, fields, changeover) != TCL_OK) {
	    return TCL_ERROR;
	};
    } else {
	if (ConvertLocalToUTCUsingTable(interp, fields, rowc, rowv, 
		dataPtr->Local2UTC.rangesVal) != TCL_OK) {
	    return TCL_ERROR;
	};
    }

    /* Cache the last conversion */
    Tcl_SetObjRef(dataPtr->Local2UTC.timezoneObj, timezoneObj);
    dataPtr->Local2UTC.localSeconds = fields->localSeconds;
    dataPtr->Local2UTC.changeover = changeover;
    dataPtr->Local2UTC.tzOffset = fields->tzOffset;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertLocalToUTCUsingTable --
 *
 *	Converts a time (in a TclDateFields structure) from local time in a
 *	given time zone to UTC.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Stores an error message in the interpreter if an error occurs; if
 *	successful, stores the 'seconds' field in 'fields.
 *
 *----------------------------------------------------------------------
 */

static int
ConvertLocalToUTCUsingTable(
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Time to convert, with 'seconds' filled in */
    int rowc,			/* Number of points at which time changes */
    Tcl_Obj *const rowv[],	/* Points at which time changes */
    Tcl_WideInt rangesVal[2])	/* Return bounds for time period */
{
    Tcl_Obj *row;
    int cellc;
    Tcl_Obj **cellv;
    int have[8];
    int nHave = 0;
    int i;
    int found;

    /*
     * Perform an initial lookup assuming that local == UTC, and locate the
     * last time conversion prior to that time. Get the offset from that row,
     * and look up again. Continue until we find an offset that we found
     * before. This definition, rather than "the same offset" ensures that we
     * don't enter an endless loop, as would otherwise happen when trying to
     * convert a non-existent time such as 02:30 during the US Spring Daylight
     * Saving Time transition.
     */

    found = 0;
    fields->tzOffset = 0;
    fields->seconds = fields->localSeconds;
    while (!found) {
	row = LookupLastTransition(interp, fields->seconds, rowc, rowv,
		    rangesVal);
	if ((row == NULL)
		|| TclListObjGetElements(interp, row, &cellc,
		    &cellv) != TCL_OK
		|| TclGetIntFromObj(interp, cellv[1],
		    &fields->tzOffset) != TCL_OK) {
	    return TCL_ERROR;
	}
	found = 0;
	for (i = 0; !found && i < nHave; ++i) {
	    if (have[i] == fields->tzOffset) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    if (nHave == 8) {
		Tcl_Panic("loop in ConvertLocalToUTCUsingTable");
	    }
	    have[nHave++] = fields->tzOffset;
	}
	fields->seconds = fields->localSeconds - fields->tzOffset;
    }
    fields->tzOffset = have[i];
    fields->seconds = fields->localSeconds - fields->tzOffset;

#if 0
    /* currently unused, test purposes only */
    /*
     * Convert back from UTC, if local times are different - wrong local time
     * (local time seems to be in between DST-hole).
     */
    if (fields->tzOffset) {

	int corrOffset;
	Tcl_WideInt backCompVal;
	/* check DST-hole interval contains UTC time */
	TclGetWideIntFromObj(NULL, cellv[0], &backCompVal);
	if ( fields->seconds >= backCompVal - fields->tzOffset 
	  && fields->seconds <= backCompVal + fields->tzOffset
	) {
	    row = LookupLastTransition(interp, fields->seconds, rowc, rowv);
	    if (row == NULL ||
		    TclListObjGetElements(interp, row, &cellc, &cellv) != TCL_OK ||
		    TclGetIntFromObj(interp, cellv[1], &corrOffset) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (fields->localSeconds != fields->seconds + corrOffset) {
		Tcl_Panic("wrong local time %ld by LocalToUTC conversion,"
		    " local time seems to be in between DST-hole", 
		    fields->localSeconds);
		/* correcting offset * /
		fields->tzOffset -= corrOffset;
		fields->seconds += fields->tzOffset;
		*/
	    }
	}
    }
#endif

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertLocalToUTCUsingC --
 *
 *	Converts a time from local wall clock to UTC when the local time zone
 *	cannot be determined. Uses 'mktime' to do the job.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Stores an error message in the interpreter if an error occurs; if
 *	successful, stores the 'seconds' field in 'fields.
 *
 *----------------------------------------------------------------------
 */

static int
ConvertLocalToUTCUsingC(
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Time to convert, with 'seconds' filled in */
    int changeover)		/* Julian Day of the Gregorian transition */
{
    struct tm timeVal;
    int localErrno;
    int secondOfDay;
    Tcl_WideInt jsec;

    /*
     * Convert the given time to a date.
     */

    jsec = fields->localSeconds + JULIAN_SEC_POSIX_EPOCH;
    fields->julianDay = (int) (jsec / SECONDS_PER_DAY);
    secondOfDay = (int)(jsec % SECONDS_PER_DAY);
    if (secondOfDay < 0) {
	secondOfDay += SECONDS_PER_DAY;
	fields->julianDay--;
    }
    GetGregorianEraYearDay(fields, changeover);
    GetMonthDay(fields);

    /*
     * Convert the date/time to a 'struct tm'.
     */

    timeVal.tm_year = fields->year - 1900;
    timeVal.tm_mon = fields->month - 1;
    timeVal.tm_mday = fields->dayOfMonth;
    timeVal.tm_hour = (secondOfDay / 3600) % 24;
    timeVal.tm_min = (secondOfDay / 60) % 60;
    timeVal.tm_sec = secondOfDay % 60;
    timeVal.tm_isdst = -1;
    timeVal.tm_wday = -1;
    timeVal.tm_yday = -1;

    /*
     * Get local time. It is rumored that mktime is not thread safe on some
     * platforms, so seize a mutex before attempting this.
     */

    TzsetIfNecessary();
    Tcl_MutexLock(&clockMutex);
    errno = 0;
    fields->seconds = (Tcl_WideInt) mktime(&timeVal);
    localErrno = errno;
    Tcl_MutexUnlock(&clockMutex);

    /*
     * If conversion fails, report an error.
     */

    if (localErrno != 0
	    || (fields->seconds == -1 && timeVal.tm_yday == -1)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"time value too large/small to represent", -1));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertUTCToLocal --
 *
 *	Converts a time (in a TclDateFields structure) from UTC to local time.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Populates the 'tzName' and 'tzOffset' fields.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
ConvertUTCToLocal(
    ClientData clientData,	/* Client data of the interpreter */
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Fields of the time */
    Tcl_Obj *timezoneObj,	/* Time zone */
    int changeover)		/* Julian Day of the Gregorian transition */
{
    ClockClientData *dataPtr = clientData;
    Tcl_Obj *tzdata;		/* Time zone data */
    int rowc;			/* Number of rows in tzdata */
    Tcl_Obj **rowv;		/* Pointers to the rows */

    /* fast phase-out for shared GMT-object (don't need to convert UTC 2 UTC) */
    if (timezoneObj == dataPtr->GMTSetupTimeZone 
	&& dataPtr->GMTSetupTimeZone != NULL 
	&& dataPtr->GMTSetupTZData != NULL
    ) {
	fields->localSeconds = fields->seconds;
	fields->tzOffset = 0;
	if ( TclListObjGetElements(interp, dataPtr->GMTSetupTZData, &rowc, &rowv) != TCL_OK
	  || Tcl_ListObjIndex(interp, rowv[0], 3, &fields->tzName) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(fields->tzName);
	return TCL_OK;
    }

    /*
     * Check cacheable conversion could be used
     * (last-period UTC2Local cache within the same TZ)
     */
    if ( timezoneObj == dataPtr->UTC2Local.timezoneObj
      && ( fields->seconds == dataPtr->UTC2Local.seconds
	|| ( fields->seconds >= dataPtr->UTC2Local.rangesVal[0]
	  && fields->seconds <	dataPtr->UTC2Local.rangesVal[1])
      )
      && changeover == dataPtr->UTC2Local.changeover
    ) {
	/* the same time zone and offset (UTC time inside the last minute) */
	Tcl_SetObjRef(fields->tzName, dataPtr->UTC2Local.tzName);
	fields->tzOffset = dataPtr->UTC2Local.tzOffset;
	fields->localSeconds = fields->seconds + fields->tzOffset;
	return TCL_OK;
    }

    /*
     * Unpack the tz data.
     */

    tzdata = ClockGetTZData(clientData, interp, timezoneObj);
    if (tzdata == NULL) {
	return TCL_ERROR;
    }

    if (TclListObjGetElements(interp, tzdata, &rowc, &rowv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Special case: If the time zone is :localtime, the tzdata will be empty.
     * Use 'localtime' to convert the time to local
     */

    if (rowc == 0) {
	dataPtr->UTC2Local.rangesVal[0] = 0;
	dataPtr->UTC2Local.rangesVal[1] = 0;

	if (ConvertUTCToLocalUsingC(interp, fields, changeover) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	if (ConvertUTCToLocalUsingTable(interp, fields, rowc, rowv, 
		dataPtr->UTC2Local.rangesVal) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /* Cache the last conversion */
    Tcl_SetObjRef(dataPtr->UTC2Local.timezoneObj, timezoneObj);
    dataPtr->UTC2Local.seconds = fields->seconds;
    dataPtr->UTC2Local.changeover = changeover;
    dataPtr->UTC2Local.tzOffset = fields->tzOffset;
    Tcl_SetObjRef(dataPtr->UTC2Local.tzName, fields->tzName);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertUTCToLocalUsingTable --
 *
 *	Converts UTC to local time, given a table of transition points
 *
 * Results:
 *	Returns a standard Tcl result
 *
 * Side effects:
 *	On success, fills fields->tzName, fields->tzOffset and
 *	fields->localSeconds. On failure, places an error message in the
 *	interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
ConvertUTCToLocalUsingTable(
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Fields of the date */
    int rowc,			/* Number of rows in the conversion table
				 * (>= 1) */
    Tcl_Obj *const rowv[],	/* Rows of the conversion table */
    Tcl_WideInt rangesVal[2])	/* Return bounds for time period */
{
    Tcl_Obj *row;		/* Row containing the current information */
    int cellc;			/* Count of cells in the row (must be 4) */
    Tcl_Obj **cellv;		/* Pointers to the cells */

    /*
     * Look up the nearest transition time.
     */

    row = LookupLastTransition(interp, fields->seconds, rowc, rowv, rangesVal);
    if (row == NULL ||
	    TclListObjGetElements(interp, row, &cellc, &cellv) != TCL_OK ||
	    TclGetIntFromObj(interp, cellv[1], &fields->tzOffset) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Convert the time.
     */

    Tcl_SetObjRef(fields->tzName, cellv[3]);
    fields->localSeconds = fields->seconds + fields->tzOffset;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertUTCToLocalUsingC --
 *
 *	Converts UTC to localtime in cases where the local time zone is not
 *	determinable, using the C 'localtime' function to do it.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	On success, fills fields->tzName, fields->tzOffset and
 *	fields->localSeconds. On failure, places an error message in the
 *	interpreter result.
 *
 *----------------------------------------------------------------------
 */

static int
ConvertUTCToLocalUsingC(
    Tcl_Interp *interp,		/* Tcl interpreter */
    TclDateFields *fields,	/* Time to convert, with 'seconds' filled in */
    int changeover)		/* Julian Day of the Gregorian transition */
{
    time_t tock;
    struct tm *timeVal;		/* Time after conversion */
    int diff;			/* Time zone diff local-Greenwich */
    char buffer[8];		/* Buffer for time zone name */

    /*
     * Use 'localtime' to determine local year, month, day, time of day.
     */

    tock = (time_t) fields->seconds;
    if ((Tcl_WideInt) tock != fields->seconds) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"number too large to represent as a Posix time", -1));
	Tcl_SetErrorCode(interp, "CLOCK", "argTooLarge", NULL);
	return TCL_ERROR;
    }
    TzsetIfNecessary();
    timeVal = ThreadSafeLocalTime(&tock);
    if (timeVal == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"localtime failed (clock value may be too "
		"large/small to represent)", -1));
	Tcl_SetErrorCode(interp, "CLOCK", "localtimeFailed", NULL);
	return TCL_ERROR;
    }

    /*
     * Fill in the date in 'fields' and use it to derive Julian Day.
     */

    fields->era = CE;
    fields->year = timeVal->tm_year + 1900;
    fields->month = timeVal->tm_mon + 1;
    fields->dayOfMonth = timeVal->tm_mday;
    GetJulianDayFromEraYearMonthDay(fields, changeover);

    /*
     * Convert that value to seconds.
     */

    fields->localSeconds = (((fields->julianDay * (Tcl_WideInt) 24
	    + timeVal->tm_hour) * 60 + timeVal->tm_min) * 60
	    + timeVal->tm_sec) - JULIAN_SEC_POSIX_EPOCH;

    /*
     * Determine a time zone offset and name; just use +hhmm for the name.
     */

    diff = (int) (fields->localSeconds - fields->seconds);
    fields->tzOffset = diff;
    if (diff < 0) {
	*buffer = '-';
	diff = -diff;
    } else {
	*buffer = '+';
    }
    sprintf(buffer+1, "%02d", diff / 3600);
    diff %= 3600;
    sprintf(buffer+3, "%02d", diff / 60);
    diff %= 60;
    if (diff > 0) {
	sprintf(buffer+5, "%02d", diff);
    }
    Tcl_SetObjRef(fields->tzName, Tcl_NewStringObj(buffer, -1));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupLastTransition --
 *
 *	Given a UTC time and a tzdata array, looks up the last transition on
 *	or before the given time.
 *
 * Results:
 *	Returns a pointer to the row, or NULL if an error occurs.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE Tcl_Obj *
LookupLastTransition(
    Tcl_Interp *interp,		/* Interpreter for error messages */
    Tcl_WideInt tick,		/* Time from the epoch */
    int rowc,			/* Number of rows of tzdata */
    Tcl_Obj *const *rowv,	/* Rows in tzdata */
    Tcl_WideInt rangesVal[2])	/* Return bounds for time period */
{
    int l = 0;
    int u;
    Tcl_Obj *compObj;
    Tcl_WideInt compVal, fromVal = tick, toVal = tick;

    /*
     * Examine the first row to make sure we're in bounds.
     */

    if (Tcl_ListObjIndex(interp, rowv[0], 0, &compObj) != TCL_OK
	    || TclGetWideIntFromObj(interp, compObj, &compVal) != TCL_OK) {
	return NULL;
    }

    /*
     * Bizarre case - first row doesn't begin at MIN_WIDE_INT. Return it
     * anyway.
     */

    if (tick < compVal) {
	goto done;
    }

    /*
     * Binary-search to find the transition.
     */

    u = rowc-1;
    while (l < u) {
	int m = (l + u + 1) / 2;

	if (Tcl_ListObjIndex(interp, rowv[m], 0, &compObj) != TCL_OK ||
		TclGetWideIntFromObj(interp, compObj, &compVal) != TCL_OK) {
	    return NULL;
	}
	if (tick >= compVal) {
	    l = m;
	    fromVal = compVal;
	} else {
	    u = m-1;
	    toVal = compVal;
	}
    }

done:

    if (rangesVal) {
	rangesVal[0] = fromVal;
	rangesVal[1] = toVal;
    }
    return rowv[l];
}

/*
 *----------------------------------------------------------------------
 *
 * GetYearWeekDay --
 *
 *	Given a date with Julian Calendar Day, compute the year, week, and day
 *	in the ISO8601 calendar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores 'iso8601Year', 'iso8601Week' and 'dayOfWeek' in the date
 *	fields.
 *
 *----------------------------------------------------------------------
 */

static void
GetYearWeekDay(
    TclDateFields *fields,	/* Date to convert, must have 'julianDay' */
    int changeover)		/* Julian Day Number of the Gregorian
				 * transition */
{
    TclDateFields temp;
    int dayOfFiscalYear;

    temp.tzName = NULL;

    /*
     * Find the given date, minus three days, plus one year. That date's
     * iso8601 year is an upper bound on the ISO8601 year of the given date.
     */

    temp.julianDay = fields->julianDay - 3;
    GetGregorianEraYearDay(&temp, changeover);
    if (temp.era == BCE) {
	temp.iso8601Year = temp.year - 1;
    } else {
	temp.iso8601Year = temp.year + 1;
    }
    temp.iso8601Week = 1;
    temp.dayOfWeek = 1;
    GetJulianDayFromEraYearWeekDay(&temp, changeover);

    /*
     * temp.julianDay is now the start of an ISO8601 year, either the one
     * corresponding to the given date, or the one after. If we guessed high,
     * move one year earlier
     */

    if (fields->julianDay < temp.julianDay) {
	if (temp.era == BCE) {
	    temp.iso8601Year += 1;
	} else {
	    temp.iso8601Year -= 1;
	}
	GetJulianDayFromEraYearWeekDay(&temp, changeover);
    }

    fields->iso8601Year = temp.iso8601Year;
    dayOfFiscalYear = fields->julianDay - temp.julianDay;
    fields->iso8601Week = (dayOfFiscalYear / 7) + 1;
    fields->dayOfWeek = (dayOfFiscalYear + 1) % 7;
    if (fields->dayOfWeek < 1) {
	fields->dayOfWeek += 7;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetGregorianEraYearDay --
 *
 *	Given a Julian Day Number, extracts the year and day of the year and
 *	puts them into TclDateFields, along with the era (BCE or CE) and a
 *	flag indicating whether the date is Gregorian or Julian.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores 'era', 'gregorian', 'year', and 'dayOfYear'.
 *
 *----------------------------------------------------------------------
 */

static void
GetGregorianEraYearDay(
    TclDateFields *fields,	/* Date fields containing 'julianDay' */
    int changeover)		/* Gregorian transition date */
{
    int jday = fields->julianDay;
    int day;
    int year;
    int n;

    if (jday >= changeover) {
	/*
	 * Gregorian calendar.
	 */

	fields->gregorian = 1;
	year = 1;

	/*
	 * n = Number of 400-year cycles since 1 January, 1 CE in the
	 * proleptic Gregorian calendar. day = remaining days.
	 */

	day = jday - JDAY_1_JAN_1_CE_GREGORIAN;
	n = day / FOUR_CENTURIES;
	day %= FOUR_CENTURIES;
	if (day < 0) {
	    day += FOUR_CENTURIES;
	    n--;
	}
	year += 400 * n;

	/*
	 * n = number of centuries since the start of (year);
	 * day = remaining days
	 */

	n = day / ONE_CENTURY_GREGORIAN;
	day %= ONE_CENTURY_GREGORIAN;
	if (n > 3) {
	    /*
	     * 31 December in the last year of a 400-year cycle.
	     */

	    n = 3;
	    day += ONE_CENTURY_GREGORIAN;
	}
	year += 100 * n;
    } else {
	/*
	 * Julian calendar.
	 */

	fields->gregorian = 0;
	year = 1;
	day = jday - JDAY_1_JAN_1_CE_JULIAN;
    }

    /*
     * n = number of 4-year cycles; days = remaining days.
     */

    n = day / FOUR_YEARS;
    day %= FOUR_YEARS;
    if (day < 0) {
	day += FOUR_YEARS;
	n--;
    }
    year += 4 * n;

    /*
     * n = number of years; days = remaining days.
     */

    n = day / ONE_YEAR;
    day %= ONE_YEAR;
    if (n > 3) {
	/*
	 * 31 December of a leap year.
	 */

	n = 3;
	day += 365;
    }
    year += n;

    /*
     * store era/year/day back into fields.
     */

    if (year <= 0) {
	fields->era = BCE;
	fields->year = 1 - year;
    } else {
	fields->era = CE;
	fields->year = year;
    }
    fields->dayOfYear = day + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMonthDay --
 *
 *	Given a date as year and day-of-year, find month and day.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores 'month' and 'dayOfMonth' in the 'fields' structure.
 *
 *----------------------------------------------------------------------
 */

static void
GetMonthDay(
    TclDateFields *fields)	/* Date to convert */
{
    int day = fields->dayOfYear;
    int month;
    const int *h = hath[IsGregorianLeapYear(fields)];

    for (month = 0; month < 12 && day > h[month]; ++month) {
	day -= h[month];
    }
    fields->month = month+1;
    fields->dayOfMonth = day;
}

/*
 *----------------------------------------------------------------------
 *
 * GetJulianDayFromEraYearWeekDay --
 *
 *	Given a TclDateFields structure containing era, ISO8601 year, ISO8601
 *	week, and day of week, computes the Julian Day Number.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores 'julianDay' in the fields.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
GetJulianDayFromEraYearWeekDay(
    TclDateFields *fields,	/* Date to convert */
    int changeover)		/* Julian Day Number of the Gregorian
				 * transition */
{
    int firstMonday;		/* Julian day number of week 1, day 1 in the
				 * given year */
    TclDateFields firstWeek;

    firstWeek.tzName = NULL;

    /*
     * Find January 4 in the ISO8601 year, which will always be in week 1.
     */

    firstWeek.era = fields->era;
    firstWeek.year = fields->iso8601Year;
    firstWeek.month = 1;
    firstWeek.dayOfMonth = 4;
    GetJulianDayFromEraYearMonthDay(&firstWeek, changeover);

    /*
     * Find Monday of week 1.
     */

    firstMonday = WeekdayOnOrBefore(1, firstWeek.julianDay);

    /*
     * Advance to the given week and day.
     */

    fields->julianDay = firstMonday + 7 * (fields->iso8601Week - 1)
	    + fields->dayOfWeek - 1;
}

/*
 *----------------------------------------------------------------------
 *
 * GetJulianDayFromEraYearMonthDay --
 *
 *	Given era, year, month, and dayOfMonth (in TclDateFields), and the
 *	Gregorian transition date, computes the Julian Day Number.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores day number in 'julianDay'
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
GetJulianDayFromEraYearMonthDay(
    TclDateFields *fields,	/* Date to convert */
    int changeover)		/* Gregorian transition date as a Julian Day */
{
    int year, ym1, month, mm1, q, r, ym1o4, ym1o100, ym1o400;

    if (fields->era == BCE) {
	year = 1 - fields->year;
    } else {
	year = fields->year;
    }

    /*
     * Reduce month modulo 12.
     */

    month = fields->month;
    mm1 = month - 1;
    q = mm1 / 12;
    r = (mm1 % 12);
    if (r < 0) {
	r += 12;
	q -= 1;
    }
    year += q;
    month = r + 1;
    ym1 = year - 1;

    /*
     * Adjust the year after reducing the month.
     */

    fields->gregorian = 1;
    if (year < 1) {
	fields->era = BCE;
	fields->year = 1-year;
    } else {
	fields->era = CE;
	fields->year = year;
    }

    /*
     * Try an initial conversion in the Gregorian calendar.
     */

#if 0 /* BUG http://core.tcl.tk/tcl/tktview?name=da340d4f32 */
    ym1o4 = ym1 / 4;
#else
    /*
     * Have to make sure quotient is truncated towards 0 when negative.
     * See above bug for details. The casts are necessary.
     */
    if (ym1 >= 0)
	ym1o4 = ym1 / 4;
    else {
	ym1o4 = - (int) (((unsigned int) -ym1) / 4);
    }
#endif
    if (ym1 % 4 < 0) {
	ym1o4--;
    }
    ym1o100 = ym1 / 100;
    if (ym1 % 100 < 0) {
	ym1o100--;
    }
    ym1o400 = ym1 / 400;
    if (ym1 % 400 < 0) {
	ym1o400--;
    }
    fields->julianDay = JDAY_1_JAN_1_CE_GREGORIAN - 1
	    + fields->dayOfMonth
	    + daysInPriorMonths[IsGregorianLeapYear(fields)][month - 1]
	    + (ONE_YEAR * ym1)
	    + ym1o4
	    - ym1o100
	    + ym1o400;

    /*
     * If the resulting date is before the Gregorian changeover, convert in
     * the Julian calendar instead.
     */

    if (fields->julianDay < changeover) {
	fields->gregorian = 0;
	fields->julianDay = JDAY_1_JAN_1_CE_JULIAN - 1
		+ fields->dayOfMonth
		+ daysInPriorMonths[year%4 == 0][month - 1]
		+ (365 * ym1)
		+ ym1o4;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetJulianDayFromEraYearDay --
 *
 *	Given era, year, and dayOfYear (in TclDateFields), and the
 *	Gregorian transition date, computes the Julian Day Number.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores day number in 'julianDay'
 *
 *----------------------------------------------------------------------
 */


MODULE_SCOPE void
GetJulianDayFromEraYearDay(
    TclDateFields *fields,	/* Date to convert */
    int changeover)		/* Gregorian transition date as a Julian Day */
{
    int year, ym1;

    /* Get absolute year number from the civil year */
    if (fields->era == BCE) {
	year = 1 - fields->year;
    } else {
	year = fields->year;
    }

    ym1 = year - 1;

    /* Try the Gregorian calendar first. */
    fields->gregorian = 1;
    fields->julianDay =
	1721425
	+ fields->dayOfYear
	+ ( 365 * ym1 )
	+ ( ym1 / 4 )
	- ( ym1 / 100 )
	+ ( ym1 / 400 );

    /* If the date is before the Gregorian change, use the Julian calendar. */

    if ( fields->julianDay < changeover ) {
	fields->gregorian = 0;
	fields->julianDay =
	    1721423
	    + fields->dayOfYear
	    + ( 365 * ym1 )
	    + ( ym1 / 4 );
    }
}
/*
 *----------------------------------------------------------------------
 *
 * IsGregorianLeapYear --
 *
 *	Tests whether a given year is a leap year, in either Julian or
 *	Gregorian calendar.
 *
 * Results:
 *	Returns 1 for a leap year, 0 otherwise.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
IsGregorianLeapYear(
    TclDateFields *fields)	/* Date to test */
{
    int year = fields->year;

    if (fields->era == BCE) {
	year = 1 - year;
    }
    if (year%4 != 0) {
	return 0;
    } else if (!(fields->gregorian)) {
	return 1;
    } else if (year%400 == 0) {
	return 1;
    } else if (year%100 == 0) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * WeekdayOnOrBefore --
 *
 *	Finds the Julian Day Number of a given day of the week that falls on
 *	or before a given date, expressed as Julian Day Number.
 *
 * Results:
 *	Returns the Julian Day Number
 *
 *----------------------------------------------------------------------
 */

static int
WeekdayOnOrBefore(
    int dayOfWeek,		/* Day of week; Sunday == 0 or 7 */
    int julianDay)		/* Reference date */
{
    int k = (dayOfWeek + 6) % 7;
    if (k < 0) {
	k += 7;
    }
    return julianDay - ((julianDay - k) % 7);
}

/*
 *----------------------------------------------------------------------
 *
 * ClockGetenvObjCmd --
 *
 *	Tcl command that reads an environment variable from the system
 *
 * Usage:
 *	::tcl::clock::getEnv NAME
 *
 * Parameters:
 *	NAME - Name of the environment variable desired
 *
 * Results:
 *	Returns a standard Tcl result. Returns an error if the variable does
 *	not exist, with a message left in the interpreter. Returns TCL_OK and
 *	the value of the variable if the variable does exist,
 *
 *----------------------------------------------------------------------
 */

int
ClockGetenvObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    const char *varName;
    const char *varValue;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "name");
	return TCL_ERROR;
    }
    varName = TclGetString(objv[1]);
    varValue = getenv(varName);
    if (varValue == NULL) {
	varValue = "";
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(varValue, -1));
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
ThreadSafeLocalTime(
    const time_t *timePtr)	/* Pointer to the number of seconds since the
				 * local system's epoch */
{
    /*
     * Get a thread-local buffer to hold the returned time.
     */

    struct tm *tmPtr = Tcl_GetThreadData(&tmKey, sizeof(struct tm));
#ifdef HAVE_LOCALTIME_R
    localtime_r(timePtr, tmPtr);
#else
    struct tm *sysTmPtr;

    Tcl_MutexLock(&clockMutex);
    sysTmPtr = localtime(timePtr);
    if (sysTmPtr == NULL) {
	Tcl_MutexUnlock(&clockMutex);
	return NULL;
    }
    memcpy(tmPtr, localtime(timePtr), sizeof(struct tm));
    Tcl_MutexUnlock(&clockMutex);
#endif
    return tmPtr;
}

/*----------------------------------------------------------------------
 *
 * ClockClicksObjCmd --
 *
 *	Returns a high-resolution counter.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock clicks' Tcl command. Refer to the user
 * documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
ClockClicksObjCmd(
    ClientData clientData,	/* Client data is unused */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter values */
{
    static const char *const clicksSwitches[] = {
	"-milliseconds", "-microseconds", NULL
    };
    enum ClicksSwitch {
	CLICKS_MILLIS, CLICKS_MICROS, CLICKS_NATIVE
    };
    int index = CLICKS_NATIVE;
    Tcl_Time now;
    Tcl_WideInt clicks = 0;

    switch (objc) {
    case 1:
	break;
    case 2:
	if (Tcl_GetIndexFromObj(interp, objv[1], clicksSwitches, "option", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	break;
    default:
	Tcl_WrongNumArgs(interp, 0, NULL, "clock clicks ?-switch?");
	return TCL_ERROR;
    }

    switch (index) {
    case CLICKS_MILLIS:
	Tcl_GetTime(&now);
	clicks = (Tcl_WideInt) now.sec * 1000 + now.usec / 1000;
	break;
    case CLICKS_NATIVE:
#ifdef TCL_WIDE_CLICKS
	clicks = TclpGetWideClicks();
#else
	clicks = (Tcl_WideInt) TclpGetClicks();
#endif
	break;
    case CLICKS_MICROS:
	clicks = TclpGetMicroseconds();
	break;
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(clicks));
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockMillisecondsObjCmd -
 *
 *	Returns a count of milliseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock milliseconds' Tcl command. Refer to the
 * user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
ClockMillisecondsObjCmd(
    ClientData clientData,	/* Client data is unused */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter values */
{
    Tcl_Time now;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock milliseconds");
	return TCL_ERROR;
    }
    Tcl_GetTime(&now);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt)
	    now.sec * 1000 + now.usec / 1000));
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockMicrosecondsObjCmd -
 *
 *	Returns a count of microseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock microseconds' Tcl command. Refer to the
 * user documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
ClockMicrosecondsObjCmd(
    ClientData clientData,	/* Client data is unused */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter values */
{
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock microseconds");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(TclpGetMicroseconds()));
    return TCL_OK;
}

static inline void
ClockInitFmtScnArgs(
    ClientData clientData,
    Tcl_Interp *interp,
    ClockFmtScnCmdArgs *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->clientData = clientData;
    opts->interp = interp;
}

/*
 *-----------------------------------------------------------------------------
 *
 * ClockParseFmtScnArgs --
 *
 *	Parses the arguments for [clock scan] and [clock format].
 *
 * Results:
 *	Returns a standard Tcl result, and stores parsed options
 *	(format, the locale, timezone and base) in structure "opts".
 *
 *-----------------------------------------------------------------------------
 */

#define CLC_FMT_ARGS	(0)
#define CLC_SCN_ARGS	(1 << 0)
#define CLC_ADD_ARGS	(1 << 1)

static int
ClockParseFmtScnArgs(
    register
    ClockFmtScnCmdArgs *opts,	/* Result vector: format, locale, timezone... */
    TclDateFields      *date,	/* Extracted date-time corresponding base 
				 * (by scan or add) resp. clockval (by format) */
    int objc,		    /* Parameter count */
    Tcl_Obj *const objv[],	/* Parameter vector */
    int		flags		/* Flags, differentiates between format, scan, add */
) {
    Tcl_Interp	    *interp =  opts->interp;
    ClockClientData *dataPtr = opts->clientData;
    int gmtFlag = 0;
    static const char *const options[] = {
	"-format",	"-gmt",		"-locale",
	"-timezone",	"-base",	NULL
    };
    enum optionInd {
	CLC_ARGS_FORMAT,    CLC_ARGS_GMT,	CLC_ARGS_LOCALE,
	CLC_ARGS_TIMEZONE,  CLC_ARGS_BASE
    };
    int optionIndex;		/* Index of an option. */
    int saw = 0;		/* Flag == 1 if option was seen already. */
    int i;
    Tcl_WideInt baseVal;	/* Base time, expressed in seconds from the Epoch */

    /* clock value (as current base) */
    if ( !(flags & (CLC_SCN_ARGS)) ) {
	opts->baseObj = objv[1];
	saw |= (1 << CLC_ARGS_BASE);
    }

    /*
     * Extract values for the keywords.
     */

    for (i = 2; i < objc; i+=2) {
	/* bypass integers (offsets) by "clock add" */
	if (flags & CLC_ADD_ARGS) {
	    Tcl_WideInt num;
	    if (TclGetWideIntFromObj(NULL, objv[i], &num) == TCL_OK) {
		continue;
	    }
	}
	/* get option */
	if (Tcl_GetIndexFromObj(interp, objv[i], options, 
	    "option", 0, &optionIndex) != TCL_OK) {
	    goto badOption;
	}
	/* if already specified */
	if (saw & (1 << optionIndex)) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad option \"%s\": doubly present", 
		TclGetString(objv[i]))
	    );
	    goto badOption;
	}
	switch (optionIndex) {
	case CLC_ARGS_FORMAT:
	    if (flags & CLC_ADD_ARGS) {
		goto badOptionMsg;
	    }
	    opts->formatObj = objv[i+1];
	    break;
	case CLC_ARGS_GMT:
	    if (Tcl_GetBooleanFromObj(interp, objv[i+1], &gmtFlag) != TCL_OK){
		return TCL_ERROR;
	    }
	    break;
	case CLC_ARGS_LOCALE:
	    opts->localeObj = objv[i+1];
	    break;
	case CLC_ARGS_TIMEZONE:
	    opts->timezoneObj = objv[i+1];
	    break;
	case CLC_ARGS_BASE:
	    if ( !(flags & (CLC_SCN_ARGS)) ) {
		goto badOptionMsg;
	    }
	    opts->baseObj = objv[i+1];
	    break;
	}
	saw |= (1 << optionIndex);
    }

    /*
     * Check options.
     */

    if ((saw & (1 << CLC_ARGS_GMT))
	    && (saw & (1 << CLC_ARGS_TIMEZONE))) {
	Tcl_SetResult(interp, (char*)"cannot use -gmt and -timezone in same call", TCL_STATIC);
	Tcl_SetErrorCode(interp, "CLOCK", "gmtWithTimezone", NULL);
	return TCL_ERROR;
    }
    if (gmtFlag) {
	opts->timezoneObj = dataPtr->literals[LIT_GMT];
    }

    /* If time zone not specified use system time zone */

    if ( opts->timezoneObj == NULL
      || TclGetString(opts->timezoneObj) == NULL 
      || opts->timezoneObj->length == 0
    ) {
	opts->timezoneObj = ClockGetSystemTimeZone(opts->clientData, interp);
	if (opts->timezoneObj == NULL) {
	    return TCL_ERROR;
	}
    }

    /* Setup timezone (normalize object if needed and load TZ on demand) */

    opts->timezoneObj = ClockSetupTimeZone(opts->clientData, interp, opts->timezoneObj);
    if (opts->timezoneObj == NULL) {
	return TCL_ERROR;
    }

    /* Base (by scan or add) or clock value (by format) */

    if (opts->baseObj != NULL) {
	register Tcl_Obj *baseObj = opts->baseObj;
	/* bypass integer recognition if looks like option "-now" */
	if (
	    (baseObj->length == 4 && baseObj->bytes && *(baseObj->bytes+1) == 'n') ||
	    TclGetWideIntFromObj(NULL, baseObj, &baseVal) != TCL_OK
	) {

	    /* we accept "-now" as current date-time */
	    static const char *const nowOpts[] = {
		"-now", NULL
	    };
	    int idx;
	    if (Tcl_GetIndexFromObj(NULL, baseObj, nowOpts, "seconds or -now",
		    TCL_EXACT, &idx) == TCL_OK
	    ) {
		goto baseNow;
	    }

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "expected integer but got \"%s\"",
		    Tcl_GetString(baseObj)));
	    Tcl_SetErrorCode(interp, "TCL", "VALUE", "INTEGER", NULL);
	    i = 1;
	    goto badOption;
	}
	/*
	 * seconds could be an unsigned number that overflowed. Make sure
	 * that it isn't.
	 */

	if (baseObj->typePtr == &tclBignumType) {
	    Tcl_SetObjResult(interp, dataPtr->literals[LIT_INTEGER_VALUE_TOO_LARGE]);
	    return TCL_ERROR;
	}

    } else {

baseNow:
	{
	    Tcl_Time now;
	    Tcl_GetTime(&now);
	    baseVal = (Tcl_WideInt) now.sec;
	}
    }

    /*
     * Extract year, month and day from the base time for the parser to use as
     * defaults
     */

    /* check base fields already cached (by TZ, last-second cache) */
    if ( dataPtr->lastBase.timezoneObj == opts->timezoneObj
      && dataPtr->lastBase.Date.seconds == baseVal) {
	memcpy(date, &dataPtr->lastBase.Date, ClockCacheableDateFieldsSize);
    } else {
	/* extact fields from base */
	date->seconds = baseVal;
	if (ClockGetDateFields(opts->clientData, interp, date, opts->timezoneObj,
	      GREGORIAN_CHANGE_DATE) != TCL_OK) { /* TODO - GREGORIAN_CHANGE_DATE should be locale-dependent */
	    return TCL_ERROR;
	}
	/* cache last base */
	memcpy(&dataPtr->lastBase.Date, date, ClockCacheableDateFieldsSize);
	Tcl_SetObjRef(dataPtr->lastBase.timezoneObj, opts->timezoneObj);
    }

    return TCL_OK;

badOptionMsg: 

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	"bad option \"%s\": unexpected for command \"%s\"",
	TclGetString(objv[i]), TclGetString(objv[0]))
    );

badOption:

    Tcl_SetErrorCode(interp, "CLOCK", "badOption",
	    i < objc ? Tcl_GetString(objv[i]) : NULL, NULL);
    
    return TCL_ERROR;
}

/*----------------------------------------------------------------------
 *
 * ClockFormatObjCmd -- , clock format --
 *
 *	This function is invoked to process the Tcl "clock format" command.
 *
 *	Formats a count of seconds since the Posix Epoch as a time of day.
 *
 *	The 'clock format' command formats times of day for output.  Refer 
 *	to the user documentation to see what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ClockFormatObjCmd(
    ClientData clientData,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const objv[])	/* Parameter values */
{
    ClockClientData *dataPtr = clientData;

    int ret;
    ClockFmtScnCmdArgs opts;	/* Format, locale, timezone and base */
    DateFormat	    dateFmt;	/* Common structure used for formatting */

    /* even number of arguments */
    if ((objc & 1) == 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock format clockval|-now "
	    "?-format string? "
	    "?-gmt boolean? "
	    "?-locale LOCALE? ?-timezone ZONE?");
	Tcl_SetErrorCode(interp, "CLOCK", "wrongNumArgs", NULL);
	return TCL_ERROR;
    }

    memset(&dateFmt, 0, sizeof(dateFmt));

    /*
     * Extract values for the keywords.
     */

    ClockInitFmtScnArgs(clientData, interp, &opts);
    ret = ClockParseFmtScnArgs(&opts, &dateFmt.date, objc, objv,
	    CLC_FMT_ARGS);
    if (ret != TCL_OK) {
	goto done;
    }

    /* Default format */
    if (opts.formatObj == NULL) {
	opts.formatObj = dataPtr->literals[LIT__DEFAULT_FORMAT];
    }

    /* Use compiled version of Format - */

    ret = ClockFormat(&dateFmt, &opts);

done:

    Tcl_UnsetObjRef(dateFmt.date.tzName);

    if (ret != TCL_OK) {
	return ret;
    }

    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockScanObjCmd -- , clock scan --
 *
 *	This function is invoked to process the Tcl "clock scan" command.
 *
 *	Inputs a count of seconds since the Posix Epoch as a time of day.
 *
 *	The 'clock scan' command scans times of day on input. Refer to the
 *	user documentation to see what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ClockScanObjCmd(
    ClientData clientData,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const objv[])	/* Parameter values */
{
    int ret;
    ClockFmtScnCmdArgs opts;	/* Format, locale, timezone and base */
    DateInfo	    yy;		/* Common structure used for parsing */
    DateInfo	   *info = &yy;

    /* even number of arguments */
    if ((objc & 1) == 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock scan string "
	    "?-base seconds? "
	    "?-format string? "
	    "?-gmt boolean? "
	    "?-locale LOCALE? ?-timezone ZONE?");
	Tcl_SetErrorCode(interp, "CLOCK", "wrongNumArgs", NULL);
	return TCL_ERROR;
    }

    ClockInitDateInfo(&yy);
    
    /*
     * Extract values for the keywords.
     */

    ClockInitFmtScnArgs(clientData, interp, &opts);
    ret = ClockParseFmtScnArgs(&opts, &yy.date, objc, objv,
	    CLC_SCN_ARGS);
    if (ret != TCL_OK) {
	goto done;
    }

    /* seconds are in localSeconds (relative base date), so reset time here */
    yyHour = 0; yyMinutes = 0; yySeconds = 0; yyMeridian = MER24;

    /* If free scan */
    if (opts.formatObj == NULL) {
	/* Use compiled version of FreeScan - */

	/* [SB] TODO: Perhaps someday we'll localize the legacy code. Right now, it's not localized. */
	if (opts.localeObj != NULL) {
	    Tcl_SetResult(interp,
		(char*)"legacy [clock scan] does not support -locale", TCL_STATIC);
	    Tcl_SetErrorCode(interp, "CLOCK", "flagWithLegacyFormat", NULL);
	    return TCL_ERROR;
	}
	ret = ClockFreeScan(&yy, objv[1], &opts);
    } 
    else {
	/* Use compiled version of Scan - */

	ret = ClockScan(&yy, objv[1], &opts);
    }

    /* Convert date info structure into UTC seconds */

    if (ret == TCL_OK) {
	ret = ClockScanCommit(clientData, &yy, &opts);
    }

done:

    Tcl_UnsetObjRef(yy.date.tzName);

    if (ret != TCL_OK) {
	return ret;
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(yy.date.seconds));
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockScanCommit --
 *
 *	Converts date info structure into UTC seconds.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ClockScanCommit(
    ClientData clientData,	/* Client data containing literal pool */
    register DateInfo  *info,	/* Clock scan info structure */
    register
    ClockFmtScnCmdArgs *opts)	/* Format, locale, timezone and base */
{
    /* If needed assemble julianDay using year, month, etc. */
    if (info->flags & CLF_ASSEMBLE_JULIANDAY) {
	if ((info->flags & CLF_ISO8601)) {
	    GetJulianDayFromEraYearWeekDay(&yydate, GREGORIAN_CHANGE_DATE);
	}
	else
	if (!(info->flags & CLF_DAYOFYEAR)) {
	    GetJulianDayFromEraYearMonthDay(&yydate, GREGORIAN_CHANGE_DATE);
	} else {
	    GetJulianDayFromEraYearDay(&yydate, GREGORIAN_CHANGE_DATE);
	}
    }

    /* some overflow checks, if not extended */
    if (!(opts->flags & CLF_EXTENDED)) {
	if (yydate.julianDay > 5373484) {
	    Tcl_SetObjResult(opts->interp, Tcl_NewStringObj(
		"requested date too large to represent", -1));
	    Tcl_SetErrorCode(opts->interp, "CLOCK", "dateTooLarge", NULL);
	    return TCL_ERROR;
	}
    }

    /* Local seconds to UTC (stored in yydate.seconds) */

    if (info->flags & (CLF_ASSEMBLE_SECONDS|CLF_ASSEMBLE_JULIANDAY)) {
	yydate.localSeconds =
	    -210866803200L
	    + ( SECONDS_PER_DAY * (Tcl_WideInt)yydate.julianDay )
	    + ( yySeconds % SECONDS_PER_DAY );
    }

    if (info->flags & (CLF_ASSEMBLE_SECONDS|CLF_ASSEMBLE_JULIANDAY|CLF_LOCALSEC)) {
	if (ConvertLocalToUTC(clientData, opts->interp, &yydate, opts->timezoneObj, 
	      GREGORIAN_CHANGE_DATE) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /* Increment UTC seconds with relative time */
    
    yydate.seconds += yyRelSeconds;

    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockFreeScan --
 *
 *	Used by ClockScanObjCmd for free scanning without format.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
ClockFreeScan(
    register
    DateInfo	       *info,	/* Date fields used for parsing & converting
				 * simultaneously a yy-parse structure of the 
				 * TclClockFreeScan */	  
    Tcl_Obj *strObj,		/* String containing the time to scan */
    ClockFmtScnCmdArgs *opts)	/* Command options */
{
    Tcl_Interp	    *interp =  opts->interp;
    ClockClientData *dataPtr = opts->clientData;

    int ret = TCL_ERROR;

    /*
     * Parse the date. The parser will fill a structure "info" with date,
     * time, time zone, relative month/day/seconds, relative weekday, ordinal
     * month.
     * Notice that many yy-defines point to values in the "info" or "date" 
     * structure, e. g. yySeconds -> info->date.secondOfDay or 
     *			yySeconds -> info->date.month (same as yydate.month)
     */
    yyInput = Tcl_GetString(strObj);

    if (TclClockFreeScan(interp, info) != TCL_OK) {
	Tcl_Obj *msg = Tcl_NewObj();
	Tcl_AppendPrintfToObj(msg, "unable to convert date-time string \"%s\": %s", 
	    Tcl_GetString(strObj), TclGetString(Tcl_GetObjResult(interp)));
	Tcl_SetObjResult(interp, msg);
	goto done;
    }

    /*
     * If the caller supplied a date in the string, update the date with
     * the value. If the caller didn't specify a time with the date, default to
     * midnight.
     */

    if (yyHaveDate) {
	if (yyYear < 100) {
	    if (yyYear >= dataPtr->yearOfCenturySwitch) {
		yyYear -= 100;
	    }
	    yyYear += dataPtr->currentYearCentury;
	}
	yydate.era = CE;
	if (yyHaveTime == 0) {
	    yyHaveTime = -1;
	}
	info->flags |= CLF_ASSEMBLE_JULIANDAY|CLF_ASSEMBLE_SECONDS;
    }

    /*
     * If the caller supplied a time zone in the string, make it into a time 
     * zone indicator of +-hhmm and setup this time zone.
     */

    if (yyHaveZone) {
	Tcl_Obj *tzObjStor = NULL;
	int minEast = -yyTimezone;
	int dstFlag = 1 - yyDSTmode;
	tzObjStor = ClockFormatNumericTimeZone(
			  60 * minEast + 3600 * dstFlag);
	Tcl_IncrRefCount(tzObjStor);

	opts->timezoneObj = ClockSetupTimeZone(dataPtr, interp, tzObjStor);

	Tcl_DecrRefCount(tzObjStor);
	if (opts->timezoneObj == NULL) {
	    goto done;
	}

	// Tcl_SetObjRef(yydate.tzName, opts->timezoneObj);

	info->flags |= CLF_ASSEMBLE_SECONDS;
    }
    
    /* 
     * Assemble date, time, zone into seconds-from-epoch
     */

    if (yyHaveTime == -1) {
	yySeconds = 0;
	info->flags |= CLF_ASSEMBLE_SECONDS;
    }
    else
    if (yyHaveTime) {
	yySeconds = ToSeconds(yyHour, yyMinutes,
			    yySeconds, yyMeridian);
	info->flags |= CLF_ASSEMBLE_SECONDS;
    } 
    else 
    if ( (yyHaveDay && !yyHaveDate)
	    || yyHaveOrdinalMonth
	    || ( yyHaveRel
		&& ( yyRelMonth != 0
		     || yyRelDay != 0 ) )
    ) {
	yySeconds = 0;
	info->flags |= CLF_ASSEMBLE_SECONDS;
    } 
    else {
	yySeconds = yydate.localSeconds % SECONDS_PER_DAY;
    }

    /*
     * Do relative times
     */

    ret = ClockCalcRelTime(info, opts);

    /* Free scanning completed - date ready */

done:

    return ret;
}

/*----------------------------------------------------------------------
 *
 * ClockCalcRelTime --
 *
 *	Used for calculating of relative times.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
ClockCalcRelTime(
    register
    DateInfo	       *info,	/* Date fields used for converting */
    ClockFmtScnCmdArgs *opts)	/* Command options */
{
    /*
     * Because some calculations require in-between conversion of the
     * julian day, we can repeat this processing multiple times 
     */
repeat_rel:

    if (yyHaveRel) {

	/* 
	 * Relative conversion normally possible in UTC time only, because
	 * of possible wrong local time increment if ignores in-between DST-hole.
	 * (see test-cases clock-34.53, clock-34.54).
	 * So increment date in julianDay, but time inside day in UTC (seconds).
	 */

	/* add months (or years in months) */

	if (yyRelMonth != 0) {
	    int m, h;

	    /* if needed extract year, month, etc. again */
	    if (info->flags & CLF_ASSEMBLE_DATE) {
		GetGregorianEraYearDay(&yydate, GREGORIAN_CHANGE_DATE);
		GetMonthDay(&yydate);
		GetYearWeekDay(&yydate, GREGORIAN_CHANGE_DATE);
		info->flags &= ~CLF_ASSEMBLE_DATE;
	    }

	    /* add the requisite number of months */
	    yyMonth += yyRelMonth - 1;
	    yyYear += yyMonth / 12;
	    m = yyMonth % 12;
	    yyMonth = m + 1;

	    /* if the day doesn't exist in the current month, repair it */
	    h = hath[IsGregorianLeapYear(&yydate)][m];
	    if (yyDay > h) {
		yyDay = h;
	    }

	    /* on demand (lazy) assemble julianDay using new year, month, etc. */
	    info->flags |= CLF_ASSEMBLE_JULIANDAY|CLF_ASSEMBLE_SECONDS;

	    yyRelMonth = 0;
	}

	/* add days (or other parts aligned to days) */
	if (yyRelDay) {

	    /* assemble julianDay using new year, month, etc. */
	    if (info->flags & CLF_ASSEMBLE_JULIANDAY) {
		GetJulianDayFromEraYearMonthDay(&yydate, GREGORIAN_CHANGE_DATE);
		info->flags &= ~CLF_ASSEMBLE_JULIANDAY;
	    }
	    yydate.julianDay += yyRelDay;
	    
	    /* julianDay was changed, on demand (lazy) extract year, month, etc. again */
	    info->flags |= CLF_ASSEMBLE_DATE|CLF_ASSEMBLE_SECONDS;

	    yyRelDay = 0;
	}

	/* relative time (seconds), if exceeds current date, do the day conversion and
	 * leave rest of the increment in yyRelSeconds to add it hereafter in UTC seconds */
	if (yyRelSeconds) {
	    int newSecs = yySeconds + yyRelSeconds;
	       
	    /* if seconds increment outside of current date, increment day */
	    if (newSecs / SECONDS_PER_DAY != yySeconds / SECONDS_PER_DAY) {
		
		yyRelDay += newSecs / SECONDS_PER_DAY;
		yySeconds = 0;		    
		yyRelSeconds = newSecs % SECONDS_PER_DAY;

		goto repeat_rel;
	    }
	}

	yyHaveRel = 0;
    }

    /*
     * Do relative (ordinal) month
     */

    if (yyHaveOrdinalMonth) {
	int monthDiff;

	/* if needed extract year, month, etc. again */
	if (info->flags & CLF_ASSEMBLE_DATE) {
	    GetGregorianEraYearDay(&yydate, GREGORIAN_CHANGE_DATE);
	    GetMonthDay(&yydate);
	    GetYearWeekDay(&yydate, GREGORIAN_CHANGE_DATE);
	    info->flags &= ~CLF_ASSEMBLE_DATE;
	}

	if (yyMonthOrdinalIncr > 0) {
	    monthDiff = yyMonthOrdinal - yyMonth;
	    if (monthDiff <= 0) {
		monthDiff += 12;
	    }
	    yyMonthOrdinalIncr--;
	} else {
	    monthDiff = yyMonth - yyMonthOrdinal;
	    if (monthDiff >= 0) {
		monthDiff -= 12;
	    }
	    yyMonthOrdinalIncr++;
	}

	/* process it further via relative times */
	yyHaveRel++;
	yyYear += yyMonthOrdinalIncr;
	yyRelMonth += monthDiff;
	yyHaveOrdinalMonth = 0;

	info->flags |= CLF_ASSEMBLE_JULIANDAY|CLF_ASSEMBLE_SECONDS;

	goto repeat_rel;
    }

    /*
     * Do relative weekday
     */

    if (yyHaveDay && !yyHaveDate) {

	/* if needed assemble julianDay now */
	if (info->flags & CLF_ASSEMBLE_JULIANDAY) {
	    GetJulianDayFromEraYearMonthDay(&yydate, GREGORIAN_CHANGE_DATE);
	    info->flags &= ~CLF_ASSEMBLE_JULIANDAY;
	}

	yydate.era = CE;
	yydate.julianDay = WeekdayOnOrBefore(yyDayNumber, yydate.julianDay + 6)
		    + 7 * yyDayOrdinal;
	if (yyDayOrdinal > 0) {
	    yydate.julianDay -= 7;
	}
	info->flags |= CLF_ASSEMBLE_DATE|CLF_ASSEMBLE_SECONDS;
    }

    return TCL_OK;
}


/*----------------------------------------------------------------------
 *
 * ClockWeekdaysOffs --
 *
 *	Get offset in days for the number of week days corresponding the
 *	given day of week (skipping Saturdays and Sundays).
 *	
 *
 * Results:
 *	Returns a day increment adjusted the given weekdays
 *
 *----------------------------------------------------------------------
 */

static inline int
ClockWeekdaysOffs(
    register int dayOfWeek,
    register int offs)
{
    register int weeks, resDayOfWeek;

    /* offset in days */
    weeks = offs / 5;
    offs = offs % 5;
    /* compiler fix for negative offs - wrap (0, -1) -> (-1, 4) */
    if (offs < 0) {
	weeks--;
	offs = 5 + offs;
    }
    offs += 7 * weeks;

    /* resulting day of week */
    {
	register int day = (offs % 7);
	/* compiler fix for negative offs - wrap (0, -1) -> (-1, 6) */
	if (day < 0) {
	    day = 7 + day;
	}
	resDayOfWeek = dayOfWeek + day;
    }

    /* adjust if we start from a weekend */
    if (dayOfWeek > 5) {
	int adj = 5 - dayOfWeek;
	offs += adj;
	resDayOfWeek += adj;
    }

    /* adjust if we end up on a weekend */
    if (resDayOfWeek > 5) {
       offs += 2;
    }

    return offs;
}



/*----------------------------------------------------------------------
 *
 * ClockAddObjCmd -- , clock add --
 *
 *	Adds an offset to a given time.
 *
 *	Refer to the user documentation to see what it exactly does.
 *
 * Syntax:
 *   clock add clockval ?count unit?... ?-option value?
 *
 * Parameters:
 *   clockval -- Starting time value
 *   count -- Amount of a unit of time to add
 *   unit -- Unit of time to add, must be one of:
 *	     years year months month weeks week
 *	     days day hours hour minutes minute
 *	     seconds second
 *
 * Options:
 *   -gmt BOOLEAN
 *	 Flag synonymous with '-timezone :GMT'
 *   -timezone ZONE
 *	 Name of the time zone in which calculations are to be done.
 *   -locale NAME
 *	 Name of the locale in which calculations are to be done.
 *	 Used to determine the Gregorian change date.
 *
 * Results:
 *	Returns a standard Tcl result with the given time adjusted 
 *	by the given offset(s) in order.
 *
 * Notes:
 *   It is possible that adding a number of months or years will adjust the
 *   day of the month as well.	For instance, the time at one month after
 *   31 January is either 28 or 29 February, because February has fewer
 *   than 31 days.
 *
 *----------------------------------------------------------------------
 */

int
ClockAddObjCmd(
    ClientData clientData,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const objv[])	/* Parameter values */
{
    ClockClientData *dataPtr = clientData;
    int ret;
    ClockFmtScnCmdArgs opts;	/* Format, locale, timezone and base */
    DateInfo	    yy;		/* Common structure used for parsing */
    DateInfo	   *info = &yy;

    /* add "week" to units also (because otherwise ambiguous) */
    static const char *const units[] = {
	"years",	"months",	    "week",	    "weeks",
	"days",		"weekdays",
	"hours",	"minutes",	    "seconds",
	NULL
    };
    enum unitInd {
	CLC_ADD_YEARS,	CLC_ADD_MONTHS,	    CLC_ADD_WEEK,   CLC_ADD_WEEKS,
	CLC_ADD_DAYS,	CLC_ADD_WEEKDAYS,
	CLC_ADD_HOURS,	CLC_ADD_MINUTES,    CLC_ADD_SECONDS
    };
    int unitIndex;		/* Index of an option. */
    int i;
    Tcl_WideInt offs;

    /* even number of arguments */
    if ((objc & 1) == 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock add clockval|-now ?number units?..."
	    "?-gmt boolean? "
	    "?-locale LOCALE? ?-timezone ZONE?");
	Tcl_SetErrorCode(interp, "CLOCK", "wrongNumArgs", NULL);
	return TCL_ERROR;
    }

    ClockInitDateInfo(&yy);
    
    /*
     * Extract values for the keywords.
     */

    ClockInitFmtScnArgs(clientData, interp, &opts);
    ret = ClockParseFmtScnArgs(&opts, &yy.date, objc, objv,
	    CLC_ADD_ARGS);
    if (ret != TCL_OK) {
	goto done;
    }

    /* time together as seconds of the day */
    yySeconds = yydate.localSeconds % SECONDS_PER_DAY;	 
    /* seconds are in localSeconds (relative base date), so reset time here */
    yyHour = 0; yyMinutes = 0; yyMeridian = MER24;

    ret = TCL_ERROR;

    /*
     * Find each offset and process date increment
     */

    for (i = 2; i < objc; i+=2) {
	/* bypass not integers (options, allready processed above) */
	if (TclGetWideIntFromObj(NULL, objv[i], &offs) != TCL_OK) {
	    continue;
	}
	if (objv[i]->typePtr == &tclBignumType) {
	    Tcl_SetObjResult(interp, dataPtr->literals[LIT_INTEGER_VALUE_TOO_LARGE]);
	    goto done;
	}
	/* get unit */
	if (Tcl_GetIndexFromObj(interp, objv[i+1], units, "unit", 0,
		&unitIndex) != TCL_OK) {
	    goto done;
	}

	/* nothing to do if zero quantity */
	if (!offs) {
	    continue;
	}

	/* if in-between conversion needed (already have relative date/time), 
	 * correct date info, because the date may be changed, 
	 * so refresh it now */

	if ( yyHaveRel
	  && ( unitIndex == CLC_ADD_WEEKDAYS 
	    /* some months can be shorter as another */
	    || yyRelMonth || yyRelDay
	    /* day changed */
	    || yySeconds + yyRelSeconds > SECONDS_PER_DAY
	    || yySeconds + yyRelSeconds < 0
	  )
	) {
	    if (ClockCalcRelTime(info, &opts) != TCL_OK) {
		goto done;
	    }
	}

	/* process increment by offset + unit */
	yyHaveRel++;
	switch (unitIndex) {
	case CLC_ADD_YEARS:
	    yyRelMonth += offs * 12;
	    break;
	case CLC_ADD_MONTHS:
	    yyRelMonth += offs;
	    break;
	case CLC_ADD_WEEK:
	case CLC_ADD_WEEKS:
	    yyRelDay += offs * 7;
	    break;
	case CLC_ADD_DAYS:
	    yyRelDay += offs;
	    break;
	case CLC_ADD_WEEKDAYS:
	    /* add number of week days (skipping Saturdays and Sundays)
	     * to a relative days value. */
	    offs = ClockWeekdaysOffs(yy.date.dayOfWeek, offs);
	    yyRelDay += offs;
	    break;
	case CLC_ADD_HOURS:
	    yyRelSeconds += offs * 60 * 60;
	    break;
	case CLC_ADD_MINUTES:
	    yyRelSeconds += offs * 60;
	    break;
	case CLC_ADD_SECONDS:
	    yyRelSeconds += offs;
	    break;
	}
    }

    /*
     * Do relative times (if not yet already processed interim):
     */

    if (yyHaveRel) {
	if (ClockCalcRelTime(info, &opts) != TCL_OK) {
	    goto done;
	}
    }

    /* Convert date info structure into UTC seconds */

    ret = ClockScanCommit(clientData, &yy, &opts);

done:

    Tcl_UnsetObjRef(yy.date.tzName);

    if (ret != TCL_OK) {
	return ret;
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(yy.date.seconds));
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ClockSecondsObjCmd -
 *
 *	Returns a count of microseconds since the epoch.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 * This function implements the 'clock seconds' Tcl command. Refer to the user
 * documentation for details on what it does.
 *
 *----------------------------------------------------------------------
 */

int
ClockSecondsObjCmd(
    ClientData clientData,	/* Client data is unused */
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const *objv)	/* Parameter values */
{
    Tcl_Time now;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 0, NULL, "clock seconds");
	return TCL_ERROR;
    }
    Tcl_GetTime(&now);
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt) now.sec));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TzsetGetEpoch --, TzsetIfNecessary --
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

static unsigned long
TzsetGetEpoch(void)
{
    static char* tzWas = INT2PTR(-1);	 /* Previous value of TZ, protected by
					  * clockMutex. */
    static long	 tzLastRefresh = 0;	 /* Used for latency before next refresh */
    static unsigned long tzWasEpoch = 0; /* Epoch, signals that TZ changed */
    static unsigned long tzEnvEpoch = 0; /* Last env epoch, for faster signaling, 
					    that TZ changed via TCL */
    
    const char *tzIsNow;		 /* Current value of TZ */
    
    /* 
     * Prevent performance regression on some platforms by resolving of system time zone:
     * small latency for check whether environment was changed (once per second)
     * no latency if environment was chaned with tcl-env (compare both epoch values)
     */
    Tcl_Time now;
    Tcl_GetTime(&now);
    if (now.sec == tzLastRefresh && tzEnvEpoch == TclEnvEpoch) {
	return tzWasEpoch;
    }
    tzEnvEpoch = TclEnvEpoch;
    tzLastRefresh = now.sec;

    /* check in lock */
    Tcl_MutexLock(&clockMutex);
    tzIsNow = getenv("TCL_TZ");
    if (tzIsNow == NULL) {
	tzIsNow = getenv("TZ");
    }
    if (tzIsNow != NULL && (tzWas == NULL || tzWas == INT2PTR(-1)
	    || strcmp(tzIsNow, tzWas) != 0)) {
	tzset();
	if (tzWas != NULL && tzWas != INT2PTR(-1)) {
	    ckfree(tzWas);
	}
	tzWas = ckalloc(strlen(tzIsNow) + 1);
	strcpy(tzWas, tzIsNow);
	tzWasEpoch++;
    } else if (tzIsNow == NULL && tzWas != NULL) {
	tzset();
	if (tzWas != INT2PTR(-1)) ckfree(tzWas);
	tzWas = NULL;
	tzWasEpoch++;
    }
    Tcl_MutexUnlock(&clockMutex);

    return tzWasEpoch;
}

static void
TzsetIfNecessary(void)
{
    TzsetGetEpoch();
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
