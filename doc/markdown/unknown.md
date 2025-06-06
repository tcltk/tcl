---
CommandName: unknown
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - info(n)
 - proc(n)
 - interp(n)
 - library(n)
 - namespace(n)
Keywords:
 - error
 - non-existent command
 - unknown
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

unknown - Handle attempts to use non-existent commands

# Synopsis

::: {.synopsis} :::
[unknown]{.cmd} [cmdName]{.arg} [arg arg]{.optdot}
:::

# Description

This command is invoked by the Tcl interpreter whenever a script tries to invoke a command that does not exist.  The default implementation of **unknown** is a library procedure defined when Tcl initializes an interpreter.  You can override the default **unknown** to change its functionality, or you can register a new handler for individual namespaces using the **namespace unknown** command.  Note that there is no default implementation of **unknown** in a safe interpreter.

If the Tcl interpreter encounters a command name for which there is not a defined command (in either the current namespace, or the global namespace), then Tcl checks for the existence of an unknown handler for the current namespace. By default, this handler is a command named **::unknown**.  If there is no such command, then the interpreter returns an error. If the **unknown** command exists (or a new handler has been registered for the current namespace), then it is invoked with arguments consisting of the fully-substituted name and arguments for the original non-existent command. The **unknown** command typically does things like searching through library directories for a command procedure with the name *cmdName*, or expanding abbreviated command names to full-length, or automatically executing unknown commands as sub-processes. In some cases (such as expanding abbreviations) **unknown** will change the original command slightly and then (re-)execute it. The result of the **unknown** command is used as the result for the original non-existent command.

The default implementation of **unknown** behaves as follows. It first calls the **auto_load** library procedure to load the command. If this succeeds, then it executes the original command with its original arguments. If the auto-load fails and Tcl is run interactively then **unknown** calls **auto_execok** to see if there is an executable file by the name *cmd*. If so, it invokes the Tcl **exec** command with *cmd* and all the *args* as arguments. If *cmd* cannot be auto-executed, **unknown** checks to see if the command was invoked at top-level and outside of any script.  If so, then **unknown** takes two additional steps. First, it sees if *cmd* has one of the following three forms: **!!**, **!***event*, or **^***old***^***new*?**^**?. If so, then **unknown** carries out history substitution in the same way that **csh** would for these constructs. Finally, **unknown** checks to see if *cmd* is a unique abbreviation for an existing Tcl command. If so, it expands the command name and executes the command with the original arguments. If none of the above efforts has been able to execute the command, **unknown** generates an error return. If the global variable **auto_noload** is defined, then the auto-load step is skipped. If the global variable **auto_noexec** is defined then the auto-exec step is skipped. Under normal circumstances the return value from **unknown** is the return value from the command that was eventually executed.

# Example

Arrange for the **unknown** command to have its standard behavior except for first logging the fact that a command was not found:

```
# Save the original one so we can chain to it
rename unknown _original_unknown

# Provide our own implementation
proc unknown args {
    puts stderr "WARNING: unknown command: $args"
    uplevel 1 [list _original_unknown {*}$args]
}
```

