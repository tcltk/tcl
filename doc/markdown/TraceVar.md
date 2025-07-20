---
CommandName: Tcl_TraceVar
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - trace(n)
Keywords:
 - clientData
 - trace
 - variable
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl_TraceVar, Tcl_TraceVar2, Tcl_UntraceVar, Tcl_UntraceVar2, Tcl_VarTraceInfo, Tcl_VarTraceInfo2 - monitor accesses to a variable

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_TraceVar]{.ccmd}[interp, varName, flags, proc, clientData]{.cargs}
[int]{.ret} [Tcl_TraceVar2]{.ccmd}[interp, name1, name2, flags, proc, clientData]{.cargs}
[Tcl_UntraceVar]{.ccmd}[interp, varName, flags, proc, clientData]{.cargs}
[Tcl_UntraceVar2]{.ccmd}[interp, name1, name2, flags, proc, clientData]{.cargs}
[void *]{.ret} [Tcl_VarTraceInfo]{.ccmd}[interp, varName, flags, proc, prevClientData]{.cargs}
[void *]{.ret} [Tcl_VarTraceInfo2]{.ccmd}[interp, name1, name2, flags, proc, prevClientData]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Interpreter containing variable. .AP "const char" *varName in Name of variable.  May refer to a scalar variable, to an array variable with no index, or to an array variable with a parenthesized index. .AP int flags in OR-ed combination of the values **TCL_TRACE_READS**, **TCL_TRACE_WRITES**, **TCL_TRACE_UNSETS**, **TCL_TRACE_ARRAY**, **TCL_GLOBAL_ONLY**, **TCL_NAMESPACE_ONLY**, **TCL_TRACE_RESULT_DYNAMIC** and **TCL_TRACE_RESULT_OBJECT**. Not all flags are used by all procedures.  See below for more information. .AP Tcl_VarTraceProc *proc in Procedure to invoke whenever one of the traced operations occurs. .AP void *clientData in Arbitrary one-word value to pass to *proc*. .AP "const char" *name1 in Name of scalar or array variable (without array index). .AP "const char" *name2 in For a trace on an element of an array, gives the index of the element.  For traces on scalar variables or on whole arrays, is NULL. .AP void *prevClientData in If non-NULL, gives last value returned by **Tcl_VarTraceInfo** or **Tcl_VarTraceInfo2**, so this call will return information about next trace.  If NULL, this call will return information about first trace.

# Description

**Tcl_TraceVar** allows a C procedure to monitor and control access to a Tcl variable, so that the C procedure is invoked whenever the variable is read or written or unset. If the trace is created successfully then **Tcl_TraceVar** returns **TCL_OK**.  If an error occurred (e.g. *varName* specifies an element of an array, but the actual variable is not an array) then **TCL_ERROR** is returned and an error message is left in the interpreter's result.

The *flags* argument to **Tcl_TraceVar** indicates when the trace procedure is to be invoked and provides information for setting up the trace.  It consists of an OR-ed combination of any of the following values:

**TCL_GLOBAL_ONLY**
: Normally, the variable will be looked up at the current level of procedure call;  if this bit is set then the variable will be looked up at global level, ignoring any active procedures.

**TCL_NAMESPACE_ONLY**
: Normally, the variable will be looked up at the current level of procedure call;  if this bit is set then the variable will be looked up in the current namespace, ignoring any active procedures.

**TCL_TRACE_READS**
: Invoke *proc* whenever an attempt is made to read the variable.

**TCL_TRACE_WRITES**
: Invoke *proc* whenever an attempt is made to modify the variable.

**TCL_TRACE_UNSETS**
: Invoke *proc* whenever the variable is unset. A variable may be unset either explicitly by an **unset** command, or implicitly when a procedure returns (its local variables are automatically unset) or when the interpreter or namespace is deleted (all variables are automatically unset).

**TCL_TRACE_ARRAY**
: Invoke *proc* whenever the array command is invoked. This gives the trace procedure a chance to update the array before array names or array get is called.  Note that this is called before an array set, but that will trigger write traces.

**TCL_TRACE_RESULT_DYNAMIC**
: The result of invoking the *proc* is a dynamically allocated string that will be released by the Tcl library via a call to **Tcl_Free**.  Must not be specified at the same time as **TCL_TRACE_RESULT_OBJECT**.

**TCL_TRACE_RESULT_OBJECT**
: The result of invoking the *proc* is a Tcl_Obj* (cast to a char*) with a reference count of at least one.  The ownership of that reference will be transferred to the Tcl core for release (when the core has finished with it) via a call to **Tcl_DecrRefCount**.  Must not be specified at the same time as **TCL_TRACE_RESULT_DYNAMIC**.


Whenever one of the specified operations occurs on the variable, *proc* will be invoked. It should have arguments and result that match the type **Tcl_VarTraceProc**:

```
typedef char *Tcl_VarTraceProc(
        void *clientData,
        Tcl_Interp *interp,
        const char *name1,
        const char *name2,
        int flags);
```

The *clientData* and *interp* parameters will have the same values as those passed to **Tcl_TraceVar** when the trace was created. *clientData* typically points to an application-specific data structure that describes what to do when *proc* is invoked. *Name1* and *name2* give the name of the variable that triggered the callback in the normal two-part form (see the description of **Tcl_TraceVar2** below for details).  In case *name1* is an alias to an array element (created through facilities such as **upvar**), *name2* holds the index of the array element, rather than NULL. *Flags* is an OR-ed combination of bits providing several pieces of information. One of the bits **TCL_TRACE_READS**, **TCL_TRACE_WRITES**, **TCL_TRACE_ARRAY**, or **TCL_TRACE_UNSETS** will be set in *flags* to indicate which operation is being performed on the variable. The bit **TCL_GLOBAL_ONLY** will be set whenever the variable being accessed is a global one not accessible from the current level of procedure call:  the trace procedure will need to pass this flag back to variable-related procedures like **Tcl_GetVar** if it attempts to access the variable. The bit **TCL_NAMESPACE_ONLY** will be set whenever the variable being accessed is a namespace one not accessible from the current level of procedure call:  the trace procedure will need to pass this flag back to variable-related procedures like **Tcl_GetVar** if it attempts to access the variable. The bit **TCL_TRACE_DESTROYED** will be set in *flags* if the trace is about to be destroyed;  this information may be useful to *proc* so that it can clean up its own internal data structures (see the section **TCL_TRACE_DESTROYED** below for more details). The trace procedure's return value should normally be NULL;  see **ERROR RETURNS** below for information on other possibilities.

**Tcl_UntraceVar** may be used to remove a trace. If the variable specified by *interp*, *varName*, and *flags* has a trace set with *flags*, *proc*, and *clientData*, then the corresponding trace is removed. If no such trace exists, then the call to **Tcl_UntraceVar** has no effect. The same bits are valid for *flags* as for calls to **Tcl_TraceVar**.

**Tcl_VarTraceInfo** may be used to retrieve information about traces set on a given variable. The return value from **Tcl_VarTraceInfo** is the *clientData* associated with a particular trace. The trace must be on the variable specified by the *interp*, *varName*, and *flags* arguments (only the **TCL_GLOBAL_ONLY** and **TCL_NAMESPACE_ONLY** bits from *flags* is used;  other bits are ignored) and its trace procedure must the same as the *proc* argument. If the *prevClientData* argument is NULL then the return value corresponds to the first (most recently created) matching trace, or NULL if there are no matching traces. If the *prevClientData* argument is not NULL, then it should be the return value from a previous call to **Tcl_VarTraceInfo**. In this case, the new return value will correspond to the next matching trace after the one whose *clientData* matches *prevClientData*, or NULL if no trace matches *prevClientData* or if there are no more matching traces after it. This mechanism makes it possible to step through all of the traces for a given variable that have the same *proc*.

# Two-part names

The procedures **Tcl_TraceVar2**, **Tcl_UntraceVar2**, and **Tcl_VarTraceInfo2** are identical to **Tcl_TraceVar**, **Tcl_UntraceVar**, and **Tcl_VarTraceInfo**, respectively, except that the name of the variable consists of two parts. *Name1* gives the name of a scalar variable or array, and *name2* gives the name of an element within an array. When *name2* is NULL, *name1* may contain both an array and an element name: if the name contains an open parenthesis and ends with a close parenthesis, then the value between the parentheses is treated as an element name (which can have any string value) and the characters before the first open parenthesis are treated as the name of an array variable. If *name2* is NULL and *name1* does not refer to an array element it means that either the variable is a scalar or the trace is to be set on the entire array rather than an individual element (see WHOLE-ARRAY TRACES below for more information).

# Accessing variables during traces

During read, write, and array traces, the trace procedure can read, write, or unset the traced variable using **Tcl_GetVar2**, **Tcl_SetVar2**, and other procedures. While *proc* is executing, traces are temporarily disabled for the variable, so that calls to **Tcl_GetVar2** and **Tcl_SetVar2** will not cause *proc* or other trace procedures to be invoked again. Disabling only occurs for the variable whose trace procedure is active;  accesses to other variables will still be traced. However, if a variable is unset during a read or write trace then unset traces will be invoked.

During unset traces the variable has already been completely expunged. It is possible for the trace procedure to read or write the variable, but this will be a new version of the variable. Traces are not disabled during unset traces as they are for read and write traces, but existing traces have been removed from the variable before any trace procedures are invoked. If new traces are set by unset trace procedures, these traces will be invoked on accesses to the variable by the trace procedures.

# Callback timing

When read tracing has been specified for a variable, the trace procedure will be invoked whenever the variable's value is read.  This includes **set** Tcl commands, **$**-notation in Tcl commands, and invocations of the **Tcl_GetVar** and **Tcl_GetVar2** procedures. *Proc* is invoked just before the variable's value is returned. It may modify the value of the variable to affect what is returned by the traced access. If it unsets the variable then the access will return an error just as if the variable never existed.

When write tracing has been specified for a variable, the trace procedure will be invoked whenever the variable's value is modified.  This includes **set** commands, commands that modify variables as side effects (such as **catch** and **scan**), and calls to the **Tcl_SetVar** and **Tcl_SetVar2** procedures). *Proc* will be invoked after the variable's value has been modified, but before the new value of the variable has been returned. It may modify the value of the variable to override the change and to determine the value actually returned by the traced access. If it deletes the variable then the traced access will return an empty string.

When array tracing has been specified, the trace procedure will be invoked at the beginning of the array command implementation, before any of the operations like get, set, or names have been invoked. The trace procedure can modify the array elements with **Tcl_SetVar** and **Tcl_SetVar2**.

When unset tracing has been specified, the trace procedure will be invoked whenever the variable is destroyed. The traces will be called after the variable has been completely unset.

# Whole-array traces

If a call to **Tcl_TraceVar** or **Tcl_TraceVar2** specifies the name of an array variable without an index into the array, then the trace will be set on the array as a whole. This means that *proc* will be invoked whenever any element of the array is accessed in the ways specified by *flags*. When an array is unset, a whole-array trace will be invoked just once, with *name1* equal to the name of the array and *name2* NULL;  it will not be invoked once for each element.

# Multiple traces

It is possible for multiple traces to exist on the same variable. When this happens, all of the trace procedures will be invoked on each access, in order from most-recently-created to least-recently-created. When there exist whole-array traces for an array as well as traces on individual elements, the whole-array traces are invoked before the individual-element traces. If a read or write trace unsets the variable then all of the unset traces will be invoked but the remainder of the read and write traces will be skipped.

# Error returns

Under normal conditions trace procedures should return NULL, indicating successful completion. If *proc* returns a non-NULL value it signifies that an error occurred. The return value must be a pointer to a static character string containing an error message, unless (*exactly* one of) the **TCL_TRACE_RESULT_DYNAMIC** and **TCL_TRACE_RESULT_OBJECT** flags is set, which specify that the result is either a dynamic string (to be released with **Tcl_Free**) or a Tcl_Obj* (cast to char* and to be released with **Tcl_DecrRefCount**) containing the error message. If a trace procedure returns an error, no further traces are invoked for the access and the traced access aborts with the given message. Trace procedures can use this facility to make variables read-only, for example (but note that the value of the variable will already have been modified before the trace procedure is called, so the trace procedure will have to restore the correct value).

The return value from *proc* is only used during read and write tracing. During unset traces, the return value is ignored and all relevant trace procedures will always be invoked.

# Restrictions

Because operations on variables may take place as part of the deletion of the interp that contains them, *proc* must be careful about checking what the *interp* parameter can be used to do. The routine **Tcl_InterpDeleted** is an important tool for this. When **Tcl_InterpDeleted** returns 1, *proc* will not be able to invoke any scripts in *interp*. You may encounter old code using a deprecated flag value **TCL_INTERP_DESTROYED** to signal this condition, but Tcl 9 no longer supports this. Any supported code must be converted to stop using it.

A trace procedure can be called at any time, even when there are partially formed results stored in the interpreter.  If the trace procedure does anything that could damage this result (such as calling **Tcl_Eval**) then it must use the **Tcl_SaveInterpState** and related routines to save and restore the original state of the interpreter before it returns.

# Undefined variables

It is legal to set a trace on an undefined variable. The variable will still appear to be undefined until the first time its value is set. If an undefined variable is traced and then unset, the unset will fail with an error ("no such variable"), but the trace procedure will still be invoked.

# Tcl_trace_destroyed flag

In an unset callback to *proc*, the **TCL_TRACE_DESTROYED** bit is set in *flags* if the trace is being removed as part of the deletion. Traces on a variable are always removed whenever the variable is deleted;  the only time **TCL_TRACE_DESTROYED** is not set is for a whole-array trace invoked when only a single element of an array is unset.

# Reference count management

When a *proc* callback is invoked, and that callback was installed with the **TCL_TRACE_RESULT_OBJECT** flag, the result of the callback is a Tcl_Obj reference when there is an error. The result will have its reference count decremented once when no longer needed, or may have additional references made to it (e.g., by setting it as the interpreter result with **Tcl_SetObjResult**).

# Bugs

Array traces are not yet integrated with the Tcl **info exists** command, nor is there Tcl-level access to array traces.

