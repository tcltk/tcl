# Étiquettes de champ : set {T}v {champ}valeur ... (TIP 764, v13)
# FR : l'étiquette est l'annotation en attente la plus récente de chaque
#      argument ; elle est lue dans le contexte du produit (nom de champ avant
#      type global). Tout ou rien : aucun argument étiqueté = positionnel.
proc montre {script} {
    if {[catch {uplevel #0 $script} r]} {return "erreur: $r"}
    return $r
}
type def paire {{string}nom; {int}age ?= 0}
puts "1 [montre {set {paire}p {nom}jean {age}12}]"
puts "2 [montre {set {paire}p {age}13 {nom}paul}]"
puts "3 [montre {set {paire}p {nom}zoe}]"
puts "4 [montre {set {paire}p jean 14}]"
puts "5 [montre {set {paire}p nom jean age 12}]"
puts "6 [montre {set {paire}p {nom}jean 12}]"
puts "7 [montre {set {paire}p {nom}jean {age}x}]"
puts "8 [montre {set {paire}p {nom}a {nom}b}]"
puts "9 [montre {set {paire}p {nom}a {taille}3}]"
# valeur portant son étiquette, par variable
set v {age}21
puts "10 [montre {set {paire}p {nom}max $v}]"
# étiquette sur une valeur déjà annotée : la plus récente compte
set w {mm}12
puts "11 [montre {set {paire}p {nom}lu {age}$w}]"
# le contexte du produit passe avant un type global du même nom
type def age int
puts "12 [montre {set {paire}p {nom}eve {age}20}]"
puts "13 [montre {set {paire}p {nom}eve {age}20; type get $p(age)}]"
# en procédure (compilé)
proc f {n a} {set {paire}q {nom}$n {age}$a; list $q(nom) $q(age)}
puts "14 [f ana 30]"
# produit imbriqué : l'étiquette donne le contexte au champ
type def pt {{int}x; {int}y}
type def seg {{pt}a; {pt}b}
puts "15 [montre {set {seg}s {b}{3 4} {a}{1 2}}]"
puts "16 [montre {set {seg}s {a}{1 2}}]"
# chemin et étiquettes donnent la même valeur
set {seg}s1(a,b) {1 2} {3 4}
set {seg}s2 {a}{1 2} {b}{3 4}
puts "17 [expr {$s1 eq $s2}]"
