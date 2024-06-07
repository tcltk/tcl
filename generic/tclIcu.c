/*
 * tclIcu.c --
 *
 * 	tclIcu.c implements various Tcl commands that make use of
 *	the ICU library if present on the system.
 *	(Adapted from tkIcu.c)
 *
 * Copyright © 2021 Jan Nijtmans
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
    U_ZERO_ERRORZ              =  0     /**< No error, no warning. */
} UErrorCodex;

struct UCharsetDetector;
typedef struct UCharsetDetector UCharsetDetector;
struct UCharsetMatch;
typedef struct UCharsetMatch UCharsetMatch;

typedef void *(*fn_ubrk_open)(UBreakIteratorTypex, const char *,
	const uint16_t *, int32_t, UErrorCodex *);
typedef void	(*fn_ubrk_close)(void *);
typedef int32_t	(*fn_ubrk_preceding)(void *, int32_t);
typedef int32_t	(*fn_ubrk_following)(void *, int32_t);
typedef int32_t	(*fn_ubrk_previous)(void *);
typedef int32_t	(*fn_ubrk_next)(void *);
typedef void	(*fn_ubrk_setText)(void *, const void *, int32_t, UErrorCodex *);
typedef UCharsetDetector * (*fn_ucsdet_open)(UErrorCodex   *status);
typedef void    (*fn_ucsdet_close)(UCharsetDetector *ucsd);
typedef void (*fn_ucsdet_setText)(UCharsetDetector *ucsd, const char *textIn, int32_t len, UErrorCodex *status);
typedef const UCharsetMatch * (*fn_ucsdet_detect)(UCharsetDetector *ucsd, UErrorCodex *status);
typedef const UCharsetMatch ** (*fn_ucsdet_detectAll)(UCharsetDetector *ucsd, int32_t *matchesFound, UErrorCodex *status);
typedef const char * (*fn_ucsdet_getName)(const UCharsetMatch *ucsm, UErrorCodex *status);

static struct {
    size_t              nopen; /* Total number of references to ALL libraries */
    /*
     * Depending on platform, ICU symbols may be distributed amongst
     * multiple libraries. For current functionality at most 2 needed.
     * Order of library loading is not guaranteed.
     */
    Tcl_LoadHandle      libs[2];
    fn_ubrk_open        _ubrk_open;
    fn_ubrk_close       _ubrk_close;
    fn_ubrk_preceding   _ubrk_preceding;
    fn_ubrk_following   _ubrk_following;
    fn_ubrk_previous    _ubrk_previous;
    fn_ubrk_next        _ubrk_next;
    fn_ubrk_setText     _ubrk_setText;

    fn_ucsdet_open      _ucsdet_open;
    fn_ucsdet_close     _ucsdet_close;
    fn_ucsdet_setText   _ucsdet_setText;
    fn_ucsdet_detect    _ucsdet_detect;
    fn_ucsdet_detectAll _ucsdet_detectAll;
    fn_ucsdet_getName   _ucsdet_getName;
} icu_fns = {
    0, {NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* ubrk* */
    NULL, NULL, NULL, NULL, NULL, NULL,       /* ucsdet* */
};

#define FLAG_WORD 1
#define FLAG_FOLLOWING 4
#define FLAG_SPACE 8

#define ubrk_open	 icu_fns._ubrk_open
#define ubrk_close	 icu_fns._ubrk_close
#define ubrk_preceding	 icu_fns._ubrk_preceding
#define ubrk_following	 icu_fns._ubrk_following
#define ubrk_previous	 icu_fns._ubrk_previous
#define ubrk_next	 icu_fns._ubrk_next
#define ubrk_setText	 icu_fns._ubrk_setText
#define ucsdet_open      icu_fns._ucsdet_open
#define ucsdet_close     icu_fns._ucsdet_close
#define ucsdet_setText   icu_fns._ucsdet_setText
#define ucsdet_detect    icu_fns._ucsdet_detect
#define ucsdet_detectAll icu_fns._ucsdet_detectAll
#define ucsdet_getName   icu_fns._ucsdet_getName

TCL_DECLARE_MUTEX(icu_mutex);

static void
icuCleanup(
    TCL_UNUSED(void *))
{
    Tcl_MutexLock(&icu_mutex);
    if (icu_fns.nopen-- <= 1) {
        int i;
        for (i = 0; i < sizeof(icu_fns.libs)/sizeof(icu_fns.libs[0]); ++i) {
            if (icu_fns.libs[i] != NULL) {
                Tcl_FSUnloadFile(NULL, icu_fns.libs[i]);
            }
        }
	memset(&icu_fns, 0, sizeof(icu_fns));
    }
    Tcl_MutexUnlock(&icu_mutex);
}

void
TclIcuInit(
    Tcl_Interp *interp)
{
    Tcl_MutexLock(&icu_mutex);
    char symbol[24];
    char icuversion[4] = "_80"; /* Highest ICU version + 1 */

    /*
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
#if defined(_WIN32) && !defined(STATIC_BUILD)
	    if (tclStubsPtr->tcl_CreateFileHandler) {
		/* Running on Cygwin, so try to load the cygwin icu dll */
		i = 2;
	    } else
#endif
	    i = 0;
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

#define ICUUC_SYM(name)                                         \
        strcpy(symbol, #name );                                 \
        strcat(symbol, icuversion);                             \
        icu_fns._##name = (fn_ ## name)                         \
            Tcl_FindSymbol(NULL, icu_fns.libs[0], symbol)
        if (icu_fns.libs[0] != NULL) {
	    ICUUC_SYM(ubrk_open);
	    ICUUC_SYM(ubrk_close);
	    ICUUC_SYM(ubrk_preceding);
	    ICUUC_SYM(ubrk_following);
	    ICUUC_SYM(ubrk_previous);
	    ICUUC_SYM(ubrk_next);
	    ICUUC_SYM(ubrk_setText);
#undef ICUUC_SYM
	}

#define ICUIN_SYM(name)                                         \
        strcpy(symbol, #name );                                 \
        strcat(symbol, icuversion);                             \
        icu_fns._##name = (fn_ ## name)                         \
            Tcl_FindSymbol(NULL, icu_fns.libs[1], symbol)
        if (icu_fns.libs[0] != NULL) {
            ICUIN_SYM(ucsdet_open);
            ICUIN_SYM(ucsdet_close);
            ICUIN_SYM(ucsdet_detect);
            ICUIN_SYM(ucsdet_detectAll);
            ICUIN_SYM(ucsdet_setText);
            ICUIN_SYM(ucsdet_getName);
#undef ICUIN_SYM
        }

    }
#undef ICU_SYM

    Tcl_MutexUnlock(&icu_mutex);

    if (icu_fns.libs[0] != NULL) {
        /* Do not currently need anything from here */
    }
    if (icu_fns.libs[1] != NULL) {
	Tcl_CreateObjCommand(interp, "::tcl::unsupported::encdetect", EncodingDetectObjCmd,
                             INT2PTR(FLAG_FOLLOWING), icuCleanup);
        icu_fns.nopen += 1;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * coding: utf-8
 * End:
 */
