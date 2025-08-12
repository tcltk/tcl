/*
 * tclOOBasic.c --
 *
 *	This file contains implementations of the "simple" commands and
 *	methods from the object-system core.
 *
 * Copyright © 2005-2013 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tclInt.h"
#include "tclOOInt.h"
#include "tclTomMath.h"

static inline Tcl_Object *AddConstructionFinalizer(Tcl_Interp *interp);
static Tcl_NRPostProc	AfterNRDestructor;
static Tcl_NRPostProc	DecrRefsPostClassConstructor;
static Tcl_NRPostProc	FinalizeConstruction;
static Tcl_NRPostProc	FinalizeEval;
static Tcl_NRPostProc	NextRestoreFrame;

/*
 * ----------------------------------------------------------------------
 *
 * AddCreateCallback, FinalizeConstruction --
 *
 *	Special version of TclNRAddCallback that allows the caller to splice
 *	the object created later on. Always calls FinalizeConstruction, which
 *	converts the object into its name and stores that in the interpreter
 *	result. This is shared by all the construction methods (create,
 *	createWithNamespace, new).
 *
 *	Note that this is the only code in this file (or, indeed, the whole of
 *	TclOO) that uses NRE internals; it is the only code that does
 *	non-standard poking in the NRE guts.
 *
 * ----------------------------------------------------------------------
 */

static inline Tcl_Object *
AddConstructionFinalizer(
    Tcl_Interp *interp)
{
    TclNRAddCallback(interp, FinalizeConstruction, NULL);
    return (Tcl_Object *) &(TOP_CB(interp)->data[0]);
}

static int
FinalizeConstruction(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Object *oPtr = (Object *) data[0];

    if (result != TCL_OK) {
	return result;
    }
    Tcl_SetObjResult(interp, TclOOObjectName(interp, oPtr));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Class_Constructor --
 *
 *	Implementation for oo::class constructor.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Class_Constructor(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);
    size_t skip = Tcl_ObjectContextSkippedArgs(context);
    if ((size_t) objc > skip + 1) {
	Tcl_WrongNumArgs(interp, skip, objv,
		"?definitionScript?");
	return TCL_ERROR;
    }

    /*
     * Make the class definition delegate. This is special; it doesn't reenter
     * here (and the class definition delegate doesn't run any constructors).
     *
     * This needs to be done before consideration of whether to pass the script
     * argument to [oo::define]. [Bug 680503]
     */

    Tcl_Obj *nameObj = Tcl_ObjPrintf("%s:: oo ::delegate",
	    oPtr->namespacePtr->fullName);
    Tcl_NewObjectInstance(interp, (Tcl_Class) oPtr->fPtr->classCls,
	    TclGetString(nameObj), NULL, TCL_INDEX_NONE, NULL, 0);
    Tcl_BounceRefCount(nameObj);

    /*
     * If there's nothing else to do, we're done.
     */

    if ((size_t) objc == skip) {
	return TCL_OK;
    }

    /*
     * Delegate to [oo::define] to do the work.
     */

    Tcl_Obj **invoke = (Tcl_Obj **) TclStackAlloc(interp, 3 * sizeof(Tcl_Obj *));
    invoke[0] = oPtr->fPtr->defineName;
    invoke[1] = TclOOObjectName(interp, oPtr);
    invoke[2] = objv[objc - 1];

    /*
     * Must add references or errors in configuration script will cause
     * trouble.
     */

    Tcl_IncrRefCount(invoke[0]);
    Tcl_IncrRefCount(invoke[1]);
    Tcl_IncrRefCount(invoke[2]);
    TclNRAddCallback(interp, DecrRefsPostClassConstructor, invoke, oPtr);

    /*
     * Tricky point: do not want the extra reported level in the Tcl stack
     * trace, so use TCL_EVAL_NOERR.
     */

    return TclNREvalObjv(interp, 3, invoke, TCL_EVAL_NOERR, NULL);
}

static int
DecrRefsPostClassConstructor(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Obj **invoke = (Tcl_Obj **) data[0];
    Object *oPtr = (Object *) data[1];

    TclDecrRefCount(invoke[0]);
    TclDecrRefCount(invoke[1]);
    TclDecrRefCount(invoke[2]);
    invoke[0] = oPtr->fPtr->mcdName;
    invoke[1] = TclOOObjectName(interp, oPtr);
    Tcl_IncrRefCount(invoke[0]);
    Tcl_IncrRefCount(invoke[1]);
    Tcl_InterpState saved = Tcl_SaveInterpState(interp, result);
    int code = Tcl_EvalObjv(interp, 2, invoke, 0);
    TclDecrRefCount(invoke[0]);
    TclDecrRefCount(invoke[1]);
    TclStackFree(interp, invoke);
    if (code != TCL_OK) {
	Tcl_DiscardInterpState(saved);
	return code;
    }
    return Tcl_RestoreInterpState(interp, saved);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Class_Create --
 *
 *	Implementation for oo::class->create method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Class_Create(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);

    /*
     * Sanity check; should not be possible to invoke this method on a
     * non-class.
     */

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj = TclOOObjectName(interp, oPtr);

	TclPrintfResult(interp, "object \"%s\" is not a class",
		TclGetString(cmdnameObj));
	OO_ERROR(interp, INSTANTIATE_NONCLASS);
	return TCL_ERROR;
    }

    /*
     * Check we have the right number of (sensible) arguments.
     */

    if (objc < 1 + Tcl_ObjectContextSkippedArgs(context)) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"objectName ?arg ...?");
	return TCL_ERROR;
    }
    Tcl_Size len;
    const char *objName = Tcl_GetStringFromObj(
	    objv[Tcl_ObjectContextSkippedArgs(context)], &len);
    if (len == 0) {
	TclPrintfResult(interp, "object name must not be empty");
	OO_ERROR(interp, EMPTY_NAME);
	return TCL_ERROR;
    }

    /*
     * Make the object and return its name.
     */

    return TclNRNewObjectInstance(interp, (Tcl_Class) oPtr->classPtr,
	    objName, NULL, objc, objv,
	    Tcl_ObjectContextSkippedArgs(context)+1,
	    AddConstructionFinalizer(interp));
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Class_CreateNs --
 *
 *	Implementation for oo::class->createWithNamespace method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Class_CreateNs(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);

    /*
     * Sanity check; should not be possible to invoke this method on a
     * non-class.
     */

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj = TclOOObjectName(interp, oPtr);

	TclPrintfResult(interp, "object \"%s\" is not a class",
		TclGetString(cmdnameObj));
	OO_ERROR(interp, INSTANTIATE_NONCLASS);
	return TCL_ERROR;
    }

    /*
     * Check we have the right number of (sensible) arguments.
     */

    if (objc + 1 < Tcl_ObjectContextSkippedArgs(context) + 3) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"objectName namespaceName ?arg ...?");
	return TCL_ERROR;
    }
    Tcl_Size len;
    const char *objName = Tcl_GetStringFromObj(
	    objv[Tcl_ObjectContextSkippedArgs(context)], &len);
    if (len == 0) {
	TclPrintfResult(interp, "object name must not be empty");
	OO_ERROR(interp, EMPTY_NAME);
	return TCL_ERROR;
    }
    const char *nsName = Tcl_GetStringFromObj(
	    objv[Tcl_ObjectContextSkippedArgs(context) + 1], &len);
    if (len == 0) {
	TclPrintfResult(interp, "namespace name must not be empty");
	OO_ERROR(interp, EMPTY_NAME);
	return TCL_ERROR;
    }

    /*
     * Make the object and return its name.
     */

    return TclNRNewObjectInstance(interp, (Tcl_Class) oPtr->classPtr,
	    objName, nsName, objc, objv,
	    Tcl_ObjectContextSkippedArgs(context) + 2,
	    AddConstructionFinalizer(interp));
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Class_New --
 *
 *	Implementation for oo::class->new method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Class_New(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);

    /*
     * Sanity check; should not be possible to invoke this method on a
     * non-class.
     */

    if (oPtr->classPtr == NULL) {
	Tcl_Obj *cmdnameObj = TclOOObjectName(interp, oPtr);

	TclPrintfResult(interp, "object \"%s\" is not a class",
		TclGetString(cmdnameObj));
	OO_ERROR(interp, INSTANTIATE_NONCLASS);
	return TCL_ERROR;
    }

    /*
     * Make the object and return its name.
     */

    return TclNRNewObjectInstance(interp, (Tcl_Class) oPtr->classPtr,
	    NULL, NULL, objc, objv, Tcl_ObjectContextSkippedArgs(context),
	    AddConstructionFinalizer(interp));
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Object_Destroy --
 *
 *	Implementation for oo::object->destroy method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Object_Destroy(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Object *oPtr = (Object *) Tcl_ObjectContextObject(context);

    if (objc != (int) Tcl_ObjectContextSkippedArgs(context)) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }
    if (!(oPtr->flags & DESTRUCTOR_CALLED)) {
	oPtr->flags |= DESTRUCTOR_CALLED;
	CallContext *contextPtr = TclOOGetCallContext(oPtr, NULL, DESTRUCTOR,
		NULL, NULL, NULL);
	if (contextPtr != NULL) {
	    contextPtr->callPtr->flags |= DESTRUCTOR;
	    contextPtr->skip = 0;
	    TclNRAddCallback(interp, AfterNRDestructor, contextPtr);
	    TclPushTailcallPoint(interp);
	    return TclOOInvokeContext(contextPtr, interp, 0, NULL);
	}
    }
    if (oPtr->command) {
	Tcl_DeleteCommandFromToken(interp, oPtr->command);
    }
    return TCL_OK;
}

static int
AfterNRDestructor(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    CallContext *contextPtr = (CallContext *) data[0];

    if (contextPtr->oPtr->command) {
	Tcl_DeleteCommandFromToken(interp, contextPtr->oPtr->command);
    }
    TclOODeleteContext(contextPtr);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Object_Eval --
 *
 *	Implementation for oo::object->eval method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Object_Eval(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    CallContext *contextPtr = (CallContext *) context;
    Tcl_Object object = Tcl_ObjectContextObject(context);
    size_t skip = Tcl_ObjectContextSkippedArgs(context);
    CallFrame *framePtr, **framePtrPtr = &framePtr;

    if ((size_t) objc < skip + 1) {
	Tcl_WrongNumArgs(interp, skip, objv, "arg ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Make the object's namespace the current namespace and evaluate the
     * command(s).
     */

    (void) TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    Tcl_GetObjectNamespace(object), FRAME_IS_METHOD);
    framePtr->clientData = context;
    framePtr->objc = objc;
    framePtr->objv = objv;	/* Reference counts do not need to be
				 * incremented here. */

    if (!(contextPtr->callPtr->flags & PUBLIC_METHOD)) {
	object = NULL;		/* Now just for error mesage printing. */
    }

    /*
     * Work out what script we are actually going to evaluate.
     *
     * When there's more than one argument, we concatenate them together with
     * spaces between, then evaluate the result. Tcl_EvalObjEx will delete the
     * object when it decrements its refcount after eval'ing it.
     */

    Tcl_Obj *scriptPtr;
    CmdFrame *invoker;
    if ((size_t) objc != skip+1) {
	scriptPtr = Tcl_ConcatObj(objc-skip, objv+skip);
	invoker = NULL;
    } else {
	scriptPtr = objv[skip];
	invoker = ((Interp *) interp)->cmdFramePtr;
    }

    /*
     * Evaluate the script now, with FinalizeEval to do the processing after
     * the script completes.
     */

    TclNRAddCallback(interp, FinalizeEval, object);
    return TclNREvalObjEx(interp, scriptPtr, 0, invoker, skip);
}

static int
FinalizeEval(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    if (result == TCL_ERROR) {
	Object *oPtr = (Object *) data[0];
	const char *namePtr;

	if (oPtr) {
	    namePtr = TclGetString(TclOOObjectName(interp, oPtr));
	} else {
	    namePtr = "my";
	}

	TclAppendPrintfToErrorInfo(interp,
		"\n    (in \"%s eval\" script line %d)",
		namePtr, Tcl_GetErrorLine(interp));
    }

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Object_Unknown --
 *
 *	Default unknown method handler method (defined in oo::object). This
 *	just creates a suitable error message.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Object_Unknown(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    CallContext *contextPtr = (CallContext *) context;
    Object *callerObj = NULL;
    Class *callerCls = NULL;
    Object *oPtr = contextPtr->oPtr;
    size_t skip = Tcl_ObjectContextSkippedArgs(context);
    CallFrame *framePtr = ((Interp *) interp)->varFramePtr;

    /*
     * If no method name, generate an error asking for a method name. (Only by
     * overriding *this* method can an object handle the absence of a method
     * name without an error).
     */

    if ((size_t) objc < skip + 1) {
	Tcl_WrongNumArgs(interp, skip, objv, "method ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * Determine if the calling context should know about extra private
     * methods, and if so, which.
     */

    if (framePtr->isProcCallFrame & FRAME_IS_METHOD) {
	CallContext *callerContext = (CallContext *) framePtr->clientData;
	Method *mPtr = callerContext->callPtr->chain[
		    callerContext->index].mPtr;

	if (mPtr->declaringObjectPtr) {
	    if (oPtr == mPtr->declaringObjectPtr) {
		callerObj = mPtr->declaringObjectPtr;
	    }
	} else {
	    if (TclOOIsReachable(mPtr->declaringClassPtr, oPtr->selfCls)) {
		callerCls = mPtr->declaringClassPtr;
	    }
	}
    }

    /*
     * Get the list of methods that we want to know about.
     */

    const char **methodNames;
    Tcl_Size numMethodNames = TclOOGetSortedMethodList(oPtr, callerObj,
	    callerCls, contextPtr->callPtr->flags & PUBLIC_METHOD,
	    &methodNames);

    /*
     * Special message when there are no visible methods at all.
     */

    if (numMethodNames == 0) {
	Tcl_Obj *tmpBuf = TclOOObjectName(interp, oPtr);
	const char *piece;

	if (contextPtr->callPtr->flags & PUBLIC_METHOD) {
	    piece = "visible methods";
	} else {
	    piece = "methods";
	}
	TclPrintfResult(interp, "object \"%s\" has no %s",
		TclGetString(tmpBuf), piece);
	TclSetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
		TclGetString(objv[skip]));
	return TCL_ERROR;
    }

    Tcl_Obj *errorMsg = Tcl_ObjPrintf("unknown method \"%s\": must be ",
	    TclGetString(objv[skip]));
    Tcl_Size i;
    for (i=0 ; i<numMethodNames-1 ; i++) {
	if (i) {
	    Tcl_AppendToObj(errorMsg, ", ", TCL_AUTO_LENGTH);
	}
	Tcl_AppendToObj(errorMsg, methodNames[i], TCL_AUTO_LENGTH);
    }
    if (i) {
	Tcl_AppendToObj(errorMsg, " or ", TCL_AUTO_LENGTH);
    }
    Tcl_AppendToObj(errorMsg, methodNames[i], TCL_AUTO_LENGTH);
    Tcl_Free((void *)methodNames);
    Tcl_SetObjResult(interp, errorMsg);
    TclSetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
	    TclGetString(objv[skip]));
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Object_LinkVar --
 *
 *	Implementation of oo::object->variable method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Object_LinkVar(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Object object = Tcl_ObjectContextObject(context);

    if (objc < Tcl_ObjectContextSkippedArgs(context)) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"?varName ...?");
	return TCL_ERROR;
    }

    /*
     * A sanity check. Shouldn't ever happen. (This is all that remains of a
     * more complex check inherited from [global] after we have applied the
     * fix for [Bug 2903811]; note that the fix involved *removing* code.)
     */

    if (iPtr->varFramePtr == NULL) {
	return TCL_OK;
    }

    for (Tcl_Size i = Tcl_ObjectContextSkippedArgs(context) ; i < objc ; i++) {
	const char *varName = TclGetString(objv[i]);

	/*
	 * The variable name must not contain a '::' since that's illegal in
	 * local names.
	 */

	if (strstr(varName, "::") != NULL) {
	    TclPrintfResult(interp,
		    "variable name \"%s\" illegal: must not contain namespace"
		    " separator", varName);
	    TclSetErrorCode(interp, "TCL", "UPVAR", "INVERTED");
	    return TCL_ERROR;
	}

	/*
	 * Switch to the object's namespace for the duration of this call.
	 * Like this, the variable is looked up in the namespace of the
	 * object, and not in the namespace of the caller. Otherwise this
	 * would only work if the caller was a method of the object itself,
	 * which might not be true if the method was exported. This is a bit
	 * of a hack, but the simplest way to do this (pushing a stack frame
	 * would be horribly expensive by comparison).
	 */

	Namespace *savedNsPtr = iPtr->varFramePtr->nsPtr;
	iPtr->varFramePtr->nsPtr = (Namespace *)
		Tcl_GetObjectNamespace(object);
	Var *aryPtr, *varPtr = TclObjLookupVar(interp, objv[i], NULL,
		TCL_NAMESPACE_ONLY, "define", 1, 0, &aryPtr);
	iPtr->varFramePtr->nsPtr = savedNsPtr;

	if (varPtr == NULL || aryPtr != NULL) {
	    /*
	     * Variable cannot be an element in an array. If aryPtr is not
	     * NULL, it is an element, so throw up an error and return.
	     */

	    TclVarErrMsg(interp, varName, NULL, "define",
		    "name refers to an element in an array");
	    TclSetErrorCode(interp, "TCL", "UPVAR", "LOCAL_ELEMENT");
	    return TCL_ERROR;
	}

	/*
	 * Arrange for the lifetime of the variable to be correctly managed.
	 * This is copied out of Tcl_VariableObjCmd...
	 */

	if (!TclIsVarNamespaceVar(varPtr)) {
	    TclSetVarNamespaceVar(varPtr);
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
 * TclOOLookupObjectVar --
 *
 *	Look up a variable in an object. Tricky because of private variables.
 *
 * Returns:
 *	Handle to the variable if it can be found, or NULL if there's an error.
 *
 * ----------------------------------------------------------------------
 */
Tcl_Var
TclOOLookupObjectVar(
    Tcl_Interp *interp,
    Tcl_Object object,		/* Object we're looking up within. */
    Tcl_Obj *varName,		/* User-visible name we're looking up. */
    Tcl_Var *aryPtr)		/* Where to write the handle to the array
				 * containing the element; if not an element,
				 * then the variable this points to is set to
				 * NULL. */
{
    const char *arg = TclGetString(varName);
    Tcl_Obj *varNamePtr;

    /*
     * Convert the variable name to fully-qualified form if it wasn't already.
     * This has to be done prior to lookup because we can run into problems
     * with resolvers otherwise. [Bug 3603695]
     *
     * We still need to do the lookup; the variable could be linked to another
     * variable and we want the target's name.
     */

    if (arg[0] == ':' && arg[1] == ':') {
	varNamePtr = varName;
    } else {
	Tcl_Namespace *namespacePtr = Tcl_GetObjectNamespace(object);
	CallFrame *framePtr = ((Interp *) interp)->varFramePtr;

	/*
	 * Private method handling. [TIP 500]
	 *
	 * If we're in a context that can see some private methods of an
	 * object, we may need to precede a variable name with its prefix.
	 * This is a little tricky as we need to check through the inheritance
	 * hierarchy when the method was declared by a class to see if the
	 * current object is an instance of that class.
	 */

	if (framePtr->isProcCallFrame & FRAME_IS_METHOD) {
	    Object *oPtr = (Object *) object;
	    CallContext *callerContext = (CallContext *) framePtr->clientData;
	    Method *mPtr = callerContext->callPtr->chain[
		    callerContext->index].mPtr;

	    if (mPtr->declaringObjectPtr == oPtr) {
		PrivateVariableMapping *pvPtr;
		FOREACH_STRUCT(pvPtr, oPtr->privateVariables) {
		    if (!TclStringCmp(pvPtr->variableObj, varName, 1, 0,
			    TCL_INDEX_NONE)) {
			varName = pvPtr->fullNameObj;
			break;
		    }
		}
	    } else if (mPtr->declaringClassPtr &&
		    mPtr->declaringClassPtr->privateVariables.num) {
		Class *clsPtr = mPtr->declaringClassPtr;
		bool isInstance = TclOOIsReachable(clsPtr, oPtr->selfCls);

		if (!isInstance) {
		    Class *mixinCls;
		    FOREACH(mixinCls, oPtr->mixins) {
			if (TclOOIsReachable(clsPtr, mixinCls)) {
			    isInstance = true;
			    break;
			}
		    }
		}
		if (isInstance) {
		    PrivateVariableMapping *pvPtr;
		    FOREACH_STRUCT(pvPtr, clsPtr->privateVariables) {
			if (!TclStringCmp(pvPtr->variableObj, varName, 1, 0,
				TCL_INDEX_NONE)) {
			    varName = pvPtr->fullNameObj;
			    break;
			}
		    }
		}
	    }
	}

	// The namespace isn't the global one; necessarily true for any object!
	varNamePtr = Tcl_ObjPrintf("%s::%s",
		namespacePtr->fullName, TclGetString(varName));
    }

    Tcl_IncrRefCount(varNamePtr);
    Tcl_Var var = (Tcl_Var) TclObjLookupVar(interp, varNamePtr, NULL,
	    TCL_NAMESPACE_ONLY|TCL_LEAVE_ERR_MSG, "refer to", 1, 1,
	    (Var **) aryPtr);
    Tcl_DecrRefCount(varNamePtr);
    if (var == NULL) {
	TclSetErrorCode(interp, "TCL", "LOOKUP", "VARIABLE", arg);
    } else if (*aryPtr == NULL && TclIsVarArrayElement((Var *) var)) {
	/*
	 * If the varPtr points to an element of an array but we don't already
	 * have the array, find it now. Note that this can't be easily
	 * backported; the arrayPtr field is new in Tcl 9.0. [Bug 2da1cb0c80]
	 */
	*aryPtr = (Tcl_Var) TclVarParentArray(var);
    }

    return var;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOO_Object_VarName --
 *
 *	Implementation of the oo::object->varname method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOO_Object_VarName(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Interpreter in which to create the object;
				 * also used for error reporting. */
    Tcl_ObjectContext context,	/* The object/call context. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* The actual arguments. */
{
    if ((int) Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"varName");
	return TCL_ERROR;
    }

    Tcl_Var aryVar, varPtr = TclOOLookupObjectVar(interp,
	    Tcl_ObjectContextObject(context), objv[objc - 1], &aryVar);
    if (varPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * The variable reference must not disappear too soon. [Bug 74b6110204]
     */
    if (!TclIsVarArrayElement((Var *) varPtr)) {
	TclSetVarNamespaceVar((Var *) varPtr);
    }

    /*
     * Now that we've pinned down what variable we're really talking about
     * (including traversing variable links), convert back to a name.
     */

    Tcl_Obj *varNamePtr;
    TclNewObj(varNamePtr);

    if (aryVar != NULL) {
	Tcl_GetVariableFullName(interp, aryVar, varNamePtr);
	Tcl_AppendPrintfToObj(varNamePtr, "(%s)", Tcl_GetString(
		VarHashGetKey(varPtr)));
    } else {
	Tcl_GetVariableFullName(interp, varPtr, varNamePtr);
    }
    Tcl_SetObjResult(interp, varNamePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOONextObjCmd, TclOONextToObjCmd --
 *
 *	Implementation of the [next] and [nextto] commands. Note that these
 *	commands are only ever to be used inside the body of a procedure-like
 *	method.
 *
 * ----------------------------------------------------------------------
 */

int
TclOONextObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;

    /*
     * Start with sanity checks on the calling context to make sure that we
     * are invoked from a suitable method context. If so, we can safely
     * retrieve the handle to the object call context.
     */

    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	TclPrintfResult(interp, "%s may only be called from inside a method",
		TclGetString(objv[0]));
	OO_ERROR(interp, CONTEXT_REQUIRED);
	return TCL_ERROR;
    }
    Tcl_ObjectContext context = (Tcl_ObjectContext) framePtr->clientData;

    /*
     * Invoke the (advanced) method call context in the caller context. Note
     * that this is like [uplevel 1] and not [eval].
     */

    TclNRAddCallback(interp, NextRestoreFrame, framePtr);
    iPtr->varFramePtr = framePtr->callerVarPtr;
    return TclNRObjectContextInvokeNext(interp, context, objc, objv, 1);
}

int
TclOONextToObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;

    /*
     * Start with sanity checks on the calling context to make sure that we
     * are invoked from a suitable method context. If so, we can safely
     * retrieve the handle to the object call context.
     */

    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	TclPrintfResult(interp, "%s may only be called from inside a method",
		TclGetString(objv[0]));
	OO_ERROR(interp, CONTEXT_REQUIRED);
	return TCL_ERROR;
    }
    CallContext *contextPtr = (CallContext *) framePtr->clientData;

    /*
     * Sanity check the arguments; we need the first one to refer to a class.
     */

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "class ?arg...?");
	return TCL_ERROR;
    }
    Class *classPtr = TclOOGetClassFromObj(interp, objv[1]);
    if (classPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Search for an implementation of a method associated with the current
     * call on the call chain past the point where we currently are. Do not
     * allow jumping backwards!
     */

    for (Tcl_Size i=contextPtr->index+1 ; i<contextPtr->callPtr->numChain ; i++) {
	MInvoke *miPtr = &contextPtr->callPtr->chain[i];

	if (!miPtr->isFilter && miPtr->mPtr->declaringClassPtr == classPtr) {
	    /*
	     * Invoke the (advanced) method call context in the caller
	     * context. Note that this is like [uplevel 1] and not [eval].
	     */

	    TclNRAddCallback(interp, NextRestoreFrame, framePtr,
		    contextPtr, INT2PTR(contextPtr->index));
	    contextPtr->index = i - 1;
	    iPtr->varFramePtr = framePtr->callerVarPtr;
	    return TclNRObjectContextInvokeNext(interp,
		    (Tcl_ObjectContext) contextPtr, objc, objv, 2);
	}
    }

    /*
     * Generate an appropriate error message, depending on whether the value
     * is on the chain but unreachable, or not on the chain at all.
     */

    const char *methodType = TclOOContextTypeName(contextPtr);
    for (Tcl_Size i=contextPtr->index ; i != TCL_INDEX_NONE ; i--) {
	MInvoke *miPtr = &contextPtr->callPtr->chain[i];

	if (!miPtr->isFilter && miPtr->mPtr->declaringClassPtr == classPtr) {
	    TclPrintfResult(interp,
		    "%s implementation by \"%s\" not reachable from here",
		    methodType, TclGetString(objv[1]));
	    OO_ERROR(interp, CLASS_NOT_REACHABLE);
	    return TCL_ERROR;
	}
    }
    TclPrintfResult(interp, "%s has no non-filter implementation by \"%s\"",
	    methodType, TclGetString(objv[1]));
    OO_ERROR(interp, CLASS_NOT_THERE);
    return TCL_ERROR;
}

static int
NextRestoreFrame(
    void *data[],
    Tcl_Interp *interp,
    int result)
{
    Interp *iPtr = (Interp *) interp;
    CallContext *contextPtr = (CallContext *) data[1];

    iPtr->varFramePtr = (CallFrame *) data[0];
    if (contextPtr != NULL) {
	contextPtr->index = PTR2UINT(data[2]);
    }
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOSelfObjCmd --
 *
 *	Implementation of the [self] command, which provides introspection of
 *	the call context.
 *
 * ----------------------------------------------------------------------
 */

static inline Tcl_Obj *
RenderMethodName(
    CallContext *contextPtr,
    Method *mPtr)
{
    if (contextPtr->callPtr->flags & CONSTRUCTOR) {
	return contextPtr->oPtr->fPtr->constructorName;
    } else if (contextPtr->callPtr->flags & DESTRUCTOR) {
	return contextPtr->oPtr->fPtr->destructorName;
    } else {
	return mPtr->namePtr;
    }
}

int
TclOOSelfObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    static const char *const subcmds[] = {
	"call", "caller", "class", "filter", "method", "namespace", "next",
	"object", "target", NULL
    };
    enum SelfCmds {
	SELF_CALL, SELF_CALLER, SELF_CLASS, SELF_FILTER, SELF_METHOD, SELF_NS,
	SELF_NEXT, SELF_OBJECT, SELF_TARGET
    } index;
    Interp *iPtr = (Interp *) interp;
    CallFrame *framePtr = iPtr->varFramePtr;

    /*
     * Start with sanity checks on the calling context and the method context.
     */

    if (framePtr == NULL || !(framePtr->isProcCallFrame & FRAME_IS_METHOD)) {
	TclPrintfResult(interp, "%s may only be called from inside a method",
		TclGetString(objv[0]));
	OO_ERROR(interp, CONTEXT_REQUIRED);
	return TCL_ERROR;
    }

    CallContext *contextPtr = (CallContext *) framePtr->clientData;

#define CurrentlyInvoked(contextPtr) \
    ((contextPtr)->callPtr->chain[(contextPtr)->index])

    /*
     * Now we do "conventional" argument parsing for a while. Note that no
     * subcommand takes arguments.
     */

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand");
	return TCL_ERROR;
    } else if (objc == 1) {
	index = SELF_OBJECT;
    } else if (Tcl_GetIndexFromObj(interp, objv[1], subcmds, "subcommand", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case SELF_OBJECT:
	Tcl_SetObjResult(interp, TclOOObjectName(interp, contextPtr->oPtr));
	return TCL_OK;
    case SELF_NS:
	Tcl_SetObjResult(interp,
		TclNewNamespaceObj(contextPtr->oPtr->namespacePtr));
	return TCL_OK;
    case SELF_CLASS: {
	Class *clsPtr = CurrentlyInvoked(contextPtr).mPtr->declaringClassPtr;

	if (clsPtr == NULL) {
	    TclPrintfResult(interp, "method not defined by a class");
	    OO_ERROR(interp, UNMATCHED_CONTEXT);
	    return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, TclOOObjectName(interp, clsPtr->thisPtr));
	return TCL_OK;
    }
    case SELF_METHOD:
	Tcl_SetObjResult(interp, RenderMethodName(contextPtr,
		CurrentlyInvoked(contextPtr).mPtr));
	return TCL_OK;
    case SELF_FILTER:
	if (!CurrentlyInvoked(contextPtr).isFilter) {
	    TclPrintfResult(interp, "not inside a filtering context");
	    OO_ERROR(interp, UNMATCHED_CONTEXT);
	    return TCL_ERROR;
	} else {
	    MInvoke *miPtr = &CurrentlyInvoked(contextPtr);
	    Object *oPtr;
	    const char *type;

	    if (miPtr->filterDeclarer != NULL) {
		oPtr = miPtr->filterDeclarer->thisPtr;
		type = "class";
	    } else {
		oPtr = contextPtr->oPtr;
		type = "object";
	    }

	    Tcl_Obj *result[] = {
		TclOOObjectName(interp, oPtr),
		Tcl_NewStringObj(type, TCL_AUTO_LENGTH),
		miPtr->mPtr->namePtr
	    };
	    Tcl_SetObjResult(interp, Tcl_NewListObj(3, result));
	    return TCL_OK;
	}
    case SELF_CALLER:
	if ((framePtr->callerVarPtr == NULL) ||
		!(framePtr->callerVarPtr->isProcCallFrame & FRAME_IS_METHOD)){
	    TclPrintfResult(interp, "caller is not an object");
	    OO_ERROR(interp, CONTEXT_REQUIRED);
	    return TCL_ERROR;
	} else {
	    CallContext *callerPtr = (CallContext *)
		    framePtr->callerVarPtr->clientData;
	    Method *mPtr = callerPtr->callPtr->chain[callerPtr->index].mPtr;
	    Object *declarerPtr;

	    if (mPtr->declaringClassPtr != NULL) {
		declarerPtr = mPtr->declaringClassPtr->thisPtr;
	    } else if (mPtr->declaringObjectPtr != NULL) {
		declarerPtr = mPtr->declaringObjectPtr;
	    } else {
		TCL_UNREACHABLE();
	    }

	    Tcl_Obj *result[] = {
		TclOOObjectName(interp, declarerPtr),
		TclOOObjectName(interp, callerPtr->oPtr),
		RenderMethodName(callerPtr, mPtr)
	    };
	    Tcl_SetObjResult(interp, Tcl_NewListObj(3, result));
	    return TCL_OK;
	}
    case SELF_NEXT:
	if (contextPtr->index < contextPtr->callPtr->numChain - 1) {
	    Method *mPtr =
		    contextPtr->callPtr->chain[contextPtr->index + 1].mPtr;
	    Object *declarerPtr;

	    if (mPtr->declaringClassPtr != NULL) {
		declarerPtr = mPtr->declaringClassPtr->thisPtr;
	    } else if (mPtr->declaringObjectPtr != NULL) {
		declarerPtr = mPtr->declaringObjectPtr;
	    } else {
		TCL_UNREACHABLE();
	    }

	    Tcl_Obj *result[] = {
		TclOOObjectName(interp, declarerPtr),
		RenderMethodName(contextPtr, mPtr)
	    };
	    Tcl_SetObjResult(interp, Tcl_NewListObj(2, result));
	}
	return TCL_OK;
    case SELF_TARGET:
	if (!CurrentlyInvoked(contextPtr).isFilter) {
	    TclPrintfResult(interp, "not inside a filtering context");
	    OO_ERROR(interp, UNMATCHED_CONTEXT);
	    return TCL_ERROR;
	} else {
	    Tcl_Size i;

	    for (i=contextPtr->index ; i<contextPtr->callPtr->numChain ; i++) {
		if (!contextPtr->callPtr->chain[i].isFilter) {
		    break;
		}
	    }
	    if (i == contextPtr->callPtr->numChain) {
		Tcl_Panic("filtering call chain without terminal non-filter");
	    }
	    Method *mPtr = contextPtr->callPtr->chain[i].mPtr;
	    Object *declarerPtr;
	    if (mPtr->declaringClassPtr != NULL) {
		declarerPtr = mPtr->declaringClassPtr->thisPtr;
	    } else if (mPtr->declaringObjectPtr != NULL) {
		declarerPtr = mPtr->declaringObjectPtr;
	    } else {
		TCL_UNREACHABLE();
	    }
	    Tcl_Obj *result[] = {
		TclOOObjectName(interp, declarerPtr),
		mPtr->namePtr
	    };
	    Tcl_SetObjResult(interp, Tcl_NewListObj(2, result));
	    return TCL_OK;
	}
    case SELF_CALL:
	Tcl_Obj *result[] = {
	    TclOORenderCallChain(interp, contextPtr->callPtr),
	    Tcl_NewWideIntObj(contextPtr->index)
	};
	Tcl_SetObjResult(interp, Tcl_NewListObj(2, result));
	return TCL_OK;
    default:
	TCL_UNREACHABLE();
    }
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

int
TclOOCopyObjectCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    if (objc < 2 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"sourceName ?targetName? ?targetNamespace?");
	return TCL_ERROR;
    }

    Tcl_Object oPtr = Tcl_GetObjectFromObj(interp, objv[1]);
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

    Tcl_Object o2Ptr;
    if (objc == 2) {
	o2Ptr = Tcl_CopyObjectInstance(interp, oPtr, NULL, NULL);
    } else {
	const char *name = TclGetString(objv[2]);
	if (name[0] == '\0') {
	    name = NULL;
	}

	/*
	 * Choose a unique namespace name if the user didn't supply one.
	 */

	const char *namespaceName = NULL;
	if (objc == 4) {
	    namespaceName = TclGetString(objv[3]);

	    if (namespaceName[0] == '\0') {
		namespaceName = NULL;
	    } else if (Tcl_FindNamespace(interp, namespaceName, NULL,
		    0) != NULL) {
		TclPrintfResult(interp, "%s refers to an existing namespace",
			namespaceName);
		return TCL_ERROR;
	    }
	}

	o2Ptr = Tcl_CopyObjectInstance(interp, oPtr, name, namespaceName);
    }

    if (o2Ptr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Return the name of the cloned object.
     */

    Tcl_SetObjResult(interp, TclOOObjectName(interp, (Object *) o2Ptr));
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
