# Une liste dont la taille dépend d'un autre champ est vérifiée dès la construction
# A list whose size depends on another field is checked as soon as it is built
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}

type def Sample {{u32le}id; {f64le}value; {u8}flags}
type def Batch {{u32le}n; {list($n).Sample}items}

puts "--- cohérent / consistent"
puts [set {Batch}a n 2 items {{1 2.5 0} {2 3.5 1}}]
puts [set {Batch}e n 0 items {}]
puts "--- incohérent : refusé à la construction / inconsistent: refused when built"
attempt {set {Batch}b n 3 items {{1 1.0 0}}}
attempt {set {Batch}c n 1 items {{1 1.0 0} {2 2.0 0}}}
puts "--- forme positionnelle / positional form"
attempt {set {Batch}d 2 {{1 1.0 0}}}
puts [set {Batch}d2 2 {{1 1.0 0} {2 2.0 1}}]
puts "--- expression : n - 1 éléments / expression: n - 1 elements"
type def Chain {{u8}n; {list($n - 1).u8}rest}
puts [set {Chain}ok n 3 rest {7 8}]
attempt {set {Chain}bad n 3 rest {7 8 9}}
puts "--- champs omis : le défaut compte / omitted fields: the default counts"
type def Pair {{u8}n = 2; {list($n).u8}xs = {1 2}}
puts [set {Pair}p1]
attempt {set {Pair}p2 n 3}
puts "--- copie et lecture binaire / copy and binary read"
puts [type binary decode Batch [type binary encode Batch $a -align natural] -align natural]
puts "--- la modification champ par champ n'est pas contrôlée (état intermédiaire légitime) mais l'encodage la vérifie"
set a(n) 3
attempt {type binary encode Batch $a -align natural}
