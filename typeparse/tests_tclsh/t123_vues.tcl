# t123 : vues à pas (tranches et colonnes de listes en cases, sans copie),
# type map -min / -max, écriture d'une tranche en cases dans des cases.
type def {-compact}vd list.double
type def pt3 {{double}a; {double}b; {double}c}
type def {-compact}pts list.pt3
proc t {} {
  set {vd}L 0 1 2 3 4 5 6 7 8 9
  set A $L(1::2)
  puts "vue : $A [type get $A] llength [llength $A] lindex [lindex $A 2] [type canonic [lindex $A 2]]"
  set {list.double}B $A
  set B(0) 99
  puts "écriture dans une copie : B=$B A=$A L=$L"
  set C $A
  puts "tranche de vue : [set {list.double}D $A; set D(::2)]"
  puts "foreach : [set s 0; foreach x $A {set s [expr {$s+$x}]}; set s]"
  puts "lsort : [lsort -real -decreasing $A]"
  puts "map : [type map {x} $A {($x * 10)}] somme [type map -sum {x} $A {($x)}]"
  set {pts}P [type cast pts {{1 2 3} {4 5 6} {7 8 9}}]
  set col $P(*.b)
  puts "colonne : $col [type get $col]"
  set P(1.b) 50
  puts "après écriture dans P : col=$col P=$P"
  puts "string : [string length $A] eq [expr {$A eq {1.0 3.0 5.0 7.0 9.0}}]"
  puts "cast : [type cast vd $A] [type is {compact?}[type cast vd $A]]"
  unset L
  puts "base libérée, vue : $A"
}
t
proc t2 {} {
  foreach {l} {{3 1 2} {1.5 2 -0.5} {2 2.0} {2.0 2} {} {a b} {9223372036854775807 9223372036854775806}} {
    foreach f {min max} {
      if {[catch {type map -$f {x} $l {($x)}} r]} {set r "ERR $r"}
      if {[catch {::tcl::mathfunc::$f {*}$l} r2]} {set r2 "ERR $r2"}
      puts "$f [list $l] -> $r / $r2"
    }
  }
}
t2
proc t3 {} {
  set {vd}L 0 1 2 3 4 5
  set L(::2) [type map {x} $L(1::2) {($x * 10)}]
  puts "cases dans cases : $L [type is {compact?}$L]"
}
t3
