# comparaison.tcl -- une même chaîne de géométrie (translate, rotate, aire,
# longueur, boîte englobante) en dix variantes, du Tcl nu au tout typé en
# C. Voir COMPARAISON.md : chaque variante illustre un principe.
#
#   tclsh comparaison.tcl ?TCLLIB_MATH_DIR? ?VARIANTE?
#
# TCLLIB_MATH_DIR : le répertoire du geometry.tcl d'origine de tcllib
# (modules/math des sources, ou lib/tcllib*/math d'une installation) ; par
# défaut, trouvé par « package require math::geometry ».
# Sans VARIANTE, chaque variante est mesurée dans son propre processus
# (les déchets d'une variante ne pèsent pas sur la suivante), trois fois,
# et le meilleur temps est gardé.
#
# Il faut geom.so (geom.c) et geomcells.so (geomcells.c) à côté de ce
# fichier pour les variantes en C (voir COMPARAISON.md pour les compiler) ;
# sans eux, ces variantes sont sautées.
#
# EN : one geometry chain in ten variants, from plain Tcl to fully typed
# values in C; each one illustrates a principle (COMPARAISON.md).

set here [file dirname [file normalize [info script]]]
set ext [info sharedlibextension]
set geomlib  [file join $here geom$ext]
set cellslib [file join $here geomcells$ext]

set variantes {
    1  "Tcl nu (tcllib)"                         orig   chT  brute
    2  "Tcl nu + C"                              origc  chT  brute
    3  "typé scintillant, Tcl"                   typed  chT  brute
    4  "typé scintillant, vectorisé"             typed  chV  brute
    5  "typé scintillant, C"                     c      chS  brute
    6  "tout typé, Tcl"                          typed  chT  polyline
    7  "tout typé, vectorisé (objets)"           typed  chV  polyline
    8  "tout typé, vectorisé (cases)"            typed  chV  polylinec
    9  "tout typé, C (cases)"                    c      chC  polylinec
    10 "tout typé, vectorisé + C (cases)"        c      chVC polylinec
}

# --- le répertoire de tcllib ---------------------------------------------
if {$argc > 0 && [lindex $argv 0] ne ""} {
    set orig [file normalize [lindex $argv 0]]
} else {
    package require math::geometry
    regexp {source (\S+)} [package ifneeded math::geometry \
	    [package present math::geometry]] -> f
    set orig [file dirname [string trim $f "{}\""]]
}

# --- sans variante : chacune dans son processus, trois fois --------------
if {$argc < 2} {
    puts [format "%-36s %12s %12s %10s" variante "1 000" "100 000" "÷ Tcl nu"]
    foreach {num nom i ch forme} $variantes {
	if {$i in {origc c} && ![file exists $geomlib]} {
	    puts [format "%-36s (pas de %s)" "$num $nom" [file tail $geomlib]]
	    continue
	}
	if {$i eq "c" && ![file exists $cellslib]} {
	    puts [format "%-36s (pas de %s)" "$num $nom" [file tail $cellslib]]
	    continue
	}
	set best {}
	for {set k 0} {$k < 3} {incr k} {
	    set r [exec [info nameofexecutable] [info script] $orig $num]
	    if {[string match ERR* $r]} { set best $r; break }
	    if {$best eq ""} { set best $r } else {
		set best [lmap a $best b $r {expr {min($a, $b)}}]
	    }
	}
	if {[string match ERR* $best]} {
	    puts [format "%-36s %s" "$num $nom" $best]
	    continue
	}
	lassign $best t1 t2
	if {$num == 1} { set nu $t2 }
	puts [format "%-36s %9.0f µs %9.0f µs %9s" "$num $nom" $t1 $t2 \
		[format ×%.1f [expr {double($nu) / $t2}]]]
    }
    return
}

# --- une variante --------------------------------------------------------
set seule [lindex $argv 1]
proc polygon {n r} {
    set l {}
    for {set i 0} {$i < $n} {incr i} {
	set a [expr {2 * 3.141592653589793 * $i / $n}]
	lappend l [expr {$r * cos($a)}] [expr {$r * sin($a)}]
    }
    return $l
}
set remplacees {areaPolygon lengthOfPolyline bbox rotate translate}

# tcllib d'origine (1, et 2 avec ses commandes lourdes remplacées par le C)
foreach i {orig origc} {
    interp create $i
    $i eval [list set auto_path [linsert $auto_path 0 [file dirname $orig]]]
    $i eval {package require math}
    $i eval [list source [file join $orig geometry.tcl]]
}
# la reformulation typée, avec ses procédures vectorisées (3 à 10)
foreach i {typed c} {
    interp create $i
    $i eval {namespace eval ::math::geometry {variable coord double}}
    $i eval [list set argv double]
    $i eval {set argc 1; set argv0 comparaison}
    $i eval [list set auto_path [linsert $auto_path 0 [file dirname $orig]]]
    $i eval [list source [file join $here vectorise.tcl]]
}
foreach {i lib} [list origc $geomlib c $geomlib c $cellslib] {
    if {[file exists $lib]} {
	$i eval [list load $lib [expr {[string match *cells* $lib]
		? "Geomcells" : "Geom"}]]
    }
}
foreach i {origc c} {
    $i eval [list set remplacees $remplacees]
    $i eval {
	if {[namespace which ::math::geometry::c::areaPolygon] ne ""} {
	    foreach cmd $remplacees {
		rename ::math::geometry::$cmd {}
		rename ::math::geometry::c::$cmd ::math::geometry::$cmd
	    }
	}
    }
}

set chaines {
    # Tcl (tcllib ou reformulé), ou C quand le C a remplacé les commandes
    proc chT {p} {
	set q [translate {3.5 -2} $p];  set r [rotate 30 $q]
	list [areaPolygon $r] [lengthOfPolyline $r] [bbox $r]
    }
    # vectorisé (type map, tranches, fenêtres)
    proc chV {p} {
	set q [translateV {3.5 -2} $p];  set r [rotateV 30 $q]
	list [areaPolygonV $r] [lengthOfPolylineV $r] [bboxV $r]
    }
    # C de cases en cases
    proc chC {p} {
	set q [translateCells {3.5 -2} $p];  set r [rotateCells 30 $q]
	list [areaPolygon $r] [lengthOfPolyline $r] [bbox $r]
    }
    # vectorisé pour les transformations, C pour les mesures
    proc chVC {p} {
	set q [translateV {3.5 -2} $p];  set r [rotateV 30 $q]
	list [areaPolygon $r] [lengthOfPolyline $r] [bbox $r]
    }
    proc chS {p} {
	set q [translateS {3.5 -2} $p];  set r [rotateS 30 $q]
	list [areaS $r] [lengthS $r] [bboxS $r]
    }
    proc best {k script} {
	set b 1e18
	for {set j 0} {$j < $k} {incr j} {
	    set t [lindex [time {uplevel #0 $script} 1] 0]
	    if {$t < $b} {set b $t}
	}
	return $b
    }
}
foreach i {orig origc typed c} {
    $i eval [list namespace eval ::math::geometry $chaines]
    $i eval {namespace path ::math::geometry}
}
# le C derrière des signatures typées en cases : une liste ordinaire est
# convertie à l'entrée (5)
c eval {
    namespace eval ::math::geometry {
	proc translateS {{vector}t; {polylinec}pl} { translateCells $t $pl }
	proc rotateS {{coord}a; {polylinec}pl} { rotateCells $a $pl }
	proc areaS {{polygonc}pg} { areaPolygon $pg }
	proc lengthS {{polylinec}pl} { lengthOfPolyline $pl }
	proc bboxS {{polylinec}pl} { bbox $pl }
    }
}

foreach {num nom i ch forme} $variantes {
    if {$num ne $seule} continue
    set out {}
    foreach n {1000 100000} k {200 7} {
	set P [polygon $n 100.0]
	$i eval [list set ::P $P]
	if {$forme ne "brute"} {
	    $i eval [list set ::P [list type cast ::math::geometry::$forme $P]]
	    $i eval {set ::P [{*}$::P]}
	}
	# les résultats, comparés à tcllib (aire et longueur)
	if {[catch {$i eval "::math::geometry::$ch \$::P"} r]} {
	    puts "ERR $r"; exit
	}
	set ref [orig eval [list ::math::geometry::chT $P]]
	foreach a [lrange $r 0 1] b [lrange $ref 0 1] {
	    if {abs($a - $b) > 1e-9 * abs($b)} { puts "ERR écart $a $b"; exit }
	}
	lappend out [$i eval [list ::math::geometry::best $k \
		"::math::geometry::$ch \$::P"]]
    }
    puts $out
}
