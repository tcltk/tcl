---
CommandName: Tcl_SetAssocData
ManualSection: 3
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - association
 - data
 - deletion procedure
 - interpreter
 - key
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl_GetAssocData, Tcl_SetAssocData, Tcl_DeleteAssocData - manage associations of string keys and user specified data with Tcl interpreters

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[void *]{.ret} [Tcl_GetAssocData]{.ccmd}[interp, key, delProcPtr]{.cargs}
[Tcl_SetAssocData]{.ccmd}[interp, key, delProc, clientData]{.cargs}
[Tcl_DeleteAssocData]{.ccmd}[interp, key]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which to execute the specified command. .AP "const char" *key in Key for association with which to store data or from which to delete or retrieve data.  Typically the module prefix for a package. .AP Tcl_InterpDeleteProc *delProc in Procedure to call when *interp* is deleted. .AP Tcl_InterpDeleteProc **delProcPtr in Pointer to location in which to store address of current deletion procedure for association.  Ignored if NULL. .AP void *clientData in Arbitrary one-word value associated with the given key in this interpreter.  This data is owned by the caller. 

# Description

These procedures allow extensions to associate their own data with a Tcl interpreter. An association consists of a string key, typically the name of the extension, and a one-word value, which is typically a pointer to a data structure holding data specific to the extension. Tcl makes no interpretation of either the key or the value for an association.

Storage management is facilitated by storing with each association a procedure to call when the interpreter is deleted. This procedure can dispose of the storage occupied by the client's data in any way it sees fit.

**Tcl_SetAssocData** creates an association between a string key and a user specified datum in the given interpreter. If there is already an association with the given *key*, **Tcl_SetAssocData** overwrites it with the new information. It is up to callers to organize their use of names to avoid conflicts, for example, by using package names as the keys. If the *deleteProc* argument is non-NULL it specifies the address of a procedure to invoke if the interpreter is deleted before the association is deleted.  *DeleteProc* should have arguments and result that match the type **Tcl_InterpDeleteProc**:

```
typedef void Tcl_InterpDeleteProc(
        void *clientData,
        Tcl_Interp *interp);
```

When *deleteProc* is invoked the *clientData* and *interp* arguments will be the same as the corresponding arguments passed to **Tcl_SetAssocData**. The deletion procedure will *not* be invoked if the association is deleted before the interpreter is deleted.

**Tcl_GetAssocData** returns the datum stored in the association with the specified key in the given interpreter, and if the *delProcPtr* field is non-**NULL**, the address indicated by it gets the address of the delete procedure stored with this association. If no association with the specified key exists in the given interpreter **Tcl_GetAssocData** returns **NULL**.

**Tcl_DeleteAssocData** deletes an association with a specified key in the given interpreter.  Then it calls the deletion procedure.

