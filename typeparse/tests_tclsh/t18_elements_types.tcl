# Éléments typés d'une liste : foreach, lindex, lmap rendent des valeurs
# typées ; somme : {i}5 et {Val.i}5 ; type get rend Val.i
proc check {what got want} {
    if {$got eq $want} {puts "ok: $what"} else {puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
set {list.int}N 4 5 6
foreach e $N {lappend r [type get $e]}
check "foreach list.int" $r {int int int}
check "lindex list.int" [type get [lindex $N 1]] int
check "lmap list.int" [lmap e $N {type get $e}] {int int int}
catch {unset e}
type def Val {{int}i, {string}s}
set {list.Val}L {i}5 {s}hello {s}7 {Val.i}8
check "L" $L {5 hello 7 8}
catch {unset e}
check "variantes foreach" [lmap e $L {type get $e}] {Val.i Val.s Val.s Val.i}
check "L(2) garde la balise" [type get $L(2)] Val.s
check "lindex L 2" [type get [lindex $L 2]] Val.s
set {list.Val}P 5 hello
catch {unset e}
check "sans balise (déduite)" [lmap e $P {type get $e}] {Val.i Val.s}
catch {set {list.Val}M {i}abc} m
check "refus {i}abc" $m {value "abc" is not valid for that variant of Val}
catch {set {list.Val}M {Val.zz}1} m
check "refus Val.zz" $m {unknown type "zz"}
set {Val.s}w 7
check "set {Val.s}w 7" [type get $w] Val.s
set {Val}v {Val.i}9
check "{Val}{Val.i}" [type get $v] Val.i
# liste de strings : pas d'enveloppe, lindex imbriqué intact
set {list}Q {list}{a b {c d}}
check "type get élément string" [type get [lindex $Q 0]] {}
check "Q(0)" [type get $Q(0)] string
check "lindex Q 2 1" [lindex $Q 2 1] d
type def Complex {{double}re = 0.0; {double}im = 0.0}
set {list.Complex}Z {Complex}{1 2} {Complex}{3 4}
check "Z" [lmap z $Z {type get $z}] {Complex Complex}
check "modif lset" [expr {[lset N 0 9; set N] eq {9 5 6}}] 1
catch {unset e}
check "après lset" [lmap e $N {type get $e}] {int int int}
catch {lset N 0 x} m
check "lset refus" $m {expected integer but got "x"}
# lindex à plusieurs indices ne détruit plus l'annotation de la variable
set {list}R {list}{a b {c d}}
check "lindex R 2 1" [lindex $R 2 1] d
check "R reste typée" [type get $R] list
check "R(0)" [type get $R(0)] string
set {list.int}K 1 2 3
check "lindex K 0 0" [lindex $K 0 0] 1
check "K reste typée" [type get $K] list<int>
# list.string : éléments atomiques ; {list} nue : Tcl ordinaire
set {list}A {hello world} {coucou à vous}
catch {unset e f}
set n 0; foreach e $A {foreach f $e {incr n}}
check "list nue : 5 mots" $n 5
set {list.string}S {hello world} {coucou à vous}
check "list.string type" [type get $S] list<string>
check "list.string llength" [llength $S] 2
set n 0; catch {unset e f}; foreach e $S {foreach f $e {incr n}}
check "list.string : 2 écritures" $n 2
check "élément atomique" [type get [lindex $S 1]] string
check "lindex S 1 1 vide" [lindex $S 1 1] {}
check "lindex S 1" [lindex $S 1] {coucou à vous}
check "string range sur élément" [string range [lindex $S 1] 0 5] coucou
set {list}B $S
set {list.string}C $A
check "list <- list.string" [type get $B] list
check "list.string <- list" [type get $C] list<string>
check "is list.string" [expr {S is list.string}] 1
check "is list" [expr {S is list}] 1
# atomicité à tous les niveaux d'imbrication
catch {unset e f g a b c d x}
set {list.string}D {hello world} {how are you ?}
set r {}
foreach e $D {foreach f $e {lappend r [type get $f]:$f}}
check "double boucle : type conservé" $r {{string:hello world} {string:how are you ?}}
set r {}
foreach e $D {foreach f $e {foreach g $f {lappend r [type get $g]:$g}}}
check "triple boucle : atomique" $r {{string:hello world} {string:how are you ?}}
set r {}
foreach a $D {foreach b $a {foreach c $b {foreach d $c {foreach x $d {lappend r $x}}}}}
check "5 niveaux : atomique" $r {{hello world} {how are you ?}}
check "lindex D 1 0 0" [lindex $D 1 0 0] {how are you ?}
check "lindex D 1 0 1" [lindex $D 1 0 1] {}
check "lmap atomique" [lmap e $D {lmap f $e {type get $f}}] {string string}
