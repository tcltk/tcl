---
CommandName: Safe Tcl
ManualSection: n
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - interp(n)
 - library(n)
 - load(n)
 - package(n)
 - pkg_mkIndex(n)
 - source(n)
 - tm(n)
 - unknown(n)
Keywords:
 - alias
 - auto-loading
 - auto_mkindex
 - load
 - parent interpreter
 - safe
 - interpreter
 - child interpreter
 - source
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

safe - Creating and manipulating safe interpreters

# Synopsis

::: {.synopsis} :::
[::safe::interpCreate]{.cmd} [child]{.optarg} [options]{.optdot}
[::safe::interpInit]{.cmd} [child]{.arg} [options]{.optdot}
[::safe::interpConfigure]{.cmd} [child]{.arg} [options]{.optdot}
[::safe::interpDelete]{.cmd} [child]{.arg}
[::safe::interpAddToAccessPath]{.cmd} [child]{.arg} [directory]{.arg}
[::safe::interpFindInAccessPath]{.cmd} [child]{.arg} [directory]{.arg}
[::safe::setSyncMode]{.cmd} [newValue]{.optarg}
[::safe::setLogCmd]{.cmd} [cmd arg]{.optdot}
:::

# Description

Safe Tcl is a mechanism for executing untrusted Tcl scripts safely and for providing mediated access by such scripts to potentially dangerous functionality.

Safe Tcl ensures that untrusted Tcl scripts cannot harm the hosting application. It prevents integrity and privacy attacks. Untrusted Tcl scripts are prevented from corrupting the state of the hosting application or computer. Untrusted scripts are also prevented from disclosing information stored on the hosting computer or in the hosting application to any party.

Safe Tcl allows a parent interpreter to create safe, restricted interpreters that contain a set of predefined aliases for the **source**, **load**, **file**, **encoding**, and **exit** commands and are able to use the auto-loading and package mechanisms.

No knowledge of the file system structure is leaked to the safe interpreter, because it has access only to a virtualized path containing tokens. When the safe interpreter requests to source a file, it uses the token in the virtual path as part of the file name to source; the parent interpreter transparently translates the token into a real directory name and executes the requested operation (see the section **SECURITY** below for details). Different levels of security can be selected by using the optional flags of the commands described below.

All commands provided in the parent interpreter by Safe Tcl reside in the **safe** namespace.

# Commands

The following commands are provided in the parent interpreter:

**::safe::interpCreate** ?*child*? ?*options...*?
: Creates a safe interpreter, installs the aliases described in the section **ALIASES** and initializes the auto-loading and package mechanism as specified by the supplied *options*. See the **OPTIONS** section below for a description of the optional arguments. If the *child* argument is omitted, a name will be generated. **::safe::interpCreate** always returns the interpreter name.
    The interpreter name *child* may include namespace separators, but may not have leading or trailing namespace separators, or excess colon characters in namespace separators.  The interpreter name is qualified relative to the global namespace ::, not the namespace in which the **::safe::interpCreate** command is evaluated.

**::safe::interpInit** *child* ?*options...*?
: This command is similar to **interpCreate** except it that does not create the safe interpreter. *child* must have been created by some other means, like **interp create** **-safe**.  The interpreter name *child* may include namespace separators, subject to the same restrictions as for **interpCreate**.

**::safe::interpConfigure** *child* ?*options...*?
: If no *options* are given, returns the settings for all options for the named safe interpreter as a list of options and their current values for that *child*. If a single additional argument is provided, it will return a list of 2 elements *name* and *value* where *name* is the full name of that option and *value* the current value for that option and the *child*. If more than two additional arguments are provided, it will reconfigure the safe interpreter and change each and only the provided options. See the section on **OPTIONS** below for options description. Example of use:

    ```
    # Create new interp with the same configuration as "$i0":
    set i1 [safe::interpCreate {*}[safe::interpConfigure $i0]]
    
    # Get the current deleteHook
    set dh [safe::interpConfigure $i0  -del]
    
    # Change (only) the statics loading ok attribute of an
    # interp and its deleteHook (leaving the rest unchanged):
    safe::interpConfigure $i0  -delete {foo bar} -statics 0
    ```

**::safe::interpDelete** *child*
: Deletes the safe interpreter and cleans up the corresponding parent interpreter data structures. If a *deleteHook* script was specified for this interpreter it is evaluated before the interpreter is deleted, with the name of the interpreter as an additional argument.

**::safe::interpFindInAccessPath** *child directory*
: This command finds and returns the token for the real directory *directory* in the safe interpreter's current virtual access path. It generates an error if the directory is not found. Example of use:

    ```
    $child eval [list set tk_library \
          [::safe::interpFindInAccessPath $name $tk_library]]
    ```

**::safe::interpAddToAccessPath** *child directory*
: This command adds *directory* to the virtual path maintained for the safe interpreter in the parent, and returns the token that can be used in the safe interpreter to obtain access to files in that directory. If the directory is already in the virtual path, it only returns the token without adding the directory to the virtual path again. Example of use:

    ```
    $child eval [list set tk_library \
          [::safe::interpAddToAccessPath $name $tk_library]]
    ```

**::safe::setSyncMode** ?*newValue*?
: This command is used to get or set the "Sync Mode" of the Safe Base. When an argument is supplied, the command returns an error if the argument is not a boolean value, or if any Safe Base interpreters exist.  Typically the value will be set as part of initialization - boolean true for "Sync Mode" on (the default), false for "Sync Mode" off.  With "Sync Mode" on, the Safe Base keeps each child interpreter's ::auto_path synchronized with its access path.  See the section **SYNC MODE** below for details.

**::safe::setLogCmd** ?*cmd arg...*?
: This command installs a script that will be called when interesting life cycle events occur for a safe interpreter. When called with no arguments, it returns the currently installed script. When called with one argument, an empty string, the currently installed script is removed and logging is turned off. The script will be invoked with one additional argument, a string describing the event of interest. The main purpose is to help in debugging safe interpreters. Using this facility you can get complete error messages while the safe interpreter gets only generic error messages. This prevents a safe interpreter from seeing messages about failures and other events that might contain sensitive information such as real directory names.
    Example of use:

    ```
    ::safe::setLogCmd puts stderr
    ```
    Below is the output of a sample session in which a safe interpreter attempted to source a file not found in its virtual access path. Note that the safe interpreter only received an error message saying that the file was not found:

    ```
    NOTICE for child interp10 : Created
    NOTICE for child interp10 : Setting accessPath=(/foo/bar) staticsok=1 nestedok=0 deletehook=()
    NOTICE for child interp10 : auto_path in interp10 has been set to {$p(:0:)}
    ERROR for child interp10 : /foo/bar/init.tcl: no such file or directory
    ```


## Options

The following options are common to **::safe::interpCreate**, **::safe::interpInit**, and **::safe::interpConfigure**. Any option name can be abbreviated to its minimal non-ambiguous name. Option names are not case sensitive.

**-accessPath** *directoryList*
: This option sets the list of directories from which the safe interpreter can **source** and **load** files. If this option is not specified, or if it is given as the empty list, the safe interpreter will use the same directories as its parent for auto-loading. See the section **SECURITY** below for more detail about virtual paths, tokens and access control.

**-autoPath** *directoryList*
: This option sets the list of directories in the safe interpreter's ::auto_path.  The option is undefined if the Safe Base has "Sync Mode" on - in that case the safe interpreter's ::auto_path is managed by the Safe Base and is a tokenized form of its access path. See the section **SYNC MODE** below for details.

**-statics** *boolean*
: This option specifies if the safe interpreter will be allowed to load statically linked packages (like **load {} Tk**). The default value is **true** : safe interpreters are allowed to load statically linked packages.

**-noStatics**
: This option is a convenience shortcut for **-statics false** and thus specifies that the safe interpreter will not be allowed to load statically linked packages.

**-nested** *boolean*
: This option specifies if the safe interpreter will be allowed to load packages into its own sub-interpreters. The default value is **false** : safe interpreters are not allowed to load packages into their own sub-interpreters.

**-nestedLoadOk**
: This option is a convenience shortcut for **-nested true** and thus specifies the safe interpreter will be allowed to load packages into its own sub-interpreters.

**-deleteHook** *script*
: When this option is given a non-empty *script*, it will be evaluated in the parent with the name of the safe interpreter as an additional argument just before actually deleting the safe interpreter. Giving an empty value removes any currently installed deletion hook script for that safe interpreter. The default value (**{}**) is not to have any deletion call back.


# Aliases

The following aliases are provided in a safe interpreter:

**source** *fileName*
: The requested file, a Tcl source file, is sourced into the safe interpreter if it is found. The **source** alias can only source files from directories in the virtual path for the safe interpreter. The **source** alias requires the safe interpreter to use one of the token names in its virtual path to denote the directory in which the file to be sourced can be found. See the section on **SECURITY** for more discussion of restrictions on valid filenames.

**load** *fileName*
: The requested file, a shared object file, is dynamically loaded into the safe interpreter if it is found. The filename must contain a token name mentioned in the virtual path for the safe interpreter for it to be found successfully. Additionally, the shared object file must contain a safe entry point; see the manual page for the **load** command for more details.

**file** ?*subcommand args...*?
: The **file** alias provides access to a safe subset of the subcommands of the **file** command; it allows only **dirname**, **join**, **extension**, **root**, **tail**, **pathtype** and **split** subcommands. For more details on what these subcommands do see the manual page for the **file** command.

**encoding** ?*subcommand args...*?
: The **encoding** alias provides access to a safe subset of the subcommands of the **encoding** command;  it disallows setting of the system encoding, but allows all other subcommands including **system** to check the current encoding.

**exit**
: The calling interpreter is deleted and its computation is stopped, but the Tcl process in which this interpreter exists is not terminated.


# Security

Safe Tcl does not attempt to completely prevent annoyance and denial of service attacks. These forms of attack prevent the application or user from temporarily using the computer to perform useful work, for example by consuming all available CPU time or all available screen real estate. These attacks, while aggravating, are deemed to be of lesser importance in general than integrity and privacy attacks that Safe Tcl is to prevent.

The commands available in a safe interpreter, in addition to the safe set as defined in **interp** manual page, are mediated aliases for **source**, **load**, **exit**, and safe subsets of **file** and **encoding**. The safe interpreter can also auto-load code and it can request that packages be loaded.

Because some of these commands access the local file system, there is a potential for information leakage about its directory structure. To prevent this, commands that take file names as arguments in a safe interpreter use tokens instead of the real directory names. These tokens are translated to the real directory name while a request to, e.g., source a file is mediated by the parent interpreter. This virtual path system is maintained in the parent interpreter for each safe interpreter created by **::safe::interpCreate** or initialized by **::safe::interpInit** and the path maps tokens accessible in the safe interpreter into real path names on the local file system thus preventing safe interpreters from gaining knowledge about the structure of the file system of the host on which the interpreter is executing. The only valid file names arguments for the **source** and **load** aliases provided to the child are path in the form of **[file join** *token filename***]** (i.e. when using the native file path formats: *token***/***filename* on Unix and *token***\***filename* on Windows), where *token* is representing one of the directories of the *accessPath* list and *filename* is one file in that directory (no sub directories access are allowed).

When a token is used in a safe interpreter in a request to source or load a file, the token is checked and translated to a real path name and the file to be sourced or loaded is located on the file system. The safe interpreter never gains knowledge of the actual path name under which the file is stored on the file system.

To further prevent potential information leakage from sensitive files that are accidentally included in the set of files that can be sourced by a safe interpreter, the **source** alias restricts access to files meeting the following constraints: the file name must fourteen characters or shorter, must not contain more than one dot ("**.**"), must end up with the extension ("**.tcl**") or be called ("**tclIndex**".)

Each element of the initial access path list will be assigned a token that will be set in the child **auto_path** and the first element of that list will be set as the **tcl_library** for that child.

If the access path argument is not given to **::safe::interpCreate** or **::safe::interpInit** or is the empty list, the default behavior is to let the child access the same packages as the parent has access to (Or to be more precise: only packages written in Tcl (which by definition cannot be dangerous as they run in the child interpreter) and C extensions that provides a _SafeInit entry point). For that purpose, the parent's **auto_path** will be used to construct the child access path. In order that the child successfully loads the Tcl library files (which includes the auto-loading mechanism itself) the **tcl_library** will be added or moved to the first position if necessary, in the child access path, so the child **tcl_library** will be the same as the parent's (its real path will still be invisible to the child though). In order that auto-loading works the same for the child and the parent in this by default case, the first-level sub directories of each directory in the parent **auto_path** will also be added (if not already included) to the child access path. You can always specify a more restrictive path for which sub directories will never be searched by explicitly specifying your directory list with the **-accessPath** flag instead of relying on this default mechanism.

When the *accessPath* is changed after the first creation or initialization (i.e. through **interpConfigure -accessPath** *list*), an **auto_reset** is automatically evaluated in the safe interpreter to synchronize its **auto_index** with the new token list.

# Typical use

In many cases, the properties of a Safe Base interpreter can be specified when the interpreter is created, and then left unchanged for the lifetime of the interpreter.

If you wish to use Safe Base interpreters with "Sync Mode" off, evaluate the command

```
 safe::setSyncMode 0
```

Use **::safe::interpCreate** or **::safe::interpInit** to create an interpreter with the properties that you require.  The simplest way is not to specify **-accessPath** or **-autoPath**, which means the safe interpreter will use the same paths as the parent interpreter.  However, if **-accessPath** is specified, then **-autoPath** must also be specified, or else it will be set to {}.

The value of **-autoPath** will be that required to access tclIndex and pkgIndex.tcl files according to the same rules as an unsafe interpreter (see pkg_mkIndex(n) and library(n)).

With "Sync Mode" on, the option **-autoPath** is undefined, and the Safe Base sets the child's ::auto_path to a tokenized form of the access path. In addition to the directories present if "Safe Mode" is off, the ::auto_path includes the numerous subdirectories and module paths that belong to the access path.

# Sync mode

Before Tcl version 9.0, the Safe Base kept each safe interpreter's ::auto_path synchronized with a tokenized form of its access path. Limitations of Tcl 8.4 and earlier made this feature necessary.  This definition of ::auto_path did not conform its specification in library(n) and pkg_mkIndex(n), but nevertheless worked perfectly well for the discovery and loading of packages.  The introduction of Tcl modules in Tcl 8.5 added a large number of directories to the access path, and it is inconvenient to have these additional directories unnecessarily appended to the ::auto_path.

In order to preserve compatibility with existing code, this synchronization of the ::auto_path and access path ("Sync Mode" on) is still the default. However, the Safe Base offers the option of limiting the safe interpreter's ::auto_path to the much shorter list of directories that is necessary for it to perform its function ("Sync Mode" off).  Use the command **::safe::setSyncMode** to choose the mode before creating any Safe Base interpreters.

In either mode, the most convenient way to initialize a safe interpreter is to call **::safe::interpCreate** or **::safe::interpInit** without the **-accessPath** or **-autoPath** options (or with the **-accessPath** option set to the empty list), which will give the safe interpreter the same access as the parent interpreter to packages, modules, and autoloader files.  With "Sync Mode" off, the Safe Base will set the value of **-autoPath** to the parent's ::auto_path, and will set the child's ::auto_path to a tokenized form of the parent's ::auto_path.

With "Sync Mode" off, if a value is specified for **-autoPath**, even the empty list, in a call to **::safe::interpCreate**, **::safe::interpInit**, or **::safe::interpConfigure**, it will be tokenized and used as the safe interpreter's ::auto_path.  Any directories that do not also belong to the access path cannot be tokenized and will be silently ignored.  However, the value of **-autoPath** will remain as specified, and will be used to re-tokenize the child's ::auto_path if **::safe::interpConfigure** is called to change the value of **-accessPath**.

With "Sync Mode" off, if the access path is reset to the values in the parent interpreter by calling **::safe::interpConfigure** with arguments **-accessPath** {}, then the ::auto_path will also be reset unless the argument **-autoPath** is supplied to specify a different value.

With "Sync Mode" off, if a non-empty value of **-accessPath** is supplied, the safe interpreter's ::auto_path will be set to {} (by **::safe::interpCreate**, **::safe::interpInit**) or left unchanged (by **::safe::interpConfigure**).  If the same command specifies a new value for **-autoPath**, it will be applied after the **-accessPath** argument has been processed.

Examples of use with "Sync Mode" off: any of these commands will set the ::auto_path to a tokenized form of its value in the parent interpreter:

```
safe::interpCreate foo
safe::interpCreate foo -accessPath {}
safe::interpInit bar
safe::interpInit bar -accessPath {}
safe::interpConfigure foo -accessPath {}
```

Example of use with "Sync Mode" off: when initializing a safe interpreter with a non-empty access path, the ::auto_path will be set to {} unless its own value is also specified:

```
safe::interpCreate foo -accessPath {
    /usr/local/TclHome/lib/tcl9.0
    /usr/local/TclHome/lib/tcl9.0/http1.0
    /usr/local/TclHome/lib/tcl9.0/opt0.4
    /usr/local/TclHome/lib/tcl9.0/msgs
    /usr/local/TclHome/lib/tcl9.0/encoding
    /usr/local/TclHome/lib
}

# The child's ::auto_path must be given a suitable value:

safe::interpConfigure foo -autoPath {
    /usr/local/TclHome/lib/tcl9.0
    /usr/local/TclHome/lib
}

# The two commands can be combined:

safe::interpCreate foo -accessPath {
    /usr/local/TclHome/lib/tcl9.0
    /usr/local/TclHome/lib/tcl9.0/http1.0
    /usr/local/TclHome/lib/tcl9.0/opt0.4
    /usr/local/TclHome/lib/tcl9.0/msgs
    /usr/local/TclHome/lib/tcl9.0/encoding
    /usr/local/TclHome/lib
} -autoPath {
    /usr/local/TclHome/lib/tcl9.0
    /usr/local/TclHome/lib
}
```

Example of use with "Sync Mode" off: the command **safe::interpAddToAccessPath** does not change the safe interpreter's ::auto_path, and so any necessary change must be made by the script:

```
safe::interpAddToAccessPath foo /usr/local/TclHome/lib/extras/Img1.4.11

lassign [safe::interpConfigure foo -autoPath] DUM childAutoPath
lappend childAutoPath /usr/local/TclHome/lib/extras/Img1.4.11
safe::interpConfigure foo -autoPath $childAutoPath
```

