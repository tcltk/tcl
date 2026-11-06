# Somme dont la variante est CHOISIE : S(nom) et paramètre de type S($p)
# Sum whose variant is CHOSEN: S(name), and S($p) with a type parameter
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# deux variantes de MÊME forme : rien à deviner / two variants of the SAME shape
type def pa {{u8}a; {u8}b}
type def pb {{u8}c; {u8}d}
type def S {{pa}x, {pb}y}

# 1. S(nom) directement / directly
type def Q {{S(y)}s}
set q [type binary decode Q "\x01\x02"]
puts "$q [type get $q(s)] $q(s.y.d)"
puts [hex [type binary encode Q $q]]
puts [type binary {sizeof?}Q]
attempt {type binary decode Q "\x01"}

# 2. par paramètre de valeur / by value parameter
type def {R({string}v)} {{u8}k; {S($v)}s}
set rx [type binary decode {R(x)} "\x07\x01\x02"]
puts "x: $rx [type binary {sizeof?}{R(x)}] [type get $rx(s)]"
puts [hex [type binary encode {R(x)} $rx]]
set ry [type binary decode {R(y)} "\x07\x01\x02"]
puts "y: $ry [type binary {sizeof?}{R(y)}] [type get $ry(s)]"
puts [hex [type binary encode {R(y)} $ry]]
attempt {type binary decode {R(zz)} "\x07\x01\x02"}

# 3. variante qui ne va pas / wrong variant
attempt {type binary encode {R(y)} $rx}

# 4. sélecteur par champ inchangé / field selector unchanged
type def F {{u8}k; {S($k)}s}
attempt {type binary {sizeof?}F}
