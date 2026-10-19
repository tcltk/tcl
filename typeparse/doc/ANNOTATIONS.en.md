# Annotations in Tcl — documentation

*19 October 2026 · version v13 · Florent MERLET, with Claude (Anthropic)*

*Other languages: [Français](ANNOTATIONS.fr.md) (reference) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md)*

## Foreword

**Authors**: Florent MERLET, with Claude (Anthropic).

This document describes the **annotations** of Tcl 9.0 proposed by TIP 764: a piece of metadata attached to a word (`{x}word`) that does not change the value, but lets you type variables, name fields and describe binary formats. It replaces the manual written along the way during development; it describes the prototype as of **version v13**.

Three parts, for three readers:

- **Part I — Concepts**: what an annotation is, the difference between a note and a type, and their rules. Read this first.
- **Part II — Reference**: the `note` and `type` commands, the type language, paths, building values, binary types and errors. For people who write Tcl.
- **Part III — Implementation**: the annotated object, the parser, the bytecode, the files and functions involved. For people who review the TIP or the code.

Then come compatibility and performance, design choices, limits, and appendices (installation, glossary, correspondences with C, Rust and LLVM).

The examples are Tcl that runs on the prototype; `;#` comments give the result. Error messages are quoted as Tcl produces them. Identifiers in the examples (`nom`, `fiche`, `couleur`, `grandeur`…) are kept as in the French original, so that they match the example scripts.

## Part I — Concepts

### 1. Annotations

#### 1.1 Where the word comes from

*To annotate* comes from the Latin *ad notare*, "to put a note beside". The word *protocol* says the same thing in another form: the Greek *prōtokollon* was the first sheet glued to the head of a papyrus roll, describing its content without being part of it. A Tcl annotation is exactly that: a sheet glued to the head of a word.

```tcl
{mm}12          ;# the value is still 12; "mm" is glued in front
{list.int}L     ;# the name is still L; "list.int" says what it will hold
```

#### 1.2 A piece of metadata

An annotation is **metadata**:

- it **does not appear in the text** of the value: `{mm}12` is `12` for `puts`, `expr`, `string length`;
- it **adds information** the value alone does not carry: a unit, a type, a field name, an option;
- it lives **outside the plane of raw values**: no existing Tcl command sees it, and no existing script changes meaning.

#### 1.3 An analogy: the imaginary part

A complex number has a real part and an imaginary part. In the same way, an annotated value has a **raw** part (the text Tcl has always handled) and an **annotated** part (what the developer has in mind).

| | complex number | annotated value |
|---|---|---|
| visible part | the real part | the raw value (`12`) |
| added part | the imaginary part | the annotation (`mm`, `int`, `nom`) |
| what you usually read | the real part | the string |

The analogy stops at computation. In a complex number, the imaginary part takes part in arithmetic. Here, **a computation loses the annotation**: `expr {$x + 1}` returns a bare number. Only copying, `lindex`, `string cat` and passing as an argument keep it. This is on purpose: the plane of raw values stays ordinary Tcl, and that is what guarantees compatibility.

#### 1.4 Guiding execution

An annotation makes explicit the developer's "imaginary part", what they know about their data and Tcl did not:

- a **type** has every write to a variable checked (`set {int}n abc` fails);
- a **field name** chooses where a value goes (`set {pt}a {age}12 {nom}Jean`);
- a **binary representation** says how to read and write bytes (`{int(u,16,be)}version`);
- a **note** marks a value so that a procedure can recognize it (`note has $v mm`).

The script keeps its shape: you add annotations where you want guarantees, and nowhere else.

### 2. Notes, types and local annotations

An annotation is always written `{text}word`. Depending on what the text says and on **who reads it**, it plays three roles.

#### 2.1 The note: the raw annotation

A **note** is an annotation that no constructor has resolved yet. It follows the value: copies, procedure arguments, list elements.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (empty: no resolved type)
```

**Stacking.** You annotate a value that is already annotated; notes stack up, from the oldest to the most recent.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px is the most recent)
```

The form `{a}{b}$x` (two annotations glued together) does not exist: it would be ambiguous with `{int}{300}`, an annotation on a braced literal. You stack by annotating a value that already is.

#### 2.2 The type: the annotation resolved by a constructor

A **type** is an annotation **resolved** in the typology the interpreter knows (base types, `type def`, `type alias`). Only a **variable constructor** resolves:

- `set {T}v …` (and `set v {T}$x`);
- binding of `proc` arguments;
- `lassign`, `foreach`, and in `expr`, `x = v` and `x is T = v`.

Annotated on the **name** of a variable, an annotation gives the variable a **contract**: every later write is checked.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Outside a constructor, `{T}$x` checks nothing and converts nothing, even when `T` is a known type: `T` is simply **pushed**, like a note. `list {int}abc` raises no error.

#### 2.3 Two stacks and an unstacking rule

An annotated value carries **two lists**: the **resolved type** (at most one) and the **stack of pending notes**. The display separates them with `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 resolved type   notes, from the oldest (mm) to the most recent (hello)
```

A constructor **unstacks** from the top:

1. it takes the most recent note and tries to resolve it;
2. it goes on while notes resolve; the first that does not stops everything and stays on the stack;
3. the unstacked types must be consistent: the most recent must fit into each of the others, otherwise `contradictory annotations px and qq`;
4. the value is converted ("cast") to the most recent type;
5. the variable's contract makes the last check.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px resolves, mm does not
type get $z          ;# px      ; note get $z: mm
```

A note set **before** its `type def` is therefore resolved by the next constructor: writing `type def mm double` later gives meaning to the `{mm}` already set.

**Context first.** A constructor resolves in its context before global types: for a sum, the variant names (`{i}5` in `set {S}v {i}5`); for a product, the field names (`{age}12`). A global type with the same name changes nothing.

#### 2.4 Local annotations

An annotation whose text contains a character that type names refuse (`-`, `?`, `:`…) can **never** be a type. It is a **local annotation**: it only lives between the caller and the called command, and does not follow the value.

```tcl
type binary {sizeof?}$T          ;# a question asked of type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# refused: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# refused: local annotation {-v} cannot declare a variable
```

Two natural families, with no risk of collision with a type name: options as `-name` and questions as `name?`. A type name contains only ASCII letters, digits, `_` and non-ASCII bytes, and does not start with a digit.

#### 2.5 The empty annotation `{}`

`{}` is "the empty type". Its effect depends on where it is:

| where | effect |
|---|---|
| on a value, in a constructor (`set v {}$x`) | removes the resolved type, keeps the notes |
| on a name (`set {}v $x`) | lifts the variable's contract; the value is stored with its notes, resolving nothing |
| on a value, outside a constructor | the reader sees the bare content (type and notes erased) |

```tcl
set {int}n 5
set {}n abc          ;# the contract is lifted: n is abc
```

The original value is never modified: `{}` produces a new view, which makes sharing objects safe.

## Part II — Reference

### 3. The `note` command

```
note get VALUE               ;# the notes, from the oldest to the most recent, joined by "."
note has VALUE NAME          ;# 1 if NAME is one of the notes, else 0
note delete VALUE ?NAME?     ;# the value without the note NAME, or without any note
```

Tcl values are immutable: `note delete` **returns** a new value, it does not modify a variable.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: no glued stacking
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Typical use: a procedure that reacts to a value's mark.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. The `type` command

```
type def NAME DEFINITION       ;# defines a nominal type (or a parameterized family)
type alias NAME DEFINITION     ;# gives a structural name to a definition
type get VALUE                 ;# the resolved type of a value (empty if none)
type cast TYPE VALUE           ;# a constructor's resolver, without a variable
type convert FROM TO LAMBDA    ;# declares an explicit conversion
type deref VALUE               ;# snapshot: each var link replaced by its value
type binary TYPE ...           ;# binary representation, encoding, decoding (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (an alias is not a new type)
type cast pt {Léa 7}          ;# Léa 7, of type pt
```

In `expr`, the **`is`** operator tests a variable's type, and an assignment can declare:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# declares z of type pt and assigns the value
expr {new is cmd}              ;# 1 if new is a command; new(…) becomes an expr function
```

`is cmd` is not a type: it tests whether a command exists and exposes it as an `expr` function.

### 5. The type system

#### 5.1 Base types

| type | values |
|---|---|
| `int` | integer (unbounded, as in Tcl) |
| `double` | floating point |
| `string` | any string |
| `list` | ordinary Tcl list (untyped elements) |
| `dict` | ordinary Tcl dictionary |
| `var` | link to a variable, by name (§ 5.3) |
| `any` | element of a heterogeneous list (`list.any`): each element keeps its type |

**Refinements** restrict a base type and carry a binary representation: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

A `type def` creates a **nominal** type: `type def port int(u,16)` is distinct from `int(u,16)`. A `type alias` only gives a name to a definition.

#### 5.2 The composition language

A type is written as an expression, parsed on the model of `expr` (`tclTypeParse.c` follows the structure of `tclCompExpr.c`).

**Three kinds of operands:**

| operand | where | example |
|---|---|---|
| type | bare word, left of a `}` or in operand position | `int`, `list`, `pt` |
| name | word glued to the right of a `}` | `{int}age`: the field `age` |
| value | in double quotes, in braces, or after `=` | `"MZ"`, `= 0` |

**Operators**, from the strongest to the weakest:

| operator | role | example |
|---|---|---|
| `.` | nesting (right-associative) | `list.dict.int` |
| `( )` | arguments, glued to the name | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` or `?=` | default value of a field | `{int}age ?= 0` |
| `;` | product | `{double}x; {double}y` |
| `,` | sum | `{int}i, {string}s` |
| `{ }name` | grouping, with a name glued to the closing brace | `{{int}x; {int}y}A` |

`;` and `,` have the same precedence: to mix a product and a sum, group with braces (`{int}a; {{int}b, {string}c}u`).

**Characters.** A type name contains only ASCII letters, digits, `_` and non-ASCII bytes, and does not start with a digit. The grammar also uses `{ } , ; . = " # ( ) * $`. Any other ASCII sign (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) is refused: `invalid character "!"`, code `TCL PARSE TYPE BADCHAR`, except after `=` (a default value) and inside a constructor's parentheses (`*(file://x, …)`). This refusal is what makes local annotations (`{-v}`, `{is?}`) safe.

A definition is **declarative**: no substitution happens when it is read, except in a default value, evaluated at each construction.

#### 5.3 Fields and containers

**Enumeration** — a sum of literals.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# a single literal: a constant (file signature)
```

**Sum** — a single variant at a time, carrying its name.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# the variant chosen by the type's name
```

**Product** — a `struct`: named, ordered fields, each of its own type.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positional
set {pt}b {age}12 {nom}Jean    ;# field tags (§ 8)
$a(nom)                        ;# Jean
```

**List** — `list` (extensible), `list.T` (elements of type T), `list(N).T` (exactly N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

The elements of a `list.T` are atomic: `lindex`, `foreach`, `$L(i)` return typed elements, never split again into words. The bare list `{list}` keeps Tcl's behaviour.

**Dictionary** — `dict` (any keys), `dict.T` (values of type T), `dict(keys).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom mandatory; age optional, 0 by default
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z may be missing; `dict()`: only the empty key. Keys are always text; only values are typed.

**`var` link** — the NAME of a variable, followed on each read.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (a snapshot, of type list.int)
```

**Type parameter `{type}T`** — in a parameterized type, a parameter that receives a type (§ 5.5).

**Binary link `*(origin,repr).T`** — an address in binary data, followed to a value T (§ 9.5).

#### 5.4 Default values

`?=` (or `=`, the older form, still accepted in a product) gives a field a default. A default can be a literal, a `[script]` evaluated at each construction, or a `$variable` read at each construction.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

A field without a default must be given: `field "x" of pt has no value and no default`.

#### 5.5 Parameterized types

The name in a `type def` can carry parameters, declared like fields: `{type}T` (a type), `{int}n`, `{double}f`, `{string}s`. They are used in the body with `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# recursive
```

#### 5.6 Phantom parameters: physical units

A parameter absent from the body is allowed: it does not change the values, but it distinguishes the instances, which are different nominal types. A family of quantities thus gives units whose value stays a `double`:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# rule not applicable
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (explicit conversion)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

A `type convert` rule can target a type or a **family** (the name of a parameterized `type def`); the lambda then receives the instances' parameters (`$de(f)`). `return -code continue` means "not applicable". Conversions stay explicit.

#### 5.7 Provided aliases

`layout/binaliases.tcl` defines, for binary formats (Rust and Zig conventions):

| alias | definition |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| suffixes `be`, `le` (`u16be`, `i32le`) | fixed endianness |
| `f32 f64` (and `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Typed variables and the life of an annotated value

#### 6.1 The contract of a typed variable

A variable declared with `set {T}v …` has a **contract**: every write is checked, whether it comes from `set`, `lappend`, `lset`, `incr`, `dict set`, a path write (`set v(x) …`) or `expr {v = …}`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Automatic widening.** A refinement goes where a wider type is expected: `int(u,8)` into `int`, `int(s,16)` or `double`; `double(32)` into `double(64)`. Narrowing is refused (it needs an explicit cast), as is mixing `bitarray(…,lsb0)` and `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# accepted: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominal types.** A nominal type gives way to its base (`px` goes into `int` and becomes `int`, without a trace). Two nominal types with no base link are never equivalent (`mm` and `kg`): that is precisely their role.

**Explicit casts.** `set v {T}$x` or `type cast T $x` convert according to, in order: a specific `type convert` rule; the chain of bases; a sum's variant; a family rule; the built-in conversions between scalars. Otherwise: `unknown way to cast the value from A to B`.

**Lifting the contract.** `set {}v value` (§ 2.5).

**Testing.** `expr {v is T}`: true if the variable is typed and its type fits into `T`; for an untyped variable, true if its value is valid for `T`.

#### 6.2 What keeps the annotation, what loses it

| operation | type | notes |
|---|---|---|
| copy (`set a $x`), argument, return value | kept | kept |
| `lindex`, `foreach`, `$L(i)` on a `list.T` | element typed `T` | — |
| `string cat $x` (a single argument) | kept | kept |
| computation (`expr`, `string toupper`, `dict set` on a value) | **lost** | **lost** |
| cast `{T}$x` in a constructor | changed | kept |
| `set {T}a $x`, typed variable | `T` | erased by widening |

**Adoption.** An **untyped** variable that receives a typed value **adopts** its type, and therefore its contract:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e becomes an int variable
set e hello                    ;# expected integer but got "hello"
```

This is the most surprising behaviour of the model. The classic trap is a reused `catch … m`: if a first `catch` stores a typed value into `m`, a second `catch` can no longer store its error message there. Use one variable per use, `unset` between uses, or `set {}m …`. (Open point, § 17.)

**Procedures.** An argument that receives a typed value is typed and strict inside the procedure; an ordinary argument does not change.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Loop variables.** `foreach`, `lmap`, `dict for`, `dict map` (and the filter of `dict filter`) forget the type of their variable before each assignment; after the loop, the variable keeps the type of the last element received.

#### 6.3 `type deref`: a snapshot of links

`type deref V` returns `V` where each `var` link is replaced by the value the variable has at that moment. The result has the "link-free" type (`list.var.int` → `list.int`), named `deref(<type>)` for a product or a sum. Only one level is followed per call; a missing variable is an error that names the slot (`type deref: at 2: can't read "::C": no such variable`), with no partial result. Works in lists, dicts, products, sums and recursive types; a cycle never loops.

### 7. Reaching the parts of a value: paths

#### 7.1 The parenthesized form

A typed value is read and written with array syntax, but the index is a **path**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# construction (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# writing a whole field
set R((A,B).x) 10 30                       ;# multiple write
${R(A.x, B.y)}                             ;# read, spaces allowed
```

Spaces are allowed in `$R(…)`, `${R(…)}` and in an annotated word `{T}R(…)`. In a bare, unannotated word (`set R(A.x, A.y) …`), Tcl splits at spaces as always: write without spaces or in double quotes.

#### 7.2 The path language

The language is the one from the Tcl wiki page "A better way to do indexation", parsed on the model of `expr` (`tclTypePath.c`).

**Operands:**

| operand | meaning |
|---|---|
| `name` (bare word) | field, dict key, variant name or list index (`3`, `end`, `end-1`): the container's type decides |
| `'key'` | always a key (`'end'` is the key "end", not an index) |
| `"tag"` | literal text copied into the result |

**Operators**, from the weakest to the strongest:

| operator | role | example |
|---|---|---|
| `,` | collection (a group) | `A,B` |
| `*` (unary) | unfolding: the group merges into its parent | `*0:2,4` |
| `.` | nesting; distributes over a group | `A.(x,y)` = `A.x,A.y` |
| `:` | list range | `0:end-1` |
| `( )` | grouping | `(A,B).x` |

**Shape of the result.** A multiple read returns lists nested according to the groups: `$R((A,B).(x,y))` gives `{1 2} {3 4}`; `$L(0:2,4)` gives `{10 20 30} 50`; `$L(*0:2,4)` gives `10 20 30 50`.

**A path substitutes nothing.** Tcl substitutes `$x`, `[cmd]` and `[(expr)]` in `$R(…)` and in bare words; a substituted value is **one leaf**, never read again as a path:

```tcl
set k file.txt ; $D($k)        ;# the key "file.txt" (not D.file.txt)
set n 1        ; $D(f_$n)      ;# glued to text, the value extends the name
set idx {0 2}  ; $L(*$idx)     ;# * unfolds a list value: 10 30
set p {[exec ls]} ; $L($p)     ;# nothing is executed: bad index "[exec ls]"
```

In braces and in `${…}`, `$` and `[` are errors: `a path does not substitute "$"…`. A computed path is read by **naming** the variable with it: `set ref R($chemin) ; set $ref`.

#### 7.3 Container by container

| container | read | write |
|---|---|---|
| product | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (revalidated) |
| sum | `$v(i)`: the current variant | `set v(i) 6` |
| list | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (one bit), `$b(1:4)` (a slice: an integer `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| enumeration | (scalar: no path) | — |

Reading then rewriting the same path always works: `set R(c) [set R(c)]`.

### 8. Building a value

#### 8.1 The forms of a product constructor

A product of *n* fields is built in four ways, with nothing guessed:

| form | example | rule |
|---|---|---|
| positional | `set {pt}a Jean 12` | exactly *n* values, in field order |
| field tags | `set {pt}a {age}12 {nom}Jean` | every value tagged; any order; omitted fields take their default |
| a single field | `set {pt}a {pt.nom}Jean` | the other fields take their default |
| path + positional values | `set {pt}a(age,nom) 12 Jean` | one value per leaf of the path (or per group, § 8.3) |

Add to these **no argument** (`set {pt}a`, every field at its default) and **a product of the same type** (copy). Anything else is an error:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

The bare name/value form (`set {pt}a nom Jean age 12`) **no longer exists**: it could be mistaken for the positional form whenever *n* arguments matched *n* fields (§ 16).

#### 8.2 Field tags

The tag is the **most recent** annotation of each argument, read in the context of the product: a field name wins over a global type of the same name.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# changes nothing
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# a tag carried by a variable counts
```

**All or nothing:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

A braced value can be tagged too: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Path and positional values

`set {T}v(path) v1 v2 …` builds the product (and its nested products); the named fields receive the values, the others their default. The **number** of values chooses the distribution, never their content:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# one value per leaf
set {rect}R((A,B).(x,y))     1 2 3 4      ;# same
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# one value per element of the head group
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# one value for the whole path
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# with unfolding: one value, flat
```

Each value has the shape its read would return; the three counts only coincide when the reads coincide.

**Path + tags does not exist**: `set {rect}R(A) {y}2 {x}1` is an error (`1 targets but 2 values`). It is not planned until a use case calls for it.

#### 8.4 Sums and lists

A sum is built with its variant as a tag, or with the variant's name in the type:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# each element keeps its variant
```

Without a tag, the variant is inferred from the value, in the order of the variants: `set {S}u 5` gives `S.i`. When several variants have the same shape (`{string}a, {string}b`), the first always fits: to choose, tag (`{b}y`).

A list receives its elements one by one: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. A single argument is **one** element.

### 9. Binary types

A type says, by itself, how it is written as bytes: no further syntax to learn. Reading and writing are done by `layout/filebinary.tcl` (loaded with `source`), on top of the core, with a fast path in C for fixed layouts (§ 9.6).

#### 9.1 `bitarray`: an integer whose bits you read

`bitarray(n, order)` is an n-bit integer (0 ≤ v < 2^n). The order says where index 0 starts: `lsb0` (default, the rightmost bit, like `1 << i`: registers, masks) or `msb0` (the leftmost bit, like RFC diagrams). They are two distinct types.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (an integer bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# in expr, a slice in double quotes
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Representations

| type | representation |
|---|---|
| `int(u,n)` / `int(s,n)` | unsigned / signed n-bit integer (multiple of 8, from 8 to 1024) |
| `double(32)`, `double(64)` | IEEE float |
| `string(raw,N)` | N raw bytes |
| `string(ENC,N)` | text on N bytes, padded with NULs |
| `string(ENC,z)` | NUL-terminated text (2 bytes in utf-16, ucs-2) |
| `bitarray(n)` | n bits (multiple of 8), read as an unsigned integer |
| `"MZ"` (sum of literals of equal length) | a constant, checked |
| `list(N).T`, a product of these types | N elements; nesting is allowed |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. A text that cannot be encoded losslessly (`é` in ascii) or does not fit in N bytes is refused at construction.

**Endianness.** Last argument: `be` or `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Without it, the call's endianness applies (`-endian`, little by default). It does not change the values: `int(u,16,be)` and `int(u,16,le)` can be assigned to each other.

**Alignment.** By default products are compact (packed). `-align natural` (or `-align N`) adds C padding: each field starts at a multiple of its alignment, a product's size is rounded up. Padding is zero when writing, ignored when reading unless `-strictpad 1`.

#### 9.3 Commands

```
type binary TYPE                                   ;# the representation, as the core sees it
type binary {what?}TYPE                            ;# same
type binary {sizeof?}TYPE ?-align A?               ;# size in bytes (error if it depends on the data)
type binary {is?}TYPE BYTES ?-exact 1?             ;# 1 if BYTES decode as TYPE, else 0
type binary decode TYPE BYTES ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TYPE VALUE ?-endian E? ?-align A? ?-dedupe 1?
type binary deref LINK ?BYTES?                     ;# follows an unfollowed link, one step
type binary space file|memory|list|get|delete …    ;# named spaces
chan binary read  CHANNEL TYPE ?options?           ;# the channel advances
chan binary write CHANNEL TYPE VALUE ?options?
```

The questions (`{what?}`, `{sizeof?}`, `{is?}`) are **local annotations** on the type argument; actions are verbs. `file binary` remains an alias of the old name.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Level 2: what the file says about itself

A size, a count, a position or a variant can be read from the file: `$field` in the type's arguments (a field **already read** of the current product or of an enclosing product), or an unsigned integer type used as a **prefix**.

| notation | meaning |
|---|---|
| `string(utf-8,int(u,16,be))` | text preceded by its size in bytes (Pascal string) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N bytes, N computed from fields already read |
| `list(int(u,8)).T`, `list($n).T` | list preceded by its number of elements, or of n elements |
| `at($off).T` | the value is at position `$off` (the cursor does not move) |
| `corps($tag)` | the sum `corps`, whose variant is chosen by the field `tag` |
| `U(tp)`, `R($v)` (bound parameter) | a sum whose variant is fixed at definition time |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# variant names = values of the tag
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Already at construction, a list whose size depends on a field is checked: `set {Batch}b(n,items) 3 {{1 1.0 0}}` is refused (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Links: `*(origin, repr, options).T`

A field often holds an address, not the value. The link type reads the address (`repr`, an integer), then the value `T` found there.

| origin | the address starts from |
|---|---|
| `//` (or `file://`) | start of the data |
| `./` | start of the enclosing structure |
| `../` | start of its parent |
| `.` | the field itself |
| `//name/`, `memory://name/` | a named space (file, or memory segments) |

Options: `null` (address 0 means "no target"), `null=V`. With `opt(T)`, a null address gives `none`, otherwise `some`: that is the difference between "null" and "non-existent". When writing, targets are appended at the end, aligned, and addresses computed; `-dedupe 1` writes identical targets only once. A link field equal to its default is written as a null link.

**`-linkdepth`** sets the number of links followed (`Inf` by default). An unfollowed link is an annotated integer (the target's absolute position); `type binary deref` follows it one step. This lets you read only the header of a large file, walk a linked list, and rewrite a field in place.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: the target
```

#### 9.6 C codecs

`type binary encode` and `decode` first try a path in C, which covers fixed layouts: integers of 1, 2, 4, 8 bytes, floats of 4 and 8 bytes, products (with `-align`), `list(N).T` and `list($n).T`. Outside this domain (strings, sums, links, `at`, bit fields, computed sizes, `-dedupe`), or on error, the Tcl layer takes over and gives its messages. `set ::tcl_typebinaryc 0` disables the C path. The bytes and values produced are identical on both paths (test `t51_codec_c`). Measurements: § 15.

### 10. Errors

Messages follow Tcl usage: they name the value, type or field involved. The type-specific `errorCode`s start with `TCL TYPE` or `TCL PARSE TYPE`.

| situation | message (example) | `errorCode` |
|---|---|---|
| forbidden character in a type | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| unknown type name on a variable name | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| local annotation on a variable name | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| local annotation stored by `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| invalid value for the type | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (depending on the base type) |
| no variant fits | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| different nominal types | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| no known conversion | `unknown way to cast the value from mm to kg` | — |
| inconsistent stacked annotations | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| unknown field | `unknown field "zz" in pt`; `pt has no field "zz"` (path) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| field with no value and no default | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| wrong number of values | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| tags and positions mixed | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| duplicate field | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| dependent size not respected | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| dict key missing or not allowed | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| list arity | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| substitution in a literal path | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| invalid list index in a path | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| link to a variable that is gone | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` on a broken link | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| annotated value created outside the parser | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| binary layer not loaded | `the binary layer is not loaded` | — |

## Part III — Implementation

### 11. Overview

The implementation fits in **seven new files** (about 15,000 lines of C), copied into `generic/`, and in **hooks** in about twenty core files. The new files are never patched: they are delivered whole; only core files receive patches.

### 12. The annotated object

#### Structure

An annotated value is an ordinary `Tcl_Obj` whose internal type is `annotated` (`annotatedObjType`, in `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   the string: that of the content V (computed   │
 │                  on demand by UpdateStringOfAnnotated)         │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   the content: an ordinary, shared Tcl_Obj        │
 │   ptr2 ──► T   the annotation, in one of these forms:          │
 │                · a resolved type reference (typeRef)            │
 │                · the raw text of the annotation (not yet        │
 │                  resolved: a note, or a type to resolve)        │
 │                · ""   the marker of the empty annotation {}     │
 │                · an "annotstate" {T, N}: resolved T (or NULL)   │
 │                  and N, the list of notes, from the oldest      │
 │                  to the most recent                             │
 └───────────────────────────────────────────────────────────────┘
```

#### Properties

- **Transparent**: the string is V's. For any command that does not know about annotations, the value is V.
- **Immutable and shared**: the `{T, N}` state is itself an immutable object; `Tcl_DuplicateObj` shares it. Annotating, unstacking or removing a note produces a new object.
- **Free without notes**: as long as there is no note, `ptr2` is T alone.
- **Abstract list** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… go through the object's list procedures (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). The elements of a `list.T` come back typed; an annotated scalar is a one-element list whose element is itself.

#### `Tcl_ObjType` routines

| routine | role |
|---|---|
| `FreeAnnotated` | frees V and T (and a scalar's "self" cell, table `SelfCell`) |
| `DupAnnotated` | copies V, shares T or the `{T, N}` state |
| `UpdateStringOfAnnotated` | V's string |
| `SetAnnotatedFromAny` | **refuses**: an annotated value is born from the parser or a typed assignment, never from a conversion (`TCL VALUE ANNOTATED`) |

#### Main functions (`tclAnnotObj.c`)

`TclAnnotateValue` (set an annotation: cast or note), `TclAnnotSettle` (a constructor's unstacking, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (type, variant, unknown, local, invalid; context before global types), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (the bare content, for core conversions), `TclAnnotDisplay` (`list<int>@mm.hello`), and the commands `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. In the parser and the compiler

#### The `{x}word` prefix

In stock Tcl 9, a braced word followed by other characters is a syntax error (`extra characters after close-brace`), except `{*}`. The annotation prefix therefore takes over a form **that had no meaning**: no valid script changes meaning.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: a word `{text}rest` produces a **`TCL_TOKEN_ANNOT`** token (512, `tcl.h`) carrying the annotation's text, followed by the tokens of the rest of the word. An annotated word `{T}name(...)` runs to the balanced parenthesis, spaces included, on one line.
- `tclCompile.c`, `TclCompileTokens`: if the rest of the word is literal (`TclAnnotSuffixIsLiteral`), the word compiles to `::tcl::annotraw text word` (a raw note, no substitution); otherwise to `::tcl::annotate text value`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: a `set` whose name is annotated goes through `TclSetAnnotatedCmd`; a local annotation written on the value (`set x {-v}$mot`) emits a call to `::tcl::annotlocal`, which raises the error (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: an annotated first argument delegates to `TclSetAnnotatedCmd` (`tclTypedVar.c`), the constructor.

#### The `$name(...)` index: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: on the `(` following a variable name, calls **`TclParseVarIndex`**, which reads the index up to the balanced `)`: nested parentheses, paired `"..."` tags, substitutions `$x`, `[cmd]`, `[(expr)]`, backslashes; braces refused. If the index is not well formed under this grammar, it **falls back** exactly to Tcl 9's rule (`info complete {a "c $dd("}` stays true).
- Only when the extended syntax is used are the components wrapped in a **`TCL_TOKEN_INDEX`** token (1024, `tcl.h`): an index valid in Tcl 9 keeps exactly its tokens. Bare words are not affected: `puts a(b c)` stays two words.
- `${name(...)}`: the whole name is literal; `Tcl_ObjGetVar2` and `INST_LOAD_STK` (via `TclTypedVarGetElementByName`) read the path when the variable is a typed scalar.
- `tclTest.c`: `testparser` shows the "index" token.

#### Bytecode

| instruction | where | role |
|---|---|---|
| `INST_INDEX_CONCAT` ("indexConcat", 6 bytes: count `uint1` + mask `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, called by `TclCompileVarSubst` and `TclPushVarName`), `tclExecute.c` | concatenates the parts of an index into a **pathindex** object that keeps the substituted spans (`TclPathIndexNew`): a substituted value stays one leaf of the path |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (compiled `dict for`, `dict map`), `tclExecute.c` | forgets a loop variable's type before assignment |

Non-compiled path: `TclSubstTokens` (`SubstIndexTokens`) produces the same pathindex object.

#### Other core hooks

| file | change |
|---|---|
| `tclBasic.c` | `TclTypesInit`: creates `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` takes the slow path for an annotated value; `INST_FOREACH_STEP` forgets the type; reading and writing typed elements (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` and `TclTypedVarAfterStore` around each write: the contract |
| `tclProc.c` | `TclTypedVarBindArg`: typed argument binding |
| `tclCmdAH.c`, `tclDictObj.c` | loop variables (`TclTypedVarForgetByName`); `TclAnnotUnwrap` in dict reads |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | core conversions on the content (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): an annotated value is never converted in place |
| `tclListObj.c` | `lappend` through an abstract list's `replaceProc`; multi-index `lindex` without flattening |
| `tclCompExpr.c` | `is` operator (lexeme `IS_TYPE`, `IsTypeAhead`), assignments `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. The added files

| file | lines | content |
|---|---|---|
| `tclTypes.c` | 4,500 | typology, `type def/alias/get/cast/convert/deref/binary`, validation, widening, casts, C codecs |
| `tclTypeParse.c` | 2,700 | parser and compiler of type expressions (model: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1,400 | parameterized types, instances, phantom parameters, binary refinements |
| `tclAnnotObj.c` | 2,150 | the annotated object, the `{T, N}` state, resolution, `note` |
| `tclTypedVar.c` | 1,350 | typed variables, constructors (`TclSetAnnotatedCmd`, `BuildProductEx`, field tags) |
| `tclTypePath.c` | 2,900 | path language, read, write, construction by path, pathindex object |
| `tclVarRef.c` | 360 | `var` links (canonical name, access, stale links) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | structures (`TclType`, `TclTypeField`, `TclBinLink`…) and interfaces |

On the Tcl side: `layout/filebinary.tcl` (binary layer: `type binary decode/encode/deref/space`, `chan binary`) and `layout/binaliases.tcl` (aliases).

## Compatibility, design, limits

### 15. Compatibility and performance

**No valid script changes meaning.** Both added syntaxes take over forms that were errors: `{x}word` ("extra characters after close-brace") and an extended `$name(...)` index, which falls back exactly to Tcl 9's rule as soon as it is not well formed. Checks:

- Tcl tests `parse`, `parseExpr`, `parseOld`, `subst` (including 257 `testparser` tests), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: results identical to the base;
- equivalence with a stock Tcl 9 on 18,000 generated cases (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): no valid result changed;
- AddressSanitizer clean; memory stable over 1,000,000 constructions.

**Types are not the enemy of speed.** Measurements on the `cstruct` example (10,000 samples, `-align natural`):

| operation | Tcl layer | C codec | classic hand-written Tcl, unchecked |
|---|---|---|---|
| encoding (`type binary encode`) | 333 ms | 1.3 ms | ≈ 3.3 ms (`binary format`; 330 µs measured for 1,000) |
| decoding (`type binary decode`) | 485 ms | 3.0 to 3.7 ms | 7.3 to 10.5 ms (`binary scan`) |

The C codec, which also **checks** every value against its type, is 2 to 3 times faster than classic hand-written Tcl, and 130 to 250 times faster than the generic Tcl layer.

### 16. Design choices

| choice | reason |
|---|---|
| `{x}word` takes over a syntax error | full compatibility, no new quoting rule |
| no glued stacking `{a}{b}$x` | ambiguous with `{int}{300}`; you stack by annotating an annotated value |
| only a constructor resolves a type | `{T}$x` outside a constructor costs nothing and never fails; checking happens where the value is stored |
| local annotations recognized by their characters | a type name cannot contain `-` or `?`: no collision possible |
| `type def` nominal, `type alias` structural | tell units apart (`mm`, `kg`) without changing values |
| automatic widening, explicit narrowing | no silent loss of information |
| explicit conversions between nominal types | `mm` → `m` needs `{m}$a` or `type cast` |
| bare name/value form removed | `set {png}p chunks $l` could be read two ways; names are now written in the source (`{chunks}…` or `p(chunks)`), never guessed from the data |
| field tags: all or nothing | a mix would be a disguised positional form |
| a path substitutes nothing | data containing `[script]` is never executed by the path parser (double substitution is gone); same rule as an array |
| atomic substituted values | `$D($k)` reads the key `$k`, even if it contains `.` or `,` |
| path + tags not provided | no proven need; we will see with use |

### 17. Limits and open points

**To fix or to decide**

- **Adoption by an untyped variable.** `set e [lindex $L 1]` makes `e` typed and strict. The `catch … m` trap (§ 6.2) follows, and the error that escapes is the original message, which makes it hard to understand. To decide: silent adoption, adoption without a contract, or an explicit message.
- **`;` and `,` mixed without braces** (`{int}a; {int}b, {int}c`) are accepted without error and produce an anonymous field; the specification's rule refuses them.
- **Reading a variant that is not a sum's current variant** gives `unknown way to cast the value from S to string` instead of a message naming the variant.
- **Variants of the same shape**: without a tag, the first is taken without warning.
- `set {paire}s {aut.nom}x` (the field of **another** product) gives `wrong # args`; a dedicated message would be clearer.
- `lset` with an index list going through `TclLsetList` keeps one reference too many (a leak, no crash).

**Not done yet**

- A "non-existent" value in general: `opt(T)` covers binary links, there is no general form.
- `union`, shared bit fields, a list "up to the end of the file" (refusing `union` is deliberate).
- A comment annotation `{#}` at parser level, annotation of the first word (`{c}proc`), declaration annotations (`proc p {{?}{…} {!}arg}`).
- Notes per element of a typed list (today a note is on the whole list); erasing all notes with `{@}$x`.
- C codecs for sums, links and computed sizes; caching the parsed type.
- The C test bench on a mock (`typeparse/test/`) needs updating; the examples `arbre_var.tcl` and `defauts_calcules.tcl` need fixing.

## Appendices

### A. Installation and tests

The prototype starts from Tcl `core-9-0-branch` with the TIP 759 branch. From the root of the Tcl tree (MSYS2 / MinGW64 or Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # extracts, copies the type sources, applies the core patches,
                                        # rebuilds, then compares each tests_tclsh/tNN_*.tcl with its .out
```

- Type sources are delivered whole in `typeparse/` and copied into `generic/`; only the core patches, listed in `noyau/ORDRE.txt`, are applied, without fuzz (`-F0`), a patch already applied being detected and skipped.
- `maj.sh` options: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "message"`.
- Notes on each patch: `noyau/LISEZMOI.md`. Runnable examples: `exemples/`.

### B. Glossary

| term | meaning |
|---|---|
| annotation | text in braces glued in front of a word: `{x}word` |
| note | unresolved annotation, which follows the value |
| type | annotation resolved in the known typology |
| local annotation | annotation whose text cannot be a type (`{-v}`, `{is?}`); it only lives between the caller and the command |
| constructor | what stores a value and resolves its annotations: `set {T}v`, argument binding, `lassign`, `foreach`, assignment in `expr` |
| contract | a variable's type, checked on every write |
| adoption | an untyped variable takes the type of the value it receives |
| cast | explicit conversion to a type (`{T}$x` in a constructor, `type cast`) |
| nominal | type created by `type def`, distinct from its definition; gives way to its base |
| refinement | restricted base type, with a binary representation (`int(u,8)`) |
| widening | automatic move from a refinement to a wider type |
| product | `struct`: named, ordered fields |
| sum | one variant among several, named |
| variant | one of the branches of a sum |
| field tag | annotation naming an argument's field: `{age}12` |
| path | index of a typed value: `R(A.(x,y))` |
| pathindex | object carrying an index and its substituted parts |
| link | `var.T` (to a variable) or `*(…).T` (to a binary address) |
| phantom parameter | type parameter that does not change values but distinguishes instances |

### C. Correspondences

| here | C | Rust | LLVM |
|---|---|---|---|
| product | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| sum | (`union` + hand-written discriminant) | `enum` with data | — |
| enumeration (sum of literals) | `enum` | `enum` without data | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (signless) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | NUL-terminated `char *` | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32-bit) | `*const T` | `ptr` |
| `opt(T)` | pointer that may be `NULL` | `Option<T>` | — |
| nominal type (`type def mm double`) | `typedef` (unchecked) | newtype `struct Mm(f64)` | — |
| `-align natural` / packed | default alignment / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | alignment / `<{ }>` |
