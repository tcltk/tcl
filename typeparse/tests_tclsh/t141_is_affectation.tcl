# t141 -- v18c : à gauche de « is », une affectation entre parenthèses
# désigne sa variable (comme une lvalue de C++) : (r = valeur) is T affecte
# puis teste r ; (r is T = valeur) is U déclare, affecte puis teste.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-36s %s" $label $r]
}
type def result {{string}ok, {string}err}
proc {result}ouvrir {fn} {
    if {[catch {open $fn} fh]} { return {/err}$fh }
    return {/ok}$fh
}
set ici [info script]
t "global : ok"                 {if {(r = [ouvrir $ici]) is result/ok} {close $r; set x lu} else {set x non}}
t "global : err"                {if {(r = [ouvrir /nulle/part]) is result/ok} {set x lu} else {list non [type get $r]}}
t "r est posée"                 {type get $r}
proc lire {fn} {
    if {(f = [ouvrir $fn]) is result/ok} { close $f; return lu }
    return "erreur ([type get $f])"
}
t "procédure : ok"              {lire $ici}
t "procédure : err"             {lire /nulle/part}
t "valeur nue"                  {list [expr {(x = 5) is int}] [expr {(x = "abc") is int}] $x}
proc p {} { list [expr {(y = 5) is int}] [expr {(y = 2.5) is int}] }
t "procédure, valeur nue"       {p}
t "variable typée"              {set {int}z 3; list [expr {(z = 7) is int}] $z}
t "déclaration"                 {expr {(d is result = [ouvrir /nulle/part]) is result/err}}
proc q {} { if {(e is result = [ouvrir /nulle/part]) is result/ok} {return oui}; return [list non [type get $e]] }
t "déclaration en procédure"    {q}
t "is ordinaire inchangé"       {expr {r is result/err}}
t "sans parenthèses : k = (5 is int)" {expr {k = 5 is int}}
