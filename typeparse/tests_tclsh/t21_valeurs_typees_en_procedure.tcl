# Une valeur typée garde son type quand elle passe par un argument de
# procédure ou par un "set a $b" compilé : $a(champ) reste utilisable.
set nfail 0
proc check {what got want} {
    global nfail
    if {$got eq $want} {puts "ok: $what"} else {incr nfail; puts "ECHEC: $what -> '$got' (attendu '$want')"}
}
type def pt {{int}x; {int}y}
set {pt}p x 1 y 2
proc arg {t} {list [type get $t] $t(x) $t(y)}
check "argument de procédure" [arg $p] {pt 1 2}
proc argdef {{t ""}} {type get $t}
check "argument ordinaire" [argdef plain] {}
proc dernier {a t} {type get $t}
check "dernier argument" [dernier 0 $p] pt
proc copie {} {set {pt}q x 3 y 4; set r $q; list [type get $r] $r(x)}
check "set r \$q compilé" [copie] {pt 3}
proc viaLindex {} {set {pt}q x 5 y 6; set s [lindex [list $q] 0]; set s(y)}
check "lindex puis accès" [viaLindex] 6
proc strict {t} {catch {set t(x) abc} m; set m}
check "l'argument typé est strict" [strict $p] {expected integer but got "abc"}
proc brut {t} {set t "texte"; set t}
check "argument non typé libre" [brut abc] texte
proc imbrique {f} {set f(payload.payload)}
type def a {{int}n}
type def b {{a}payload}
type def c {{b}payload}
set {a}aa n 7; set {b}bb payload $aa; set {c}cc payload $bb
check "chemin imbriqué en argument" [imbrique $cc] {7}
puts [expr {$nfail ? "$nfail ECHEC(S)" : "tout est ok"}]
