set L {10 20 30 40}
expr {(a,b,c,d)=$L}; puts "plat: $a $b $c $d"
set N {{1 2} 3 {4 {5 6}}}
expr {((a,b),c,(d,(e,f)))=$N}
puts "imbriqué: a=$a b=$b c=$c d=$d e=$e f=$f"
expr {(h,(t1,t2))=[list x {y z}]}
puts "h=$h t1=$t1 t2=$t2"
expr {((p,q),r)=[list {1} 2]}; puts "manquants: p=$p q=<$q> r=$r"
# valeur de l'expression
puts [expr {(u,(v,w))=[list 1 {2 3}]}]
# dans une procédure
proc dist {P1 P2} {(
  (x1,y1) = $P1; (x2,y2) = $P2;
  [expr {hypot($x2-$x1, $y2-$y1)}]
)}
puts [dist {0 0} {3 4}]
proc cmul {z1 z2} {(
  (a,b) = $z1; (c,d) = $z2;
  [list [($a*$c-$b*$d)] [($a*$d+$b*$c)]]
)}
puts [cmul {3 4} {1 -2}]
# liste de couples : boucle
foreach pair {{1 2} {3 4}} {expr {(i,j)=$pair}; puts "i=$i j=$j"}
puts [expr {(1,2)==(1,2)}]
