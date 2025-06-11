---
CommandName: coroutine
ManualSection: n
Version: 8.6
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - apply(n)
 - info(n)
 - proc(n)
 - return(n)
Keywords:
 - coroutine
 - generator
Copyright:
 - Copyright (c) 2009 Donal K. Fellows.
---

# Name

coroutine, yield, yieldto, coroinject, coroprobe - Create and produce values from coroutines

# Synopsis

::: {.synopsis} :::
[coroutine]{.cmd} [name]{.arg} [command]{.arg} [arg...]{.optarg}

[yield]{.cmd} [value]{.optarg}
[yieldto]{.cmd} [command]{.arg} [arg...]{.optarg}
[name]{.ins} [+value...=]{.sub}

[coroinject]{.cmd} [coroName]{.arg} [command]{.arg} [arg...]{.optarg}
[coroprobe]{.cmd} [coroName]{.arg} [command]{.arg} [arg...]{.optarg}
:::

# Description

The **coroutine** command creates a new coroutine context (with associated command) named *name* and executes that context by calling *command*, passing in the other remaining arguments without further interpretation. Once *command* returns normally or with an exception (e.g., an error) the coroutine context *name* is deleted.

Within the context, values may be generated as results by using the **yield** command; if no *value* is supplied, the empty string is used. When that is called, the context will suspend execution and the **coroutine** command will return the argument to **yield**. The execution of the context can then be resumed by calling the context command, optionally passing in the *single* value to use as the result of the **yield** call that caused the context to be suspended. If the coroutine context never yields and instead returns conventionally, the result of the **coroutine** command will be the result of the evaluation of the context.

The coroutine may also suspend its execution by use of the **yieldto** command, which instead of returning, cedes execution to some command called *command* (resolved in the context of the coroutine) and to which *any number* of arguments may be passed. Since every coroutine has a context command, **yieldto** can be used to transfer control directly from one coroutine to another (this is only advisable if the two coroutines are expecting this to happen) but *any* command may be the target. If a coroutine is suspended by this mechanism, the coroutine processing can be resumed by calling the context command optionally passing in an arbitrary number of arguments. The return value of the **yieldto** call will be the list of arguments passed to the context command; it is up to the caller to decide what to do with those values.

The recommended way of writing a version of **yield** that allows resumption with multiple arguments is by using **yieldto** and the **return** command, like this:

```
proc yieldMultiple {value} {
    tailcall yieldto string cat $value
}
```

The coroutine can also be deleted by destroying the command *name*, and the name of the current coroutine can be retrieved by using **info coroutine**. If there are deletion traces on variables in the coroutine's implementation, they will fire at the point when the coroutine is explicitly deleted (or, naturally, if the command returns conventionally).

At the point when *command* is called, the current namespace will be the global namespace and there will be no stack frames above it (in the sense of **upvar** and **uplevel**). However, which command to call will be determined in the namespace that the **coroutine** command was called from.

::: {.info -version="TIP383"}
A suspended coroutine (i.e., one that has **yield**ed or **yieldto**-d) may have its state inspected (or modified) at that point by using **coroprobe** to run a command at the point where the coroutine is at. The command takes the name of the coroutine to run the command in, *coroName*, and the name of a command (any any arguments it requires) to immediately run at that point. The result of that command is the result of the **coroprobe** command, and the gross state of the coroutine remains the same afterwards (i.e., the coroutine is still expecting the results of a **yield** or **yieldto** as before) though variables may have been changed.
:::

Similarly, the **coroinject** command may be used to place a command to be run inside a suspended coroutine (when it is resumed) to process arguments, with quite a bit of similarity to **coroprobe**. However, with **coroinject** there are several key differences:

- The coroutine is not immediately resumed after the injection has been done.  A consequence of this is that multiple injections may be done before the coroutine is resumed. The injected commands are performed in *reverse order of definition* (that is, they are internally stored on a stack).

- An additional two arguments are appended to the list of arguments to be run (that is, the *command* and its *args* are extended by two elements). The first is the name of the command that suspended the coroutine (**yield** or **yieldto**), and the second is the argument (or list of arguments, in the case of **yieldto**) that is the current resumption value.

- The result of the injected command is used as the result of the **yield** or **yieldto** that caused the coroutine to become suspended. Where there are multiple injected commands, the result of one becomes the resumption value processed by the next.


The injection is a one-off. It is not retained once it has been executed. It may **yield** or **yieldto** as part of its execution.

Note that running coroutines may be neither probed nor injected; the operations may only be applied to coroutines that are suspended. (If a coroutine is running then any introspection code would be merely inspecting the state of where it is currently running; **coroinject**/**coroprobe** are unnecessary in that case.)

# Examples

This example shows a coroutine that will produce an infinite sequence of even values, and a loop that consumes the first ten of them.

```
proc allNumbers {} {
    yield
    set i 0
    while 1 {
        yield $i
        incr i 2
    }
}
coroutine nextNumber allNumbers
for {set i 0} {$i < 10} {incr i} {
    puts "received [nextNumber]"
}
rename nextNumber {}
```

In this example, the coroutine acts to add up the arguments passed to it.

```
coroutine accumulator apply {{} {
    set x 0
    while 1 {
        incr x [yield $x]
    }
}}
for {set i 0} {$i < 10} {incr i} {
    puts "$i -> [accumulator $i]"
}
```

This example demonstrates the use of coroutines to implement the classic Sieve of Eratosthenes algorithm for finding prime numbers. Note the creation of coroutines inside a coroutine.

```
proc filterByFactor {source n} {
    yield [info coroutine]
    while 1 {
        set x [$source]
        if {$x % $n} {
            yield $x
        }
    }
}
coroutine allNumbers apply {{} {while 1 {yield [incr x]}}}
coroutine eratosthenes apply {c {
    yield
    while 1 {
        set n [$c]
        yield $n
        set c [coroutine prime$n filterByFactor $c $n]
    }
}} allNumbers
for {set i 1} {$i <= 20} {incr i} {
    puts "prime#$i = [eratosthenes]"
}
```

This example shows how a value can be passed around a group of three coroutines that yield to each other:

```
proc juggler {name target {value ""}} {
    if {$value eq ""} {
        set value [yield [info coroutine]]
    }
    while {$value ne ""} {
        puts "$name : $value"
        set value [string range $value 0 end-1]
        lassign [yieldto $target $value] value
    }
}
coroutine j1 juggler Larry [
    coroutine j2 juggler Curly [
        coroutine j3 juggler Moe j1]] "Nyuck!Nyuck!Nyuck!"
```

::: {.info -version="TIP383"}
This example shows a simple coroutine that collects non-empty values and returns a list of them when not given an argument. It also shows how we can look inside the coroutine to find out what it is doing, and how we can modify the input on a one-off basis.
:::

```
proc collectorImpl {} {
    set me [info coroutine]
    set accumulator {}
    for {set val [yield $me]} {$val ne ""} {set val [yield]} {
        lappend accumulator $val
    }
    return $accumulator
}

coroutine collect collectorImpl
collect 123
collect "abc def"
collect 456

puts [coroprobe collect set accumulator]
# ==> 123 {abc def} 456

collect "pqr"

coroinject collect apply {{type value} {
    puts "Received '$value' at a $type in [info coroutine]"
    return [string toupper $value]
}}

collect rst
# ==> Received 'rst' at a yield in ::collect
collect xyz

puts [collect]
# ==> 123 {abc def} 456 pqr RST xyz
```

## Detailed semantics

This example demonstrates that coroutines start from the global namespace, and that *command* resolution happens before the coroutine stack is created.

```
proc report {where level} {
    # Where was the caller called from?
    set ns [uplevel 2 {namespace current}]
    yield "made $where $level context=$ns name=[info coroutine]"
}
proc example {} {
    report outer [info level]
}
namespace eval demo {
    proc example {} {
        report inner [info level]
    }
    proc makeExample {} {
        puts "making from [info level]"
        puts [coroutine coroEg example]
    }
    makeExample
}
```

Which produces the output below. In particular, we can see that stack manipulation has occurred (comparing the levels from the first and second line) and that the parent level in the coroutine is the global namespace. We can also see that coroutine names are local to the current namespace if not qualified, and that coroutines may yield at depth (e.g., in called procedures).

```
making from 2
made inner 1 context=:: name=::demo::coroEg
```

