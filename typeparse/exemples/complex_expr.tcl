# complex_expr.tcl --
#
# Nombres complexes derrière UNE seule procédure `Complex`, avec des switch,
# de simili opérateurs unaires et binaires (modèle : complex.tcl), écrite avec
# les nouveautés de expr :
#   - le type Complex et `var is Complex = ...` (déclaration typée) ;
#   - la destructuration (a,b) = $z ;
#   - les tuples (re, im) comme résultat ;
#   - `Complex is cmd` : la procédure s'appelle aussi comme fonction de expr.
#
#   Complex <texte>              -> (re, im)      "1"  "i"  "-1"  "3+4i"  "2.5-i"
#   Complex <op> <z>             -> opérateur unaire
#   Complex <z1> <op> <z2>       -> opérateur binaire
#
# Noyau requis : TIP 759 + tip759-fonctions / -lassign / -is /
# -affectation-typee, et tclTypes.c à jour.

type def Complex {{double}re = 0.0; {double}im = 0.0}

proc Complex args {
    if {[llength $args] == 1} then {
        return [::cx::parse [lindex $args 0]]
    } elseif {[llength $args] == 2} then {
        # opérateur unaire : z doit être un Complex valide (sinon erreur typée)
        lassign $args op z
        expr {(zz is Complex = $z; (a,b) = $zz)}
        return [switch -- $op {
            conj   {($a,-$b)}
            real   {($a)}
            img    {($b)}
            mod    {(hypot($a,$b))}
            arg    {($a != 0.0 || $b != 0.0 ? atan2($b,$a) : 0.0)}
            exp    {(exp($a)*cos($b), exp($a)*sin($b))}
            cos    {(cos($a)*cosh($b), -sin($a)*sinh($b))}
            sin    {(sin($a)*cosh($b), cos($a)*sinh($b))}
            tan    { Complex [Complex sin $zz] / [Complex cos $zz] }
            log    {(
                log(hypot($a,$b)),
                $a != 0.0 || $b != 0.0 ? atan2($b,$a) : 0.0
            )}
            sqrt   {(
                argz = ($a != 0.0 || $b != 0.0 ? atan2($b,$a)/2.0 : 0.0);
                modz = sqrt(hypot($a,$b));
                $modz*cos($argz), $modz*sin($argz)
            )}
            format {(
                "[( $a )][( $b<0 ? "-" : "+")][( abs($b) )]i"
            )}
            "-"    {(-$a,-$b)}
            default { error "opérateur unaire inconnu \"$op\"" }
        }]
    } elseif {[llength $args] == 3} then {
        # opérateur binaire ; le second opérande de x est un scalaire
        lassign $args z1 op z2
        expr {(w1 is Complex = $z1; (a,b) = $w1)}
        if {$op ne "x"} {
            expr {(w2 is Complex = $z2; (c,d) = $w2)}
        } else {
            set c $z2; set d 0
        }
        return [switch -- $op {
            +  {($a+$c, $b+$d)}
            -  {($a-$c, $b-$d)}
            *  {($a*$c - $b*$d, $a*$d + $b*$c)}
            /  {(
                denom = double($c**2 + $d**2);
                ($a*$c + $b*$d)/$denom,
                (-$a*$d + $b*$c)/$denom
            )}
            ** { Complex exp [Complex [Complex log $w1] * $w2] }
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
            if {![regexp {i$} $s]} {
                return [list [expr {$re eq "" ? 0.0 : double($re)}] 0.0]
            }
            if {$im eq ""} { set im 1 }
            if {$sgn eq "-"} { set im -$im }
            return [list [expr {$re eq "" ? 0.0 : double($re)}] [expr {double($im)}]]
        }
        error "nombre complexe invalide : \"$s\""
    }
}

# Complex devient aussi une FONCTION de expr :
#   Complex($a, "+", $b)   Complex("conj", $z)   Complex("3+4i")
expr {Complex is cmd}

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

    # Valeurs invalides : refusées par le type
    catch {Complex conj {1 abc}} m;   puts "conj {1 abc}  -> $m"
    catch {Complex {1 2 3} + $a} m;   puts "{1 2 3} + a   -> $m"
    catch {Complex $a ? $b} m;        puts "a ? b         -> $m"

    # Comme fonction de expr (après `Complex is cmd`)
    expr {p is Complex = Complex($a, "+", $b)}
    expr {q is Complex = Complex(Complex($b, "*", $b), "-", $a)}
    puts "Complex(a,\"+\",b)        = $p"
    puts "Complex(b*b - a)       = $q"
    puts "mod(3+4i) en fonction  = [expr {Complex("mod", Complex("3+4i"))}]"

    # Ensemble de Mandelbrot, z <- z*z + c. Boucles for à scripts
    # arithmétiques : for (init) {test} {(pas)} {corps}
    proc mandel {cx cy {max 30}} {
        expr {(c is Complex = ($cx,$cy); z is Complex = (0,0))}
        for (n=0) {$n < $max && Complex("mod", $z) <= 2.0} {(n=$n+1)} {(z = Complex(Complex($z, "*", $z), "+", $c))}
        return $n
    }
    expr {mandel is cmd}
    puts ""
    set palette { .:-=+*#%@}
    for (y=1.1) {$y >= -1.1} {(y=$y-0.2)} {
        set line ""
        for (x=-2.1) {$x <= 0.75} {(x=$x+0.05)} {
            append line [string index $palette [expr {min(9, mandel($x,$y,30) * 9 / 30)}]]
        }
        puts $line
    }
}
