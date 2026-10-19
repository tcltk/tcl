# Produit : la forme nom/valeur nue (set {T}v nom val ...) est supprimée.
# n arguments pour n champs = lecture positionnelle, toujours ; un champ se
# nomme par une étiquette {champ}valeur ou par un chemin v(champ,...).
# Plus aucune lecture n'est devinée : l'ambiguïté d'autrefois disparaît.
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
proc essai {script} {
    if {[catch {uplevel #0 $script} r]} {return [lindex [split $r :] 0]}
    return "ok: $r"
}
type def png {{string}signature ?= "SIG"; {list.string}chunks}
check "ancien piège (chunks \$l) : positionnel" [essai {set {png}p1 chunks {a b}}] {ok: chunks {a b}}
check "étiquettes" [essai {set {png}p2 {signature}X {chunks}{a b}}] {ok: X {a b}}
check "étiquettes, ordre libre" [essai {set {png}p3 {chunks}{a b} {signature}X}] {ok: X {a b}}
check "étiquette seule, défaut" [essai {set {png}p4 {chunks}{a b}}] {ok: SIG {a b}}
check "chemin" [essai {set {png}p5(chunks) {a b}}] {ok: SIG {a b}}
check "nom/valeur nue : refusée" [essai {set {png}p6 signature X chunks {a b}}] {wrong # args}
type def paire {{string}nom ?= "?"; {string}age ?= "?"}
check "deux champs, deux valeurs : positionnel" [essai {set {paire}q1 nom Jean}] {ok: nom Jean}
check "positionnel ordinaire" [essai {set {paire}q2 Jean 12}] {ok: Jean 12}
check "étiquettes complètes" [essai {set {paire}q3 {nom}Jean {age}12}] {ok: Jean 12}
check "mélange refusé" [essai {set {paire}q4 {nom}Jean 12}] {can't mix values named by a field annotation ({field}value) and positional values in a paire constructor}
set ::niveau 3
type def trace {{int}niveau ?= $::niveau; {string}texte}
check "étiquette partielle (niveau par défaut)" [essai {set {trace}t {texte}a}] {ok: 3 a}
type def couple {{int}x; {int}y}
check "positionnel numérique" [essai {set {couple}c 1 2}] {ok: 1 2}
check "champ sans défaut manquant" [essai {set {couple}d {x}5}] {field "y" of couple has no value and no default}
puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
