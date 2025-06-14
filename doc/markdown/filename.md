---
CommandName: filename
ManualSection: n
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - file(n)
 - glob(n)
 - zipfs(n)
Keywords:
 - current directory
 - absolute file name
 - relative file name
 - volume-relative file name
 - portability
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

filename - File name conventions supported by Tcl commands

# Introduction

All Tcl commands and C procedures that take file names as arguments expect the file names to be in one of three forms, depending on the current platform.  On each platform, Tcl supports file names in the standard forms(s) for that platform.  In addition, on all platforms, Tcl supports a Unix-like syntax intended to provide a convenient way of constructing simple file names.  However, scripts that are intended to be portable should not assume a particular form for file names. Instead, portable scripts must use the **file split** and **file join** commands to manipulate file names (see the **file** manual entry for more details).

# Path types

File names are grouped into three general types based on the starting point for the path used to specify the file: absolute, relative, and volume-relative.  Absolute names are completely qualified, giving a path to the file relative to a particular volume and the root directory on that volume.  Relative names are unqualified, giving a path to the file relative to the current working directory.  Volume-relative names are partially qualified, either giving the path relative to the root directory on the current volume, or relative to the current directory of the specified volume.  The **file pathtype** command can be used to determine the type of a given path.

# Path syntax

The rules for native names depend on the value reported in the Tcl **platform** element of the **tcl_platform** array:

**Unix**
: On Unix and Apple macOS platforms, Tcl uses path names where the components are separated by slashes.  Path names may be relative or absolute, and file names may contain any character other than slash. The file names **\&.** and **\&..** are special and refer to the current directory and the parent of the current directory respectively. Multiple adjacent slash characters are interpreted as a single separator, except for the first double slash **//** in absolute paths. Any number of trailing slash characters at the end of a path are simply ignored, so the paths **foo**, **foo/** and **foo//** are all identical, and in particular **foo/** does not necessarily mean a directory is being referred.
    The following examples illustrate various forms of path names:

**/**
: Absolute path to the root directory.

**/etc/passwd**
: Absolute path to the file named **passwd** in the directory **etc** in the root directory.

**\&.**
: Relative path to the current directory.

**foo**
: Relative path to the file **foo** in the current directory.

**foo/bar**
: Relative path to the file **bar** in the directory **foo** in the current directory.

**\&../foo**
: Relative path to the file **foo** in the directory above the current directory.


**Windows**
: On Microsoft Windows platforms, Tcl supports both drive-relative and UNC style names.  Both **/** and **\** may be used as directory separators in either type of name.  Drive-relative names consist of an optional drive specifier followed by an absolute or relative path.  UNC paths follow the general form **\\servername\sharename\path\file**, but must at the very least contain the server and share components, i.e. **\\servername\sharename**.  In both forms, the file names **.** and **..** are special and refer to the current directory and the parent of the current directory respectively.  The following examples illustrate various forms of path names:

**\&\\Host\share/file**
: Absolute UNC path to a file called **file** in the root directory of the export point **share** on the host **Host**.  Note that repeated use of **file dirname** on this path will give **//Host/share**, and will never give just **//Host**.

**c:foo**
: Volume-relative path to a file **foo** in the current directory on drive **c**.

**c:/foo**
: Absolute path to a file **foo** in the root directory of drive **c**.

**foo\bar**
: Relative path to a file **bar** in the **foo** directory in the current directory on the current volume.

**\&\foo**
: Volume-relative path to a file **foo** in the root directory of the current volume.

**\&\\foo**
: Volume-relative path to a file **foo** in the root directory of the current volume.  This is not a valid UNC path, so the assumption is that the extra backslashes are superfluous.


**Zipfs**
: On all platforms where **zipfs** support is enabled, paths within mounted ZIP archives begin with the string returned by the **zipfs root** command. Zipfs paths are case-sensitive on all platforms.


# Tilde substitution

Unlike earlier versions of Tcl, Tcl 9 does not do implicit tilde substitution on file paths with the exception noted below. The commands **file home** and **file tildeexpand** may be used to explicitly accomplish the same.

The exception to the above is initialization of the **auto_path** variable and the Tcl module search paths as documented in the manpages for **tclvars** and **tm**. When any path in an environment variable used to initialize these starts with a tilde, it will be interpreted as if the first element is replaced with the location of the home directory for the given user. If the tilde is followed immediately by a separator, the **$HOME** environment variable is substituted. Otherwise the characters between the tilde and the next separator are taken as a user name, which is used to retrieve the user's home directory for substitution. This works on POSIX, macOS and Windows platforms.

# Portability issues

Not all file systems are case sensitive, so scripts should avoid code that depends on the case of characters in a file name.  In addition, the character sets allowed on different devices may differ, so scripts should choose file names that do not contain special characters like: **<>:?"/\|**. The safest approach is to use names consisting of alphanumeric characters only.  Care should be taken with filenames which contain spaces (common on Windows systems) and filenames where the backslash is the directory separator (Windows native path names).

On Windows platforms there are file and path length restrictions. Complete paths or filenames longer than about 260 characters will lead to errors in most file operations.

Another Windows peculiarity is that any number of trailing dots "." in filenames are totally ignored, so, for example, attempts to create a file or directory with a name "foo." will result in the creation of a file/directory with name "foo". This fact is reflected in the results of **file normalize**. Furthermore, a file name consisting only of dots "........." or dots with trailing characters ".....abc" is illegal.

