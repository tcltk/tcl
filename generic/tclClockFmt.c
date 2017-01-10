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
#include "tclStrIdxTree.h"
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

/* Msgcat index literals prefixed with _IDX_, used for quick dictionary search */
CLOCK_LOCALE_LITERAL_ARRAY(MsgCtLitIdxs, "_IDX_");

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
 * Type definition.
 */

Tcl_ObjType ClockFmtObjType = {
    "clock-format",	       /* name */
    ClockFmtObj_FreeInternalRep, /* freeIntRepProc */
    ClockFmtObj_DupInternalRep,	 /* dupIntRepProc */
    ClockFmtObj_UpdateString,	 /* updateStringProc */
    ClockFmtObj_SetFromAny	 /* setFromAnyProc */
};

#define ObjClockFmtScn(objPtr) \
    (*((ClockFmtScnStorage **)&(objPtr)->internalRep.twoPtrValue.ptr1))

#define ObjLocFmtKey(objPtr) \
    (*((Tcl_Obj **)&(objPtr)->internalRep.twoPtrValue.ptr2))

/*
 *----------------------------------------------------------------------
 */
static void
ClockFmtObj_DupInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;
    Tcl_Obj *copyPtr;
{
    ClockFmtScnStorage *fss = ObjClockFmtScn(srcPtr);

    if (fss != NULL) {
	Tcl_MutexLock(&ClockFmtMutex);
	fss->objRefCount++;
	Tcl_MutexUnlock(&ClockFmtMutex);
    }

    ObjClockFmtScn(copyPtr) = fss;
    /* regards special case - format not localizable */
    if (ObjLocFmtKey(srcPtr) != srcPtr) {
	Tcl_InitObjRef(ObjLocFmtKey(copyPtr), ObjLocFmtKey(srcPtr));
    } else {
	ObjLocFmtKey(copyPtr) = copyPtr;
    }
    copyPtr->typePtr = &ClockFmtObjType;


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
    ObjClockFmtScn(objPtr) = NULL;
    if (ObjLocFmtKey(objPtr) != objPtr) {
	Tcl_UnsetObjRef(ObjLocFmtKey(objPtr));
    } else {
	ObjLocFmtKey(objPtr) = NULL;
    }
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
    /* validate string representation before free old internal represenation */
    (void)TclGetString(objPtr);

    /* free old internal represenation */
    if (objPtr->typePtr && objPtr->typePtr->freeIntRepProc)
	objPtr->typePtr->freeIntRepProc(objPtr);

    /* initial state of format object */
    ObjClockFmtScn(objPtr) = NULL;
    ObjLocFmtKey(objPtr) = NULL;
    objPtr->typePtr = &ClockFmtObjType;

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
 */
MODULE_SCOPE Tcl_Obj*
ClockFrmObjGetLocFmtKey(
    Tcl_Interp *interp,
    Tcl_Obj    *objPtr)
{
    Tcl_Obj *keyObj;

    if (objPtr->typePtr != &ClockFmtObjType) {
	if (ClockFmtObj_SetFromAny(interp, objPtr) != TCL_OK) {
	    return NULL;
	}
    }
   
    keyObj = ObjLocFmtKey(objPtr);
    if (keyObj) {
	return keyObj;
    }

    keyObj = Tcl_ObjPrintf("FMT_%s", TclGetString(objPtr));
    Tcl_InitObjRef(ObjLocFmtKey(objPtr), keyObj);

    return keyObj;
}

/*
 *----------------------------------------------------------------------
 */

static ClockFmtScnStorage *
FindOrCreateFmtScnStorage(
    Tcl_Interp *interp, 
    Tcl_Obj    *objPtr)
{
    const char *strFmt = TclGetString(objPtr);
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

	ObjClockFmtScn(objPtr) = fss;
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
	fss = FindOrCreateFmtScnStorage(interp, objPtr);
    }

    return fss;
}

/* 
 * DetermineGreedySearchLen --
 *
 * Determine min/max lengths as exact as possible (speed, greedy match) 
 *
 */
inline
void DetermineGreedySearchLen(ClockFmtScnCmdArgs *opts,
    DateInfo *info, ClockScanToken *tok, int *minLen, int *maxLen)
{
    register const char*p = yyInput;
    *minLen = 0;
    /* max length to the end regarding distance to end (min-width of following tokens) */
    *maxLen = info->dateEnd - p - tok->endDistance;

    /* if no tokens anymore */
    if (!(tok+1)->map) {
	/* should match to end or first space */
	while (!isspace(UCHAR(*p)) && ++p < info->dateEnd) {};
	*minLen = p - yyInput;
    } else 
    /* next token is a word */
    if ((tok+1)->map->type == CTOKT_WORD) {
	/* should match at least to the first char of this word */
	while (*p != *((tok+1)->tokWord.start) && ++p < info->dateEnd) {};
	*minLen = p - yyInput;
    }

    if (*minLen > *maxLen) {
	*maxLen = *minLen;
    }
}

inline int 
ObjListSearch(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, int *val, 
    Tcl_Obj **lstv, int lstc,
    int minLen, int maxLen)
{
    int	       i, l, lf = -1;
    const char *s, *f, *sf;
    /* search in list */
    for (i = 0; i < lstc; i++) {
	s = TclGetString(lstv[i]);
	l = lstv[i]->length;

	if ( l >= minLen
	  && (f = TclUtfFindEqualNC(yyInput, yyInput + maxLen, s, s + l, &sf)) > yyInput
	) {
	    l = f - yyInput;
	    if (l < minLen) {
		continue;
	    }
	    /* found, try to find longest value (greedy search) */
	    if (l < maxLen && minLen != maxLen) {
		lf = i;
		minLen = l + 1;
		continue;
	    }
	    /* max possible - end of search */
	    *val = i;
	    yyInput += l;
	    break;
	}
    }

    /* if found */
    if (i < lstc) {
	return TCL_OK;
    }
    if (lf >= 0) {
	*val = lf;
	yyInput += minLen - 1;
	return TCL_OK;
    }
    return TCL_RETURN;
}

static int 
LocaleListSearch(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, int mcKey, int *val, 
    int minLen, int maxLen)
{
    Tcl_Obj **lstv;
    int	      lstc;
    Tcl_Obj *valObj;

    /* get msgcat value */
    valObj = ClockMCGet(opts, mcKey);
    if (valObj == NULL) {
	return TCL_ERROR;
    }

    /* is a list */
    if (TclListObjGetElements(opts->interp, valObj, &lstc, &lstv) != TCL_OK) {
	return TCL_ERROR;
    }

    /* search in list */
    return ObjListSearch(opts, info, val, lstv, lstc,
	minLen, maxLen);
}

static TclStrIdxTree *
ClockMCGetListIdxTree(
    ClockFmtScnCmdArgs *opts, 
    int	 mcKey)
{
    TclStrIdxTree * idxTree;
    Tcl_Obj *objPtr = ClockMCGetIdx(opts, mcKey);
    if ( objPtr != NULL
      && (idxTree = TclStrIdxTreeGetFromObj(objPtr)) != NULL
    ) {
	return idxTree;

    } else {
	/* build new index */

	Tcl_Obj **lstv;
	int	  lstc;
	Tcl_Obj *valObj;

	objPtr = TclStrIdxTreeNewObj();
	if ((idxTree = TclStrIdxTreeGetFromObj(objPtr)) == NULL) {
	    goto done; /* unexpected, but ...*/
	}

	valObj = ClockMCGet(opts, mcKey);
	if (valObj == NULL) {
	    goto done;
	}

	if (TclListObjGetElements(opts->interp, valObj, 
		&lstc, &lstv) != TCL_OK) {
	    goto done;
	};

	if (TclStrIdxTreeBuildFromList(idxTree, lstc, lstv) != TCL_OK) {
	    goto done;
	}

	ClockMCSetIdx(opts, mcKey, objPtr);
	objPtr = NULL;
    };

done:
    if (objPtr) {
	Tcl_DecrRefCount(objPtr);
	idxTree = NULL;
    }

    return idxTree;
}

static TclStrIdxTree *
ClockMCGetMultiListIdxTree(
    ClockFmtScnCmdArgs *opts, 
    int	 mcKey, 
    int *mcKeys)
{
    TclStrIdxTree * idxTree;
    Tcl_Obj *objPtr = ClockMCGetIdx(opts, mcKey);
    if ( objPtr != NULL
      && (idxTree = TclStrIdxTreeGetFromObj(objPtr)) != NULL
    ) {
	return idxTree;

    } else {
	/* build new index */

	Tcl_Obj **lstv;
	int	  lstc;
	Tcl_Obj *valObj;

	objPtr = TclStrIdxTreeNewObj();
	if ((idxTree = TclStrIdxTreeGetFromObj(objPtr)) == NULL) {
	    goto done; /* unexpected, but ...*/
	}

	while (*mcKeys) {

	    valObj = ClockMCGet(opts, *mcKeys);
	    if (valObj == NULL) {
		goto done;
	    }

	    if (TclListObjGetElements(opts->interp, valObj, 
		    &lstc, &lstv) != TCL_OK) {
		goto done;
	    };

	    if (TclStrIdxTreeBuildFromList(idxTree, lstc, lstv) != TCL_OK) {
		goto done;
	    }
	    mcKeys++;
	}

	ClockMCSetIdx(opts, mcKey, objPtr);
	objPtr = NULL;
    };

done:
    if (objPtr) {
	Tcl_DecrRefCount(objPtr);
	idxTree = NULL;
    }

    return idxTree;
}

inline int
ClockStrIdxTreeSearch(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, TclStrIdxTree *idxTree, int *val, 
    int minLen, int maxLen)
{
    const char *f;
    TclStrIdx  *foundItem;
    f = TclStrIdxTreeSearch(NULL, &foundItem, idxTree, 
	    yyInput, yyInput + maxLen);
 
    if (f <= yyInput || (f - yyInput) < minLen) {
	/* not found */
	return TCL_RETURN;
    }
    if (foundItem->value == -1) {
	/* ambigous */
	return TCL_RETURN;
    }

    *val = foundItem->value;

    /* shift input pointer */
    yyInput = f;

    return TCL_OK;
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
    if (*s != NULL) {
	return TCL_OK;
    }
    return TCL_RETURN;
}

inline const char *
FindWordEnd(
    ClockScanToken *tok, 
    register const char * p, const char * end)
{
    register const char *x = tok->tokWord.start;
    const char *pfnd;
    if (x == tok->tokWord.end - 1) { /* fast phase-out for single char word */
	if (*p == *x) {
	    return ++p;
	}
    }
    /* multi-char word */
    x = TclUtfFindEqualNC(x, tok->tokWord.end, p, end, &pfnd);
    if (x < tok->tokWord.end) {
	/* no match -> error */
	return NULL;
    }
    return pfnd;
}

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
	return TCL_RETURN;
    }
    yyMonth = (val % 12) + 1;
    return TCL_OK;
    */

    static int monthsKeys[] = {MCLIT_MONTHS_FULL, MCLIT_MONTHS_ABBREV, 0};

    int ret, val;
    int minLen, maxLen;
    TclStrIdxTree *idxTree;

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    /* get or create tree in msgcat dict */

    idxTree = ClockMCGetMultiListIdxTree(opts, MCLIT_MONTHS_COMB, monthsKeys);
    if (idxTree == NULL) {
	return TCL_ERROR;
    }

    ret = ClockStrIdxTreeSearch(opts, info, idxTree, &val, minLen, maxLen);
    if (ret != TCL_OK) {
	return ret;
    }

    yyMonth = val + 1;
    return TCL_OK;

}

static int 
ClockScnToken_DayOfWeek_Proc(ClockFmtScnCmdArgs *opts,
    DateInfo *info, ClockScanToken *tok)
{
    static int dowKeys[] = {MCLIT_DAYS_OF_WEEK_ABBREV, MCLIT_DAYS_OF_WEEK_FULL, 0};

    int ret, val;
    int minLen, maxLen;
    char curTok = *tok->tokWord.start;
    TclStrIdxTree *idxTree;

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    /* %u %w %Ou %Ow */
    if ( curTok != 'a' && curTok != 'A'
      && ((minLen <= 1 && maxLen >= 1) || (int)tok->map->data)
    ) {
	
	val = -1;

	if (!(int)tok->map->data) {
	    if (*yyInput >= '0' && *yyInput <= '9') {
		val = *yyInput - '0';
	    }
	} else {
	    idxTree = ClockMCGetListIdxTree(opts, (int)tok->map->data /* mcKey */);
	    if (idxTree == NULL) {
		return TCL_ERROR;
	    }

	    ret = ClockStrIdxTreeSearch(opts, info, idxTree, &val, minLen, maxLen);
	    if (ret != TCL_OK) {
		return ret;
	    }
	}

	if (val != -1) {
	    if (val == 0) {
		val = 7;
	    }
	    if (val > 7) {
		Tcl_SetResult(opts->interp, "day of week is greater than 7",
		    TCL_STATIC);
		Tcl_SetErrorCode(opts->interp, "CLOCK", "badDayOfWeek", NULL);
		return TCL_ERROR;
	    }
	    info->date.dayOfWeek = val;
	    yyInput++;
	    return TCL_OK;
	}


	return TCL_RETURN;
    }

    /* %a %A */
    idxTree = ClockMCGetMultiListIdxTree(opts, MCLIT_DAYS_OF_WEEK_COMB, dowKeys);
    if (idxTree == NULL) {
	return TCL_ERROR;
    }

    ret = ClockStrIdxTreeSearch(opts, info, idxTree, &val, minLen, maxLen);
    if (ret != TCL_OK) {
	return ret;
    }

    if (val == 0) {
	val = 7;
    }
    info->date.dayOfWeek = val;
    return TCL_OK;

}

static int 
ClockScnToken_amPmInd_Proc(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, ClockScanToken *tok)
{
    int ret, val;
    int minLen, maxLen;
    Tcl_Obj *amPmObj[2];

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    amPmObj[0] = ClockMCGet(opts, MCLIT_AM);
    amPmObj[1] = ClockMCGet(opts, MCLIT_PM);

    if (amPmObj[0] == NULL || amPmObj[1] == NULL) {
	return TCL_ERROR;
    }

    ret = ObjListSearch(opts, info, &val, amPmObj, 2,
	minLen, maxLen);
    if (ret != TCL_OK) {
	return ret; 
    }

    if (val == 0) {
	yyMeridian = MERam;
    } else {
	yyMeridian = MERpm;
    }

    return TCL_OK;
}

static int 
ClockScnToken_LocaleERA_Proc(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, ClockScanToken *tok)
{
    ClockClientData *dataPtr = opts->clientData;

    int ret, val;
    int minLen, maxLen;
    Tcl_Obj *eraObj[6];

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    eraObj[0] = ClockMCGet(opts, MCLIT_BCE);
    eraObj[1] = ClockMCGet(opts, MCLIT_CE);
    eraObj[2] = dataPtr->mcLiterals[MCLIT_BCE2];
    eraObj[3] = dataPtr->mcLiterals[MCLIT_CE2];
    eraObj[4] = dataPtr->mcLiterals[MCLIT_BCE3];
    eraObj[5] = dataPtr->mcLiterals[MCLIT_CE3];

    if (eraObj[0] == NULL || eraObj[1] == NULL) {
	return TCL_ERROR;
    }

    ret = ObjListSearch(opts, info, &val, eraObj, 6,
	minLen, maxLen);
    if (ret != TCL_OK) {
	return ret; 
    }

    if (val & 1) {
	yydate.era = CE;
    } else {
	yydate.era = BCE;
    }

    return TCL_OK;
}

static int 
ClockScnToken_LocaleListMatcher_Proc(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, ClockScanToken *tok)
{
    int ret, val;
    int minLen, maxLen;
    TclStrIdxTree *idxTree;

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    /* get or create tree in msgcat dict */

    idxTree = ClockMCGetListIdxTree(opts, (int)tok->map->data /* mcKey */);
    if (idxTree == NULL) {
	return TCL_ERROR;
    }

    ret = ClockStrIdxTreeSearch(opts, info, idxTree, &val, minLen, maxLen);
    if (ret != TCL_OK) {
	return ret;
    }

    if (tok->map->offs > 0) {
	*(time_t *)(((char *)info) + tok->map->offs) = val;
    }

    return TCL_OK;
}

static int 
ClockScnToken_TimeZone_Proc(ClockFmtScnCmdArgs *opts, 
    DateInfo *info, ClockScanToken *tok)
{
    int minLen, maxLen;
    int len = 0;
    register const char *p = yyInput;
    Tcl_Obj *tzObjStor = NULL;

    DetermineGreedySearchLen(opts, info, tok, &minLen, &maxLen);

    /* numeric timezone */
    if (*p == '+' || *p == '-') {
	/* max chars in numeric zone = "+00:00:00" */
    #define MAX_ZONE_LEN 9
	char buf[MAX_ZONE_LEN + 1]; 
	char *bp = buf;
	*bp++ = *p++; len++;
	if (maxLen > MAX_ZONE_LEN)
	    maxLen = MAX_ZONE_LEN;
	/* cumulate zone into buf without ':' */
	while (len + 1 < maxLen) {
	    if (!isdigit(UCHAR(*p))) break;
	    *bp++ = *p++; len++;
	    if (!isdigit(UCHAR(*p))) break;
	    *bp++ = *p++; len++;
	    if (len + 2 < maxLen) {
		if (*p == ':') {
		    *p++; len++;
		}
	    }
	}
	*bp = '\0';

	if (len < minLen) {
	    return TCL_RETURN;
	}
    #undef MAX_ZONE_LEN
	
	/* timezone */
	tzObjStor = Tcl_NewStringObj(buf, bp-buf);
    } else {
	/* legacy (alnum) timezone like CEST, etc. */
	if (maxLen > 4)
	    maxLen = 4;
	while (len < maxLen) {
	    if ( (*p & 0x80) 
	      || (!isalpha(UCHAR(*p)) && !isdigit(UCHAR(*p)))
	    ) {	      /* INTL: ISO only. */
		break;
	    }
	    p++; len++;
	}

	if (len < minLen) {
	    return TCL_RETURN;
	}

	/* timezone */
	tzObjStor = Tcl_NewStringObj(yyInput, p-yyInput);

	/* convert using dict */
    }

    /* try to apply new time zone */
    Tcl_IncrRefCount(tzObjStor);

    opts->timezoneObj = ClockSetupTimeZone(opts->clientData, opts->interp, 
	tzObjStor);

    Tcl_DecrRefCount(tzObjStor);
    if (opts->timezoneObj == NULL) {
	return TCL_ERROR;
    }

    yyInput += len;

    return TCL_OK;
}

static const char *ScnSTokenMapIndex = 
    "dmbyYHMSpJjCgGVazs";
static ClockScanTokenMap ScnSTokenMap[] = {
    /* %d %e */
    {CTOKT_DIGIT, CLF_DAYOFMONTH, 0, 1, 2, TclOffset(DateInfo, date.dayOfMonth),
	NULL},
    /* %m */
    {CTOKT_DIGIT, CLF_MONTH, 0, 1, 2, TclOffset(DateInfo, date.month),
	NULL},
    /* %b %B %h */
    {CTOKT_PARSER, CLF_MONTH, 0, 0, 0, 0,
	    ClockScnToken_Month_Proc},
    /* %y */
    {CTOKT_DIGIT, CLF_YEAR, 0, 1, 2, TclOffset(DateInfo, date.year),
	NULL},
    /* %Y */
    {CTOKT_DIGIT, CLF_YEAR | CLF_CENTURY, 0, 4, 4, TclOffset(DateInfo, date.year),
	NULL},
    /* %H %k %I %l */
    {CTOKT_DIGIT, CLF_TIME, 0, 1, 2, TclOffset(DateInfo, date.hour),
	NULL},
    /* %M */
    {CTOKT_DIGIT, CLF_TIME, 0, 1, 2, TclOffset(DateInfo, date.minutes),
	NULL},
    /* %S */
    {CTOKT_DIGIT, CLF_TIME, 0, 1, 2, TclOffset(DateInfo, date.secondOfDay),
	NULL},
    /* %p %P */
    {CTOKT_PARSER, CLF_ISO8601, 0, 0, 0, 0,
	ClockScnToken_amPmInd_Proc, NULL},
    /* %J */
    {CTOKT_DIGIT, CLF_JULIANDAY, 0, 1, 0xffff, TclOffset(DateInfo, date.julianDay),
	NULL},
    /* %j */
    {CTOKT_DIGIT, CLF_DAYOFYEAR, 0, 1, 3, TclOffset(DateInfo, date.dayOfYear),
	NULL},
    /* %C */
    {CTOKT_DIGIT, CLF_CENTURY|CLF_ISO8601CENTURY, 0, 1, 2, TclOffset(DateInfo, dateCentury),
	NULL},
    /* %g */
    {CTOKT_DIGIT, CLF_ISO8601YEAR | CLF_ISO8601, 0, 2, 2, TclOffset(DateInfo, date.iso8601Year),
	NULL},
    /* %G */
    {CTOKT_DIGIT, CLF_ISO8601YEAR | CLF_ISO8601 | CLF_ISO8601CENTURY, 0, 4, 4, TclOffset(DateInfo, date.iso8601Year),
	NULL},
    /* %V */
    {CTOKT_DIGIT, CLF_ISO8601, 0, 1, 2, TclOffset(DateInfo, date.iso8601Week),
	NULL},
    /* %a %A %u %w */
    {CTOKT_PARSER, CLF_ISO8601, 0, 0, 0, 0,
	ClockScnToken_DayOfWeek_Proc, NULL},
    /* %z %Z */
    {CTOKT_PARSER, CLF_OPTIONAL, 0, 0, 0, 0,
	ClockScnToken_TimeZone_Proc, NULL},
    /* %s */
    {CTOKT_DIGIT, CLF_LOCALSEC | CLF_SIGNED, 0, 1, 0xffff, TclOffset(DateInfo, date.localSeconds),
	NULL},
};
static const char *ScnSTokenWrapMapIndex[2] = {
    "eNBhkIlPAuwZ",
    "dmbbHHHpaaaz"
};

static const char *ScnETokenMapIndex = 
    "Ey";
static ClockScanTokenMap ScnETokenMap[] = {
    /* %EE */
    {CTOKT_PARSER, 0, 0, 0, 0, TclOffset(DateInfo, date.year),
	ClockScnToken_LocaleERA_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %Ey */
    {CTOKT_PARSER, 0, 0, 0, 0, 0, /* currently no capture, parse only token */
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
};
static const char *ScnETokenWrapMapIndex[2] = {
    "",
    ""
};

static const char *ScnOTokenMapIndex = 
    "dmyHMSu";
static ClockScanTokenMap ScnOTokenMap[] = {
    /* %Od %Oe */
    {CTOKT_PARSER, CLF_DAYOFMONTH, 0, 0, 0, TclOffset(DateInfo, date.dayOfMonth),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %Om */
    {CTOKT_PARSER, CLF_MONTH, 0, 0, 0, TclOffset(DateInfo, date.month),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %Oy */
    {CTOKT_PARSER, CLF_YEAR, 0, 0, 0, TclOffset(DateInfo, date.year),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %OH %Ok %OI %Ol */
    {CTOKT_PARSER, CLF_TIME, 0, 0, 0, TclOffset(DateInfo, date.hour),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %OM */
    {CTOKT_PARSER, CLF_TIME, 0, 0, 0, TclOffset(DateInfo, date.minutes),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %OS */
    {CTOKT_PARSER, CLF_TIME, 0, 0, 0, TclOffset(DateInfo, date.secondOfDay),
	ClockScnToken_LocaleListMatcher_Proc, (void *)MCLIT_LOCALE_NUMERALS},
    /* %Ou Ow */
    {CTOKT_PARSER, CLF_ISO8601, 0, 0, 0, 0,
	ClockScnToken_DayOfWeek_Proc, (void *)MCLIT_LOCALE_NUMERALS},
};
static const char *ScnOTokenWrapMapIndex[2] = {
    "ekIlw",
    "dHHHu"
};

static const char *ScnSpecTokenMapIndex = 
    " ";
static ClockScanTokenMap ScnSpecTokenMap[] = {
    {CTOKT_SPACE,  0, 0, 0, 0xffff, 0, 
	NULL},
};

static ClockScanTokenMap ScnWordTokenMap = {
    CTOKT_WORD,	   0, 0, 1, 0, 0, 
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
    Tcl_Obj    *formatObj)	/* Format container */
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
	fss = FindOrCreateFmtScnStorage(interp, formatObj);
	if (fss == NULL) {
	    return NULL;
	}
    }

    /* if first time scanning - tokenize format */
    if (fss->scnTok == NULL) {
	const char *strFmt;
	register const char *p, *e, *cp;

	e = strFmt = HashEntry4FmtScn(fss)->key.string;
	e += strlen(strFmt);

	/* estimate token count by % char and format length */
	fss->scnTokC = 0;
	p = strFmt;
	while (p != e) {
	    if (*p++ == '%') fss->scnTokC++;
	}
	p = strFmt + fss->scnTokC * 2;
	if (p < e) {
	    if ((unsigned int)(e - p) < fss->scnTokC) {
		fss->scnTokC += (e - p);
	    } else {
		fss->scnTokC += fss->scnTokC;
	    }
	}
	fss->scnTokC++;

	Tcl_MutexLock(&ClockFmtMutex);

	fss->scnTok = tok = ckalloc(sizeof(*tok) * fss->scnTokC);
	memset(tok, 0, sizeof(*(tok)));
	for (p = strFmt; p < e;) {
	    switch (*p) {
	    case '%':
	    if (1) {
		ClockScanTokenMap * scnMap = ScnSTokenMap;
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
		    tok->tokWord.start = p;
		    tok->tokWord.end = p+1;
		    AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
		    p++;
		    continue;
		break;
		case 'E':
		    scnMap = ScnETokenMap, 
		    mapIndex =	ScnETokenMapIndex,
		    wrapIndex = ScnETokenWrapMapIndex;
		    p++;
		break;
		case 'O':
		    scnMap = ScnOTokenMap,
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
		tok->map = &scnMap[cp - mapIndex];
		tok->tokWord.start = p;
		/* calculate look ahead value by standing together tokens */
		if (tok > fss->scnTok && tok->map->minSize) {
		    unsigned int	lookAhead = tok->map->minSize;
		    ClockScanToken     *prevTok = tok - 1;

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
		p++;
		continue;
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
		p++;
		continue;
	    break;
	    default:
word_tok:
	    if (1) {
		ClockScanToken	 *wordTok = tok;
		if (tok > fss->scnTok && (tok-1)->map == &ScnWordTokenMap) {
		    wordTok = tok-1;
		}
		wordTok->tokWord.end = p+1;
		if (wordTok == tok) {
		    wordTok->tokWord.start = p;
		    wordTok->map = &ScnWordTokenMap;
		    AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
		}
	    }
	    break;
	    }

	    p = TclUtfNext(p);
	}

	/* calculate end distance value for each tokens */
	if (tok > fss->scnTok) {
	    unsigned int	endDist = 0;
	    ClockScanToken     *prevTok = tok-1;

	    while (prevTok >= fss->scnTok) {
		prevTok->endDistance = endDist;
		if (prevTok->map->type != CTOKT_WORD) {
		    endDist += prevTok->map->minSize;
		} else {
		    endDist += prevTok->tokWord.end - prevTok->tokWord.start;
		}
		prevTok--;
	    }
	}	 
done:
	Tcl_MutexUnlock(&ClockFmtMutex);
    }

    return fss->scnTok;
}

MODULE_SCOPE Tcl_Obj *
ClockLocalizeFormat(
    ClockFmtScnCmdArgs *opts)
{
    ClockClientData *dataPtr = opts->clientData;
    Tcl_Obj *valObj = NULL, *keyObj;

    keyObj = ClockFrmObjGetLocFmtKey(opts->interp, opts->formatObj);

    /* special case - format object is not localizable */
    if (keyObj == opts->formatObj) {
	return opts->formatObj;
    }

    if (opts->mcDictObj == NULL) {
	ClockMCDict(opts);
	if (opts->mcDictObj == NULL)
	    return NULL;
    }

    /* try to find in cache within locale mc-catalog */
    if (Tcl_DictObjGet(NULL, opts->mcDictObj, 
	    keyObj, &valObj) != TCL_OK) {
	return NULL;
    }

    /* call LocalizeFormat locale format fmtkey */
    if (valObj == NULL) {
	Tcl_Obj *callargs[4];
	callargs[0] = dataPtr->literals[LIT_LOCALIZE_FORMAT];
	callargs[1] = opts->localeObj;
	callargs[2] = opts->formatObj;
	callargs[3] = keyObj;
	Tcl_IncrRefCount(keyObj);
	if (Tcl_EvalObjv(opts->interp, 4, callargs, 0) != TCL_OK
	) {
	    goto clean;
	}

	valObj = Tcl_GetObjResult(opts->interp);

	/* cache it inside mc-dictionary (this incr. ref count of keyObj/valObj) */
	if (Tcl_DictObjPut(opts->interp, opts->mcDictObj,
		keyObj, valObj) != TCL_OK
	) {
	    valObj = NULL;
	    goto clean;
	}

	/* check special case - format object is not localizable */
	if (valObj == opts->formatObj) {
	    /* mark it as unlocalizable, by setting self as key (without refcount incr) */
	    if (opts->formatObj->typePtr == &ClockFmtObjType) {
		Tcl_UnsetObjRef(ObjLocFmtKey(opts->formatObj));
		ObjLocFmtKey(opts->formatObj) = opts->formatObj;
	    }
	}
clean:

	Tcl_UnsetObjRef(keyObj);
	if (valObj) {
	    Tcl_ResetResult(opts->interp);
	}
    }

    return (opts->formatObj = valObj);
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

    /* get localized format */
    if (ClockLocalizeFormat(opts) == NULL) {
	return TCL_ERROR;
    }

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
	/* end of input string */
	if (p >= end) {
	    break;
	}
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
		if ((map->flags & CLF_OPTIONAL)) {
		    yyInput = p;
		    continue;
		}
		goto not_match;
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
	    flags = (flags & ~map->clearFlags) | map->flags;
	}
	break;
	case CTOKT_PARSER:
	    switch (map->parser(opts, info, tok)) {
		case TCL_OK:
		break;
		case TCL_RETURN:
		    if ((map->flags & CLF_OPTIONAL)) {
			yyInput = p;
			continue;
		    }
		    goto not_match;
		break;
		default:
		    goto done;
		break;
	    };
	    p = yyInput;
	    flags = (flags & ~map->clearFlags) | map->flags;
	break;
	case CTOKT_SPACE:
	    /* at least one space in strict mode */
	    if (opts->flags & CLF_STRICT) {
		if (!isspace(UCHAR(*p))) {
		    /* unmatched -> error */
		    goto not_match;
		}
		p++;
	    }
	    while (p < end && isspace(UCHAR(*p))) {
		p++;
	    }
	break;
	case CTOKT_WORD:
	    x = FindWordEnd(tok, p, end);
	    if (!x) {
		/* no match -> error */
		goto not_match;
	    }
	    p = x;
	    continue;
	break;
	}
    }
    /* check end was reached */
    if (p < end) {
	/* ignore spaces at end */
	while (p < end && isspace(UCHAR(*p))) {
	    p++;
	}
	if (p < end) {
	    /* something after last token - wrong format */
	    goto not_match;
	}
    }
    /* end of string, check only optional tokens at end, otherwise - not match */
    if (tok->map != NULL) {
	if ( !(opts->flags & CLF_STRICT) && (tok->map->type == CTOKT_SPACE)
	  || (tok->map->flags & CLF_OPTIONAL)) {
	    goto not_match;
	}
    }

    /* 
     * Invalidate result 
     */

    /* seconds token (%s) take precedence over all other tokens */
    if ((opts->flags & CLF_EXTENDED) || !(flags & CLF_LOCALSEC)) {
	if (flags & CLF_DATE) {

	    if (!(flags & CLF_JULIANDAY)) {
		info->flags |= CLF_ASSEMBLE_SECONDS|CLF_ASSEMBLE_JULIANDAY;

		/* dd precedence below ddd */
		switch (flags & (CLF_MONTH|CLF_DAYOFYEAR|CLF_DAYOFMONTH)) {
		    case (CLF_DAYOFYEAR|CLF_DAYOFMONTH):
		    /* miss month: ddd over dd (without month) */
		    flags &= ~CLF_DAYOFMONTH;
		    case (CLF_DAYOFYEAR):
		    /* ddd over naked weekday */
		    if (!(flags & CLF_ISO8601YEAR)) {
			flags &= ~CLF_ISO8601;
		    }
		    break;
		    case (CLF_MONTH|CLF_DAYOFYEAR|CLF_DAYOFMONTH):
		    /* both available: mmdd over ddd */
		    flags &= ~CLF_DAYOFYEAR;
		    case (CLF_MONTH|CLF_DAYOFMONTH):
		    case (CLF_DAYOFMONTH):
		    /* mmdd / dd over naked weekday */
		    if (!(flags & CLF_ISO8601YEAR)) {
			flags &= ~CLF_ISO8601;
		    }
		    break;
		}

		/* YearWeekDay below YearMonthDay */
		if ( (flags & CLF_ISO8601) 
		  && ( (flags & (CLF_YEAR|CLF_DAYOFYEAR)) == (CLF_YEAR|CLF_DAYOFYEAR)
		    || (flags & (CLF_YEAR|CLF_DAYOFMONTH|CLF_MONTH)) == (CLF_YEAR|CLF_DAYOFMONTH|CLF_MONTH)
		  ) 
		) {
		    /* yy precedence below yyyy */
		    if (!(flags & CLF_ISO8601CENTURY) && (flags & CLF_CENTURY)) {
			/* normally precedence of ISO is higher, but no century - so put it down */
			flags &= ~CLF_ISO8601;
		    } 
		    else 
		    /* yymmdd or yyddd over naked weekday */
		    if (!(flags & CLF_ISO8601YEAR)) {
			flags &= ~CLF_ISO8601;
		    }
		}

		if (!(flags & CLF_ISO8601)) {
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
		} else {
		    if (info->date.iso8601Year < 100) {
			if (!(flags & CLF_ISO8601CENTURY)) {
			    if (info->date.iso8601Year >= dataPtr->yearOfCenturySwitch) {
				info->date.iso8601Year -= 100;
			    }
			    info->date.iso8601Year += dataPtr->currentYearCentury;
			} else {
			    info->date.iso8601Year += info->dateCentury * 100;
			}
		    }
		}
	    }
	}

	/* if no time - reset time */
	if (!(flags & (CLF_TIME|CLF_LOCALSEC))) {
	    info->flags |= CLF_ASSEMBLE_SECONDS;
	    yydate.localSeconds = 0;
	}

	if (flags & CLF_TIME) {
	    info->flags |= CLF_ASSEMBLE_SECONDS;
	    yySeconds = ToSeconds(yyHour, yyMinutes,
				yySeconds, yyMeridian);
	} else
	if (!(flags & CLF_LOCALSEC)) {
	    info->flags |= CLF_ASSEMBLE_SECONDS;
	    yySeconds = yydate.localSeconds % SECONDS_PER_DAY;
	}
    }

    /* tell caller which flags were set */
    info->flags |= flags;

    ret = TCL_OK;
    goto done;

overflow:

    Tcl_SetResult(interp, "requested date too large to represent", 
	TCL_STATIC);
    Tcl_SetErrorCode(interp, "CLOCK", "dateTooLarge", NULL);
    goto done;

not_match:

    Tcl_SetResult(interp, "input string does not match supplied format",
	TCL_STATIC);
    Tcl_SetErrorCode(interp, "CLOCK", "badInputString", NULL);

done:

    return ret;
}

/*
 *----------------------------------------------------------------------
 */
int
ClockFormat(
    ClientData clientData,	/* Client data containing literal pool */
    Tcl_Interp *interp,		/* Tcl interpreter */
    register DateInfo *info,	/* Date fields used for parsing & converting */
    ClockFmtScnCmdArgs *opts)	/* Command options */
{
    ClockClientData *dataPtr = clientData;
    ClockFormatToken	*tok;
    ClockFormatTokenMap *map;

    /* get localized format */
    if (ClockLocalizeFormat(opts) == NULL) {
	return TCL_ERROR;
    }

/*    if ((tok = ClockGetOrParseFmtFormat(interp, opts->formatObj)) == NULL) {
	return TCL_ERROR;
    }
*/
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
