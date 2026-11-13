# Sondes pour doc/PISTES-any-string.md : valeur nue, méta-types, any.
#
# Chaque sonde compare le comportement actuel à celui que propose la note :
#   OK      le comportement actuel est déjà celui de la proposition
#   ÉCART   il diffère (la note dit pourquoi)
# Lancer : tclsh doc/any/sondes.tcl  (sortie de référence : sondes.out)

set ok 0; set ecarts 0
proc sonde {nom attendu script} {
    global ok ecarts
    # (dans une portée neuve : une variable typée ne doit pas fuir d'une sonde à l'autre)
    if {[catch {apply [list {} $script]} r]} {set r "ERREUR: $r"}
    if {$r eq $attendu} {
	incr ok
	puts [format "OK     %-46s %s" $nom $r]
    } else {
	incr ecarts
	puts [format "ÉCART  %-46s obtenu : %s" $nom $r]
	puts [format "       %-46s proposé : %s" "" $attendu]
    }
}
# type et notes d'une valeur
proc tn {v} {list [type get $v] [note get $v]}

type def ::tson {"null", {string}s, {int}n, {list.::tson}l, {dict.::tson}d}
type def ::Jour string

puts "== 1. les quatre états existent"
sonde "nue"              {{} {}}    {tn 3}
sonde "typée"            {int {}}   {set {int}b 3; tn $b}
sonde "notée"            {{} mm}    {set a {mm}3; tn $a}
sonde "typée et notée"   {int mm}   {set {int}g {mm}3; tn $g}

puts "\n== 2. une string annotée est un scalaire ; {} la rend nue"
sonde "lindex d'une string"        {a b c} {set {string}s "a b c"; lindex $s 0}
sonde "{} : relecture explicite"   3       {set {string}s "a b c"; llength {}$s}
sonde "{} garde les notes"         {{} mm} {set a {mm}3; set {int}b $a; tn {}$b}
sonde "type cast any garde tout"   {int mm} {set {int}g {mm}3; tn [type cast any $g]}

puts "\n== 3. type meta (absent aujourd'hui)"
sonde "type meta d'une valeur nue (int)" "bare int" {
    set i 1; incr i; set m [type meta $i]; list [type variant $m] [note get $m]}
sonde "type meta d'une valeur typée"     "typed int" {
    set {int}i 1; set m [type meta $i]; list [type variant $m] [note get $m]}

puts "\n== 4. les conteneurs gardent les notes"
sonde "list (commande) garde les notes"  {{{} int} {{} this}} {
    set L [list {int}5 {this}6]; lmap x $L {tn $x}}
sonde "{list} garde les notes"           {{{} {}} {{} this}} {
    set {list}P 5 {this}6; lmap x $P {tn $x}}
sonde "{list.any} garde les notes"       {{{} {}} {{} this}} {
    set {list.any}P 5 {this}6; lmap x $P {tn $x}}
sonde "{list.double} garde les notes"    {{double vitesse}} {
    set {list.double}D {vitesse}12.5; lmap x $D {tn $x}}
sonde "{int@this} dans un constructeur"  {int this} {set v {int@this}5; tn $v}

puts "\n== 5. un seul ordre de résolution (set, type cast, argument)"
proc ::f {{::tson}t} {type variant $t}
type convert string ::tson {{s} {type cast ::tson [list d [list lu [list s $s]]]}}
sonde "set : string -> variante s"       s {set {string}x "d {a {n 1}}"; set {::tson}T $x; type variant $T}
sonde "type cast : string -> variante s" s {set {string}x "d {a {n 1}}"; type variant [type cast ::tson $x]}
sonde "argument : string -> variante s"  s {set {string}x "d {a {n 1}}"; ::f $x}
sonde "nue : forme native lue"           d {set x "d {a {n 1}}"; set {::tson}T $x; type variant $T}
type convert any ::tson {{t} {type cast ::tson [list d [list lu [list s $t]]]}}
sonde "nue non native : la règle (set)"        d {set x "menu {d}{id}"; set {::tson}T $x; type variant $T}
sonde "nue non native : la règle (type cast)"  d {set x "menu {d}{id}"; type variant [type cast ::tson $x]}
sonde "nue non native : la règle (argument)"   d {set x "menu {d}{id}"; ::f $x}
sonde "nue native : la règle ne passe pas"     n {set x "n 3"; set {::tson}T $x; type variant $T}

puts "\n== 6. normalisation dès la construction"
sonde "int 0x10"        16     {set {int}n 0x10; set n}
sonde "int \" 3 \""     3      {set {int}n " 3 "; set n}
sonde "double 2e1"      20.0   {set {double}x 2e1; set x}
sonde "double 1"        1.0    {set {double}x 1; set x}
sonde "list.double"     {1.0 20.0} {set {list.double}L {*}{1 2e1}; set L}
sonde "string inchangée" 0.10  {set {string}s 0.10; set s}

puts "\n== 7. aller-retour (type deref aujourd'hui, type canon proposé)"
foreach {T v} {
    int 42
    double 0.1
    list.int {1 2 3}
    ::tson {d {menu {d {id {s file}}}}}
    ::tson null
} {
    sonde "deref : $T" [list [type cast $T $v] [type get [type cast $T $v]]] [string map [list @T [list $T] @v [list $v]] {
	set x [type cast @T @v]; set y [type cast @T [type deref $x]]; list $y [type get $y]
    }]
}

puts "\n== 8. nominal validé : la lecture d'une valeur nue valide aussi"
type convert string ::Jour {{s} {
    if {![regexp {^\d{4}-\d\d-\d\d$} $s]} {error "« $s » n'est pas une date"}
    return $s
}}
sonde "string invalide refusée"   refus {expr {[catch {type cast ::Jour [type cast string 10/11]}] ? "refus" : "lu"}}
sonde "nue invalide refusée"      refus {expr {[catch {type cast ::Jour 10/11}] ? "refus" : "lu"}}

puts "\n$ok OK, $ecarts ÉCART"
