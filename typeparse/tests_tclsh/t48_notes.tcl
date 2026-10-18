# t48 : notes (nominaux en attente) portées par la valeur, à côté du type
# résolu. « type get » ne rend que le type résolu, « note get » les notes ;
# « show » les affiche ensemble, symétriques autour de « @ » : list<int>@mm.hello.
proc show {v} {
    set t [type get $v]; set n [note get $v]
    expr {$n eq "" ? $t : "$t@$n"}
}
proc attempt {name script} { puts "$name: [catch {uplevel 1 $script} m] $m" }

puts "-- l'exemple de CONCEPTION 6.3"
set {list.int}L 10 20 30 40
proc test {l} {
    switch [note get $l] {
        mm { return "mm [show $l]" }
        px { return "px [show $l]" }
        default { return "sans note" }
    }
}
puts [test {mm}$L]
puts [test {px}$L]
puts [test $L]
proc keep {l} { return $l }
set A [keep {mm}$L]
puts "A: [show $A]"
proc essai {v} { return "[show $v] / [note get $v]" }
puts "essai: [essai {hello}$A]"
puts "L reste: [show $L]"

puts "-- note has / delete"
attempt has_mm    {note has $A mm}
attempt has_px    {note has $A px}
set B [keep {hello}$A]
attempt B         {show $B}
attempt del_un    {show [note delete $B mm]}
attempt del_tout  {show [note delete $B]}
attempt del_rien  {show [note delete $L]}
attempt contenu   {note delete $B}

puts "-- valeur ordinaire, note seule"
set s abc
attempt note_seule {show [keep {mm}$s]}
attempt litterale  {show [keep {mm}5]}
attempt litt_note  {note get [keep {mm}5]}
attempt litt_type  {show [keep {int}5]}
attempt litt_notenote {note get [keep {int}5]}

puts "-- une variable non typée garde la note"
proc adopte {v} { set m $v; list [show $m] [note get $m] [string length $m] }
attempt adopte     {adopte {mm}$s}
attempt adopte_lit {adopte {mm}abc}

puts "-- une variable typée refuse un nom inconnu"
attempt typee      {set {string}t [keep {mm}$s]}

puts "-- {} retire le type, pas les notes"
attempt vide      {show [keep {}$A]}

puts "-- le cast garde les notes"
attempt cast      {show [keep {list.int}$A]}
set X [keep {mm}$s]
attempt X_string  {show [keep {string}$X]}

puts "-- type def donne un sens à la note"
type def mm2 int
set N [keep {mm2}5]
attempt N_type    {show $N}
set M [keep {mmdef}$s]
type def mmdef string
attempt apres_def {set {mmdef}z $M; show $z}

puts "-- lset garde la note"
attempt lset      {set l2 [keep {mm}$L]; lset l2 0 99; list [show $l2] $l2}

puts "-- copie : les notes suivent"
attempt copie     {set c $A; show $c}
attempt string    {show [string toupper $A]}

puts "-- erreurs"
attempt usage1    {note}
attempt usage2    {note frob $A}
attempt usage3    {note has $A}
attempt local     {show [keep {-v}$s]}

puts "-- type get ne rend que le type résolu"
attempt tg_note   {type get [keep {mm}$L]}
attempt tg_seule  {type get [keep {mm}$s]}
attempt tg_vide   {type get $s}
