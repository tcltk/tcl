# Présence dynamique : variante sans valeur ({unit}) et sélecteur-expression S($expr)
# Dynamic presence: valueless variant ({unit}) and selector expression S($expr)
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# le type sans valeur (binaliases.tcl) : un littéral vide, 0 octet
# the valueless type (binaliases.tcl): the empty literal, 0 bytes
puts [type binary unit]
type def opt01 {{u8}1, {unit}0}

# 1. sélection par un champ 0/1 / by a 0/1 field
type def A {{u8}present; {opt01($present)}x}
foreach b {"\x01\x07" "\x00"} {
    set v [type binary decode A $b]
    puts "$v [type get $v(x)]"
    unset v
}
puts [hex [type binary encode A [type binary decode A "\x01\x07"]]]
puts [hex [type binary encode A [type binary decode A "\x00"]]]

# 2. sélecteur-expression / selector expression
type def B {{u8}off; {opt01($off != 0)}x}
puts [type binary decode B "\x05\x09"]
puts [type binary decode B "\x00"]
type def C {{u8}f; {u8}g; {opt01($f + $g > 1)}x}
puts [type binary decode C "\x01\x01\x09"]
puts [type binary decode C "\x00\x01"]
attempt {type binary decode C "\x01\x01"}

# 3. incohérence à l'écriture / mismatch on write
attempt {type binary encode A [set {A}bad(present,x) 0 {1}9]}
attempt {type binary encode A [set {A}bad2(present,x) 1 {0}{}]}

# 4. champ cité non encore lu / field not yet read
attempt {type def D {{opt01($zz != 0)}x; {u8}zz}; type binary decode D "\x01"}
