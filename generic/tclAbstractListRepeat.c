/*
 * TODO - Tcl_ALNewObjProc should take an interp for error reporting.
 *   May be other functions too?
 */
/*
 * tclAbstractListRepeat.c --
 *
 *     This file implements the concrete representation for an AbstractList
 *     containing repeated elements.
 *
 * Copyright © 2022 Ashok P. Nadkarni.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <assert.h>
#include "tcl.h"
#include "tclInt.h"
#include "tclAbstractList.h"

/*
 * The structure below defines the arithmetic series Tcl Obj Type by means of
 * procedures that can be invoked by generic object code.
 *
 * The repeated list object is a Tcl_AbstractList representing a sequence
 * of elements repeated a given number of times. The lrepeat command
 * generates such lists.
 *
 * The internal representation consists of a Tcl_Obj holding the list
 * of elements (this may itself be an abstract list) and a count of
 * how many times that list is repeated.
 */

static void DupRepeatedListRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void FreeRepeatedListRep(Tcl_Obj *repeatedListObj);
static Tcl_WideInt RepeatedListLength(Tcl_Obj *repeatedListObj);
static int RepeatedListIndex(Tcl_Interp *interp, Tcl_Obj *repeatedListObj, Tcl_WideInt index, Tcl_Obj **elemObj);
static int RepeatedGetElements(Tcl_Interp *interp, Tcl_Obj *repeatetListObj, int *objcPtr, Tcl_Obj ***objvPtr);

static Tcl_AbstractListType repeatedListType = {
	TCL_ABSTRACTLIST_VERSION_1,
	"repeatedlist",
	NULL, /* TBD - newObjProc - mandatory? */
	DupRepeatedListRep,
	RepeatedListLength,
	RepeatedListIndex,
	NULL, /* TODO - slice - needed? */
	NULL, /* TODO - reverse - needed? */
	RepeatedGetElements, /* GetElements */
	FreeRepeatedListRep,
	NULL /* TBD - UpdateString - needed? */
};

typedef struct RepeatedListRep {
    Tcl_Obj *elements; /* List of repeated elements. May be an abstract list */
    Tcl_WideInt nElements; /* Cached size of elements. */
    Tcl_WideInt nTotal; /* Total number of elements in abstract list */
    Tcl_Obj **elemList; /* traditional element list to support 'GetElements' */
} RepeatedListRep;


/*
 *----------------------------------------------------------------------
 *
 * DupRepeatedListRep --
 *
 *	Initialize the internal representation of a repeated list
 *	Tcl_Obj to a copy of the internal representation of an one.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	We set "copyPtr"s internal rep to a pointer to a
 *	newly allocated AbstractList structure.
 *----------------------------------------------------------------------
 */

static void
DupRepeatedListRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr)
{
    RepeatedListRep *srcRepPtr =
	(RepeatedListRep *)Tcl_AbstractListGetConcreteRep(srcPtr);
    RepeatedListRep *copyRepPtr =
	(RepeatedListRep *)ckalloc(sizeof(RepeatedListRep));

    *copyRepPtr = *srcRepPtr;
    Tcl_IncrRefCount(copyRepPtr->elements);
    copyRepPtr->elemList = NULL; /* let copy regenrate list if needed */

    /* Note: we do not have to be worry about existing internal rep because
       copyPtr is supposed to be freshly initialized */
    Tcl_AbstractListSetConcreteRep(copyPtr, copyRepPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * FreeRepeatedListRep --
 *
 *	Free any allocated memory in the ArithSeries Rep
 *
 * Results:
 *	None.
 *
 * Side effects:
 * 	The internal representation is freed.
 *
 *----------------------------------------------------------------------
 */
static void
FreeRepeatedListRep(Tcl_Obj *repeatedListObj)  /* Free any allocated memory */
{
    RepeatedListRep *repPtr =
	(RepeatedListRep *)Tcl_AbstractListGetConcreteRep(repeatedListObj);
    if (repPtr) {
	if (repPtr->elemList) {
	    int i;
	    for (i = 0; i < repPtr->nTotal; i++) {
		Tcl_DecrRefCount(repPtr->elemList[i]);
	    }
	    ckfree((char*)repPtr->elemList);
	}
	if (repPtr->elements) {
	    Tcl_DecrRefCount(repPtr->elements);
	}
	ckfree((char*)repPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewRepeatedListObj --
 *
 *	Creates a new repeated list object. The returned object has
 *	refcount = 0.
 *
 * Results:
 *
 * 	A Tcl_Obj pointer to the created object.
 * 	A NULL pointer on errors.
 *
 * Side Effects:
 *
 * 	None.
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclNewRepeatedListObj(Tcl_Interp *interp,
		      Tcl_WideInt repetitions,
		      Tcl_Obj *elementsObj)
{
#if TCL_MAJOR_VERSION == 8
    int subLen;
#else
    size_t subLen;
#endif
    Tcl_WideInt numElements;
    RepeatedListRep *repPtr;
    Tcl_Obj *resultObj;

    if (repetitions < 0) {
	if (interp) {
	    Tcl_SetObjResult(
		interp, Tcl_NewStringObj("Negative repetitions specified.", -1));
	}
	return NULL;
    }
    if (elementsObj) {
	if (Tcl_ListObjLength(interp, elementsObj, &subLen) != TCL_OK) {
	    return NULL;
	}
	if (subLen && repetitions > (LIST_MAX / subLen)) {
	    if (interp) {
		Tcl_SetObjResult(
		    interp,
		    Tcl_NewStringObj("Maximum list length exceeded.", -1));
	    }
	    return NULL;
	}
	numElements = subLen;
    } else {
	numElements = 0;
    }

    repPtr = (RepeatedListRep *)ckalloc(sizeof(*repPtr));
    if (elementsObj) {
	Tcl_IncrRefCount(elementsObj);
    }
    repPtr->elements = elementsObj;
    repPtr->nElements = numElements;
    repPtr->nTotal = numElements * repetitions;
    repPtr->elemList = NULL;

    resultObj = Tcl_NewAbstractListObj(interp, &repeatedListType);
    Tcl_AbstractListSetConcreteRep(resultObj, repPtr);
    return resultObj;
}

/*
 *----------------------------------------------------------------------
 *
 * RepeatedListLength
 *
 *	Returns the length of the repeated list
 *
 * Results:
 *
 * 	The length of the series as Tcl_WideInt.
 *
 * Side Effects:
 *
 * 	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_WideInt RepeatedListLength(Tcl_Obj *repeatedListObj)
{
    assert(Tcl_AbstractListGetType(repeatedListObj) == &repeatedListType);

    RepeatedListRep *repPtr =
	(RepeatedListRep *)Tcl_AbstractListGetConcreteRep(repeatedListObj);
    return repPtr->nTotal;
}

/*
 *----------------------------------------------------------------------
 *
 * RepeatedListIndex --
 *
 *	Returns the element with the specified index in the list.
 *
 * Results:
 *
 * 	The Tcl_Obj at the specified index or NULL if index is out of range.
 *	The returned Tcl_Obj does NOT have its reference count incremented.
 *
 * Side Effects:
 *
 *	None.
 *----------------------------------------------------------------------
 */
int
RepeatedListIndex(Tcl_Interp *interp, Tcl_Obj *repeatedListObj, Tcl_WideInt index, Tcl_Obj **elemObjPtr)
{
    RepeatedListRep *repPtr;
    Tcl_WideInt offset;
    Tcl_Obj *elemObj;

    repPtr = (RepeatedListRep *)Tcl_AbstractListGetConcreteRep(repeatedListObj);

    if (index < 0 || index >= repPtr->nTotal) {
	/* Traditionally, index out of bounds results in empty string */
	TclNewObj(*elemObjPtr);
	return TCL_OK;
    }

    offset = index % repPtr->nElements;
    if (Tcl_ListObjIndex(NULL, repPtr->elements, offset, &elemObj) != TCL_OK) {
	(void)interp; /* TODO - error message */
	return TCL_ERROR;
    } else {
        *elemObjPtr = elemObj;
	return TCL_OK;
    }
}

/*
** Handle GetElements call
*/

int
RepeatedGetElements(
    Tcl_Interp *interp,		/* Used to report errors if not NULL. */
    Tcl_Obj *repeatedListObj,	/* List object for which an element
				 * array is to be returned. */
    int *objcPtr,		/* Where to store the count of objects
				 * referenced by objv. */
    Tcl_Obj ***objvPtr)		/* Where to store the pointer to an array of
				 * pointers to the list's objects. */
{
    RepeatedListRep *repPtr =
	(RepeatedListRep*)Tcl_AbstractListGetConcreteRep(repeatedListObj);
    Tcl_Obj **objv;
    int i, objc;

    objc = repPtr->nTotal;

    if (objvPtr == NULL) {
	if (objcPtr) {
	    *objcPtr = objc;
	    return TCL_OK;
	}
	return TCL_ERROR;
    }

    if (objc && objvPtr && repPtr->elemList) {
	objv = repPtr->elemList;
    } else if (objc > 0) {
	objv = (Tcl_Obj **)ckalloc(sizeof(Tcl_Obj*) * objc);
	if (objv == NULL) {
	    if (interp) {
		Tcl_SetObjResult(
		    interp,
		    Tcl_NewStringObj("max length of a Tcl list exceeded", -1));
		Tcl_SetErrorCode(interp, "TCL", "MEMORY", NULL);
	    }
	    return TCL_ERROR;
	}
	for (i = 0; i < objc; i++) {
	    if (RepeatedListIndex(interp, repeatedListObj, i, &objv[i]) == TCL_OK) {
		Tcl_IncrRefCount(objv[i]);
	    } else {
		// TODO: some cleanup needed here
		return TCL_ERROR;
	    }
	}
    } else {
	objv = NULL;
    }
    repPtr->elemList = objv;
    *objvPtr = objv;
    *objcPtr = objc;
    return TCL_OK;
}



/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
