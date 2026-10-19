proc t {script} { set c [catch {uplevel 1 $script} m]; puts "$c: $m" }
type def pt {{int}x; {int}y}
type def seg {{pt}a; {pt}b}
set {list.int}L 10 20 30 40 50
t {set L(1:3)}
t {set L(end-1,0)}
t {set L([(1+2)])}
t {set L($::i)}
set i 4
t {set L(0:2,4)}
t {set L([(1+1)]) 99}
t {set L}
set {seg}S {1 2} {3 4}
t {set S((a,b).x)}
t {set S(b.y) 9}
t {set S}
t {set S(a.(x,y)) 5 6}
t {set S}
t {set {dict.int}D end 1 x 2}
t {set D('end')}
t {set D(end)}
t {set L(0,"|",1)}
t {set L(1.)}
t {set L('end')}
