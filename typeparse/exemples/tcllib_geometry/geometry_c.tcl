# geometry_c.tcl --
#
#	math::geometry with its computing core in C (geom.c): the reformulated
#	geometry.tcl of this directory, whose heavy commands are REPLACED by
#	the C commands of geom.so / geom.dll (same names, same arguments, same
#	results). The C structs come from the Tcl types (type {c!}export,
#	geom_types.h); a polygon held in cells ({-compact} list of doubles) is
#	read by C in place.
#
#	package require math; source geometry_c.tcl ?LIBRARY?
#
#	FR : geometry.tcl, dont les commandes lourdes sont remplacées par
#	celles de geom.c : l'appel va directement à la fonction C.

namespace eval ::math::geometry::c {
    variable dir [file dirname [file normalize [info script]]]
    # the commands that C replaces / les commandes remplacées par le C
    variable replaced {
        distance areaPolygon lengthOfPolyline bbox pointInsidePolygon
        pointInsidePolygonAlt calculateDistanceToPolyline
        findClosestPointOnPolyline rotate reflect translate
        findLineIntersection lineSegmentsIntersect polylinesIntersect
        polylinesBoundingIntersect
    }
}
source [file join $::math::geometry::c::dir geometry.tcl]
load [expr {$argc > 0 && [string match *geom* [lindex $argv 0]]
            ? [lindex $argv 0]
            : [file join $::math::geometry::c::dir geom[info sharedlibextension]]}] Geom

# The C structs must have the layout of the Tcl types.
if {[::math::geometry::c::layout] ne "point 16 line 32"} {
    error "geom: unexpected struct sizes: [::math::geometry::c::layout]"
}

foreach cmd $::math::geometry::c::replaced {
    rename ::math::geometry::$cmd {}
    rename ::math::geometry::c::$cmd ::math::geometry::$cmd
}
unset cmd
