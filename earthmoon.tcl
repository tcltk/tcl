# earthmoon.tcl -- the Sun, the Earth and the Moon, with 4x4 matrices
#
# A demo of TIP 759 (expression syntax, native lists, tuple assignments,
# user-defined operators), on the branch tip-759h-tcl91-user-op.
#
# Every body is placed by a matrix; the matrices are composed as in the
# sky: the Moon is placed in the frame of the Earth, which is placed in the
# frame of the Sun. A viewpoint is the inverse of the matrix of the eye,
# and the perspective one more matrix, followed by a division by w.
#
# Points are row vectors: a point P is moved by P · M, so in a product
# A \matrixProd B, A acts first.
#
# Scales are compressed: the distances are not to scale, the Earth turns
# once every 4 days instead of once a day, and the orbit of the Moon is
# not inclined (so that there is an eclipse every month).
#
#   wish earthmoon.tcl                     the demo
#   wish earthmoon.tcl -record DIR N       N frames per viewpoint over a month,
#                                          as PNG files (needs ImageMagick)
lappend auto_path "C:/Tcl/src/repos/tcl-9-0-expr-type/app/lib"
package require Tk

# -------------------------------------------------------------------
# Operators, for the expressions (they would belong in a library such as
# math::linearalgebra): vectors are lists of 3 numbers, matrices lists of
# 4 rows of 4 numbers.
# -------------------------------------------------------------------
proc tcl::mathop::add {U V} {( (a, b, c) = $U; (d, e, f) = $V; ($a + $d, $b + $e, $c + $f) )}
proc tcl::mathop::sub {U V} {( (a, b, c) = $U; (d, e, f) = $V; ($a - $d, $b - $e, $c - $f) )}
proc tcl::mathop::scale {U k} {( (a, b, c) = $U; ($a*$k, $b*$k, $c*$k) )}
proc tcl::mathop::dot {U V} {( (a, b, c) = $U; (d, e, f) = $V; $a*$d + $b*$e + $c*$f )}
proc tcl::mathop::crossProd {U V} {(
    (a, b, c) = $U; (d, e, f) = $V;
    ($b*$f - $c*$e, $c*$d - $a*$f, $a*$e - $b*$d)
)}
proc tcl::mathop::normalize {U} {( $U \scale (1.0/sqrt($U \dot $U)) )}

# the point P moved by M: (x, y, z, 1) · M, the four coordinates
proc tcl::mathop::transform {P M} {(
    (x, y, z) = $P;
    ((a0,a1,a2,a3), (b0,b1,b2,b3), (c0,c1,c2,c3), (d0,d1,d2,d3)) = $M;
    ($x*$a0 + $y*$b0 + $z*$c0 + $d0,
     $x*$a1 + $y*$b1 + $z*$c1 + $d1,
     $x*$a2 + $y*$b2 + $z*$c2 + $d2,
     $x*$a3 + $y*$b3 + $z*$c3 + $d3)
)}
proc tcl::mathop::matrixProd {M1 M2} {
    lassign $M2 B0 B1 B2 B3
    lmap row $M1 {(
        (a0, a1, a2, a3) = $row;
        lmap b0 $B0 b1 $B1 b2 $B2 b3 $B3 {( $a0*$b0 + $a1*$b1 + $a2*$b2 + $a3*$b3 )}
    )}
}
# the inverse of a rigid move (a rotation R, then a translation t):
# its rotation is the transpose of R, its translation -t·Rᵀ
proc tcl::mathop::inverse {M} {(
    ((a0,a1,a2,a3), (b0,b1,b2,b3), (c0,c1,c2,c3), (tx,ty,tz,t3)) = $M;
    t = ($tx, $ty, $tz);
    (($a0, $b0, $c0, 0),
     ($a1, $b1, $c1, 0),
     ($a2, $b2, $c2, 0),
     (-($t \dot ($a0,$a1,$a2)), -($t \dot ($b0,$b1,$b2)), -($t \dot ($c0,$c1,$c2)), 1))
)}
# the matrix of an eye at C looking at T, the y axis up: its rows are the
# right, up and forward directions of the eye, then its position
proc tcl::mathop::lookAt {C T} {(
    f = \normalize ($T \sub $C);
    r = \normalize ($f \crossProd (0, 1, 0));
    u = $r \crossProd $f;
    (rx, ry, rz) = $r; (ux, uy, uz) = $u; (fx, fy, fz) = $f; (cx, cy, cz) = $C;
    (($rx, $ry, $rz, 0), ($ux, $uy, $uz, 0), ($fx, $fy, $fz, 0), ($cx, $cy, $cz, 1))
)}

# -------------------------------------------------------------------
# Matrices
# -------------------------------------------------------------------
proc Translation {x y z} {( ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), ($x, $y, $z, 1)) )}
proc Scale {k} {( (($k, 0, 0, 0), (0, $k, 0, 0), (0, 0, $k, 0), (0, 0, 0, 1)) )}
proc RotY {a} {(
    c = cos($a); s = sin($a);
    (($c, 0, -$s, 0), (0, 1, 0, 0), ($s, 0, $c, 0), (0, 0, 0, 1))
)}
proc RotZ {a} {(
    c = cos($a); s = sin($a);
    (($c, $s, 0, 0), (-$s, $c, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1))
)}
# perspective: (x, y, z, 1) becomes (f·x + cx·z, -f·y + cy·z, z, z); the
# division by w = z gives the point on the screen, z its depth
proc Perspective {f cx cy} {( (($f, 0, 0, 0), (0, -$f, 0, 0), ($cx, $cy, 1, 1), (0, 0, 0, 0)) )}

# -------------------------------------------------------------------
# A sphere: an icosahedron, each face divided in 4, n times, the new
# vertices pushed out to the unit sphere
# -------------------------------------------------------------------
proc Sphere {n} {(
    t = (1 + sqrt(5))/2;
    V = lmap v [(
        (-1, $t, 0), (1, $t, 0), (-1, -$t, 0), (1, -$t, 0),
        (0, -1, $t), (0, 1, $t), (0, -1, -$t), (0, 1, -$t),
        ($t, 0, -1), ($t, 0, 1), (-$t, 0, -1), (-$t, 0, 1)
    )] {( \normalize $v )};
    F = {{0 11 5} {0 5 1} {0 1 7} {0 7 10} {0 10 11} {1 5 9} {5 11 4} {11 10 2}
         {10 7 6} {7 1 8} {3 9 4} {3 4 2} {3 2 6} {3 6 8} {3 8 9} {4 9 5}
         {2 4 11} {6 2 10} {8 6 7} {9 8 1}};
    for {set k 0} {$k < $n} {incr k} {
        set mid {}
        set F [concat {*}[lmap face $F {
            lassign $face a b c
            set ab [Middle V mid $a $b]
            set bc [Middle V mid $b $c]
            set ca [Middle V mid $c $a]
            list [list $a $ab $ca] [list $b $bc $ab] [list $c $ca $bc] [list $ab $bc $ca]
        }]]
    };
    ($V, $F)
)}
proc Middle {vName midName i j} {
    upvar 1 $vName V $midName mid
    set key [expr {$i < $j ? "$i,$j" : "$j,$i"}]
    if {![dict exists $mid $key]} {
        lappend V [( \normalize ([lindex $V $i] \add [lindex $V $j]) )]
        dict set mid $key [expr {[llength $V] - 1}]
    }
    dict get $mid $key
}

# -------------------------------------------------------------------
# Colours: a function of the point of the unit sphere, in the frame of
# the body (so the continents turn with the Earth)
# -------------------------------------------------------------------
proc EarthColour {P} {(
    (x, y, z) = $P;
    land = sin(2.6*$x + 1.1) * cos(2.2*$y - 0.4) + 0.7*sin(3.1*$z + 2.0*$x) + 0.35*cos(5*$y + 3*$z);
    abs($y) > 0.86 ? (235, 240, 250) :
    $land > 0.55 ? (165, 140, 85) :
    $land > 0.2 ? (55, 125, 55) : (25, 70, 160)
)}
# the near side, which always faces the Earth, is the side x < 0
proc MoonColour {P} {(
    (x, y, z) = $P;
    mare = sin(4.0*$y + 1.0) * cos(3.0*$z) + 0.5*sin(6*$z - 2*$y);
    $x < -0.2 && $mare > 0.3 ? (85, 85, 92) : (160, 158, 152)
)}
proc SunColour {P} {( (255, 205, 70) )}

proc Rgb {c k} {(
    (r, g, b) = $c;
    [format #%02x%02x%02x [( min(255, round($r*$k)) )] [( min(255, round($g*$k)) )] [( min(255, round($b*$k)) )]]
)}

# -------------------------------------------------------------------
# The scene
# -------------------------------------------------------------------
set pi [expr {acos(-1)}]
array set sky {
    sunR 1.6  earthR 0.6  moonR 0.18
    earthOrbit 12.0  moonOrbit 3.2
    year 365.25  month 27.32  day 4.0  sunDay 25.4
    tilt 23.44
}
lassign [Sphere 3] unitV unitF
set bodies {Sun Earth Moon}
foreach b $bodies {
    # the colour of each face, from its centre on the unit sphere
    set colours($b) [lmap face $unitF {(
        (i, j, k) = $face;
        [${b}Colour [( \normalize ([lindex $unitV $i] \add [lindex $unitV $j] \add [lindex $unitV $k]) )]]
    )}]
}
# stars: fixed directions, far away
expr {srand(7)}
set stars [lmap i [lseq 220] {( \normalize (2*rand() - 1, 2*rand() - 1, 2*rand() - 1) )}]

# the matrices of the bodies on day d
proc Matrices {d} {(
    global sky pi;
    yearAngle = 2*$pi*$d/$sky(year);
    # the Earth goes round the Sun: its orbit frame is only a translation
    E = (($sky(earthOrbit), 0, 0) \transform [RotY $yearAngle]);
    Orbit = [Translation {*}[lrange $E 0 2]];
    # it turns on its own axis, tilted, fixed in space (the seasons)
    Earth = [Scale $sky(earthR)] \matrixProd [RotY [( 2*$pi*$d/$sky(day) )]] \matrixProd
            [RotZ [( $sky(tilt)*$pi/180 )]] \matrixProd $Orbit;
    # the Moon: moved out on its orbit, then turned with it around the
    # Earth, then carried by the Earth's orbit. One rotation for its orbit
    # and its turn on itself: the same face always looks at the Earth
    Moon = [Scale $sky(moonR)] \matrixProd [Translation $sky(moonOrbit) 0 0] \matrixProd
           [RotY [( 2*$pi*$d/$sky(month) )]] \matrixProd $Orbit;
    Sun = [Scale $sky(sunR)] \matrixProd [RotY [( 2*$pi*$d/$sky(sunDay) )]];
    ("Sun", $Sun, "Earth", $Earth, "Moon", $Moon)
)}

# the eye, for a viewpoint: (position, target, body not drawn)
proc Eye {view M} {(
    global sky;
    E = lrange [( (0, 0, 0) \transform [dict get $M Earth] )] 0 2;
    L = lrange [( (0, 0, 0) \transform [dict get $M Moon] )] 0 2;
    $view eq "moon"  ? ($L \add (\normalize ($E \sub $L) \scale (1.3*$sky(moonR))), $E, "Moon") :
    $view eq "earth" ? ($E \add (\normalize ($L \sub $E) \scale (1.12*$sky(earthR))), $L, "Earth") :
    ((0, 15, 21), (0, 0, 0), "")
)}

# -------------------------------------------------------------------
# Drawing a frame
# -------------------------------------------------------------------
proc Draw {c view d} {
    global sky unitV unitF colours stars bodies pi W H
    $c delete all
    set M [Matrices $d]
    lassign [Eye $view $M] C T hidden
    # the view: the inverse of the matrix of the eye; then the perspective
    # the field of view: wide from the Moon, narrow from the Earth
    set fov [dict get {moon 50 earth 24 overview 35} $view]
    set VP [( \inverse ($C \lookAt $T) \matrixProd [Perspective [( $W/2.0/tan($fov*$pi/180) )] [( $W/2.0 )] [( $H/2.0 )]] )]

    # the stars, around the eye
    foreach s $stars {(
        (x, y, z, w) = ($C \add ($s \scale 500)) \transform $VP;
        if {$w > 0} {
            $c create rectangle [( $x/$w )] [( $y/$w )] [( $x/$w + 1 )] [( $y/$w + 1 )] -fill #8890a0 -outline ""
        }
    )}
    # the orbits, seen from outside
    if {$view eq "overview"} {
        Orbit $c $VP [lmap i [lseq 0 361 3] {( ($sky(earthOrbit)*cos($i*$pi/180), 0, -$sky(earthOrbit)*sin($i*$pi/180)) )}] #3a4458
        set E [lrange [( (0, 0, 0) \transform [dict get $M Earth] )] 0 2]
        Orbit $c $VP [lmap i [lseq 0 361 6] {( $E \add ($sky(moonOrbit)*cos($i*$pi/180), 0, -$sky(moonOrbit)*sin($i*$pi/180)) )}] #3a4558
    }

    # the faces of the bodies, painted from the farthest to the nearest
    set faces {}
    foreach b $bodies {
        if {$b eq $hidden} continue
        set Mb [dict get $M $b]
        set world [lmap v $unitV {lrange [( $v \transform $Mb )] 0 2}]
        set screen [lmap p $world {( $p \transform $VP )}]
        foreach face $unitF colour $colours($b) {(
            (i, j, k) = $face;
            (A, B, D) = (lindex $world $i, lindex $world $j, lindex $world $k);
            centre = ($A \add $B \add $D) \scale (1.0/3);
            n = \normalize (($B \sub $A) \crossProd ($D \sub $A));
            # seen from behind: not drawn
            if {[( $n \dot ($C \sub $centre) )] <= 0} continue;
            ((x1,y1,z1,w1), (x2,y2,z2,w2), (x3,y3,z3,w3)) = (lindex $screen $i, lindex $screen $j, lindex $screen $k);
            if {[( min($w1, $w2, $w3) )] < 0.01} continue;
            # light: from the Sun, at the origin; the Sun shines by itself
            light = $b eq "Sun" ? 0.8 + 0.25*($n \dot \normalize ($C \sub $centre)) :
                                  0.1 + 0.9*max(0, $n \dot \normalize ((0, 0, 0) \sub $centre));
            lappend faces [( ($w1 + $w2 + $w3, $x1/$w1, $y1/$w1, $x2/$w2, $y2/$w2, $x3/$w3, $y3/$w3, [Rgb $colour $light]) )]
        )}
    }
    foreach f [lsort -real -decreasing -index 0 $faces] {
        lassign $f - x1 y1 x2 y2 x3 y3 fill
        $c create polygon $x1 $y1 $x2 $y2 $x3 $y3 -fill $fill -outline $fill
    }
    $c create text 12 12 -anchor nw -fill #c8d0e0 -font {Helvetica 13 bold} \
        -text [dict get {moon "From the Moon" earth "From the Earth" overview "The Sun, the Earth and the Moon"} $view]
    $c create text 12 32 -anchor nw -fill #8890a0 -font {Helvetica 10} -text [format "day %.1f" $d]
    $c create text 12 [expr {$H - 10}] -anchor sw -fill #606878 -font {Helvetica 9} \
        -text "Not to scale: distances compressed, the Earth turns in 4 days, the Moon's orbit is not inclined."
}
proc Orbit {c VP points colour} {
    set xy {}
    foreach p $points {(
        (x, y, z, w) = $p \transform $VP;
        if {$w > 0} {lappend xy [( $x/$w )] [( $y/$w )]}
    )}
    if {[llength $xy] >= 4} {$c create line {*}$xy -fill $colour}
}

# -------------------------------------------------------------------
# The window
# -------------------------------------------------------------------
set W 640; set H 480
set view moon
set d 0.0
set step 0.25
wm title . "The Sun, the Earth and the Moon (TIP 759)"
pack [canvas .c -width $W -height $H -bg black -highlightthickness 0]
pack [frame .b] -fill x
foreach {v label} {moon "From the Moon" earth "From the Earth" overview "Overview"} {
    pack [radiobutton .b.$v -text $label -variable view -value $v -indicatoron 0 -padx 8] -side left
}
pack [checkbutton .b.pause -text Pause -variable pause -indicatoron 0 -padx 8] -side right
set pause 0

proc Tick {} {
    global d step pause view
    if {!$pause} {set d [expr {$d + $step}]}
    Draw .c $view $d
    after 40 Tick
}

if {[lindex $argv 0] eq "-record"} {
    lassign $argv - dir n
    file mkdir $dir
    set k 0
    foreach view {moon earth overview} {
        set d 0.0
        for {set i 0} {$i < $n} {incr i} {
            Draw .c $view $d
            update
            exec import -window [winfo id .c] [file join $dir [format f%04d.png $k]]
            incr k
            set d [expr {$d + 29.53/$n}]    ;# one month (new Earth to new Earth)
        }
    }
    exit
}
Tick
