# Les fonctions de conversion du noyau passent par TclAnnotUnwrap : une valeur
# annotée garde son type quand une commande la lit (expr, string, format, if,
# incr, dict...) et les conversions inadmissibles sont refusées.
set nfail 0
proc keeps {name script setup} {
    global nfail
    catch {uplevel #0 {unset ::V}}
    uplevel #0 $setup
    catch {uplevel #0 $script}
    if {[catch {uplevel #0 {set ::V ##invalide##}}]} {
        puts "ok: $name garde son type"
    } else {
        puts "ECHEC: $name perd son type"; incr nfail
    }
}
foreach op {
    {expr {$::V + 1}} {expr {$::V < 10}} {incr ::V} {string length $::V}
    {string index $::V 0} {if {$::V} {}} {format %d $::V}
    {string map {5 6} $::V} {string is integer $::V} {puts -nonewline ""}
} {
    keeps $op $op {set {int}::V 5}
}
foreach op {
    {dict get $::V a} {dict exists $::V a} {dict size $::V} {dict keys $::V}
    {dict for {k v} $::V {}} {dict set ::V a 3} {dict incr ::V a}
    {dict unset ::V b} {dict update ::V a x {set x 9}} {dict with ::V {}}
    {dict map {k v} $::V {set v}} {dict merge $::V {}} {dict append ::V a 1}
} {
    keeps $op $op {set {dict(a,b?).int}::V a 1 b 2}
}
proc bad {script} {
    global nfail
    if {![catch {uplevel #0 $script} r]} { puts "ECHEC (pas d'erreur): $script"; incr nfail; return }
    puts "ok: $script -> $r"
}
proc good {script expected} {
    global nfail
    set r [uplevel #0 $script]
    if {$r ne $expected} { puts "ECHEC: $script -> '$r' (attendu '$expected')"; incr nfail } else { puts "ok: $script" }
}
set {int}i 5
set {double}d 2.5
set {list.int}L 1 2 3
set {dict(a,b).int}D a 1 b 2
good {expr {$i + 1}} 6
good {expr {$i * $d}} 12.5
good {incr i; set i} 6
bad  {incr d}
bad  {expr {$L + 1}}
bad  {if {$L} {}}
bad  {dict get $L 1}
bad  {dict get $i a}
bad  {dict set D a zz}
bad  {dict set D c 3}
good {dict set D a 7; set D} {a 7 b 2}
good {dict incr D a; set D} {a 8 b 2}
bad  {set i abc}
puts [expr {$nfail == 0 ? "tout est ok" : "$nfail échec(s)"}]
