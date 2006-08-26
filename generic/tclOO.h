/*
 * tclOO.c --
 *
 *	This file contains the structures for the object-system (NB:
 *	not Tcl_Obj, but ::oo)
 *
 * Copyright (c) 2006 by Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclOO.h,v 1.1.2.11 2006/08/26 22:15:49 dkf Exp $
 */

/*
 * Forward declarations.
 */

struct Class;
struct Object;
struct Method;
struct CallContext;
//struct Foundation;

/*
 * The types of callbacks used in method implementations.
 */

typedef int (*Tcl_OOMethodCallProc)(ClientData clientData, Tcl_Interp *interp,
	struct CallContext *contextPtr, int objc, Tcl_Obj *const *objv);
typedef void (*Tcl_OOMethodDeleteProc)(ClientData clientData);

/*
 * The data that needs to be stored per method. This record is used to collect
 * information about all sorts of methods, including forwards, constructors
 * and destructors.
 */

typedef struct Method {
    Tcl_OOMethodCallProc callPtr;
    ClientData clientData;
    Tcl_OOMethodDeleteProc deletePtr;
    Tcl_Obj *namePtr;
    struct Object *declaringObjectPtr;
    struct Class *declaringClassPtr;
    int epoch;
    int flags;
} Method;

/*
 * Procedure-like methods have the following extra information.
 */

typedef struct ProcedureMethod {
    Proc *procPtr;
} ProcedureMethod;

/*
 * Forwarded methods have the following extra information.
 */

typedef struct ForwardMethod {
    Tcl_Obj *prefixObj;
} ForwardMethod;

/*
 * Now, the definition of what an object actually is.
 */

typedef struct Object {
    Namespace *nsPtr;		/* This object's tame namespace. */
    Tcl_Command command;	/* Reference to this object's public
				 * command. */
    Tcl_Command myCommand;	/* Reference to this object's internal
				 * command. */
    struct Class *selfCls;	/* This object's class. */
    Tcl_HashTable methods;	/* Tcl_Obj (method name) to Method*
				 * mapping. */
    struct {			/* Classes mixed into this object. */
	int num;
	struct Class **list;
    } mixins;
    struct {
	int num;
	Tcl_Obj **list;
    } filters;
    struct Class *classPtr;	/* All classes have this non-NULL; it points
				 * to the class structure. Everything else has
				 * this NULL. */
    int flags;
    Tcl_HashTable publicContextCache;	/* Place to keep unused contexts. */
    Tcl_HashTable privateContextCache;	/* Place to keep unused contexts. */
} Object;

#define OBJECT_DELETED	1	/* Flag to say that an object has been
				 * destroyed. */

/*
 * And the definition of a class. Note that every class also has an associated
 * object, through which it is manipulated.
 */

typedef struct Class {
    struct Object *thisPtr;
    int flags;
    struct {
	int num;
	struct Class **list;
    } superclasses;
    struct {
	int num;
	struct Class **list;
	int size;
    } subclasses;
    struct {
	int num;
	struct Object **list;
	int size;
    } instances;
    Tcl_HashTable classMethods;
    struct Method *constructorPtr;
    struct Method *destructorPtr;
} Class;

/*
 * The foundation of the object system within an interpreter contains
 * references to the key classes and namespaces, together with a few other
 * useful bits and pieces. Probably ought to eventually go in the Interp
 * structure itself.
 */

//typedef struct ObjectStack {
//    Object *oPtr;
//    struct ObjectStack *nextPtr;
//} ObjectStack;

typedef struct Foundation {
    struct Class *objectCls;	/* The root of the object system. */
    struct Class *classCls;	/* The class of all classes. */
    struct Class *definerCls;	/* A metaclass that includes methods that make
				 * classes more convenient to work with at a
				 * cost of bloat. */
    struct Class *structCls;	/* A metaclass that includes methods that make
				 * it easier to build data-oriented
				 * classes. */
    Tcl_Namespace *defineNs;	/* Namespace containing special commands for
				 * manipulating objects and classes. The
				 * "oo::define" command acts as a special kind
				 * of ensemble for this namespace. */
    Tcl_Namespace *helpersNs;	/* Namespace containing the commands that are
				 * only valid when executing inside a
				 * procedural method. */
    int epoch;
    int nsCount;
    Tcl_Obj *unknownMethodNameObj;
    //ObjectStack *objStack;	// should this be in stack frames?
} Foundation;

/*
 * A call context structure is built when a method is called. They contain the
 * chain of method implementations that are to be invoked by a particular
 * call, and the process of calling walks the chain, with the [next] command
 * proceeding to the next entry in the chain.
 */

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
    int skip;
    int numCallChain;
    struct MInvoke *callChain;
    struct MInvoke staticCallChain[CALL_CHAIN_STATIC_SIZE];
    int filterLength;
} CallContext;

/*
 * Bits for the 'flags' field of the call context.
 */

#define OO_UNKNOWN_METHOD	1
#define PUBLIC_METHOD		2
#define CONSTRUCTOR		4
#define DESTRUCTOR		8

/*
 * Private definitions, some of which perhaps ought to be exposed properly or
 * maybe just put in the internal stubs table.
 */

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
MODULE_SCOPE int	TclObjInterpProcCore(register Tcl_Interp *interp,
			    CallFrame *framePtr, Tcl_Obj *procNameObj,
			    int skip);
// Expose this one?
MODULE_SCOPE int	TclOOIsReachable(Class *targetPtr, Class *startPtr);

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
