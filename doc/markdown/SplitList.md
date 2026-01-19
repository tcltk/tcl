---
CommandName: Tcl_SplitList
ManualSection: 3
Version: 8.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_ListObjGetElements(3)
Keywords:
 - backslash
 - convert
 - element
 - list
 - merge
 - split
 - strings
Copyright:
 - Copyright (c) 1989-1993 The Regents of the University of California.
 - Copyright (c) 1994-1996 Sun Microsystems, Inc.
---

# Name

Tcl\_SplitList, Tcl\_Merge, Tcl\_ScanElement, Tcl\_ConvertElement, Tcl\_ScanCountedElement, Tcl\_ConvertCountedElement - manipulate Tcl lists

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_SplitList]{.ccmd}[interp, list, argcPtr, argvPtr]{.cargs}
[char \*]{.ret} [Tcl\_Merge]{.ccmd}[argc, argv]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ScanElement]{.ccmd}[src, flagsPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ScanCountedElement]{.ccmd}[src, length, flagsPtr]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ConvertElement]{.ccmd}[src, dst, flags]{.cargs}
[Tcl\_Size]{.ret} [Tcl\_ConvertCountedElement]{.ccmd}[src, length, dst, flags]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp out Interpreter to use for error reporting.  If NULL, then no error message is left. .AP "const char" \*list in Pointer to a string with proper list structure. .AP "Tcl\_Size | int" \*argcPtr out Filled in with number of elements in *list*. May be (Tcl\_Size \*)NULL when not used. If it points to a variable which type is not **Tcl\_Size**, a compiler warning will be generated. If your extensions is compiled with **-DTCL\_8\_API**, this function will return TCL\_ERROR for lists with more than INT\_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP "const char" \*\*\*argvPtr out *\*argvPtr* will be filled in with the address of an array of pointers to the strings that are the extracted elements of *list*. There will be *\*argcPtr* valid entries in the array, followed by a NULL entry. .AP Tcl\_Size argc in Number of elements in *argv*. .AP "const char \*const" \*argv in Array of strings to merge together into a single list. Each string will become a separate element of the list. .AP "const char" \*src in String that is to become an element of a list. .AP int \*flagsPtr in Pointer to word to fill in with information about *src*. The value of \**flagsPtr* must be passed to **Tcl\_ConvertElement**. .AP Tcl\_Size length in Number of bytes in string *src*. .AP char \*dst in Place to copy converted list element.  Must contain enough characters to hold converted string. .AP int flags in Information about *src*. Must be value returned by previous call to **Tcl\_ScanElement**, possibly OR-ed with **TCL\_DONT\_USE\_BRACES**.

# Description

These procedures may be used to disassemble and reassemble Tcl lists. **Tcl\_SplitList** breaks a list up into its constituent elements, returning an array of pointers to the elements using *argcPtr* and *argvPtr*. While extracting the arguments, **Tcl\_SplitList** obeys the usual rules for backslash substitutions and braces.  The area of memory pointed to by *\*argvPtr* is dynamically allocated;  in addition to the array of pointers, it also holds copies of all the list elements.  It is the caller's responsibility to free up all of this storage. For example, suppose that you have called **Tcl\_SplitList** with the following code:

```
Tcl_Size argc;
int code;
char *string;
char **argv;
...
code = Tcl_SplitList(interp, string, &argc, &argv);
```

Then you should eventually free the storage with a call like the following:

```
Tcl_Free(argv);
```

**Tcl\_SplitList** normally returns **TCL\_OK**, which means the list was successfully parsed. If *sizePtr* points to a variable of type **int** and the list contains more than 2\*\*31 key/value pairs, or there was a syntax error in *list*, then **TCL\_ERROR** is returned and the interpreter's result will point to an error message describing the problem (if *interp* was not NULL). If **TCL\_ERROR** is returned then no memory is allocated and *\*argvPtr* is not modified.

**Tcl\_Merge** is the inverse of **Tcl\_SplitList**:  it takes a collection of strings given by *argc* and *argv* and generates a result string that has proper list structure. This means that commands like **index** may be used to extract the original elements again. In addition, if the result of **Tcl\_Merge** is passed to **Tcl\_Eval**, it will be parsed into *argc* words whose values will be the same as the *argv* strings passed to **Tcl\_Merge**. **Tcl\_Merge** will modify the list elements with braces and/or backslashes in order to produce proper Tcl list structure. The result string is dynamically allocated using **Tcl\_Alloc**;  the caller must eventually release the space using **Tcl\_Free**.

If the result of **Tcl\_Merge** is passed to **Tcl\_SplitList**, the elements returned by **Tcl\_SplitList** will be identical to those passed into **Tcl\_Merge**. However, the converse is not true:  if **Tcl\_SplitList** is passed a given string, and the resulting *argc* and *argv* are passed to **Tcl\_Merge**, the resulting string may not be the same as the original string passed to **Tcl\_SplitList**. This is because **Tcl\_Merge** may use backslashes and braces differently than the original string.

**Tcl\_ScanElement** and **Tcl\_ConvertElement** are the procedures that do all of the real work of **Tcl\_Merge**. **Tcl\_ScanElement** scans its *src* argument and determines how to use backslashes and braces when converting it to a list element. It returns an overestimate of the number of characters required to represent *src* as a list element, and it stores information in *\*flagsPtr* that is needed by **Tcl\_ConvertElement**.

**Tcl\_ConvertElement** is a companion procedure to **Tcl\_ScanElement**. It does the actual work of converting a string to a list element. Its *flags* argument must be the same as the value returned by **Tcl\_ScanElement**. **Tcl\_ConvertElement** writes a proper list element to memory starting at \**dst* and returns a count of the total number of characters written, which will be no more than the result returned by **Tcl\_ScanElement**. **Tcl\_ConvertElement** writes out only the actual list element without any leading or trailing spaces: it is up to the caller to include spaces between adjacent list elements.

**Tcl\_ConvertElement** uses one of two different approaches to handle the special characters in *src*.  Wherever possible, it handles special characters by surrounding the string with braces. This produces clean-looking output, but cannot be used in some situations, such as when *src* contains unmatched braces. In these situations, **Tcl\_ConvertElement** handles special characters by generating backslash sequences for them. The caller may insist on the second approach by OR-ing the flag value returned by **Tcl\_ScanElement** with **TCL\_DONT\_USE\_BRACES**. Although this will produce an uglier result, it is useful in some special situations, such as when **Tcl\_ConvertElement** is being used to generate a portion of an argument for a Tcl command. In this case, surrounding *src* with curly braces would cause the command not to be parsed correctly.

By default, **Tcl\_ConvertElement** will use quoting in its output to be sure the first character of an element is not the hash character ("#".) This is to be sure the first element of any list passed to [eval] is not mis-parsed as the beginning of a comment. When a list element is not the first element of a list, this quoting is not necessary.  When the caller can be sure that the element is not the first element of a list, it can disable quoting of the leading hash character by OR-ing the flag value returned by **Tcl\_ScanElement** with **TCL\_DONT\_QUOTE\_HASH**.

**Tcl\_ScanCountedElement** and **Tcl\_ConvertCountedElement** are the same as **Tcl\_ScanElement** and **Tcl\_ConvertElement**, except the length of string *src* is specified by the *length* argument, and the string may contain embedded nulls.


[eval]: eval.md

