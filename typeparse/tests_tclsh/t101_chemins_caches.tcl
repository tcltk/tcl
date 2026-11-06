# t101 -- chemins compilés gardés (v16v) : le même index lu plusieurs fois, sur
# des valeurs de types différents, après une erreur, substitué, écrit, et
# l'objet index réutilisé ailleurs.

proc t {label script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-34s %s" $label $r]
}

type def pt   {{double}x; {double}y}
type def seg  {{pt}a; {pt}b}
type def tri  {{pt}a; {pt}b; {pt}c}
type def poly list.pt
type def boite {{seg}diag; {string}nom}

proc lectures {} {
    set {seg}s {1.0 2.0} {3.0 4.0}
    set {tri}r {0.0 0.0} {5.0 0.0} {0.0 5.0}
    set {poly}g {1.0 1.0} {2.0 2.0} {3.0 3.0}
    set {boite}b {{0.0 0.0} {9.0 9.0}} carton
    set out {}
    foreach v {s r s} {
	# le même littéral "a.x", deux types de produits
	upvar 0 $v w
	for {set i 0} {$i < 2} {incr i} {lappend out $w(a.x) $w(b.y)}
    }
    upvar 0 s S r R
    for {set i 0} {$i < 2} {incr i} {
	lappend out $S(a.x) $R(b.x) $g(1.y) $g(end.x) $b(diag.b.y) $b(nom)
	lappend out $S(a,b) $S(b.(x,y)) $g(0:1) [type get $S(a.x)] [type get $S(a)]
    }
    return $out
}
t "lectures répétées"       {lectures}
t "lectures, 2e appel"      {lectures}

proc erreurs {} {
    set {seg}s {1.0 2.0} {3.0 4.0}
    set out {}
    for {set i 0} {$i < 2} {incr i} {
	catch {set s(a.z)} m; lappend out $m
	catch {set s(c.x)} m; lappend out $m
	catch {set s(a.x.y)} m; lappend out $m
	catch {set s(a..x)} m; lappend out $m
    }
    return $out
}
t "erreurs, deux fois"      {erreurs}

proc substitue {} {
    set {seg}s {1.0 2.0} {3.0 4.0}
    set out {}
    foreach k {a b a b} f {x y y x} {
	lappend out $s($k.$f)
    }
    # une valeur substituée est un seul nom : "a.x" n'est pas un chemin
    set p a.x
    catch {set s($p)} m
    lappend out $m
    set p a
    lappend out $s($p.x)
    return $out
}
t "index substitués"        {substitue}

proc ecrit {} {
    set {seg}s {1.0 2.0} {3.0 4.0}
    for {set i 0} {$i < 3} {incr i} {
	set s(a.x) [expr {$s(a.x) + 1}]
	set s(b.(x,y)) $i $i
    }
    list $s(a.x) $s(b) [type get $s]
}
t "écritures répétées"      {ecrit}

proc construit {} {
    set out {}
    for {set i 0} {$i < 2} {incr i} {
	set {seg}s(a.x,a.y,b.x,b.y) 1.0 2.0 3.0 $i.0
	lappend out $s
    }
    return $out
}
t "construction par chemins" {construit}

proc reutilise {} {
    # l'objet index "a.x" sert aussi de chaîne et de liste ordinaires
    set {seg}s {1.0 2.0} {3.0 4.0}
    set out [list $s(a.x)]
    lappend out [llength a.x] [string length a.x] [split a.x .]
    lappend out $s(a.x)
    set d [dict create a.x 7]
    lappend out [dict get $d a.x] $s(a.x)
}
t "index réutilisé ailleurs" {reutilise}

proc lass {} {
    set {poly}g {1.0 2.0} {3.0 4.0}
    set out {}
    for {set i 0} {$i < 2} {incr i} {
	foreach p $g {
	    lassign $p x y
	    lappend out $x $y [type get $x] [lindex $p 1] [llength $p]
	    lappend out [lrange $p 2 end] [lrange $p 0 0]
	}
    }
    lappend out $g [type get $g]
}
t "lassign sur des points"  {lass}
