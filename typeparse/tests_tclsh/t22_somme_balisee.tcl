# Somme dont deux variantes ont la même forme : la balise explicite fixe la
# variante, aussi dans une liste de sommes, y compris avec lappend.
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
type def texte {{string}keyword; {string}text}
type def autre {{string}name; {string}hex}
type def bloc {{texte}tEXt, {autre}inconnu, {string}IEND}
set {texte}t keyword Comment text bonjour
set {autre}a name zzzz hex 00ff
check "scalaire tEXt" [type get [set {bloc}b {tEXt}$t]] bloc.tEXt
check "scalaire inconnu" [type get [set {bloc}c {inconnu}$a]] bloc.inconnu
check "sans balise : ambigu" [catch {set {bloc}d {Comment bonjour}} m] 1
set {list.bloc}L {tEXt}$t {inconnu}$a {IEND}{}
check "liste" [lmap e $L {type get $e}] {bloc.tEXt bloc.inconnu bloc.IEND}
check "accès tEXt" [lindex $L 0 0] Comment
set {list.bloc}M
lappend M $b
lappend M $c
check "lappend de sommes typées" [lmap e $M {type get $e}] {bloc.tEXt bloc.inconnu}
proc enproc {t a} {
    set {list.bloc}N
    lappend N [set {bloc}x {tEXt}$t]
    lappend N [set {bloc}y {inconnu}$a]
    lmap e $N {type get $e}
}
check "lappend en procédure" [enproc $t $a] {bloc.tEXt bloc.inconnu}
check "champ de la variante" [set b(tEXt.text)] bonjour
puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
