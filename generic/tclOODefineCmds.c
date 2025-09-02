/*
 * tclOODefineCmds.c --
 *
 *	This file contains the implementation of the ::oo::define command,
 *	part of the object-system core (NB: not Tcl_Obj, but ::oo).
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

/*
 * The actual value used to mark private declaration frames.
 */

#define PRIVATE_FRAME (FRAME_IS_OO_DEFINE | FRAME_IS_PRIVATE_DEFINE)

/*
 * The maximum length of fully-qualified object name to use in an errorinfo
 * message. Longer than this will be curtailed.
 */

#define OBJNAME_LENGTH_IN_ERRORINFO_LIMIT 30

/*
 * Some things that make it easier to declare a slot.
 */
typedef struct DeclaredSlot {
    const char *name;
    const Tcl_MethodType2 getterType;
    const Tcl_MethodType2 setterType;
    const Tcl_MethodType2 resolverType;
} DeclaredSlot;

#define SLOT(name,getter,setter,resolver)				\
    {"::oo::" name,							\
	    {TCL_OO_METHOD_VERSION_2, "core method: " name " Getter", \
		    getter, NULL, NULL},				\
	    {TCL_OO_METHOD_VERSION_2, "core method: " name " Setter", \
		    setter, NULL, NULL},				\
	    {TCL_OO_METHOD_VERSION_2, "core method: " name " Resolver", \
		    resolver, NULL, NULL}}

/*
 * A [string match] pattern used to determine if a method should be exported.
 */

#define PUBLIC_PATTERN		"[a-z]*"

/*
 * Forward declarations.
 */

static inline void	BumpGlobalEpoch(Tcl_Interp *interp, Class *classPtr);
static inline void	BumpInstanceEpoch(Object *oPtr);
static Tcl_Command	FindCommand(Tcl_Interp *interp, Tcl_Obj *stringObj,
			    Tcl_Namespace *const namespacePtr);
static inline void	GenerateErrorInfo(Tcl_Interp *interp, Object *oPtr,
			    Tcl_Obj *savedNameObj, const char *typeOfSubject);
static inline int	MagicDefinitionInvoke(Tcl_Interp *interp,
			    Tcl_Namespace *nsPtr, int cmdIndex,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static inline Class *	GetClassInOuterContext(Tcl_Interp *interp,
			    Tcl_Obj *className, const char *errMsg);
static inline Tcl_Namespace *GetNamespaceInOuterContext(Tcl_Interp *interp,
			    Tcl_Obj *namespaceName);
static inline int	InitDefineContext(Tcl_Interp *interp,
			    Tcl_Namespace *namespacePtr, Object *oPtr,
			    Tcl_Size objc, Tcl_Obj *const objv[]);
static inline void	RecomputeClassCacheFlag(Object *oPtr);
static int		RenameDeleteMethod(Tcl_Interp *interp, Object *oPtr,
			    int useClass, Tcl_Obj *const fromPtr,
			    Tcl_Obj *const toPtr);
static int		ClassFilter_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassFilter_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassMixin_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassMixin_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassSuper_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassSuper_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassVars_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ClassVars_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjFilter_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjFilter_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjMixin_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjMixin_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjVars_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ObjVars_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ClassReadableProps_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ClassReadableProps_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ClassWritableProps_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ClassWritableProps_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ObjectReadableProps_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ObjectReadableProps_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ObjectWritableProps_Get(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		Configurable_ObjectWritableProps_Set(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);
static int		ResolveClass(void *clientData,
			    Tcl_Interp *interp, Tcl_ObjectContext context,
			    Tcl_Size objc, Tcl_Obj *const *objv);

/*
 * Now define the slots used in declarations.
 */

static const DeclaredSlot slots[] = {
    SLOT("define::filter",      ClassFilter_Get, ClassFilter_Set, NULL),
    SLOT("define::mixin",       ClassMixin_Get,  ClassMixin_Set, ResolveClass),
    SLOT("define::superclass",  ClassSuper_Get,  ClassSuper_Set, ResolveClass),
    SLOT("define::variable",    ClassVars_Get,   ClassVars_Set, NULL),
    SLOT("objdefine::filter",   ObjFilter_Get,   ObjFilter_Set, NULL),
    SLOT("objdefine::mixin",    ObjMixin_Get,    ObjMixin_Set, ResolveClass),
    SLOT("objdefine::variable", ObjVars_Get,     ObjVars_Set, NULL),
    SLOT("configuresupport::readableproperties",
	    Configurable_ClassReadableProps_Get,
	    Configurable_ClassReadableProps_Set, NULL),
    SLOT("configuresupport::writableproperties",
	    Configurable_ClassWritableProps_Get,
	    Configurable_ClassWritableProps_Set, NULL),
    SLOT("configuresupport::objreadableproperties",
	    Configurable_ObjectReadableProps_Get,
	    Configurable_ObjectReadableProps_Set, NULL),
    SLOT("configuresupport::objwritableproperties",
	    Configurable_ObjectWritableProps_Get,
	    Configurable_ObjectWritableProps_Set, NULL),
    {NULL, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}}
};

/*
 * How to build the in-namespace name of a private variable. This is a pattern
 * used with Tcl_ObjPrintf().
 */

#define PRIVATE_VARIABLE_PATTERN "%" TCL_Z_MODIFIER "d : %s"

/*
 * ----------------------------------------------------------------------
 *
 * IsPrivateDefine --
 *
 *	Extracts whether the current context is handling private definitions.
 *
 * ----------------------------------------------------------------------
 */

static inline int
IsPrivateDefine(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    if (!iPtr->varFramePtr) {
	return 0;
    }
    return iPtr->varFramePtr->isProcCallFrame == PRIVATE_FRAME;
}

/*
 * ----------------------------------------------------------------------
 *
 * BumpGlobalEpoch --
 *
 *	Utility that ensures that call chains that are invalid will get thrown
 *	away at an appropriate time. Note that exactly which epoch gets
 *	advanced will depend on exactly what the class is tangled up in; in
 *	the worst case, the simplest option is to advance the global epoch,
 *	causing *everything* to be thrown away on next usage.
 *
 * ----------------------------------------------------------------------
 */

static inline void
BumpGlobalEpoch(
    Tcl_Interp *interp,
    Class *classPtr)
{
    if (classPtr != NULL
	    && classPtr->subclasses.num == 0
	    && classPtr->instances.num == 0
	    && classPtr->mixinSubs.num == 0) {
	/*
	 * If a class has no subclasses or instances, and is not mixed into
	 * anything, a change to its structure does not require us to
	 * invalidate any call chains. Note that we still bump our object's
	 * epoch if it has any mixins; the relation between a class and its
	 * representative object is special. But it won't hurt.
	 */

	if (classPtr->thisPtr->mixins.num > 0) {
	    classPtr->thisPtr->epoch++;

	    /*
	     * Invalidate the property caches directly.
	     */

	    if (classPtr->properties.allReadableCache) {
		Tcl_DecrRefCount(classPtr->properties.allReadableCache);
		classPtr->properties.allReadableCache = NULL;
	    }
	    if (classPtr->properties.allWritableCache) {
		Tcl_DecrRefCount(classPtr->properties.allWritableCache);
		classPtr->properties.allWritableCache = NULL;
	    }
	}
	return;
    }

    /*
     * Either there's no class (?!) or we're reconfiguring something that is
     * in use. Force regeneration of call chains and properties.
     */

    TclOOGetFoundation(interp)->epoch++;
}

/*
 * ----------------------------------------------------------------------
 *
 * BumpInstanceEpoch --
 *
 *	Advances the epoch and clears the property cache of an object. The
 *	equivalent for classes is BumpGlobalEpoch(), as classes have a more
 *	complex set of relationships to other entities.
 *
 * ----------------------------------------------------------------------
 */

static inline void
BumpInstanceEpoch(
    Object *oPtr)
{
    oPtr->epoch++;
    if (oPtr->properties.allReadableCache) {
	Tcl_DecrRefCount(oPtr->properties.allReadableCache);
	oPtr->properties.allReadableCache = NULL;
    }
    if (oPtr->properties.allWritableCache) {
	Tcl_DecrRefCount(oPtr->properties.allWritableCache);
	oPtr->properties.allWritableCache = NULL;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * RecomputeClassCacheFlag --
 *
 *	Determine whether the object is prototypical of its class, and hence
 *	able to use the class's method chain cache.
 *
 * ----------------------------------------------------------------------
 */

static inline void
RecomputeClassCacheFlag(
    Object *oPtr)
{
    if ((oPtr->methodsPtr == NULL || oPtr->methodsPtr->numEntries == 0)
	    && (oPtr->mixins.num == 0) && (oPtr->filters.num == 0)) {
	oPtr->flags |= USE_CLASS_CACHE;
    } else {
	oPtr->flags &= ~USE_CLASS_CACHE;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOObjectSetFilters --
 *
 *	Install a list of filter method names into an object.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOObjectSetFilters(
    Object *oPtr,
    Tcl_Size numFilters,
    Tcl_Obj *const *filters)
{
    Tcl_Size i;

    if (oPtr->filters.num) {
	Tcl_Obj *filterObj;

	FOREACH(filterObj, oPtr->filters) {
	    Tcl_DecrRefCount(filterObj);
	}
    }

    if (numFilters == 0) {
	/*
	 * No list of filters was supplied, so we're deleting filters.
	 */

	Tcl_Free(oPtr->filters.list);
	oPtr->filters.list = NULL;
	oPtr->filters.num = 0;
	RecomputeClassCacheFlag(oPtr);
    } else {
	/*
	 * We've got a list of filters, so we're creating filters.
	 */

	Tcl_Obj **filtersList;
	size_t size = sizeof(Tcl_Obj *) * numFilters;

	if (oPtr->filters.num == 0) {
	    filtersList = (Tcl_Obj **) Tcl_Alloc(size);
	} else {
	    filtersList = (Tcl_Obj **) Tcl_Realloc(oPtr->filters.list, size);
	}
	for (i = 0 ; i < numFilters ; i++) {
	    filtersList[i] = filters[i];
	    Tcl_IncrRefCount(filters[i]);
	}
	oPtr->filters.list = filtersList;
	oPtr->filters.num = numFilters;
	oPtr->flags &= ~USE_CLASS_CACHE;
    }
    BumpInstanceEpoch(oPtr);	// Only this object can be affected.
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOClassSetFilters --
 *
 *	Install a list of filter method names into a class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOClassSetFilters(
    Tcl_Interp *interp,
    Class *classPtr,
    Tcl_Size numFilters,
    Tcl_Obj *const *filters)
{
    Tcl_Size i;

    if (classPtr->filters.num) {
	Tcl_Obj *filterObj;

	FOREACH(filterObj, classPtr->filters) {
	    Tcl_DecrRefCount(filterObj);
	}
    }

    if (numFilters == 0) {
	/*
	 * No list of filters was supplied, so we're deleting filters.
	 */

	Tcl_Free(classPtr->filters.list);
	classPtr->filters.list = NULL;
	classPtr->filters.num = 0;
    } else {
	/*
	 * We've got a list of filters, so we're creating filters.
	 */

	Tcl_Obj **filtersList;
	size_t size = sizeof(Tcl_Obj *) * numFilters;

	if (classPtr->filters.num == 0) {
	    filtersList = (Tcl_Obj **) Tcl_Alloc(size);
	} else {
	    filtersList = (Tcl_Obj **)
		    Tcl_Realloc(classPtr->filters.list, size);
	}
	for (i = 0 ; i < numFilters ; i++) {
	    filtersList[i] = filters[i];
	    Tcl_IncrRefCount(filters[i]);
	}
	classPtr->filters.list = filtersList;
	classPtr->filters.num = numFilters;
    }

    /*
     * There may be many objects affected, so bump the global epoch.
     */

    BumpGlobalEpoch(interp, classPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOObjectSetMixins --
 *
 *	Install a list of mixin classes into an object.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOObjectSetMixins(
    Object *oPtr,
    Tcl_Size numMixins,
    Class *const *mixins)
{
    Class *mixinPtr;
    Tcl_Size i;

    if (numMixins == 0) {
	if (oPtr->mixins.num != 0) {
	    FOREACH(mixinPtr, oPtr->mixins) {
		TclOORemoveFromInstances(oPtr, mixinPtr);
		TclOODecrRefCount(mixinPtr->thisPtr);
	    }
	    Tcl_Free(oPtr->mixins.list);
	    oPtr->mixins.num = 0;
	}
	RecomputeClassCacheFlag(oPtr);
    } else {
	if (oPtr->mixins.num != 0) {
	    FOREACH(mixinPtr, oPtr->mixins) {
		if (mixinPtr && mixinPtr != oPtr->selfCls) {
		    TclOORemoveFromInstances(oPtr, mixinPtr);
		}
		TclOODecrRefCount(mixinPtr->thisPtr);
	    }
	    oPtr->mixins.list = (Class **) Tcl_Realloc(oPtr->mixins.list,
		    sizeof(Class *) * numMixins);
	} else {
	    oPtr->mixins.list = (Class **)
		    Tcl_Alloc(sizeof(Class *) * numMixins);
	    oPtr->flags &= ~USE_CLASS_CACHE;
	}
	oPtr->mixins.num = numMixins;
	memcpy(oPtr->mixins.list, mixins, sizeof(Class *) * numMixins);
	FOREACH(mixinPtr, oPtr->mixins) {
	    if (mixinPtr != oPtr->selfCls) {
		TclOOAddToInstances(oPtr, mixinPtr);

		/*
		 * For the new copy created by memcpy().
		 */

		AddRef(mixinPtr->thisPtr);
	    }
	}
    }
    BumpInstanceEpoch(oPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOClassSetMixins --
 *
 *	Install a list of mixin classes into a class.
 *
 * ----------------------------------------------------------------------
 */

void
TclOOClassSetMixins(
    Tcl_Interp *interp,
    Class *classPtr,
    Tcl_Size numMixins,
    Class *const *mixins)
{
    Class *mixinPtr;
    Tcl_Size i;

    if (numMixins == 0) {
	if (classPtr->mixins.num != 0) {
	    FOREACH(mixinPtr, classPtr->mixins) {
		TclOORemoveFromMixinSubs(classPtr, mixinPtr);
		TclOODecrRefCount(mixinPtr->thisPtr);
	    }
	    Tcl_Free(classPtr->mixins.list);
	    classPtr->mixins.num = 0;
	}
    } else {
	if (classPtr->mixins.num != 0) {
	    FOREACH(mixinPtr, classPtr->mixins) {
		TclOORemoveFromMixinSubs(classPtr, mixinPtr);
		TclOODecrRefCount(mixinPtr->thisPtr);
	    }
	    classPtr->mixins.list = (Class **)
		    Tcl_Realloc(classPtr->mixins.list,
			    sizeof(Class *) * numMixins);
	} else {
	    classPtr->mixins.list = (Class **)
		    Tcl_Alloc(sizeof(Class *) * numMixins);
	}
	classPtr->mixins.num = numMixins;
	memcpy(classPtr->mixins.list, mixins, sizeof(Class *) * numMixins);
	FOREACH(mixinPtr, classPtr->mixins) {
	    TclOOAddToMixinSubs(classPtr, mixinPtr);

	    /*
	     * For the new copy created by memcpy.
	     */

	    AddRef(mixinPtr->thisPtr);
	}
    }
    BumpGlobalEpoch(interp, classPtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * InstallStandardVariableMapping, InstallPrivateVariableMapping --
 *
 *	Helpers for installing standard and private variable maps.
 *
 * ----------------------------------------------------------------------
 */

static inline void
InstallStandardVariableMapping(
    VariableNameList *vnlPtr,
    Tcl_Size varc,
    Tcl_Obj *const *varv)
{
    Tcl_Obj *variableObj;
    Tcl_Size i, n;
    int created;
    Tcl_HashTable uniqueTable;

    for (i=0 ; i<varc ; i++) {
	Tcl_IncrRefCount(varv[i]);
    }
    FOREACH(variableObj, *vnlPtr) {
	Tcl_DecrRefCount(variableObj);
    }
    if (i != varc) {
	if (varc == 0) {
	    Tcl_Free(vnlPtr->list);
	} else if (i) {
	    vnlPtr->list = (Tcl_Obj **)
		    Tcl_Realloc(vnlPtr->list, sizeof(Tcl_Obj *) * varc);
	} else {
	    vnlPtr->list = (Tcl_Obj **) Tcl_Alloc(sizeof(Tcl_Obj *) * varc);
	}
    }
    vnlPtr->num = 0;
    if (varc > 0) {
	Tcl_InitObjHashTable(&uniqueTable);
	for (i=n=0 ; i<varc ; i++) {
	    Tcl_CreateHashEntry(&uniqueTable, varv[i], &created);
	    if (created) {
		vnlPtr->list[n++] = varv[i];
	    } else {
		Tcl_DecrRefCount(varv[i]);
	    }
	}
	vnlPtr->num = n;

	/*
	 * Shouldn't be necessary, but maintain num/list invariant.
	 */

	if (n != varc) {
	    vnlPtr->list = (Tcl_Obj **)
		    Tcl_Realloc(vnlPtr->list, sizeof(Tcl_Obj *) * n);
	}
	Tcl_DeleteHashTable(&uniqueTable);
    }
}

static inline void
InstallPrivateVariableMapping(
    PrivateVariableList *pvlPtr,
    Tcl_Size varc,
    Tcl_Obj *const *varv,
    Tcl_Size creationEpoch)
{
    PrivateVariableMapping *privatePtr;
    Tcl_Size i, n;
    int created;
    Tcl_HashTable uniqueTable;

    for (i=0 ; i<varc ; i++) {
	Tcl_IncrRefCount(varv[i]);
    }
    FOREACH_STRUCT(privatePtr, *pvlPtr) {
	Tcl_DecrRefCount(privatePtr->variableObj);
	Tcl_DecrRefCount(privatePtr->fullNameObj);
    }
    if (i != varc) {
	if (varc == 0) {
	    Tcl_Free(pvlPtr->list);
	} else if (i) {
	    pvlPtr->list = (PrivateVariableMapping *)
		    Tcl_Realloc(pvlPtr->list,
			    sizeof(PrivateVariableMapping) * varc);
	} else {
	    pvlPtr->list = (PrivateVariableMapping *)
		    Tcl_Alloc(sizeof(PrivateVariableMapping) * varc);
	}
    }

    pvlPtr->num = 0;
    if (varc > 0) {
	Tcl_InitObjHashTable(&uniqueTable);
	for (i=n=0 ; i<varc ; i++) {
	    Tcl_CreateHashEntry(&uniqueTable, varv[i], &created);
	    if (created) {
		privatePtr = &(pvlPtr->list[n++]);
		privatePtr->variableObj = varv[i];
		privatePtr->fullNameObj = Tcl_ObjPrintf(
			PRIVATE_VARIABLE_PATTERN,
			creationEpoch, TclGetString(varv[i]));
		Tcl_IncrRefCount(privatePtr->fullNameObj);
	    } else {
		Tcl_DecrRefCount(varv[i]);
	    }
	}
	pvlPtr->num = n;

	/*
	 * Shouldn't be necessary, but maintain num/list invariant.
	 */

	if (n != varc) {
	    pvlPtr->list = (PrivateVariableMapping *) Tcl_Realloc(pvlPtr->list,
		    sizeof(PrivateVariableMapping) * n);
	}
	Tcl_DeleteHashTable(&uniqueTable);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * RenameDeleteMethod --
 *
 *	Core of the code to rename and delete methods.
 *
 * ----------------------------------------------------------------------
 */

static int
RenameDeleteMethod(
    Tcl_Interp *interp,
    Object *oPtr,
    int useClass,
    Tcl_Obj *const fromPtr,
    Tcl_Obj *const toPtr)
{
    Tcl_HashEntry *hPtr, *newHPtr = NULL;
    Method *mPtr;
    int isNew;

    if (!useClass) {
	if (!oPtr->methodsPtr) {
	noSuchMethod:
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "method %s does not exist", TclGetString(fromPtr)));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
		    TclGetString(fromPtr), (char *)NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(oPtr->methodsPtr, fromPtr);
	if (hPtr == NULL) {
	    goto noSuchMethod;
	}
	if (toPtr) {
	    newHPtr = Tcl_CreateHashEntry(oPtr->methodsPtr, toPtr,
		    &isNew);
	    if (hPtr == newHPtr) {
	    renameToSelf:
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"cannot rename method to itself", TCL_AUTO_LENGTH));
		OO_ERROR(interp, RENAME_TO_SELF);
		return TCL_ERROR;
	    } else if (!isNew) {
	    renameToExisting:
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"method called %s already exists",
			TclGetString(toPtr)));
		OO_ERROR(interp, RENAME_OVER);
		return TCL_ERROR;
	    }
	}
    } else {
	hPtr = Tcl_FindHashEntry(&oPtr->classPtr->classMethods, fromPtr);
	if (hPtr == NULL) {
	    goto noSuchMethod;
	}
	if (toPtr) {
	    newHPtr = Tcl_CreateHashEntry(&oPtr->classPtr->classMethods,
		    (char *) toPtr, &isNew);
	    if (hPtr == newHPtr) {
		goto renameToSelf;
	    } else if (!isNew) {
		goto renameToExisting;
	    }
	}
    }

    /*
     * Complete the splicing by changing the method's name.
     */

    mPtr = (Method *) Tcl_GetHashValue(hPtr);
    if (toPtr) {
	Tcl_IncrRefCount(toPtr);
	Tcl_DecrRefCount(mPtr->namePtr);
	mPtr->namePtr = toPtr;
	Tcl_SetHashValue(newHPtr, mPtr);
    } else {
	if (!useClass) {
	    RecomputeClassCacheFlag(oPtr);
	}
	TclOODelMethodRef(mPtr);
    }
    Tcl_DeleteHashEntry(hPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOUnknownDefinition --
 *
 *	Handles what happens when an unknown command is encountered during the
 *	processing of a definition script. Works by finding a command in the
 *	operating definition namespace that the requested command is a unique
 *	prefix of.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOUnknownDefinition(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Namespace *nsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);
    FOREACH_HASH_DECLS;
    Tcl_Size soughtLen;
    const char *soughtStr, *nameStr, *matchedStr = NULL;

    if (objc < 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"bad call of unknown handler", TCL_AUTO_LENGTH));
	OO_ERROR(interp, BAD_UNKNOWN);
	return TCL_ERROR;
    }
    if (TclOOGetDefineCmdContext(interp) == NULL) {
	return TCL_ERROR;
    }

    soughtStr = TclGetStringFromObj(objv[1], &soughtLen);
    if (soughtLen == 0) {
	goto noMatch;
    }
    FOREACH_HASH_KEY(nameStr, &nsPtr->cmdTable) {
	if (strncmp(soughtStr, nameStr, soughtLen) == 0) {
	    if (matchedStr != NULL) {
		goto noMatch;
	    }
	    matchedStr = nameStr;
	}
    }

    if (matchedStr != NULL) {
	/*
	 * Got one match, and only one match!
	 */

	Tcl_Obj **newObjv = (Tcl_Obj **)
		TclStackAlloc(interp, sizeof(Tcl_Obj*) * (objc - 1));
	int result;

	newObjv[0] = Tcl_NewStringObj(matchedStr, TCL_AUTO_LENGTH);
	Tcl_IncrRefCount(newObjv[0]);
	if (objc > 2) {
	    memcpy(newObjv + 1, objv + 2, sizeof(Tcl_Obj *) * (objc - 2));
	}
	result = Tcl_EvalObjv(interp, objc - 1, newObjv, 0);
	Tcl_DecrRefCount(newObjv[0]);
	TclStackFree(interp, newObjv);
	return result;
    }

  noMatch:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "invalid command name \"%s\"", soughtStr));
    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "COMMAND", soughtStr, (char *)NULL);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * FindCommand --
 *
 *	Specialized version of Tcl_FindCommand that handles command prefixes
 *	and disallows namespace magic.
 *
 * ----------------------------------------------------------------------
 */

static Tcl_Command
FindCommand(
    Tcl_Interp *interp,
    Tcl_Obj *stringObj,
    Tcl_Namespace *const namespacePtr)
{
    Tcl_Size length;
    const char *nameStr, *string = TclGetStringFromObj(stringObj, &length);
    Namespace *const nsPtr = (Namespace *) namespacePtr;
    FOREACH_HASH_DECLS;
    Tcl_Command cmd, cmd2;

    /*
     * If someone is playing games, we stop playing right now.
     */

    if (string[0] == '\0' || strstr(string, "::") != NULL) {
	return NULL;
    }

    /*
     * Do the exact lookup first.
     */

    cmd = Tcl_FindCommand(interp, string, namespacePtr, TCL_NAMESPACE_ONLY);
    if (cmd != NULL) {
	return cmd;
    }

    /*
     * Bother, need to perform an approximate match. Iterate across the hash
     * table of commands in the namespace.
     */

    FOREACH_HASH(nameStr, cmd2, &nsPtr->cmdTable) {
	if (strncmp(string, nameStr, length) == 0) {
	    if (cmd != NULL) {
		return NULL;
	    }
	    cmd = cmd2;
	}
    }

    /*
     * Either we found one thing or we found nothing. Either way, return it.
     */

    return cmd;
}

/*
 * ----------------------------------------------------------------------
 *
 * InitDefineContext --
 *
 *	Does the magic incantations necessary to push the special stack frame
 *	used when processing object definitions. It is up to the caller to
 *	dispose of the frame (with TclPopStackFrame) when finished.
 *
 * ----------------------------------------------------------------------
 */

static inline int
InitDefineContext(
    Tcl_Interp *interp,
    Tcl_Namespace *namespacePtr,
    Object *oPtr,
    Tcl_Size objc,
    Tcl_Obj *const objv[])
{
    CallFrame *framePtr, **framePtrPtr = &framePtr;

    if (namespacePtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"no definition namespace available", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    /*
     * framePtrPtr is needed to satisfy GCC 3.3's strict aliasing rules.
     */

    (void) TclPushStackFrame(interp, (Tcl_CallFrame **) framePtrPtr,
	    namespacePtr, FRAME_IS_OO_DEFINE);
    framePtr->clientData = oPtr;
    framePtr->objc = objc;
    framePtr->objv = objv;	/* Reference counts do not need to be
				 * incremented here. */
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOGetDefineCmdContext, TclOOGetClassDefineCmdContext --
 *
 *	Extracts the magic token from the current stack frame, or returns NULL
 *	(and leaves an error message) otherwise.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Object
TclOOGetDefineCmdContext(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Object object;

    if ((iPtr->varFramePtr == NULL)
	    || (iPtr->varFramePtr->isProcCallFrame != FRAME_IS_OO_DEFINE
	    && iPtr->varFramePtr->isProcCallFrame != PRIVATE_FRAME)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"this command may only be called from within the context of"
		" an ::oo::define or ::oo::objdefine command",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return NULL;
    }
    object = (Tcl_Object) iPtr->varFramePtr->clientData;
    if (Tcl_ObjectDeleted(object)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"this command cannot be called when the object has been"
		" deleted", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return NULL;
    }
    return object;
}

Class *
TclOOGetClassDefineCmdContext(
    Tcl_Interp *interp)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return NULL;
    }
    if (!oPtr->classPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return NULL;
    }
    return oPtr->classPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * GetClassInOuterContext, GetNamespaceInOuterContext --
 *
 *	Wrappers round Tcl_GetObjectFromObj and TclGetNamespaceFromObj to
 *	perform the lookup in the context that called oo::define (or
 *	equivalent). Note that this may have to go up multiple levels to get
 *	the level that we started doing definitions at.
 *
 * ----------------------------------------------------------------------
 */

static inline Class *
GetClassInOuterContext(
    Tcl_Interp *interp,
    Tcl_Obj *className,
    const char *errMsg)
{
    Interp *iPtr = (Interp *) interp;
    Object *oPtr;
    CallFrame *savedFramePtr = iPtr->varFramePtr;

    while (iPtr->varFramePtr->isProcCallFrame == FRAME_IS_OO_DEFINE
	    || iPtr->varFramePtr->isProcCallFrame == PRIVATE_FRAME) {
	if (iPtr->varFramePtr->callerVarPtr == NULL) {
	    Tcl_Panic("getting outer context when already in global context");
	}
	iPtr->varFramePtr = iPtr->varFramePtr->callerVarPtr;
    }
    oPtr = (Object *) Tcl_GetObjectFromObj(interp, className);
    iPtr->varFramePtr = savedFramePtr;
    if (oPtr == NULL) {
	return NULL;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(errMsg, TCL_AUTO_LENGTH));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		TclGetString(className), (char *)NULL);
	return NULL;
    }
    return oPtr->classPtr;
}

static inline Tcl_Namespace *
GetNamespaceInOuterContext(
    Tcl_Interp *interp,
    Tcl_Obj *namespaceName)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Namespace *nsPtr;
    int result;
    CallFrame *savedFramePtr = iPtr->varFramePtr;

    while (iPtr->varFramePtr->isProcCallFrame == FRAME_IS_OO_DEFINE
	    || iPtr->varFramePtr->isProcCallFrame == PRIVATE_FRAME) {
	if (iPtr->varFramePtr->callerVarPtr == NULL) {
	    Tcl_Panic("getting outer context when already in global context");
	}
	iPtr->varFramePtr = iPtr->varFramePtr->callerVarPtr;
    }
    result = TclGetNamespaceFromObj(interp, namespaceName, &nsPtr);
    iPtr->varFramePtr = savedFramePtr;
    if (result != TCL_OK) {
	return NULL;
    }
    return nsPtr;
}

/*
 * ----------------------------------------------------------------------
 *
 * GenerateErrorInfo --
 *
 *	Factored out code to generate part of the error trace messages.
 *
 * ----------------------------------------------------------------------
 */

static inline void
GenerateErrorInfo(
    Tcl_Interp *interp,		/* Where to store the error info trace. */
    Object *oPtr,		/* What object (or class) was being configured
				 * when the error occurred? */
    Tcl_Obj *savedNameObj,	/* Name of object saved from before script was
				 * evaluated, which is needed if the object
				 * goes away part way through execution. OTOH,
				 * if the object isn't deleted then its
				 * current name (post-execution) has to be
				 * used. This matters, because the object
				 * could have been renamed... */
    const char *typeOfSubject)	/* Part of the message, saying whether it was
				 * an object, class or class-as-object that
				 * was being configured. */
{
    Tcl_Size length;
    Tcl_Obj *realNameObj = Tcl_ObjectDeleted((Tcl_Object) oPtr)
	    ? savedNameObj : TclOOObjectName(interp, oPtr);
    const char *objName = TclGetStringFromObj(realNameObj, &length);
    int limit = OBJNAME_LENGTH_IN_ERRORINFO_LIMIT;
    int overflow = (length > limit);

    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
	    "\n    (in definition script for %s \"%.*s%s\" line %d)",
	    typeOfSubject, (overflow ? limit : (int) length), objName,
	    (overflow ? "..." : ""), Tcl_GetErrorLine(interp)));
}

/*
 * ----------------------------------------------------------------------
 *
 * MagicDefinitionInvoke --
 *
 *	Part of the implementation of the "oo::define" and "oo::objdefine"
 *	commands that is used to implement the more-than-one-argument case,
 *	applying ensemble-like tricks with dispatch so that error messages are
 *	clearer. Doesn't handle the management of the stack frame.
 *
 * ----------------------------------------------------------------------
 */

static inline int
MagicDefinitionInvoke(
    Tcl_Interp *interp,
    Tcl_Namespace *nsPtr,
    int cmdIndex,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Obj *objPtr, *obj2Ptr, **objs;
    Tcl_Command cmd;
    int isRoot, result, offset = cmdIndex + 1;
    Tcl_Size dummy;

    /*
     * More than one argument: fire them through the ensemble processing
     * engine so that everything appears to be good and proper in error
     * messages. Note that we cannot just concatenate and send through
     * Tcl_EvalObjEx, as that doesn't do ensemble processing, and we cannot go
     * through Tcl_EvalObjv without the extra work to pre-find the command, as
     * that finds command names in the wrong namespace at the moment. Ugly!
     */

    isRoot = TclInitRewriteEnsemble(interp, offset, 1, objv);

    /*
     * Build the list of arguments using a Tcl_Obj as a workspace. See
     * comments above for why these contortions are necessary.
     */

    TclNewObj(objPtr);
    TclNewObj(obj2Ptr);
    cmd = FindCommand(interp, objv[cmdIndex], nsPtr);
    if (cmd == NULL) {
	/*
	 * Punt this case!
	 */

	Tcl_AppendObjToObj(obj2Ptr, objv[cmdIndex]);
    } else {
	Tcl_GetCommandFullName(interp, cmd, obj2Ptr);
    }
    Tcl_ListObjAppendElement(NULL, objPtr, obj2Ptr);
    // TODO: overflow?
    Tcl_ListObjReplace(NULL, objPtr, 1, 0, objc - offset, objv + offset);
    TclListObjGetElements(NULL, objPtr, &dummy, &objs);

    result = Tcl_EvalObjv(interp, objc - cmdIndex, objs, TCL_EVAL_INVOKE);
    if (isRoot) {
	TclResetRewriteEnsemble(interp, 1);
    }
    Tcl_DecrRefCount(objPtr);

    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineObjCmd --
 *
 *	Implementation of the "oo::define" command. Works by effectively doing
 *	the same as 'namespace eval', but with extra magic applied so that the
 *	object to be modified is known to the commands in the target
 *	namespace. Also does ensemble-like tricks with dispatch so that error
 *	messages are clearer.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Namespace *nsPtr;
    Object *oPtr;
    int result;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "className arg ?arg ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->classPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"%s does not refer to a class", TclGetString(objv[1])));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		TclGetString(objv[1]), (char *)NULL);
	return TCL_ERROR;
    }

    /*
     * Make the oo::define namespace the current namespace and evaluate the
     * command(s).
     */

    nsPtr = TclOOGetDefineContextNamespace(interp, oPtr, 1);
    if (InitDefineContext(interp, nsPtr, oPtr, objc, objv) != TCL_OK) {
	return TCL_ERROR;
    }

    AddRef(oPtr);
    if (objc == 3) {
	Tcl_Obj *objNameObj = TclOOObjectName(interp, oPtr);

	Tcl_IncrRefCount(objNameObj);
	result = TclEvalObjEx(interp, objv[2], 0,
		((Interp *) interp)->cmdFramePtr, 2);
	if (result == TCL_ERROR) {
	    GenerateErrorInfo(interp, oPtr, objNameObj, "class");
	}
	TclDecrRefCount(objNameObj);
    } else {
	result = MagicDefinitionInvoke(interp, nsPtr, 2, objc, objv);
    }
    TclOODecrRefCount(oPtr);

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOOObjDefObjCmd --
 *
 *	Implementation of the "oo::objdefine" command. Works by effectively
 *	doing the same as 'namespace eval', but with extra magic applied so
 *	that the object to be modified is known to the commands in the target
 *	namespace. Also does ensemble-like tricks with dispatch so that error
 *	messages are clearer.
 *
 * ----------------------------------------------------------------------
 */

int
TclOOObjDefObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Namespace *nsPtr;
    Object *oPtr;
    int result;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "objectName arg ?arg ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[1]);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * Make the oo::objdefine namespace the current namespace and evaluate the
     * command(s).
     */

    nsPtr = TclOOGetDefineContextNamespace(interp, oPtr, 0);
    if (InitDefineContext(interp, nsPtr, oPtr, objc, objv) != TCL_OK) {
	return TCL_ERROR;
    }

    AddRef(oPtr);
    if (objc == 3) {
	Tcl_Obj *objNameObj = TclOOObjectName(interp, oPtr);

	Tcl_IncrRefCount(objNameObj);
	result = TclEvalObjEx(interp, objv[2], 0,
		((Interp *) interp)->cmdFramePtr, 2);
	if (result == TCL_ERROR) {
	    GenerateErrorInfo(interp, oPtr, objNameObj, "object");
	}
	TclDecrRefCount(objNameObj);
    } else {
	result = MagicDefinitionInvoke(interp, nsPtr, 2, objc, objv);
    }
    TclOODecrRefCount(oPtr);

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineSelfObjCmd --
 *
 *	Implementation of the "self" subcommand of the "oo::define" command.
 *	Works by effectively doing the same as 'namespace eval', but with
 *	extra magic applied so that the object to be modified is known to the
 *	commands in the target namespace. Also does ensemble-like tricks with
 *	dispatch so that error messages are clearer.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineSelfObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Namespace *nsPtr;
    Object *oPtr;
    int result, isPrivate;

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    if (objc < 2) {
	Tcl_SetObjResult(interp, TclOOObjectName(interp, oPtr));
	return TCL_OK;
    }

    isPrivate = IsPrivateDefine(interp);

    /*
     * Make the oo::objdefine namespace the current namespace and evaluate the
     * command(s).
     */

    nsPtr = TclOOGetDefineContextNamespace(interp, oPtr, 0);
    if (InitDefineContext(interp, nsPtr, oPtr, objc, objv) != TCL_OK) {
	return TCL_ERROR;
    }
    if (isPrivate) {
	((Interp *) interp)->varFramePtr->isProcCallFrame = PRIVATE_FRAME;
    }

    AddRef(oPtr);
    if (objc == 2) {
	Tcl_Obj *objNameObj = TclOOObjectName(interp, oPtr);

	Tcl_IncrRefCount(objNameObj);
	result = TclEvalObjEx(interp, objv[1], 0,
		((Interp *) interp)->cmdFramePtr, 1);
	if (result == TCL_ERROR) {
	    GenerateErrorInfo(interp, oPtr, objNameObj, "class object");
	}
	TclDecrRefCount(objNameObj);
    } else {
	result = MagicDefinitionInvoke(interp, nsPtr, 1, objc, objv);
    }
    TclOODecrRefCount(oPtr);

    /*
     * Restore the previous "current" namespace.
     */

    TclPopStackFrame(interp);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineObjSelfObjCmd --
 *
 *	Implementation of the "self" subcommand of the "oo::objdefine"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineObjSelfObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, NULL);
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOObjectName(interp, oPtr));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefinePrivateObjCmd --
 *
 *	Implementation of the "private" subcommand of the "oo::define"
 *	and "oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefinePrivateObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstancePrivate = (clientData != NULL);
				/* Just so that we can generate the correct
				 * error message depending on the context of
				 * usage of this function. */
    Interp *iPtr = (Interp *) interp;
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    int saved;			/* The saved flag. We restore it on exit so
				 * that [private private ...] doesn't make
				 * things go weird. */
    int result;

    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (objc == 1) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(IsPrivateDefine(interp)));
	return TCL_OK;
    }

    /*
     * Change the frame type flag while evaluating the body.
     */

    saved = iPtr->varFramePtr->isProcCallFrame;
    iPtr->varFramePtr->isProcCallFrame = PRIVATE_FRAME;

    /*
     * Evaluate the body; standard pattern.
     */

    AddRef(oPtr);
    if (objc == 2) {
	Tcl_Obj *objNameObj = TclOOObjectName(interp, oPtr);

	Tcl_IncrRefCount(objNameObj);
	result = TclEvalObjEx(interp, objv[1], 0, iPtr->cmdFramePtr, 1);
	if (result == TCL_ERROR) {
	    GenerateErrorInfo(interp, oPtr, objNameObj,
		    isInstancePrivate ? "object" : "class");
	}
	TclDecrRefCount(objNameObj);
    } else {
	result = MagicDefinitionInvoke(interp, TclGetCurrentNamespace(interp),
		1, objc, objv);
    }
    TclOODecrRefCount(oPtr);

    /*
     * Restore the frame type flag to what it was previously.
     */

    iPtr->varFramePtr->isProcCallFrame = saved;
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineClassObjCmd --
 *
 *	Implementation of the "class" subcommand of the "oo::objdefine"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineClassObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr;
    Class *clsPtr;
    Foundation *fPtr = TclOOGetFoundation(interp);
    int wasClass, willBeClass;

    /*
     * Parse the context to get the object to operate on.
     */

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr->flags & ROOT_OBJECT) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not modify the class of the root object class",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }
    if (oPtr->flags & ROOT_CLASS) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not modify the class of the class of classes",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    /*
     * Parse the argument to get the class to set the object's class to.
     */

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "className");
	return TCL_ERROR;
    }
    clsPtr = GetClassInOuterContext(interp, objv[1],
	    "the class of an object must be a class");
    if (clsPtr == NULL) {
	return TCL_ERROR;
    }
    if (oPtr == clsPtr->thisPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not change classes into an instance of themselves",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    /*
     * Set the object's class.
     */

    wasClass = (oPtr->classPtr != NULL);
    willBeClass = (TclOOIsReachable(fPtr->classCls, clsPtr));

    if (oPtr->selfCls != clsPtr) {
	TclOORemoveFromInstances(oPtr, oPtr->selfCls);
	TclOODecrRefCount(oPtr->selfCls->thisPtr);
	oPtr->selfCls = clsPtr;
	AddRef(oPtr->selfCls->thisPtr);
	TclOOAddToInstances(oPtr, oPtr->selfCls);

	/*
	 * Create or delete the class guts if necessary.
	 */

	if (wasClass && !willBeClass) {
	    /*
	     * This is the most global of all epochs. Bump it! No cache can be
	     * trusted!
	     */

	    TclOORemoveFromMixins(oPtr->classPtr, oPtr);
	    oPtr->fPtr->epoch++;
	    oPtr->flags |= DONT_DELETE;
	    TclOODeleteDescendants(interp, oPtr);
	    oPtr->flags &= ~DONT_DELETE;
	    TclOOReleaseClassContents(interp, oPtr);
	    Tcl_Free(oPtr->classPtr);
	    oPtr->classPtr = NULL;
	} else if (!wasClass && willBeClass) {
	    TclOOAllocClass(interp, oPtr);
	}

	if (oPtr->classPtr != NULL) {
	    BumpGlobalEpoch(interp, oPtr->classPtr);
	} else {
	    BumpInstanceEpoch(oPtr);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineConstructorObjCmd --
 *
 *	Implementation of the "constructor" subcommand of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineConstructorObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Method method;
    Tcl_Size bodyLength;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "arguments body");
	return TCL_ERROR;
    }

    (void) TclGetStringFromObj(objv[2], &bodyLength);
    if (bodyLength > 0) {
	/*
	 * Create the method structure.
	 */

	method = (Tcl_Method) TclOONewProcMethod(interp, clsPtr,
		PUBLIC_METHOD, NULL, objv[1], objv[2], NULL);
	if (method == NULL) {
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Delete the constructor method record and set the field in the
	 * class record to NULL.
	 */

	method = NULL;
    }

    /*
     * Place the method structure in the class record. Note that we might not
     * immediately delete the constructor as this might be being done during
     * execution of the constructor itself.
     */

    Tcl_ClassSetConstructor(interp, (Tcl_Class) clsPtr, method);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineDefnNsObjCmd --
 *
 *	Implementation of the "definitionnamespace" subcommand of the
 *	"oo::define" command.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineDefnNsObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    static const char *kindList[] = {
	"-class",
	"-instance",
	NULL
    };
    int kind = 0;
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Namespace *nsPtr;
    Tcl_Obj *nsNamePtr, **storagePtr;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (clsPtr->thisPtr->flags & (ROOT_OBJECT | ROOT_CLASS)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not modify the definition namespace of the root classes",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    /*
     * Parse the arguments and work out what the user wants to do.
     */

    if (objc != 2 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?kind? namespace");
	return TCL_ERROR;
    }
    if (objc == 3 && Tcl_GetIndexFromObj(interp, objv[1], kindList, "kind", 0,
	    &kind) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!TclGetString(objv[objc - 1])[0]) {
	nsNamePtr = NULL;
    } else {
	nsPtr = GetNamespaceInOuterContext(interp, objv[objc - 1]);
	if (nsPtr == NULL) {
	    return TCL_ERROR;
	}
	nsNamePtr = TclNewNamespaceObj(nsPtr);
	Tcl_IncrRefCount(nsNamePtr);
    }

    /*
     * Update the correct field of the class definition.
     */

    if (kind) {
	storagePtr = &clsPtr->objDefinitionNs;
    } else {
	storagePtr = &clsPtr->clsDefinitionNs;
    }
    if (*storagePtr != NULL) {
	Tcl_DecrRefCount(*storagePtr);
    }
    *storagePtr = nsNamePtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineDeleteMethodObjCmd --
 *
 *	Implementation of the "deletemethod" subcommand of the "oo::define"
 *	and "oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineDeleteMethodObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstanceDeleteMethod = (clientData != NULL);
    Object *oPtr;
    int i;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "name ?name ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (!isInstanceDeleteMethod && !oPtr->classPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    for (i = 1; i < objc; i++) {
	/*
	 * Delete the method structure from the appropriate hash table.
	 */

	if (RenameDeleteMethod(interp, oPtr, !isInstanceDeleteMethod,
		objv[i], NULL) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    if (isInstanceDeleteMethod) {
	BumpInstanceEpoch(oPtr);
    } else {
	BumpGlobalEpoch(interp, oPtr->classPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineDestructorObjCmd --
 *
 *	Implementation of the "destructor" subcommand of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineDestructorObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Method method;
    Tcl_Size bodyLength;
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "body");
	return TCL_ERROR;
    }


    (void) TclGetStringFromObj(objv[1], &bodyLength);
    if (bodyLength > 0) {
	/*
	 * Create the method structure.
	 */

	method = (Tcl_Method) TclOONewProcMethod(interp, clsPtr,
		PUBLIC_METHOD, NULL, NULL, objv[1], NULL);
	if (method == NULL) {
	    return TCL_ERROR;
	}
    } else {
	/*
	 * Delete the destructor method record and set the field in the class
	 * record to NULL.
	 */

	method = NULL;
    }

    /*
     * Place the method structure in the class record. Note that we might not
     * immediately delete the destructor as this might be being done during
     * execution of the destructor itself. Also note that setting a
     * destructor during a destructor is fairly dumb anyway.
     */

    Tcl_ClassSetDestructor(interp, (Tcl_Class) clsPtr, method);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineExportObjCmd --
 *
 *	Implementation of the "export" subcommand of the "oo::define" and
 *	"oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineExportObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstanceExport = (clientData != NULL);
    Object *oPtr;
    Method *mPtr;
    Tcl_HashEntry *hPtr;
    Class *clsPtr;
    int i, isNew, changed = 0;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "name ?name ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;
    if (!isInstanceExport && !clsPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    for (i = 1; i < objc; i++) {
	/*
	 * Exporting is done by adding the PUBLIC_METHOD flag to the method
	 * record. If there is no such method in this object or class (i.e.
	 * the method comes from something inherited from or that we're an
	 * instance of) then we put in a blank record with that flag; such
	 * records are skipped over by the call chain engine *except* for
	 * their flags member.
	 */

	if (isInstanceExport) {
	    if (!oPtr->methodsPtr) {
		oPtr->methodsPtr = (Tcl_HashTable *)
			Tcl_Alloc(sizeof(Tcl_HashTable));
		Tcl_InitObjHashTable(oPtr->methodsPtr);
		oPtr->flags &= ~USE_CLASS_CACHE;
	    }
	    hPtr = Tcl_CreateHashEntry(oPtr->methodsPtr, objv[i],
		    &isNew);
	} else {
	    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, objv[i],
		    &isNew);
	}

	if (isNew) {
	    mPtr = (Method *) Tcl_Alloc(sizeof(Method));
	    memset(mPtr, 0, sizeof(Method));
	    mPtr->refCount = 1;
	    mPtr->namePtr = objv[i];
	    Tcl_IncrRefCount(objv[i]);
	    Tcl_SetHashValue(hPtr, mPtr);
	} else {
	    mPtr = (Method *) Tcl_GetHashValue(hPtr);
	}
	if (isNew || !(mPtr->flags & (PUBLIC_METHOD | PRIVATE_METHOD))) {
	    mPtr->flags |= PUBLIC_METHOD;
	    mPtr->flags &= ~TRUE_PRIVATE_METHOD;
	    changed = 1;
	}
    }

    /*
     * Bump the right epoch if we actually changed anything.
     */

    if (changed) {
	if (isInstanceExport) {
	    BumpInstanceEpoch(oPtr);
	} else {
	    BumpGlobalEpoch(interp, clsPtr);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineForwardObjCmd --
 *
 *	Implementation of the "forward" subcommand of the "oo::define" and
 *	"oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineForwardObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstanceForward = (clientData != NULL);
    Object *oPtr;
    Method *mPtr;
    int isPublic;
    Tcl_Obj *prefixObj;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "name cmdName ?arg ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (!isInstanceForward && !oPtr->classPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }
    isPublic = Tcl_StringMatch(TclGetString(objv[1]), PUBLIC_PATTERN)
	    ? PUBLIC_METHOD : 0;
    if (IsPrivateDefine(interp)) {
	isPublic = TRUE_PRIVATE_METHOD;
    }

    /*
     * Create the method structure.
     */

    prefixObj = Tcl_NewListObj(objc - 2, objv + 2);
    if (isInstanceForward) {
	mPtr = TclOONewForwardInstanceMethod(interp, oPtr, isPublic, objv[1],
		prefixObj);
    } else {
	mPtr = TclOONewForwardMethod(interp, oPtr->classPtr, isPublic,
		objv[1], prefixObj);
    }
    if (mPtr == NULL) {
	Tcl_DecrRefCount(prefixObj);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineMethodObjCmd --
 *
 *	Implementation of the "method" subcommand of the "oo::define" and
 *	"oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineMethodObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    /*
     * Table of export modes for methods and their corresponding enum.
     */

    static const char *const exportModes[] = {
	"-export",
	"-private",
	"-unexport",
	NULL
    };
    enum ExportMode {
	MODE_EXPORT,
	MODE_PRIVATE,
	MODE_UNEXPORT
    } exportMode;

    int isInstanceMethod = (clientData != NULL);
    Object *oPtr;
    int isPublic = 0;

    if (objc < 4 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "name ?option? args body");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (!isInstanceMethod && !oPtr->classPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }
    if (objc == 5) {
	if (Tcl_GetIndexFromObj(interp, objv[2], exportModes, "export flag",
		0, &exportMode) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (exportMode) {
	case MODE_EXPORT:
	    isPublic = PUBLIC_METHOD;
	    break;
	case MODE_PRIVATE:
	    isPublic = TRUE_PRIVATE_METHOD;
	    break;
	case MODE_UNEXPORT:
	    isPublic = 0;
	    break;
	}
    } else {
	if (IsPrivateDefine(interp)) {
	    isPublic = TRUE_PRIVATE_METHOD;
	} else {
	    isPublic = Tcl_StringMatch(TclGetString(objv[1]), PUBLIC_PATTERN)
		    ? PUBLIC_METHOD : 0;
	}
    }

    /*
     * Create the method by using the right back-end API.
     */

    if (isInstanceMethod) {
	if (TclOONewProcInstanceMethod(interp, oPtr, isPublic, objv[1],
		objv[objc - 2], objv[objc - 1], NULL) == NULL) {
	    return TCL_ERROR;
	}
    } else {
	if (TclOONewProcMethod(interp, oPtr->classPtr, isPublic, objv[1],
		objv[objc - 2], objv[objc - 1], NULL) == NULL) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineRenameMethodObjCmd --
 *
 *	Implementation of the "renamemethod" subcommand of the "oo::define"
 *	and "oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineRenameMethodObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstanceRenameMethod = (clientData != NULL);
    Object *oPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "oldName newName");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    if (!isInstanceRenameMethod && !oPtr->classPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    /*
     * Delete the method entry from the appropriate hash table, and transfer
     * the thing it points to to its new entry. To do this, we first need to
     * get the entries from the appropriate hash tables (this can generate a
     * range of errors...)
     */

    if (RenameDeleteMethod(interp, oPtr, !isInstanceRenameMethod,
	    objv[1], objv[2]) != TCL_OK) {
	return TCL_ERROR;
    }

    if (isInstanceRenameMethod) {
	BumpInstanceEpoch(oPtr);
    } else {
	BumpGlobalEpoch(interp, oPtr->classPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineUnexportObjCmd --
 *
 *	Implementation of the "unexport" subcommand of the "oo::define" and
 *	"oo::objdefine" commands.
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineUnexportObjCmd(
    void *clientData,
    Tcl_Interp *interp,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    int isInstanceUnexport = (clientData != NULL);
    Object *oPtr;
    Method *mPtr;
    Tcl_HashEntry *hPtr;
    Class *clsPtr;
    int i, isNew, changed = 0;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "name ?name ...?");
	return TCL_ERROR;
    }

    oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    if (oPtr == NULL) {
	return TCL_ERROR;
    }
    clsPtr = oPtr->classPtr;
    if (!isInstanceUnexport && !clsPtr) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"attempt to misuse API", TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    }

    for (i = 1; i < objc; i++) {
	/*
	 * Unexporting is done by removing the PUBLIC_METHOD flag from the
	 * method record. If there is no such method in this object or class
	 * (i.e. the method comes from something inherited from or that we're
	 * an instance of) then we put in a blank record without that flag;
	 * such records are skipped over by the call chain engine *except* for
	 * their flags member.
	 */

	if (isInstanceUnexport) {
	    if (!oPtr->methodsPtr) {
		oPtr->methodsPtr = (Tcl_HashTable *)
			Tcl_Alloc(sizeof(Tcl_HashTable));
		Tcl_InitObjHashTable(oPtr->methodsPtr);
		oPtr->flags &= ~USE_CLASS_CACHE;
	    }
	    hPtr = Tcl_CreateHashEntry(oPtr->methodsPtr, objv[i],
		    &isNew);
	} else {
	    hPtr = Tcl_CreateHashEntry(&clsPtr->classMethods, objv[i],
		    &isNew);
	}

	if (isNew) {
	    mPtr = (Method *) Tcl_Alloc(sizeof(Method));
	    memset(mPtr, 0, sizeof(Method));
	    mPtr->refCount = 1;
	    mPtr->namePtr = objv[i];
	    Tcl_IncrRefCount(objv[i]);
	    Tcl_SetHashValue(hPtr, mPtr);
	} else {
	    mPtr = (Method *) Tcl_GetHashValue(hPtr);
	}
	if (isNew || mPtr->flags & (PUBLIC_METHOD | TRUE_PRIVATE_METHOD)) {
	    mPtr->flags &= ~(PUBLIC_METHOD | TRUE_PRIVATE_METHOD);
	    changed = 1;
	}
    }

    /*
     * Bump the right epoch if we actually changed anything.
     */

    if (changed) {
	if (isInstanceUnexport) {
	    BumpInstanceEpoch(oPtr);
	} else {
	    BumpGlobalEpoch(interp, clsPtr);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Tcl_ClassSetConstructor, Tcl_ClassSetDestructor --
 *
 *	How to install a constructor or destructor into a class; API to call
 *	from C.
 *
 * ----------------------------------------------------------------------
 */

void
Tcl_ClassSetConstructor(
    Tcl_Interp *interp,
    Tcl_Class clazz,
    Tcl_Method method)
{
    Class *clsPtr = (Class *) clazz;

    if (method != (Tcl_Method) clsPtr->constructorPtr) {
	TclOODelMethodRef(clsPtr->constructorPtr);
	clsPtr->constructorPtr = (Method *) method;

	/*
	 * Remember to invalidate the cached constructor chain for this class.
	 * [Bug 2531577]
	 */

	if (clsPtr->constructorChainPtr) {
	    TclOODeleteChain(clsPtr->constructorChainPtr);
	    clsPtr->constructorChainPtr = NULL;
	}
	BumpGlobalEpoch(interp, clsPtr);
    }
}

void
Tcl_ClassSetDestructor(
    Tcl_Interp *interp,
    Tcl_Class clazz,
    Tcl_Method method)
{
    Class *clsPtr = (Class *) clazz;

    if (method != (Tcl_Method) clsPtr->destructorPtr) {
	TclOODelMethodRef(clsPtr->destructorPtr);
	clsPtr->destructorPtr = (Method *) method;
	if (clsPtr->destructorChainPtr) {
	    TclOODeleteChain(clsPtr->destructorChainPtr);
	    clsPtr->destructorChainPtr = NULL;
	}
	BumpGlobalEpoch(interp, clsPtr);
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOODefineSlots --
 *
 *	Create the "::oo::Slot" class and its standard instances. Class
 *	definition is empty at the stage (added by scripting).
 *
 * ----------------------------------------------------------------------
 */

int
TclOODefineSlots(
    Foundation *fPtr)
{
    const DeclaredSlot *slotInfoPtr;
    Tcl_Interp *interp = fPtr->interp;
    Tcl_Obj *getName, *setName, *resolveName;
    Tcl_Object object = Tcl_NewObjectInstance(interp, (Tcl_Class)
	    fPtr->classCls, "::oo::Slot", NULL, TCL_INDEX_NONE, NULL, 0);
    Class *slotCls;

    if (object == NULL) {
	return TCL_ERROR;
    }
    slotCls = ((Object *) object)->classPtr;
    if (slotCls == NULL) {
	return TCL_ERROR;
    }

    TclNewLiteralStringObj(getName, "Get");
    TclNewLiteralStringObj(setName, "Set");
    TclNewLiteralStringObj(resolveName, "Resolve");
    for (slotInfoPtr = slots ; slotInfoPtr->name ; slotInfoPtr++) {
	Tcl_Object slotObject = Tcl_NewObjectInstance(interp,
		(Tcl_Class) slotCls, slotInfoPtr->name, NULL, TCL_INDEX_NONE,
		NULL, 0);

	if (slotObject == NULL) {
	    continue;
	}
	TclNewInstanceMethod(interp, slotObject, getName, 0,
		&slotInfoPtr->getterType, NULL);
	TclNewInstanceMethod(interp, slotObject, setName, 0,
		&slotInfoPtr->setterType, NULL);
	if (slotInfoPtr->resolverType.callProc) {
	    TclNewInstanceMethod(interp, slotObject, resolveName, 0,
		    &slotInfoPtr->resolverType, NULL);
	}
    }
    Tcl_BounceRefCount(getName);
    Tcl_BounceRefCount(setName);
    Tcl_BounceRefCount(resolveName);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassFilter_Get, ClassFilter_Set --
 *
 *	Implementation of the "filter" slot accessors of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassFilter_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Obj *resultObj, *filterObj;
    Tcl_Size i;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(filterObj, clsPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

static int
ClassFilter_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size filterc;
    Tcl_Obj **filterv;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"filterList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (TclListObjGetElements(interp, objv[0], &filterc,
	    &filterv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOClassSetFilters(interp, clsPtr, filterc, filterv);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassMixin_Get, ClassMixin_Set --
 *
 *	Implementation of the "mixin" slot accessors of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassMixin_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Obj *resultObj;
    Class *mixinPtr;
    Tcl_Size i;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(mixinPtr, clsPtr->mixins) {
	Tcl_ListObjAppendElement(NULL, resultObj,
		TclOOObjectName(interp, mixinPtr->thisPtr));
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

static int
ClassMixin_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size mixinc, i;
    Tcl_Obj **mixinv;
    Class **mixins;		/* The references to the classes to actually
				 * install. */
    Tcl_HashTable uniqueCheck;	/* Note that this hash table is just used as a
				 * set of class references; it has no payload
				 * values and keys are always pointers. */
    int isNew;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"mixinList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (TclListObjGetElements(interp, objv[0], &mixinc, &mixinv) != TCL_OK) {
	return TCL_ERROR;
    }

    mixins = (Class **) TclStackAlloc(interp, sizeof(Class *) * mixinc);
    Tcl_InitHashTable(&uniqueCheck, TCL_ONE_WORD_KEYS);

    for (i = 0; i < mixinc; i++) {
	mixins[i] = GetClassInOuterContext(interp, mixinv[i],
		"may only mix in classes");
	if (mixins[i] == NULL) {
	    i--;
	    goto freeAndError;
	}
	(void) Tcl_CreateHashEntry(&uniqueCheck, mixins[i], &isNew);
	if (!isNew) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "class should only be a direct mixin once",
		    TCL_AUTO_LENGTH));
	    OO_ERROR(interp, REPETITIOUS);
	    goto freeAndError;
	}
	if (TclOOIsReachable(clsPtr, mixins[i])) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "may not mix a class into itself", TCL_AUTO_LENGTH));
	    OO_ERROR(interp, SELF_MIXIN);
	    goto freeAndError;
	}
    }

    TclOOClassSetMixins(interp, clsPtr, mixinc, mixins);
    Tcl_DeleteHashTable(&uniqueCheck);
    TclStackFree(interp, mixins);
    return TCL_OK;

  freeAndError:
    Tcl_DeleteHashTable(&uniqueCheck);
    TclStackFree(interp, mixins);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassSuper_Get, ClassSuper_Set --
 *
 *	Implementation of the "superclass" slot accessors of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassSuper_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Obj *resultObj;
    Class *superPtr;
    Tcl_Size i;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
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

static int
ClassSuper_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size superc, j;
    Tcl_Size i;
    Tcl_Obj **superv;
    Class **superclasses, *superPtr;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"superclassList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    Foundation *fPtr = clsPtr->thisPtr->fPtr;
    if (clsPtr == fPtr->objectCls) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not modify the superclass of the root object",
		TCL_AUTO_LENGTH));
	OO_ERROR(interp, MONKEY_BUSINESS);
	return TCL_ERROR;
    } else if (TclListObjGetElements(interp, objv[0], &superc,
	    &superv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Allocate some working space.
     */

    superclasses = (Class **) Tcl_Alloc(sizeof(Class *) * superc);

    /*
     * Parse the arguments to get the class to use as superclasses.
     *
     * Note that zero classes is special, as it is equivalent to just the
     * class of objects. [Bug 9d61624b3d]
     */

    if (superc == 0) {
	superclasses = (Class **) Tcl_Realloc(superclasses, sizeof(Class *));
	if (TclOOIsReachable(fPtr->classCls, clsPtr)) {
	    superclasses[0] = fPtr->classCls;
	} else {
	    superclasses[0] = fPtr->objectCls;
	}
	superc = 1;
	AddRef(superclasses[0]->thisPtr);
    } else {
	for (i = 0; i < superc; i++) {
	    superclasses[i] = GetClassInOuterContext(interp, superv[i],
		    "only a class can be a superclass");
	    if (superclasses[i] == NULL) {
		goto failedAfterAlloc;
	    }
	    for (j = 0; j < i; j++) {
		if (superclasses[j] == superclasses[i]) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "class should only be a direct superclass once",
			    TCL_AUTO_LENGTH));
		    OO_ERROR(interp, REPETITIOUS);
		    goto failedAfterAlloc;
		}
	    }
	    if (TclOOIsReachable(clsPtr, superclasses[i])) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"attempt to form circular dependency graph",
			TCL_AUTO_LENGTH));
		OO_ERROR(interp, CIRCULARITY);
	    failedAfterAlloc:
		for (; i-- > 0 ;) {
		    TclOODecrRefCount(superclasses[i]->thisPtr);
		}
		Tcl_Free(superclasses);
		return TCL_ERROR;
	    }

	    /*
	     * Corresponding TclOODecrRefCount() is near the end of this
	     * function.
	     */

	    AddRef(superclasses[i]->thisPtr);
	}
    }

    /*
     * Install the list of superclasses into the class. Note that this also
     * involves splicing the class out of the superclasses' subclass list that
     * it used to be a member of and splicing it into the new superclasses'
     * subclass list.
     */

    if (clsPtr->superclasses.num != 0) {
	FOREACH(superPtr, clsPtr->superclasses) {
	    TclOORemoveFromSubclasses(clsPtr, superPtr);
	    TclOODecrRefCount(superPtr->thisPtr);
	}
	Tcl_Free(clsPtr->superclasses.list);
    }
    clsPtr->superclasses.list = superclasses;
    clsPtr->superclasses.num = superc;
    FOREACH(superPtr, clsPtr->superclasses) {
	TclOOAddToSubclasses(clsPtr, superPtr);
    }
    BumpGlobalEpoch(interp, clsPtr);

    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ClassVars_Get, ClassVars_Set --
 *
 *	Implementation of the "variable" slot accessors of the "oo::define"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ClassVars_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    if (IsPrivateDefine(interp)) {
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

static int
ClassVars_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size i;
    Tcl_Size varc;
    Tcl_Obj **varv;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"filterList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (TclListObjGetElements(interp, objv[0], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < varc; i++) {
	const char *varName = TclGetString(varv[i]);

	if (strstr(varName, "::") != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "invalid declared variable name \"%s\": must not %s",
		    varName, "contain namespace separators"));
	    OO_ERROR(interp, BAD_DECLVAR);
	    return TCL_ERROR;
	}
	if (Tcl_StringMatch(varName, "*(*)")) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "invalid declared variable name \"%s\": must not %s",
		    varName, "refer to an array element"));
	    OO_ERROR(interp, BAD_DECLVAR);
	    return TCL_ERROR;
	}
    }

    if (IsPrivateDefine(interp)) {
	InstallPrivateVariableMapping(&clsPtr->privateVariables,
		varc, varv, clsPtr->thisPtr->creationEpoch);
    } else {
	InstallStandardVariableMapping(&clsPtr->variables, varc, varv);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjFilter_Get, ObjFilter_Set --
 *
 *	Implementation of the "filter" slot accessors of the "oo::objdefine"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjFilter_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Obj *resultObj, *filterObj;
    Tcl_Size i;

    if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(filterObj, oPtr->filters) {
	Tcl_ListObjAppendElement(NULL, resultObj, filterObj);
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

static int
ObjFilter_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Size filterc;
    Tcl_Obj **filterv;

    if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"filterList");
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);
    if (TclListObjGetElements(interp, objv[0], &filterc, &filterv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOObjectSetFilters(oPtr, filterc, filterv);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjMixin_Get, ObjMixin_Set --
 *
 *	Implementation of the "mixin" slot accessors of the "oo::objdefine"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjMixin_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Obj *resultObj;
    Class *mixinPtr;
    Tcl_Size i;

    if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    FOREACH(mixinPtr, oPtr->mixins) {
	if (mixinPtr) {
	    Tcl_ListObjAppendElement(NULL, resultObj,
		    TclOOObjectName(interp, mixinPtr->thisPtr));
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

static int
ObjMixin_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Size mixinc, i;
    Tcl_Obj **mixinv;
    Class **mixins;		/* The references to the classes to actually
				 * install. */
    Tcl_HashTable uniqueCheck;	/* Note that this hash table is just used as a
				 * set of class references; it has no payload
				 * values and keys are always pointers. */
    int isNew;

    if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"mixinList");
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);
    if (TclListObjGetElements(interp, objv[0], &mixinc, &mixinv) != TCL_OK) {
	return TCL_ERROR;
    }

    mixins = (Class **) TclStackAlloc(interp, sizeof(Class *) * mixinc);
    Tcl_InitHashTable(&uniqueCheck, TCL_ONE_WORD_KEYS);

    for (i = 0; i < mixinc; i++) {
	mixins[i] = GetClassInOuterContext(interp, mixinv[i],
		"may only mix in classes");
	if (mixins[i] == NULL) {
	    goto freeAndError;
	}
	(void) Tcl_CreateHashEntry(&uniqueCheck, mixins[i], &isNew);
	if (!isNew) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "class should only be a direct mixin once",
		    TCL_AUTO_LENGTH));
	    OO_ERROR(interp, REPETITIOUS);
	    goto freeAndError;
	}
    }

    TclOOObjectSetMixins(oPtr, mixinc, mixins);
    TclStackFree(interp, mixins);
    Tcl_DeleteHashTable(&uniqueCheck);
    return TCL_OK;

  freeAndError:
    TclStackFree(interp, mixins);
    Tcl_DeleteHashTable(&uniqueCheck);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------
 *
 * ObjVars_Get, ObjVars_Set --
 *
 *	Implementation of the "variable" slot accessors of the "oo::objdefine"
 *	command.
 *
 * ----------------------------------------------------------------------
 */

static int
ObjVars_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    if (IsPrivateDefine(interp)) {
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

static int
ObjVars_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Size varc, i;
    Tcl_Obj **varv;

    if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"variableList");
	return TCL_ERROR;
    } else if (oPtr == NULL) {
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);
    if (TclListObjGetElements(interp, objv[0], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < varc; i++) {
	const char *varName = TclGetString(varv[i]);

	if (strstr(varName, "::") != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "invalid declared variable name \"%s\": must not %s",
		    varName, "contain namespace separators"));
	    OO_ERROR(interp, BAD_DECLVAR);
	    return TCL_ERROR;
	}
	if (Tcl_StringMatch(varName, "*(*)")) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "invalid declared variable name \"%s\": must not %s",
		    varName, "refer to an array element"));
	    OO_ERROR(interp, BAD_DECLVAR);
	    return TCL_ERROR;
	}
    }

    if (IsPrivateDefine(interp)) {
	InstallPrivateVariableMapping(&oPtr->privateVariables, varc, varv,
		oPtr->creationEpoch);
    } else {
	InstallStandardVariableMapping(&oPtr->variables, varc, varv);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * ResolveClass --
 *
 *	Implementation of the "Resolve" support method for some slots (those
 *	that are slots around a list of classes). This resolves possible class
 *	names to their fully-qualified names if possible.
 *
 * ----------------------------------------------------------------------
 */

static int
ResolveClass(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Tcl_Size idx = Tcl_ObjectContextSkippedArgs(context);
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Class *clsPtr;

    /*
     * Check if were called wrongly. The definition context isn't used...
     * except that GetClassInOuterContext() assumes that it is there.
     */

    if (oPtr == NULL) {
	return TCL_ERROR;
    } else if (objc != idx + 1) {
	Tcl_WrongNumArgs(interp, idx, objv, "slotElement");
	return TCL_ERROR;
    }

    /*
     * Resolve the class if possible. If not, remove any resolution error and
     * return what we've got anyway as the failure might not be fatal overall.
     */

    clsPtr = GetClassInOuterContext(interp, objv[idx],
	    "USER SHOULD NOT SEE THIS MESSAGE");
    if (clsPtr == NULL) {
	Tcl_ResetResult(interp);
	Tcl_SetObjResult(interp, objv[idx]);
    } else {
	Tcl_SetObjResult(interp, TclOOObjectName(interp, clsPtr->thisPtr));
    }

    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Configurable_ClassReadableProps_Get, Configurable_ClassReadableProps_Set,
 * Configurable_ObjectReadableProps_Get, Configurable_ObjectReadableProps_Set --
 *
 *	Implementations of the "readableproperties" slot accessors for classes
 *	and instances.
 *
 * ----------------------------------------------------------------------
 */

static int
Configurable_ClassReadableProps_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetPropertyList(&clsPtr->properties.readable));
    return TCL_OK;
}

static int
Configurable_ClassReadableProps_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size varc;
    Tcl_Obj **varv;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"filterList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (TclListObjGetElements(interp, objv[0], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOInstallReadableProperties(&clsPtr->properties, varc, varv);
    BumpGlobalEpoch(interp, clsPtr);
    return TCL_OK;
}

static int
Configurable_ObjectReadableProps_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);

    if (oPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetPropertyList(&oPtr->properties.readable));
    return TCL_OK;
}

static int
Configurable_ObjectReadableProps_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Size varc;
    Tcl_Obj **varv;

    if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"filterList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (oPtr == NULL) {
	return TCL_ERROR;
    } else if (TclListObjGetElements(interp, objv[0], &varc,
	    &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOInstallReadableProperties(&oPtr->properties, varc, varv);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * Configurable_ClassWritableProps_Get, Configurable_ClassWritableProps_Set,
 * Configurable_ObjectWritableProps_Get, Configurable_ObjectWritableProps_Set --
 *
 *	Implementations of the "writableproperties" slot accessors for classes
 *	and instances.
 *
 * ----------------------------------------------------------------------
 */

static int
Configurable_ClassWritableProps_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetPropertyList(&clsPtr->properties.writable));
    return TCL_OK;
}

static int
Configurable_ClassWritableProps_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Class *clsPtr = TclOOGetClassDefineCmdContext(interp);
    Tcl_Size varc;
    Tcl_Obj **varv;

    if (clsPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"propertyList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (TclListObjGetElements(interp, objv[0], &varc, &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOInstallWritableProperties(&clsPtr->properties, varc, varv);
    BumpGlobalEpoch(interp, clsPtr);
    return TCL_OK;
}

static int
Configurable_ObjectWritableProps_Get(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);

    if (oPtr == NULL) {
	return TCL_ERROR;
    } else if (Tcl_ObjectContextSkippedArgs(context) != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, TclOOGetPropertyList(&oPtr->properties.writable));
    return TCL_OK;
}

static int
Configurable_ObjectWritableProps_Set(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    Tcl_ObjectContext context,
    Tcl_Size objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *) TclOOGetDefineCmdContext(interp);
    Tcl_Size varc;
    Tcl_Obj **varv;

    if (Tcl_ObjectContextSkippedArgs(context) + 1 != objc) {
	Tcl_WrongNumArgs(interp, Tcl_ObjectContextSkippedArgs(context), objv,
		"propertyList");
	return TCL_ERROR;
    }
    objv += Tcl_ObjectContextSkippedArgs(context);

    if (oPtr == NULL) {
	return TCL_ERROR;
    } else if (TclListObjGetElements(interp, objv[0], &varc,
	    &varv) != TCL_OK) {
	return TCL_ERROR;
    }

    TclOOInstallWritableProperties(&oPtr->properties, varc, varv);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOORegisterProperty, TclOORegisterInstanceProperty --
 *
 *	Helpers to add or remove a name from the property slots of a class or
 *	instance.
 *
 * BuildPropertyList --
 *
 *	Helper for the helpers. Scans a property list and does the filtering
 *	or adding of the property to add or remove
 *
 * ----------------------------------------------------------------------
 */

static int
BuildPropertyList(
    PropertyList *propsList,	/* Property list to scan. */
    Tcl_Obj *propName,		/* Property to add/remove. */
    int addingProp,		/* True if we're adding, false if removing. */
    Tcl_Obj *listObj)		/* The list of property names we're building */
{
    int present = 0, changed = 0, i;
    Tcl_Obj *other;

    Tcl_SetListObj(listObj, 0, NULL);
    FOREACH(other, *propsList) {
	if (!TclStringCmp(propName, other, 1, 0, TCL_INDEX_NONE)) {
	    present = 1;
	    if (!addingProp) {
		changed = 1;
		continue;
	    }
	}
	Tcl_ListObjAppendElement(NULL, listObj, other);
    }
    if (!present && addingProp) {
	Tcl_ListObjAppendElement(NULL, listObj, propName);
	changed = 1;
    }
    return changed;
}

void
TclOORegisterInstanceProperty(
    Object *oPtr,		/* Object that owns the property slots. */
    Tcl_Obj *propName,		/* Property to add/remove. Must include the
				 * hyphen if one is desired; this is the value
				 * that is actually placed in the slot. */
    int registerReader,		/* True if we're adding the property name to
				 * the readable property slot. False if we're
				 * removing the property name from the slot. */
    int registerWriter)		/* True if we're adding the property name to
				 * the writable property slot. False if we're
				 * removing the property name from the slot. */
{
    Tcl_Obj *listObj = Tcl_NewObj();	/* Working buffer. */
    Tcl_Obj **objv;
    Tcl_Size count;

    if (BuildPropertyList(&oPtr->properties.readable, propName, registerReader,
	    listObj)) {
	TclListObjGetElements(NULL, listObj, &count, &objv);
	TclOOInstallReadableProperties(&oPtr->properties, count, objv);
    }

    if (BuildPropertyList(&oPtr->properties.writable, propName, registerWriter,
	    listObj)) {
	TclListObjGetElements(NULL, listObj, &count, &objv);
	TclOOInstallWritableProperties(&oPtr->properties, count, objv);
    }
    Tcl_BounceRefCount(listObj);
}

void
TclOORegisterProperty(
    Class *clsPtr,		/* Class that owns the property slots. */
    Tcl_Obj *propName,		/* Property to add/remove. Must include the
				 * hyphen if one is desired; this is the value
				 * that is actually placed in the slot. */
    int registerReader,		/* True if we're adding the property name to
				 * the readable property slot. False if we're
				 * removing the property name from the slot. */
    int registerWriter)		/* True if we're adding the property name to
				 * the writable property slot. False if we're
				 * removing the property name from the slot. */
{
    Tcl_Obj *listObj = Tcl_NewObj();	/* Working buffer. */
    Tcl_Obj **objv;
    Tcl_Size count;
    int changed = 0;

    if (BuildPropertyList(&clsPtr->properties.readable, propName,
	    registerReader, listObj)) {
	TclListObjGetElements(NULL, listObj, &count, &objv);
	TclOOInstallReadableProperties(&clsPtr->properties, count, objv);
	changed = 1;
    }

    if (BuildPropertyList(&clsPtr->properties.writable, propName,
	    registerWriter, listObj)) {
	TclListObjGetElements(NULL, listObj, &count, &objv);
	TclOOInstallWritableProperties(&clsPtr->properties, count, objv);
	changed = 1;
    }
    Tcl_BounceRefCount(listObj);
    if (changed) {
	BumpGlobalEpoch(clsPtr->thisPtr->fPtr->interp, clsPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
