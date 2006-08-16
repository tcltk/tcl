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
 * RCS: @(#) $Id: tclOODefineCmds.c,v 1.1.2.4 2006/08/16 20:51:21 dkf Exp $
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

	// TODO: Leverage the arg rewriting mechanism used by ensembles

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

int
TclOODefineConstructorObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;
    Class *clsPtr;
    int bodyLength;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "arguments body");
	return TCL_ERROR;
    }

    /*
     * Extract and validate the context, which is the class that we wish to
     * modify.
     */

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "only classes may have constructors defined",
		NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    (void) Tcl_GetStringFromObj(objv[2], &bodyLength);
    if (bodyLength > 0) {
	/*
	 * Create the method structure.
	 */

	Method *mPtr;

	mPtr = TclNewProcClassMethod(interp, clsPtr, 1, NULL, objv[1],
		objv[2]);
	if (mPtr == NULL) {
	    return TCL_ERROR;
	}

	/*
	 * Place the method structure in the class record. Note that we might
	 * not immediately delete the constructor as this might be being done
	 * during execution of the constructor itself.
	 */

	TclDeleteMethod(clsPtr->constructorPtr);
	clsPtr->constructorPtr = mPtr;
    } else {
	/*
	 * Delete the constructor method record and set the field in the class
	 * record to NULL.
	 */

	TclDeleteMethod(clsPtr->constructorPtr);
	clsPtr->constructorPtr = NULL;
    }

    return TCL_OK;
}

int
TclOODefineCopyObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?targetName?");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineDestructorObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;
    Class *clsPtr;
    int bodyLength;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "body");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "only classes may have destructors defined",
		NULL);
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;

    (void) Tcl_GetStringFromObj(objv[1], &bodyLength);
    if (bodyLength > 0) {
	/*
	 * Create the method structure.
	 */

	Method *mPtr;

	mPtr = TclNewProcClassMethod(interp, clsPtr, 1, NULL, NULL, objv[1]);
	if (mPtr == NULL) {
	    return TCL_ERROR;
	}

	/*
	 * Place the method structure in the class record. Note that we might
	 * not immediately delete the destructor as this might be being done
	 * during execution of the destructor itself.
	 */

	TclDeleteMethod(clsPtr->destructorPtr);
	clsPtr->destructorPtr = mPtr;
    } else {
	/*
	 * Delete the destructor method record and set the field in the class
	 * record to NULL.
	 */

	TclDeleteMethod(clsPtr->destructorPtr);
	clsPtr->destructorPtr = NULL;
    }

    return TCL_OK;
}

int
TclOODefineExportObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfExport = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pattern ?pattern ...?");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfExport |= (oPtr->classPtr == NULL);

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineFilterObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfFilter = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfFilter |= (oPtr->classPtr == NULL);

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineForwardObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfForward = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;
    Method *mPtr;
    int isPublic;
    Tcl_Obj *prefixObj;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "name cmdName ?arg ...?");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfForward |= (oPtr->classPtr == NULL);
    isPublic = Tcl_StringMatch(Tcl_GetString(objv[1]), "[a-z]*");

    /*
     * Create the method structure.
     */

    prefixObj = Tcl_NewListObj(objc-2, objv+2);
    if (isSelfForward) {
	mPtr = TclNewForwardMethod(interp, oPtr, isPublic, objv[1], prefixObj);
    } else {
	mPtr = TclNewForwardClassMethod(interp, oPtr->classPtr, isPublic,
		objv[1], prefixObj);
    }
    if (mPtr == NULL) {
	Tcl_DecrRefCount(prefixObj);
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
TclOODefineMethodObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfMethod = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;
    int bodyLength;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "name args body");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfMethod |= (oPtr->classPtr == NULL);

    (void) Tcl_GetStringFromObj(objv[3], &bodyLength);
    if (bodyLength > 0) {
	/*
	 * Create the method structure.
	 */

	Method *mPtr;
	int isPublic = Tcl_StringMatch(Tcl_GetString(objv[1]), "[a-z]*");

	if (isSelfMethod) {
	    mPtr = TclNewProcMethod(interp, oPtr, isPublic, objv[1], objv[2],
		    objv[3]);
	} else {
	    mPtr = TclNewProcClassMethod(interp, oPtr->classPtr, isPublic,
		    objv[1], objv[2], objv[3]);
	}
	if (mPtr == NULL) {
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Delete the method structure from the appropriate hash table.
	 */

	Tcl_HashEntry *hPtr;

	if (isSelfMethod) {
	    hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *)objv[1]);
	} else {
	    hPtr = Tcl_FindHashEntry(&oPtr->classPtr->classMethods,
		    (char *)objv[1]);
	}
	if (hPtr != NULL) {
	    Method *mPtr = (Method *) Tcl_GetHashValue(hPtr);

	    Tcl_DeleteHashEntry(hPtr);
	    TclDeleteMethod(mPtr);
	}
    }

    return TCL_OK;
}

int
TclOODefineMixinObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfMixin = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfMixin |= (oPtr->classPtr == NULL);

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineParameterObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Must nail down the semantics of this!
     */

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineSelfClassObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineSuperclassObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_AppendResult(interp, "only classes may have superclasses defined",
		NULL);
	return TCL_ERROR;
    }

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

int
TclOODefineUnexportObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    int isSelfUnexport = (clientData != NULL);
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pattern ?pattern ...?");
	return TCL_ERROR;
    }

    if (iPtr->framePtr->isProcCallFrame != FRAME_IS_OO_DEFINE) {
	Tcl_AppendResult(interp, "this command may only be called from within "
		"the context of the ::oo::define command", NULL);
	return TCL_ERROR;
    }
    oPtr = TclGetObjectFromObj(interp,
	    (Tcl_Obj *) iPtr->framePtr->ooContextPtr);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    isSelfUnexport |= (oPtr->classPtr == NULL);

    Tcl_AppendResult(interp, "TODO: not yet finished", NULL);
    return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
