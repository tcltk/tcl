---
CommandName: registry
ManualSection: n
Version: 1.1
TclPart: registry
TclDescription: Tcl Bundled Packages
Keywords:
 - registry
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
 - Copyright (c) 2002 ActiveState Corporation.
---

# Name

registry - Manipulate the Windows registry

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [registry]{.lit} [1.4]{.lit}

[registry]{.cmd} [-mode]{.optarg} [option]{.arg} [keyName]{.arg} [arg arg]{.optdot}
:::

# Description

The **registry** package provides a general set of operations for manipulating the Windows registry.  The package implements the **registry** Tcl command.  This command is only supported on the Windows platform.  Warning: this command should be used with caution as a corrupted registry can leave your system in an unusable state.

*KeyName* is the name of a registry key.  Registry keys must be one of the following forms:

**\\***hostname***\***rootname***\***keypath*

*rootname***\***keypath*

*rootname*

*Hostname* specifies the name of any valid Windows host that exports its registry.  The *rootname* component must be one of **HKEY_LOCAL_MACHINE**, **HKEY_USERS**, **HKEY_CLASSES_ROOT**, **HKEY_CURRENT_USER**, **HKEY_CURRENT_CONFIG**, **HKEY_PERFORMANCE_DATA**, or **HKEY_DYN_DATA**.  The *keypath* can be one or more registry key names separated by backslash (**\**) characters.

The optional *-mode* argument indicates which registry to work with; when it is **-32bit** the 32-bit registry will be used, and when it is **-64bit** the 64-bit registry will be used. If this argument is omitted, the system's default registry will be the subject of the requested operation.

*Option* indicates what to do with the registry key name.  Any unique abbreviation for *option* is acceptable.  The valid options are:

**registry broadcast** *keyName* ?**-timeout** *milliseconds*?
: Sends a broadcast message to the system and running programs to notify them of certain updates.  This is necessary to propagate changes to key registry keys like Environment.  The timeout specifies the amount of time, in milliseconds, to wait for applications to respond to the broadcast message. It defaults to 3000.  The following example demonstrates how to add a path to the global Environment and notify applications of the change without requiring a logoff/logon step (assumes admin privileges):

    ```
    set regPath [join {
        HKEY_LOCAL_MACHINE
        SYSTEM
        CurrentControlSet
        Control
        {Session Manager}
        Environment
    } "\\"]
    set curPath [registry get $regPath "Path"]
    registry set $regPath "Path" "$curPath;$addPath"
    registry broadcast "Environment"
    ```

**registry delete** *keyName* ?*valueName*?
: If the optional *valueName* argument is present, the specified value under *keyName* will be deleted from the registry.  If the optional *valueName* is omitted, the specified key and any subkeys or values beneath it in the registry hierarchy will be deleted.  If the key could not be deleted then an error is generated.  If the key did not exist, the command has no effect.

**registry get** *keyName valueName*
: Returns the data associated with the value *valueName* under the key *keyName*.  If either the key or the value does not exist, then an error is generated.  For more details on the format of the returned data, see **SUPPORTED TYPES**, below.

**registry keys** *keyName* ?*pattern*?
: If *pattern* is not specified, returns a list of names of all the subkeys of *keyName*.  If *pattern* is specified, only those names matching *pattern* are returned.  Matching is determined using the same rules as for **string match**.  If the specified *keyName* does not exist, then an error is generated.

**registry set** *keyName* ?*valueName data* ?*type*??
: If *valueName* is not specified, creates the key *keyName* if it does not already exist.  If *valueName* is specified, creates the key *keyName* and value *valueName* if necessary.  The contents of *valueName* are set to *data* with the type indicated by *type*.  If *type* is not specified, the type **sz** is assumed.  For more details on the data and type arguments, see **SUPPORTED TYPES** below.

**registry type** *keyName valueName*
: Returns the type of the value *valueName* in the key *keyName*.  For more information on the possible types, see **SUPPORTED TYPES**, below.

**registry values** *keyName* ?*pattern*?
: If *pattern* is not specified, returns a list of names of all the values of *keyName*.  If *pattern* is specified, only those names matching *pattern* are returned.  Matching is determined using the same rules as for **string match**.


# Supported types

Each value under a key in the registry contains some data of a particular type in a type-specific representation.  The **registry** command converts between this internal representation and one that can be manipulated by Tcl scripts.  In most cases, the data is simply returned as a Tcl string.  The type indicates the intended use for the data, but does not actually change the representation.  For some types, the **registry** command returns the data in a different form to make it easier to manipulate.  The following types are recognized by the registry command:

**binary**
: The registry value contains arbitrary binary data.  The data is represented exactly in Tcl, including any embedded nulls.

**none**
: The registry value contains arbitrary binary data with no defined type.  The data is represented exactly in Tcl, including any embedded nulls.

**sz**
: The registry value contains a null-terminated string.  The data is represented in Tcl as a string.

**expand_sz**
: The registry value contains a null-terminated string that contains unexpanded references to environment variables in the normal Windows style (for example, "%PATH%"). The data is represented in Tcl as a string.

**dword**
: The registry value contains a little-endian 32-bit number.  The data is represented in Tcl as a decimal string.

**dword_big_endian**
: The registry value contains a big-endian 32-bit number.  The data is represented in Tcl as a decimal string.

**link**
: The registry value contains a symbolic link.  The data is represented exactly in Tcl, including any embedded nulls.

**multi_sz**
: The registry value contains an array of null-terminated strings.  The data is represented in Tcl as a list of strings.

**resource_list**
: The registry value contains a device-driver resource list.  The data is represented exactly in Tcl, including any embedded nulls.


In addition to the symbolically named types listed above, unknown types are identified using a 32-bit integer that corresponds to the type code returned by the system interfaces.  In this case, the data is represented exactly in Tcl, including any embedded nulls.

# Portability issues

The registry command is only available on Windows.

# Example

Print out how double-clicking on a Tcl script file will invoke a Tcl interpreter:

```
package require registry
set ext .tcl

# Read the type name
set type [registry get HKEY_CLASSES_ROOT\\$ext {}]
# Work out where to look for the command
set path HKEY_CLASSES_ROOT\\$type\\Shell\\Open\\command
# Read the command!
set command [registry get $path {}]

puts "$ext opens with $command"
```

