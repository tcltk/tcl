# e2e.tcl -- une chaîne de bout en bout : translation, rotation, puis aire,
# longueur et boîte englobante, sur un polygone typé UNE fois au départ.
#   tclsh e2e.tcl            (coordonnées double)
namespace eval ::math::geometry {variable coord double}
set argv {}; set argc 0
source [file join [file dirname [file normalize [info script]]] vectorise.tcl]

namespace path ::math::geometry
proc chaine {p V} {
    if {$V} {
	set q [translateV {3.5 -2} $p];  set r [rotateV 30 $q]
	list [areaPolygonV $r] [lengthOfPolylineV $r] [bboxV $r]
    } else {
	set q [translate {3.5 -2} $p];   set r [rotate 30 $q]
	list [areaPolygon $r] [lengthOfPolyline $r] [bbox $r]
    }
}
puts "\nchaîne : translate, rotate, area, length, bbox"
puts [format "%-36s %10s %10s" variante 1000 100000]
foreach {nom forme V} {
    "geometry.tcl, liste ordinaire"     brut       0
    "geometry.tcl, polyline typée"      polyline   0
    "type map, polyline (objets)"       polyline   1
    "type map, polylinec (en cases)"    polylinec  1
} {
    set ligne [format "%-36s" $nom]
    foreach n {1000 100000} {
	set P [polygon $n 100.0]
	if {$forme ne "brut"} {set P [type cast $forme $P]}
	set r [chaine $P $V]
	if {![info exists ref($n)]} {set ref($n) $r} elseif {![pareil [lrange $r 0 1] [lrange $ref($n) 0 1]]} {
	    append ligne " (ÉCART)"
	}
	append ligne [format " %8.0f µs" [best [list chaine $P $V]]]
    }
    puts $ligne
}
