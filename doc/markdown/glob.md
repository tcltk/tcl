---
CommandName: glob
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - file(n)
Keywords:
 - exist
 - file
 - glob
 - pattern
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

glob - Return names of files that match patterns

# Synopsis

::: {.synopsis} :::
[glob]{.cmd} [switches]{.optarg} [pattern]{.optdot}
:::

# Description

This command performs file name "globbing" in a fashion similar to the csh shell or bash shell. It returns a list of the files whose names match any of the \fIpattern\fR arguments. No particular order is guaranteed in the list, so if a sorted list is required the caller should use \fBlsort\fR.

## Options

If the initial arguments to \fBglob\fR start with \fB-\fR then they are treated as switches. The following switches are currently supported:

**-directory** \fIdirectory\fR
: Search for files which match the given patterns starting in the given \fIdirectory\fR. This allows searching of directories whose name contains glob-sensitive characters without the need to quote such characters explicitly. This option may not be used in conjunction with \fB-path\fR, which is used to allow searching for complete file paths whose names may contain glob-sensitive characters.

**-join**
: The remaining pattern arguments, after option processing, are treated as a single pattern obtained by joining the arguments with directory separators.

**-nocomplain**
: Allows an empty list to be returned without error; This is the default behavior in Tcl 9.0, so this switch has no effect any more.

**-path** \fIpathPrefix\fR
: Search for files with the given \fIpathPrefix\fR where the rest of the name matches the given patterns. This allows searching for files with names similar to a given file (as opposed to a directory) even when the names contain glob-sensitive characters. This option may not be used in conjunction with \fB-directory\fR. For example, to find all files with the same root name as $path, but differing extensions, you should use "**glob -path [file rootname $path] .***" which will work even if \fB$path\fR contains numerous glob-sensitive characters.

**-tails**
: Only return the part of each file found which follows the last directory named in any \fB-directory\fR or \fB-path\fR path specification. Thus "**glob -tails -directory $dir ***" is equivalent to "**set pwd [pwd]; cd $dir; glob *; cd $pwd**". For \fB-path\fR specifications, the returned names will include the last path segment, so "**glob -tails -path [file rootname /home/fred/foo.tex] .***" will return paths like \fBfoo.aux foo.bib foo.tex\fR etc.

**-types** \fItypeList\fR
: Only list files or directories which match \fItypeList\fR, where the items in the list have two forms. The first form is like the -type option of the Unix find command: \fIb\fR (block special file), \fIc\fR (character special file), \fId\fR (directory), \fIf\fR (plain file), \fIl\fR (symbolic link), \fIp\fR (named pipe), or \fIs\fR (socket), where multiple types may be specified in the list. \fBGlob\fR will return all files which match at least one of the types given. Note that symbolic links will be returned both if \fB-types l\fR is given, or if the target of a link matches the requested type. So, a link to a directory will be returned if \fB-types d\fR was specified.
    The second form specifies types where all the types given must match. These are \fIr\fR, \fIw\fR, \fIx\fR as file permissions, and \fIreadonly\fR, \fIhidden\fR as special permission cases. On the Macintosh, macOS types and creators are also supported, where any item which is four characters long is assumed to be a macOS type (e.g. \fBTEXT\fR). Items which are of the form \fI{macintosh type XXXX}\fR or \fI{macintosh creator XXXX}\fR will match types or creators respectively. Unrecognized types, or specifications of multiple macOS types/creators will signal an error.
    The two forms may be mixed, so \fB-types {d f r w}\fR will find all regular files OR directories that have both read AND write permissions. The following are equivalent:

    ```
    glob \-type d *
    glob */
    ```
    except that the first case doesn't return the trailing "/" and is more platform independent.

**-\|-**
: Marks the end of switches. The argument following this one will be treated as a \fIpattern\fR even if it starts with a \fB-\fR.


## Globbing patterns

The \fIpattern\fR arguments may contain any of the following special characters, which are a superset of those supported by \fBstring match\fR:

**?**
: Matches any single character.

*****
: Matches any sequence of zero or more characters.

**[***chars***]**
: Matches any single character in \fIchars\fR. If \fIchars\fR contains a sequence of the form \fIa\fR\fB-\fR\fIb\fR then any character between \fIa\fR and \fIb\fR (inclusive) will match.

**\***x*
: Matches the character \fIx\fR.

**{***a***,***b***,***...***}**
: Matches any of the sub-patterns \fIa\fR, \fIb\fR, etc.


On Unix, as with csh, a "."\| at the beginning of a file's name or just after a "/" must be matched explicitly or with a {} construct, unless the \fB-types hidden\fR flag is given (since "."\| at the beginning of a file's name indicates that it is hidden). On other platforms, files beginning with a "."\| are handled no differently to any others, except the special directories "."\| and ".."\| which must be matched explicitly (this is to avoid a recursive pattern like "glob -join * * * *" from recursing up the directory hierarchy as well as down). In addition, all "/" characters must be matched explicitly.

The \fBglob\fR command differs from csh globbing in two ways. First, it does not sort its result list (use the \fBlsort\fR command if you want the list sorted). Second, \fBglob\fR only returns the names of files that actually exist; in csh no check for existence is made unless a pattern contains a ?, *, or [] construct.

# Windows portability issues

For Windows UNC names, the servername and sharename components of the path may not contain ?, *, or [] constructs.

Since the backslash character has a special meaning to the glob command, glob patterns containing Windows style path separators need special care. The pattern "*C:\\foo\\**" is interpreted as "*C:\foo\**" where "*\f*" will match the single character "*f*" and "*\**" will match the single character "***" and will not be interpreted as a wildcard character. One solution to this problem is to use the Unix style forward slash as a path separator. Windows style paths can be converted to Unix style paths with the command "**file join** \fB$path\fR" or "**file normalize** \fB$path\fR".

# Examples

Find all the Tcl files in the current directory:

```
glob *.tcl
```

Find all the Tcl files in the user's home directory, irrespective of what the current directory is:

```
glob \-directory [file home] *.tcl
```

Find all subdirectories of the current directory:

```
glob \-type d *
```

Find all files whose name contains an "a", a "b" or the sequence "cde":

```
glob \-type f *{a,b,cde}*
```

