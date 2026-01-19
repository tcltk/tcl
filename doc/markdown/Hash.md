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

Tcl\_InitHashTable, Tcl\_InitCustomHashTable, Tcl\_InitObjHashTable, Tcl\_DeleteHashTable, Tcl\_CreateHashEntry, Tcl\_AttemptCreateHashEntry, Tcl\_DeleteHashEntry, Tcl\_FindHashEntry, Tcl\_GetHashValue, Tcl\_SetHashValue, Tcl\_GetHashKey, Tcl\_FirstHashEntry, Tcl\_NextHashEntry, Tcl\_HashStats - procedures to manage hash tables

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_InitHashTable]{.ccmd}[tablePtr, keyType]{.cargs}
[Tcl\_InitCustomHashTable]{.ccmd}[tablePtr, keyType, typePtr]{.cargs}
[Tcl\_InitObjHashTable]{.ccmd}[tablePtr]{.cargs}
[Tcl\_DeleteHashTable]{.ccmd}[tablePtr]{.cargs}
[Tcl\_HashEntry \*]{.ret} [Tcl\_CreateHashEntry]{.ccmd}[tablePtr, key, newPtr]{.cargs}
[Tcl\_HashEntry \*]{.ret} [Tcl\_AttemptCreateHashEntry]{.ccmd}[tablePtr, key, newPtr]{.cargs}
[Tcl\_DeleteHashEntry]{.ccmd}[entryPtr]{.cargs}
[Tcl\_HashEntry \*]{.ret} [Tcl\_FindHashEntry]{.ccmd}[tablePtr, key]{.cargs}
[void \*]{.ret} [Tcl\_GetHashValue]{.ccmd}[entryPtr]{.cargs}
[Tcl\_SetHashValue]{.ccmd}[entryPtr, value]{.cargs}
[void \*]{.ret} [Tcl\_GetHashKey]{.ccmd}[tablePtr, entryPtr]{.cargs}
[Tcl\_HashEntry \*]{.ret} [Tcl\_FirstHashEntry]{.ccmd}[tablePtr, searchPtr]{.cargs}
[Tcl\_HashEntry \*]{.ret} [Tcl\_NextHashEntry]{.ccmd}[searchPtr]{.cargs}
[char \*]{.ret} [Tcl\_HashStats]{.ccmd}[tablePtr]{.cargs}
:::

# Arguments

.AP Tcl\_HashTable \*tablePtr in Address of hash table structure (for all procedures but **Tcl\_InitHashTable**, this must have been initialized by previous call to **Tcl\_InitHashTable**). .AP int keyType in Kind of keys to use for new hash table.  Must be either **TCL\_STRING\_KEYS**, **TCL\_ONE\_WORD\_KEYS**, **TCL\_CUSTOM\_TYPE\_KEYS**, **TCL\_CUSTOM\_PTR\_KEYS**, or an integer value greater than 1. .AP Tcl\_HashKeyType \*typePtr in Address of structure which defines the behavior of the hash table. .AP "const void" \*key in Key to use for probe into table.  Exact form depends on *keyType* used to create table. .AP int \*newPtr out The word at *\*newPtr* is set to 1 if a new entry was created and 0 if there was already an entry for *key*. .AP Tcl\_HashEntry \*entryPtr in Pointer to hash table entry. .AP void \*value in New value to assign to hash table entry. .AP Tcl\_HashSearch \*searchPtr in Pointer to record to use to keep track of progress in enumerating all the entries in a hash table.

# Description

A hash table consists of zero or more entries, each consisting of a key and a value.  Given the key for an entry, the hashing routines can very quickly locate the entry, and hence its value. There may be at most one entry in a hash table with a particular key, but many entries may have the same value.  Keys can take one of four forms: strings, one-word values, integer arrays, or custom keys defined by a Tcl\_HashKeyType structure (See section **THE TCL\_HASHKEYTYPE STRUCTURE** below). All of the keys in a given table have the same form, which is specified when the table is initialized.

The value of a hash table entry can be anything that fits in the same space as a "char \*" pointer.  Values for hash table entries are managed entirely by clients, not by the hash module itself.  Typically each entry's value is a pointer to a data structure managed by client code.

Hash tables grow gracefully as the number of entries increases, so that there are always less than three entries per hash bucket, on average. This allows for fast lookups regardless of the number of entries in a table.

The core provides three functions for the initialization of hash tables, Tcl\_InitHashTable, Tcl\_InitObjHashTable and Tcl\_InitCustomHashTable.

**Tcl\_InitHashTable** initializes a structure that describes a new hash table.  The space for the structure is provided by the caller, not by the hash module.  The value of *keyType* indicates what kinds of keys will be used for all entries in the table. All of the key types described later are allowed, with the exception of **TCL\_CUSTOM\_TYPE\_KEYS** and **TCL\_CUSTOM\_PTR\_KEYS**.

**Tcl\_InitObjHashTable** is a wrapper around **Tcl\_InitCustomHashTable** and initializes a hash table whose keys are Tcl\_Obj \*.

**Tcl\_InitCustomHashTable** initializes a structure that describes a new hash table. The space for the structure is provided by the caller, not by the hash module.  The value of *keyType* indicates what kinds of keys will be used for all entries in the table. *KeyType* must have one of the following values:

**TCL\_STRING\_KEYS**
: Keys are null-terminated strings. They are passed to hashing routines using the address of the first character of the string.

**TCL\_ONE\_WORD\_KEYS**
: Keys are single-word values;  they are passed to hashing routines and stored in hash table entries as "char \*" values. The pointer value is the key;  it need not (and usually does not) actually point to a string.

**TCL\_CUSTOM\_TYPE\_KEYS**
: Keys are of arbitrary type, and are stored in the entry. Hashing and comparison is determined by *typePtr*. The Tcl\_HashKeyType structure is described in the section **THE TCL\_HASHKEYTYPE STRUCTURE** below.

**TCL\_CUSTOM\_PTR\_KEYS**
: Keys are pointers to an arbitrary type, and are stored in the entry. Hashing and comparison is determined by *typePtr*. The Tcl\_HashKeyType structure is described in the section **THE TCL\_HASHKEYTYPE STRUCTURE** below.

*other*
: If *keyType* is not one of the above, then it must be an integer value greater than 1. In this case the keys will be arrays of "int" values, where *keyType* gives the number of ints in each key. This allows structures to be used as keys. All keys must have the same size. Array keys are passed into hashing functions using the address of the first int in the array.


**Tcl\_DeleteHashTable** deletes all of the entries in a hash table and frees up the memory associated with the table's bucket array and entries. It does not free the actual table structure (pointed to by *tablePtr*), since that memory is assumed to be managed by the client. **Tcl\_DeleteHashTable** also does not free or otherwise manipulate the values of the hash table entries. If the entry values point to dynamically-allocated memory, then it is the client's responsibility to free these structures before deleting the table.

**Tcl\_CreateHashEntry** locates the entry corresponding to a particular key, creating a new entry in the table if there was not already one with the given key. If an entry already existed with the given key then *\*newPtr* is set to zero. If a new entry was created, then *\*newPtr* is set to a non-zero value and the value of the new entry will be set to zero. *\*newPtr* is allowed to be NULL. The return value from **Tcl\_CreateHashEntry** is a pointer to the entry, which may be used to retrieve and modify the entry's value or to delete the entry from the table.

**Tcl\_AttemptCreateHashEntry** does the same as **Tcl\_CreateHashEntry**, except in case of a memory overflow. **Tcl\_AttemptCreateHashEntry** returns NULL in that case while **Tcl\_CreateHashEntry** panics.

**Tcl\_DeleteHashEntry** will remove an existing entry from a table. The memory associated with the entry itself will be freed, but the client is responsible for any cleanup associated with the entry's value, such as freeing a structure that it points to.

**Tcl\_FindHashEntry** is similar to **Tcl\_CreateHashEntry** except that it does not create a new entry if the key doesn't exist; instead, it returns NULL as result.

**Tcl\_GetHashValue** and **Tcl\_SetHashValue** are used to read and write an entry's value, respectively.

**Tcl\_GetHashKey** returns the key for a given hash table entry, either as a pointer to a string, a one-word ("char \*") key, or as a pointer to the first word of an array of integers, depending on the *keyType* used to create a hash table. In all cases **Tcl\_GetHashKey** returns a result with type "char \*". When the key is a string or array, the result of **Tcl\_GetHashKey** points to information in the table entry;  this information will remain valid until the entry is deleted or its table is deleted.

**Tcl\_FirstHashEntry** and **Tcl\_NextHashEntry** may be used to scan all of the entries in a hash table. A structure of type "Tcl\_HashSearch", provided by the client, is used to keep track of progress through the table. **Tcl\_FirstHashEntry** initializes the search record and returns the first entry in the table (or NULL if the table is empty). Each subsequent call to **Tcl\_NextHashEntry** returns the next entry in the table or NULL if the end of the table has been reached. A call to **Tcl\_FirstHashEntry** followed by calls to **Tcl\_NextHashEntry** will return each of the entries in the table exactly once, in an arbitrary order. It is inadvisable to modify the structure of the table, e.g. by creating or deleting entries, while the search is in progress, with the exception of deleting the entry returned by **Tcl\_FirstHashEntry** or **Tcl\_NextHashEntry**.

**Tcl\_HashStats** returns a dynamically-allocated string with overall information about a hash table, such as the number of entries it contains, the number of buckets in its hash array, and the utilization of the buckets. It is the caller's responsibility to free the result string by passing it to **Tcl\_Free**.

The header file **tcl.h** defines the actual data structures used to implement hash tables. This is necessary so that clients can allocate Tcl\_HashTable structures and so that macros can be used to read and write the values of entries. However, users of the hashing routines should never refer directly to any of the fields of any of the hash-related data structures; use the procedures and macros defined here.

# The tcl\_hashkeytype structure

Extension writers can define new hash key types by defining four procedures, initializing a **Tcl\_HashKeyType** structure to describe the type, and calling **Tcl\_InitCustomHashTable**. The **Tcl\_HashKeyType** structure is defined as follows:

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

The *version* member is the version of the table. If this structure is extended in future then the version can be used to distinguish between different structures. It should be set to **TCL\_HASH\_KEY\_TYPE\_VERSION**.

The *flags* member is 0 or one or more of the following values OR'ed together:

**TCL\_HASH\_KEY\_RANDOMIZE\_HASH**
: There are some things, pointers for example which do not hash well because they do not use the lower bits. If this flag is set then the hash table will attempt to rectify this by randomizing the bits and then using the upper N bits as the index into the table.

**TCL\_HASH\_KEY\_SYSTEM\_HASH**
: This flag forces Tcl to use the memory allocation procedures provided by the operating system when allocating and freeing memory used to store the hash table data structures, and not any of Tcl's own customized memory allocation routines. This is important if the hash table is to be used in the implementation of a custom set of allocation routines, or something that a custom set of allocation routines might depend on, in order to avoid any circular dependency.


The *hashKeyProc* member contains the address of a function called to calculate a hash value for the key.

```
typedef size_t Tcl_HashKeyProc(
        Tcl_HashTable *tablePtr,
        void *keyPtr);
```

If this is NULL then *keyPtr* is used and **TCL\_HASH\_KEY\_RANDOMIZE\_HASH** is assumed.

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

If this is NULL then **Tcl\_Alloc** is used to allocate enough space for a Tcl\_HashEntry, the key pointer is assigned to key.oneWordValue and the clientData is set to NULL. String keys and array keys use this function to allocate enough space for the entry and the key in one block, rather than doing it in two blocks. This saves space for a pointer to the key from the entry and another memory allocation. Tcl\_Obj\* keys use this function to allocate enough space for an entry and increment the reference count on the value.

The *freeEntryProc* member contains the address of a function called to free space for an entry.

```
typedef void Tcl_FreeHashEntryProc(
        Tcl_HashEntry *hPtr);
```

If this is NULL then **Tcl\_Free** is used to free the space for the entry. Tcl\_Obj\* keys use this function to decrement the reference count on the value.

# Reference count management

When a hash table is created with **Tcl\_InitCustomHashTable**, the **Tcl\_CreateHashEntry** function will increment the reference count of its *key* argument when it creates a key (but not if there is an existing matching key). The reference count of the key will be decremented when the corresponding hash entry is deleted, whether with **Tcl\_DeleteHashEntry** or with **Tcl\_DeleteHashTable**. The **Tcl\_GetHashKey** function will return the key without further modifying its reference count.

Custom hash tables that use a Tcl\_Obj\* as key will generally need to do something similar in their *allocEntryProc*.

