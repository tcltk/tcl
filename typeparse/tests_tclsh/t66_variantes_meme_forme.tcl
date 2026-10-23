# t66 : variantes de même forme. Déduction par la plus stricte ; si plusieurs
#   variantes également strictes conviennent : erreur qui propose {variante}valeur
#   (et set {S}v(variante) valeur). La variante choisie survit dans les listes,
#   les produits, et aux modifications (la variante courante départage).
proc t {l s} { set rc [catch {uplevel 1 $s} m]; puts [format "%-30s %s %s" $l $rc $m] }
type def A {{int}i, {int}j}
type def B {{int}i, {double}d, {string}s}
type def P {{int}x; {int}y}
type def Q {{int}a; {int}b}
type def C {{P}p, {Q}q}
type def W {{int}, {int}}
type def R {{A}a; {int}n}
puts "-- déduction"
t "B 5 : int, la plus stricte"  {set {B}v 5; type get $v}
t "B 2.5"                       {set {B}v 2.5; type get $v}
t "B abc"                       {set {B}v abc; type get $v}
t "A 5 : i et j"                {set {A}v 5}
t "C {1 2} : p et q"            {set {C}v {1 2}}
t "W 5 : anonymes"              {set {W}v 5}
t "A {j}5"                      {set {A}v {j}5; type get $v}
t "A v(j) 5"                    {set {A}v(j) 5; type get $v}
t "type cast A 5"               {type cast A 5}
t "type cast A {j}5"            {type get [type cast A {j}5]}
t "type cast B {s}5"            {type get [type cast B {s}5]}
set {A}v {j}5
puts "-- la variante survit"
t "copie"                       {set {A}w $v; type get $w}
t "liste"                       {set {list.A}L $v {i}6; lmap e $L {type get $e}}
t "liste nue"                   {set {list.A}L 7}
t "lappend"                     {set {list.A}L; lappend L $v; type get [lindex $L 0]}
t "lappend nu"                  {set {list.A}L; lappend L 3}
t "produit"                     {set {R}r $v 1; list $r [type get $r(a)]}
t "produit {a}"                 {set {R}r {a}$v {n}1; type get $r(a)}
t "B {s}5 dans une liste"       {set {B}b {s}5; set {list.B}M $b; type get [lindex $M 0]}
puts "-- modifications : la variante courante départage"
t "set v 8"                     {set {A}v {j}5; set v 8; list $v [type get $v]}
t "incr v"                      {set {A}v {j}5; incr v; list $v [type get $v]}
t "set v(i) 8"                  {set {A}v {j}5; set v(i) 8; type get $v}
t "set v {i}8"                  {set {A}v {j}5; set v {i}8; type get $v}
t "L(0) 4"                      {set {A}u {j}5; set {list.A}L $u; set L(0) 4; type get [lindex $L 0]}
t "lset L 0 9"                  {set {A}u {j}5; set {list.A}L $u; lset L 0 9; type get [lindex $L 0]}
t "r(a) 7"                      {set {A}u {j}5; set {R}r $u 1; set r(a) 7; type get $r(a)}
t "r(n) 2"                      {set {A}u {j}5; set {R}r $u 1; set r(n) 2; type get $r(a)}
