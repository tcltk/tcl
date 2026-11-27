# t98 : v16r. Dépliage : {*}T, {**}T en tête d'une annotation de livraison
# (type de retour d'une procédure, type {…}mot). La valeur est vérifiée comme
# un T, puis rendue en données Tcl ordinaires : {*} une liste (les valeurs,
# comme $P(*)), {**} un dict (les noms et les valeurs), en profondeur ; une
# somme rend sa variante (avec {**}, nommée).
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
type def point {{double}x; {double}y}
type def rect {{point}nw; {point}se}
type def inter {{point}en, "aucune"}
proc {{*}point}v {{double}h} {( 0, $h )}
proc {{**}point}vd {{double}h} {( 0, $h )}
proc {point}vt {{double}h} {( 0, $h )}
t "{*} liste"          {set r [v 5]; list $r [llength $r] [type get $r] [lassign $r a b] $a $b}
t "{**} dict"          {set r [vd 5]; list $r [dict get $r y] [type get $r]}
t "sans dépliage"      {set r [vt 5]; list $r [llength $r] [type get $r]}
proc {{*}rect}r {} {( (0, 0), (10, 10) )}
proc {{**}rect}rd {} {( (0, 0), (10, 10) )}
t "rect {*}"           {set r [r]; list $r [lindex $r 1 0] [type get [lindex $r 1]]}
t "rect {**}"          {set r [rd]; list $r [dict get $r se y]}
proc {{*}list.point}lp {} {list {1 2} {3 4}}
proc {{**}list.point}lpd {} {list {1 2} {3 4}}
t "list.point"         {list [lp] [lpd] [llength [lindex [lp] 0]]}
proc {{*}inter}i {w} {expr {$w ? [list en: 1 2] : "aucune"}}
proc {{*}inter}i2 {w} { if {$w} {return {/en}[list 1 2]}; return aucune }
proc {{**}inter}i3 {w} { if {$w} {return {/en}[list 1 2]}; return aucune }
t "somme {*}"          {list [i2 1] [i2 0]}
t "somme {**}"         {list [i3 1] [i3 0]}
t "mauvais retour"     {proc {{*}point}bad {} {list a b}; bad}
t "type {{*}T}"        {set {point}p 1 2; list [type {{*}point}$p] [llength [type {{*}point}$p]] [type {{**}point}$p] [type get [type {{*}point}$p]]}
t "type {{*}T} liste"  {list [type {{*}rect}{{1 2} {3 4}}] [type {{**}rect}{{1 2} {3 4}}]}
t "type {{*}T} erreur" {type {{*}point}{a b}}
t "type proc"          {list [dict get [type proc v] return] [dict get [type proc vd] return]}
t "type inconnu"       {proc {{*}zz}z {} {}}
type def {-compact}cl list.double
proc {{*}cl}c {} {list 1 2 3}
proc {{**}cl}cd {} {list 1 2 3}
t "compact"            {list [c] [type get [c]] [cd]}
proc {{*}point}fast {{point}p} {set p}
t "déjà typé"          {set {point}q 3 4; set r [fast $q]; list $r [llength $r] [type get $r]}
