/*
 * tclList.c --
 *
 *	Data structure and operations for Tcl list values.
 *
 * Contributions from Don Porter, NIST, 2013. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
 
#include "tclInt.h"
#include "tclList.h"

#define SPAN_MAX USHRT_MAX
#define SPAN_SIZE(elems) \
	(sizeof(Span) + ((elems) - 1) * sizeof(Span *))

static int	ListAppendSpan(Tcl_Interp *interp, TclList **listPtrPtr,
			unsigned short int spanSize);
static Span *	SpanAllocate(Tcl_Interp *interp,
			unsigned short int numElements);
static void 	SpanRelease(Span *spanPtr);

static Tcl_FreeInternalRepProc	FreeList;
static Tcl_DupInternalRepProc	DupList;
static Tcl_UpdateStringProc	UpdateStringOfList;
static Tcl_SetFromAnyProc	SetListFromAny;

const Tcl_ObjType listType = {
    "List",
    FreeList,
    DupList,
    UpdateStringOfList,
    SetListFromAny
};

#undef ListRepPtr
#define ListRepPtr(objPtr) \
    ((TclList *) (objPtr)->internalRep.ptrAndLongRep.ptr)

#undef ListIsCanonical
#define ListIsCanonical(objPtr) \
    (objPtr)->internalRep.ptrAndLongRep.value

#undef ListSetIntRep
#define ListSetIntRep(objPtr, listPtr) \
    (objPtr)->internalRep.ptrAndLongRep.ptr = (listPtr), \
    (objPtr)->typePtr = &listType

static void
DupList(
    Tcl_Obj *srcPtr,
    Tcl_Obj *copyPtr)
{
    TclList *listPtr = ListRepPtr(srcPtr);

    ListIsCanonical(copyPtr) = ListIsCanonical(srcPtr);
    ListSetIntRep(copyPtr, TclListCopy(listPtr));
}

static void
FreeList(
    Tcl_Obj *objPtr)
{
    TclList *listPtr = ListRepPtr(objPtr);

    TclListRelease(listPtr);
}

static void
UpdateStringOfList(
    Tcl_Obj *objPtr)
{
    TclList *listPtr = ListRepPtr(objPtr);
    size_t numElems = TclListLength(listPtr);
    TclListIndex *indexPtr;
    Tcl_Obj *elemPtr;
    char *dst, *flagPtr;
    int i, length, bytesNeeded = 0;
    const char *elem;

    ListIsCanonical(objPtr) = 1;

    if (numElems == 0) {
	objPtr->bytes = tclEmptyStringRep;
	objPtr->length = 0;
	return;
    }

    /* TODO: Convert to repeated appends to a string type */
    flagPtr = ckalloc(numElems * sizeof(char));

    indexPtr = TclListIndexCreate(listPtr, 0);
    i = 0;
    while (NULL != (elemPtr = TclListIndexGetValue(indexPtr))) {
	flagPtr[i] = (i ? TCL_DONT_QUOTE_HASH : 0);
	elem = TclGetStringFromObj(elemPtr, &length);
	bytesNeeded += TclScanElement(elem, length, flagPtr+i);
	if (bytesNeeded < 0) {
	    Tcl_Panic("too big");
	}
        TclListIndexIncrement(indexPtr);
	i++;
    }
    TclListIndexRelease(indexPtr);

    if (bytesNeeded > INT_MAX - numElems + 1) {
	Tcl_Panic("too big");
    }
    bytesNeeded += numElems;

    objPtr->length = bytesNeeded - 1;
    objPtr->bytes = ckalloc(bytesNeeded);
    dst = objPtr->bytes;

    indexPtr = TclListIndexCreate(listPtr, 0);
    i = 0;
    while (NULL != (elemPtr = TclListIndexGetValue(indexPtr))) {
	flagPtr[i] |= (i ? TCL_DONT_QUOTE_HASH : 0);
	elem = TclGetStringFromObj(elemPtr, &length);
	dst += TclConvertElement(elem, length, dst, flagPtr[i]);
	*dst++ = ' ';
        TclListIndexIncrement(indexPtr);
	i++;
    }
    TclListIndexRelease(indexPtr);

    objPtr->bytes[objPtr->length] = '\0';
}

static int
SetListFromAny(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    TclList *listPtr;
    int length;
    const char *limit, *nextElem = Tcl_GetStringFromObj(objPtr, &length);

    /* Allocate enough space to hold each (possible) element */
    listPtr = TclListAllocate(interp,
	    TclMaxListLength(nextElem, length, &limit));
    if (listPtr == NULL) {
	return TCL_ERROR;
    }

    /* Each iteration, parse and store a list element. */
    while (nextElem < limit) {
	const char *elemStart;
	int elemSize, literal;
	Tcl_Obj *elemPtr;

	if (TCL_OK != TclFindElement(interp, nextElem, limit - nextElem,
		&elemStart, &nextElem, &elemSize, &literal)) {
	    TclListRelease(listPtr);
	    return TCL_ERROR;
	}
	if (elemStart == limit) {
	    break;
	}
	if (literal) {
	    TclNewStringObj(elemPtr, elemStart, elemSize);
	} else {
	    TclNewObj(elemPtr);
	    elemPtr->bytes = ckalloc(elemSize + 1);
	    elemPtr->length = TclCopyAndCollapse(elemSize, elemStart,
		    elemPtr->bytes);
	}
	if (TCL_OK != TclListAppend(interp, &listPtr, elemPtr)) {
	    TclListRelease(listPtr);
	    return TCL_ERROR;
	}
    }

    /*
     * Creation of listPtr intrep succeeded.  Only now free the old
     * internalRep since there's no longer a chance of error and wanted
     * to fallback to it.
     */

    TclFreeIntRep(objPtr);
    ListIsCanonical(objPtr) = 0;
    ListSetIntRep(objPtr, listPtr);
    return TCL_OK;
}

TclListIndex *
TclListIndexCreate(
    TclList *listPtr,
    size_t index)
{
    TclListIndex *indexPtr = ckalloc(sizeof(TclListIndex));
    unsigned short int span = listPtr->first;
    size_t passed = 0;

    indexPtr->listPtr = TclListCopy(listPtr);
    indexPtr->index = index;

    /* Find the span that holds the index */
    while (span < listPtr->last && passed <= index) {
	Span *spanPtr = listPtr->span[span];
	passed += spanPtr->last - spanPtr->first;
	span++;
    }

    if (passed > index) {
	indexPtr->span = --span;
	indexPtr->elem = listPtr->span[span]->last - (passed - index);
    } else {
	/* Index is beyond end of list */
	indexPtr->span = listPtr->last;
	while (--span >= listPtr->first) {
	    Span *spanPtr = listPtr->span[span];
	    if (spanPtr->last > spanPtr->first) {
		indexPtr->span = span;
		indexPtr->elem = spanPtr->last;
	    }
	}
    }

    return indexPtr;
}

Tcl_Obj *
TclListIndexGetValue(
    TclListIndex *indexPtr)
{
    TclList *listPtr = indexPtr->listPtr;
    Span *spanPtr;

    if (indexPtr->span == listPtr->last) {
	return NULL;
    }
    spanPtr = listPtr->span[indexPtr->span];
    if (indexPtr->elem == spanPtr->last) {
	return NULL;
    }
    return spanPtr->objv[indexPtr->elem];
}

void
TclListIndexIncrement(
    TclListIndex *indexPtr)
{
    TclList *listPtr = indexPtr->listPtr;
    Span *spanPtr;

    if (indexPtr->span == listPtr->last) {
	return;
    }
    spanPtr = listPtr->span[indexPtr->span];
    if (indexPtr->elem == spanPtr->last) {
	return;
    }
    indexPtr->elem++;
    while (1) {
	if (indexPtr->elem < spanPtr->last) {
	    return;
	}
	/* assert (indexPtr->elem == spanPtr->last) */
	indexPtr->span++;
	if (indexPtr->span == listPtr->last) {
	    return;
	}
	spanPtr = listPtr->span[indexPtr->span];
	indexPtr->elem = spanPtr->first;
    }
}

void
TclListIndexRelease(
    TclListIndex *indexPtr)
{
    TclListRelease(indexPtr->listPtr);
    ckfree(indexPtr);
}

TclList *
TclListCopy(
    TclList *listPtr)
{
    listPtr->refCount++;
    return listPtr;
}

size_t
TclListLength(
    TclList *listPtr)
{
    return listPtr->length;
}

void
TclListRelease(
    TclList *listPtr)
{
    unsigned short int i = listPtr->first;
    unsigned short int end = listPtr->last;

    if (--listPtr->refCount) {
	return;
    }
    while (i < end) {
	SpanRelease(listPtr->span[i++]);
    }
    ckfree(listPtr);
}

TclList *
TclListAllocate(
    Tcl_Interp *interp,
    size_t numElements)
{
    unsigned short int numWholeSpans, lastSpanElements, toAllocate, i;
    TclList *listPtr;

    if (numElements > LIST_MAX) {
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "max length of a Tcl list (%lu elements) exceeded",
		    LIST_MAX));
	    /* TODO: should be some other "limit" error, not mem? */
	    Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
	}
	return NULL;
    }

    if (numElements == 0) {
	/* TODO: consider one shared empty list ? */
	listPtr = attemptckalloc(LIST_SIZE(1));
	if (listPtr == NULL) {
	    return NULL;
	}
	listPtr->size = 1;
	listPtr->refCount = 1;
	listPtr->first = 0;
	listPtr->last = 0;
	listPtr->length = 0;
    }

    numWholeSpans = ((numElements - 1) / SPAN_MAX);
    lastSpanElements = numElements - numWholeSpans * SPAN_MAX;
    toAllocate = numWholeSpans + (lastSpanElements > 0);

    listPtr = attemptckalloc(LIST_SIZE(toAllocate));
    if (listPtr == NULL) {
	return NULL;
    }
    listPtr->size = toAllocate;
    listPtr->refCount = 1;
    listPtr->first = 0;
    listPtr->last = 0;
    listPtr->length = 0;	/* Allocate != initialize */

    i = 0;
    for (i=0; toAllocate > 0; toAllocate--, numWholeSpans--) {
	Span *spanPtr = SpanAllocate(interp,
		numWholeSpans ? SPAN_MAX : lastSpanElements);

	if (spanPtr == NULL) {
	    TclListRelease(listPtr);
	    return NULL;
	}
	listPtr->span[i++] = spanPtr;
	listPtr->last++;
    }
    return listPtr;
}

static int
ListAppendSpan(
    Tcl_Interp *interp,
    TclList **listPtrPtr,
    unsigned short int spanSize)
{
    TclList *listPtr = *listPtrPtr;
    Span *spanPtr = SpanAllocate(interp, spanSize);
    if (spanPtr == NULL) {
	return TCL_ERROR;
    }

    if (listPtr->last == listPtr->size) {
	TclList *newPtr;
	unsigned short int needed, newSize;

	if (listPtr->size == USHRT_MAX) {
	    /* TODO: Restructure spans to make room */
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"max spans of a Tcl list (%d spans) exceeded", USHRT_MAX));
	    }
	    SpanRelease(spanPtr);
	    return TCL_ERROR;
	}

	needed = listPtr->size + 1;
	newSize = (needed < USHRT_MAX/2) ? 2*needed : USHRT_MAX;

	newPtr = attemptckrealloc(listPtr, LIST_SIZE(newSize));
	if (newPtr == NULL) {
	    newSize = (needed < USHRT_MAX - TCL_MIN_GROWTH) ?
		    needed + TCL_MIN_GROWTH : USHRT_MAX;

	    newPtr = attemptckrealloc(listPtr, LIST_SIZE(newSize));
	    if (newPtr == NULL) {
		newSize = needed;
		newPtr = attemptckrealloc(listPtr, LIST_SIZE(newSize));
		if (newPtr == NULL) {
		    if (interp) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf(
				"unable to alloc %lu bytes",
				LIST_SIZE(newSize)));
			Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
		    }

		    SpanRelease(spanPtr);
		    return TCL_ERROR;
		}
	    }
	}
	listPtr = newPtr;
	listPtr->size = newSize;
    }
    /* assert (listPtr->last < listPtr->size) */
    listPtr->span[listPtr->last++] = spanPtr;
    *listPtrPtr = listPtr;
    return TCL_OK;
}

int
TclListAppend(
    Tcl_Interp *interp,
    TclList **listPtrPtr,
    Tcl_Obj *objPtr)
{
    TclList *listPtr = *listPtrPtr;
    Span *spanPtr = NULL;
    unsigned short int spanIdx;

    if (listPtr->refCount > 1) {
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "TclListAppend attempted on shared TclList"));
	}
	return TCL_ERROR;
    }

    /* Find Span that holds last element, if any */
    /* TODO: Address back scan in pre-allocated TclList */
    if (listPtr->length) {
	for ( spanIdx = listPtr->last; spanIdx > listPtr->first; ) {
	    spanPtr = listPtr->span[--spanIdx];
	    if (spanPtr->last > spanPtr->first) {
		break;
	    }
	}
    }

    if (spanPtr == NULL) {
	/* No Span contains elements -- empty list */
	if (listPtr->last == listPtr->first) { /* No Spans */
	    /* TODO: good minimimum alloc value and macro-ize */
	    if (TCL_OK != ListAppendSpan(interp, &listPtr, 16)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("append fail"));
		return TCL_ERROR;
	    }
	}
	spanIdx = listPtr->first;
	spanPtr = listPtr->span[spanIdx];
    }

    /* spanPtr points to the Span where we should try to append */

    while (spanPtr->refCount > 1 || spanPtr->last == SPAN_MAX) {
	/* The Span is shared.  Can't change it. Usually the cheapest
	 * thing to do is start a new Span for appending, and preserve
	 * the sharing.  Do that, if it's possible. */

	if (spanIdx + 1 == listPtr->last) {
	    if (TCL_OK != ListAppendSpan(NULL, &listPtr, spanPtr->size)) {
		unsigned short int i;
		Span *newPtr;

		if (spanPtr->last == SPAN_MAX) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf("append fail"));
		    return TCL_ERROR;
		}
		newPtr = SpanAllocate(NULL, spanPtr->size);
		if (newPtr == NULL) {
		    newPtr = SpanAllocate(interp, spanPtr->last + 1);
		    if (newPtr == NULL) {
			return TCL_ERROR;
		    }
		}
		newPtr->first = spanPtr->first;
		newPtr->last = spanPtr->last;
		for (i = spanPtr->first; i < spanPtr->last; i++) {
		    Tcl_Obj *copyPtr = spanPtr->objv[i];

		    Tcl_IncrRefCount(copyPtr);
		    newPtr->objv[i] = copyPtr;
		}
		SpanRelease(spanPtr);
		spanPtr = newPtr;
	    }
	}
	spanPtr = listPtr->span[++spanIdx];
    }

    /* spanPtr points to unshared Span where we should try to append */

    if (spanPtr->last == spanPtr->size) {
	/* Have to grow the span before we can append */
	/* spanPtr->size == SPAN_MAX can't happen */
	
	Span *newPtr;
	unsigned short int needed = spanPtr->size + 1;
	unsigned short int newSize =
		(needed < SPAN_MAX/2) ? 2*needed : SPAN_MAX;

	newPtr = attemptckrealloc(spanPtr, SPAN_SIZE(newSize));
	if (newPtr == NULL) {
	    newSize = (needed < SPAN_MAX - TCL_MIN_GROWTH) ?
		    needed + TCL_MIN_GROWTH : SPAN_MAX;

	    newPtr = attemptckrealloc(spanPtr, SPAN_SIZE(newSize));
	    if (newPtr == NULL) {
		newSize = needed;
		newPtr = attemptckrealloc(spanPtr, SPAN_SIZE(newSize));
		if (newPtr == NULL) {
		    if (interp) {
			Tcl_SetObjResult(interp, Tcl_ObjPrintf(
				"unable to alloc %lu bytes",
				SPAN_SIZE(newSize)));
			Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
		    }
		    return TCL_ERROR;
		}
	    }
	}
	spanPtr = newPtr;
	spanPtr->size = newSize;
	listPtr->span[spanIdx] = spanPtr;
    }

    spanPtr->objv[spanPtr->last] = objPtr;
    Tcl_IncrRefCount(objPtr);
    spanPtr->last++;

    listPtr->length++;
    *listPtrPtr = listPtr;
    return TCL_OK;
}

static Span *
SpanAllocate(
    Tcl_Interp *interp,
    unsigned short int numElements)
{
    Span *spanPtr = attemptckalloc(SPAN_SIZE(numElements));

    if (spanPtr == NULL) {
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "list creation failed: unable to aloc %lu bytes",
		    SPAN_SIZE(numElements)));
	    Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
	}
	return NULL;
    }
    spanPtr->size = numElements;
    spanPtr->refCount = 1;
    spanPtr->first = 0;
    spanPtr->last = 0;
    return spanPtr;
}

static void
SpanRelease(
    Span *spanPtr)
{
    unsigned short int i = spanPtr->first;
    unsigned short int end = spanPtr->last;

    if (--spanPtr->refCount) {
	return;
    }
    while (i < end) {
	Tcl_DecrRefCount(spanPtr->objv[i++]);
    }
    ckfree(spanPtr);
}

