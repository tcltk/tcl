---
CommandName: build-info
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - info(n)
Keywords:
 - build-info
Copyright:
 - Copyright (c) 2025 Jan Nijtmans.
---

# Name

buildinfo - Build info

# Synopsis

::: {.synopsis} :::
[::tcl::build-info]{.cmd} [field]{.optarg}
:::

# Description

This command provides a way to retrieve information about how Tcl was built. Without any options, the command returns the Tcl patchlevel, followed by the '+'-sign, followed by the fossil commit-id followed by a list of dot-separated tags. If a *field* is given, this command extracts that field as described below. Any other *field* value not mentioned below will always return "0". For official Tcl releases, the *field*s are:

[::tcl::build-info]{.cmd} [clang]{.sub}
: Returns the clang version number (as 4 digits) if Tcl is compiled with clang, 0 otherwise.

**::tcl::build-info commit**
: Returns the fossil commit-id where Tcl was built from.

**::tcl::build-info compiledebug**
: Returns 1 if Tcl is compiled with **-DTCL\_COMPILE\_DEBUG**, 0 otherwise.

**::tcl::build-info compiler**
: Returns the compiler name (either clang, gcc, icc or msvc), followed by a dash and a (4-digit) version number.

**::tcl::build-info compilestats**
: Returns 1 if Tcl is compiled with **-DTCL\_COMPILE\_STATS**, 0 otherwise.

**::tcl::build-info cplusplus**
: Returns 1 if Tcl is compiled with a C++ compiler, 0 otherwise.

**::tcl::build-info debug**
: Returns 1 if Tcl is not compiled with **-DNDEBUG**, 0 otherwise.

**::tcl::build-info gcc**
: Returns the gcc version number (as 4 digits) if Tcl is compiled with gcc, 0 otherwise.

**::tcl::build-info icc**
: Returns the icc version number (as 4 digits) if Tcl is compiled with icc, 0 otherwise.

**::tcl::build-info ilp32**
: Returns 1 if Tcl is compiled such that integers, longs and pointers are all 32-bit, 0 otherwise.

**::tcl::build-info memdebug**
: Returns 1 if Tcl is compiled with **-DTCL\_MEM\_DEBUG**, 0 otherwise.

**::tcl::build-info msvc**
: Returns the msvc version number (as 4 digits) if Tcl is compiled with msvc, 0 otherwise.

**::tcl::build-info nmake**
: Returns 1 if Tcl is built using nmake, 0 otherwise.

**::tcl::build-info no-deprecate**
: Returns 1 if Tcl is compiled with **-DTCL\_NO\_DEPRECATED**, 0 otherwise.

**::tcl::build-info no-thread**
: Returns 1 if Tcl is compiled with **-DTCL\_THREADS=0**, 0 otherwise.

**::tcl::build-info no-optimize**
: Returns 1 if Tcl is not compiled with **-DTCL\_CFG\_OPTIMIZED**, 0 otherwise.

**::tcl::build-info objective-c**
: Returns 1 if Tcl is compiled with an objective-c compiler, 0 otherwise.

**::tcl::build-info objective-cplusplus**
: Returns 1 if Tcl is compiled with an objective-c++ compiler, 0 otherwise.

**::tcl::build-info patchlevel**
: Returns the Tcl patchlevel, same as [info patchlevel][info].

**::tcl::build-info profile**
: Returns 1 if Tcl is compiled with **-DTCL\_CFG\_PROFILED**, 0 otherwise.

**::tcl::build-info purify**
: Returns 1 if Tcl is compiled with **-DPURIFY**, 0 otherwise.

**::tcl::build-info static**
: Returns 1 if Tcl is compiled as a static library, 0 otherwise.

**::tcl::build-info tommath**
: Returns the libtommath version number (as 4 digits) if libtommath is built into Tcl, 0 otherwise.

**::tcl::build-info version**
: Returns the Tcl version, same as [info tclversion][info].

**::tcl::build-info zlib**
: Returns the zlib version number (as 4 digits) if zlib is built into Tcl, 0 otherwise.


# Examples

These show the use of **::tcl::build-info**.

```
::tcl::build-info
     → 9.0.2+af16c07b81655fabde8028374161ad54b84ef9956843c63f49976b4ef601b611.gcc-1204
::tcl::build-info commit
     → af16c07b81655fabde8028374161ad54b84ef9956843c63f49976b4ef601b611
::tcl::build-info compiler
     → gcc-1204
::tcl::build-info gcc
     → 1204
::tcl::build-info version
     → 9.0
::tcl::build-info patchlevel
     → 9.0.2
```


[info]: info.md

