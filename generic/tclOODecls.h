/*
 * $Id: tclOODecls.h,v 1.2 2008/05/31 23:35:27 das Exp $
 *
 * This file is (mostly) automatically generated from tclOO.decls.
 */


#if defined(USE_TCLOO_STUBS)
extern const char *TclOOInitializeStubs(
	Tcl_Interp *, const char *version, int epoch, int revision);
#define Tcl_OOInitStubs(interp) TclOOInitializeStubs( \
	interp, TCLOO_VERSION, TCLOO_STUBS_EPOCH, TCLOO_STUBS_REVISION)
#else
#define Tcl_OOInitStubs(interp) Tcl_PkgRequire(interp, "TclOO", TCLOO_VERSION)
#endif


/* !BEGIN!: Do not edit below this line. */

#define TCLOO_STUBS_EPOCH 0
#define TCLOO_STUBS_REVISION 44

#if !defined(USE_TCLOO_STUBS)

/*
 * Exported function declarations:
 */

/* 0 */
TCLOOAPI Tcl_Object	Tcl_CopyObjectInstance (Tcl_Interp * interp, 
				Tcl_Object sourceObject, 
				const char * targetName, 
				const char * targetNamespaceName);
/* 1 */
TCLOOAPI Tcl_Object	Tcl_GetClassAsObject (Tcl_Class clazz);
/* 2 */
TCLOOAPI Tcl_Class	Tcl_GetObjectAsClass (Tcl_Object object);
/* 3 */
TCLOOAPI Tcl_Command	Tcl_GetObjectCommand (Tcl_Object object);
/* 4 */
TCLOOAPI Tcl_Object	Tcl_GetObjectFromObj (Tcl_Interp * interp, 
				Tcl_Obj * objPtr);
/* 5 */
TCLOOAPI Tcl_Namespace * Tcl_GetObjectNamespace (Tcl_Object object);
/* 6 */
TCLOOAPI Tcl_Class	Tcl_MethodDeclarerClass (Tcl_Method method);
/* 7 */
TCLOOAPI Tcl_Object	Tcl_MethodDeclarerObject (Tcl_Method method);
/* 8 */
TCLOOAPI int		Tcl_MethodIsPublic (Tcl_Method method);
/* 9 */
TCLOOAPI int		Tcl_MethodIsType (Tcl_Method method, 
				const Tcl_MethodType * typePtr, 
				ClientData * clientDataPtr);
/* 10 */
TCLOOAPI Tcl_Obj *	Tcl_MethodName (Tcl_Method method);
/* 11 */
TCLOOAPI Tcl_Method	Tcl_NewInstanceMethod (Tcl_Interp * interp, 
				Tcl_Object object, Tcl_Obj * nameObj, 
				int isPublic, const Tcl_MethodType * typePtr, 
				ClientData clientData);
/* 12 */
TCLOOAPI Tcl_Method	Tcl_NewMethod (Tcl_Interp * interp, Tcl_Class cls, 
				Tcl_Obj * nameObj, int isPublic, 
				const Tcl_MethodType * typePtr, 
				ClientData clientData);
/* 13 */
TCLOOAPI Tcl_Object	Tcl_NewObjectInstance (Tcl_Interp * interp, 
				Tcl_Class cls, const char * nameStr, 
				const char * nsNameStr, int objc, 
				Tcl_Obj *const * objv, int skip);
/* 14 */
TCLOOAPI int		Tcl_ObjectDeleted (Tcl_Object object);
/* 15 */
TCLOOAPI int		Tcl_ObjectContextIsFiltering (
				Tcl_ObjectContext context);
/* 16 */
TCLOOAPI Tcl_Method	Tcl_ObjectContextMethod (Tcl_ObjectContext context);
/* 17 */
TCLOOAPI Tcl_Object	Tcl_ObjectContextObject (Tcl_ObjectContext context);
/* 18 */
TCLOOAPI int		Tcl_ObjectContextSkippedArgs (
				Tcl_ObjectContext context);
/* 19 */
TCLOOAPI ClientData	Tcl_ClassGetMetadata (Tcl_Class clazz, 
				const Tcl_ObjectMetadataType * typePtr);
/* 20 */
TCLOOAPI void		Tcl_ClassSetMetadata (Tcl_Class clazz, 
				const Tcl_ObjectMetadataType * typePtr, 
				ClientData metadata);
/* 21 */
TCLOOAPI ClientData	Tcl_ObjectGetMetadata (Tcl_Object object, 
				const Tcl_ObjectMetadataType * typePtr);
/* 22 */
TCLOOAPI void		Tcl_ObjectSetMetadata (Tcl_Object object, 
				const Tcl_ObjectMetadataType * typePtr, 
				ClientData metadata);
/* 23 */
TCLOOAPI int		Tcl_ObjectContextInvokeNext (Tcl_Interp * interp, 
				Tcl_ObjectContext context, int objc, 
				Tcl_Obj *const * objv, int skip);
/* 24 */
TCLOOAPI Tcl_ObjectMapMethodNameProc Tcl_ObjectGetMethodNameMapper (
				Tcl_Object object);
/* 25 */
TCLOOAPI void		Tcl_ObjectSetMethodNameMapper (Tcl_Object object, 
				Tcl_ObjectMapMethodNameProc mapMethodNameProc);
/* 26 */
TCLOOAPI void		Tcl_ClassSetConstructor (Tcl_Interp * interp, 
				Tcl_Class clazz, Tcl_Method method);
/* 27 */
TCLOOAPI void		Tcl_ClassSetDestructor (Tcl_Interp * interp, 
				Tcl_Class clazz, Tcl_Method method);

#endif /* !defined(USE_TCLOO_STUBS) */

typedef struct TclOOStubs {
    int magic;
    int epoch;
    int revision;
    CONST struct TclOOStubHooks *hooks;

    Tcl_Object (*tcl_CopyObjectInstance) (Tcl_Interp * interp, Tcl_Object sourceObject, const char * targetName, const char * targetNamespaceName); /* 0 */
    Tcl_Object (*tcl_GetClassAsObject) (Tcl_Class clazz); /* 1 */
    Tcl_Class (*tcl_GetObjectAsClass) (Tcl_Object object); /* 2 */
    Tcl_Command (*tcl_GetObjectCommand) (Tcl_Object object); /* 3 */
    Tcl_Object (*tcl_GetObjectFromObj) (Tcl_Interp * interp, Tcl_Obj * objPtr); /* 4 */
    Tcl_Namespace * (*tcl_GetObjectNamespace) (Tcl_Object object); /* 5 */
    Tcl_Class (*tcl_MethodDeclarerClass) (Tcl_Method method); /* 6 */
    Tcl_Object (*tcl_MethodDeclarerObject) (Tcl_Method method); /* 7 */
    int (*tcl_MethodIsPublic) (Tcl_Method method); /* 8 */
    int (*tcl_MethodIsType) (Tcl_Method method, const Tcl_MethodType * typePtr, ClientData * clientDataPtr); /* 9 */
    Tcl_Obj * (*tcl_MethodName) (Tcl_Method method); /* 10 */
    Tcl_Method (*tcl_NewInstanceMethod) (Tcl_Interp * interp, Tcl_Object object, Tcl_Obj * nameObj, int isPublic, const Tcl_MethodType * typePtr, ClientData clientData); /* 11 */
    Tcl_Method (*tcl_NewMethod) (Tcl_Interp * interp, Tcl_Class cls, Tcl_Obj * nameObj, int isPublic, const Tcl_MethodType * typePtr, ClientData clientData); /* 12 */
    Tcl_Object (*tcl_NewObjectInstance) (Tcl_Interp * interp, Tcl_Class cls, const char * nameStr, const char * nsNameStr, int objc, Tcl_Obj *const * objv, int skip); /* 13 */
    int (*tcl_ObjectDeleted) (Tcl_Object object); /* 14 */
    int (*tcl_ObjectContextIsFiltering) (Tcl_ObjectContext context); /* 15 */
    Tcl_Method (*tcl_ObjectContextMethod) (Tcl_ObjectContext context); /* 16 */
    Tcl_Object (*tcl_ObjectContextObject) (Tcl_ObjectContext context); /* 17 */
    int (*tcl_ObjectContextSkippedArgs) (Tcl_ObjectContext context); /* 18 */
    ClientData (*tcl_ClassGetMetadata) (Tcl_Class clazz, const Tcl_ObjectMetadataType * typePtr); /* 19 */
    void (*tcl_ClassSetMetadata) (Tcl_Class clazz, const Tcl_ObjectMetadataType * typePtr, ClientData metadata); /* 20 */
    ClientData (*tcl_ObjectGetMetadata) (Tcl_Object object, const Tcl_ObjectMetadataType * typePtr); /* 21 */
    void (*tcl_ObjectSetMetadata) (Tcl_Object object, const Tcl_ObjectMetadataType * typePtr, ClientData metadata); /* 22 */
    int (*tcl_ObjectContextInvokeNext) (Tcl_Interp * interp, Tcl_ObjectContext context, int objc, Tcl_Obj *const * objv, int skip); /* 23 */
    Tcl_ObjectMapMethodNameProc (*tcl_ObjectGetMethodNameMapper) (Tcl_Object object); /* 24 */
    void (*tcl_ObjectSetMethodNameMapper) (Tcl_Object object, Tcl_ObjectMapMethodNameProc mapMethodNameProc); /* 25 */
    void (*tcl_ClassSetConstructor) (Tcl_Interp * interp, Tcl_Class clazz, Tcl_Method method); /* 26 */
    void (*tcl_ClassSetDestructor) (Tcl_Interp * interp, Tcl_Class clazz, Tcl_Method method); /* 27 */
} TclOOStubs;

extern CONST TclOOStubs *tclOOStubsPtr;

#if defined(USE_TCLOO_STUBS)

/*
 * Inline function declarations:
 */

#ifndef Tcl_CopyObjectInstance
#define Tcl_CopyObjectInstance \
	(tclOOStubsPtr->tcl_CopyObjectInstance) /* 0 */
#endif
#ifndef Tcl_GetClassAsObject
#define Tcl_GetClassAsObject \
	(tclOOStubsPtr->tcl_GetClassAsObject) /* 1 */
#endif
#ifndef Tcl_GetObjectAsClass
#define Tcl_GetObjectAsClass \
	(tclOOStubsPtr->tcl_GetObjectAsClass) /* 2 */
#endif
#ifndef Tcl_GetObjectCommand
#define Tcl_GetObjectCommand \
	(tclOOStubsPtr->tcl_GetObjectCommand) /* 3 */
#endif
#ifndef Tcl_GetObjectFromObj
#define Tcl_GetObjectFromObj \
	(tclOOStubsPtr->tcl_GetObjectFromObj) /* 4 */
#endif
#ifndef Tcl_GetObjectNamespace
#define Tcl_GetObjectNamespace \
	(tclOOStubsPtr->tcl_GetObjectNamespace) /* 5 */
#endif
#ifndef Tcl_MethodDeclarerClass
#define Tcl_MethodDeclarerClass \
	(tclOOStubsPtr->tcl_MethodDeclarerClass) /* 6 */
#endif
#ifndef Tcl_MethodDeclarerObject
#define Tcl_MethodDeclarerObject \
	(tclOOStubsPtr->tcl_MethodDeclarerObject) /* 7 */
#endif
#ifndef Tcl_MethodIsPublic
#define Tcl_MethodIsPublic \
	(tclOOStubsPtr->tcl_MethodIsPublic) /* 8 */
#endif
#ifndef Tcl_MethodIsType
#define Tcl_MethodIsType \
	(tclOOStubsPtr->tcl_MethodIsType) /* 9 */
#endif
#ifndef Tcl_MethodName
#define Tcl_MethodName \
	(tclOOStubsPtr->tcl_MethodName) /* 10 */
#endif
#ifndef Tcl_NewInstanceMethod
#define Tcl_NewInstanceMethod \
	(tclOOStubsPtr->tcl_NewInstanceMethod) /* 11 */
#endif
#ifndef Tcl_NewMethod
#define Tcl_NewMethod \
	(tclOOStubsPtr->tcl_NewMethod) /* 12 */
#endif
#ifndef Tcl_NewObjectInstance
#define Tcl_NewObjectInstance \
	(tclOOStubsPtr->tcl_NewObjectInstance) /* 13 */
#endif
#ifndef Tcl_ObjectDeleted
#define Tcl_ObjectDeleted \
	(tclOOStubsPtr->tcl_ObjectDeleted) /* 14 */
#endif
#ifndef Tcl_ObjectContextIsFiltering
#define Tcl_ObjectContextIsFiltering \
	(tclOOStubsPtr->tcl_ObjectContextIsFiltering) /* 15 */
#endif
#ifndef Tcl_ObjectContextMethod
#define Tcl_ObjectContextMethod \
	(tclOOStubsPtr->tcl_ObjectContextMethod) /* 16 */
#endif
#ifndef Tcl_ObjectContextObject
#define Tcl_ObjectContextObject \
	(tclOOStubsPtr->tcl_ObjectContextObject) /* 17 */
#endif
#ifndef Tcl_ObjectContextSkippedArgs
#define Tcl_ObjectContextSkippedArgs \
	(tclOOStubsPtr->tcl_ObjectContextSkippedArgs) /* 18 */
#endif
#ifndef Tcl_ClassGetMetadata
#define Tcl_ClassGetMetadata \
	(tclOOStubsPtr->tcl_ClassGetMetadata) /* 19 */
#endif
#ifndef Tcl_ClassSetMetadata
#define Tcl_ClassSetMetadata \
	(tclOOStubsPtr->tcl_ClassSetMetadata) /* 20 */
#endif
#ifndef Tcl_ObjectGetMetadata
#define Tcl_ObjectGetMetadata \
	(tclOOStubsPtr->tcl_ObjectGetMetadata) /* 21 */
#endif
#ifndef Tcl_ObjectSetMetadata
#define Tcl_ObjectSetMetadata \
	(tclOOStubsPtr->tcl_ObjectSetMetadata) /* 22 */
#endif
#ifndef Tcl_ObjectContextInvokeNext
#define Tcl_ObjectContextInvokeNext \
	(tclOOStubsPtr->tcl_ObjectContextInvokeNext) /* 23 */
#endif
#ifndef Tcl_ObjectGetMethodNameMapper
#define Tcl_ObjectGetMethodNameMapper \
	(tclOOStubsPtr->tcl_ObjectGetMethodNameMapper) /* 24 */
#endif
#ifndef Tcl_ObjectSetMethodNameMapper
#define Tcl_ObjectSetMethodNameMapper \
	(tclOOStubsPtr->tcl_ObjectSetMethodNameMapper) /* 25 */
#endif
#ifndef Tcl_ClassSetConstructor
#define Tcl_ClassSetConstructor \
	(tclOOStubsPtr->tcl_ClassSetConstructor) /* 26 */
#endif
#ifndef Tcl_ClassSetDestructor
#define Tcl_ClassSetDestructor \
	(tclOOStubsPtr->tcl_ClassSetDestructor) /* 27 */
#endif

#endif /* defined(USE_TCLOO_STUBS) */

/* !END!: Do not edit above this line. */
