# complexes_tip759.tcl --
#
# Nombres complexes avec la syntaxe de la TIP 759 :
#   - [( ... )]       substitution arithmétique en ligne ;
#   - {( ... )}       corps de procédure compilé comme une expression ;
#   - lassign / set ... dans l'expression, séparés par ';'.
#
# Limite constatée sur l'instantané TIP 759 (même sur un arbre non patché
# par mes modifications) : un APPEL DE FONCTION mathématique dans ( ... ),
# par ex. [(sqrt(4.0))], plante. Les fonctions passent donc par expr {...}.

type def complexe {
    {double}re = 0.0;
    {double}im = 0.0
}

namespace eval cx {
    proc new {re im} { set {complexe}z re $re im $im }

    proc add {a b} {(lassign $a ar ai; lassign $b br bi;
                     [new [($ar+$br)] [($ai+$bi)]])}
    proc sub {a b} {(lassign $a ar ai; lassign $b br bi;
                     [new [($ar-$br)] [($ai-$bi)]])}
    proc mul {a b} {(lassign $a ar ai; lassign $b br bi;
                     [new [($ar*$br - $ai*$bi)] [($ar*$bi + $ai*$br)]])}
    proc conj {a}  {(lassign $a r i; [new $r [(-$i)]])}
    proc norm2 {a} {(lassign $a r i; $r*$r + $i*$i)}
    proc abs {a}   { expr {sqrt([norm2 $a])} }     ;# fonction => expr
    proc div {a b} {
        set d [norm2 $b]
        if {$d == 0.0} { error "division par zéro" }
        lassign [mul $a [conj $b]] nr ni
        new [($nr/$d)] [($ni/$d)]
    }
    proc str {a} {
        lassign $a r i
        format "%g%s%gi" $r [expr {$i < 0 ? "-" : "+"}] [expr {abs($i)}]
    }
}

set {complexe}a re 3 im 4
set {complexe}b re 1 im -2
puts "a+b = [cx::str [cx::add $a $b]]"
puts "a-b = [cx::str [cx::sub $a $b]]"
puts "a*b = [cx::str [cx::mul $a $b]]"
puts "a/b = [cx::str [cx::div $a $b]]"
puts "|a| = [cx::abs $a]"
puts "|a|^2 = [cx::norm2 $a]"
catch {set {complexe}bad re abc im 1} m
puts "refus : $m"
