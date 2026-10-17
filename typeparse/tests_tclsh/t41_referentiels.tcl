# Référentiels nommés : //nom/, memory://nom/, file binary space
# Named spaces: //name/, memory://name/, file binary space
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

type def rec {{u8}a; {u8}b}

puts "--- création : valeur annotée de type space, enregistrée / creation: registered space value"
set sp [file binary space file "\x00\x00\x07\x08" -name lib]
puts "[type get $sp] $sp(name) $sp(base) [file binary space list]"
set sa [file binary space file "xy"]
puts "$sa(name) [file binary space list]"
file binary space delete $sa(name)
attempt {file binary space file "z" -name lib}
attempt {file binary space delete nope}

puts "--- lien nommé / named link"
type def H {{u8}m; {*(//lib/,u8,null).rec}r}
puts [file binary decode H "\x01\x02"]
set h0 [file binary decode H "\x01\x02" -linkdepth 0]
puts "$h0 [type get $h0(r)]"
puts "--- deref sans DATA / without DATA"
puts [file binary deref $h0(r)]
puts "--- réécriture de l'adresse / rewrite the address"
puts [hex [file binary encode H $h0]]
puts "--- écrire une vraie cible : erreur / writing a real target: error"
type def H2 {{u8}m; {*(//lib/,u8,null).rec}r}
attempt {file binary encode H2 [set {H2}v2 m 1 r {7 8}]}

puts "--- memory:// : adresses virtuelles et segments / virtual addresses and segments"
file binary space memory "ABCD\x07\x08" {{4096 4 2}} -name mem
type def M {{*(memory://mem/,u32le,null).rec}p}
puts [file binary decode M "\x00\x10\x00\x00"]
attempt {file binary decode M "\x00\x20\x00\x00"}
type def Mx {{*(//mem/,u32le,null).rec}p}
attempt {file binary decode Mx "\x00\x10\x00\x00"}

puts "--- référentiel inconnu / unknown space"
type def Z {{*(//zzz/,u8,null).rec}p}
attempt {file binary decode Z "\x01"}
puts "--- lien anonyme sans DATA / anonymous link without DATA"
type def A {{*(//,u8,null).rec}p}
set a0 [file binary decode A "\x01\x07\x08" -linkdepth 0]
attempt {file binary deref $a0(p)}
puts [file binary deref $a0(p) "\x01\x07\x08"]
