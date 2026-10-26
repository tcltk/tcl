# usage : tclsh test.tcl [chemin/vers/mat.so|.dll]
# FR : vérifie que Tcl et C voient les mêmes matrices, en cases ou non.
# EN : checks that Tcl and C see the same matrices, in cells or not.
set here [file dirname [file normalize [info script]]]
source [file join $here matrices.tcl]
mat::init {*}$argv

proc check {label cond} {
    set ok [uplevel 1 [list expr $cond]]
    puts [format "%-58s %s" $label [expr {$ok ? "OK" : "ÉCHEC"}]]
}
proc proche {a b} {expr {abs($a - $b) < 1e-9}}
proc memes {A B} {
    foreach la $A lb $B {foreach x $la y $lb {if {![proche $x $y]} {return 0}}}
    return 1
}

puts "--- 1. La disposition : mat_types.h est bien celui des types Tcl"
set f [open [file join $here mat_types.h]]; set h [read $f]; close $f
check "mat_types.h = type {c!}export matrix3" {$h eq [mat::header]}
set c [mat::layout]
check "sizeof(matrix3) : C=[dict get $c sizeof(matrix3)]" \
    {[dict get $c sizeof(matrix3)] == [dict get [tcl::unsupported::compact layout matrix3] size]}
check "une case d'une matrix3 = une ligne de 24 octets" \
    {[dict get [tcl::unsupported::compact info [set {matrix3}M0 {1 2 3} {4 5 6} {7 8 9}]] stride] == 24}

puts "--- 2. Tcl -> C : matrices construites en Tcl, rangées en cases"
set {matrix3}A {2 0 1} {1 3 2} {1 1 2}
set {matrix3}B {1 2 3} {0 1 4} {5 6 0}
check "A est en cases" {[type is {compact?}$A]}
check "trace : C [mat::trace $A] = Tcl [mat::tcl_trace $A]" \
    {[proche [mat::trace $A] [mat::tcl_trace $A]]}
check "déterminant : C [mat::det $A] = Tcl [mat::tcl_det $A]" \
    {[proche [mat::det $A] [mat::tcl_det $A]]}
check "produit : C = Tcl" {[memes [mat::mul $A $B] [mat::tcl_mul $A $B]]}

puts "--- 3. C -> Tcl : matrices fabriquées en C"
set P [mat::mul $A $B]
check "le produit est une matrix3 en cases" \
    {[type get $P] eq "matrix3" && [type is {compact?}$P]}
check "lu en Tcl : P(1.2) = [set P(1.2)]" {[proche $P(1.2) 15.0]}
set I [mat::identite]
check "identité : [list $I]" {$I eq {{1.0 0.0 0.0} {0.0 1.0 0.0} {0.0 0.0 1.0}}}
check "A × I = A" {[memes [mat::mul $A $I] $A]}
set R [mat::rotz [expr {acos(-1) / 6}]]
check "rotation : det = 1" {[proche [mat::det $R] 1.0]}
check "écrit en Tcl, relu en C" {[set P(0.0) 0.5; proche [mat::trace $P] [mat::tcl_trace $P]]}

puts "--- 4. Un lot de matrices : un tableau C de matrix3"
set {lot}L
for {set k 0} {$k < 1000} {incr k} {lappend L [mat::rotz [expr {$k * 0.01}]]}
lappend L $A $B
check "le lot est en cases ([llength $L] matrices de 72 octets)" \
    {[type is {compact?}$L] && [dict get [tcl::unsupported::compact info $L] stride] == 72}
set dc [mat::lot_det $L]
set dt [lmap M $L {mat::tcl_det $M}]
check "lot_det : C = Tcl, matrice par matrice" \
    {[llength $dc] == [llength $dt] && [memes [list $dc] [list $dt]]}
check "lot_det : A et B en fin de lot" \
    {[proche [lindex $dc end-1] [mat::det $A]] && [proche [lindex $dc end] [mat::det $B]]}

puts "--- 5. Hors cases : la même chose par les listes"
set O {{2 0 1} {1 3 2} {1 1 2}}
check "liste ordinaire : det [mat::det $O]" {[proche [mat::det $O] [mat::det $A]]}
type def matrix3o list(3).list(3).double
set {matrix3o}Q {2 0 1} {1 3 2} {1 1 2}
check "matrix3o (sans -compact) : pas en cases, même produit" \
    {![type is {compact?}$Q] && [memes [mat::mul $Q $B] [mat::mul $A $B]]}
check "lot ordinaire" {[memes [list [mat::lot_det [list $O $O]]] [list [list [mat::det $A] [mat::det $A]]]]}

puts "--- 6. Les erreurs"
check "matrice 2×3 : [catch {mat::det {{1 2 3} {4 5 6}}} m] $m" {[string match "expected a 3x3*" $m]}
check "nombre invalide : [catch {mat::det {{1 2 3} {4 x 6} {7 8 9}}} m] $m" {[string match "expected floating*" $m]}
check "ligne trop courte en Tcl : [catch {set {matrix3}Z {1 2 3} {4 5} {7 8 9}} m]" \
    {[string match "*needs exactly 3*" $m]}
