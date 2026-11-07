# Les arguments variadiques n'importe où : {T}args...
#  - sans défaut : au moins une fois ; avec ?= : zéro fois ou plus ;
#  - les arguments d'après se prennent depuis la fin (règle de TIP 288) ;
#  - un produit s'étale sur ses champs : {{list}vars; {list}valeurs}args...
#    prend les mots deux par deux, comme foreach ;
#  - {list.T}args en dernier garde son sens : zéro ou plus, un mot par T.

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-44s -> %s" $script $r]
}

puts "--- foreach, en Tcl : varList list ?varList list ...? body"
proc chacun {{{list}vars; {list}valeurs}args...; {string}corps} {
    set tours 0
    foreach p $args {
        set k [llength $p(vars)]
        set tours [expr {max($tours, ([llength $p(valeurs)] + $k - 1) / $k)}]
    }
    for {set i 0} {$i < $tours} {incr i} {
        foreach p $args {
            set k [llength $p(vars)]
            foreach v $p(vars) x [lrange $p(valeurs) [expr {$i * $k}] end] {
                if {$v ne ""} { uplevel 1 [list set $v $x] }
            }
        }
        switch [catch {uplevel 1 $corps} r o] {
            0 - 4 {}
            3 break
            default { return -options $o $r }
        }
    }
}
chacun {a b} {1 2 3 4} c {x y} { puts "  a=$a b=$b c=$c" }
chacun n {1 2 3 4 5} { if {$n == 2} continue; if {$n == 4} break; puts "  n=$n" }
montre {chacun {a b} {1 2}}
montre {chacun {puts}}

puts "\n--- dict set : dictVar key ?key ...? value"
proc dict_set {{var.dict}d; {string}args...; {string}valeur} {
    dict set d {*}$args $valeur
}
set D {}
dict_set D fruits pomme rouge
dict_set D fruits poire jaune
dict_set D total 2
puts "  $D"
montre {dict_set D 5}
montre {set X abc; dict_set X a 1}

puts "\n--- dict create : ?key value ...?"
proc dict_create {{{string}cle; {string}valeur}args... ?= {}} {
    set d {}
    foreach p $args { dict set d $p(cle) $p(valeur) }
    return $d
}
montre {dict_create}
montre {dict_create a 1 b 2}
montre {dict_create a 1 b}

puts "\n--- des points, à plat : x1 y1 x2 y2 ?x y ...?"
type def pt {{double}x; {double}y}
proc {double}longueur {{pt}args...} {
    set l 0.0
    set prec {}
    foreach p $args {
        if {$prec ne ""} {
            set l [expr {$l + hypot($p(x) - $prec(x), $p(y) - $prec(y))}]
        }
        set prec $p
    }
    return $l
}
montre {longueur 0 0 3 4 3 0}
montre {longueur 0 0 3}
montre {longueur 0 0 3 x}

puts "\n--- regexp : ?switches? exp string ?matchVar? ?subMatchVar ...?"
proc chercher_re {{flag}-nocase; {string}exp; {string}chaine;
                  {string}m ?= {}; {string}args... ?= {}} {
    set opts [expr {$nocase ? {-nocase} : {}}]
    set vars [lmap v [list $m {*}$args] {if {$v eq ""} continue; set v}]
    foreach v $vars { upvar 1 $v $v }
    regexp {*}$opts -- $exp $chaine {*}$vars
}
montre {chercher_re {(\d+)-(\d+)} "du 12-34" tout de a; list $tout $de $a}
montre {chercher_re -nocase {ABC} "xabcx"}
montre {chercher_re}
