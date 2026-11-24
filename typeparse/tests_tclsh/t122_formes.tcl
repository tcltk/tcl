# t122 : formes d'un type (type def T DEF {-compact}F) : le même type sous
# deux dispositions ; une forme entre telle quelle là où T est attendu ;
# vers la forme, un cast écrit ; type cast passe de l'une à l'autre.
type def poly list.double {-compact}polyc
proc f {{poly}p} { list [type get $p] [type is {compact?}$p] }
proc g {{polyc}p} { type get $p }
proc h {{poly}p} { set p [type cast poly {1 2 3}]; type get $p }
set o [type cast poly {1 2 3}]
set c [type cast polyc $o]
puts "cast : [type get $o] [type is {compact?}$o] / [type get $c] [type is {compact?}$c]"
set r [type cast poly $c]
puts "retour : [type get $r] [type is {compact?}$r] $r"
puts "{poly} reçoit polyc : [f $c] ; poly : [f $o]"
puts "{polyc} reçoit polyc : [g $c]"
catch {g $o} e; puts "{polyc} reçoit poly : $e"
puts "réaffectation dans la famille : [h $c]"
catch {set {polyc}x $o} e; puts "set {polyc} : $e"
set {poly}y $c; puts "set {poly} d'un polyc : [type get $y]"
puts "définition : [type def polyc]"
catch {type def pp list.string {-compact}ppc} e; puts "list.string : [string range $e 0 40]..."
puts "pp retiré : [catch {type def pp}]"
catch {type def qq list.double {-strict}qqc} e; puts "option : $e"
catch {type def rr list.double rrc} e; puts "sans option : $e"
catch {type def {pa({type}T)} {list.$T} {-compact}pac} e; puts "famille : $e"
