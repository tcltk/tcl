---
CommandName: Tcl_Ensemble
ManualSection: 3
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - namespace(n)
 - Tcl_DeleteCommandFromToken(3)
Keywords:
 - command
 - ensemble
Copyright:
 - Copyright (c) 2005 Donal K. Fellows
---

# Name

Tcl\_CreateEnsemble, Tcl\_FindEnsemble, Tcl\_GetEnsembleFlags, Tcl\_GetEnsembleMappingDict, Tcl\_GetEnsembleNamespace, Tcl\_GetEnsembleParameterList, Tcl\_GetEnsembleUnknownHandler, Tcl\_GetEnsembleSubcommandList, Tcl\_IsEnsemble, Tcl\_SetEnsembleFlags, Tcl\_SetEnsembleMappingDict, Tcl\_SetEnsembleParameterList, Tcl\_SetEnsembleSubcommandList, Tcl\_SetEnsembleUnknownHandler - manipulate ensemble commands

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_Command]{.ret} [Tcl\_CreateEnsemble]{.ccmd}[interp, name, namespacePtr, ensFlags]{.cargs}
[Tcl\_Command]{.ret} [Tcl\_FindEnsemble]{.ccmd}[interp, cmdNameObj, flags]{.cargs}
[int]{.ret} [Tcl\_IsEnsemble]{.ccmd}[token]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleFlags]{.ccmd}[interp, token, ensFlagsPtr]{.cargs}
[int]{.ret} [Tcl\_SetEnsembleFlags]{.ccmd}[interp, token, ensFlags]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleMappingDict]{.ccmd}[interp, token, dictObjPtr]{.cargs}
[int]{.ret} [Tcl\_SetEnsembleMappingDict]{.ccmd}[interp, token, dictObj]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleParameterList]{.ccmd}[interp, token, listObjPtr]{.cargs}
[int]{.ret} [Tcl\_SetEnsembleParameterList]{.ccmd}[interp, token, listObj]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleSubcommandList]{.ccmd}[interp, token, listObjPtr]{.cargs}
[int]{.ret} [Tcl\_SetEnsembleSubcommandList]{.ccmd}[interp, token, listObj]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleUnknownHandler]{.ccmd}[interp, token, listObjPtr]{.cargs}
[int]{.ret} [Tcl\_SetEnsembleUnknownHandler]{.ccmd}[interp, token, listObj]{.cargs}
[int]{.ret} [Tcl\_GetEnsembleNamespace]{.ccmd}[interp, token, namespacePtrPtr]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in/out The interpreter in which the ensemble is to be created or found. Also where error result messages are written. The functions whose names start with **Tcl\_GetEnsemble** may have a NULL for the *interp*, but all other functions must not. .AP "const char" \*name in The name of the ensemble command to be created. .AP Tcl\_Namespace \*namespacePtr in The namespace to which the ensemble command is to be bound, or NULL for the current namespace. .AP int ensFlags in An OR'ed set of flag bits describing the basic configuration of the ensemble. Currently only one bit has meaning, **TCL\_ENSEMBLE\_PREFIX**, which is present when the ensemble command should also match unambiguous prefixes of subcommands. .AP Tcl\_Obj \*cmdNameObj in A value holding the name of the ensemble command to look up. .AP int flags in An OR'ed set of flag bits controlling the behavior of **Tcl\_FindEnsemble**. Currently only **TCL\_LEAVE\_ERR\_MSG** is supported. .AP Tcl\_Command token in A normal command token that refers to an ensemble command, or which you wish to use for testing as an ensemble command in **Tcl\_IsEnsemble**. .AP int \*ensFlagsPtr out Pointer to a variable into which to write the current ensemble flag bits; currently only the bit **TCL\_ENSEMBLE\_PREFIX** is defined. .AP Tcl\_Obj \*dictObj in A dictionary value to use for the subcommand to implementation command prefix mapping dictionary in the ensemble. May be NULL if the mapping dictionary is to be removed. .AP Tcl\_Obj \*\*dictObjPtr out Pointer to a variable into which to write the current ensemble mapping dictionary. .AP Tcl\_Obj \*listObj in A list value to use for the list of formal pre-subcommand parameters, the defined list of subcommands in the dictionary or the unknown subcommand handler command prefix. May be NULL if the subcommand list or unknown handler are to be removed. .AP Tcl\_Obj \*\*listObjPtr out Pointer to a variable into which to write the current list of formal pre-subcommand parameters, the defined list of subcommands or the current unknown handler prefix. .AP Tcl\_Namespace \*\*namespacePtrPtr out Pointer to a variable into which to write the handle of the namespace to which the ensemble is bound.

# Description

An ensemble is a command, bound to some namespace, which consists of a collection of subcommands implemented by other Tcl commands. The first argument to the ensemble command is always interpreted as a selector that states what subcommand to execute.

Ensembles are created using **Tcl\_CreateEnsemble**, which takes four arguments: the interpreter to work within, the name of the ensemble to create, the namespace within the interpreter to bind the ensemble to, and the default set of ensemble flags. The result of the function is the command token for the ensemble, which may be used to further configure the ensemble using the API described below in [Ensemble properties].

Given the name of an ensemble command, the token for that command may be retrieved using **Tcl\_FindEnsemble**. If the given command name (in *cmdNameObj*) does not refer to an ensemble command, the result of the function is NULL and (if the **TCL\_LEAVE\_ERR\_MSG** bit is set in *flags*) an error message is left in the interpreter result.

A command token may be checked to see if it refers to an ensemble using **Tcl\_IsEnsemble**. This returns 1 if the token refers to an ensemble, or 0 otherwise.

## Ensemble properties

Every ensemble has four read-write properties and a read-only property. The properties are:

**flags** (read-write)
: The set of flags for the ensemble, expressed as a bit-field. Currently, the only public flag is **TCL\_ENSEMBLE\_PREFIX** which is set when unambiguous prefixes of subcommands are permitted to be resolved to implementations as well as exact matches. The flags may be read and written using **Tcl\_GetEnsembleFlags** and **Tcl\_SetEnsembleFlags** respectively. The result of both of those functions is a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble).

**mapping dictionary** (read-write)
: A dictionary containing a mapping from subcommand names to lists of words to use as a command prefix (replacing the first two words of the command which are the ensemble command itself and the subcommand name), or NULL if every subcommand is to be mapped to the command with the same unqualified name in the ensemble's bound namespace. Defaults to NULL. May be read and written using **Tcl\_GetEnsembleMappingDict** and **Tcl\_SetEnsembleMappingDict** respectively. The result of both of those functions is a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble) and the dictionary obtained from **Tcl\_GetEnsembleMappingDict** should always be treated as immutable even if it is unshared. All command names in prefixes set via **Tcl\_SetEnsembleMappingDict** must be fully qualified.

**formal pre-subcommand parameter list** (read-write)
: A list of formal parameter names (the names only being used when generating error messages) that come at invocation of the ensemble between the name of the ensemble and the subcommand argument. NULL (the default) is equivalent to the empty list. May be read and written using **Tcl\_GetEnsembleParameterList** and **Tcl\_SetEnsembleParameterList** respectively. The result of both of those functions is a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble) and the dictionary obtained from **Tcl\_GetEnsembleParameterList** should always be treated as immutable even if it is unshared.

**subcommand list** (read-write)
: A list of all the subcommand names for the ensemble, or NULL if this is to be derived from either the keys of the mapping dictionary (see above) or (if that is also NULL) from the set of commands exported by the bound namespace. May be read and written using **Tcl\_GetEnsembleSubcommandList** and **Tcl\_SetEnsembleSubcommandList** respectively. The result of both of those functions is a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble) and the list obtained from **Tcl\_GetEnsembleSubcommandList** should always be treated as immutable even if it is unshared.

[unknown subcommand handler command prefix][unknown] (read-write)
: A list of words to prepend on the front of any subcommand when the subcommand is unknown to the ensemble (according to the current prefix handling rule); see the [namespace ensemble][namespace] command for more details. If NULL, the default behavior - generate a suitable error message - will be used when an unknown subcommand is encountered. May be read and written using **Tcl\_GetEnsembleUnknownHandler** and **Tcl\_SetEnsembleUnknownHandler** respectively. The result of both functions is a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble) and the list obtained from **Tcl\_GetEnsembleUnknownHandler** should always be treated as immutable even if it is unshared.

**bound namespace** (read-only)
: The namespace to which the ensemble is bound; when the namespace is deleted, so too will the ensemble, and this namespace is also the namespace whose list of exported commands is used if both the mapping dictionary and the subcommand list properties are NULL. May be read using **Tcl\_GetEnsembleNamespace** which returns a Tcl result code (**TCL\_OK**, or **TCL\_ERROR** if the token does not refer to an ensemble).


# Reference count management

**Tcl\_FindEnsemble** does not modify the reference count of its *cmdNameObj* argument; it only reads. Note however that this function may set the interpreter result; if that is the only place that is holding a reference to the object, it will be deleted.

The ensemble property getters (**Tcl\_GetEnsembleMappingDict**, **Tcl\_GetEnsembleParameterList**, **Tcl\_GetEnsembleSubcommandList**, and **Tcl\_GetEnsembleUnknownHandler**) do not manipulate the reference count of the values they provide out; if those are non-NULL, they will have a reference count of at least 1.  Note that these functions may set the interpreter result.

The ensemble property setters (**Tcl\_SetEnsembleMappingDict**, **Tcl\_SetEnsembleParameterList**, **Tcl\_SetEnsembleSubcommandList**, and **Tcl\_SetEnsembleUnknownHandler**) will increment the reference count of the new value of the property they are given if they succeed (and decrement the reference count of the old value of the property, if relevant). If the property setters return **TCL\_ERROR**, the reference count of the Tcl\_Obj argument is left unchanged.


[namespace]: namespace.md
[unknown]: unknown.md

