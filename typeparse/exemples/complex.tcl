# complex.tcl --
#
# Nombres complexes derrière UNE seule procédure `Complex`, à la manière de
# complex.tcl fourni : un `switch` sur l'opérateur, le résultat est un
# couple {re im} produit par une expression TIP 759 : ( re, im ).
#
#   Complex <texte>            -> {re im}        (analyse : 1, i, -1, 3+4i, 2.5-i ...)
#   Complex <op> <z>           -> opérateur unaire
#   Complex <z1> <op> <z2>     -> opérateur binaire
#
# Nécessite le noyau TIP 759 (tuples, affectation, appels de fonction dans
# ( ... )), avec le correctif noyau/tip759-fonctions.patch.

# Le type Complex : un couple {re im} de doubles (défauts 0.0).
#   set {Complex}z re 3 im 4      ;# variable typée
#   expr {z is Complex}           ;# opérateur "is" : vrai (voir noyau/tip759-is.patch)
type def Complex {{double}re = 0.0; {double}im = 0.0}

proc Complex args {
    if {[llength $args] == 1} then {
        return [::cx::parse [lindex $args 0]]
    } elseif {[llength $args] == 2} then {
        lassign $args op z
        lassign $z a b
        return [switch -- $op {
            conj {($a,-$b)}
            real {($a)}
            img  {($b)}
            mod  {(hypot($a,$b))}
            arg  {($a != 0.0 || $b != 0.0 ? atan2($b,$a) : 0.0)}
            exp  {(exp($a)*cos($b), exp($a)*sin($b))}
            cos  {(cos($a)*cosh($b), -sin($a)*sinh($b))}
            sin  {(sin($a)*cosh($b), cos($a)*sinh($b))}
            tan  {
                # tan z = sin z / cos z
                Complex [Complex sin $z] / [Complex cos $z]
            }
            log  {(
                log(hypot($a,$b)),
                $a != 0.0 || $b != 0.0 ? atan2($b,$a) : 0.0
            )}
            sqrt {(
                argz = ($a != 0.0 || $b != 0.0 ? atan2($b,$a)/2.0 : 0.0);
                modz = sqrt(hypot($a,$b));
                $modz*cos($argz), $modz*sin($argz)
            )}
            format {(
                "[( $a )][( $b<0 ? "-" : "+")][( abs($b) )]i"
            )}
            "-" {(-$a,-$b)}
            default { error "opérateur unaire inconnu \"$op\"" }
        }]
    } elseif {[llength $args] == 3} then {
        lassign $args z1 op z2
        lassign $z1 a b
        lassign $z2 c d
        return [switch -- $op {
            + {($a+$c, $b+$d)}
            - {($a-$c, $b-$d)}
            * {($a*$c - $b*$d, $a*$d + $b*$c)}
            / {(
                denom = double($c**2 + $d**2);
                ($a*$c+$b*$d)/$denom,
                (-$a*$d+$b*$c)/$denom
            )}
            ** { Complex exp [Complex [Complex log $z1] * $z2] }
            x  {($a*$c, $b*$c)}
            default { error "opérateur binaire inconnu \"$op\"" }
        }]
    }
    error "wrong # args: should be \"Complex text | op z | z1 op z2\""
}

namespace eval ::cx {
    # "1" "i" "-i" "-1" "3+4i" "2.5-i" "4i"
    proc parse {s} {
        set s [string map {" " ""} $s]
        set num {[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?}
        if {[regexp "^(-?$num)?(?:(\[-+\]?)($num)?i)?\$" $s -> re sgn im]} {
            if {![regexp {i$} $s]} { return [list [expr {$re eq "" ? 0.0 : double($re)}] 0.0] }
            if {$im eq ""} { set im 1 }
            if {$sgn eq "-"} { set im -$im }
            return [list [expr {$re eq "" ? 0.0 : double($re)}] [expr {double($im)}]]
        }
        error "nombre complexe invalide : \"$s\""
    }
}

# --- essais ----------------------------------------------------------
if {[info exists argv0] && [file tail $argv0] eq [file tail [info script]]} {
    set a [Complex 1]; set b [Complex i]; set c [Complex -1]
    puts "a = [Complex format $a]; b = [Complex format $b]; c = [Complex format $c]"
    puts "a+b = [Complex $a + $b]   (= 1 1)"
    puts "a-b = [Complex $a - $b]   (= 1 -1)"
    puts "-a  = [Complex - $a]   (= -1 -0)"
    puts "a*a = [Complex $a * $a]   (= 1 0)"
    puts "b*b = [Complex $b * $b]   (= -1 0)"
    puts "a/b = [Complex $a / $b]   (= 0 -1)"
    puts "a x 4 = [Complex $a x 4]  (= 4 0)"
    puts "exp(a) = [Complex exp $a] (= e 0)"
    puts "exp(b) = [Complex exp $b] (= cos1 sin1)"
    puts "conj(b) = [Complex conj $b] (= 0 -1)"
    puts "mod(b) = [Complex mod $b]  (= 1)"
    puts "arg(b) = [Complex arg $b]  (= pi/2)"
    puts "real/img(3+4i) = [Complex real {3 4}] [Complex img {3 4}]"
    puts "sqrt(c) = [Complex sqrt $c] (= 0 1)"
    puts "log(c) = [Complex log $c]  (= 0 pi)"
    puts "i**i = [Complex $b ** $b]  (= e**(-pi/2) = 0.2078795763507619)"
    puts "3+4i -> [Complex format [Complex 3+4i]],  2.5-i -> [Complex format [Complex 2.5-i]]"
    puts "tan(b) = [Complex tan $b]"

    # --- l'opérateur "is" : variable à gauche, type à droite -----------
    set {Complex}z re 3 im 4
    set {Complex}w {*}[Complex $a * $b]    ;# résultat rangé dans une variable typée
    set plain {1 2}
    set {int}n 5
    puts "z is Complex     = [expr {z is Complex}]      (= 1)"
    puts "w is Complex     = [expr {w is Complex}]      (= 1)"
    puts "n is Complex     = [expr {n is Complex}]      (= 0)"
    puts "plain is Complex = [expr {plain is Complex}]      (= 1 : valeur {1 2} valide)"
    set s abc
    puts "s is Complex     = [expr {s is Complex}]      (= 0)"
    puts "absente is Complex = [expr {absente is Complex}]  (= 0)"
    if {z is Complex && w is Complex} { puts "z et w sont des Complex" }
}
