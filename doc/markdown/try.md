---
CommandName: try
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - catch(n)
 - error(n)
 - return(n)
 - throw(n)
Keywords:
 - cleanup
 - error
 - exception
 - final
 - resource management
Copyright:
 - Copyright (c) 2008 Donal K. Fellows
---

# Name

try - Trap and process errors and exceptions

# Synopsis

::: {.synopsis} :::
[try]{.cmd} [body]{.arg} [handler...]{.optarg} [[finally]{.lit} [script]{.arg}]{.optarg}
:::

# Description

This command executes the script *body* and, depending on what the outcome of that script is (normal exit, error, or some other exceptional result), runs a handler script to deal with the case. Once that has all happened, if the **finally** clause is present, the *script* it includes will be run and the result of the handler (or the *body* if no handler matched) is allowed to continue to propagate. Note that the **finally** clause is processed even if an error occurs and irrespective of which, if any, *handler* is used.

The *handler* clauses are each expressed as several words, and must have one of the following forms:

**on** *code variableList script*
: This clause matches if the evaluation of *body* completed with the exception code *code*. The *code* may be expressed as an integer or one of the following literal words: **ok**, **error**, **return**, **break**, or **continue**. Those literals correspond to the integers 0 through 4 respectively.

**trap** *pattern variableList script*
: This clause matches if the evaluation of *body* resulted in an error and the prefix of the **-errorcode** from the interpreter's status dictionary is equal to the *pattern*. The number of prefix words taken from the **-errorcode** is equal to the list-length of *pattern*, and inter-word spaces are normalized in both the **-errorcode** and *pattern* before comparison.


The *variableList* word in each *handler* is always interpreted as a list of variable names. If the first word of the list is present and non-empty, it names a variable into which the result of the evaluation of *body* (from the main **try**) will be placed; this will contain the human-readable form of any errors. If the second word of the list is present and non-empty, it names a variable into which the options dictionary of the interpreter at the moment of completion of execution of *body* will be placed.

The *script* word of each *handler* is also always interpreted the same: as a Tcl script to evaluate if the clause is matched. If *script* is a literal "-" and the *handler* is not the last one, the *script* of the following *handler* is invoked instead (just like with the **switch** command).

Note that *handler* clauses are matched against in order, and that the first matching one is always selected. At most one *handler* clause will selected. As a consequence, an **on error** will mask any subsequent **trap** in the **try**. Also note that **on error** is equivalent to **trap {}**.

If an exception (i.e. any non-**ok** result) occurs during the evaluation of either the *handler* or the **finally** clause, the original exception's status dictionary will be added to the new exception's status dictionary under the **-during** key.

# Examples

Ensure that a file is closed no matter what:

```
set f [open /some/file/name a]
try {
    puts $f "some message"
    # ...
} finally {
    close $f
}
```

Handle different reasons for a file to not be openable for reading:

```
try {
    set f [open /some/file/name r]
} trap {POSIX EISDIR} {} {
    puts "failed to open /some/file/name: it's a directory"
} trap {POSIX ENOENT} {} {
    puts "failed to open /some/file/name: it doesn't exist"
}
```

Proc to read a file in utf-8 encoding and return its contents. The file is closed in success and error case by the finally clause. It is allowed to call **return** within the **try** block. Remark that with tcl 9, the read command may also throw utf-8 conversion errors:

```
proc readfile {filename} {
    set f [open $filename r]
    try {
        fconfigure $f -encoding utf-8 -profile strict
        return [read $f]
    } finally {
        close $f
    }
}
```

