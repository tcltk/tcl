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

This document describes the facilities for locating and loading Tcl Modules (see **MODULE DEFINITION** for the definition of a Tcl Module). The following commands are supported:

**::tcl::tm::path add** ?*path ...*?
: The paths are added at the head to the list of module paths, in order of appearance. This means that the last argument ends up as the new head of the list.
    The command enforces the restriction that no path may be an ancestor directory of any other path on the list. If any of the new paths violates this restriction an error will be raised, before any of the paths have been added. In other words, if only one path argument violates the restriction then none will be added.
    If a path is already present as is, no error will be raised and no action will be taken.
    Paths are searched later in the order of their appearance in the list. As they are added to the front of the list they are searched in reverse order of addition. In other words, the paths added last are looked at first.

**::tcl::tm::path remove** ?*path ...*?
: Removes the paths from the list of module paths. The command silently ignores all paths which are not on the list.

**::tcl::tm::path list**
: Returns a list containing all registered module paths, in the order that they are searched for modules.

**::tcl::tm::roots** *paths*
: Similar to **path add**, and layered on top of it. This command takes a single argument containing a list of paths, extends each with "**tcl***X***/site-tcl**", and "**tcl***X***/***X***.***y*", for major version *X* of the Tcl interpreter and minor version *y* less than or equal to the minor version of the interpreter, and adds the resulting set of paths to the list of paths to search.
    This command is used internally by the system to set up the system-specific default paths.
    The command has been exposed to allow a build system to define additional root paths beyond those described by this document.


# Module definition

A Tcl Module is a Tcl Package contained in a single file, and no other files required by it. This file has to be **source**able. In other words, a Tcl Module is always imported via:

```
source module_file
```

The **load** command is not directly used. This restriction is not an actual limitation, as some may believe. Ever since 8.4 the Tcl **source** command reads only until the first ^Z character. This allows us to combine an arbitrary Tcl script with arbitrary binary data into one file, where the script processes the attached data in any it chooses to fully import and activate the package.

The name of a module file has to match the regular expression:

```
([_[:alpha:]][:_[:alnum:]]*)-([[:digit:]].*)\.tm
```

The first capturing parentheses provides the name of the package, the second clause its version. In addition to matching the pattern, the extracted version number must not raise an error when used in the command:

```
package vcompare $version 0
```

# Finding modules

The directory tree for storing Tcl modules is separate from other parts of the filesystem and independent of **auto_path**.

Tcl Modules are searched for in all directories listed in the result of the command **::tcl::tm::path list**. This is called the *Module path*. Neither the **auto_path** nor the **tcl_pkgPath** variables are used. All directories on the module path have to obey one restriction:

For any two directories, neither is an ancestor directory of the other.

This is required to avoid ambiguities in package naming. If for example the two directories "*foo/*" and "*foo/cool*" were on the path a package named **cool::ice** could be found via the names **cool::ice** or **ice**, the latter potentially obscuring a package named **ice**, unqualified.

Before the search is started, the name of the requested package is translated into a partial path, using the following algorithm:

All occurrences of "**::**" in the package name are replaced by the appropriate directory separator character for the platform we are on. On Unix, for example, this is "**/**".

Example:

The requested package is **encoding::base64**. The generated partial path is "*encoding/base64*".

After this translation the package is looked for in all module paths, by combining them one-by-one, first to last with the partial path to form a complete search pattern. Note that the search algorithm rejects all files where the filename does not match the regular expression given in the section **MODULE DEFINITION**. For the remaining files *provide scripts* are generated and added to the package ifneeded database.

The algorithm falls back to the previous unknown handler when none of the found module files satisfy the request. If the request was satisfied the fall-back is ignored.

Note that packages in module form have *no* control over the *index* and *provide script*s entered into the package database for them. For a module file **MF** the *index script* is always:

```
package ifneeded PNAME PVERSION [list source MF]
```

and the *provide script* embedded in the above is:

```
source MF
```

Both package name **PNAME** and package version **PVERSION** are extracted from the filename **MF** according to the definition below:

```
MF = /module_path/PNAME\(fm-PVERSION.tm
```

Where **PNAME\(fm** is the partial path of the module as defined in section **FINDING MODULES**, and translated into **PNAME** by changing all directory separators to "**::**", and **module_path** is the path (from the list of paths to search) that we found the module file under.

Note also that we are here creating a connection between package names and paths. Tcl is case-sensitive when it comes to comparing package names, but there are filesystems which are not, like NTFS. Luckily these filesystems do store the case of the name, despite not using the information when comparing.

Given the above we allow the names for packages in Tcl modules to have mixed-case, but also require that there are no collisions when comparing names in a case-insensitive manner. In other words, if a package **Foo** is deployed in the form of a Tcl Module, packages like **foo**, **fOo**, etc. are not allowed anymore.

# Default paths

The default list of paths on the module path is computed by a **tclsh** as follows, where *X* is the major version of the Tcl interpreter and *y* is less than or equal to the minor version of the Tcl interpreter.

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
    This definition assumes that a package defined for Tcl *X***.***y* can also be used by all interpreters which have the same major number *X* and a minor number greater than *y*.

**file normalize EXEC/tcl***X***/***X***.***y*
: Where **EXEC** is **file normalize [info nameofexecutable]/../lib** or **file normalize [::tcl::pkgconfig get libdir,runtime]**
    This sets of paths is handled equivalently to the set coming before, except that it is anchored in **EXEC_PREFIX**. For a build with **PREFIX** = **EXEC_PREFIX** the two sets are identical.


## Site specific paths

**file normalize [info library]/../tcl***X***/site-tcl**
: Note that this is always a single entry because *X* is always a specific value (the current major version of Tcl).


## User specific paths

**$::env(TCL***X***_***y***_TM_PATH)**
: A list of paths, separated by either **:** (Unix) or **;** (Windows). This is user and site specific as this environment variable can be set not only by the user's profile, but by system configuration scripts as well.

**$::env(TCL***X***.***y***_TM_PATH)**
: Same meaning and content as the previous variable. However the use of dot '.' to separate major and minor version number makes this name less to non-portable and its use is discouraged. Support of this variable has been kept only for backward compatibility with the original specification, i.e. TIP 189.


These paths are seen and therefore shared by all Tcl shells in the **$::env(PATH)** of the user.

Note that *X* and *y* follow the general rules set out above. In other words, Tcl 9.1, for example, will look at these 4 environment variables:

```
$::env(TCL9.1_TM_PATH)  $::env(TCL9_1_TM_PATH)
$::env(TCL9.0_TM_PATH)  $::env(TCL9_0_TM_PATH)
```

Paths initialized from the environment variables undergo tilde substitution (see **filename**). Any path whose tilde substitution fails because the user is unknown will be omitted from search paths.

"*Tcl Modules*" (online at https://core.tcl-lang.org/tips/doc/trunk/tip/189.md), Tcl Improvement Proposal #190 "*Implementation Choices for Tcl Modules*" (online at https://core.tcl-lang.org/tips/doc/trunk/tip/190.md)

