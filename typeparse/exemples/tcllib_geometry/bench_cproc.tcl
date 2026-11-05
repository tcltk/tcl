# bench_cproc.tcl -- les commandes lourdes de la géométrie : Tcl typé
# (geometry.tcl), C avec colle écrite à la main (geometry_c.tcl + geom.c),
# cproc compilé par gcc, cproc compilé par tcc. Temps par appel (µs, le
# meilleur de 10 tours).
#
#   tclsh bench_cproc.tcl TCLLIB_MATH_DIR ?GEOM_LIBRARY? ?TCC?
#
# TCLLIB_MATH_DIR : le répertoire de tcllib qui contient math.tcl
# (modules/math des sources). GEOM_LIBRARY : geom.so/geom.dll (sinon
# ./geom[info sharedlibextension] ; colonne absente si introuvable).
# TCC : le chemin de tcc (sinon "tcc" ; colonne absente s'il échoue).

set here [file dirname [file normalize [info script]]]
lassign $argv mathdir geomlib tcc
if {$geomlib eq ""} {set geomlib [file join $here geom[info sharedlibextension]]}
if {$tcc eq ""} {set tcc tcc}

set variants {}
foreach {name file cc} [list typed geometry.tcl {} c geometry_c.tcl {} \
	cproc_gcc geometry_cproc.tcl gcc cproc_tcc geometry_cproc.tcl $tcc] {
    if {$name eq "c" && ![file exists $geomlib]} continue
    interp create $name
    $name eval [list set auto_path [linsert $::auto_path 0 [file dirname $mathdir]]]
    $name eval {package require math}
    $name eval [list set argv [list $geomlib]]
    $name eval [list set argc 1]
    if {$cc ne ""} {$name eval [list set ::env(CPROC_CC) $cc]}
    if {[catch {$name eval [list source [file join $here $file]]} err]} {
	puts stderr "$name : [lindex [split $err \n] 0]"
	interp delete $name
	continue
    }
    lappend variants $name
}
catch {unset ::env(CPROC_CC)}

proc polygon {n r} {
    set l {}
    for {set i 0} {$i < $n} {incr i} {
	set a [expr {2 * 3.141592653589793 * $i / $n}]
	lappend l [expr {$r * cos($a)}] [expr {$r * sin($a)}]
    }
    return $l
}
set P1000 [polygon 1000 100]
foreach v $variants {
    $v eval [list set P1000 $P1000]
    $v eval {
	namespace path ::math::geometry
	set {point}A 0 0;  set {point}B 3 4;  set {point}Q 10 10
	# des doubles écrits comme tels : le C calcule en double ; expr, sur
	# des entiers écrits 0 et 10, calculerait en entiers (5 et non 5.0)
	set {line}L1 0.0 0.0 10.0 10.0;  set {line}L2 0.0 10.0 10.0 0.0
	set {polygon}TP {*}$P1000
	# en cases : la forme compacte (interrupteur global ; nombres
	# canoniques), que le C lit en place
	::tcl::unsupported::compact on
	set {polygon}CP {*}$P1000
	::tcl::unsupported::compact off
	proc measure {cmd n} {uplevel #0 [list time $cmd $n]}
    }
}

set cases {
    "distance (points typés)"        {::math::geometry::distance $A $B}         20000
    "findLineIntersection"           {::math::geometry::findLineIntersection $L1 $L2} 20000
    "lineSegmentsIntersect"          {::math::geometry::lineSegmentsIntersect $L1 $L2} 20000
    "areaPolygon 1000 (liste)"       {::math::geometry::areaPolygon $P1000}     200
    "areaPolygon 1000 (typé)"        {::math::geometry::areaPolygon $TP}        200
    "areaPolygon 1000 (cases)"       {::math::geometry::areaPolygon $CP}        2000
    "lengthOfPolyline 1000 (cases)"  {::math::geometry::lengthOfPolyline $CP}   2000
    "pointInsidePolygonAlt (cases)"  {::math::geometry::pointInsidePolygonAlt $Q $CP} 2000
    "calculateDistanceToPolyline"    {::math::geometry::calculateDistanceToPolyline $Q $CP} 500
    "rotate 30 (1000 sommets)"       {::math::geometry::rotate 30 $CP}         200
}

proc best {v cmd n} {
    set b 1e9
    for {set r 0} {$r < 10} {incr r} {
	set t [lindex [$v eval [list measure $cmd $n]] 0]
	if {$t < $b} {set b $t}
    }
    return $b
}
puts [format "%-32s %s" "" [join [lmap v $variants {format %10s $v}] ""]]
foreach {label cmd n} $cases {
    set line [format "%-32s" $label]
    set ref [[lindex $variants 0] eval $cmd]
    set same 1
    foreach v $variants {
	if {[$v eval $cmd] ne $ref} {set same 0}
	append line [format %10.2f [best $v $cmd $n]]
    }
    puts "$line[expr {$same ? {} : {   (résultats différents)}}]"
}
