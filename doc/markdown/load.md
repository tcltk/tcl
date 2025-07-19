---
CommandName: load
ManualSection: n
Version: 7.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - info sharedlibextension
 - package(n)
 - Tcl_StaticLibrary(3)
 - safe(n)
Keywords:
 - binary code
 - dynamic library
 - load
 - safe interpreter
 - shared library
Copyright:
 - Copyright (c) 1995-1996 Sun Microsystems, Inc.
---

# Name

load - Load machine code and initialize new commands

# Synopsis

::: {.synopsis} :::
[load]{.cmd} [-global]{.optlit} [-lazy]{.optlit} [--]{.optlit} [fileName]{.arg}
[load]{.cmd} [-global]{.optlit} [-lazy]{.optlit} [--]{.optlit} [fileName]{.arg} [prefix]{.arg}
[load]{.cmd} [-global]{.optlit} [-lazy]{.optlit} [--]{.optlit} [fileName]{.arg} [prefix]{.arg} [interp]{.arg}
:::

# Description

This command loads binary code from a file into the application's address space and calls an initialization procedure in the library to incorporate it into an interpreter.  *fileName* is the name of the file containing the code;  its exact form varies from system to system but on most systems it is a shared library, such as a **.so** file under Solaris or a DLL under Windows. *prefix* is used to compute the name of an initialization procedure. *interp* is the path name of the interpreter into which to load the library (see the **interp** manual entry for details); if *interp* is omitted, it defaults to the interpreter in which the **load** command was invoked.

Once the file has been loaded into the application's address space, one of two initialization procedures will be invoked in the new code. Typically the initialization procedure will add new commands to a Tcl interpreter. The name of the initialization procedure is determined by *prefix* and whether or not the target interpreter is a safe one.  For normal interpreters the name of the initialization procedure will have the form *prefix***_Init**.  For example, if *prefix* is **Foo**, the initialization procedure's name will be **Foo_Init**.

If the target interpreter is a safe interpreter, then the name of the initialization procedure will be *prefix***_SafeInit** instead of *prefix***_Init**. The *prefix***_SafeInit** function should be written carefully, so that it initializes the safe interpreter only with partial functionality provided by the library that is safe for use by untrusted code. For more information on Safe-Tcl, see the **safe** manual entry.

The initialization procedure must match the following prototype:

```
typedef int Tcl_LibraryInitProc(
        Tcl_Interp *interp);
```

The *interp* argument identifies the interpreter in which the library is to be loaded.  The initialization procedure must return **TCL_OK** or **TCL_ERROR** to indicate whether or not it completed successfully;  in the event of an error it should set the interpreter's result to point to an error message.  The result of the **load** command will be the result returned by the initialization procedure.

The actual loading of a file will only be done once for each *fileName* in an application.  If a given *fileName* is loaded into multiple interpreters, then the first **load** will load the code and call the initialization procedure;  subsequent **load**s will call the initialization procedure without loading the code again. For Tcl versions lower than 8.5, it is not possible to unload or reload a library. From version 8.5 however, the **unload** command allows the unloading of libraries loaded with **load**, for libraries that are aware of the Tcl's unloading mechanism.

The **load** command also supports libraries that are statically linked with the application, if those libraries have been registered by calling the **Tcl_StaticLibrary** procedure. If *fileName* is an empty string, then *prefix* must be specified.

If *prefix* is omitted or specified as an empty string, Tcl tries to guess the prefix by taking the last element of *fileName*, strip off the first three characters if they are **lib**, then strip off the next four characters if they are **tcl9**, and use any following wordchars but not digits, converted to titlecase as the prefix. For example, the command **load libxyz4.2.so** uses the prefix **Xyz** and the command **load bin/last.so {}** uses the prefix **Last**.

If *fileName* is an empty string, then *prefix* must be specified. The **load** command first searches for a statically loaded library (one that has been registered by calling the **Tcl_StaticLibrary** procedure) by that name; if one is found, it is used. Otherwise, the **load** command searches for a dynamically loaded library by that name, and uses it if it is found.  If several different files have been **load**ed with different versions of the library, Tcl picks the file that was loaded first.

If **-global** is specified preceding the filename, all symbols found in the shared library are exported for global use by other libraries. The option **-lazy** delays the actual loading of symbols until their first actual use. The options may be abbreviated. The option **--** indicates the end of the options, and should be used if you wish to use a filename which starts with **-** and you provide a prefix to the **load** command.

On platforms which do not support the **-global** or **-lazy** options, the options still exist but have no effect. Note that use of the **-global** or **-lazy** option may lead to crashes in your application later (in case of symbol conflicts resp. missing symbols), which cannot be detected during the **load**. So, only use this when you know what you are doing, you will not get a nice error message when something is wrong with the loaded library.

# Portability issues

**Windows**
: When a load fails with "library not found" error, it is also possible that a dependent library was not found.  To see the dependent libraries, type "dumpbin -imports <dllname>" in a DOS console to see what the library must import. When loading a DLL in the current directory, Windows will ignore "./" as a path specifier and use a search heuristic to find the DLL instead. To avoid this, load the DLL with:

    ```
    load [file join [pwd] mylib.DLL]
    ```


# Bugs

If the same file is **load**ed by different *fileName*s, it will be loaded into the process's address space multiple times.  The behavior of this varies from system to system (some systems may detect the redundant loads, others may not).

# Example

The following is a minimal extension:

```
#include <tcl.h>
#include <stdio.h>
static int fooCmd(void *clientData,
        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    printf("called with %d arguments\n", objc);
    return TCL_OK;
}
int Foo_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
    printf("creating foo command");
    Tcl_CreateObjCommand(interp, "foo", fooCmd, NULL, NULL);
    return TCL_OK;
}
```

When built into a shared/dynamic library with a suitable name (e.g. **foo.dll** on Windows, **libfoo.so** on Solaris and Linux) it can then be loaded into Tcl with the following:

```
# Load the extension
switch $tcl_platform(platform) {
    windows {
        load [file join [pwd] foo.dll]
    }
    unix {
        load [file join [pwd] libfoo[info sharedlibextension]]
    }
}

# Now execute the command defined by the extension
foo
```

