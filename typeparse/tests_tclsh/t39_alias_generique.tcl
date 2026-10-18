# Alias générique : type alias {p32({type}T)} {corps}
# Generic alias: the instance is the body type itself (structural)
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
type alias {p32({type}T)} {*(//,u32le).$T}
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {p32(rec)}r}
puts [type binary {p32(rec)}]
puts [type binary decode H "\x01\x05\x00\x00\x00\x07\x08"]
puts [type binary decode H "\x01\x00\x00\x00\x00"]
# deux instances identiques sont le même type / same instances are the same type
type def X {{p32(rec)}r}
type def Y {{*(//,u32le).rec}r}
puts [type binary decode X "\x04\x00\x00\x00\x07\x08"]
puts [type binary decode Y "\x04\x00\x00\x00\x07\x08"]
attempt {type alias {q32({type}T,{type}U)} {*(//,u32le).$T}}
