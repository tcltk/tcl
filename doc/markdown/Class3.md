---
CommandName: Tcl_Class
ManualSection: 3
Version: 0.1
TclPart: TclOO
TclDescription: TclOO Library Functions
Links:
 - Method(3)
 - oo::class(n)
 - oo::copy(n)
 - oo::define(n)
 - oo::object(n)
Keywords:
 - class
 - constructor
 - object
Copyright:
 - Copyright (c) 2007 Donal K. Fellows
---

# Name

Tcl_ClassGetMetadata, Tcl_ClassSetMetadata, Tcl_CopyObjectInstance, Tcl_GetClassAsObject, Tcl_GetObjectAsClass, Tcl_GetObjectCommand, Tcl_GetObjectFromObj, Tcl_GetObjectName, Tcl_GetObjectNamespace, Tcl_NewObjectInstance, Tcl_ObjectDeleted, Tcl_ObjectGetMetadata, Tcl_ObjectGetMethodNameMapper, Tcl_ObjectSetMetadata, Tcl_ObjectSetMethodNameMapper - manipulate objects and classes

# Synopsis

::: {.synopsis} :::
**#include <tclOO.h>**
[Tcl_Object]{.ret} [Tcl_GetObjectFromObj]{.ccmd}[interp, objPtr]{.cargs}
[Tcl_Object]{.ret} [Tcl_GetClassAsObject]{.ccmd}[class]{.cargs}
[Tcl_Class]{.ret} [Tcl_GetObjectAsClass]{.ccmd}[object]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_GetObjectName]{.ccmd}[interp, object]{.cargs}
[Tcl_Command]{.ret} [Tcl_GetObjectCommand]{.ccmd}[object]{.cargs}
[Tcl_Namespace *]{.ret} [Tcl_GetObjectNamespace]{.ccmd}[object]{.cargs}
[Tcl_Object]{.ret} [Tcl_NewObjectInstance]{.ccmd}[interp, class, name, nsName, objc, objv, skip]{.cargs}
[Tcl_Object]{.ret} [Tcl_CopyObjectInstance]{.ccmd}[interp, object, name, nsName]{.cargs}
[int]{.ret} [Tcl_ObjectDeleted]{.ccmd}[object]{.cargs}
[void *]{.ret} [Tcl_ObjectGetMetadata]{.ccmd}[object, metaTypePtr]{.cargs}
[Tcl_ObjectSetMetadata]{.ccmd}[object, metaTypePtr, metadata]{.cargs}
[void *]{.ret} [Tcl_ClassGetMetadata]{.ccmd}[class, metaTypePtr]{.cargs}
[Tcl_ClassSetMetadata]{.ccmd}[class, metaTypePtr, metadata]{.cargs}
[Tcl_ObjectMapMethodNameProc]{.ret} [Tcl_ObjectGetMethodNameMapper]{.ccmd}[object]{.cargs}
[Tcl_ObjectSetMethodNameMapper]{.ccmd}[object=, +methodNameMapper]{.cargs}
[Tcl_Class]{.ret} [Tcl_GetClassOfObject]{.ccmd}[object]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_GetObjectClassName]{.ccmd}[interp=, +object]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in/out Interpreter providing the context for looking up or creating an object, and into whose result error messages will be written on failure. .AP Tcl_Obj *objPtr in The name of the object to look up. .AP Tcl_Object object in Reference to the object to operate upon. .AP Tcl_Class class in Reference to the class to operate upon. .AP "const char" *name in The name of the object to create, or NULL if a new unused name is to be automatically selected. .AP "const char" *nsName in The name of the namespace to create for the object's private use, or NULL if a new unused name is to be automatically selected. The namespace must not already exist. .AP Tcl_Size objc in The number of elements in the *objv* array. .AP "Tcl_Obj *const" *objv in The arguments to the command to create the instance of the class. .AP Tcl_Size skip in The number of arguments at the start of the argument array, *objv*, that are not arguments to any constructors. This allows the generation of correct error messages even when complicated calling patterns are used (e.g., via the **next** command). .AP Tcl_ObjectMetadataType *metaTypePtr in The type of *metadata* being set with **Tcl_ClassSetMetadata** or retrieved with **Tcl_ClassGetMetadata**. .AP void *metadata in An item of metadata to attach to the class, or NULL to remove the metadata associated with a particular *metaTypePtr*. .AP "Tcl_ObjectMapMethodNameProc" "methodNameMapper" in A pointer to a function to call to adjust the mapping of objects and method names to implementations, or NULL when no such mapping is required.

# Description

Objects are typed entities that have a set of operations ("methods") associated with them. Classes are objects that can manufacture objects. Each class can be viewed as an object itself; the object view can be retrieved using **Tcl_GetClassAsObject** which always returns the object when applied to a non-destroyed class, and an object can be viewed as a class with the aid of the **Tcl_GetObjectAsClass** (which either returns the class, or NULL if the object is not a class). An object may be looked up using the **Tcl_GetObjectFromObj** function, which either returns an object or NULL (with an error message in the interpreter result) if the object cannot be found. The correct way to look up a class by name is to look up the object with that name, and then to use **Tcl_GetObjectAsClass**.

Every object has its own command and namespace associated with it. The command may be retrieved using the **Tcl_GetObjectCommand** function, the name of the object (and hence the name of the command) with **Tcl_GetObjectName**, and the namespace may be retrieved using the **Tcl_GetObjectNamespace** function. Note that the Tcl_Obj reference returned by **Tcl_GetObjectName** is a shared reference. You can also get whether the object has been marked for deletion with **Tcl_ObjectDeleted** (it returns true if deletion of the object has begun); this can be useful during the processing of methods.

::: {.info version="TIP605"}
The class of an object can be retrieved with **Tcl_GetClassOfObject**, and the name of the class of an object with **Tcl_GetObjectClassName**; note that these two *may* return NULL during deletion of an object (this is transient, and only occurs when the object is a long way through being deleted).
:::

Instances of classes are created using **Tcl_NewObjectInstance**, which creates an object from any class (and which is internally called by both the **create** and **new** methods of the **oo::class** class). It takes parameters that optionally give the name of the object and namespace to create, and which describe the arguments to pass to the class's constructor (if any). The result of the function will be either a reference to the newly created object, or NULL if the creation failed (when an error message will be left in the interpreter result). In addition, objects may be copied by using **Tcl_CopyObjectInstance** which creates a copy of an object without running any constructors.

Note that the lifetime management of objects is handled internally within TclOO, and does not use **Tcl_Preserve**. *It is not safe to put a Tcl_Object handle in a C structure with a lifespan different to the object;* you should use the object's command name (as retrieved with **Tcl_GetObjectName**) instead. It is safe to use a Tcl_Object handle for the lifespan of a call of a method on that object; handles do not become invalid while there is an outstanding call on their object (even if the only operation guaranteed to be safe on them is **Tcl_ObjectDeleted**; the other operations are only guaranteed to work on non-deleted objects).

# Object and class metadata

Every object and every class may have arbitrary amounts of metadata attached to it, which the object or class attaches no meaning to beyond what is described in a Tcl_ObjectMetadataType structure instance. Metadata to be attached is described by the type of the metadata (given in the *metaTypePtr* argument) and an arbitrary pointer (the *metadata* argument) that are given to **Tcl_ObjectSetMetadata** and **Tcl_ClassSetMetadata**, and a particular piece of metadata can be retrieved given its type using **Tcl_ObjectGetMetadata** and **Tcl_ClassGetMetadata**. If the *metadata* parameter to either **Tcl_ObjectSetMetadata** or **Tcl_ClassSetMetadata** is NULL, the metadata is removed if it was attached, and the results of **Tcl_ObjectGetMetadata** and **Tcl_ClassGetMetadata** are NULL if the given type of metadata was not attached. It is not an error to request or remove a piece of metadata that was not attached.

## Tcl_objectmetadatatype structure

The contents of the Tcl_ObjectMetadataType structure are as follows:

```
typedef const struct {
    int version;
    const char *name;
    Tcl_ObjectMetadataDeleteProc *deleteProc;
    Tcl_CloneProc *cloneProc;
} Tcl_ObjectMetadataType;
```

The *version* field allows for future expansion of the structure, and should always be declared equal to TCL_OO_METADATA_VERSION_CURRENT. The *name* field provides a human-readable name for the type, and is reserved for debugging.

The *deleteProc* field gives a function of type Tcl_ObjectMetadataDeleteProc that is used to delete a particular piece of metadata, and is called when the attached metadata is replaced or removed; the field must not be NULL.

The *cloneProc* field gives a function that is used to copy a piece of metadata (used when a copy of an object is created using **Tcl_CopyObjectInstance**); if NULL, the metadata will be just directly copied.

## Tcl_objectmetadatadeleteproc function signature

Functions matching this signature are used to delete metadata associated with a class or object.

```
typedef void Tcl_ObjectMetadataDeleteProc(
        void *metadata);
```

The *metadata* argument gives the address of the metadata to be deleted.

## Tcl_cloneproc function signature

Functions matching this signature are used to create copies of metadata associated with a class or object.

```
typedef int Tcl_CloneProc(
        Tcl_Interp *interp,
        void *srcMetadata,
        void **dstMetadataPtr);
```

The *interp* argument gives a place to write an error message when the attempt to clone the object is to fail, in which case the clone procedure must also return TCL_ERROR; it should return TCL_OK otherwise. The *srcMetadata* argument gives the address of the metadata to be cloned, and the cloned metadata should be written into the variable pointed to by *dstMetadataPtr*; a NULL should be written if the metadata is to not be cloned but the overall object copy operation is still to succeed.

# Object method name mapping

It is possible to control, on a per-object basis, what methods are invoked when a particular method is invoked. Normally this is done by looking up the method name in the object and then in the class hierarchy, but fine control of exactly what the value used to perform the look up is afforded through the ability to set a method name mapper callback via **Tcl_ObjectSetMethodNameMapper** (and its introspection counterpart, **Tcl_ObjectGetMethodNameMapper**, which returns the current mapper). The current mapper (if any) is invoked immediately before looking up what chain of method implementations is to be used.

## Tcl_objectmapmethodnameproc function signature

The *Tcl_ObjectMapMethodNameProc* callback is defined as follows:

```
typedef int Tcl_ObjectMapMethodNameProc(
        Tcl_Interp *interp,
        Tcl_Object object,
        Tcl_Class *startClsPtr,
        Tcl_Obj *methodNameObj);
```

If the result is TCL_OK, the remapping is assumed to have been done. If the result is TCL_ERROR, an error message will have been left in *interp* and the method call will fail. If the result is TCL_BREAK, the standard method name lookup rules will be used; the behavior of other result codes is currently undefined. The *object* parameter says which object is being processed. The *startClsPtr* parameter points to a variable that contains the first class to provide a definition in the method chain to process, or NULL if the whole chain is to be processed (the argument itself is never NULL); this variable may be updated by the callback. The *methodNameObj* parameter gives an unshared object containing the name of the method being invoked, as provided by the user; this object may be updated by the callback.

# Reference count management

The *objPtr* argument to **Tcl_GetObjectFromObj** will not have its reference count manipulated, but this function may modify the interpreter result (to report any error) so interpreter results should not be fed into this without an additional reference being used.

The result of **Tcl_GetObjectName** is a value that is owned by the object that is regenerated when this function is first called after the object is renamed.  If the value is to be retained at all, the caller should increment the reference count.

The first *objc* values in the *objv* argument to **Tcl_NewObjectInstance** are the arguments to pass to the constructor. They must have a reference count of at least 1, and may have their reference counts changed during the running of the constructor. Constructors may modify the interpreter result, which consequently means that interpreter results should not be used as arguments without an additional reference being taken.

The *methodNameObj* argument to a Tcl_ObjectMapMethodNameProc implementation will be a value with a reference count of at least 1 where at least one reference is not held by the interpreter result. It is expected that method name mappers will only read their *methodNameObj* arguments.

