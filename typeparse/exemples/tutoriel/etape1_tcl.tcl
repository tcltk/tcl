# Étape 1 : le prototype, en Tcl ordinaire.
#
# Un polygone est une liste de points, un point la liste {x y}.

namespace eval poly {}

proc poly::point {x y} {
    list $x $y
}

# L'aire (formule du lacet)
proc poly::aire {pg} {
    set a 0.0
    lassign [lindex $pg end] x0 y0
    foreach p $pg {
	lassign $p x y
	set a [expr {$a + $x0 * $y - $y0 * $x}]
	set x0 $x
	set y0 $y
    }
    expr {abs($a) / 2}
}

# Le périmètre ; -ouvert : une ligne brisée, sans le dernier côté
proc poly::perimetre {args} {
    set ouvert 0
    while {[string match -* [lindex $args 0]]} {
	set args [lassign $args opt]
	switch -- $opt {
	    -ouvert {set ouvert 1}
	    --      {break}
	    default {return -code error "bad option \"$opt\": must be -ouvert or --"}
	}
    }
    if {[llength $args] != 1} {
	return -code error {wrong # args: should be "poly::perimetre ?-ouvert? ?--? pg"}
    }
    set pg [lindex $args 0]
    set l 0.0
    if {$ouvert} {
	lassign [lindex $pg 0] x0 y0
	set pg [lrange $pg 1 end]
    } else {
	lassign [lindex $pg end] x0 y0
    }
    foreach p $pg {
	lassign $p x y
	set l [expr {$l + hypot($x - $x0, $y - $y0)}]
	set x0 $x
	set y0 $y
    }
    return $l
}

# Le point P est-il dans le polygone ? (lancer de rayon)
proc poly::contient {pg P} {
    lassign $P px py
    set dedans 0
    lassign [lindex $pg end] x0 y0
    foreach p $pg {
	lassign $p x y
	if {($y > $py) != ($y0 > $py)
		&& $px < ($x0 - $x) * ($py - $y) / ($y0 - $y) + $x} {
	    set dedans [expr {!$dedans}]
	}
	set x0 $x
	set y0 $y
    }
    return $dedans
}

proc poly::deplace {pg dx {dy 0.0}} {
    lmap p $pg {
	lassign $p x y
	list [expr {$x + $dx}] [expr {$y + $dy}]
    }
}

# La boîte englobante : {xmin ymin xmax ymax}
proc poly::boite {pg} {
    lassign [lindex $pg 0] xmin ymin
    set xmax $xmin
    set ymax $ymin
    foreach p $pg {
	lassign $p x y
	set xmin [expr {min($xmin, $x)}]
	set xmax [expr {max($xmax, $x)}]
	set ymin [expr {min($ymin, $y)}]
	set ymax [expr {max($ymax, $y)}]
    }
    list $xmin $ymin $xmax $ymax
}
