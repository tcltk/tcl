/*
 * tclOO.c --
 *
 *	This file contains the method call chain management code for the
 *	object-system core.
 *
 * Copyright (c) 2005-2006 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOOCall.c,v 1.1.2.1 2006/10/08 15:39:59 dkf Exp $
 */

#include "tclInt.h"
#include "tclOO.h"

/*
 * Extra flags used for call chain management.
 */

#define DEFINITE_PRIVATE 0x100000
#define DEFINITE_PUBLIC  0x200000
#define KNOWN_STATE	 (DEFINITE_PRIVATE | DEFINITE_PUBLIC)
#define SPECIAL		 (CONSTRUCTOR | DESTRUCTOR)

/*
 * Function declarations for things defined in this file.
 */

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
static void		InitClassHierarchy(Foundation *fPtr, Class *classPtr);

/*
 * ----------------------------------------------------------------------
 *
 * TclOODeleteContext --
 *
 *	Destroys a method call-chain context, which should not be in use.
 *
 * ----------------------------------------------------------------------
 */

void
TclOODeleteContext(
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
 * TclOOInvokeContext --
 *
 *	Invokes a single step along a method call-chain context. Note that the
 *	invokation of a step along the chain can cause further steps along the
 *	chain to be invoked. Note that this function is written to be as light
 *	in stack usage as possible.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOInvokeContext(
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
    int isFilter = contextPtr->callChain[contextPtr->index].isFilter;
    int wasFilter;

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

    /*
     * Save whether we were in a filter and set up whether we are now.
     */

    wasFilter = contextPtr->oPtr->flags & FILTER_HANDLING;
    if (isFilter || contextPtr->flags & FILTER_HANDLING) {
	contextPtr->oPtr->flags |= FILTER_HANDLING;
    } else {
	contextPtr->oPtr->flags &= ~FILTER_HANDLING;
    }

    /*
     * Run the method implementation.
     */

    result = mPtr->typePtr->callProc(mPtr->clientData, interp,
	    (Tcl_ObjectContext) contextPtr, objc, objv);

    /*
     * Restore the old filter-ness, release any locks on method
     * implementations, and return the result code.
     */

    if (wasFilter) {
	contextPtr->oPtr->flags |= FILTER_HANDLING;
    } else {
	contextPtr->oPtr->flags &= ~FILTER_HANDLING;
    }
    if (isFirst) {
	int i;

	for (i=0 ; i<contextPtr->numCallChain ; i++) {
	    Tcl_Release(contextPtr->callChain[i].mPtr);
	}
    }
    return result;
}

static void
InitClassHierarchy(
    Foundation *fPtr,
    Class *classPtr)
{
    if (classPtr == fPtr->objectCls) {
	return;
    }
    if (classPtr->classHierarchyEpoch != fPtr->epoch) {
	int i;
	Class *superPtr;

	if (classPtr->classHierarchy.num != 0) {
	    ckfree((char *) classPtr->classHierarchy.list);
	}
	FOREACH(superPtr, classPtr->superclasses) {
	    InitClassHierarchy(fPtr, superPtr);
	}
	if (i == 1) {
	    Class **hierlist = (Class **)
		    ckalloc(sizeof(Class*) * (1+superPtr->classHierarchy.num));

	    hierlist[0] = superPtr;
	    memcpy(hierlist+1, superPtr->classHierarchy.list,
		    sizeof(Class*) * superPtr->classHierarchy.num);
	    classPtr->classHierarchy.num = 1 + superPtr->classHierarchy.num;
	    classPtr->classHierarchy.list = hierlist;
	    classPtr->classHierarchyEpoch = fPtr->epoch;
	    return;
	} else {
	    int num = classPtr->superclasses.num, j = 0, k, realNum;
	    Class **hierlist;		/* Temporary work space. */

	    FOREACH(superPtr, classPtr->superclasses) {
		num += superPtr->classHierarchy.num;
	    }
	    hierlist = (Class **) ckalloc(sizeof(Class *) * num);
	    FOREACH(superPtr, classPtr->superclasses) {
		hierlist[j++] = superPtr;
		if (superPtr == fPtr->objectCls) {
		    continue;
		}
		memcpy(hierlist+j, superPtr->classHierarchy.list,
			sizeof(Class *) * superPtr->classHierarchy.num);
		j += superPtr->classHierarchy.num;
	    }
	    realNum = num;
	    for (j=0 ; j<num-1 ; j++) {
		for (k=num-1 ; k>j ; k--) {
		    if (hierlist[j] == hierlist[k]) {
			hierlist[j] = NULL;
			realNum--;
			break;
		    }
		}
	    }
	    classPtr->classHierarchy.num = realNum;
	    classPtr->classHierarchy.list = (Class **)
		    ckalloc(sizeof(Class *) * realNum);
	    for (j=k=0 ; j<num ; j++) {
		if (hierlist[j] != NULL) {
		    classPtr->classHierarchy.list[k++] = hierlist[j];
		}
	    }
	    ckfree((char *) hierlist);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOGetSortedMethodList --
 *
 *	Discovers the list of method names supported by an object.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOGetSortedMethodList(
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
     * Scope all declarations so that the compiler can stand a good chance of
     * making the recursive step highly efficient. We also hand-implement the
     * tail-recursive case using a while loop; C compilers typically cannot do
     * tail-recursion optimization usefully.
     */

    if (clsPtr->mixins.num != 0) {
	Class *mixinPtr;
	int i;

	// TODO: Beware of infinite loops!
	FOREACH(mixinPtr, clsPtr->mixins) {
	    AddClassMethodNames(mixinPtr, publicOnly, namesPtr);
	}
    }

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
 * TclOOGetCallContext --
 *
 *	Responsible for constructing the call context, an ordered list of all
 *	method implementations to be called as part of a method invokation.
 *	This method is central to the whole operation of the OO system.
 *
 * ----------------------------------------------------------------------
 */

CallContext *
TclOOGetCallContext(
    Foundation *fPtr,		/* The foundation of the object system. */
    Object *oPtr,		/* The object to get the context for. */
    Tcl_Obj *methodNameObj,	/* The name of the method to get the context
				 * for. NULL when getting a constructor or
				 * destructor chain. */
    int flags,			/* What sort of context are we looking for.
				 * Only the bits OO_PUBLIC_METHOD,
				 * CONSTRUCTOR, DESTRUCTOR and FILTER_HANDLING
				 * are useful. */
    Tcl_HashTable *cachePtr)	/* Where to cache the chain. Ignored for both
				 * constructors and destructors. */
{
    CallContext *contextPtr;
    int i, count, doFilters;
    Tcl_HashEntry *hPtr;
    Tcl_HashTable doneFilters;

    if (flags&(SPECIAL|FILTER_HANDLING) || (oPtr->flags&FILTER_HANDLING)) {
	hPtr = NULL;
	doFilters = 0;
    } else {
	doFilters = 1;
	hPtr = Tcl_FindHashEntry(cachePtr, (char *) methodNameObj);
	if (hPtr != NULL && Tcl_GetHashValue(hPtr) != NULL) {
	    contextPtr = Tcl_GetHashValue(hPtr);
	    Tcl_SetHashValue(hPtr, NULL);
	    if ((contextPtr->globalEpoch == fPtr->epoch)
		    && (contextPtr->localEpoch == oPtr->epoch)) {
		return contextPtr;
	    }
	    TclOODeleteContext(contextPtr);
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
    if (flags & (PUBLIC_METHOD | SPECIAL | FILTER_HANDLING)) {
	contextPtr->flags |= flags & (PUBLIC_METHOD|SPECIAL|FILTER_HANDLING);
    }
    contextPtr->oPtr = oPtr;
    contextPtr->index = 0;

    /*
     * Ensure that the class hierarchy is trivially iterable.
     */

    InitClassHierarchy(fPtr, oPtr->selfCls);

    /*
     * Add all defined filters (if any, and if we're going to be processing
     * them; they're not processed for constructors, destructors or when we're
     * in the middle of processing a filter).
     */

    if (doFilters) {
	Tcl_Obj *filterObj;
	Class *mixinPtr;

	doFilters = 1;
	Tcl_InitObjHashTable(&doneFilters);
	FOREACH(mixinPtr, oPtr->mixins) {
	    AddClassFiltersToCallContext(oPtr, mixinPtr, contextPtr,
		    &doneFilters);
	}
	FOREACH(filterObj, oPtr->filters) {
	    AddSimpleChainToCallContext(oPtr, filterObj, contextPtr,
		    &doneFilters, 0);
	}
	AddClassFiltersToCallContext(oPtr, oPtr->selfCls, contextPtr,
		&doneFilters);
	Tcl_DeleteHashTable(&doneFilters);
    }
    count = contextPtr->filterLength = contextPtr->numCallChain;

    /*
     * Add the actual method implementations.
     */

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

	if (flags & SPECIAL) {
	    TclOODeleteContext(contextPtr);
	    return NULL;
	}
	AddSimpleChainToCallContext(oPtr, fPtr->unknownMethodNameObj,
		contextPtr, NULL, 0);
	contextPtr->flags |= OO_UNKNOWN_METHOD;
	contextPtr->globalEpoch = -1;
	if (count == contextPtr->numCallChain) {
	    TclOODeleteContext(contextPtr);
	    return NULL;
	}
    } else if (doFilters) {
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

    if (!(flags & (KNOWN_STATE | SPECIAL))) {
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
    if (!(flags & SPECIAL)) {
	Tcl_HashEntry *hPtr;
	Class *mixinPtr, *superPtr;

	FOREACH(mixinPtr, oPtr->mixins) {
	    AddSimpleClassChainToCallContext(mixinPtr, methodNameObj,
		    contextPtr, doneFilters, flags);
	}
	FOREACH(mixinPtr, oPtr->selfCls->mixins) {
	    AddSimpleClassChainToCallContext(mixinPtr, methodNameObj,
		    contextPtr, doneFilters, flags);
	}
	FOREACH(superPtr, oPtr->selfCls->classHierarchy) {
	    int j=i;// HACK: save index so we can nest FOREACHes
	    FOREACH(mixinPtr, superPtr->mixins) {
		AddSimpleClassChainToCallContext(mixinPtr, methodNameObj,
			contextPtr, doneFilters, flags);
	    }
	    i=j;
	}
	hPtr = Tcl_FindHashEntry(&oPtr->methods, (char *) methodNameObj);
	if (hPtr != NULL) {
	    AddMethodToCallChain(Tcl_GetHashValue(hPtr), contextPtr,
		    doneFilters);
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
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
