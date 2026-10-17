proc t {script} { set c [catch {uplevel 1 $script} m]; puts "$c: $m" }
type def point {{double}x; {double}y}
type def seg {{point}a; {point}b}
set {seg}S a {1 2} b {3 4}
t {set S(b.y)}
t {set S(b.y) 9.5}
t {set S(a.x,b.x) 0 100}
t {set S}
t {set S(a.z)}
t {set S(a.x) foo}
set ::niveau 3
type def trace {{int}niveau = $::niveau; {string}texte; {int}k = 0}
t {set {trace}t1 texte a; set t1(niveau)}
set ::niveau 5
t {set {trace}t2 texte b; set t2(niveau)}
t {set t1(niveau)}
proc ::fake {} {return 1000}
type def {horo({type}T,{cmd}h = ::fake)} {{int}date = $h; {$T}valeur; {int}k=0}
t {set {horo(string,::fake)}h1 valeur x; set h1(date)}
set {point}Q 1 2
type def holder {{var.point}p; {int}n}
set {holder}H p ::Q n 3
t {set H(p.y)}
t {set H(p.y) 77; set Q}
proc g {} { set {point}loc 5 6; set {holder}hh p loc n 1; set hh(p.x) 50; return $loc }
t g
t {set Q(x.y) 1}
t {set {var}u ::Q; set u(x)}
t {unset Q; set H(p.x)}
t {info vars H}
