# Spécialisation externe : P(i.champ) dans une liste de produits ordinaire
type def pt {{double}x; {double}y}
puts "avant : [list [type ops list.pt] [type ops list.double]]"
load ./libproduits[info sharedlibextension] Produits
puts "après : [list [type ops list.pt] [type ops list.double]]"
proc lire {n} {
    set {list.pt}P {1.5 2.5} {3.5 4.5}
    set s 0.0
    for {set k 0} {$k < $n} {incr k} { set i [expr {$k % 2}]; set s [expr {$s + $P($i.x)}] }
    list $s [type canonic $P(1.y)]
}
puts "lire : [lire 4], appels [produits::appels]"
