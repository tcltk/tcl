proc t {script} { set c [catch {uplevel 1 $script} m]; puts "$c: $m" }
set {list.int}L 10 20 30 40 50
t {set {L([(1+2)])}}
set i 4
t {set {L($i)}}
t {set {L([expr {$i-1}])}}
t {set {L(0,$i)}}
