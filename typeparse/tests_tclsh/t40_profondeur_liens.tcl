# Profondeur de suivi des liens : -linkdepth 0, 1, N, Inf ; file binary deref
# How deep to follow links: -linkdepth 0, 1, N, Inf; file binary deref
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# --- un en-tête et un enregistrement / a header and a record ---
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {ptr32(rec)}r}
set d "\x01\x05\x00\x00\x00\x07\x08"

puts "--- Inf (défaut) / Inf (default)"
puts [file binary decode H $d]
puts [file binary decode H $d -linkdepth Inf]
puts "--- 0 : le lien reste une adresse annotée / the link stays an annotated address"
set h0 [file binary decode H $d -linkdepth 0]
puts "$h0 | m=$h0(m) r=$h0(r) | [type get $h0(r)]"
puts "--- 1"
puts [file binary decode H $d -linkdepth 1]
puts "--- deref : un pas / one step"
puts [file binary deref $h0(r) $d]

puts "--- réécriture : l'adresse de frontière est écrite telle quelle"
puts [hex [file binary encode H $h0]]
set h1 $h0
set h1(m) 9
puts [hex [file binary encode H $h1]]

puts "--- un calcul perd l'annotation / arithmetic drops the annotation"
attempt {file binary deref [expr {$h0(r) + 0}] $d}
puts "--- erreurs / errors"
attempt {file binary decode H $d -linkdepth -2}
attempt {file binary decode H $d -linkdepth abc}
attempt {file binary deref 5 $d}

# --- une liste chaînée (type récursif) / a linked list (recursive type) ---
type def N {{u8}x; {*(//,u8,null).opt(N)}next}
puts "--- liste chaînée / linked list"
set l "\x0a\x02\x0b\x04\x0c\x00"
puts [file binary decode N $l]
puts [file binary decode N $l -linkdepth 0]
puts [file binary decode N $l -linkdepth 1]
puts "sizeof N = [file binary sizeof N]"
set cur [file binary decode N $l -linkdepth 0]
set k 0
while 1 {
    puts "cellule $k : x=$cur(x)"
    unset -nocomplain nx
    set nx $cur(next)
    if {[string index [type get $nx] 0] ne "&"} break
    set r [file binary deref $nx $l]
    unset cur
    set cur $r(some)
    incr k
}
puts "--- un cycle / a cycle"
set c "\x0a\x02\x0b\x02"
attempt {file binary decode N $c}
puts [file binary decode N $c -linkdepth 3]
