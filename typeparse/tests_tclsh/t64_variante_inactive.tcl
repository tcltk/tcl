# t64 : lire une variante inactive : cast de la valeur courante, ou message qui nomme
#   les deux variantes ; écrire dans une variante inactive : message qui guide.
type def S {{int}i, {string}s}
type def pt {{int}x; {int}y}
type def U {{pt}p, {int}n}
set {S}v {i}5
set {U}u {n}3
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-30s %s %s" $l $rc $m] }
t "v(i) active"      {set v(i)}
t "v(s) inactive"    {set v(s)}
t "u(p) inactive"    {set u(p)}
t "u(p.x) inactive"  {set u(p.x)}
t "u(n) active"      {set u(n)}
t "v(zz) inconnue"   {set v(zz)}
t "set v(s) abc (écrit)" {set v(s) abc; list $v [type get $v]}
t "set u(p.x) 1"     {set u(p.x) 1}
set {S}v {i}5
set v {s}abc; puts "$v [type get $v]"
set {U}u {n}3
set u {p}{1 2}; puts "$u [type get $u] $u(p.x)"
