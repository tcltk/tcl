# bench_options.tcl -- options et drapeaux typés (v16q) contre la même
# analyse d'options écrite à la main (préfixes, options exclusives, contrôle
# de -index, message d'usage).
#   usage : tclsh bench_options.tcl
#
# Résultat type (µs par appel, meilleur de 15 passes alternées) :
#   typé, -dec -index 1 -integer   2,2     à la main   3,3
#   typé, sans option              1,5     à la main   1,1
#   procédure sans options         0,5
# Avec des options, la signature typée est plus rapide que l'analyse en Tcl ;
# sans option, elle coûte ~0,4 µs de plus (les locales des options sont
# déclarées typées, avec leurs défauts).

# A : options typées
proc {list}trierT {
    {flag}-decreasing;
    {int}-index ?= -1;
    {"ascii", "dictionary", "integer"}-mode ?= ascii;
    {list}l
} {
    set o [list -$mode]
    if {$decreasing} {lappend o -decreasing}
    if {$index >= 0} {lappend o -index $index}
    lsort {*}$o $l
}
# B : la même, analyse des options écrite à la main (préfixes, exclusives, contrôles)
proc trierC {args} {
    set decreasing 0; set index -1; set mode ascii; set modeSet ""
    while {[llength $args] > 1} {
        set opt [lindex $args 0]
        if {![string match -* $opt]} break
        set args [lrange $args 1 end]
        if {$opt eq "--"} break
        set opt [tcl::prefix match -message option {-ascii -decreasing -dictionary -index -integer} $opt]
        switch -- $opt {
            -decreasing {set decreasing 1}
            -index {
                if {[llength $args] == 0} {error "option \"-index\" needs a value"}
                set args [lassign $args index]
                if {![string is entier -strict $index]} {error "option \"-index\": expected integer but got \"$index\""}
            }
            default {
                set m [string range $opt 1 end]
                if {$modeSet ne "" && $modeSet ne $m} {error "options -$modeSet and -$m are exclusive"}
                set mode $m; set modeSet $m
            }
        }
    }
    if {[llength $args] != 1} {error "wrong # args: should be \"trierC ?-decreasing? ?-index value? ?-ascii|-dictionary|-integer? l\""}
    set l [lindex $args 0]
    if {[catch {llength $l}]} {error "argument \"l\": expected list"}
    set o [list -$mode]
    if {$decreasing} {lappend o -decreasing}
    if {$index >= 0} {lappend o -index $index}
    lsort {*}$o $l
}
# C : sans options (plancher : lsort direct dans une procédure)
proc trierN {l} { lsort -decreasing -index 1 $l }
set L {{a 3} {b 1} {c 2}}
set cas {
    "A typé : -dec -index 1 -integer"      {trierT -dec -index 1 -integer $L}
    "B à la main : idem"                   {trierC -dec -index 1 -integer $L}
    "A typé : sans option"                 {trierT $L}
    "B à la main : sans option"            {trierC $L}
    "C procédure sans options"             {trierN $L}
}
foreach {lib sc} $cas { set best($lib) 1e9; uplevel #0 [list time $sc 2000] }
for {set r 0} {$r < 15} {incr r} {
    foreach {lib sc} $cas {
        set t [lindex [uplevel #0 [list time $sc 100000]] 0]
        if {$t < $best($lib)} { set best($lib) $t }
    }
}
foreach {lib sc} $cas { puts [format "%-40s %6.3f µs" $lib $best($lib)] }
puts [list [trierT -dec -index 1 -integer $L] [trierC -dec -index 1 -integer $L]]
foreach c {{trierT -ascii -integer $L} {trierC -ascii -integer $L} {trierT -index x $L} {trierC -index x $L} {trierT -zz $L} {trierC -zz $L}} {
    catch $c m; puts "$c -> $m"
}
