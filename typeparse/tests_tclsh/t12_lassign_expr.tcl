set L {10 20 30 40}
puts "r=[expr {(a,b,c,d)=$L}]  a=$a b=$b c=$c d=$d"
expr {(x,y)=$L}; puts "x=$x y=$y (reste ignoré)"
expr {(p,q,r,s,t)=$L}; puts "t=<$t> (manquant => vide)"
puts [expr {(1,2)==(1,2)}]
puts [expr {(m,n)=[list 3 4]; $m*$n}]
puts [((u,v)=[list 5 6])]
proc swap {a b} {((x,y)=[list $b $a]; [list $x $y])}
puts [swap 1 2]
proc f {L} {((h,t)=$L; [list $h $t])}
puts [f {7 8}]
set {int}n 0
catch {expr {(n,z)=[list abc 1]}} m; puts "typé: $m"
puts [expr {(a)=5}]; puts $a
# fonctions dans une affectation
puts [expr {(re,im)=[list [expr {hypot(3,4)}] 1]; $re+$im}]
