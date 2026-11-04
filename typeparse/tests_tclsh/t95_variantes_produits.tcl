# t95 : v16o. La variante d'une somme dont le type est un produit se construit
# par ses champs : étiquettes (set {S.v}x {f:}a {g:}b), positionnel (set {S.v}x a b),
# ou chemin à travers la somme (set {S}x(v.(f,g)) a b, calculable). type def NOM,
# sans définition, rend la définition (de S, d'une variante S.v, d'un champ P.f).
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def {-strict}seg {
    {string}texte,
    {{string}cible; {list.seg}contenu}lien,
    {{string}style; {list.seg}contenu ?= {}}marque
}
set c [list [type {seg.texte}abc]]

puts "--- étiquettes et positionnel"
t "étiquettes"                   {set {seg.lien}s {cible:}u {contenu:}$c; list [type get $s] $s}
t "ordre libre"                  {set {seg.lien}s {contenu:}$c {cible:}u; set s(lien.cible)}
t "positionnel"                  {set {seg.lien}s u $c; list [type get $s] $s}
t "un seul argument (la liste)"  {set {seg.lien}s [list u $c]; type get $s}
t "défaut"                       {set {seg.marque}s {style:}gras; list [type get $s] $s}
t "champ inconnu"                {set {seg.lien}s {href:}u {contenu:}$c}
t "champ manquant"               {set {seg.lien}s {contenu:}$c}
t "mélange"                      {set {seg.lien}s {cible:}u $c}

puts "--- chemin à travers la somme"
t "lien.(cible,contenu)"         {set {seg}s(lien.(cible,contenu)) u $c; list [type get $s] $s}
t "lien.cible,lien.contenu"      {set {seg}s(lien.cible,lien.contenu) u $c; type get $s}
t "calculé"                      {set v lien; set ch cible,contenu; set {seg}s($v.($ch)) u $c; type get $s}
t "défaut par chemin"            {set {seg}s(marque.style) gras; list [type get $s] $s}
t "variante entière"             {set {seg}s(texte) abc; type get $s}
t "deux variantes"               {set {seg}s(lien.cible,marque.style) u x}
t "variante inconnue"            {set {seg}s(gros.cible) u}
t "champ inconnu"                {set {seg}s(lien.(href,contenu)) u $c}
type def {-deduce}N {{int}i, {string}s}
type def P2 {{N}n; {int}m}
t "somme dans un produit"        {set {P2}r(n.i,m) 4 5; list [type get $r] [type get $r(n)]}
type def style {{string}famille ?= "Helvetica"; {int}taille ?= 11; {string}poids ?= "normal"}
proc st {args} {set {style}s([join [dict keys $args] ,]) {*}[dict values $args]; return $s}
t "chemin calculé d'un produit"  {st poids bold taille 9}

puts "--- type def NOM : la définition"
type def pt {{double}x ?= 0; {double}y}
type def E {{int}code, {"ok","ko"}etat}
type def D {dict(x, y?, z?=0).int}
foreach n {seg seg.lien seg.texte pt pt.x style E E.etat D list(3).int int} {
    t "type def $n" [list type def $n]
}
t "variante inconnue"            {type def seg.zz}
t "champ inconnu"                {type def pt.zz}
t "type inconnu"                 {type def zz}
foreach n {pt style E D} {
    set d [type def $n]
    catch {type forget X}
    t "relu : $n"                [list apply {{d} {type def X $d; expr {[type def X] eq $d}}} $d]
}
