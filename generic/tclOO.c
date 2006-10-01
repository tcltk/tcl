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
 * RCS: @(#) $Id: tclOO.c,v 1.1.2.53 2006/10/01 21:27:24 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"
#include <assert.h>

/*
 * Commands in oo::define.
 */

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
#ifdef SUPPORT_OO_CLASS_MIXINS
    {"mixin", TclOODefineMixinObjCmd, 0},
    {"self.mixin", TclOODefineMixinObjCmd, 1},
#else
    {"mixin", TclOODefineMixinObjCmd, 1},
#endif
#ifdef SUPPORT_OO_PARAMETERS
    {"parameter", TclOODefineParameterObjCmd, 0},
#endif
    {"superclass", TclOODefineSuperclassObjCmd, 0},
    {"unexport", TclOODefineUnexportObjCmd, 0},
    {"self.unexport", TclOODefineUnexportObjCmd, 1},
    {"self.class", TclOODefineSelfClassObjCmd, 1},
    {NULL, NULL, 0}
};

/*
 * Forwarded methods in oo::struct.
 */

struct StructCmdInfo {
    const char *cmdName;
    int varIdx;
    const char *extraPrefix;
    const char *extraInsert;
    int flags;
};
static struct StructCmdInfo structCmds[] = {
    {"append",	0, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {"array",	1, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {"exists",	0, "info",NULL,	      0},
    {"incr",	0, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {"lappend", 0, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {"set",	0, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {"trace",	1, NULL,  "variable", TCL_LEAVE_ERR_MSG},
    {"unset",  -1, NULL,  NULL,	      TCL_LEAVE_ERR_MSG},
    {NULL}
};

/*
 * What sort of size of things we like to allocate.
 */

#define ALLOC_CHUNK 8

/*
 * Extra flags used for call chain management.
 */

#define DEFINITE_PRIVATE 0x100000
#define DEFINITE_PUBLIC  0x200000
#define KNOWN_STATE	 (DEFINITE_PRIVATE | DEFINITE_PUBLIC)

/*
 * Function declarations for things defined in this file.
 */

static Class *		AllocClass(Tcl_Interp *interp, Object *useThisObj);
static Object *		AllocObject(Tcl_Interp *interp, const char *nameStr);
static void		DeclareClassMethod(Tcl_Interp *interp, Class *clsPtr,
			    const char *name, int isPublic,
			    Tcl_MethodCallProc callProc);
static void		AddClassFiltersToCallContext(Object *oPtr,
			    Class *clsPtr, CallContext *contextPtr,
			    Tcl_HashTable *doneFilters);
static void		AddClassMethodNames(Class *clsPtr, int publicOnly,
			    Tcl_HashTable *namesPtr);
static void		AddMethodToCallChain(Method *mPtr,
			    CallContext *contextPtr,
			    Tcl_HashTable *doneFilters);
static void		AddSimpleChainToCallContext(Object *oPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    Tcl_HashTable *doneFilters, int isPublic);
static void		AddSimpleClassChainToCallContext(Class *classPtr,
			    Tcl_Obj *methodNameObj, CallContext *contextPtr,
			    Tcl_HashTable *doneFilters, int isPublic);
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
static void		ObjectDeletedTrace(ClientData clientData,
			    Tcl_Interp *interp, const char *oldName,
			    const char *newName, int flags);
static void		ReleaseClassContents(Tcl_Interp *interp,Object *oPtr);

static int		PublicObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);
static int		PrivateObjectCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const *objv);

static int		SimpleInvoke(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		StructInvoke(ClientData clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    int objc, Tcl_Obj *const *objv);
static int		SimpleClone(ClientData clientData,
			    ClientData *newClientData);
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
			    int insertIndex, Tcl_Obj *insertObj,
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
static int		StructVar(ClientData clientData, Tcl_Interp *interp,
			    Tcl_ObjectContext context, int objc,
			    Tcl_Obj *const *objv);
static int		StructVwait(ClientData clientData, Tcl_Interp *interp,
			    Tcl_ObjectContext context, int objc,
			    Tcl_Obj *const *objv);
static char *		StructVwaitVarProc(ClientData clientData,
			    Tcl_Interp *interp, const char *name1,
			    const char *name2, int flags);

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
    SimpleInvoke, NULL, SimpleClone
};
static const Tcl_MethodType structMethodType = {
    TCL_OO_METHOD_VERSION_CURRENT, "core forward method",
    StructInvoke, NULL, SimpleClone
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

	argsPtr = Tcl_NewStringObj("{definitionScript {}}", -1);
	bodyPtr = Tcl_NewStringObj(
		"set c [self];set d [namespace origin define];foreach cmd {"
		"constructor destructor export forward "
		"method parameter superclass unexport "
		"} {define $c self.forward $cmd $d $c $cmd}\n"
		"next $definitionScript", -1);
	fPtr->definerCls->constructorPtr = TclNewProcClassMethod(interp,
		fPtr->definerCls, 0, NULL, argsPtr, bodyPtr);
	argsPtr = Tcl_NewStringObj("className {definitionScript {}}", -1);
	bodyPtr = Tcl_NewStringObj("uplevel 1 [list "
		"[self] create $className $definitionScript]", -1);
	TclNewProcMethod(interp, fPtr->definerCls->thisPtr, 0,
		fPtr->unknownMethodNameObj, argsPtr, bodyPtr);
	TclNewProcClassMethod(interp, fPtr->definerCls, 0,
		fPtr->unknownMethodNameObj, argsPtr, bodyPtr);
    }

    /*
     * Build the 'struct' class, which is useful for building "structures".
     * Structures are objects that expose their internal state variables.
     */

    fPtr->structCls = AllocClass(interp, AllocObject(interp, "::oo::struct"));
    for (i=0 ; structCmds[i].cmdName!=NULL ; i++) {
	Tcl_Obj *namePtr = Tcl_NewStringObj(structCmds[i].cmdName, -1);

	Tcl_NewClassMethod(interp, (Tcl_Class) fPtr->structCls, namePtr, 1,
		&structMethodType, &structCmds[i]);
    }
    Tcl_NewClassMethod(interp, (Tcl_Class) fPtr->structCls,
	    Tcl_NewStringObj("eval", 4), 1, NULL, NULL);
    DeclareClassMethod(interp, fPtr->structCls, "var", 0, StructVar);
    DeclareClassMethod(interp, fPtr->structCls, "vwait", 1, StructVwait);

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
    Class *clsPtr, **subs;
    Object **insts;

    clsPtr = oPtr->classPtr;
    Tcl_Preserve(clsPtr);

    /*
     * Must empty list now so that things happen in the correct order.
     */

    subs = clsPtr->subclasses.list;
    n = clsPtr->subclasses.num;
    clsPtr->subclasses.list = NULL;
    clsPtr->subclasses.num = 0;
    clsPtr->subclasses.size = 0;
    for (i=0 ; i<n ; i++) {
	Tcl_Preserve(subs[i]);
    }
    for (i=0 ; i<n ; i++) {
	if (!(subs[i]->flags & OBJECT_DELETED) && interp != NULL) {
	    Tcl_DeleteCommandFromToken(interp, subs[i]->thisPtr->command);
	}
	Tcl_Release(subs[i]);
    }
    if (subs != NULL) {
	ckfree((char *) subs);
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
	TclDeleteMethod(mPtr);
    }
    Tcl_DeleteHashTable(&oPtr->methods);
    FOREACH_HASH_VALUE(contextPtr, &oPtr->publicContextCache) {
	if (contextPtr) {
	    DeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->publicContextCache);
    FOREACH_HASH_VALUE(contextPtr, &oPtr->privateContextCache) {
	if (contextPtr) {
	    DeleteContext(contextPtr);
	}
    }
    Tcl_DeleteHashTable(&oPtr->privateContextCache);

    clsPtr = oPtr->classPtr;
    if (clsPtr != NULL && !(oPtr->flags & ROOT_OBJECT)) {
	Class *superPtr;

	clsPtr->flags |= OBJECT_DELETED;
	FOREACH(superPtr, clsPtr->superclasses) {
	    if (!(superPtr->flags & OBJECT_DELETED)) {
		TclOORemoveFromSubclasses(clsPtr, superPtr);
	    }
	}
	clsPtr->superclasses.num = 0;
	ckfree((char *) clsPtr->superclasses.list);
	clsPtr->subclasses.num = 0;
	ckfree((char *) clsPtr->subclasses.list);
	clsPtr->instances.num = 0;
	ckfree((char *) clsPtr->instances.list);

	FOREACH_HASH_VALUE(mPtr, &clsPtr->classMethods) {
	    TclDeleteMethod(mPtr);
	}
	Tcl_DeleteHashTable(&clsPtr->classMethods);
	TclDeleteMethod(clsPtr->constructorPtr);
	TclDeleteMethod(clsPtr->destructorPtr);
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
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    clsPtr->constructorPtr = NULL;
    clsPtr->destructorPtr = NULL;
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
	 * to.
	 */

	AllocClass(interp, oPtr);
	oPtr->selfCls = (Class *) cls; // Repatch
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

    return (Tcl_Object) oPtr;
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
 * TclDeleteMethod --
 *
 *	How to delete a method.
 *
 * ----------------------------------------------------------------------
 */

void
TclDeleteMethod(
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
 * SimpleInvoke, SimpleClone --
 *
 *	How to invoke and clone a simple method.
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

static int
SimpleClone(
    ClientData clientData,	/* Pointer to function that implements the
				 * method. */
    ClientData *newClientData)	/* Place to copy the pointer to. */
{
    *newClientData = clientData;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * StructInvoke --
 *
 *	How to invoke the special sort of forwarded methods used by the
 *	oo::struct class. In many ways, this is a complicated and nasty hack.
 *
 * ----------------------------------------------------------------------
 */

static int
StructInvoke(
    ClientData clientData,	/* Pointer to some per-method context. */
    Tcl_Interp *interp,
    Tcl_ObjectContext context,	/* The method calling context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    CallContext *contextPtr = (CallContext *) context;
    struct StructCmdInfo *infoPtr = clientData;
    Tcl_CallFrame *dummyFrame;
    Var *dummyAryVar;
    int result, len;
    Tcl_Obj **argObjs;

    /*
     * Set the object's namespace as current context.
     */

    TclPushStackFrame(interp, &dummyFrame, contextPtr->oPtr->namespacePtr, 0);

    /*
     * Ensure that the variables exist properly (even if in an undefined
     * state) before we do the call to the underlying code. This is required
     * if we are to enforce the specification that variables are to be always
     * interpreted as variables in the namespace of the class. However, we
     * only do this if we can fail; for the case where we can't - currently
     * just targetting [info exists] - we skip this.
     */

    if (!(infoPtr->flags & TCL_LEAVE_ERR_MSG)) {
	goto doForwardEnsemble;
    }

    if (infoPtr->varIdx < 0) {
	int i;

	for (i=contextPtr->skip ; i<objc ; i++) {
	    if (TclObjLookupVar(interp, objv[i], NULL,
		    TCL_NAMESPACE_ONLY|infoPtr->flags, "refer to", 1, 0,
		    &dummyAryVar)==NULL) {
		TclPopStackFrame(interp);
		return TCL_ERROR;
	    }
	}
    } else if (infoPtr->varIdx+contextPtr->skip >= objc) {
	Tcl_Obj *prefixObj = Tcl_NewStringObj(infoPtr->cmdName, -1);

	argObjs = InitEnsembleRewrite(interp, objc, objv, contextPtr->skip, 1,
		&prefixObj, 0, NULL, &len);
	goto doInvoke;
    } else if (TclObjLookupVar(interp, objv[infoPtr->varIdx+contextPtr->skip],
	    NULL, TCL_NAMESPACE_ONLY|infoPtr->flags, "refer to", 1, 0,
	    &dummyAryVar) == NULL) {
	TclPopStackFrame(interp);
	return TCL_ERROR;
    }

    /*
     * Construct the forward to the command within the object's namespace.
     */

  doForwardEnsemble:
    if (infoPtr->extraPrefix) {
	Tcl_Obj *prefix[2];

	prefix[1] = Tcl_NewStringObj(infoPtr->cmdName, -1);
	prefix[0] = Tcl_NewStringObj(infoPtr->extraPrefix, -1);
	argObjs = InitEnsembleRewrite(interp, objc, objv, contextPtr->skip, 2,
		prefix, 0, NULL, &len);
    } else {
	Tcl_Obj *prefixObj = Tcl_NewStringObj(infoPtr->cmdName, -1);
	Tcl_Obj *insertObj = (infoPtr->extraInsert ?
		Tcl_NewStringObj(infoPtr->extraInsert, -1) : NULL);

	argObjs = InitEnsembleRewrite(interp, objc, objv, contextPtr->skip, 1,
		&prefixObj, 2, insertObj, &len);
    }

    /*
     * Now we have constructed the arguments to use, pass through to the
     * command we are forwarding to.
     */

  doInvoke:
    result = Tcl_EvalObjv(interp, len, argObjs, TCL_EVAL_INVOKE);
    ckfree((char *) argObjs);

    /*
     * We're done now; drop the context namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclNewProcMethod --
 *
 *	Create a new procedure-like method for an object.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclNewProcMethod(
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
 * TclNewProcClassMethod --
 *
 *	Create a new procedure-like method for a class.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclNewProcClassMethod(
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
 * TclNewForwardMethod --
 *
 *	Create a forwarded method for an object.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclNewForwardMethod(
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
 * TclNewForwardClassMethod --
 *
 *	Create a new forwarded method for a class.
 *
 * ----------------------------------------------------------------------
 */

Method *
TclNewForwardClassMethod(
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
	    numPrefixes, prefixObjs, -1, NULL, &len);

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

/*
 * ----------------------------------------------------------------------
 *
 * DeleteContext --
 *
 *	Destroys a method call-chain context, which should not be in use.
 *
 * ----------------------------------------------------------------------
 */

static void
DeleteContext(
    CallContext *contextPtr)
{
    if (contextPtr->callChain != contextPtr->staticCallChain) {
	ckfree((char *) contextPtr->callChain);
    }
    ckfree((char *) contextPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * InvokeContext --
 *
 *	Invokes a single step along a method call-chain context. Note that the
 *	invokation of a step along the chain can cause further steps along the
 *	chain to be invoked. Note that this function is written to be as light
 *	in stack usage as possible.
 *
 * ----------------------------------------------------------------------
 */

static int
InvokeContext(
    Tcl_Interp *interp,		/* Interpreter for error reporting, and many
				 * other sorts of context handling (e.g.,
				 * commands, variables) depending on method
				 * implementation. */
    CallContext *contextPtr,	/* The method call context. */
    int objc,			/* The number of arguments. */
    Tcl_Obj *const *objv)	/* The arguments as actually seen. */
{
    Method *mPtr = contextPtr->callChain[contextPtr->index].mPtr;
    int result, isFirst = (contextPtr->index == 0);

    /*
     * If this is the first step along the chain, we preserve the method
     * entries in the chain so that they do not get deleted out from under our
     * feet.
     */

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Preserve(contextPtr->callChain[i].mPtr);
	}
    }

    result = mPtr->typePtr->callProc(mPtr->clientData, interp,
	    (Tcl_ObjectContext) contextPtr, objc, objv);

    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Release(contextPtr->callChain[i].mPtr);
	}
    }
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetSortedMethodList --
 *
 *	Discovers the list of method names supported by an object.
 *
 * ----------------------------------------------------------------------
 */

static int
GetSortedMethodList(
    Object *oPtr,		/* The object to get the method names for. */
    int publicOnly,		/* Whether we just want the public method
				 * names. */
    const char ***stringsPtr)	/* Where to write a pointer to the array of
				 * strings to. */
{
    Tcl_HashTable names;
    FOREACH_HASH_DECLS;
    int i;
    const char **strings;
    Class *mixinPtr;
    Tcl_Obj *namePtr;
    Method *mPtr;
    void *isWanted;

    Tcl_InitObjHashTable(&names);

    FOREACH_HASH(namePtr, mPtr, &oPtr->methods) {
	int isNew;

	hPtr = Tcl_CreateHashEntry(&names, (char *) namePtr, &isNew);
	if (isNew) {
	    isWanted = (void *) (!publicOnly || mPtr->flags & PUBLIC_METHOD);
	    Tcl_SetHashValue(hPtr, isWanted);
	}
    }

    AddClassMethodNames(oPtr->selfCls, publicOnly, &names);
    FOREACH(mixinPtr, oPtr->mixins) {
	AddClassMethodNames(mixinPtr, publicOnly, &names);
    }

    if (names.numEntries == 0) {
	Tcl_DeleteHashTable(&names);
	return 0;
    }

    strings = (const char **) ckalloc(sizeof(char *) * names.numEntries);
    i = 0;
    FOREACH_HASH(namePtr, isWanted, &names) {
	if (!publicOnly || isWanted) {
	    strings[i++] = TclGetString(namePtr);
	}
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

/* Comparator for GetSortedMethodList */
static int
CmpStr(
    const void *ptr1,
    const void *ptr2)
{
    const char **strPtr1 = (const char **) ptr1;
    const char **strPtr2 = (const char **) ptr2;

    return TclpUtfNcmp2(*strPtr1, *strPtr2, strlen(*strPtr1)+1);
}

/*
 * ----------------------------------------------------------------------
 *
 * AddClassMethodNames --
 *
 *	Adds the method names defined by a class (or its superclasses) to the
 *	collection being built. The collection is built in a hash table to
 *	ensure that duplicates are excluded. Helper for GetSortedMethodList().
 *
 * ----------------------------------------------------------------------
 */

static void
AddClassMethodNames(
    Class *clsPtr,		/* Class to get method names from. */
    const int publicOnly,	/* Whether we are interested in just the
				 * public method names. */
    Tcl_HashTable *const namesPtr)
				/* Reference to the hash table to put the
				 * information in. The hash table maps the
				 * Tcl_Obj * method name to an integral value
				 * describing whether the method is wanted.
				 * This ensures that public/private override
				 * semantics are handled correctly.*/
{
    /*
     * Scope these declarations so that the compiler can stand a good chance
     * of making the recursive step highly efficient. We also hand-implement
     * the tail-recursive case using a while loop; C compilers typically
     * cannot do tail-recursion optimization usefully.
     */

    while (1) {
	FOREACH_HASH_DECLS;
	Tcl_Obj *namePtr;
	Method *mPtr;

	FOREACH_HASH(namePtr, mPtr, &clsPtr->classMethods) {
	    int isNew;

	    hPtr = Tcl_CreateHashEntry(namesPtr, (char *) namePtr, &isNew);
	    if (isNew) {
		int isWanted = (!publicOnly || mPtr->flags & PUBLIC_METHOD);

		Tcl_SetHashValue(hPtr, (void *) isWanted);
	    }
	}

	if (clsPtr->superclasses.num != 1) {
	    break;
	}
	clsPtr = clsPtr->superclasses.list[0];
    }
    if (clsPtr->superclasses.num != 0) {
	Class *superPtr;
	int i;

	FOREACH(superPtr, clsPtr->superclasses) {
	    AddClassMethodNames(superPtr, publicOnly, namesPtr);
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
    Foundation *fPtr,		/* The foundation of the object system. */
    Object *oPtr,		/* The object to get the context for. */
    Tcl_Obj *methodNameObj,	/* The name of the method to get the context
				 * for. NULL when getting a constructor or
				 * destructor chain. */
    int flags,			/* What sort of context are we looking for.
				 * Only the bits OO_PUBLIC_METHOD, CONSTRUCTOR
				 * and DESTRUCTOR are useful. */
    Tcl_HashTable *cachePtr)	/* Where to cache the chain. Ignored for both
				 * constructors and destructors. */
{
    CallContext *contextPtr;
    int i, count, doFilters = 0;
    Tcl_HashEntry *hPtr;
    Tcl_HashTable doneFilters;

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

    /*
     * Add object filters if any are defined.
     */

    if (!(flags & (CONSTRUCTOR | DESTRUCTOR))) {
	Tcl_Obj *filterObj;
	Class *mixinPtr;

	doFilters = 1;
	Tcl_InitObjHashTable(&doneFilters);
	FOREACH(filterObj, oPtr->filters) {
	    AddSimpleChainToCallContext(oPtr, filterObj, contextPtr,
		    &doneFilters, 0);
	}
	FOREACH(mixinPtr, oPtr->mixins) {
	    AddClassFiltersToCallContext(oPtr, mixinPtr, contextPtr,
		    &doneFilters);
	}
    }
    if (doFilters) {
	// TODO: Move later in this function. Doing this requires other code
	// to be reorganized though (especially for distinguishing between a
	// filtering and a non-filtering context) so it is trickier than it
	// looks.
	AddClassFiltersToCallContext(oPtr, oPtr->selfCls, contextPtr,
		&doneFilters);
	Tcl_DeleteHashTable(&doneFilters);
    }
    count = contextPtr->filterLength = contextPtr->numCallChain;

    /*
     * Add the actual method implementation.
     */

    // Class filter handling has to go in here, between the object/mixin
    // method processing and the class method processing. What a mess. :-(
    AddSimpleChainToCallContext(oPtr, methodNameObj, contextPtr, NULL, flags);

    /*
     * Check to see if the method has no implementation. If so, we probably
     * need to add in a call to the unknown method. Otherwise, set up the
     * cacheing of the method implementation (if relevant).
     */

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
		contextPtr, NULL, 0);
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

/*
 * ----------------------------------------------------------------------
 *
 * AddClassFiltersToCallContext --
 *
 *	Logic to make extracting all the filters from the class context much
 *	easier.
 *
 * ----------------------------------------------------------------------
 */

static void
AddClassFiltersToCallContext(
    Object *const oPtr,		/* Object that the filters operate on. */
    Class *clsPtr,		/* Class to get the filters from. */
    CallContext *const contextPtr,
				/* Context to fill with call chain entries. */
    Tcl_HashTable *const doneFilters)
				/* Where to record what filters have been
				 * processed. Keys are objects, values are
				 * ignored. */
{
    int i;
    Class *superPtr;
    Tcl_Obj *filterObj;

  tailRecurse:
    if (clsPtr == NULL) {
	return;
    }

    /*
     * Add all the class filters from the current class. Note that the filters
     * are added starting at the object root, as this allows the object to
     * override how filters work to extend their behaviour.
     */

    FOREACH(filterObj, clsPtr->filters) {
	int isNew;

	(void) Tcl_CreateHashEntry(doneFilters, (char *) filterObj, &isNew);
	if (isNew) {
	    AddSimpleChainToCallContext(oPtr, filterObj, contextPtr,
		    doneFilters, 0);
	}
    }

    /*
     * Now process the recursive case. Notice the tail-call optimization.
     */

    switch (clsPtr->superclasses.num) {
    case 1:
	clsPtr = clsPtr->superclasses.list[0];
	goto tailRecurse;
    default:
	FOREACH(superPtr, clsPtr->superclasses) {
	    AddClassFiltersToCallContext(oPtr, superPtr, contextPtr,
		    doneFilters);
	}
    case 0:
	return;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * AddSimpleChainToCallContext --
 *
 *	The core of the call-chain construction engine, this handles calling a
 *	particular method on a particular object. Note that filters and
 *	unknown handling are already handled by the logic that uses this
 *	function.
 *
 * ----------------------------------------------------------------------
 */

static void
AddSimpleChainToCallContext(
    Object *oPtr,		/* Object to add call chain entries for. */
    Tcl_Obj *methodNameObj,	/* Name of method to add the call chain
				 * entries for. */
    CallContext *contextPtr,	/* Where to add the call chain entries. */
    Tcl_HashTable *doneFilters,	/* Where to record what call chain entries
				 * have been processed. */
    int flags)			/* What sort of call chain are we building. */
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
	Class *mixinPtr;

	hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *) methodNameObj);
	if (hPtr != NULL) {
	    AddMethodToCallChain(Tcl_GetHashValue(hPtr), contextPtr,
		    doneFilters);
	}
	FOREACH(mixinPtr, oPtr->mixins) {
	    AddSimpleClassChainToCallContext(mixinPtr, methodNameObj,
		    contextPtr, doneFilters, flags);
	}
    }
    AddSimpleClassChainToCallContext(oPtr->selfCls, methodNameObj, contextPtr,
	    doneFilters, flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * AddSimpleClassChainToCallContext --
 *
 *	Construct a call-chain from a class hierarchy.
 *
 * ----------------------------------------------------------------------
 */

static void
AddSimpleClassChainToCallContext(
    Class *classPtr,		/* Class to add the call chain entries for. */
    Tcl_Obj *const methodNameObj,
				/* Name of method to add the call chain
				 * entries for. */
    CallContext *const contextPtr,
				/* Where to add the call chain entries. */
    Tcl_HashTable *const doneFilters,
				/* Where to record what call chain entries
				 * have been processed. */
    int flags)			/* What sort of call chain are we building. */
{
    /*
     * We hard-code the tail-recursive form. It's by far the most common case
     * *and* it is much more gentle on the stack.
     */

  tailRecurse:
    if (flags & CONSTRUCTOR) {
	AddMethodToCallChain(classPtr->constructorPtr, contextPtr,
		doneFilters);
    } else if (flags & DESTRUCTOR) {
	AddMethodToCallChain(classPtr->destructorPtr, contextPtr,
		doneFilters);
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
	    AddMethodToCallChain(mPtr, contextPtr, doneFilters);
	}
    }

    switch (classPtr->superclasses.num) {
    case 1:
	classPtr = classPtr->superclasses.list[0];
	goto tailRecurse;
    default:
    {
	int i;
	Class *superPtr;

	FOREACH(superPtr, classPtr->superclasses) {
	    AddSimpleClassChainToCallContext(superPtr, methodNameObj,
		    contextPtr, doneFilters, flags);
	}
    }
    case 0:
	return;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * AddMethodToCallChain --
 *
 *	Utility method that manages the adding of a particular method
 *	implementation to a call-chain.
 *
 * ----------------------------------------------------------------------
 */

static void
AddMethodToCallChain(
    Method *mPtr,		/* Actual method implementation to add to call
				 * chain (or NULL, a no-op). */
    CallContext *contextPtr,	/* The call chain to add the method
				 * implementation to. */
    Tcl_HashTable *doneFilters)	/* Where to record what filters have been
				 * processed. If NULL, not processing filters.
				 * Note that this function does not update
				 * this hashtable. */
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
		&& contextPtr->callChain[i].isFilter == (doneFilters!=NULL)) {
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
	    contextPtr->callChain[i].isFilter = (doneFilters != NULL);
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
		ckalloc(sizeof(struct MInvoke)*(contextPtr->numCallChain+1));
	memcpy(contextPtr->callChain, contextPtr->staticCallChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    } else if (contextPtr->numCallChain > CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke *)
		ckrealloc((char *) contextPtr->callChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    }
    contextPtr->callChain[contextPtr->numCallChain].mPtr = mPtr;
    contextPtr->callChain[contextPtr->numCallChain].isFilter =
	    (doneFilters != NULL);
    contextPtr->numCallChain++;
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

    numMethodNames = GetSortedMethodList(oPtr,
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
 *	Implementation of oo::object->var method.
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
 * StructVar --
 *
 *	Implementation of the oo::struct->variable method.
 *
 * ----------------------------------------------------------------------
 */

static int
StructVar(
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
 * StructVwait --
 *
 *	Implementation of the oo::struct->vwait command.
 *
 * ----------------------------------------------------------------------
 */

static int
StructVwait(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Tcl_CallFrame *dummyFrame;
    int done, foundEvent;
    Tcl_Namespace *objectNsPtr;

    if (Tcl_ObjectContextSkippedArgs(context)+1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"varName");
	return TCL_ERROR;
    }
    objectNsPtr = Tcl_GetObjectNamespace(Tcl_ObjectContextObject(context));

    /*
     * Set up the trace. Note that, unlike the normal [vwait] implementation,
     * this code locates the variable within the pushed namespace context. We
     * only keep the namespace context on the Tcl stack for as short a time as
     * possible. (Modifying the calling context like this is a horrible hack,
     * but fast. We only push a context if we are in the global scope.)
     */

    TclPushStackFrame(interp, &dummyFrame, objectNsPtr, 0);
    if (Tcl_TraceVar(interp, TclGetString(objv[objc-1]),
	    TCL_NAMESPACE_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
	    StructVwaitVarProc, &done) != TCL_OK) {
	TclPopStackFrame(interp);
	return TCL_ERROR;
    }
    TclPopStackFrame(interp);

    /*
     * Run an event loop until one of:
     * 1) our trace is triggered by a write or unset of the variable,
     * 2) there are no further event handlers (a blocking case), or
     * 3) a limit has triggered.
     */

    done = 0;
    foundEvent = 1;
    while (!done && foundEvent) {
	foundEvent = Tcl_DoOneEvent(TCL_ALL_EVENTS);
	if (Tcl_LimitExceeded(interp)) {
	    break;
	}
    }

    /*
     * Clear out the trace if the namespace isn't also going away; if the
     * namespace is doomed, things are going to be complicated to unpick if we
     * try to do it here, the trace will be cleaned out anyway and we're done.
     */

    if (!(((Namespace *) objectNsPtr)->flags & (NS_DYING|NS_KILLED))) {
	TclPushStackFrame(interp, &dummyFrame, objectNsPtr, 0);
	Tcl_UntraceVar(interp, TclGetString(objv[objc-1]),
		TCL_NAMESPACE_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		StructVwaitVarProc, &done);
	TclPopStackFrame(interp);
    }

    /*
     * Now produce error messages (if any) and return.
     */

    Tcl_ResetResult(interp);
    if (!foundEvent) {
	Tcl_AppendResult(interp, "can't wait for variable \"",
		TclGetString(objv[objc-1]), "\": would wait forever", NULL);
	return TCL_ERROR;
    }
    if (!done) {
	Tcl_AppendResult(interp, "limit exceeded", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

	/* ARGSUSED */
static char *
StructVwaitVarProc(
    ClientData clientData,	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp,		/* Interpreter containing variable. */
    const char *name1,		/* Name of variable. */
    const char *name2,		/* Second part of variable name. */
    int flags)			/* Information about what happened. */
{
    int *donePtr = clientData;

    *donePtr = 1;
    return NULL;
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
    result = InvokeContext(interp, contextPtr, objc, objv);
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
	    Method *mPtr =
		    contextPtr->callChain[contextPtr->filterLength].mPtr;
	    Tcl_Obj *cmdName;

	    // TODO: should indicate who has the filter registration, not the
	    // first non-filter after the filter!
	    TclNewObj(cmdName);
	    Tcl_GetCommandFullName(interp, contextPtr->oPtr->command,
		    cmdName);
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), cmdName);
	    // TODO: Add what type of filter this is
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
		    mPtr->namePtr);
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
 * TclOOGetProcFromMethod --
 *
 *	Utility function used for procedure-like method introspection.
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

/*
 * ----------------------------------------------------------------------
 *
 * InitEnsembleRewrite --
 *
 *	Utility function that wraps up a lot of the complexity involved in
 *	doing ensemble-like command forwarding. There are many baroque bits
 *	attached to this function so that it can support much of the rewriting
 *	used in the oo::struct implementation; perhaps it should be tidied up
 *	some day instead.
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
    int insertIndex,		/* Where to insert a magical "extra" arg. */
    Tcl_Obj *insertObj,		/* Extra arg to insert, or NULL to not do
				 * it. */
    int *lengthPtr)		/* Where to write the resulting length of the
				 * array of rewritten arguments. */
{
    Interp *iPtr = (Interp *) interp;
    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);
    Tcl_Obj **argObjs;
    unsigned len = rewriteLength + objc - toRewrite;
    int stepRem = 0, stepIns = 0;

    if (insertObj) {
	len++;
	stepRem = insertIndex - rewriteLength;
	stepIns = stepRem + 1;
    }

    /*
     * Picture of memory management so that I can see what is going on where
     * so the copies of chunks of arrays will work. Note that 'transfer' is a
     * derived value.
     *
     *               <-----------------objc----------------------->
     * objv:        |=============|============|===================|
     *               <-toRewrite-> <-transfer->         \
     *                                    \              \
     *               <-rewriteLength->     \              \
     * rewriteObjs: |=================|     \              \
     *                      |                |              |
     *                      V                V              V
     * argObjs:     |=================|============||===================|
     *               <---------insertIndex-------->
     *               <-------------------*lengthPtr-------------------->
     *
     * The insertIndex is ignored if insertObj is NULL; that case is much
     * simpler and looks like this:
     *
     *               <-----------------objc---------------------->
     * objv:        |=============|===============================|
     *               <-toRewrite->           |
     *                                        \
     *               <-rewriteLength->         \
     * rewriteObjs: |=================|         \
     *                      |                    |
     *                      V                    V
     * argObjs:     |=================|===============================|
     *               <------------------*lengthPtr------------------->
     */

    argObjs = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * len);
    memcpy(argObjs, rewriteObjs, rewriteLength * sizeof(Tcl_Obj *));
    if (insertObj) {
	int transfer = insertIndex - rewriteLength;

	memcpy(argObjs + rewriteLength, objv + toRewrite,
		sizeof(Tcl_Obj *) * transfer);
	argObjs[insertIndex] = insertObj;
	memcpy(argObjs + insertIndex + 1, objv + toRewrite + transfer,
		sizeof(Tcl_Obj *) * (objc - toRewrite - transfer));
    } else {
	memcpy(argObjs + rewriteLength, objv + toRewrite,
		sizeof(Tcl_Obj *) * (objc - toRewrite));
    }

    /*
     * Now plumb this into the core ensemble logging system so that
     * Tcl_WrongNumArgs() can rewrite its result appropriately.
     */

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = toRewrite + stepRem;
	iPtr->ensembleRewrite.numInsertedObjs = rewriteLength + stepIns;
    } else {
	int ni = iPtr->ensembleRewrite.numInsertedObjs;
	if (ni < toRewrite) {
	    iPtr->ensembleRewrite.numRemovedObjs += toRewrite + stepRem - ni;
	    iPtr->ensembleRewrite.numInsertedObjs +=
		    rewriteLength + stepIns - 1;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs +=
		    rewriteLength + stepIns - toRewrite;
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
