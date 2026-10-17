# -dedupe : une cible identique n'est écrite qu'une fois
# -dedupe: an identical target is written only once
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
proc hex {s} {binary scan $s H* h; return $h}

type def rec {{u8}a; {u8}b}
type def P {{*(//,u8,null).rec}x; {*(//,u8,null).rec}y; {*(//,u8,null).rec}z}
set v [set {P}v1 x {7 8} y {7 8} z {9 9}]

puts "--- sans -dedupe : trois cibles / without: three targets"
puts [hex [file binary encode P $v]]
puts "--- avec -dedupe : x et y partagent la cible / x and y share the target"
puts [hex [file binary encode P $v -dedupe 1]]
puts "--- relecture identique / reading gives the same value"
puts [file binary decode P [file binary encode P $v -dedupe 1]]
puts [expr {[file binary decode P [file binary encode P $v -dedupe 1]] eq [file binary decode P [file binary encode P $v]]}]

puts "--- une cible qui contient un lien n'est pas dédupliquée / a target holding a link is not deduped"
type def N {{u8}x; {*(//,u8,null).opt(N)}next}
type def Q {{*(//,u8,null).N}a; {*(//,u8,null).N}b}
set q [file binary decode Q "\x02\x02\x0a\x04\x0b\x00"]
puts $q
puts [hex [file binary encode Q $q]]
puts [hex [file binary encode Q $q -dedupe 1]]
puts [expr {[file binary decode Q [file binary encode Q $q -dedupe 1]] eq $q}]
puts "--- -dedupe 0 explicite / explicit -dedupe 0"
puts [hex [file binary encode P $v -dedupe 0]]
