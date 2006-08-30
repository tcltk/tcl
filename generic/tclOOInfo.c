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
 * RCS: @(#) $Id: tclOOInfo.c,v 1.1.2.3 2006/08/30 15:35:16 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

static int		InfoObjectArgsCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectBodyCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectClassCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectDefaultCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectFiltersCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectIsACmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectMethodsCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectMixinsCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoObjectVarsCmd(Object *oPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassArgsCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassBodyCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassDefaultCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassInstancesCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassMethodsCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassParametersCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassSubsCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		InfoClassSupersCmd(Class *cPtr, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);

int
TclInfoObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    static const char *subcommands[] = {
	"args", "body", "class", "default", "filters", "isa", "methods",
	"mixins", "vars", NULL
    };
    enum IOSubCmds {
	IOArgs, IOBody, IOClass, IODefault, IOFilters, IOIsA, IOMethods,
	IOMixins, IOVars
    };
    int idx;
    Object *oPtr;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "objName subcommand ?arg ...?");
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp, objv[2]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], subcommands, "subcommand", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }
    switch ((enum IOSubCmds) idx) {
    case IOArgs:
	return InfoObjectArgsCmd(oPtr, interp, objc, objv);
    case IOBody:
	return InfoObjectBodyCmd(oPtr, interp, objc, objv);
    case IOClass:
	return InfoObjectClassCmd(oPtr, interp, objc, objv);
    case IODefault:
	return InfoObjectDefaultCmd(oPtr, interp, objc, objv);
    case IOFilters:
	return InfoObjectFiltersCmd(oPtr, interp, objc, objv);
    case IOIsA:
	return InfoObjectIsACmd(oPtr, interp, objc, objv);
    case IOMethods:
	return InfoObjectMethodsCmd(oPtr, interp, objc, objv);
    case IOMixins:
	return InfoObjectMixinsCmd(oPtr, interp, objc, objv);
    case IOVars:
	return InfoObjectVarsCmd(oPtr, interp, objc, objv);
    }
    Tcl_Panic("unexpected fallthrough");
    return TCL_ERROR; /* NOTREACHED */
}

int
TclInfoClassCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    static const char *subcommands[] = {
	"args", "body", "default", "instances", "methods", "parameters",
	"subclasses", "superclasses", NULL
    };
    enum ICSubCmds {
	ICArgs, ICBody, ICDefault, ICInstances, ICMethods, ICParameters,
	ICSubs, ICSupers
    };
    int idx;
    Object *oPtr;

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "className subcommand ?arg ...?");
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp, objv[2]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "\"", TclGetString(objv[2]),
		"\" is not a class", NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[3], subcommands, "subcommand", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }

    switch((enum ICSubCmds) idx) {
    case ICArgs:
	return InfoClassArgsCmd(oPtr->classPtr, interp, objc, objv);
    case ICBody:
	return InfoClassBodyCmd(oPtr->classPtr, interp, objc, objv);
    case ICDefault:
	return InfoClassDefaultCmd(oPtr->classPtr, interp, objc, objv);
    case ICInstances:
	return InfoClassInstancesCmd(oPtr->classPtr, interp, objc, objv);
    case ICMethods:
	return InfoClassMethodsCmd(oPtr->classPtr, interp, objc, objv);
    case ICParameters:
	return InfoClassParametersCmd(oPtr->classPtr, interp, objc, objv);
    case ICSubs:
	return InfoClassSubsCmd(oPtr->classPtr, interp, objc, objv);
    case ICSupers:
	return InfoClassSupersCmd(oPtr->classPtr, interp, objc, objv);
    }
    Tcl_Panic("unexpected fallthrough");
    return TCL_ERROR; /* NOTREACHED */
}

static int
InfoObjectArgsCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoObjectBodyCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoObjectClassCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc == 4) {
	Tcl_GetCommandFullName(interp, oPtr->selfCls->thisPtr->command,
		Tcl_GetObjResult(interp));
	return TCL_OK;
    } else if (objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "objName class ?className?");
	return TCL_ERROR;
    } else {
	Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
	return TCL_ERROR;
    }
}

static int
InfoObjectDefaultCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoObjectFiltersCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "objName mixins");
	return TCL_ERROR;
    }
    for (i=0 ; i<oPtr->filters.num ; i++) {
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
		oPtr->filters.list[i]);
    }
    return TCL_OK;
}

static int
InfoObjectIsACmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoObjectMethodsCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int flag = PUBLIC_METHOD;

    if (objc != 4 && objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "objName methods ?-private?");
	return TCL_ERROR;
    }
    if (objc == 5) {
	int len;
	const char *str = Tcl_GetStringFromObj(objv[4], &len);

	if (len < 2 || strncmp("-private", str, len)) {
	    Tcl_AppendResult(interp, "unknown switch \"", str,
		    "\": must be -private", NULL);
	    return TCL_ERROR;
	}
	flag = 0;
    }
    hPtr = Tcl_FirstHashEntry(&oPtr->methods, &search);
    for (; hPtr != NULL ; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&oPtr->methods, hPtr);
	Method *mPtr = Tcl_GetHashValue(hPtr);

	if (mPtr->callPtr != NULL && (mPtr->flags & flag) == flag) {
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), namePtr);
	}
    }
    return TCL_OK;
}

static int
InfoObjectMixinsCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "objName mixins");
	return TCL_ERROR;
    }
    for (i=0 ; i<oPtr->mixins.num ; i++) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp,
		oPtr->mixins.list[i]->thisPtr->command, tmpObj);
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoObjectVarsCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassArgsCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassBodyCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassDefaultCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassInstancesCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "className instances");
	return TCL_ERROR;
    }
    for (i=0 ; i<cPtr->instances.num ; i++) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp, cPtr->instances.list[i]->command,
		tmpObj);
	Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), tmpObj);
    }
    return TCL_OK;
}

static int
InfoClassMethodsCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int flag = PUBLIC_METHOD;

    if (objc != 4 && objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "className methods ?-private?");
	return TCL_ERROR;
    }
    if (objc == 5) {
	int len;
	const char *str = Tcl_GetStringFromObj(objv[4], &len);

	if (len < 2 || strncmp("-private", str, len)) {
	    Tcl_AppendResult(interp, "unknown switch \"", str,
		    "\": must be -private", NULL);
	    return TCL_ERROR;
	}
	flag = 0;
    }
    hPtr = Tcl_FirstHashEntry(&cPtr->classMethods, &search);
    for (; hPtr != NULL ; hPtr = Tcl_NextHashEntry(&search)) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&cPtr->classMethods,
		hPtr);
	Method *mPtr = Tcl_GetHashValue(hPtr);

	if (mPtr->callPtr != NULL && (mPtr->flags & flag) == flag) {
	    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp), namePtr);
	}
    }
    return TCL_OK;
}

static int
InfoClassParametersCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassSubsCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;
    const char *pattern = NULL;

    if (objc != 4 && objc != 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "className subclasses ?pattern?");
	return TCL_ERROR;
    }
    if (objc == 5) {
	pattern = TclGetString(objv[4]);
    }
    for (i=0 ; i<cPtr->subclasses.num ; i++) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp,
		cPtr->subclasses.list[i]->thisPtr->command, tmpObj);
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
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int i;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "className superclasses");
	return TCL_ERROR;
    }
    for (i=0 ; i<cPtr->superclasses.num ; i++) {
	Tcl_Obj *tmpObj;

	TclNewObj(tmpObj);
	Tcl_GetCommandFullName(interp,
		cPtr->superclasses.list[i]->thisPtr->command, tmpObj);
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
