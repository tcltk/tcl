type def paire {{string}nom = "?"; {int}age = 0}
type def png {{string}signature = "SIG"; {list.string}chunks = {}}
type def aut {{string}nom = "x"; {int}z = 1}
set {paire}p {paire.nom}Jean
puts "$p | [type get $p]"
set {paire}q {paire.age}12
puts $q
set {png}r {png.chunks}{a b}
puts $r
puts [catch {set {paire}s {paire.zz}x} m]; puts $m
puts [catch {set {paire}s2 {aut.nom}x} m2]; puts $m2
proc f {} {set {paire}u {paire.nom}Zed; return $u}
puts [f]
set {paire}w Jean 3; puts $w
