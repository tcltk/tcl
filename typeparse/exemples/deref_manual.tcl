# deref_manual.tcl -- every example of MANUAL-deref.md, runnable as a script.
# Each "puts" is followed by the expected output in a comment.

# --- 1. Links: the var type ------------------------------------------------
set A 1; set B 2; set C 3

set {list.var}raw A B
puts $raw                               ;# ::A ::B           (names, not values)
puts $raw(0)                            ;# 1                 (reading follows the link)

set {list.var.int}refs A B C
puts $refs                              ;# ::A ::B ::C
puts $refs(1)                           ;# 2
set B 20
puts $refs(1)                           ;# 20                (the link follows B)
set refs(1) 7                           ;# writes INTO B
puts $B                                 ;# 7
puts $refs                              ;# ::A ::B ::C      (the list never changes)
set B 2

set {int}n 5
set {list.var(typed).int}trefs n
puts $trefs(0)                          ;# 5
puts [catch {set {list.var(typed).int}bad A} msg]; puts $msg
# 1
# variable "::A" must be a typed variable (declared with set {type}name ...)

# --- 2. type deref: a snapshot --------------------------------------------
set snap [type deref $refs]
puts $snap                              ;# 1 2 3
puts [type get $snap]                   ;# list<int>
set B 20
puts $refs(1)                           ;# 20                (live)
puts $snap                              ;# 1 2 3             (frozen)
puts [type deref $refs]                 ;# 1 20 3            (a new snapshot)
set B 2

# --- 3. Where deref goes ----------------------------------------------------
set {dict.var.int}dd x A y B
puts [type deref $dd]                   ;# x 1 y 2

type def link {{var.int}ref; {string}label}
set {link}p ref A label first
set q [type deref $p]
puts "$q(ref) $q(label)"                ;# 1 first
puts [type get $q]                      ;# deref(link)

set {list.list.var.int}mm {A B} {B A}
puts [type deref $mm]                   ;# {1 2} {2 1}

# --- 4. Sums ------------------------------------------------------------------
type def mix {{int}a, {var.int}b, {string}s}
set {mix}s1 {b}A
set t1 [type deref $s1]
puts "$t1 [type get $t1]"              ;# 1 deref(mix).b

set {list.mix}L {b}A {a}7 {b}B
puts [lmap e [type deref $L] {type get $e}]   ;# deref(mix).b deref(mix).a deref(mix).b

# --- 5. Trees -----------------------------------------------------------------
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}
set {tree(var)}T A {{B {}} {C {}}}
set dt [type deref $T]
puts $dt                                ;# 1 {{2 {}} {3 {}}}
puts [type get $dt]                     ;# deref(tree(var))
puts $dt(children.0.value)              ;# 2

# --- 6. Graphs: one level at a time ------------------------------------------
type def node {{string}name; {list.var.node}next}
set {node}na name A next {nb nc}
set {node}nb name B next {nc}
set {node}nc name C next {na}
set g1 [type deref $na]
puts $g1                                ;# A {{B ::nc} {C ::na}}
puts "$g1(next.0.name) $g1(next.1.name)"  ;# B C
set g2 [type deref $g1(next.0)]
puts $g2                                ;# B {{C ::na}}

# --- 7. Errors ----------------------------------------------------------------
unset nc
puts [catch {type deref $na} msg]; puts $msg
# 1
# type deref: at next.1: can't read "::nc": no such variable
puts [catch {type deref abc} msg]; puts $msg
# 1
# type deref needs a typed value

proc demo {} { set local 42; set {list.var.int}r local; return $r }
set r [demo]
puts [catch {type deref $r} msg]; puts $msg
# 1
# type deref: at 0: can't access "frame:1:local": the procedure call that owned this variable has returned
