# t103 : v16y. type debug : les notes d'instrument ({@trace}, {@step}) et
# l'inventaire des proto-types (notes dont le nom n'est pas un type).
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def pt {{double}x; {double}y}
proc {double}norme {{pt}p} {
    set x2 [expr {$p(x) ** 2}]
    return [expr {sqrt($x2 + [lindex $p 1] ** 2)}]
}
proc chemin {L} {
    set s 0.0
    foreach p $L { set s [expr {$s + [norme $p]}] }
    return $s
}

puts "--- éteint par défaut"
t "état"                         {type debug}
t "une note @ n'est pas un type" {set q {@trace}{3.0 4.0}; list $q [type get $q]}
t "rien ne s'arrête"             {norme $q}
t "type def @x"                  {type def @x int}

puts "--- la trace, par un crochet"
set ::log {}
proc rec {note words index} { lappend ::log [list $note [lindex $words 0] $index [lindex $words $index]] }
type debug hook rec
t "crochet"                      {type debug hook}
type debug on
t "état"                         {type debug}
set P {@step}{3.0 4.0}
norme $P
chemin [list $P {1.0 0.0}]
type debug off
foreach e $::log { puts "  $e" }
t "éteint : rien"                {set ::log {}; norme $P; set ::log}

puts "--- une erreur du crochet arrête la commande"
proc stop {note words index} { return -code error "arrêt sur [lindex $words 0]" }
type debug hook stop
type debug on
t "arrêt"                        {norme $P}
t "sans note : rien"             {norme {3.0 4.0}}
type debug off

puts "--- pas à pas : type debug next"
set ::log {}
proc pas {note words index} {
    lappend ::log [list $note [lindex $words 0]]
    if {[llength $::log] < 4} { type debug next }
}
type debug hook pas
type debug on
norme $P
type debug off
foreach e $::log { puts "  $e" }

puts "--- le crochet par défaut (@trace : sans arrêt)"
type debug hook ::tcl::typedebug::hit
type debug on
set m {@trace}{3.0 4.0}
chemin [list $m]
type debug off

puts "--- conditions : type debug when"
type debug on
t "when x > 5"                   {type debug when trace {$value > 5}}
t "when @deux (hits)"            {type debug when @deux {$hits == 2}}
t "les conditions"               {type debug when}
t "une"                          {type debug when deux}
t "syntaxe"                      {type debug when z {$value <}}
foreach v {3 8 4 9} { set x {@trace}$v }
set ::tcl::typedebug::run 1   ;# comme « r » : s'affiche sans s'arrêter
t "@deux, 1er passage"           {set y {@deux}a}
t "@deux, 2e passage"            {set y {@deux}b}
t "@deux, 3e passage"            {set y {@deux}c}
type debug when trace {}
t "retirée"                      {type debug when}
type debug when deux {}
type debug off

puts "--- proto-types"
type debug clear
type debug hook rec
type debug on
set a {pointt}{3 4}
norme $a
set v {vitesse}12.5
set w {vitesse}abc
set t {angle}0.5
type debug off
t "notes"                        {lsort [dict keys [type debug notes]]}
t "vitesse"                      {dict get [type debug notes] vitesse}
puts [type debug report]
type def vitesse double
type def angle {{double}rad; {double}deg}
puts "-- après définition"
puts [type debug report]
t "statut"                       {lmap n {angle pointt vitesse} {dict get [type debug notes] $n status}}
type debug clear
t "vidé"                         {type debug report}

puts "--- erreurs"
t "sous-commande"                {type debug zz}
t "variable {@trace}x"           {set {@trace}x 1}
t "type cast @trace"             {type cast @trace 1}
