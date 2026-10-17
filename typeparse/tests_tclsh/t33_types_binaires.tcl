puts [type binary int(u,16,be)]
puts [type binary int(s,32,le)]
puts [type binary double(64,be)]
puts [type binary bitarray(16,msb0,be)]
puts [type binary int(u,16)]
catch {type binary be.int(u,16)} m; puts $m
catch {type binary int(u,8,le,be)} m; puts $m
catch {type binary string} m; puts $m
# widening
set {int}n 1
set {int(u,8,be)}u 3
puts [catch {set n $u} m]$m
puts "n=$n [type get $n]"
set {double}x 0.5
puts [catch {set x $u} m2]$m2
puts "x=$x [type get $x]"
puts [catch {set {int(s,16)}s 5; set {int(s,32)}t 1; set t $s} m3]$m3
puts [catch {set {int(u,8)}v 5; set s $v} m4]$m4
puts [catch {set {int(u,16,be)}p 1; set {int(u,16,le)}q 2; set p $q; set q $p} m5]$m5
puts "p=$p [type get $p]"
puts [catch {set {int(u,8)}w 5; set w $n} m6]$m6
puts [catch {set {int(u,8)}w2 5; set w2 $t} m7]$m7
type def age int
set {age}a 5
puts [catch {set {int}n2 0; set n2 $a} m8]$m8
type def port int(u,16)
set {port}pt 80
set {int(u,32)}big 1
puts [catch {set big $pt} m9]$m9
puts [catch {set {int(u,16)}plain 5; set pt $plain} m10]$m10
puts [catch {set {int}z 1; set z [expr {$u + 1}]} m11]$m11
set {bitarray(16,msb0)}bm 5
set {bitarray(16,lsb0)}bl 5
puts [catch {set bm $bl} m12]$m12
puts [catch {set {int(u,32)}wide 0; set wide $bm} m13]$m13
set {bitarray(16,lsb0,be)}bb 1
set bb(3) 1
puts "bb=$bb $bb(0) [type get $bb] [type binary bitarray(16,lsb0,be)]"
type def pr {{int(u,16,be)}a; {int}b}
set {pr}rr a 1 b $u
puts "$rr [type binary pr]"
