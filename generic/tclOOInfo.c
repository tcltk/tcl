/*
 * tclOODefineCmds.c --
 *
 *	This file contains the implementation of the ::oo-related [info]
 *	subcommands.
 *
 * Copyright © 2006-2019 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tclInt.h"
#include "tclOOInt.h"

static inline Class *	GetClassFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		SortPropList(Tcl_Obj *list);
static Tcl_ObjCmdProc InfoObjectCallCmd;
static Tcl_ObjCmdProc InfoObjectClassCmd;
static Tcl_ObjCmdProc InfoObjectDefnCmd;
static Tcl_ObjCmdProc InfoObjectFiltersCmd;
static Tcl_ObjCmdProc InfoObjectForwardCmd;
static Tcl_ObjCmdProc InfoObjectIdCmd;
static Tcl_ObjCmdProc InfoObjectIsACmd;
static Tcl_ObjCmdProc InfoObjectMethodsCmd;
static Tcl_ObjCmdProc InfoObjectMethodTypeCmd;
static Tcl_ObjCmdProc InfoObjectMixinsCmd;
static Tcl_ObjCmdProc InfoObjectNsCmd;
static Tcl_ObjCmdProc InfoObjectPropCmd;
static Tcl_ObjCmdProc InfoObjectVarsCmd;
static Tcl_ObjCmdProc InfoObjectVariablesCmd;
static Tcl_ObjCmdProc InfoClassCallCmd;
static Tcl_ObjCmdProc InfoClassConstrCmd;
static Tcl_ObjCmdProc InfoClassDefnCmd;
static Tcl_ObjCmdProc InfoClassDefnNsCmd;
static Tcl_ObjCmdProc InfoClassDestrCmd;
static Tcl_ObjCmdProc InfoClassFiltersCmd;
static Tcl_ObjCmdProc InfoClassForwardCmd;
static Tcl_ObjCmdProc InfoClassInstancesCmd;
static Tcl_ObjCmdProc InfoClassMethodsCmd;
static Tcl_ObjCmdProc InfoClassMethodTypeCmd;
static Tcl_ObjCmdProc InfoClassMixinsCmd;
static Tcl_ObjCmdProc InfoClassPropCmd;
static Tcl_ObjCmdProc InfoClassSubsCmd;
static Tcl_ObjCmdProc InfoClassSupersCmd;
static Tcl_ObjCmdProc InfoClassVariablesCmd;

/*
 * List of commands that are used to implement the [info object] subcommands.
 */

static const EnsembleImplMap infoObjectCmds[] = {
    {"call",	   InfoObjectCallCmd,	    TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"class",	   InfoObjectClassCmd,	    TclCompileInfoObjectClassCmd, NULL, NULL, 0},
    {"creationid", InfoObjectIdCmd,	    TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"definition", InfoObjectDefnCmd,	    TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"filters",	   InfoObjectFiltersCmd,    TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"forward",	   InfoObjectForwardCmd,    TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"isa",	   InfoObjectIsACmd,	    TclCompileInfoObjectIsACmd, NULL, NULL, 0},
    {"methods",	   InfoObjectMethodsCmd,    TclCompileBasicMin1ArgCmd, NULL, NULL, 0},
    {"methodtype", InfoObjectMethodTypeCmd, TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"mixins",	   InfoObjectMixinsCmd,	    TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"namespace",  InfoObjectNsCmd,	    TclCompileInfoObjectNamespaceCmd, NULL, NULL, 0},
    {"properties", InfoObjectPropCmd,	    TclCompileBasicMin1ArgCmd, NULL, NULL, 0},
    {"variables",  InfoObjectVariablesCmd,  TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {"vars",	   InfoObjectVarsCmd,	    TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {NULL, NULL, NULL, NULL, NULL, 0}
};

/*
 * List of commands that are used to implement the [info class] subcommands.
 */

static const EnsembleImplMap infoClassCmds[] = {
    {"call",	     InfoClassCallCmd,		TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"constructor",  InfoClassConstrCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"definition",   InfoClassDefnCmd,		TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"definitionnamespace", InfoClassDefnNsCmd,	TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {"destructor",   InfoClassDestrCmd,		TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"filters",	     InfoClassFiltersCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"forward",	     InfoClassForwardCmd,	TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"instances",    InfoClassInstancesCmd,	TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {"methods",	     InfoClassMethodsCmd,	TclCompileBasicMin1ArgCmd, NULL, NULL, 0},
    {"methodtype",   InfoClassMethodTypeCmd,	TclCompileBasic2ArgCmd, NULL, NULL, 0},
    {"mixins",	     InfoClassMixinsCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"properties",   InfoClassPropCmd,		TclCompileBasicMin1ArgCmd, NULL, NULL, 0},
    {"subclasses",   InfoClassSubsCmd,		TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {"superclasses", InfoClassSupersCmd,	TclCompileBasic1ArgCmd, NULL, NULL, 0},
    {"variables",    InfoClassVariablesCmd,	TclCompileBasic1Or2ArgCmd, NULL, NULL, 0},
    {NULL, NULL, NULL, NULL, NULL, 0}
};

/*
 * ----------------------------------------------------------------------
 *
 * TclOOInitInfo --
 *
 *	Adjusts the Tcl core [info] command to contain subcommands ("object"
 *	and "class") for introspection of objects and classes.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOInitInfo(
    Tcl_Interp *interp)
{
    Tcl_Command infoCmd;
    Tcl_Obj *mapDict;

    /*
     * Build the ensembles used to implement [info object] and [info class].
     */

    TclMakeEnsemble(interp, "::oo::InfoObject", infoObjectCmds);
    TclMakeEnsemble(interp, "::oo::InfoClass", infoClassCmds);

    /*
     * Install into the [info] ensemble.
     */

    infoCmd = Tcl_FindCommand(interp, "info", NULL, TCL_GLOBAL_ONLY);
    if (infoCmd) {
	Tcl_GetEnsembleMappingDict(NULL, infoCmd, &mapDict);
	Tcl_DictObjPut(NULL, mapDict,
		Tcl_NewStringObj("object", TCL_AUTO_LENGTH),
		Tcl_NewStringObj("::oo::InfoObject", TCL_AUTO_LENGTH));
	Tcl_DictObjPut(NULL, mapDict,
		Tcl_NewStringObj("class", TCL_AUTO_LENGTH),
		Tcl_NewStringObj("::oo::InfoClass", TCL_AUTO_LENGTH));
	Tcl_SetEnsembleMappingDict(interp, infoCmd, mapDict);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * GetClassFromObj --
 *
 *	How to correctly get a class from a Tcl_Obj. Just a wrapper round
 *	Tcl_GetObjectFromObj, but this is an idiom that was used heavily.
 *
 * ----------------------------------------------------------------------
 */
static inline Class *
GetClassFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr)
{
    Object *oPtr = (Object *) Tcl_GetObjectFromObj(interp, objPtr);

    if (oPtr == NULL) {
	return NULL;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"\"%s\" is not a class", TclGetString(objPtr)));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		TclGetString(objPtr), (char *)NULL);
	return NULL;
    }
    return oPtr->classPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetClassMethodFromObj --
 *
 *	Helper for looking up a class-defined method.
 *
 * ----------------------------------------------------------------------
 */
static inline Method *
GetClassMethodFromObj(
    Tcl_Interp *interp,
    Class *clsPtr,
    Tcl_Obj *methodName)
{
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&clsPtr->classMethods, methodName);
    Method *mPtr;

    if (hPtr == NULL) {
	goto unknownMethod;
    }
    mPtr = (Method *)Tcl_GetHashValue(hPtr);
    if (mPtr->typePtr == NULL) {
	/*
	 * Special entry for visibility control: pretend the method doesnt
	 * exist.
	 */

	goto unknownMethod;
    }
    return mPtr;

  unknownMethod:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "unknown method \"%s\"", TclGetString(methodName)));
    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
	    TclGetString(methodName), (char *)NULL);
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetInstanceMethodFromObj --
 *
 *	Helper for looking up an object-instance-defined method.
 *
 * ----------------------------------------------------------------------
 */
static inline Method *
GetInstanceMethodFromObj(
    Tcl_Interp *interp,
    Object *oPtr,
    Tcl_Obj *methodName)
{
    Tcl_HashEntry *hPtr;
    Method *mPtr;

    if (!oPtr->methodsPtr) {
	goto unknownMethod;
    }
    hPtr = Tcl_FindHashEntry(oPtr->methodsPtr, methodName);
    if (hPtr == NULL) {
	goto unknownMethod;
    }
    mPtr = (Method *)Tcl_GetHashValue(hPtr);
    if (mPtr->typePtr == NULL) {
	/*
	 * Special entry for visibility control: pretend the method doesnt
	 * exist.
	 */

	goto unknownMethod;
    }
    return mPtr;

  unknownMethod:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "unknown method \"%s\"", TclGetString(methodName)));
    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
	    TclGetString(methodName), (char *)NULL);
    return NULL;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetProcFromMethod --
 *
 *	Helper for looking up a procedure-like method's definition details.
 *
 * ----------------------------------------------------------------------
 */
static inline Proc *
GetProcFromMethod(
    Tcl_Interp *interp,
    Method *mPtr)
{
    Proc *procPtr = TclOOGetProcFromMethod(mPtr);
    if (procPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"definition not available for this kind of method",
		TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "OO", "METHOD_TYPE", (char *)NULL);
	return NULL;
    }
    return procPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetForwardFromMethod --
 *
 *	Helper for looking up a forwarded method's definition details.
 *
 * ----------------------------------------------------------------------
 */
static inline Tcl_Obj *
GetForwardFromMethod(
    Tcl_Interp *interp,
    Method *mPtr)
{
    Tcl_Obj *prefixObj = TclOOGetFwdFromMethod(mPtr);
    if (prefixObj == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"prefix argument list not available for this kind of method",
		TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "OO", "METHOD_TYPE", (char *)NULL);
	return NULL;
    }
    return prefixObj;
}


/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectClassCmd --
 *
 *	Implements [info object class $objName ?$className?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectClassCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?className?");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    if (objc == 2) {
	Tcl_SetObjResult(interp,
		TclOOObjectName(interp, oPtr->selfCls->thisPtr));
	return TCL_OK;
    } else {
	Class *mixinPtr, *o2clsPtr;
	Tcl_Size i;

	o2clsPtr = GetClassFromObj(interp, objv[2]);
	if (o2clsPtr == NULL) {
	    return TCL_ERROR;
	}

	FOREACH(mixinPtr, oPtr->mixins) {
	    if (!mixinPtr) {
		continue;
	    }
	    if (TclOOIsReachable(o2clsPtr, mixinPtr)) {
		Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
		return TCL_OK;
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		TclOOIsReachable(o2clsPtr, oPtr->selfCls)));
	return TCL_OK;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectDefnCmd --
 *
 *	Implements [info object definition $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectDefnCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Method *mPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];
    Tcl_Obj *methodName;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }
    methodName = objv[2];

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetInstanceMethodFromObj(interp, oPtr, methodName);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    procPtr = GetProcFromMethod(interp, mPtr);
    if (procPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObjs[0]);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, TCL_AUTO_LENGTH));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(mPtr);
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectFiltersCmd --
 *
 *	Implements [info object filters $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectFiltersCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Size i;
    Tcl_Obj *filterObj, *resultObj;
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    TclNewObj(resultObj);

    FOREACH(filterObj, oPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectForwardCmd --
 *
 *	Implements [info object forward $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectForwardCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Method *mPtr;
    Tcl_Obj *prefixObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetInstanceMethodFromObj(interp, oPtr, objv[2]);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    prefixObj = GetForwardFromMethod(interp, mPtr);
    if (prefixObj == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectIsACmd --
 *
 *	Implements [info object isa $category $objName ...]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectIsACmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *const categories[] = {
	"class", "metaclass", "mixin", "object", "typeof", NULL
    };
    enum IsACats {
	IsClass, IsMetaclass, IsMixin, IsObject, IsType
    } idx;
    Object *oPtr, *o2Ptr;
    int result = 0;
    Tcl_Size i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "category objName ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], categories, "category", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Now we know what test we are doing, we can check we've got the right
     * number of arguments.
     */

    switch (idx) {
    case IsObject:
    case IsClass:
    case IsMetaclass:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName");
	    return TCL_ERROR;
	}
	break;
    case IsMixin:
    case IsType:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName className");
	    return TCL_ERROR;
	}
	break;
    }

    /*
     * Perform the check. Note that we can guarantee that we will not fail
     * from here on; "failures" result in a false-TCL_OK result.
     */

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
    if (oPtr == NULL) {
	goto failPrecondition;
    }

    switch (idx) {
    case IsObject:
	result = 1;
	break;
    case IsClass:
	result = (oPtr->classPtr != NULL);
	break;
    case IsMetaclass:
	if (oPtr->classPtr != NULL) {
	    result = TclOOIsReachable(TclOOGetFoundation(interp)->classCls,
		    oPtr->classPtr);
	}
	break;
    case IsMixin:
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    goto failPrecondition;
	}
	if (o2Ptr->classPtr != NULL) {
	    Class *mixinPtr;

	    FOREACH(mixinPtr, oPtr->mixins) {
		if (!mixinPtr) {
		    continue;
		}
		if (TclOOIsReachable(o2Ptr->classPtr, mixinPtr)) {
		    result = 1;
		    break;
		}
	    }
	}
	break;
    case IsType:
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    goto failPrecondition;
	}
	if (o2Ptr->classPtr != NULL) {
	    result = TclOOIsReachable(o2Ptr->classPtr, oPtr->selfCls);
	}
	break;
    }
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(result));
    return TCL_OK;

  failPrecondition:
    Tcl_ResetResult(interp);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
    return TCL_OK;
}

/*
 * Scopes as passed via the -scope option to [info class methods] and [info
 * object methods].
 */
enum Scopes {
    SCOPE_PRIVATE,		/* User said -scope private */
    SCOPE_PUBLIC,		/* User said -scope public */
    SCOPE_UNEXPORTED,		/* User said -scope unexported */
#ifdef TCLOO_SUPPORT_LOCALPRIVATE_SCOPE_IN_INFO_METHODS
    SCOPE_LOCALPRIVATE,		/* User said -scope localprivate (not enabled) */
#endif
    SCOPE_NONE = -1		/* No scope set. Use old semantics. */
};

/*
 * ----------------------------------------------------------------------
 *
 * ParseMethodsArgs --
 *
 *	Parse the options to [info class methods] and [info object methods]. 
 *
 * ----------------------------------------------------------------------
 */
static inline int
ParseMethodsArgs(
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[],
    int *recursePtr,
    int *flagPtr,
    int *scopePtr)
{
    static const char *const scopes[] = {
	"private", "public", "unexported"
#ifdef TCLOO_SUPPORT_LOCALPRIVATE_SCOPE_IN_INFO_METHODS
	, "localprivate"
#endif
    };
    static const char *const options[] = {
	"-all", "-localprivate", "-private", "-scope", NULL
    };
    enum Options {
	OPT_ALL, OPT_LOCALPRIVATE, OPT_PRIVATE, OPT_SCOPE
    } idx;
    int i;

    /*
     * Set the defaults.
     */
    *flagPtr = PUBLIC_METHOD, *recursePtr = 0, *scopePtr = SCOPE_NONE;
    if (objc == 2) {
	return TCL_OK;
    }

    /*
     * Parse the options.
     */
    for (i=2 ; i<objc ; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		&idx) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (idx) {
	case OPT_ALL:
	    *recursePtr = 1;
	    break;
	case OPT_LOCALPRIVATE:
	    *flagPtr = PRIVATE_METHOD;
	    break;
	case OPT_PRIVATE:
	    *flagPtr = 0;
	    break;
	case OPT_SCOPE:
	    if (++i >= objc) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"missing option for -scope"));
		Tcl_SetErrorCode(interp, "TCL", "ARGUMENT", "MISSING",
			(char *)NULL);
		return TCL_ERROR;
	    }
	    if (*scopePtr != SCOPE_NONE) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"-scope option provided twice"));
		Tcl_SetErrorCode(interp, "TCL", "ARGUMENT", "DOUBLED",
			(char *)NULL);
		return TCL_ERROR;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[i], scopes, "scope", 0,
		    scopePtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	}
    }

    /*
     * Handle interlocking between features: -scope disables much.
     */
    if (*scopePtr != SCOPE_NONE) {
	*recursePtr = 0;
	switch (*scopePtr) {
	case SCOPE_PRIVATE:
	    *flagPtr = TRUE_PRIVATE_METHOD;
	    break;
	case SCOPE_PUBLIC:
	    *flagPtr = PUBLIC_METHOD;
	    break;
#ifdef TCLOO_SUPPORT_LOCALPRIVATE_SCOPE_IN_INFO_METHODS
	case SCOPE_LOCALPRIVATE:
	    *flagPtr = PRIVATE_METHOD;
	    break;
#endif
	case SCOPE_UNEXPORTED:
	    *flagPtr = 0;
	    break;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ListLocalMatchingMethods --
 *
 *	Get the method names out of a hash table of methods where the methods
 *	match the given filter options. Used by [info class methods] and
 *	[info object methods] when not recursing through the inheritance
 *	hierarchy.
 *
 * ----------------------------------------------------------------------
 */
static inline Tcl_Obj *
ListLocalMatchingMethods(
    Tcl_HashTable *methodsPtr,	/* Hash table of methods. */
    int scope,			/* Sought scope. */
    int flag)			/* Sought flag. */
{
    FOREACH_HASH_DECLS;
    Tcl_Obj *namePtr, *resultObj;
    Method *mPtr;

    /*
     * Can't pre-allocate the list buffer; not sure how many elements there
     * are going to be yet.
     */
    TclNewObj(resultObj);
    if (scope == SCOPE_NONE) {
	/*
	 * Handle legacy-mode matching. [Bug 36e5517a6850]
	 */
	int scopeFilter = flag | TRUE_PRIVATE_METHOD;

	FOREACH_HASH(namePtr, mPtr, methodsPtr) {
	    if (mPtr->typePtr && (mPtr->flags & scopeFilter) == flag) {
		Tcl_ListObjAppendElement(NULL, resultObj, namePtr);
	    }
	}
    } else {
	FOREACH_HASH(namePtr, mPtr, methodsPtr) {
	    if (mPtr->typePtr && (mPtr->flags & SCOPE_FLAGS) == flag) {
		Tcl_ListObjAppendElement(NULL, resultObj, namePtr);
	    }
	}
    }
    return resultObj;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectMethodsCmd --
 *
 *	Implements [info object methods $objName ?$option ...?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectMethodsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    int flag, recurse, scope;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?-option value ...?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (ParseMethodsArgs(interp, objc, objv,
	    &recurse, &flag, &scope) != TCL_OK) {
	return TCL_ERROR;
    }

    if (recurse) {
	Tcl_Obj **names;
	int numNames = TclOOGetSortedMethodList(oPtr, NULL, NULL, flag,
		&names);

	if (numNames > 0) {
	    Tcl_SetObjResult(interp, Tcl_NewListObj(numNames, names));
	    Tcl_Free((void *) names);
	}
    } else if (oPtr->methodsPtr) {
	Tcl_SetObjResult(interp,
		ListLocalMatchingMethods(oPtr->methodsPtr, scope, flag));
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectMethodTypeCmd --
 *
 *	Implements [info object methodtype $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectMethodTypeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Method *mPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetInstanceMethodFromObj(interp, oPtr, objv[2]);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    mPtr->typePtr->name, TCL_AUTO_LENGTH));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectMixinsCmd --
 *
 *	Implements [info object mixins $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectMixinsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *mixinPtr;
    Object *oPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(mixinPtr, oPtr->mixins) {
	if (!mixinPtr) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, mixinPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectIdCmd --
 *
 *	Implements [info object creationid $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectIdCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(oPtr->creationEpoch));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectNsCmd --
 *
 *	Implements [info object namespace $objName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectNsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj(oPtr->namespacePtr->fullName, TCL_AUTO_LENGTH));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectVariablesCmd --
 *
 *	Implements [info object variables $objName ?-private?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectVariablesCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;
    int isPrivate = 0;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?-private?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (strcmp("-private", TclGetString(objv[2])) != 0) {
	    return TCL_ERROR;
	}
	isPrivate = 1;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    if (isPrivate) {
	PrivateVariableMapping *privatePtr;

	FOREACH_STRUCT(privatePtr, oPtr->privateVariables) {
	    Tcl_ListObjAppendElement(NULL, resultObj, privatePtr->variableObj);
	}
    } else {
	Tcl_Obj *variableObj;

	FOREACH(variableObj, oPtr->variables) {
	    Tcl_ListObjAppendElement(NULL, resultObj, variableObj);
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectVarsCmd --
 *
 *	Implements [info object vars $objName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectVarsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    const char *pattern = NULL;
    FOREACH_HASH_DECLS;
    VarInHash *vihPtr;
    Tcl_Obj *nameObj, *resultObj;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?pattern?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }
    TclNewObj(resultObj);

    /*
     * Extract the information we need from the object's namespace's table of
     * variables. Note that this involves horrific knowledge of the guts of
     * tclVar.c, so we can't leverage our hash-iteration macros properly.
     */

    FOREACH_HASH_VALUE(vihPtr,
	    &((Namespace *) oPtr->namespacePtr)->varTable.table) {
	nameObj = vihPtr->entry.key.objPtr;

	if (TclIsVarUndefined(&vihPtr->var)
		|| !TclIsVarNamespaceVar(&vihPtr->var)) {
	    continue;
	}
	if (pattern != NULL
		&& !Tcl_StringMatch(TclGetString(nameObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, nameObj);
    }

    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassConstrCmd --
 *
 *	Implements [info class constructor $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassConstrCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (clsPtr->constructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = GetProcFromMethod(interp, clsPtr->constructorPtr);
    if (procPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObjs[0]);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, TCL_AUTO_LENGTH));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(clsPtr->constructorPtr);
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassDefnCmd --
 *
 *	Implements [info class definition $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassDefnCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Method *mPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *resultObjs[2];
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetClassMethodFromObj(interp, clsPtr, objv[2]);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    procPtr = GetProcFromMethod(interp, mPtr);
    if (procPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObjs[0]);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, TCL_AUTO_LENGTH));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, resultObjs[0], argObj);
	}
    }
    resultObjs[1] = TclOOGetMethodBody(mPtr);
    Tcl_SetObjResult(interp, Tcl_NewListObj(2, resultObjs));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassDefnNsCmd --
 *
 *	Implements [info class definitionnamespace $clsName ?$kind?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassDefnNsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *kindList[] = {
	"-class",
	"-instance",
	NULL
    };
    int kind = 0;
    Tcl_Obj *nsNamePtr;
    Class *clsPtr;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?kind?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3 && Tcl_GetIndexFromObj(interp, objv[2], kindList, "kind", 0,
	    &kind) != TCL_OK) {
	return TCL_ERROR;
    }

    if (kind) {
	nsNamePtr = clsPtr->objDefinitionNs;
    } else {
	nsNamePtr = clsPtr->clsDefinitionNs;
    }
    if (nsNamePtr) {
	Tcl_SetObjResult(interp, nsNamePtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassDestrCmd --
 *
 *	Implements [info class destructor $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassDestrCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    if (clsPtr->destructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = GetProcFromMethod(interp, clsPtr->destructorPtr);
    if (procPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetMethodBody(clsPtr->destructorPtr));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassFiltersCmd --
 *
 *	Implements [info class filters $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassFiltersCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Size i;
    Tcl_Obj *filterObj, *resultObj;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(filterObj, clsPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassForwardCmd --
 *
 *	Implements [info class forward $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassForwardCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Method *mPtr;
    Tcl_Obj *prefixObj;
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetClassMethodFromObj(interp, clsPtr, objv[2]);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    prefixObj = GetForwardFromMethod(interp, mPtr);
    if (prefixObj == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassInstancesCmd --
 *
 *	Implements [info class instances $clsName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassInstancesCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr;
    Tcl_Size i;
    const char *pattern = NULL;
    Tcl_Obj *resultObj;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    TclNewObj(resultObj);
    FOREACH(oPtr, clsPtr->instances) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, oPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassMethodsCmd --
 *
 *	Implements [info class methods $clsName ?options...?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassMethodsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    int flag, recurse, scope;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?-option value ...?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (ParseMethodsArgs(interp, objc, objv,
	    &recurse, &flag, &scope) != TCL_OK) {
	return TCL_ERROR;
    }

    if (recurse) {
	Tcl_Obj **names;
	Tcl_Size numNames = TclOOGetSortedClassMethodList(clsPtr, flag, &names);

	if (numNames > 0) {
	    Tcl_SetObjResult(interp, Tcl_NewListObj(numNames, names));
	    Tcl_Free((void *)names);
	}
    } else {
	Tcl_SetObjResult(interp,
		ListLocalMatchingMethods(&clsPtr->classMethods, scope, flag));
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassMethodTypeCmd --
 *
 *	Implements [info class methodtype $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassMethodTypeCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    Method *mPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    mPtr = GetClassMethodFromObj(interp, clsPtr, objv[2]);
    if (mPtr == NULL) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(
	    mPtr->typePtr->name, TCL_AUTO_LENGTH));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassMixinsCmd --
 *
 *	Implements [info class mixins $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassMixinsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *mixinPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(mixinPtr, clsPtr->mixins) {
	if (!mixinPtr) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, mixinPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassSubsCmd --
 *
 *	Implements [info class subclasses $clsName ?$pattern?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassSubsCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *subclassPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;
    const char *pattern = NULL;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    TclNewObj(resultObj);
    FOREACH(subclassPtr, clsPtr->subclasses) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, subclassPtr->thisPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    FOREACH(subclassPtr, clsPtr->mixinSubs) {
	Tcl_Obj *tmpObj = TclOOObjectName(interp, subclassPtr->thisPtr);

	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, resultObj, tmpObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassSupersCmd --
 *
 *	Implements [info class superclasses $clsName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassSupersCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr, *superPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(superPtr, clsPtr->superclasses) {
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, superPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassVariablesCmd --
 *
 *	Implements [info class variables $clsName ?-private?]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassVariablesCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    Tcl_Obj *resultObj;
    Tcl_Size i;
    int isPrivate = 0;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?-private?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	if (strcmp("-private", TclGetString(objv[2])) != 0) {
	    return TCL_ERROR;
	}
	isPrivate = 1;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    if (isPrivate) {
	PrivateVariableMapping *privatePtr;

	FOREACH_STRUCT(privatePtr, clsPtr->privateVariables) {
	    Tcl_ListObjAppendElement(NULL, resultObj, privatePtr->variableObj);
	}
    } else {
	Tcl_Obj *variableObj;

	FOREACH(variableObj, clsPtr->variables) {
	    Tcl_ListObjAppendElement(NULL, resultObj, variableObj);
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoObjectCallCmd --
 *
 *	Implements [info object call $objName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoObjectCallCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    CallContext *contextPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Get the call context and render its call chain.
     */

    contextPtr = TclOOGetCallContext(oPtr, objv[2], PUBLIC_METHOD, NULL, NULL,
	    NULL);
    if (contextPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cannot construct any call chain", TCL_AUTO_LENGTH));
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp,
	    TclOORenderCallChain(interp, contextPtr->callPtr));
    TclOODeleteContext(contextPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassCallCmd --
 *
 *	Implements [info class call $clsName $methodName]
 *
 * ----------------------------------------------------------------------
 */

static int
InfoClassCallCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    CallChain *callPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Get an render the stereotypical call chain.
     */

    callPtr = TclOOGetStereotypeCallChain(clsPtr, objv[2], PUBLIC_METHOD);
    if (callPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cannot construct any call chain", TCL_AUTO_LENGTH));
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TclOORenderCallChain(interp, callPtr));
    TclOODeleteChain(callPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * InfoClassPropCmd, InfoObjectPropCmd --
 *
 *	Implements [info class properties $clsName ?$option...?] and
 *	[info object properties $objName ?$option...?]
 *
 * ----------------------------------------------------------------------
 */

enum PropOpt {
    PROP_ALL, PROP_READABLE, PROP_WRITABLE
};
static const char *const propOptNames[] = {
    "-all", "-readable", "-writable",
    NULL
};

static int
InfoClassPropCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *clsPtr;
    int i, idx, all = 0, writable = 0, allocated = 0;
    Tcl_Obj *result, *propObj;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?options...?");
	return TCL_ERROR;
    }
    clsPtr = GetClassFromObj(interp, objv[1]);
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    for (i = 2; i < objc; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], propOptNames, "option", 0,
		&idx) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (idx) {
	case PROP_ALL:
	    all = 1;
	    break;
	case PROP_READABLE:
	    writable = 0;
	    break;
	case PROP_WRITABLE:
	    writable = 1;
	    break;
	}
    }

    /*
     * Get the properties.
     */

    if (all) {
	result = TclOOGetAllClassProperties(clsPtr, writable, &allocated);
	if (allocated) {
	    SortPropList(result);
	}
    } else {
	TclNewObj(result);
	if (writable) {
	    FOREACH(propObj, clsPtr->properties.writable) {
		Tcl_ListObjAppendElement(NULL, result, propObj);
	    }
	} else {
	    FOREACH(propObj, clsPtr->properties.readable) {
		Tcl_ListObjAppendElement(NULL, result, propObj);
	    }
	}
	SortPropList(result);
    }
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}

static int
InfoObjectPropCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    int i, idx, all = 0, writable = 0, allocated = 0;
    Tcl_Obj *result, *propObj;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?options...?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    for (i = 2; i < objc; i++) {
	if (Tcl_GetIndexFromObj(interp, objv[i], propOptNames, "option", 0,
		&idx) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (idx) {
	case PROP_ALL:
	    all = 1;
	    break;
	case PROP_READABLE:
	    writable = 0;
	    break;
	case PROP_WRITABLE:
	    writable = 1;
	    break;
	}
    }

    /*
     * Get the properties.
     */

    if (all) {
	result = TclOOGetAllObjectProperties(oPtr, writable, &allocated);
	if (allocated) {
	    SortPropList(result);
	}
    } else {
	TclNewObj(result);
	if (writable) {
	    FOREACH(propObj, oPtr->properties.writable) {
		Tcl_ListObjAppendElement(NULL, result, propObj);
	    }
	} else {
	    FOREACH(propObj, oPtr->properties.readable) {
		Tcl_ListObjAppendElement(NULL, result, propObj);
	    }
	}
	SortPropList(result);
    }
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * SortPropList --
 *	Sort a list of names of properties. Simple support function. Assumes
 *	that the list Tcl_Obj is unshared and doesn't have a string
 *	representation.
 *
 * ----------------------------------------------------------------------
 */

static int
PropNameCompare(
    const void *a,
    const void *b)
{
    Tcl_Obj *first = *(Tcl_Obj **) a;
    Tcl_Obj *second = *(Tcl_Obj **) b;

    return strcmp(Tcl_GetString(first), Tcl_GetString(second));
}

static void
SortPropList(
    Tcl_Obj *list)
{
    Tcl_Size ec;
    Tcl_Obj **ev;

    Tcl_ListObjGetElements(NULL, list, &ec, &ev);
    qsort(ev, ec, sizeof(Tcl_Obj *), PropNameCompare);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
