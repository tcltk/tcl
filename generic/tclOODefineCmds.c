/*
 * tclOODefineCmds.c --
 *
 *	This file contains the implementation of the ::oo::define command,
 *	part of the object-system core (NB: not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2005 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOODefineCmds.c,v 1.1.2.1 2006/07/10 23:24:24 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

int
TclOODefineObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    CallFrame *framePtr, **framePtrPtr;
    Foundation *fPtr = ((Interp *) interp)->ooFoundation;
    int result;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objectName arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Make the object's namespace the current namespace and evaluate the
     * command(s).
     */

    /* This is needed to satisfy GCC 3.3's strict aliasing rules */
    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) fPtr->defineNs, FRAME_IS_OO_DEFINE);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    framePtr->ooContextPtr = objv[1];
    framePtr->objc = objc;
    framePtr->objv = objv;	/* Reference counts do not need to be
				 * incremented here. */

    if (objc == 3) {
	result = Tcl_EvalObjEx(interp, objv[2], 0);

	if (result == TCL_ERROR) {
	    int length;
	    const char *objName = Tcl_GetStringFromObj(objv[1], &length);
	    int limit = 200;
	    int overflow = (length > limit);

	    // TODO: fix trace
	    TclFormatToErrorInfo(interp,
		    "\n    (in ::oo::define \"%.*s%s\" script line %d)",
		    (overflow ? limit : length), objName,
		    (overflow ? "..." : ""), interp->errorLine);
	}
    } else {
	Tcl_Obj *objPtr;

	/*
	 * More than one argument: make a list of them, then evaluate the
	 * result. Tcl_EvalObjEx will delete the object when it decrements its
	 * refcount after eval'ing it.
	 */

	objPtr = Tcl_NewListObj(objc-2, objv+2);
	result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);

	if (result == TCL_ERROR) {
	    TclFormatToErrorInfo(interp, "\n    (in ::oo::define command)");
	}
    }

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
