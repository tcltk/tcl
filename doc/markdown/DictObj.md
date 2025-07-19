---
CommandName: Tcl_DictObj
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_NewObj
 - Tcl_DecrRefCount
 - Tcl_IncrRefCount
 - Tcl_InitObjHashTable
Keywords:
 - dict
 - dict value
 - dictionary
 - dictionary value
 - hash table
 - iteration
 - value
Copyright:
 - Copyright (c) 2003 Donal K. Fellows
---

# Name

Tcl_NewDictObj, Tcl_DictObjPut, Tcl_DictObjGet, Tcl_DictObjRemove, Tcl_DictObjSize, Tcl_DictObjFirst, Tcl_DictObjNext, Tcl_DictObjDone, Tcl_DictObjPutKeyList, Tcl_DictObjRemoveKeyList - manipulate Tcl values as dictionaries

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_Obj *]{.ret} [Tcl_NewDictObj]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl_DictObjGet]{.ccmd}[interp, dictPtr, keyPtr, valuePtrPtr]{.cargs}
[int]{.ret} [Tcl_DictObjPut]{.ccmd}[interp, dictPtr, keyPtr, valuePtr]{.cargs}
[int]{.ret} [Tcl_DictObjRemove]{.ccmd}[interp, dictPtr, keyPtr]{.cargs}
[int]{.ret} [Tcl_DictObjSize]{.ccmd}[interp, dictPtr, sizePtr]{.cargs}
[int]{.ret} [Tcl_DictObjFirst]{.ccmd}[interp, dictPtr, searchPtr, keyPtrPtr, valuePtrPtr, donePtr]{.cargs}
[Tcl_DictObjNext]{.ccmd}[searchPtr, keyPtrPtr, valuePtrPtr, donePtr]{.cargs}
[Tcl_DictObjDone]{.ccmd}[searchPtr]{.cargs}
[int]{.ret} [Tcl_DictObjPutKeyList]{.ccmd}[interp, dictPtr, keyc, keyv, valuePtr]{.cargs}
[int]{.ret} [Tcl_DictObjRemoveKeyList]{.ccmd}[interp, dictPtr, keyc, keyv]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in If an error occurs while converting a value to be a dictionary value, an error message is left in the interpreter's result value unless *interp* is NULL. .AP Tcl_Obj *dictPtr in/out Points to the dictionary value to be manipulated. If *dictPtr* does not already point to a dictionary value, an attempt will be made to convert it to one. .AP Tcl_Obj *keyPtr in Points to the key for the key/value pair being manipulated within the dictionary value. .AP Tcl_Obj **keyPtrPtr out Points to a variable that will have the key from a key/value pair placed within it.  May be NULL to indicate that the caller is not interested in the key. .AP Tcl_Obj *valuePtr in Points to the value for the key/value pair being manipulated within the dictionary value (or sub-value, in the case of **Tcl_DictObjPutKeyList**.) .AP Tcl_Obj **valuePtrPtr out Points to a variable that will have the value from a key/value pair placed within it.  For **Tcl_DictObjFirst** and **Tcl_DictObjNext**, this may be NULL to indicate that the caller is not interested in the value. .AP "Tcl_Size \&| int" *sizePtr out Points to a variable that will have the number of key/value pairs contained within the dictionary placed within it. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return NULL for dictionaries larger than INT_MAX (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl_DictSearch *searchPtr in/out Pointer to record to use to keep track of progress in enumerating all key/value pairs in a dictionary.  The contents of the record will be initialized by the call to **Tcl_DictObjFirst**.  If the enumerating is to be terminated before all values in the dictionary have been returned, the search record *must* be passed to **Tcl_DictObjDone** to enable the internal locks to be released. .AP int *donePtr out Points to a variable that will have a non-zero value written into it when the enumeration of the key/value pairs in a dictionary has completed, and a zero otherwise. .AP Tcl_Size keyc in Indicates the number of keys that will be supplied in the *keyv* array. .AP "Tcl_Obj *const" *keyv in Array of *keyc* pointers to values that **Tcl_DictObjPutKeyList** and **Tcl_DictObjRemoveKeyList** will use to locate the key/value pair to manipulate within the sub-dictionaries of the main dictionary value passed to them. 

# Description

Tcl dictionary values have an internal representation that supports efficient mapping from keys to values and which guarantees that the particular ordering of keys within the dictionary remains the same modulo any keys being deleted (which removes them from the order) or added (which adds them to the end of the order). If reinterpreted as a list, the values at the even-valued indices in the list will be the keys of the dictionary, and each will be followed (in the odd-valued index) by the value associated with that key.

The procedures described in this man page are used to create, modify, index, and iterate over dictionary values from C code.

**Tcl_NewDictObj** creates a new, empty dictionary value.  The string representation of the value will be invalid, and the reference count of the value will be zero.

**Tcl_DictObjGet** looks up the given key within the given dictionary and writes a pointer to the value associated with that key into the variable pointed to by *valuePtrPtr*, or a NULL if the key has no mapping within the dictionary.  The result of this procedure is **TCL_OK**, or **TCL_ERROR** if the *dictPtr* cannot be converted to a dictionary.

**Tcl_DictObjPut** updates the given dictionary so that the given key maps to the given value; any key may exist at most once in any particular dictionary.  The dictionary must not be shared, but the key and value may be.  This procedure may increase the reference count of both key and value if it proves necessary to store them.  Neither key nor value should be NULL.  The result of this procedure is **TCL_OK**, or **TCL_ERROR** if the *dictPtr* cannot be converted to a dictionary.

**Tcl_DictObjRemove** updates the given dictionary so that the given key has no mapping to any value.  The dictionary must not be shared, but the key may be.  The key actually stored in the dictionary will have its reference count decremented if it was present.  It is not an error if the key did not previously exist.  The result of this procedure is **TCL_OK**, or **TCL_ERROR** if the *dictPtr* cannot be converted to a dictionary.

**Tcl_DictObjSize** updates the given variable with the number of key/value pairs currently in the given dictionary. The result of this procedure is **TCL_OK**, or **TCL_ERROR** if the *dictPtr* cannot be converted to a dictionary or if *sizePtr* points to a variable of type **int** and the dict contains more than 2**31 key/value pairs.

**Tcl_DictObjFirst** commences an iteration across all the key/value pairs in the given dictionary, placing the key and value in the variables pointed to by the *keyPtrPtr* and *valuePtrPtr* arguments (which may be NULL to indicate that the caller is uninterested in they key or variable respectively.)  The next key/value pair in the dictionary may be retrieved with **Tcl_DictObjNext**.  Concurrent updates of the dictionary's internal representation will not modify the iteration processing unless the dictionary is unshared, when this will trigger premature termination of the iteration instead (which Tcl scripts cannot trigger via the **dict** command.)  The *searchPtr* argument points to a piece of context that is used to identify which particular iteration is being performed, and is initialized by the call to **Tcl_DictObjFirst**.  The *donePtr* argument points to a variable that is updated to be zero of there are further key/value pairs to be iterated over, or non-zero if the iteration is complete. The order of iteration is implementation-defined.  If the *dictPtr* argument cannot be converted to a dictionary, **Tcl_DictObjFirst** returns **TCL_ERROR** and the iteration is not commenced, and otherwise it returns **TCL_OK**.

When **Tcl_DictObjFirst** is called upon a dictionary, a lock is placed on the dictionary to enable that dictionary to be iterated over safely without regard for whether the dictionary is modified during the iteration. Because of this, once the iteration over a dictionary's keys has finished (whether because all values have been iterated over as indicated by the variable indicated by the *donePtr* argument being set to one, or because no further values are required) the **Tcl_DictObjDone** function must be called with the same *searchPtr* as was passed to **Tcl_DictObjFirst** so that the internal locks can be released. Once a particular *searchPtr* is passed to **Tcl_DictObjDone**, passing it to **Tcl_DictObjNext** (without first initializing it with **Tcl_DictObjFirst**) will result in no values being produced and the variable pointed to by *donePtr* being set to one. It is safe to call **Tcl_DictObjDone** multiple times on the same *searchPtr* for each call to **Tcl_DictObjFirst**.

The procedures **Tcl_DictObjPutKeyList** and **Tcl_DictObjRemoveKeyList** are the close analogues of **Tcl_DictObjPut** and **Tcl_DictObjRemove** respectively, except that instead of working with a single dictionary, they are designed to operate on a nested tree of dictionaries, with inner dictionaries stored as values inside outer dictionaries.  The *keyc* and *keyv* arguments specify a list of keys (with outermost keys first) that acts as a path to the key/value pair to be affected.  Note that there is no corresponding operation for reading a value for a path as this is easy to construct from repeated use of **Tcl_DictObjGet**. With **Tcl_DictObjPutKeyList**, nested dictionaries are created for non-terminal keys where they do not already exist. With **Tcl_DictObjRemoveKeyList**, all non-terminal keys must exist and have dictionaries as their values.

# Reference count management

**Tcl_NewDictObj** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_DictObjPut** does not modify the reference count of its *dictPtr* argument, but does require that the object be unshared.  If **Tcl_DictObjPut** returns **TCL_ERROR** it does not manipulate any reference counts; but if it returns **TCL_OK** then it definitely increments the reference count of *valuePtr* and may increment the reference count of *keyPtr*; the latter case happens exactly when the key did not previously exist in the dictionary.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to an object, it will be deleted.

**Tcl_DictObjGet** only reads from its *dictPtr* and *keyPtr* arguments, and does not manipulate their reference counts at all.  If the *valuePtrPtr* argument is not set to NULL (and the function doesn't return **TCL_ERROR**), it will be set to a value with a reference count of at least 1, with a reference owned by the dictionary.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to an object, it will be deleted.

**Tcl_DictObjRemove** does not modify the reference count of its *dictPtr* argument, but does require that the object be unshared. It does not manipulate the reference count of its *keyPtr* argument at all.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to an object, it will be deleted.

**Tcl_DictObjSize** does not modify the reference count of its *dictPtr* argument; it only reads.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the dictionary object, it will be deleted.

**Tcl_DictObjFirst** does not modify the reference count of its *dictPtr* argument; it only reads. The variables given by the *keyPtrPtr* and *valuePtrPtr* arguments (if not NULL) will be updated to contain references to the relevant values in the dictionary; their reference counts will be at least 1 (due to the dictionary holding a reference to them). It may also manipulate internal references; these are not exposed to user code, but require a matching **Tcl_DictObjDone** call.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the dictionary object, it will be deleted.

Similarly for **Tcl_DictObjNext**; the variables given by the *keyPtrPtr* and *valuePtrPtr* arguments (if not NULL) will be updated to contain references to the relevant values in the dictionary; their reference counts will be at least 1 (due to the dictionary holding a reference to them).

**Tcl_DictObjDone** does not manipulate (user-visible) reference counts.

**Tcl_DictObjPutKeyList** is similar to **Tcl_DictObjPut**; it does not modify the reference count of its *dictPtr* argument, but does require that the object be unshared. It may increment the reference count of any value passed in the *keyv* argument, and will increment the reference count of the *valuePtr* argument on success. It is recommended that values passed via *keyv* and *valuePtr* do not have zero reference counts.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to an object, it will be deleted.

**Tcl_DictObjRemoveKeyList** is similar to **Tcl_DictObjRemove**; it does not modify the reference count of its *dictPtr* argument, but does require that the object be unshared, and does not modify the reference counts of any of the values passed in the *keyv* argument.  Note however that this function may set the interpreter result; if that is the only place that is holding a reference to an object, it will be deleted.

# Example

Using the dictionary iteration interface to search determine if there is a key that maps to itself:

```
Tcl_DictSearch search;
Tcl_Obj *key, *value;
int done;

/*
 * Assume interp and objPtr are parameters.  This is the
 * idiomatic way to start an iteration over the dictionary; it
 * sets a lock on the internal representation that ensures that
 * there are no concurrent modification issues when normal
 * reference count management is also used.  The lock is
 * released automatically when the loop is finished, but must
 * be released manually when an exceptional exit from the loop
 * is performed. However it is safe to try to release the lock
 * even if we've finished iterating over the loop.
 */
if (Tcl_DictObjFirst(interp, objPtr, &search,
        &key, &value, &done) != TCL_OK) {
    return TCL_ERROR;
}
for (; !done ; Tcl_DictObjNext(&search, &key, &value, &done)) {
    /*
     * Note that strcmp() is not a good way of comparing
     * values and is just used here for demonstration
     * purposes.
     */
    if (!strcmp(Tcl_GetString(key), Tcl_GetString(value))) {
        break;
    }
}
Tcl_DictObjDone(&search);
Tcl_SetObjResult(interp, Tcl_NewBooleanObj(!done));
return TCL_OK;
```

