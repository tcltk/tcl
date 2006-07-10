/*
 * tclOO.c --
 *
 *	This file contains the object-system core (NB: not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2005 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOO.c,v 1.1.2.6 2006/07/10 01:17:32 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

void			TclOOInit(Tcl_Interp *interp);
Tcl_Method		Tcl_OONewMethod(Tcl_Interp *interp, Tcl_Object object,
			    Tcl_Obj *nameObj, int isPublic,
			    Tcl_OOMethodCallProc callProc,
			    ClientData clientData,
			    Tcl_OOMethodDeleteProc deleteProc);
Tcl_Method		Tcl_OONewClassMethod(Tcl_Interp *interp, Tcl_Class cls,
			    Tcl_Obj *nameObj, int isPublic,
			    Tcl_OOMethodCallProc callProc,
			    ClientData clientData,
			    Tcl_OOMethodDeleteProc deleteProc);


#define ALLOC_CHUNK 8

/*
 * Function declarations.
 */

static Class *		AllocClass(Tcl_Interp *interp, Object *useThisObj);
static Object *		AllocObject(Tcl_Interp *interp, const char *nameStr);
static int		DeclareClassMethod(Tcl_Interp *interp, Class *clsPtr,
			    const char *name, int isPublic,
			    Tcl_OOMethodCallProc callProc);
static void		AddClassMethodNames(Class *clsPtr, int publicOnly,
			    Tcl_HashTable *namesPtr);
static void		AddMethodToCallChain(Tcl_HashTable *methodTablePtr,
			    Tcl_Obj *methodObj, CallContext *contextPtr,
			    int isFilter, int isPublic);
static void		AddSimpleChainToCallContext(Object *oPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    int isFilter, int isPublic);
static void		AddSimpleClassChainToCallContext(Class *classPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    int isFilter, int isPublic);
static int		CmpStr(const void *ptr1, const void *ptr2);
static void		DeleteContext(CallContext *contextPtr);
static CallContext *	GetCallContext(Foundation *fPtr, Object *oPtr,
			    Tcl_Obj *methodNameObj, int isPublic,
			    Tcl_HashTable *cachePtr);
static int		InvokeContext(Tcl_Interp *interp,
			    CallContext *contextPtr, int objc,
			    Tcl_Obj *const *objv);
static int		ObjectCmd(Object *oPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv, int publicOnly,
			    Tcl_HashTable *cachePtr);
static Object *		NewInstance(Tcl_Interp *interp, Class *clsPtr,
			    char *name, int objc, Tcl_Obj *const *objv);
static Method *		NewProcMethod(Tcl_Interp *interp, Object *oPtr,
			    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
			    Tcl_Obj *bodyObj);
static void		ObjectNamespaceDeleted(ClientData clientData);
static void		ObjNameChangedTrace(ClientData clientData,
			    Tcl_Interp *interp, const char *oldName,
			    const char *newName, int flags);

static int		PublicObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);

static int		InvokeProcedureMethod(ClientData clientData,
			    Tcl_Interp *interp, CallContext *oPtr,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteProcedureMethod(ClientData clientData);
static int		InvokeForwardMethod(ClientData clientData,
			    Tcl_Interp *interp, CallContext *oPtr,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteForwardMethod(ClientData clientData);

static int		ClassCreate(ClientData clientData, Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);
static int		ClassNew(ClientData clientData, Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);
static int		ObjectDestroy(ClientData clientData,Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);
static int		ObjectEval(ClientData clientData, Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);
static int		ObjectLinkVar(ClientData clientData,Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);
static int		ObjectUnknown(ClientData clientData,Tcl_Interp *interp,
			    CallContext *oPtr, int objc, Tcl_Obj *const *objv);

void
TclOOInit(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr;

    fPtr = iPtr->ooFoundation = (Foundation *) ckalloc(sizeof(Foundation));
    Tcl_CreateNamespace(interp, "::oo", fPtr, NULL);
    Tcl_CreateNamespace(interp, "::oo::define", NULL, NULL);
    fPtr->helpersNs = Tcl_CreateNamespace(interp, "::oo::Helpers", NULL,
	    NULL);

    fPtr->objStack = NULL;
    fPtr->objectCls = AllocClass(interp, AllocObject(interp, "::oo::object"));
    fPtr->classCls = AllocClass(interp, AllocObject(interp, "::oo::class"));
    fPtr->objectCls->thisPtr->selfCls = fPtr->classCls;
    fPtr->objectCls->numSuperclasses = 0;
    ckfree((char *) fPtr->objectCls->superclasses);
    fPtr->objectCls->superclasses = NULL;
    fPtr->classCls->thisPtr->selfCls = fPtr->classCls;

    DeclareClassMethod(interp, fPtr->objectCls, "destroy", 1, ObjectDestroy);
    DeclareClassMethod(interp, fPtr->objectCls, "eval", 0, ObjectEval);
    DeclareClassMethod(interp, fPtr->objectCls, "unknown", 0, ObjectUnknown);
    DeclareClassMethod(interp, fPtr->objectCls, "variable", 0, ObjectLinkVar);
    DeclareClassMethod(interp, fPtr->classCls, "create", 1, ClassCreate);
    DeclareClassMethod(interp, fPtr->classCls, "new", 1, ClassNew);

    /*
     * TODO: finish splicing "object" and "class" together.
     * Need to create the following methods:
     *	- object::destroy
     *	- object::eval
     *	- object::unknown
     *	- object::variable
     *	- class::create
     *	- class::new
     */

    fPtr->definerCls = AllocClass(interp,
	    AllocObject(interp, "::oo::definer"));
    fPtr->structCls = AllocClass(interp, AllocObject(interp, "::oo::struct"));

    /*
     * TODO: set up 'definer' and 'struct' less magically by evaluating a Tcl
     * script.
     */

    fPtr->epoch = 0;
    fPtr->nsCount = 0;
    fPtr->unknownMethodNameObj = Tcl_NewStringObj("unknown", -1);
    Tcl_IncrRefCount(fPtr->unknownMethodNameObj);

    /*
     * TODO: arrange for iPtr->ooFoundation to be torn down when the
     * interpreter is deleted.
     */
}

/*
 * ----------------------------------------------------------------------
 *
 * AllocObject --
 *
 *	Allocate an object of basic type. Does not splice the object into its
 *	class's instance list.
 *
 * ----------------------------------------------------------------------
 */

static Object *
AllocObject(
    Tcl_Interp *interp,
    const char *nameStr)
{
    Object *oPtr;
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr = iPtr->ooFoundation;
    Tcl_Obj *cmdnameObj;
    Tcl_DString buffer;

    oPtr = (Object *) ckalloc(sizeof(Object));
    do {
	char objName[10 + TCL_INTEGER_SPACE];

	sprintf(objName, "::oo::Obj%d", ++fPtr->nsCount);
	oPtr->nsPtr = (Namespace *) Tcl_CreateNamespace(interp, objName,
		oPtr, ObjectNamespaceDeleted);
    } while (oPtr->nsPtr == NULL);
    TclSetNsPath(oPtr->nsPtr, 1, &fPtr->helpersNs);
    oPtr->selfCls = fPtr->objectCls;
    Tcl_InitObjHashTable(&oPtr->methods);
    Tcl_InitObjHashTable(&oPtr->publicContextCache);
    Tcl_InitObjHashTable(&oPtr->privateContextCache);
    oPtr->numMixins = 0;
    oPtr->mixins = NULL;
    oPtr->classPtr = NULL;
    oPtr->interp = interp;

    /*
     * Initialize the traces.
     */

    Tcl_DStringInit(&buffer);
    if (nameStr) {
	if (nameStr[0] != ':' || nameStr[1] != ':') {
	    Tcl_DStringAppend(&buffer,
		    ((Namespace *) Tcl_GetCurrentNamespace(interp))->fullName,
		    -1);
	    Tcl_DStringAppend(&buffer, "::", 2);
	}
	Tcl_DStringAppend(&buffer, nameStr, -1);
    } else {
	Tcl_DStringAppend(&buffer, oPtr->nsPtr->fullName, -1);
    }
    oPtr->command = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
	    PublicObjectCmd, oPtr, NULL);
    if (nameStr) {
	Tcl_DStringFree(&buffer);
	Tcl_DStringAppend(&buffer, oPtr->nsPtr->fullName, -1);
    }
    Tcl_DStringAppend(&buffer, "::my", 4);
    oPtr->myCommand = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
	    PrivateObjectCmd, oPtr, NULL);
    Tcl_DStringFree(&buffer);

    TclNewObj(cmdnameObj);
    Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
    Tcl_TraceCommand(interp, TclGetString(cmdnameObj),
	    TCL_TRACE_RENAME|TCL_TRACE_DELETE, ObjNameChangedTrace, oPtr);
    Tcl_DecrRefCount(cmdnameObj);

    return oPtr;
}

static void
ObjNameChangedTrace(
    ClientData clientData,
    Tcl_Interp *interp,
    const char *oldName,
    const char *newName,
    int flags)
{
    Object *oPtr = clientData;

    if (newName == NULL) {
	Tcl_DeleteNamespace((Tcl_Namespace *) oPtr->nsPtr);

	/*
	 * What else to do to delete an object?
	 */
    } else {
	/*
	 * Not quite sure what to do here...
	 */
    }
}

static void
ObjectNamespaceDeleted(
    ClientData clientData)
{
    Object *oPtr = clientData;

    /*
     * Splice the object out of its context.
     */

    //TODO

    /*
     * Delete the object structure itself.
     */

    ckfree((char *) oPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * AllocClass --
 *
 *	Allocate a basic class. Does not splice the class object into its
 *	class's instance list.
 *
 * ----------------------------------------------------------------------
 */

static Class *
AllocClass(
    Tcl_Interp *interp,
    Object *useThisObj)
{
    Class *clsPtr;
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr = iPtr->ooFoundation;

    clsPtr = (Class *) ckalloc(sizeof(Class));
    if (useThisObj == NULL) {
	clsPtr->thisPtr = AllocObject(interp, NULL);
    } else {
	clsPtr->thisPtr = useThisObj;
    }
    clsPtr->thisPtr->selfCls = fPtr->classCls;
    clsPtr->thisPtr->classPtr = clsPtr;
    clsPtr->flags = 0;
    clsPtr->numSuperclasses = 1;
    clsPtr->superclasses = (Class **) ckalloc(sizeof(Class *));
    clsPtr->superclasses[0] = fPtr->objectCls;
    clsPtr->numSubclasses = 0;
    clsPtr->subclasses = NULL;
    clsPtr->subclassesSize = 0;
    clsPtr->numInstances = 0;
    clsPtr->instances = NULL;
    clsPtr->instancesSize = 0;
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    clsPtr->constructorPtr = NULL;
    clsPtr->destructorPtr = NULL;
    return clsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * NewInstance --
 *
 *	Allocate a new instance of an object.
 *
 * ----------------------------------------------------------------------
 */

static Object *
NewInstance(
    Tcl_Interp *interp,
    Class *clsPtr,
    char *name,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = AllocObject(interp, NULL);
    Class *classPtr;

    oPtr->selfCls = clsPtr;
    if (clsPtr->instancesSize == 0) {
	clsPtr->instancesSize = ALLOC_CHUNK;
	clsPtr->instances = (Object **)
		ckalloc(sizeof(Object *) * ALLOC_CHUNK);
    } else if (clsPtr->numInstances == clsPtr->instancesSize) {
	clsPtr->instancesSize += ALLOC_CHUNK;
	clsPtr->instances = (Object **) ckrealloc((char *) clsPtr->instances,
		sizeof(Object *) * clsPtr->instancesSize);
    }
    clsPtr->instances[clsPtr->numInstances++] = oPtr;

    if (name != NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	if (TclRenameCommand(interp, TclGetString(cmdnameObj),
		name) != TCL_OK) {
	    Tcl_DecrRefCount(cmdnameObj);
	    Tcl_DeleteCommandFromToken(interp, oPtr->command);
	    return NULL;
	}
	Tcl_DecrRefCount(cmdnameObj);
    }

    /*
     * Check to see if we're really creating a class. If so, allocate the
     * class structure as well.
     */

    for (classPtr=clsPtr ; classPtr->numSuperclasses>0 ;
	    classPtr=classPtr->superclasses[0]) { //TODO: fix multiple inheritance
	Foundation *fPtr = ((Interp *) interp)->ooFoundation;

	if (classPtr == fPtr->classCls) {
	    /*
	     * Is a class, so attach a class structure. Note that the
	     * AllocClass function splices the structure into the object, so
	     * we don't have to.
	     */

	    AllocClass(interp, oPtr);
	    oPtr->selfCls = clsPtr; // Repatch
	    break;
	}
    }

    // TODO: call constructors with objc/objv

    return oPtr;
}

static int
DeclareClassMethod(
    Tcl_Interp *interp,
    Class *clsPtr,
    const char *name,
    int isPublic,
    Tcl_OOMethodCallProc callPtr)
{
    Tcl_Obj *namePtr;

    TclNewStringObj(namePtr, name, strlen(name));
    Tcl_IncrRefCount(namePtr);
    Tcl_OONewClassMethod(interp, (Tcl_Class) clsPtr, namePtr, isPublic,
	    callPtr, NULL, NULL);
    TclDecrRefCount(namePtr);
    return TCL_OK;
}

Tcl_Method
Tcl_OONewMethod(
    Tcl_Interp *interp,
    Tcl_Object object,
    Tcl_Obj *nameObj,
    int isPublic,
    Tcl_OOMethodCallProc callProc,
    ClientData clientData,
    Tcl_OOMethodDeleteProc deleteProc)
{
    register Object *oPtr = (Object *) object;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    hPtr = Tcl_CreateHashEntry(&oPtr->methods, (char *) nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->deletePtr != NULL) {
	    mPtr->deletePtr(mPtr->clientData);
	}
    }
    mPtr->callPtr = callProc;
    mPtr->clientData = clientData;
    mPtr->deletePtr = deleteProc;
    mPtr->epoch = ((Interp *) interp)->ooFoundation->epoch;
    mPtr->flags = 0;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }
    return (Tcl_Method) mPtr;
}

Tcl_Method
Tcl_OONewClassMethod(
    Tcl_Interp *interp,
    Tcl_Class cls,
    Tcl_Obj *nameObj,
    int isPublic,
    Tcl_OOMethodCallProc callProc,
    ClientData clientData,
    Tcl_OOMethodDeleteProc deleteProc)
{
    register Class *clsPtr = (Class *) cls;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, (char *)nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->deletePtr != NULL) {
	    mPtr->deletePtr(mPtr->clientData);
	}
    }
    mPtr->callPtr = callProc;
    mPtr->clientData = clientData;
    mPtr->deletePtr = deleteProc;
    mPtr->epoch = ((Interp *) interp)->ooFoundation->epoch;
    mPtr->flags = 0;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }
    return (Tcl_Method) mPtr;
}

static Method *
NewProcMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    int isPublic,
    Tcl_Obj *nameObj,
    Tcl_Obj *argsObj,
    Tcl_Obj *bodyObj)
{
    int argsc;
    Tcl_Obj **argsv;
    register ProcedureMethod *pmPtr;

    if (Tcl_ListObjGetElements(interp, argsObj, &argsc, &argsv) != TCL_OK) {
	return NULL;
    }
    pmPtr = (ProcedureMethod *) ckalloc(sizeof(ProcedureMethod));
    pmPtr->bodyObj = bodyObj;
    Tcl_IncrRefCount(bodyObj);
    pmPtr->formalc = argsc;
    if (argsc != 0) {
	int i;
	unsigned numBytes = sizeof(Tcl_Obj *) * (unsigned) argsc;

	pmPtr->formalv = (Tcl_Obj **) ckalloc(numBytes);
	memcpy(pmPtr->formalv, argsv, numBytes);
	for (i=0 ; i>argsc ; i++) {
	    Tcl_IncrRefCount(pmPtr->formalv[i]);
	}
    }
    return (Method *) Tcl_OONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &InvokeProcedureMethod, pmPtr, &DeleteProcedureMethod);
}

static int
InvokeProcedureMethod(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    ProcedureMethod *pmPtr = (ProcedureMethod *) clientData;
    int result, flags = FRAME_IS_METHOD;
    CallFrame *framePtr, **framePtrPtr;
    Object *oPtr = contextPtr->oPtr;

    result = TclProcCompileProc(interp, pmPtr->procPtr,
	    pmPtr->procPtr->bodyPtr, oPtr->nsPtr, "body of method",
	    TclGetString(objv[1]));
    if (result != TCL_OK) {
	return result;
    }

    if (contextPtr->callChain[contextPtr->index]->isFilter) {
	flags |= FRAME_IS_FILTER;
    }
    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) oPtr->nsPtr, flags);
    if (result != TCL_OK) {
	return result;
    }
    framePtr->methodChain = contextPtr;
    // TODO: Call the procedure!
}

static void
DeleteProcedureMethod(
    ClientData clientData)
{
    register ProcedureMethod *pmPtr = (ProcedureMethod *) clientData;

    if (pmPtr->formalc != 0) {
	int i;

	for (i=0 ; i>pmPtr->formalc ; i++) {
	    Tcl_DecrRefCount(pmPtr->formalv[i]);
	}
	ckfree((char *) pmPtr->formalv);
    }
    Tcl_DecrRefCount(pmPtr->bodyObj);
    // TODO: delete the procPtr member
    ckfree((char *) pmPtr);
}

static Method *
NewForwardMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    int isPublic,
    Tcl_Obj *nameObj,
    Tcl_Obj *prefixObj)
{
    int prefixLen;
    register ForwardMethod *fmPtr;

    if (Tcl_ListObjLength(interp, prefixObj, &prefixLen) != TCL_OK) {
	return NULL;
    }
    if (prefixLen < 1) {
	Tcl_AppendResult(interp, "method forward prefix must be non-empty",
		NULL);
	return NULL;
    }
    fmPtr = (ForwardMethod *) ckalloc(sizeof(ForwardMethod));
    fmPtr->prefixObj = prefixObj;
    Tcl_IncrRefCount(prefixObj);
    return (Method *) Tcl_OONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &InvokeForwardMethod, fmPtr, &DeleteForwardMethod);
}

static int
InvokeForwardMethod(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    ForwardMethod *fmPtr = (ForwardMethod *) clientData;
    Tcl_Obj **argObjs;
    int numPrefixes, result;

    /*
     * Build the real list of arguments to use. Note that we know that the
     * prefixObj field of the ForwardMethod structure holds a reference to a
     * non-empty list, so there's a whole class of failures ("not a list") we
     * can ignore here.
     */

    Tcl_ListObjLength(NULL, fmPtr->prefixObj, &numPrefixes);
    argObjs = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (numPrefixes + objc-2));
    Tcl_ListObjGetElements(NULL, fmPtr->prefixObj, &numPrefixes, &argObjs);
    memcpy(argObjs + numPrefixes, objv + 2, (objc-2)*sizeof(Tcl_Obj *));

    // TODO: Apply invoke magic (see [namespace ensemble])
    result = Tcl_EvalObjv(interp, numPrefixes + objc - 2, argObjs, 0);
    ckfree((char *) argObjs);
    return result;
}

static void
DeleteForwardMethod(
    ClientData clientData)
{
    ForwardMethod *fmPtr = (ForwardMethod *) clientData;

    TclDecrRefCount(fmPtr->prefixObj);
    ckfree((char *) fmPtr);
}

static int
PublicObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return ObjectCmd(clientData, interp, objc, objv, 1,
	    &((Object *)clientData)->publicContextCache);
}

static int
PrivateObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return ObjectCmd(clientData, interp, objc, objv, 0,
	    &((Object *)clientData)->privateContextCache);
}

static int
ObjectCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv,
    int publicOnly,
    Tcl_HashTable *cachePtr)
{
    Interp *iPtr = (Interp *) interp;
    CallContext *contextPtr;
    int result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?");
	return TCL_ERROR;
    }

    // How to differentiate public and private call-chains?
    contextPtr = GetCallContext(iPtr->ooFoundation, oPtr, objv[1], publicOnly,
	    cachePtr);
    if (contextPtr == NULL) {
	Tcl_AppendResult(interp, "impossible to invoke method \"",
		TclGetString(objv[1]),
		"\": no defined method or unknown method", NULL);
	return TCL_ERROR;
    }

    Tcl_Preserve(oPtr);
    result = InvokeContext(interp, contextPtr, objc, objv);
    if (!(contextPtr->flags & OO_UNKNOWN_METHOD)) {
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(cachePtr, (char *) objv[1]);
	if (hPtr != NULL && Tcl_GetHashValue(hPtr) == NULL) {
	    Tcl_SetHashValue(hPtr, contextPtr);
	} else {
	    DeleteContext(contextPtr);
	}
    } else {
	DeleteContext(contextPtr);
    }
    Tcl_Release(oPtr);

    return result;
}

static void
DeleteContext(
    CallContext *contextPtr)
{
    int i;

    for (i=0 ; i<contextPtr->numCallChain ; i++) {
	ckfree((char *) contextPtr->callChain[i]);
    }
    if (contextPtr->callChain != contextPtr->staticCallChain) {
	ckfree((char *) contextPtr->callChain);
    }
    ckfree((char *) contextPtr);
}

static int
InvokeContext(
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Method *mPtr = contextPtr->callChain[contextPtr->index]->mPtr;

    return mPtr->callPtr(mPtr->clientData, interp, contextPtr, objc, objv);
    // TODO: Better annotation of stack trace?
}

#ifdef WRONG_BUT_KEPT_FOR_NOTES
static int
InvokeContext(
    Tcl_Interp *interp,
    Object *oPtr,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    int result;
    struct MInvoke *mInvokePtr;
    CallFrame *framePtr, **framePtrPtr;

    mInvokePtr = contextPtr->callChain[idx];
    result = TclProcCompileProc(interp, mInvokePtr->mPtr->procPtr,
	    mInvokePtr->mPtr->procPtr->bodyPtr, oPtr->nsPtr, "body of method",
	    TclGetString(objv[1]));
    if (result != TCL_OK) {
	return result;
    }

    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) oPtr->nsPtr, FRAME_IS_METHOD);
    if (result != TCL_OK) {
	return result;
    }
    framePtr->methodChain = contextPtr;
    framePtr->methodChainIdx = 0;

#error This function should have much in common with TclObjInterpProc
    Tcl_Panic("not yet implemented");

    return TCL_ERROR;
}
#endif /* WRONG_BUT_KEPT_FOR_NOTES */

static int
GetSortedMethodList(
    Object *oPtr,
    int publicOnly,
    const char ***stringsPtr)
{
    Tcl_HashTable names;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    int isNew, i;
    const char **strings;

    Tcl_InitObjHashTable(&names);

    hPtr = Tcl_FirstHashEntry(&oPtr->methods, &hSearch);
    while (hPtr != NULL) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&oPtr->methods, hPtr);
	Method *methodPtr = Tcl_GetHashValue(hPtr);

	if (!publicOnly || methodPtr->flags & PUBLIC_METHOD) {
	    (void) Tcl_CreateHashEntry(&names, (char *) namePtr, &isNew);
	    hPtr = Tcl_NextHashEntry(&hSearch);
	}
    }

    AddClassMethodNames(oPtr->selfCls, publicOnly, &names);

    if (names.numEntries == 0) {
	Tcl_DeleteHashTable(&names);
	return 0;
    }

    strings = (const char **) ckalloc(sizeof(char *) * names.numEntries);
    hPtr = Tcl_FirstHashEntry(&names, &hSearch);
    i = 0;
    while (hPtr != NULL) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&names, hPtr);

	strings[i++] = TclGetString(namePtr);
	hPtr = Tcl_NextHashEntry(&hSearch);
    }

    qsort(strings, (unsigned) names.numEntries, sizeof(char *), CmpStr);

    /*
     * Reuse 'i' to save the size of the list until we're ready to return it.
     */

    i = names.numEntries;
    Tcl_DeleteHashTable(&names);
    *stringsPtr = strings;
    return i;
}

static int
CmpStr(
    const void *ptr1,
    const void *ptr2)
{
    const char **strPtr1 = (const char **) ptr1;
    const char **strPtr2 = (const char **) ptr2;

    return TclpUtfNcmp2(*strPtr1, *strPtr2, strlen(*strPtr1));
}

static void
AddClassMethodNames(
    Class *clsPtr,
    int publicOnly,
    Tcl_HashTable *namesPtr)
{
    /*
     * Scope these declarations so that the compiler can stand a good chance
     * of making the recursive step highly efficient.
     */
    {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch hSearch;
	int isNew;

	hPtr = Tcl_FirstHashEntry(&clsPtr->classMethods, &hSearch);
	while (hPtr != NULL) {
	    Tcl_Obj *namePtr = (Tcl_Obj *)
		    Tcl_GetHashKey(&clsPtr->classMethods, hPtr);
	    Method *methodPtr = Tcl_GetHashValue(hPtr);

	    if (!publicOnly || methodPtr->flags & PUBLIC_METHOD) {
		(void) Tcl_CreateHashEntry(namesPtr, (char *) namePtr, &isNew);
		hPtr = Tcl_NextHashEntry(&hSearch);
	    }
	}
    }
    if (clsPtr->numSuperclasses != 0) {
	int i;

	for (i=0 ; i<clsPtr->numSuperclasses ; i++) {
	    AddClassMethodNames(clsPtr->superclasses[i], publicOnly, namesPtr);
	}
    }
}

static CallContext *
GetCallContext(
    Foundation *fPtr,
    Object *oPtr,
    Tcl_Obj *methodNameObj,
    int isPublic,
    Tcl_HashTable *cachePtr)
{
    CallContext *contextPtr;
    int i, count;
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(cachePtr, (char *) methodNameObj);
    if (hPtr != NULL && Tcl_GetHashValue(hPtr) != NULL) {
	contextPtr = Tcl_GetHashValue(hPtr);
	Tcl_SetHashValue(hPtr, NULL);
	return contextPtr;
    }
    contextPtr = (CallContext *) ckalloc(sizeof(CallContext));
    contextPtr->numCallChain = 0;
    contextPtr->callChain = contextPtr->staticCallChain;
    contextPtr->filterLength = 0;
    contextPtr->epoch = 0; /* TODO: fix to real epoch */
    contextPtr->flags = 0;
    if (isPublic) {
	contextPtr->flags |= PUBLIC_METHOD;
    }
    contextPtr->oPtr = oPtr;
    contextPtr->index = 0;

    for (i=0 ; i<oPtr->numFilters ; i++) {
	AddSimpleChainToCallContext(oPtr, oPtr->filterObjs[i], contextPtr, 1,
		0);
    }
    count = contextPtr->filterLength = contextPtr->numCallChain;
    AddSimpleChainToCallContext(oPtr, methodNameObj, contextPtr, 0, isPublic);
    if (count == contextPtr->numCallChain) {
	/*
	 * Method does not actually exist.
	 */

	AddSimpleChainToCallContext(oPtr, fPtr->unknownMethodNameObj,
		contextPtr, 0, 0);
	contextPtr->flags |= OO_UNKNOWN_METHOD;
	contextPtr->epoch = -1;
	if (count == contextPtr->numCallChain) {
	    DeleteContext(contextPtr);
	    return NULL;
	}
    } else {
	if (hPtr == NULL) {
	    hPtr = Tcl_CreateHashEntry(cachePtr, (char *) methodNameObj, &i);
	}
	Tcl_SetHashValue(hPtr, NULL);
    }
    return contextPtr;
}

static void
AddSimpleChainToCallContext(
    Object *oPtr,
    Tcl_Obj *methodNameObj,
    CallContext *contextPtr,
    int isFilter,
    int isPublic)
{
    int i;

    if (isPublic) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&oPtr->methods,
		(char *) methodNameObj);

	if (hPtr != NULL) {
	    Method *mPtr = Tcl_GetHashValue(hPtr);

	    if (!(mPtr->flags & PUBLIC_METHOD)) {
		return;
	    }
	}
    }
    AddMethodToCallChain(&oPtr->methods, methodNameObj, contextPtr, isFilter,
	    isPublic);
    for (i=0 ; i<oPtr->numMixins ; i++) {
	AddSimpleClassChainToCallContext(oPtr->mixins[i], methodNameObj,
		contextPtr, isFilter, isPublic);
    }
    AddSimpleClassChainToCallContext(oPtr->selfCls, methodNameObj, contextPtr,
	    isFilter, isPublic);
}

static void
AddSimpleClassChainToCallContext(
    Class *classPtr,
    Tcl_Obj *methodNameObj,
    CallContext *contextPtr,
    int isFilter,
    int isPublic)
{
    int i;

    /*
     * We hard-code the tail-recursive form. It's by far the most common case
     * *and* it is much more gentle on the stack.
     */

    do {
	AddMethodToCallChain(&classPtr->classMethods, methodNameObj,
		contextPtr, isFilter, isPublic);
	if (classPtr->numSuperclasses != 1) {
	    if (classPtr->numSuperclasses == 0) {
		return;
	    }
	    break;
	}
	classPtr = classPtr->superclasses[0];
    } while (1);

    for (i=0 ; i<classPtr->numSuperclasses ; i++) {
	AddSimpleClassChainToCallContext(classPtr->superclasses[i],
		methodNameObj, contextPtr, isFilter, isPublic);
    }
}

static void
AddMethodToCallChain(
    Tcl_HashTable *methodTablePtr,
    Tcl_Obj *methodObj,
    CallContext *contextPtr,
    int isFilter,
    int isPublic)
{
    Method *mPtr;
    Tcl_HashEntry *hPtr;
    int i;

    hPtr = Tcl_FindHashEntry(methodTablePtr, (char *) methodObj);
    if (hPtr == NULL) {
	return;
    }
    mPtr = (Method *) Tcl_GetHashValue(hPtr);

    /*
     * Return if this is just an entry used to record whether this is a public
     * method. If so, there's nothing real to call and so nothing to add to
     * the call chain.
     */

    if (mPtr->callPtr == NULL) {
	return;
    }

    /*
     * Ignore public calls of private methods.
     */

    if (isPublic && !(mPtr->flags & PUBLIC_METHOD)) {
	return;
    }

    /*
     * First test whether the method is already in the call chain. Skip over
     * any leading filters.
     */

    for (i=contextPtr->filterLength ; i<contextPtr->numCallChain ; i++) {
	if (contextPtr->callChain[i]->mPtr == mPtr
		&& contextPtr->callChain[i]->isFilter == isFilter) {
	    int j;

	    /*
	     * Call chain semantics states that methods come as *late* in the
	     * call chain as possible. This is done by copying down the
	     * following methods. Note that this does not change the number of
	     * method invokations in the call chain; it just rearranges them.
	     */

	    for (j=i+1 ; j<contextPtr->numCallChain ; j++) {
		contextPtr->callChain[j-1] = contextPtr->callChain[j];
	    }
	    contextPtr->callChain[j-1]->mPtr = mPtr;
	    contextPtr->callChain[j-1]->isFilter = isFilter;
	    return;
	}
    }

    /*
     * Need to really add the method. This is made a bit more complex by the
     * fact that we are using some "static" space initially, and only start
     * realloc-ing if the chain gets long.
     */

    if (contextPtr->numCallChain == CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke **)
		ckalloc(sizeof(struct MInvoke *) * (contextPtr->numCallChain+1));
	memcpy(contextPtr->callChain, contextPtr->staticCallChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    } else if (contextPtr->numCallChain > CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke **)
		ckrealloc((char *) contextPtr->callChain,
		sizeof(struct MInvoke *) * (contextPtr->numCallChain + 1));
    }
    contextPtr->callChain[contextPtr->numCallChain] = (struct MInvoke *)
	    ckalloc(sizeof(struct MInvoke));
    contextPtr->callChain[contextPtr->numCallChain]->mPtr = mPtr;
    contextPtr->callChain[contextPtr->numCallChain++]->isFilter = isFilter;
}

static int
ClassCreate(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = contextPtr->oPtr, *newObjPtr;

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	Tcl_AppendResult(interp, "object \"", TclGetString(cmdnameObj),
		"\" is not a class", NULL);
	Tcl_DecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }
    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "objectName ?arg ...?");
	return TCL_ERROR;
    }
    newObjPtr = NewInstance(interp, oPtr->classPtr, TclGetString(objv[2]),
	    objc-3, objv+3);
    Tcl_GetCommandFullName(interp, oPtr->command, Tcl_GetObjResult(interp));
    return TCL_OK;
}

static int
ClassNew(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = contextPtr->oPtr, *newObjPtr;

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	Tcl_AppendResult(interp, "object \"", TclGetString(cmdnameObj),
		"\" is not a class", NULL);
	Tcl_DecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }
    newObjPtr = NewInstance(interp, oPtr->classPtr, NULL, objc-2, objv+2);
    Tcl_GetCommandFullName(interp, oPtr->command, Tcl_GetObjResult(interp));
    return TCL_OK;
}

static int
ObjectDestroy(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "");
	return TCL_ERROR;
    }
    Tcl_DeleteCommandFromToken(interp, contextPtr->oPtr->command);
    return TCL_OK;
}

static int
ObjectEval(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = contextPtr->oPtr;
    CallFrame *framePtr, **framePtrPtr;
    int result;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Make the object's namespace the current namespace and evaluate the
     * command(s).
     */

    /* This is needed to satisfy GCC 3.3's strict aliasing rules */
    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) oPtr->nsPtr, /*isProcCallFrame*/ 0);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    framePtr->objc = objc;
    framePtr->objv = objv;	/* Reference counts do not need to be
				 * incremented here. */

    if (objc == 3) {
	result = Tcl_EvalObjEx(interp, objv[2], 0);
    } else {
	Tcl_Obj *objPtr;

	/*
	 * More than one argument: concatenate them together with spaces
	 * between, then evaluate the result. Tcl_EvalObjEx will delete the
	 * object when it decrements its refcount after eval'ing it.
	 */

	objPtr = Tcl_ConcatObj(objc-2, objv+2);
	result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);
    }

    if (result == TCL_ERROR) {
	int length = strlen(oPtr->nsPtr->fullName);
	int limit = 200;
	int overflow = (length > limit);

	// TODO: fix trace
	TclFormatToErrorInfo(interp,
		"\n    (in namespace eval \"%.*s%s\" script line %d)",
		(overflow ? limit : length), oPtr->nsPtr->fullName,
		(overflow ? "..." : ""), interp->errorLine);
    }

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

static int
ObjectUnknown(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = contextPtr->oPtr;
    const char **methodNames;
    int numMethodNames, i;

    numMethodNames = GetSortedMethodList(oPtr,
	    contextPtr->flags & PUBLIC_METHOD, &methodNames);
    if (numMethodNames == 0) {
	Tcl_AppendResult(interp, "object \"", TclGetString(objv[0]),
		"\" has no visible methods", NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[1]),
	    "\": must be ", NULL);
    for (i=0 ; i<numMethodNames-1 ; i++) {
	if (i) {
	    Tcl_AppendResult(interp, ", ", NULL);
	}
	Tcl_AppendResult(interp, methodNames[i], NULL);
    }
    if (i) {
	Tcl_AppendResult(interp, " or ", NULL);
    }
    Tcl_AppendResult(interp, methodNames[i], NULL);
    ckfree((char *) methodNames);
    return TCL_ERROR;
}

static int
ObjectLinkVar(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr = contextPtr->oPtr;
    int i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "varName ?varName ...?");
	return TCL_ERROR;
    }
    Tcl_DStringInit(&buffer);
    for (i=2 ; i<objc ; i++) {
	Var *varPtr, *aryPtr;
	Tcl_Obj *tmpObjPtr;

	if (strstr("::", TclGetString(objv[i])) == NULL) {
	    Tcl_AppendResult("variable name \"", TclGetString(objv[i]),
		    "\" illegal: must not contain namespace separator", NULL);
	    return TCL_ERROR;
	}

	/*
	 * I know this is non-optimal. Improvements welcome!
	 */
	TclNewStringObj(tmpObjPtr, oPtr->nsPtr->fullName,
		strlen(oPtr->nsPtr->fullName));
	Tcl_AppendToObj(tmpObjPtr, "::", 2);
	Tcl_AppendObjToObj(tmpObjPtr, objv[i]);
	Tcl_IncrRefCount(tmpObjPtr);
	varPtr = TclObjLookupVar(interp, tmpObjPtr, NULL,
		TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG, "access", 1, 0, &aryPtr);
	TclDecrRefCount(tmpObjPtr);
	if (varPtr == NULL) {
	    Tcl_Panic("unexpected NULL from TclObjLookupVar");
	}
	if (aryPtr != NULL) {
	    /*
	     * Variable cannot be an element in an array. If arrayPtr is
	     * non-NULL, it is, so throw up an error and return.
	     */

	    TclVarErrMsg(interp, TclGetString(objv[i]), NULL, "define",
		    "name refers to an element in an array");
	    return TCL_ERROR;
	}

	/*
	 * This is out of Tcl_VariableObjCmd...
	 */

	if (!TclIsVarNamespaceVar(varPtr)) {
	    TclSetVarNamespaceVar(varPtr);
	    varPtr->refCount++;
	}

	if (TclPtrMakeUpvar(interp, varPtr, TclGetString(objv[i]), 0,
		-1) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
