# geometry.tcl --
#
#	tcllib math::geometry 1.4.2 (geometry.tcl), reformulated with the
#	TIP 764 prototype: typed procedure signatures (v16p) and expression
#	bodies (TIP 759). Same namespace, same commands, same arguments and
#	same results: tcllib's own geometry.test passes on it. The two other
#	files of the package (geometry_circle.tcl, geometry_ext.tcl) are
#	sourced unchanged and call these commands.
#
#	Reformulé avec le prototype TIP 764 : signatures typées, corps en
#	expressions. Mêmes commandes, mêmes résultats (geometry.test passe).
#
# Copyright (c) 2001 by Ideogramic ApS and other parties.
# Copyright (c) 2004 Arjen Markus
# Copyright (c) 2010 Andreas Kupries
# Copyright (c) 2010 Kevin Kenny
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

namespace eval ::math::geometry {}

package require Tcl 9
package require math

# Results: proc {T}name checks the result as a T. A product (a point) is a
# container: a caller that uses lists sees its fields (lassign $P x y).

namespace eval ::math::geometry {
    # The types belong to the namespace (v16u): ::math::geometry::point, and
    # so on; inside it, and for a caller with "namespace path ::math::geometry",
    # they are simply point, line...
    #
    # A point, a vector: two coordinates. A line, a line segment, a rectangle
    # (NW and SE corners): four. A polyline, a polygon: a flat list of
    # coordinates, x1 y1 x2 y2 ... (tcllib's conventions: callers pass plain
    # lists).
    # A coordinate: a number as Tcl reads it (v17b), an integer staying an
    # integer and dividing as one, as in tcllib -- {int}i | {double}d is a
    # union, disjoint on canonical forms (16 / 2.0). The C variants
    # (geometry_c.tcl, geometry_cproc.tcl) set coord to double before
    # sourcing this file: their structs hold doubles.
    variable coord
    if {![info exists coord]} {set coord num}
    # num est intégré (v17z) : num.int, num.double
    type alias coord $coord
    type def point    {{coord}x; {coord}y}
    type alias vector point
    type def line     {{coord}x1; {coord}y1; {coord}x2; {coord}y2}
    type alias segment line
    type alias rect    line
    if {$coord eq "double"} {
	# v17m : la même polyligne, en cases (des doubles contigus) : une
	# forme du même type, pour le C et les commandes de masse
	type def polyline list.coord {-compact}polylinec
	type alias polygonc polylinec
    } else {
	type def polyline list.coord
    }
    type alias polygon polyline
    # what an intersection or a search returns: a point, or a word
    type def intersection {{point}at | "coincident" | "none"}
    type def found        {{point}at | "none"}
}

namespace eval ::math::geometry {
    variable pi    [expr { 4 * atan(1) }]
    variable torad [expr { (4 * atan(1)) / 180.0 }]
    variable todeg [expr { 180.0 / (4 * atan(1)) }]
}

# --- Points and vectors

proc {point}::math::geometry::p  {{coord}x; {coord}y}     {( $x, $y )}
proc {point}::math::geometry::+  {{point}pa; {point}pb}     {( $pa(x) + $pb(x), $pa(y) + $pb(y) )}
proc {point}::math::geometry::-  {{point}pa; {point}pb}     {( $pa(x) - $pb(x), $pa(y) - $pb(y) )}
proc {point}::math::geometry::s* {{coord}factor; {point}p} {( $p(x) * $factor, $p(y) * $factor )}
proc ::math::geometry::distance {{point}pa; {point}pb} {( hypot($pb(x)-$pa(x), $pb(y)-$pa(y)) )}
proc ::math::geometry::length   {{vector}v}         {( hypot($v(x), $v(y)) )}
proc {point}::math::geometry::v {{coord}h}                {( 0, $h )}
proc {point}::math::geometry::h {{coord}w}                {( $w, 0 )}
proc ::math::geometry::x {{point}p}                 {set p(x)}
proc ::math::geometry::y {{point}p}                 {set p(y)}
proc {point}::math::geometry::conjx {{point}p}             {( -$p(x), $p(y) )}
proc {point}::math::geometry::conjy {{point}p}             {( $p(x), -$p(y) )}
proc {line}::math::geometry::rect  {{point}pa; {point}pb} {( $pa(x), $pa(y), $pb(x), $pb(y) )}
proc {list(2).point}::math::geometry::nwse  {{rect}r}              {( ($r(x1), $r(y1)), ($r(x2), $r(y2)) )}

# Unit vector in the direction given by an angle (degrees)
proc {vector}::math::geometry::direction {{coord}angle} {(
    t = $angle * $::math::geometry::torad;
    cos($t), sin($t)
)}

# The point at fraction s from pa to pb (0 => pa, 1 => pb)
proc {point}::math::geometry::between {{point}pa; {point}pb; {coord}s} {(
    $pa(x) + ($pb(x) - $pa(x)) * $s, $pa(y) + ($pb(y) - $pa(y)) * $s
)}

# The octant of the direction of a vector
proc ::math::geometry::octant {{vector}p} {(
    a = atan2($p(y), $p(x)) * $::math::geometry::todeg;
    a = $a < 0 ? 360 + $a : $a;
    $a <= 22.5  ? "east"      : $a <= 67.5  ? "northeast" :
    $a <= 112.5 ? "north"     : $a <= 157.5 ? "northwest" :
    $a <= 202.5 ? "west"      : $a <= 247.5 ? "southwest" :
    $a <= 292.5 ? "south"     : $a <= 337.5 ? "southeast" : "east"
)}

# --- Distances and closest points (FAQ 1.02, comp.graphics.algorithms)

proc ::math::geometry::calculateDistanceToLine {{point}P; {line}line} {(
    (Ax, Ay, Bx, By) = $line;  (Cx, Cy) = $P;
    $Ax == $Bx && $Ay == $By
        ? hypot($Cx-$Ax, $Cy-$Ay)
        : abs(($Ay-$Cy)*($Bx-$Ax)-($Ax-$Cx)*($By-$Ay)) / hypot($Bx-$Ax, $By-$Ay)
)}

proc {point}::math::geometry::findClosestPointOnLine {{point}P; {line}line} {
    lindex [findClosestPointOnLineImpl $P $line] 0
}

# {Q r}: Q the projection of P, r its position (0: A, 1: B, <0, >1: outside)
proc ::math::geometry::findClosestPointOnLineImpl {{point}P; {line}line} {(
    (Ax, Ay, Bx, By) = $line;  (Cx, Cy) = $P;
    Lsquared = pow($Bx-$Ax,2) + pow($By-$Ay,2);
    r = $Lsquared == 0 ? 0 : (($Cx-$Ax)*($Bx-$Ax) + ($Cy-$Ay)*($By-$Ay))/$Lsquared;
    $Lsquared == 0 ? (($Ax, $Ay), 0)
                   : (($Ax + $r*($Bx-$Ax), $Ay + $r*($By-$Ay)), $r)
)}

proc ::math::geometry::calculateDistanceToLineSegment {{point}P; {segment}seg} {(
    (dist, r) = [calculateDistanceToLineSegmentImpl $P $seg];
    $r < 0 ? hypot($P(x)-$seg(x1), $P(y)-$seg(y1))
  : $r > 1 ? hypot($P(x)-$seg(x2), $P(y)-$seg(y2))
  : $dist
)}

# {dist r}: the distance from P to the line, and r as above
proc ::math::geometry::calculateDistanceToLineSegmentImpl {{point}P; {segment}seg} {(
    (Ax, Ay, Bx, By) = $seg;  (Cx, Cy) = $P;
    L = hypot($Bx-$Ax, $By-$Ay);
    $L == 0 ? (hypot($Cx-$Ax, $Cy-$Ay), 0)
            : (abs(($Ay-$Cy)*($Bx-$Ax)-($Ax-$Cx)*($By-$Ay)) / $L,
               (($Cx-$Ax)*($Bx-$Ax) + ($Cy-$Ay)*($By-$Ay))/pow($L,2))
)}

proc {point}::math::geometry::findClosestPointOnLineSegment {{point}P; {segment}seg} {(
    (Q, r) = [findClosestPointOnLineImpl $P $seg];
    $r < 0 ? ($seg(x1), $seg(y1)) : $r > 1 ? ($seg(x2), $seg(y2)) : $Q
)}

proc ::math::geometry::calculateDistanceToPolyline {{point}P; {polyline}pl} {(
    minDist = "Inf";
    foreach {Bx By} [lassign $pl Ax Ay] {(
        minDist = min($minDist, [calculateDistanceToLineSegment $P [list $Ax $Ay $Bx $By]]);
        Ax = $Bx;  Ay = $By
    )};
    $minDist
)}

proc ::math::geometry::calculateDistanceToPolygon {{point}P; {polygon}pg} {
    calculateDistanceToPolyline $P [ClosedPolygon $pg]
}

proc {found}::math::geometry::findClosestPointOnPolyline {{point}P; {polyline}pl} {(
    closest = "none";  best = "Inf";
    foreach {Bx By} [lassign $pl Ax Ay] {(
        Q = [findClosestPointOnLineSegment $P [list $Ax $Ay $Bx $By]];
        d = [distance $P $Q];
        closest = $d < $best ? $Q : $closest;
        best = min($d, $best);
        Ax = $Bx;  Ay = $By
    )};
    $closest
)}

proc ::math::geometry::lengthOfPolyline {{polyline}pl} {(
    len = 0;
    foreach {x2 y2} [lassign $pl x1 y1] {(
        len = $len + hypot($x1-$x2, $y1-$y2);  x1 = $x2;  y1 = $y2
    )};
    $len
)}

# Move P in a direction (degrees, counter-clockwise) over a distance
proc {point}::math::geometry::movePointInDirection {{point}P; {coord}direction; {coord}dist} {(
    pi = 4*atan(1);
    $P(x) + $dist*cos(($direction*$pi)/180), $P(y) + $dist*sin(($direction*$pi)/180)
)}

# The angle (degrees, counter-clockwise) from the horizon to a line
proc ::math::geometry::angle {{line}line} {(
    (x1, y1, x2, y2) = $line;  pi = 4*atan(1);
    a = $x1 == $x2 ? 0 : atan(abs((1.0*$y1-$y2)/(1.0*$x1-$x2)));
    $x1 == $x2 ? ($y1 < $y2 ? 90 : 270)
      : ($y1 <= $y2 ? ($x1 < $x2 ? $a : $pi-$a)
                    : ($x1 < $x2 ? 2*$pi-$a : $pi+$a)) / $pi*180
)}

# --- Intersections

# Do two segments have a point in common? (Sedgewick)
proc ::math::geometry::lineSegmentsIntersect {{segment}s1; {segment}s2} {
    set margin 1.0e-7
    foreach {P seg} [list [list $s1(x1) $s1(y1)] $s2  [list $s1(x2) $s1(y2)] $s2 \
                          [list $s2(x1) $s2(y1)] $s1  [list $s2(x2) $s2(y2)] $s1] {
        if {[calculateDistanceToLineSegment $P $seg] < $margin} {return 1}
    }
    set A [list $s1(x1) $s1(y1)];  set B [list $s1(x2) $s1(y2)]
    set C [list $s2(x1) $s2(y1)];  set D [list $s2(x2) $s2(y2)]
    expr {[ccw $A $B $C] * [ccw $A $B $D] <= 0 && [ccw $C $D $A] * [ccw $C $D $B] <= 0}
}

# The intersection point of two segments, or "coincident", or "none"
proc {intersection}::math::geometry::findLineSegmentIntersection {{segment}s1; {segment}s2} {
    if {![lineSegmentsIntersect $s1 $s2]} {return none}
    set p [findLineIntersection $s1 $s2]
    if {$p ne "coincident"} {return $p}
    # coincident lines: do the SEGMENTS overlap (x or y, for vertical lines)?
    if {[intervalsOverlap $s1(x1) $s1(x2) $s2(x1) $s2(x2) 1]
            || [intervalsOverlap $s1(y1) $s1(y2) $s2(y1) $s2(y2) 1]} {
        return coincident
    }
    # adjacent segments: the common end point
    foreach {x y} [list $s1(x1) $s1(y1) $s1(x2) $s1(y2)] {
        if {($x == $s2(x1) && $y == $s2(y1)) || ($x == $s2(x2) && $y == $s2(y2))} {
            return [list $x $y]
        }
    }
    return none
}

# The intersection point of two lines, or "coincident", or "none"
# (http://wiki.tcl.tk/12070, Kevin Kenny ; Paul Bourke, lineline2d)
proc {intersection}::math::geometry::findLineIntersection {{line}l1; {line}l2} {(
    d  = ($l2(y2) - $l2(y1)) * ($l1(x2) - $l1(x1)) - ($l2(x2) - $l2(x1)) * ($l1(y2) - $l1(y1));
    na = ($l2(x2) - $l2(x1)) * ($l1(y1) - $l2(y1)) - ($l2(y2) - $l2(y1)) * ($l1(x1) - $l2(x1));
    $d == 0 ? ($na == 0 ? "coincident" : "none")
            : ($l1(x1) + $na * ($l1(x2) - $l1(x1)) / $d, $l1(y1) + $na * ($l1(y2) - $l1(y1)) / $d)
)}

proc ::math::geometry::polylinesIntersect {{polyline}pl1; {polyline}pl2} {
    polylinesBoundingIntersect $pl1 $pl2 0
}

# Same, at a granularity: the polylines are cut into parts of `granularity`
# points, and only the bounding boxes of the parts are compared
# (granularity <= 1: exact answer)
proc ::math::geometry::polylinesBoundingIntersect {{polyline}pl1; {polyline}pl2; {int}granularity} {
    set perfect [expr {$granularity <= 1}]
    if {$perfect} {set granularity 10}
    set step [expr {2*$granularity - 2}]
    foreach pl {pl1 pl2} {
        set parts($pl) [lmap i [lseq 0 [expr {[llength [set $pl]] - 1}] $step] {
            lrange [set $pl] $i [expr {$i + 2*$granularity - 1}]
        }]
    }
    foreach part1 $parts(pl1) {
        foreach part2 $parts(pl2) {
            lassign [bbox $part1] a1 b1 a2 b2;  lassign [bbox $part2] c1 d1 c2 d2
            if {![rectanglesOverlap [list $a1 $b1] [list $a2 $b2] [list $c1 $d1] [list $c2 $d2] 0]} {
                continue
            }
            if {!$perfect} {return 1}
            foreach {l1x2 l1y2} [lassign $part1 l1x1 l1y1] {
                foreach {l2x2 l2y2} [lassign $part2 l2x1 l2y1] {
                    if {[lineSegmentsIntersect [list $l1x1 $l1y1 $l1x2 $l1y2] [list $l2x1 $l2y1 $l2x2 $l2y2]]} {
                        return 1
                    }
                    set l2x1 $l2x2;  set l2y1 $l2y2
                }
                set l1x1 $l1x2;  set l1y1 $l1y2
            }
            return 0
        }
    }
    return 0
}

# Is A -> B -> C counter-clockwise? (1, -1, or 0; Sedgewick)
proc ::math::geometry::ccw {{point}A; {point}B; {point}C} {(
    dx1 = $B(x) - $A(x);  dy1 = $B(y) - $A(y);
    dx2 = $C(x) - $A(x);  dy2 = $C(y) - $A(y);
    $dx1*$dy2 > $dy1*$dx2 ? 1 : $dx1*$dy2 < $dy1*$dx2 ? -1
      : ($dx1*$dx2 < 0) || ($dy1*$dy2 < 0) ? -1
      : ($dx1*$dx1 + $dy1*$dy1) < ($dx2*$dx2 + $dy2*$dy2) ? 1 : 0
)}

# --- Overlaps, bounding boxes, polygons

proc ::math::geometry::intervalsOverlap {{coord}y1; {coord}y2; {coord}y3; {coord}y4; {int}strict} {(
    (y1, y2) = $y1 > $y2 ? ($y2, $y1) : ($y1, $y2);
    (y3, y4) = $y3 > $y4 ? ($y4, $y3) : ($y3, $y4);
    $strict ? $y2 > $y3 && $y4 > $y1 : $y2 >= $y3 && $y4 >= $y1
)}

proc ::math::geometry::rectanglesOverlap {{point}P1; {point}P2; {point}Q1; {point}Q2; {int}strict} {(
    [intervalsOverlap $P1(x) $P2(x) $Q1(x) $Q2(x) $strict]
        && [intervalsOverlap $P1(y) $P2(y) $Q1(y) $Q2(y) $strict]
)}

proc ::math::geometry::bbox {{polyline}pl} {(
    xs = lmap {x y} $pl {set x};  ys = lmap {x y} $pl {set y};
    [llength $xs] == 0 ? [lrepeat 4 {}]
      : ([tcl::mathfunc::min {*}$xs], [tcl::mathfunc::min {*}$ys],
         [tcl::mathfunc::max {*}$xs], [tcl::mathfunc::max {*}$ys])
)}

proc ::math::geometry::ClosedPolygon {{polygon}pg} {
    if {[lindex $pg 0] != [lindex $pg end-1] || [lindex $pg 1] != [lindex $pg end]} {
        lappend pg [lindex $pg 0] [lindex $pg 1]
    }
    return $pg
}

# Is P strictly inside the polygon? (pnpoly, W. R. Franklin; ticket dc49af96c2)
proc ::math::geometry::pointInsidePolygon {{point}P; {polygon}pg} {(
    c = 0;  (xj, yj) = [lrange $pg end-1 end];
    foreach {xi yi} $pg {(
        c = (($yi > $P(y)) != ($yj > $P(y))) && ($P(x) < ($xj - $xi) * ($P(y) - $yi) / ($yj - $yi) + $xi)
            ? !$c : $c;
        xj = $xi;  yj = $yi
    )};
    $c
)}

proc ::math::geometry::LeftOfEdge {x0 y0 x1 y1 x2 y2} {(
    ($x1 - $x0) * ($y2 - $y0) - ($x2 - $x0) * ($y1 - $y0)
)}

# Same, by the winding number (complex polygons; geomalgorithms.com a03)
proc ::math::geometry::pointInsidePolygonAlt {{point}P; {polygon}pg} {(
    w = 0;  (xi, yi) = [lrange $pg end-1 end];
    foreach {xj yj} $pg {(
        left = ($xj - $xi) * ($P(y) - $yi) - ($P(x) - $xi) * ($yj - $yi);
        w = $yi <= $P(y) ? ($yj > $P(y) && $left > 0.0 ? $w + 1 : $w)
                         : ($yj <= $P(y) && $left < 0.0 ? $w - 1 : $w);
        xi = $xj;  yi = $yj
    )};
    $w != 0
)}

# Is the rectangle P1 P2 completely inside the polygon?
proc ::math::geometry::rectangleInsidePolygon {{point}P1; {point}P2; {polygon}pg} {
    lassign [bbox $pg] x1 y1 x2 y2
    if {![rectanglesOverlap $P1 $P2 [list $x1 $y1] [list $x2 $y2] 0]} {return 0}
    # a vertex of the polygon inside the rectangle
    foreach {x y} $pg {
        if {$P1(x) < $x && $x < $P2(x) && $P1(y) < $y && $y < $P2(y)} {return 0}
    }
    # a side of the polygon crossing the rectangle
    set r [list $P1(x) $P1(y) $P2(x) $P1(y) $P2(x) $P2(y) $P1(x) $P2(y) $P1(x) $P1(y)]
    if {[polylinesIntersect [ClosedPolygon $pg] $r]} {return 0}
    # otherwise, the rectangle is completely inside or completely outside
    pointInsidePolygon $P1 $pg
}

# The area of a (non-complex) polygon: the shoelace formula
proc ::math::geometry::areaPolygon {{polygon}pg} {(
    area = 0.0;  (b1, b2) = [lrange $pg end-1 end];
    foreach {c1 c2} $pg {(
        area = $area + ($b1*$c2 - $b2*$c1);  b1 = $c1;  b2 = $c2
    )};
    0.5*abs($area)
)}

# --- Vectors of any dimension, transformations

proc ::math::geometry::inproduct {{list.coord}v1; {list.coord}v2} {(
    s = 0.0;
    foreach a $v1 b $v2 {( s = $s + $a * $b )};
    $s
)}

proc ::math::geometry::angleBetween {{list.coord}v1; {list.coord}v2} {(
    s = 0.0;  l1 = 0.0;  l2 = 0.0;
    foreach a $v1 b $v2 {( s = $s + $a * $b;  l1 = $l1 + $a * $a;  l2 = $l2 + $b * $b )};
    acos($s/sqrt($l1 * $l2)) * $::math::geometry::todeg
)}

proc ::math::geometry::areaParallellogram {{vector}v1; {vector}v2} {(
    abs($v2(x) * $v1(y) - $v1(x) * $v2(y))
)}

proc ::math::geometry::translate {{vector}t; {polyline}pl} {(
    [concat {*}[lmap {x y} $pl {( $x + $t(x), $y + $t(y) )}]]
)}

# Rotation (counter-clockwise, degrees) around the origin
proc ::math::geometry::rotate {{coord}angle; {polyline}pl} {(
    a = $::math::geometry::torad * $angle;  c = cos($a);  s = sin($a);
    [concat {*}[lmap {x y} $pl {( $c * $x - $s *$y, $s * $x + $c *$y )}]]
)}

# Reflection in the line through the origin at an angle (degrees)
proc ::math::geometry::reflect {{coord}angle; {polyline}pl} {(
    a = 2.0 * $::math::geometry::torad * $angle;  c = cos($a);  s = sin($a);
    [concat {*}[lmap {x y} $pl {( $c * $x + $s *$y, $s * $x - $c *$y )}]]
)}

proc ::math::geometry::degToRad {{coord}angle} {( $angle * $::math::geometry::torad )}
proc ::math::geometry::radToDeg {{coord}angle} {( $angle * $::math::geometry::todeg )}

namespace eval ::math::geometry {
    namespace export \
	+ - s* direction v h p between distance length \
	nwse rect octant findLineSegmentIntersection \
	findLineIntersection bbox x y conjx conjy \
	calculateDistanceToLine findClosestPointOnLine \
	calculateDistanceToLineSegment findClosestPointOnLineSegment \
	calculateDistanceToPolylineSegment findClosestPointOnPolyline lengthOfPolyline \
	movePointInDirection lineSegmentsIntersect findLineSegmentIntersection findLineIntersection \
	polylinesIntersect polylinesBoundingIntersect intervalsOverlap rectanglesOverlap pointInsidePolygon pointInsidePolygonAlt \
	rectangleInsidePolygon areaPolygon translate rotate reflect degToRad radToDeg \
	calculateDistanceToPolyline calculateDistanceToPolygon areaParallellogram angle inproduct angleBetween
}

source [file join [file dirname [info script]] geometry_circle.tcl]
source [file join [file dirname [info script]] geometry_ext.tcl]

package provide math::geometry 1.4.2
