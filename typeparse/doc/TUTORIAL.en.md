# From a Tcl prototype to C, in three steps

A program is often written in three stages: a **prototype** to find the right
shape, then a **robust** version, and finally a **fast** version of the parts
that matter. In Tcl, the second and third stages usually mean rewriting: hand-written
argument checks, then a C extension of which half is glue between Tcl and C.

With the types of the TIP 764 prototype, the same program goes through the three
steps **without its callers changing**:

1. **Tcl**: the prototype;
2. **Typed Tcl**: the same procedures, now stating what they take and what they
   return;
3. **Tcl and C**: the same signatures, with the bodies that matter written in C.

The example is small: polygons (area, perimeter, point inside, translation,
bounding box). The code is in `exemples/tutoriel`. Its identifiers are French,
the language of the project:

| French | English |
|---|---|
| `aire`, `perimetre`, `contient` | area, perimeter, contains |
| `deplace`, `boite`, `polygone` | move (translate), box, polygon |
| `-ouvert`, `etape`, `essais`, `vitesse` | open (not closed), step, tests, speed |

| file | role |
|---|---|
| `etape1_tcl.tcl`, `etape2_type.tcl`, `etape3_c.tcl` | the three steps |
| `essais.tcl 1\|2\|3` | the same calls for each step (`essais_1.out`, `essais_2.out`) |
| `vitesse.tcl ?tcc?` | the three steps measured side by side |

## Step 1 — the prototype

A point is the list `{x y}`, a polygon the list of its points:

```tcl
proc poly::aire {pg} {
    set a 0.0
    lassign [lindex $pg end] x0 y0
    foreach p $pg {
        lassign $p x y
        set a [expr {$a + $x0 * $y - $y0 * $x}]
        set x0 $x
        set y0 $y
    }
    expr {abs($a) / 2}
}
```

This is good Tcl: short, readable, and it works. Two things are missing.

**Nobody checks the shape of the data.** A point with three values goes through
silently, and the area is wrong:

```
poly::aire {{0.0 0.0 0.0} {1.0 0.0} {1.0 1.0}}   ;# 0.5, no error
poly::aire {{0.0 0.0} {x 1.0} {1.0 1.0}}         ;# cannot use non-numeric string "x" as right operand of "*"
```

The second message comes from deep inside the loop, without saying which
argument was wrong. To do better, you would have to write the checks yourself:
`llength $p == 2`, `string is double`, on every point of every call.

**An option costs a page.** `poly::perimetre ?-ouvert? pg` requires parsing
`args` by hand: the loop over options, `--`, the message for an unknown option,
the message for a wrong number of arguments. That is 29 lines for the
procedure, half of them for its arguments.

## Step 2 — typed Tcl

### Naming the data

```tcl
namespace eval poly {
    type def point    {{double}x; {double}y}
    type def polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}
```

- **`point` is a product**: two named fields, each a `double`.
- **`polygone` is a list of points.**
- **The types belong to the namespace** `::poly`, like the procedures
  (`::poly::point`).

The value does not change shape: a `point` is still the list `{x y}`, and a
polygon still the list of its points. The type only says what the value must be.

### Signatures

```tcl
proc {double}poly::aire {{polygone}pg} {(
    a = 0.0;  q = [lindex $pg end];
    foreach p $pg {( a = $a + $q(x) * $p(y) - $q(y) * $p(x);  q = $p )};
    abs($a) / 2
)}
```

- **`{double}poly::aire`**: the type of the result. It is checked on return.
- **`{{polygone}pg}`**: the argument list is itself a type (a product). The
  argument `pg` must be a polygon. A plain list of the right shape is accepted,
  and checked point by point, at the call.
- **`$p(x)`**: a field is read by its name.
- **`( … )`**: the body is an expression. Inside it, `a = …` assigns, `;` chains,
  and the last expression is the result. `( x, y )` builds a list, and
  `(xmin, ymin) = $p` takes one apart.

The option that cost a page is written in the signature:

```tcl
proc {double}poly::perimetre {{flag}-ouvert; {polygone}pg} {(
    l = 0.0;
    q = $ouvert ? [lindex $pg 0] : [lindex $pg end];
    foreach p [lrange $pg $ouvert end] {( l = $l + hypot($p(x) - $q(x), $p(y) - $q(y));  q = $p )};
    $l
)}
```

`{flag}-ouvert` is an option without a value, and the local `ouvert` is 0 or 1.
That one line also gives unique prefixes (`-ouv`), `--` and the usage messages:

```
poly::perimetre -ferme $carre    ;# bad option "-ferme": must be -ouvert or --
poly::deplace $carre             ;# wrong # args: should be "poly::deplace pg dx ?dy?"
```

A default is written `?= value`: `{double}dy ?= 0.0`.

### What you gain

Both errors of step 1 are stopped at the call, with the name of the argument:

```
poly::aire {{0.0 0.0 0.0} {1.0 0.0} {1.0 1.0}}   ;# argument "pg": ::poly::point needs 2 values but got 3
poly::aire {{0.0 0.0} {x 1.0} {1.0 1.0}}         ;# argument "pg": expected floating-point number but got "x"
```

The **results are the same** as in step 1: `essais.tcl 1` and `essais.tcl 2`
differ only in these messages. The file goes from 78 lines to 38.

A caller that knows nothing about types sees no difference: a `point` is still a
list (`lassign [poly::point 1.0 2.0] x y`), a polygon a list of lists, and a
`boite` the list of its four fields.

### What it costs

In the prototype, checking and path access are not free. Here, step 2 is **1.4
to 1.8 times slower** than step 1 (table below):

- **Binding.** A typed procedure costs about 0.4 µs more per call.
- **Checking.** A 1,000-point polygon given as a plain list is checked point by
  point at every call (about 75 µs). A typed variable
  (`set {poly::polygone}G …`) is checked only once, when it is filled.
- **Paths.** Inside the loop, `$p(x)` costs more than `lassign`.

This is the price of safety in Tcl. It weighs little on most procedures, and
step 3 wins it back where it does weigh.

## Step 3 — C, for what matters

```tcl
source ../cproc/cproc.tcl

namespace eval poly {
    type def point    {{double}x; {double}y}
    type def {-compact}polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}

cproc {double}poly::aire {{polygone}pg} {
    double a = 0.0;
    point q = pg.at[pg.n - 1];

    for (Tcl_Size i = 0; i < pg.n; i++) {
        a = a + q.x * pg.at[i].y - q.y * pg.at[i].x;
        q = pg.at[i];
    }
    return fabs(a) / 2;
}
```

`cproc` has the syntax of `proc`. **The signature is that of step 2, word for
word**; only the body changes language. On the C side:

| Tcl | C |
|---|---|
| `{point}P` | `point P`: the struct, with its fields `P.x`, `P.y` |
| `{polygone}pg` | `pg.at[i]`, `pg.n`: an array of `point` and its length |
| `{double}dx`, `{flag}-ouvert` | `double dx`, `int ouvert` |
| `{boite}` as the result | `return (boite){…}` |
| `{polygone}` as the result | `return cproc_list_point(array, n)` |

The struct `point` comes from the type `point` (`type {c!}export`). Everything
that usually makes up a C extension is generated: converting the arguments,
building the result, creating the commands, initialising the stubs.

**`{-compact}`** stores a polygon **in cells**, like a C array of `point`. The C
code then reads it in place, without copying or converting. This is the only
difference between the types of steps 2 and 3.

`poly::point` stays in Tcl: it is too simple to gain anything in C.

**Compilation** happens at the first call, with gcc by default. For tcc, use
`cproc::config -cc tcc` or the environment variable `CPROC_CC=tcc`. The library
produced is cached and recompiled only when the C changes. A compiler error
points to the `.tcl` file and to the line in the body.

The **error messages are those of step 2**, since binding stays in Tcl:
`essais.tcl 3` gives exactly the output of `essais.tcl 2`, with gcc as with tcc.

## The three steps side by side

`tclsh vitesse.tcl tcc` (µs per call, 1,000-vertex polygon, Linux x86-64):

| | step 1: Tcl | step 2: typed Tcl | step 3: gcc | step 3: tcc |
|---|---:|---:|---:|---:|
| `point 1.0 2.0` | 0.21 | 0.60 | 0.59 | 0.59 |
| `aire`, plain list | 197 | 356 | 50 | 54 |
| `aire`, typed variable | 198 | 281 | **1.85** | 4.7 |
| `perimetre` | 265 | 363 | **6.2** | 9.1 |
| `contient` | 149 | 241 | **2.2** | 8.9 |
| `boite` | 377 | 679 | **10.3** | 11.0 |
| `deplace` | 221 | 295 | 203 | 216 |

- **The computations** (`aire`, `perimetre`, `contient`, `boite`) run **35 to 100
  times faster than in step 1**, provided the polygon is already typed. tcc is
  somewhat slower than gcc, but it compiles in a few tens of milliseconds.
- **A plain list** passed to step 3 is first checked, then put in cells (50 µs).
  The gain appears when the program keeps its polygons in typed variables: that
  is the reward of step 2.
- **`deplace`** gains almost nothing. The time goes into building 1,000 Tcl
  points for the result (120 µs), then checking them on return (50 µs).
  Returning cells directly from C is the next improvement for `cproc`.
- **`point`**: the call costs more than the computation, at every typed step.

| | step 1 | step 2 | step 3 |
|---|---|---|---|
| lines of code | 78 | 38 | 58 (39 of them C) |
| argument shape checked | no | yes, at the call | yes, at the call |
| options, defaults, usage | by hand | by the signature | by the signature |
| callers | — | unchanged | unchanged |

## The approach

- **Type the data that circulates first** (`point`, `polygone`), and the
  interface procedures. Errors move to the call, with the argument's name.
- **Keep what the program handles often in typed variables.** That is what
  makes moving to C pay off.
- **Move only the loops to C**: the procedures where the computation outweighs
  the call. Copy the signature, write the body in C, and add `{-compact}` to the
  lists that the C code walks through.

## Going further

- **The type language**: `doc/764.md` (an English summary) and
  `ANNOTATIONS.fr.md`, the full reference in French. In the latter, see § 5 for
  types, § 5.8 for namespaces, § 6.4 and 6.5 for typed procedures and their
  options, and § 7 for paths.
- **`cproc`**: `exemples/cproc/LISEZMOI.md`, in French. It has the full type
  mapping, the configuration and the Windows notes.
- **A real example**: `exemples/tcllib_geometry`, tcllib's `math::geometry`
  package taken through the three steps. tcllib's 210 tests pass at each one.
