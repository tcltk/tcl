---
CommandName: library
ManualSection: n
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - env(n)
 - info(n)
 - re_syntax(n)
Keywords:
 - auto-exec
 - auto-load
 - library
 - unknown
 - word
 - whitespace
Copyright:
 - Copyright (c) 1991-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

auto_execok, auto_import, auto_load, auto_mkindex, auto_qualify, auto_reset, foreachLine, parray, readFile, tcl_findLibrary, tcl_endOfWord, tcl_nonwordchars, tcl_startOfNextWord, tcl_startOfPreviousWord, tcl_wordBreakAfter, tcl_wordBreakBefore, tcl_wordchars, writeFile - standard library of Tcl procedures

# Synopsis

::: {.synopsis} :::
[auto_execok]{.cmd} [cmd]{.arg}
[auto_import]{.cmd} [pattern]{.arg}
[auto_load]{.cmd} [cmd]{.arg}
[auto_mkindex]{.cmd} [dir]{.arg} [pattern]{.arg} [pattern]{.arg} [...]{.arg}
[auto_qualify]{.cmd} [command]{.arg} [namespace]{.arg}
[auto_reset]{.cmd}
[tcl_findLibrary]{.cmd} [basename]{.arg} [version]{.arg} [patch]{.arg} [initScript]{.arg} [enVarName]{.arg} [varName]{.arg}
[parray]{.cmd} [arrayName]{.arg} [pattern]{.optarg}
[tcl_endOfWord]{.cmd} [str]{.arg} [start]{.arg}
[tcl_startOfNextWord]{.cmd} [str]{.arg} [start]{.arg}
[tcl_startOfPreviousWord]{.cmd} [str]{.arg} [start]{.arg}
[tcl_wordBreakAfter]{.cmd} [str]{.arg} [start]{.arg}
[tcl_wordBreakBefore]{.cmd} [str]{.arg} [start]{.arg}
[foreachLine]{.cmd} [filename]{.arg} [varName]{.arg} [body]{.arg}
[readFile]{.cmd} [filename]{.arg} [text=|§binary]{.optlit}
[writeFile]{.cmd} [filename]{.arg} [text=|§binary]{.optlit} [contents]{.arg}
:::

# Introduction

Tcl includes a library of Tcl procedures for commonly-needed functions. The procedures defined in the Tcl library are generic ones suitable for use by many different applications. The location of the Tcl library is returned by the **info library** command. In addition to the Tcl library, each application will normally have its own library of support procedures as well;  the location of this library is normally given by the value of the **$***app***_library** global variable, where *app* is the name of the application. For example, the location of the Tk library is kept in the variable **tk_library**.

To access the procedures in the Tcl library, an application should source the file **init.tcl** in the library, for example with the Tcl command

```
source [file join [info library] init.tcl]
```

If the library procedure **Tcl_Init** is invoked from an application's **Tcl_AppInit** procedure, this happens automatically. The code in **init.tcl** will define the **unknown** procedure and arrange for the other procedures to be loaded on-demand using the auto-load mechanism defined below.

# Command procedures

The following procedures are provided in the Tcl library:

**auto_execok** *cmd*
: Determines whether there is an executable file or shell builtin by the name *cmd*.  If so, it returns a list of arguments to be passed to **exec** to execute the executable file or shell builtin named by *cmd*.  If not, it returns an empty string.  This command examines the directories in the current search path (given by the PATH environment variable) in its search for an executable file named *cmd*.  On Windows platforms, the search is expanded with the same directories and file extensions as used by **exec**. **Auto_execok** remembers information about previous searches in an array named **auto_execs**;  this avoids the path search in future calls for the same *cmd*.  The command **auto_reset** may be used to force **auto_execok** to forget its cached information.
    For example, to run the *umask* shell builtin on Linux, you would do:

    ```
    exec {*}[auto_execok umask]
    ```
    To run the *DIR* shell builtin on Windows, you would do:

    ```
    exec {*}[auto_execok dir]
    ```
    To discover if there is a *frobnicate* binary on the user's PATH, you would do:

    ```
    set mayFrob [expr {[llength [auto_execok frobnicate]] > 0}]
    ```

**auto_import** *pattern*
: **Auto_import** is invoked during **namespace import** to see if the imported commands specified by *pattern* reside in an autoloaded library.  If so, the commands are loaded so that they will be available to the interpreter for creating the import links.  If the commands do not reside in an autoloaded library, **auto_import** does nothing.  The pattern matching is performed according to the matching rules of **namespace import**.
    It is not normally necessary to call this command directly.

**auto_load** *cmd*
: This command attempts to load the definition for a Tcl command named *cmd*.  To do this, it searches an *auto-load path*, which is a list of one or more directories.  The auto-load path is given by the global variable **auto_path** if it exists.  If there is no **auto_path** variable, then the **TCLLIBPATH** environment variable is used, if it exists.  Otherwise the auto-load path consists of just the Tcl library directory.  Within each directory in the auto-load path there must be a file **tclIndex** that describes one or more commands defined in that directory and a script to evaluate to load each of the commands.  The **tclIndex** file should be generated with the **auto_mkindex** command.  If *cmd* is found in an index file, then the appropriate script is evaluated to create the command.  The **auto_load** command returns 1 if *cmd* was successfully created.  The command returns 0 if there was no index entry for *cmd* or if the script did not actually define *cmd* (e.g. because index information is out of date).  If an error occurs while processing the script, then that error is returned. **Auto_load** only reads the index information once and saves it in the array **auto_index**;  future calls to **auto_load** check for *cmd* in the array rather than re-reading the index files.  The cached index information may be deleted with the command **auto_reset**.  This will force the next **auto_load** command to reload the index database from disk.
    It is not normally necessary to call this command directly; the default **unknown** handler will do so.

**auto_mkindex** *dir pattern pattern ...*
: Generates an index suitable for use by **auto_load**.  The command searches *dir* for all files whose names match any of the *pattern* arguments (matching is done with the **glob** command), generates an index of all the Tcl command procedures defined in all the matching files, and stores the index information in a file named **tclIndex** in *dir*. If no pattern is given a pattern of ***.tcl** will be assumed.  For example, the command

    ```
    auto_mkindex foo *.tcl
    ```
    will read all the **.tcl** files in subdirectory **foo** and generate a new index file **foo/tclIndex**.
    **Auto_mkindex** parses the Tcl scripts by sourcing them into a child interpreter and monitoring the proc and namespace commands that are executed.  Extensions can use the (undocumented) auto_mkindex_parser package to register other commands that can contribute to the auto_load index. You will have to read through auto.tcl to see how this works.
    **Auto_mkindex_old** (which has the same syntax as **auto_mkindex**) parses the Tcl scripts in a relatively unsophisticated way:  if any line contains the word "**proc**" as its first characters then it is assumed to be a procedure definition and the next word of the line is taken as the procedure's name. Procedure definitions that do not appear in this way (e.g.\ they have spaces before the **proc**) will not be indexed.  If your script contains "dangerous" code, such as global initialization code or procedure names with special characters like **$**, *****, **[** or **]**, you are safer using **auto_mkindex_old**.

**auto_reset**
: Destroys all the information cached by **auto_execok** and **auto_load**.  This information will be re-read from disk the next time it is needed.  **Auto_reset** also deletes any procedures listed in the auto-load index, so that fresh copies of them will be loaded the next time that they are used.

**auto_qualify** *command namespace*
: Computes a list of fully qualified names for *command*.  This list mirrors the path a standard Tcl interpreter follows for command lookups:  first it looks for the command in the current namespace, and then in the global namespace.  Accordingly, if *command* is relative and *namespace* is not **::**, the list returned has two elements:  *command* scoped by *namespace*, as if it were a command in the *namespace* namespace; and *command* as if it were a command in the global namespace.  Otherwise, if either *command* is absolute (it begins with **::**), or *namespace* is **::**, the list contains only *command* as if it were a command in the global namespace.
    **Auto_qualify** is used by the auto-loading facilities in Tcl, both for producing auto-loading indexes such as *pkgIndex.tcl*, and for performing the actual auto-loading of functions at runtime.

**tcl_findLibrary** *basename version patch initScript enVarName varName*
: This is a standard search procedure for use by extensions during their initialization.  They call this procedure to look for their script library in several standard directories. The last component of the name of the library directory is normally *basenameversion* (e.g., tk8.0), but it might be "library" when in the build hierarchies. The *initScript* file will be sourced into the interpreter once it is found.  The directory in which this file is found is stored into the global variable *varName*. If this variable is already defined (e.g., by C code during application initialization) then no searching is done. Otherwise the search looks in these directories: the directory named by the environment variable *enVarName*; relative to the Tcl library directory; relative to the executable file in the standard installation bin or bin/*arch* directory; relative to the executable file in the current build tree; relative to the executable file in a parallel build tree.

**parray** *arrayName* ?*pattern*?
: Prints on standard output the names and values of all the elements in the array *arrayName*, or just the names that match *pattern* (using the matching rules of **string match**) and their values if *pattern* is given. *ArrayName* must be an array accessible to the caller of **parray**. It may be either local or global. The result of this command is the empty string.
    For example, to print the contents of the **tcl_platform** array, do:

    ```
    parray tcl_platform
    ```


## Word boundary helpers

These procedures are mainly used internally by Tk.

**tcl_endOfWord** *str start*
: Returns the index of the first end-of-word location that occurs after a starting index *start* in the string *str*.  An end-of-word location is defined to be the first non-word character following the first word character after the starting point.  Returns -1 if there are no more end-of-word locations after the starting point.  See the description of **tcl_wordchars** and **tcl_nonwordchars** below for more details on how Tcl determines which characters are word characters.

**tcl_startOfNextWord** *str start*
: Returns the index of the first start-of-word location that occurs after a starting index *start* in the string *str*.  A start-of-word location is defined to be the first word character following a non-word character.  Returns -1 if there are no more start-of-word locations after the starting point.
    For example, to print the indices of the starts of each word in a string according to platform rules:

    ```
    set theString "The quick brown fox"
    for {set idx 0} {$idx >= 0} {
            set idx [tcl_startOfNextWord $theString $idx]} {
        puts "Word start index: $idx"
    }
    ```

**tcl_startOfPreviousWord** *str start*
: Returns the index of the first start-of-word location that occurs before a starting index *start* in the string *str*.  Returns -1 if there are no more start-of-word locations before the starting point.

**tcl_wordBreakAfter** *str start*
: Returns the index of the first word boundary after the starting index *start* in the string *str*.  Returns -1 if there are no more boundaries after the starting point in the given string.  The index returned refers to the second character of the pair that comprises a boundary.

**tcl_wordBreakBefore** *str start*
: Returns the index of the first word boundary before the starting index *start* in the string *str*.  Returns -1 if there are no more boundaries before the starting point in the given string.  The index returned refers to the second character of the pair that comprises a boundary.


## File access helpers

**foreachLine** *varName filename body*
: This reads in the text file named *filename* one line at a time (using system defaults for reading text files). It writes that line to the variable named by *varName* and then executes *body* for that line. The result value of *body* is ignored, but **error**, **return**, **break** and **continue** may be used within it to produce an error, return from the calling context, stop the loop, or go to the next line respectively. The overall result of **foreachLine** is the empty string (assuming no errors from I/O or from evaluating the body of the loop); the file will be closed prior to the procedure returning.

**readFile** *filename* ?**text**|**binary**?
: Reads in the file named in *filename* and returns its contents. The second argument says how to read in the file, either as **text** (using the system defaults for reading text files) or as **binary** (as uninterpreted bytes). The default is **text**. When read as text, this will include any trailing newline. The file will be closed prior to the procedure returning.

**writeFile** *filename* ?**text**|**binary**? *contents*
: Writes the *contents* to the file named in *filename*. The optional second argument says how to write to the file, either as **text** (using the system defaults for writing text files) or as **binary** (as uninterpreted bytes). The default is **text**. If a trailing newline is required, it will need to be provided in *contents*. The result of this command is the empty string; the file will be closed prior to the procedure returning.


# Variables

The following global variables are defined or used by the procedures in the Tcl library. They fall into two broad classes, handling unknown commands and packages, and determining what are words.

## Autoloading and package management variables

**auto_execs**
: Used by **auto_execok** to record information about whether particular commands exist as executable files.
    Not normally usefully accessed directly by user code.

**auto_index**
: Used by **auto_load** to save the index information read from disk.
    Not normally usefully accessed directly by user code.

**auto_noexec**
: If set to any value, then **unknown** will not attempt to auto-exec any commands.

**auto_noload**
: If set to any value, then **unknown** will not attempt to auto-load any commands.

**auto_path**
: If set, then it must contain a valid Tcl list giving directories to search during auto-load operations (including for package index files when using the default **package unknown** handler). This variable is initialized during startup to contain, in order: the directories listed in the **TCLLIBPATH** environment variable, the directory named by the **tcl_library** global variable, the parent directory of **tcl_library**, the directories listed in the **tcl_pkgPath** variable. Additional locations to look for files and package indices should normally be added to this variable using **lappend**.
    For example, to add the *lib* directory next to the running script, you would do:

    ```
    lappend auto_path [file dirname [info script]]/lib
    ```
    Note that if the script uses **cd**, it is advisable to ensure that entries on the **auto_path** are **file normalize**d.

**env(TCL_LIBRARY)**
: If set, then it specifies the location of the directory containing library scripts (the value of this variable will be assigned to the **tcl_library** variable and therefore returned by the command **info library**).  If this variable is not set then a default value is used.
    Use of this environment variable is not recommended outside of testing. Tcl installations should already know where to find their own script files, as the value is baked in during the build or installation.

**env(TCLLIBPATH)**
: If set, then it must contain a valid Tcl list giving directories to search during auto-load operations.  Directories must be specified in Tcl format, using "/" as the path separator, regardless of platform. This variable is only used when initializing the **auto_path** variable.
    A key consequence of this variable is that it gives a way to let the user of a script specify the list of places where that script may use **package require** to read packages from. It is not normally usefully settable within a Tcl script itself *except* to influence where other interpreters load from (whether made with **interp create** or launched as their own threads or subprocesses).


## Word boundary determination variables

These variables are only used in the **tcl_endOfWord**, **tcl_startOfNextWord**, **tcl_startOfPreviousWord**, **tcl_wordBreakAfter**, and **tcl_wordBreakBefore** commands.

**tcl_nonwordchars**
: This variable contains a regular expression that is used by routines like **tcl_endOfWord** to identify whether a character is part of a word or not.  If the pattern matches a character, the character is considered to be a non-word character. The default value is **\W**.

**tcl_wordchars**
: This variable contains a regular expression that is used by routines like **tcl_endOfWord** to identify whether a character is part of a word or not.  If the pattern matches a character, the character is considered to be a word character. The default value is **\w**.


