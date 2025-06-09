---
CommandName: pkg::create
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - package(n)
Keywords:
 - auto-load
 - index
 - package
 - version
Copyright:
 - Copyright (c) 1998-2000 Scriptics Corporation.
---

# Name

pkg::create - Construct an appropriate 'package ifneeded' command for a given package specification

# Synopsis

::: {.synopsis} :::
[::pkg::create]{.cmd} [-name]{.lit} [packageName]{.arg} [-version]{.lit} [packageVersion]{.arg} [[-load]{.lit} [filespec]{.arg}]{.optdot} [[-source]{.lit} [filespec]{.arg}]{.optdot}
:::

# Description

**::pkg::create** is a utility procedure that is part of the standard Tcl library.  It is used to create an appropriate **package ifneeded** command for a given package specification.  It can be used to construct a **pkgIndex.tcl** file for use with the **package** mechanism.

# Options

The parameters supported are:

**-name** *packageName*
: This parameter specifies the name of the package.  It is required.

**-version** *packageVersion*
: This parameter specifies the version of the package.  It is required.

**-load** *filespec*
: This parameter specifies a library that must be loaded with the **load** command.  *filespec* is a list with two elements.  The first element is the name of the file to load.  The second, optional element is a list of commands supplied by loading that file.  If the list of procedures is empty or omitted, **::pkg::create** will set up the library for direct loading (see **pkg_mkIndex**).  Any number of **-load** parameters may be specified.

**-source** *filespec*
: This parameter is similar to the **-load** parameter, except that it specifies a Tcl library that must be loaded with the **source** command.  Any number of **-source** parameters may be specified.


At least one **-load** or **-source** parameter must be given.

