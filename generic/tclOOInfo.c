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
 * RCS: @(#) $Id: tclOOInfo.c,v 1.1.2.1 2006/08/30 14:23:03 dkf Exp $
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
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
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
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
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
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoObjectMixinsCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
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
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassMethodsCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
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
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

static int
InfoClassSupersCmd(
    Class *cPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_AppendResult(interp, "TODO: not yet implemented", NULL);
    return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
