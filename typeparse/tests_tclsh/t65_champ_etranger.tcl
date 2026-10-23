# t65 : étiquette de champ d'un autre produit ({aut.nom}x) en construisant un paire :
#   message qui nomme les deux produits, au lieu de « wrong # args » ou d'une
#   lecture positionnelle silencieuse.
type def paire {{string}nom; {int}n}
type def aut   {{string}nom = "x"; {int}age = 1}
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-36s %s %s" $l $rc $m] }
t "{aut.nom}x seul"            {set {paire}s {aut.nom}x}
t "{aut.nom}x {n}3"            {set {paire}s {aut.nom}x {n}3}
t "{aut.age}3 {n}1"            {set {paire}s {aut.age}3 {n}1}
t "tous étrangers"             {set {paire}s {aut.nom}x {aut.age}3}
t "{aut.zz}x (champ inconnu)"  {set {paire}s {aut.zz}x}
t "{nom}x {n}3 (correct)"      {set {paire}s {nom}x {n}3}
t "{paire.nom}x {paire.n}3"    {set {paire}s {paire.nom}x {paire.n}3; set s}
t "positionnel x 3"            {set {paire}s x 3}
