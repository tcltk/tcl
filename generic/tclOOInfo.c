/*
 * tclOODefineCmds.c --
 *
 *	This file contains the implementation of the ::oo-related [info]
 *	subcommands.
 *
 * Copyright (c) 2006 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOOInfo.c,v 1.1.2.20 2006/10/22 23:01:37 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

static Tcl_ObjCmdProc InfoObjectClassCmd;
static Tcl_ObjCmdProc InfoObjectDefnCmd;
static Tcl_ObjCmdProc InfoObjectFiltersCmd;
static Tcl_ObjCmdProc InfoObjectForwardCmd;
static Tcl_ObjCmdProc InfoObjectIsACmd;
static Tcl_ObjCmdProc InfoObjectMethodsCmd;
static Tcl_ObjCmdProc InfoObjectMixinsCmd;
static Tcl_ObjCmdProc InfoObjectVarsCmd;
static Tcl_ObjCmdProc InfoClassConstrCmd;
static Tcl_ObjCmdProc InfoClassDefnCmd;
static Tcl_ObjCmdProc InfoClassDestrCmd;
static Tcl_ObjCmdProc InfoClassFiltersCmd;
static Tcl_ObjCmdProc InfoClassForwardCmd;
static Tcl_ObjCmdProc InfoClassInstancesCmd;
static Tcl_ObjCmdProc InfoClassMethodsCmd;
static Tcl_ObjCmdProc InfoClassMixinsCmd;
static Tcl_ObjCmdProc InfoClassSubsCmd;
static Tcl_ObjCmdProc InfoClassSupersCmd;

struct NameProcMap { const char *name; Tcl_ObjCmdProc *proc; };

/*
 * List of commands that are used to implement the [info object] subcommands.
 */

static const struct NameProcMap infoObjectCmds[] = {
    {"::oo::InfoObject::class",	     InfoObjectClassCmd},
    {"::oo::InfoObject::definition", InfoObjectDefnCmd},
    {"::oo::InfoObject::filters",    InfoObjectFiltersCmd},
    {"::oo::InfoObject::forward",    InfoObjectForwardCmd},
    {"::oo::InfoObject::isa",	     InfoObjectIsACmd},
    {"::oo::InfoObject::methods",    InfoObjectMethodsCmd},
    {"::oo::InfoObject::mixins",     InfoObjectMixinsCmd},
    {"::oo::InfoObject::vars",	     InfoObjectVarsCmd},
    {NULL, NULL}
};

/*
 * List of commands that are used to implement the [info class] subcommands.
 */

static const struct NameProcMap infoClassCmds[] = {
    {"::oo::InfoClass::constructor",  InfoClassConstrCmd},
    {"::oo::InfoClass::definition",   InfoClassDefnCmd},
    {"::oo::InfoClass::destructor",   InfoClassDestrCmd},
    {"::oo::InfoClass::filters",      InfoClassFiltersCmd},
    {"::oo::InfoClass::forward",      InfoClassForwardCmd},
    {"::oo::InfoClass::instances",    InfoClassInstancesCmd},
    {"::oo::InfoClass::methods",      InfoClassMethodsCmd},
    {"::oo::InfoClass::mixins",	      InfoClassMixinsCmd},
    {"::oo::InfoClass::subclasses",   InfoClassSubsCmd},
    {"::oo::InfoClass::superclasses", InfoClassSupersCmd},
    {NULL, NULL}
};

void
TclOOInitInfo(
    Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr;
    int i;

    /*
     * Build the ensemble used to implement [info object].
     */

    nsPtr = Tcl_CreateNamespace(interp, "::oo::InfoObject", NULL, NULL);
    Tcl_CreateEnsemble(interp, nsPtr->fullName, nsPtr, TCL_ENSEMBLE_PREFIX);
    Tcl_Export(interp, nsPtr, "[a-z]*", 1);
    for (i=0 ; infoObjectCmds[i].name!=NULL ; i++) {
	Tcl_CreateObjCommand(interp, infoObjectCmds[i].name,
		infoObjectCmds[i].proc, NULL, NULL);
    }

    /*
     * Build the ensemble used to implement [info class].
     */

    nsPtr = Tcl_CreateNamespace(interp, "::oo::InfoClass", NULL, NULL);
    Tcl_CreateEnsemble(interp, nsPtr->fullName, nsPtr, TCL_ENSEMBLE_PREFIX);
    Tcl_Export(interp, nsPtr, "[a-z]*", 1);
    for (i=0 ; infoClassCmds[i].name!=NULL ; i++) {
	Tcl_CreateObjCommand(interp, infoClassCmds[i].name,
		infoClassCmds[i].proc, NULL, NULL);
    }
}

int
TclInfoObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    int result;
    Tcl_Obj **newobjv = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * objc-1);
    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = 2;
	iPtr->ensembleRewrite.numInsertedObjs = 1;
    } else {
	int ni = iPtr->ensembleRewrite.numInsertedObjs;
	if (ni < 2) {
	    iPtr->ensembleRewrite.numRemovedObjs += 2 - ni;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs--;
	}
    }

    TclNewStringObj(newobjv[0], "::oo::InfoObject",
	    strlen("::oo::InfoObject"));
    Tcl_IncrRefCount(newobjv[0]);
    if (objc > 2) {
	memcpy(newobjv+1, objv+2, sizeof(Tcl_Obj *) * (objc-2));
    }
    result = Tcl_EvalObjv(interp, objc-1, newobjv, TCL_EVAL_INVOKE);
    TclDecrRefCount(newobjv[0]);
    ckfree((char *) newobjv);
    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = NULL;
	iPtr->ensembleRewrite.numRemovedObjs = 0;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    }
    return result;
}

int
TclInfoClassCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    int result;
    Tcl_Obj **newobjv = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * objc-1);
    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = 2;
	iPtr->ensembleRewrite.numInsertedObjs = 1;
    } else {
	int ni = iPtr->ensembleRewrite.numInsertedObjs;
	if (ni < 2) {
	    iPtr->ensembleRewrite.numRemovedObjs += 2 - ni;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs--;
	}
    }

    TclNewStringObj(newobjv[0], "::oo::InfoClass", strlen("::oo::InfoClass"));
    Tcl_IncrRefCount(newobjv[0]);
    if (objc > 2) {
	memcpy(newobjv+1, objv+2, sizeof(Tcl_Obj *) * (objc-2));
    }
    result = Tcl_EvalObjv(interp, objc-1, newobjv, TCL_EVAL_INVOKE);
    TclDecrRefCount(newobjv[0]);
    ckfree((char *) newobjv);
    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = NULL;
	iPtr->ensembleRewrite.numRemovedObjs = 0;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    }
    return result;
}

static int
InfoObjectClassCmd(
    ClientData clientData,
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
	Tcl_GetCommandFullName(interp, oPtr->selfCls->thisPtr->command,
		Tcl_GetObjResult(interp));
	return TCL_OK;
    } else {
	Object *o2Ptr;
	Class *mixinPtr;
	int i;

	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "object \"", TclGetString(objv[2]),
		    "\" is not a class", NULL);
	    return TCL_ERROR;
	}

	FOREACH(mixinPtr, oPtr->mixins) {
	    if (TclOOIsReachable(o2Ptr->classPtr, mixinPtr)) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
		return TCL_OK;
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(
		TclOOIsReachable(o2Ptr->classPtr, oPtr->selfCls)));
	return TCL_OK;
    }
}

static int
InfoObjectDefnCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_HashEntry *hPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *argsObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 3, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    procPtr = TclOOGetProcFromMethod(Tcl_GetHashValue(hPtr));
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    TclNewObj(argsObj);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, argsObj, argObj);
	}
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), argsObj);

    /*
     * This is copied from the [info body] implementation. See the comments
     * there for why this copy has to be done here.
     */

    if (procPtr->bodyPtr->bytes == NULL) {
	(void) Tcl_GetString(procPtr->bodyPtr);
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewStringObj(procPtr->bodyPtr->bytes,
	    procPtr->bodyPtr->length));
    return TCL_OK;
}

static int
InfoObjectFiltersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;
    Tcl_Obj *filterObj;
    Object *oPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    FOREACH(filterObj, oPtr->filters) {
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), filterObj);
    }
    return TCL_OK;
}

static int
InfoObjectForwardCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *prefixObj;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName methodName");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    prefixObj = TclOOGetFwdFromMethod(Tcl_GetHashValue(hPtr));
    if (prefixObj == NULL) {
	Tcl_AppendResult(interp,
		"prefix argument list not available for this kind of method",
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

static int
InfoObjectIsACmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    static const char *categories[] = {
	"class", "metaclass", "mixin", "object", "typeof", NULL
    };
    enum IsACats {
	IsClass, IsMetaclass, IsMixin, IsObject, IsType
    };
    Object *oPtr, *o2Ptr;
    int idx, i;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "category objName ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], categories, "category", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }

    if (idx == IsObject) {
	int ok = (Tcl_GetObjectFromObj(interp, objv[2]) != NULL);

	if (!ok) {
	    Tcl_ResetResult(interp);
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(ok ? 1 : 0));
	return TCL_OK;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    switch ((enum IsACats) idx) {
    case IsClass:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName");
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(oPtr->classPtr ? 1 : 0));
	return TCL_OK;
    case IsMetaclass:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName");
	    return TCL_ERROR;
	}
	if (oPtr->classPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	} else {
	    Foundation *fPtr = ((Interp *)interp)->ooFoundation;

	    Tcl_SetObjResult(interp, Tcl_NewIntObj(
		    TclOOIsReachable(fPtr->classCls, oPtr->classPtr) ? 1 : 0));
	}
	return TCL_OK;
    case IsMixin:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName className");
	    return TCL_ERROR;
	}
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "non-classes cannot be mixins", NULL);
	    return TCL_ERROR;
	} else {
	    Class *mixinPtr;

	    FOREACH(mixinPtr, oPtr->mixins) {
		if (mixinPtr == o2Ptr->classPtr) {
		    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
		    return TCL_OK;
		}
	    }
	}
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	return TCL_OK;
    case IsType:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objName className");
	    return TCL_ERROR;
	}
	o2Ptr = (Object *) Tcl_GetObjectFromObj(interp, objv[3]);
	if (o2Ptr == NULL) {
	    return TCL_ERROR;
	}
	if (o2Ptr->classPtr == NULL) {
	    Tcl_AppendResult(interp, "non-classes cannot be types", NULL);
	    return TCL_ERROR;
	}
	if (TclOOIsReachable(o2Ptr->classPtr, oPtr->selfCls)) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
	} else {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	}
	return TCL_OK;
    case IsObject:
	Tcl_Panic("unexpected fallthrough");
    }
    return TCL_ERROR;
}

static int
InfoObjectMethodsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    int flag = PUBLIC_METHOD;
    FOREACH_HASH_DECLS;
    Tcl_Obj *namePtr;
    Method *mPtr;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objName ?-private?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 3) {
	int len;
	const char *str = Tcl_GetStringFromObj(objv[2], &len);

	if (len == 13 && !strcmp("-localprivate", str)) {
	    flag = PRIVATE_METHOD;
	} else if (len < 2 || strncmp("-private", str, (unsigned)len)) {
	    Tcl_AppendResult(interp, "unknown switch \"", str,
		    "\": must be -private", NULL);
	    return TCL_ERROR;
	} else {
	    flag = 0;
	}
    }

    FOREACH_HASH(namePtr, mPtr, &oPtr->methods) {
	if (mPtr->typePtr != NULL && (mPtr->flags & flag) == flag) {
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), namePtr);
	}
    }
    return TCL_OK;
}

static int
InfoObjectMixinsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Class *mixinPtr;
    Object *oPtr;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 3, objv, "objName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    FOREACH(mixinPtr, oPtr->mixins) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, mixinPtr->thisPtr->command, tmpObj);
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoObjectVarsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    const char *pattern = NULL, *name;
    FOREACH_HASH_DECLS;
    Var *varPtr;

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

    FOREACH_HASH(name, varPtr, &((Namespace *) oPtr->namespacePtr)->varTable) {
	if (varPtr->flags & VAR_UNDEFINED) {
	    continue;
	}
	if (pattern != NULL && !Tcl_StringMatch(name, pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
		Tcl_NewStringObj(name, -1));
    }

    return TCL_OK;
}

static int
InfoClassConstrCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *argsObj;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    if (clsPtr->constructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = TclOOGetProcFromMethod(clsPtr->constructorPtr);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    TclNewObj(argsObj);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, argsObj, argObj);
	}
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), argsObj);
    if (procPtr->bodyPtr->bytes == NULL) {
	(void) Tcl_GetString(procPtr->bodyPtr);
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewStringObj(procPtr->bodyPtr->bytes,
	    procPtr->bodyPtr->length));
    return TCL_OK;
}

static int
InfoClassDefnCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Proc *procPtr;
    CompiledLocal *localPtr;
    Tcl_Obj *argsObj;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    hPtr = Tcl_FindHashEntry(&clsPtr->classMethods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    procPtr = TclOOGetProcFromMethod(Tcl_GetHashValue(hPtr));
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    TclNewObj(argsObj);
    for (localPtr=procPtr->firstLocalPtr; localPtr!=NULL;
	    localPtr=localPtr->nextPtr) {
	if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *argObj;

	    TclNewObj(argObj);
	    Tcl_ListObjAppendElement(NULL, argObj,
		    Tcl_NewStringObj(localPtr->name, -1));
	    if (localPtr->defValuePtr != NULL) {
		Tcl_ListObjAppendElement(NULL, argObj, localPtr->defValuePtr);
	    }
	    Tcl_ListObjAppendElement(NULL, argsObj, argObj);
	}
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), argsObj);
    if (procPtr->bodyPtr->bytes == NULL) {
	(void) Tcl_GetString(procPtr->bodyPtr);
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewStringObj(procPtr->bodyPtr->bytes,
	    procPtr->bodyPtr->length));
    return TCL_OK;
}

static int
InfoClassDestrCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Proc *procPtr;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    if (clsPtr->destructorPtr == NULL) {
	return TCL_OK;
    }
    procPtr = TclOOGetProcFromMethod(clsPtr->destructorPtr);
    if (procPtr == NULL) {
	Tcl_AppendResult(interp,
		"definition not available for this kind of method", NULL);
	return TCL_ERROR;
    }

    if (procPtr->bodyPtr->bytes == NULL) {
	(void) Tcl_GetString(procPtr->bodyPtr);
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewStringObj(procPtr->bodyPtr->bytes,
	    procPtr->bodyPtr->length));
    return TCL_OK;
}

static int
InfoClassFiltersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;
    Tcl_Obj *filterObj;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    FOREACH(filterObj, clsPtr->filters) {
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), filterObj);
    }
    return TCL_OK;
}

static int
InfoClassForwardCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Tcl_Obj *prefixObj;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className methodName");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    hPtr = Tcl_FindHashEntry(&clsPtr->classMethods, (char *) objv[2]);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown method \"", TclGetString(objv[2]),
		"\"", NULL);
	return TCL_ERROR;
    }
    prefixObj = TclOOGetFwdFromMethod(Tcl_GetHashValue(hPtr));
    if (prefixObj == NULL) {
	Tcl_AppendResult(interp,
		"prefix argument list not available for this kind of method",
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, prefixObj);
    return TCL_OK;
}

static int
InfoClassInstancesCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr;
    int i;
    const char *pattern = NULL;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    FOREACH(oPtr, clsPtr->instances) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, oPtr->command, tmpObj);
	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    TclDecrRefCount(tmpObj);
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoClassMethodsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int flag = PUBLIC_METHOD;
    FOREACH_HASH_DECLS;
    Tcl_Obj *namePtr;
    Method *mPtr;
    Object *oPtr;
    Class *clsPtr;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?-private?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;
    if (objc == 3) {
	int len;
	const char *str = Tcl_GetStringFromObj(objv[2], &len);

	if (len == 13 && !strcmp("-localprivate", str)) {
	    flag = PRIVATE_METHOD;
	} else if (len < 2 || strncmp("-private", str, (unsigned)len)) {
	    Tcl_AppendResult(interp, "unknown switch \"", str,
		    "\": must be -private", NULL);
	    return TCL_ERROR;
	} else {
	    flag = 0;
	}
    }

    FOREACH_HASH(namePtr, mPtr, &clsPtr->classMethods) {
	if (mPtr->typePtr != NULL && (mPtr->flags & flag) == flag) {
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), namePtr);
	}
    }
    return TCL_OK;
}

static int
InfoClassMixinsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr, *mixinPtr;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    FOREACH(mixinPtr, clsPtr->mixins) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, mixinPtr->thisPtr->command, tmpObj);
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoClassSubsCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr, *subclassPtr;
    int i;
    const char *pattern = NULL;

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className ?pattern?");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;
    if (objc == 3) {
	pattern = TclGetString(objv[2]);
    }

    FOREACH(subclassPtr, clsPtr->subclasses) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, subclassPtr->thisPtr->command, tmpObj);
	if (pattern && !Tcl_StringMatch(TclGetString(tmpObj), pattern)) {
	    TclDecrRefCount(tmpObj);
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoClassSupersCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Object *oPtr;
    Class *clsPtr, *superPtr;
    int i;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[1]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    FOREACH(superPtr, clsPtr->superclasses) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, superPtr->thisPtr->command, tmpObj);
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
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
