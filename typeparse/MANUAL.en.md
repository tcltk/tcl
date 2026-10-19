# Annotated types manual — chapters

*(English translation of `MANUEL.md`, which remains the reference. This file
collects the manual's chapters as they are written; patch notes stay in
`noyau/LISEZMOI.md`. Identifiers in the examples are kept as in the French
original, so that they match the example scripts and error messages.)*

## Dict and product: two ways of describing a structure

For a C programmer: a Tcl **list** is an array, a **dict** is a structure
with free keys, a **product type** is a declared `struct`. The last two look
very much alike, and one often moves from one to the other.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# product
type def fiche  {dict(nom, age?=0).string}              ;# typed dict
```

### What they have in common

| | product `pt` | dict `fiche` |
|---|---|---|
| construction by name | `set {pt}a {nom}Jean {age}12` or `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| access to a field | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| default of a field/key | `age ?= 0` (or `=`) | `age?=0` |
| unknown field | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| serialized value | Tcl list | Tcl key/value list |

Defaults are written the same way: `?=` is used in both cases, and a default
can be a literal, `[script]` (evaluated at each construction) or
`$variable`. In a product, the older `=` is still accepted.

### What sets them apart

* **Fields present.** In a product, every field always exists: an omitted
  field takes its default, and a field without a default must be given
  (`field "x" of pt has no value and no default`). In a dict, a key without
  `?` is mandatory (`key "nom" is missing in fiche`) and a key marked `?`
  may be absent.
* **Order.** A product has ordered fields; it can also be built
  **positionally** (`set {pt}b Jean 12`, like initializing a C struct). A
  dict has no positional form.
* **A single field.** A product accepts the `{T.field}` prefix:
  `set {pt}c {pt.nom}Jean` sets `nom`, the other fields take their default
  (`Jean 0`). It is the same notation as `{Val.i}5` for a sum.
* **Field types.** Each field of a product has its own type
  (`{string}nom; {int}age`); a `dict(...)` has a single one for all its
  values (`.string`).
* **Size.** A product has a fixed number of fields, set by its definition.

### Naming fields: tags or path (no more bare name/value)

The bare name/value form (`set {pt}a nom Jean age 12`) has been **removed**:
it could be mistaken for the positional form whenever *n* arguments matched
*n* fields. A product is now built in three ways, with nothing guessed:

* **positional**: as many values as fields, in order (`set {pt}b Jean 12`);
* **field tags**: `set {pt}a {age}12 {nom}Jean`. The tag is the most recent
  annotation of each argument; it is read in the context of the product, so
  a field name wins over a global type of the same name (`type def age int`
  changes nothing), just like a variant name for a sum. Any order; omitted
  fields take their default. **All or nothing**: mixing tags and positional
  values is an error (`can't mix values named by a field annotation …`), and
  so is an unknown tag (`unknown field "taille" in pt`). A value carrying its
  tag in a variable counts (`set v {age}12; set {pt}a {nom}Jean $v`);
* **path**: `set {pt}a(age,nom) 12 Jean`, handy when the names are computed
  or the values are in braces.

The dict keeps its key/value form: a dict has no positional form.

### Choosing

* A **known, stable** set of fields with varied types: product.
* **Free or optional** keys of a single type: dict.
* A format with several possible shapes (PNG, GIF…): a **sum** of products
  (see `exemples/png_somme.tcl`).

## bitarray: an integer whose bits you read

`bitarray(n, order)` is an n-bit integer (0 ≤ v < 2^n). The order says where
index 0 starts:

| | `lsb0` (default) | `msb0` |
|---|---|---|
| bit at index 0 | the RIGHTMOST one (`1 << 0`) | the LEFTMOST one |
| use | registers, masks, C | protocol diagrams (RFCs) |
| index of a bit if the width changes | unchanged | shifted |

Take the byte `0xB2`, written `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Reading.** `$b(i)` gives the bit (0 or 1). `$b(i:j)` gives bits i to j as
ONE integer, of type `bitarray(j-i+1, order)`: `$b(1:4)` is 9 for lsb0;
`$c(0:3)` is 11 for msb0 (the four leftmost bits, `1011`).

**Writing.** `set b(0) 1`, `set b(4:7) 0`. In `expr`, a slice is written in
double quotes, because `:` belongs to the `expr` parser:
`expr {"b(1:2)" = 3}`. A value that does not fit in the targeted bits is
rejected.

**Two orders, two types.** `bitarray(8,lsb0)` and `bitarray(8,msb0)` do not
mix: assigning one to the other is a `type mismatch`. The default
`bitarray(8)` is `bitarray(8,lsb0)`.

**Functions.** `popcount(x)`: number of bits set to 1; `bitlen(x)`: width of
a bitarray, otherwise the number of bits of the integer.

**Inside other types.** A product field (`{bitarray(8)}flags`), a list
element (`list.bitarray(4)`) or a procedure variable are bitarrays like any
other: `set r(flags.7) 1`, `set L(2.3)`.

**Does it look like a dict?** No: no keys, no fields, a single integer. To
describe the fields of a binary header, combine a product of bitarrays (see
the previous chapter).

Complete examples: `exemples/bitarray_exemples.tcl` (the "packed" byte of a
GIF and TCP flags in `msb0`, Unix permissions in `lsb0`).

## type deref: taking a snapshot of links

A `var` link holds the NAME of a variable and follows its value on every
read. `type deref` replaces each link by the value the variable has AT THAT
MOMENT: the result no longer changes afterwards.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : the link follows B
    puts $v                             ;# 1 2 3 : the snapshot is frozen

* The result has the "link-free" type: `list.var.int` becomes `list.int`,
  and is handled like any list of integers.
* A missing variable stops everything with an error naming the slot:
  `type deref: at 2: can't read "::C": no such variable`. There is no
  partial result and the original value is untouched.
* Only one level is followed: if a target variable itself holds links, they
  are kept; a second `type deref` follows them.
* Works in lists, dicts, products and SUMS, nested or not, and in RECURSIVE
  types (trees such as `tree(var)`). The variant of a sum is kept.
* Graphs are handled level by level: if a node's neighbours are links
  (`{list.var.node}next`), `type deref` gives the node with its neighbours
  replaced by their values, which still contain links; a second
  `type deref` goes one level deeper. A cycle therefore never loops.
* The result's type is a type of its own, named `deref(<original type>)`
  (`deref(tree(var))`, `deref(mix)`), computed once and then reused: two
  snapshots of the same type have the same type.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# a tree whose values are links
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

See also `MANUAL-deref.md`: the `var` link type and `type deref`, with
examples in `exemples/deref_manual.tcl`.

## Sum and C `union`: not to be confused

For a C programmer: a **sum** (`{{A}x, {B}y}`) looks like a `union`, but it
is not the same thing.

| | C `union` | sum |
|---|---|---|
| what it holds | the **same bytes**, read in several ways | **a single** variant, which carries its name |
| who knows which one is valid | nobody: the programmer must know | the value itself (`type get` gives `S.x`) |
| reading the wrong variant | allowed, you read arbitrary bytes | impossible: you access the variant you have |
| size in a file | that of the largest view | that of the chosen variant |
| choice of the variant | none, there is none | by a field already read (`S($tag)`) or by its name (`S(x)`) |

Consequences:
* There is **no `union` in this model**, on purpose: the safeguards are
  kept. A sum cannot be read "the wrong way".
* When the format does not say which variant is valid (Tcl's
  `Tcl_ObjInternalRep`, whose discriminant is the `typePtr` address, outside
  the union), the variant is chosen **at the call**:
  `Tcl_ObjAs(twoPtrValue)` (see `exemples/tcl_obj.tcl`). The caller knows,
  as in C, but the type is still checked.
* A sum where several variants have the **same shape** is not ambiguous
  once the variant is chosen by its name (`S(y)`): there is nothing left to
  guess.

### Existing, being null, not existing

Do not confuse three situations for a value pointed to by a link (type
`*(...)`, to come):
* **present**: the link designates a value;
* **present but null or empty**: the value exists, it is `0`, `{}` or `""`;
* **non-existent**: the link is null (offset 0, for instance), there is
  nothing to read.

A field default (`= {}`) does not tell the last two apart. A sum with a
valueless variant (`{{T}some, {}none}`) does. This is not implemented yet;
see the design note.

## Binary types: reading and writing bytes (`type binary`, `chan binary`)

A field says, through its TYPE, how it is written as bytes. Nothing else to
learn: it is the principle of `bitarray(16)`, which already carries its
width. The `.` is only used to COMPOSE (`list(3).T`); the representation is
in the type's parentheses.

    type def mz {"MZ"}                           ;# a constant: sum of one literal
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| type | representation |
|---|---|
| `int(u,n)` / `int(s,n)` | unsigned / signed n-bit integer (multiple of 8, 8 to 1024) |
| `double(32)` `double(64)` | IEEE float |
| `string(raw,N)` | N raw bytes |
| `string(ENC,N)` | text on N bytes, padded with NULs |
| `string(ENC,z)` | NUL-terminated text (2 bytes in utf-16, ucs-2) |
| `bitarray(n)` | n bits (multiple of 8), read as an unsigned integer |
| `"MZ"` (sum of literals of equal length) | a constant, checked by the type |
| `list(N).T`, a product of these types | N elements; nesting is allowed |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. A text
that cannot be encoded losslessly (`é` in ascii) or that does not fit in N
bytes is rejected.

**Endianness.** Last argument: `be` (big-endian, network order) or `le`:
`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Without it, the
call's endianness applies (`-endian`, little by default). It does not change
the values: `int(u,16,be)` and `int(u,16,le)` are interchangeable.

**Automatic widening.** A refinement goes where a wider type is expected:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: every int(u,8) is an int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 fits in -32768..32767
    set {int(u,8)}v $n               ;# rejected: narrowing (explicit cast)
    set {list.int(u,8)}L $liste_int  ;# rejected for lists too

A type defined by `type def port int(u,16)` stays nominal: `port` goes into
an int, but a plain `int(u,16)` does not go into a `port`.

**Alignment.** By default products are compact (packed), like network and
PE/ELF headers. `-align natural` (or `-align N`) adds the padding of C and
LLVM: each field starts at a multiple of its alignment (its size, capped at
8 or N), and a product's size is rounded up to its alignment. Padding is
zero when writing and ignored when reading, unless `-strictpad 1`.

    type binary entete                           ;# the representation, as the core sees it
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# the channel advances
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

Reading and writing are written in Tcl (`layout/filebinary.tcl`) on top of
the core: `type binary` describes, decodes and encodes bytes in memory,
`chan binary` reads and writes on a channel. The name `file binary` (the old
one) remains available as an alias.

**`type binary`, queries and verbs.** Questions about a type are written as
a local annotation on the type argument (the final `?` distinguishes it from
any type name); actions on bytes remain verbs:

    type binary entete                          ;# the representation (bare form)
    type binary {what?}$T                       ;# same
    type binary {sizeof?}$T ?-align A?          ;# size in bytes (error if variable)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 if $octets decodes as T, else 0
    type binary decode T $octets ...            ;# reads the bytes in memory
    type binary encode T $valeur ...            ;# same, encode
    type binary deref ...                       ;# same, deref
    type binary space ...                       ;# same, space (named spaces)

By default `{is?}` accepts a valid prefix (recognizing a header); `-exact 1`
requires all the bytes to be consumed. An invalid option, or a type that is
unknown or has no binary representation, are still errors: only a decoding
failure gives 0. An unknown annotation ending in `?` is an error
(`unknown question "oups?"`); any other annotation is ignored. A type named
after a verb (`type def decode ...`) is still described by
`type binary decode`, as long as no other argument follows. `decode`,
`encode`, `deref` and `space` require `layout/filebinary.tcl` to be loaded
(otherwise: `the binary layer is not loaded`).

**Short aliases.** `layout/binaliases.tcl` gives short names to the usual
representations (the Rust and Zig convention): `u8 u16 u32 u64`,
`i8 i16 i32 i64`, `f32 f64`, with the suffixes `be`/`le` (`u16be`, `i32le`,
`f64be`), and `cstring`, `wstringz`, `latin1z`. They are `type alias`es: the
library also serves as a cheat sheet for binary formats. The `i8…i64` here
are signed; LLVM's `iN` have no sign, so there is no exact correspondence
(arrays `[N x T]` and packed structs correspond to `list(N).T` and to
products).


### Level 2: sizes, counts, positions and variants read from the file

Always the same syntax: `$field` in the type's arguments, like the
parameters of parameterized types, or an unsigned integer type used as a
prefix.

| notation | meaning |
|---|---|
| `string(utf-8,int(u,16,be))` | text preceded by its size in bytes (Pascal string) |
| `string(raw,$len)` | N bytes, N being the value of the field `len` |
| `string(utf-8,$len - 4)` | a Tcl expression over fields already read |
| `list(int(u,8)).T` | list preceded by its number of elements |
| `list($n).T` | n elements, n being a field |
| `at($off).T` | the value is at position `$off` in the file (the cursor does not move) |
| `corps($tag)` | the sum `corps`, whose variant is chosen by the field `tag` |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# variant names = tags
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Rules: a `$field` must designate a field **already read** of the current
product or of an enclosing product (`type binary` checks it); a product
field is read as `$h(off)`. When writing, a size or count that does not
match the field it depends on is an error, and the tag must be the
variant's. `type binary {sizeof?}` rejects a type whose size depends on the
data. Complete example (PE, ELF, messages): `exemples/niveau2.tcl`.

Examples: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Limits: no
unions or shared bit fields, no "until end of file" list.

#### Chosen variant: `S(name)` and type parameter

A sum `S` can also have its variant **chosen at definition time**, without a
selector field in the file:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# the variant tp, spelled out
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# or given by a parameter
type binary decode {R(tp)} $octets
```

`U(tp)` is a single-variant type: its size is known
(`type binary {sizeof?}`), the value read is `U(tp).tp` and there is
nothing to guess, even if several variants have the same shape (which made
the value ambiguous before). `U($field)` keeps its meaning: a variant chosen
by a field already read. If `$v` is a bound parameter of the type, the first
case applies. Example: `exemples/tcl_obj.tcl` (`Tcl_ObjAs(twoPtrValue)`).
Test: `t36_somme_choisie.tcl`.

#### The link type `*(origin,repr).T`

A file field often holds an address, not the value. The link type reads the
address (`repr`, an integer) then the value `T` found there; the field's
value is the target's.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Origins, as in a file system: `//` (or `file://`) start of the data, `./`
start of the enclosing structure, `../` its parent, `.` the field itself
(see below). Options: `null` (address 0 means "no target"), `null=V`. With
`opt(T)` (see `unit`), a null address gives `none`, otherwise `some`: that is
the difference between "null" and "non-existent".

Generic aliases (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) give short
names: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` in
`layout/binaliases.tcl`. When writing, the targets are appended at the end,
aligned, and the addresses are computed. Recursive types, `memory://` and
named spaces are described below. Details: `noyau/CONCEPTION-lien.md`.

**The dot `.`: the address is relative to the field's location.** `./`
marks the *start of the structure*; `.` marks *the field itself* (where the
address is written). It is the origin of "from here" offsets, common in
formats with relative offsets (`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# field r is at offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# same result, but with ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Memo: `//` start of the data · `./` start of the structure · `../` start of
the parent structure · `.` position of the field. The `.` notation is kept
short on purpose; a more telling name will only be considered after use.

#### How far to follow links: `-linkdepth`

A link can be followed right away or left as is. The call option
`-linkdepth` sets the number of links followed (like the range of
radiation: a gamma ray goes through everything, a weakly penetrating ray
stops at the skin):

    type binary decode H $d                   ;# 1 {7 8}   Inf, the default: follow everything
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   one level of links
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      no link followed
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

An unfollowed link is an annotated integer (absolute position of the target
in the data). Copying, `lindex` and `string cat` keep the annotation; a
computation loses it, and the result can no longer be followed. A link is
followed on demand, one step at a time:

    type binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 by default: a single step)

What this gives you: reading only the header of a large file; walking a
linked list or a tree (the type can be recursive:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); locating a damaged link;
modifying a field in place, since `type binary encode` rewrites an
unfollowed address as is instead of reorganizing the file. A cycle is an
error only with `Inf`.


## Named spaces

A link can designate a space registered by name instead of the call's data.

```tcl
set sp [type binary space file $octets -name lib]          ;# annotated value of type space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segments {address offset size}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ is equivalent
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# no DATA: the space is enough
type binary space list | get NAME | delete NAME
```

The default name is computed (`space1`, …); the creation command registers
the space; a name already taken is an error. `//name/` requires a space
without segments, `memory://name/` a space with segments; an address outside
the segments is an error. A real target cannot be written into a named
space: only the frontier address (`-linkdepth 0`) is rewritten.

## Writing a null link

A link field that has a default value and whose value equals that default is
written as a null link (the link's "null" address, no target written).
Reading does the reverse: null address → default value. Without a default
(and without `opt`), reading a null link is still an error.

## -dedupe: sharing identical targets

`type binary encode TYPE VALUE -dedupe 1` (also `write`) writes only once
the link targets whose bytes are identical; the links concerned get the same
address. By default (`-dedupe 0`) every target is written. Only targets with
no nested link are shared (the internal addresses of a target depend on its
position); reading back gives the same value.

## Size depending on a field: checked at construction

A list whose number of elements is an expression over other fields
(`{list($n).Sample}items`, `list($n - 1).u8`) is checked as soon as the
product is built: `set {Batch}b(n,items) 3 {{1 1.0 0}}` is rejected
(`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).
Omitted fields count with their default value. The check does not cover
field-by-field assignments (`set b(n) 3`: an intermediate state is
legitimate); `type binary encode` checks again when writing. Byte sizes of
strings (`string(raw,$len)`) are not checked at construction.
