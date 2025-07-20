---
CommandName: Tcl_Hash
ManualSection: 3
Version: unknown
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Dict(3)
Keywords:
 - hash table
 - key
 - lookup
 - search
 - value
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_InitHashTable, Tcl_InitCustomHashTable, Tcl_InitObjHashTable, Tcl_DeleteHashTable, Tcl_CreateHashEntry, Tcl_AttemptCreateHashEntry, Tcl_DeleteHashEntry, Tcl_FindHashEntry, Tcl_GetHashValue, Tcl_SetHashValue, Tcl_GetHashKey, Tcl_FirstHashEntry, Tcl_NextHashEntry, Tcl_HashStats - procedures to manage hash tables

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_InitHashTable]{.ccmd}[tablePtr, keyType]{.cargs}
[Tcl_InitCustomHashTable]{.ccmd}[tablePtr, keyType, typePtr]{.cargs}
[Tcl_InitObjHashTable]{.ccmd}[tablePtr]{.cargs}
[Tcl_DeleteHashTable]{.ccmd}[tablePtr]{.cargs}
[Tcl_HashEntry *]{.ret} [Tcl_CreateHashEntry]{.ccmd}[tablePtr, key, newPtr]{.cargs}
[Tcl_HashEntry *]{.ret} [Tcl_AttemptCreateHashEntry]{.ccmd}[tablePtr, key, newPtr]{.cargs}
[Tcl_DeleteHashEntry]{.ccmd}[entryPtr]{.cargs}
[Tcl_HashEntry *]{.ret} [Tcl_FindHashEntry]{.ccmd}[tablePtr, key]{.cargs}
[void *]{.ret} [Tcl_GetHashValue]{.ccmd}[entryPtr]{.cargs}
[Tcl_SetHashValue]{.ccmd}[entryPtr, value]{.cargs}
[void *]{.ret} [Tcl_GetHashKey]{.ccmd}[tablePtr, entryPtr]{.cargs}
[Tcl_HashEntry *]{.ret} [Tcl_FirstHashEntry]{.ccmd}[tablePtr, searchPtr]{.cargs}
[Tcl_HashEntry *]{.ret} [Tcl_NextHashEntry]{.ccmd}[searchPtr]{.cargs}
[char *]{.ret} [Tcl_HashStats]{.ccmd}[tablePtr]{.cargs}
:::

# Arguments

.AP Tcl_HashTable *tablePtr in Address of hash table structure (for all procedures but **Tcl_InitHashTable**, this must have been initialized by previous call to **Tcl_InitHashTable**). .AP int keyType in Kind of keys to use for new hash table.  Must be either **TCL_STRING_KEYS**, **TCL_ONE_WORD_KEYS**, **TCL_CUSTOM_TYPE_KEYS**, **TCL_CUSTOM_PTR_KEYS**, or an integer value greater than 1. .AP Tcl_HashKeyType *typePtr in Address of structure which defines the behavior of the hash table. .AP "const void" *key in Key to use for probe into table.  Exact form depends on *keyType* used to create table. .AP int *newPtr out The word at **newPtr* is set to 1 if a new entry was created and 0 if there was already an entry for *key*. .AP Tcl_HashEntry *entryPtr in Pointer to hash table entry. .AP void *value in New value to assign to hash table entry. .AP Tcl_HashSearch *searchPtr in Pointer to record to use to keep track of progress in enumerating all the entries in a hash table.

# Description

A hash table consists of zero or more entries, each consisting of a key and a value.  Given the key for an entry, the hashing routines can very quickly locate the entry, and hence its value. There may be at most one entry in a hash table with a particular key, but many entries may have the same value.  Keys can take one of four forms: strings, one-word values, integer arrays, or custom keys defined by a Tcl_HashKeyType structure (See section **THE TCL_HASHKEYTYPE STRUCTURE** below). All of the keys in a given table have the same form, which is specified when the table is initialized.

The value of a hash table entry can be anything that fits in the same space as a "char *" pointer.  Values for hash table entries are managed entirely by clients, not by the hash module itself.  Typically each entry's value is a pointer to a data structure managed by client code.

Hash tables grow gracefully as the number of entries increases, so that there are always less than three entries per hash bucket, on average. This allows for fast lookups regardless of the number of entries in a table.

The core provides three functions for the initialization of hash tables, Tcl_InitHashTable, Tcl_InitObjHashTable and Tcl_InitCustomHashTable.

**Tcl_InitHashTable** initializes a structure that describes a new hash table.  The space for the structure is provided by the caller, not by the hash module.  The value of *keyType* indicates what kinds of keys will be used for all entries in the table. All of the key types described later are allowed, with the exception of **TCL_CUSTOM_TYPE_KEYS** and **TCL_CUSTOM_PTR_KEYS**.

**Tcl_InitObjHashTable** is a wrapper around **Tcl_InitCustomHashTable** and initializes a hash table whose keys are Tcl_Obj *.

**Tcl_InitCustomHashTable** initializes a structure that describes a new hash table. The space for the structure is provided by the caller, not by the hash module.  The value of *keyType* indicates what kinds of keys will be used for all entries in the table. *KeyType* must have one of the following values:

**TCL_STRING_KEYS**
: Keys are null-terminated strings. They are passed to hashing routines using the address of the first character of the string.

**TCL_ONE_WORD_KEYS**
: Keys are single-word values;  they are passed to hashing routines and stored in hash table entries as "char *" values. The pointer value is the key;  it need not (and usually does not) actually point to a string.

**TCL_CUSTOM_TYPE_KEYS**
: Keys are of arbitrary type, and are stored in the entry. Hashing and comparison is determined by *typePtr*. The Tcl_HashKeyType structure is described in the section **THE TCL_HASHKEYTYPE STRUCTURE** below.

**TCL_CUSTOM_PTR_KEYS**
: Keys are pointers to an arbitrary type, and are stored in the entry. Hashing and comparison is determined by *typePtr*. The Tcl_HashKeyType structure is described in the section **THE TCL_HASHKEYTYPE STRUCTURE** below.

*other*
: If *keyType* is not one of the above, then it must be an integer value greater than 1. In this case the keys will be arrays of "int" values, where *keyType* gives the number of ints in each key. This allows structures to be used as keys. All keys must have the same size. Array keys are passed into hashing functions using the address of the first int in the array.


**Tcl_DeleteHashTable** deletes all of the entries in a hash table and frees up the memory associated with the table's bucket array and entries. It does not free the actual table structure (pointed to by *tablePtr*), since that memory is assumed to be managed by the client. **Tcl_DeleteHashTable** also does not free or otherwise manipulate the values of the hash table entries. If the entry values point to dynamically-allocated memory, then it is the client's responsibility to free these structures before deleting the table.

**Tcl_CreateHashEntry** locates the entry corresponding to a particular key, creating a new entry in the table if there was not already one with the given key. If an entry already existed with the given key then **newPtr* is set to zero. If a new entry was created, then **newPtr* is set to a non-zero value and the value of the new entry will be set to zero. **newPtr* is allowed to be NULL. The return value from **Tcl_CreateHashEntry** is a pointer to the entry, which may be used to retrieve and modify the entry's value or to delete the entry from the table.

**Tcl_AttemptCreateHashEntry** does the same as **Tcl_CreateHashEntry**, except in case of a memory overflow. **Tcl_AttemptCreateHashEntry** returns NULL in that case while **Tcl_CreateHashEntry** panics.

**Tcl_DeleteHashEntry** will remove an existing entry from a table. The memory associated with the entry itself will be freed, but the client is responsible for any cleanup associated with the entry's value, such as freeing a structure that it points to.

**Tcl_FindHashEntry** is similar to **Tcl_CreateHashEntry** except that it does not create a new entry if the key doesn't exist; instead, it returns NULL as result.

**Tcl_GetHashValue** and **Tcl_SetHashValue** are used to read and write an entry's value, respectively.

**Tcl_GetHashKey** returns the key for a given hash table entry, either as a pointer to a string, a one-word ("char *") key, or as a pointer to the first word of an array of integers, depending on the *keyType* used to create a hash table. In all cases **Tcl_GetHashKey** returns a result with type "char *". When the key is a string or array, the result of **Tcl_GetHashKey** points to information in the table entry;  this information will remain valid until the entry is deleted or its table is deleted.

**Tcl_FirstHashEntry** and **Tcl_NextHashEntry** may be used to scan all of the entries in a hash table. A structure of type "Tcl_HashSearch", provided by the client, is used to keep track of progress through the table. **Tcl_FirstHashEntry** initializes the search record and returns the first entry in the table (or NULL if the table is empty). Each subsequent call to **Tcl_NextHashEntry** returns the next entry in the table or NULL if the end of the table has been reached. A call to **Tcl_FirstHashEntry** followed by calls to **Tcl_NextHashEntry** will return each of the entries in the table exactly once, in an arbitrary order. It is inadvisable to modify the structure of the table, e.g. by creating or deleting entries, while the search is in progress, with the exception of deleting the entry returned by **Tcl_FirstHashEntry** or **Tcl_NextHashEntry**.

**Tcl_HashStats** returns a dynamically-allocated string with overall information about a hash table, such as the number of entries it contains, the number of buckets in its hash array, and the utilization of the buckets. It is the caller's responsibility to free the result string by passing it to **Tcl_Free**.

The header file **tcl.h** defines the actual data structures used to implement hash tables. This is necessary so that clients can allocate Tcl_HashTable structures and so that macros can be used to read and write the values of entries. However, users of the hashing routines should never refer directly to any of the fields of any of the hash-related data structures; use the procedures and macros defined here.

# The tcl_hashkeytype structure

Extension writers can define new hash key types by defining four procedures, initializing a **Tcl_HashKeyType** structure to describe the type, and calling **Tcl_InitCustomHashTable**. The **Tcl_HashKeyType** structure is defined as follows:

```
typedef struct {
    int version;
    int flags;
    Tcl_HashKeyProc *hashKeyProc;
    Tcl_CompareHashKeysProc *compareKeysProc;
    Tcl_AllocHashEntryProc *allocEntryProc;
    Tcl_FreeHashEntryProc *freeEntryProc;
} Tcl_HashKeyType;
```

The *version* member is the version of the table. If this structure is extended in future then the version can be used to distinguish between different structures. It should be set to **TCL_HASH_KEY_TYPE_VERSION**.

The *flags* member is 0 or one or more of the following values OR'ed together:

**TCL_HASH_KEY_RANDOMIZE_HASH**
: There are some things, pointers for example which do not hash well because they do not use the lower bits. If this flag is set then the hash table will attempt to rectify this by randomizing the bits and then using the upper N bits as the index into the table.

**TCL_HASH_KEY_SYSTEM_HASH**
: This flag forces Tcl to use the memory allocation procedures provided by the operating system when allocating and freeing memory used to store the hash table data structures, and not any of Tcl's own customized memory allocation routines. This is important if the hash table is to be used in the implementation of a custom set of allocation routines, or something that a custom set of allocation routines might depend on, in order to avoid any circular dependency.


The *hashKeyProc* member contains the address of a function called to calculate a hash value for the key.

```
typedef size_t Tcl_HashKeyProc(
        Tcl_HashTable *tablePtr,
        void *keyPtr);
```

If this is NULL then *keyPtr* is used and **TCL_HASH_KEY_RANDOMIZE_HASH** is assumed.

The *compareKeysProc* member contains the address of a function called to compare two keys.

```
typedef int Tcl_CompareHashKeysProc(
        void *keyPtr,
        Tcl_HashEntry *hPtr);
```

If this is NULL then the *keyPtr* pointers are compared. If the keys do not match then the function returns 0, otherwise it returns 1.

The *allocEntryProc* member contains the address of a function called to allocate space for an entry and initialize the key and clientData.

```
typedef Tcl_HashEntry *Tcl_AllocHashEntryProc(
        Tcl_HashTable *tablePtr,
        void *keyPtr);
```

If this is NULL then **Tcl_Alloc** is used to allocate enough space for a Tcl_HashEntry, the key pointer is assigned to key.oneWordValue and the clientData is set to NULL. String keys and array keys use this function to allocate enough space for the entry and the key in one block, rather than doing it in two blocks. This saves space for a pointer to the key from the entry and another memory allocation. Tcl_Obj* keys use this function to allocate enough space for an entry and increment the reference count on the value.

The *freeEntryProc* member contains the address of a function called to free space for an entry.

```
typedef void Tcl_FreeHashEntryProc(
        Tcl_HashEntry *hPtr);
```

If this is NULL then **Tcl_Free** is used to free the space for the entry. Tcl_Obj* keys use this function to decrement the reference count on the value.

# Reference count management

When a hash table is created with **Tcl_InitCustomHashTable**, the **Tcl_CreateHashEntry** function will increment the reference count of its *key* argument when it creates a key (but not if there is an existing matching key). The reference count of the key will be decremented when the corresponding hash entry is deleted, whether with **Tcl_DeleteHashEntry** or with **Tcl_DeleteHashTable**. The **Tcl_GetHashKey** function will return the key without further modifying its reference count.

Custom hash tables that use a Tcl_Obj* as key will generally need to do something similar in their *allocEntryProc*.

