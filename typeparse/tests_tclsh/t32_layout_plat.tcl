source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
type def mgc {"MAGI"}
type def hdr {
    {mgc}magic = MAGI; {int(u,8)}ver; {bitarray(16)}flags; {string(utf-8,6)}name; {list(3).int(u,16,be)}pts;
    {string(utf-8,z)}tag; {string(raw,2)}pad = "\x00\x00"; {int(s,32,le)}neg; {double(64,be)}d; {int(u,24)}t24; {int}calc = 7
}
puts [type binary hdr]
set bytes MAGI
append bytes \x02 \x80\x01 abc\x00\x00\x00 \x00\x01\x00\x02\x00\x03 hi\x00 \x00\x00 [binary format i -5] [binary format Q 1.5] \x01\x02\x03
set h [type binary decode hdr $bytes -consumed used]
puts "$h used=$used"
puts "[type get $h] $h(calc) $h(flags.15) $h(t24) $h(magic)"
puts [expr {[type binary encode hdr $h] eq $bytes}]
catch {type binary decode hdr [string range $bytes 0 9]} m; puts $m
catch {type binary decode hdr "XXXX[string range $bytes 4 end]"} m2; puts $m2
catch {type binary {sizeof?}hdr} m3; puts $m3
puts [type binary {sizeof?}{list(3).int(u,16)}]
set f [file tempfile tmp]
chan configure $f -translation binary
chan binary write $f hdr $h
close $f
set f [open $tmp rb]
set h2 [chan binary read $f hdr]
puts "from channel: $h2 at [tell $f]"
close $f
file delete $tmp
set v [type binary decode int(u,16,be) "\x01\x02"]
puts "$v [type get $v]"
