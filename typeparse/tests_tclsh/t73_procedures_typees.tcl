# t73 : procédures typées plus rapides, même sens. Éléments d'une liste typée
#   lue par chemin (V partagé) : toujours typés, par lindex comme par foreach
#   (avant : selon le partage de l'objet). Déclarations X is T = v : valeur
#   déjà de type T, valeur nue, valeur à convertir. Noms affectés dans expr :
#   variables locales compilées, global et upvar compris.
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-34s %s %s" $l $rc $m] }
type def {tree({type}T)} { {$T}value; {list.tree($T)}children }
set {tree(string)}A /usr {{bin {}} {lib {{tcl9.0 {}}}}}
t "lindex \$A(children) 0"        {type get [lindex $A(children) 0]}
t "copie puis lindex"             {set x $A(children); type get [lindex $x 1]}
proc prof {t} {
    set d 0
    foreach c $t(children) { set d [expr {max($d, [prof $c])}] }
    return [expr {$d + 1}]
}
t "foreach en procédure (récursif)" {prof $A}
type def point {{double}x; {double}y}
type def mm double
proc d1 {a} {( P is point = $a; [list [type get $P] $P(x)] )}
t "déclaration, déjà typé"        {d1 [type cast point {1.5 2.5}]}
t "déclaration, valeur nue"       {d1 {3.5 4.5}}
t "déclaration, refus"            {d1 {a b}}
proc d2 {v} {( L is mm = $v; [type get $L] )}
t "déclaration nominal"           {d2 2.5}
t "déclaration d'une note"        {d2 {cm}3.5}
proc g1 {} {( global gx; gx = 7; $gx )}
t "global puis affectation"       {list [g1] $::gx}
proc u1 {nom} {( upvar 1 $nom y; y = 9; $y )}
t "upvar puis affectation"        {set z 1; list [u1 z] $z}
proc v1 {} {( a = 1; b = $a + 1; [info locals] )}
t "locales"                       {lsort [v1]}
type def {-compact}poly list.point
set {poly}P {0 0} {1 1}
t "compact : lindex typé"         {type get [lindex $P 1]}
