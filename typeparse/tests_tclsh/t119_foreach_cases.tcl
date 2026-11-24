# t119 : foreach sur une liste en cases : l'élément écrit dans la valeur que
# la variable est seule à tenir ; une valeur gardée par le corps reste intacte.
type def {-compact}octets list.int(u,8)
type def {-compact}flottants list.double
proc t {} {
  set o [type cast octets {1 2 3 4 5}]
  set vus {}; foreach x $o {lappend vus $x}; puts "lappend : $vus"
  set d {}; foreach x $o {dict set d k$x $x}; puts "dict : $d"
  foreach x $o {set y $x}; puts "y=$y x=$x [type meta $x]"
  set r [lmap x $o {expr {$x*2}}]; puts "lmap : $r"
  foreach {a b} $o {lappend p "$a/$b"}; puts "paires : $p"
  set f [type cast flottants {1.5 2.5}]; set s 0; foreach x $f {set s [expr {$s+$x}]}; puts "somme $s"
  set n 0; trace add variable x write {apply {args {upvar n n; incr n}}}
  foreach x $o {}; puts "trace : $n écritures"
  set z {}; foreach x $o {append x !; lappend z $x}; puts "append : $z"
}
t
