# t104 : v16y. Signatures : une option peut être un lien ({var.T}-o) ; le
# défaut calculé d'une option absente voit les arguments positionnels.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
proc compter {{var.int}-compte; {list}l} { set compte [llength $l] }
proc compter2 {{var.int}-compte ?= nb; {list}l} { set compte [llength $l] }

puts "--- option lien"
t "donnée"                     {compter -compte k {a b}; set k}
t "absente : une locale"       {compter {a b c}}
t "absente : défaut nb"        {compter2 {a b c}; set nb}
t "contrôle à l'entrée"        {set s abc; compter -compte s {a}}
t "la variable n'a pas bougé"  {set s}
t "usage"                      {compter}
t "préfixe"                    {compter -c j {x}; set j}

puts "--- défaut calculé d'option"
proc fin {{list}l; {int}-fin ?= [expr {[llength $l] - 1}]} { set fin }
t "lit l"                      {fin {a b c}}
t "donnée"                     {fin -fin 7 {a b c}}
set ::cpt 0
proc num {{int}-n ?= [incr ::cpt]; {string}m} { list $n $m }
t "évalué à chaque appel"      {list [num a] [num b]}
t "pas évalué si donnée"       {list [num -n 9 c] $::cpt}
proc lit {{int}-a ?= 2; {int}-b ?= [expr {$a * 10}]; {int}x ?= [expr {$a + 1}]} { list $a $b $x }
t "un défaut lit une option"   {lit}
t "... donnée"                 {lit -a 5}
proc mauvais {{list}l; {int}-fin ?= [llength $nulle]} { set fin }
t "erreur du défaut"           {mauvais {a}}

puts "--- lien positionnel, transmis"
proc bar {{var.int}a; {var.int}b} { incr a; incr b }
proc foo {{var.int}a; {var.int}b} { bar a b }
t "TIP 460"                    {set v1 0; set v2 1; foo v1 v2; list $v1 $v2}
proc echanger {{var}a; {var}b} { lassign [list $b $a] a b }
t "échanger"                   {set x {1 2}; set y z; echanger x y; list $x $y}
