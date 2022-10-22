/*
 * tclAbstractList.h --
 *
 *	The AbstractList Obj Type -- a psuedo List
 *
 * Copyright © 2022 by Brian Griffin. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLABSTRACTLIST
#define _TCLABSTRACTLIST

#include "tclInt.h"

Tcl_AbstractListType * Tcl_AbstractListGetType(Tcl_Obj *objPtr);

static inline const char*
Tcl_AbstractListTypeName(
    Tcl_Obj *objPtr) /* Should be of type AbstractList */
{
    Tcl_AbstractListType *typePtr;
    typePtr = Tcl_AbstractListGetType(objPtr);
    if (typePtr && typePtr->typeName) {
	return typePtr->typeName;
    } else {
	return "abstractlist";
    }
}

Tcl_Obj *   Tcl_AbstractListObjNew(Tcl_Interp *interp, const Tcl_AbstractListType *);
Tcl_WideInt Tcl_AbstractListObjLength(Tcl_Obj *abstractListPtr);
int	    Tcl_AbstractListObjIndex(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_WideInt index, Tcl_Obj **elemObj);
int	    Tcl_AbstractListObjRange(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_WideInt fromIdx, Tcl_WideInt toIdx, Tcl_Obj **newObjPtr);
int	    Tcl_AbstractListObjReverse(Tcl_Interp *interp, Tcl_Obj *abstractListPtr,
		Tcl_Obj **newObjPtr);
int	    Tcl_AbstractListObjGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, int *objcPtr,
		Tcl_Obj ***objvPtr);
Tcl_Obj *   Tcl_AbstractListObjCopy(Tcl_Interp *interp, Tcl_Obj *listPtr);
void	*   Tcl_AbstractListGetConcreteRep(Tcl_Obj *objPtr);
Tcl_Obj *   Tcl_AbstractListSetElement(Tcl_Interp *interp, Tcl_Obj *listPtr,
				       Tcl_Obj *indicies, Tcl_Obj *valueObj);
int Tcl_AbstractListObjReplace(
    Tcl_Interp *interp,		  /* Used for error reporting if not NULL. */
    Tcl_Obj *listObj,		  /* List object whose elements to replace. */
    ListSizeT first,		  /* Index of first element to replace. */
    ListSizeT numToDelete,	  /* Number of elements to replace. */
    ListSizeT numToInsert,	  /* Number of objects to insert. */
    Tcl_Obj *const insertObjs[]); /* Tcl objects to insert */

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
