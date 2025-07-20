---
CommandName: Tcl_RegisterConfig
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - embedding
 - configuration
 - library
Copyright:
 - Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
---

# Name

Tcl_RegisterConfig - procedures to register embedded configuration information

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_RegisterConfig]{.ccmd}[interp, pkgName, configuration, valEncoding]{.cargs}
:::

# Arguments

.AP Tcl_Interp *interp in Refers to the interpreter the embedded configuration information is registered for. Must not be NULL. .AP "const char" *pkgName in Contains the name of the package registering the embedded configuration as ASCII string. This means that this information is in UTF-8 too. Must not be NULL. .AP "const Tcl_Config" *configuration in Refers to an array of Tcl_Config entries containing the information embedded in the library. Must not be NULL. The end of the array is signaled by either a key identical to NULL, or a key referring to the empty string. .AP "const char" *valEncoding in Contains the name of the encoding used to store the configuration values as ASCII string. This means that this information is in UTF-8 too. Must not be NULL.

# Description

The function described here has its base in TIP 59 and provides extensions with support for the embedding of configuration information into their library and the generation of a Tcl-level interface for querying this information.

To embed configuration information into their library an extension has to define a non-volatile array of Tcl_Config entries in one if its source files and then call **Tcl_RegisterConfig** to register that information.

**Tcl_RegisterConfig** takes four arguments; first, a reference to the interpreter we are registering the information with, second, the name of the package registering its configuration information, third, a pointer to an array of structures, and fourth a string declaring the encoding used by the configuration values.

The string *valEncoding* contains the name of an encoding known to Tcl.  All these names are use only characters in the ASCII subset of UTF-8 and are thus implicitly in the UTF-8 encoding. It is expected that keys are legible English text and therefore using the ASCII subset of UTF-8. In other words, they are expected to be in UTF-8 too. The values associated with the keys can be any string however. For these the contents of *valEncoding* define which encoding was used to represent the characters of the strings.

Each element of the *configuration* array refers to two strings containing the key and the value associated with that key. The end of the array is signaled by either an empty key or a key identical to NULL. The function makes **no** copy of the *configuration* array. This means that the caller has to make sure that the memory holding this array is never released. This is the meaning behind the word **non-volatile** used earlier. The easiest way to accomplish this is to define a global static array of Tcl_Config entries. See the file "generic/tclPkgConfig.c" in the sources of the Tcl core for an example.

When called **Tcl_RegisterConfig** will

1. create a namespace having the provided *pkgName*, if not yet existing.

2. create the command **pkgconfig** in that namespace and link it to the provided information so that the keys from *configuration* and their associated values can be retrieved through calls to **pkgconfig**.


The command **pkgconfig** will provide two subcommands, **list** and **get**:

::*pkgName*::**pkgconfig** list
: Returns a list containing the names of all defined keys.

::*pkgName*::**pkgconfig** get *key*
: Returns the configuration value associated with the specified *key*.


# Tcl_config

The **Tcl_Config** structure contains the following fields:

```
typedef struct {
    const char *key;
    const char *value;
} Tcl_Config;
```

