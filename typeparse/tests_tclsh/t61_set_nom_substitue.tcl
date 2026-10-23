# t61 : set à un seul argument : un nom substitué qui porte un type ou des notes
#   ($r lien var<int>, {mm}n) LIT ; seul un nom annoté dans le source construit.
set {int}A 5
set {list.var.int}L A
set r [lindex $L 0]
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-36s %s %s <%s>" $l $rc $m [catch {type get $m} tt; set tt]] }
t "r"                     {set r}
t "type get \$r"          {type get $r}
t "set \$r"               {set $r}
t "set \$r (proc)"        {apply {{r} {set $r}} $r}
t "set \$r (non compilé)" {eval [list set $r]}
set {int}n 7
type def nom string
set nm {nom}n
set nm2 {mm}n
t "set {int}x (sans valeur)" {set {int}x}
t "set \$nm (nom annoté en var)" {set $nm}
t "set \$nm2 (note mm)" {set $nm2}
type def pt {{int}x = 1; {int}y = 2}
t "set {pt}p (défauts, littéral)" {set {pt}p}
t "set {int}z (littéral, sans défaut)" {set {int}z}
t "set \$nm 9 (écrit)"              {set $nm 9; list $n [type get $n]}
t "set \$r 6 (écrit via lien)"      {set $r 6; set A}
t "lappend {list.int}M littéral"    {set M {1 2}; lappend {list.int}M 3; type get $M}
