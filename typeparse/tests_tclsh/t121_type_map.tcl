# t121 : type map VARS LISTE ?VARS LISTE ...? {(EXPR)} : une expression
# appliquée à chaque élément, évaluée en C quand elle n'est faite que de
# nombres, de variables, d'arithmétique, de comparaisons et des fonctions
# usuelles ; sinon par expr (même résultat). {x y z} prend trois colonnes.
type def pt3 {{double}a; {double}b; {double}c}
type def {-compact}vd list.double
proc t {} {
    set {list.pt3}P {1 2 3} {4 5 6} {7.5 8 9}
    set r [type map {x y z} $P(*.(a,b,c)) {($x + $y + $z)}]
    puts "colonnes : $r [type canonic $r]"
    puts "entiers : [type canonic [type map {x} {1 2 3} {($x * 2)}]]"
    puts "deux listes : [type map {x} {1 2 3} {k} {10 20 30} {($x * $k + 1)}]"
    set s 0.5
    puts "variable : [type map {x} {1 2 3} {($x * $s)}]"
    puts "division entière : [type map {x} {7 -7 7} {d} {2 2 -2} {($x / $d)}]"
    puts "par zéro (flottant) : [type map {x} {1 0 2} {(1.0 / $x)}]"
    catch {type map {x} {1 0 2} {(1 / $x)}} e; puts "par zéro (entier) : $e"
    puts "débordement : [type map {x} {9223372036854775807 1} {($x + 1)}]"
    puts "round : [type map {x} {2.5 -2.5 3} {(round($x))}]"
    puts "?: : [type map {x} {-2 0 3} {($x > 0 ? sqrt($x) : -1)}]"
    puts "chaînes (expr) : [type canonic [type map {x} {a bb ccc} {([string length $x])}]]"
    puts "après : x=$x"
    set V [type cast vd {0.5 1.5 2.5}]
    puts "en cases : [type map {x} $V {y} $V {($x * $y)}]"
    catch {type map {x} {1 2} {$x + 1}} e; puts "corps : $e"
    catch {type map {x} {1 2} {y} {1} {($x)}} e; puts "longueurs : $e"
    catch {type map {x y z} {{1 2} {3 4}} {($x)}} e; puts "colonnes : $e"
    catch {type map {x} {1 a 3} {($x * 2)}} e; puts "valeur : $e"
}
t
proc s {} {
    puts "somme : [type map -sum {x} {1 2 3} {($x * 2)}] [type map -sum {x} {0.1 0.2 0.3} {($x)}] [type map -sum {x} {} {($x)}]"
    puts "somme (grand entier) : [type map -sum {x} {9223372036854775807 1} {($x)}]"
    set {list.double}L 1 2 3 4 5 6
    puts "tranches : [type canonic $L(0:end-2:2)] [type canonic $L(1::2)]"
    set L(::2) [type map {x} $L(::2) {($x * 10)}]
    puts "écriture d'une tranche typée : $L"
}
s
