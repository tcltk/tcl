# t100 : v16u, espaces de noms des types.
#   type def NOM dans un espace de noms y range le type (::geom::point) ; un
#   nom se résout comme une commande : espace courant, namespace path, global.
#   type get et les messages donnent le nom qualifié (nom simple pour un type
#   global). Une valeur garde son type après namespace delete ou interp delete.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-34s %s" $label $r]
}
namespace eval geom { type def point {{double}x; {double}y} }
namespace eval rgb  { type def point {{int}r; {int}g; {int}b} }
type def point {{string}nom}
t "trois point"            {list [type def ::geom::point] [type def ::rgb::point] [type def point]}
t "dans geom"              {namespace eval geom {set {point}P 1 2; list [type get $P] $P}}
t "global"                 {set {point}G abc; type get $G}
t "qualifié relatif"       {set {geom::point}Q 3 4; type get $Q}
t "qualifié absolu"        {set {::rgb::point}C 1 2 3; type get $C}
t "list. qualifié"         {set {list.geom::point}L {1 2} {3 4}; list [type get $L] [type get [lindex $L 0]]}
t "relire le nom affiché"  {set {list.::geom::point}L2 {1 2}; type get $L2}
namespace eval rgb2 {type def point {{double}x; {double}y}}
t "nominal: deux formes égales" {set {rgb2::point}R 1 2; type {geom::point}$R}
namespace eval geom { type def seg {{point}a; {point}b} }
t "seg utilise geom::point" {set {geom::seg}S {1 2} {3 4}; list [type get [lindex $S 0]] [type def geom::seg]}
t "inconnu"                {set {nowhere::point}Z 1}
t "espace inconnu (def)"   {type def nowhere::x int}
t "réservé dans un espace" {namespace eval geom {type def int double}}
namespace eval app { namespace path ::geom }
t "namespace path"         {namespace eval app {set {point}P 5 6; type get $P}}
t "proc dans geom"         {proc {point}::geom::mk {{double}x; {double}y} {( $x, $y )}; type get [::geom::mk 1 2]}
t "proc args"              {proc ::geom::norme {{point}p} {expr {hypot($p(x), $p(y))}}; ::geom::norme {3 4}}
namespace eval geom { type def {grille({int}n)} {list($n).point} }
t "famille dans geom"      {set {geom::grille(2)}M {1 2} {3 4}; list [type get $M] [type get [lindex $M 0]]}
namespace eval geom { type def S {{point}at, "none"} }
t "somme"                  {set {geom::S}s {/at}{1 2}; list [type get $s] $s}
t "type forget"            {namespace eval geom {type forget S}; catch {type def ::geom::S} m; set m}
set keep $::geom::P
namespace delete geom
t "après namespace delete" {list [type get $keep] $keep [catch {set {geom::point}Y 1 2} m] $m}
namespace eval geom { type def point {{int}i} }
t "redéfini"               {set {geom::point}N 7; list [type get $N] [type def ::geom::point] [type get $keep]}
t "convert"                {type def ::vec {{double}x; {double}y}; namespace eval geom2 {type def p {{double}x; {double}y}}; type convert vec geom2::p {{v} {set v}}; set {vec}V 1 2; type get [type {geom2::p}$V]}
interp create c
c eval {namespace eval k {type def kp {{int}a; {int}b}; set {kp}V 1 2}}
set kv [c eval {set k::V}]
interp delete c
t "interp supprimé"        {list [type get $kv] $kv [llength $kv]}
oo::class create Cl { method m {} { type def op {{int}q}; set {op}z 5; type get $z } }
t "TclOO (espace de l'objet)" {set o [Cl new]; set r [$o m]; $o destroy; string match ::oo::Obj*::op $r}
proc cnames {h} {lmap {- n} [regexp -all -inline {\} ([A-Za-z_0-9]+);} $h] {set n}}
t "type export : nom C"    {namespace eval ce {type def cp {{int(s,32)}a; {double}b}}; cnames [type {c!}export ce::cp]}
namespace eval ce2 {type def cp {{int(s,32)}z}}
t "export, même nom C"     {cnames [type {c!}export ce::cp ce2::cp]}
