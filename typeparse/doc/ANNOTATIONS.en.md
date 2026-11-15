# Annotations in Tcl — documentation

*7 November 2026 · version v16z · Florent MERLET, with Claude (Anthropic)*

*Other languages: [Français](ANNOTATIONS.fr.md) (reference) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md) — the German, Dutch, Spanish, Portuguese and Italian translations still describe v13*

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
- a **field name** chooses where a value goes (`set {pt}a {age:}12 {nom:}Jean`);
- a **binary representation** says how to read and write bytes (`{int(u,16,be)}version`);
- a **note** marks a value so that a procedure can recognize it (`note has $v mm`).

The script keeps its shape: you add annotations where you want guarantees, and nowhere else.

### 2. Notes, types and local annotations

An annotation is always written `{text}word`. Depending on what the text says and on **who reads it**, it plays three roles.

#### 2.1 The note: the raw annotation

A **note** is an annotation that no constructor has resolved. It follows the value: copies, procedure arguments, list elements. Since v16m it is **metadata**: it can be read (`note get`, `type get` with `@`), but an annotation acts only on the command that receives the word (§ 2.2); past that command, what was not resolved stays a note for good.

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

**Two uses of notes (v16y).** A note is a *proto-type*: a name put on values before the type exists (`{speed}12.5`), between plain Tcl and typed Tcl. A note that starts with `@` (`{@trace}`, `{@step}`) is an **instrument note**: never a type, it is used for debugging. Both are read by `type debug` (§ 3.1).

The form `{a}{b}$x` (two annotations glued together) does not exist: it would be ambiguous with `{int}{300}`, an annotation on a braced literal. You stack by annotating a value that already is.

#### 2.2 The type: the annotation resolved by a constructor

A **type** is an annotation **resolved** in the typology the interpreter knows (base types, `type def`, `type alias`). Only a **variable constructor** resolves:

- `set {T}v …` (and `set v {T}$x`);
- binding of `proc` and `apply` arguments;
- `lassign`, `foreach`, `lmap`, `dict for`, `dict map` for their lists and dicts (`foreach {x y} {list}$p`);
- `lappend`, `lset`, `linsert`, `lreplace` on a typed variable, for the elements received;
- in `expr`, `x = v` and `x is T = v`;
- **`type {T}word`**, the explicit cast (v16m): `type {int}$s`, `type {list}$p`, `type {int}[cmd]`, `type {int}[(expr)]`.

Annotated on the **name** of a variable, an annotation gives the variable a **contract**: every later write is checked.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

**Where an annotation acts (v16m).** An annotation acts only on the **command that receives the word**. Nothing is resolved during substitution: Tcl substitutes every word before calling the command, and only the command knows whether a word is a value or a variable name (`set {int}$name 5` declares the variable named by `$name`). If that command is a constructor, it resolves the annotation; otherwise the annotation does nothing. When the command ends, what is still pending **expires**: a note, which no constructor resolves any more, even if the value was returned (`concat`, `return`), stored in a list or in a variable, or if the type is defined later. A label `{name:}` or a local annotation disappears.

```tcl
set s 3.0
catch {concat {int}$s} r      ;# 0: concat checks nothing; r is 3.0, note int
catch {set y {int}$s} r       ;# 1: set receives the word, it checks: expected integer...
catch {type {int}$s} r        ;# 1: the explicit cast
set q [type {list}$p]         ;# a product to the list of its fields
```

The error of a cast therefore always arises in the command that contains the word, and a `catch` around it catches it (before v16m, `catch {concat {int}$s} r` left the annotation pending, and the error arose when storing `r`, outside the `catch`). v16l made an immediate cast of `{T}$x`; it was removed: it cast the name in `set {int}$name 5`.

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

This stack exists only for the annotations of **one word**, received by one constructor. A note set before its `type def` does not take on meaning later (v16m): the command that received it made it expire.

**Labels (v16l).** A product field or a sum variant is named by a **label**, `{name:}value`: `set {pt}a {age:}12`, `set {S}v {i:}5`. A label is a local annotation: it can never be a type, so a global type of the same name changes nothing, and `{name}` without a colon is always a type or a note. It is read by the constructor that receives the word; through another command (`set {pt}a [concat {age:}12]`), it has expired (v16m) and the argument is positional. A label that no constructor reads is not stored. The old form `{age}12`, when `age` names a field and no type, is an error that gives the new spelling.

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

#### 2.6 Meta-types: `bare`, `typed`, `noted`, `typednoted`, `any` (v17e)

A value is in one of four states, its **meta-types**: `bare` (no type, no note; it shimmers), `typed` (a resolved type), `noted` (notes, no type), `typednoted` (both). `any` is their union: no requirement. As a type, a meta-type is a **requirement**: the value enters as it is, type and notes kept, if its state fits; nothing is converted.

```tcl
proc f {{typed}x} {...}          ;# a typed value, of any type
f 5                              ;# expected a typed value but got a bare value "5"
proc g {{bare}x} {...}           ;# only a bare value; g {int}$i: refused
type def annotated {noted | typednoted}   ;# unions (meta-types are disjoint)
set {list.typed}L {int}5 [type cast double 2.5]   ;# each element keeps its type
type cast any $v                 ;# identity; so are {any}$v and set {any}v $v
set {list.any}P 5 {this}6 $v     ;# bare, noted, typed: each as it is
```

A meta-type in `set {typed}v $x` checks the value there, without setting a contract. Making a value bare is still `{}` (§ 2.5).

`type meta $v` returns the variant of the meta-type (an enumeration: `type variant` reads it), noted with what is known of the type: the resolved type of a typed value; for a bare value, its **representation**, the name of its current `Tcl_ObjType` (`int`, `list`, `dict`, `bytearray`, `cmdName`…), none for a pure string. It is the built-in counterpart of `tcl::unsupported::representation`: these names are internal to Tcl and change between versions.

```tcl
set i 1; incr i
set m [type meta $i]
list [type variant $m] [note get $m]     ;# bare int
type meta [list a b]                     ;# bare, note list
```

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

#### 3.1 Debugging: `type debug` (v16y)

A note follows its value: copy, argument, list element, loop variable, procedure parameter. That is exactly what is needed to follow a piece of data through a program. `type debug on` puts a trace on every command (commands are no longer inlined while it is set); before each command, it looks at the notes of its arguments.

```
type debug ?on|off?        ;# switch on, off; without argument: the state
type debug next            ;# stop at the next command (a breakpoint written in the code)
type debug when ?NOTE ?EXPR?? ;# a condition: {@NOTE} reports only if EXPR is true
type debug console ?on|off? ;# the Tk console (otherwise the prompt on stdin)
type debug hook ?CMD?      ;# the hook called (by default ::tcl::typedebug::hit)
type debug notes           ;# the inventory of proto-types (a dict)
type debug report          ;# the same, readable, with suggestions and checks
type debug clear           ;# empties the inventory
```

**Instrument notes.** `{@trace}` reports every command that receives the value; `{@step}` also stops there. Switched off, `type debug` costs nothing and `@` notes do nothing: they can stay in the code.

```tcl
type def pt {{double}x; {double}y}
proc {double}norm {{pt}p} {
    set x2 [expr {$p(x) ** 2}]
    return [expr {sqrt($x2 + [lindex $p 1] ** 2)}]
}
proc path {L} {
    set s 0.0
    foreach p $L { set s [expr {$s + [norm $p]}] }
    return $s
}
type debug on
set P {@step}{3.0 4.0}
path [list $P {1.0 0.0}]
```

```
@step  set argument 2 = 3.0 4.0   (d.tcl:12, in (global))
step> c
@step  list argument 1 = 3.0 4.0   (d.tcl:13, in (global))
step> c
@step  ::norm p = 3.0 4.0   (d.tcl:8, in path)
step> s
@next  expr {$p(x) ** 2}   (d.tcl:3, in norm)
step> p p
  3.0 4.0
step> s
@next  set x2 9.0   (d.tcl:3, in norm)
step> r
@step  lindex argument 1 = 3.0 4.0   (d.tcl:4, in norm)
```

The value is followed into `list`, into the `foreach` of `path`, down to the parameter `p` of `norm` (the parameter's name is given). The prompt:

| key | effect |
|---|---|
| `c` or Enter | continue |
| `s` | step: stop at the next command (inside the called procedure, if there is one) |
| `r` | run without stopping again (`@trace` and `@step` are still shown) |
| `o` | switch `type debug` off |
| `q` | quit: the command returns an error |
| `w` | the call stack |
| `v` | the local variables |
| `p NAME` | the value of a variable |
| anything else | a Tcl command, run where execution stopped |

A stop on a note comes before the command: execution is in the caller. `s` enters the procedure, where its parameters are visible. `type debug next` written in the code is a breakpoint without a note.

**Conditional notes.** Any `@name` note other than `@trace` stops like `@step`. `type debug when name EXPR` gives it a condition: it reports only if the expression is true. The expression sees `$value` (the noted value), `$cmd` (the command), `$arg` (the parameter, or "argument N") and `$hits` (how many times this note has been met). A condition on `trace` filters the `@trace` reports.

```tcl
type debug when neg {$value < 0}      ;# stops only on a negative value
type debug when five {$hits == 5}     ;# on the fifth pass
type debug when trace {$cmd eq "::norm"}
root {@neg}$v
type debug when neg {}                ;# removes the condition
```

**The Tk console.** `type debug console` replaces the prompt with a window: the current stop (note, command, place, followed value with its type), buttons (Continue F5, Step F10, Run F8, Quit F12, Off), the call stack, the variables of the frame selected in the stack with their **type and notes**, the log of passes, and a command line run in the selected frame. During a stop, the console runs the event loop; closing the window goes back to the prompt on stdin. (`exemples/mise_au_point/console.tcl`, screenshot `console.png`.)

**The hook.** `type debug hook CMD` replaces the display: `CMD note words index` is called (without tracing) with the note, the words of the command and the index of the noted argument (0 for a `next` stop). An error in the hook prevents the command from running; that is how an assertion, or a log, is written. A custom hook receives every `@` note: the conditions of `type debug when` belong to the default hook.

```tcl
proc rec {note words index} { lappend ::log [list $note [lindex $words 0] $index] }
type debug hook rec
```

**Proto-types.** While `type debug` is on, every note whose name is not (yet) a type is inventoried: how many times it was seen, where the first time, through which commands, and up to three values, each with its place. When the type is defined later, `type debug report` checks the values seen after the fact; as long as it is not, it suggests the closest defined type (a typo).

```tcl
type def point {{double}x; {double}y}
type debug on
set a {pointt}{3 4}
set v {speed}12.5
set w {speed}abc
type debug off
type def speed double
puts [type debug report]
```

```
pointt: seen 1 time, first at e.tcl:3, via set
    values: 3 4
    not a type; did you mean point?
speed: seen 2 times, first at e.tcl:4, via set
    values: 12.5 | abc
    now a type, but:
      abc (e.tcl:5): expected floating-point number but got "abc"
```

`type debug notes` returns the same as a dict: `name {count N where F:L commands {...} samples {{value place}...} status note|ok|mismatch ?errors {{value place message}...}?}`.

### 4. The `type` command

```
type def NAME DEFINITION       ;# defines a nominal type (or a parameterized family)
type def NAME                  ;# returns the definition (of S, of a variant S.v, of a field P.f)
type alias NAME DEFINITION     ;# gives a structural name to a definition
type get VALUE                 ;# the resolved type of a value (empty if none)
type cast TYPE VALUE           ;# a constructor's resolver, without a variable
type {T}WORD                   ;# the same cast, written as an annotation (v16m)
type convert FROM TO LAMBDA    ;# declares an explicit conversion
type deref VALUE               ;# snapshot: each var link replaced by its value
type canonic ?-notes? VALUE    ;# the canonical text: {T}content, {T@notes}content (v17c)
type parse TEXT                ;# reads a canonical text back into a typed value (v17c)
type meta VALUE                ;# bare, typed, noted, typednoted; note: type or representation (v17e)
type binary TYPE ...           ;# binary representation, encoding, decoding (§ 9)
type def {-compact}NAME DEF    ;# a list (or any list of a product) always in cells (§ 14.1)
type def {-a,-b}NAME DEF       ;# several options, in the syntax of notes
type is {compact?}TYPE|VALUE   ;# a question about a type or a value
type is {disjoint?}S           ;# the pairs of variants that can accept the same value
type is {union?}S              ;# 1 if S is a union (|) (v16w, § 8.4)
type variant VALUE             ;# the variant of a value of a sum or a union (v16w)
type proc NAME                 ;# the signature of a typed procedure: args, return, record (§ 6.4, 6.5)
type call ?TYPE?               ;# the current call as a value: the reified call (§ 6.4, v16z)
type {{*}T}WORD, {{**}T}WORD   ;# the explicit cast, returned unfolded as a list or a dict (§ 6.4)
type {c!}export TYPE ?TYPE ...? ;# C header of named types (§ 14.2); {llvm!} planned
type debug ?on|off|next|when|console|hook|notes|report|clear? ;# debugging through notes (§ 3.1, v16y)
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
| `any` | any value, as it is (the union of the meta-types, § 2.6); `list.any`: each element keeps what it carries |
| `bare`, `typed`, `noted`, `typednoted` | meta-types: a requirement on the state of the value (§ 2.6) |

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

**Characters.** A type name contains only ASCII letters, digits, `_` and non-ASCII bytes, and does not start with a digit. The grammar also uses `{ } , ; | . = " # ( ) * $`. Any other ASCII sign (`! % & ' + - / : < > ? @ [ \ ] ^ ~`) is refused: `invalid character "!"`, code `TCL PARSE TYPE BADCHAR`, except after `=` (a default value) and inside a constructor's parentheses (`*(file://x, …)`). This refusal is what makes local annotations (`{-v}`, `{is?}`) safe.

A definition is **declarative**: no substitution happens when it is read, except in a default value, evaluated at each construction.

**Grammar (ISO EBNF).** Precedence, from the weakest: `;`, then `,` and `|`, then `=`, then `.`. `;`, `,` and `|` do not mix at one level.

```ebnf
type        = item , { ";" , item }            (* product *)
            | item , { "," , item }            (* tagged sum *)
            | item , { "|" , item } ;          (* disjoint union *)
item        = chain , [ default ] ;
default     = ( "=" | "?=" ) , value ;
chain       = primary , [ "." , chain ] ;      (* nesting, right-associative *)
primary     = group
            | typename , [ "(" , args , ")" ]  (* list(3), int(u,16), pt(double) *)
            | "$" , name                       (* a parameter, in a family body *)
            | "*(" , linkargs , ")"            (* binary link *(origin, repr, options) *)
            | quoted ;                         (* a literal: "red", "MZ" *)
group       = "{" , type , "}" , [ fieldname ] ;   (* {T}name: a field; {T} alone: grouping *)
fieldname   = [ "-" ] , name , [ "..." ] ;         (* -name: an option; name...: variadic *)
typename    = [ "::" ] , name , { "::" , name } ;
name        = namestart , { namechar } ;
namestart   = letter | "_" | nonascii ;
namechar    = namestart | digit ;
value       = quoted | braced | bareword       (* a literal default *)
            | "[" , script , "]"               (* computed at each construction *)
            | "$" , name ;                     (* a parameter, or an argument already bound *)
comment     = "#" , { any - newline } ;

args        = count                                              (* list(N) *)
            | count , ":" , ( count | "Inf" )                    (* list(LO:HI), v17g *)
            | ( "u" | "s" ) , "," , count , [ "," , endian ]     (* int *)
            | ( "32" | "64" ) , [ "," , endian ]                 (* double *)
            | encoding , "," , ( count | "z" | expr )            (* string(utf-8,z), string(raw,$len - 4) *)
            | count , [ "," , ( "lsb0" | "msb0" ) ] , [ "," , endian ]   (* bitarray *)
            | [ key , { "," , key } ]                            (* dict(x, z?=0); dict() *)
            | expr                                               (* at($off) *)
            | type , { "," , type } ;                            (* a family instance: pt(double) *)
key         = name , [ "?" , [ "=" , value ] ] ;
endian      = "be" | "le" ;
family-head = name , "(" , param , { "," , param } , ")" ;      (* type def {pt({type}T)} ... *)
param       = "{" , ( "type" | "int" | "double" | "string" ) , "}" , name ;
```

What the grammar does not say: in a product, every field and every group has a name; in a sum, every typed variant does; `-name` and `name...` exist only in a procedure signature.

A signature is accepted by `proc` and, since v16z, by `apply`: a lambda is a procedure without a name, so the annotation on the lambda word is its return type, as `proc {T}name` gives it to a procedure.

```tcl
set somme [apply {double}{{{double}a; {double}b} {( $a + $b )}} 5 10]    ;# 15
apply {int}{{x} {return $x}} 2.5      ;# return value {int}: expected integer but got "2.5"
type convert celsius kelvin {{{celsius}c} {expr {$c + 273.15}}}          ;# a typed rule
```

#### 5.3 Fields and containers

**Enumeration** — a sum of literals.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# a single literal: a constant (file signature)
```

**Sum** — a single variant at a time, whose value **writes its name** (v16w): `variant payload`, or the literal alone.

```tcl
type def S {{int}i, {string}s}
set {S}v {i:}5                 ;# v is "i 5"
set {S}v i 5                   ;# the same thing, written plainly
type get $v                    ;# S.i
set {S.s}w abc                 ;# w is "s abc": the variant chosen by the type's name
```

**Union** — a value of one of the forms, without a tag: the value is the payload alone, and the alternative is computed. The alternatives must be disjoint (v16w, § 8.4).

```tcl
type def U {{int}n | {list(2).int}pair}
set {U}u 3 4                   ;# u is "3 4", of type U.pair
```

**Product** — a `struct`: named, ordered fields, each of its own type.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positional
set {pt}b {age:}12 {nom:}Jean  ;# field labels (§ 8)
$a(nom)                        ;# Jean
```

**List** — `list` (extensible), `list.T` (elements of type T), `list(N).T` (exactly N), `list(LO:HI).T` (LO to HI, `HI` = `Inf` for no bound: `list(1:Inf).T`, a non-empty list; v17g). The `:` is the one of path ranges (`$L(1:3)`); `list(N:N)` is `list(N)`, `list(0:Inf)` is `list.T`. The bound is checked at every construction and every write (`lappend`, `lpop`, `lset`, `set L(i)`, assignment).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

**Scalars and containers (v16t).** The structure of a typed value comes from its type, never from re-reading its string:

- a typed **scalar** (`string`, a number, a sum's literal) is **one** list element: `set {string}s "a b c"` gives `llength $s` = 1. The elements of a `list.string` stay strings at every nesting level: `foreach r $M {foreach s $r …}` never splits `{hello world}` into two words;
- a **container** (`list`, `dict`, **product**) unfolds: a product is the list of its fields, each in its type. `set {point}P 1 2` gives `llength $P` = 2, `lassign $P x y`, `foreach {x y} $P`, `{*}$P`, `concat $P $P` = `1 2 1 2`, as for the list `{1 2}`. A `string` field stays atomic.

The bare list `{list}` keeps Tcl's behaviour. (Up to v16s, a product was also a single element, "rule B": two values with the same string then had two structures, see `doc/CARTOGRAPHIE-EIAS.md`.)

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

**Reading a link alone.** This is the usual Tcl principle (`set A 1; set r A`: `$r` gives `A`, `set $r` gives `1`). A link taken out of its container, or a variable of type `var.int`, is the variable's **canonical name**; `set` gives its value. In a container, a path read follows the link, and a path write writes **into** the variable (its contract is checked).

```tcl
set r [lindex $L 0]            ;# ::A   (type var<int>)
set $r                         ;# 7
set {var.int}R A               ;# ::A ; set $R gives 7
set L(0) 8                     ;# writes 8 into ::A
```

Every write of a link stores the canonical name: construction, `lset L 1 C`, `lappend L C` (`::C`). An empty name is refused; any Tcl variable name is accepted (`lappend L 5` stores `::5`). A link to a procedure's local variable has a canonical name specific to the frame (`frame:1:x`): `$M(0)` follows it, but it is not a name that `set` knows.

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

**One resolution order (v17d)**, the same for `set`, `type cast` and the arguments of a procedure. A *typed* value enters a type T as it is if it has type T, if it widens into T (`int` → `double`, `Celsius` → `double`), or if T is a sum with a variant of exactly its type (`{S}` with `{int}i` takes `{int}5` as `i 5`, before any rule). A rule (`type convert`) or a built-in conversion (truncation, `double` → `Celsius`, `string` → `int`) is a cast, and a cast is written: `type cast`, or `{T}$x`, including at a call (`VeutEntier {int}$d`); a typed argument is not a cast. A *bare* value (or one with notes only) is read by T's own reader; a nominal type with a rule from its underlying type (`type convert string Jour`, `type convert double Pos`) has that rule as its validation, applied to every bare value; when the native reading fails, a rule `type convert any T` (or `string T`) reads the text. `set {Pt}p $x` with a single argument for a product of several fields takes `$x` as the whole value.

```tcl
proc VeutEntier {{int}i} {...}
VeutEntier 2.5                 ;# error: expected integer but got "2.5"
VeutEntier $d                  ;# ($d a double) type mismatch ... (use an explicit cast)
VeutEntier {int}$d             ;# 2
type convert string Jour {{s} {if {![regexp {^\d{4}-\d\d-\d\d$} $s]} {error "not a date"}; set s}}
set {Jour}j 10/11              ;# not a date
```

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
| `unit`, `opt(T)` | `""` ; `{T}some, "none"` (values `some X` or `none`, v16w) |

These aliases are global (`::u8`…): the file defines them with their qualified name from its namespace (§ 5.8).

#### 5.8 Namespaces (v16u)

A type belongs to the namespace where it is defined, like a procedure:

```tcl
namespace eval geom {
    type def point {{double}x; {double}y}      ;# ::geom::point
}
type def point {{string}name}                   ;# point, global
type def ::geom::seg {{geom::point}a; {geom::point}b}
```

- **Definition.** `type def NAME` puts the type in the current namespace; `type def ::NAME` in the global namespace; `type def a::NAME` in the namespace `a` (relative to the current namespace, then global, like `proc`), which must exist. Predefined types (`int`, `string`, `list`…) cannot be redefined in a namespace.
- **Resolution.** A name resolves like a command: `point` in the current namespace, then in its path (`namespace path`), then in the global namespace; `geom::point` in the current namespace then global; `::geom::point` as is. A type's body is read in the namespace where it is defined; a family's body (`type def {grid({int}n)} …`) in the family's namespace, wherever it is instantiated; a procedure's signature (arguments and return) in the procedure's namespace, like its body: `proc {point}::geom::mk {…}` returns a `::geom::point`, even when defined from the global namespace. A lambda's signature is read in the lambda's namespace (v16z).
- **Displayed name.** `type get`, `type def` and messages give the qualified name of a namespaced type (`::geom::point`, `list<::geom::point>`, `::geom::S.at`), the simple name of a global type. This name can be read back. To compare with a short name, `namespace tail`: `switch [namespace tail [type get $b]] {bloc.para {…}}`.
- **Libraries.** A type name passed as a string to a procedure of another namespace resolves there in that namespace (like a command name): pass the qualified name. A package that wants global types names them `::u8` (`binaliases.tcl`, `cstruct.tcl`).
- **Deletion.** `namespace delete geom` removes the names of its types (and its `type convert` rules); values keep their type, under its qualified name. A new `::geom::point` is another type.
- **C.** `type {c!}export` names the C struct after the last word (`::math::geometry::point` → `point`); if two types exported together share it, the second takes the qualified name (`ce2_cp`).
- **Lifetime.** A typed value keeps alive the type table of the interpreter that created it: passed to another interpreter, it survives `interp delete` (v16u; before, reading it read freed memory).

### 6. Typed variables and the life of an annotated value

#### 6.1 The contract of a typed variable

A variable declared with `set {T}v …` has a **contract**: every write is checked, whether it comes from `set`, `lappend`, `lset`, `incr`, `dict set`, a path write (`set v(x) …`) or `expr {v = …}`. All these commands also write through a path: `incr v(age)`, `append v(name) X`, `lappend v(marks) 3`, `dict set v(d) k 1` read the part, modify it and write it back, with the same checks as `set v(age) …`.

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

**Nominal types.** A nominal type gives way to its base (`px` goes into `int` and becomes `int`, without a trace). For a collection, the base is its structure: `type def polygon list.point` goes into `list.point`, `list(4).point` or a field `{list($n).point}` (the number of elements is checked). The reverse is a written cast, like `double` → `Celsius`: a `list.point` enters a `polygon` (by `set` or as an argument) only through `{polygon}$L` or `type cast` (v17d). Two nominal types with no base link are never equivalent (`mm` and `kg`): that is precisely their role.

**Explicit casts.** `set v {T}$x` or `type cast T $x` convert according to, in order: a specific `type convert` rule; the chain of bases; a sum's variant; a family rule; the built-in conversions between scalars. Otherwise: `unknown way to cast the value from A to B`.

**Product to collection.** Any product converts to a list or a dict: `{list}$p` gives the list of its fields, bare (since v16t, `foreach {x y} $p` is enough); `{list.any}$p` keeps them annotated, like `$p(*)`; `{list(2).double}$p` validates them; `{dict}$p` gives `x 3 y 4`. A nominal list converts the same way to a structural list (`{list.point}$P`, `{list}$P`). The cast happens in the command that receives the word: `foreach {x y} {list}$p`, `lassign {list}$p x y`, `dict for {k v} {dict}$p`, or `type {list}$p` elsewhere (`llength [type {list}$p]`); `llength {list}$p` casts nothing (v16m).

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

**No adoption (v16l).** An **untyped** variable that receives a typed value keeps the value and its type, but takes **no contract**: it accepts any value afterwards. Only `set {T}v` (or a declaration) sets a contract.

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e is 2, of type int
set e hello                    ;# accepted: e has no contract
```

The `catch … m` trap of earlier versions is therefore gone.

**Procedures.** An ordinary argument (classic argument list) receives the value and its type, without a contract: `proc f {x} {set x 1.5}` then `f $n`, with `n` an `int`, returns 1.5. For a checked argument, use a typed signature (§ 6.4).

**Conversions.** A typed value reads as a number if its string is one and its type is not a collection: `string`, an enumeration (`type def pi {"3.14159"}` then `expr {$PI * 2}`), a single-field product (`{{int}i}`). A product of several fields, a list or a dict are still refused. `string is` always judges the string.

**Loop variables.** `foreach`, `lmap`, `dict for`, `dict map` (and the filter of `dict filter`) forget the type of their variable before each assignment; after the loop, the variable keeps the type of the last element received.

**Lists of numbers (v16s, v16t).** The elements of a `list.int` or `list.double` (simple types: neither nominal nor binary), and the `int` or `double` fields of a product, stay bare in the list: they were checked with it. `foreach`, `lmap`, `lassign` (the rest) and `lrange` return them bare; `lindex $L i` and `$L(i)` make a typed value of them on demand. A typed variable or procedure that receives a bare element checks it again. Lists of strings, products, sums, `any` or nominal types keep their elements typed. (Wrapping each element of a new list cost 2.5 times the loop. A bare number cannot be read again as a list; a bare string could: v16s also left the elements of a `list.string` bare, v16t keeps them typed.)

#### 6.3 `type deref`: a snapshot of links

`type deref V` returns `V` where each `var` link is replaced by the value the variable has at that moment. The result has the "link-free" type (`list.var.int` → `list.int`), named `deref(<type>)` for a product or a sum. Only one level is followed per call; a missing variable is an error that names the slot (`type deref: at 2: can't read "::C": no such variable`), with no partial result. Works in lists, dicts, products, sums and recursive types; a cycle never loops.

#### 6.3b `type canonic` and `type parse`: the text form of a typed value (v17c)

The type is never in a value's string (EIAS). Where only text is kept (a file, a Tk tag, a key), `type canonic V` gives a text that keeps it: `{T}content`, with `T` the type's text (its name when it is named, qualified for a namespace's type; without the variant: a tagged sum carries it in its content, a union reads it from its own) and the content in its canonical form (numbers normalised, lists, dicts, products and sums rebuilt by the type), quoted as one list element. Notes are a set: `{double@alpha,zeta}5.0`, sorted, without duplicates, comma-separated; in a note any ASCII character other than letters, digits, `_` and `.` is written `%XX`. A value with notes only gives `{@vitesse}5`; a bare value `{bare}5`. `-notes` gives `T@notes` without the content.

`type parse TEXT` reads `{T}content`, `{T@notes}content` or `{@notes}content`: the type must exist (checked against the current definition), the content must fit it, then the notes are added. Two laws: `type parse [type canonic $v]` is `$v`, and `type canonic [type parse $t]` is `$t` when `$t` is canonical.

```tcl
type canonic [type cast int 0x10]                 ;# {int}16
type canonic [type cast list.int "1   2"]         ;# {list.int}{1 2}
type canonic {vitesse}[type cast double 5]        ;# {double@vitesse}5.0
type get [type parse {{pt}{1 2.5}}]               ;# pt
```

Not kept: the notes of elements inside a container, the types of the elements of a `list.any`, the `{-compact}` option (a representation). A bare value is written `{bare}text` (v17e).

**`{T@notes}` in a constructor (v17f).** The form written by `type canonic` is also read by a constructor: `set v {int@this}5` is an `int` noted `this`; `{double@zeta,alpha}5` carries two notes; `%XX` is decoded, the dot stays in the note (`{int@aspect.pente}1`). `{int@}5` is `{int}5`. Not on a variable name (`set {int@a}v`: a name takes a type), and `{@step}` stays an instrument note.

#### 6.4 Typed procedures (v16p)

The argument list of a procedure may be a **product type**, written in the type syntax; the annotation on the name gives the **return type**.

```tcl
type def pt {{double}x; {double}y}
proc {pt}shift {{pt}p; {double}dx ?= 0; {double}dy ?= 0} {
    set {pt}q [expr {$p(x) + $dx}] [expr {$p(y) + $dy}]
    return $q
}
shift $a 1 1                   ;# positional
shift $a {dy:}5                ;# positional, then labels
shift {dy:}5 {p:}$a            ;# labels only, any order
```

- **Fields.** Each field becomes a local variable carrying its contract (`set dx abc` in the body is refused). A default `?= v` makes the field optional; `?= [cmd]` is evaluated at each call, in the procedure: it can read the earlier fields (`{int}b ?= [expr {$a * 2}]`).
- **Call.** Positional arguments, then `{name:}` labels; a positional argument after a label is refused, and so is a field given twice. A label that does not name a field stays on the value if the target field is a sum (`ps {s:}abc` chooses the variant `s`), otherwise it is an error (`unknown argument label {zz:}: must be p:, dx: or dy:`). Messages: `wrong # args: should be "shift p ?dx? ?dy?"`, `missing argument "p"`, `argument "dx": expected floating-point number but got "abc"`.
- **Types written in the signature.** A field's type may be an anonymous product: the signature then reads by itself, and a bare list is checked and becomes the field's value. A field whose type is a product **all of whose fields have a default** is optional: when absent, it takes those defaults. An incomplete list is still refused (the constructor rule: to give only some fields, name them).

```tcl
proc {pt}shift {
    {{double}x; {double}y}p;
    {{double}x ?= 0; {double}y ?= 0}dp
} {(
    $p(x) + $dp(x), $p(y) + $dp(y)
)}
shift {100 200} {50 30}        ;# 150 230, of type pt
shift {100 200}                ;# 100 200: dp takes its defaults
shift {100 200} {50}           ;# argument "dp": {{double}x ?= 0; {double}y ?= 0} needs 2 values but got 1
```

- **Name the types that travel.** A product written in the signature is a type of its own: a `pt` value of the same shape is refused there (`unknown way to cast the value from pt to {{double}x; {double}y}`); it would need `{list}$a` or a `type convert`. And a value already of a named field's type goes in without being checked again (`{pt}p; {vec}dp`: 1.38 µs with typed values, 1.95 µs with bare lists, like the anonymous form). Types that pass from procedure to procedure are therefore better named (`type def pt …`); the anonymous form suits what only comes in as a list or as text.
- **The rest.** A last field `{list.T}args` receives the extra arguments, each validated (`proc sum {{int}a; {list.int}args} …`).
- **Variadics anywhere (v16y).** A field whose name ends with `...` (`{T}args...`) takes a variable number of words, wherever it is: without a default, **at least once**; with `?=` (often `?= {}`), zero times or more, and the default is a list of `T`. The fields before it are filled left to right (a field with a default takes a word only if enough remain for the rest), the fields after it are taken from the end, as in TIP 288; they cannot have a default. If `T` is a product, each repetition takes as many words as it has fields. The local (`args`) is a list of `T`. Only one variadic field per signature, and not a `var.T`. A last `{list.T}args` keeps its meaning (zero or more, one word per `T`).

```tcl
proc each {{{list}vars; {list}values}args...; {string}body} { … }   ;# foreach
each {a b} {1 2 3 4} c {x y} { … }          ;# usage: each vars values ?vars values ...? body
proc dict_set {{var.dict}d; {string}args...; {string}value} { dict set d {*}$args $value }
proc dict_create {{{string}key; {string}value}args... ?= {}} { … }
proc length {{pt}args...} { … }             ;# length 0 0 3 4 3 0: the points flattened
```

- **Automatic upvar.** A field `{var.T}n` receives a variable name: `n` is linked to the caller's variable, whose current value (if it exists) must be a `T`.

```tcl
proc incrv {{var.int}n; {int}d ?= 1} { incr n $d }
incrv k 10                     ;# k is the caller's variable
```

- **Return.** `proc {T}name`: the result of a normal return (end of the body, `return`, `return -code ok`) is cast to `T`, like `type {T}word`; a failure is an error `return value {T}: …`. An error, `break`, `return -level 2`, `tailcall` pass without a cast. A `return {U}$x` in the body adds nothing: `{U}` is only a local note, which expires with the procedure (no accumulation of types on the return).
- **Return labels.** When the return type is a sum, `return {v:}$x` chooses the variant `v`: `$x` is its payload. A label that is not a variant, or on a return type that is not a sum, is an error. `proc {S.v}f` always returns the variant `v`. `return {v 4}` (the value written plainly, v16w) works too. The label is also read in a body that is not compiled (`eval`, `uplevel`, `switch -matchvar`, v16x): the return is made in the procedure's type there before the label expires.

```tcl
type def R {{pt}point, {int}n}
proc {R}g {w} {
    if {$w eq "p"} { set {pt}z 1 2; return {point:}$z }
    return {n:}4
}
type get [g p]                 ;# R.point
```

- **Introspection.** `type proc name` returns `args {…} return T` (the canonical signature, empty for a classic procedure); `info args` returns the names of the fields, `info default` their defaults (`[cmd]` for a computed default).
- **Cost.** A typed procedure runs as fast as the same classic procedure doing all these checks by hand (1.15 µs against 1.17 µs for `shift $a 1 1`, without checks 0.47 µs); a label costs about 0.5 µs. See `exemples/bench_procs.tcl`.
- **Compatibility.** An argument list that is a valid Tcl list stays classic (`{a {b 2} args}`, and even `{a;}`); it is typed when it is not a list and contains a `{T}name`, or when it contains `;` and reads as a product. A classic procedure can have a return type (`proc {int}f {a} …`).

- **Unfolding the result (v16r)**: `proc {{*}T}name` checks the result as a `T`, then returns it **unfolded**, as ordinary Tcl data; `{{**}T}` returns it as a dict. The `{*}` unfolds like Tcl's (and like `$P(*)`), `{**}` gives the names, like `*args` and `**kwargs` in Python. Unfolding is deep: a product of products becomes a list of lists (or nested dicts), a `list.point` a list of lists; a sum returns the content of its variant (with `{**}`, a dict `{variant value}`; a literal variant stays itself). Since v16t, a typed product already unfolds in the list view (`lassign $P x y` works on a `point`): `{*}T` is now only used to return **untyped** data, and `{**}T` a dict.

```tcl
proc {{*}point}v  {{double}h} {( 0, $h )}     ;# 0 5      (checked as a point)
proc {{**}point}vd {{double}h} {( 0, $h )}    ;# x 0 y 5
type def rect {{point}nw; {point}se}
proc {{*}rect}r {} {( (0, 0), (10, 10) )}     ;# {0 0} {10 10}
proc {{**}rect}rd {} {( (0, 0), (10, 10) )}   ;# nw {x 0 y 0} se {x 10 y 10}
```

  An explicit cast follows the same rule: `type {{*}point}$p` returns the checked list `{x y}`, `type {{**}point}$p` the dict. The prefix is only recognised at the head of a delivery annotation (in the type language, `{*}point` would read as a field named `point`). Cost: 0.3 to 0.9 µs per call to check and unfold a point (`exemples/tcllib_geometry`).

- **The reified call: `type call` (v16z).** A signature is a product: the bound arguments of a call are a value of it. `type call`, in a procedure, returns the current call as a value of the **record** of its signature: each field under the name of its local (`-mode`: `mode`), with its type; a variadic field is a list there (`{int}args...`: `list.int`), a link `{var.T}n` the value of the variable. A field without a value at this call (an absent option without a default, a link to a variable that does not exist) is left out. `type call TYPE` builds instead a value of `TYPE`, a named product, each field taking the local of the same name, or its default. `type proc NAME` gives the text of the record (`record`), which can be named: `type def Request [dict get [type proc request] record]`. A classic procedure has the record `{any}a; …; {list}args`.

```tcl
proc {Rec}move {{pt}p; {double}dx; {string}-note ?= "?"} {
    set evt [type call Rec]          ;# Rec: {{pt}p; {double}dx; {string}note}
    lappend ::log $evt
    return $evt
}
move -note back {1 0} -1             ;# {1 0} -1 back, of type Rec
dict get [type proc move] record     ;# {pt}p; {double}dx; {string}note
```

  This is the event as data: a call that is stored, logged, or received as an argument by another call (`exemples/attentrel`, where one convolution targets another).

- **Lambdas: `apply` (v16z).** A lambda is a procedure without a name: its argument list accepts the same signature, and the annotation on the lambda word is its return type, like the one on the name in `proc {T}name`. Types are resolved in the lambda's namespace (its third element). The annotation may be written on a substitution: `apply {T}$f …` gives a return type to a lambda kept in a variable. A `type convert` rule, being a lambda, can type its argument.

```tcl
set sum [apply {double}{
   {
      {double}a;
      {double}b
   } {(
      $a + $b
   )}
} 5 10]                                          ;# 15
apply {int}{{x} {return $x}} 2.5                 ;# return value {int}: expected integer but got "2.5"
apply {int}$f 20.5 5                             ;# the same, on a lambda kept in f
lmap x $L { apply {int}{{{int}x} {expr {$x * $x}}} $x }
type convert celsius kelvin {{{celsius}c} {expr {$c + 273.15}}}
```

  The literal `{T}{…}` is rebuilt at each execution; the interpreter keeps one Proc per `{T}lambda` text (at most 512, then the table is emptied), so that the body is not recompiled at each call. Cost: 0.8 µs per call against 0.5 µs for the same lambda without a return type. A note (`{@trace}{…}`) is not a return type.

#### 6.5 Options and flags (v16q)

A field whose name starts with `-` is an **option**. Options are given at the start of the call, in any order, up to the first word that is not one (or `--`); a unique prefix is enough (`-dec` for `-decreasing`); the local variable drops the `-`.

```tcl
proc {list}sort {
    {flag}-decreasing;
    {int}-index ?= -1;
    {"ascii", "dictionary", "integer", {string}command}-mode ?= ascii;
    {list}l
} {
    set o [list -$mode]                ;# mode: ascii, dictionary, integer or command
    if {$mode eq "command"} {set o [list -command $command]}
    if {$decreasing} {lappend o -decreasing}
    if {$index >= 0} {lappend o -index $index}
    lsort {*}$o $l
}
sort -dec -index 1 -integer $L
```

- **Flag**: `{flag}-x`; the local is `1` if the option is given, otherwise `0` (or its default). `flag` is a predefined type (an integer).
- **Option with a value**: `{T}-x ?= v`; `-x value`, checked like an argument. Without a default and absent, the local stays undefined (`info exists x`). A computed default (`?= [cmd]`, `?= $var`) is evaluated only if the option is missing, and after the positional arguments: it can read them (`{int}-end ?= [expr {[llength $l] - 1}]`, v16y). A computed default of an argument sees the options given and those with a literal default.
- **Link option**: `{var.T}-o`; `-o name` links the local `o` to the caller's variable `name`, like `{var.T}o` (v16y). `?= name` gives the default variable; without a default and absent, `o` is a plain local. The usage writes it `?-o varName?`.
- **Exclusive options**: a sum. Each variant is an option: a literal (`"ascii"`) is an option without a value, a typed variant (`{string}command`) takes a value. The field's local (`mode`) receives the **name** of the chosen variant, the variant's local (`command`) its value; `-mode ascii` also chooses a variant without a value. Two different variants: `options -ascii and -integer are exclusive`. The default names a variant.
- **Dependent options**: a variant whose type is a product. Its fields without `-` are the values that follow the option, its `-x` fields are options that only make sense with it; all become locals.

```tcl
proc walk {
    {"flat", {{string}cmd; {int}-depth ?= 1; {flag}-follow}command}-mode ?= flat;
    {string}dir
} {...}
walk -command ls -depth 3 -follow /tmp   ;# mode=command cmd=ls depth=3 follow=1
walk -depth 3 /tmp                       ;# option "-depth" requires -command
```

- **Group**: `{T}-o`, `T` a product (named, reusable between procedures). The fields of `T` are the options (`-decreasing`, `-index`, and the variants of a sum field); the local `o` is a value of `T` (`$o(index)`).

```tcl
type def sortopts {{flag}decreasing; {int}index ?= -1; {"ascii", "dictionary"}mode ?= ascii}
proc sort2 {{sortopts}-o; {list}l} { ... $o(decreasing) ... }
sort2 -decreasing -index 3 -dictionary $L
```

- **Messages**: `bad option "-zz": must be -ascii, -command, -decreasing, … or --`, `ambiguous option "-d": must be …`, `option "-index" needs a value`, `option "-index": expected integer but got "x"`; the usage shows them (`?-decreasing? ?-index value? ?-ascii|-dictionary|-integer|-command value? ?--? l`).
- **`--`** ends the options and is not passed on (`sort -dec -- -x`: `l` is `-x`). It is only recognised in the option zone, in full (no abbreviation); after the options, or in a value position (`-index --`), it is an ordinary word; a procedure without options does not treat it specially. A negative number (`-5`) also ends the options; any other value that starts with `-` needs `--`.
- **With the rest**: options combine with positional arguments, labels, `{list.T}args` and the return type. `info args` returns the fields as written (`-decreasing`), `info default` their defaults (`0` for a flag).
- **Cost**: `sort -dec -index 1 -integer $L` 2.2 µs, against 3.3 µs for the same parsing written in Tcl (`tcl::prefix match`, checks, exclusivity); without options, 1.5 µs against 1.1 µs (`exemples/bench_options.tcl`).

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

**Grammar (EBNF).** Precedence, from the weakest: `,`, then `*`, then `.`, then `:`, then `( )`.

```ebnf
path        = spread , { "," , spread } ;      (* collection: a group *)
spread      = [ "*" ] , nest ;                 (* unfolding: the group merges into its parent *)
nest        = range , [ "." , nest ] ;         (* nesting; distributes: A.(x,y) = A.x,A.y *)
range       = step , [ ":" , step ] ;          (* list range: 0:end-1 *)
step        = word                             (* field, key, variant or index: the type decides *)
            | "'" , key , "'"                  (* always a key: 'end' *)
            | '"' , label , '"'                (* a label, copied as is into the result *)
            | "(" , path , ")" ;
index       = integer | "end" , [ "-" , integer ] ;

read        = "$" , varname , "(" , path , ")"
            | "${" , varname , "(" , path , ")" , "}" ;
write       = "set " , varname , "(" , path , ")" , value , { value } ;
construct   = "set {" , type , "}" , varname , "(" , path , ")" , value , { value } ;
```

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

A product of *n* fields is built in four ways, without guessing anything:

| form | example | rule |
|---|---|---|
| positional | `set {pt}a Jean 12` | exactly *n* values, in field order |
| field labels | `set {pt}a {age:}12 {nom:}Jean` | every value labelled; any order; omitted fields at their default |
| a single field | `set {pt}a {pt.nom}Jean` | the other fields at their default |
| path + positional values | `set {pt}a(age,nom) 12 Jean` | one value per leaf of the path (or per group, § 8.3) |

Added to these: **no argument** (`set {pt}a`, every field at its default) and **a product of the same type** (a copy). Anything else is an error:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

The bare name/value form (`set {pt}a nom Jean age 12`) **no longer exists**: it was confused with the positional form as soon as *n* arguments could be *n* fields (§ 16).

#### 8.2 Field labels

Each argument names its field with a label, `{field:}value` (v16l). A label cannot be confused with a global type of the same name.

```tcl
type def pair {{string}nom; {int}age ?= 0}
type def age int                       ;# changes nothing
set {pair}p {nom:}jean {age:}12         ;# jean 12
set {pair}p {nom:}zoe                   ;# zoe 0
set {pair}p {nom}jean                   ;# {nom} is not a type: ... write {nom:}
set v {age:}21                          ;# local annotation {age:} cannot be stored by "set"
```

**All or nothing:**

```tcl
set {pair}p {nom:}jean 12        ;# can't mix values named by a field label ({field:}value)
                                 ;#   and positional values in a pair constructor: name every field, or none
set {pair}p {nom:}a {size:}3     ;# unknown field "size:" in pair
set {pair}p {nom:}a {nom:}b      ;# duplicate field "nom" in pair
set {pair}p {aut.nom}x           ;# aut.nom names a field of aut, not of pair
```

A braced value can be labelled too: `set {seg}s {a:}{1 2} {b:}{3 4}`.

#### 8.3 Path and positional values

`set {T}v(path) v1 v2 …` builds the product (and its nested products); the named fields receive the values, the others their default. The **number** of values chooses the distribution, never their content:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# one value per leaf
set {rect}R((A,B).(x,y))     1 2 3 4      ;# the same
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# one value per element of the head group
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# one value for the whole path
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# with unfolding: one value, flat
```

Each value has the shape its read would return; the three counts coincide only if the reads coincide.

**Path + labels does not exist**: `set {rect}R(A) {y:}2 {x:}1` is an error (`1 targets but 2 values`). It is not planned until a use asks for it.

#### 8.4 Sums, unions and lists

**Two constructions (v16w).** The form of the declaration says whether the value carries its variant:

| | sum `,` | union `\|` |
|---|---|---|
| example | `{double}celsius, {double}fahrenheit` | `{int}n \| {list(2).int}pair` |
| value | `variant payload`: `fahrenheit 50.0`; a literal variant: its text | the payload alone: `5`, `3 4` |
| the variant | written in the value | computed from the value |
| at definition | every typed variant has a name (its tag) | the alternatives must be disjoint |
| direct `expr` | no: `$t(celsius)` | yes |

A sum never guesses: its value says its variant, through the string, in a file, from one interpreter to another. Two variants of the same shape (units, `{int}a, {int}b`) are therefore no problem. A union keeps nothing: its alternatives cannot overlap, so the value always shows which one it is. `type is {disjoint?}S` gives the pairs that can accept the same value; an overlapping union is refused at definition:

```tcl
type def U2 {{int}i | {string}s}
  ;# union: alternatives i and s can take the same value, so the value could not tell
  ;# which one it is; a union must be disjoint. Write a sum with "," to tag the alternatives
type def P {{pt}p | {vec}v}            ;# refused too: two products of the same shape
type def P {{pt}p, {vec}v}             ;# the sum: "p {1 2}" or "v {1 2}"
```

An enumeration (`{"on", "off"}`, or `{"on" | "off"}`) has the same value in both cases: a literal variant is its own name. The `{-strict}` and `{-deduce}` options of v16n–v16v no longer exist.

**Building a sum.** All these forms give `s 5`:

```tcl
set {S}v {s:}5                 ;# the label: a shortcut for the variant
set {S}v s 5                   ;# plainly: the variant, then its payload
set {S.s}v 5                   ;# by the variant's type
set {S}v(s) 5                  ;# by the path
set {S}v [read $f]             ;# by the string "s 5"
set {S}v 5                     ;# refused: value "5" names no variant of the sum S: a value of
                               ;# a sum is the name of its variant, then its value, as {i 5}, or {i:}5
set {list.S}L {i:}1 {s:}a      ;# each element: "i 1", "s a"
lappend L {s:}x                ;# lappend and lset receive the element's label
```

A value already typed with a variant's type goes into the sum (`set {int}n 5; set {W}w $n` gives `i 5` if `W` has only one `int` variant; otherwise the label is needed).

**A variant that is a product (v16o)** is built by its fields, like a named product:

```tcl
type def segment {{string}text, {{string}target; {list.segment}content}link}
set {segment.link}s {target:}$url {content:}$segs   ;# labels, any order, defaults
set {segment.link}s $url $segs                      ;# positional
set {segment}s(link.(target,content)) $url $segs    ;# a path through the sum
set {segment}s($v.($fields)) {*}$values             ;# the same, computed
type def segment.link                               ;# {string}target; {list.segment}content
```

The value is `link {URL SEGMENTS}`. The first step of a path chooses the variant (a single one for all the paths), the rest builds the value of its type; a path also goes through a sum that is a field of a product (`set {P}r(n.i,m) 4 5`). Messages name the anonymous product of a variant `S.v`.

**Reading.** `$v(s)` returns the payload, typed; `$f(w)` reads a field of the payload directly when the name is not a variant (`shape.rect`: `$f(w)` like `$f(rect.w)`). `type variant $v` returns the variant's name (its text for a literal). A sum value is a two-element list: `lassign $v name payload` also works without types.

```tcl
expr {$v(i) * 2}               ;# the payload, as a number
switch [type variant $v] { i {…} s {…} }
```

**Modifying.** The value carries its variant, so a write always says it: `set v 8` is refused, `set v(i) 8` writes `i 8`, `set v(s) x` changes the variant (`s x`), `incr v(i)` increments the payload.

**Casts.** Any value reads as a `string`: its text (`on`, `i 5`). A union reads as its alternative's type (`{U}5` goes to an `{int}` argument). A sum never does: `a value of the sum S is not of type int: it is "i" then its value; read that value by its path, as $v(i)`. What it says (the unit, the kind) is not lost silently.

**Conversions between variants.** Two variants of the same sum can receive a rule, one direction at a time:

```tcl
type def temp {{double}celsius, {double}fahrenheit}
type convert temp.celsius temp.fahrenheit {{x} {expr {$x*9/5.0 + 32}}}
set {temp}t {celsius:}100      ;# t is "celsius 100"
set t(fahrenheit)              ;# 212.0: reading an inactive variant, through the rule
set u {temp.fahrenheit}$t      ;# explicit cast: u is "fahrenheit 212.0"
type cast temp.fahrenheit $t   ;# the same
```

Reading an inactive variant and the cast `{S.v}` (or `type cast S.v`) take, in order: the rule; a refusal if the two variants have the same shape (an identity cast would say a celsius is a fahrenheit: `… both have the same form, so reading one as the other needs a rule: type convert temp.celsius temp.fahrenheit {{x} ...}`); otherwise the ordinary cast between the types of the two variants (`{int}i, {string}s`: `{i}5` read by `$v(s)` gives `5`). A rule never applies to a write (`set t(fahrenheit) 50` changes the variant without converting). No chaining (a rule a → b and a rule b → c do not make a → c), no assumed reverse.

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

**No annotation before a closing character or an operator (v16y).** A value never starts with `] } ) = < > ! & | ^ ? , ; ~ %`: after a close-brace, these characters keep Tcl 9's error, `extra characters after close-brace`. This keeps the diagnosis of a misplaced brace, the most common typo in Tcl: `proc p {} {set w foo}]` stays an error, instead of silently defining a procedure whose body would be `]`. The useful value starts remain allowed: `{flag}-x`, `{int}::n`, `{path}/tmp`, `{T}$x`, `{T}[cmd]`, `{T}"text"`.

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

**Expiration (v16m).** Each value annotated by a word (`::tcl::annotate`, `::tcl::annotraw`, `TclSubstTokens`), and each new value of a constructor that keeps a pending annotation, is registered in a per-interpreter list (`TclAnnotRegisterFresh`). Expiration changes the object's state **in place** (the `ptr2`, shared by its copies): the raw text and the pending notes become expired notes, under a mark (an empty element of the list of notes), which `TclAnnotSettle` no longer reads. It happens at the end of the command (`annotExpire`; `TclEvalEx` for a non-compiled script), and on leaving a bytecode (`TEBCresume`: `return {T}$x`, a command that ended in error). A registered value that nothing holds any more, neither itself nor its state, goes without expiring.

#### Other core hooks

| file | change |
|---|---|
| `tclBasic.c` | `TclTypesInit`: creates `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` takes the slow path for an annotated value; `INST_FOREACH_STEP` forgets the type; reading and writing typed elements (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` and `TclTypedVarAfterStore` around each write: the contract |
| `tclProc.c` | `TclTypedVarBindArg`: typed argument binding; typed signatures of `proc` and `apply` (`TclTypedProcSig`, `TclTypedLambdaRet`) |
| `tclCmdAH.c`, `tclDictObj.c` | loop variables (`TclTypedVarForgetByName`); `TclAnnotUnwrap` in dict reads |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | core conversions on the content (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): an annotated value is never converted in place |
| `tclListObj.c` | `lappend` through an abstract list's `replaceProc`; multi-index `lindex` without flattening |
| `tclCompExpr.c` | `is` operator (lexeme `IS_TYPE`, `IsTypeAhead`), assignments `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. The added files

| file | lines | content |
|---|---|---|
| `tclTypes.c` | 12,100 | typology, `type def/alias/get/cast/convert/deref/binary`, validation, widening, casts, C codecs, procedure and lambda signatures |
| `tclAnnotObj.c` | 6,200 | the annotated object, the `{T, N}` state, resolution, `note`, `type debug` (trace, proto-type inventory) |
| `tclTypePath.c` | 4,300 | path language, read, write, construction by path, pathindex object |
| `tclTypeParse.c` | 2,800 | parser and compiler of type expressions (model: `tclCompExpr.c`) |
| `tclTypedVar.c` | 2,200 | typed variables, constructors (`TclSetAnnotatedCmd`, `BuildProductEx`, field labels) |
| `tclTypeGeneric.c` | 1,400 | parameterized types, instances, phantom parameters, binary refinements |
| `tclVarRef.c` | 370 | `var` links (canonical name, access, stale links) |
| `tclTypes.h`, `tclTypesInt.h`, `tclAnnotatedApi.h`, `tclCompact.h` | 1,050 | structures (`TclType`, `TclTypeField`, `TclBinLink`…), interfaces, public C API, compact form |
| `tclTypeDebug.tcl` | 410 | the Tcl side of `type debug`: the prompt, conditions, the Tk console, the report; built into `tclTypeDebug.h` by `gen_typedebug.tcl` |

On the Tcl side: `layout/filebinary.tcl` (binary layer: `type binary decode/encode/deref/space`, `chan binary`) and `layout/binaliases.tcl` (aliases).

**Examples** (`exemples/`), besides the small scripts:

| directory | what it shows |
|---|---|
| `signatures` | procedure signatures against TIPs 288, 457, 460, 479, 480; options, links, variadics, typed lambdas |
| `bulletin` | a buggy procedure, debugged, typed, then moved to C, in four steps |
| `mise_au_point` | `type debug` step by step, proto-types, the Tk console |
| `cproc` | procedures with a C body, compiled at the first call (gcc or tcc) |
| `ffi` | calling a shared library function from its typed signature, through a C stub, without libffi |
| `tcllib_geometry` | tcllib's `math::geometry` in Tcl, typed Tcl and C; its 210 tests pass in all three |
| `cstruct`, `geometrie`, `matrices` | C codecs, compact form, measurements |
| `zip`, `journal`, `texte_enrichi`, `minidll`, `arduino` | real binary formats read and written through their types |

#### 14.1 The compact form (prototype)

**Per type**: `type def {-compact}polygon list.point`. Values of this type are **always** in cells: numbers are normalised at construction (the constructor is a conversion: `{0 0}` becomes `{0.0 0.0}`, `0x10` becomes `16`, a `double(32)` is rounded to `float`), and a core conversion (`string length`, `string range`, `dict`…) works on a string copy kept beside the cells. The type must be a list whose element has a layout: otherwise `type def` refuses, and the name stays free. Not yet for a family (`type def {-compact}{v({int}n)} …`); no option for an alias. `type is {compact?}polygon` says whether the type asks for it, `type is {compact?}$P` whether the value is in cells.

```tcl
type def {-compact}polygon list.point
set {polygon}P {0 0} {4 0} {4 3}      ;# {0.0 0.0} {4.0 0.0} {4.0 3.0}
type is {compact?}$P                   ;# 1, even after string length $P
```

**On a product**, `{-compact}` means "any list of this product is in cells", like an array of `struct` in C, whether the list is a variable, an element of another value or a field of a product. The product must have a layout (only fixed-size scalars, or products of such fields).

```tcl
type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
set {polyC}p 2 {{1 2} {3 4}}           ;# 2 {{1.0 2.0} {3.0 4.0}}
type is {compact?}pointC               ;# 1
type is {compact?}$p(pts)              ;# 1: the field pts is in cells
```

A list that is a field of a product (or of a nested product) is put in cells in the same cases as a list on its own: its type or its element asks for it, or the compact form is switched on everywhere. It stays so after a path write, a `lappend p(pts)` or a `type binary decode`.

**Fixed-size lists: matrices and N-dimensional arrays** (v16h). An element `list(n).T`, where T itself has a cell layout (a scalar, a product, or in turn a fixed-size list), is stored in the cell like the C array `T[n]`: `list(3).list(3).double` is a list of 3 cells of 24 bytes, `list(2).list(3).list(2).int(s,32)` an array `int32_t[2][3][2]`, and a product can have a field `{list(2).double}v` in its cells. A cell has at most 4,096 parts (a `list(100).list(100).double` matrix fits: 100 cells of 800 bytes).

```tcl
type def {-compact}matrix3 list(3).list(3).double
set {matrix3}M {1 2 3} {4 5 6} {7 8 9}   ;# normalised: {1.0 2.0 3.0} ...
type is {compact?}$M                      ;# 1
expr {$M(0.0) * $M(1.1) + $M(2.2)}        ;# 14.0, read in the cells
set M(1.2) 60.5                           ;# written in the cell
lindex $M 1 2                             ;# 60.5: each row is a list
tcl::unsupported::compact layout list(2).list(3).int(s,16)
    ;# size 12 align 2 fields {{0.0 int 2 0} {0.1 int 2 2} ... {1.2 int 2 10}}
```

A family goes through a named instance: `type def {-compact}mat3 mat(3,3)`. In C, a matrix in cells is read with `Tcl_AnnotatedGetCells` and the type of its rows (name the row: `type def row3 list(3).double`, `type def {-compact}matrix3 list(3).row3`), and made with `Tcl_AnnotatedCreateCells`; a batch `list.matrix3` is an array `matrix3[n]`: see `exemples/matrices/`. Like a product row, a matrix row is an ordinary list to the script (`lindex`, `foreach`, `lset`, arity checked); only the storage changes. `$M(i.j)` is read directly in the cell (≈ 0.2 µs, against 0.9 µs through the general path), and `set M(i.j) v` of a number writes the cell in place when nothing else sees the value (neither a copy nor a live view: otherwise the buffer is copied as before): 0.14 µs against 2.2 µs. Product of two 3×3 matrices (Tcl loops and `$A($i.$k)`): 33 µs, against 75 µs in ordinary form and 9 µs for raw lists with `lindex`/`lset`.

**Cells and bytes.** When the cell layout is the layout of the bytes asked for (same scalars at the same offsets under `-align`, the machine's byte order), `type binary encode` copies the cells and `type binary decode` fills cells directly, without an object per element: for 10,000 points, 15 µs and 10 µs instead of 0.5 and 3 ms. The bytes are the same as without cells; otherwise, the codec follows its ordinary path. A cast to a list in cells (`type cast list.pointC $P`) copies in one block the cells of a source with the same layout, or writes the elements of an ordinary list directly into cells. From C, `Tcl_AnnotatedCreateCells` makes a value in cells from an array of `struct`, without an object per element (`tcl::unsupported::compact cells TYPE BYTES ?STRIDE?` in Tcl).

**For the whole program**, behind `tcl::unsupported::compact on` (or the environment variable `TCL_TYPES_COMPACT=1`, to run the tests), the content V of a `list.T` list is stored in **cells** when T is a fixed-size scalar (`int(s,n)`, `int(u,n)` with n = 8, 16, 32 or 64; `double`, `double(32|64)`) or a product of such fields, nested products and fixed-size lists `list(n).T` included. One cell per element, laid out like the equivalent C struct (natural alignment, measured at compile time: `tcl::unsupported::abi`). `int` without a size does not qualify (it is a big integer).

```tcl
tcl::unsupported::compact on
type def point {{double}x; {double}y}
set {list.point}P {0.5 1.5} {2.5 3.5}
tcl::unsupported::compact info $P     ;# n 2 stride 16 elem point shared 1
tcl::unsupported::compact layout point ;# size 16 align 8 fields {{x float 8 0} {y float 8 8}}
```

- **Nothing changes for the script**: V stays a `Tcl_Obj`, of type `compactlist`, an abstract list (length, index, replace, elements). Every test and example gives the same output in compact form or not (`t70_compact` compares each case in both forms).
- **EIAS** (global switch only; a `{-compact}` type normalises): the compact form is used only if every number is written in its canonical form (the one Tcl regenerates: `0.5`, not `1` for a `double`, nor `0x10`, nor `1e3`) and every element too (spaces); otherwise, an ordinary list. The original string of the whole list is kept as is until the first modification. Writing a non-canonical value (`set P(1.y) 9`) makes the list ordinary.
- **Reading**: `lindex`, `foreach` return a **view** on the cell (type `compactelem`), without copying the element; `$P(i.x)` reads the number directly in the cell. A number read is still an allocation.
- **Writing**: `set P(1.y) …`, `lset`, `lappend` write in the cells (the buffer is copied if shared: a view or a copy keeps the value it read). An invalid value is refused before it is written.
- **What needs all the elements at once** (`lsort`, `lrange`, `{*}`) builds the ordinary list once, kept beside the cells as long as the value does not change. Core conversions (`string length $P`, `string range`, `dict`) work on a string copy, kept likewise: the cells stay. `tcl::unsupported::content $P` returns a copy that shares the cells.
- **For C**: `typeparse/tclCompact.h` gives `TclCompactViewOf(obj)` (data, n, stride, name of the element type), through the registered object types (`Tcl_GetObjType`). The pointer is valid as long as the value is neither modified nor converted. A stub function will replace this route.

Measurements (Linux x86-64):

| | ordinary form | compact form |
|---|---|---|
| memory, 200,000 points (`mallinfo`, PURIFY build) | 264 B/point | 16 B/point |
| area of 10,000 points in C, object route (`geo.c`) | 564 µs | 29 µs |
| `lappend` of 20,000 points to a `list.point` | 8.7 s | 0.25 s |
| area of 10,000 points in annotated Tcl (`lindex` + `$a(x)`) | 15 ms | 23 ms |

Reading element by element from Tcl is slower (a view and a number built at each access, instead of the kept element); the gain is in memory, at the C boundary and in modifications.

#### 14.2 `type {c!}export`: the C header of types

`type {c!}export T ?T ...?` returns the text of a C header (the target is a local annotation on the verb; `{llvm!}` is reserved): one `typedef` per named type, in dependency order, and `_Static_assert`s on `sizeof` and `offsetof` computed by Tcl with the same alignments as the compact form. If the extension's compiler lays things out differently (Linux i386: `double` aligned on 4), compilation fails instead of reading wrong bytes.

| Tcl type | C |
|---|---|
| `int(s,8/16/32/64)`, `int(u,…)` | `int8_t` … `uint64_t` |
| `double`, `double(32)` | `double`, `float` |
| product | `typedef struct {…} name;` (anonymous product: `struct {…}` in place) |
| `list(N).T` | `T field[N]`; `list(N).list(M).T`: `T field[N][M]` |
| `list($n).T` as the last field | flexible member `T field[]` |
| sum of literals | `enum { name_a, name_b }` |
| sum | `struct { int tag; union {…} u; }` (the comment gives the order of the tags) |
| `S($field)` | the `union` alone (the sibling field chooses) |
| `string(enc,N)`, `string(raw,N)` | `char field[N]`, `uint8_t field[N]` |
| `string`, `list.T`, `dict`, link | `Tcl_Obj *` (with `#include <tcl.h>`) |
| numeric defaults of all fields | `#define NAME_INIT { 1, 2.5 }` |

Refused, with a reason: `int` without a size, a variable-size array that is not the last field, a byte order foreign to the machine, a recursive type (no pointers yet), `bitarray`. Names are made valid in C (`carré` → `carre`, a reserved word → `int_`).

```tcl
type def point {{double}x; {double}y}
puts [type {c!}export point]
;# typedef struct {
;#     double x;
;#     double y;
;# } point;
;# _Static_assert(sizeof(point) == 16, "point: size");
;# _Static_assert(offsetof(point, x) == 0, "point.x: offset");
;# _Static_assert(offsetof(point, y) == 8, "point.y: offset");
```

`tcl::unsupported::abi` gives what these computations use: `byteOrder`, `pointer`, and for `i8 … i64 f32 f64 ptr` the size and alignment inside a structure (which `tcl_platform` does not say).

## Compatibility, design, limits

### 15. Compatibility and performance

**No valid script changes meaning.** Both added syntaxes take over forms that were errors: `{x}word` ("extra characters after close-brace") and an extended `$name(...)` index, which falls back exactly to Tcl 9's rule as soon as it is not well formed. Checks:

- the full Tcl test suite (`make test`, 51,095 tests, v16z): the only failures are those of a stock Tcl 9.0 in the same environment (8 `httpProxy` tests, no network). Tests that assumed `{x}word` is an error were updated (patch `tests-syntaxe.patch`): when they checked the error message, they write `{x}=word`, which keeps it; those of `testparser` show the `annot` token;
- equivalence with a stock Tcl 9 on 18,000 generated cases (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): no valid result changed;
- AddressSanitizer clean; memory stable over 1,000,000 constructions.

**Types are not the enemy of speed.** Measurements on the `cstruct` example (10,000 samples, `-align natural`):

| operation | Tcl layer | C codec | classic hand-written Tcl, unchecked |
|---|---|---|---|
| encoding (`type binary encode`) | 333 ms | 1.3 ms | ≈ 3.3 ms (`binary format`; 330 µs measured for 1,000) |
| decoding (`type binary decode`) | 485 ms | 3.0 to 3.7 ms | 7.3 to 10.5 ms (`binary scan`) |

The C codec, which also **checks** every value against its type, is 2 to 3 times faster than classic hand-written Tcl, and 130 to 250 times faster than the generic Tcl layer.

**Typed procedures.** The text of a type is parsed only once: the object that carries it keeps its classification as long as the interpreter's type table has not changed (a `type def` invalidates all classifications). In a procedure, the name assigned by `x = …` or `x is T = …` is a compiled local variable, stored without a lookup by name (`storeScalar`, and the `declareLocal` instruction for `X is T = v`, v16c), and `X is T = v` stores an already conforming value without going through a note again. Measurements (`geometrie` example, 10,000 points):

| operation | annotated | classic Tcl |
|---|---|---|
| `X is point = $a` | 0.6 µs | — |
| `geo::distance` | 4.7 µs | 1.9 µs |
| perimeter | ≈ 38 ms | ≈ 18 ms |
| point in polygon (`dedans_tous`) | ≈ 141 ms | ≈ 68 ms |

**Loops.** `foreach` and `lmap` on a typed list walk its content, whose elements are typed once and for all: the loop variable receives the typed element without it being rebuilt at each iteration. On 10,000 points, `foreach q $P {}` takes 0.24 ms against 0.16 ms in plain Tcl (2.6 ms before v15u). The same holds for a list in cells (v15y). A product field read by `$a(x)` is also typed once and for all in the content when the content is not shared.

**Reading a field.** A compiled read `$a(x)` keeps, in its index object, the product type seen last and the field's position: the next read on the same type goes straight to the element. In `expr`, an operand typed `int` or `double` is read directly in its content. Since v16c, `$a(x)` as the operand of an arithmetic operator or a numeric comparison is compiled to `loadFieldNum`: the field's number, without a type wrapper (the operator would only read the content), read directly in the cell for a view in compact form; ≈ 60 ns, against 90 ns before (and 0.14 µs in v15x). Since v16g, the same holds for the bit operators (`&`, `|`, `^`, `<<`, `>>`, `~`).

**Declaring with `set`.** `set {T}x a b…` of a product whose fields are scalars (`int`, `double`, `string`), with as many bare values as fields, validates each value in place and stores it in the content without a copy; in a procedure, `x` (a simple name) is a compiled local variable, and the value is stored without a second lookup of the name. `set {point}A 1.0 2.0`: 0.6 µs, against 1.3 µs in v16f. Any other form (field labels, annotated values, products of products, a refused value) follows the general route, which no longer copies the content it has just built.

### 16. Design choices

| choice | reason |
|---|---|
| `{x}word` takes over a syntax error | full compatibility, no new quoting rule |
| no glued stacking `{a}{b}$x` | ambiguous with `{int}{300}`; you stack by annotating an annotated value |
| only a constructor resolves a type | `{T}$x` outside a constructor costs nothing and never fails |
| an annotation expires with the command that receives the word (v16m) | no action at a distance: the error of a cast arises in the command that contains the word (a `catch` catches it); a note does not change meaning when a type is defined later |
| local annotations recognised by their characters | a type name cannot contain `-` or `?`: no collision possible |
| `type def` nominal, `type alias` structural | tell units apart (`mm`, `kg`) without changing values |
| automatic widening, explicit narrowing | no silent loss of information |
| explicit conversions between nominal types | `mm` → `m` needs `{m}$a` or `type cast` |
| bare name/value form removed | `set {png}p chunks $l` could be read two ways; names are now written in the source (`{chunks}…` or `p(chunks)`), never guessed from the data |
| field labels: all or nothing | a mix would be a disguised positional form |
| a path substitutes nothing | data containing `[script]` is never executed by the path parser (double substitution is gone); same rule as an array |
| atomic substituted values | `$D($k)` reads the key `$k`, even if it contains `.` or `,` |
| path + labels not provided | no proven need; we will see with use |

### 17. Limits and open points

**To fix or to decide**

- **Tensions with EIAS.** A type lost through the string (no readable text form), equality, bare values of a `dict.point`, types specific to an interpreter (the same name designates two types in two interpreters): see `doc/CARTOGRAPHIE-EIAS.md`. Settled: the atomicity of products (v16t), reading after free and global names (v16u), the variant of a sum, now in its value (v16w, `doc/DIAGNOSTIC-SOMMES.md`).
- **`namespace path` and caches.** Resolving a name follows the path of the current namespace; a change of path invalidates the caches (`resolverEpoch`). A TclOO object's namespace has its types like any other; they disappear with the object.

**Not done yet**

- A "non-existent" value in general: `opt(T)` covers binary links, there is no general form.
- A binary union (two readings of the same bytes, C's `union`: refused deliberately, not to be confused with the type union `|`), shared bit fields, a list "up to the end of the file".
- A comment annotation `{#}` at parser level, annotation of the first word (`{c}proc`), declaration annotations (`proc p {{?}{…} {!}arg}`).
- `type debug` (v16y): no remote debugger nor editor integration; no breakpoint per line; the place of a command in a body written in expression syntax is approximate; a note put on a list is on the list, not on its elements (`foreach`, `lindex`, `$L(0)` do not propagate it: note the elements); a stop condition is written in a hook.
- Notes per element of a typed list (today a note is on the whole list); erasing all notes with `{@}$x`.
- C codec: named spaces, finite `-linkdepth`, `-dedupe`, a null link for a field with a default, bit fields (the rest is covered since v16d).
- Compact form: cells with pointers (`string`, nested lists of variable size), `lindex` without a new view, a stub function for C, `lappend` in constant time (today one copy per append, as for an ordinary typed list).
- `type {c!}export`: pointers for recursive types; an `{llvm!}` target; the reverse direction (C header to `type def`).
- `type def {-compact}` for a family itself (a named instance works: `type def {-compact}mat3 mat(3,3)`); `type is`: a value whose text is also a type name (`type is {compact?}$x` with `x` = `point`) is read as the type.
- Typed signatures for TclOO methods; commands with two calling forms (`switch string pattern body …` or `switch string {pattern body …}`), which would need a union of signatures.
- `cproc`: lists of strings, sums. `ffi` (prototype): structs by value, variadic functions, memory ownership, `errno`, checked pointers; struct encoding still goes through the Tcl layer.
- Optimisations: writing to a typed variable (about 7 times a plain write); a computed default, re-evaluated at each call.
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
| label | local annotation naming the field or the variant of an argument: `{age:}12`, `{i:}5` (v16l) |
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
