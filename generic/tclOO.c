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
 * RCS: @(#) $Id: tclOO.c,v 1.1.2.40 2006/09/02 21:04:09 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"
#include <assert.h>

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

#define DEFINITE_PRIVATE 0x100000
#define DEFINITE_PUBLIC  0x200000
#define KNOWN_STATE	 (DEFINITE_PRIVATE | DEFINITE_PUBLIC)

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
static void		KillFoundation(ClientData clientData,
			    Tcl_Interp *interp);
static int		ObjectCmd(Object *oPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv, int publicOnly,
			    Tcl_HashTable *cachePtr);
static void		ObjectNamespaceDeleted(ClientData clientData);
static void		ObjNameChangedTrace(ClientData clientData,
			    Tcl_Interp *interp, const char *oldName,
			    const char *newName, int flags);
static void		ReleaseClassContents(Tcl_Interp *interp, Object *oPtr);

static int		PublicObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);

static int		SimpleInvoke(ClientData clientData,
			    Tcl_Interp *interp, CallContext *oPtr,
			    int objc, Tcl_Obj *const *objv);
static int		SimpleClone(ClientData clientData,
			    ClientData *newClientData);
static int		InvokeProcedureMethod(ClientData clientData,
			    Tcl_Interp *interp, CallContext *oPtr,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteProcedureMethod(ClientData clientData);
static int		CloneProcedureMethod(ClientData clientData,
			    ClientData *newClientData);
static int		InvokeForwardMethod(ClientData clientData,
			    Tcl_Interp *interp, CallContext *oPtr,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteForwardMethod(ClientData clientData);
static int		CloneForwardMethod(ClientData clientData,
			    ClientData *newClientData);

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
static int		SelfObjCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const *objv);

static const Tcl_OOMethodType procMethodType = {
    "procedural method",
    InvokeProcedureMethod, DeleteProcedureMethod, CloneProcedureMethod
};
static const Tcl_OOMethodType fwdMethodType = {
    "forward",
    InvokeForwardMethod, DeleteForwardMethod, CloneForwardMethod
};
static const Tcl_OOMethodType coreMethodType = {
    "core method",
    SimpleInvoke, NULL, SimpleClone
};

int
TclOOInit(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr;
    int i;
    Tcl_DString buffer;

    /*
     * Construct the foundation of the object system. This is a structure
     * holding references to the magical bits that need to be known about in
     * other places.
     */

    fPtr = iPtr->ooFoundation = (Foundation *) ckalloc(sizeof(Foundation));
    memset(fPtr, 0, sizeof(Foundation));
    fPtr->ooNs = Tcl_CreateNamespace(interp, "::oo", fPtr, NULL);
    fPtr->defineNs = Tcl_CreateNamespace(interp, "::oo::define", NULL, NULL);
    fPtr->helpersNs = Tcl_CreateNamespace(interp, "::oo::Helpers", NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::Helpers::next", NextObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::Helpers::self", SelfObjCmd, NULL,
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
    fPtr->epoch = 0;
    fPtr->nsCount = 0;
    fPtr->unknownMethodNameObj = Tcl_NewStringObj("unknown", -1);
    Tcl_IncrRefCount(fPtr->unknownMethodNameObj);

    Tcl_CallWhenDeleted(interp, KillFoundation, fPtr);

    /*
     * Create the objects at the core of the object system. These need to be
     * spliced manually.
     */

    fPtr->objectCls = AllocClass(interp, AllocObject(interp, "::oo::object"));
    fPtr->classCls = AllocClass(interp, AllocObject(interp, "::oo::class"));
    fPtr->objectCls->thisPtr->selfCls = fPtr->classCls;
    fPtr->objectCls->thisPtr->flags |= ROOT_OBJECT;
    fPtr->objectCls->superclasses.num = 0;
    ckfree((char *) fPtr->objectCls->superclasses.list);
    fPtr->objectCls->superclasses.list = NULL;
    fPtr->classCls->thisPtr->selfCls = fPtr->classCls;
    TclOOAddToInstances(fPtr->objectCls->thisPtr, fPtr->classCls);
    TclOOAddToInstances(fPtr->classCls->thisPtr, fPtr->classCls);

    /*
     * Basic method declarations for the core classes.
     */

    DeclareClassMethod(interp, fPtr->objectCls, "destroy", 1, ObjectDestroy);
    DeclareClassMethod(interp, fPtr->objectCls, "eval", 0, ObjectEval);
    DeclareClassMethod(interp, fPtr->objectCls, "unknown", 0, ObjectUnknown);
    DeclareClassMethod(interp, fPtr->objectCls, "variable", 0, ObjectLinkVar);
    DeclareClassMethod(interp, fPtr->classCls, "create", 1, ClassCreate);
    DeclareClassMethod(interp, fPtr->classCls, "new", 1, ClassNew);

    /*
     * Finish setting up the class of classes.
     */

    {
	Tcl_Obj *namePtr, *argsPtr, *bodyPtr;

	/*
	 * Mark the 'new' method in oo::class as private; classes, unlike
	 * general objects, must have explicit names.
	 */

	namePtr = Tcl_NewStringObj("new", -1);
	TclOONewMethod(interp, (Tcl_Object) fPtr->classCls->thisPtr, namePtr,
		0 /* ==private */, NULL, NULL);

	argsPtr = Tcl_NewStringObj("{configuration {}}", -1);
	bodyPtr = Tcl_NewStringObj(
		"if {[catch {define [self] $configuration} msg opt]} {\n"
		"set eilist [split [dict get $opt -errorinfo] \\n]\n"
		"dict set opt -errorinfo [join [lrange $eilist 0 end-2] \\n]\n"
		"dict set opt -errorline 0xdeadbeef\n"
		"}\n"
		"return -options $opt $msg", -1);
	fPtr->classCls->constructorPtr = TclNewProcClassMethod(interp,
		fPtr->classCls, 0, NULL, argsPtr, bodyPtr);
    }

    /*
     * Build the definer metaclass, which is a kind of class with many
     * convenience methods.
     */

    fPtr->definerCls = AllocClass(interp,
	    AllocObject(interp, "::oo::definer"));
    fPtr->definerCls->superclasses.list[0] = fPtr->classCls;
    TclOORemoveFromSubclasses(fPtr->definerCls, fPtr->objectCls);
    TclOOAddToSubclasses(fPtr->definerCls, fPtr->classCls);
    {
	Tcl_Obj *argsPtr, *bodyPtr;

	argsPtr = Tcl_NewStringObj("{configuration {}}", -1);
	bodyPtr = Tcl_NewStringObj(
		"set c [self];set d [namespace origin define];foreach cmd {"
		"constructor destructor export forward "
		"method parameter superclass unexport "
		"} {define $c self.forward $cmd $d $c $cmd};"
		"next $configuration", -1);
	fPtr->definerCls->constructorPtr = TclNewProcClassMethod(interp,
		fPtr->definerCls, 0, NULL, argsPtr, bodyPtr);
    }

    /*
     * TODO: set up 'struct' less magically by evaluating a Tcl script.
     */

    fPtr->structCls = AllocClass(interp, AllocObject(interp, "::oo::struct"));

    return TCL_OK;
}

static void
KillFoundation(
    ClientData clientData,
    Tcl_Interp *interp)
{
    Foundation *fPtr = clientData;

    TclDecrRefCount(fPtr->unknownMethodNameObj);
    ckfree((char *) fPtr);
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
    memset(oPtr, 0, sizeof(Object));
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
    oPtr->filters.num = 0;
    oPtr->filters.list = NULL;
    oPtr->mixins.num = 0;
    oPtr->mixins.list = NULL;
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
    Interp *iPtr = (Interp *) interp;
    Object *oPtr = clientData;
    Class *cPtr = NULL;

    Tcl_Preserve(oPtr);
    oPtr->flags |= OBJECT_DELETED;
    if (!Tcl_InterpDeleted(interp)) {
	CallContext *contextPtr = GetCallContext(iPtr->ooFoundation, oPtr,
		NULL, DESTRUCTOR, NULL);

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
    }

    if (oPtr->classPtr != NULL) {
	ReleaseClassContents(interp, oPtr);
    }

    Tcl_DeleteNamespace((Tcl_Namespace *) oPtr->nsPtr);
    if (cPtr) {
	Tcl_Release(cPtr);
    }
    Tcl_Release(oPtr);

    /*
     * What else to do to delete an object?
     */
}

static void
ReleaseClassContents(
    Tcl_Interp *interp,
    Object *oPtr)
{
    int i, n;
    Class *cPtr, **subs;
    Object **insts;

    cPtr = oPtr->classPtr;
    Tcl_Preserve(cPtr);

    /*
     * Must empty list now so that things happen in the correct order.
     */

    subs = cPtr->subclasses.list;
    n = cPtr->subclasses.num;
    cPtr->subclasses.list = NULL;
    cPtr->subclasses.num = 0;
    cPtr->subclasses.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(subs[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(subs[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, subs[i]->thisPtr->command);
	}
	Tcl_Release(subs[i]);
    }
    ckfree((char *) subs);

    insts = cPtr->instances.list;
    n = cPtr->instances.num;
    cPtr->instances.list = NULL;
    cPtr->instances.num = 0;
    cPtr->instances.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(insts[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(insts[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, insts[i]->command);
	}
	Tcl_Release(insts[i]);
    }
    ckfree((char *) insts);
}

static void
ObjectNamespaceDeleted(
    ClientData clientData)
{
    Object *oPtr = clientData;
    Class *cPtr;
    int i;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    /*
     * Instruct everyone to no longer use any allocated fields of the object.
     */

    if (!(oPtr->flags & OBJECT_DELETED)) {
	Tcl_Preserve(oPtr);
	if (oPtr->classPtr != NULL) {
	    ReleaseClassContents(NULL, oPtr);
	}
    }
    oPtr->flags |= OBJECT_DELETED;

    /*
     * Splice the object out of its context. After this, we must *not* call
     * methods on the object.
     */

    if (!(oPtr->flags & ROOT_OBJECT)) {
	TclOORemoveFromInstances(oPtr, oPtr->selfCls);
    }
    for (i=0 ; i<oPtr->mixins.num ; i++) {
	TclOORemoveFromInstances(oPtr, oPtr->mixins.list[i]);
    }
    if (i) {
	ckfree((char *)oPtr->mixins.list);
    }
    for (i=0 ; i<oPtr->filters.num ; i++) {
	TclDecrRefCount(oPtr->filters.list[i]);
    }
    if (i) {
	ckfree((char *)oPtr->filters.list);
    }
    for (hPtr = Tcl_FirstHashEntry(&oPtr->methods, &search);
	    hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	TclDeleteMethod(Tcl_GetHashValue(hPtr));
    }
    Tcl_DeleteHashTable(&oPtr->methods);
    for (hPtr = Tcl_FirstHashEntry(&oPtr->publicContextCache, &search);
	    hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	CallContext *contextPtr = Tcl_GetHashValue(hPtr);

	if (contextPtr) {
	    DeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->publicContextCache);
    for (hPtr = Tcl_FirstHashEntry(&oPtr->privateContextCache, &search);
	    hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	CallContext *contextPtr = Tcl_GetHashValue(hPtr);

	if (contextPtr) {
	    DeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->privateContextCache);

    cPtr = oPtr->classPtr;
    if (cPtr != NULL && !(oPtr->flags & ROOT_OBJECT)) {
	cPtr->flags |= OBJECT_DELETED;

	for (i=0 ; i<cPtr->superclasses.num ; i++) {
	    if (!(cPtr->superclasses.list[i]->flags & OBJECT_DELETED)) {
		TclOORemoveFromSubclasses(cPtr, cPtr->superclasses.list[i]);
	    }
	}
	cPtr->superclasses.num = 0;
	ckfree((char *) cPtr->superclasses.list);
	cPtr->subclasses.num = 0;
	ckfree((char *) cPtr->subclasses.list);
	cPtr->instances.num = 0;
	ckfree((char *) cPtr->instances.list);

	for (hPtr = Tcl_FirstHashEntry(&cPtr->classMethods, &search);
		hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	    TclDeleteMethod(Tcl_GetHashValue(hPtr));
	}
	Tcl_DeleteHashTable(&cPtr->classMethods);
	TclDeleteMethod(cPtr->constructorPtr);
	TclDeleteMethod(cPtr->destructorPtr);
	Tcl_EventuallyFree(cPtr, TCL_DYNAMIC);
    }

    /*
     * Delete the object structure itself.
     */

    if (!(oPtr->flags & OBJECT_DELETED)) {
	Tcl_EventuallyFree(oPtr, TCL_DYNAMIC);
	Tcl_Release(oPtr);
    } else {
	Tcl_EventuallyFree(oPtr, TCL_DYNAMIC);
    }
}

void
TclOORemoveFromInstances(
    Object *oPtr,
    Class *cPtr)
{
    int i;

    for (i=0 ; i<cPtr->instances.num ; i++) {
	if (oPtr == cPtr->instances.list[i]) {
	    if (i+1 < cPtr->instances.num) {
		cPtr->instances.list[i] =
			cPtr->instances.list[cPtr->instances.num - 1];
	    }
	    cPtr->instances.list[cPtr->instances.num - 1] = NULL;
	    cPtr->instances.num--;
	    break;
	}
    }
}

void
TclOOAddToInstances(
    Object *oPtr,
    Class *cPtr)
{
    if (cPtr->instances.num >= cPtr->instances.size) {
	cPtr->instances.size += ALLOC_CHUNK;
	if (cPtr->instances.size == ALLOC_CHUNK) {
	    cPtr->instances.list = (Object **)
		    ckalloc(sizeof(Object *) * ALLOC_CHUNK);
	} else {
	    cPtr->instances.list = (Object **)
		    ckrealloc((char *) cPtr->instances.list,
		    sizeof(Object *) * cPtr->instances.size);
	}
    }
    cPtr->instances.list[cPtr->instances.num++] = oPtr;
}

void
TclOORemoveFromSubclasses(
    Class *subPtr,
    Class *superPtr)
{
    int i;

    for (i=0 ; i<superPtr->subclasses.num ; i++) {
	if (subPtr == superPtr->subclasses.list[i]) {
	    if (i+1 < superPtr->subclasses.num) {
		superPtr->subclasses.list[i] =
			superPtr->subclasses.list[superPtr->subclasses.num-1];
	    }
	    superPtr->subclasses.list[--superPtr->subclasses.num] =
		    (Class *) NULL;
	    break;
	}
    }
}

void
TclOOAddToSubclasses(
    Class *subPtr,
    Class *superPtr)
{
    if (superPtr->subclasses.num >= superPtr->subclasses.size) {
	superPtr->subclasses.size += ALLOC_CHUNK;
	if (superPtr->subclasses.size == ALLOC_CHUNK) {
	    superPtr->subclasses.list = (Class **)
		    ckalloc(sizeof(Class *) * ALLOC_CHUNK);
	} else {
	    superPtr->subclasses.list = (Class **)
		    ckrealloc((char *) superPtr->subclasses.list,
		    sizeof(Class *) * superPtr->subclasses.size);
	}
    }
    superPtr->subclasses.list[superPtr->subclasses.num++] = subPtr;
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
    memset(clsPtr, 0, sizeof(Class));
    if (useThisObj == NULL) {
	clsPtr->thisPtr = AllocObject(interp, NULL);
    } else {
	clsPtr->thisPtr = useThisObj;
    }
    clsPtr->thisPtr->selfCls = fPtr->classCls;
    if (fPtr->classCls != NULL) {
	TclOOAddToInstances(clsPtr->thisPtr, fPtr->classCls);
	TclOOAddToSubclasses(clsPtr, fPtr->objectCls);
    }
    {
	Tcl_Namespace *path[2];

	path[0] = fPtr->helpersNs;
	path[1] = fPtr->ooNs;
	TclSetNsPath(clsPtr->thisPtr->nsPtr, 2, path);
    }
    clsPtr->thisPtr->classPtr = clsPtr;
    clsPtr->flags = 0;
    clsPtr->superclasses.num = 1;
    clsPtr->superclasses.list = (Class **) ckalloc(sizeof(Class *));
    clsPtr->superclasses.list[0] = fPtr->objectCls;
    clsPtr->subclasses.num = 0;
    clsPtr->subclasses.list = NULL;
    clsPtr->subclasses.size = 0;
    clsPtr->instances.num = 0;
    clsPtr->instances.list = NULL;
    clsPtr->instances.size = 0;
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    clsPtr->constructorPtr = NULL;
    clsPtr->destructorPtr = NULL;
    return clsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOONewInstance --
 *
 *	Allocate a new instance of an object.
 *
 * ----------------------------------------------------------------------
 */

Object *
TclOONewInstance(
    Tcl_Interp *interp,		/* Interpreter context. */
    Class *clsPtr,		/* Class to create an instance of. */
    char *name,			/* Name of object to create, or NULL to ask
				 * the code to pick its own unique name. */
    int objc,			/* Number of arguments. Negative value means
				 * do not call constructor. */
    Tcl_Obj *const *objv,	/* Argument list. */
    int skip)			/* Number of arguments to _not_ pass to the
				 * constructor. */
{
    Object *oPtr = AllocObject(interp, NULL);
    CallContext *contextPtr;

    oPtr->selfCls = clsPtr;
    TclOOAddToInstances(oPtr, clsPtr);

    if (name != NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	if (TclRenameCommand(interp, TclGetString(cmdnameObj),
		name) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "can't create object \"", name,
		    "\": command already exists with that name", NULL);
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

    if (TclOOIsReachable((((Interp *) interp)->ooFoundation)->classCls,
	    clsPtr)) {
	/*
	 * Is a class, so attach a class structure. Note that the AllocClass
	 * function splices the structure into the object, so we don't have
	 * to.
	 */

	AllocClass(interp, oPtr);
	oPtr->selfCls = clsPtr; // Repatch
    }

    if (objc >= 0) {
	contextPtr = GetCallContext(((Interp *)interp)->ooFoundation, oPtr,
		NULL, CONSTRUCTOR, NULL);
	if (contextPtr != NULL) {
	    int result;
	    Tcl_InterpState state;

	    Tcl_Preserve(oPtr);
	    state = Tcl_SaveInterpState(interp, TCL_OK);
	    contextPtr->flags |= CONSTRUCTOR;
	    contextPtr->skip = skip;
	    result = InvokeContext(interp, contextPtr, objc, objv);
	    DeleteContext(contextPtr);
	    Tcl_Release(oPtr);
	    if (result != TCL_OK) {
		Tcl_DiscardInterpState(state);
		Tcl_DeleteCommandFromToken(interp, oPtr->command);
		return NULL;
	    }
	    (void) Tcl_RestoreInterpState(interp, state);
	}
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
    TclOONewClassMethod(interp, (Tcl_Class) clsPtr, namePtr, isPublic,
	    &coreMethodType, callPtr);
    TclDecrRefCount(namePtr);
    return TCL_OK;
}
static int
SimpleInvoke(
    ClientData clientData,
    Tcl_Interp *interp,
    CallContext *oPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_OOMethodCallProc callPtr = clientData;

    return callPtr(clientData, interp, oPtr, objc, objv);
}
static int
SimpleClone(
    ClientData clientData,
    ClientData *newClientData)
{
    *newClientData = clientData;
    return TCL_OK;
}

Tcl_Method
TclOONewMethod(
    Tcl_Interp *interp,
    Tcl_Object object,
    Tcl_Obj *nameObj, /* May be NULL; if so, up to caller to manage storage. */
    int isPublic,
    const Tcl_OOMethodType *typePtr,
    ClientData clientData)
{
    register Object *oPtr = (Object *) object;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (nameObj == NULL) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	mPtr->namePtr = NULL;
	goto populate;
    }
    hPtr = Tcl_CreateHashEntry(&oPtr->methods, (char *) nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	mPtr->namePtr = nameObj;
	Tcl_IncrRefCount(nameObj);
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->typePtr != NULL && mPtr->typePtr->deletePtr != NULL) {
	    mPtr->typePtr->deletePtr(mPtr->clientData);
	}
    }

  populate:
    mPtr->typePtr = typePtr;
    mPtr->clientData = clientData;
    mPtr->epoch = ++((Interp *) interp)->ooFoundation->epoch;
    mPtr->flags = 0;
    mPtr->declaringObjectPtr = oPtr;
    mPtr->declaringClassPtr = NULL;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }
    return (Tcl_Method) mPtr;
}

Tcl_Method
TclOONewClassMethod(
    Tcl_Interp *interp,
    Tcl_Class cls,
    Tcl_Obj *nameObj, /* May be NULL; if so, up to caller to manage storage. */
    int isPublic,
    const Tcl_OOMethodType *typePtr,
    ClientData clientData)
{
    register Class *clsPtr = (Class *) cls;
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew;

    if (nameObj == NULL) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	mPtr->namePtr = NULL;
	goto populate;
    }
    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, (char *)nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	mPtr->namePtr = nameObj;
	Tcl_IncrRefCount(nameObj);
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->typePtr != NULL && mPtr->typePtr->deletePtr != NULL) {
	    mPtr->typePtr->deletePtr(mPtr->clientData);
	}
    }

  populate:
    mPtr->typePtr = typePtr;
    mPtr->clientData = clientData;
    mPtr->epoch = ++((Interp *) interp)->ooFoundation->epoch;
    mPtr->flags = 0;
    mPtr->declaringObjectPtr = NULL;
    mPtr->declaringClassPtr = clsPtr;
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
    return (Method *) TclOONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &procMethodType, pmPtr);
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
    return (Method *) TclOONewClassMethod(interp, (Tcl_Class) cPtr, nameObj,
	    isPublic, &procMethodType, pmPtr);
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

    if (contextPtr->callChain[contextPtr->index].isFilter) {
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

    TclProcDeleteProc(pmPtr->procPtr);
    ckfree((char *) pmPtr);
}

static int
CloneProcedureMethod(
    ClientData clientData,
    ClientData *newClientData)
{
    ProcedureMethod *pmPtr = (ProcedureMethod *) clientData;
    ProcedureMethod *pm2Ptr = (ProcedureMethod *)
	    ckalloc(sizeof(ProcedureMethod));

    pm2Ptr->procPtr = pmPtr->procPtr;
    pm2Ptr->procPtr->refCount++;
    *newClientData = pm2Ptr;
    return TCL_OK;
}

/* To be called from Tcl_EventuallyFree */
static void
DeleteMethodStruct(
    char *buffer)
{
    Method *mPtr = (Method *) buffer;

    if (mPtr->typePtr != NULL && mPtr->typePtr->deletePtr != NULL) {
	mPtr->typePtr->deletePtr(mPtr->clientData);
    }
    if (mPtr->namePtr != NULL) {
	TclDecrRefCount(mPtr->namePtr);
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

    fmPtr = (ForwardMethod *) ckalloc(sizeof(ForwardMethod));
    fmPtr->prefixObj = prefixObj;
    Tcl_IncrRefCount(prefixObj);
    return (Method *) TclOONewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &fwdMethodType, fmPtr);
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
    return (Method *) TclOONewClassMethod(interp, (Tcl_Class) cPtr, nameObj,
	    isPublic, &fwdMethodType, fmPtr);
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
    Interp *iPtr = (Interp *) interp;
    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);

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

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = skip;
	iPtr->ensembleRewrite.numInsertedObjs = numPrefixes;
    } else {
	int ni = iPtr->ensembleRewrite.numInsertedObjs;
	if (ni < skip) {
	    iPtr->ensembleRewrite.numRemovedObjs += skip - ni;
	    iPtr->ensembleRewrite.numInsertedObjs += numPrefixes - 1;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs += numPrefixes - skip;
	}
    }

    result = Tcl_EvalObjv(interp, numPrefixes + objc - skip, argObjs,
	    TCL_EVAL_INVOKE);
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
CloneForwardMethod(
    ClientData clientData,
    ClientData *newClientData)
{
    ForwardMethod *fmPtr = (ForwardMethod *) clientData;
    ForwardMethod *fm2Ptr = (ForwardMethod *) ckalloc(sizeof(ForwardMethod));

    fm2Ptr->prefixObj = fmPtr->prefixObj;
    Tcl_IncrRefCount(fm2Ptr->prefixObj);
    *newClientData = fm2Ptr;
    return TCL_OK;
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
    if (!(contextPtr->flags & OO_UNKNOWN_METHOD)
	    && !(oPtr->flags & OBJECT_DELETED)) {
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
    Method *mPtr = contextPtr->callChain[contextPtr->index].mPtr;
    int result, isFirst = (contextPtr->index == 0);

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Preserve(contextPtr->callChain[i].mPtr);
	}
    }

    result = mPtr->typePtr->callPtr(mPtr->clientData, interp, contextPtr,
	    objc, objv);

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Release(contextPtr->callChain[i].mPtr);
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
    for (i=0 ; i<oPtr->mixins.num ; i++) {
	AddClassMethodNames(oPtr->mixins.list[i], publicOnly, &names);
    }

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

	if (clsPtr->superclasses.num != 1) {
	    break;
	}
	clsPtr = clsPtr->superclasses.list[0];
    }
    if (clsPtr->superclasses.num != 0) {
	int i;

	for (i=0 ; i<clsPtr->superclasses.num ; i++) {
	    AddClassMethodNames(clsPtr->superclasses.list[i], publicOnly,
		    namesPtr);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * GetCallContext --
 *
 *	Responsible for constructing the call context, an ordered list of all
 *	method implementations to be called as part of a method invokation.
 *	This method is central to the whole operation of the OO system.
 *
 * ----------------------------------------------------------------------
 */

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
	    if ((contextPtr->globalEpoch == fPtr->epoch)
		    && (contextPtr->localEpoch == oPtr->epoch)) {
		return contextPtr;
	    }
	    DeleteContext(contextPtr);
	}
    }
    contextPtr = (CallContext *) ckalloc(sizeof(CallContext));
    contextPtr->numCallChain = 0;
    contextPtr->callChain = contextPtr->staticCallChain;
    contextPtr->filterLength = 0;
    contextPtr->globalEpoch = fPtr->epoch;
    contextPtr->localEpoch = oPtr->epoch;
    contextPtr->flags = 0;
    contextPtr->skip = 2;
    if (flags & (PUBLIC_METHOD | CONSTRUCTOR | DESTRUCTOR)) {
	contextPtr->flags |=
		flags & (PUBLIC_METHOD | CONSTRUCTOR | DESTRUCTOR);
    }
    contextPtr->oPtr = oPtr;
    contextPtr->index = 0;

    if (!(flags & (CONSTRUCTOR | DESTRUCTOR))) {
	for (i=0 ; i<oPtr->filters.num ; i++) {
	    AddSimpleChainToCallContext(oPtr, oPtr->filters.list[i],
		    contextPtr, 1, 0);
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
	contextPtr->globalEpoch = -1;
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

    if (!(flags & (KNOWN_STATE | CONSTRUCTOR | DESTRUCTOR))) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&oPtr->methods,
		(char *) methodNameObj);

	if (hPtr != NULL) {
	    Method *mPtr = Tcl_GetHashValue(hPtr);

	    if (flags & PUBLIC_METHOD) {
		if (!(mPtr->flags & PUBLIC_METHOD)) {
		    return;
		} else {
		    flags |= DEFINITE_PUBLIC;
		}
	    } else {
		flags |= DEFINITE_PRIVATE;
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
	for (i=0 ; i<oPtr->mixins.num ; i++) {
	    AddSimpleClassChainToCallContext(oPtr->mixins.list[i],
		    methodNameObj, contextPtr, isFilter, flags);
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
    /*
     * We hard-code the tail-recursive form. It's by far the most common case
     * *and* it is much more gentle on the stack.
     */

  tailRecurse:
    if (flags & CONSTRUCTOR) {
	AddMethodToCallChain(classPtr->constructorPtr, contextPtr, isFilter,
		flags);
    } else if (flags & DESTRUCTOR) {
	AddMethodToCallChain(classPtr->destructorPtr, contextPtr, isFilter,
		flags);
    } else {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&classPtr->classMethods,
		(char *) methodNameObj);

	if (hPtr != NULL) {
	    register Method *mPtr = Tcl_GetHashValue(hPtr);

	    if (!(flags & KNOWN_STATE)) {
		if (flags & PUBLIC_METHOD) {
		    if (mPtr->flags & PUBLIC_METHOD) {
			flags |= DEFINITE_PUBLIC;
		    } else {
			return;
		    }
		} else {
		    flags |= DEFINITE_PRIVATE;
		}
	    }
	    AddMethodToCallChain(mPtr, contextPtr, isFilter, flags);
	}
    }

    switch (classPtr->superclasses.num) {
    case 1:
	classPtr = classPtr->superclasses.list[0];
	goto tailRecurse;
    default:
    {
	int i;

	for (i=0 ; i<classPtr->superclasses.num ; i++) {
	    AddSimpleClassChainToCallContext(classPtr->superclasses.list[i],
		    methodNameObj, contextPtr, isFilter, flags);
	}
    }
    case 0:
	return;
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

    if (mPtr == NULL || mPtr->typePtr == NULL) {
	return;
    }

    /*
     * First test whether the method is already in the call chain. Skip over
     * any leading filters.
     */

    for (i=contextPtr->filterLength ; i<contextPtr->numCallChain ; i++) {
	if (contextPtr->callChain[i].mPtr == mPtr
		&& contextPtr->callChain[i].isFilter == isFilter) {
	    /*
	     * Call chain semantics states that methods come as *late* in the
	     * call chain as possible. This is done by copying down the
	     * following methods. Note that this does not change the number of
	     * method invokations in the call chain; it just rearranges them.
	     */

	    for (; i+1<contextPtr->numCallChain ; i++) {
		contextPtr->callChain[i] = contextPtr->callChain[i+1];
	    }
	    contextPtr->callChain[i].mPtr = mPtr;
	    contextPtr->callChain[i].isFilter = isFilter;
	    return;
	}
    }

    /*
     * Need to really add the method. This is made a bit more complex by the
     * fact that we are using some "static" space initially, and only start
     * realloc-ing if the chain gets long.
     */

    if (contextPtr->numCallChain == CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke *)
		ckalloc(sizeof(struct MInvoke) * (contextPtr->numCallChain+1));
	memcpy(contextPtr->callChain, contextPtr->staticCallChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    } else if (contextPtr->numCallChain > CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke *)
		ckrealloc((char *) contextPtr->callChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    }
    contextPtr->callChain[contextPtr->numCallChain].mPtr = mPtr;
    contextPtr->callChain[contextPtr->numCallChain].isFilter = isFilter;
    contextPtr->numCallChain++;
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
    if (objc-contextPtr->skip < 1) {
	Tcl_WrongNumArgs(interp, contextPtr->skip, objv,
		"objectName ?arg ...?");
	return TCL_ERROR;
    }
    newObjPtr = TclOONewInstance(interp, oPtr->classPtr,
	    TclGetString(objv[2]), objc, objv, contextPtr->skip+1);
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
    newObjPtr = TclOONewInstance(interp, oPtr->classPtr, NULL, objc, objv,
	    contextPtr->skip);
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
    if (objc-contextPtr->skip != 0) {
	Tcl_WrongNumArgs(interp, contextPtr->skip, objv, NULL);
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
    Tcl_Obj *objnameObj;
    int result;

    if (objc-contextPtr->skip < 1) {
	Tcl_WrongNumArgs(interp, contextPtr->skip, objv, "arg ?arg ...?");
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

    if (contextPtr->flags & PUBLIC_METHOD) {
	TclNewObj(objnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, objnameObj);
    } else {
	TclNewStringObj(objnameObj, "my", 2);
    }
    Tcl_IncrRefCount(objnameObj);

    if (objc == contextPtr->skip+1) {
	result = Tcl_EvalObjEx(interp, objv[contextPtr->skip], 0);
    } else {
	Tcl_Obj *objPtr;

	/*
	 * More than one argument: concatenate them together with spaces
	 * between, then evaluate the result. Tcl_EvalObjEx will delete the
	 * object when it decrements its refcount after eval'ing it.
	 */

	objPtr = Tcl_ConcatObj(objc-contextPtr->skip, objv+contextPtr->skip);
	result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);
    }

    if (result == TCL_ERROR) {
	TclFormatToErrorInfo(interp,
		"\n    (in \"%s eval\" script line %d)",
		TclGetString(objnameObj), interp->errorLine);
    }

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    TclDecrRefCount(objnameObj);
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

    if (objc-contextPtr->skip < 1) {
	Tcl_WrongNumArgs(interp, contextPtr->skip, objv,
		"varName ?varName ...?");
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
    CallFrame *framePtr = iPtr->varFramePtr, *savedFramePtr;
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
	Tcl_AppendResult(interp, "no superclass ",
		(contextPtr->flags & CONSTRUCTOR ? "constructor" :
		(contextPtr->flags & DESTRUCTOR ? "destructor" : "method")),
		" implementation", NULL);
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
     * Invoke the (advanced) method call context in the caller context. Note
     * that this is like [uplevel 1] and not [eval].
     */

    savedFramePtr = iPtr->varFramePtr;
    iPtr->varFramePtr = savedFramePtr->callerVarPtr;
    result = InvokeContext(interp, contextPtr, objc, objv);
    iPtr->varFramePtr = savedFramePtr;

    /*
     * If an error happened, add information about this to the trace.
     */

    if (result == TCL_ERROR) {
	Tcl_Obj *tmpObj = NULL;
	const char *classname = "superclass";
	struct MInvoke *miPtr = &contextPtr->callChain[index+1];

	if (!Tcl_InterpDeleted(interp)) {
	    if (miPtr->mPtr->declaringClassPtr != NULL) {
		TclNewObj(tmpObj);
		Tcl_GetCommandFullName(interp,
			miPtr->mPtr->declaringClassPtr->thisPtr->command,
			tmpObj);
		classname = TclGetString(tmpObj);
	    } else if (miPtr->mPtr->declaringObjectPtr != NULL) {
		TclNewObj(tmpObj);
		Tcl_GetCommandFullName(interp,
			miPtr->mPtr->declaringObjectPtr->command, tmpObj);
		classname = TclGetString(tmpObj);
	    }
	}

	if (contextPtr->flags & CONSTRUCTOR) {
	    TclFormatToErrorInfo(interp,
		    "\n    (\"%s\" implementation of constructor)",
		    classname);
	} else if (contextPtr->flags & DESTRUCTOR) {
	    TclFormatToErrorInfo(interp,
		    "\n    (\"%s\" implementation of destructor)", classname);
	} else {
	    Tcl_Obj *methodNameObj = miPtr->mPtr->namePtr;
	    TclFormatToErrorInfo(interp,
		    "\n    (\"%s\" implementation of \"%s\" method)",
		    classname, TclGetString(methodNameObj));
	}
	if (tmpObj != NULL) {
	    TclDecrRefCount(tmpObj);
	}
    }

    /*
     * Restore the call chain context index as we've finished the inner invoke
     * and want to operate in the outer context again.
     */

    contextPtr->index = index;
    contextPtr->skip = skip;

    return result;
}

static int
SelfObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    static const char *subcmds[] = {
	"caller", "class", "filter", "method", "namespace", "next", "object",
	"target", NULL
    };
    enum SelfCmds {
	SELF_CALLER, SELF_CLASS, SELF_FILTER, SELF_METHOD, SELF_NS, SELF_NEXT,
	SELF_OBJECT, SELF_TARGET
    };
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;
    CallContext *contextPtr;
    int index;

    /*
     * Start with sanity checks on the calling context and the method context.
     */

    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	Tcl_AppendResult(interp, TclGetString(objv[0]),
		" may only be called from inside a method", NULL);
	return TCL_ERROR;
    }

    contextPtr = framePtr->ooContextPtr;

    /*
     * Now we do "conventional" argument parsing for a while. Note that no
     * subcommand takes arguments.
     */

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand");
	return TCL_ERROR;
    }
    if (objc == 1) {
	index = SELF_OBJECT;
    } else if (Tcl_GetIndexFromObj(interp, objv[1], subcmds, "subcommand", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum SelfCmds) index) {
    case SELF_OBJECT:
	Tcl_GetCommandFullName(interp, contextPtr->oPtr->command,
		Tcl_GetObjResult(interp));
	return TCL_OK;
    case SELF_NS:
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj(contextPtr->oPtr->nsPtr->fullName, -1));
	return TCL_OK;
    case SELF_CLASS: {
	Method *mPtr = contextPtr->callChain[contextPtr->index].mPtr;
	Object *declarerPtr;

	if (mPtr->declaringClassPtr != NULL) {
	    declarerPtr = mPtr->declaringClassPtr->thisPtr;
	} else if (mPtr->declaringObjectPtr != NULL) {
	    declarerPtr = mPtr->declaringObjectPtr;
	} else {
	    /*
	     * This should be unreachable code.
	     */

	    Tcl_AppendResult(interp, "method without declarer!", NULL);
	    return TCL_ERROR;
	}

	Tcl_GetCommandFullName(interp, declarerPtr->command,
		Tcl_GetObjResult(interp));

	return TCL_OK;
    }
    case SELF_METHOD: {
	Method *mPtr = contextPtr->callChain[contextPtr->index].mPtr;

	if (contextPtr->flags & CONSTRUCTOR) {
	    Tcl_AppendResult(interp, "<constructor>", NULL);
	} else if (contextPtr->flags & DESTRUCTOR) {
	    Tcl_AppendResult(interp, "<destructor>", NULL);
	} else {
	    Tcl_SetObjResult(interp, mPtr->namePtr);
	}
	return TCL_OK;
    }
    default:
	Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
	return TCL_ERROR;
    }
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

int
TclOOIsReachable(
    Class *targetPtr,
    Class *startPtr)
{
    int i;

  tailRecurse:
    if (startPtr == targetPtr) {
	return 1;
    }
    if (startPtr->superclasses.num == 1) {
	startPtr = startPtr->superclasses.list[0];
	goto tailRecurse;
    }
    for (i=0 ; i<startPtr->superclasses.num ; i++) {
	if (TclOOIsReachable(targetPtr, startPtr->superclasses.list[i])) {
	    return 1;
	}
    }
    return 0;
}

Proc *
TclOOGetProcFromMethod(
    Method *mPtr)
{
    if (mPtr->typePtr == &procMethodType) {
	ProcedureMethod *pmPtr = mPtr->clientData;

	return pmPtr->procPtr;
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
