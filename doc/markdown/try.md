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

This command executes the script \fIbody\fR and, depending on what the outcome of that script is (normal exit, error, or some other exceptional result), runs a handler script to deal with the case. Once that has all happened, if the \fBfinally\fR clause is present, the \fIscript\fR it includes will be run and the result of the handler (or the \fIbody\fR if no handler matched) is allowed to continue to propagate. Note that the \fBfinally\fR clause is processed even if an error occurs and irrespective of which, if any, \fIhandler\fR is used.

The \fIhandler\fR clauses are each expressed as several words, and must have one of the following forms:

**on** \fIcode variableList script\fR
: This clause matches if the evaluation of \fIbody\fR completed with the exception code \fIcode\fR. The \fIcode\fR may be expressed as an integer or one of the following literal words: \fBok\fR, \fBerror\fR, \fBreturn\fR, \fBbreak\fR, or \fBcontinue\fR. Those literals correspond to the integers 0 through 4 respectively.

**trap** \fIpattern variableList script\fR
: This clause matches if the evaluation of \fIbody\fR resulted in an error and the prefix of the \fB-errorcode\fR from the interpreter's status dictionary is equal to the \fIpattern\fR. The number of prefix words taken from the \fB-errorcode\fR is equal to the list-length of \fIpattern\fR, and inter-word spaces are normalized in both the \fB-errorcode\fR and \fIpattern\fR before comparison.


The \fIvariableList\fR word in each \fIhandler\fR is always interpreted as a list of variable names. If the first word of the list is present and non-empty, it names a variable into which the result of the evaluation of \fIbody\fR (from the main \fBtry\fR) will be placed; this will contain the human-readable form of any errors. If the second word of the list is present and non-empty, it names a variable into which the options dictionary of the interpreter at the moment of completion of execution of \fIbody\fR will be placed.

The \fIscript\fR word of each \fIhandler\fR is also always interpreted the same: as a Tcl script to evaluate if the clause is matched. If \fIscript\fR is a literal "-" and the \fIhandler\fR is not the last one, the \fIscript\fR of the following \fIhandler\fR is invoked instead (just like with the \fBswitch\fR command).

Note that \fIhandler\fR clauses are matched against in order, and that the first matching one is always selected. At most one \fIhandler\fR clause will selected. As a consequence, an \fBon error\fR will mask any subsequent \fBtrap\fR in the \fBtry\fR. Also note that \fBon error\fR is equivalent to \fBtrap {}\fR.

If an exception (i.e. any non-\fBok\fR result) occurs during the evaluation of either the \fIhandler\fR or the \fBfinally\fR clause, the original exception's status dictionary will be added to the new exception's status dictionary under the \fB-during\fR key.

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

Proc to read a file in utf-8 encoding and return its contents. The file is closed in success and error case by the finally clause. It is allowed to call \fBreturn\fR within the \fBtry\fR block. Remark that with tcl 9, the read command may also throw utf-8 conversion errors:

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

