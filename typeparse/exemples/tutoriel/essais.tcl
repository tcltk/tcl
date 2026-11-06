# essais.tcl -- les mêmes appels pour les trois étapes
#
#   tclsh essais.tcl 1|2|3 ?COMPILATEUR?
#
# Les résultats sont les mêmes aux trois étapes ; seuls les messages
# d'erreur de l'étape 1 diffèrent (essais_1.out ; étapes 2 et 3 : essais_2.out).

set here [file dirname [file normalize [info script]]]
lassign $argv etape cc
if {$cc ne ""} {set ::env(CPROC_CC) $cc}
source [file join $here [dict get {1 etape1_tcl.tcl 2 etape2_type.tcl 3 etape3_c.tcl} $etape]]

proc t {label script} {
    if {[catch {uplevel #0 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-24s %s" $label $r]
}

set carre {{0.0 0.0} {4.0 0.0} {4.0 4.0} {0.0 4.0}}
set tri   {{0.0 0.0} {6.0 0.0} {0.0 3.0}}

t "point"               {poly::point 1.5 2.0}
t "aire carré"          {poly::aire $carre}
t "aire triangle"       {poly::aire $tri}
t "périmètre"           {poly::perimetre $carre}
t "périmètre -ouvert"   {poly::perimetre -ouvert $carre}
t "contient (2,2)"      {poly::contient $carre {2.0 2.0}}
t "contient (5,1)"      {poly::contient $carre {5.0 1.0}}
t "déplace"             {poly::deplace $tri 1.0 0.5}
t "déplace, dy défaut"  {poly::deplace $tri 1.0}
t "boîte"               {poly::boite $tri}
t "aire après déplace"  {poly::aire [poly::deplace $carre 10.0 -3.0]}
# un polygone régulier de 1000 sommets, de rayon 1 : aire proche de pi
set n 1000
set grand [lmap i [lseq $n] {list [expr {cos(2*3.141592653589793*$i/$n)}] [expr {sin(2*3.141592653589793*$i/$n)}]}]
t "aire, 1000 sommets"  {format %.6f [poly::aire $grand]}
t "périm., 1000 sommets" {format %.6f [poly::perimetre $grand]}

puts "--- erreurs (différentes selon l'étape)"
t "point non numérique"  {poly::aire {{0.0 0.0} {x 1.0} {1.0 1.0}}}
t "point à 3 valeurs"    {poly::aire {{0.0 0.0 0.0} {1.0 0.0} {1.0 1.0}}}
t "option inconnue"      {poly::perimetre -ferme $carre}
t "argument manquant"    {poly::deplace $carre}
