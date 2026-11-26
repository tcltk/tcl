# vectorise.tcl -- quelques commandes de geometry.tcl réécrites avec les
# commandes qui connaissent le type (TIP 764, v17m) : tranches à pas pour
# séparer les x et les y d'une polyligne (x1 y1 x2 y2 ...), type map pour le
# calcul sur chaque sommet, écriture d'une tranche pour réentrelacer ; v17n :
# fenêtres (x[i], y[i], x[i+1], y[i+1]) = $pl((0,1,2,3)::2), et (x, y) en
# deux colonnes = $pl((0,1)::2), en lecture comme en écriture.
#
#   tclsh vectorise.tcl ?num|double?     (dans exemples/tcllib_geometry)
#
# Chaque version est comparée à celle de geometry.tcl (même résultat), puis
# chronométrée sur des polygones de 1 000 et 100 000 sommets.

set here [file dirname [file normalize [info script]]]
# coordonnées : num (int | double, comme tcllib) ou double (argument)
if {$argc > 0} {
    namespace eval ::math::geometry [list variable coord [lindex $argv 0]]
}
if {[catch {package require math}]} {
    package provide math 1.0

}
source [file join $here geometry.tcl]

namespace eval ::math::geometry {}

# Aire (formule du lacet) : les termes x[i]*y[i+1] - x[i+1]*y[i] lus par
# une fenêtre (x[i], y[i], x[i+1], y[i+1]) qui avance de 2, plus le terme
# qui ferme
proc ::math::geometry::areaPolygonV {{polygon}pg} {
    if {[llength $pg] < 6} {return [areaPolygon $pg]}
    set a [type map -sum {x0 y0 x1 y1} $pg((0,1,2,3)::2) \
	    {($x0*$y1 - $x1*$y0)}]
    set n [llength $pg]
    set c [expr {[lindex $pg end-1]*[lindex $pg 1] - [lindex $pg 0]*[lindex $pg end]}]
    expr {0.5*abs(0.0 + $a + $c)}
}

# Longueur : la somme des hypot entre sommets consécutifs
proc ::math::geometry::lengthOfPolylineV {{polyline}pl} {
    if {[llength $pl] < 4} {return [lengthOfPolyline $pl]}
    type map -sum {x0 y0 x1 y1} $pl((0,1,2,3)::2) \
	    {(hypot($x0-$x1, $y0-$y1))}
}

# Translation et rotation : x et y lus par la fenêtre (0,1)::2, un seul
# type map typé qui rend les deux colonnes (le tuple du corps), puis
# réentrelacées par une seule écriture de fenêtre
proc ::math::geometry::translateV {{vector}t; {polyline}pl} {
    set tx $t(x);  set ty $t(y)
    set {polyline}out [Vide $pl]
    set out((0,1)::2) {*}[type map {{coord}x; {coord}y} $pl((0,1)::2) \
	    {{coord}u; {coord}v}{($x + $tx, $y + $ty)}]
    return $out
}

# une polyligne de même longueur et de même forme que PL (en cases si PL
# l'est : le résultat reste dans la forme de l'entrée)
proc ::math::geometry::Vide {pl} {
    if {[namespace tail [type get $pl]] eq "polylinec"} {
	# des zéros en cases, faits par type map, puis la forme en cases
	return [type repeat -into polylinec [llength $pl] 0.0]
    }
    set {polyline}z {*}[lrepeat [llength $pl] 0.0]
    return $z
}

proc ::math::geometry::rotateV {{coord}angle; {polyline}pl} {
    set a [expr {$::math::geometry::torad * $angle}]
    set c [expr {cos($a)}];  set s [expr {sin($a)}]
    set {polyline}out [Vide $pl]
    set out((0,1)::2) {*}[type map {{coord}x; {coord}y} $pl((0,1)::2) \
	    {{coord}u; {coord}v}{($c * $x - $s * $y, $s * $x + $c * $y)}]
    return $out
}

# Boîte englobante : min et max des deux tranches (type map -min / -max)
proc ::math::geometry::bboxV {{polyline}pl} {
    lassign $pl((0,1)::2) X Y
    if {[llength $X] == 0} {return [lrepeat 4 {}]}
    list [type map -min {x} $X {($x)}] [type map -min {y} $Y {($y)}] \
	 [type map -max {x} $X {($x)}] [type map -max {y} $Y {($y)}]
}

proc polygon {n r} {
    set l {}
    for {set i 0} {$i < $n} {incr i} {
	set a [expr {2 * 3.141592653589793 * $i / $n}]
	lappend l [expr {$r * cos($a)}] [expr {$r * sin($a)}]
    }
    return $l
}

# même résultat, au dernier chiffre près pour les sommes (l'ordre des
# additions peut différer d'un arrondi)
proc pareil {a b} {
    if {[llength $a] != [llength $b]} {return 0}
    foreach x $a y $b {
	if {abs($x - $y) > 1e-9 * (1.0 + abs($y))} {return 0}
    }
    return 1
}

proc best {cmd} {
    set b 1e18
    for {set k 0} {$k < 5} {incr k} {
	set t [lindex [time {uplevel #0 $cmd}] 0]
	if {$t < $b} {set b $t}
    }
    return $b
}

if {[info script] ne $::argv0} return	;# lu par e2e.tcl : les définitions seules

namespace path ::math::geometry
puts [format "%-16s %8s %12s %12s %9s" commande sommets "Tcl typé" "type map" gain]
foreach n {1000 100000} {
    set ::P [polygon $n 100.0]
    foreach {nom ancien nouveau} {
	areaPolygon       {areaPolygon $::P}        {areaPolygonV $::P}
	lengthOfPolyline  {lengthOfPolyline $::P}   {lengthOfPolylineV $::P}
	translate         {translate {3.5 -2} $::P} {translateV {3.5 -2} $::P}
	rotate            {rotate 30 $::P}          {rotateV 30 $::P}
	bbox              {bbox $::P}               {bboxV $::P}
    } {
	set r1 [uplevel #0 $ancien];  set r2 [uplevel #0 $nouveau]
	if {![pareil $r1 $r2]} {puts "ÉCART $nom : [lrange $r1 0 3] / [lrange $r2 0 3]"}
	set t1 [best $ancien];  set t2 [best $nouveau]
	puts [format "%-16s %8d %10.0f µs %10.0f µs %8.1f×" $nom $n $t1 $t2 [expr {double($t1)/$t2}]]
    }
}
