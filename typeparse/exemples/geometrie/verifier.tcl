# verifier.tcl --
#
# FR : Rejoue les exemples dans les trois étapes (et les deux voies de
#      l'étape 3) et vérifie qu'elles donnent le même résultat.
#        tclsh verifier.tcl
# EN : Replays the examples in the three stages (and both ways of stage 3)
#      and checks they all give the same result.

source [file join [file dirname [file normalize [info script]]] etapes.tcl]

# numéro  appel  {type valeur}...  attendu
set cas {
    1   geo::projete         {point {3 4}} {droite {{0 0} {10 0}}}       "3 0"
    1b  geo::projete         {point {2 0}} {droite {{0 0} {1 1}}}        "1 1"
    2   geo::distance_segment {point {5 5}} {segment {{0 0} {10 0}}}     5
    2b  geo::distance_segment {point {-3 4}} {segment {{0 0} {10 0}}}    5
    3   geo::intersection    {segment {{0 0} {4 4}}} {segment {{0 4} {4 0}}} "2 2"
    3b  geo::intersection    {segment {{0 0} {4 0}}} {segment {{0 1} {4 1}}} aucune
    4   geo::longueur        {polyligne {{0 0} {3 4} {3 10}}}            11
    5   geo::aire            {triangle {{0 0} {4 0} {0 3}}}              6
    5b  geo::perimetre       {triangle {{0 0} {4 0} {0 3}}}              12
    6   geo::aire            {carre {{0 0} {4 0} {4 4} {0 4}}}           16
    7   geo::dedans          {point {2 2}} {polygone {{0 0} {4 0} {4 4} {0 4}}} oui
    7b  geo::dedans          {point {5 1}} {polygone {{0 0} {4 0} {4 4} {0 4}}} non
    7c  geo::dedans_tous     {nuage {{2 2} {5 1} {1 3}}} {polygone {{0 0} {4 0} {4 4} {0 4}}} "1 0 1"
    8   geo::cercle_droite   {cercle {{0 0} 5}} {droite {{0 3} {1 3}}}   "{-4 3} {4 3}"
    9   geo::circonscrit     {triangle {{0 0} {4 0} {0 3}}}              "{2 1.5} 2.5"
    10  geo::gravite         {triangle {{0 0} {6 0} {0 3}}}              "2 1"
    11  geo::rot90           {vecteur {1 0}}                             "0 1"
    11b geo::scal            {vecteur {1 2}} {vecteur {3 4}}             11
    12  geo::segment2rect    {segment {{0 0} {4 4}}}                     "{0 0} {0 4} {4 4} {4 0}"
    13  geo::englobant       {triangle {{0 0} {4 0} {0 3}}}              "{0 0} {4 3}"
    14  geo::bezier_point    {bezier {{0 0} {0 4} {4 4} {4 0}}} {- 0.5} "2 3"
    14b geo::bezier_echantillons {bezier {{0 0} {0 4} {4 4} {4 0}}} {- 3} "{0 0} {2 3} {4 0}"
}

set etapes {brut annote octets objet}
foreach e $etapes {set I($e) [etape::creer $e]}

puts [format "%-4s %-24s %-26s %s" n° appel "résultat (normalisé)" "étapes"]
set erreurs 0
foreach {n cmd args attendu} [concat {*}[lmap ligne [split [string trim $cas] \n] {
        set l [string trim $ligne]
        list [lindex $l 0] [lindex $l 1] [lrange $l 2 end-1] [lindex $l end]
    }]] {
    set res {}
    foreach e $etapes {
        set i $I($e)
        set appel [list $cmd]
        foreach a $args {lappend appel [$i eval [list donnee {*}$a]]}
        if {[catch {$i eval [list normalise [$i eval $appel]]} r]} {set r "ERREUR : $r"}
        dict set res $e $r
    }
    set valeurs [lsort -unique [dict values $res]]
    set ok [expr {[llength $valeurs] == 1 && [lindex $valeurs 0] eq $attendu}]
    if {!$ok} {incr erreurs}
    puts [format "%-4s %-24s %-26s %s" $n $cmd [lindex $valeurs 0] \
        [expr {$ok ? "les 4 identiques, = attendu" : "DIFFÉRENCE : $res (attendu $attendu)"}]]
}
puts [expr {$erreurs ? "\n$erreurs différence(s)" : "\nles 4 versions concordent sur les [expr {[llength [split [string trim $cas] \n]]}] cas"}]
