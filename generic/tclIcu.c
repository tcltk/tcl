/*
 * tclIcu.c --
 *
 * 	tclIcu.c implements various Tcl commands that make use of
 *	the ICU library if present on the system.
 *	(Adapted from tkIcu.c)
 *
 * Copyright © 2021 Jan Nijtmans
 * Copyright © 2024 Ashok P. Nadkarni
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * Runtime linking of libicu.
 */
typedef enum UBreakIteratorTypex {
	  UBRK_CHARACTERX = 0,
	  UBRK_WORDX = 1
} UBreakIteratorTypex;

typedef enum UErrorCodex {
    U_AMBIGUOUS_ALIAS_WARNING = -122,
    U_ZERO_ERRORZ              =  0,     /**< No error, no warning. */
} UErrorCodex;

#define U_SUCCESS(x) ((x)<=U_ZERO_ERRORZ)
#define U_FAILURE(x) ((x)>U_ZERO_ERRORZ)

struct UEnumeration;
typedef struct UEnumeration UEnumeration;
struct UCharsetDetector;
typedef struct UCharsetDetector UCharsetDetector;
struct UCharsetMatch;
typedef struct UCharsetMatch UCharsetMatch;

/*
 * Prototypes for ICU functions sorted by category.
 */
typedef void        (*fn_u_cleanup)(void);
typedef const char *(*fn_u_errorName)(UErrorCodex);

typedef uint16_t    (*fn_ucnv_countAliases)(const char *, UErrorCodex *);
typedef int32_t     (*fn_ucnv_countAvailable)();
typedef const char *(*fn_ucnv_getAlias)(const char *, uint16_t, UErrorCodex *);
typedef const char *(*fn_ucnv_getAvailableName)(int32_t);

typedef void *(*fn_ubrk_open)(UBreakIteratorTypex, const char *,
	const uint16_t *, int32_t, UErrorCodex *);
typedef void	(*fn_ubrk_close)(void *);
typedef int32_t	(*fn_ubrk_preceding)(void *, int32_t);
typedef int32_t	(*fn_ubrk_following)(void *, int32_t);
typedef int32_t	(*fn_ubrk_previous)(void *);
typedef int32_t	(*fn_ubrk_next)(void *);
typedef void	(*fn_ubrk_setText)(void *, const void *, int32_t, UErrorCodex *);

typedef UCharsetDetector * (*fn_ucsdet_open)(UErrorCodex   *status);
typedef void               (*fn_ucsdet_close)(UCharsetDetector *ucsd);
typedef void               (*fn_ucsdet_setText)(UCharsetDetector *ucsd, const char *textIn, int32_t len, UErrorCodex *status);
typedef const char *       (*fn_ucsdet_getName)(const UCharsetMatch *ucsm, UErrorCodex *status);
typedef UEnumeration *     (*fn_ucsdet_getAllDetectableCharsets)(UCharsetDetector *ucsd, UErrorCodex *status);
typedef const UCharsetMatch *  (*fn_ucsdet_detect)(UCharsetDetector *ucsd, UErrorCodex *status);
typedef const UCharsetMatch ** (*fn_ucsdet_detectAll)(UCharsetDetector *ucsd, int32_t *matchesFound, UErrorCodex *status);

typedef void        (*fn_uenum_close)(UEnumeration *);
typedef int32_t     (*fn_uenum_count)(UEnumeration *, UErrorCodex *);
typedef const char *(*fn_uenum_next)(UEnumeration *, int32_t *, UErrorCodex *);

#define FIELD(name) fn_ ## name _ ## name
static struct {
    size_t              nopen; /* Total number of references to ALL libraries */
    /*
     * Depending on platform, ICU symbols may be distributed amongst
     * multiple libraries. For current functionality at most 2 needed.
     * Order of library loading is not guaranteed.
     */
    Tcl_LoadHandle      libs[2];

    FIELD(u_cleanup);
    FIELD(u_errorName);

    FIELD(ubrk_open);
    FIELD(ubrk_close);
    FIELD(ubrk_preceding);
    FIELD(ubrk_following);
    FIELD(ubrk_previous);
    FIELD(ubrk_next);
    FIELD(ubrk_setText);

    FIELD(ucnv_countAliases);
    FIELD(ucnv_countAvailable);
    FIELD(ucnv_getAlias);
    FIELD(ucnv_getAvailableName);

    FIELD(ucsdet_close);
    FIELD(ucsdet_detect);
    FIELD(ucsdet_detectAll);
    FIELD(ucsdet_getAllDetectableCharsets);
    FIELD(ucsdet_getName);
    FIELD(ucsdet_open);
    FIELD(ucsdet_setText);

    FIELD(uenum_close);
    FIELD(uenum_count);
    FIELD(uenum_next);

} icu_fns = {
    0, {NULL, NULL}, /* Reference count, library handles */
    NULL, NULL, /* u_* */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* ubrk* */
    NULL, NULL, NULL, NULL, /* ucnv_* */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* ucsdet* */
    NULL, NULL, NULL, /* uenum_* */
};

#define u_cleanup        icu_fns._u_cleanup
#define u_errorName      icu_fns._u_errorName

#define ubrk_open        icu_fns._ubrk_open
#define ubrk_close       icu_fns._ubrk_close
#define ubrk_preceding   icu_fns._ubrk_preceding
#define ubrk_following   icu_fns._ubrk_following
#define ubrk_previous    icu_fns._ubrk_previous
#define ubrk_next        icu_fns._ubrk_next
#define ubrk_setText     icu_fns._ubrk_setText

#define ucnv_countAliases     icu_fns._ucnv_countAliases
#define ucnv_countAvailable   icu_fns._ucnv_countAvailable
#define ucnv_getAlias         icu_fns._ucnv_getAlias
#define ucnv_getAvailableName icu_fns._ucnv_getAvailableName

#define ucsdet_close     icu_fns._ucsdet_close
#define ucsdet_detect    icu_fns._ucsdet_detect
#define ucsdet_detectAll icu_fns._ucsdet_detectAll
#define ucsdet_getAllDetectableCharsets icu_fns._ucsdet_getAllDetectableCharsets
#define ucsdet_getName   icu_fns._ucsdet_getName
#define ucsdet_open      icu_fns._ucsdet_open
#define ucsdet_setText   icu_fns._ucsdet_setText

#define uenum_next       icu_fns._uenum_next
#define uenum_close      icu_fns._uenum_close
#define uenum_count      icu_fns._uenum_count


TCL_DECLARE_MUTEX(icu_mutex);

static int FunctionNotAvailableError(Tcl_Interp *interp) {
    if (interp) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("ICU function not available", TCL_INDEX_NONE));
    }
    return TCL_ERROR;
}

static int IcuError(Tcl_Interp *interp, const char *message, UErrorCodex code)
{
    if (interp) {
	const char *codeMessage = NULL;
	if (u_errorName) {
	    codeMessage = u_errorName(code);
	}
	Tcl_SetObjResult(interp,
			 Tcl_ObjPrintf("%s. ICU error (%d): %s",
				       message,
				       code,
				       codeMessage ? codeMessage : ""));
    }
    return TCL_ERROR;
}

static int DetectEncoding(Tcl_Interp *interp, Tcl_Obj *objPtr, int all)
{
    Tcl_Size len;
    const char *bytes;
    const UCharsetMatch *match;
    const UCharsetMatch **matches;
    int nmatches;
    int ret;

    if (ucsdet_open == NULL || ucsdet_setText == NULL ||
	ucsdet_detect == NULL || ucsdet_detectAll == NULL ||
	ucsdet_getName == NULL || ucsdet_close == NULL) {
	return FunctionNotAvailableError(interp);
    }

    bytes = (char *) Tcl_GetBytesFromObj(interp, objPtr, &len);
    if (bytes == NULL) {
	return TCL_ERROR;
    }
    UErrorCodex status = U_ZERO_ERRORZ;

    UCharsetDetector* csd = ucsdet_open(&status);
    if (U_FAILURE(status)) {
	return IcuError(interp, "Could not open charset detector.", status);
    }

    ucsdet_setText(csd, bytes, len, &status);
    if (U_FAILURE(status)) {
	IcuError(interp, "Could not set detection text.", status);
	ucsdet_close(csd);
	return TCL_ERROR;
    }

    if (all) {
	matches = ucsdet_detectAll(csd, &nmatches, &status);
    }
    else {
	match = ucsdet_detect(csd, &status);
	matches = &match;
	nmatches = match ? 1 : 0;
    }

    if (U_FAILURE(status) || nmatches == 0) {
	ret = IcuError(interp, "Could not detect character set.", status);
    }
    else {
	int i;
	Tcl_Obj *resultObj = Tcl_NewListObj(nmatches, NULL);
	for (i = 0; i < nmatches; ++i) {
	    const char *name = ucsdet_getName(matches[i], &status);
	    if (U_FAILURE(status) || name == NULL) {
		name = "unknown";
		status = U_ZERO_ERRORZ; /* Reset on failure */
	    }
	    Tcl_ListObjAppendElement(
		NULL, resultObj, Tcl_NewStringObj(name, -1));
	}
	Tcl_SetObjResult(interp, resultObj);
	ret = TCL_OK;
    }

    ucsdet_close(csd);
    return ret;
}

static int DetectableEncodings(Tcl_Interp *interp)
{
    if (ucsdet_open == NULL || ucsdet_getAllDetectableCharsets == NULL ||
	ucsdet_close == NULL || uenum_next == NULL || uenum_count == NULL ||
	uenum_close == NULL) {
	return FunctionNotAvailableError(interp);
    }
    UErrorCodex status = U_ZERO_ERRORZ;

    UCharsetDetector* csd = ucsdet_open(&status);
    if (U_FAILURE(status)) {
	return IcuError(interp, "Could not open charset detector.", status);
    }

    int ret;
    UEnumeration *enumerator = ucsdet_getAllDetectableCharsets(csd, &status);
    if (U_FAILURE(status) || enumerator == NULL) {
	IcuError(interp, "Could not get list of detectable encodings.", status);
	ret = TCL_ERROR;
    } else {
	int32_t count;
	count = uenum_count(enumerator, &status);
	if (U_FAILURE(status)) {
	    IcuError(interp, "Could not get charset enumerator count.", status);
	    ret = TCL_ERROR;
	} else {
	    int i;
	    Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);
	    for (i = 0; i < count; ++i) {
		const char *name;
		int32_t len;
		name = uenum_next(enumerator, &len, &status);
		if (name == NULL || U_FAILURE(status)) {
		    name = "unknown";
		    len = 7;
		    status = U_ZERO_ERRORZ; /* Reset on error */
		}
		Tcl_ListObjAppendElement(
		    interp, resultObj, Tcl_NewStringObj(name, len));
	    }
	    Tcl_SetObjResult(interp, resultObj);
	    ret = TCL_OK;
	}
	uenum_close(enumerator);
    }

    ucsdet_close(csd);
    return ret;
}

/*
 *------------------------------------------------------------------------
 *
 * EncodingDetectObjCmd --
 *
 *    Implements the Tcl command EncodingDetect.
 *       encdetect - returns names of all detectable encodings
 *       encdetect BYTES ?-all? - return detected encoding(s)
 *
 * Results:
 *    TCL_OK    - Success.
 *    TCL_ERROR - Error.
 *
 * Side effects:
 *    Interpreter result holds result or error message.
 *
 *------------------------------------------------------------------------
 */
static int
IcuDetectObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const objv[])
{
    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 1 , objv, "?bytes ?-all??");
	return TCL_ERROR;
    }

    if (objc == 1) {
	return DetectableEncodings(interp);
    }

    int all = 0;
    if (objc == 3) {
	if (strcmp("-all", Tcl_GetString(objv[2]))) {
	    Tcl_SetObjResult(
		interp,
		Tcl_ObjPrintf("Invalid option %s, must be \"-all\"",
			      Tcl_GetString(objv[2])));
	    return TCL_ERROR;
	}
	all = 1;
    }

    return DetectEncoding(interp, objv[1], all);
}

/*
 *------------------------------------------------------------------------
 *
 * IcuConverterNamesObjCmd --
 *
 *    Sets interp result to list of available ICU converters.
 *
 * Results:
 *    TCL_OK    - Success.
 *    TCL_ERROR - Error.
 *
 * Side effects:
 *    Interpreter result holds list of converter names.
 *
 *------------------------------------------------------------------------
 */
static int
IcuConverterNamesObjCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,    /* Current interpreter. */
    Tcl_Size objc,              /* Number of arguments. */
    Tcl_Obj *const objv[]) /* Argument objects. */
{

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1 , objv, "");
	return TCL_ERROR;
    }
    if (ucnv_countAvailable == NULL || ucnv_getAvailableName == NULL) {
	return FunctionNotAvailableError(interp);
    }

    int32_t count = ucnv_countAvailable();
    if (count <= 0) {
	return TCL_OK;
    }
    Tcl_Obj *resultObj = Tcl_NewListObj(count, NULL);
    int32_t i;
    for (i = 0; i < count; ++i) {
	const char *name = ucnv_getAvailableName(i);
	if (name) {
	    Tcl_ListObjAppendElement(
		NULL, resultObj, Tcl_NewStringObj(name, -1));
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *------------------------------------------------------------------------
 *
 * IcuConverterAliasesObjCmd --
 *
 *    Sets interp result to list of available ICU converters.
 *
 * Results:
 *    TCL_OK    - Success.
 *    TCL_ERROR - Error.
 *
 * Side effects:
 *    Interpreter result holds list of converter names.
 *
 *------------------------------------------------------------------------
 */
static int
IcuConverterAliasesObjCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,    /* Current interpreter. */
    Tcl_Size objc,              /* Number of arguments. */
    Tcl_Obj *const objv[]) /* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1 , objv, "convertername");
	return TCL_ERROR;
    }
    if (ucnv_countAliases == NULL || ucnv_getAlias == NULL) {
	return FunctionNotAvailableError(interp);
    }

    const char *name = Tcl_GetString(objv[1]);
    UErrorCodex status = U_ZERO_ERRORZ;
    uint16_t count = ucnv_countAliases(name, &status);
    if (status != U_AMBIGUOUS_ALIAS_WARNING && U_FAILURE(status)) {
	return IcuError(interp, "Could not get aliases.", status);
    }
    if (count <= 0) {
	return TCL_OK;
    }
    Tcl_Obj *resultObj = Tcl_NewListObj(count, NULL);
    uint16_t i;
    for (i = 0; i < count; ++i) {
	status = U_ZERO_ERRORZ; /* Reset in case U_AMBIGUOUS_ALIAS_WARNING */
	const char *aliasName = ucnv_getAlias(name, i, &status);
	if (status != U_AMBIGUOUS_ALIAS_WARNING && U_FAILURE(status)) {
	    status = U_ZERO_ERRORZ; /* Reset error for next iteration */
	    continue;
	}
	if (aliasName) {
	    Tcl_ListObjAppendElement(
		NULL, resultObj, Tcl_NewStringObj(aliasName, -1));
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

static void
TclIcuCleanup(
    TCL_UNUSED(void *))
{
    Tcl_MutexLock(&icu_mutex);
    if (icu_fns.nopen-- <= 1) {
	int i;
	if (u_cleanup != NULL) {
	    u_cleanup();
	}
	for (i = 0; i < (int)(sizeof(icu_fns.libs) / sizeof(icu_fns.libs[0]));
	     ++i) {
	    if (icu_fns.libs[i] != NULL) {
		Tcl_FSUnloadFile(NULL, icu_fns.libs[i]);
	    }
	}
	memset(&icu_fns, 0, sizeof(icu_fns));
    }
    Tcl_MutexUnlock(&icu_mutex);
}

static void
TclIcuInit(
    Tcl_Interp *interp)
{
    Tcl_MutexLock(&icu_mutex);
    char symbol[256];
    char icuversion[4] = "_80"; /* Highest ICU version + 1 */

    /*
     * The initialization below clones the existing one from Tk. May need
     * revisiting.
     * ICU shared library names as well as function names *may* be versioned.
     * See https://unicode-org.github.io/icu/userguide/icu4c/packaging.html
     * for the gory details.
     */
    if (icu_fns.nopen == 0) {
	int i = 0;
	Tcl_Obj *nameobj;
	static const char *iculibs[] = {
#if defined(_WIN32)
#  define DLLNAME "icu%s%s.dll"
	    "icuuc??.dll", /* Windows, user-provided */
	    NULL,
	    "cygicuuc??.dll", /* When running under Cygwin */
#elif defined(__CYGWIN__)
#  define DLLNAME "cygicu%s%s.dll"
	    "cygicuuc??.dll",
#elif defined(MAC_OSX_TCL)
#  define DLLNAME "libicu%s.%s.dylib"
	    "libicuuc.??.dylib",
#else
#  define DLLNAME "libicu%s.so.%s"
	    "libicuuc.so.??",
#endif
	    NULL
	};

	/* Going back down to ICU version 60 */
	while ((icu_fns.libs[0] == NULL) && (icuversion[1] >= '6')) {
	    if (--icuversion[2] < '0') {
		icuversion[1]--; icuversion[2] = '9';
	    }
#if defined(__CYGWIN__)
	    i = 2;
#else
	    i = 0;
#endif
	    while (iculibs[i] != NULL) {
		Tcl_ResetResult(interp);
		nameobj = Tcl_NewStringObj(iculibs[i], TCL_INDEX_NONE);
		char *nameStr = Tcl_GetString(nameobj);
		char *p = strchr(nameStr, '?');
		if (p != NULL) {
		    memcpy(p, icuversion+1, 2);
		}
		Tcl_IncrRefCount(nameobj);
		if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[0])
			== TCL_OK) {
		    if (p == NULL) {
			icuversion[0] = '\0';
		    }
		    Tcl_DecrRefCount(nameobj);
		    break;
		}
		Tcl_DecrRefCount(nameobj);
		++i;
	    }
	}
	if (icu_fns.libs[0] != NULL) {
	    /* Loaded icuuc, load others with the same version */
	    nameobj = Tcl_ObjPrintf(DLLNAME, "i18n", icuversion+1);
	    Tcl_IncrRefCount(nameobj);
	    /* Ignore errors. Calls to contained functions will fail. */
	    (void) Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[1]);
	    Tcl_DecrRefCount(nameobj);
	}
#if defined(_WIN32)
	/*
	 * On Windows, if no ICU install found, look for the system's
	 * (Win10 1703 or later). There are two cases. Newer systems
	 * have icu.dll containing all functions. Older systems have
	 * icucc.dll and icuin.dll
	 */
	if (icu_fns.libs[0] == NULL) {
	    Tcl_ResetResult(interp);
		nameobj = Tcl_NewStringObj("icu.dll", TCL_INDEX_NONE);
		Tcl_IncrRefCount(nameobj);
		if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[0])
			== TCL_OK) {
		    /* Reload same for second set of functions. */
		    (void) Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[1]);
		    /* Functions do NOT have version suffixes */
		    icuversion[0] = '\0';
		}
		Tcl_DecrRefCount(nameobj);
	}
	if (icu_fns.libs[0] == NULL) {
	    /* No icu.dll. Try last fallback */
	    Tcl_ResetResult(interp);
	    nameobj = Tcl_NewStringObj("icuuc.dll", TCL_INDEX_NONE);
	    Tcl_IncrRefCount(nameobj);
	    if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[0])
		== TCL_OK) {
		Tcl_DecrRefCount(nameobj);
		nameobj = Tcl_NewStringObj("icuin.dll", TCL_INDEX_NONE);
		Tcl_IncrRefCount(nameobj);
		(void) Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.libs[1]);
		/* Functions do NOT have version suffixes */
		icuversion[0] = '\0';
	    }
	    Tcl_DecrRefCount(nameobj);
	}
#endif

    /* Try for symbol without version (Windows, FreeBSD), then with version */
#define ICUUC_SYM(name)                                                   \
    do {                                                                  \
	strcpy(symbol, #name);                                            \
	icu_fns._##name =                                                 \
	    (fn_##name)Tcl_FindSymbol(NULL, icu_fns.libs[0], symbol);     \
	if (icu_fns._##name == NULL) {                                    \
	    strcat(symbol, icuversion);                                   \
	    icu_fns._##name =                                             \
		(fn_##name)Tcl_FindSymbol(NULL, icu_fns.libs[0], symbol); \
	}                                                                 \
    } while (0)

	if (icu_fns.libs[0] != NULL) {
	    ICUUC_SYM(u_cleanup);
	    ICUUC_SYM(u_errorName);

	    ICUUC_SYM(ucnv_countAliases);
	    ICUUC_SYM(ucnv_countAvailable);
	    ICUUC_SYM(ucnv_getAlias);
	    ICUUC_SYM(ucnv_getAvailableName);

	    ICUUC_SYM(ubrk_open);
	    ICUUC_SYM(ubrk_close);
	    ICUUC_SYM(ubrk_preceding);
	    ICUUC_SYM(ubrk_following);
	    ICUUC_SYM(ubrk_previous);
	    ICUUC_SYM(ubrk_next);
	    ICUUC_SYM(ubrk_setText);

	    ICUUC_SYM(uenum_close);
	    ICUUC_SYM(uenum_count);
	    ICUUC_SYM(uenum_next);

#undef ICUUC_SYM
	}

#define ICUIN_SYM(name)                                                   \
    do {                                                                  \
	strcpy(symbol, #name);                                            \
	icu_fns._##name =                                                 \
	    (fn_##name)Tcl_FindSymbol(NULL, icu_fns.libs[1], symbol);     \
	if (icu_fns._##name == NULL) {                                    \
	    strcat(symbol, icuversion);                                   \
	    icu_fns._##name =                                             \
		(fn_##name)Tcl_FindSymbol(NULL, icu_fns.libs[1], symbol); \
	}                                                                 \
    } while (0)

	if (icu_fns.libs[1] != NULL) {
	    ICUIN_SYM(ucsdet_close);
	    ICUIN_SYM(ucsdet_detect);
	    ICUIN_SYM(ucsdet_detectAll);
	    ICUIN_SYM(ucsdet_getName);
	    ICUIN_SYM(ucsdet_getAllDetectableCharsets);
	    ICUIN_SYM(ucsdet_open);
	    ICUIN_SYM(ucsdet_setText);
#undef ICUIN_SYM
	}

    }
#undef ICU_SYM

    Tcl_MutexUnlock(&icu_mutex);

    if (icu_fns.libs[0] != NULL) {
	/*
	 * Note refcounts updated BEFORE command definition to protect
	 * against self redefinition.
	 */
	if (icu_fns.libs[1] != NULL) {
	    /* Commands needing both libraries */

	    /* Ref count number of commands */
	    icu_fns.nopen += 1;
	    Tcl_CreateObjCommand2(interp,
				 "::tcl::unsupported::icu::detect",
				 IcuDetectObjCmd,
				 0,
				 TclIcuCleanup);
	}
	/* Commands needing only libs[0] (icuuc) */

	/* Ref count number of commands */
	icu_fns.nopen += 2;
	Tcl_CreateObjCommand2(interp,
			     "::tcl::unsupported::icu::converters",
			     IcuConverterNamesObjCmd,
			     0,
			     TclIcuCleanup);
	Tcl_CreateObjCommand2(interp,
			     "::tcl::unsupported::icu::aliases",
			     IcuConverterAliasesObjCmd,
			     0,
			     TclIcuCleanup);
    }
}

/*
 *------------------------------------------------------------------------
 *
 * TclLoadIcuObjCmd --
 *
 *    Loads and initializes ICU
 *
 * Results:
 *    TCL_OK    - Success.
 *    TCL_ERROR - Error.
 *
 * Side effects:
 *    Interpreter result holds result or error message.
 *
 *------------------------------------------------------------------------
 */
int
TclLoadIcuObjCmd (
    TCL_UNUSED(void *),
    Tcl_Interp *interp,    /* Current interpreter. */
    Tcl_Size objc,              /* Number of arguments. */
    Tcl_Obj *const objv[]) /* Argument objects. */
{
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1 , objv, "");
	return TCL_ERROR;
    }
    TclIcuInit(interp);
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * coding: utf-8
 * End:
 */
