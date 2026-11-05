# bench.tcl -- tcllib math::geometry (original) against its reformulation
# (geometry.tcl of this directory) and against the reformulation with its
# heavy commands in C (geometry_c.tcl + geom.c), in the same tclsh: same
# calls, results compared, time per call (µs, best of 10 rounds, rounds
# alternated).
#
#   tclsh bench.tcl ?TCLLIB_MATH_DIR? ?GEOM_LIBRARY?
#
# Columns: tcllib; typed Tcl; C; then typed Tcl and C with arguments already
# typed by the caller (for C, the polygons in cells: {-compact}).
#
# TCLLIB_MATH_DIR: the directory of tcllib's geometry.tcl (modules/math of
# the sources, or lib/tcllib*/math of an installation); by default, found
# through "package require math::geometry".

set here [file dirname [file normalize [info script]]]
set geomlib [file join $here geom[info sharedlibextension]]
if {$argc > 1} {set geomlib [file normalize [lindex $argv 1]]}
if {$argc > 0} {
    set orig [file normalize [lindex $argv 0]]
} else {
    package require math::geometry
    regexp {source (\S+)} [package ifneeded math::geometry [package present math::geometry]] -> f
    set orig [file dirname [string trim $f "{}\""]]
}

# Three interpreters: tcllib, the reformulation, the reformulation with C.
foreach {name file} [list orig [file join $orig geometry.tcl] \
        typed [file join $here geometry.tcl] c [file join $here geometry_c.tcl]] {
    interp create $name
    $name eval [list set auto_path [linsert $::auto_path 0 [file dirname $orig]]]
    $name eval {package require math}
    $name eval [list set argv [list $geomlib]]
    $name eval [list set argc 1]
    $name eval [list source $file]
}

# A regular polygon of n vertices (flat list), radius r, centre (cx, cy).
proc polygon {n r {cx 0} {cy 0}} {
    set l {}
    for {set i 0} {$i < $n} {incr i} {
        set a [expr {2 * 3.141592653589793 * $i / $n}]
        lappend l [expr {$cx + $r * cos($a)}] [expr {$cy + $r * sin($a)}]
    }
    return $l
}
set P1000 [polygon 1000 100]
set P200a [polygon 200 50]
set P200b [polygon 200 50 120 0]
set Sq    {0 0 10 0 10 10 0 10}

# The same arguments, already typed (made once by a typed caller); for C,
# the polygons are held in cells ({-compact}), which C reads in place.
foreach i {typed c} {
$i eval [list set P1000 $P1000]
$i eval [list set P200a $P200a]
$i eval [list set P200b $P200b]
}
c eval {type def {-compact}polygon_c list.double}
foreach {i T} {typed polygon c polygon_c} {
$i eval [string map [list %T $T] {
    namespace path ::math::geometry	;# the types point, line... (v16u)
    set {point}A 0 0;  set {point}B 3 4;  set {point}C 5 10;  set {point}Q 10 10
    set {point}P12 1 2;  set {point}P34 3 4;  set {point}C55 5 5
    set {line}L1 0 0 10 10;  set {line}L2 0 10 10 0
    set {%T}TP1000 {*}$P1000
    set {%T}TP200a {*}$P200a;  set {%T}TP200b {*}$P200b
    set {%T}TSq 0 0 10 0 10 10 0 10
}]
}

# label, call with plain lists (both), call with typed values (typed only)
set cases {
    "p 1 2"                                 {p 1 2}                   {}
    "+ {1 2} {3 4}"                         {+ {1 2} {3 4}}           {+ $P12 $P34}
    "distance {0 0} {3 4}"                  {distance {0 0} {3 4}}    {distance $A $B}
    "direction 30"                          {direction 30}            {}
    "between {0 0} {10 10} 0.25"            {between {0 0} {10 10} 0.25} {}
    "octant {-1 -2}"                        {octant {-1 -2}}          {}
    "angle {10 10 15 13}"                   {angle {10 10 15 13}}     {}
    "movePointInDirection"                  {movePointInDirection {0 0} 45.0 10} {}
    "calculateDistanceToLine"               {calculateDistanceToLine {5 10} {0 0 10 10}} {calculateDistanceToLine $C $L1}
    "findClosestPointOnLine"                {findClosestPointOnLine {5 10} {0 0 10 10}}  {findClosestPointOnLine $C $L1}
    "calculateDistanceToLineSegment"        {calculateDistanceToLineSegment {-10 0} {0 0 10 10}} {}
    "findClosestPointOnLineSegment"         {findClosestPointOnLineSegment {5 10} {0 0 10 10}} {}
    "findLineIntersection"                  {findLineIntersection {0 0 10 10} {0 10 10 0}} {findLineIntersection $L1 $L2}
    "lineSegmentsIntersect"                 {lineSegmentsIntersect {0 0 10 10} {0 10 10 0}} {lineSegmentsIntersect $L1 $L2}
    "findLineSegmentIntersection"           {findLineSegmentIntersection {0 0 10 10} {0 10 10 0}} {findLineSegmentIntersection $L1 $L2}
    "intervalsOverlap 2 4 4 6 0"            {intervalsOverlap 2 4 4 6 0} {}
    "rectanglesOverlap"                     {rectanglesOverlap {0 10} {10 0} {10 10} {20 0} 0} {}
    "inproduct {1 2 3} {4 5 6}"             {inproduct {1 2 3} {4 5 6}} {}
    "areaPolygon (square)"                  {areaPolygon $::Sq}       {areaPolygon $TSq}
    "pointInsidePolygon (square)"           {pointInsidePolygon {5 5} $::Sq} {pointInsidePolygon $C55 $TSq}
    "rectangleInsidePolygon (square)"       {rectangleInsidePolygon {2 2} {8 8} $::Sq} {}
    "--- 1000 vertices"                     {} {}
    "areaPolygon"                           {areaPolygon $::P1000}    {areaPolygon $TP1000}
    "lengthOfPolyline"                      {lengthOfPolyline $::P1000} {lengthOfPolyline $TP1000}
    "bbox"                                  {bbox $::P1000}           {bbox $TP1000}
    "pointInsidePolygon"                    {pointInsidePolygon {10 10} $::P1000} {pointInsidePolygon $Q $TP1000}
    "pointInsidePolygonAlt"                 {pointInsidePolygonAlt {10 10} $::P1000} {pointInsidePolygonAlt $Q $TP1000}
    "calculateDistanceToPolyline"           {calculateDistanceToPolyline {10 10} $::P1000} {calculateDistanceToPolyline $Q $TP1000}
    "findClosestPointOnPolyline"            {findClosestPointOnPolyline {10 10} $::P1000} {findClosestPointOnPolyline $Q $TP1000}
    "rotate 30"                             {rotate 30 $::P1000}      {rotate 30 $TP1000}
    "translate {1 2}"                       {translate {1 2} $::P1000} {translate $P12 $TP1000}
    "--- 2 x 200 vertices"                  {} {}
    "polylinesIntersect (disjoint)"         {polylinesIntersect $::P200a $::P200b} {polylinesIntersect $TP200a $TP200b}
}

# Timed inside a procedure of each interpreter (compiled call, as in a
# program): best of 10 rounds, alternated.
foreach i {orig typed c} {
    $i eval {proc measure {cmd n} {uplevel #0 [list time $cmd $n]}}
}
proc best {interp cmd n} {
    set b 1e9
    for {set r 0} {$r < 10} {incr r} {
        set t [lindex [$interp eval [list measure $cmd $n]] 0]
        if {$t < $b} {set b $t}
    }
    return $b
}

puts [format "%-32s %8s %8s %5s %8s %5s %8s %5s %8s %5s  %s" "" tcllib typed / C / \
        "typed*" / "C*" / "same result"]
foreach {label script tscript} $cases {
    if {$script eq ""} {puts $label; continue}
    set cmd [uplevel #0 "list $script"]
    set cmd [lreplace $cmd 0 0 ::math::geometry::[lindex $cmd 0]]
    set ro [orig eval $cmd]
    set same "yes"
    foreach i {typed c} {
        if {[$i eval $cmd] ne $ro} {append same " NO($i: [$i eval $cmd])"}
    }
    set t0 [lindex [orig eval [list time $cmd 1]] 0]
    set n [expr {max(3, min(20000, int(200000.0 / max(1, $t0))))}]
    set bo [best orig $cmd $n]
    set line [format "%-32s %8.2f" $label $bo]
    foreach i {typed c} {
        set b [best $i $cmd $n]
        append line [format " %8.2f %5.2f" $b [expr {$b / $bo}]]
    }
    if {$tscript ne ""} {
        set tcmd "::math::geometry::$tscript"
        foreach i {typed c} {
            set r [$i eval $tcmd]
            if {[lsort -real [concat $r]] ne [lsort -real [concat $ro]]
                    && [string map {.0 {}} $r] ne [string map {.0 {}} $ro]} {
                append same " NO($i*: $r)"
            }
            set b [best $i $tcmd $n]
            append line [format " %8.2f %5.2f" $b [expr {$b / $bo}]]
        }
    } else {
        append line [format " %8s %5s %8s %5s" "" "" "" ""]
    }
    puts "$line  [string range $same 0 60]"
}
