# t63 : ";" et "," mêlés au même niveau refusés ; un groupe champ de produit a un nom ;
#   les variantes anonymes d'une somme restent permises.
foreach d {
  {{int}a; {int}b, {string}c}
  {{int}a, {int}b; {string}c}
  {{int}a; {{int}b, {string}c}}
  {{int}a; {{int}b, {string}c}u}
  {{{int}a; {int}b}p, {string}c}
  {{{int}a; {int}b}, {string}c}
  {{int}, {string}}
  {{int}x; {int}y}
} { incr k; puts "[format %-34s $d] -> [catch {if {$k in {5 6 7}} {type def M$k $d} else {type def M$k $d}} m] $m" }
