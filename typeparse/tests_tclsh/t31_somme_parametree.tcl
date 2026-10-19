type def a1 {{int}x}
type def {p1({int}n)} {{list($n).int}l}
type def {exe({int}n)} {{a1}elf, {p1($n)}pe}
set {p1(2)}P(l) {4 5}
set {exe(2)}X {pe}$P
puts [type get $X]
puts $X(pe.l.1)
set {a1}A {x}3
set {exe(2)}Y {elf}$A
puts "[type get $Y] $Y(elf.x)"
