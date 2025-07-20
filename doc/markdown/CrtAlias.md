---
CommandName: Tcl_CreateAlias
ManualSection: 3
Version: 7.6
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - interp(n)
Keywords:
 - alias
 - command
 - exposed commands
 - hidden commands
 - interpreter
 - invoke
 - parent
 - child
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

Tcl_IsSafe, Tcl_CreateChild, Tcl_GetChild, Tcl_GetParent, Tcl_GetInterpPath, Tcl_CreateAlias, Tcl_CreateAliasObj, Tcl_GetAliasObj, Tcl_ExposeCommand, Tcl_HideCommand - manage multiple Tcl interpreters, aliases and hidden commands

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_IsSafe]{.ccmd}[interp]{.cargs}
[Tcl_Interp *]{.ret} [Tcl_CreateChild]{.ccmd}[interp, name, isSafe]{.cargs}
[Tcl_Interp *]{.ret} [Tcl_GetChild]{.ccmd}[interp, name]{.cargs}
[Tcl_Interp *]{.ret} [Tcl_GetParent]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_GetInterpPath]{.ccmd}[interp, childInterp]{.cargs}
[int]{.ret} [Tcl_CreateAlias]{.ccmd}[childInterp, childCmd, targetInterp, targetCmd, argc, argv]{.cargs}
[int]{.ret} [Tcl_CreateAliasObj]{.ccmd}[childInterp, childCmd, targetInterp, targetCmd, objc, objv]{.cargs}
[int]{.ret} [Tcl_GetAliasObj]{.ccmd}[interp, childCmd, targetInterpPtr, targetCmdPtr, objcPtr, objvPtr]{.cargs}
[int]{.ret} [Tcl_ExposeCommand]{.ccmd}[interp, hiddenCmdName, cmdName]{.cargs}
[int]{.ret} [Tcl_HideCommand]{.ccmd}[interp, cmdName, hiddenCmdName]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter in which to execute the specified command. .AP "const char" *name in Name of child interpreter to create or manipulate. .AP int isSafe in If non-zero, a "safe" child that is suitable for running untrusted code is created, otherwise a trusted child is created. .AP Tcl_Interp *childInterp in Interpreter to use for creating the source command for an alias (see below). .AP "const char" *childCmd in Name of source command for alias. .AP Tcl_Interp *targetInterp in Interpreter that contains the target command for an alias. .AP "const char" *targetCmd in Name of target command for alias in *targetInterp*. .AP Tcl_Size argc in Count of additional arguments to pass to the alias command. .AP "const char *const" *argv in Vector of strings, the additional arguments to pass to the alias command. This storage is owned by the caller. .AP Tcl_Size objc in Count of additional value arguments to pass to the aliased command. .AP Tcl_Obj **objv in Vector of Tcl_Obj structures, the additional value arguments to pass to the aliased command. This storage is owned by the caller. .AP Tcl_Interp **targetInterpPtr in Pointer to location to store the address of the interpreter where a target command is defined for an alias. .AP "const char" **targetCmdPtr out Pointer to location to store the address of the name of the target command for an alias. .AP "Tcl_Size \&| int" *objcPtr out Pointer to location to store count of additional value arguments to be passed to the alias. The location is in storage owned by the caller. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return TCL_ERROR for aliases with more than INT_MAX value arguments, otherwise expect it to crash .AP Tcl_Obj ***objvPtr out Pointer to location to store a vector of Tcl_Obj structures, the additional arguments to pass to an alias command. The location is in storage owned by the caller, the vector of Tcl_Obj structures is owned by the called function. .AP "const char" *cmdName in Name of an exposed command to hide or create. .AP "const char" *hiddenCmdName in Name under which a hidden command is stored and with which it can be exposed or invoked. 

# Description

These procedures are intended for access to the multiple interpreter facility from inside C programs. They enable managing multiple interpreters in a hierarchical relationship, and the management of aliases, commands that when invoked in one interpreter execute a command in another interpreter. The return value for those procedures that return an **int** is either **TCL_OK** or **TCL_ERROR**. If **TCL_ERROR** is returned then the interpreter's result contains an error message.

**Tcl_CreateChild** creates a new interpreter as a child of *interp*. It also creates a child command named *name* in *interp* which allows *interp* to manipulate the new child. If *isSafe* is zero, the command creates a trusted child in which Tcl code has access to all the Tcl commands. If it is **1**, the command creates a "safe" child in which Tcl code has access only to set of Tcl commands defined as "Safe Tcl"; see the manual entry for the Tcl **interp** command for details. If the creation of the new child interpreter failed, **NULL** is returned.

**Tcl_IsSafe** returns **1** if *interp* is "safe" (was created with the **TCL_SAFE_INTERPRETER** flag specified), **0** otherwise.

**Tcl_GetChild** returns a pointer to a child interpreter of *interp*. The child interpreter is identified by *name*. If no such child interpreter exists, **NULL** is returned.

**Tcl_GetParent** returns a pointer to the parent interpreter of *interp*. If *interp* has no parent (it is a top-level interpreter) then **NULL** is returned.

**Tcl_GetInterpPath** stores in the result of *interp* the relative path between *interp* and *childInterp*; *childInterp* must be a child of *interp*. If the computation of the relative path succeeds, **TCL_OK** is returned, else **TCL_ERROR** is returned and an error message is stored as the result of *interp*.

**Tcl_CreateAlias** creates a command named *childCmd* in *childInterp* that when invoked, will cause the command *targetCmd* to be invoked in *targetInterp*. The arguments specified by the strings contained in *argv* are always prepended to any arguments supplied in the invocation of *childCmd* and passed to *targetCmd*. This operation returns **TCL_OK** if it succeeds, or **TCL_ERROR** if it fails; in that case, an error message is left in the value result of *childInterp*. Note that there are no restrictions on the ancestry relationship (as created by **Tcl_CreateChild**) between *childInterp* and *targetInterp*. Any two interpreters can be used, without any restrictions on how they are related.

**Tcl_CreateAliasObj** is similar to **Tcl_CreateAlias** except that it takes a vector of values to pass as additional arguments instead of a vector of strings.

**Tcl_GetAliasObj** returns information in the form of a pointer to a vector of Tcl_Obj structures about an alias *aliasName* in *interp*. Any of the result fields can be **NULL**, in which case the corresponding datum is not returned. If a result field is non-**NULL**, the address indicated is set to the corresponding datum. For example, if *targetCmdPtr* is non-**NULL** it is set to a pointer to the string containing the name of the target command.

**Tcl_ExposeCommand** moves the command named *hiddenCmdName* from the set of hidden commands to the set of exposed commands, putting it under the name *cmdName*. *HiddenCmdName* must be the name of an existing hidden command, or the operation will return **TCL_ERROR** and leave an error message as the result of *interp*. If an exposed command named *cmdName* already exists, the operation returns **TCL_ERROR** and leaves an error message as the result of *interp*. If the operation succeeds, it returns **TCL_OK**. After executing this command, attempts to use *cmdName* in any script evaluation mechanism will again succeed.

**Tcl_HideCommand** moves the command named *cmdName* from the set of exposed commands to the set of hidden commands, under the name *hiddenCmdName*. *CmdName* must be the name of an existing exposed command, or the operation will return **TCL_ERROR** and leave an error message as the result of *interp*. Currently both *cmdName* and *hiddenCmdName* must not contain namespace qualifiers, or the operation will return **TCL_ERROR** and leave an error message as the result of *interp*. The *CmdName* will be looked up in the global namespace, and not relative to the current namespace, even if the current namespace is not the global one. If a hidden command whose name is *hiddenCmdName* already exists, the operation also returns **TCL_ERROR** and an error message is left as the result of *interp*. If the operation succeeds, it returns **TCL_OK**. After executing this command, attempts to use *cmdName* in any script evaluation mechanism will fail.

For a description of the Tcl interface to multiple interpreters, see *interp(n)*.

# Reference count management

**Tcl_CreateAliasObj** increments the reference counts of the values in its *objv* argument. (That reference lasts the same length of time as the owning alias.)

**Tcl_GetAliasObj** returns (via its *objvPtr* argument) a pointer to values that it holds a reference to.

