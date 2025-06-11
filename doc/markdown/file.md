---
CommandName: file
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - filename(n)
 - open(n)
 - close(n)
 - eof(n)
 - gets(n)
 - tell(n)
 - seek(n)
 - fblocked(n)
 - flush(n)
Keywords:
 - attributes
 - copy files
 - delete files
 - directory
 - file
 - move files
 - name
 - rename files
 - stat
 - user
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

file - Manipulate file names and attributes

# Synopsis

::: {.synopsis} :::
[file]{.cmd} [option]{.arg} [name]{.arg} [arg arg]{.optdot}
:::

# Description

This command provides several operations on a file's name or attributes.  The *name* argument is the name of a file in most cases. The *option* argument indicates what to do with the file name.  Any unique abbreviation for *option* is acceptable.  The valid options are:

**file atime** *name* ?*time*?
: Returns a decimal string giving the time at which file *name* was last accessed.  If *time* is specified, it is an access time to set for the file.  The time is measured in the standard POSIX fashion as seconds from a fixed starting time (often January 1, 1970).  If the file does not exist or its access time cannot be queried or set then an error is generated.  On Windows, FAT file systems do not support access time. On **zipfs** file systems, access time is mapped to the modification time.

**file attributes** *name*
: ...see next...

**file attributes** *name* ?*option*?
: ...see next...

**file attributes** *name* ?*option value option value...*?
: This subcommand returns or sets platform-specific values associated with a file. The first form returns a list of the platform-specific options and their values. The second form returns the value for the given option. The third form sets one or more of the values. The values are as follows:
    On Unix, **-group** gets or sets the group name for the file. A group id can be given to the command, but it returns a group name. **-owner** gets or sets the user name of the owner of the file. The command returns the owner name, but the numerical id can be passed when setting the owner. **-permissions** retrieves or sets a file's access permissions, using octal notation by default. This option also provides limited support for setting permissions using the symbolic notation accepted by the **chmod** command, following the form [**ugo**]?[[**+-=**][**rwxst**]**,**[...]]. Multiple permission specifications may be given, separated by commas. E.g., **u+s,go-rw** would set the setuid bit for a file's owner as well as remove read and write permission for the file's group and other users. An **ls**-style string of the form **rwxrwxrwx** is also accepted but must always be 9 characters long. E.g., **rwxr-xr-t** is equivalent to **01755**. On versions of Unix supporting file flags, **-readonly** returns the value of, or sets, or clears the readonly attribute of a file, i.e., the user immutable flag (**uchg**) to the **chflags** command.
    On Windows, **-archive** gives the value or sets or clears the archive attribute of the file. **-hidden** gives the value or sets or clears the hidden attribute of the file. **-longname** will expand each path element to its long version. This attribute cannot be set. **-readonly** gives the value or sets or clears the readonly attribute of the file. **-shortname** gives a string where every path element is replaced with its short (8.3) version of the name if possible. For path elements that cannot be mapped to short names, the long name is retained. This attribute cannot be set. **-system** gives or sets or clears the value of the system attribute of the file.
    On macOS and Darwin, **-creator** gives or sets the Finder creator type of the file. **-hidden** gives or sets or clears the hidden attribute of the file. **-readonly** gives or sets or clears the readonly attribute of the file. **-rsrclength** gives the length of the resource fork of the file, this attribute can only be set to the value 0, which results in the resource fork being stripped off the file.
    On all platforms, files in **zipfs** mounted archives return the following attributes. These are all read-only and cannot be directly set.

**-archive**
: The path of the mounted ZIP archive containing the file.

**-compsize**
: The compressed size of the file within the archive. This is **0** for directories.

**-crc**
: The CRC of the file if present, else **0**.

**-mount**
: The path where the containing archive is mounted.

**-offset**
: The offset of the file within the archive.

**-uncompsize**
: The uncompressed size of the file. This is **0** for directories.



Other attributes may be present in the returned list. These should be ignored.

**file channels** ?*pattern*?
: If *pattern* is not specified, returns a list of names of all registered open channels in this interpreter.  If *pattern* is specified, only those names matching *pattern* are returned.  Matching is determined using the same rules as for **string match**.

**file copy** ?**-force**? ?**-\|-**? *source target*
: ...see next...

**file copy** ?**-force**? ?**-\|-**? *source* ?*source* ...? *targetDir*
: The first form makes a copy of the file or directory *source* under the pathname *target*. If *target* is an existing directory, then the second form is used.  The second form makes a copy inside *targetDir* of each *source* file listed.  If a directory is specified as a *source*, then the contents of the directory will be recursively copied into *targetDir*. Existing files will not be overwritten unless the **-force** option is specified (when Tcl will also attempt to adjust permissions on the destination file or directory if that is necessary to allow the copy to proceed).  When copying within a single filesystem, *file copy* will copy soft links (i.e. the links themselves are copied, not the things they point to).  Trying to overwrite a non-empty directory, overwrite a directory with a file, or overwrite a file with a directory will all result in errors even if **-force** was specified.  Arguments are processed in the order specified, halting at the first error, if any.  A **-\|-** marks the end of switches; the argument following the **-\|-** will be treated as a *source* even if it starts with a **-**.

**file delete** ?**-force**? ?**-\|-**? ?*pathname* ... ?
: Removes the file or directory specified by each *pathname* argument.  Non-empty directories will be removed only if the **-force** option is specified.  When operating on symbolic links, the links themselves will be deleted, not the objects they point to. Trying to delete a non-existent file is not considered an error. Trying to delete a read-only file will cause the file to be deleted, even if the **-force** flags is not specified.  If the **-force** option is specified on a directory, Tcl will attempt both to change permissions and move the current directory "pwd" out of the given path if that is necessary to allow the deletion to proceed.  Arguments are processed in the order specified, halting at the first error, if any. A **-\|-** marks the end of switches; the argument following the **-\|-** will be treated as a *pathname* even if it starts with a **-**.

**file dirname** *name*
: Returns a name comprised of all of the path components in *name* excluding the last element.  If *name* is a relative file name and only contains one path element, then returns "**.**". If *name* refers to a root directory, then the root directory is returned.  For example,

    ```
    file dirname c:/
    ```
    returns **c:/**.

**file executable** *name*
: Returns **1** if file *name* is executable by the current user, **0** otherwise. On Windows, which does not have an executable attribute, the command treats all directories and any files with extensions **exe**, **com**, **cmd** or **bat** as executable.

**file exists** *name*
: Returns **1** if file *name* exists and the current user has search privileges for the directories leading to it, **0** otherwise.

**file extension** *name*
: Returns all of the characters in *name* after and including the last dot in the last element of *name*.  If there is no dot in the last element of *name* then returns the empty string.

**file home ?***username*?
: If no argument is specified, the command returns the home directory of the current user. This is generally the value of the **$HOME** environment variable except that on Windows platforms backslashes in the path are replaced by forward slashes. An error is raised if the **$HOME** environment variable is not set.
    If *username* is specified, the command returns the home directory configured in the system for the specified user. Note this may be different than the value of the **$HOME** environment variable even when *username* corresponds to the current user. An error is raised if the *username* does not correspond to a user account on the system.

**file isdirectory** *name*
: Returns **1** if file *name* is a directory, **0** otherwise.

**file isfile** *name*
: Returns **1** if file *name* is a regular file, **0** otherwise.

**file join** *name* ?*name ...*?
: Takes one or more file names and combines them, using the correct path separator for the current platform.  If a particular *name* is relative, then it will be joined to the previous file name argument. Otherwise, any earlier arguments will be discarded, and joining will proceed from the current argument.  For example,

    ```
    file join a b /foo bar
    ```
    returns **/foo/bar**.
    Note that any of the names can contain separators, and that the result is always canonical for the current platform: **/** for Unix and Windows.

**file link** ?*-linktype*? *linkName* ?*target*?
: If only one argument is given, that argument is assumed to be *linkName*, and this command returns the value of the link given by *linkName* (i.e. the name of the file it points to).  If *linkName* is not a link or its value cannot be read (as, for example, seems to be the case with hard links, which look just like ordinary files), then an error is returned.
    If 2 arguments are given, then these are assumed to be *linkName* and *target*. If *linkName* already exists, or if *target* does not exist, an error will be returned.  Otherwise, Tcl creates a new link called *linkName* which points to the existing filesystem object at *target* (which is also the returned value), where the type of the link is platform-specific (on Unix a symbolic link will be the default).  This is useful for the case where the user wishes to create a link in a cross-platform way, and does not care what type of link is created.
    If the user wishes to make a link of a specific type only, (and signal an error if for some reason that is not possible), then the optional *-linktype* argument should be given.  Accepted values for *-linktype* are "**-symbolic**" and "**-hard**".
    On Unix, symbolic links can be made to relative paths, and those paths must be relative to the actual *linkName*'s location (not to the cwd), but on all other platforms where relative links are not supported, target paths will always be converted to absolute, normalized form before the link is created (and therefore relative paths are interpreted as relative to the cwd). When creating links on filesystems that either do not support any links, or do not support the specific type requested, an error message will be returned.  Most Unix platforms support both symbolic and hard links (the latter for files only). Windows supports symbolic directory links and hard file links on NTFS drives.

**file lstat** *name* ?*varName*?
: Same as **stat** option (see below) except uses the *lstat* kernel call instead of *stat*.  This means that if *name* refers to a symbolic link the information returned is for the link rather than the file it refers to.  On systems that do not support symbolic links this option behaves exactly the same as the **stat** option.

**file mkdir** ?*dir* ...?
: Creates each directory specified.  For each pathname *dir* specified, this command will create all non-existing parent directories as well as *dir* itself.  If an existing directory is specified, then no action is taken and no error is returned.  Trying to overwrite an existing file with a directory will result in an error.  Arguments are processed in the order specified, halting at the first error, if any.

**file mtime** *name* ?*time*?
: Returns a decimal string giving the time at which file *name* was last modified.  If *time* is specified, it is a modification time to set for the file (equivalent to Unix **touch**).  The time is measured in the standard POSIX fashion as seconds from a fixed starting time (often January 1, 1970).  If the file does not exist or its modified time cannot be queried or set then an error is generated. On **zipfs** file systems, modification time cannot be explicitly set.

**file nativename** *name*
: Returns the platform-specific name of the file. This is useful if the filename is needed to pass to a platform-specific call, such as to a subprocess via **exec** under Windows (see **EXAMPLES** below).

**file normalize** *name*
: Returns a unique normalized path representation for the file-system object (file, directory, link, etc), whose string value can be used as a unique identifier for it.  A normalized path is an absolute path which has all "../" and "./" removed.  Also it is one which is in the "standard" format for the native platform.  On Unix, this means the segments leading up to the path must be free of symbolic links/aliases (but the very last path component may be a symbolic link), and on Windows it also means we want the long form with that form's case-dependence (which gives us a unique, case-dependent path).  The one exception concerning the last link in the path is necessary, because Tcl or the user may wish to operate on the actual symbolic link itself (for example **file delete**, **file rename**, **file copy** are defined to operate on symbolic links, not on the things that they point to).

**file owned** *name*
: Returns **1** if file *name* is owned by the current user, **0** otherwise.

**file pathtype** *name*
: Returns one of **absolute**, **relative**, **volumerelative**. If *name* refers to a specific file on a specific volume, the path type will be **absolute**. If *name* refers to a file relative to the current working directory, then the path type will be **relative**. If *name* refers to a file relative to the current working directory on a specified volume, or to a specific file on the current working volume, then the path type is **volumerelative**.

**file readable** *name*
: Returns **1** if file *name* is readable by the current user, **0** otherwise.

**file readlink** *name*
: Returns the value of the symbolic link given by *name* (i.e. the name of the file it points to).  If *name* is not a symbolic link or its value cannot be read, then an error is returned.  On systems that do not support symbolic links this option is undefined.

**file rename** ?**-force**? ?**-\|-**? *source target*
: ...see next...

**file rename** ?**-force**? ?**-\|-**? *source* ?*source* ...? *targetDir*
: The first form takes the file or directory specified by pathname *source* and renames it to *target*, moving the file if the pathname *target* specifies a name in a different directory.  If *target* is an existing directory, then the second form is used. The second form moves each *source* file or directory into the directory *targetDir*. Existing files will not be overwritten unless the **-force** option is specified.  When operating inside a single filesystem, Tcl will rename symbolic links rather than the things that they point to.  Trying to overwrite a non-empty directory, overwrite a directory with a file, or a file with a directory will all result in errors.  Arguments are processed in the order specified, halting at the first error, if any.  A **-\|-** marks the end of switches; the argument following the **-\|-** will be treated as a *source* even if it starts with a **-**.

**file rootname** *name*
: Returns all of the characters in *name* up to but not including the last "." character in the last component of name.  If the last component of *name* does not contain a dot, then returns *name*.

**file separator** ?*name*?
: If no argument is given, returns the character which is used to separate path segments for native files on this platform.  If a path is given, the filesystem responsible for that path is asked to return its separator character.  If no file system accepts *name*, an error is generated.

**file size** *name*
: Returns a decimal string giving the size of file *name* in bytes.  If the file does not exist or its size cannot be queried then an error is generated.

**file split** *name*
: Returns a list whose elements are the path components in *name*.  The first element of the list will have the same path type as *name*. All other elements will be relative.  Path separators will be discarded unless they are needed to ensure that an element is unambiguously relative.

**file stat** *name* ?*varName*?
: Invokes the **stat** kernel call on *name*, and returns a dictionary with the information returned from the kernel call. If *varName* is given, it uses the variable to hold the information. *VarName* is treated as an array variable, and in such case the command returns the empty string. The following elements are set: **atime**, **ctime**, **dev**, **gid**, **ino**, **mode**, **mtime**, **nlink**, **size**, **type**, **uid**.  Each element except **type** is a decimal string with the value of the corresponding field from the **stat** return structure; see the manual entry for **stat** for details on the meanings of the values.  The **type** element gives the type of the file in the same form returned by the command **file type**.

**file system** *name*
: Returns a list of one or two elements, the first of which is the name of the filesystem to use for the file, and the second, if given, an arbitrary string representing the filesystem-specific nature or type of the location within that filesystem.  If a filesystem only supports one type of file, the second element may not be supplied.  For example the native files have a first element "native", and a second element which when given is a platform-specific type name for the file's system (e.g. "NTFS", "FAT", on Windows).  A generic virtual file system might return the list "vfs ftp" to represent a file on a remote ftp site mounted as a virtual filesystem through an extension called "vfs". If the file does not belong to any filesystem, an error is generated.

**file tail** *name*
: Returns all of the characters in the last filesystem component of *name*.  Any trailing directory separator in *name* is ignored. If *name* contains no separators then returns *name*.  So, **file tail a/b**, **file tail a/b/** and **file tail b** all return **b**.

**file tempdir** ?*template*?
: Creates a temporary directory (guaranteed to be newly created and writable by the current script) and returns its name. If *template* is given, it specifies one of or both of the existing directory (on a filesystem controlled by the operating system) to contain the temporary directory, and the base part of the directory name; it is considered to have the location of the directory if there is a directory separator in the name, and the base part is everything after the last directory separator (if non-empty).  The default containing directory is determined by system-specific operations, and the default base name prefix is "**tcl**".
    The following output is typical and illustrative; the actual output will vary between platforms:

    ```
    % file tempdir
    /var/tmp/tcl_u0kuy5
     % file tempdir /tmp/myapp
    /tmp/myapp_8o7r9L
    % file tempdir /tmp/
    /tmp/tcl_1mOJHD
    % file tempdir myapp
    /var/tmp/myapp_0ihS0n
    ```

**file tempfile** ?*nameVar*? ?*template*?
: Creates a temporary file and returns a read-write channel opened on that file. If the *nameVar* is given, it specifies a variable that the name of the temporary file will be written into; if absent, Tcl will attempt to arrange for the temporary file to be deleted once it is no longer required. If the *template* is present, it specifies parts of the template of the filename to use when creating it (such as the directory, base-name or extension) though some platforms may ignore some or all of these parts and use a built-in default instead.
    Note that temporary files are *only* ever created on the native filesystem. As such, they can be relied upon to be used with operating-system native APIs and external programs that require a filename.

**file tildeexpand** *name*
: Returns the result of performing tilde substitution on *name*. If the name begins with a tilde, then the file name will be interpreted as if the first element is replaced with the location of the home directory for the given user. If the tilde is followed immediately by a path separator, the **$HOME** environment variable is substituted.  Otherwise the characters between the tilde and the next separator are taken as a user name, which is used to retrieve the user's home directory for substitution.  An error is raised if the **$HOME** environment variable or user does not exist.
    If the file name does not begin with a tilde, it is returned unmodified.

**file type** *name*
: Returns a string giving the type of file *name*, which will be one of **file**, **directory**, **characterSpecial**, **blockSpecial**, **fifo**, **link**, or **socket**.

**file volumes**
: Returns the absolute paths to the volumes mounted on the system, as a proper Tcl list.  Without any virtual filesystems mounted as root volumes, on UNIX, the command will always return "/", since all filesystems are locally mounted. On Windows, it will return a list of the available local drives (e.g. "a:/ c:/"). If any virtual filesystem has mounted additional volumes, they will be in the returned list.

**file writable** *name*
: Returns **1** if file *name* is writable by the current user, **0** otherwise.


# Portability issues

**Unix**
: These commands always operate using the real user and group identifiers, not the effective ones.

**Windows**
: The **file owned** subcommand uses the user identifier (SID) of the process token, not the thread token which may be impersonating some other user.


# Examples

This procedure shows how to search for C files in a given directory that have a correspondingly-named object file in the current directory:

```
proc findMatchingCFiles {dir} {
    set files {}
    switch $::tcl_platform(platform) {
        windows {
            set ext .obj
        }
        unix {
           set ext .o
        }
    }
    foreach file [glob -nocomplain -directory $dir *.c] {
        set objectFile [file tail [file rootname $file]]$ext
        if {[file exists $objectFile]} {
            lappend files $file
        }
    }
    return $files
}
```

Rename a file and leave a symbolic link pointing from the old location to the new place:

```
set oldName foobar.txt
set newName foo/bar.txt
# Make sure that where we're going to move to exists...
if {![file isdirectory [file dirname $newName]]} {
    file mkdir [file dirname $newName]
}
file rename $oldName $newName
file link -symbolic $oldName $newName
```

On Windows, a file can be "started" easily enough (equivalent to double-clicking on it in the Explorer interface) but the name passed to the operating system must be in native format:

```
exec {*}[auto_execok start] {} [file nativename C:/Users/fred/example.txt]
```

