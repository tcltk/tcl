---
CommandName: tm
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - package(n)
 - Tcl Improvement Proposal #189
Keywords:
 - modules
 - package
Copyright:
 - Copyright (c) 2004-2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
---

# Name

tm - Facilities for locating and loading of Tcl Modules

# Synopsis

::: {.synopsis} :::
[::tcl::tm::path]{.cmd} [add]{.sub} [path]{.optdot}
[::tcl::tm::path]{.cmd} [remove]{.sub} [path]{.optdot}
[::tcl::tm::path]{.cmd} [list]{.sub}
[::tcl::tm::roots]{.cmd} [paths]{.arg}
:::

# Description

This document describes the facilities for locating and loading Tcl Modules (see \fBMODULE DEFINITION\fR for the definition of a Tcl Module). The following commands are supported:

**::tcl::tm::path add** ?\fIpath ...\fR?
: The paths are added at the head to the list of module paths, in order of appearance. This means that the last argument ends up as the new head of the list.
    The command enforces the restriction that no path may be an ancestor directory of any other path on the list. If any of the new paths violates this restriction an error will be raised, before any of the paths have been added. In other words, if only one path argument violates the restriction then none will be added.
    If a path is already present as is, no error will be raised and no action will be taken.
    Paths are searched later in the order of their appearance in the list. As they are added to the front of the list they are searched in reverse order of addition. In other words, the paths added last are looked at first.

**::tcl::tm::path remove** ?\fIpath ...\fR?
: Removes the paths from the list of module paths. The command silently ignores all paths which are not on the list.

**::tcl::tm::path list**
: Returns a list containing all registered module paths, in the order that they are searched for modules.

**::tcl::tm::roots** \fIpaths\fR
: Similar to \fBpath add\fR, and layered on top of it. This command takes a single argument containing a list of paths, extends each with "**tcl***X***/site-tcl**", and "**tcl***X***/***X***.***y*", for major version \fIX\fR of the Tcl interpreter and minor version \fIy\fR less than or equal to the minor version of the interpreter, and adds the resulting set of paths to the list of paths to search.
    This command is used internally by the system to set up the system-specific default paths.
    The command has been exposed to allow a build system to define additional root paths beyond those described by this document.


# Module definition

A Tcl Module is a Tcl Package contained in a single file, and no other files required by it. This file has to be \fBsource\fRable. In other words, a Tcl Module is always imported via:

```
source module_file
```

The \fBload\fR command is not directly used. This restriction is not an actual limitation, as some may believe. Ever since 8.4 the Tcl \fBsource\fR command reads only until the first ^Z character. This allows us to combine an arbitrary Tcl script with arbitrary binary data into one file, where the script processes the attached data in any it chooses to fully import and activate the package.

The name of a module file has to match the regular expression:

```
([_[:alpha:]][:_[:alnum:]]*)-([[:digit:]].*)\.tm
```

The first capturing parentheses provides the name of the package, the second clause its version. In addition to matching the pattern, the extracted version number must not raise an error when used in the command:

```
package vcompare $version 0
```

# Finding modules

The directory tree for storing Tcl modules is separate from other parts of the filesystem and independent of \fBauto_path\fR.

Tcl Modules are searched for in all directories listed in the result of the command \fB::tcl::tm::path list\fR. This is called the \fIModule path\fR. Neither the \fBauto_path\fR nor the \fBtcl_pkgPath\fR variables are used. All directories on the module path have to obey one restriction:

For any two directories, neither is an ancestor directory of the other.

This is required to avoid ambiguities in package naming. If for example the two directories "*foo/*" and "*foo/cool*" were on the path a package named \fBcool::ice\fR could be found via the names \fBcool::ice\fR or \fBice\fR, the latter potentially obscuring a package named \fBice\fR, unqualified.

Before the search is started, the name of the requested package is translated into a partial path, using the following algorithm:

All occurrences of "**::**" in the package name are replaced by the appropriate directory separator character for the platform we are on. On Unix, for example, this is "**/**".

Example:

The requested package is \fBencoding::base64\fR. The generated partial path is "*encoding/base64*".

After this translation the package is looked for in all module paths, by combining them one-by-one, first to last with the partial path to form a complete search pattern. Note that the search algorithm rejects all files where the filename does not match the regular expression given in the section \fBMODULE DEFINITION\fR. For the remaining files \fIprovide scripts\fR are generated and added to the package ifneeded database.

The algorithm falls back to the previous unknown handler when none of the found module files satisfy the request. If the request was satisfied the fall-back is ignored.

Note that packages in module form have \fIno\fR control over the \fIindex\fR and \fIprovide script\fRs entered into the package database for them. For a module file \fBMF\fR the \fIindex script\fR is always:

```
package ifneeded PNAME PVERSION [list source MF]
```

and the \fIprovide script\fR embedded in the above is:

```
source MF
```

Both package name \fBPNAME\fR and package version \fBPVERSION\fR are extracted from the filename \fBMF\fR according to the definition below:

```
MF = /module_path/PNAME\(fm-PVERSION.tm
```

Where \fBPNAME\(fm\fR is the partial path of the module as defined in section \fBFINDING MODULES\fR, and translated into \fBPNAME\fR by changing all directory separators to "**::**", and \fBmodule_path\fR is the path (from the list of paths to search) that we found the module file under.

Note also that we are here creating a connection between package names and paths. Tcl is case-sensitive when it comes to comparing package names, but there are filesystems which are not, like NTFS. Luckily these filesystems do store the case of the name, despite not using the information when comparing.

Given the above we allow the names for packages in Tcl modules to have mixed-case, but also require that there are no collisions when comparing names in a case-insensitive manner. In other words, if a package \fBFoo\fR is deployed in the form of a Tcl Module, packages like \fBfoo\fR, \fBfOo\fR, etc. are not allowed anymore.

# Default paths

The default list of paths on the module path is computed by a \fBtclsh\fR as follows, where \fIX\fR is the major version of the Tcl interpreter and \fIy\fR is less than or equal to the minor version of the Tcl interpreter.

All the default paths are added to the module path, even those paths which do not exist. Non-existent paths are filtered out during actual searches. This enables a user to create one of the paths searched when needed and all running applications will automatically pick up any modules placed in them.

The paths are added in the order as they are listed below, and for lists of paths defined by an environment variable in the order they are found in the variable.

## System specific paths

**file normalize [info library]/../tcl***X***/***X***.***y*
: In other words, the interpreter will look into a directory specified by its major version and whose minor versions are less than or equal to the minor version of the interpreter.
    For example for Tcl 8.4 the paths searched are:

    ```
    [info library]/../tcl8/8.4
    [info library]/../tcl8/8.3
    [info library]/../tcl8/8.2
    [info library]/../tcl8/8.1
    [info library]/../tcl8/8.0
    ```
    This definition assumes that a package defined for Tcl \fIX\fR\fB.\fR\fIy\fR can also be used by all interpreters which have the same major number \fIX\fR and a minor number greater than \fIy\fR.

**file normalize EXEC/tcl***X***/***X***.***y*
: Where \fBEXEC\fR is \fBfile normalize [info nameofexecutable]/../lib\fR or \fBfile normalize [::tcl::pkgconfig get libdir,runtime]\fR
    This sets of paths is handled equivalently to the set coming before, except that it is anchored in \fBEXEC_PREFIX\fR. For a build with \fBPREFIX\fR = \fBEXEC_PREFIX\fR the two sets are identical.


## Site specific paths

**file normalize [info library]/../tcl***X***/site-tcl**
: Note that this is always a single entry because \fIX\fR is always a specific value (the current major version of Tcl).


## User specific paths

**$::env(TCL***X***_***y***_TM_PATH)**
: A list of paths, separated by either \fB:\fR (Unix) or \fB;\fR (Windows). This is user and site specific as this environment variable can be set not only by the user's profile, but by system configuration scripts as well.

**$::env(TCL***X***.***y***_TM_PATH)**
: Same meaning and content as the previous variable. However the use of dot '.' to separate major and minor version number makes this name less to non-portable and its use is discouraged. Support of this variable has been kept only for backward compatibility with the original specification, i.e. TIP 189.


These paths are seen and therefore shared by all Tcl shells in the \fB$::env(PATH)\fR of the user.

Note that \fIX\fR and \fIy\fR follow the general rules set out above. In other words, Tcl 9.1, for example, will look at these 4 environment variables:

```
$::env(TCL9.1_TM_PATH)  $::env(TCL9_1_TM_PATH)
$::env(TCL9.0_TM_PATH)  $::env(TCL9_0_TM_PATH)
```

Paths initialized from the environment variables undergo tilde substitution (see \fBfilename\fR). Any path whose tilde substitution fails because the user is unknown will be omitted from search paths.

"*Tcl Modules*" (online at https://core.tcl-lang.org/tips/doc/trunk/tip/189.md), Tcl Improvement Proposal #190 "*Implementation Choices for Tcl Modules*" (online at https://core.tcl-lang.org/tips/doc/trunk/tip/190.md)

