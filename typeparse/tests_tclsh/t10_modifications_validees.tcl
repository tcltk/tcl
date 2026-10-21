# lappend, append, lset, lreplace sur une variable typée : le résultat est
# revalidé et la variable garde son type ; en cas d'erreur, rien ne change.
proc bad {script} {
    if {![catch {uplevel 1 $script} r]} { puts "ECHEC (pas d'erreur): $script"; return }
    puts "ok: $script -> $r"
}
proc good {script expected} {
    set r [uplevel 1 $script]
    if {$r ne $expected} { puts "ECHEC: $script -> '$r' (attendu '$expected')" } else { puts "ok: $script" }
}
set {list.int}M 4 5 6
bad {lappend M x}
bad {lappend M 1 x}
bad {append M " y"}
bad {lset M 0 z}
bad {lset M 0 {}}
good {set M} {4 5 6}
good {lappend M 7; set M} {4 5 6 7}
good {lset M 0 10; set M} {10 5 6 7}
good {lset M end 11; set M} {10 5 6 11}
bad {set M {1 x}}
set {list.list.int}N {1 2} {3 4}
good {lset N 0 1 9; set N} {{1 9} {3 4}}
bad {lset N 0 1 z}
good {lappend N {5 6}; set N} {{1 9} {3 4} {5 6}}
bad {lappend N {a}}
set {int}i 5
bad {lappend i 1}
good {lset i 0 7; set i} 7
type def point {{int}x; {int}y}
set {point}P 1 2
good {set P(x) 9; set P} {9 2}
bad {set P(x) x}
