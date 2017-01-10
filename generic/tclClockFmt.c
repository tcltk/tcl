/*
 * tclClockFmt.c --
 *
 *	Contains the date format (and scan) routines. This code is back-ported
 *	from the time and date facilities of tclSE engine, by Serg G. Brester.
 *
 * Copyright (c) 2015 by Sergey G. Brester aka sebres. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclDate.h"

/*
 * Miscellaneous forward declarations and functions used within this file
 */

static void
ClockFmtObj_DupInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void
ClockFmtObj_FreeInternalRep(Tcl_Obj *objPtr);
static int
ClockFmtObj_SetFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void
ClockFmtObj_UpdateString(Tcl_Obj *objPtr);


TCL_DECLARE_MUTEX(ClockFmtMutex); /* Serializes access to common format list. */

static void ClockFmtScnStorageDelete(ClockFmtScnStorage *fss);

static void ClockFrmScnFinalize(ClientData clientData);

/*
 * Clock scan and format facilities.
 */

inline int
_str2int(
    time_t     *out,
    register 
    const char *p,
    const char *e,
    int sign)
{
    register time_t val = 0, prev = 0;
    if (sign > 0) {
	while (p < e) {
	    val = val * 10 + (*p++ - '0');
	    if (val < prev) {
		return TCL_ERROR;
	    }
	    prev = val;
	}
    } else {
	while (p < e) {
	    val = val * 10 - (*p++ - '0');
	    if (val > prev) {
		return TCL_ERROR;
	    }
	    prev = val;
	}
    }
    *out = val;
    return TCL_OK;
} 

inline int
_str2wideInt(
    Tcl_WideInt *out,
    register 
    const char	*p,
    const char	*e,
    int sign)
{
    register Tcl_WideInt val = 0, prev = 0;
    if (sign > 0) {
	while (p < e) {
	    val = val * 10 + (*p++ - '0');
	    if (val < prev) {
		return TCL_ERROR;
	    }
	    prev = val;
	}
    } else {
	while (p < e) {
	    val = val * 10 - (*p++ - '0');
	    if (val > prev) {
		return TCL_ERROR;
	    }
	    prev = val;
	}
    }
    *out = val;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 */

#if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0

static struct {
    ClockFmtScnStorage	  *stackPtr;
    ClockFmtScnStorage	  *stackBound;
    unsigned int	   count;
} ClockFmtScnStorage_GC = {NULL, NULL, 0};

inline void
ClockFmtScnStorageGC_In(ClockFmtScnStorage *entry) 
{
    /* add new entry */
    TclSpliceIn(entry, ClockFmtScnStorage_GC.stackPtr);
    if (ClockFmtScnStorage_GC.stackBound == NULL) {
	ClockFmtScnStorage_GC.stackBound = entry;
    }
    ClockFmtScnStorage_GC.count++;

    /* if GC ist full */
    if (ClockFmtScnStorage_GC.count > CLOCK_FMT_SCN_STORAGE_GC_SIZE) {

	/* GC stack is LIFO: delete first inserted entry */
	ClockFmtScnStorage *delEnt = ClockFmtScnStorage_GC.stackBound;
	ClockFmtScnStorage_GC.stackBound = delEnt->prevPtr;
	TclSpliceOut(delEnt, ClockFmtScnStorage_GC.stackPtr);
	ClockFmtScnStorage_GC.count--;
	delEnt->prevPtr = delEnt->nextPtr = NULL;
	/* remove it now */
	ClockFmtScnStorageDelete(delEnt);
    }
}
inline void
ClockFmtScnStorage_GC_Out(ClockFmtScnStorage *entry)
{
    TclSpliceOut(entry, ClockFmtScnStorage_GC.stackPtr);
    ClockFmtScnStorage_GC.count--;
    if (ClockFmtScnStorage_GC.stackBound == entry) {
	ClockFmtScnStorage_GC.stackBound = entry->prevPtr;
    }
    entry->prevPtr = entry->nextPtr = NULL;
}

#endif

/*
 *----------------------------------------------------------------------
 */

static Tcl_HashTable FmtScnHashTable;
static int	     initialized = 0;

inline Tcl_HashEntry *
HashEntry4FmtScn(ClockFmtScnStorage *fss) {
    return (Tcl_HashEntry*)(fss + 1);
};
inline ClockFmtScnStorage *
FmtScn4HashEntry(Tcl_HashEntry *hKeyPtr) {
    return (ClockFmtScnStorage*)(((char*)hKeyPtr) - sizeof(ClockFmtScnStorage));
};

/* 
 * Format storage hash (list of formats shared across all threads).
 */

static Tcl_HashEntry *
ClockFmtScnStorageAllocProc(
    Tcl_HashTable *tablePtr,	/* Hash table. */
    void *keyPtr)		/* Key to store in the hash table entry. */
{
    ClockFmtScnStorage *fss;

    const char *string = (const char *) keyPtr;
    Tcl_HashEntry *hPtr;
    unsigned int size, 
	allocsize = sizeof(ClockFmtScnStorage) + sizeof(Tcl_HashEntry);

    allocsize += (size = strlen(string) + 1);
    if (size > sizeof(hPtr->key)) {
	allocsize -= sizeof(hPtr->key);
    }

    fss = ckalloc(allocsize);

    /* initialize */
    memset(fss, 0, sizeof(*fss));

    hPtr = HashEntry4FmtScn(fss);
    memcpy(&hPtr->key.string, string, size);
    hPtr->clientData = 0; /* currently unused */

    return hPtr;
}

static void 
ClockFmtScnStorageFreeProc(
    Tcl_HashEntry *hPtr)
{
    ClockFmtScnStorage *fss = FmtScn4HashEntry(hPtr);

    if (fss->scnTok != NULL) {
	ckfree(fss->scnTok);
	fss->scnTok = NULL;
	fss->scnTokC = 0;
    }
    if (fss->fmtTok != NULL) {
	ckfree(fss->fmtTok);
	fss->fmtTok = NULL;
	fss->fmtTokC = 0;
    }

    ckfree(fss);
}

static void 
ClockFmtScnStorageDelete(ClockFmtScnStorage *fss) {
    Tcl_HashEntry   *hPtr = HashEntry4FmtScn(fss);
    /* 
     * This will delete a hash entry and call "ckfree" for storage self, if
     * some additionally handling required, freeEntryProc can be used instead
     */
    Tcl_DeleteHashEntry(hPtr);
}


/* 
 * Derivation of tclStringHashKeyType with another allocEntryProc
 */

static Tcl_HashKeyType ClockFmtScnStorageHashKeyType;


/*
 *----------------------------------------------------------------------
 */

static ClockFmtScnStorage *
FindOrCreateFmtScnStorage(
    Tcl_Interp *interp, 
    const char *strFmt)
{
    ClockFmtScnStorage *fss = NULL;
    int new;
    Tcl_HashEntry *hPtr;

    Tcl_MutexLock(&ClockFmtMutex);

    /* if not yet initialized */
    if (!initialized) {
	/* initialize type */
	memcpy(&ClockFmtScnStorageHashKeyType, &tclStringHashKeyType, sizeof(tclStringHashKeyType));
	ClockFmtScnStorageHashKeyType.allocEntryProc = ClockFmtScnStorageAllocProc;
	ClockFmtScnStorageHashKeyType.freeEntryProc = ClockFmtScnStorageFreeProc;

	/* initialize hash table */
	Tcl_InitCustomHashTable(&FmtScnHashTable, TCL_CUSTOM_TYPE_KEYS,
	    &ClockFmtScnStorageHashKeyType);

	initialized = 1;
	Tcl_CreateExitHandler(ClockFrmScnFinalize, NULL);
    }

    /* get or create entry (and alocate storage) */
    hPtr = Tcl_CreateHashEntry(&FmtScnHashTable, strFmt, &new);
    if (hPtr != NULL) {
	
	fss = FmtScn4HashEntry(hPtr);

	#if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0
	  /* unlink if it is currently in GC */
	  if (new == 0 && fss->objRefCount == 0) {
	      ClockFmtScnStorage_GC_Out(fss);
	  }
	#endif

	/* new reference, so increment in lock right now */
	fss->objRefCount++;
    }

    Tcl_MutexUnlock(&ClockFmtMutex);

    if (fss == NULL && interp != NULL) {
	Tcl_AppendResult(interp, "retrieve clock format failed \"",
	    strFmt ? strFmt : "", "\"", NULL);
	Tcl_SetErrorCode(interp, "TCL", "EINVAL", NULL);
    }

    return fss;
}


/*
 * Type definition.
 */

Tcl_ObjType ClockFmtObjType = {
    "clock-format",	       /* name */
    ClockFmtObj_FreeInternalRep, /* freeIntRepProc */
    ClockFmtObj_DupInternalRep,	 /* dupIntRepProc */
    ClockFmtObj_UpdateString,	 /* updateStringProc */
    ClockFmtObj_SetFromAny	 /* setFromAnyProc */
};

#define SetObjClockFmtScn(objPtr, fss) \
    objPtr->internalRep.twoPtrValue.ptr1 = fss
#define ObjClockFmtScn(objPtr) \
    (ClockFmtScnStorage *)objPtr->internalRep.twoPtrValue.ptr1;

#define SetObjLitStorage(objPtr, lit) \
    objPtr->internalRep.twoPtrValue.ptr2 = lit
#define ObjLitStorage(objPtr) \
    (ClockLitStorage *)objPtr->internalRep.twoPtrValue.ptr2;

#define ClockFmtObj_SetObjIntRep(objPtr, fss, lit) \
    objPtr->internalRep.twoPtrValue.ptr1 = fss, \
    objPtr->internalRep.twoPtrValue.ptr2 = lit, \
    objPtr->typePtr = &ClockFmtObjType;

/*
 *----------------------------------------------------------------------
 */
static void
ClockFmtObj_DupInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;
    Tcl_Obj *copyPtr;
{
    ClockFmtScnStorage *fss = ObjClockFmtScn(srcPtr);
    // ClockLitStorage	  *lit = ObjLitStorage(srcPtr);

    if (fss != NULL) {
	Tcl_MutexLock(&ClockFmtMutex);
	fss->objRefCount++;
	Tcl_MutexUnlock(&ClockFmtMutex);
    }

    ClockFmtObj_SetObjIntRep(copyPtr, fss, NULL);

    /* if no format representation, dup string representation */
    if (fss == NULL) {
	copyPtr->bytes = ckalloc(srcPtr->length + 1);
	memcpy(copyPtr->bytes, srcPtr->bytes, srcPtr->length + 1);
	copyPtr->length = srcPtr->length;
    }
}
/*
 *----------------------------------------------------------------------
 */
static void
ClockFmtObj_FreeInternalRep(objPtr)
    Tcl_Obj *objPtr;
{
    ClockFmtScnStorage *fss = ObjClockFmtScn(objPtr);
    // ClockLitStorage	  *lit = ObjLitStorage(objPtr);
    if (fss != NULL) {
	Tcl_MutexLock(&ClockFmtMutex);
	/* decrement object reference count of format/scan storage */
	if (--fss->objRefCount <= 0) {
	    #if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0
	      /* don't remove it right now (may be reusable), just add to GC */
	      ClockFmtScnStorageGC_In(fss);
	    #else 
	      /* remove storage (format representation) */
	      ClockFmtScnStorageDelete(fss);
	    #endif 
	}
	Tcl_MutexUnlock(&ClockFmtMutex);
    }
    SetObjClockFmtScn(objPtr, NULL);
    SetObjLitStorage(objPtr, NULL);
    objPtr->typePtr = NULL;
};
/*
 *----------------------------------------------------------------------
 */
static int
ClockFmtObj_SetFromAny(interp, objPtr)
    Tcl_Interp *interp;
    Tcl_Obj    *objPtr;
{
    ClockFmtScnStorage *fss;
    const char *strFmt = TclGetString(objPtr);
    
    if (!strFmt || (fss = FindOrCreateFmtScnStorage(interp, strFmt)) == NULL) {
	return TCL_ERROR;
    }
    
    if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc)
	objPtr->typePtr->freeIntRepProc(objPtr);
    ClockFmtObj_SetObjIntRep(objPtr, fss, NULL);
    return TCL_OK;
};
/*
 *----------------------------------------------------------------------
 */
static void
ClockFmtObj_UpdateString(objPtr)
    Tcl_Obj  *objPtr;
{
    char *name = "UNKNOWN";
    int	  len;
    ClockFmtScnStorage *fss = ObjClockFmtScn(objPtr);

    if (fss != NULL) {
	Tcl_HashEntry *hPtr = HashEntry4FmtScn(fss);
	name = hPtr->key.string;
    }
    len = strlen(name);
    objPtr->length = len,
    objPtr->bytes = ckalloc((size_t)++len);
    if (objPtr->bytes)
	memcpy(objPtr->bytes, name, len);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetClockFrmScnFromObj --
 *
 *  Returns a clock format/scan representation of (*objPtr), if possible.
 *  If something goes wrong, NULL is returned, and if interp is non-NULL,
 *  an error message is written there.
 *
 * Results:
 *  Valid representation of type ClockFmtScnStorage.
 *
 * Side effects:
 *  Caches the ClockFmtScnStorage reference as the internal rep of (*objPtr)
 *  and in global hash table, shared across all threads.
 *
 *----------------------------------------------------------------------
 */

ClockFmtScnStorage *
Tcl_GetClockFrmScnFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    ClockFmtScnStorage *fss;
    
    if (objPtr->typePtr != &ClockFmtObjType) {
	if (ClockFmtObj_SetFromAny(interp, objPtr) != TCL_OK) {
	    return NULL;
	}
    }

    fss = ObjClockFmtScn(objPtr);

    if (fss == NULL) {
	const char *strFmt = TclGetString(objPtr);
	fss = FindOrCreateFmtScnStorage(interp, strFmt);
    }

    return fss;
}


static int 
LocaleListSearch(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, const char * mcKey, int *val)
{
    Tcl_Obj *callargs[4] = {NULL, NULL, NULL, NULL}, **lstv;
    int	     i, lstc;
    Tcl_Obj *valObj = NULL;
    int ret = TCL_RETURN;

    /* get msgcat value */
    TclNewLiteralStringObj(callargs[0], "::msgcat::mcget");
    TclNewLiteralStringObj(callargs[1], "::tcl::clock");
    callargs[2] = opts->localeObj;
    if (opts->localeObj == NULL) {
	TclNewLiteralStringObj(callargs[1], "c");
    }
    callargs[3] = Tcl_NewStringObj(mcKey, -1);

    for (i = 0; i < 4; i++) {
	Tcl_IncrRefCount(callargs[i]);
    }
    if (Tcl_EvalObjv(opts->interp, 4, callargs, 0) != TCL_OK) {
	goto done;
    }

    Tcl_InitObjRef(valObj, Tcl_GetObjResult(opts->interp));

    /* is a list */
    if (TclListObjGetElements(opts->interp, valObj, &lstc, &lstv) != TCL_OK) {
	goto done;
    }

    /* search in list */
    for (i = 0; i < lstc; i++) {
	const char *s = TclGetString(lstv[i]);
	int	    l = lstv[i]->length;
	if ( l <= info->dateEnd - yyInput
	  && strncasecmp(yyInput, s, l) == 0
	) {
	    *val = i;
	    yyInput += l;
	    break;
	}
    }

    ret = TCL_OK;
    /* if not found */
    if (i >= lstc) {
	ret = TCL_ERROR;
    }

done:

    Tcl_UnsetObjRef(valObj);

    for (i = 0; i < 4; i++) {
	Tcl_UnsetObjRef(callargs[i]);
    }

    return ret;
}

static int 
StaticListSearch(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, const char **lst, int *val)
{
    int len;
    const char **s = lst;
    while (*s != NULL) {
	len = strlen(*s);
	if ( len <= info->dateEnd - yyInput 
	  && strncasecmp(yyInput, *s, len) == 0
	) {
	    *val = (s - lst);
	    yyInput += len;
	    break;
	}
	s++;
    }
    if (*s == NULL) {
	return TCL_ERROR;
    }

    return TCL_OK;
};

static int 
ClockScnToken_Month_Proc(ClockFmtScnCmdArgs *opts,
    DateInfo *info, ClockScanToken *tok)
{
    /*
    static const char * months[] = {
	/* full * /
	"January", "February", "March",
	"April",   "May",      "June",
	"July",	   "August",   "September",
	"October", "November", "December",
	/* abbr * /
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	NULL
    };
    int val;
    if (StaticListSearch(opts, info, months, &val) != TCL_OK) {
	return TCL_ERROR;
    }
    yyMonth = (val % 12) + 1;
    return TCL_OK;
    */

    int val;
    int ret = LocaleListSearch(opts, info, "MONTHS_FULL", &val);
    if (ret != TCL_OK) {
	if (ret == TCL_RETURN) {
	    return ret;
	}
	ret = LocaleListSearch(opts, info, "MONTHS_ABBREV", &val);
	if (ret != TCL_OK) {
	    return ret;
	}
    }

    yyMonth = val + 1;
    return TCL_OK;

}


static int 
ClockScnToken_LocaleListMatcher_Proc(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, ClockScanToken *tok)
{
    int	     val;

    int ret = LocaleListSearch(opts, info, (char *)tok->map->data, &val);
    if (ret != TCL_OK) {
	return ret;
    }

    *(time_t *)(((char *)info) + tok->map->offs) = val;

    return TCL_OK;
}


static const char *ScnSTokenMapIndex = 
    "dmbyYHMSJCs";
static ClockScanTokenMap ScnSTokenMap[] = {
    /* %d %e */
    {CTOKT_DIGIT, CLF_DATE, 1, 2, TclOffset(DateInfo, date.dayOfMonth),
	NULL},
    /* %m */
    {CTOKT_DIGIT, CLF_DATE, 1, 2, TclOffset(DateInfo, date.month),
	NULL},
    /* %b %B %h */
    {CTOKT_PARSER, CLF_DATE, 0, 0, 0,
	    ClockScnToken_Month_Proc},
    /* %y */
    {CTOKT_DIGIT, CLF_DATE, 1, 2, TclOffset(DateInfo, date.year),
	NULL},
    /* %Y */
    {CTOKT_DIGIT, CLF_DATE | CLF_CENTURY, 1, 4, TclOffset(DateInfo, date.year),
	NULL},
    /* %H */
    {CTOKT_DIGIT, CLF_TIME, 1, 2, TclOffset(DateInfo, date.hour),
	NULL},
    /* %M */
    {CTOKT_DIGIT, CLF_TIME, 1, 2, TclOffset(DateInfo, date.minutes),
	NULL},
    /* %S */
    {CTOKT_DIGIT, CLF_TIME, 1, 2, TclOffset(DateInfo, date.secondOfDay),
	NULL},
    /* %J */
    {CTOKT_DIGIT, CLF_DATE | CLF_JULIANDAY,  1, 0xffff, TclOffset(DateInfo, date.julianDay),
	NULL},
    /* %C */
    {CTOKT_DIGIT, CLF_DATE | CLF_CENTURY,    1, 2, TclOffset(DateInfo, dateCentury),
	NULL},
    /* %s */
    {CTOKT_DIGIT, CLF_LOCALSEC | CLF_SIGNED, 1, 0xffff, TclOffset(DateInfo, date.localSeconds),
	NULL},
};
static const char *ScnSTokenWrapMapIndex[2] = {
    "eBh",
    "dbb"
};

static const char *ScnETokenMapIndex = 
    "";
static ClockScanTokenMap ScnETokenMap[] = {
    {0, 0, 0}
};
static const char *ScnETokenWrapMapIndex[2] = {
    "",
    ""
};

static const char *ScnOTokenMapIndex = 
    "d";
static ClockScanTokenMap ScnOTokenMap[] = {
    /* %Od %Oe */
    {CTOKT_PARSER, CLF_DATE, 0, 0, TclOffset(DateInfo, date.dayOfMonth),
	ClockScnToken_LocaleListMatcher_Proc, "LOCALE_NUMERALS"},
};
static const char *ScnOTokenWrapMapIndex[2] = {
    "e",
    "d"
};

static const char *ScnSpecTokenMapIndex = 
    " ";
static ClockScanTokenMap ScnSpecTokenMap[] = {
    {CTOKT_SPACE,  0,	    1, 0xffff, 0, 
	NULL},
};

static ClockScanTokenMap ScnWordTokenMap = {
    CTOKT_WORD,	   0,	    1, 0, 0, 
	NULL
};


#define AllocTokenInChain(tok, chain, tokCnt) \
    if (++(tok) >= (chain) + (tokCnt)) { \
	(char *)(chain) = ckrealloc((char *)(chain), \
	    (tokCnt + CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE) * sizeof(*(tok))); \
	if ((chain) == NULL) { goto done; }; \
	(tok) = (chain) + (tokCnt); \
	(tokCnt) += CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE; \
    } \
    memset(tok, 0, sizeof(*(tok)));

/*
 *----------------------------------------------------------------------
 */
ClockScanToken *
ClockGetOrParseScanFormat(
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *formatObj)		/* Format container */
{
    ClockFmtScnStorage *fss;
    ClockScanToken     *tok;

    if (formatObj->typePtr != &ClockFmtObjType) {
	if (ClockFmtObj_SetFromAny(interp, formatObj) != TCL_OK) {
	    return NULL;
	}
    }

    fss = ObjClockFmtScn(formatObj);

    if (fss == NULL) {
	fss = FindOrCreateFmtScnStorage(interp, TclGetString(formatObj));
	if (fss == NULL) {
	    return NULL;
	}
    }

    /* if first time scanning - tokenize format */
    if (fss->scnTok == NULL) {
	const char *strFmt;
	register const char *p, *e, *cp;

	Tcl_MutexLock(&ClockFmtMutex);

	fss->scnTokC = CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE;
	fss->scnTok =
		tok = ckalloc(sizeof(*tok) * CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE);
	memset(tok, 0, sizeof(*(tok)));
	strFmt = HashEntry4FmtScn(fss)->key.string;
	for (e = p = strFmt, e += strlen(strFmt); p != e; p++) {
	    switch (*p) {
	    case '%':
	    if (1) {
		ClockScanTokenMap * maps = ScnSTokenMap;
		const char *mapIndex =	ScnSTokenMapIndex,
			  **wrapIndex = ScnSTokenWrapMapIndex;
		if (p+1 >= e) {
		    goto word_tok;
		}
		p++;
		/* try to find modifier: */
		switch (*p) {
		case '%':
		    /* begin new word token - don't join with previous word token, 
		     * because current mapping should be "...%%..." -> "...%..." */
		    tok->map = &ScnWordTokenMap;
		    tok->tokWord.start = tok->tokWord.end = p;
		    AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
		    continue;
		break;
		case 'E':
		    maps = ScnETokenMap, 
		    mapIndex =	ScnETokenMapIndex,
		    wrapIndex = ScnETokenWrapMapIndex;
		    p++;
		break;
		case 'O':
		    maps = ScnOTokenMap,
		    mapIndex = ScnOTokenMapIndex,
		    wrapIndex = ScnOTokenWrapMapIndex;
		    p++;
		break;
		}
		/* search direct index */
		cp = strchr(mapIndex, *p);
		if (!cp || *cp == '\0') {
		    /* search wrapper index (multiple chars for same token) */
		    cp = strchr(wrapIndex[0], *p);
		    if (!cp || *cp == '\0') {
			p--;
			goto word_tok;
		    }
		    cp = strchr(mapIndex, wrapIndex[1][cp - wrapIndex[0]]);
		    if (!cp || *cp == '\0') { /* unexpected, but ... */
		    #ifdef DEBUG
			Tcl_Panic("token \"%c\" has no map in wrapper resolver", *p);
		    #endif
			p--;
			goto word_tok;
		    }
		}
		tok->map = &ScnSTokenMap[cp - mapIndex];
		tok->tokWord.start = p;
		/* calculate look ahead value by standing together tokens */
		if (tok > fss->scnTok) {
		    ClockScanToken     *prevTok = tok - 1;
		    unsigned int	lookAhead = tok->map->minSize;

		    while (prevTok >= fss->scnTok) {
			if (prevTok->map->type != tok->map->type) {
			    break;
			}
			prevTok->lookAhead += lookAhead;
			prevTok--;
		    }
		}
		/* next token */
		AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
	    }
	    break;
	    case ' ':
		cp = strchr(ScnSpecTokenMapIndex, *p);
		if (!cp || *cp == '\0') {
		    p--;
		    goto word_tok;
		}
		tok->map = &ScnSpecTokenMap[cp - ScnSpecTokenMapIndex];
		AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
	    break;
	    default:
word_tok:
	    if (1) {
		ClockScanToken	 *wordTok = tok;
		if (tok > fss->scnTok && (tok-1)->map == &ScnWordTokenMap) {
		    wordTok = tok-1;
		}
		wordTok->tokWord.end = p;
		if (wordTok == tok) {
		    wordTok->tokWord.start = p;
		    wordTok->map = &ScnWordTokenMap;
		    AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
		}
		continue;
	    }
	    break;
	    }

	    continue;
	}

done:
	Tcl_MutexUnlock(&ClockFmtMutex);
    }

    return fss->scnTok;
}

/*
 *----------------------------------------------------------------------
 */
int
ClockScan(
    ClientData clientData,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    register DateInfo *info,	/* Date fields used for parsing & converting */
    Tcl_Obj *strObj,		/* String containing the time to scan */
    ClockFmtScnCmdArgs *opts)	/* Command options */
{
    ClockClientData *dataPtr = clientData;
    ClockScanToken	*tok;
    ClockScanTokenMap	*map;
    register const char *p, *x, *end;
    unsigned short int	 flags = 0;
    int ret = TCL_ERROR;

    if ((tok = ClockGetOrParseScanFormat(interp, opts->formatObj)) == NULL) {
	return TCL_ERROR;
    }

    /* prepare parsing */

    yyMeridian = MER24;

    p = TclGetString(strObj);
    end = p + strObj->length;
    /* in strict mode - bypass spaces at begin / end only (not between tokens) */
    if (opts->flags & CLF_STRICT) {
	while (p < end && isspace(UCHAR(*p))) {
	    p++;
	}
    }
    info->dateStart = yyInput = p;
    info->dateEnd = end;
    
    /* parse string */
    for (; tok->map != NULL; tok++) {
	map = tok->map;
	/* bypass spaces at begin of input before parsing each token */
	if ( !(opts->flags & CLF_STRICT) 
	  && (map->type != CTOKT_SPACE && map->type != CTOKT_WORD)
	) {
	    while (p < end && isspace(UCHAR(*p))) {
		p++;
	    }
	}
	yyInput = p;
	switch (map->type)
	{
	case CTOKT_DIGIT:
	if (1) {
	    int size = map->maxSize;
	    int sign = 1;
	    if (map->flags & CLF_SIGNED) {
		if (*p == '+') { yyInput = ++p; }
		else
		if (*p == '-') { yyInput = ++p; sign = -1; };
	    }
	    /* greedy find digits (look for forward digits consider spaces), 
	     * corresponding pre-calculated lookAhead */
	    if (size != map->minSize && tok->lookAhead) {
		int spcnt = 0;
		const char *pe;
		size += tok->lookAhead;
		x = p + size; if (x > end) { x = end; };
		pe = x;
		while (p < x) {
		    if (isspace(UCHAR(*p))) {
			if (pe > p) { pe = p; };
			if (x < end) x++;
			p++;
			spcnt++;
			continue;
		    }
		    if (isdigit(UCHAR(*p))) {
			p++;
			continue;
		    }
		    break;
		}
		/* consider reserved (lookAhead) for next tokens */
		p -= tok->lookAhead + spcnt;
		if (p > pe) {
		    p = pe;
		}
	    } else {
		x = p + size; if (x > end) { x = end; };
		while (isdigit(UCHAR(*p)) && p < x) { p++; };
	    }
	    size = p - yyInput;
	    if (size < map->minSize) {
		/* missing input -> error */
		goto error;
	    }
	    /* string 2 number, put number into info structure by offset */
	    p = yyInput; x = p + size;
	    if (!(map->flags & CLF_LOCALSEC)) {
		if (_str2int((time_t *)(((char *)info) + map->offs), 
			p, x, sign) != TCL_OK) {
		    goto overflow;
		}
		p = x;
	    } else {
		if (_str2wideInt((Tcl_WideInt *)(((char *)info) + map->offs), 
			p, x, sign) != TCL_OK) {
		    goto overflow;
		}
		p = x;
	    }
	    flags |= map->flags;
	}
	break;
	case CTOKT_PARSER:
	    switch (map->parser(opts, info, tok)) {
		case TCL_OK:
		break;
		case TCL_RETURN:
		    goto done;
		break;
		default:
		    goto error;
		break;
	    };
	    p = yyInput;
	    flags |= map->flags;
	break;
	case CTOKT_SPACE:
	    /* at least one space in strict mode */
	    if (opts->flags & CLF_STRICT) {
		if (!isspace(UCHAR(*p))) {
		    /* unmatched -> error */
		    goto error;
		}
		p++;
	    }
	    while (p < end && isspace(UCHAR(*p))) {
		p++;
	    }
	break;
	case CTOKT_WORD:
	    x = tok->tokWord.start;
	    if (x == tok->tokWord.end) { /* single char word */
		if (*p != *x) {
		    /* no match -> error */
		    goto error;
		}
		p++;
		continue;
	    }
	    /* multi-char word */
	    while (p < end && x < tok->tokWord.end && *p++ == *x++) {};
	    if (x < tok->tokWord.end) {
		/* no match -> error */
		goto error;
	    }
	break;
	}
    }
    
    /* ignore spaces at end */
    while (p < end && isspace(UCHAR(*p))) {
	p++;
    }
    /* check end was reached */
    if (p < end) {
	/* something after last token - wrong format */
	goto error;
    }

    /* invalidate result */
    if (flags & CLF_DATE) {

	if (!(flags & CLF_JULIANDAY)) {
	    info->flags |= CLF_INVALIDATE_SECONDS|CLF_INVALIDATE_JULIANDAY;

	    if (yyYear < 100) {
		if (!(flags & CLF_CENTURY)) {
		    if (yyYear >= dataPtr->yearOfCenturySwitch) {
			yyYear -= 100;
		    }
		    yyYear += dataPtr->currentYearCentury;
		} else {
		    yyYear += info->dateCentury * 100;
		}
	    }
	    yydate.era = CE;
	}
	/* if date but no time - reset time */
	if (!(flags & (CLF_TIME|CLF_LOCALSEC))) {
	    info->flags |= CLF_INVALIDATE_SECONDS;
	    yydate.localSeconds = 0;
	}
    }

    if (flags & CLF_TIME) {
	info->flags |= CLF_INVALIDATE_SECONDS;
	yySeconds = ToSeconds(yyHour, yyMinutes,
			    yySeconds, yyMeridian);
    } else
    if (!(flags & CLF_LOCALSEC)) {
	info->flags |= CLF_INVALIDATE_SECONDS;
	yySeconds = yydate.localSeconds % SECONDS_PER_DAY;
    }

    ret = TCL_OK;
    goto done;

overflow:

    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	"integer value too large to represent", -1));
    Tcl_SetErrorCode(interp, "CLOCK", "integervalueTooLarge", NULL);
    goto done;

error:

    Tcl_SetResult(interp,
	"input string does not match supplied format", TCL_STATIC);
    Tcl_SetErrorCode(interp, "CLOCK", "badInputString", NULL);

done:

    return ret;
}


MODULE_SCOPE void
ClockFrmScnClearCaches(void)
{
    Tcl_MutexLock(&ClockFmtMutex);
    /* clear caches ... */
    Tcl_MutexUnlock(&ClockFmtMutex);
}

static void
ClockFrmScnFinalize(
    ClientData clientData)  /* Not used. */
{
    Tcl_MutexLock(&ClockFmtMutex);
#if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0
    /* clear GC */
    ClockFmtScnStorage_GC.stackPtr = NULL;
    ClockFmtScnStorage_GC.stackBound = NULL;
    ClockFmtScnStorage_GC.count = 0;
#endif
    if (initialized) {
	Tcl_DeleteHashTable(&FmtScnHashTable);
	initialized = 0;
    }
    Tcl_MutexUnlock(&ClockFmtMutex);
    Tcl_MutexFinalize(&ClockFmtMutex);
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
