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

# FR : la référence est le Tcl nu (étape 1). Entre parenthèses, l'écart avec
#      lui : « / 2.4 » = 2,4 fois plus lent, « x 3.0 » = 3 fois plus rapide.
# EN : the reference is plain Tcl (stage 1). In parentheses, the ratio to it:
#      "/ 2.4" = 2.4 times slower, "x 3.0" = 3 times faster.
proc ecart {t ref} {
    if {$t >= $ref} {
        return [format "(/ %.1f)" [expr {$t / $ref}]]
    }
    return [format "(x %.1f)" [expr {$ref / $t}]]
}

# FR : la sortie est un tableau markdown (colonnes séparées par « | »), lisible
#      tel quel et à copier dans un document.
# EN : the output is a markdown table.
puts "N = $N, µs par appel ; entre parenthèses : écart avec le Tcl nu"
puts "(/ = plus lent, x = plus rapide)\n"
puts [format "| %-26s | %10s | %22s | %22s | %22s |" "calcul" [etape::nom brut] \
          [etape::nom annote] [etape::nom octets] [etape::nom objet]]
puts "|[string repeat - 28]|[string repeat - 11]:|[string repeat - 23]:|[string repeat - 23]:|[string repeat - 23]:|"
foreach {libelle script} $mesures {
    set ligne [format "| %-26s |" $libelle]
    foreach e $etapes {
        set t [lindex [$I($e) eval [list timerate $script 1000]] 0]
        set T($e) $t
        if {$e eq "brut"} {
            append ligne [format " %10.1f |" $t]
        } else {
            append ligne [format " %22s |" [format "%.1f %s" $t [ecart $t $T(brut)]]]
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
puts "\n| aire, d'où vient le temps | µs |"
puts "|---|---:|"
puts [format "| voie octets : encodage | %.1f |" $enc]
puts [format "| voie octets : calcul C | %.1f |" $cal]
puts [format "| voie objet : type, contenu, lecture des points, calcul C | %.1f |" $obj]
