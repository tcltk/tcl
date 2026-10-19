# t52 : constructeur par chemin et lecture imbriquée.
#   set {T}R(chemin) v...  construit un produit (et ses produits imbriqués) :
#   les champs nommés reçoivent les valeurs, les autres leur défaut. Le
#   NOMBRE de valeurs choisit la répartition (jamais leur contenu) : une par
#   feuille, une par élément du groupe de tête, ou une pour tout le chemin,
#   chaque valeur ayant la forme de ce que sa lecture rendrait.
#   Pas d'espace dans le mot R(chemin) : Tcl coupe les mots aux espaces.
proc e {label script} {
    if {[catch {uplevel #0 $script} r]} {set out "ERREUR: [lindex [split $r \n] 0]"} else {set out "-> [list [string range $r 0 end]] [type get $r]"}
    puts [format "%-34s | %-58s | %s" $label $script $out]
}
type def couple {{int}x; {int}y}
type def trace  {{int}niveau = 3; {string}texte}
type def rect {{{int}x; {int}y}A ; {{int}x; {int}y}B }
type def paire  {{string}nom = "?"; {string}age = "?"}
type def couple3 {{int}x; {int}y; {int}z = 0}
set Ax 1; set Ay 2; set Bx 3; set By 4
e "1 feuilles"              {set {rect}R1(A.x,A.y,B.x,B.y) $Ax $Ay $Bx $By}
e "2 produit de groupes"    {set {rect}R2((A,B).(x,y)) $Ax $Ay $Bx $By}
e "3 un élément par groupe" {set {rect}R3(A.(x,y),B.(x,y)) [list $Ax $Ay] [list $Bx $By]}
e "3' parenthèses en plus"  {set {rect}R4((A.(x,y),B.(x,y))) [list $Ax $Ay] [list $Bx $By]}
e "4 une valeur pour tout"  {set {rect}R5((A,B).(x,y)) {{1 2} {3 4}}}
e "5 champs entiers"        {set {rect}R6(A,B) {1 2} {3 4}}
e "6 ordre libre"           {set {rect}R7(B.y,A.x,B.x,A.y) 4 1 3 2}
e "7 défauts"               {set {paire}P1(age) 12}
e "8 pas d'ambiguïté"       {set {paire}P2(nom) age}
e "9 évolution du type"     {set {couple3}C1(x,y) 1 2}
e "lecture imbriquée"       {set R2((A,B).(x,y))}
e "lecture mixte"           {set R2(A.x,B.(x,y))}
e "aller-retour"            {set {rect}R8((A,B).(x,y)) [set R2((A,B).(x,y))]}
e "écriture par groupes"    {set R2(A.(x,y),B.(x,y)) {9 8} {7 6}}
e "écriture une valeur"     {set R2((A,B).(x,y)) {{5 6} {7 8}}}
e "err: champ manquant"     {set {rect}E1(A.x,B.x,B.y) 1 3 4}
e "err: champ inconnu"      {set {rect}E2(A.z) 1}
e "err: deux fois"          {set {rect}E3(A,A.x,B) {1 2} 5 {3 4}}
e "err: nombre"             {set {rect}E4((A,B).(x,y)) 1 2 3}
e "err: forme"              {set {rect}E5(A.(x,y),B.(x,y)) {1 2 3} {3 4}}
e "err: type"               {set {rect}E6(A.x,A.y,B.x,B.y) a 2 3 4}
e "err: pas un produit"     {set {rect}E7(A.x.q,A.y,B) 1 2 {3 4}}
e "err: intervalle"         {set {rect}E8(A,0:1) 1 2}
# Les cas du tableau d'ambiguïté, écrits avec un chemin : le sens se lit
# dans le source, les données ne le changent plus.
e "amb. données = nom de champ"   {set lv texte; set {trace}T1(niveau,texte) $lv a}
e "amb. données = nom de champ 2" {set a age; set b 12; set {paire}P3(nom,age) $a $b}
e "amb. nommé partiel"            {set {trace}T2(texte) a}
e "positionnel inchangé"          {set {couple}C2 1 2}
# Dans une procédure (variables compilées, lecture par bytecode)
proc f {} {
    set {rect}R((A,B).(x,y)) 1 2 3 4
    set R(A.(x,y),B.x) {5 6} 7
    list [set R((A,B).(x,y))] $R(B.y) $R(A.x,B.x) [set R(A)]
}
e "dans une procédure"            {f}
# Parenthèses équilibrées dans l'index d'une substitution $nom(...)
# (patch index-parentheses, Tcl_ParseVarName) ; guillemets et accolades
# restent refusés.
e "\$R(A.(x,y))"                   {set {rect}R9(A,B) {1 2} {3 4}; list $R9(A.(x,y))}
e "\$R((A,B).(x,y))"               {list $R9((A,B).(x,y))}
e "\$k dans le chemin"             {set k B; list $R9($k.(x,y))}
proc g {k} { set {rect}Q(A,B) {5 6} {7 8}; list $Q($k.(x,y)) $Q((A,B).x) }
e "procédure, \$Q(\$k.(x,y))"       {g A}
e "tableau ordinaire \$a(b(c))"    {array set tab {{b(c)} 1}; list $tab(b(c)) [set tab(b(c))]}
e "non équilibré"                  {list $tab(b(c)}
# Étiquettes dans l'index d'un $ (Tcl_ParseVarIndex) : guillemets appariés,
# substitutions faites par Tcl, la valeur substituée restant opaque. Un "
# sans partenaire ramène à la règle de Tcl 9 : dans "c $dd(", il ferme le mot.
e "étiquette"                      {set {list.int}L0 10 20 30; list $L0(0,"|",1)}
e "étiquette dans une chaîne"      {set s "<$L0(0,"|",1)>"}
e "parenthèse dans l'étiquette"    {list $L0(0,")",1)}
e "substitution dans l'étiquette"  {set sep {a"b}; list $L0(0,"$sep",1)}
e "forme échappée \\\"…\\\""            {list $L0(0,\"|\",1)}
e "guillemet sans partenaire"      {list $L0(0,"|,1)}
e "info complete {a \"c \$dd(\"}"      {info complete {a "c $dd("}}
e "accolade refusée"               {list $tab({x})}
# Dépliage "*" : le groupe ou l'intervalle se fond dans le groupe parent
set {list.int}L 10 20 30 40 50
e "intervalle groupé"              {set L(0:2,4)}
e "intervalle déplié"              {set L(*0:2,4)}
e "deux dépliages"                 {set L(*0:1,*3:4)}
e "groupe déplié"                  {set R9(*A.(x,y),B.x)}
e "à droite d'un point"            {set R9((A,B).*(x,y))}
e "construction à plat"            {set {rect}Q2((A,B).*(x,y)) {5 6 7 8}}
e "écriture dépliée"               {set R9(*A.(x,y),B.x) 7 8 9; set R9}
e "racine : sans effet"            {set R9(*(A,B).(x,y))}
