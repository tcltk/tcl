# t57 : somme choisie par un champ, S($champ) : la variante vient du sélecteur,
#   même quand plusieurs variantes ont la même forme ({u8}, {u8; u8}).
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
type def c_mode {{u8}broche; {u8}mode}
type def c_lire {{u8}broche}
type def c_pwm  {{u8}broche; {u8}rapport}
type def corps {{c_mode}1, {c_lire}3, {c_pwm}4}
type def req {{u8}genre; {corps($genre)}corps}
foreach octets [list "\x04\x09\x80" "\x01\x09\x01" "\x03\x0e"] {
    set r [type binary decode req $octets]
    puts "décodé : $r -> [type get $r(corps)]"
}
set r [type binary decode req "\x04\x09\x80"]
puts "chemin : broche $r(corps.4.broche) rapport $r(corps.4.rapport)"
set {req}a 4 {9 128};            puts "construit : [type get $a(corps)]"
set {req}b(corps,genre) {9 1} 1; puts "nom/valeur, sélecteur après : [type get $b(corps)]"
set a(corps.4.rapport) 60;       puts "écriture par chemin : $a"
attempt {set a(genre) 3}
set a(genre) 1;                  puts "sélecteur changé : [type get $a(corps)]"
binary scan [type binary encode req $a] cu* o; puts "encodé : $o"
set {c_pwm}p 9 200
attempt {set {req}c 1 $p}
attempt {set {req}d 3 {4}{9 1}}
attempt {type binary decode req "\x09\x01"}
