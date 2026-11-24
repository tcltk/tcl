# t124 : lappend L a b ... dans une liste typée : élément par élément, sur
# place (linéaire) ; tout ou rien ; une copie reste intacte ; en cases aussi.
type def poly list.double {-compact}polyc
proc t {} {
  set {poly}p 0.0; set p [lrange $p 1 0]
  lappend p 1 2.5 0x10
  puts "$p [type get $p]"
  catch {lappend p 3 abc 4} e; puts "refus : $e ; p=$p"
  set q $p; lappend p 7
  puts "copie intacte : q=$q p=$p"
  set {polyc}c 0.0; lappend c 1 2; puts "$c [type is {compact?}$c]"
}
t
