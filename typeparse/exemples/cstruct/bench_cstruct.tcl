# bench_cstruct.tcl annote|annote-tcl|classique ?typeparse? ?N?
# FR : mesure, sur un lot de N mesures (10000 par défaut), chaque étape de
#      l'exemple cstruct, en version annotée et en version Tcl classique.
#      Résultat : µs par opération. annote-tcl : codecs en Tcl seulement
#      (set ::tcl_typebinaryc 0), pour comparer avec le codec C.
#      La version classique tourne aussi sur un tclsh 9.0 d'origine (sans patch).
#   tclsh bench_cstruct.tcl annote ; tclsh bench_cstruct.tcl classique
set here [file dirname [file normalize [info script]]]
lassign $argv mode tp N
if {$tp eq ""} {set tp [file join $here .. ..]}
if {$N eq ""} {set N 10000}
if {$mode eq "annote-tcl"} {set ::tcl_typebinaryc 0; set mode annote}
source [file join $here cstruct_classique.tcl]
if {$mode eq "annote"} {
    source [file join $tp layout filebinary.tcl]
    source [file join $tp layout binaliases.tcl]
    source [file join $tp exemples cstruct cstruct.tcl]
    cs::init [file join $here cstruct.so]
} else {
    csc::init [file join $here cstruct.so]
}
set bytes [cs::generate $N 42]
set C [csc::decode $bytes]
set items [lindex $C 1]

# reps : répétitions pour 1000 mesures, ramenées à la taille du lot
proc mesure {label reps script} {
    set reps [expr {max(3, $reps * 1000 / $::N)}]
    uplevel 1 $script                     ;# échauffement (compilation, caches)
    set t [lindex [uplevel 1 [list time $script $reps]] 0]
    puts [format "%-34s %10.1f" $label $t]
}

# Chaque mesure tourne dans une procédure, comme du vrai code (variables compilées).
if {$mode eq "annote"} {
    set A [cs::make $N 42]
    proc p_build {n items} {set {Batch}b {n}$n {items}$items; return $b}
    proc p_encode {b} {type binary encode Batch $b -align natural}
    proc p_decode {bytes} {type binary decode Batch $bytes -align natural}
    proc p_stats {b} {cs::stats $b}
    proc p_statsobj {b} {cs::stats_typed $b}
    proc p_compute {b} {
        set sum 0.0; set cnt 0; set mn ""; set mx ""
        foreach it $b(items) {
            if {$it(flags) & 1} continue
            set v $it(value); set sum [expr {$sum + $v}]; incr cnt
            if {$mn eq "" || $v < $mn} {set mn $v}
            if {$mx eq "" || $v > $mx} {set mx $v}
        }
        list $cnt $mn $mx
    }
    proc p_compute_lassign {b} {
        set sum 0.0; set cnt 0; set mn ""; set mx ""
        foreach it $b(items) {
            lassign $it(*) id v fl
            if {$fl & 1} continue
            set sum [expr {$sum + $v}]; incr cnt
            if {$mn eq "" || $v < $mn} {set mn $v}
            if {$mx eq "" || $v > $mx} {set mx $v}
        }
        list $cnt $mn $mx
    }
    mesure "1 construction (set {Batch})"     200 {p_build $N $items}
    mesure "2 encodage (type binary encode)"  200 {p_encode $A}
    mesure "3 C stats_raw sur les octets"     2000 {cs::stats_raw $bytes}
    mesure "4 décodage (type binary decode)"  200 {p_decode $bytes}
    mesure "5 calcul Tcl, \$it(champ)"         200 {p_compute $A}
    mesure "5b calcul Tcl, lassign"           200 {p_compute_lassign $A}
    mesure "6 chaîne complète (cs::stats)"    200 {p_stats $A}
    mesure "7 voie objet (cs::stats_obj)"     200 {p_statsobj $A}
} else {
    proc p_build {n items} {list $n $items}
    proc p_check {n items} {csc::check [list $n $items]}
    mesure "1 construction nue (list)"        2000 {p_build $N $items}
    mesure "1b construction contrôlée"        200 {p_check $N $items}
    mesure "2 encodage (binary format)"       200 {csc::encode $C}
    mesure "3 C stats_raw sur les octets"     2000 {cs::stats_raw $bytes}
    mesure "4 décodage (binary scan)"         200 {csc::decode $bytes}
    mesure "5 calcul Tcl, lassign"            200 {csc::compute $C}
    mesure "6 chaîne complète nue"            200 {csc::stats $C}
    mesure "6b chaîne complète contrôlée"     200 {csc::stats_checked $C}
}
