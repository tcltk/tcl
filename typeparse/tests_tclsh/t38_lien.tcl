# Le type lien *(origine,repr).T : un entier qui désigne une valeur du fichier
# The link type *(origin,repr).T: an integer that designates a value in the file
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# unit et opt(T) viennent de binaliases.tcl / unit and opt(T) come from binaliases.tcl
type def rec {{u8}a; {u8}b}

# 1. //  : offset depuis le début des données / from the start of the data
type def H {{u8}magic; {*(//,u32le).rec}r}
puts "taille en place: [file binary sizeof H]"
set d "\x07\x08\x00\x00\x00\x00\x00\x00\x01\x02"
set v [file binary decode H $d]
puts "$v $v(r.b)"
puts [hex [file binary encode H $v]]
unset v

# 2. ./  : depuis le début de la structure englobante / from the start of the enclosing structure
type def T {{u8}n; {*(./,u8).rec}x; {*(./,u8).rec}y}
set v [file binary decode T "\x09\x04\x06\x00\x00\x01\x02\x03\x04"]
puts $v
unset v
puts [hex [file binary encode T [set {T}t n 9 x {5 6} y {7 8}]]]

# 3. .  : depuis le champ lui-même / from the field itself
type def U {{u8}pad; {*(.,u8).rec}x}
set v [file binary decode U "\x00\x02\x00\x07\x08"]
puts $v
unset v

# 4. ../ : depuis la structure au-dessus / from the structure above
type def inner {{u8}k; {*(../,u8).rec}z}
type def outer {{u8}pad; {inner}i}
set v [file binary decode outer "\x00\x00\x05\x00\x00\x0a\x0b"]
puts $v
unset v
attempt {file binary decode inner "\x00\x00" }
attempt {type def top {{*(../,u8).rec}z}; file binary decode top "\x00\x00\x00"}

# 5. liste de liens, aller-retour / list of links, round trip
type def L {{u8}n; {list($n).*(//,u8).rec}rs}
set b "\x02\x03\x05\x01\x02\x03\x04"
set v [file binary decode L $b]
puts $v
puts [expr {[file binary encode L $v] eq $b}]
unset v

# 6. pointeur vers pointeur / pointer to pointer
type def PP {{*(//,u8).*(//,u8).rec}p}
set b "\x01\x03\x00\x05\x06"
set v [file binary decode PP $b]
puts $v
puts [hex [file binary encode PP $v]]
unset v

# 7. présence : opt, lien nul / presence: opt, null link
type def E {{u8}n; {*(//,u32le,null).opt(rec)}r}
foreach b {"\x07\x05\x00\x00\x00\x01\x02" "\x07\x00\x00\x00\x00"} {
    set v [file binary decode E $b]
    puts "$v [type get $v(r)]"
    puts [hex [file binary encode E $v]]
    unset v
}
attempt {file binary decode {*(//,u32le).opt(rec)} "\x00\x00\x00\x00"}

# 8. lien nul et champ à défaut / null link and field with a default
type def D {{u8}n; {*(//,u8,null).rec}r = {9 9}}
puts [file binary decode D "\x01\x00"]
puts [file binary decode D "\x01\x02\x03\x04"]
type def D2 {{u8}n; {*(//,u8,null).rec}r}
attempt {file binary decode D2 "\x01\x00"}

# 9. adresse signée : une cible AVANT le champ / signed address: a target BEFORE the field
type def S {{u8}k; {u8}j; {*(.,i8).rec}r}
puts [file binary decode S "\x01\x02\xfe\x0a\x0b"]
attempt {file binary decode S "\x01\x02\xf0"}
attempt {file binary decode S "\x01\x02\x7f"}

# 10. erreurs de définition / definition errors
attempt {type binary {*(//,string).rec}}
attempt {type binary {*(?,u8).rec}}
attempt {type binary {*(//).rec}}
attempt {type binary {*(//,u8)}}
attempt {type binary {*(//,u8,opaque).rec}}
attempt {type binary {*(memory://,u64le).rec}}
attempt {type binary {*(//,u8,blabla).rec}}

# 11. la cible cite un champ qui SUIT l'adresse (ELF : e_shoff avant e_shnum)
# the target cites a field that FOLLOWS the address (ELF: e_shoff before e_shnum)
type def sec {{u8}id; {u8}sz}
type def EH {{u8}tag; {*(//,u8,null).opt(list($n).sec)}sections; {u8}n}
set b "\x01\x04\x02\x0a\x01\x0b\x02\x0c"
set v [file binary decode EH $b]
puts "$v"
puts [hex [file binary encode EH $v]]
unset v
set v [file binary decode EH "\x01\x00\x00"]
puts "$v [type get $v(sections)]"
puts [hex [file binary encode EH $v]]
