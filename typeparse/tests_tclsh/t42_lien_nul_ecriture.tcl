# Écriture d'un lien nul : un champ qui vaut son défaut s'écrit comme lien nul
# Writing a null link: a field holding its default is written as a null link
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
proc hex {s} {binary scan $s H* h; return $h}
proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}

type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u8,null).rec}r = {0 0}}

puts "--- lecture : adresse nulle -> défaut / read: null address -> default"
puts [type binary decode H "\x01\x00"]
puts "--- écriture : défaut -> adresse nulle / write: default -> null address"
puts [hex [type binary encode H [type binary decode H "\x01\x00"]]]
puts [hex [type binary encode H [set {H}a m 3]]]
puts "--- écriture : autre valeur -> cible écrite / other value -> target written"
puts [hex [type binary encode H [set {H}b m 2 r {7 8}]]]
puts "--- aller-retour / round trip"
puts [type binary decode H [type binary encode H [set {H}c m 2 r {7 8}]]]
puts "--- sans défaut : lecture d'un lien nul = erreur / without default: reading a null link is an error"
type def G {{u8}m; {*(//,u8,null).rec}r}
attempt {type binary decode G "\x01\x00"}
