---
CommandName: unload
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - info sharedlibextension
 - load(n)
 - safe(n)
Keywords:
 - binary code
 - unloading
 - safe interpreter
 - shared library
Copyright:
 - Copyright (c) 2003 George Petasis <petasis@iit.demokritos.gr>.
---

# Name

unload - Unload machine code

# Synopsis

::: {.synopsis} :::
[unload]{.cmd} [switches]{.optarg} [fileName]{.arg}
[unload]{.cmd} [switches]{.optarg} [fileName]{.arg} [prefix]{.arg}
[unload]{.cmd} [switches]{.optarg} [fileName]{.arg} [prefix]{.arg} [interp]{.arg}
:::

# Description

This command tries to unload shared libraries previously loaded with **load** from the application's address space.  *fileName* is the name of the file containing the library file to be unload;  it must be the same as the filename provided to **load** for loading the library. The *prefix* argument is the prefix (as determined by or passed to **load**), and is used to compute the name of the unload procedure; if not supplied, it is computed from *fileName* in the same manner as **load**. The *interp* argument is the path name of the interpreter from which to unload the package (see the **interp** manual entry for details); if *interp* is omitted, it defaults to the interpreter in which the **unload** command was invoked.

If the initial arguments to **unload** start with **-** then they are treated as switches.  The following switches are currently supported:

**-nocomplain**
: Suppresses all error messages. If this switch is given, **unload** will never report an error.

**-keeplibrary**
: This switch will prevent **unload** from issuing the operating system call that will unload the library from the process.

**-\|-**
: Marks the end of switches.  The argument following this one will be treated as a *fileName* even if it starts with a **-**.


## Unload operation

When a file containing a shared library is loaded through the **load** command, Tcl associates two reference counts to the library file. The first counter shows how many times the library has been loaded into normal (trusted) interpreters while the second describes how many times the library has been loaded into safe interpreters. As a file containing a shared library can be loaded only once by Tcl (with the first **load** call on the file), these counters track how many interpreters use the library. Each subsequent call to **load** after the first simply increments the proper reference count.

**unload** works in the opposite direction. As a first step, **unload** will check whether the library is unloadable: an unloadable library exports a special unload procedure. The name of the unload procedure is determined by *prefix* and whether or not the target interpreter is a safe one.  For normal interpreters the name of the initialization procedure will have the form *pfx***_Unload**, where *pfx* is the same as *prefix* except that the first letter is converted to upper case and all other letters are converted to lower case.  For example, if *prefix* is **foo** or **FOo**, the initialization procedure's name will be **Foo_Unload**. If the target interpreter is a safe interpreter, then the name of the initialization procedure will be *pkg***_SafeUnload** instead of *pkg***_Unload**.

If **unload** determines that a library is not unloadable (or unload functionality has been disabled during compilation), an error will be returned. If the library is unloadable, then **unload** will call the unload procedure. If the unload procedure returns **TCL_OK**, **unload** will proceed and decrease the proper reference count (depending on the target interpreter type). When both reference counts have reached 0, the library will be detached from the process.

## Unload hook prototype

The unload procedure must match the following prototype:

```
typedef int Tcl_LibraryUnloadProc(
        Tcl_Interp *interp,
        int flags);
```

The *interp* argument identifies the interpreter from which the library is to be unloaded.  The unload procedure must return **TCL_OK** or **TCL_ERROR** to indicate whether or not it completed successfully;  in the event of an error it should set the interpreter's result to point to an error message.  In this case, the result of the **unload** command will be the result returned by the unload procedure.

The *flags* argument can be either **TCL_UNLOAD_DETACH_FROM_INTERPRETER** or **TCL_UNLOAD_DETACH_FROM_PROCESS**. In case the library will remain attached to the process after the unload procedure returns (i.e. because the library is used by other interpreters), **TCL_UNLOAD_DETACH_FROM_INTERPRETER** will be defined. However, if the library is used only by the target interpreter and the library will be detached from the application as soon as the unload procedure returns, the *flags* argument will be set to **TCL_UNLOAD_DETACH_FROM_PROCESS**.

## Notes

The **unload** command cannot unload libraries that are statically linked with the application. If *fileName* is an empty string, then the *prefix* argument must be specified.

If *prefix* is omitted or specified as an empty string, Tcl tries to guess the prefix. This may be done differently on different platforms. The default guess, which is used on most UNIX platforms, is to take the last element of *fileName*, strip off the first three characters if they are **lib**, then strip off the next three characters if they are **tcl9**, and use any following wordchars but not digits, converted to titlecase as the prefix. For example, the command **unload libxyz4.2.so** uses the prefix **Xyz** and the command **unload bin/last.so {}** uses the prefix **Last**.

# Portability issues

**Unix**
: Not all unix operating systems support library unloading. Under such an operating system **unload** returns an error (unless **-nocomplain** has been specified).


# Bugs

If the same file is **load**ed by different *fileName*s, it will be loaded into the process's address space multiple times.  The behavior of this varies from system to system (some systems may detect the redundant loads, others may not). In case a library has been silently detached by the operating system (and as a result Tcl thinks the library is still loaded), it may be dangerous to use **unload** on such a library (as the library will be completely detached from the application while some interpreters will continue to use it).

# Example

If an unloadable module in the file **foobar.dll** had been loaded using the **load** command like this (on Windows):

```
load c:/some/dir/foobar.dll
```

then it would be unloaded like this:

```
unload c:/some/dir/foobar.dll
```

This allows a C code module to be installed temporarily into a long-running Tcl program and then removed again (either because it is no longer needed or because it is being updated with a new version) without having to shut down the overall Tcl process.

