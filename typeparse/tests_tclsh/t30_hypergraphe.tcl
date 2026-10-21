# hypergraphe.tcl --
#
# Un hypergraphe : des sommets et des hyperarêtes, chacune étant un sous-
# ensemble QUELCONQUE de sommets (2, 3, 5...). Exemple classique : les
# co-auteurs. Les sommets sont les auteurs, chaque article est une
# hyperarête qui regroupe ses auteurs.
#
# Deux représentations, complémentaires :
#
#   A. des données qui vivent : chaque auteur est une VARIABLE typée, et un
#      article est une liste de liens (var) vers ses auteurs. Modifier un
#      auteur est visible de tous ses articles ; type deref prend un
#      instantané.
#   B. de l'analyse : un article est un MASQUE DE BITS (bitarray) sur la
#      liste des auteurs. Taille, intersection, union, degré, dual : des
#      opérations sur entiers.
#
# Une procédure fait le pont de A vers B. Aucun fichier externe.

# ===========================================================================
# B. Hyperarêtes = masques de bits
# ===========================================================================

puts "=== B. analyse avec des masques de bits ==="

# n sommets : la liste des étiquettes a exactement n éléments, et chaque
# hyperarête est un entier de n bits (le bit i = le sommet i en fait partie).
type def {hg({int}n)} {{list($n).string}labels; {list.bitarray($n)}edges}

set {hg(5)}H(labels,edges) {alice bob carol dave erin} {0b00111 0b01100 0b11000 0b00011}
# articles :   0: alice bob carol   1: carol dave   2: dave erin   3: alice bob
puts "type : [type get $H]"

# les membres d'un masque, par leurs noms
proc noms {labels masque} {
    set out {}
    set i 0
    foreach nom $labels {
	if {($masque >> $i) & 1} {lappend out $nom}
	incr i
    }
    return $out
}
set etiquettes $H(labels)

puts "\nArticles"
for {set i 0} {$i < 4} {incr i} {
    puts [format "  article %d : %-22s taille %d" $i \
	    [join [noms $etiquettes $H(edges.$i)] ", "] \
	    [expr {popcount($H(edges.$i))}]]
}

puts "\nChaque bit se lit directement : carol a-t-elle écrit l'article 0 ? $H(edges.0.2)"

puts "\nArticles qui partagent des auteurs (intersection non vide)"
for {set i 0} {$i < 4} {incr i} {
    for {set j [expr {$i + 1}]} {$j < 4} {incr j} {
	set commun [expr {$H(edges.$i) & $H(edges.$j)}]
	if {$commun} {
	    puts "  $i et $j : [join [noms $etiquettes $commun] ", "]"
	}
    }
}

puts "\nDegré de chaque sommet (nombre d'articles)"
for {set k 0} {$k < 5} {incr k} {
    set deg 0
    for {set i 0} {$i < 4} {incr i} { incr deg $H(edges.$i.$k) }
    puts [format "  %-6s %d" [lindex $etiquettes $k] $deg]
}

puts "\nCo-auteurs de carol (union de ses articles, sans elle)"
set union 0
for {set i 0} {$i < 4} {incr i} {
    if {$H(edges.$i.2)} { set union [expr {$union | $H(edges.$i)}] }
}
puts "  [join [noms $etiquettes [expr {$union & ~(1 << 2)}]] ", "]"

# L'hypergraphe DUAL : les articles deviennent les sommets, les auteurs les
# hyperarêtes (c'est la matrice transposée).
proc dual {H} {
    set nart [llength $H(edges)]
    set masques {}
    for {set k 0} {$k < [llength $H(labels)]} {incr k} {
	set m 0
	for {set i 0} {$i < $nart} {incr i} {
	    if {$H(edges.$i.$k)} { set m [expr {$m | (1 << $i)}] }
	}
	lappend masques $m
    }
    return $masques
}
set {hg(4)}D(labels,edges) {art0 art1 art2 art3} [dual $H]
puts "\nDual : les hyperarêtes sont les auteurs"
for {set k 0} {$k < 5} {incr k} {
    puts [format "  %-6s a écrit : %s" [lindex $etiquettes $k] \
	    [join [noms $D(labels) $D(edges.$k)] ", "]]
}

puts "\nGaranties du typage"
puts "  [catch {set H(edges.1) 0b100000} msg] $msg"
puts "  [catch {set {hg(5)}faux(labels,edges) {a b c} {1}} msg] $msg"

# ===========================================================================
# A. Données vivantes : sommets = variables, hyperarêtes = listes de liens
# ===========================================================================

puts "\n=== A. données vivantes avec des liens ==="

type def auteur {{string}nom; {int}articles = 0}
type def papier {{string}titre; {list.var(typed).auteur}auteurs}

set {auteur}alice {nom}alice
set {auteur}bob {nom}bob
set {auteur}carol {nom}carol
set {auteur}dave {nom}dave

set {papier}pa(titre,auteurs) "Types et liens" {alice bob carol}
set {papier}pb(titre,auteurs) "Sommes et variants" {carol dave}

# un article regroupe des LIENS : on voit les noms de variables, pas les valeurs
puts "pa = $pa"

# publier : chaque auteur du papier gagne un article (on écrit À TRAVERS les liens)
proc publier {papier} {
    upvar 1 $papier p
    for {set i 0} {$i < [llength $p(auteurs)]} {incr i} {
	set p(auteurs.$i.articles) [expr {$p(auteurs.$i.articles) + 1}]
    }
}
publier pa
publier pb
puts "apres publication : alice=$alice(articles) carol=$carol(articles) dave=$dave(articles)"

# les liens suivent les variables : une seule carol, partagée par les deux
set carol(nom) "Carol Dupont"
puts "carol vue par pa : $pa(auteurs.2.nom) ; vue par pb : $pb(auteurs.0.nom)"

# un instantané, figé : le résultat est une vraie liste d'auteurs
set avant [type deref $pa]
publier pa
set apres [type deref $pa]
puts "instantané avant : [lmap a $avant(auteurs) {lindex $a(*) 1}]"
puts "instantané après : [lmap a $apres(auteurs) {lindex $a(*) 1}]   (nombre d'articles de chacun)"
puts "type : [type get $avant]"

# un auteur supprimé : erreur franche, qui dit où
unset bob
puts "\nauteur supprimé : [catch {type deref $pa} msg] $msg"
set {auteur}bob {nom}bob {articles}2;# on le recrée

# une variable non typée n'est pas acceptée comme auteur
set intrus "x"
puts "variable non typée : [catch {set {papier}pz(titre,auteurs) "?" {intrus}} msg] $msg"

# ===========================================================================
# Le pont : de la forme A (liens) à la forme B (masques)
# ===========================================================================

puts "\n=== A vers B ==="

# Un masque de bits sur la liste d'étiquettes donnée, à partir d'un papier.
proc masque_de {papier etiquettes} {
    upvar 1 $papier p
    set m 0
    for {set i 0} {$i < [llength $p(auteurs)]} {incr i} {
	set k [lsearch -exact $etiquettes $p(auteurs.$i.nom)]
	if {$k < 0} { error "auteur inconnu : $p(auteurs.$i.nom)" }
	set m [expr {$m | (1 << $k)}]
    }
    return $m
}
set carol(nom) carol                          ;# nom d'origine
set noms {alice bob carol dave}
set {hg(4)}G {labels}$noms {edges}[list [masque_de pa $noms] [masque_de pb $noms]]
puts "pa -> [format %04b $G(edges.0)]   pb -> [format %04b $G(edges.1)]"
puts "carol est dans les deux : intersection = [join [noms $noms [expr {$G(edges.0) & $G(edges.1)}]] ", "]"
