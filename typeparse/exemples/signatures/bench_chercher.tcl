# Le coût de la signature : chercher (signature typée) contre
# chercher_classique (analyse à la main, mêmes contrôles), et contre la même
# procédure sans analyse ni contrôle (arguments positionnels, même corps) :
# l'écart avec elle est le coût de l'analyse des arguments.
set d [file dirname [file normalize [info script]]]
source [file join $d chercher_classique.tcl]
set f [open [file join $d chercher.tcl]]; set src [read $f]; close $f
eval [string range $src 0 [string first "\nproc montre" $src]]
proc chercher_nu {mode nocase all debut fin compte liste motif} {
    set trouves {}
    if {$nocase} { set motif [string tolower $motif] }
    for {set i $debut} {$i <= $fin} {incr i} {
        set e [lindex $liste $i]
        if {$nocase} { set e [string tolower $e] }
        set ok [switch $mode {
            exact  { expr {$e eq $motif} }
            glob   { string match $motif $e }
            regexp { regexp -- $motif $e }
        }]
        if {$ok} {
            lappend trouves $i
            if {!$all} break
        }
    }
    set compte [llength $trouves]
    return $trouves
}
set L {Pomme poire Prune pêche abricot pomme}
foreach {nom script} {
    "signature typée"        {chercher -all -nocase -debut 1 -compte n $L p*}
    "Tcl classique"          {chercher_classique -all -nocase -debut 1 -compte n $L p*}
    "même corps, positionnel" {chercher_nu glob 1 1 1 5 n $L p*}
} {
    eval $script
    set t [lindex [time $script 200000] 0]
    puts [format "%-24s %6.2f µs" $nom $t]
}
