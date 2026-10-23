# t55 : cast d'un produit (ou d'une liste nominale) vers une collection
#   {list}$p : la liste des champs, nus ; {list.any}$p : annotés de leur type ;
#   {list.T}$p, {list(N).T}$p : validés ; {dict}$p : nom -> champ.
type def point {{double}x; {double}y}
type def poly list.point
type def cercle {{point}centre; {double}rayon}
type def S {{int}i, {point}p}
set {poly}P {0 0} {3 4}
set {point}p 3 4
set {cercle}C {1 2} 5
foreach q $P { foreach {x y} {list}$q { puts "foreach : $x,$y" } }
lassign {list}$p a b;            puts "lassign : $a $b"
puts "llength : [llength {list}$p] (le point seul : [llength $p])"
foreach {k v} {dict}$p           { puts "dict : $k=$v" }
dict for {k v} {dict}$C          { puts "dict for : $k=$v" }
proc montre {v} { return "<$v> [type get $v] : [lmap e $v {type get $e}]" }
puts "list       : [montre [type cast list $C]]"
puts "list.any   : [montre [type cast list.any $C]]"
puts "list.double: [montre [type cast list.double $p]]"
puts "list(2).int: [montre [type cast list(2).int $p]]"
puts "dict.any   : [type get [dict get [type cast dict.any $C] centre]]"
set {S}s {p}{3 4};  puts "somme      : [montre [type cast list $s]]"
puts "poly       : [montre [type cast list.point $P]]"
puts "poly nu    : [montre [type cast list $P]]"
puts "chaîne     : [string equal [type cast list $p] $p]"
foreach e {
    {type cast list.int $C}
    {type cast list(3).double $p}
    {type cast list(3).point $P}
} { catch $e m; puts "refus : $m" }
proc f {p} { set n 0; foreach c {list}$p {incr n}; return $n }
puts "dans une procédure : [f $p]"
