---
CommandName: pkg_mkIndex
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
 - Copyright (c) 1996 Sun Microsystems, Inc.
---

# Name

pkg\_mkIndex - Build an index for automatic loading of packages

# Synopsis

::: {.synopsis} :::
[pkg\_mkIndex]{.cmd} [options...]{.optarg} [dir]{.arg} [pattern pattern]{.optdot}
:::

# Description

**Pkg\_mkIndex** is a utility procedure that is part of the standard Tcl library. It is used to create index files that allow packages to be loaded automatically when [package require][package] commands are executed. To use **pkg\_mkIndex**, follow these steps:

1. Create the package(s). Each package may consist of one or more Tcl script files or binary files. Binary files must be suitable for loading with the [load] command with a single argument;  for example, if the file is **test.so** it must be possible to load this file with the command [load test.so][load]. Each script file must contain a [package provide][package] command to declare the package and version number, and each binary file must contain a call to **Tcl\_PkgProvide**.

2. Create the index by invoking **pkg\_mkIndex**. The *dir* argument gives the name of a directory and each *pattern* argument is a [glob]-style pattern that selects script or binary files in *dir*. The default pattern is **\*.tcl** and **\*.[info sharedlibextension]**.
**Pkg\_mkIndex** will create a file **pkgIndex.tcl** in *dir* with package information about all the files given by the *pattern* arguments. It does this by loading each file into a child interpreter and seeing what packages and new commands appear (this is why it is essential to have [package provide][package] commands or **Tcl\_PkgProvide** calls in the files, as described above). If you have a package split among scripts and binary files, or if you have dependencies among files, you may have to use the **-load** option or adjust the order in which **pkg\_mkIndex** processes the files.  See [Complex cases] below.

3. Install the package as a subdirectory of one of the directories given by the **tcl\_pkgPath** variable.  If **$tcl\_pkgPath** contains more than one directory, machine-dependent packages (e.g., those that contain binary shared libraries) should normally be installed under the first directory and machine-independent packages (e.g., those that contain only Tcl scripts) should be installed under the second directory. The subdirectory should include the package's script and/or binary files as well as the **pkgIndex.tcl** file.  As long as the package is installed as a subdirectory of a directory in **$tcl\_pkgPath** it will automatically be found during [package require][package] commands.
If you install the package anywhere else, then you must ensure that the directory containing the package is in the **auto\_path** global variable or an immediate subdirectory of one of the directories in **auto\_path**. **Auto\_path** contains a list of directories that are searched by both the auto-loader and the package loader; by default it includes **$tcl\_pkgPath**. The package loader also checks all of the subdirectories of the directories in **auto\_path**. You can add a directory to **auto\_path** explicitly in your application, or you can add the directory to your **TCLLIBPATH** environment variable:  if this environment variable is present, Tcl initializes **auto\_path** from it during application startup.

4. Once the above steps have been taken, all you need to do to use a package is to invoke [package require][package]. For example, if versions 2.1, 2.3, and 3.1 of package **Test** have been indexed by **pkg\_mkIndex**, the command [package require Test][package] will make version 3.1 available and the command [package require -exact Test 2.1][package] will make version 2.1 available. There may be many versions of a package in the various index files in **auto\_path**, but only one will actually be loaded in a given interpreter, based on the first call to [package require][package]. Different versions of a package may be loaded in different interpreters.


# Options

The optional switches are:

[-direct]{.lit}
: The generated index will implement direct loading of the package upon [package require][package].  This is the default.

[-lazy]{.lit}
: The generated index will manage to delay loading the package until the use of one of the commands provided by the package, instead of loading it immediately upon [package require][package].  This is not compatible with the use of *auto\_reset*, and therefore its use is discouraged.

[-load]{.lit} [pkgPat]{.arg}
: The index process will preload any packages that exist in the current interpreter and match *pkgPat* into the child interpreter used to generate the index.  The pattern match uses string match rules, but without making case distinctions. See [Complex cases] below.

[-verbose]{.lit}
: Generate output during the indexing process.  Output is via the [tclLog] procedure, which by default prints to stderr.

[--]{.lit}
: End of the flags, in case *dir* begins with a dash.


# Packages and the auto-loader

The package management facilities overlap somewhat with the auto-loader, in that both arrange for files to be loaded on-demand. However, package management is a higher-level mechanism that uses the auto-loader for the last step in the loading process. It is generally better to index a package with **pkg\_mkIndex** rather than **auto\_mkindex** because the package mechanism provides version control:  several versions of a package can be made available in the index files, with different applications using different versions based on [package require][package] commands. In contrast, **auto\_mkindex** does not understand versions so it can only handle a single version of each package. It is probably not a good idea to index a given package with both **pkg\_mkIndex** and **auto\_mkindex**. If you use **pkg\_mkIndex** to index a package, its commands cannot be invoked until [package require][package] has been used to select a version;  in contrast, packages indexed with **auto\_mkindex** can be used immediately since there is no version control.

# How it works

**Pkg\_mkIndex** depends on the [package unknown][package] command, the [package ifneeded][package] command, and the auto-loader. The first time a [package require][package] command is invoked, the [package unknown][package] script is invoked. This is set by Tcl initialization to a script that evaluates all of the **pkgIndex.tcl** files in the **auto\_path**. The **pkgIndex.tcl** files contain [package ifneeded][package] commands for each version of each available package;  these commands invoke [package provide][package] commands to announce the availability of the package, and they setup auto-loader information to load the files of the package. If the **-lazy** flag was provided when the **pkgIndex.tcl** was generated, a given file of a given version of a given package is not actually loaded until the first time one of its commands is invoked. Thus, after invoking [package require][package] you may not see the package's commands in the interpreter, but you will be able to invoke the commands and they will be auto-loaded.

# Direct loading

Some packages, for instance packages which use namespaces and export commands or those which require special initialization, might select that their package files be loaded immediately upon [package require][package] instead of delaying the actual loading to the first use of one of the package's command. This is the default mode when generating the package index.  It can be overridden by specifying the **-lazy** argument.

# Complex cases

Most complex cases of dependencies among scripts and binary files, and packages being split among scripts and binary files are handled OK.  However, you may have to adjust the order in which files are processed by **pkg\_mkIndex**. These issues are described in detail below.

If each script or file contains one package, and packages are only contained in one file, then things are easy. You simply specify all files to be indexed in any order with some glob patterns.

In general, it is OK for scripts to have dependencies on other packages. If scripts contain [package require][package] commands, these are stubbed out in the interpreter used to process the scripts, so these do not cause problems. If scripts call into other packages in global code, these calls are handled by a stub [unknown] command. However, if scripts make variable references to other package's variables in global code, these will cause errors.  That is also bad coding style.

If binary files have dependencies on other packages, things can become tricky because it is not possible to stub out C-level APIs such as **Tcl\_PkgRequire** API when loading a binary file. For example, suppose the BLT package requires Tk, and expresses this with a call to **Tcl\_PkgRequire** in its **Blt\_Init** routine. To support this, you must run **pkg\_mkIndex** in an interpreter that has Tk loaded.  You can achieve this with the **-load** *pkgPat* option.  If you specify this option, **pkg\_mkIndex** will load any packages listed by [info loaded][info] and that match *pkgPat* into the interpreter used to process files. In most cases this will satisfy the **Tcl\_PkgRequire** calls made by binary files.

If you are indexing two binary files and one depends on the other, you should specify the one that has dependencies last. This way the one without dependencies will get loaded and indexed, and then the package it provides will be available when the second file is processed. You may also need to load the first package into the temporary interpreter used to create the index by using the **-load** flag; it will not hurt to specify package patterns that are not yet loaded.

If you have a package that is split across scripts and a binary file, then you should avoid the **-load** flag. The problem is that if you load a package before computing the index it masks any other files that provide part of the same package. If you must use **-load**, then you must specify the scripts first; otherwise the package loaded from the binary file may mask the package defined by the scripts.


[glob]: glob.md
[info]: info.md
[load]: load.md
[package]: package.md
[tclLog]: tclLog.md
[unknown]: unknown.md

