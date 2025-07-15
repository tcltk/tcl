---
CommandName: process
ManualSection: n
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - exec(n)
 - open(n)
 - pid(n)
 - Tcl_DetachPids(3)
 - Tcl_WaitPid(3)
 - Tcl_ReapDetachedProcs(3)
Keywords:
 - background
 - child
 - detach
 - process
 - wait
Copyright:
 - Copyright (c) 2017 Frederic Bonnet.
---

# Name

tcl::process - Subprocess management

# Synopsis

::: {.synopsis} :::
[::tcl::process]{.cmd} [option]{.arg} [arg arg]{.optdot}
:::

# Description

This command provides a way to manage subprocesses created by the \fBopen\fR and \fBexec\fR commands, as identified by the process identifiers (PIDs) of those subprocesses. The legal \fIoptions\fR (which may be abbreviated) are:

**::tcl::process autopurge** ?\fIflag\fR?
: Automatic purge facility. If \fIflag\fR is specified as a boolean value then it activates or deactivate autopurge. In all cases it returns the current status as a boolean value. When autopurge is active, \fBTcl_ReapDetachedProcs\fR is called each time the \fBexec\fR command is executed or a pipe channel created by \fBopen\fR is closed. When autopurge is inactive, \fB::tcl::process\fR purge must be called explicitly. By default autopurge is active.

**::tcl::process list**
: Returns the list of subprocess PIDs. This includes all currently executing subprocesses and all terminated subprocesses that have not yet had their corresponding process table entries purged.

**::tcl::process purge** ?\fIpids\fR?
: Cleans up all data associated with terminated subprocesses. If \fIpids\fR is specified as a list of PIDs then the command only cleans up data for the matching subprocesses if they exist. If a process listed is still active, this command does nothing to that process. Any PID that does not correspond to a subprocess is ignored.

**::tcl::process status** ?\fIswitches\fR? ?\fIpids\fR?
: Returns a dictionary mapping subprocess PIDs to their respective status. If \fIpids\fR is specified as a list of PIDs then the command only returns the status of the matching subprocesses if they exist. Any PID that does not correspond to a subprocess is ignored. For active processes, the status is an empty value. For terminated processes, the status is a list with the following format: "**{***code* ?\fImsg errorCode\fR?\fB}\fR", where:

*code*
: is a standard Tcl return code, i.e., \fB0\fR for TCL_OK and \fB1\fR for TCL_ERROR,

*msg*
: is the human-readable error message,

*errorCode*
: uses the same format as the \fBerrorCode\fR global variable

    Note that \fBmsg\fR and \fBerrorCode\fR are only present for abnormally terminated processes (i.e. those where the \fIcode\fR is nonzero). Under the hood this command calls \fBTcl_WaitPid\fR with the \fBWNOHANG\fR flag set for non-blocking behavior, unless the \fB-wait\fR switch is set (see below).
    Additionally, \fB::tcl::process status\fR accepts the following switches:

**-wait**
: By default the command returns immediately (the underlying \fBTcl_WaitPid\fR is called with the \fBWNOHANG\fR flag set) unless this switch is set. If \fIpids\fR is specified as a list of PIDs then the command waits until the status of the matching subprocesses are available. If \fIpids\fR was not specified, this command will wait for all known subprocesses.

**-\|-**
: Marks the end of switches.  The argument following this one will be treated as the first \fIarg\fR even if it starts with a \fB-\fR.



# Examples

These show the use of \fB::tcl::process\fR. Some of the results from \fB::tcl::process status\fR are split over multiple lines for readability.

```
::tcl::process autopurge
     \(-> true
::tcl::process autopurge false
     \(-> false

set pid1 [exec command1 a b c | command2 d e f &]
     \(-> 123 456
set chan [open "|command1 a b c | command2 d e f"]
     \(-> file123
set pid2 [pid $chan]
     \(-> 789 1011

::tcl::process list
     \(-> 123 456 789 1011

::tcl::process status
     \(-> 123 0
       456 {1 "child killed: write on pipe with no readers" {
         CHILDKILLED 456 SIGPIPE "write on pipe with no readers"}}
       789 {1 "child suspended: background tty read" {
         CHILDSUSP 789 SIGTTIN "background tty read"}}
       1011 {}

::tcl::process status 123
     \(-> 123 0

::tcl::process status 1011
     \(-> 1011 {}

::tcl::process status -wait
     \(-> 123 0
       456 {1 "child killed: write on pipe with no readers" {
         CHILDKILLED 456 SIGPIPE "write on pipe with no readers"}}
       789 {1 "child suspended: background tty read" {
         CHILDSUSP 789 SIGTTIN "background tty read"}}
       1011 {1 "child process exited abnormally" {
         CHILDSTATUS 1011 -1}}

::tcl::process status 1011
     \(-> 1011 {1 "child process exited abnormally" {
         CHILDSTATUS 1011 -1}}

::tcl::process purge
exec command1 1 2 3 &
     \(-> 1213
::tcl::process list
     \(-> 1213
```

