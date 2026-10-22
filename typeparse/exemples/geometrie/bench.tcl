# bench.tcl ?N? --
#
# FR : Compare la vitesse des trois étapes avec timerate (µs par appel).
#      N : taille du grand polygone (10000 par défaut). Le nuage a N/10 points
#      et le polygone qu'on teste N/50 sommets ; la Bézier donne N points.
#        tclsh bench.tcl ; tclsh bench.tcl 2000
# EN : Compares the speed of the three stages with timerate (µs per call).

source [file join [file dirname [file normalize [info script]]] etapes.tcl]
set N [expr {$argc ? [lindex $argv 0] : 10000}]

# FR : les données, en Tcl nu ; chaque étape les type à sa façon.
# EN : the data, as plain Tcl; each stage types them its own way.
proc cercle {n r} {
    lmap k [lseq $n] {list [expr {$r*cos(6.283185307179586*$k/$n)}] \
                          [expr {$r*sin(6.283185307179586*$k/$n)}]}
}
set grand  [cercle $N 100.0]
set petit  [cercle [expr {max(3, $N/50)}] 100.0]
expr {srand(42)}
set nuage  [lmap k [lseq [expr {$N/10}]] {list [expr {rand()*240-120}] [expr {rand()*240-120}]}]
set bez    {{0 0} {0 4} {4 4} {4 0}}

set mesures {
    "aire (N points)"             {geo::aire $G}
    "périmètre (N points)"        {geo::perimetre $G}
    "englobant (N points)"        {geo::englobant $G}
    "dedans_tous (N/10 x N/50)"   {geo::dedans_tous $Nu $Pe}
    "Bézier, N points"            {geo::bezier_echantillons $B $::N}
}

set etapes {brut annote octets objet}
foreach e $etapes {
    set i [etape::creer $e]
    set I($e) $i
    $i eval [list set ::N $N]
    $i eval [list set G  [$i eval [list donnee polygone $grand]]]
    $i eval [list set Pe [$i eval [list donnee polygone $petit]]]
    $i eval [list set Nu [$i eval [list donnee nuage $nuage]]]
    $i eval [list set B  [$i eval [list donnee bezier $bez]]]
}

puts "N = $N   (µs par appel ; entre parenthèses : gain sur l'étape 2)"
puts [format "%-28s %12s %12s %20s %20s" "" [etape::nom brut] [etape::nom annote] \
          [etape::nom octets] [etape::nom objet]]
foreach {libelle script} $mesures {
    set ligne [format "%-28s" $libelle]
    foreach e $etapes {
        set t [lindex [$I($e) eval [list timerate $script 1000]] 0]
        set T($e) $t
        if {$e in {octets objet}} {
            append ligne [format " %11.1f (x%5.0f)" $t [expr {$T(annote) / $t}]]
        } else {
            append ligne [format " %12.1f" $t]
        }
    }
    puts $ligne
}

# FR : où passe le temps dans la voie octets : l'encodage ou le calcul ?
# EN : where the time goes in the bytes way: encoding or computing?
set i $I(octets)
$i eval {set octets [geo::octets $G]}
set enc [lindex [$i eval {timerate {geo::octets $G} 1000}] 0]
set cal [lindex [$i eval {timerate {geo::c::aire_raw $octets} 1000}] 0]
set obj [lindex [$I(objet) eval {timerate {geo::c::aire_obj $G} 1000}] 0]
puts "\nvoie octets, aire : encodage $enc µs + calcul C $cal µs"
puts "voie objet,  aire : type + contenu + lecture des points + calcul C $obj µs"
