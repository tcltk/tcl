# t49 : le cast se fait dans les constructeurs (patch types-binaires-cast-constructeur).
# Hors constructeur, {A}$x empile A ; un constructeur dépile depuis la plus récente
# tant que l'annotation se résout, s'arrête à la première qui ne se résout pas,
# vérifie la cohérence de ce qu'il a dépilé, caste vers la plus récente, puis le
# contrat de la variable fait le dernier contrôle. Chaque ligne reprend un cas du
# doc « Constructeurs de variables : annotations, types et casts ».
type def mm    double
type def dist  mm
type def kg    double
type def px    int
type def port  int(u,16)
type def Shape {{double}circle, {list.double}rect}

# id : rend son argument tel quel sans être un constructeur (ce n'est pas
# une proc), pour empiler plusieurs annotations : {b}[id {a}5] -> @a.b
interp alias {} id {} return -level 0
# show : type résolu @ pile (comme dans t48)
proc show {v} {
    set t [type get $v]; set n [note get $v]
    expr {$n eq "" ? $t : "$t@$n"}
}
# Chaque cas tourne dans une portée neuve.
proc T {label script} {
    set r [catch {apply [list {} $script]} m]
    puts [format "%-5s %s" $label [expr {$r ? "ERREUR: $m" : "ok: $m"}]]
}

puts "-- côté nom : set {A}y valeur"
T N1  {set {mm}y 5; show $y}
T N2  {set {mm}y abc}
T N3  {set {zz}y 5}
T N4  {set {-v}y 5}
T N5  {set {int(}y 5}
T N6  {set {mm}y 5; set {px}y 6; show $y}
T N7  {set {mm}y 5; set {}y abc; list [show $y] [set y xyz]}
T N8  {set {Shape}y {circle:}3.0; show $y}

puts "-- côté valeur, variable libre : set y {A}\$x"
T V1  {set y 5; show $y}
T V2  {set x 5; set y {mm}$x; show $y}
T V3  {set x abc; set y {mm}$x}
T V4  {set x 5; set y {zz}$x; show $y}
T V5  {set x 5; set y [lindex [list {-v}$x] 0]; list [show $y] $y}
T V6  {set {mm}x 5; set y {}$x; list [show $y] [set y abc]}
T V7  {set {mm}x 5; set h {hello}$x; set y $h; show $y}
T V8  {set x 3.0; set y {circle}$x; show $y}
T V9  {set x {mm9}10; set y {stop}$x; set z {px}$y; list [show $x] [show $y] [show $z]}
type def mm9 double
T V10 {set x [list {mm10}1]; type def mm10 double; set w [lindex $x 0]; show $w}
T V11 {set x [list {dist11}[id {mm11}2.5]]; type def mm11 double; type def dist11 mm11; set y [lindex $x 0]; show $y}
T V11b {set x [list {stop}[id {px}[id {mm}5]]]; set y [lindex $x 0]; show $y}
T V12 {set x [list {px}[id {qq}100]]; type def qq double; set v [lindex $x 0]; show $v}
T V12b {set x [list {qq2}[id {px}100]]; type def qq2 double; set v [lindex $x 0]}
T V13 {set {mm}x 10.5; set z {px}$x; list [show $z] $z}

puts "-- côté valeur, variable typée : le contrat"
T C1  {set {int}Position {zzpx}100; show $Position}
T C2  {set {int}Position {px}100; show $Position}
T C3  {set x 5; set {int}y {port}$x; show $y}
T C4  {set x 5; set {double}y {mm}$x; show $y}
T C5  {set {dist}d 5; set {mm}y $d; show $y}
T C6  {set {dist}d 5; set {double}y $d; show $y}
T C7  {set {mm}m 5; set {dist}y $m; show $y}
T C8  {set x 5; set {mm}y {px}$x}
T C9  {set x 5; set {mm}y {double}$x; show $y}
T C10 {set {mm}y 1; set x 5; set y $x; show $y}
T C11 {set {mm}y 1; set x 5; set y {zz}$x; show $y}
T C12 {set {mm}y 1; set x 5; set y {px}$x}
T C13 {set x 3.0; set {Shape}y {circle:}$x; show $y}
T C14 {set {int(u,8)}w 5; set {int}n 7; set w $n}

puts "-- casts"
T K1  {set {mm}m 5.5; set y {kg}$m; list [show $y] $y}
T K2  {set {mm}m 5.5; set y {int}$m; list [show $y] $y}
T K3  {set x 70000; set y {port}$x}
T K4  {set x {a b}; set {list.int}y {mm}$x}
type convert px mm {x {expr {$x * 0.2645}}}
T K5  {set {px}p 100; set y {mm}$p; list [show $y] $y}

puts "-- hors constructeur"
# show est une proc : sa liaison d'argument est un constructeur. Hors
# constructeur, on lit donc la valeur avec type get / note get directement.
T H1  {set x abc; set L [list {px}$x]; list [type get [lindex $L 0]] [note get [lindex $L 0]] [lindex $L 0]}
T H2  {set x 5; list [type get {px}$x] [note get {px}$x]}
T H3  {proc r {} {set x 5; return {mm}$x}; list [type get [r]] [note get [r]] [apply {{} {set q [r]; type get $q}}]}
T H4  {proc g {a} {show $a}; set x 5; g {mm}$x}
T H4b {proc g {a} {show $a}; set x abc; g {mm}$x}
T H5  {set {mm}m 5; lassign [list {zz}$m] u; show $u}
T H6  {set x 5; foreach u [list {mm}$x] {set s [show $u]}; set s}
T H7  {expr {u is mm = 5}; show $u}
T H8  {set a {zz}5; expr {$a + 1}}

puts "-- l'annotation vide {}"
# Une variable ne garde jamais un type connu en attente (son constructeur le
# résout) : pour @px, le type est défini APRÈS avoir été posé. L'affichage
# se fait sans proc (une liaison d'argument résoudrait la pile).
proc vue {} {return {[expr {[note get $V] eq "" ? [type get $V] : "[type get $V]@[note get $V]"}]}}
set k 0
foreach {nom code} {
    nu      {set x 5}
    mm@     {set {mm}x 5}
    @zz     {set x {zz}5}
    @px     {set x {PX}5; type def PX int}
    mm@zz   {set {mm}x0 5; set x {zz}$x0}
    mm@px   {set {mm}x0 5; set x {PX}$x0; type def PX int}
} {
    incr k
    set code [string map [list PX px$k] $code]
    T "E-$nom" [string map [list @CODE@ $code @V1@ [string map {V y1} [vue]] \
	    @V2@ [string map {V y2} [vue]] @V3@ [string map {V y3} [vue]] \
	    @H@ [string map {V {[lindex [list {}$x] 0]}} [vue]]] {
        @CODE@
        set a [catch {set {}y1 $x; return -level 0 @V1@} r1]
        set b [catch {set y2 {}$x; return -level 0 @V2@} r2]
        set c [catch {set {mm}y3 1; set y3 {}$x; return -level 0 @V3@} r3]
        set d [catch {return -level 0 @H@} r4]
        list nom=$r1 libre=$r2 typée=$r3 hors=$r4
    }]
}
T E-suite {set y {pxs}5; type def pxs int; set {}a $y; set z $a; list [note get $a] [type get $a] [type get $z]}

puts "-- type cast"
T TC1 {set x 5; show [type cast px $x]}
T TC2 {set {mm}m 5.5; set c [type cast kg $m]; list [show $c] $c}
T TC3 {type cast zz 5}
T TC4 {type cast px abc}

puts "-- paramètres fantômes : des unités (le paramètre distingue les instances)"
T F1  {type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double; list ok}
type def m       {grandeur(L)}
type def mmF     {grandeur(L, 0.001)}
type def kgF     {grandeur(M)}
type def couple  {grandeur(M.L2.T-2)}
type def energie {grandeur(M.L2.T-2)}
T F2  {set {mmF}a 1500; list [show $a] $a [expr {$a * 2}]}
T F3  {set {mmF}a 1500; set {m}b 1; set b $a}
T F4  {set {mmF}a 1500; set c {kgF}$a}
T F5  {set {couple}t 3; set {energie}u $t}
T F6  {set {mmF}a 1500; set {double}d $a; show $d}
T F7  {set {grandeur(L)}x 1; set {grandeur(L,1.0,0.0)}y 2; set x $y; show $x}
T F8  {type alias {ga({string}u)} double}
type convert mmF m {x {expr {$x / 1000.}}}
T F9  {set {mmF}a 1500; set z {m}$a; list [show $z] $z}
