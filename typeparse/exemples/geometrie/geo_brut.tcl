# geo_brut.tcl --
#
# FR : Étape 1 : le prototype en Tcl ordinaire, sans annotations, à la manière
#      de tcllib math::geometry : un point est une liste {x y}, un segment ou
#      une droite une liste de deux points, un polygone une liste de points.
#      Rien n'empêche de passer un segment là où l'on attend une droite :
#      c'est le défaut que l'étape 2 (../geometrie.tcl) corrige.
#      Mêmes noms et mêmes arguments que l'étape 2 : verifier.tcl les compare.
#
# EN : Stage 1: the prototype in plain Tcl, without annotations, the
#      math::geometry way: a point is a list {x y}, a segment or a line a list
#      of two points, a polygon a list of points. Nothing stops a segment from
#      being passed where a line is expected: stage 2 (../geometrie.tcl) fixes
#      that. Same names and arguments as stage 2: verifier.tcl compares them.

namespace eval geo {}

# --- vecteurs / vectors ----------------------------------------------------
proc geo::vec {a b} {
    lassign $a xa ya; lassign $b xb yb
    list [expr {$xb - $xa}] [expr {$yb - $ya}]
}
proc geo::scal {u v} {
    lassign $u ux uy; lassign $v vx vy
    expr {$ux*$vx + $uy*$vy}
}
proc geo::vectoriel {u v} {
    lassign $u ux uy; lassign $v vx vy
    expr {$ux*$vy - $uy*$vx}
}
proc geo::norme {u} {expr {sqrt([geo::scal $u $u])}}
proc geo::rot90 {u} {lassign $u ux uy; list [expr {-$uy}] $ux}
proc geo::distance {a b} {geo::norme [geo::vec $a $b]}

# --- droites et segments / lines and segments ------------------------------
proc geo::projete {p d} {
    lassign $d a b
    set u [geo::vec $a $b]; set w [geo::vec $a $p]
    set t [expr {[geo::scal $w $u] / double([geo::scal $u $u])}]
    lassign $a xa ya; lassign $u ux uy
    list [expr {$xa + $t*$ux}] [expr {$ya + $t*$uy}]
}
proc geo::distance_segment {p s} {
    lassign $s a b
    set u [geo::vec $a $b]; set w [geo::vec $a $p]
    set t [expr {max(0.0, min(1.0, [geo::scal $w $u] / double([geo::scal $u $u])))}]
    lassign $a xa ya; lassign $u ux uy
    geo::distance $p [list [expr {$xa + $t*$ux}] [expr {$ya + $t*$uy}]]
}
proc geo::intersection {s1 s2} {
    lassign $s1 a b; lassign $s2 c d
    set r [geo::vec $a $b]; set s [geo::vec $c $d]; set q [geo::vec $a $c]
    set den [expr {double([geo::vectoriel $r $s])}]
    if {$den == 0} {return aucune}
    set t [expr {[geo::vectoriel $q $s] / $den}]
    set u [expr {[geo::vectoriel $q $r] / $den}]
    if {$t < 0 || $t > 1 || $u < 0 || $u > 1} {return aucune}
    lassign $a xa ya; lassign $r rx ry
    list [expr {$xa + $t*$rx}] [expr {$ya + $t*$ry}]
}

# --- polylignes et polygones / polylines and polygons ----------------------
proc geo::longueur {l} {
    set s 0.0
    foreach i [lseq 1 [expr {[llength $l] - 1}]] {
        set s [expr {$s + [geo::distance [lindex $l $i-1] [lindex $l $i]]}]
    }
    return $s
}
proc geo::perimetre {p} {
    set n [llength $p]; set s 0.0
    foreach i [lseq $n] {
        set s [expr {$s + [geo::distance [lindex $p $i] [lindex $p [expr {($i+1) % $n}]]]}]
    }
    return $s
}
proc geo::aire {p} {
    set n [llength $p]; set s 0.0
    foreach i [lseq $n] {
        lassign [lindex $p $i] xa ya
        lassign [lindex $p [expr {($i+1) % $n}]] xb yb
        set s [expr {$s + $xa*$yb - $xb*$ya}]
    }
    expr {abs($s) / 2.0}
}
proc geo::dedans {p q} {
    lassign $p x y; set n [llength $q]; set c 0
    foreach i [lseq $n] {
        lassign [lindex $q $i] xa ya
        lassign [lindex $q [expr {($i+1) % $n}]] xb yb
        if {($ya > $y) != ($yb > $y)
                && $x < $xa + ($y - $ya) * ($xb - $xa) / double($yb - $ya)} {
            set c [expr {1 - $c}]
        }
    }
    expr {$c ? "oui" : "non"}
}
proc geo::dedans_tous {pts q} {
    lmap p $pts {expr {[geo::dedans $p $q] eq "oui"}}
}
proc geo::englobant {p} {
    set xs [lmap q $p {lindex $q 0}]; set ys [lmap q $p {lindex $q 1}]
    list [list [tcl::mathfunc::min {*}$xs] [tcl::mathfunc::min {*}$ys]] \
         [list [tcl::mathfunc::max {*}$xs] [tcl::mathfunc::max {*}$ys]]
}
proc geo::segment2rect {s} {
    lassign $s a c; lassign $a xa ya; lassign $c xc yc
    set xm [expr {($xa + $xc) / 2.0}]; set ym [expr {($ya + $yc) / 2.0}]
    set xb [expr {$xm - ($yc - $ym)}]; set yb [expr {$ym + ($xc - $xm)}]
    set xd [expr {2*$xm - $xb}];       set yd [expr {2*$ym - $yb}]
    list [list $xa $ya] [list $xb $yb] [list $xc $yc] [list $xd $yd]
}

# --- triangles et cercles / triangles and circles --------------------------
proc geo::gravite {t} {
    lassign $t a b c; lassign $a ax ay; lassign $b bx by; lassign $c cx cy
    list [expr {($ax + $bx + $cx) / 3.0}] [expr {($ay + $by + $cy) / 3.0}]
}
proc geo::circonscrit {t} {
    lassign $t a b c; lassign $a ax ay; lassign $b bx by; lassign $c cx cy
    set d [expr {2.0 * ($ax*($by - $cy) + $bx*($cy - $ay) + $cx*($ay - $by))}]
    set a2 [expr {$ax**2 + $ay**2}]; set b2 [expr {$bx**2 + $by**2}]
    set c2 [expr {$cx**2 + $cy**2}]
    set o [list [expr {($a2*($by - $cy) + $b2*($cy - $ay) + $c2*($ay - $by)) / $d}] \
                [expr {($a2*($cx - $bx) + $b2*($ax - $cx) + $c2*($bx - $ax)) / $d}]]
    list $o [geo::distance $o $a]
}
proc geo::cercle_droite {c d} {
    lassign $c o r
    set h [geo::projete $o $d]; set dist [geo::distance $o $h]
    set u [geo::vec {*}$d]
    if {$dist > $r} {return {}}
    set k [expr {sqrt(max(0.0, $r**2 - $dist**2)) / [geo::norme $u]}]
    lassign $h hx hy; lassign $u ux uy
    if {$k == 0} {return [list $h]}
    list [list [expr {$hx - $k*$ux}] [expr {$hy - $k*$uy}]] \
         [list [expr {$hx + $k*$ux}] [expr {$hy + $k*$uy}]]
}

# --- Bézier cubique / cubic Bézier -------------------------------------------
proc geo::bezier_point {b t} {
    lassign $b p0 p1 p2 p3
    lassign $p0 x0 y0; lassign $p1 x1 y1; lassign $p2 x2 y2; lassign $p3 x3 y3
    set s [expr {1.0 - $t}]
    list [expr {$s**3*$x0 + 3*$s**2*$t*$x1 + 3*$s*$t**2*$x2 + $t**3*$x3}] \
         [expr {$s**3*$y0 + 3*$s**2*$t*$y1 + 3*$s*$t**2*$y2 + $t**3*$y3}]
}
proc geo::bezier_echantillons {b n} {
    lmap i [lseq $n] {geo::bezier_point $b [expr {$i / double($n - 1)}]}
}
