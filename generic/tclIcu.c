/*
 * tkIcu.c --
 *
 * 	tkIcu.c implements various Tk commands which can find
 * 	grapheme cluster and workchar bounderies in Unicode strings.
 *
 * Copyright © 2021 Jan Nijtmans
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"

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

typedef void *(*fn_icu_open)(UBreakIteratorTypex, const char *,
	const uint16_t *, int32_t, UErrorCodex *);
typedef void	(*fn_icu_close)(void *);
typedef int32_t	(*fn_icu_preceding)(void *, int32_t);
typedef int32_t	(*fn_icu_following)(void *, int32_t);
typedef int32_t	(*fn_icu_previous)(void *);
typedef int32_t	(*fn_icu_next)(void *);
typedef void	(*fn_icu_setText)(void *, const void *, int32_t, UErrorCodex *);

static struct {
    size_t				nopen;
    Tcl_LoadHandle		lib;
    fn_icu_open			open;
    fn_icu_close		close;
    fn_icu_preceding	preceding;
    fn_icu_following	following;
    fn_icu_previous	previous;
    fn_icu_next	next;
    fn_icu_setText	setText;
} icu_fns = {
    0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

#define FLAG_WORD 1
#define FLAG_FOLLOWING 4
#define FLAG_SPACE 8

#define icu_open			icu_fns.open
#define icu_close			icu_fns.close
#define icu_preceding		icu_fns.preceding
#define icu_following		icu_fns.following
#define icu_previous		icu_fns.previous
#define icu_next		icu_fns.next
#define icu_setText		icu_fns.setText

TCL_DECLARE_MUTEX(icu_mutex);

static int
startEndOfCmd(
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_DString ds;
    Tcl_Size len;
    const char *str;
    UErrorCodex errorCode = U_ZERO_ERRORZ;
    void *it;
    Tcl_Size idx;
    int flags = PTR2INT(clientData);
    const uint16_t *ustr;
    const char *locale = NULL;

    if ((unsigned)(objc - 3) > 1) {
	Tcl_WrongNumArgs(interp, 1 , objv, "str start ?locale?");
	return TCL_ERROR;
    }
    if (objc > 3) {
	locale = Tcl_GetString(objv[3]);
	if (!*locale) {
	    locale = NULL;
	}
    }
    Tcl_DStringInit(&ds);
    str = Tcl_GetStringFromObj(objv[1], &len);
    Tcl_UtfToChar16DString(str, len, &ds);
    len = Tcl_DStringLength(&ds)/2;
    Tcl_Size ulen = Tcl_GetCharLength(objv[1]);
    if (TkGetIntForIndex(objv[2], ulen-1, 0, &idx) != TCL_OK) {
	Tcl_DStringFree(&ds);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad index \"%s\": must be integer?[+-]integer?, end?[+-]integer?, or \"\"", Tcl_GetString(objv[2])));
	Tcl_SetErrorCode(interp, "TK", "ICU", "INDEX", (char *)NULL);
	return TCL_ERROR;
    }
    it = icu_open((UBreakIteratorTypex)(flags&3), locale,
    		NULL, -1, &errorCode);
    if (it != NULL) {
	errorCode = U_ZERO_ERRORZ;
	ustr = (const uint16_t *)Tcl_DStringValue(&ds);
	icu_setText(it, ustr, len, &errorCode);
    }
    if (it == NULL || errorCode != U_ZERO_ERRORZ) {
    	Tcl_DStringFree(&ds);
    	Tcl_SetObjResult(interp, Tcl_ObjPrintf("cannot open ICU iterator, errorcode: %d", (int)errorCode));
    	Tcl_SetErrorCode(interp, "TK", "ICU", "CANNOTOPEN", (char *)NULL);
    	return TCL_ERROR;
    }
    if (idx > 0 && len != ulen) {
	/* The string contains codepoints > \uFFFF. Determine UTF-16 index */
	Tcl_Size newIdx = 0;
	for (Tcl_Size i = 0; i < idx; i++) {
	    newIdx += 1 + (((newIdx < (Tcl_Size)len-1) && (ustr[newIdx]&0xFC00) == 0xD800) && ((ustr[newIdx+1]&0xFC00) == 0xDC00));
	}
	idx = newIdx;
    }
    if (flags & FLAG_FOLLOWING) {
	if ((idx < 0) && (flags & FLAG_WORD)) {
	    idx = 0;
	}
	idx = icu_following(it, idx);
	if ((flags & FLAG_WORD) && idx >= len) {
	    idx = -1;
	}
    } else if (idx > 0) {
	if (!(flags & FLAG_WORD)) {
	    idx += 1 + (((ustr[idx]&0xFC00) == 0xD800) && ((ustr[idx+1]&0xFC00) == 0xDC00));
	}
	idx = icu_preceding(it, idx);
	if (idx == 0 && (flags & FLAG_WORD)) {
	    flags &= ~FLAG_WORD; /* If 0 is reached here, don't do a further search */
	}
    }
    if ((flags & FLAG_WORD) && (idx != TCL_INDEX_NONE)) {
	if (!(flags & FLAG_SPACE) == ((idx >= len) || Tcl_UniCharIsSpace(ustr[idx]))) {
	    if (flags & FLAG_FOLLOWING) {
		idx = icu_next(it);
		if (idx >= len) {
		    idx = TCL_INDEX_NONE;
		}
	    } else {
		idx = icu_previous(it);
	    }
	} else if (idx == 0 && !(flags & FLAG_FOLLOWING)) {
	    idx = TCL_INDEX_NONE;
	}
    }
    icu_close(it);
    Tcl_DStringFree(&ds);
    if (idx != TCL_INDEX_NONE) {
	if (idx > 0 && len != ulen) {
	    /* The string contains codepoints > \uFFFF. Determine UTF-32 index */
	    Tcl_Size newIdx = 1;
	    for (Tcl_Size i = 1; i < idx; i++) {
    	if (((ustr[i-1]&0xFC00) != 0xD800) || ((ustr[i]&0xFC00) != 0xDC00)) newIdx++;
	    }
	    idx = newIdx;
	}
	Tcl_SetObjResult(interp, TkNewIndexObj(idx));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SysNotifyDeleteCmd --
 *
 *      Delete notification and clean up.
 *
 * Results:
 *	Window destroyed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
icuCleanup(
    TCL_UNUSED(void *))
{
    Tcl_MutexLock(&icu_mutex);
    if (icu_fns.nopen-- <= 1) {
	if (icu_fns.lib != NULL) {
	    Tcl_FSUnloadFile(NULL, icu_fns.lib);
	}
	memset(&icu_fns, 0, sizeof(icu_fns));
    }
    Tcl_MutexUnlock(&icu_mutex);
}

void
Icu_Init(
    Tcl_Interp *interp)
{
    Tcl_MutexLock(&icu_mutex);
    char symbol[24];
    char icuversion[4] = "_80"; /* Highest ICU version + 1 */

    if (icu_fns.nopen == 0) {
	int i = 0;
	Tcl_Obj *nameobj;
	static const char *iculibs[] = {
#if defined(_WIN32)
	    "icuuc??.dll", /* When running under Windows, user-provided */
	    NULL,
	    "cygicuuc??.dll", /* When running under Cygwin */
#elif defined(__CYGWIN__)
	    "cygicuuc??.dll",
#elif defined(MAC_OSX_TCL)
	    "libicuuc.??.dylib",
#else
	    "libicuuc.so.??",
#endif
	    NULL
	};

	/* Going back down to ICU version 60 */
	while ((icu_fns.lib == NULL) && (icuversion[1] >= '6')) {
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
		if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.lib)
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
#if defined(_WIN32)
	if (icu_fns.lib == NULL) {
	    Tcl_ResetResult(interp);
		nameobj = Tcl_NewStringObj("icu.dll", TCL_INDEX_NONE);
		Tcl_IncrRefCount(nameobj);
		if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.lib)
			== TCL_OK) {
		    icuversion[0] = '\0';
		}
		Tcl_DecrRefCount(nameobj);
	}
	if (icu_fns.lib == NULL) {
	    Tcl_ResetResult(interp);
		nameobj = Tcl_NewStringObj("icuuc.dll", TCL_INDEX_NONE);
		Tcl_IncrRefCount(nameobj);
		if (Tcl_LoadFile(interp, nameobj, NULL, 0, NULL, &icu_fns.lib)
			== TCL_OK) {
		    icuversion[0] = '\0';
		}
		Tcl_DecrRefCount(nameobj);
	}
#endif
	if (icu_fns.lib != NULL) {
#define ICU_SYM(name)							\
	    strcpy(symbol, "ubrk_" #name ); \
	    strcat(symbol, icuversion); \
	    icu_fns.name = (fn_icu_ ## name)				\
		Tcl_FindSymbol(NULL, icu_fns.lib, symbol)
	    ICU_SYM(open);
	    ICU_SYM(close);
	    ICU_SYM(preceding);
	    ICU_SYM(following);
	    ICU_SYM(previous);
	    ICU_SYM(next);
	    ICU_SYM(setText);
#undef ICU_SYM
	}
    }
    Tcl_MutexUnlock(&icu_mutex);

    if (icu_fns.lib != NULL) {
	Tcl_CreateObjCommand(interp, "::tk::startOfCluster", startEndOfCmd,
		INT2PTR(0), icuCleanup);
	Tcl_CreateObjCommand(interp, "::tk::startOfNextWord", startEndOfCmd,
		INT2PTR(FLAG_WORD|FLAG_FOLLOWING), icuCleanup);
	Tcl_CreateObjCommand(interp, "::tk::startOfPreviousWord", startEndOfCmd,
		INT2PTR(FLAG_WORD), icuCleanup);
	Tcl_CreateObjCommand(interp, "::tk::endOfCluster", startEndOfCmd,
		INT2PTR(FLAG_FOLLOWING), icuCleanup);
	Tcl_CreateObjCommand(interp, "::tk::endOfWord", startEndOfCmd,
		INT2PTR(FLAG_WORD|FLAG_FOLLOWING|FLAG_SPACE), icuCleanup);
    icu_fns.nopen += 5;
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

