# Les proto-types : notes inventoriées, faute de frappe, vérification après coup
type def point {{double}x; {double}y}
type debug on
set a {pointt}{3 4}
set v {vitesse}12.5
set w {vitesse}abc
type debug off
type def vitesse double
puts [type debug report]
