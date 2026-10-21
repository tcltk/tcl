# complexes.tcl --
#
# Nombres complexes : un type produit {re, im} et quelques opérations.
# Exécuté avec le tclsh patché.

type def complexe {
    {double}re = 0.0;
    {double}im = 0.0
}

namespace eval cx {
    # Une valeur de type produit se lit comme une liste {re im}.
    proc new {re im} { set {complexe}z {re}$re {im}$im}

    proc add {a b} {
        lassign $a(*) ar ai; lassign $b(*) br bi
        new [expr {$ar + $br}] [expr {$ai + $bi}]
    }
    proc sub {a b} {
        lassign $a(*) ar ai; lassign $b(*) br bi
        new [expr {$ar - $br}] [expr {$ai - $bi}]
    }
    proc mul {a b} {
        lassign $a(*) ar ai; lassign $b(*) br bi
        new [expr {$ar*$br - $ai*$bi}] [expr {$ar*$bi + $ai*$br}]
    }
    proc conj {a} { lassign $a(*) r i; new $r [expr {-$i}] }
    proc abs  {a} { lassign $a(*) r i; expr {hypot($r, $i)} }
    proc div {a b} {
        lassign $b(*) br bi
        set d [expr {$br*$br + $bi*$bi}]
        if {$d == 0.0} { error "division par zéro" }
        set m [mul $a [conj $b]]; lassign $m(*) nr ni
        new [expr {$nr/$d}] [expr {$ni/$d}]
    }
    proc str {a} {
        lassign $a(*) r i
        format "%g%s%gi" $r [expr {$i < 0 ? "-" : "+"}] [expr {abs($i)}]
    }
}

# --- utilisation -------------------------------------------------
set {complexe}a {re}3 {im}4
set {complexe}b {re}1 {im}-2
set {complexe}zero                       ;# défauts : 0 + 0i
puts "a    = [cx::str $a]"
puts "b    = [cx::str $b]"
puts "zero = [cx::str $zero]"
puts "a+b  = [cx::str [cx::add $a $b]]"
puts "a-b  = [cx::str [cx::sub $a $b]]"
puts "a*b  = [cx::str [cx::mul $a $b]]"
puts "a/b  = [cx::str [cx::div $a $b]]"
puts "|a|  = [cx::abs $a]"
puts "a(re)=$a(re)"

# Le typage protège : une partie réelle non numérique est refusée.
catch {set {complexe}bad {re}abc {im}1} m
puts "refus : $m"
catch {set a(im) xyz} m
puts "refus : $m"

# i*i = -1
set {complexe}i {re}0 {im}1
puts "i*i  = [cx::str [cx::mul $i $i]]"
