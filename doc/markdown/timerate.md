---
CommandName: timerate
ManualSection: n
Version: unknown
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - time(n)
Keywords:
 - performance measurement
 - script
 - time
Copyright:
 - Copyright (c) 2005 Sergey Brester aka sebres.
---

# Name

timerate - Calibrated performance measurements of script execution time

# Synopsis

::: {.synopsis} :::
[timerate]{.cmd} [script]{.arg} [time]{.optarg} [max-count]{.optarg}
[timerate]{.cmd} [-direct]{.optlit} [[-overhead]{.lit} [estimate]{.arg}]{.optarg} [script]{.arg} [time]{.optarg} [max-count]{.optarg}
[timerate]{.cmd} [-calibrate]{.optlit} [-direct]{.optlit} [script]{.arg} [time]{.optarg} [max-count]{.optarg}
:::

# Description

The \fBtimerate\fR command does calibrated performance measurement of a Tcl command or script, \fIscript\fR. The \fIscript\fR should be written so that it can be executed multiple times during the performance measurement process. Time is measured in elapsed time using the finest timer resolution as possible, not CPU time; if \fIscript\fR interacts with the OS, the cost of that interaction is included. This command may be used to provide information as to how well a script or Tcl command is performing, and can help determine bottlenecks and fine-tune application performance.

The first and second form will evaluate \fIscript\fR until the interval \fItime\fR given in milliseconds elapses, or for 1000 milliseconds (1 second) if \fItime\fR is not specified.

The parameter \fImax-count\fR could additionally impose a further restriction by the maximal number of iterations to evaluate the script. If \fImax-count\fR is specified, the evaluation will stop either this count of iterations is reached or the time is exceeded.

It will then return a canonical Tcl-list of the form:

```
0.095977 \(mcs/# 52095836 # 10419167 #/sec 5000.000 net-ms
```

which indicates:

- the average amount of time required per iteration, in microseconds ([\fBlindex\fR $result 0])

- the count how many times it was executed ([\fBlindex\fR $result 2])

- the estimated rate per second ([\fBlindex\fR $result 4])

- the estimated real execution time without measurement overhead ([\fBlindex\fR $result 6])


The following options may be supplied to the \fBtimerate\fR command:

**-calibrate**
: To measure very fast scripts as exactly as possible, a calibration process may be required. The \fB-calibrate\fR option is used to calibrate \fBtimerate\fR itself, calculating the estimated overhead of the given script as the default overhead for future invocations of the \fBtimerate\fR command. If the \fItime\fR parameter is not specified, the calibrate procedure runs for up to 10 seconds.
    Note that the calibration process is not thread safe in the current implementation.

**-overhead** \fIestimate\fR
: The \fB-overhead\fR parameter supplies an estimate (in microseconds, which may be a floating point number) of the measurement overhead of each iteration of the tested script. The passed value overrides, for the current invocation of \fBtimerate\fR, the overhead estimated by a previous calibration. Overrides may themselves be measured using \fBtimerate\fR as illustrated by a later example.

**-direct**
: The \fB-direct\fR option causes direct execution of the supplied script, without compilation, in a manner similar to the \fBtime\fR command. It can be used to measure the cost of \fBTcl_EvalObjEx\fR, of the invocation of canonical lists, and of the uncompiled versions of bytecoded commands.


As opposed to the \fBtime\fR command, which runs the tested script for a fixed number of iterations, the \fBtimerate\fR command runs it for a fixed time. Additionally, the compiled variant of the script will be used during the entire measurement, as if the script were part of a compiled procedure, if the \fB-direct\fR option is not specified. The fixed time period and possibility of compilation allow for more precise results and prevent very long execution times by slow scripts, making it practical for measuring scripts with highly uncertain execution times.

# Examples

Estimate how fast it takes for a simple Tcl \fBfor\fR loop (including operations on variable \fIi\fR) to count to ten:

```
# calibrate
timerate -calibrate {}

# measure
timerate { for {set i 0} {$i<10} {incr i} {} } 5000
```

Estimate how fast it takes for a simple Tcl \fBfor\fR loop, ignoring the overhead of the management of the variable that controls the loop:

```
# calibrate for overhead of variable operations
set i 0; timerate -calibrate {expr {$i<10}; incr i} 1000

# measure
timerate {
    for {set i 0} {$i<10} {incr i} {}
} 5000
```

Estimate the speed of calculating the hour of the day using \fBclock format\fR only, ignoring overhead of the portion of the script that prepares the time for it to calculate:

```
# calibrate
timerate -calibrate {}

# estimate overhead
set tm 0
set ovh [lindex [timerate -overhead 0 {
    incr tm [expr {24*60*60}]
}] 0]

# measure using estimated overhead
set tm 0
timerate -overhead $ovh {
    clock format $tm -format %H
    incr tm [expr {24*60*60}]; # overhead for this is ignored
} 5000
```

In this last example, note that the overhead itself is measured using \fBtimerate\fR invoked with \fB-overhead 0\fR. This is necessary because explicit overheads are assumed to be absolute values, and not an increment over the default calibrated overhead. It is therefore important that the calibrated overhead is excluded in the measurement of the overhead value itself. This is accomplished by passing \fB-overhead 0\fR when measuring the overhead.

