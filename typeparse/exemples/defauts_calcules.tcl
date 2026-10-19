# defauts_calcules.tcl --
#
# Défauts calculés : une commande ou une variable évaluée à CHAQUE création
# d'une valeur, dont le résultat est stocké (jamais rappelé ensuite).
# Même style que arbre_graphe.tcl. Non exécuté sur le vrai Tcl 9.0.

# 1. Défaut par script : [commande], au niveau global.
type def journal {
    {int}epoch = [clock seconds];
    {string}message;
    {string}auteur = [info hostname]
}

set {journal}a(message) "démarrage"
after 1500
set {journal}b(message) "suite"
# $a(epoch) et $b(epoch) diffèrent : le script a tourné deux fois.
# Une valeur fournie court-circuite le script :
set {journal}c(epoch,message) 0 "à la main";# clock seconds n'est pas appelé

# 2. Défaut par variable : $::nom est lue à chaque construction.
set ::niveau 3
type def trace {
    {int}niveau = $::niveau;
    {string}texte
}
set {trace}t1(texte) "a"
set ::niveau 5
set {trace}t2(texte) "b";# $t2(niveau) vaut 5, $t1(niveau) reste 3

# 3. Paramètre {cmd} : la source de la valeur se choisit à l'instanciation.
proc ::maintenant {} { clock seconds }          ;# une commande = un seul mot
proc ::test::fakeclock {} { return 1000 }
proc ::test::compteur {} { incr ::test::n }

type def {horodate(
    {type}T,
    {cmd}horloge = ::maintenant
)} {
    {int}date = $horloge;
    {$T}valeur
}

set {horodate(string, ::test::fakeclock)}h(valeur) "x";# $h(date) = 1000, déterministe
set {horodate(string, ::test::compteur)}k(valeur) "y";# 1, puis 2, puis 3...
# horodate(string, ::a) et horodate(string, ::b) sont deux types distincts.

# 4. Clé de dictionnaire à défaut calculé.
type def etiquette {dict(nom, cree?=[clock seconds], niveau?=$::niveau).string}
set {etiquette}e nom "alpha"        ;# nom alpha cree <maintenant> niveau 5
