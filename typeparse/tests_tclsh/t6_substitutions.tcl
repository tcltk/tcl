# t6 : substitutions et chemins. Le chemin ne substitue RIEN : Tcl substitue
# $x, [cmd] et [(expr)] dans un mot nu et dans $L(...), et une valeur
# substituée est UNE feuille (jamais relue comme un chemin). Entre accolades,
# et dans ${...}, le chemin est littéral, comme la clé d'un tableau.
proc t {script} { set c [catch {uplevel 1 $script} m]; puts "$c: [lindex [split $m \n] 0]" }
set {list.int}L 10 20 30 40 50
t {set L([(1+2)])}
set i 4
t {set L($i)}
t {set L([expr {$i-1}])}
t {set L(0,$i)}
t {list $L(0,$i)}
set idx {0 2}
t {set L(*$idx)}
t {set L(*$idx,$i)}
set {dict.int}D file.txt 1 a.b 2
set k file.txt
t {list $D($k) $D('a.b')}
t {set D($k) 7; list $D($k)}
set p {[error INJECTION]}
t {list $L($p)}
# littéral entre accolades et dans ${...}
t {set {L($i)}}
t {set {L([(1+2)])}}
t {list ${L($i)}}
t {list ${L(0,3)}}
