# t58 : incr, append, lappend, dict set sur une partie (chemin) d'une valeur typée
type def pt {{string}nom ?= "?"; {int}age ?= 0; {list.int}notes ?= {}; {dict}d ?= {}}
proc t {label script} { set r [catch {uplevel 1 $script} m]; puts [format "%-44s %s %s" $label $r $m] }
set {pt}c Jean 12 {1 2} {}
t "résultat incr"            {incr c(age)}
t "résultat append"          {append c(nom) " " Dupont}
t "résultat lappend 2 val"   {lappend c(notes) 3 4}
t "contrat lappend x"        {lappend c(notes) x}
t "contrat incr nom"         {incr c(nom)}
t "incr chemin imbriqué"     {incr c(notes.0) 10}
t "dict set c(d) k 1"        {dict set c(d) k 1}
t "dict incr c(d) n"         {dict incr c(d) n}
t "valeur"                   {set c}
t "type"                     {type get $c}
t "liste directe (non compilé)" {eval [list incr c(age) 100]}
t "liste directe append"     {eval [list append c(nom) !]}
t "liste directe lappend"    {eval [list lappend c(notes) 9]}
set n c(age); t "nom calculé incr \$n"  {incr $n}
proc p {v} {incr v(age); lappend v(notes) 5; append v(nom) ?; return $v}
t "proc arguments"           {p $c}
t "c inchangé par p"         {set c}
set {pt}e(nom) Ana
t "lappend sur défaut vide"  {lappend e(notes) 1}
t "variable non typée"       {set z 1; incr z(a)}
array set arr {a 1}; t "vrai tableau incr" {incr arr(a)}
t "tableau append" {append arr(b) x}
