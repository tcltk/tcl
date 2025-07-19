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

This command performs file name "globbing" in a fashion similar to the csh shell or bash shell. It returns a list of the files whose names match any of the *pattern* arguments. No particular order is guaranteed in the list, so if a sorted list is required the caller should use **lsort**.

## Options

If the initial arguments to **glob** start with **-** then they are treated as switches. The following switches are currently supported:

**-directory** *directory*
: Search for files which match the given patterns starting in the given *directory*. This allows searching of directories whose name contains glob-sensitive characters without the need to quote such characters explicitly. This option may not be used in conjunction with **-path**, which is used to allow searching for complete file paths whose names may contain glob-sensitive characters.

**-join**
: The remaining pattern arguments, after option processing, are treated as a single pattern obtained by joining the arguments with directory separators.

**-nocomplain**
: Allows an empty list to be returned without error; This is the default behavior in Tcl 9.0, so this switch has no effect any more.

**-path** *pathPrefix*
: Search for files with the given *pathPrefix* where the rest of the name matches the given patterns. This allows searching for files with names similar to a given file (as opposed to a directory) even when the names contain glob-sensitive characters. This option may not be used in conjunction with **-directory**. For example, to find all files with the same root name as $path, but differing extensions, you should use "**glob -path [file rootname $path] .***" which will work even if **$path** contains numerous glob-sensitive characters.

**-tails**
: Only return the part of each file found which follows the last directory named in any **-directory** or **-path** path specification. Thus "**glob -tails -directory $dir ***" is equivalent to "**set pwd [pwd]; cd $dir; glob *; cd $pwd**". For **-path** specifications, the returned names will include the last path segment, so "**glob -tails -path [file rootname /home/fred/foo.tex] .***" will return paths like **foo.aux foo.bib foo.tex** etc.

**-types** *typeList*
: Only list files or directories which match *typeList*, where the items in the list have two forms. The first form is like the -type option of the Unix find command: *b* (block special file), *c* (character special file), *d* (directory), *f* (plain file), *l* (symbolic link), *p* (named pipe), or *s* (socket), where multiple types may be specified in the list. **Glob** will return all files which match at least one of the types given. Note that symbolic links will be returned both if **-types l** is given, or if the target of a link matches the requested type. So, a link to a directory will be returned if **-types d** was specified.
    The second form specifies types where all the types given must match. These are *r*, *w*, *x* as file permissions, and *readonly*, *hidden* as special permission cases. On the Macintosh, macOS types and creators are also supported, where any item which is four characters long is assumed to be a macOS type (e.g. **TEXT**). Items which are of the form *{macintosh type XXXX}* or *{macintosh creator XXXX}* will match types or creators respectively. Unrecognized types, or specifications of multiple macOS types/creators will signal an error.
    The two forms may be mixed, so **-types {d f r w}** will find all regular files OR directories that have both read AND write permissions. The following are equivalent:

    ```
    glob \-type d *
    glob */
    ```
    except that the first case doesn't return the trailing "/" and is more platform independent.

**-\|-**
: Marks the end of switches. The argument following this one will be treated as a *pattern* even if it starts with a **-**.


## Globbing patterns

The *pattern* arguments may contain any of the following special characters, which are a superset of those supported by **string match**:

**?**
: Matches any single character.

*****
: Matches any sequence of zero or more characters.

**[***chars***]**
: Matches any single character in *chars*. If *chars* contains a sequence of the form *a***-***b* then any character between *a* and *b* (inclusive) will match.

**\***x*
: Matches the character *x*.

**{***a***,***b***,***...***}**
: Matches any of the sub-patterns *a*, *b*, etc.


On Unix, as with csh, a "."\| at the beginning of a file's name or just after a "/" must be matched explicitly or with a {} construct, unless the **-types hidden** flag is given (since "."\| at the beginning of a file's name indicates that it is hidden). On other platforms, files beginning with a "."\| are handled no differently to any others, except the special directories "."\| and ".."\| which must be matched explicitly (this is to avoid a recursive pattern like "glob -join * * * *" from recursing up the directory hierarchy as well as down). In addition, all "/" characters must be matched explicitly.

The **glob** command differs from csh globbing in two ways. First, it does not sort its result list (use the **lsort** command if you want the list sorted). Second, **glob** only returns the names of files that actually exist; in csh no check for existence is made unless a pattern contains a ?, *, or [] construct.

# Windows portability issues

For Windows UNC names, the servername and sharename components of the path may not contain ?, *, or [] constructs.

Since the backslash character has a special meaning to the glob command, glob patterns containing Windows style path separators need special care. The pattern "*C:\\foo\\**" is interpreted as "*C:\foo\**" where "*\f*" will match the single character "*f*" and "*\**" will match the single character "***" and will not be interpreted as a wildcard character. One solution to this problem is to use the Unix style forward slash as a path separator. Windows style paths can be converted to Unix style paths with the command "**file join** **$path**" or "**file normalize** **$path**".

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

