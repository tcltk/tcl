// Tcl Abstract List test command: "lstring"
#include <string.h>
#include <limits.h>
#include "tcl.h"

/*
 * Forward references
 */

Tcl_Obj *myNewLStringObj(Tcl_WideInt start,
			 Tcl_WideInt length);
static void freeRep(Tcl_Obj* alObj);
static Tcl_Obj* my_LStringObjSetElem(Tcl_Interp *interp,
				     Tcl_Obj *listPtr,
				     Tcl_Obj *indicies,
				     Tcl_Obj *valueObj);
static void DupLStringRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static Tcl_WideInt my_LStringObjLength(Tcl_Obj *lstringObjPtr);
static int my_LStringObjIndex(Tcl_Interp *interp,
			      Tcl_Obj *lstringObj,
			      Tcl_WideInt index,
			      Tcl_Obj **charObjPtr);
static int my_LStringObjRange(Tcl_Interp *interp, Tcl_Obj *lstringObj,
			      Tcl_WideInt fromIdx, Tcl_WideInt toIdx,
			      Tcl_Obj **newObjPtr);
static int my_LStringObjReverse(Tcl_Interp *interp, Tcl_Obj *srcObj,
			  Tcl_Obj **newObjPtr);
static int my_LStringReplace(Tcl_Interp *interp,
		      Tcl_Obj *listObj,
		      Tcl_WideInt first,
		      Tcl_WideInt numToDelete,
		      Tcl_WideInt numToInsert,
		      Tcl_Obj *const insertObjs[]);

/*
 * Internal Representation of an lstring type value
 */

typedef struct LString {
    char *string;          // NULL terminated utf-8 string
    Tcl_WideInt strlen;    // num bytes in string
    Tcl_WideInt allocated; // num bytes allocated
} LString;

/*
 * AbstractList definition of an lstring type
 */
static Tcl_AbstractListType lstringType = {
	TCL_ABSTRACTLIST_VERSION_1,
	"lstring",
	NULL,
	DupLStringRep,
	my_LStringObjLength,
	my_LStringObjIndex,
	my_LStringObjRange,/*ObjRange*/
	my_LStringObjReverse,
        NULL/*my_LStringGetElements*/,
        freeRep,
	NULL /*toString*/,
	my_LStringObjSetElem, /* use default update string */
	my_LStringReplace
};




/*
 *----------------------------------------------------------------------
 *
 * my_LStringObjIndex --
 *
 *	Implements the AbstractList Index function for the lstring type.  The
 *	Index function returns the value at the index position given. Caller
 *	is resposible for freeing the Obj.
 *
 * Results:
 *	TCL_OK on success. Returns a new Obj, with a 0 refcount in the
 *	supplied charObjPtr location. Call has ownership of the Obj.
 *
 * Side effects:
 *	Obj allocated.
 *
 *----------------------------------------------------------------------
 */

static int
my_LStringObjIndex(
    Tcl_Interp *interp,
    Tcl_Obj *lstringObj,
    Tcl_WideInt index,
    Tcl_Obj **charObjPtr)
{
  LString *lstringRepPtr = (LString*)Tcl_AbstractListGetConcreteRep(lstringObj);

  (void)interp;

  if (0 <= index && index < lstringRepPtr->strlen) {
      char cchar[2];
      cchar[0] = lstringRepPtr->string[index];
      cchar[1] = 0;
      *charObjPtr = Tcl_NewStringObj(cchar,1);
  } else {
      *charObjPtr = Tcl_NewObj();
  }

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * my_LStringObjLength --
 *
 *	Implements the AbstractList Length function for the lstring type.
 *	The Length function returns the number of elements in the list.
 *
 * Results:
 *	WideInt number of elements in the list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_WideInt
my_LStringObjLength(Tcl_Obj *lstringObjPtr)
{
    LString *lstringRepPtr = (LString *)Tcl_AbstractListGetConcreteRep(lstringObjPtr);
    return lstringRepPtr->strlen;
}


/*
 *----------------------------------------------------------------------
 *
 * DupLStringRep --
 *
 *	Replicates the internal representation of the src value, and storing
 *	it in the copy
 *
 * Results:
 *	void
 *
 * Side effects:
 *	Modifies the rep of the copyObj.
 *
 *----------------------------------------------------------------------
 */

static void
DupLStringRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr)
{
  LString *srcLString = (LString*)Tcl_AbstractListGetConcreteRep(srcPtr);
  LString *copyLString = (LString*)Tcl_Alloc(sizeof(LString));

  memcpy(copyLString, srcLString, sizeof(LString));
  copyLString->string = (char*)Tcl_Alloc(srcLString->allocated);
  strcpy(copyLString->string, srcLString->string);
  Tcl_AbstractListSetConcreteRep(copyPtr,copyLString);

  return;
}

/*
 *----------------------------------------------------------------------
 *
 * my_LStringObjSetElem --
 *
 *	Replace the element value at the given (nested) index with the
 *	valueObj provided.  If the lstring obj is shared, a new list is
 *	created conntaining the modifed element.
 *
 * Results:
 *	The modifed lstring is returned, either new or original. If the
 *	index is invalid, NULL is returned, and an error is added to the
 *	interp, if provided.
 *
 * Side effects:
 *	A new obj may be created.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
my_LStringObjSetElem(
    Tcl_Interp *interp,
    Tcl_Obj *lstringObj,
    Tcl_Obj *indicies,
    Tcl_Obj *valueObj)
{
    LString *lstringRepPtr = (LString*)Tcl_AbstractListGetConcreteRep(lstringObj);
    int indc;
    Tcl_Obj **indv;
    int index;
    const char *newvalue;
    int status;
    Tcl_Obj *returnObj;

    if (Tcl_ListObjGetElements(interp, indicies, &indc, &indv) != TCL_OK) {
	return NULL;
    }

    if (indc > 1) {
	Tcl_SetObjResult(interp,
	    Tcl_ObjPrintf("Multiple indicies not supported by lstring."));
	return NULL;
    }

    status = Tcl_GetIntForIndex(interp, indv[0], lstringRepPtr->strlen, &index);
    if (status != TCL_OK) {
	return NULL;
    }

    returnObj = Tcl_IsShared(lstringObj) ? Tcl_DuplicateObj(lstringObj) : lstringObj;
    lstringRepPtr = (LString*)Tcl_AbstractListGetConcreteRep(returnObj);

    if (index < 0) {
	index = 0;
    }
    if (index >= lstringRepPtr->strlen) {
	index = lstringRepPtr->strlen;
	lstringRepPtr->strlen++;
	lstringRepPtr->string = (char*)Tcl_Realloc(lstringRepPtr->string, lstringRepPtr->strlen+1);
    }

    newvalue = Tcl_GetString(valueObj);
    lstringRepPtr->string[index] = newvalue[0];

    Tcl_InvalidateStringRep(returnObj);

    return returnObj;
}

/*
 *----------------------------------------------------------------------
 *
 * my_LStringObjRange --
 *
 *	Creates a new Obj with a slice of the src listPtr.
 *
 * Results:
 *	A new Obj is assigned to newObjPtr. Returns TCL_OK
 *
 * Side effects:
 *	A new Obj is created.
 *
 *----------------------------------------------------------------------
 */

static int my_LStringObjRange(
    Tcl_Interp *interp,
    Tcl_Obj *lstringObj,
    Tcl_WideInt fromIdx,
    Tcl_WideInt toIdx,
    Tcl_Obj **newObjPtr)
{
    Tcl_Obj *rangeObj;
    LString *lstringRepPtr = (LString*)Tcl_AbstractListGetConcreteRep(lstringObj);
    LString *rangeRep;
    Tcl_WideInt len = toIdx - fromIdx + 1;

    if ((fromIdx < 0 || lstringRepPtr->strlen < fromIdx) ||
	(toIdx < 0 || lstringRepPtr->strlen < toIdx)) {
	Tcl_SetObjResult(interp,
	    Tcl_ObjPrintf("Range out of bounds "));
	return TCL_ERROR;
    }

    rangeObj = Tcl_AbstractListObjNew(interp, &lstringType);

    if (len <= 0) {
	// Return empty value;
	*newObjPtr = Tcl_NewObj();
    } else {
	rangeRep = (LString*)Tcl_Alloc(sizeof(LString));
	rangeRep->allocated = len+1;
	rangeRep->strlen = len;
	rangeRep->string = (char*)Tcl_Alloc(rangeRep->allocated);
	strncpy(rangeRep->string,&lstringRepPtr->string[fromIdx],len);
	rangeRep->string[len] = 0;
	Tcl_AbstractListSetConcreteRep(rangeObj, rangeRep);
	*newObjPtr = rangeObj;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * my_LStringObjReverse --
 *
 *	Creates a new Obj with the the order of the elements in the lstring
 *	value reversed, where first is last and last is first, etc.
 *
 * Results:
 *	A new Obj is assigned to newObjPtr. Returns TCL_OK
 *
 * Side effects:
 *	A new Obj is created.
 *
 *----------------------------------------------------------------------
 */

static int
my_LStringObjReverse(Tcl_Interp *interp, Tcl_Obj *srcObj, Tcl_Obj **newObjPtr)
{
    Tcl_Obj *revObj = Tcl_AbstractListObjNew(interp, &lstringType);
    LString *srcRep = (LString*)Tcl_AbstractListGetConcreteRep(srcObj);
    LString *revRep = (LString*)Tcl_Alloc(sizeof(LString));
    Tcl_WideInt len;
    char *srcp, *dstp, *endp;
    len = srcRep->strlen;
    revRep->strlen = len;
    revRep->allocated = len+1;
    revRep->string = (char*)Tcl_Alloc(revRep->allocated);
    srcp = srcRep->string;
    endp = &srcRep->string[len];
    dstp = &revRep->string[len];
    *dstp-- = 0;
    while (srcp < endp) {
	*dstp-- = *srcp++;
    }
    Tcl_AbstractListSetConcreteRep(revObj, revRep);
    *newObjPtr = revObj;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * my_LStringReplace --
 *
 *	Delete and/or Insert elements in the list, starting at index first.
 *	See more details in the comments below. This should not be called with
 *	a Shared Obj.
 *
 * Results:
 *	The value of the listObj is modified.
 *
 * Side effects:
 *	The string rep is invalidated.
 *
 *----------------------------------------------------------------------
 */

static int
my_LStringReplace(
    Tcl_Interp *interp,
    Tcl_Obj *listObj,
    Tcl_WideInt first,
    Tcl_WideInt numToDelete,
    Tcl_WideInt numToInsert,
    Tcl_Obj *const insertObjs[])
{
    LString *lstringRep = (LString*)Tcl_AbstractListGetConcreteRep(listObj);
    Tcl_WideInt newLen;
    Tcl_WideInt x, ix, kx;
    char *newStr;
    char *oldStr = lstringRep->string;
    (void)interp;

    if (numToDelete < 0) numToDelete = 0;
    if (numToInsert < 0) numToInsert = 0;

    newLen = lstringRep->strlen - numToDelete + numToInsert;

    if (newLen >= lstringRep->allocated) {
	lstringRep->allocated = newLen+1;
	newStr = Tcl_Alloc(lstringRep->allocated);
	newStr[newLen] = 0;
    } else {
	newStr = oldStr;
    }

    /* Tcl_ListObjReplace replaces zero or more elements of the list
     * referenced by listPtr with the objc values in the array referenced by
     * objv.
     *
     * If listPtr does not point to a list value, Tcl_ListObjReplace
     * will attempt to convert it to one; if the conversion fails, it returns
     * TCL_ERROR and leaves an error message in the interpreter's result value
     * if interp is not NULL. Otherwise, it returns TCL_OK after replacing the
     * values.
     *
     *    * If objv is NULL, no new elements are added.
     *
     *    * If the argument first is zero or negative, it refers to the first
     *      element.
     *
     *    * If first is greater than or equal to the number of elements in the
     *      list, then no elements are deleted; the new elements are appended
     *      to the list. count gives the number of elements to replace.
     *
     *    * If count is zero or negative then no elements are deleted; the new
     *      elements are simply inserted before the one designated by first.
     *      Tcl_ListObjReplace invalidates listPtr's old string representation.
     *
     *    * The reference counts of any elements inserted from objv are
     *      incremented since the resulting list now refers to them. Similarly,
     *      the reference counts for any replaced values are decremented.
     */

    // copy 0 to first-1
    if (newStr != oldStr) {
	strncpy(newStr, oldStr, first);
    }

    // move front elements to keep
    for(x=0, kx=0; x<newLen && kx<first; kx++, x++) {
	newStr[x] = oldStr[kx];
    }
    // Insert new elements into new string
    for(x=first, ix=0; ix<numToInsert; x++, ix++) {
	char const *svalue = Tcl_GetString(insertObjs[ix]);
	newStr[x] = svalue[0];
    }
    // Move remaining elements
    if ((first+numToDelete) < newLen) {
	for(/*x,*/ kx=first+numToDelete; (kx <lstringRep->strlen && x<newLen); x++, kx++) {
	    newStr[x] = oldStr[kx];
	}
    }

    // Terminate new string.
    newStr[newLen] = 0;


    if (oldStr != newStr) {
	Tcl_Free(oldStr);
    }
    lstringRep->string = newStr;
    lstringRep->strlen = newLen;

    /* Changes made to value, string rep no longer valid */
    Tcl_InvalidateStringRep(listObj);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * my_NewLStringObj --
 *
 *	Creates a new lstring Obj using the string value of objv[0]
 *
 * Results:
 *	results
 *
 * Side effects:
 *	side effects
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
my_NewLStringObj(
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj * const objv[])
{
    LString *lstringRepPtr;
    size_t repSize;
    Tcl_Obj *lstringPtr;
    const char *string;

    if (objc != 1) {
	return NULL;
    }
    string = Tcl_GetString(objv[0]);

    repSize = sizeof(LString);
    lstringPtr = Tcl_AbstractListObjNew(interp, &lstringType);
    lstringRepPtr = (LString*)Tcl_Alloc(repSize);
    lstringRepPtr->strlen = strlen(string);
    lstringRepPtr->allocated = lstringRepPtr->strlen + 1;
    lstringRepPtr->string = (char*)Tcl_Alloc(lstringRepPtr->allocated);
    strcpy(lstringRepPtr->string, string);
    Tcl_AbstractListSetConcreteRep(lstringPtr, lstringRepPtr);
    if (lstringRepPtr->strlen > 0) {
	Tcl_InvalidateStringRep(lstringPtr);
    } else {
	Tcl_InitStringRep(lstringPtr, NULL, 0);
    }

    return lstringPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * freeRep --
 *
 *	Free the value storage of the lstring Obj.
 *
 * Results:
 *	void
 *
 * Side effects:
 *	Memory free'd.
 *
 *----------------------------------------------------------------------
 */

static void
freeRep(Tcl_Obj* lstringObj)
{
    LString *lstringRepPtr = (LString*)Tcl_AbstractListGetConcreteRep(lstringObj);
    if (lstringRepPtr->string) {
	Tcl_Free(lstringRepPtr->string);
    }
    Tcl_Free((char*)lstringRepPtr);
    Tcl_AbstractListSetConcreteRep(lstringObj, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * lLStringObjCmd --
 *
 *	Script level command that creats an lstring Obj value.
 *
 * Results:
 *	Returns and lstring Obj value in the interp results.
 *
 * Side effects:
 *	Interp results modified.
 *
 *----------------------------------------------------------------------
 */

static int
lLStringObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj * const objv[])
{
  Tcl_Obj *lstringObj;
  (void)clientData;
  if (objc != 2) {
      Tcl_WrongNumArgs(interp, 1, objv, "string");
      return TCL_ERROR;
  }

  lstringObj = my_NewLStringObj(interp, objc-1, &objv[1]);

  if (lstringObj) {
      Tcl_SetObjResult(interp, lstringObj);
      return TCL_OK;
  }
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Lstring_Init --
 *
 *	DL load init function. Defines the "lstring" command.
 *
 * Results:
 *	"lstring" command added to the interp.
 *
 * Side effects:
 *	A new command is defined.
 *
 *----------------------------------------------------------------------
 */

int Tcl_ABSListTest_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.7", 0) == NULL) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand(interp, "lstring", lLStringObjCmd, NULL, NULL);
    Tcl_PkgProvide(interp, "abstractlisttest", "1.0.0");
    return TCL_OK;
}
