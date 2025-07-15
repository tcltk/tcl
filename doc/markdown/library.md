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

Tcl includes a library of Tcl procedures for commonly-needed functions. The procedures defined in the Tcl library are generic ones suitable for use by many different applications. The location of the Tcl library is returned by the \fBinfo library\fR command. In addition to the Tcl library, each application will normally have its own library of support procedures as well;  the location of this library is normally given by the value of the \fB$\fR\fIapp\fR\fB_library\fR global variable, where \fIapp\fR is the name of the application. For example, the location of the Tk library is kept in the variable \fBtk_library\fR.

To access the procedures in the Tcl library, an application should source the file \fBinit.tcl\fR in the library, for example with the Tcl command

```
source [file join [info library] init.tcl]
```

If the library procedure \fBTcl_Init\fR is invoked from an application's \fBTcl_AppInit\fR procedure, this happens automatically. The code in \fBinit.tcl\fR will define the \fBunknown\fR procedure and arrange for the other procedures to be loaded on-demand using the auto-load mechanism defined below.

# Command procedures

The following procedures are provided in the Tcl library:

**auto_execok** \fIcmd\fR
: Determines whether there is an executable file or shell builtin by the name \fIcmd\fR.  If so, it returns a list of arguments to be passed to \fBexec\fR to execute the executable file or shell builtin named by \fIcmd\fR.  If not, it returns an empty string.  This command examines the directories in the current search path (given by the PATH environment variable) in its search for an executable file named \fIcmd\fR.  On Windows platforms, the search is expanded with the same directories and file extensions as used by \fBexec\fR. \fBAuto_execok\fR remembers information about previous searches in an array named \fBauto_execs\fR;  this avoids the path search in future calls for the same \fIcmd\fR.  The command \fBauto_reset\fR may be used to force \fBauto_execok\fR to forget its cached information.
    For example, to run the \fIumask\fR shell builtin on Linux, you would do:

    ```
    exec {*}[auto_execok umask]
    ```
    To run the \fIDIR\fR shell builtin on Windows, you would do:

    ```
    exec {*}[auto_execok dir]
    ```
    To discover if there is a \fIfrobnicate\fR binary on the user's PATH, you would do:

    ```
    set mayFrob [expr {[llength [auto_execok frobnicate]] > 0}]
    ```

**auto_import** \fIpattern\fR
: **Auto_import** is invoked during \fBnamespace import\fR to see if the imported commands specified by \fIpattern\fR reside in an autoloaded library.  If so, the commands are loaded so that they will be available to the interpreter for creating the import links.  If the commands do not reside in an autoloaded library, \fBauto_import\fR does nothing.  The pattern matching is performed according to the matching rules of \fBnamespace import\fR.
    It is not normally necessary to call this command directly.

**auto_load** \fIcmd\fR
: This command attempts to load the definition for a Tcl command named \fIcmd\fR.  To do this, it searches an \fIauto-load path\fR, which is a list of one or more directories.  The auto-load path is given by the global variable \fBauto_path\fR if it exists.  If there is no \fBauto_path\fR variable, then the \fBTCLLIBPATH\fR environment variable is used, if it exists.  Otherwise the auto-load path consists of just the Tcl library directory.  Within each directory in the auto-load path there must be a file \fBtclIndex\fR that describes one or more commands defined in that directory and a script to evaluate to load each of the commands.  The \fBtclIndex\fR file should be generated with the \fBauto_mkindex\fR command.  If \fIcmd\fR is found in an index file, then the appropriate script is evaluated to create the command.  The \fBauto_load\fR command returns 1 if \fIcmd\fR was successfully created.  The command returns 0 if there was no index entry for \fIcmd\fR or if the script did not actually define \fIcmd\fR (e.g. because index information is out of date).  If an error occurs while processing the script, then that error is returned. \fBAuto_load\fR only reads the index information once and saves it in the array \fBauto_index\fR;  future calls to \fBauto_load\fR check for \fIcmd\fR in the array rather than re-reading the index files.  The cached index information may be deleted with the command \fBauto_reset\fR.  This will force the next \fBauto_load\fR command to reload the index database from disk.
    It is not normally necessary to call this command directly; the default \fBunknown\fR handler will do so.

**auto_mkindex** \fIdir pattern pattern ...\fR
: Generates an index suitable for use by \fBauto_load\fR.  The command searches \fIdir\fR for all files whose names match any of the \fIpattern\fR arguments (matching is done with the \fBglob\fR command), generates an index of all the Tcl command procedures defined in all the matching files, and stores the index information in a file named \fBtclIndex\fR in \fIdir\fR. If no pattern is given a pattern of \fB*.tcl\fR will be assumed.  For example, the command

    ```
    auto_mkindex foo *.tcl
    ```
    will read all the \fB.tcl\fR files in subdirectory \fBfoo\fR and generate a new index file \fBfoo/tclIndex\fR.
    **Auto_mkindex** parses the Tcl scripts by sourcing them into a child interpreter and monitoring the proc and namespace commands that are executed.  Extensions can use the (undocumented) auto_mkindex_parser package to register other commands that can contribute to the auto_load index. You will have to read through auto.tcl to see how this works.
    **Auto_mkindex_old** (which has the same syntax as \fBauto_mkindex\fR) parses the Tcl scripts in a relatively unsophisticated way:  if any line contains the word "**proc**" as its first characters then it is assumed to be a procedure definition and the next word of the line is taken as the procedure's name. Procedure definitions that do not appear in this way (e.g.\ they have spaces before the \fBproc\fR) will not be indexed.  If your script contains "dangerous" code, such as global initialization code or procedure names with special characters like \fB$\fR, \fB*\fR, \fB[\fR or \fB]\fR, you are safer using \fBauto_mkindex_old\fR.

**auto_reset**
: Destroys all the information cached by \fBauto_execok\fR and \fBauto_load\fR.  This information will be re-read from disk the next time it is needed.  \fBAuto_reset\fR also deletes any procedures listed in the auto-load index, so that fresh copies of them will be loaded the next time that they are used.

**auto_qualify** \fIcommand namespace\fR
: Computes a list of fully qualified names for \fIcommand\fR.  This list mirrors the path a standard Tcl interpreter follows for command lookups:  first it looks for the command in the current namespace, and then in the global namespace.  Accordingly, if \fIcommand\fR is relative and \fInamespace\fR is not \fB::\fR, the list returned has two elements:  \fIcommand\fR scoped by \fInamespace\fR, as if it were a command in the \fInamespace\fR namespace; and \fIcommand\fR as if it were a command in the global namespace.  Otherwise, if either \fIcommand\fR is absolute (it begins with \fB::\fR), or \fInamespace\fR is \fB::\fR, the list contains only \fIcommand\fR as if it were a command in the global namespace.
    **Auto_qualify** is used by the auto-loading facilities in Tcl, both for producing auto-loading indexes such as \fIpkgIndex.tcl\fR, and for performing the actual auto-loading of functions at runtime.

**tcl_findLibrary** \fIbasename version patch initScript enVarName varName\fR
: This is a standard search procedure for use by extensions during their initialization.  They call this procedure to look for their script library in several standard directories. The last component of the name of the library directory is normally \fIbasenameversion\fR (e.g., tk8.0), but it might be "library" when in the build hierarchies. The \fIinitScript\fR file will be sourced into the interpreter once it is found.  The directory in which this file is found is stored into the global variable \fIvarName\fR. If this variable is already defined (e.g., by C code during application initialization) then no searching is done. Otherwise the search looks in these directories: the directory named by the environment variable \fIenVarName\fR; relative to the Tcl library directory; relative to the executable file in the standard installation bin or bin/\fIarch\fR directory; relative to the executable file in the current build tree; relative to the executable file in a parallel build tree.

**parray** \fIarrayName\fR ?\fIpattern\fR?
: Prints on standard output the names and values of all the elements in the array \fIarrayName\fR, or just the names that match \fIpattern\fR (using the matching rules of \fBstring match\fR) and their values if \fIpattern\fR is given. \fIArrayName\fR must be an array accessible to the caller of \fBparray\fR. It may be either local or global. The result of this command is the empty string.
    For example, to print the contents of the \fBtcl_platform\fR array, do:

    ```
    parray tcl_platform
    ```


## Word boundary helpers

These procedures are mainly used internally by Tk.

**tcl_endOfWord** \fIstr start\fR
: Returns the index of the first end-of-word location that occurs after a starting index \fIstart\fR in the string \fIstr\fR.  An end-of-word location is defined to be the first non-word character following the first word character after the starting point.  Returns -1 if there are no more end-of-word locations after the starting point.  See the description of \fBtcl_wordchars\fR and \fBtcl_nonwordchars\fR below for more details on how Tcl determines which characters are word characters.

**tcl_startOfNextWord** \fIstr start\fR
: Returns the index of the first start-of-word location that occurs after a starting index \fIstart\fR in the string \fIstr\fR.  A start-of-word location is defined to be the first word character following a non-word character.  Returns -1 if there are no more start-of-word locations after the starting point.
    For example, to print the indices of the starts of each word in a string according to platform rules:

    ```
    set theString "The quick brown fox"
    for {set idx 0} {$idx >= 0} {
            set idx [tcl_startOfNextWord $theString $idx]} {
        puts "Word start index: $idx"
    }
    ```

**tcl_startOfPreviousWord** \fIstr start\fR
: Returns the index of the first start-of-word location that occurs before a starting index \fIstart\fR in the string \fIstr\fR.  Returns -1 if there are no more start-of-word locations before the starting point.

**tcl_wordBreakAfter** \fIstr start\fR
: Returns the index of the first word boundary after the starting index \fIstart\fR in the string \fIstr\fR.  Returns -1 if there are no more boundaries after the starting point in the given string.  The index returned refers to the second character of the pair that comprises a boundary.

**tcl_wordBreakBefore** \fIstr start\fR
: Returns the index of the first word boundary before the starting index \fIstart\fR in the string \fIstr\fR.  Returns -1 if there are no more boundaries before the starting point in the given string.  The index returned refers to the second character of the pair that comprises a boundary.


## File access helpers

**foreachLine** \fIvarName filename body\fR
: This reads in the text file named \fIfilename\fR one line at a time (using system defaults for reading text files). It writes that line to the variable named by \fIvarName\fR and then executes \fIbody\fR for that line. The result value of \fIbody\fR is ignored, but \fBerror\fR, \fBreturn\fR, \fBbreak\fR and \fBcontinue\fR may be used within it to produce an error, return from the calling context, stop the loop, or go to the next line respectively. The overall result of \fBforeachLine\fR is the empty string (assuming no errors from I/O or from evaluating the body of the loop); the file will be closed prior to the procedure returning.

**readFile** \fIfilename\fR ?\fBtext\fR|\fBbinary\fR?
: Reads in the file named in \fIfilename\fR and returns its contents. The second argument says how to read in the file, either as \fBtext\fR (using the system defaults for reading text files) or as \fBbinary\fR (as uninterpreted bytes). The default is \fBtext\fR. When read as text, this will include any trailing newline. The file will be closed prior to the procedure returning.

**writeFile** \fIfilename\fR ?\fBtext\fR|\fBbinary\fR? \fIcontents\fR
: Writes the \fIcontents\fR to the file named in \fIfilename\fR. The optional second argument says how to write to the file, either as \fBtext\fR (using the system defaults for writing text files) or as \fBbinary\fR (as uninterpreted bytes). The default is \fBtext\fR. If a trailing newline is required, it will need to be provided in \fIcontents\fR. The result of this command is the empty string; the file will be closed prior to the procedure returning.


# Variables

The following global variables are defined or used by the procedures in the Tcl library. They fall into two broad classes, handling unknown commands and packages, and determining what are words.

## Autoloading and package management variables

**auto_execs**
: Used by \fBauto_execok\fR to record information about whether particular commands exist as executable files.
    Not normally usefully accessed directly by user code.

**auto_index**
: Used by \fBauto_load\fR to save the index information read from disk.
    Not normally usefully accessed directly by user code.

**auto_noexec**
: If set to any value, then \fBunknown\fR will not attempt to auto-exec any commands.

**auto_noload**
: If set to any value, then \fBunknown\fR will not attempt to auto-load any commands.

**auto_path**
: If set, then it must contain a valid Tcl list giving directories to search during auto-load operations (including for package index files when using the default \fBpackage unknown\fR handler). This variable is initialized during startup to contain, in order: the directories listed in the \fBTCLLIBPATH\fR environment variable, the directory named by the \fBtcl_library\fR global variable, the parent directory of \fBtcl_library\fR, the directories listed in the \fBtcl_pkgPath\fR variable. Additional locations to look for files and package indices should normally be added to this variable using \fBlappend\fR.
    For example, to add the \fIlib\fR directory next to the running script, you would do:

    ```
    lappend auto_path [file dirname [info script]]/lib
    ```
    Note that if the script uses \fBcd\fR, it is advisable to ensure that entries on the \fBauto_path\fR are \fBfile normalize\fRd.

**env(TCL_LIBRARY)**
: If set, then it specifies the location of the directory containing library scripts (the value of this variable will be assigned to the \fBtcl_library\fR variable and therefore returned by the command \fBinfo library\fR).  If this variable is not set then a default value is used.
    Use of this environment variable is not recommended outside of testing. Tcl installations should already know where to find their own script files, as the value is baked in during the build or installation.

**env(TCLLIBPATH)**
: If set, then it must contain a valid Tcl list giving directories to search during auto-load operations.  Directories must be specified in Tcl format, using "/" as the path separator, regardless of platform. This variable is only used when initializing the \fBauto_path\fR variable.
    A key consequence of this variable is that it gives a way to let the user of a script specify the list of places where that script may use \fBpackage require\fR to read packages from. It is not normally usefully settable within a Tcl script itself \fIexcept\fR to influence where other interpreters load from (whether made with \fBinterp create\fR or launched as their own threads or subprocesses).


## Word boundary determination variables

These variables are only used in the \fBtcl_endOfWord\fR, \fBtcl_startOfNextWord\fR, \fBtcl_startOfPreviousWord\fR, \fBtcl_wordBreakAfter\fR, and \fBtcl_wordBreakBefore\fR commands.

**tcl_nonwordchars**
: This variable contains a regular expression that is used by routines like \fBtcl_endOfWord\fR to identify whether a character is part of a word or not.  If the pattern matches a character, the character is considered to be a non-word character. The default value is \fB\W\fR.

**tcl_wordchars**
: This variable contains a regular expression that is used by routines like \fBtcl_endOfWord\fR to identify whether a character is part of a word or not.  If the pattern matches a character, the character is considered to be a word character. The default value is \fB\w\fR.


