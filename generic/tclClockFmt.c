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

/*
 * Clock scan and format facilities.
 */

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
static int	     initFmtScnHashTable = 0;

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
    if (!initFmtScnHashTable) {
	/* initialize type */
	memcpy(&ClockFmtScnStorageHashKeyType, &tclStringHashKeyType, sizeof(tclStringHashKeyType));
	ClockFmtScnStorageHashKeyType.allocEntryProc = ClockFmtScnStorageAllocProc;
	ClockFmtScnStorageHashKeyType.freeEntryProc = ClockFmtScnStorageFreeProc;
	initFmtScnHashTable = 1;

	/* initialize hash table */
	Tcl_InitCustomHashTable(&FmtScnHashTable, TCL_CUSTOM_TYPE_KEYS,
	    &ClockFmtScnStorageHashKeyType);
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


#define AllocTokenInChain(tok, chain, tokCnt) \
    if (++(tok) >= (chain) + (tokCnt)) { \
	(char *)(chain) = ckrealloc((char *)(chain), \
	    (tokCnt + CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE) * sizeof(*(tok))); \
	if ((chain) == NULL) { goto done; }; \
	(tok) = (chain) + (tokCnt); \
	(tokCnt) += CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE; \
    } \
    *(tok) = NULL;

const char *ScnSTokenMapChars = 
    "dmyYHMS";
static ClockScanToken ScnSTokenMap[] = {
    {CTOKT_DIGIT,  1, 2, 0},
    {CTOKT_DIGIT,  1, 2, 0},
    {CTOKT_DIGIT,  1, 2, 0},
    {CTOKT_DIGIT,  1, 4, 0},
    {CTOKT_DIGIT,  1, 2, 0},
    {CTOKT_DIGIT,  1, 2, 0},
    {CTOKT_DIGIT,  1, 2, 0},
};
const char *ScnSpecTokenMapChars = 
    " %";
static ClockScanToken ScnSpecTokenMap[] = {
    {CTOKT_SPACE,  1, 0xffff, 0},
};

/*
 *----------------------------------------------------------------------
 */
ClockScanToken **
ClockGetOrParseScanFormat(
    Tcl_Interp *interp,		/* Tcl interpreter */
    Tcl_Obj *formatObj)		/* Format container */
{
    ClockFmtScnStorage *fss;
    ClockScanToken    **tok;

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
	register const char *p, *e, *cp, *word_start = NULL;

	Tcl_MutexLock(&ClockFmtMutex);

	fss->scnTokC = CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE;
	fss->scnTok =
		tok = ckalloc(sizeof(*tok) * CLOCK_MIN_TOK_CHAIN_BLOCK_SIZE);
	*tok = NULL;
	strFmt = TclGetString(formatObj);
	for (e = p = strFmt, e += formatObj->length; p != e; p++) {
	    switch (*p) {
	    case '%':
		if (p+1 >= e) {
		    word_start = p;
		    continue;
		}
		p++;
		/* try to find modifier: */
		switch (*p) {
		case '%':
		    word_start = p-1;
		    continue;
		break;
		case 'E':
		    goto ext_tok_E;
		break;
		case 'O':
		    goto ext_tok_O;
		break;
		default:
		    cp = strchr(ScnSTokenMapChars, *p);
		    if (!cp || *cp == '\0') {
			word_start = p-1;
			continue;
		    }
		    *tok = &ScnSTokenMap[cp - ScnSTokenMapChars];
		    AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
		break;
		}
	    break;
	    case ' ':
		cp = strchr(ScnSpecTokenMapChars, *p);
		if (!cp || *cp == '\0') {
		    p--;
		    goto word_tok;
		}
		*tok = &ScnSpecTokenMap[cp - ScnSpecTokenMapChars];
		AllocTokenInChain(tok, fss->scnTok, fss->scnTokC);
	    break;
	    default:
word_tok:

	    continue;
	    }

	    continue;
ext_tok_E:

ext_tok_O:

	    /*******************/
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
    TclDateFields *date,	/* Date fields used for converting */
    Tcl_Obj *strObj,		/* String containing the time to scan */
    ClockFmtScnCmdArgs *opts)	/* Command options */
{
    ClockScanToken    **tok;

    if ((tok = ClockGetOrParseScanFormat(interp, opts->formatObj)) == NULL) {
	return TCL_ERROR;
    }
    
    //***********************************

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt)tok));
    return TCL_OK;


    return TCL_ERROR;
}


static void
Tcl_GetClockFrmScnFinalize() 
{
#if CLOCK_FMT_SCN_STORAGE_GC_SIZE > 0
    /* clear GC */
    ClockFmtScnStorage_GC.stackPtr = NULL;
    ClockFmtScnStorage_GC.stackBound = NULL;
    ClockFmtScnStorage_GC.count = 0;
#endif
    if (initFmtScnHashTable) {
	Tcl_DeleteHashTable(&FmtScnHashTable);
    }
    Tcl_MutexFinalize(&ClockFmtMutex);
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
