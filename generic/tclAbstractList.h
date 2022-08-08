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

static inline AbstractList* AbstractListGetInternalRep(
    Tcl_Obj *objPtr)         /* Object of type AbstractList */
{									
    const Tcl_ObjInternalRep *irPtr;
    irPtr = TclFetchInternalRep((objPtr), &tclAbstractListType);
    return irPtr ? (AbstractList *)(irPtr->twoPtrValue.ptr1) : NULL;
}


static inline const char*
Tcl_AbstractListTypeName(
    Tcl_Obj *objPtr) /* Should be of type AbstractList */
{
    AbstractList *abstractListRepPtr = 
	AbstractListGetInternalRep(objPtr);
    return (abstractListRepPtr->typeName 
	    ? abstractListRepPtr->typeName 
	    : (objPtr->typePtr 
	       ? objPtr->typePtr->name
	       : "pure string"));
}


Tcl_Obj*    Tcl_NewAbstractListObj(Tcl_Interp *interp, const char* typeName, size_t requiredSize);
int         Tcl_AbstractListCheckedSetProc(Tcl_Obj *objPtr, Tcl_AbstractListProcType ptype, void** procPtr);
Tcl_WideInt Tcl_AbstractListObjLength(Tcl_Obj *abstractListPtr);
Tcl_Obj*    Tcl_AbstractListObjIndex(Tcl_Obj *abstractListPtr, Tcl_WideInt index);
Tcl_Obj*    Tcl_AbstractListObjRange(Tcl_Obj *abstractListPtr, Tcl_WideInt fromIdx, Tcl_WideInt toIdx);
Tcl_Obj*    Tcl_AbstractListObjReverse(Tcl_Obj *abstractListPtr);
int         Tcl_AbstractListObjGetElements(Tcl_Interp *interp, Tcl_Obj *objPtr, int *objcPtr, Tcl_Obj ***objvPtr);

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
