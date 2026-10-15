# `type deref` and the `var` link type

*Manual for the typed-values extension of Tcl 9.0. Every example below is in
`exemples/deref_manual.tcl` and can be run as a script.*

## 1. Overview

A **link** is a value that holds the *name* of a variable instead of a
value. The `var` type creates links. Reading through a link returns the
variable's value **at the time of the read**, so a structure made of links
always shows current values.

`type deref` is the opposite operation: it takes a structure that contains
links and returns the same structure with each link replaced by the value its
variable has **now**. The result is a *snapshot*: it no longer changes when
the variables do.

```tcl
set A 1; set B 2; set C 3
set {list.var.int}refs A B C        ;# a list of three links
set snap [type deref $refs]         ;# 1 2 3, of type list<int>
```

## 2. The `var` type (links)

### 2.1 Forms

| Type | Meaning |
|---|---|
| `var` | a link to a variable holding any value (the target's value is read as a `string`) |
| `var.T` | a link to a variable whose value must be valid for type `T` |
| `var(typed).T` | like `var.T`, and the variable must itself be a *typed* variable |

`var` is an element type, like any other. It is normally used inside a
collection or a field: `list.var.int`, `dict.var.string`, or a product field
`{var.int}ref`.

### 2.2 What a link contains

A link cell holds the **canonical name** of the variable: `::A` for a global
or namespace variable, or `frame:N:x` for a local variable of a procedure
call. The name is canonicalised when the link is stored, so the link keeps
pointing at the same variable whatever the current namespace is.

```tcl
set A 1; set B 2; set C 3
set {list.var}raw A B
puts $raw                           ;# ::A ::B     (names, not values)
puts $raw(0)                        ;# 1            (reading follows the link)
```

### 2.3 Reading and writing through a link

Reading `$refs(i)` reads the target variable each time. The value is never
copied into the container.

```tcl
set {list.var.int}refs A B C
puts $refs(1)                       ;# 2
set B 20
puts $refs(1)                       ;# 20          (the link follows B)
```

Writing `set refs(i) v` writes **into the target variable**. The container
itself is unchanged, since it only holds the name.

```tcl
set refs(1) 7                       ;# writes into B
puts $B                             ;# 7
puts $refs                          ;# ::A ::B ::C
```

The value must be valid for `T`. With `var.int`, a target holding `abc` makes
the read fail with `expected integer but got "abc"`.

### 2.4 Typed targets: `var(typed)`

`var(typed).T` additionally requires the target to be a typed variable, so
every write through the link is checked by the variable's own type as well:

```tcl
set {int}n 5
set {list.var(typed).int}trefs n    ;# accepted: n is typed
puts $trefs(0)                      ;# 5
set {list.var(typed).int}bad A      ;# error: A is not typed
# variable "::A" must be a typed variable (declared with set {type}name ...)
```

### 2.5 Raw links cannot be traversed

A plain `var` (no `.T`, no `(typed)`) can be read at the end of a path, but a
path cannot go *through* it, because nothing says what the target contains:

```
can't go through the untyped link var with "foo": declare var.T or var(typed)
```

### 2.6 Lifetime

A link to a global or namespace variable is valid as long as the variable
exists. A link to a *local* variable (`frame:N:x`) is valid only while that
procedure call is running. After it returns, reading the link, or applying
`type deref` to it, is an error (see section 5).

## 3. The `type deref` command

```
type deref value
```

`value` must be a typed value, in practice the contents of a typed variable
(`$refs`). `type deref` returns a new value; the argument and the variables
it points to are never modified.

### 3.1 What it does

Every `var` link inside `value` is replaced by the current value of the
variable it names. Lists, dicts, products and sums are rebuilt around the
replaced values, at any depth.

```tcl
set {list.var.int}refs A B C
set snap [type deref $refs]
puts $snap                          ;# 1 2 3
puts [type get $snap]               ;# list<int>

set B 20
puts $refs(1)                       ;# 20        (the links are live)
puts $snap                          ;# 1 2 3     (the snapshot is frozen)
puts [type deref $refs]             ;# 1 20 3    (a new snapshot)
```

A value that contains no link is returned unchanged, with the same type.

### 3.2 The type of the result

The result has the type "without the links":

| Type of the argument | Type of the result |
|---|---|
| `var.T` | `T` |
| `var` | `string` |
| `list.var.int` | `list.int` (same arity, e.g. `list(3).var.int` gives `list(3).int`) |
| `dict.var.int`, `dict(x,y).var.int` | `dict.int`, `dict(x,y).int` (same keys) |
| a product or a sum containing links | a new type named `deref(<original name>)` |
| anything without a link | unchanged |

The `deref(...)` type is built once and reused, so two snapshots of the same
type have the same type, and repeated calls do not grow the type table.

### 3.3 One level at a time

`type deref` follows each link once. If the value of a linked variable
itself contains links, they are **kept**. Call `type deref` again to go one
level deeper. This is what makes cycles safe (section 4.5): the command never
loops.

### 3.4 All or nothing

A broken link stops the whole operation with an error that names the cell
(section 5). No partial result is produced, and nothing is modified.

## 4. Examples

### 4.1 Dicts, products, nested lists

```tcl
set {dict.var.int}dd x A y B
puts [type deref $dd]               ;# x 1 y 2

type def link {{var.int}ref; {string}label}
set {link}p ref A label first
set q [type deref $p]
puts "$q(ref) $q(label)"            ;# 1 first
puts [type get $q]                  ;# deref(link)

set {list.list.var.int}mm {A B} {B A}
puts [type deref $mm]               ;# {1 2} {2 1}
```

### 4.2 Sums

The variant of a sum is preserved, even if the order of the variants changes
in the new type:

```tcl
type def mix {{int}a, {var.int}b, {string}s}
set {mix}s1 {b}A
set t1 [type deref $s1]
puts "$t1 [type get $t1]"           ;# 1 deref(mix).b

set {list.mix}L {b}A {a}7 {b}B
puts [lmap e [type deref $L] {type get $e}]
                                    ;# deref(mix).b deref(mix).a deref(mix).b
```

### 4.3 Trees (recursive types)

```tcl
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}
set {tree(var)}T A {{B {}} {C {}}}  ;# every node value is a link
set dt [type deref $T]
puts $dt                            ;# 1 {{2 {}} {3 {}}}
puts [type get $dt]                 ;# deref(tree(var))
puts $dt(children.0.value)          ;# 2
```

### 4.4 Local variables

A procedure can build links to its own locals, and use them while it runs:

```tcl
proc demo {} {
    set local 42
    set {list.var.int}r local
    return [type deref $r]          ;# 42
}
```

but must not return the links themselves (section 2.6).

### 4.5 Graphs, one level at a time

A graph in which each node lists its neighbours as links:

```tcl
type def node {{string}name; {list.var.node}next}
set {node}na name A next {nb nc}
set {node}nb name B next {nc}
set {node}nc name C next {na}       ;# C points back to A: a cycle

set g1 [type deref $na]
puts $g1                            ;# A {{B ::nc} {C ::na}}
puts "$g1(next.0.name) $g1(next.1.name)"   ;# B C
set g2 [type deref $g1(next.0)]     ;# go one level deeper, from B
puts $g2                            ;# B {{C ::na}}
```

`var.node` becomes `node` in the result without being expanded further, so
the neighbours of A are the values of B and C, and *their* neighbours are
still links. The cycle A → B → C → A never causes a loop.

## 5. Errors

| Situation | Message |
|---|---|
| a target variable does not exist | `type deref: at next.1: can't read "::nc": no such variable` |
| the owning procedure call has returned | `type deref: at 0: can't access "frame:1:local": the procedure call that owned this variable has returned` |
| the argument is not typed | `type deref needs a typed value` |
| wrong number of arguments | `wrong # args: should be "type deref value"` |
| a recursive *collection* (no product or sum) contains links | `type deref: the recursive type L contains links` |

The text after `at` is the path of the failing cell, written with the same
dotted syntax as an index: `next.1` is element 1 of field `next`;
`children.1.value` is the field `value` of the second child.

## 6. Notes and pitfalls

* **A typed variable keeps its type.** A variable that received a typed value
  takes that type and refuses a value of another type. Reusing one name for
  results of different types gives `type mismatch`. Use a fresh name, or
  `unset` first.

  ```tcl
  set {dict.var.int}d x A
  set d [type deref $tree]          ;# error: expected dict<var<int>> ...
  ```

* **Snapshot or live?** Keep the structure of links when you want to follow
  the variables, and call `type deref` when you want a value that will not
  change (to display it, compare two moments, or send it somewhere).

* **Cost.** `type deref` visits every cell of the argument once, so its cost
  is linear in the size of the structure.

* **Not supported.** A recursive collection that has neither a product nor a
  sum in its cycle (`type def L list.L` containing links).
