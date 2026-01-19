---
CommandName: Tcl_Namespace
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateCommand(3)
 - Tcl_ListObjAppendList(3)
 - Tcl_SetVar(3)
Keywords:
 - namespace
 - command
Copyright:
 - Copyright (c) 2003 Donal K. Fellows
---

# Name

Tcl\_AppendExportList, Tcl\_CreateNamespace, Tcl\_DeleteNamespace, Tcl\_Export, Tcl\_FindCommand, Tcl\_FindNamespace, Tcl\_ForgetImport, Tcl\_GetCurrentNamespace, Tcl\_GetGlobalNamespace, Tcl\_GetNamespaceUnknownHandler, Tcl\_Import, Tcl\_SetNamespaceUnknownHandler - manipulate namespaces

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Namespace \*]{.ret} [Tcl\_CreateNamespace]{.ccmd}[interp, name, clientData, deleteProc]{.cargs}
[Tcl\_DeleteNamespace]{.ccmd}[nsPtr]{.cargs}
[int]{.ret} [Tcl\_AppendExportList]{.ccmd}[interp, nsPtr, objPtr]{.cargs}
[int]{.ret} [Tcl\_Export]{.ccmd}[interp, nsPtr, pattern, resetListFirst]{.cargs}
[int]{.ret} [Tcl\_Import]{.ccmd}[interp, nsPtr, pattern, allowOverwrite]{.cargs}
[int]{.ret} [Tcl\_ForgetImport]{.ccmd}[interp, nsPtr, pattern]{.cargs}
[Tcl\_Namespace \*]{.ret} [Tcl\_GetCurrentNamespace]{.ccmd}[interp]{.cargs}
[Tcl\_Namespace \*]{.ret} [Tcl\_GetGlobalNamespace]{.ccmd}[interp]{.cargs}
[Tcl\_Namespace \*]{.ret} [Tcl\_FindNamespace]{.ccmd}[interp, name, contextNsPtr, flags]{.cargs}
[Tcl\_Command]{.ret} [Tcl\_FindCommand]{.ccmd}[interp, name, contextNsPtr, flags]{.cargs}
[Tcl\_Obj \*]{.ret} [Tcl\_GetNamespaceUnknownHandler]{.ccmd}[interp, nsPtr]{.cargs}
[int]{.ret} [Tcl\_SetNamespaceUnknownHandler]{.ccmd}[interp, nsPtr, handlerPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in/out The interpreter in which the namespace exists and where name lookups are performed. Also where error result messages are written. .AP "const char" \*name in The name of the namespace or command to be created or accessed. .AP void \*clientData in A context pointer by the creator of the namespace.  Not interpreted by Tcl at all. .AP Tcl\_NamespaceDeleteProc \*deleteProc in A pointer to function to call when the namespace is deleted, or NULL if no such callback is to be performed. .AP Tcl\_Namespace \*nsPtr in The namespace to be manipulated, or NULL (for other than **Tcl\_DeleteNamespace**) to manipulate the current namespace. .AP Tcl\_Obj \*objPtr out A reference to an unshared value to which the function output will be written. .AP "const char" \*pattern in The glob-style pattern (see **Tcl\_StringMatch**) that describes the commands to be imported or exported. .AP int resetListFirst in Whether the list of export patterns should be reset before adding the current pattern to it. .AP int allowOverwrite in Whether new commands created by this import action can overwrite existing commands. .AP Tcl\_Namespace \*contextNsPtr in The location in the namespace hierarchy where the search for a namespace or command should be conducted relative to when the search term is not rooted at the global namespace.  NULL indicates the current namespace. .AP int flags in OR-ed combination of bits controlling how the search is to be performed.  The following flags are supported: **TCL\_GLOBAL\_ONLY** (indicates that the search is always to be conducted relative to the global namespace), **TCL\_NAMESPACE\_ONLY** (just for **Tcl\_FindCommand**; indicates that the search is always to be conducted relative to the context namespace), and **TCL\_LEAVE\_ERR\_MSG** (indicates that an error message should be left in the interpreter if the search fails.) .AP Tcl\_Obj \*handlerPtr in A script fragment to be installed as the unknown command handler for the namespace, or NULL to reset the handler to its default.

# Description

Namespaces are hierarchic naming contexts that can contain commands and variables.  They also maintain a list of patterns that describes what commands are exported, and can import commands that have been exported by other namespaces.  Namespaces can also be manipulated through the Tcl command [namespace].

The *Tcl\_Namespace* structure encapsulates a namespace, and is guaranteed to have the following fields in it: *name* (the local name of the namespace, with no namespace separator characters in it, with empty denoting the global namespace), *fullName* (the fully specified name of the namespace), *clientData*, *deleteProc* (the values specified in the call to **Tcl\_CreateNamespace**), and *parentPtr* (a pointer to the containing namespace, or NULL for the global namespace.)

**Tcl\_CreateNamespace** creates a new namespace.  The *deleteProc* will have the following type signature:

```
typedef void Tcl_NamespaceDeleteProc(
        void *clientData);
```

**Tcl\_DeleteNamespace** deletes a namespace, calling the *deleteProc* defined for the namespace (if any).

**Tcl\_AppendExportList** retrieves the export patterns for a namespace given namespace and appends them (as list items) to *objPtr*.

**Tcl\_Export** sets and appends to the export patterns for a namespace.  Patterns are appended unless the *resetListFirst* flag is true.

**Tcl\_Import** imports commands matching a pattern into a namespace.  Note that the pattern must include the name of the namespace to import from.  This function returns TCL\_ERROR if an attempt to import a command over an existing command is made, unless the *allowOverwrite* flag has been set.

**Tcl\_ForgetImport** removes imports matching a pattern.

**Tcl\_GetCurrentNamespace** returns the current namespace for an interpreter.

**Tcl\_GetGlobalNamespace** returns the global namespace for an interpreter.

**Tcl\_FindNamespace** searches for a namespace named *name* within the context of the namespace *contextNsPtr*.  If the namespace cannot be found, NULL is returned.

**Tcl\_FindCommand** searches for a command named *name* within the context of the namespace *contextNsPtr*.  If the command cannot be found, NULL is returned.

**Tcl\_GetNamespaceUnknownHandler** returns the unknown command handler for the namespace, or NULL if none is set.

**Tcl\_SetNamespaceUnknownHandler** sets the unknown command handler for the namespace. If *handlerPtr* is NULL, then the handler is reset to its default.

# Reference count management

The *objPtr* argument to **Tcl\_AppendExportList** should be an unshared object, as it will be modified by this function. The reference count of *objPtr* will not be altered.

**Tcl\_GetNamespaceUnknownHandler** returns a possibly shared value. Its reference count should be incremented if the value is to be retained.

The *handlerPtr* argument to **Tcl\_SetNamespaceUnknownHandler** will have its reference count incremented if it is a non-empty list.


[namespace]: namespace.md

