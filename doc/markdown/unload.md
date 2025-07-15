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

This command tries to unload shared libraries previously loaded with \fBload\fR from the application's address space.  \fIfileName\fR is the name of the file containing the library file to be unload;  it must be the same as the filename provided to \fBload\fR for loading the library. The \fIprefix\fR argument is the prefix (as determined by or passed to \fBload\fR), and is used to compute the name of the unload procedure; if not supplied, it is computed from \fIfileName\fR in the same manner as \fBload\fR. The \fIinterp\fR argument is the path name of the interpreter from which to unload the package (see the \fBinterp\fR manual entry for details); if \fIinterp\fR is omitted, it defaults to the interpreter in which the \fBunload\fR command was invoked.

If the initial arguments to \fBunload\fR start with \fB-\fR then they are treated as switches.  The following switches are currently supported:

**-nocomplain**
: Suppresses all error messages. If this switch is given, \fBunload\fR will never report an error.

**-keeplibrary**
: This switch will prevent \fBunload\fR from issuing the operating system call that will unload the library from the process.

**-\|-**
: Marks the end of switches.  The argument following this one will be treated as a \fIfileName\fR even if it starts with a \fB-\fR.


## Unload operation

When a file containing a shared library is loaded through the \fBload\fR command, Tcl associates two reference counts to the library file. The first counter shows how many times the library has been loaded into normal (trusted) interpreters while the second describes how many times the library has been loaded into safe interpreters. As a file containing a shared library can be loaded only once by Tcl (with the first \fBload\fR call on the file), these counters track how many interpreters use the library. Each subsequent call to \fBload\fR after the first simply increments the proper reference count.

**unload** works in the opposite direction. As a first step, \fBunload\fR will check whether the library is unloadable: an unloadable library exports a special unload procedure. The name of the unload procedure is determined by \fIprefix\fR and whether or not the target interpreter is a safe one.  For normal interpreters the name of the initialization procedure will have the form \fIpfx\fR\fB_Unload\fR, where \fIpfx\fR is the same as \fIprefix\fR except that the first letter is converted to upper case and all other letters are converted to lower case.  For example, if \fIprefix\fR is \fBfoo\fR or \fBFOo\fR, the initialization procedure's name will be \fBFoo_Unload\fR. If the target interpreter is a safe interpreter, then the name of the initialization procedure will be \fIpkg\fR\fB_SafeUnload\fR instead of \fIpkg\fR\fB_Unload\fR.

If \fBunload\fR determines that a library is not unloadable (or unload functionality has been disabled during compilation), an error will be returned. If the library is unloadable, then \fBunload\fR will call the unload procedure. If the unload procedure returns \fBTCL_OK\fR, \fBunload\fR will proceed and decrease the proper reference count (depending on the target interpreter type). When both reference counts have reached 0, the library will be detached from the process.

## Unload hook prototype

The unload procedure must match the following prototype:

```
typedef int Tcl_LibraryUnloadProc(
        Tcl_Interp *interp,
        int flags);
```

The \fIinterp\fR argument identifies the interpreter from which the library is to be unloaded.  The unload procedure must return \fBTCL_OK\fR or \fBTCL_ERROR\fR to indicate whether or not it completed successfully;  in the event of an error it should set the interpreter's result to point to an error message.  In this case, the result of the \fBunload\fR command will be the result returned by the unload procedure.

The \fIflags\fR argument can be either \fBTCL_UNLOAD_DETACH_FROM_INTERPRETER\fR or \fBTCL_UNLOAD_DETACH_FROM_PROCESS\fR. In case the library will remain attached to the process after the unload procedure returns (i.e. because the library is used by other interpreters), \fBTCL_UNLOAD_DETACH_FROM_INTERPRETER\fR will be defined. However, if the library is used only by the target interpreter and the library will be detached from the application as soon as the unload procedure returns, the \fIflags\fR argument will be set to \fBTCL_UNLOAD_DETACH_FROM_PROCESS\fR.

## Notes

The \fBunload\fR command cannot unload libraries that are statically linked with the application. If \fIfileName\fR is an empty string, then the \fIprefix\fR argument must be specified.

If \fIprefix\fR is omitted or specified as an empty string, Tcl tries to guess the prefix. This may be done differently on different platforms. The default guess, which is used on most UNIX platforms, is to take the last element of \fIfileName\fR, strip off the first three characters if they are \fBlib\fR, then strip off the next three characters if they are \fBtcl9\fR, and use any following wordchars but not digits, converted to titlecase as the prefix. For example, the command \fBunload libxyz4.2.so\fR uses the prefix \fBXyz\fR and the command \fBunload bin/last.so {}\fR uses the prefix \fBLast\fR.

# Portability issues

**Unix**
: Not all unix operating systems support library unloading. Under such an operating system \fBunload\fR returns an error (unless \fB-nocomplain\fR has been specified).


# Bugs

If the same file is \fBload\fRed by different \fIfileName\fRs, it will be loaded into the process's address space multiple times.  The behavior of this varies from system to system (some systems may detect the redundant loads, others may not). In case a library has been silently detached by the operating system (and as a result Tcl thinks the library is still loaded), it may be dangerous to use \fBunload\fR on such a library (as the library will be completely detached from the application while some interpreters will continue to use it).

# Example

If an unloadable module in the file \fBfoobar.dll\fR had been loaded using the \fBload\fR command like this (on Windows):

```
load c:/some/dir/foobar.dll
```

then it would be unloaded like this:

```
unload c:/some/dir/foobar.dll
```

This allows a C code module to be installed temporarily into a long-running Tcl program and then removed again (either because it is no longer needed or because it is being updated with a new version) without having to shut down the overall Tcl process.

