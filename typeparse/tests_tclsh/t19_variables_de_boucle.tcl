# Les variables de boucle (foreach, lmap, dict for, dict map) perdent leur
# type juste avant d'être affectées. Chaque scénario est exécuté au niveau
# global (chemin commande) ET dans une procédure (chemin compilé).
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
proc both {name body want} {
    proc runproc {} $body
    unset -nocomplain ::r
    check "$name (global)" [uplevel #0 $body] $want
    check "$name (proc)"   [runproc] $want
}

# 1. une variable de boucle déjà typée est librement réaffectée
both "variable int puis string" {
    set {list.string}S {a b} {c d}
    set {int}x 5
    foreach x $S {lappend r [type get $x]}
    set r
} {string string}

# 2. même nom réutilisé avec deux types d'éléments
both "réemploi du nom" {
    set {list.string}S {a b} {c d}
    set {list.int}N 1 2
    foreach x $S {lappend r [type get $x]}
    foreach x $N {lappend r [type get $x]}
    set r
} {string string int int}

# 3. lmap
both "lmap" {
    set {int}x 5
    set {list.string}S {a b} {c d}
    lmap x $S {type get $x}
} {string string}

# 4. foreach à plusieurs variables
both "foreach multi" {
    set {int}a 1; set {int}b 2
    set {list.string}S x y z w
    set r {}
    foreach {a b} $S {lappend r [type get $a][type get $b]}
    set r
} {stringstring stringstring}

# 5. dict for / dict map : clés et valeurs sont librement réaffectées
both "dict for" {
    set {int}k 1; set {int}v 2
    set d {a x b y}
    set r {}
    dict for {k v} $d {lappend r $k$v}
    set r
} {ax by}
both "dict map" {
    set {int}k 1; set {int}v 2
    dict map {k v} {a x b y} {string cat $v$k}
} {a xa b yb}

# 6. la variable reste typée par la dernière valeur reçue après la boucle
both "type conservé après la boucle" {
    set {list.int}N 1 2
    foreach x $N {}
    type get $x
} int

# 7. après la boucle, la variable reste libre : sans déclaration sur son nom,
#    elle n'a pas de contrat (v14 : plus d'adoption)
both "libre après" {
    set {list.int}N 1 2
    foreach x $N {}
    catch {set x abc} m
    set m
} abc

# 8. variable via global/upvar
set {int}::gx 7
proc viaglobal {} {
    global gx
    set {list.string}S a b
    foreach gx $S {}
    type get $gx
}
check "variable globale liée" [viaglobal] string
proc viaupvar {} {
    upvar 1 ux ux
    set {list.string}S a b
    foreach ux $S {}
    type get $ux
}
set {int}ux 3
check "variable upvar" [viaupvar] string

# 9. valeurs brutes : la boucle affecte quand même
both "valeurs plates" {
    set {int}x 5
    foreach x {a b} {lappend r $x}
    set r
} {a b}

puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
