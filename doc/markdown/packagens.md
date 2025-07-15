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

**::pkg::create** is a utility procedure that is part of the standard Tcl library.  It is used to create an appropriate \fBpackage ifneeded\fR command for a given package specification.  It can be used to construct a \fBpkgIndex.tcl\fR file for use with the \fBpackage\fR mechanism.

# Options

The parameters supported are:

**-name** \fIpackageName\fR
: This parameter specifies the name of the package.  It is required.

**-version** \fIpackageVersion\fR
: This parameter specifies the version of the package.  It is required.

**-load** \fIfilespec\fR
: This parameter specifies a library that must be loaded with the \fBload\fR command.  \fIfilespec\fR is a list with two elements.  The first element is the name of the file to load.  The second, optional element is a list of commands supplied by loading that file.  If the list of procedures is empty or omitted, \fB::pkg::create\fR will set up the library for direct loading (see \fBpkg_mkIndex\fR).  Any number of \fB-load\fR parameters may be specified.

**-source** \fIfilespec\fR
: This parameter is similar to the \fB-load\fR parameter, except that it specifies a Tcl library that must be loaded with the \fBsource\fR command.  Any number of \fB-source\fR parameters may be specified.


At least one \fB-load\fR or \fB-source\fR parameter must be given.

