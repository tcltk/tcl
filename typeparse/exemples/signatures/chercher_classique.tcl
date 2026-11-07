# La même procédure en Tcl classique : l'analyse des arguments à la main,
# avec les mêmes contrôles que la signature de chercher.tcl.
proc chercher_classique {args} {
    set usage {wrong # args: should be "chercher_classique ?-exact|-glob|-regexp? ?-nocase? ?-all? ?-debut value? ?-fin value? ?-compte varName? ?--? liste motif"}
    set opts {-all -compte -debut -exact -fin -glob -mode -nocase -regexp --}
    set mode {}; set nocase 0; set all 0; set debut 0
    set i 0
    while {$i < [llength $args]} {
        set w [lindex $args $i]
        if {![string match -* $w] || [string is double -strict $w]} break
        set o [tcl::prefix match -message option $opts $w]
        incr i
        if {$o eq "--"} break
        switch -- $o {
            -exact - -glob - -regexp {
                set m [string range $o 1 end]
                if {$mode ne "" && $mode ne $m} {
                    return -code error "options -$mode and -$m are exclusive"
                }
                set mode $m
            }
            -nocase { set nocase 1 }
            -all    { set all 1 }
            default {
                if {$i >= [llength $args]} {
                    return -code error "option \"$o\" needs a value"
                }
                set v [lindex $args $i]; incr i
                switch -- $o {
                    -mode {
                        if {$v ni {exact glob regexp}} {
                            return -code error "bad value \"$v\" for option \"-mode\": must be exact, glob or regexp"
                        }
                        set mode $v
                    }
                    -debut - -fin {
                        if {![string is entier -strict $v]} {
                            return -code error "option \"$o\": expected integer but got \"$v\""
                        }
                        set [string range $o 1 end] $v
                    }
                    -compte {
                        upvar 1 $v compte
                        if {[info exists compte] && ![string is entier -strict $compte]} {
                            return -code error "option \"-compte\": expected integer but got \"$compte\""
                        }
                    }
                }
            }
        }
    }
    if {[llength $args] - $i != 2} { return -code error $usage }
    lassign [lrange $args $i end] liste motif
    if {[catch {llength $liste}]} {
        return -code error "argument \"liste\": expected list but got \"$liste\""
    }
    if {$mode eq ""} { set mode glob }
    if {![info exists fin]} { set fin [expr {[llength $liste] - 1}] }

    # le corps, identique
    set trouves {}
    if {$nocase} { set motif [string tolower $motif] }
    for {set k $debut} {$k <= $fin} {incr k} {
        set e [lindex $liste $k]
        if {$nocase} { set e [string tolower $e] }
        set ok [switch $mode {
            exact  { expr {$e eq $motif} }
            glob   { string match $motif $e }
            regexp { regexp -- $motif $e }
        }]
        if {$ok} {
            lappend trouves $k
            if {!$all} break
        }
    }
    set compte [llength $trouves]
    return $trouves
}
