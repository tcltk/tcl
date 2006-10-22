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
 * RCS: @(#) $Id: tclOO.c,v 1.1.2.64 2006/10/22 00:26:30 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

/*
 * Commands in oo::define.
 */

static const struct {
    const char *name;
    Tcl_ObjCmdProc *objProc;
    int flag;
} defineCmds[] = {
    {"constructor", TclOODefineConstructorObjCmd, 0},
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
    {"superclass", TclOODefineSuperclassObjCmd, 0},
    {"unexport", TclOODefineUnexportObjCmd, 0},
    {"self.unexport", TclOODefineUnexportObjCmd, 1},
    {"self.class", TclOODefineSelfClassObjCmd, 1},
    {NULL, NULL, 0}
};

/*
 * What sort of size of things we like to allocate.
 */

#define ALLOC_CHUNK 8

/*
 * Function declarations for things defined in this file.
 */

static Class *		AllocClass(Tcl_Interp *interp, Object *useThisObj);
static Object *		AllocObject(Tcl_Interp *interp, const char *nameStr);
static Method *		CloneClassMethod(Tcl_Interp *interp, Class *clsPtr,
			    Method *mPtr, Tcl_Obj *namePtr);
static Method *		CloneObjectMethod(Tcl_Interp *interp, Object *oPtr,
			    Method *mPtr, Tcl_Obj *namePtr);
static void		DeclareClassMethod(Tcl_Interp *interp, Class *clsPtr,
			    const char *name, int isPublic,
			    Tcl_MethodCallProc callProc);
static void		KillFoundation(ClientData clientData,
			    Tcl_Interp *interp);
static int		ObjectCmd(Object *oPtr, Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv, int publicOnly,
			    Tcl_HashTable *cachePtr);
static void		ObjectNamespaceDeleted(ClientData clientData);
static void		ObjectDeletedTrace(ClientData clientData,
			    Tcl_Interp *interp, const char *oldName,
			    const char *newName, int flags);
static void		ReleaseClassContents(Tcl_Interp *interp,Object *oPtr);

static int		CopyObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PublicObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);

static int		SimpleInvoke(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		InvokeProcedureMethod(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteProcedureMethod(ClientData clientData);
static int		CloneProcedureMethod(ClientData clientData,
			    ClientData *newClientData);
static int		InvokeForwardMethod(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static void		DeleteForwardMethod(ClientData clientData);
static int		CloneForwardMethod(ClientData clientData,
			    ClientData *newClientData);
static Tcl_Obj **	InitEnsembleRewrite(Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv, int toRewrite,
			    int rewriteLength, Tcl_Obj *const *rewriteObjs,
			    int *lengthPtr);

static int		ClassCreate(ClientData clientData, Tcl_Interp *interp,
			    Tcl_ObjectContext context, int objc,
			    Tcl_Obj *const *objv);
static int		ClassNew(ClientData clientData, Tcl_Interp *interp,
			    Tcl_ObjectContext context, int objc,
			    Tcl_Obj *const *objv);
static int		ObjectDestroy(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		ObjectEval(ClientData clientData, Tcl_Interp *interp,
			    Tcl_ObjectContext context, int objc,
			    Tcl_Obj *const *objv);
static int		ObjectLinkVar(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		ObjectUnknown(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		ObjectVarName(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);

static int		NextObjCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const *objv);
static int		SelfObjCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const *objv);

/*
 * The types of methods defined by the core OO system.
 */

static const Tcl_MethodType procMethodType = {
    TCL_OO_METHOD_VERSION_CURRENT, "procedural method",
    InvokeProcedureMethod, DeleteProcedureMethod, CloneProcedureMethod
};
static const Tcl_MethodType fwdMethodType = {
    TCL_OO_METHOD_VERSION_CURRENT, "forward",
    InvokeForwardMethod, DeleteForwardMethod, CloneForwardMethod
};
static const Tcl_MethodType coreMethodType = {
    TCL_OO_METHOD_VERSION_CURRENT, "core method",
    SimpleInvoke, NULL, NULL
};

/*
 * ----------------------------------------------------------------------
 *
 * TclOOInit --
 *
 *	Called to initialise the OO system within an interpreter.
 *
 * Result:
 *	TCL_OK if the setup succeeded. Currently assumed to always work.
 *
 * Side effects:
 *	Creates namespaces, commands, several classes and a number of
 *	callbacks. Upon return, the OO system is ready for use.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOInit(
    Tcl_Interp *interp)		/* The interpreter to install into. */
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
    Tcl_Export(interp, fPtr->ooNs, "[a-z]*", 1);
    fPtr->defineNs = Tcl_CreateNamespace(interp, "::oo::define", NULL, NULL);
    fPtr->helpersNs = Tcl_CreateNamespace(interp, "::oo::Helpers", NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::Helpers::next", NextObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::Helpers::self", SelfObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::define", TclOODefineObjCmd, NULL,
	    NULL);
    Tcl_CreateObjCommand(interp, "::oo::copy", CopyObjectCmd, NULL, NULL);
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
    DeclareClassMethod(interp, fPtr->objectCls, "varname", 0, ObjectVarName);
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
	Tcl_NewMethod(interp, (Tcl_Object) fPtr->classCls->thisPtr, namePtr,
		0 /* ==private */, NULL, NULL);

	argsPtr = Tcl_NewStringObj("{definitionScript {}}", -1);
	bodyPtr = Tcl_NewStringObj(
		"if {[catch {define [self] $definitionScript} msg opt]} {\n"
		"set ei [split [dict get $opt -errorinfo] \\n]\n"
		"dict set opt -errorinfo [join [lrange $ei 0 end-2] \\n]\n"
		"dict set opt -errorline 0xdeadbeef\n"
		"}\n"
		"return -options $opt $msg", -1);
	fPtr->classCls->constructorPtr = TclOONewProcClassMethod(interp,
		fPtr->classCls, 0, NULL, argsPtr, bodyPtr);
    }

    /*
     * Finish setting up the [info object] and [info class] commands.
     */

    TclOOInitInfo(interp);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * KillFoundation --
 *
 *	Delete those parts of the OO core that are not deleted automatically
 *	when the objects and classes themselves are destroyed.
 *
 * ----------------------------------------------------------------------
 */

static void
KillFoundation(
    ClientData clientData,	/* Pointer to the OO system foundation
				 * structure. */
    Tcl_Interp *interp)		/* The interpreter containing the OO system
				 * foundation. */
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
    Tcl_Interp *interp,		/* Interpreter within which to create the
				 * object. */
    const char *nameStr)	/* The name of the object to create, or NULL
				 * if the OO system should pick the object
				 * name itself. */
{
    Foundation *fPtr = ((Interp *) interp)->ooFoundation;
    Tcl_Obj *cmdnameObj;
    Tcl_DString buffer;
    Object *oPtr;

    oPtr = (Object *) ckalloc(sizeof(Object));
    memset(oPtr, 0, sizeof(Object));
    while (1) {
	char objName[10 + TCL_INTEGER_SPACE];

	sprintf(objName, "::oo::Obj%d", ++fPtr->nsCount);
	oPtr->namespacePtr = Tcl_CreateNamespace(interp, objName, oPtr,
		ObjectNamespaceDeleted);
	if (oPtr->namespacePtr != NULL) {
	    break;
	}

	/*
	 * Could not make that namespace, so we make another. But first we
	 * have to get rid of the error message from Tcl_CreateNamespace,
	 * since that's something that should not be exposed to the user.
	 */

	Tcl_ResetResult(interp);
    }
    TclSetNsPath((Namespace *) oPtr->namespacePtr, 1, &fPtr->helpersNs);
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
    oPtr->metadataPtr = NULL;

    /*
     * Initialize the traces.
     */

    Tcl_DStringInit(&buffer);
    if (nameStr) {
	if (nameStr[0] != ':' || nameStr[1] != ':') {
	    Tcl_DStringAppend(&buffer,
		    Tcl_GetCurrentNamespace(interp)->fullName, -1);
	    Tcl_DStringAppend(&buffer, "::", 2);
	}
	Tcl_DStringAppend(&buffer, nameStr, -1);
    } else {
	Tcl_DStringAppend(&buffer, oPtr->namespacePtr->fullName, -1);
    }
    oPtr->command = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
	    PublicObjectCmd, oPtr, NULL);
    if (nameStr) {
	Tcl_DStringFree(&buffer);
	Tcl_DStringAppend(&buffer, oPtr->namespacePtr->fullName, -1);
    }
    Tcl_DStringAppend(&buffer, "::my", 4);
    oPtr->myCommand = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&buffer),
	    PrivateObjectCmd, oPtr, NULL);
    Tcl_DStringFree(&buffer);

    TclNewObj(cmdnameObj);
    Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
    Tcl_TraceCommand(interp, TclGetString(cmdnameObj),
	    TCL_TRACE_DELETE, ObjectDeletedTrace, oPtr);
    TclDecrRefCount(cmdnameObj);

    return oPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectDeletedTrace --
 *
 *	This callback is triggered when the object is deleted by any
 *	mechanism. It runs the destructors and arranges for the actual cleanup
 *	of the object's namespace, which in turn triggers cleansing of the
 *	object data structures.
 *
 * ----------------------------------------------------------------------
 */

static void
ObjectDeletedTrace(
    ClientData clientData,	/* The object being deleted. */
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    const char *oldName,	/* What the object was (last) called. */
    const char *newName,	/* Always NULL. */
    int flags)			/* Why was the object deleted? */
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr = clientData;
    Class *clsPtr;

    Tcl_Preserve(oPtr);
    oPtr->flags |= OBJECT_DELETED;
    if (!Tcl_InterpDeleted(interp)) {
	CallContext *contextPtr = TclOOGetCallContext(iPtr->ooFoundation,
		oPtr, NULL, DESTRUCTOR, NULL);

	if (contextPtr != NULL) {
	    int result;
	    Tcl_InterpState state;

	    contextPtr->flags |= DESTRUCTOR;
	    contextPtr->skip = 0;
	    state = Tcl_SaveInterpState(interp, TCL_OK);
	    result = TclOOInvokeContext(interp, contextPtr, 0, NULL);
	    if (result != TCL_OK) {
		Tcl_BackgroundError(interp);
	    }
	    (void) Tcl_RestoreInterpState(interp, state);
	    TclOODeleteContext(contextPtr);
	}
    }

    clsPtr = oPtr->classPtr;
    if (clsPtr != NULL) {
	ReleaseClassContents(interp, oPtr);
    }

    Tcl_DeleteNamespace(oPtr->namespacePtr);
    if (clsPtr) {
	Tcl_Release(clsPtr);
    }
    Tcl_Release(oPtr);

    /*
     * What else to do to delete an object?
     */
}

/*
 * ----------------------------------------------------------------------
 *
 * ReleaseClassContents --
 *
 *	Tear down the special class data structure, including deleting all
 *	dependent classes and objects.
 *
 * ----------------------------------------------------------------------
 */

static void
ReleaseClassContents(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Object *oPtr)		/* The object representing the class. */
{
    int i, n;
    Class *clsPtr, **list;
    Object **insts;

    clsPtr = oPtr->classPtr;
    Tcl_Preserve(clsPtr);

    /*
     * Must empty list before processing the members of the list so that
     * things happen in the correct order even if something tries to play
     * fast-and-loose.
     */

    list = clsPtr->mixinSubs.list;
    n = clsPtr->mixinSubs.num;
    clsPtr->mixinSubs.list = NULL;
    clsPtr->mixinSubs.num = 0;
    clsPtr->mixinSubs.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(list[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(list[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, list[i]->thisPtr->command);
	}
	Tcl_Release(list[i]);
    }
    if (list != NULL) {
	ckfree((char *) list);
    }

    list = clsPtr->subclasses.list;
    n = clsPtr->subclasses.num;
    clsPtr->subclasses.list = NULL;
    clsPtr->subclasses.num = 0;
    clsPtr->subclasses.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(list[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(list[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, list[i]->thisPtr->command);
	}
	Tcl_Release(list[i]);
    }
    if (list != NULL) {
	ckfree((char *) list);
    }

    insts = clsPtr->instances.list;
    n = clsPtr->instances.num;
    clsPtr->instances.list = NULL;
    clsPtr->instances.num = 0;
    clsPtr->instances.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(insts[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(insts[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, insts[i]->command);
	}
	Tcl_Release(insts[i]);
    }
    if (insts != NULL) {
	ckfree((char *) insts);
    }

    if (clsPtr->filters.num) {
	Tcl_Obj *filterObj;

	FOREACH(filterObj, clsPtr->filters) {
	    TclDecrRefCount(filterObj);
	}
	ckfree((char *) clsPtr->filters.list);
	clsPtr->filters.num = 0;
    }

    if (clsPtr->metadataPtr != NULL) {
	FOREACH_HASH_DECLS;
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value;

	FOREACH_HASH(metadataTypePtr, value, clsPtr->metadataPtr) {
	    metadataTypePtr->deleteProc(value);
	}
	Tcl_DeleteHashTable(clsPtr->metadataPtr);
	ckfree((char *) clsPtr->metadataPtr);
	clsPtr->metadataPtr = NULL;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectNamespaceDeleted --
 *
 *	Callback when the object's namespace is deleted. Used to clean up the
 *	data structures associated with the object. The complicated bit is
 *	that this can sometimes happen before the object's command is deleted
 *	(interpreter teardown is complex!)
 *
 * ----------------------------------------------------------------------
 */

static void
ObjectNamespaceDeleted(
    ClientData clientData)	/* Pointer to the class whose namespace is
				 * being deleted. */
{
    Object *oPtr = clientData;
    FOREACH_HASH_DECLS;
    Class *clsPtr, *mixinPtr;
    CallContext *contextPtr;
    Method *mPtr;
    Tcl_Obj *filterObj;
    int i;

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
    FOREACH(mixinPtr, oPtr->mixins) {
	TclOORemoveFromInstances(oPtr, mixinPtr);
    }
    if (i) {
	ckfree((char *)oPtr->mixins.list);
    }
    FOREACH(filterObj, oPtr->filters) {
	TclDecrRefCount(filterObj);
    }
    if (i) {
	ckfree((char *)oPtr->filters.list);
    }
    FOREACH_HASH_VALUE(mPtr, &oPtr->methods) {
	TclOODeleteMethod(mPtr);
    }
    Tcl_DeleteHashTable(&oPtr->methods);
    FOREACH_HASH_VALUE(contextPtr, &oPtr->publicContextCache) {
	if (contextPtr) {
	    TclOODeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->publicContextCache);
    FOREACH_HASH_VALUE(contextPtr, &oPtr->privateContextCache) {
	if (contextPtr) {
	    TclOODeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->privateContextCache);

    if (oPtr->metadataPtr != NULL) {
	FOREACH_HASH_DECLS;
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value;

	FOREACH_HASH(metadataTypePtr, value, oPtr->metadataPtr) {
	    metadataTypePtr->deleteProc(value);
	}
	Tcl_DeleteHashTable(oPtr->metadataPtr);
	ckfree((char *) oPtr->metadataPtr);
	oPtr->metadataPtr = NULL;
    }

    clsPtr = oPtr->classPtr;
    if (clsPtr != NULL && !(oPtr->flags & ROOT_OBJECT)) {
	Class *superPtr, *mixinPtr;

	clsPtr->flags |= OBJECT_DELETED;
	FOREACH(mixinPtr, clsPtr->mixins) {
	    if (!(mixinPtr->flags & OBJECT_DELETED)) {
		TclOORemoveFromSubclasses(clsPtr, mixinPtr);
	    }
	}
	if (i) {
	    ckfree((char *) clsPtr->mixins.list);
	    clsPtr->mixins.num = 0;
	}
	FOREACH(superPtr, clsPtr->superclasses) {
	    if (!(superPtr->flags & OBJECT_DELETED)) {
		TclOORemoveFromSubclasses(clsPtr, superPtr);
	    }
	}
	if (i) {
	    ckfree((char *) clsPtr->superclasses.list);
	    clsPtr->superclasses.num = 0;
	}
	if (clsPtr->subclasses.list) {
	    ckfree((char *) clsPtr->subclasses.list);
	    clsPtr->subclasses.num = 0;
	}
	if (clsPtr->instances.list) {
	    ckfree((char *) clsPtr->instances.list);
	    clsPtr->instances.num = 0;
	}
	if (clsPtr->mixinSubs.list) {
	    ckfree((char *) clsPtr->mixinSubs.list);
	    clsPtr->mixinSubs.num = 0;
	}
	if (clsPtr->classHierarchy.list) {
	    ckfree((char *) clsPtr->classHierarchy.list);
	    clsPtr->classHierarchy.num = 0;
	}

	FOREACH_HASH_VALUE(mPtr, &clsPtr->classMethods) {
	    TclOODeleteMethod(mPtr);
	}
	Tcl_DeleteHashTable(&clsPtr->classMethods);
	TclOODeleteMethod(clsPtr->constructorPtr);
	TclOODeleteMethod(clsPtr->destructorPtr);
	Tcl_EventuallyFree(clsPtr, TCL_DYNAMIC);
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

/*
 * ----------------------------------------------------------------------
 *
 * TclOORemoveFromInstances --
 *
 *	Utility function to remove an object from the list of instances within
 *	a class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromInstances(
    Object *oPtr,		/* The instance to remove. */
    Class *clsPtr)		/* The class (possibly) containing the
				 * reference to the instance. */
{
    int i;
    Object *instPtr;

    FOREACH(instPtr, clsPtr->instances) {
	if (oPtr == instPtr) {
	    goto removeInstance;
	}
    }
    return;

  removeInstance:
    clsPtr->instances.num--;
    if (i < clsPtr->instances.num) {
	clsPtr->instances.list[i] =
		clsPtr->instances.list[clsPtr->instances.num];
    }
    clsPtr->instances.list[clsPtr->instances.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToInstances --
 *
 *	Utility function to add an object to the list of instances within a
 *	class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToInstances(
    Object *oPtr,		/* The instance to add. */
    Class *clsPtr)		/* The class to add the instance to. It is
				 * assumed that the class is not already
				 * present as an instance in the class. */
{
    if (clsPtr->instances.num >= clsPtr->instances.size) {
	clsPtr->instances.size += ALLOC_CHUNK;
	if (clsPtr->instances.size == ALLOC_CHUNK) {
	    clsPtr->instances.list = (Object **)
		    ckalloc(sizeof(Object *) * ALLOC_CHUNK);
	} else {
	    clsPtr->instances.list = (Object **)
		    ckrealloc((char *) clsPtr->instances.list,
		    sizeof(Object *) * clsPtr->instances.size);
	}
    }
    clsPtr->instances.list[clsPtr->instances.num++] = oPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOORemoveFromSubclasses --
 *
 *	Utility function to remove a class from the list of subclasses within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromSubclasses(
    Class *subPtr,		/* The subclass to remove. */
    Class *superPtr)		/* The superclass to (possibly) remove the
				 * subclass reference from. */
{
    int i;
    Class *subclsPtr;

    FOREACH(subclsPtr, superPtr->subclasses) {
	if (subPtr == subclsPtr) {
	    goto removeSubclass;
	}
    }
    return;

  removeSubclass:
    superPtr->subclasses.num--;
    if (i < superPtr->subclasses.num) {
	superPtr->subclasses.list[i] =
	    superPtr->subclasses.list[superPtr->subclasses.num];
    }
    superPtr->subclasses.list[superPtr->subclasses.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToSubclasses --
 *
 *	Utility function to add a class to the list of subclasses within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToSubclasses(
    Class *subPtr,		/* The subclass to add. */
    Class *superPtr)		/* The superclass to add the subclass to. It
				 * is assumed that the class is not already
				 * present as a subclass in the superclass. */
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
 * TclOORemoveFromMixinSubs --
 *
 *	Utility function to remove a class from the list of mixinSubs within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOORemoveFromMixinSubs(
    Class *subPtr,		/* The subclass to remove. */
    Class *superPtr)		/* The superclass to (possibly) remove the
				 * subclass reference from. */
{
    int i;
    Class *subclsPtr;

    FOREACH(subclsPtr, superPtr->mixinSubs) {
	if (subPtr == subclsPtr) {
	    goto removeSubclass;
	}
    }
    return;

  removeSubclass:
    superPtr->mixinSubs.num--;
    if (i < superPtr->mixinSubs.num) {
	superPtr->mixinSubs.list[i] =
	    superPtr->mixinSubs.list[superPtr->mixinSubs.num];
    }
    superPtr->mixinSubs.list[superPtr->mixinSubs.num] = NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOAddToMixinSubs --
 *
 *	Utility function to add a class to the list of mixinSubs within
 *	another class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOAddToMixinSubs(
    Class *subPtr,		/* The subclass to add. */
    Class *superPtr)		/* The superclass to add the subclass to. It
				 * is assumed that the class is not already
				 * present as a subclass in the superclass. */
{
    if (superPtr->mixinSubs.num >= superPtr->mixinSubs.size) {
	superPtr->mixinSubs.size += ALLOC_CHUNK;
	if (superPtr->mixinSubs.size == ALLOC_CHUNK) {
	    superPtr->mixinSubs.list = (Class **)
		    ckalloc(sizeof(Class *) * ALLOC_CHUNK);
	} else {
	    superPtr->mixinSubs.list = (Class **)
		    ckrealloc((char *) superPtr->mixinSubs.list,
		    sizeof(Class *) * superPtr->mixinSubs.size);
	}
    }
    superPtr->mixinSubs.list[superPtr->mixinSubs.num++] = subPtr;
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
    Tcl_Interp *interp,		/* Interpreter within which to allocate the
				 * class. */
    Object *useThisObj)		/* Object that is to act as the class
				 * representation, or NULL if a new object
				 * (with automatic name) is to be used. */
{
    Class *clsPtr;
    Foundation *fPtr = ((Interp *) interp)->ooFoundation;

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
	TclSetNsPath((Namespace *) clsPtr->thisPtr->namespacePtr, 2, path);
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
    clsPtr->filters.list = NULL;
    clsPtr->filters.num = 0;
    clsPtr->mixins.list = NULL;
    clsPtr->mixins.num = 0;
    clsPtr->mixinSubs.list = NULL;
    clsPtr->mixinSubs.num = 0;
    clsPtr->mixinSubs.size = 0;
    clsPtr->classHierarchy.list = NULL;
    clsPtr->classHierarchy.num = 0;
    clsPtr->classHierarchyEpoch = fPtr->epoch-1;
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    clsPtr->constructorPtr = NULL;
    clsPtr->destructorPtr = NULL;
    clsPtr->metadataPtr = NULL;
    return clsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_NewObjectInstance --
 *
 *	Allocate a new instance of an object.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_NewObjectInstance(
    Tcl_Interp *interp,		/* Interpreter context. */
    Tcl_Class cls,		/* Class to create an instance of. */
    const char *name,		/* Name of object to create, or NULL to ask
				 * the code to pick its own unique name. */
    int objc,			/* Number of arguments. Negative value means
				 * do not call constructor. */
    Tcl_Obj *const *objv,	/* Argument list. */
    int skip)			/* Number of arguments to _not_ pass to the
				 * constructor. */
{
    Object *oPtr = AllocObject(interp, NULL);
    CallContext *contextPtr;

    oPtr->selfCls = (Class *) cls;
    TclOOAddToInstances(oPtr, (Class *) cls);

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
	    (Class *) cls)) {
	/*
	 * Is a class, so attach a class structure. Note that the AllocClass
	 * function splices the structure into the object, so we don't have
	 * to. Once that's done, we need to repatch the object to have the
	 * right class since AllocClass interferes with that.
	 */

	AllocClass(interp, oPtr);
	oPtr->selfCls = (Class *) cls;
    }

    if (objc >= 0) {
	contextPtr = TclOOGetCallContext(((Interp *)interp)->ooFoundation,
		oPtr, NULL, CONSTRUCTOR, NULL);
	if (contextPtr != NULL) {
	    int result;
	    Tcl_InterpState state;

	    Tcl_Preserve(oPtr);
	    state = Tcl_SaveInterpState(interp, TCL_OK);
	    contextPtr->flags |= CONSTRUCTOR;
	    contextPtr->skip = skip;
	    result = TclOOInvokeContext(interp, contextPtr, objc, objv);
	    TclOODeleteContext(contextPtr);
	    Tcl_Release(oPtr);
	    if (result != TCL_OK) {
		Tcl_DiscardInterpState(state);
		Tcl_DeleteCommandFromToken(interp, oPtr->command);
		return NULL;
	    }
	    (void) Tcl_RestoreInterpState(interp, state);
	}
    }

    return (Tcl_Object) oPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_CopyObjectInstance --
 *
 *	Creates a copy of an object. Does not copy the backing namespace,
 *	since the correct way to do that (e.g., shallow/deep) depends on the
 *	object/class's own policies.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_CopyObjectInstance(
    Tcl_Interp *interp,
    Tcl_Object sourceObject,
    const char *targetName)
{
    Object *oPtr = (Object *) sourceObject, *o2Ptr;
    Interp *iPtr = (Interp *) interp;
    FOREACH_HASH_DECLS;
    Method *mPtr;
    Class *mixinPtr;
    Tcl_Obj *keyPtr, *filterObj;
    int i;

    /*
     * Sanity checks.
     */

    if (targetName == NULL && oPtr->classPtr != NULL) {
	Tcl_AppendResult(interp, "must supply a name when copying a class",
		NULL);
	return NULL;
    }
    if (oPtr->classPtr == iPtr->ooFoundation->classCls) {
	Tcl_AppendResult(interp, "may not clone the class of classes", NULL);
	return NULL;
    }

    /*
     * Build the instance. Note that this does not run any constructors.
     */

    o2Ptr = (Object *) Tcl_NewObjectInstance(interp,
	    (Tcl_Class) oPtr->selfCls, targetName, -1, NULL, -1);
    if (o2Ptr == NULL) {
	return NULL;
    }

    /*
     * Copy the object-local methods to the new object.
     */

    FOREACH_HASH(keyPtr, mPtr, &oPtr->methods) {
	(void) CloneObjectMethod(interp, o2Ptr, mPtr, keyPtr);
    }

    /*
     * Copy the object's mixin references to the new object.
     */

    FOREACH(mixinPtr, o2Ptr->mixins) {
	if (mixinPtr != o2Ptr->selfCls) {
	    TclOORemoveFromInstances(o2Ptr, mixinPtr);
	}
    }
    DUPLICATE(o2Ptr->mixins, oPtr->mixins, Class *);
    FOREACH(mixinPtr, o2Ptr->mixins) {
	if (mixinPtr != o2Ptr->selfCls) {
	    TclOOAddToInstances(o2Ptr, mixinPtr);
	}
    }

    /*
     * Copy the object's filter list to the new object.
     */

    DUPLICATE(o2Ptr->filters, oPtr->filters, Tcl_Obj *);
    FOREACH(filterObj, o2Ptr->filters) {
	Tcl_IncrRefCount(filterObj);
    }

    /*
     * Copy the object's flags to the new object, clearing those that must be
     * kept object-local. The duplicate is never deleted at this point, nor is
     * it the root of the object system or in the midst of processing a filter
     * call.
     */

    o2Ptr->flags = oPtr->flags & ~(
	    OBJECT_DELETED | ROOT_OBJECT | FILTER_HANDLING);

    /*
     * Copy the object's metadata.
     */

    if (oPtr->metadataPtr != NULL) {
	Tcl_ObjectMetadataType *metadataTypePtr;
	ClientData value, duplicate;

	FOREACH_HASH(metadataTypePtr, value, oPtr->metadataPtr) {
	    if (metadataTypePtr->cloneProc == NULL) {
		continue;
	    }
	    duplicate = metadataTypePtr->cloneProc(value);
	    if (duplicate != NULL) {
		Tcl_ObjectSetMetadata((Tcl_Object) o2Ptr, metadataTypePtr,
			duplicate);
	    }
	}
    }

    /*
     * Copy the class, if present. Note that if there is a class present in
     * the source object, there must also be one in the copy.
     */

    if (oPtr->classPtr != NULL) {
	Class *clsPtr = oPtr->classPtr;
	Class *cls2Ptr = o2Ptr->classPtr;
	Class *superPtr;

	/*
	 * Copy the class flags across.
	 */

	cls2Ptr->flags = clsPtr->flags;

	/*
	 * Ensure that the new class's superclass structure is the same as the
	 * old class's.
	 */

	FOREACH(superPtr, cls2Ptr->superclasses) {
	    TclOORemoveFromSubclasses(cls2Ptr, superPtr);
	}
	if (cls2Ptr->superclasses.num) {
	    cls2Ptr->superclasses.list = (Class **)
		    ckrealloc((char *) cls2Ptr->superclasses.list,
		    sizeof(Class *) * clsPtr->superclasses.num);
	} else {
	    cls2Ptr->superclasses.list = (Class **)
		    ckalloc(sizeof(Class *) * clsPtr->superclasses.num);
	}
	memcpy(cls2Ptr->superclasses.list, clsPtr->superclasses.list,
		sizeof(Class *) * clsPtr->superclasses.num);
	cls2Ptr->superclasses.num = clsPtr->superclasses.num;
	FOREACH(superPtr, cls2Ptr->superclasses) {
	    TclOOAddToSubclasses(cls2Ptr, superPtr);
	}

	/*
	 * Duplicate the source class's filters.
	 */

	DUPLICATE(cls2Ptr->filters, clsPtr->filters, Tcl_Obj *);
	FOREACH(filterObj, cls2Ptr->filters) {
	    Tcl_IncrRefCount(filterObj);
	}

	/*
	 * Duplicate the source class's mixins (which cannot be circular
	 * references to the duplicate).
	 */

	FOREACH(mixinPtr, cls2Ptr->mixins) {
	    TclOORemoveFromMixinSubs(cls2Ptr, mixinPtr);
	}
	if (cls2Ptr->mixins.num != 0) {
	    ckfree((char *) clsPtr->mixins.list);
	}
	DUPLICATE(cls2Ptr->mixins, clsPtr->mixins, Class *);
	FOREACH(mixinPtr, cls2Ptr->mixins) {
	    TclOOAddToMixinSubs(cls2Ptr, mixinPtr);
	}

	/*
	 * Duplicate the source class's methods, constructor and destructor.
	 */

	FOREACH_HASH(keyPtr, mPtr, &clsPtr->classMethods) {
	    (void) CloneClassMethod(interp, cls2Ptr, mPtr, keyPtr);
	}
	if (clsPtr->constructorPtr) {
	    cls2Ptr->constructorPtr = CloneClassMethod(interp, cls2Ptr,
		    clsPtr->constructorPtr, NULL);
	}
	if (clsPtr->destructorPtr) {
	    cls2Ptr->destructorPtr = CloneClassMethod(interp, cls2Ptr,
		    clsPtr->destructorPtr, NULL);
	}

	/*
	 * Duplicate the class's metadata.
	 */

	if (clsPtr->metadataPtr != NULL) {
	    Tcl_ObjectMetadataType *metadataTypePtr;
	    ClientData value, duplicate;

	    FOREACH_HASH(metadataTypePtr, value, clsPtr->metadataPtr) {
		if (metadataTypePtr->cloneProc == NULL) {
		    continue;
		}
		duplicate = metadataTypePtr->cloneProc(value);
		if (duplicate != NULL) {
		    Tcl_ClassSetMetadata((Tcl_Class) cls2Ptr, metadataTypePtr,
			    duplicate);
		}
	    }
	}
    }

    return (Tcl_Object) o2Ptr;
}

/*
 * ----------------------------------------------------------------------
 *
 * CloneObjectMethod, CloneClassMethod --
 *
 *	Helper functions used for cloning methods. They work identically to
 *	each other, except for the difference between them in how they
 *	register the cloned method on a successful clone.
 *
 * ----------------------------------------------------------------------
 */

static Method *
CloneObjectMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    Method *mPtr,
    Tcl_Obj *namePtr)
{
    if (mPtr->typePtr == NULL) {
	return (Method *) Tcl_NewMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, NULL, NULL);
    } else if (mPtr->typePtr->cloneProc) {
	ClientData newClientData;

	if (mPtr->typePtr->cloneProc(mPtr->clientData,
		&newClientData) != TCL_OK) {
	    return NULL;
	}
	return (Method *) Tcl_NewMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, mPtr->typePtr, newClientData);
    } else {
	return (Method *) Tcl_NewMethod(interp, (Tcl_Object) oPtr, namePtr,
		mPtr->flags & PUBLIC_METHOD, mPtr->typePtr, mPtr->clientData);
    }
}

static Method *
CloneClassMethod(
    Tcl_Interp *interp,
    Class *clsPtr,
    Method *mPtr,
    Tcl_Obj *namePtr)
{
    if (mPtr->typePtr == NULL) {
	return (Method *) Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, NULL, NULL);
    } else if (mPtr->typePtr->cloneProc) {
	ClientData newClientData;

	if (mPtr->typePtr->cloneProc(mPtr->clientData,
		&newClientData) != TCL_OK) {
	    return NULL;
	}
	return (Method *) Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, mPtr->typePtr,
		newClientData);
    } else {
	return (Method *) Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr,
		namePtr, mPtr->flags & PUBLIC_METHOD, mPtr->typePtr,
		mPtr->clientData);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_NewMethod --
 *
 *	Attach a method to an object.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Tcl_NewMethod(
    Tcl_Interp *interp,		/* Unused? */
    Tcl_Object object,		/* The object that has the method attached to
				 * it. */
    Tcl_Obj *nameObj,		/* The name of the method. May be NULL; if so,
				 * up to caller to manage storage (e.g., when
				 * it is a constructor or destructor). */
    int isPublic,		/* Whether this is a public method. */
    const Tcl_MethodType *typePtr,
				/* The type of method this is, which defines
				 * how to invoke, delete and clone the
				 * method. */
    ClientData clientData)	/* Some data associated with the particular
				 * method to be created. */
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
	if (mPtr->typePtr != NULL && mPtr->typePtr->deleteProc != NULL) {
	    mPtr->typePtr->deleteProc(mPtr->clientData);
	}
    }

  populate:
    mPtr->typePtr = typePtr;
    mPtr->clientData = clientData;
    mPtr->flags = 0;
    mPtr->declaringObjectPtr = oPtr;
    mPtr->declaringClassPtr = NULL;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }
    oPtr->epoch++;
    return (Tcl_Method) mPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_NewClassMethod --
 *
 *	Attach a method to a class.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Tcl_NewClassMethod(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Tcl_Class cls,		/* The class to attach the method to. */
    Tcl_Obj *nameObj,		/* The name of the object. May be NULL (e.g.,
				 * for constructors or destructors); if so, up
				 * to caller to manage storage. */
    int isPublic,		/* Whether this is a public method. */
    const Tcl_MethodType *typePtr,
				/* The type of method this is, which defines
				 * how to invoke, delete and clone the
				 * method. */
    ClientData clientData)	/* Some data associated with the particular
				 * method to be created. */
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
    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, (char *)nameObj,&isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	mPtr->namePtr = nameObj;
	Tcl_IncrRefCount(nameObj);
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->typePtr != NULL && mPtr->typePtr->deleteProc != NULL) {
	    mPtr->typePtr->deleteProc(mPtr->clientData);
	}
    }

  populate:
    ((Interp *) interp)->ooFoundation->epoch++;
    mPtr->typePtr = typePtr;
    mPtr->clientData = clientData;
    mPtr->flags = 0;
    mPtr->declaringObjectPtr = NULL;
    mPtr->declaringClassPtr = clsPtr;
    if (isPublic) {
	mPtr->flags |= PUBLIC_METHOD;
    }

    return (Tcl_Method) mPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteMethodStruct --
 *
 *	Function used when deleting a method. Always called indirectly via
 *	Tcl_EventuallyFree().
 *
 * ----------------------------------------------------------------------
 */

static void
DeleteMethodStruct(
    char *buffer)
{
    Method *mPtr = (Method *) buffer;

    if (mPtr->typePtr != NULL && mPtr->typePtr->deleteProc != NULL) {
	mPtr->typePtr->deleteProc(mPtr->clientData);
    }
    if (mPtr->namePtr != NULL) {
	TclDecrRefCount(mPtr->namePtr);
    }

    ckfree(buffer);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODeleteMethod --
 *
 *	How to delete a method.
 *
 * ----------------------------------------------------------------------
 */

void
TclOODeleteMethod(
    Method *mPtr)
{
    if (mPtr != NULL) {
	Tcl_EventuallyFree(mPtr, DeleteMethodStruct);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * DeclareClassMethod --
 *
 *	Helper that makes it cleaner to create very simple methods during
 *	basic system initialization. Not suitable for general use.
 *
 * ----------------------------------------------------------------------
 */

static void
DeclareClassMethod(
    Tcl_Interp *interp,
    Class *clsPtr,		/* Class to attach the method to. */
    const char *name,		/* Name of the method. */
    int isPublic,		/* Whether the method is public. */
    Tcl_MethodCallProc callPtr)
				/* Method implementation function. */
{
    Tcl_Obj *namePtr;

    TclNewStringObj(namePtr, name, strlen(name));
    Tcl_IncrRefCount(namePtr);
    Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr, namePtr, isPublic,
	    &coreMethodType, callPtr);
    TclDecrRefCount(namePtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * SimpleInvoke --
 *
 *	How to invoke a simple method.
 *
 * ----------------------------------------------------------------------
 */

static int
SimpleInvoke(
    ClientData clientData,	/* Pointer to function that implements the
				 * method. */
    Tcl_Interp *interp,
    Tcl_ObjectContext context,	/* The method calling context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    Tcl_MethodCallProc callPtr = clientData;

    return (*callPtr)(NULL, interp, context, objc, objv);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOONewProcMethod --
 *
 *	Create a new procedure-like method for an object.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclOONewProcMethod(
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    Object *oPtr,		/* The object to modify. */
    int isPublic,		/* Whether this is a public method. */
    Tcl_Obj *nameObj,		/* The name of the method, which must not be
				 * NULL. */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which must not be NULL. */
    Tcl_Obj *bodyObj)		/* The body of the method, which must not be
				 * NULL. */
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
    return (Method *) Tcl_NewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &procMethodType, pmPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOONewProcClassMethod --
 *
 *	Create a new procedure-like method for a class.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclOONewProcClassMethod(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Class *clsPtr,		/* The class to modify. */
    int isPublic,		/* Whether this is a public method. */
    Tcl_Obj *nameObj,		/* The name of the method, which may be NULL;
				 * if so, up to caller to manage storage
				 * (e.g., because it is a constructor or
				 * destructor). */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which may be NULL; if so, it is equivalent
				 * to an empty list. */
    Tcl_Obj *bodyObj)		/* The body of the method, which must not be
				 * NULL. */
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
    return (Method *) Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr, nameObj,
	    isPublic, &procMethodType, pmPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * InvokeProcedureMethod --
 *
 *	How to invoke a procedure-like method.
 *
 * ----------------------------------------------------------------------
 */

static int
InvokeProcedureMethod(
    ClientData clientData,	/* Pointer to some per-method context. */
    Tcl_Interp *interp,
    Tcl_ObjectContext context,	/* The method calling context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    CallContext *contextPtr = (CallContext *) context;
    ProcedureMethod *pmPtr = (ProcedureMethod *) clientData;
    int result, flags = FRAME_IS_METHOD, skip = contextPtr->skip;
    CallFrame *framePtr, **framePtrPtr;
    Object *oPtr = contextPtr->oPtr;
    Command cmd;
    const char *namePtr;
    Tcl_Obj *nameObj;

    cmd.nsPtr = (Namespace *) oPtr->namespacePtr;
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
	    pmPtr->procPtr->bodyPtr, (Namespace *) oPtr->namespacePtr,
	    "body of method", namePtr);
    if (result != TCL_OK) {
	return result;
    }

    if (contextPtr->callChain[contextPtr->index].isFilter) {
	flags |= FRAME_IS_FILTER;
    }
    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    oPtr->namespacePtr, flags);
    if (result != TCL_OK) {
	return result;
    }
    framePtr->ooContextPtr = contextPtr;
    framePtr->objc = objc;
    framePtr->objv = objv;	/* ref counts for args are incremented below */
    framePtr->procPtr = pmPtr->procPtr;

    if (contextPtr->flags & OO_UNKNOWN_METHOD) {
	skip--;
    }
    result = TclObjInterpProcCore(interp, framePtr, nameObj, skip);
    if (contextPtr->flags & (CONSTRUCTOR | DESTRUCTOR)) {
	TclDecrRefCount(nameObj);
    }
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteProcedureMethod, CloneProcedureMethod --
 *
 *	How to delete and clone procedure-like methods.
 *
 * ----------------------------------------------------------------------
 */

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

/*
 * ----------------------------------------------------------------------
 *
 * TclOONewForwardMethod --
 *
 *	Create a forwarded method for an object.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclOONewForwardMethod(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    Object *oPtr,		/* The object to attach the method to. */
    int isPublic,		/* Whether the method is public or not. */
    Tcl_Obj *nameObj,		/* The name of the method. */
    Tcl_Obj *prefixObj)		/* List of arguments that form the command
				 * prefix to forward to. */
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
    return (Method *) Tcl_NewMethod(interp, (Tcl_Object) oPtr, nameObj,
	    isPublic, &fwdMethodType, fmPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOONewForwardClassMethod --
 *
 *	Create a new forwarded method for a class.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclOONewForwardClassMethod(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    Class *clsPtr,		/* The class to attach the method to. */
    int isPublic,		/* Whether the method is public or not. */
    Tcl_Obj *nameObj,		/* The name of the method. */
    Tcl_Obj *prefixObj)		/* List of arguments that form the command
				 * prefix to forward to. */
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
    return (Method *) Tcl_NewClassMethod(interp, (Tcl_Class) clsPtr, nameObj,
	    isPublic, &fwdMethodType, fmPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * InvokeForwardMethod --
 *
 *	How to invoke a forwarded method. Works by doing some ensemble-like
 *	command rearranging and then invokes some other Tcl command.
 *
 * ----------------------------------------------------------------------
 */

static int
InvokeForwardMethod(
    ClientData clientData,	/* Pointer to some per-method context. */
    Tcl_Interp *interp,
    Tcl_ObjectContext context,	/* The method calling context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    CallContext *contextPtr = (CallContext *) context;
    ForwardMethod *fmPtr = (ForwardMethod *) clientData;
    Tcl_Obj **argObjs, **prefixObjs;
    int numPrefixes, result, len;

    /*
     * Build the real list of arguments to use. Note that we know that the
     * prefixObj field of the ForwardMethod structure holds a reference to a
     * non-empty list, so there's a whole class of failures ("not a list") we
     * can ignore here.
     */

    Tcl_ListObjGetElements(NULL, fmPtr->prefixObj, &numPrefixes, &prefixObjs);
    argObjs = InitEnsembleRewrite(interp, objc, objv, contextPtr->skip,
	    numPrefixes, prefixObjs, &len);

    result = Tcl_EvalObjv(interp, len, argObjs, TCL_EVAL_INVOKE);
    ckfree((char *) argObjs);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * DeleteForwardMethod, CloneForwardMethod --
 *
 *	How to delete and clone forwarded methods.
 *
 * ----------------------------------------------------------------------
 */

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

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_ClassGetMetadata, Tcl_ClassSetMetadata, Tcl_ObjectGetMetadata,
 * Tcl_ObjectSetMetadata --
 *
 *	Metadata management API. The metadata system allows code in extensions
 *	to attach arbitrary non-NULL pointers to objects and classes without
 *	the different things that might be interested being able to interfere
 *	with each other. Apart from non-NULL-ness, these routines attach no
 *	interpretation to the meaning of the metadata pointers.
 *
 *	The Tcl_*GetMetadata routines get the metadata pointer attached that
 *	has been related with a particular type, or NULL if no metadata
 *	associated with the given type has been attached.
 *
 *	The Tcl_*SetMetadata routines set or delete the metadata pointer that
 *	is related to a particular type. The value associated with the type is
 *	deleted (if present; no-op otherwise) if the value is NULL, and
 *	attached (replacing the previous value, which is deleted if present)
 *	otherwise. This means it is impossible to attach a NULL value for any
 *	metadata type.
 *
 * ----------------------------------------------------------------------
 */

ClientData
Tcl_ClassGetMetadata(
    Tcl_Class clazz,
    const Tcl_ObjectMetadataType *typePtr)
{
    Class *clsPtr = (Class *) clazz;
    Tcl_HashEntry *hPtr;

    /*
     * If there's no metadata store attached, the type in question has
     * definitely not been attached either!
     */

    if (clsPtr->metadataPtr == NULL) {
	return NULL;
    }

    /*
     * There is a metadata store, so look in it for the given type.
     */

    hPtr = Tcl_FindHashEntry(clsPtr->metadataPtr, (char *) typePtr);

    /*
     * Return the metadata value if we found it, otherwise NULL.
     */

    if (hPtr == NULL) {
	return NULL;
    } else {
	return Tcl_GetHashValue(hPtr);
    }
}

void
Tcl_ClassSetMetadata(
    Tcl_Class clazz,
    const Tcl_ObjectMetadataType *typePtr,
    ClientData metadata)
{
    Class *clsPtr = (Class *) clazz;
    Tcl_HashEntry *hPtr;
    int isNew;

    /*
     * Attach the metadata store if not done already.
     */

    if (clsPtr->metadataPtr == NULL) {
	if (metadata == NULL) {
	    return;
	}
	clsPtr->metadataPtr = (Tcl_HashTable*) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(clsPtr->metadataPtr, TCL_ONE_WORD_KEYS);
    }

    /*
     * If the metadata is NULL, we're deleting the metadata for the type.
     */

    if (metadata == NULL) {
	hPtr = Tcl_FindHashEntry(clsPtr->metadataPtr, (char *) typePtr);
	if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	return;
    }

    /*
     * Otherwise we're attaching the metadata. Note that if there was already
     * some metadata attached of this type, we delete that first.
     */

    hPtr = Tcl_CreateHashEntry(clsPtr->metadataPtr, (char *) typePtr, &isNew);
    if (!isNew) {
	typePtr->deleteProc(Tcl_GetHashValue(hPtr));
    }
    Tcl_SetHashValue(hPtr, metadata);
}

ClientData
Tcl_ObjectGetMetadata(
    Tcl_Object object,
    const Tcl_ObjectMetadataType *typePtr)
{
    Object *oPtr = (Object *) object;
    Tcl_HashEntry *hPtr;

    /*
     * If there's no metadata store attached, the type in question has
     * definitely not been attached either!
     */

    if (oPtr->metadataPtr == NULL) {
	return NULL;
    }

    /*
     * There is a metadata store, so look in it for the given type.
     */

    hPtr = Tcl_FindHashEntry(oPtr->metadataPtr, (char *) typePtr);

    /*
     * Return the metadata value if we found it, otherwise NULL.
     */

    if (hPtr == NULL) {
	return NULL;
    } else {
	return Tcl_GetHashValue(hPtr);
    }
}

void
Tcl_ObjectSetMetadata(
    Tcl_Object object,
    const Tcl_ObjectMetadataType *typePtr,
    ClientData metadata)
{
    Object *oPtr = (Object *) object;
    Tcl_HashEntry *hPtr;
    int isNew;

    /*
     * Attach the metadata store if not done already.
     */

    if (oPtr->metadataPtr == NULL) {
	if (metadata == NULL) {
	    return;
	}
	oPtr->metadataPtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(oPtr->metadataPtr, TCL_ONE_WORD_KEYS);
    }

    /*
     * If the metadata is NULL, we're deleting the metadata for the type.
     */

    if (metadata == NULL) {
	hPtr = Tcl_FindHashEntry(oPtr->metadataPtr, (char *) typePtr);
	if (hPtr != NULL) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	return;
    }

    /*
     * Otherwise we're attaching the metadata. Note that if there was already
     * some metadata attached of this type, we delete that first.
     */

    hPtr = Tcl_CreateHashEntry(oPtr->metadataPtr, (char *) typePtr, &isNew);
    if (!isNew) {
	typePtr->deleteProc(Tcl_GetHashValue(hPtr));
    }
    Tcl_SetHashValue(hPtr, metadata);
}

/*
 * ----------------------------------------------------------------------
 *
 * PublicObjectCmd, PrivateObjectCmd, ObjectCmd --
 *
 *	Main entry point for object invokations. The Public* and Private*
 *	wrapper functions are just thin wrappers round the main ObjectCmd
 *	function that does call chain creation, management and invokation.
 *
 * ----------------------------------------------------------------------
 */

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
    Object *oPtr,		/* The object being invoked. */
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    int objc,			/* How many arguments are being passed in. */
    Tcl_Obj *const *objv,	/* The array of arguments. */
    int publicOnly,		/* Whether this is an invokation through the
				 * public or the private command interface. */
    Tcl_HashTable *cachePtr)	/* What call chain cache to use. */
{
    Interp *iPtr = (Interp *) interp;
    CallContext *contextPtr;
    int result;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?");
	return TCL_ERROR;
    }

    contextPtr = TclOOGetCallContext(iPtr->ooFoundation, oPtr, objv[1],
	    (publicOnly ? PUBLIC_METHOD :0) | (oPtr->flags & FILTER_HANDLING),
	    cachePtr);
    if (contextPtr == NULL) {
	Tcl_AppendResult(interp, "impossible to invoke method \"",
		TclGetString(objv[1]),
		"\": no defined method or unknown method", NULL);
	return TCL_ERROR;
    }

    Tcl_Preserve(oPtr);
    result = TclOOInvokeContext(interp, contextPtr, objc, objv);
    if (!(contextPtr->flags & OO_UNKNOWN_METHOD)
	    && !(oPtr->flags & OBJECT_DELETED)) {
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(cachePtr, (char *) objv[1]);
	if (hPtr != NULL && Tcl_GetHashValue(hPtr) == NULL) {
	    Tcl_SetHashValue(hPtr, contextPtr);
	} else {
	    TclOODeleteContext(contextPtr);
	}
    } else {
	TclOODeleteContext(contextPtr);
    }
    Tcl_Release(oPtr);

    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassCreate --
 *
 *	Implementation for oo::class->create method.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassCreate(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);
    Tcl_Object newObject;
    const char *objName;
    int len;

    /*
     * Sanity check; should not be possible to invoke this method on a
     * non-class.
     */

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	Tcl_AppendResult(interp, "object \"", TclGetString(cmdnameObj),
		"\" is not a class", NULL);
	TclDecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }

    /*
     * Check we have the right number of (sensible) arguments.
     */

    if (objc - Tcl_ObjectContextSkippedArgs(context) < 1) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"objectName ?arg ...?");
	return TCL_ERROR;
    }
    objName = Tcl_GetStringFromObj(
	    objv[Tcl_ObjectContextSkippedArgs(context)], &len);
    if (len == 0) {
	Tcl_AppendResult(interp, "object name must not be empty", NULL);
	return TCL_ERROR;
    }

    /*
     * Make the object and return its name.
     */

    newObject = Tcl_NewObjectInstance(interp, (Tcl_Class) oPtr->classPtr,
	    objName, objc, objv, Tcl_ObjectContextSkippedArgs(context)+1);
    if (newObject == NULL) {
	return TCL_ERROR;
    }
    Tcl_GetCommandFullName(interp, Tcl_GetObjectCommand(newObject),
	    Tcl_GetObjResult(interp));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassNew --
 *
 *	Implementation for oo::class->new method.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassNew(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);
    Tcl_Object newObject;

    /*
     * Sanity check; should not be possible to invoke this method on a
     * non-class.
     */

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	Tcl_AppendResult(interp, "object \"", TclGetString(cmdnameObj),
		"\" is not a class", NULL);
	TclDecrRefCount(cmdnameObj);
	return TCL_ERROR;
    }

    /*
     * Make the object and return its name.
     */

    newObject = Tcl_NewObjectInstance(interp, (Tcl_Class) oPtr->classPtr,
	    NULL, objc, objv, Tcl_ObjectContextSkippedArgs(context));
    if (newObject == NULL) {
	return TCL_ERROR;
    }
    Tcl_GetCommandFullName(interp, Tcl_GetObjectCommand(newObject),
	    Tcl_GetObjResult(interp));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectDestroy --
 *
 *	Implementation for oo::object->destroy method.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjectDestroy(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    if (objc != Tcl_ObjectContextSkippedArgs(context)) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }
    Tcl_DeleteCommandFromToken(interp,
	    Tcl_GetObjectCommand(Tcl_ObjectContextObject(context)));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectEval --
 *
 *	Implementation for oo::object->eval method.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjectEval(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    CallContext *contextPtr = (CallContext *) context;
    Tcl_Object object = Tcl_ObjectContextObject(context);
    CallFrame *framePtr, **framePtrPtr;
    Tcl_Obj *objnameObj;
    int result;

    if (objc-1 < Tcl_ObjectContextSkippedArgs(context)) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Make the object's namespace the current namespace and evaluate the
     * command(s).
     */

    /* This is needed to satisfy GCC 3.3's strict aliasing rules */
    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    Tcl_GetObjectNamespace(object), 0);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    framePtr->objc = objc;
    framePtr->objv = objv;	/* Reference counts do not need to be
				 * incremented here. */

    if (contextPtr->flags & PUBLIC_METHOD) {
	TclNewObj(objnameObj);
	Tcl_GetCommandFullName(interp, Tcl_GetObjectCommand(object),
		objnameObj);
    } else {
	TclNewStringObj(objnameObj, "my", 2);
    }
    Tcl_IncrRefCount(objnameObj);

    if (objc == Tcl_ObjectContextSkippedArgs(context)+1) {
	result = Tcl_EvalObjEx(interp,
		objv[Tcl_ObjectContextSkippedArgs(context)], 0);
    } else {
	Tcl_Obj *objPtr;

	/*
	 * More than one argument: concatenate them together with spaces
	 * between, then evaluate the result. Tcl_EvalObjEx will delete the
	 * object when it decrements its refcount after eval'ing it.
	 */

	objPtr = Tcl_ConcatObj(objc-Tcl_ObjectContextSkippedArgs(context),
		objv+Tcl_ObjectContextSkippedArgs(context));
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

/*
 * ----------------------------------------------------------------------
 *
 * ObjectUnknown --
 *
 *	Default unknown method handler method (defined in oo::object). This
 *	just creates a suitable error message.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjectUnknown(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    CallContext *contextPtr = (CallContext *) context;
    Object *oPtr = contextPtr->oPtr;
    const char **methodNames;
    int numMethodNames, i;

    /*
     * Get the list of methods that we want to know about.
     */

    numMethodNames = TclOOGetSortedMethodList(oPtr,
	    contextPtr->flags & PUBLIC_METHOD, &methodNames);

    /*
     * Special message when there are no visible methods at all.
     */

    if (numMethodNames == 0) {
	Tcl_Obj *tmpBuf;

	TclNewObj(tmpBuf);
	Tcl_GetCommandFullName(interp, oPtr->command, tmpBuf);
	Tcl_AppendResult(interp, "object \"", TclGetString(tmpBuf),
		"\" has no visible methods", NULL);
	TclDecrRefCount(tmpBuf);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "unknown method \"",
	    TclGetString(objv[Tcl_ObjectContextSkippedArgs(context)-1]),
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

/*
 * ----------------------------------------------------------------------
 *
 * ObjectLinkVar --
 *
 *	Implementation of oo::object->variable method.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjectLinkVar(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Object object = Tcl_ObjectContextObject(context);
    Namespace *savedNsPtr;
    int i;

    if (objc-Tcl_ObjectContextSkippedArgs(context) < 1) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"varName ?varName ...?");
	return TCL_ERROR;
    }

    /*
     * Do nothing if we are not called from the body of a method. In this
     * respect, we are like the [global] command.
     */

    if (iPtr->varFramePtr == NULL ||
	    !(iPtr->varFramePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	return TCL_OK;
    }

    for (i=Tcl_ObjectContextSkippedArgs(context) ; i<objc ; i++) {
	Var *varPtr, *aryPtr;
	Tcl_Obj **argObjs;
	const char *varName;
	int len;

	/*
	 * Parse to see if we have a single value in the argument (just the
	 * name of a variable to use in both the namespace and local scope) or
	 * a two-argument list (namespace variable name and local variable
	 * name). Other cases are an error.
	 */

	if (Tcl_ListObjGetElements(interp, objv[i], &len, &argObjs)!=TCL_OK) {
	    return TCL_ERROR;
	}
	if (len != 1 && len != 2) {
	    Tcl_AppendResult(interp, "argument must be list "
		    "of one or two variable names", NULL);
	    return TCL_ERROR;
	}

	varName = TclGetString(argObjs[len-1]);
	if (strstr(varName, "::") != NULL) {
	    /*
	     * The local var name must not contain a '::' but the ns name is
	     * OK. Naturally, if they're the same, then the restriction is
	     * applied equally to both.
	     */

	    Tcl_AppendResult(interp, "variable name \"", varName,
		    "\" illegal: must not contain namespace separator", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Switch to the object's namespace for the duration of this call.
	 * Like this, the variable is looked up in the namespace of the
	 * object, and not in the namespace of the caller. Otherwise this
	 * would only work if the caller was a method of the object itself,
	 * which might not be true if the method was exported. This is a bit
	 * of a hack, but the simplest way to do this (pushing a stack frame
	 * would be horribly expensive by comparison). We never have to worry
	 * about the case where we're dealing with the global namespace; we've
	 * already checked that we are inside a method.
	 */

	savedNsPtr = iPtr->varFramePtr->nsPtr;
	iPtr->varFramePtr->nsPtr = (Namespace *)
		Tcl_GetObjectNamespace(object);
	varPtr = TclObjLookupVar(interp, argObjs[0], NULL, TCL_NAMESPACE_ONLY,
		"define", 1, 0, &aryPtr);
	iPtr->varFramePtr->nsPtr = savedNsPtr;

	if (varPtr == NULL || aryPtr != NULL) {
	    /*
	     * Variable cannot be an element in an array. If aryPtr is not
	     * NULL, it is an element, so throw up an error and return.
	     */

	    TclVarErrMsg(interp, TclGetString(argObjs[0]), NULL, "define",
		    "name refers to an element in an array");
	    return TCL_ERROR;
	}

	/*
	 * Arrange for the lifetime of the variable to be correctly managed.
	 * This is copied out of Tcl_VariableObjCmd...
	 */

	if (!TclIsVarNamespaceVar(varPtr)) {
	    TclSetVarNamespaceVar(varPtr);
	    varPtr->refCount++;
	}

	if (TclPtrMakeUpvar(interp, varPtr, varName, 0, -1) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjectVarName --
 *
 *	Implementation of the oo::object->varname method.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjectVarName(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Interp *iPtr = (Interp *) interp;
    Var *varPtr, *aryVar;
    Tcl_Obj *varNamePtr;

    if (Tcl_ObjectContextSkippedArgs(context)+1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"varName");
	return TCL_ERROR;
    }

    /*
     * Switch to the object's namespace for the duration of this call. Like
     * this, the variable is looked up in the namespace of the object, and not
     * in the namespace of the caller. Otherwise this would only work if the
     * caller was a method of the object itself, which might not be true if
     * the method was exported. This is a bit of a hack, but the simplest way
     * to do this (pushing a stack frame would be horribly expensive by
     * comparison, and is only done when we'd otherwise interfere with the
     * global namespace).
     */

    if (iPtr->varFramePtr == NULL) {
	Tcl_CallFrame *dummyFrame;

	TclPushStackFrame(interp, &dummyFrame,
		Tcl_GetObjectNamespace(Tcl_ObjectContextObject(context)),0);
	varPtr = TclObjLookupVar(interp, objv[objc-1], NULL,
		TCL_NAMESPACE_ONLY|TCL_LEAVE_ERR_MSG, "refer to",1,1,&aryVar);
	TclPopStackFrame(interp);
    } else {
	Namespace *savedNsPtr;

	savedNsPtr = iPtr->varFramePtr->nsPtr;
	iPtr->varFramePtr->nsPtr = (Namespace *)
		Tcl_GetObjectNamespace(Tcl_ObjectContextObject(context));
	varPtr = TclObjLookupVar(interp, objv[objc-1], NULL,
		TCL_NAMESPACE_ONLY|TCL_LEAVE_ERR_MSG, "refer to",1,1,&aryVar);
	iPtr->varFramePtr->nsPtr = savedNsPtr;
    }

    if (varPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(varNamePtr);
    Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr, varNamePtr);
    Tcl_SetObjResult(interp, varNamePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * NextObjCmd --
 *
 *	Implementation of the [next] command. Note that this command is only
 *	ever to be used inside the body of a procedure-like method.
 *
 * ----------------------------------------------------------------------
 */

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
    result = TclOOInvokeContext(interp, contextPtr, objc, objv);
    iPtr->varFramePtr = savedFramePtr;

    /*
     * Restore the call chain context index as we've finished the inner invoke
     * and want to operate in the outer context again.
     */

    contextPtr->index = index;
    contextPtr->skip = skip;

    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * SelfObjCmd --
 *
 *	Implementation of the [self] command, which provides introspection of
 *	the call context.
 *
 * ----------------------------------------------------------------------
 */

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
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		contextPtr->oPtr->namespacePtr->fullName,-1));
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
    case SELF_METHOD:
	if (contextPtr->flags & CONSTRUCTOR) {
	    Tcl_AppendResult(interp, "<constructor>", NULL);
	} else if (contextPtr->flags & DESTRUCTOR) {
	    Tcl_AppendResult(interp, "<destructor>", NULL);
	} else {
	    Method *mPtr = contextPtr->callChain[contextPtr->index].mPtr;

	    Tcl_SetObjResult(interp, mPtr->namePtr);
	}
	return TCL_OK;
    case SELF_FILTER:
	if (!contextPtr->callChain[contextPtr->index].isFilter) {
	    Tcl_AppendResult(interp, "not inside a filtering context", NULL);
	    return TCL_ERROR;
	} else {
	    struct MInvoke *miPtr = &contextPtr->callChain[contextPtr->index];
	    Tcl_Obj *result[3];
	    Object *oPtr;
	    const char *type;

	    if (miPtr->filterDeclarer != NULL) {
		oPtr = miPtr->filterDeclarer->thisPtr;
		type = "class";
	    } else {
		oPtr = contextPtr->oPtr;
		type = "object";
	    }

	    TclNewObj(result[0]);
	    Tcl_GetCommandFullName(interp, oPtr->command, result[0]);
	    result[1] = Tcl_NewStringObj(type, -1);
	    result[2] = miPtr->mPtr->namePtr;
	    Tcl_SetObjResult(interp, Tcl_NewListObj(3, result));
	    return TCL_OK;
	}
    case SELF_CALLER:
	if ((framePtr->callerVarPtr != NULL) &&
		(framePtr->callerVarPtr->isProcCallFrame & FRAME_IS_METHOD)) {
	    CallContext *callerPtr = framePtr->callerVarPtr->ooContextPtr;
	    Method *mPtr = callerPtr->callChain[callerPtr->index].mPtr;
	    Object *declarerPtr;
	    Tcl_Obj *tmpObj;

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

	    TclNewObj(tmpObj);
	    Tcl_GetCommandFullName(interp, declarerPtr->command, tmpObj);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
	    TclNewObj(tmpObj);
	    Tcl_GetCommandFullName(interp, callerPtr->oPtr->command, tmpObj);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
	    if (callerPtr->flags & CONSTRUCTOR) {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			Tcl_NewStringObj("<constructor>", -1));
	    } else if (callerPtr->flags & DESTRUCTOR) {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			Tcl_NewStringObj("<destructor>", -1));
	    } else {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			mPtr->namePtr);
	    }
	    return TCL_OK;
	} else {
	    Tcl_AppendResult(interp, "caller is not an object", NULL);
	    return TCL_ERROR;
	}
    case SELF_NEXT:
	if (contextPtr->index < contextPtr->numCallChain-1) {
	    Method *mPtr = contextPtr->callChain[contextPtr->index+1].mPtr;
	    Object *declarerPtr;
	    Tcl_Obj *tmpObj;

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

	    TclNewObj(tmpObj);
	    Tcl_GetCommandFullName(interp, declarerPtr->command, tmpObj);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
	    if (contextPtr->flags & CONSTRUCTOR) {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			Tcl_NewStringObj("<constructor>", -1));
	    } else if (contextPtr->flags & DESTRUCTOR) {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			Tcl_NewStringObj("<destructor>", -1));
	    } else {
		Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
			mPtr->namePtr);
	    }
	}
	return TCL_OK;
    case SELF_TARGET:
	if (!contextPtr->callChain[contextPtr->index].isFilter) {
	    Tcl_AppendResult(interp, "not inside a filtering context", NULL);
	    return TCL_ERROR;
	} else {
	    Method *mPtr;
	    Object *declarerPtr;
	    Tcl_Obj *cmdName;
	    int i;

	    for (i=contextPtr->index ; i<contextPtr->numCallChain ; i++) {
		if (!contextPtr->callChain[i].isFilter) {
		    break;
		}
	    }
	    if (i == contextPtr->numCallChain) {
		Tcl_Panic("filtering call chain without terminal non-filter");
	    }
	    mPtr = contextPtr->callChain[i].mPtr;
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
	    TclNewObj(cmdName);
	    Tcl_GetCommandFullName(interp, declarerPtr->command, cmdName);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), cmdName);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
		    mPtr->namePtr);
	    return TCL_OK;
	}
    }
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * CopyObjectCmd --
 *
 *	Implementation of the [oo::copy] command, which clones an object (but
 *	not its namespace). Note that no constructors are called during this
 *	process.
 *
 * ----------------------------------------------------------------------
 */

static int
CopyObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Tcl_Object oPtr, o2Ptr;

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "sourceName ?targetName?");
	return TCL_ERROR;
    }

    oPtr = Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Create a cloned object of the correct class. Note that constructors are
     * not called. Also note that we must resolve the object name ourselves
     * because we do not want to create the object in the current namespace,
     * but rather in the context of the namespace of the caller of the overall
     * [oo::define] command.
     */

    if (objc == 2) {
	o2Ptr = Tcl_CopyObjectInstance(interp, oPtr, NULL);
    } else {
	char *name;
	Tcl_DString buffer;

	name = TclGetString(objv[2]);
	Tcl_DStringInit(&buffer);
	if (name[0]!=':' || name[1]!=':') {
	    Interp *iPtr = (Interp *) interp;

	    if (iPtr->varFramePtr != NULL) {
		Tcl_DStringAppend(&buffer,
			iPtr->varFramePtr->nsPtr->fullName, -1);
	    }
	    Tcl_DStringAppend(&buffer, "::", 2);
	    Tcl_DStringAppend(&buffer, name, -1);
	    name = Tcl_DStringValue(&buffer);
	}
	o2Ptr = Tcl_CopyObjectInstance(interp, oPtr, name);
	Tcl_DStringFree(&buffer);
    }

    if (o2Ptr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Return the name of the cloned object.
     */

    Tcl_GetCommandFullName(interp, Tcl_GetObjectCommand(o2Ptr),
	    Tcl_GetObjResult(interp));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_GetObjectFromObj --
 *
 *	Utility function to get an object from a Tcl_Obj containing its name.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
Tcl_GetObjectFromObj(
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
 * ----------------------------------------------------------------------
 *
 * TclOOIsReachable --
 *
 *	Utility function that tests whether a class is a subclass (whether
 *	directly or indirectly) of another class.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOIsReachable(
    Class *targetPtr,
    Class *startPtr)
{
    int i;
    Class *superPtr;

  tailRecurse:
    if (startPtr == targetPtr) {
	return 1;
    }
    if (startPtr->superclasses.num == 1) {
	startPtr = startPtr->superclasses.list[0];
	goto tailRecurse;
    }
    FOREACH(superPtr, startPtr->superclasses) {
	if (TclOOIsReachable(targetPtr, superPtr)) {
	    return 1;
	}
    }
    return 0;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOGetProcFromMethod, TclOOGetFwdFromMethod --
 *
 *	Utility functions used for procedure-like and forwarding method
 *	introspection.
 *
 * ----------------------------------------------------------------------
 */

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

Tcl_Obj *
TclOOGetFwdFromMethod(
    Method *mPtr)
{
    if (mPtr->typePtr == &fwdMethodType) {
	ForwardMethod *fwPtr = mPtr->clientData;

	return fwPtr->prefixObj;
    }
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * InitEnsembleRewrite --
 *
 *	Utility function that wraps up a lot of the complexity involved in
 *	doing ensemble-like command forwarding. Here is a picture of memory
 *	management plan:
 *
 *                    <-----------------objc---------------------->
 *      objv:        |=============|===============================|
 *                    <-toRewrite->           |
 *                                             \
 *                    <-rewriteLength->         \
 *      rewriteObjs: |=================|         \
 *                           |                    |
 *                           V                    V
 *      argObjs:     |=================|===============================|
 *                    <------------------*lengthPtr------------------->
 *
 * ----------------------------------------------------------------------
 */

static Tcl_Obj **
InitEnsembleRewrite(
    Tcl_Interp *interp,		/* Place to log the rewrite info. */
    int objc,			/* Number of real arguments. */
    Tcl_Obj *const *objv,	/* The real arguments. */
    int toRewrite,		/* Number of real arguments to replace. */
    int rewriteLength,		/* Number of arguments to insert instead. */
    Tcl_Obj *const *rewriteObjs,/* Arguments to insert instead. */
    int *lengthPtr)		/* Where to write the resulting length of the
				 * array of rewritten arguments. */
{
    Interp *iPtr = (Interp *) interp;
    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);
    Tcl_Obj **argObjs;
    unsigned len = rewriteLength + objc - toRewrite;

    argObjs = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * len);
    memcpy(argObjs, rewriteObjs, rewriteLength * sizeof(Tcl_Obj *));
    memcpy(argObjs + rewriteLength, objv + toRewrite,
	    sizeof(Tcl_Obj *) * (objc - toRewrite));

    /*
     * Now plumb this into the core ensemble rewrite logging system so that
     * Tcl_WrongNumArgs() can rewrite its result appropriately. The rules for
     * how to store the rewrite rules get complex solely because of the case
     * where an ensemble rewrites itself out of the picture; when that
     * happens, the quality of the error message rewrite falls drastically
     * (and unavoidably).
     */

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = toRewrite;
	iPtr->ensembleRewrite.numInsertedObjs = rewriteLength;
    } else {
	int numIns = iPtr->ensembleRewrite.numInsertedObjs;

	if (numIns < toRewrite) {
	    iPtr->ensembleRewrite.numRemovedObjs += toRewrite - numIns;
	    iPtr->ensembleRewrite.numInsertedObjs += rewriteLength - 1;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs +=
		    rewriteLength - toRewrite;
	}
    }

    *lengthPtr = len;
    return argObjs;
}

Tcl_Method
Tcl_ObjectContextMethod(
    Tcl_ObjectContext context)
{
    CallContext *contextPtr = (CallContext *) context;
    return (Tcl_Method) contextPtr->callChain[contextPtr->index].mPtr;
}

int
Tcl_ObjectContextIsFiltering(
    Tcl_ObjectContext context)
{
    CallContext *contextPtr = (CallContext *) context;
    return contextPtr->callChain[contextPtr->index].isFilter;
}

Tcl_Object
Tcl_ObjectContextObject(
    Tcl_ObjectContext context)
{
    return (Tcl_Object) ((CallContext *)context)->oPtr;
}

int
Tcl_ObjectContextSkippedArgs(
    Tcl_ObjectContext context)
{
    return ((CallContext *)context)->skip;
}

Tcl_Object
Tcl_MethodDeclarerObject(
    Tcl_Method method)
{
    return (Tcl_Object) ((Method *) method)->declaringObjectPtr;
}

Tcl_Class
Tcl_MethodDeclarerClass(
    Tcl_Method method)
{
    return (Tcl_Class) ((Method *) method)->declaringClassPtr;
}

Tcl_Obj *
Tcl_MethodName(
    Tcl_Method method)
{
    return ((Method *) method)->namePtr;
}

int
Tcl_MethodIsType(
    Tcl_Method method,
    const Tcl_MethodType *typePtr,
    ClientData *clientDataPtr)
{
    Method *mPtr = (Method *) method;

    if (mPtr->typePtr == typePtr) {
	if (clientDataPtr != NULL) {
	    *clientDataPtr = mPtr->clientData;
	}
	return 1;
    }
    return 0;
}

int
Tcl_MethodIsPublic(
    Tcl_Method method)
{
    return (((Method *)method)->flags & PUBLIC_METHOD) ? 1 : 0;
}

Tcl_Namespace *
Tcl_GetObjectNamespace(
    Tcl_Object object)
{
    return ((Object *)object)->namespacePtr;
}

Tcl_Command
Tcl_GetObjectCommand(
    Tcl_Object object)
{
    return ((Object *)object)->command;
}

Tcl_Class
Tcl_GetObjectAsClass(
    Tcl_Object object)
{
    return (Tcl_Class) ((Object *)object)->classPtr;
}

int
Tcl_ObjectDeleted(
    Tcl_Object object)
{
    return (((Object *)object)->flags & OBJECT_DELETED) ? 1 : 0;
}

Tcl_Object
Tcl_GetClassAsObject(
    Tcl_Class clazz)
{
    return (Tcl_Object) ((Class *)clazz)->thisPtr;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
