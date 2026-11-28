# t142 (v18g) : type match T VALEUR -> auto | cast | mismatch.
#   auto     : set {T}x $v, un argument {T}x l'acceptent tels quels
#   cast     : seul type cast T $v l'accepte
#   mismatch : ni l'un ni l'autre
type def mm int; type def kg int; type def m double
type convert mm m {{{mm}x} {expr {$x/1000.0}}}
type def even int(($%2==0))
type def temp {{double}celsius, {double}fahrenheit}
type convert temp/celsius temp/fahrenheit {{x} {expr {$x*9/5.0+32}}}
type def pt {{double}x; {double}y}
set {mm}a 10; set {kg}c 3; set {int(u,8)}u 7; set {int}i 4; set {double}d 2.5
set {temp/celsius}t 100.0; set {temp}t2 {/fahrenheit}50.0; set {pt}p {1.0 2.0}
set {list.mm}lm 1 2; set {m}lg 1.5

puts "== cas types"
foreach {T v} [list mm $a mm 3 mm abc mm $c m $a double $u int $u int(u,8) 300 \
	int(u,8) $a int $d int 2.5 even 4 even 3 even $i even $a temp $t \
	temp/fahrenheit $t temp/celsius $t temp/celsius $t2 list.int {1 2} \
	list.int {1 x} list.int $lm string $a pt $p pt {1 2} pt $lm] {
    puts [format "%-16s %-10s %-16s -> %s" $T $v [type get $v] [type match $T $v]]
}

# la réponse ne contredit jamais un argument {T}x ni type cast
puts "== accord avec l'argument et type cast"
set types {mm kg m double int int(u,8) even string temp temp/celsius
	   temp/fahrenheit list.int list.mm pt}
set vals [list $a $c $u $i $d $t $t2 $p $lm $lg 3 4 abc 2.5 300 {1 2} {1 x} {1.0 2.0} "" -1]
set n 0; set bad 0
foreach T $types { foreach v $vals {
    incr n
    set arg [expr {![catch {apply [list "{$T}x" {}] $v}]}]
    set cast [expr {![catch {type cast $T $v}]}]
    set ref [expr {$arg ? "auto" : ($cast ? "cast" : "mismatch")}]
    set w [type match $T $v]
    if {$w ne $ref} { incr bad; puts "écart : $T <$v:[type get $v]> match=$w attendu=$ref" }
}}
puts "$n cas, $bad écarts"

puts "== prédicat : argument et cast suivent set (v18g)"
proc f {{even}x} { type get $x }
puts "f int : [f $i]  f mm : [f $a]"
puts "cast even mm : [type get [type cast even $a]]"
puts [catch {f 3} r]:$r

puts "== l'exemple d'Ashok"
proc ajoute {nom v} {
    upvar 1 $nom var
    switch [type match [type get $var] $v] {
	auto     { incr var $v }
	cast     { incr var [type cast [type get $var] $v] }
	mismatch { error "type mismatch: [type get $var] += [type get $v]" }
    }
}
set {mm}b 10
puts [ajoute b 5]
puts [catch {ajoute b $c} r]:$r

puts "== erreurs"
puts [catch {type match nosuch 1} r]:$r
puts [catch {type match mm} r]:$r
puts "la valeur reste : $a [type get $a], résultat : [type match m $a]"
