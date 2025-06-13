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

This command keeps a simple database of the packages available for use by the current interpreter and how to load them into the interpreter. It supports multiple versions of each package and arranges for the correct version of a package to be loaded based on what is needed by the application. This command also detects and reports version clashes. Typically, only the **package require** and **package provide** commands are invoked in normal Tcl scripts;  the other commands are used primarily by system scripts that maintain the package database.

The behavior of the **package** command is determined by its first argument. The following forms are permitted:

**package files** *package*
: Lists all files forming part of *package*. Auto-loaded files are not included in this list, only files which were directly sourced during package initialization. The list order corresponds with the order in which the files were sourced.

**package forget** ?*package package ...*?
: Removes all information about each specified package from this interpreter, including information provided by both **package ifneeded** and **package provide**.

**package ifneeded** *package version* ?*script*?
: This command typically appears only in system configuration scripts to set up the package database. It indicates that a particular version of a particular package is available if needed, and that the package can be added to the interpreter by executing *script*. The script is saved in a database for use by subsequent **package require** commands;  typically, *script* sets up auto-loading for the commands in the package (or calls **load** and/or **source** directly), then invokes **package provide** to indicate that the package is present. There may be information in the database for several different versions of a single package. If the database already contains information for *package* and *version*, the new *script* replaces the existing one. If the *script* argument is omitted, the current script for version *version* of package *package* is returned, or an empty string if no **package ifneeded** command has been invoked for this *package* and *version*.

**package names**
: Returns a list of the names of all packages in the interpreter for which a version has been provided (via **package provide**) or for which a **package ifneeded** script is available. The order of elements in the list is arbitrary.

**package present** ?**-exact**? *package* ?*requirement...*?
: This command is equivalent to **package require** except that it does not try and load the package if it is not already loaded.

**package provide** *package* ?*version*?
: This command is invoked to indicate that version *version* of package *package* is now present in the interpreter. It is typically invoked once as part of an **ifneeded** script, and again by the package itself when it is finally loaded. An error occurs if a different version of *package* has been provided by a previous **package provide** command. If the *version* argument is omitted, then the command returns the version number that is currently provided, or an empty string if no **package provide** command has been invoked for *package* in this interpreter.

**package require** *package* ?*requirement...*?
: This command is typically invoked by Tcl code that wishes to use a particular version of a particular package.  The arguments indicate which package is wanted, and the command ensures that a suitable version of the package is loaded into the interpreter. If the command succeeds, it returns the version number that is loaded;  otherwise it generates an error.
    A suitable version of the package is any version which satisfies at least one of the requirements as defined in the section **REQUIREMENT** below. If multiple versions are suitable the implementation with the highest version is chosen. This last part is additionally influenced by the selection mode set with **package prefer**.
    In the "stable" selection mode the command will select the highest stable version satisfying the requirements, if any. If no stable version satisfies the requirements, the highest unstable version satisfying the requirements will be selected.  In the "latest" selection mode the command will accept the highest version satisfying all the requirements, regardless of its stableness.
    If a version of *package* has already been provided (by invoking the **package provide** command), then its version number must satisfy the *requirement*s and the command returns immediately. Otherwise, the command searches the database of information provided by previous **package ifneeded** commands to see if an acceptable version of the package is available. If so, the script for the highest acceptable version number is evaluated in the global namespace; it must do whatever is necessary to load the package, including calling **package provide** for the package. If the **package ifneeded** database does not contain an acceptable version of the package and a **package unknown** command has been specified for the interpreter then that command is evaluated in the global namespace;  when it completes, Tcl checks again to see if the package is now provided or if there is a **package ifneeded** script for it. If all of these steps fail to provide an acceptable version of the package, then the command returns an error.

**package require -exact** *package version*
: This form of the command is used when only the given *version* of *package* is acceptable to the caller.  This command is equivalent to **package require** *package version*-*version*.

**package unknown** ?*command*?
: This command supplies a "last resort" command to invoke during **package require** if no suitable version of a package can be found in the **package ifneeded** database. If the *command* argument is supplied, it contains the first part of a command;  when the command is invoked during a **package require** command, Tcl appends one or more additional arguments giving the desired package name and requirements. For example, if *command* is **foo bar** and later the command **package require test 2.4** is invoked, then Tcl will execute the command **foo bar test 2.4** to load the package. If no requirements are supplied to the **package require** command, then only the name will be added to invoked command. If the **package unknown** command is invoked without a *command* argument, then the current **package unknown** script is returned, or an empty string if there is none. If *command* is specified as an empty string, then the current **package unknown** script is removed, if there is one.

**package vcompare** *version1 version2*
: Compares the two version numbers given by *version1* and *version2*. Returns -1 if *version1* is an earlier version than *version2*, 0 if they are equal, and 1 if *version1* is later than *version2*.

**package versions** *package*
: Returns a list of all the version numbers of *package* for which information has been provided by **package ifneeded** commands.

**package vsatisfies** *version requirement...*
: Returns 1 if the *version* satisfies at least one of the given requirements, and 0 otherwise. *requirements* are defined in the **REQUIREMENT** section below.

**package prefer** ?**latest**|**stable**?
: With no arguments, the commands returns either "latest" or "stable", whichever describes the current mode of selection logic used by **package require**.
    When passed the argument "latest", it sets the selection logic mode to "latest".
    When passed the argument "stable", if the mode is already "stable", that value is kept.  If the mode is already "latest", then the attempt to set it back to "stable" is ineffective and the mode value remains "latest".
    When passed any other value as an argument, raise an invalid argument error.
    When an interpreter is created, its initial selection mode value is set to "stable" unless the environment variable **TCL_PKG_PREFER_LATEST** is set (to any value) or the Tcl package itself is unstable. Otherwise the initial (and permanent) selection mode value is set to "latest".


# Version numbers

Version numbers consist of one or more decimal numbers separated by dots, such as 2 or 1.162 or 3.1.13.1. The first number is called the major version number. Larger numbers correspond to later versions of a package, with leftmost numbers having greater significance. For example, version 2.1 is later than 1.3 and version 3.4.6 is later than 3.3.5. Missing fields are equivalent to zeroes:  version 1.3 is the same as version 1.3.0 and 1.3.0.0, so it is earlier than 1.3.1 or 1.3.0.2. In addition, the letters "a" (alpha) and/or "b" (beta) may appear exactly once to replace a dot for separation. These letters semantically add a negative specifier into the version, where "a" is -2, and "b" is -1. Each may be specified only once, and "a" or "b" are mutually exclusive in a specifier. Thus 1.3a1 becomes (semantically) 1.3.-2.1, 1.3b1 is 1.3.-1.1. Negative numbers are not directly allowed in version specifiers. A version number not containing the letters "a" or "b" as specified above is called a **stable** version, whereas presence of the letters causes the version to be called is **unstable**. A later version number is assumed to be upwards compatible with an earlier version number as long as both versions have the same major version number. For example, Tcl scripts written for version 2.3 of a package should work unchanged under versions 2.3.2, 2.4, and 2.5.1. Changes in the major version number signify incompatible changes: if code is written to use version 2.1 of a package, it is not guaranteed to work unmodified with either version 1.7.3 or version 3.1.

# Package indices

The recommended way to use packages in Tcl is to invoke **package require** and **package provide** commands in scripts, and use the procedure **pkg_mkIndex** to create package index files. Once you have done this, packages will be loaded automatically in response to **package require** commands. See the documentation for **pkg_mkIndex** for details.

# Requirement

A *requirement* string checks, if a compatible version number of a package is present. Most commands accept a list of requirement strings where the highest suitable version is matched.

Each *requirement* is allowed to have any of the forms:

*min*
: This form is called "min-bounded".

*min***-**
: This form is called "min-unbound".

*min***-***max*
: This form is called "bounded".


where "*min*" and "*max*" are valid version numbers. The legacy syntax is a special case of the extended syntax, keeping backward compatibility. Regarding satisfaction the rules are:

1. The *version* has to pass at least one of the listed *requirement*s to be satisfactory.

2. A version satisfies a "bounded" requirement when

[a]
: For *min* equal to the *max* if, and only if the *version* is equal to the *min*.

[b]
: Otherwise if, and only if the *version* is greater than or equal to the *min*, and less than the *max*, where both *min* and *max* have been padded internally with "a0". Note that while the comparison to *min* is inclusive, the comparison to *max* is exclusive.


3. A "min-bounded" requirement is a "bounded" requirement in disguise, with the *max* part implicitly specified as the next higher major version number of the *min* part. A version satisfies it per the rules above.

4. A *version* satisfies a "min-unbound" requirement if, and only if it is greater than or equal to the *min*, where the *min* has been padded internally with "a0". There is no constraint to a maximum.


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

