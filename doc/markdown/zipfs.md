---
CommandName: zipfs
ManualSection: n
Version: 1.0
TclPart: Zipfs
TclDescription: zipfs Commands
Links:
 - tclsh(1)
 - file(n)
 - zipfs(3)
 - zlib(n)
Keywords:
 - compress
 - filesystem
 - zip
Copyright:
 - Copyright (c) 2015 Jan Nijtmans <jan.nijtmans@gmail.com>
 - Copyright (c) 2015 Christian Werner <chw@ch-werner.de>
 - Copyright (c) 2015 Sean Woods <yoda@etoyoc.com>
---

# Name

zipfs - Mount and work with ZIP files within Tcl

# Synopsis

::: {.synopsis} :::
[zipfs]{.cmd} [canonical]{.sub} [mountpoint]{.optarg} [filename]{.arg}
[zipfs]{.cmd} [exists]{.sub} [filename]{.arg}
[zipfs]{.cmd} [find]{.sub} [directoryName]{.arg}
[zipfs]{.cmd} [info]{.sub} [filename]{.arg}
[zipfs]{.cmd} [list]{.sub} [matchingType]{.optarg} [pattern]{.optarg}
[zipfs]{.cmd} [lmkimg]{.sub} [outfile]{.arg} [inlist]{.arg} [password]{.optarg} [infile]{.optarg}
[zipfs]{.cmd} [lmkzip]{.sub} [outfile]{.arg} [inlist]{.arg} [password]{.optarg}
[zipfs]{.cmd} [mkimg]{.sub} [outfile]{.arg} [indir]{.arg} [strip]{.optarg} [password]{.optarg} [infile]{.optarg}
[zipfs]{.cmd} [mkkey]{.sub} [password]{.arg}
[zipfs]{.cmd} [mkzip]{.sub} [outfile]{.arg} [indir]{.arg} [strip]{.optarg} [password]{.optarg}
[zipfs]{.cmd} [mount]{.sub} [zipfile]{.optarg} [mountpoint]{.optarg} [password]{.optarg}
[zipfs]{.cmd} [mountdata]{.sub} [data]{.arg} [mountpoint]{.arg}
[zipfs]{.cmd} [root]{.sub}
[zipfs]{.cmd} [unmount]{.sub} [mountpoint]{.arg}
:::

# Description

The **zipfs** command provides Tcl with the ability to mount the contents of a ZIP archive file as a virtual file system. Tcl's ZIP archive support is limited to basic features and options. Supported storage methods include only STORE and DEFLATE with optional simple encryption, sufficient to prevent casual inspection of their contents but not able to prevent access by even a moderately determined attacker. Strong encryption, multi-part archives, platform metadata, zip64 formats and other compression methods like bzip2 are not supported.

Files within mounted archives can be written to but new files or directories cannot be created. Further, modifications to files are limited to the mounted archive in memory and are not persisted to disk.

Paths in mounted archives are case-sensitive on all platforms.

**zipfs canonical** ?*mountpoint*? *filename*
: This takes the name of a file, *filename*, and produces where it would be mapped into a zipfs mount as its result. If specified, *mountpoint* says within which mount the mapping will be done; if omitted, the main root of the zipfs system is used.

**zipfs exists** *filename*
: Return 1 if the given filename exists in the mounted zipfs and 0 if it does not.

**zipfs find** *directoryName*
: Returns the list of paths under directory *directoryName* which need not be within a zipfs mounted archive. The paths are prefixed with *directoryName*. This command is also used by the **zipfs mkzip** and **zipfs mkimg** commands.

**zipfs info** *file*
: Return information about the given *file* in the mounted zipfs.  The information consists of:

1.     the name of the ZIP archive file that contains the file,

2.     the size of the file after decompressions,

3.     the compressed size of the file, and

4.     the offset of the compressed data in the ZIP archive file.

    As a special case, querying the mount point gives the start of the zip data as the offset in (4), which can be used to truncate the zip information from an executable. Querying an ancestor of a mount point will raise an error.

**zipfs list** ?*matchingType*? ?*pattern*?
: If *pattern* is not specified, the command returns a list of files across all zipfs mounted archives. If *pattern* is specified, only those paths matching the pattern are returned. By default, or with *matchingType* specified as **-glob**, the pattern is treated as a glob pattern and matching is done as described for the **string match** command. Alternatively, the *matchingType* **-regexp** may be used to specify a matching **pattern** as a regular expression. The file names are returned in arbitrary order. Note that path separators are treated as ordinary characters in the matching. Thus forward slashes should be used as path separators in the pattern. The returned paths only include those actually in the archive and does not include intermediate directories in mount paths.

**zipfs mount**
: ...see next...

**zipfs mount** *mountpoint*
: ...see next...

**zipfs mount** *zipfile mountpoint* ?*password*?
: The **zipfs mount** command mounts ZIP archives as Tcl virtual file systems and returns information about current mounts.
    With no arguments, the command returns a dictionary mapping mount points to the path of the corresponding ZIP archive.
    In the single argument form, the command returns the file path of the ZIP archive mounted at the specified mount point.
    In the third form, the command mounts the ZIP archive *zipfile* as a Tcl virtual filesystem at *mountpoint*.  After this command executes, files contained in *zipfile* will appear to Tcl to be regular files at the mount point. If *mountpoint* is specified as an empty string, it is defaulted to the **[zipfs root]**. The command returns the normalized mount point path.
    If not under the zipfs file system root, *mountpoint* is normalized with respect to it. For example, a mount point passed as either **mt** or **/mt** would be normalized to **//zipfs:/mt** (given that **zipfs root** returns "//zipfs:/"). An error is raised if the mount point includes a drive or UNC volume.
    **NB:** because the current working directory is a concept maintained by the operating system, using **cd** into a mounted archive will only work in the current process, and then not entirely consistently (e.g., if a shared library uses direct access to the OS rather than through Tcl's filesystem API, it will not see the current directory as being inside the mount and will not be able to access the files inside the mount).

**zipfs mountdata** *data* *mountpoint*
: Mounts the ZIP archive content *data* as a Tcl virtual filesystem at *mountpoint*.

**zipfs root**
: Returns a constant string which indicates the mount point for zipfs volumes for the current platform. User should not rely on the mount point being the same constant string for all platforms.

**zipfs unmount** *mountpoint*
: Unmounts a previously mounted ZIP archive mounted to *mountpoint*. The command will fail with an error exception if there are any files within the mounted archive are open.


## Zip creation commands

This package also provides several commands to aid the creation of ZIP archives as Tcl applications.

**zipfs mkzip** *outfile indir* ?*strip*? ?*password*?
: Creates a ZIP archive file named *outfile* from the contents of the input directory *indir* (contained regular files only) with optional ZIP password *password*. While processing the files below *indir* the optional file name prefix given in *strip* is stripped off the beginning of the respective file name if non-empty.  When stripping, it is common to remove either the whole source directory name or the name of its parent directory.
    **Caution:** the choice of the *indir* parameter (less the optional stripped prefix) determines the later root name of the archive's content.

**zipfs mkimg** *outfile indir* ?*strip*? ?*password*? ?*infile*?
: Creates an image (potentially a new executable file) similar to **zipfs mkzip**; see that command for a description of most parameters to this command, as they behave identically here. If *outfile* exists, it will be silently overwritten.
    If the *infile* parameter is specified, this file is prepended in front of the ZIP archive, otherwise the file returned by **info nameofexecutable** (i.e., the executable file of the running process, typically **wish** or **tclsh**) is used. If the *password* parameter is not the empty string, an obfuscated version of that password (see **zipfs mkkey**) is placed between the image and ZIP chunks of the output file and the contents of the ZIP chunk are protected with that password. If the starting image has a ZIP archive already attached to it, it is removed from the copy in *outfile* before the new ZIP archive is added.
    If there is a file, **main.tcl**, in the root directory of the resulting archive and the image file that the archive is attached to is a **tclsh** (or **wish**) instance (true by default, but depends on your configuration), then the resulting image is an executable that will **source** the script in that **main.tcl** after mounting the ZIP archive, and will **exit** once that script has been executed.
    **Note:** **tclsh** and **wish** can be built using either dynamic binding or static binding of the core implementation libraries. With a dynamic binding, the base application Tcl_Library contents are attached to the **libtcl** and **libtk** shared library, respectively. With a static binding, the Tcl_Library contents, etc., are attached to the application, **tclsh** or **wish**. When using **mkimg** with a statically built tclsh, it is the user's responsibility to preserve the attached archive by first extracting it to a temporary location, and then add whatever additional files desired, before creating and attaching the new archive to the new application.

**zipfs mkkey** *password*
: Given the clear text *password* argument, an obfuscated string version is returned with the same format used in the **zipfs mkimg** command.

**zipfs lmkimg** *outfile inlist* ?*password*? ?*infile*?
: This command is like **zipfs mkimg**, but instead of an input directory, *inlist* must be a Tcl list where the odd elements are the names of files to be copied into the archive in the image, and the even elements are their respective names within that archive.

**zipfs lmkzip** *outfile inlist* ?*password*?
: This command is like **zipfs mkzip**, but instead of an input directory, *inlist* must be a Tcl list where the odd elements are the names of files to be copied into the archive, and the even elements are their respective names within that archive.


# Note

The current syntax for certain subcommands using multiple optional parameters might change in the future to support an *?-option value?* pattern instead. Therfore, the current syntax should not be considered stable.

# Examples

Mounting an ZIP archive as an application directory and running code out of it before unmounting it again:

```
set zip myApp.zip
set base [file join [zipfs root] myApp]

zipfs mount $zip $base
# $base now has the contents of myApp.zip

source [file join $base app.tcl]
# use the contents, load libraries from it, etc...

zipfs unmount $base
```

Creating a ZIP archive, given that a directory exists containing the content to put in the archive. Note that the source directory is given twice, in order to strip the exterior directory name from each filename in the archive.

```
set sourceDirectory [file normalize myApp]
set targetZip myApp.zip

zipfs mkzip $targetZip $sourceDirectory $sourceDirectory
```

Encryption can be applied to ZIP archives by providing a password when building the ZIP and when mounting it.

```
set zip myApp.zip
set sourceDir [file normalize myApp]
set password "hunter2"
set base [file join [zipfs root] myApp]

# Create with password
zipfs mkzip $targetZip $sourceDir $sourceDir $password

# Mount with password
zipfs mount $zip $base $password
```

The following example creates an executable application by appending a ZIP archive to the tclsh file it was called from and storing the resulting executable in the file "myApp.bin". When creating an executable image with a password, the password is placed within the executable in a shrouded form so that the application can read files inside the embedded ZIP archive yet casual inspection cannot read it.

```
set appDir [file normalize myApp]
set img "myApp.bin"
set password "hunter2"

# Create some simple content to define a basic application
file mkdir $appDir
set f [open $appDir/main.tcl w]
puts $f {
    puts "Hi. This is [info script]"
}
close $f

# Create the executable application
zipfs mkimg $img $appDir $appDir $password

# remove the now obsolete temporary appDir folder
file delete -force $appDir

# Launch the executable, printing its output to stdout
exec $img >@stdout
# prints the following line assuming [zipfs root] returns "//zipfs:/":
# Hi. This is //zipfs:/app/main.tcl
```

