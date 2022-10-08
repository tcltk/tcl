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

Tcl_Obj *   Tcl_NewAbstractListObj(Tcl_Interp *interp, const Tcl_AbstractListType *);
Tcl_WideInt Tcl_AbstractListObjLength(Tcl_Obj *abstractListPtr);
int         Tcl_AbstractListObjIndex(Tcl_Obj *abstractListPtr, Tcl_WideInt index, Tcl_Obj **elemObj);
Tcl_Obj *   Tcl_AbstractListObjRange(Tcl_Obj *abstractListPtr, Tcl_WideInt fromIdx, Tcl_WideInt toIdx);
Tcl_Obj *   Tcl_AbstractListObjReverse(Tcl_Obj *abstractListPtr);
int         Tcl_AbstractListObjGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, int *objcPtr, Tcl_Obj ***objvPtr);
Tcl_Obj *   Tcl_AbstractListObjCopy(Tcl_Interp *interp, Tcl_Obj *listPtr);

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
