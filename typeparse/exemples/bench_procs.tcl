# bench_procs.tcl -- procédure typée (v16p) contre la même procédure classique
# qui fait à la main tous les contrôles que le typage fait d'office.
#   usage : tclsh bench_procs.tcl
#
# Résultat type (µs par appel, meilleur de 15 passes alternées) :
#   A  typé, p déjà pt          1,15     B  classique + contrôles    1,17
#   A  typé, p liste nue        1,25     C  classique sans contrôle  0,47
#   B' contrôles + rend un pt   2,27     A  avec étiquette {dy:}1    1,7
# Le typage ne coûte rien de plus que les contrôles écrits à la main ; c'est
# le contrôle lui-même qui coûte (~0,7 µs ici). Rendre un pt « à la main »
# (type {pt}[…]) est plus cher que le cast de retour d'une procédure typée.

type def pt {{double}x; {double}y}

# A : typé
proc {pt}decaleT {
    {pt}p;
    {double}dx ?= 0;
    {double}dy ?= 0
} {( $p(x) + $dx, $p(y) + $dy )}

# A2 : la même, types écrits dans la signature (produits anonymes)
proc {pt}decale2 {
    {{double}x; {double}y}p;
    {{double}x ?= 0; {double}y ?= 0}dp
} {(
    $p(x) + $dp(x), $p(y) + $dp(y)
)}

# B : classique, mêmes contrôles écrits à la main, résultat liste nue
proc decaleC {p {dx 0} {dy 0}} {
    if {[catch {llength $p} n] || $n != 2} {
        return -code error -errorcode {TCL TYPE PT} "argument \"p\": expected pt but got \"$p\""
    }
    lassign $p x y
    foreach v {x y} {
        if {![string is double -strict [set $v]]} {
            return -code error -errorcode {TCL TYPE DOUBLE} "argument \"p\": field $v: expected floating-point number but got \"[set $v]\""
        }
    }
    foreach v {dx dy} {
        if {![string is double -strict [set $v]]} {
            return -code error -errorcode {TCL TYPE DOUBLE} "argument \"$v\": expected floating-point number but got \"[set $v]\""
        }
    }
    set rx [expr {$x + $dx}]
    set ry [expr {$y + $dy}]
    # le contrôle du retour (deux doubles) : expr rend toujours un nombre,
    # mais le typé le fait ; on le refait pour être équitable
    if {![string is double -strict $rx] || ![string is double -strict $ry]} {
        return -code error "return value {pt}: bad"
    }
    list $rx $ry
}

# B' : idem, mais le résultat est rendu typé pt (ce que fait A)
proc decaleCT {p {dx 0} {dy 0}} {
    if {[catch {llength $p} n] || $n != 2} {
        return -code error -errorcode {TCL TYPE PT} "argument \"p\": expected pt but got \"$p\""
    }
    lassign $p x y
    foreach v {x y} {
        if {![string is double -strict [set $v]]} {
            return -code error "argument \"p\": field $v: expected floating-point number but got \"[set $v]\""
        }
    }
    foreach v {dx dy} {
        if {![string is double -strict [set $v]]} {
            return -code error "argument \"$v\": expected floating-point number but got \"[set $v]\""
        }
    }
    type {pt}[list [expr {$x + $dx}] [expr {$y + $dy}]]
}

# C : classique sans aucun contrôle (plancher)
proc decaleN {p {dx 0} {dy 0}} {
    lassign $p x y
    list [expr {$x + $dx}] [expr {$y + $dy}]
}

set {pt}a 1 2
set l {1 2}
proc boucleT {n} { set {pt}a 1 2; for {set i 0} {$i < $n} {incr i} { decaleT $a 1 1 } }
proc boucleC {n} { set a {1 2};   for {set i 0} {$i < $n} {incr i} { decaleC $a 1 1 } }
proc boucleN {n} { set a {1 2};   for {set i 0} {$i < $n} {incr i} { decaleN $a 1 1 } }
set cas {
    "A  typé, p déjà pt"                   {decaleT $a 1 1}
    "A  typé, p liste nue"                 {decaleT $l 1 1}
    "B  classique + contrôles"             {decaleC $l 1 1}
    "B' classique + contrôles, rend un pt" {decaleCT $l 1 1}
    "C  classique sans contrôle"           {decaleN $l 1 1}
    "A  défauts (decaleT \$a)"             {decaleT $a}
    "B  défauts (decaleC \$l)"             {decaleC $l}
    "A  étiquette ({dy:}1)"                {decaleT $a {dy:}1}
    "A2 produits anonymes, listes nues"    {decale2 $l {1 1}}
    "B  équivalent (0 1)"                  {decaleC $l 0 1}
    "A  appel compilé (boucle)"            {boucleT 1000}
    "B  appel compilé (boucle)"            {boucleC 1000}
    "C  appel compilé (boucle)"            {boucleN 1000}
}
foreach {lib sc} $cas { set best($lib) 1e9; uplevel #0 [list time $sc 2000] }
for {set r 0} {$r < 15} {incr r} {
    foreach {lib sc} $cas {
        set k [expr {[string match *boucle* $sc] ? 200 : 200000}]
        set t [lindex [uplevel #0 [list time $sc $k]] 0]
        if {[string match *boucle* $sc]} { set t [expr {$t / 1000.0}] }
        if {$t < $best($lib)} { set best($lib) $t }
    }
}
foreach {lib sc} $cas { puts [format "%-40s %6.3f µs" $lib $best($lib)] }
puts "--- vérif : mêmes résultats, mêmes refus"
puts [list [decale2 {100 200} {50 30}] [decaleT $a 1 1] [type get [decaleT $a 1 1]] [decaleC $l 1 1] [decaleCT $l 1 1] [type get [decaleCT $l 1 1]]]
foreach c {{decaleT abc} {decaleC abc} {decaleT $a x} {decaleC $l x} {decaleT {1 b}} {decaleC {1 b}}} {
    catch $c m; puts "$c -> $m"
}
