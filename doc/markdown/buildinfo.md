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

[::tcl::build-info]{.cmd} [commit]{.sub}
: Returns the fossil commit-id where Tcl was built from.

[::tcl::build-info]{.cmd} [compiledebug]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_COMPILE_DEBUG**, 0 otherwise.

[::tcl::build-info]{.cmd} [compiler]{.sub}
: Returns the compiler name (either clang, gcc, icc or msvc), followed by a dash and a (4-digit) version number.

[::tcl::build-info]{.cmd} [compilestats]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_COMPILE_STATS**, 0 otherwise.

[::tcl::build-info]{.cmd} [cplusplus]{.sub}
: Returns 1 if Tcl is compiled with a C++ compiler, 0 otherwise.

[::tcl::build-info]{.cmd} [debug]{.sub}
: Returns 1 if Tcl is not compiled with **-DNDEBUG**, 0 otherwise.

[::tcl::build-info]{.cmd} [gcc]{.sub}
: Returns the gcc version number (as 4 digits) if Tcl is compiled with gcc, 0 otherwise.

[::tcl::build-info]{.cmd} [icc]{.sub}
: Returns the icc version number (as 4 digits) if Tcl is compiled with icc, 0 otherwise.

[::tcl::build-info]{.cmd} [ilp32]{.sub}
: Returns 1 if Tcl is compiled such that integers, longs and pointers are all 32-bit, 0 otherwise.

[::tcl::build-info]{.cmd} [memdebug]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_MEM_DEBUG**, 0 otherwise.

[::tcl::build-info]{.cmd} [msvc]{.sub}
: Returns the msvc version number (as 4 digits) if Tcl is compiled with msvc, 0 otherwise.

[::tcl::build-info]{.cmd} [nmake]{.sub}
: Returns 1 if Tcl is built using nmake, 0 otherwise.

[::tcl::build-info]{.cmd} [no-deprecate]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_NO_DEPRECATED**, 0 otherwise.

[::tcl::build-info]{.cmd} [no-thread]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_THREADS=0**, 0 otherwise.

[::tcl::build-info]{.cmd} [no-optimize]{.sub}
: Returns 1 if Tcl is not compiled with **-DTCL_CFG_OPTIMIZED**, 0 otherwise.

[::tcl::build-info]{.cmd} [objective-c]{.sub}
: Returns 1 if Tcl is compiled with an objective-c compiler, 0 otherwise.

[::tcl::build-info]{.cmd} [objective-cplusplus]{.sub}
: Returns 1 if Tcl is compiled with an objective-c++ compiler, 0 otherwise.

[::tcl::build-info]{.cmd} [patchlevel]{.sub}
: Returns the Tcl patchlevel, same as **info patchlevel**.

[::tcl::build-info]{.cmd} [profile]{.sub}
: Returns 1 if Tcl is compiled with **-DTCL_CFG_PROFILED**, 0 otherwise.

[::tcl::build-info]{.cmd} [purify]{.sub}
: Returns 1 if Tcl is compiled with **-DPURIFY**, 0 otherwise.

[::tcl::build-info]{.cmd} [static]{.sub}
: Returns 1 if Tcl is compiled as a static library, 0 otherwise.

[::tcl::build-info]{.cmd} [tommath]{.sub}
: Returns the libtommath version number (as 4 digits) if libtommath is built into Tcl, 0 otherwise.

[::tcl::build-info]{.cmd} [version]{.sub}
: Returns the Tcl version, same as **info tclversion**.

[::tcl::build-info]{.cmd} [zlib]{.sub}
: Returns the zlib version number (as 4 digits) if zlib is built into Tcl, 0 otherwise.


# Examples

These show the use of **::tcl::build-info**.

```
::tcl::build-info
     \(-> 9.0.2+af16c07b81655fabde8028374161ad54b84ef9956843c63f49976b4ef601b611.gcc-1204
::tcl::build-info commit
     \(-> af16c07b81655fabde8028374161ad54b84ef9956843c63f49976b4ef601b611
::tcl::build-info compiler
     \(-> gcc-1204
::tcl::build-info gcc
     \(-> 1204
::tcl::build-info version
     \(-> 9.0
::tcl::build-info patchlevel
     \(-> 9.0.2
```

