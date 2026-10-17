# Produit : n arguments pour n champs (n pair) peuvent se lire en positionnel
# OU en nom/valeur. Une seule lecture valide : on la prend ; les deux : erreur.
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
proc essai {script} {
    if {[catch {uplevel #0 $script} r]} {return [lindex [split $r :] 0]}
    return "ok: $r"
}
type def png {{string}signature = "SIG"; {list.string}chunks}
check "piège de l'énoncé (chunks \$l)" [essai {set {png}p1 chunks {a b}}] {ambiguous arguments for png}
check "tout nommé" [essai {set {png}p2 signature X chunks {a b}}] {ok: X {a b}}
check "ordre libre" [essai {set {png}p3 chunks {a b} signature X}] {ok: X {a b}}
check "positionnel sans nom de champ en tête" [essai {set {png}p4 X {a b}}] {ok: X {a b}}
type def paire {{string}nom = "?"; {string}age = "?"}
check "deux lectures valides" [essai {set {paire}q1 nom Jean}] {ambiguous arguments for paire}
check "positionnel ordinaire" [essai {set {paire}q2 Jean 12}] {ok: Jean 12}
check "nom/valeur complet" [essai {set {paire}q3 nom Jean age 12}] {ok: Jean 12}
# une seule lecture valide : on la prend
set ::niveau 3
type def trace {{int}niveau = $::niveau; {string}texte}
check "nom/valeur partiel (niveau par défaut)" [essai {set {trace}t texte a}] {ok: 3 a}
type def couple {{int}x; {int}y}
check "positionnel numérique" [essai {set {couple}c 1 2}] {ok: 1 2}
check "nom/valeur n=2 sans défaut : positionnel" [essai {set {couple}d x 5}] {expected integer but got "x"}
puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
