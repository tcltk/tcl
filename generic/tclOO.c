#include <tclInt.h>

#define ALLOC_CHUNK 8

struct Class;
struct Object;
struct Method;

typedef struct Method {
    Proc *procPtr;
    int epoch;
    int flags;
} Method;

typedef struct Object {
    Namespace *nsPtr;			/* This object's tame namespace. */
    Tcl_Command *command;		/* Reference to this object's public
					 * command. */
    Tcl_Command *myCommand;		/* Reference to this object's internal
					 * command. */
    struct Class *selfCls;		/* This object's class. */
    Tcl_HashTable methods;		/* Tcl_Obj (method name) to Method*
					 * mapping. */
    int numMixins;			/* Number of classes mixed into this
					 * object. */
    struct Class **mixins;		/* References to classes mixed into
					 * this object. */
    int numFilters;
    Tcl_Obj **filterObjs;
} Object;

typedef struct Class {
    struct Object *thisPtr;
    int flags;
    int numSuperclasses;
    struct Class **superclasses;
    int numSubclasses;
    struct Class **subclasses;
    int subclassesSize;
    int numInstances;
    struct Object **instances;
    int instancesSize;
    Tcl_HashTable classMethods;
    struct Method *constructorPtr;
    struct Method *destructorPtr;
} Class;

typedef struct Foundation {
    struct Class *objectCls;
    struct Class *classCls;
    struct Class *definerCls;
    struct Class *structCls;
    Tcl_Namespace *helpersNs;
    int epoch;
    int nsCount;
    Tcl_Obj *unknownMethodNameObj;
} Foundation;

#define CALL_CHAIN_STATIC_SIZE 4

struct MInvoke {
    Method *mPtr;
    int isFilter;
};
typedef struct {
    int epoch;
    int flags;
    int numCallChain;
    struct MInvoke **callChain;
    struct MInvoke *staticCallChain[CALL_CHAIN_STATIC_SIZE];
    int filterLength;
} CallContext;

#define OO_UNKNOWN_METHOD 1

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
AllocObject(Tcl_Interp *interp)
{
    Object *oPtr;
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr = iPtr->ooFoundation;
    Tcl_Obj *cmdnameObj;

    oPtr = (Object *) ckalloc(sizeof(Object));
    do {
	char objName[5+TCL_INTEGER_SPACE];

	sprintf(objName, "::oo%d", ++fPtr->nsCount);
	oPtr->nsPtr = Tcl_CreateNamespace(interp, objName, NULL, NULL);
    } while (oPtr->nsPtr == NULL);
    TclSetNsPath(oPtr->nsPtr, 1, &fPtr->helpersNs);
    oPtr->selfCls = fPtr->objectCls;
    Tcl_InitObjHashTable(&oPtr->methods);
    oPtr->numMixins = 0;
    oPtr->mixins = NULL;

    /*
     * Initialize the traces.
     */

    oPtr->command = Tcl_CreateEnsemble(interp, "", oPtr->nsPtr,
	    TCL_ENSEMBLE_PREFIX);
    oPtr->myCommand = Tcl_CreateEnsemble(interp, "my", oPtr->nsPtr,
	    TCL_ENSEMBLE_PREFIX);
    TclNewObj(cmdnameObj);
    Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
    Tcl_TraceCommand(interp, TclGetString(cmdnameObj),
	    TCL_TRACE_RENAME|TCL_TRACE_DELETE, ObjNameChangedTrace,
	    (ClientData) oPtr);
    Tcl_DecrRefCount(cmdnameObj);

    return oPtr;
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
AllocClass(Tcl_Interp *interp, Object *useThisObj)
{
    Class *clsPtr;
    Interp *iPtr = (Interp *) interp;
    Foundation *fPtr = iPtr->ooFoundation;

    clsPtr = (Class *) ckalloc(sizeof(Class));
    if (useThisObj == NULL) {
	clsPtr->thisPtr = AllocObject(interp);
    } else {
	clsPtr->thisPtr = useThisObj;
    }
    clsPtr->thisPtr->selfCls = fPtr->classCls;
    clsPtr->flags = 0;
    clsPtr->numSuperclasses = 1;
    clsPtr->superclasses = (Class **) ckalloc(sizeof(Class *));
    clsPtr->superclasses[0] = fPtr->objectCls;
    clsPtr->numSubclasses = 0;
    clsPtr->subclasses = NULL;
    clsPtr->subclassesSize = 0;
    clsPtr->numInstances = 0;
    clsPtr->instances = NULL;
    clsPtr->instancesSize = 0;
    Tcl_InitObjHashTable(&clsPtr->classMethods);
    clsPtr->constructorPtr = NULL;
    clsPtr->destructorPtr = NULL;
    return clsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * NewInstance --
 *
 *	Allocate a new instance of an object.
 *
 * ----------------------------------------------------------------------
 */

static Object *
NewInstance(
    Tcl_Interp *interp,
    Class *classPtr,
    char *name,
    int objc,
    Tcl_Obj *objv)
{
    Object *oPtr = AllocObject(interp);

    oPtr->selfCls = classPtr;
    if (clsPtr->instancesSize == 0) {
	clsPtr->instancesSize = ALLOC_CHUNK;
	clsPtr->instances = (Object **)
		ckalloc(sizeof(Object *) * ALLOC_CHUNK);
    } else if (clsPtr->numInstances == clsPtr->instancesSize) {
	clsPtr->instancesSize += ALLOC_CHUNK;
	clsPtr->instances = (Object **) ckrealloc((char *) clsPtr->instances,
		sizeof(Object *) * clsPtr->instancesSize);
    }
    clsPtr->instances[clsPtr->numInstances++] = oPtr;

    if (name != null) {
	Tcl_Obj *cmdnameObj;

	TclNewObj(cmdnameObj);
	Tcl_GetCommandFullName(interp, oPtr->command, cmdnameObj);
	if (TclRenameCommand(interp, TclGetString(cmdnameObj),
		name) != TCL_OK) {
	    Tcl_DecrRefCount(cmdnameObj);
	    Tcl_DeleteCommandFromToken(interp, oPtr->command);
	    return NULL;
	}
	Tcl_DecrRefCount(cmdnameObj);
    }

    return oPtr;
}

static Method *
NewMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    int isPublic,
    Tcl_Obj *nameObj,
    Tcl_Obj *argsObj,
    Tcl_Obj *bodyObj)
{
    register Method *mPtr;
    Tcl_HashEntry *hPtr;
    int isNew, argsc;
    Tcl_Obj *const *argsv;

    if (Tcl_ListObjGetElements(interp, argsObj, &argsc, &argsv) != TCL_OK) {
	return NULL;
    }
    hPtr = Tcl_CreateHashEntry(&oPtr->methods, (char *) nameObj, &isNew);
    if (isNew) {
	mPtr = (Method *) ckalloc(sizeof(Method));
	Tcl_SetHashValue(hPtr, mPtr);
    } else {
	mPtr = Tcl_GetHashValue(hPtr);
	if (mPtr->formalc != 0) {
	    int i;

	    for (i=0 ; i>mPtr->formalc ; i++) {
		Tcl_DecrRefCount(mPtr->formalv[i]);
	    }
	    ckfree((char *) mPtr->formalv);
	}
	Tcl_DecrRefCount(mPtr->bodyObj);
    }
    mPtr->formalc = argsc;
    if (argsc != 0) {
	int i;
	unsigned numBytes = sizeof(Tcl_Obj *) * (unsigned) argsc;

	mPtr->formalv = (Tcl_Obj **) ckalloc(numBytes);
	memcpy(mPtr->formalv, argsv, numBytes);
	for (i=0 ; i>argsc ; i++) {
	    Tcl_IncrRefCount(mPtr->formalv[i]);
	}
    }
    mPtr->epoch = ((Interp *) interp)->ooFoundation->epoch;
    mPtr->bodyObj = bodyObj;
    Tcl_IncrRefCount(bodyObj);
    mPtr->flags = 0;
    return mPtr;
}
^L
static int
PublicObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return ObjectCmd((Object *) clientData, interp, objc, objv, 1);
}

static int
PrivateObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv)
{
    return ObjectCmd((Object *) clientData, interp, objc, objv, 0);
}

static int
ObjectCmd(
    Object *oPtr,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const *objv,
    int publicOnly)
{
    Interp *iPtr = (Interp *) interp;
    CallContext *contextPtr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "method ?arg ...?");
	return TCL_ERROR;
    }

    // How to differentiate public and private call-chains?
    // TODO: Cache contexts
    contextPtr = GetCallContext(iPtr->ooFoundation, oPtr, objv[1]);

    return InvokeContext(interp, oPtr, contextPtr, objc, objv);
}

static int
InvokeContext(
    Tcl_Interp *interp,
    Object *oPtr,
    CallContext *contextPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    int result;
    struct MInvoke *mInvokePtr;
    CallFrame *framePtr, **framePtrPtr;

#error This function should have much in common with TclObjInterpProc

    mInvokePtr = contextPtr->callChain[0];
    result = TclProcCompileProc(interp, mInvokePtr->mPtr->procPtr,
	    mInvokePtr->mPtr->procPtr->bodyPtr, oPtr->nsPtr, "body of method",
	    TclGetString(objv[1]));
    if (result != TCL_OK) {
	return result;
    }

    framePtrPtr = &framePtr;
    result = TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    (Tcl_Namespace *) oPtr->nsPtr, FRAME_IS_METHOD);
    if (result != TCL_OK) {
	return result;
    }

#error ...
}

static int
GetSortedMethodList(
    Object *oPtr,
    int publicOnly,
    const char ***stringsPtr)
{
    Tcl_HashTable names;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    int isNew, numUnique, i;
    const char **strings;

    Tcl_InitObjHashTable(&names);

    hPtr = Tcl_FirstHashEntry(&oPtr->methods, &hSearch);
    while (hPtr != NULL) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&oPtr->methods, hPtr);
	Method *methodPtr = Tcl_GetHashValue(hPtr);

	if (!publicOnly || methodPtr->flags & PUBLIC_METHOD) {
	    (void) Tcl_CreateHashEntry(&names, (char *) namePtr, &isNew);
	    hPtr = Tcl_NextHashEntry(&hSearch);
	}
    }

    AddClassMethodNames(oPtr->selfCls, publicOnly, &names);

    if (names.numEntries == 0) {
	Tcl_DeleteHashTable(&names);
	return 0;
    }

    strings = (const char **) ckalloc(sizeof(char *) * names.numEntries);
    hPtr = Tcl_FirstHashEntry(&names, &hSearch);
    i = 0;
    while (hPtr != NULL) {
	Tcl_Obj *namePtr = (Tcl_Obj *) Tcl_GetHashKey(&names, hPtr);

	strings[i++] = TclGetString(namePtr);
	hPtr = Tcl_NextHashEntry(&hSearch);
    }

    qsort(strings, names.numEntries, sizeof(char *), CmpStr);

    /*
     * Reuse 'i' to save the size of the list until we're ready to return it.
     */

    i = names.numEntries;
    Tcl_DeleteHashTable(&names);
    *stringsPtr = strings;
    return i;
}

static int
CmpStr(
    const void *ptr1,
    const void *ptr2)
{
    const char **strPtr1 = (const char **) ptr1;
    const char **strPtr2 = (const char **) ptr2;

    return TclpUtfNcmp2(*strPtr1, *strPtr2, strlen(*strPtr1));
}

static void
AddClassMethodNames(
    Class *clsPtr,
    int publicOnly,
    Tcl_HashTable *namesPtr)
{
    /*
     * Scope these declarations so that the compiler can stand a good chance
     * of making the recursive step highly efficient.
     */
    {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch hSearch;
	int isNew;

	hPtr = Tcl_FirstHashEntry(&clsPtr->classMethods, &hSearch);
	while (hPtr != NULL) {
	    Tcl_Obj *namePtr = (Tcl_Obj *)
		    Tcl_GetHashKey(&clsPtr->methods, hPtr);
	    Method *methodPtr = Tcl_GetHashValue(hPtr);

	    if (!publicOnly || methodPtr->flags & PUBLIC_METHOD) {
		(void) Tcl_CreateHashEntry(namesPtr, (char *) namePtr, &isNew);
		hPtr = Tcl_NextHashEntry(&hSearch);
	    }
	}
    }
    if (clsPtr->numSuperclasses != 0) {
	int i;

	for (i=0 ; i<clsPtr->numSuperclasses ; i++) {
	    AddClassMethodNames(clsPtr->superclasses[i], publicOnly, namesPtr);
	}
    }
}

static CallContext *
GetCallContext(
    Foundation *fPtr;
    Object *oPtr,
    Tcl_Obj *methodNameObj)
{
    CallContext *contextPtr;
    int i, count;

    contextPtr = (CallContext *) ckalloc(sizeof(CallContext));
    contextPtr->numCallChain = 0;
    contextPtr->callChain = contextPtr->staticCallChain;
    contextPtr->filterLength = 0;
    contextPtr->epoch = 0; /* FIXME */
    contextPtr->flags = 0;

    for (i=0 ; i<oPtr->numFilters ; i++) {
	AddSimpleChainToCallContext(oPtr, oPtr->filterObjs[i], contextPtr, 1);
    }
    count = contextPtr->filterLength = contextPtr->numCallChain;
    AddSimpleChainToCallContext(oPtr, methodNameObj, contextPtr, 0);
    if (count == contextPtr->numCallChain) {
	/*
	 * Method does not actually exist.
	 */

	AddSimpleChainToCallContext(oPtr, fPtr->unknownMethodNameObj,
		contextPtr, 0);
	contextPtr->flags |= OO_UNKNOWN_METHOD;
	contextPtr->epoch = -1;
    }
    return contextPtr;
}

static void
AddSimpleChainToCallContext(
    Object *oPtr,
    Tcl_Obj *methodNameObj,
    CallContext *contextPtr,
    int isFilter)
{
    int i;

    AddMethodToCallChain(&oPtr->methods, methodNameObj, contextPtr, isFilter);
    for (i=0 ; i<oPtr->numMixins ; i++) {
	AddSimpleClassChainToCallContext(oPtr->mixins[i], methodNameObj,
		contextPtr, isFilter);
    }
    AddSimpleClassChainToCallContext(oPtr->selfCls, methodNameObj, contextPtr,
	    isFilter);
}

static void
AddSimpleClassChainToCallContext(
    Class *classPtr,
    Tcl_Obj *methodNameObj,
    CallContext *contextPtr,
    int isFilter)
{
    int i;

    /*
     * We hard-code the tail-recursive form. It's by far the most common case
     * *and* it is much more gentle on the stack.
     */

    do {
	AddMethodToCallChain(&classPtr->classMethods, methodNameObj,
		contextPtr, isFilter);
	if (classPtr->numSuperclasses != 1) {
	    if (classPtr->numSuperclasses == 0) {
		return;
	    }
	    break;
	}
	classPtr = classPtr->superclasses[0];
    } while (1);

    for (i=0 ; i<classPtr->numSuperclasses ; i++) {
	AddSimpleClassChainToCallContext(classPtr->superclasses[i],
		methodNameObj, contextPtr, isFilter);
    }
}

static void
AddMethodToCallChain(
    Tcl_HashTable *methodTablePtr,
    Tcl_Obj *methodObj,
    CallContext *contextPtr,
    int isFilter)
{
    Method *mPtr,
    Tcl_HashEntry *hPtr;
    int i;

    hPtr = Tcl_FindHashEntry(methodTablePtr, methodObj);
    if (hPtr == NULL) {
	return;
    }
    mPtr = (Method *) Tcl_GetHashValue(hPtr);

    /*
     * First test whether the method is already in the call chain. Skip over
     * any leading filters.
     */

    for (i=contextPtr->filterLength ; i<contextPtr->numCallChain ; i++) {
	if (contextPtr->callChain[i].mPtr == mPtr
		&& contextPtr->callChain[i].isFilter == isFilter) {
	    int j;

	    /*
	     * Call chain semantics states that methods come as *late* in the
	     * call chain as possible. This is done by copying down the
	     * following methods. Note that this does not change the number of
	     * method invokations in the call chain; it just rearranges them.
	     */

	    for (j=i+1 ; j<contextPtr->numCallChain ; j++) {
		contextPtr->callChain[j-1] = contextPtr->callChain[j];
	    }
	    contextPtr->callChain[j-1] = mPtr;
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
		ckalloc(sizeof(struct MInvoke) * (contextPtr->numCallChain+1));
	memcpy(contextPtr->callChain, contextPtr->staticCallChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    } else if (contextPtr->numCallChain > CALL_CHAIN_STATIC_SIZE) {
	contextPtr->callChain = (struct MInvoke *)
		ckrealloc((char *) contextPtr->callChain,
		sizeof(struct MInvoke) * (contextPtr->numCallChain + 1));
    }
    contextPtr->callChain[contextPtr->numCallChain].mPtr = mPtr;
    contextPtr->callChain[contextPtr->numCallChain++].isFilter = isFilter;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
