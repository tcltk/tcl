# TIP XXX: Vectorised Execution of Pure Loops
	Author:         Florent Merlet <prenom.nom@gmail.com>
	Author:         Claude (Anthropic)
	State:          Draft
	Type:           Project
	Vote:           Pending
	Created:        24-Nov-2026
	Tcl-Version:    9.2
	Keywords:       foreach, lmap, dict, for, performance, vectorisation
	Post-History:
-----

# Abstract

This TIP lets `foreach`, `lmap`, `dict for`, `dict map` and counting `for` loops run without the bytecode interpreter when their body is a **pure expression**: arithmetic on the loop variables, or an accumulation into one variable. Such a loop is evaluated by a small compiled engine, block by block, 10 to 30 times faster than today. Nothing else changes: results, errors, `errorInfo` and the values of the variables after the loop are those of the ordinary loop. When a loop does not meet the conditions, it runs as it does today.

No syntax is added. The common idioms of existing code are recognised as they are:

```tcl
set d [lmap x $X y $Y {expr {$x*$x + $y*$y}}]
set s 0.0; foreach u $U v $V {set s [expr {$s + $u*$v}]}
```

# Rationale

A loop over a list costs, per element, one round trip through the bytecode interpreter: load the element, set the loop variable, run the body, store the result, test the end. For a body made of a few arithmetic operations, this round trip is most of the time. On a Linux x86-64 machine (gcc -O2), with lists of doubles:

| 100,000 elements | compiled loop today | vectorised |
|---|---:|---:|
| `lmap x $X y $Y {expr {$x*2 + $y}}` | 77 ns per element | 7 ns |
| `foreach … {set s [expr {$s + $u*$v}]}` | 75 ns | 6.5 ns |
| the same, list built in cells by an extension | | 2–3 ns |
| a loop whose body comes from a variable (not compiled) | 1–3 µs | 7 ns |

The usual answers to this cost each have a price. Writing the loop in C (an extension) gives about ×6 on lists of objects, because the C code still reads one `Tcl_Obj` per number; it also leaves the script language. A numeric extension (an array type, NAP, VecTcl) gives more, but asks the programmer to change the representation of the data and to learn a second language. Making `expr` operators act element-wise on lists (`$X * 2`) would break "everything is a string": `{5}` is both a one-element list and the number 5, so the meaning of an expression would depend on the shape of a string.

This TIP takes a different road. The loop is written as today. When its body is a pure expression, the interpreter is taken out of the loop, as a compiler hoists invariant code: the expression is compiled once, the elements are gathered by blocks of 256 into arrays of doubles, and each operation of the expression is applied to the whole block. Loops that are not pure are left alone.

# Specification

## 1. The guarantee

A vectorised loop is indistinguishable from the ordinary loop, except by its duration:

 * the same result (`lmap`, `dict map`) or the same empty result (`foreach`, `dict for`, `for`), with the same string representation;
 * the same errors, with the same message, `-errorcode` and `-errorinfo`;
 * the same values of all variables after the loop, loop variables included (the last element, as today);
 * the same values computed for each element: operations are performed in the same order, with the same rounding, and an accumulation is never reordered (no parallel partial sums).

To keep this guarantee, the engine refuses any loop it cannot run exactly as the ordinary loop would (section 4), and when an element raises an error, the ordinary loop is run again from the start (section 5).

## 2. Commands

 * `foreach varList list ?varList list ...? body` and `lmap varList list ?varList list ...? body`.
 * `dict map {keyVar valueVar} dictionary body`: the keys are kept as they are and in the same order; only the values are computed.
 * `dict for {keyVar valueVar} dictionary body`: accumulations only (a `dict for` without accumulation has no visible effect besides its variables).
 * `for {set i A} {$i OP B} {incr i ?C?} body` where `OP` is `<`, `<=`, `>` or `>=`, `A` and `C` are integers, `B` does not change in the loop, and the body does not assign `i`. The number of iterations is then known in advance; the loop is run as a `foreach` over the arithmetic sequence (the sequence is not built: `i` is generated block by block). After the loop, `i` holds the first value that fails the test, as today.

`while` and other forms of `for` are not covered (see "Future developments").

## 3. Pure bodies

### 3.1 Pure expressions

A pure expression is made of:

 * integer and floating-point literals;
 * variable reads `$name` (scalars);
 * the operators `+ - * / % **`, unary `-` `+` `!`, the comparisons `< <= > >= == !=`, `&&`, `||` and `?:`;
 * the built-in functions `abs`, `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `exp`, `log`, `log10`, `floor`, `ceil`, `round`, `int`, `double`, `pow`, `atan2`, `fmod`, `hypot`, `min`, `max`.

It contains no command substitution, no string operation and no array read. Variables other than loop variables and accumulators are read once, before the loop: since the body is pure, they cannot change during the loop.

### 3.2 Recognised bodies

For `lmap` and `dict map`, the body is one command, `expr {E}`, with `E` a pure expression in braces.

For `foreach`, `dict for` and `for`, the body is one accumulation, or several in sequence:

```tcl
set s [expr {E}]
set s [expr {E1}]; set q [expr {E2}]
```

where `s`, `q` are not loop variables and exist before the loop. Each accumulation gives the new value of its variable for each element, exactly as the ordinary loop does. Accumulations that do not read one another are independent; accumulations that read one another are run in sequence, in the order of the body.

An integer accumulation `incr s ?E?` is recognised too (`E` an integer literal, a loop variable `$v`, or `[expr {E}]`): it is run as `set s [expr {$s + E}]`, and if `s` or any value of `E` is not an integer, the ordinary loop is run again from the start and gives the error of `incr`.

When TIP 759 is present, the forms `{(E)}` and `{(s = E)}` are recognised too. An assignment `(s = E)` whose expression may return one of its operands unchanged (`$u`, `?:`, `min`, `max`, `abs`, `int`, `double`, unary `+`) is only accepted if TIP 759 normalises the assigned value as `expr` does.

### 3.3 Accumulations

When the top of `E` is a binary operation (`+ - * / **`, `pow`, `atan2`, `fmod`, `hypot`) one side of which reads only `s` and fixed values, and the other side does not read `s`, the other side is computed by blocks and only the combination with `s` remains sequential. Dot products (`$s + $u*$v`) and recursive filters (`0.9*$s + 0.1*$u`) fall in this case. Other accumulations are evaluated element by element in the engine, still without the bytecode interpreter.

## 4. Conditions

A loop is vectorised only if all these hold; otherwise it runs as today:

 * each `varList` names one scalar variable, which is neither an array, nor an array element, nor traced (the traces would see each iteration);
 * the lists have the same length (`foreach` and `lmap` pad the shorter lists with empty strings; the engine does not);
 * the lists are long enough for the gain to cover the set-up cost (16 elements in the reference implementation);
 * the body is one of the forms of section 3.2, its expressions are pure, and `expr`, `set` and `incr` resolve to the built-in commands;
 * accumulation variables exist, and are neither traced nor arrays;
 * the interpreter has no command-count limit (`interp limit -commands`): the engine runs no command, so such a limit would never trigger.

## 5. Execution

Elements that are not plain numbers (a string, a big integer, an integer where the expression mixes integer arithmetic) are evaluated one by one by the same rules as `expr`, so `$a / $b` keeps integer division for integers. An operation whose result is not finite is evaluated by `expr` for that element, which returns `Inf` or raises the error `expr` raises.

When the evaluation of an element raises an error, the engine stops, restores the accumulation variables to their value before the loop, and runs the ordinary loop from the start. Since the body is pure, this has no other effect, and the error is the one the ordinary loop raises, with its own `errorInfo`.

The engine checks every 256 elements for time limits, `interp cancel` and pending asynchronous events, so that a long loop stays interruptible as it is today.

## 6. Two levels

**At run time.** `foreach`, `lmap`, `dict for`, `dict map` and `for` consult the vectorisers (section 7) when they are invoked as commands: body from a variable, dynamic invocation, uncompiled code.

**At compile time.** When the body is a literal of one of the forms of section 3.2, the compile procedure emits a guarded call to the engine before the ordinary bytecode. At run time the guard checks the conditions of section 4; if they fail, or if the lists are short, the ordinary bytecode runs, so that small loops keep today's cost.

## 7. C level

A vectoriser is registered with:

```c
typedef int (Tcl_LoopVectorizerProc)(Tcl_Interp *interp, int command,
        Tcl_Size objc, Tcl_Obj *const objv[]);
int Tcl_RegisterLoopVectorizer(const char *name,
        Tcl_LoopVectorizerProc *proc, int priority);
```

`command` tells which loop command is being run (`TCL_LOOP_FOREACH`, `TCL_LOOP_LMAP`, `TCL_LOOP_DICT_FOR`, `TCL_LOOP_DICT_MAP`, `TCL_LOOP_FOR`). The vectorisers are tried by decreasing priority; a vectoriser returns `TCL_CONTINUE` when it cannot run that call exactly as the ordinary loop would, and the result of the loop otherwise. The built-in vectoriser has priority 0. An extension providing a compact representation of numeric lists can register a vectoriser that reads its representation directly; it must give the same guarantee (section 1).

`::tcl::unsupported::vectorize ?on|off?` turns the vectorisers on or off for the interpreter (for testing and comparison).

# Compatibility

No script changes meaning: the guarantee of section 1 holds for every accepted loop, and every other loop runs as today.

Two observable differences remain, both of the kind bytecode compilation already introduced:

 * `info cmdcount` counts fewer commands for a vectorised loop;
 * a loop that runs faster may change the timing of scripts that depend on it.

# Reference Implementation

The reference implementation is part of the typeparse work (TIP 764), in a form independent of types:

 * the engine (pure expression compiler, block evaluation, accumulations) is in `generic/tclTypes.c` (`type map`, `type map -fold`);
 * the registry and the built-in vectoriser are in the same file (`TclVecRegister`, `TclVecEachloop`, `VecTypeMap`);
 * the call from the loop commands is a patch of 11 lines to `EachloopCmd` in `generic/tclCmdAH.c`;
 * the compile-time level is a patch to `CompileEachloopCmd` in `generic/tclCompCmds.c`: when the literal body has a vectorisable form, a call to `::tcl::unsupported::vecloop` is emitted before the ordinary bytecode. It returns `{1 result}`, or `{0 list ...}` with the lists already evaluated, and the ordinary loop then runs on them. Other loops compile as before.

Status: the run-time and compile-time levels for `foreach` and `lmap` are done and tested (bodies `(E)`, `expr {E}`, `(s = E)`, `set s [expr {E}]`, `incr s ?E?`) (Tcl's own `foreach`, `lmap`, `for`, `var` and `trace` tests pass; a differential fuzzer compares results, messages, `errorInfo` and variables with the ordinary loop). To do: several accumulations in one body, `dict map`, `dict for`, counting `for`, the limit checks of section 5, `::tcl::unsupported::vectorize` (an environment variable, `TCL_TYPE_NOVECLOOP`, plays its role today), and moving the engine out of the typeparse files into its own core file.

# Future Developments

**Native execution of loops with a computed end.** `while` loops, and `for` loops whose test depends on the values computed in the body, cannot be vectorised: the number of iterations depends on the data, and each iteration depends on the previous one.

```tcl
while {$s < 1000} {set s [expr {$s * 1.1 + 1}]}
```

The same engine can still run them without the bytecode interpreter: the test and the body (a sequence of pure assignments to a few state variables) are evaluated iteration after iteration in C. The gain is more modest, about ×3 to ×5, since nothing can be computed by blocks; it would cover the classic numerical iterations (Newton's method, sequences and series until convergence). It would follow the same guarantee, the same conditions on traces and limits, and the same fallback.

**Comprehensions in expressions.** With TIP 759, a comprehension written inside an expression could compile directly to the engine.

**Typed representations.** With TIP 764, lists stored in cells (`{-compact}`) and strided views are read in place by the vectoriser, which brings the cost to 2–3 ns per element.

# Rejected Alternatives

**Element-wise operators in `expr`** (`$X * 2` on a list). The meaning of an expression would depend on whether a string looks like a list of one or several elements, and some of today's errors would become silent results.

**A new loop syntax** (`each u of $U do (...)`, `u @ $U : (...)`). It would have to be learnt, and existing code would not benefit. Recognising the existing idioms gives the same speed to all code.

**Parallel reductions.** Summing by blocks in parallel lanes would be faster, but changes the rounding of floating-point sums; the result would no longer be the one of the loop.

**Temporary arrays per operation** (as array libraries do for `X*c - Y*s`). The engine evaluates the whole expression per block, without an intermediate array for each operation.

# Open Issues

 * Whether `Tcl_RegisterLoopVectorizer` belongs to the public stubs table, or stays internal in a first version.
 * The threshold under which the ordinary loop is kept (16 elements in the reference implementation).
 * The normalisation of the value assigned by `(s = E)` in TIP 759 (section 3.2).
 * `for` loops with a floating-point counter: `incr` refuses them today, so they are out of scope; a `set i [expr {$i + 0.1}]` step could be recognised later.

# Copyright

This document has been placed in the public domain.
