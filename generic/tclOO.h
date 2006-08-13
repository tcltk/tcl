/*
 * tclOO.c --
 *
 *	This file contains the structures for the object-system (NB:
 *	not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2005 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOO.h,v 1.1.2.4 2006/08/13 21:35:57 dkf Exp $
 */

struct Class;
struct Object;
struct Method;
struct CallContext;
//struct Foundation;

typedef int (*Tcl_OOMethodCallProc)(ClientData clientData, Tcl_Interp *interp,
	struct CallContext *contextPtr, int objc, Tcl_Obj *const *objv);
typedef void (*Tcl_OOMethodDeleteProc)(ClientData clientData);

typedef struct Method {
    Tcl_OOMethodCallProc callPtr;
    ClientData clientData;
    Tcl_OOMethodDeleteProc deletePtr;
    int epoch;
    int flags;
} Method;

typedef struct ProcedureMethod {
    Tcl_Obj *bodyObj;
    Proc *procPtr;
    int formalc;
    Tcl_Obj **formalv;
} ProcedureMethod;

typedef struct ForwardMethod {
    Tcl_Obj *prefixObj;
} ForwardMethod;


typedef struct Object {
    Namespace *nsPtr;		/* This object's tame namespace. */
    Tcl_Command command;	/* Reference to this object's public
				 * command. */
    Tcl_Command myCommand;	/* Reference to this object's internal
				 * command. */
    struct Class *selfCls;	/* This object's class. */
    Tcl_HashTable methods;	/* Tcl_Obj (method name) to Method*
				 * mapping. */
    int numMixins;		/* Number of classes mixed into this
				 * object. */
    struct Class **mixins;	/* References to classes mixed into this
				 * object. */
    int numFilters;
    Tcl_Obj **filterObjs;
    struct Class *classPtr;	/* All classes have this non-NULL; it points
				 * to the class structure. Everything else has
				 * this NULL. */
    Tcl_Interp *interp;		/* The interpreter (for the PushObject and
				 * PopObject callbacks. */
    Tcl_HashTable publicContextCache;	/* Place to keep unused contexts. */
    Tcl_HashTable privateContextCache;	/* Place to keep unused contexts. */
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

typedef struct ObjectStack {
    Object *oPtr;
    struct ObjectStack *nextPtr;
} ObjectStack;

typedef struct Foundation {
    struct Class *objectCls;
    struct Class *classCls;
    struct Class *definerCls;
    struct Class *structCls;
    Tcl_Namespace *defineNs;
    Tcl_Namespace *helpersNs;
    int epoch;
    int nsCount;
    Tcl_Obj *unknownMethodNameObj;
    ObjectStack *objStack;	// should this be in stack frames?
} Foundation;

#define CALL_CHAIN_STATIC_SIZE 4

struct MInvoke {
    Method *mPtr;
    int isFilter;
};
typedef struct CallContext {
    Object *oPtr;
    int epoch;
    int flags;
    int index;
    int numCallChain;
    struct MInvoke **callChain;
    struct MInvoke *staticCallChain[CALL_CHAIN_STATIC_SIZE];
    int filterLength;
} CallContext;

#define OO_UNKNOWN_METHOD	1
#define PUBLIC_METHOD		2

MODULE_SCOPE Object *	TclGetObjectFromObj(Tcl_Interp *interp,
			    Tcl_Obj *objPtr);
MODULE_SCOPE Method *	TclNewProcMethod(Tcl_Interp *interp, Object *oPtr,
			    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
			    Tcl_Obj *bodyObj);
MODULE_SCOPE Method *	TclNewForwardMethod(Tcl_Interp *interp, Object *oPtr,
			    int isPublic, Tcl_Obj *nameObj,
			    Tcl_Obj *prefixObj);
MODULE_SCOPE Method *	TclNewProcClassMethod(Tcl_Interp *interp, Class *cPtr,
			    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
			    Tcl_Obj *bodyObj);
MODULE_SCOPE Method *	TclNewForwardClassMethod(Tcl_Interp *interp,
			    Class *cPtr, int isPublic, Tcl_Obj *nameObj,
			    Tcl_Obj *prefixObj);
MODULE_SCOPE void	TclDeleteMethod(Method *method);

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
