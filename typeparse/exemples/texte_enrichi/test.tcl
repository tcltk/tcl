# test.tcl -- le modèle sans Tk : un faux widget text enregistre les appels.
set ici [file dirname [file normalize [info script]]]
source [file join $ici modele.tcl]
source [file join $ici fauxtext.tcl]

proc faux {args} {
    global appels
    lappend appels $args
}
proc t {l s} { if {[catch {uplevel #0 $s} r]} {puts "$l -> ERREUR $r"} else {puts "$l -> $r"} }

puts "--- styles"
t "table" {te::styles {
    gras     {poids bold}
    italique {pente italic}
    titre1   {taille 18 poids bold couleur #1d2433}
    titre2   {taille 14 poids bold}
    titre3   {taille 12 poids bold pente italic}
    code     {famille Courier taille 10 couleur #4a5468}
    puce     {marge 16}
    alerte   {couleur #b4531f souligne 1}
}}
t "un style"        {set s [dict get $te::styles titre1]; list $s(taille) $s(poids) $s(pente) [type get $s]}
t "style faux"      {te::style poids gras}

puts "--- document"
set doc [te::document \
    [te::titre 1 "Texte enrichi"] \
    [te::para "Un " [te::m gras "texte"] " avec un passage " \
	[te::m italique "en italique, " [te::m gras "et gras dedans"]] "."] \
    [te::puces {"premier point"} [list "second, avec un " [te::lien https://tcl.tk "lien"]]] \
    [te::code "set {style}s(poids) bold"]]
t "type"            {list [type get $doc] [lmap b $doc {type get $b}]}

puts "--- refus à la construction"
t "style inconnu"   {te::m grass "x"}
t "titre de niveau 4" {te::titre 4 "x"}
t "bloc inconnu"    {te::document [te::para "a"] "pas un bloc"}

puts "--- rendu"
set appels {}
te::rendre faux $doc {ouvrir}
foreach a $appels {
    if {[lindex $a 0] eq "insert"} {
	puts [format "  insert %-28s %s" [list [lindex $a 2]] [lindex $a 3]]
    } elseif {[lindex $a 0] eq "tag" && [lindex $a 1] eq "bind"} {
	puts "  tag bind [lrange $a 2 end]"
    }
}
puts "  tags configurés : [lmap a $appels {expr {[lrange $a 0 1] eq {tag configure} ? [lindex $a 2] : [continue]}}]"

puts "--- changer la table des styles"
t "nouvelle table"  {te::styles {gras {poids bold} rouge {couleur red}}}
t "ancien document" {list [type get $doc] [type get [lindex $doc 1]]}
t "italique refusé" {te::m italique "x"}
t "rouge accepté"   {type get [te::m rouge "x"]}

puts "--- relire le widget (te::lire)"
te::styles {
    gras     {poids bold}
    italique {pente italic}
    titre1   {taille 18 poids bold}
    titre2   {taille 14 poids bold}
    titre3   {taille 12 poids bold}
    code     {famille Courier}
    puce     {marge 16}
}
set doc [te::document \
    [te::titre 1 "Texte enrichi"] \
    [te::para "Un " [te::m gras "texte"] " avec un passage " \
	[te::m italique "en italique, " [te::m gras "et gras dedans"]] "."] \
    [te::puces {"premier point"} [list "second, avec un " [te::lien https://tcl.tk "lien"]]] \
    [te::code "set x 1\n\nset y 2"] \
    [te::para "fin"]]
fauxtext::creer w1
te::rendre w1 $doc
t "aller-retour exact" {expr {[te::lire w1] eq $doc}}
t "type relu"          {lmap b [te::lire w1] {type get $b}}

fauxtext::creer w2
te::rendre w2 $doc
w2 tag add [te::Tag segment/marque style gras] 3.20 3.30
t "chevauchement, coupé" {set p [lindex [te::lire w2] 1]; lmap s $p(para) {list [type get $s] $s}}
t "chevauchement, erreur" {te::lire w2 -chevauchement erreur}

fauxtext::creer w3
te::rendre w3 $doc
w3 tag add [te::Tag segment/marque style gras] 3.0 3.3
t "deux gras voisins fusionnent" {set p [lindex [te::lire w3] 1]; lrange $p(para) 0 1}

fauxtext::creer w4
te::rendre w4 $doc
w4 tag add surligne 3.0 3.2
t "tag non typé : ignoré" {expr {[te::lire w4] eq $doc}}
w4 tag add [te::Tag segment/marque style surligne] 3.0 3.2
t "tag typé, valeur hors du type" {te::lire w4}
w4 tag add [te::Tag bloc/titre niveau 7] 1.0 1.4
t "titre de niveau 7" {te::lire w4}

fauxtext::creer w7
te::rendre w7 $doc
w7 insert end "Un paragraphe tapé à la main."
t "texte hors bloc" {set d [te::lire w7]; list [type get [lindex $d end]] [lindex $d end]}

puts "--- les noms de tags"
fauxtext::creer w8
set cible "https://wiki.tcl-lang.org/page/Été 2026?q=a.b&x=1_2"
te::rendre w8 [te::document [te::titre 2 "T"] [te::para [te::m gras [te::lien $cible "x"]]]]
foreach {cle valeur index} [w8 dump -text -tag 1.0 end] {
    if {$cle eq "tagon" && [te::Tete $valeur] ne ""} {puts "  $valeur  ->  [te::Tete $valeur]"}
}
t "cible relue à l'identique" {
    set p [lindex [te::lire w8] 1]
    set m [lindex $p(para) 0]
    set l [lindex $m(marque.contenu) 0]
    expr {$l(lien.cible) eq $cible}
}

puts "--- la syntaxe des notes"
foreach n {
    {gras,rouge,italique}
    {font.(family=helvetica,size=10),gras}
    {gras,font.size=10,font.family=helvetica}
    {size=10,size=12}
    {font=x,font.size=10}
    {font.size=10,font=x}
    {a.b.(c=1,d=2),a.e=3}
    {cible="https://tcl.tk/a,b.c?x=(1)"}
    {"une clé"="é\u00e9 \"q\" \\ fin"}
    {a=(}
    {a.(b=1}
    {a="sans fin}
    {,a}
} {
    t "  $n" {
	set d [te::LireNotes $n]
	list [te::EcrireNotes $d] $d
    }
}
t "forme canonique stable" {
    set n {"une clé"="é\u00e9 \"q\"\n",a.(z=1,b.c=2)}
    set c [te::EcrireNotes [te::LireNotes $n]]
    expr {[te::EcrireNotes [te::LireNotes $c]] eq $c}
}

fauxtext::creer w5
te::rendre w5 $doc
w5 tag add sel 3.0 3.5
t "la sélection est ignorée" {expr {[te::lire w5] eq $doc}}

fauxtext::creer w6
te::rendre w6 [te::document [te::para "a" "b"]]
t "deux textes voisins fusionnent" {te::lire w6}

puts "--- apparence : un tag d'aspect par morceau, styles cumulés"
fauxtext::creer w9
te::rendre w9 $doc
proc aspects {w} {
    set res {}
    foreach {cle valeur index} [$w dump -text -tag 1.0 end] {
	if {$cle eq "tagon" && [string match aspect* $valeur]} {lappend res "$index $valeur"}
    }
    return $res
}
t "para, gras, italique, gras dans italique" {lrange [aspects w9] 1 5}
w9 tag add [te::Tag segment/marque style italique] 3.3 3.8
te::habiller w9
t "après italique sur « texte », te::habiller" {lrange [aspects w9] 1 4}
t "relu : les aspects sont ignorés" {set p [lindex [te::lire w9] 1]; lrange $p(para) 0 1}

puts "--- écrire en gabarit : sans guillemets"
te::styles {
    gras     {poids bold}
    italique {pente italic}
    titre1   {taille 18 poids bold couleur #1d2433}
    titre2   {taille 14 poids bold}
    titre3   {taille 12 poids bold pente italic}
    code     {famille Courier taille 10 couleur #4a5468}
    puce     {marge 16}
    alerte   {couleur #b4531f souligne 1}
}
set main [te::document \
    [te::titre 1 "Texte enrichi"] \
    [te::para "Un " [te::m gras "texte"] " avec un passage " \
	[te::m italique "en italique, " [te::m gras "et gras dedans"]] "."] \
    [te::puces {"premier point"} [list "second, avec un " [te::lien https://tcl.tk "lien"]]]]
set gab [te::document \
    [te::h 1 {Texte enrichi}] \
    [te::p {Un [gras -- texte] avec un passage
	[italique -- en italique, [gras -- et gras dedans]].}] \
    [te::ul {premier point} {second, avec un [lien@cible="https://tcl.tk" -- lien]}]]
t "même document qu'à la main"   {expr {$gab eq $main}}
t "mêmes types"                  {expr {[lmap b $gab {type get $b}] eq [lmap b $main {type get $b}]}}
fauxtext::creer w10
te::rendre w10 $gab
t "aller-retour"                 {expr {[te::lire w10] eq $gab}}
t "un lien, attribut cité"       {set p [te::p {[lien@cible="https://a.b/c?x=1,2" -- ici]}]; set s [lindex $p(para) 0]; list [type get $s] $s(lien.cible)}
t "crochets et barre échappés"   {te::p {un \[crochet\] et une barre \\ seule}}
t "un -- dans le contenu"        {te::p {[gras -- a -- b]}}
t "balise inconnue"              {te::p {[grass -- x]}}
t "attribut inconnu"             {te::p {[lien@href=x -- y]}}
t "attribut manquant"            {te::p {[lien -- y]}}
t "style hors du type"           {te::p {[marque@style=surligne -- y]}}
t "sans --"                      {te::p {a [gras b]}}
t "crochet non fermé"            {te::p {a [gras -- b}}

puts "--- te::ecrire : un interpréteur sûr, des noms courts"
set ecrit [te::ecrire {
    h 1 {Texte enrichi}
    p {Un [gras -- texte] avec un passage
       [italique -- en italique, [gras -- et gras dedans]].}
    ul {premier point} {second, avec un [lien@cible="https://tcl.tk" -- lien]}
}]
t "même document"                {expr {$ecrit eq $main}}
t "types"                        {lmap b $ecrit {type get $b}}
t "code"                         {lmap b [te::ecrire {code {set x [list 1 2]}}] {type get $b}}
t "pas de fichier"               {te::ecrire {open /etc/passwd}}
t "pas de te::"                  {te::ecrire {te::p x}}
t "erreur de balise"             {te::ecrire {p {[grass -- x]}}}
t "rien n'est gardé ensuite"     {te::ecrire {}}
