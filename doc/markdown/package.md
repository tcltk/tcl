---
CommandName: package
ManualSection: n
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - msgcat(n)
 - packagens(n)
 - pkgMkIndex(n)
Keywords:
 - package
 - version
Copyright:
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

package - Facilities for package loading and version control

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [files]{.sub} [package]{.arg}
[package]{.cmd} [forget]{.sub} [package package]{.optdot}
[package]{.cmd} [ifneeded]{.sub} [package]{.arg} [version]{.arg} [script]{.optarg}
[package]{.cmd} [names]{.sub}
[package]{.cmd} [present]{.sub} [package]{.arg} [requirement...]{.optarg}
[package]{.cmd} [present]{.sub} [-exact]{.lit} [package]{.arg} [version]{.arg}
[package]{.cmd} [provide]{.sub} [package]{.arg} [version]{.optarg}
[package]{.cmd} [require]{.sub} [package]{.arg} [requirement...]{.optarg}
[package]{.cmd} [require]{.sub} [-exact]{.lit} [package]{.arg} [version]{.arg}
[package]{.cmd} [unknown]{.sub} [command]{.optarg}
[package]{.cmd} [vcompare]{.sub} [version1]{.arg} [version2]{.arg}
[package]{.cmd} [versions]{.sub} [package]{.arg}
[package]{.cmd} [vsatisfies]{.sub} [version]{.arg} [requirement...]{.arg}
[package]{.cmd} [prefer]{.sub} [latest=|§stable]{.optlit}
:::

# Description

This command keeps a simple database of the packages available for use by the current interpreter and how to load them into the interpreter. It supports multiple versions of each package and arranges for the correct version of a package to be loaded based on what is needed by the application. This command also detects and reports version clashes. Typically, only the \fBpackage require\fR and \fBpackage provide\fR commands are invoked in normal Tcl scripts;  the other commands are used primarily by system scripts that maintain the package database.

The behavior of the \fBpackage\fR command is determined by its first argument. The following forms are permitted:

**package files** \fIpackage\fR
: Lists all files forming part of \fIpackage\fR. Auto-loaded files are not included in this list, only files which were directly sourced during package initialization. The list order corresponds with the order in which the files were sourced.

**package forget** ?\fIpackage package ...\fR?
: Removes all information about each specified package from this interpreter, including information provided by both \fBpackage ifneeded\fR and \fBpackage provide\fR.

**package ifneeded** \fIpackage version\fR ?\fIscript\fR?
: This command typically appears only in system configuration scripts to set up the package database. It indicates that a particular version of a particular package is available if needed, and that the package can be added to the interpreter by executing \fIscript\fR. The script is saved in a database for use by subsequent \fBpackage require\fR commands;  typically, \fIscript\fR sets up auto-loading for the commands in the package (or calls \fBload\fR and/or \fBsource\fR directly), then invokes \fBpackage provide\fR to indicate that the package is present. There may be information in the database for several different versions of a single package. If the database already contains information for \fIpackage\fR and \fIversion\fR, the new \fIscript\fR replaces the existing one. If the \fIscript\fR argument is omitted, the current script for version \fIversion\fR of package \fIpackage\fR is returned, or an empty string if no \fBpackage ifneeded\fR command has been invoked for this \fIpackage\fR and \fIversion\fR.

**package names**
: Returns a list of the names of all packages in the interpreter for which a version has been provided (via \fBpackage provide\fR) or for which a \fBpackage ifneeded\fR script is available. The order of elements in the list is arbitrary.

**package present** ?\fB-exact\fR? \fIpackage\fR ?\fIrequirement...\fR?
: This command is equivalent to \fBpackage require\fR except that it does not try and load the package if it is not already loaded.

**package provide** \fIpackage\fR ?\fIversion\fR?
: This command is invoked to indicate that version \fIversion\fR of package \fIpackage\fR is now present in the interpreter. It is typically invoked once as part of an \fBifneeded\fR script, and again by the package itself when it is finally loaded. An error occurs if a different version of \fIpackage\fR has been provided by a previous \fBpackage provide\fR command. If the \fIversion\fR argument is omitted, then the command returns the version number that is currently provided, or an empty string if no \fBpackage provide\fR command has been invoked for \fIpackage\fR in this interpreter.

**package require** \fIpackage\fR ?\fIrequirement...\fR?
: This command is typically invoked by Tcl code that wishes to use a particular version of a particular package.  The arguments indicate which package is wanted, and the command ensures that a suitable version of the package is loaded into the interpreter. If the command succeeds, it returns the version number that is loaded;  otherwise it generates an error.
    A suitable version of the package is any version which satisfies at least one of the requirements as defined in the section \fBREQUIREMENT\fR below. If multiple versions are suitable the implementation with the highest version is chosen. This last part is additionally influenced by the selection mode set with \fBpackage prefer\fR.
    In the "stable" selection mode the command will select the highest stable version satisfying the requirements, if any. If no stable version satisfies the requirements, the highest unstable version satisfying the requirements will be selected.  In the "latest" selection mode the command will accept the highest version satisfying all the requirements, regardless of its stableness.
    If a version of \fIpackage\fR has already been provided (by invoking the \fBpackage provide\fR command), then its version number must satisfy the \fIrequirement\fRs and the command returns immediately. Otherwise, the command searches the database of information provided by previous \fBpackage ifneeded\fR commands to see if an acceptable version of the package is available. If so, the script for the highest acceptable version number is evaluated in the global namespace; it must do whatever is necessary to load the package, including calling \fBpackage provide\fR for the package. If the \fBpackage ifneeded\fR database does not contain an acceptable version of the package and a \fBpackage unknown\fR command has been specified for the interpreter then that command is evaluated in the global namespace;  when it completes, Tcl checks again to see if the package is now provided or if there is a \fBpackage ifneeded\fR script for it. If all of these steps fail to provide an acceptable version of the package, then the command returns an error.

**package require -exact** \fIpackage version\fR
: This form of the command is used when only the given \fIversion\fR of \fIpackage\fR is acceptable to the caller.  This command is equivalent to \fBpackage require\fR \fIpackage version\fR-\fIversion\fR.

**package unknown** ?\fIcommand\fR?
: This command supplies a "last resort" command to invoke during \fBpackage require\fR if no suitable version of a package can be found in the \fBpackage ifneeded\fR database. If the \fIcommand\fR argument is supplied, it contains the first part of a command;  when the command is invoked during a \fBpackage require\fR command, Tcl appends one or more additional arguments giving the desired package name and requirements. For example, if \fIcommand\fR is \fBfoo bar\fR and later the command \fBpackage require test 2.4\fR is invoked, then Tcl will execute the command \fBfoo bar test 2.4\fR to load the package. If no requirements are supplied to the \fBpackage require\fR command, then only the name will be added to invoked command. If the \fBpackage unknown\fR command is invoked without a \fIcommand\fR argument, then the current \fBpackage unknown\fR script is returned, or an empty string if there is none. If \fIcommand\fR is specified as an empty string, then the current \fBpackage unknown\fR script is removed, if there is one.

**package vcompare** \fIversion1 version2\fR
: Compares the two version numbers given by \fIversion1\fR and \fIversion2\fR. Returns -1 if \fIversion1\fR is an earlier version than \fIversion2\fR, 0 if they are equal, and 1 if \fIversion1\fR is later than \fIversion2\fR.

**package versions** \fIpackage\fR
: Returns a list of all the version numbers of \fIpackage\fR for which information has been provided by \fBpackage ifneeded\fR commands.

**package vsatisfies** \fIversion requirement...\fR
: Returns 1 if the \fIversion\fR satisfies at least one of the given requirements, and 0 otherwise. \fIrequirements\fR are defined in the \fBREQUIREMENT\fR section below.

**package prefer** ?\fBlatest\fR|\fBstable\fR?
: With no arguments, the commands returns either "latest" or "stable", whichever describes the current mode of selection logic used by \fBpackage require\fR.
    When passed the argument "latest", it sets the selection logic mode to "latest".
    When passed the argument "stable", if the mode is already "stable", that value is kept.  If the mode is already "latest", then the attempt to set it back to "stable" is ineffective and the mode value remains "latest".
    When passed any other value as an argument, raise an invalid argument error.
    When an interpreter is created, its initial selection mode value is set to "stable" unless the environment variable \fBTCL_PKG_PREFER_LATEST\fR is set (to any value) or the Tcl package itself is unstable. Otherwise the initial (and permanent) selection mode value is set to "latest".


# Version numbers

Version numbers consist of one or more decimal numbers separated by dots, such as 2 or 1.162 or 3.1.13.1. The first number is called the major version number. Larger numbers correspond to later versions of a package, with leftmost numbers having greater significance. For example, version 2.1 is later than 1.3 and version 3.4.6 is later than 3.3.5. Missing fields are equivalent to zeroes:  version 1.3 is the same as version 1.3.0 and 1.3.0.0, so it is earlier than 1.3.1 or 1.3.0.2. In addition, the letters "a" (alpha) and/or "b" (beta) may appear exactly once to replace a dot for separation. These letters semantically add a negative specifier into the version, where "a" is -2, and "b" is -1. Each may be specified only once, and "a" or "b" are mutually exclusive in a specifier. Thus 1.3a1 becomes (semantically) 1.3.-2.1, 1.3b1 is 1.3.-1.1. Negative numbers are not directly allowed in version specifiers. A version number not containing the letters "a" or "b" as specified above is called a \fBstable\fR version, whereas presence of the letters causes the version to be called is \fBunstable\fR. A later version number is assumed to be upwards compatible with an earlier version number as long as both versions have the same major version number. For example, Tcl scripts written for version 2.3 of a package should work unchanged under versions 2.3.2, 2.4, and 2.5.1. Changes in the major version number signify incompatible changes: if code is written to use version 2.1 of a package, it is not guaranteed to work unmodified with either version 1.7.3 or version 3.1.

# Package indices

The recommended way to use packages in Tcl is to invoke \fBpackage require\fR and \fBpackage provide\fR commands in scripts, and use the procedure \fBpkg_mkIndex\fR to create package index files. Once you have done this, packages will be loaded automatically in response to \fBpackage require\fR commands. See the documentation for \fBpkg_mkIndex\fR for details.

# Requirement

A \fIrequirement\fR string checks, if a compatible version number of a package is present. Most commands accept a list of requirement strings where the highest suitable version is matched.

Each \fIrequirement\fR is allowed to have any of the forms:

*min*
: This form is called "min-bounded".

*min***-**
: This form is called "min-unbound".

*min***-***max*
: This form is called "bounded".


where "*min*" and "*max*" are valid version numbers. The legacy syntax is a special case of the extended syntax, keeping backward compatibility. Regarding satisfaction the rules are:

1. The \fIversion\fR has to pass at least one of the listed \fIrequirement\fRs to be satisfactory.

2. A version satisfies a "bounded" requirement when

[a]
: For \fImin\fR equal to the \fImax\fR if, and only if the \fIversion\fR is equal to the \fImin\fR.

[b]
: Otherwise if, and only if the \fIversion\fR is greater than or equal to the \fImin\fR, and less than the \fImax\fR, where both \fImin\fR and \fImax\fR have been padded internally with "a0". Note that while the comparison to \fImin\fR is inclusive, the comparison to \fImax\fR is exclusive.


3. A "min-bounded" requirement is a "bounded" requirement in disguise, with the \fImax\fR part implicitly specified as the next higher major version number of the \fImin\fR part. A version satisfies it per the rules above.

4. A \fIversion\fR satisfies a "min-unbound" requirement if, and only if it is greater than or equal to the \fImin\fR, where the \fImin\fR has been padded internally with "a0". There is no constraint to a maximum.


# Examples

To state that a Tcl script requires the Tk and http packages, put this at the top of the script:

```
package require Tk
package require http
```

To test to see if the Snack package is available and load if it is (often useful for optional enhancements to programs where the loss of the functionality is not critical) do this:

```
if {[catch {package require Snack}]} {
    # Error thrown - package not found.
    # Set up a dummy interface to work around the absence
} else {
    # We have the package, configure the app to use it
}
```

