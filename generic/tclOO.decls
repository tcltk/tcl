# -*- tcl -*-
# $Id: tclOO.decls,v 1.1.2.2 2008/05/31 21:02:04 dgp Exp $

# public API
library tclOO
interface tclOO
epoch 0
scspec TCLOOAPI

declare 0 current {
    Tcl_Object Tcl_CopyObjectInstance(Tcl_Interp *interp,
	    Tcl_Object sourceObject, const char *targetName,
	    const char *targetNamespaceName)
}
declare 1 current {
    Tcl_Object Tcl_GetClassAsObject(Tcl_Class clazz)
}
declare 2 current {
    Tcl_Class Tcl_GetObjectAsClass(Tcl_Object object)
}
declare 3 current {
    Tcl_Command Tcl_GetObjectCommand(Tcl_Object object)
}
declare 4 current {
    Tcl_Object Tcl_GetObjectFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 5 current {
    Tcl_Namespace *Tcl_GetObjectNamespace(Tcl_Object object)
}
declare 6 current {
    Tcl_Class Tcl_MethodDeclarerClass(Tcl_Method method)
}
declare 7 current {
    Tcl_Object Tcl_MethodDeclarerObject(Tcl_Method method)
}
declare 8 current {
    int Tcl_MethodIsPublic(Tcl_Method method)
}
declare 9 current {
    int Tcl_MethodIsType(Tcl_Method method, const Tcl_MethodType *typePtr,
	    ClientData *clientDataPtr)
}
declare 10 current {
    Tcl_Obj *Tcl_MethodName(Tcl_Method method)
}
declare 11 current {
    Tcl_Method Tcl_NewInstanceMethod(Tcl_Interp *interp, Tcl_Object object,
	    Tcl_Obj *nameObj, int isPublic, const Tcl_MethodType *typePtr,
	    ClientData clientData)
}
declare 12 current {
    Tcl_Method Tcl_NewMethod(Tcl_Interp *interp, Tcl_Class cls,
	    Tcl_Obj *nameObj, int isPublic, const Tcl_MethodType *typePtr,
	    ClientData clientData)
}
declare 13 current {
    Tcl_Object Tcl_NewObjectInstance(Tcl_Interp *interp, Tcl_Class cls,
	    const char *nameStr, const char *nsNameStr, int objc,
	    Tcl_Obj *const *objv, int skip)
}
declare 14 current {
    int Tcl_ObjectDeleted(Tcl_Object object)
}
declare 15 current {
    int Tcl_ObjectContextIsFiltering(Tcl_ObjectContext context)
}
declare 16 current {
    Tcl_Method Tcl_ObjectContextMethod(Tcl_ObjectContext context)
}
declare 17 current {
    Tcl_Object Tcl_ObjectContextObject(Tcl_ObjectContext context)
}
declare 18 current {
    int Tcl_ObjectContextSkippedArgs(Tcl_ObjectContext context)
}
declare 19 current {
    ClientData Tcl_ClassGetMetadata(Tcl_Class clazz,
	    const Tcl_ObjectMetadataType *typePtr)
}
declare 20 current {
    void Tcl_ClassSetMetadata(Tcl_Class clazz,
	    const Tcl_ObjectMetadataType *typePtr, ClientData metadata)
}
declare 21 current {
    ClientData Tcl_ObjectGetMetadata(Tcl_Object object,
	    const Tcl_ObjectMetadataType *typePtr)
}
declare 22 current {
    void Tcl_ObjectSetMetadata(Tcl_Object object,
	    const Tcl_ObjectMetadataType *typePtr, ClientData metadata)
}
declare 23 current {
    int Tcl_ObjectContextInvokeNext(Tcl_Interp *interp,
	    Tcl_ObjectContext context, int objc, Tcl_Obj *const *objv,
	    int skip)
}
declare 24 current {
    Tcl_ObjectMapMethodNameProc Tcl_ObjectGetMethodNameMapper(
	    Tcl_Object object)
}
declare 25 current {
    void Tcl_ObjectSetMethodNameMapper(Tcl_Object object,
	    Tcl_ObjectMapMethodNameProc mapMethodNameProc)
}
declare 26 current {
    void Tcl_ClassSetConstructor(Tcl_Interp *interp, Tcl_Class clazz,
	    Tcl_Method method)
}
declare 27 current {
    void Tcl_ClassSetDestructor(Tcl_Interp *interp, Tcl_Class clazz,
	    Tcl_Method method)
}

# private API, exposed to support advanced OO systems that plug in on top
interface tclOOInt
declare 0 current {
    Tcl_Object TclOOGetDefineCmdContext(Tcl_Interp *interp)
}
declare 1 current {
    Tcl_Method TclOOMakeProcInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    const Tcl_MethodType *typePtr, ClientData clientData,
	    Proc **procPtrPtr)
}
declare 2 current {
    Tcl_Method TclOOMakeProcMethod(Tcl_Interp *interp, Class *clsPtr,
	    int flags, Tcl_Obj *nameObj, const char *namePtr,
	    Tcl_Obj *argsObj, Tcl_Obj *bodyObj, const Tcl_MethodType *typePtr,
	    ClientData clientData, Proc **procPtrPtr)
}
declare 3 current {
    Method *TclOONewProcInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    ProcedureMethod **pmPtrPtr)
}
declare 4 current {
    Method *TclOONewProcMethod(Tcl_Interp *interp, Class *clsPtr,
	    int flags, Tcl_Obj *nameObj, Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
	    ProcedureMethod **pmPtrPtr)
}
declare 5 current {
    int TclOOObjectCmdCore(Object *oPtr, Tcl_Interp *interp, int objc,
	    Tcl_Obj *const *objv, int publicOnly, Class *startCls)
}
declare 6 current {
    int TclOOIsReachable(Class *targetPtr, Class *startPtr)
}
declare 7 current {
    Method *TclOONewForwardMethod(Tcl_Interp *interp, Class *clsPtr,
	    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *prefixObj)
}
declare 8 current {
    Method *TclOONewForwardInstanceMethod(Tcl_Interp *interp, Object *oPtr,
	    int isPublic, Tcl_Obj *nameObj, Tcl_Obj *prefixObj)
}
declare 9 current {
    Tcl_Method TclOONewProcInstanceMethodEx(Tcl_Interp *interp,
	    Tcl_Object oPtr, TclOO_PreCallProc preCallPtr,
	    TclOO_PostCallProc postCallPtr, ProcErrorProc errProc,
	    ClientData clientData, Tcl_Obj *nameObj, Tcl_Obj *argsObj,
	    Tcl_Obj *bodyObj, int flags, void **internalTokenPtr)
}
declare 10 current {
    Tcl_Method TclOONewProcMethodEx(Tcl_Interp *interp, Tcl_Class clsPtr,
	    TclOO_PreCallProc preCallPtr, TclOO_PostCallProc postCallPtr,
	    ProcErrorProc errProc, ClientData clientData, Tcl_Obj *nameObj,
	    Tcl_Obj *argsObj, Tcl_Obj *bodyObj, int flags,
	    void **internalTokenPtr)
}
declare 11 current {
    int TclOOInvokeObject(Tcl_Interp *interp, Tcl_Object object,
	    Tcl_Class startCls, int publicPrivate, int objc,
	    Tcl_Obj *const *objv)
}
declare 12 current {
    void TclOOObjectSetFilters(Object *oPtr, int numFilters,
	    Tcl_Obj *const *filters)
}
declare 13 current {
    void TclOOClassSetFilters(Tcl_Interp *interp, Class *classPtr,
	    int numFilters, Tcl_Obj *const *filters)
}
declare 14 current {
    void TclOOObjectSetMixins(Object *oPtr, int numMixins,
	    Class *const *mixins)
}
declare 15 current {
    void TclOOClassSetMixins(Tcl_Interp *interp, Class *classPtr,
	    int numMixins, Class *const *mixins)
}
