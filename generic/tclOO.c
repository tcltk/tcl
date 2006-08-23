/*
 * tclOO.c --
 *
 *	This file contains the object-system core (NB: not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2005-2006 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOO.c,v 1.1.2.20 2006/08/23 09:29:58 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

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

static const struct {
    const char *name;
    Tcl_ObjCmdProc *objProc;
    int flag;
} defineCmds[] = {
    {"constructor", TclOODefineConstructorObjCmd, 0},
    {"copy", TclOODefineCopyObjCmd, 0},
    {"destructor", TclOODefineDestructorObjCmd, 0},
    {"export", TclOODefineExportObjCmd, 0},
    {"self.export", TclOODefineExportObjCmd, 1},
    {"filter", TclOODefineFilterObjCmd, 0},
    {"self.filter", TclOODefineFilterObjCmd, 1},
    {"forward", TclOODefineForwardObjCmd, 0},
    {"self.forward", TclOODefineForwardObjCmd, 1},
    {"method", TclOODefineMethodObjCmd, 0},
    {"self.method", TclOODefineMethodObjCmd, 1},
    {"mixin", TclOODefineMixinObjCmd, 0},
    {"self.mixin", TclOODefineMixinObjCmd, 1},
    {"parameter", TclOODefineParameterObjCmd, 0},
    {"superclass", TclOODefineSuperclassObjCmd, 0},
    {"unexport", TclOODefineUnexportObjCmd, 0},
    {"self.unexport", TclOODefineUnexportObjCmd, 1},
    {"self.class", TclOODefineSelfClassObjCmd, 1},
    {NULL, NULL, 0}
};

#define ALLOC_CHUNK 8

//typedef struct Method {
//    Tcl_Obj *bodyObj;
//    Proc *procPtr;
//    int epoch;
//    int flags;
//    int formalc;
//    Tcl_Obj **formalv;
//} Method;
//
//typedef struct Object {
//    Namespace *nsPtr;		/* This object's tame namespace. */
//    Tcl_Command command;	/* Reference to this object's public
//				 * command. */
//    Tcl_Command myCommand;	/* Reference to this object's internal
//				 * command. */
//    struct Class *selfCls;	/* This object's class. */
//    Tcl_HashTable methods;	/* Tcl_Obj (method name) to Method*
//				 * mapping. */
//    int numMixins;		/* Number of classes mixed into this
//				 * object. */
//    struct Class **mixins;	/* References to classes mixed into this
//				 * object. */
//    int numFilters;
//    Tcl_Obj **filterObjs;
//    struct Class *classPtr;	/* All classes have this non-NULL; it points
//				 * to the class structure. Everything else has
//				 * this NULL. */
//    Tcl_Interp *interp;		/* The interpreter (for the PushObject and
//				 * PopObject callbacks. */
//} Object;
//
//struct Class {
//    struct Object *thisPtr;
//    int flags;
//    int numSuperclasses;
//    struct Class **superclasses;
//    int numSubclasses;
//    struct Class **subclasses;
//    int subclassesSize;
//    int numInstances;
//    struct Object **instances;
//    int instancesSize;
//    Tcl_HashTable classMethods;
//    struct Method *constructorPtr;
//    struct Method *destructorPtr;
//};

//typedef struct ObjectStack {
//    Object *oPtr;
//    struct ObjectStack *nextPtr;
//} ObjectStack;
//
//typedef struct Foundation {
//    struct Class *objectCls;
//    struct Class *classCls;
//    struct Class *definerCls;
//    struct Class *structCls;
//    Tcl_Namespace *helpersNs;
//    int epoch;
//    int nsCount;
//    Tcl_Obj *unknownMethodNameObj;
//    ObjectStack *objStack;	// should this be in stack frames?
//} Foundation;
//
//#define CALL_CHAIN_STATIC_SIZE 4
//
//struct MInvoke {
//    Method *mPtr;
//    int isFilter;
//};
//struct CallContext {
//    int epoch;
//    int flags;
//    int numCallChain;
//    struct MInvoke **callChain;
//    struct MInvoke *staticCallChain[CALL_CHAIN_STATIC_SIZE];
//    int filterLength;
//};

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
static void		AddMethodToCallChain(Method *mPtr,
			    CallContext *contextPtr, int isFilter, int flags);
static void		AddSimpleChainToCallContext(Object *oPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    int isFilter, int isPublic);
static void		AddSimpleClassChainToCallContext(Class *classPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    int isFilter, int isPublic);
static int		CmpStr(const void *ptr1, const void *ptr2);
static void		DeleteContext(CallContext *contextPtr);
static CallContext *	GetCallContext(Foundation *fPtr, Object *oPtr,
			    Tcl_Obj *methodNameObj, int flags,
			    Tcl_HashTable *cachePtr);
static int		InvokeContext(Tcl_Interp *interp,
			    CallContext *contextPtr, int objc,
			    Tcl_Obj *const *objv);
static int		ObjectCmd(Object *oPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv, int publicOnly,
			    Tcl_HashTable *cachePtr);
static Object *		NewInstance(Tcl_Interp *interp, Class *clsPtr,
			    char *name, int objc, Tcl_Obj *const *objv,
			    int skip);
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

static int		NextObjCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const *objv);

void
TclOOInit(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr;
    int i;
    Tcl_DString buffer;

    fPtr = iPtr->ooFoundation = (Foundation *) ckalloc(sizeof(Foundation));
    Tcl_CreateNamespace(interp, "::oo", fPtr, NULL);
    fPtr->defineNs = Tcl_CreateNamespace(interp, "::oo::define", NULL, NULL);
    fPtr->helpersNs = Tcl_CreateNamespace(interp, "::oo::Helpers", NULL,
	    NULL);

    Tcl_CreateObjCommand(interp, "::oo::Helpers::next", NextObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::define", TclOODefineObjCmd, NULL,
	    NULL);
    Tcl_DStringInit(&buffer);
    for (i=0 ; defineCmds[i].name ; i++) {
	Tcl_DStringAppend(&buffer, "::oo::define::", 14);
	Tcl_DStringAppend(&buffer, defineCmds[i].name, -1);
	Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
		defineCmds[i].objProc, (void *) defineCmds[i].flag, NULL);
	Tcl_DStringFree(&buffer);
    }

    //fPtr->objStack = NULL;
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
    while (1) {
	char objName[10 + TCL_INTEGER_SPACE];

	sprintf(objName, "::oo::Obj%d", ++fPtr->nsCount);
	oPtr->nsPtr = (Namespace *) Tcl_CreateNamespace(interp, objName,
		oPtr, ObjectNamespaceDeleted);
	if (oPtr->nsPtr != NULL) {
	    break;
	}

	/*
	 * Could not make that namespace, so we make another. But first we
	 * have to get rid of the error message from Tcl_CreateNamespace,
	 * since that's something that should not be exposed to the user.
	 */

	Tcl_ResetResult(interp);
    }
    TclSetNsPath(oPtr->nsPtr, 1, &fPtr->helpersNs);
    oPtr->selfCls = fPtr->objectCls;
    Tcl_InitObjHashTable(&oPtr->methods);
    Tcl_InitObjHashTable(&oPtr->publicContextCache);
    Tcl_InitObjHashTable(&oPtr->privateContextCache);
    oPtr->numFilters = 0;
    oPtr->filterObjs = NULL;
    oPtr->numMixins = 0;
    oPtr->mixins = NULL;
    oPtr->classPtr = NULL;
    oPtr->flags = 0;

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
	    TCL_TRACE_DELETE, ObjNameChangedTrace, oPtr);
    TclDecrRefCount(cmdnameObj);

    return oPtr;
}

static void
ObjNameChangedTrace(
    ClientData clientData,
    Tcl_Interp *interp,
    const char *oldName,
    const char *newName, /* always NULL */
    int flags)
{
    Object *oPtr = clientData;
    CallContext *contextPtr = GetCallContext(((Interp *)interp)->ooFoundation,
	    oPtr, NULL, DESTRUCTOR, NULL);

    Tcl_Preserve(oPtr);
    oPtr->flags |= OBJECT_DELETED;
    if (contextPtr != NULL) {
	int result;
	Tcl_InterpState state;

	contextPtr->flags |= DESTRUCTOR;
	contextPtr->skip = 0;
	state = Tcl_SaveInterpState(interp, TCL_OK);
	result = InvokeContext(interp, contextPtr, 0, NULL);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	(void) Tcl_RestoreInterpState(interp, state);
	DeleteContext(contextPtr);
    }

    Tcl_DeleteNamespace((Tcl_Namespace *) oPtr->nsPtr);
    Tcl_Release(oPtr);

    /*
     * What else to do to delete an object?
     */
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

    Tcl_EventuallyFree(oPtr, TCL_DYNAMIC);
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
    Tcl_Obj *const *objv,
    int skip)
{
    Object *oPtr = AllocObject(interp, NULL);
    Class *classPtr;
    CallContext *contextPtr;

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
	    TclDecrRefCount(cmdnameObj);
	    Tcl_DeleteCommandFromToken(interp, oPtr->command);
	    return NULL;
	}
	TclDecrRefCount(cmdnameObj);
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

    contextPtr = GetCallContext(((Interp *)interp)->ooFoundation, oPtr, NULL,
	    CONSTRUCTOR, NULL);
    if (contextPtr != NULL) {
	int result;

	Tcl_Preserve(oPtr);
	contextPtr->flags |= CONSTRUCTOR;
	contextPtr->skip = skip;
	result = InvokeContext(interp, contextPtr, objc, objv);
	DeleteContext(contextPtr);
	Tcl_Release(oPtr);
	if (result != TCL_OK) {
	    Tcl_DeleteCommandFromToken(interp, oPtr->command);
	    return NULL;
	}
	Tcl_ResetResult(interp);
    }

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
    Tcl_Obj *nameObj, /* May be NULL; if so, up to caller to manage storage. */
    int isPublic,
    Tcl_OOMethodCallProc callProc,
    ClientData clientData,
    Tcl_OOMethodDeleteProc deleteProc)
{
    register Object *oPtr = (Object *) object;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (nameObj == NULL) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	goto populate;
    }
    hPtr = Tcl_CreateHashEntry(&oPtr->methods, (char *) nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	Tcl_SetHashValue(hPtr, mPtr);
	//TODO: Put a reference to the name in the Method struct
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->deletePtr != NULL) {
	    mPtr->deletePtr(mPtr->clientData);
	}
    }

  populate:
    mPtr->callPtr = callProc;
    mPtr->clientData = clientData;
    mPtr->deletePtr = deleteProc;
    mPtr->epoch = ++((Interp *) interp)->ooFoundation->epoch;
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
    Tcl_Obj *nameObj, /* May be NULL; if so, up to caller to manage storage. */
    int isPublic,
    Tcl_OOMethodCallProc callProc,
    ClientData clientData,
    Tcl_OOMethodDeleteProc deleteProc)
{
    register Class *clsPtr = (Class *) cls;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (nameObj == NULL) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	goto populate;
    }
    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, (char *)nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	Tcl_SetHashValue(hPtr, mPtr);
	//TODO: Put a reference to the name in the Method struct
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->deletePtr != NULL) {
	    mPtr->deletePtr(mPtr->clientData);
	}
    }

  populate:
    mPtr->callPtr = callProc;
    mPtr->clientData = clientData;
    mPtr->deletePtr = deleteProc;
    mPtr->epoch = ++((Interp *) interp)->ooFoundation->epoch;
    mPtr->flags = 0;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }

    return (Tcl_Method) mPtr;
}

Method *
TclNewProcMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    int isPublic,
    Tcl_Obj *nameObj,		/* Must not be NULL. */
    Tcl_Obj *argsObj,		/* Must not be NULL. */
    Tcl_Obj *bodyObj)		/* Must not be NULL. */
{
    int argsc;
    Tcl_Obj **argsv;
    register ProcedureMethod *pmPtr;
    const char *procName;

    if (Tcl_ListObjGetElements(interp, argsObj, &argsc, &argsv) != TCL_OK) {
	return NULL;
    }
    pmPtr = (ProcedureMethod *) ckalloc(sizeof(ProcedureMethod));
    procName = TclGetString(nameObj);
    if (TclCreateProc(interp, NULL, procName, argsObj, bodyObj,
	    &pmPtr->procPtr) != TCL_OK) {
	ckfree((char *) pmPtr);
	return NULL;
    }
    return (Method *) Tcl_OONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &InvokeProcedureMethod, pmPtr, &DeleteProcedureMethod);
}

Method *
TclNewProcClassMethod(
    Tcl_Interp *interp,
    Class *cPtr,
    int isPublic,
    Tcl_Obj *nameObj, /* May be NULL; if so, up to caller to manage storage. */
    Tcl_Obj *argsObj, /* May be NULL; if so, equiv to empty list. */
    Tcl_Obj *bodyObj)
{
    int argsLen;		/* -1 => delete argsObj before exit */
    register ProcedureMethod *pmPtr;
    const char *procName;

    if (argsObj == NULL) {
	argsLen = -1;
	TclNewObj(argsObj);
	Tcl_IncrRefCount(argsObj);
	procName = "<destructor>";
    } else if (Tcl_ListObjLength(interp, argsObj, &argsLen) != TCL_OK) {
	return NULL;
    } else {
	procName = (nameObj==NULL ? "<constructor>" : TclGetString(nameObj));
    }
    pmPtr = (ProcedureMethod *) ckalloc(sizeof(ProcedureMethod));
    if (TclCreateProc(interp, NULL, procName, argsObj, bodyObj,
	    &pmPtr->procPtr) != TCL_OK) {
	if (argsLen == -1) {
	    TclDecrRefCount(argsObj);
	}
	ckfree((char *) pmPtr);
	return NULL;
    }
    if (argsLen == -1) {
	TclDecrRefCount(argsObj);
    }
    return (Method *) Tcl_OONewClassMethod(interp, (Tcl_Class) cPtr, nameObj,
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
    Command cmd;
    const char *namePtr;
    Tcl_Obj *nameObj;

    cmd.nsPtr = oPtr->nsPtr;
    pmPtr->procPtr->cmdPtr = &cmd;
    if (contextPtr->flags & CONSTRUCTOR) {
	namePtr = "<constructor>";
	flags |= FRAME_IS_CONSTRUCTOR;
	nameObj = Tcl_NewStringObj("<constructor>", -1);
	Tcl_IncrRefCount(nameObj);
    } else if (contextPtr->flags & DESTRUCTOR) {
	namePtr = "<destructor>";
	flags |= FRAME_IS_DESTRUCTOR;
	nameObj = Tcl_NewStringObj("<destructor>", -1);
	Tcl_IncrRefCount(nameObj);
    } else {
	nameObj = objv[contextPtr->skip-1];
	namePtr = TclGetString(nameObj);
    }
    result = TclProcCompileProc(interp, pmPtr->procPtr,
	    pmPtr->procPtr->bodyPtr, oPtr->nsPtr, "body of method", namePtr);
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
    framePtr->ooContextPtr = contextPtr;
    framePtr->objc = objc;
    framePtr->objv = objv;	/* ref counts for args are incremented below */
    framePtr->procPtr = pmPtr->procPtr;

    result = TclObjInterpProcCore(interp, framePtr, nameObj, contextPtr->skip);
    if (contextPtr->flags & (CONSTRUCTOR | DESTRUCTOR)) {
	TclDecrRefCount(nameObj);
    }
    return result;
}

static void
DeleteProcedureMethod(
    ClientData clientData)
{
    register ProcedureMethod *pmPtr = (ProcedureMethod *) clientData;

    //TclProcDeleteProc(pmPtr->procPtr);
    TclProcCleanupProc(pmPtr->procPtr);
    ckfree((char *) pmPtr);
}

/* To be called from Tcl_EventuallyFree */
static void
DeleteMethodStruct(
    char *buffer)
{
    Method *mPtr = (Method *) buffer;

    if (mPtr->deletePtr != NULL) {
	mPtr->deletePtr(mPtr->clientData);
    }

    ckfree(buffer);
}

void
TclDeleteMethod(
    Method *mPtr)
{
    if (mPtr != NULL) {
	Tcl_EventuallyFree(mPtr, DeleteMethodStruct);
    }
}

Method *
TclNewForwardMethod(
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
//    mPtr->epoch = ((Interp *) interp)->ooFoundation->epoch;
//    mPtr->bodyObj = bodyObj;
//    Tcl_IncrRefCount(bodyObj);
//    mPtr->flags = 0;
//    return mPtr;
//
//    if (TclCreateProc(interp, oPtr->nsPtr, TclGetString(nameObj), argsObj,
//	    bodyObj, &mPtr->procPtr) != TCL_OK) {
//	Tcl_AddErrorInfo(interp, "\n    (creating method \"");
//	Tcl_AddErrorInfo(interp, TclGetString(nameObj));
//	Tcl_AddErrorInfo(interp, "\")");
//	return NULL;
//    }
//    procPtr->isMethod = 1;
    fmPtr = (ForwardMethod *) ckalloc(sizeof(ForwardMethod));
    fmPtr->prefixObj = prefixObj;
    Tcl_IncrRefCount(prefixObj);
    return (Method *) Tcl_OONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &InvokeForwardMethod, fmPtr, &DeleteForwardMethod);
}

Method *
TclNewForwardClassMethod(
    Tcl_Interp *interp,
    Class *cPtr,
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
    return (Method *) Tcl_OONewClassMethod(interp, (Tcl_Class) cPtr, nameObj,
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
    Tcl_Obj **argObjs, **prefixObjs;
    int numPrefixes, result, skip = contextPtr->skip;

    /*
     * Build the real list of arguments to use. Note that we know that the
     * prefixObj field of the ForwardMethod structure holds a reference to a
     * non-empty list, so there's a whole class of failures ("not a list") we
     * can ignore here.
     */

    Tcl_ListObjGetElements(NULL, fmPtr->prefixObj, &numPrefixes, &prefixObjs);
    argObjs = (Tcl_Obj**) ckalloc(sizeof(Tcl_Obj*) * (numPrefixes+objc-skip));
    memcpy(argObjs, prefixObjs, numPrefixes * sizeof(Tcl_Obj *));
    memcpy(argObjs + numPrefixes, objv+skip, (objc-skip) * sizeof(Tcl_Obj *));

    // TODO: Apply invoke magic (see [namespace ensemble])
    result = Tcl_EvalObjv(interp, numPrefixes + objc - skip, argObjs, 0);
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
    contextPtr = GetCallContext(iPtr->ooFoundation, oPtr, objv[1],
	    (publicOnly ? PUBLIC_METHOD : 0), cachePtr);
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
    int result, isFirst = (contextPtr->index == 0);

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Preserve(contextPtr->callChain[i]->mPtr);
	}
    }

    result = mPtr->callPtr(mPtr->clientData, interp, contextPtr, objc, objv);

    // TODO: Better annotation of stack trace?

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Release(contextPtr->callChain[i]->mPtr);
	}
    }
    return result;
}

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
    for (; hPtr != NULL; hPtr = Tcl_NextHashEntry(&hSearch)) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&oPtr->methods, hPtr);
	Method *methodPtr = Tcl_GetHashValue(hPtr);

	hPtr = Tcl_CreateHashEntry(&names, (char *) namePtr, &isNew);
	if (isNew) {
	    int isWanted = (!publicOnly || methodPtr->flags & PUBLIC_METHOD);

	    Tcl_SetHashValue(hPtr, (int) isWanted);
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

	if (!publicOnly || (int) Tcl_GetHashValue(hPtr)) {
	    strings[i++] = TclGetString(namePtr);
	}
	hPtr = Tcl_NextHashEntry(&hSearch);
    }

    /*
     * Note that 'i' may well be less than names.numEntries when we are
     * dealing with public method names.
     */

    qsort(strings, (unsigned) i, sizeof(char *), CmpStr);

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
     * of making the recursive step highly efficient. We also hand-implement
     * the tail-recursive case using a while loop; C compilers typically
     * cannot do tail-recursion optimization usefully.
     */

    while (1) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch hSearch;
	int isNew;

	hPtr = Tcl_FirstHashEntry(&clsPtr->classMethods, &hSearch);
	for (; hPtr != NULL; hPtr = Tcl_NextHashEntry(&hSearch)) {
	    Tcl_Obj *namePtr = (Tcl_Obj *)
		    Tcl_GetHashKey(&clsPtr->classMethods, hPtr);
	    Method *methodPtr = Tcl_GetHashValue(hPtr);

	    hPtr = Tcl_CreateHashEntry(namesPtr, (char *) namePtr, &isNew);
	    if (isNew) {
		int isWanted = (!publicOnly
			|| methodPtr->flags & PUBLIC_METHOD);

		Tcl_SetHashValue(hPtr, (int) isWanted);
	    }
	}

	if (clsPtr->numSuperclasses != 1) {
	    break;
	}
	clsPtr = clsPtr->superclasses[0];
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
    int flags,
    Tcl_HashTable *cachePtr)
{
    CallContext *contextPtr;
    int i, count;
    Tcl_HashEntry *hPtr;

    if (flags & (CONSTRUCTOR | DESTRUCTOR)) {
	hPtr = NULL;
    } else {
	hPtr = Tcl_FindHashEntry(cachePtr, (char *) methodNameObj);
	if (hPtr != NULL && Tcl_GetHashValue(hPtr) != NULL) {
	    contextPtr = Tcl_GetHashValue(hPtr);
	    Tcl_SetHashValue(hPtr, NULL);
	    if (contextPtr->epoch == fPtr->epoch) {
		return contextPtr;
	    }
	    DeleteContext(contextPtr);
	}
    }
    contextPtr = (CallContext *) ckalloc(sizeof(CallContext));
    contextPtr->numCallChain = 0;
    contextPtr->callChain = contextPtr->staticCallChain;
    contextPtr->filterLength = 0;
    contextPtr->epoch = fPtr->epoch;
    contextPtr->flags = 0;
    contextPtr->skip = 2;
    if (flags & (PUBLIC_METHOD | CONSTRUCTOR | DESTRUCTOR)) {
	contextPtr->flags |=
		flags & (PUBLIC_METHOD | CONSTRUCTOR | DESTRUCTOR);
    }
    contextPtr->oPtr = oPtr;
    contextPtr->index = 0;

    if (!(flags & (CONSTRUCTOR | DESTRUCTOR))) {
	for (i=0 ; i<oPtr->numFilters ; i++) {
	    AddSimpleChainToCallContext(oPtr, oPtr->filterObjs[i], contextPtr,
		     1, 0);
	}
    }
    count = contextPtr->filterLength = contextPtr->numCallChain;
    AddSimpleChainToCallContext(oPtr, methodNameObj, contextPtr, 0, flags);
    if (count == contextPtr->numCallChain) {
	/*
	 * Method does not actually exist. If we're dealing with constructors
	 * or destructors, this isn't a problem.
	 */

	if (flags & (CONSTRUCTOR | DESTRUCTOR)) {
	    DeleteContext(contextPtr);
	    return NULL;
	}
	AddSimpleChainToCallContext(oPtr, fPtr->unknownMethodNameObj,
		contextPtr, 0, 0);
	contextPtr->flags |= OO_UNKNOWN_METHOD;
	contextPtr->epoch = -1;
	if (count == contextPtr->numCallChain) {
	    DeleteContext(contextPtr);
	    return NULL;
	}
    } else if (!(flags & (CONSTRUCTOR | DESTRUCTOR))) {
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
    int flags)
{
    int i;

    if (flags & PUBLIC_METHOD) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&oPtr->methods,
		(char *) methodNameObj);

	if (hPtr != NULL) {
	    Method *mPtr = Tcl_GetHashValue(hPtr);

	    if (!(mPtr->flags & PUBLIC_METHOD)) {
		return;
	    } else {
		flags &= ~PUBLIC_METHOD;
	    }
	}
    }
    if (!(flags & (CONSTRUCTOR | DESTRUCTOR))) {
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *) methodNameObj);
	if (hPtr != NULL) {
	    AddMethodToCallChain(Tcl_GetHashValue(hPtr), contextPtr, isFilter,
		    flags);
	}
	for (i=0 ; i<oPtr->numMixins ; i++) {
	    AddSimpleClassChainToCallContext(oPtr->mixins[i], methodNameObj,
		    contextPtr, isFilter, flags);
	}
    }
    AddSimpleClassChainToCallContext(oPtr->selfCls, methodNameObj, contextPtr,
	    isFilter, flags);
}

static void
AddSimpleClassChainToCallContext(
    Class *classPtr,
    Tcl_Obj *methodNameObj,
    CallContext *contextPtr,
    int isFilter,
    int flags)
{
    int i;

    /*
     * We hard-code the tail-recursive form. It's by far the most common case
     * *and* it is much more gentle on the stack.
     */

    do {
	register int numSuper;

	if (flags & CONSTRUCTOR) {
	    AddMethodToCallChain(classPtr->constructorPtr, contextPtr,
		    isFilter, flags);
	} else if (flags & DESTRUCTOR) {
	    AddMethodToCallChain(classPtr->destructorPtr, contextPtr,
		    isFilter, flags);
	} else {
	    Tcl_HashEntry *hPtr;

	    hPtr = Tcl_FindHashEntry(&classPtr->classMethods,
		    (char *) methodNameObj);
	    if (hPtr != NULL) {
		AddMethodToCallChain(Tcl_GetHashValue(hPtr), contextPtr,
			isFilter, flags);
	    }
	}
	numSuper = classPtr->numSuperclasses;
	if (numSuper != 1) {
	    if (numSuper == 0) {
		return;
	    }
	    break;
	}
	classPtr = classPtr->superclasses[0];
    } while (1);

    for (i=0 ; i<classPtr->numSuperclasses ; i++) {
	AddSimpleClassChainToCallContext(classPtr->superclasses[i],
		methodNameObj, contextPtr, isFilter, flags);
    }
}

static void
AddMethodToCallChain(
    Method *mPtr,
    CallContext *contextPtr,
    int isFilter,
    int flags)
{
    int i;

    /*
     * Return if this is just an entry used to record whether this is a public
     * method. If so, there's nothing real to call and so nothing to add to
     * the call chain.
     */

    if (mPtr == NULL || mPtr->callPtr == NULL) {
	return;
    }

    /*
     * Ignore public calls of private methods.
     */

    if ((flags & PUBLIC_METHOD) && !(mPtr->flags & PUBLIC_METHOD)) {
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
		ckalloc(sizeof(struct MInvoke *)*(contextPtr->numCallChain+1));
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
	TclDecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }
    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "objectName ?arg ...?");
	return TCL_ERROR;
    }
    newObjPtr = NewInstance(interp, oPtr->classPtr, TclGetString(objv[2]),
	    objc, objv, 3);
    if (newObjPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_GetCommandFullName(interp, newObjPtr->command,
	    Tcl_GetObjResult(interp));
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
	TclDecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }
    newObjPtr = NewInstance(interp, oPtr->classPtr, NULL, objc, objv, 2);
    if (newObjPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_GetCommandFullName(interp, newObjPtr->command,
	    Tcl_GetObjResult(interp));
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
		"\n    (in %s eval \"%.*s%s\" script line %d)",
		TclGetString(objv[0]), (overflow ? limit : length),
		oPtr->nsPtr->fullName, (overflow ? "..." : ""),
		interp->errorLine);
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
    Object *oPtr = contextPtr->oPtr;
    int i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "varName ?varName ...?");
	return TCL_ERROR;
    }
    for (i=2 ; i<objc ; i++) {
	Var *varPtr, *aryPtr;
	Tcl_Obj *tmpObjPtr;

	if (strstr("::", TclGetString(objv[i])) == NULL) {
	    Tcl_AppendResult(interp, "variable name \"", TclGetString(objv[i]),
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

static int
NextObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;
    CallContext *contextPtr;
    int index, result, skip;

    /*
     * Start with sanity checks on the calling context and the method context.
     */

    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	Tcl_AppendResult(interp, TclGetString(objv[0]),
		" may only be called from inside a method", NULL);
	return TCL_ERROR;
    }

    contextPtr = framePtr->ooContextPtr;

    index = contextPtr->index;
    skip = contextPtr->skip;
    if (index+1 >= contextPtr->numCallChain) {
	Tcl_AppendResult(interp, "no superclass method implementation", NULL);
	return TCL_ERROR;
    }

    /*
     * Advance to the next method implementation in the chain in the method
     * call context while we process the body. However, need to adjust the
     * argument-skip control because we're guaranteed to have a single prefix
     * arg (i.e., 'next') and not the variable amount that can happen because
     * method invokations (i.e., '$obj meth' and 'my meth'), constructors
     * (i.e., '$cls new' and '$cls create obj') and destructors (no args at
     * all) come through the same code. From here on, the skip is always 1.
     */

    contextPtr->index = index+1;
    contextPtr->skip = 1;

    /*
     * Invoke the (advanced) method call context. This might need some
     * temporary space for building the array of arguments (it only doesn't in
     * the no-arg case).
     */

    result = InvokeContext(interp, contextPtr, objc, objv);

    /*
     * Restore the call chain context index as we've finished the inner invoke
     * and want to operate in the outer context again.
     */

    contextPtr->index = index;
    contextPtr->skip = skip;

    /*
     * If an error happened, add information about this to the trace.
     */

    if (result == TCL_ERROR) {
	// TODO: Better error info for filters
	TclFormatToErrorInfo(interp,
		"\n    (superclass implementation of %s method)",
		TclGetString(framePtr->objv[1]));
    }

    return result;
}

Object *
TclGetObjectFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Command *cmdPtr = (Command *) Tcl_GetCommandFromObj(interp, objPtr);

    if (cmdPtr == NULL || cmdPtr->objProc != PublicObjectCmd) {
	Tcl_AppendResult(interp, TclGetString(objPtr),
		" does not refer to an object", NULL);
	return NULL;
    }
    return cmdPtr->objClientData;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
