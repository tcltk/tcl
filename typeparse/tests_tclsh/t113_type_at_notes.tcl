# t113 (v17f) : {T@notes}valeur dans un constructeur, la forme qu'écrit
#   type canonic : T le type, puis des notes (virgules, %XX ; le point
#   reste dans la note). {@step} garde son sens (note d'instrument).
proc tn {v} {list [type get $v] [note get $v] $v}
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-32s %s" $nom $r]
}
t "set v {int@this}5"            {set v {int@this}5; tn $v}
t "deux notes"                   {set v {double@zeta,alpha}5; list [tn $v] [type canonic $v]}
t "note encodée"                 {set v {int@a%2Cb}1; note get $v}
t "note imbriquée (point)"       {set v {int@aspect.pente}1; note get $v}
t "aucune note : {int@}"         {tn {int@}5}
t "list.typednoted"              {set {list.typednoted}L {int@this}5 {double@that}2.5; lmap e $L {tn $e}}
t "list.int : la note suit"      {set {list.int}L {int@a}5 6; lmap e $L {note get $e}}
t "argument {typednoted}"        {proc f {{typednoted}x} {tn $x}; f {int@this}5}
t "le cast se fait"              {tn {int@n}0x10}
t "type canonic reçoit le mot"   {type canonic {double@vitesse}5}
type def pt113 {{int}x; {int}y}
t "aller-retour (produit)"       {set c [type canonic {pt113@a}[list 1 2]]; list $c [tn [type parse $c]]}
t "canonique relu"               {set v {double@b,a}5; set c [type canonic $v]; list $c [type canonic [type parse $c]]}
t "note d'instrument {@step}"    {note get {@step}5}
t "type inconnu"                 {set v {intt@a}5}
t "contenu refusé"               {set v {int@a}abc}
t "pas sur un nom de variable"   {set {int@a}v 5}
