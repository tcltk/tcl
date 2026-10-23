# t71 : type c : en-tête C produit depuis les types (typedef, enum, union,
#   membre flexible, macros d'initialisation, _Static_assert) et refus motivés.
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]
type def point {{double}x; {double}y}
type def pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
type def carré {{point}coin; {double}côté = 1.0}
type def couleur {"rouge", "vert", "bleu"}
type def forme {{point}p, {carré}c, "vide"}
type def tri list(3).point
type def nommé {{string}nom; {list.point}pts; {u8}r; {i16}g; {tri}t; {couleur}k; {forme}f}
type def def3 {{i32}a = 1; {double}b = 2.5}
type def m3 {{list(3).list(3).f32}m}
puts [type c point polyC carré nommé def3 m3]
type def bad1 {{int}n}
type def bad2 {{u32}n; {list($n).point}p; {u8}z}
type def bad3 {{u32be}n}
foreach t {bad1 bad2 bad3 double} {catch {type c $t} m; puts "$t: $m"}
