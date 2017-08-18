/*
 * tclProcess.c --
 *
 *	This file implements the "tcl::process" ensemble for subprocess 
 *      management as defined by TIP #462.
 *
 * Copyright (c) 2017 Frederic Bonnet.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

static int autopurge = 1;		/* Autopurge flag. */

 /*
 * Prototypes for functions defined later in this file:
 */

static int             ProcessListObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int             ProcessStatusObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int             ProcessPurgeObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int             ProcessAutopurgeObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);

/*----------------------------------------------------------------------
 *
 * ProcessListObjCmd --
 *
 *	This function implements the 'tcl::process list' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessListObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,	/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, NULL);
	return TCL_ERROR;
    }

    /* TODO */
    return TCL_ERROR;
 }

/*----------------------------------------------------------------------
 *
 * ProcessStatusObjCmd --
 *
 *	This function implements the 'tcl::process status' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessStatusObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,	/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc < 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "?switches? ?pids?");
	return TCL_ERROR;
    }

    /* TODO */
    return TCL_ERROR;
 }

/*----------------------------------------------------------------------
 *
 * ProcessPurgeObjCmd --
 *
 *	This function implements the 'tcl::process purge' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessPurgeObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,	/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 1 && objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?pids?");
	return TCL_ERROR;
    }

    /* TODO */
    return TCL_ERROR;
 }

/*----------------------------------------------------------------------
 *
 * ProcessAutopurgeObjCmd --
 *
 *	This function implements the 'tcl::process autopurge' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessAutopurgeObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,	/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
 {
    if (objc != 1 && objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?flag?");
	return TCL_ERROR;
    }

    if (objc == 2) {
        /*
         * Set given value.
         */
        
        int flag;
        int result = Tcl_GetBooleanFromObj(interp, objv[1], &flag);
        if (result != TCL_OK) {
            return result;
        }

        TclProcessSetAutopurge(flag);
    }

    /* 
     * Return current value. 
     */

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(TclProcessGetAutopurge()));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitProcessCmd --
 *
 *	This procedure creates the "tcl::process" Tcl command. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclInitProcessCmd(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    static const EnsembleImplMap processImplMap[] = {
        {"list", ProcessListObjCmd, TclCompileBasic0ArgCmd, NULL, NULL, 1},
        {"status", ProcessStatusObjCmd, TclCompileBasicMin0ArgCmd, NULL, NULL, 1},
        {"purge", ProcessPurgeObjCmd, TclCompileBasic0Or1ArgCmd, NULL, NULL, 1},
        {"autopurge", ProcessAutopurgeObjCmd, TclCompileBasic0Or1ArgCmd, NULL, NULL, 1},
        {NULL, NULL, NULL, NULL, NULL, 0}
    };
    Tcl_Command processCmd;

    processCmd = TclMakeEnsemble(interp, "::tcl::process", processImplMap);
    Tcl_Export(interp, Tcl_FindNamespace(interp, "::tcl", NULL, 0),
            "process", 0);
    return processCmd;
}

/*
 *----------------------------------------------------------------------
 *
 * TclProcessGetAutopurge --
 *
 *	This function queries the value of the autopurge flag.
 *
 * Results:
 *	The current boolean value of the autopurge flag.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclProcessGetAutopurge(void)
{
    return autopurge;
}

/*
 *----------------------------------------------------------------------
 *
 * TclProcessSetAutopurge --
 *
 *	This function sets the value of the autopurge flag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the autopurge static variable.
 *
 *----------------------------------------------------------------------
 */

void
TclProcessSetAutopurge(
    int flag)				/* New value for autopurge. */
{
    autopurge = !!flag;
}
