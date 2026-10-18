# usage : tclsh test.tcl [chemin/vers/minidll.so|.dll]
set here [file dirname [file normalize [info script]]]
namespace eval mini {}
source [file join $here minidll.tcl]
source [file join $here .. tcl_obj.tcl]
mini::init {*}$argv

puts "add      : [mini::add 40 2]"
puts "reverse  : [mini::reverse abcdé]"
set x [string repeat ab 3]
set o [mini::objinfo $x]
puts "objinfo  : [type get $o] refCount=$o(refCount) length=$o(length) type=$o(typeName)"
puts "peek     : [mini::peek $o(bytesPtr) $o(length)]"
catch {mini::add a 1} e; puts "erreur   : $e"

# --- le modèle face à la mémoire vivante ------------------------------------
# FR : on relit le Tcl_Obj réel avec nos types binaires (tcl_obj.tcl) et on le
#      compare à ce que C a lu directement.
# EN : read the real Tcl_Obj back with our binary types and compare with what C
#      read directly.
set dump [mini::peek $o(objAddr) [type binary {sizeof?}Tcl_Obj]]
set t [type binary decode Tcl_Obj $dump]
# FR : refCount est volontairement exclu : il vaut 4 pendant l'appel de objinfo
#      (variable, argument, enveloppe Tcl, commande C) et 1 quand on relit la
#      mémoire après coup. Ce n'est pas un écart du modèle, c'est le comptage.
# EN : refCount is excluded on purpose: it is 4 during the objinfo call
#      (variable, argument, Tcl wrapper, C command) and 1 when the memory is
#      read afterwards. Not a model gap, just reference counting.
foreach {f g} {bytes bytesPtr length length typePtr typePtr} {
    set cv [string range $o($g) 0 end]
    set mv [string range $t($f) 0 end]
    puts [format "%-8s C=%-16s modèle=%-16s %s" $f $cv $mv [expr {$cv == $mv ? "OK" : "DIFFÉRENT"}]]
}
puts "refCount : C=$o(refCount) (pendant l'appel), modèle=$t(refCount) (après)"

# --- avec des liens : bytes pointe vers la chaîne, dans un référentiel ------
# FR : deux segments du processus : le Tcl_Obj et sa chaîne ; bytes devient un
#      lien vers l'adresse virtuelle, suivi dans le référentiel `live`.
# EN : two process segments: the Tcl_Obj and its string; bytes becomes a link
#      to the virtual address, followed in the `live` space.
set str [mini::peek $o(bytesPtr) [expr {$o(length) + 1}]]
set sz [type binary {sizeof?}Tcl_Obj]
catch {type binary space delete live}
type binary space memory $dump$str [list [list $o(objAddr) 0 $sz] [list $o(bytesPtr) $sz [expr {$o(length)+1}]]] -name live
type def Tcl_ObjLive {
    {Tcl_Size}refCount;
    {*(memory://live/,u64le,null).cstring}bytes;
    {Tcl_Size}length;
    {ptr}typePtr;
    {string(raw,16)}internalRep
}
set l [type binary decode Tcl_ObjLive $dump]
puts "lien     : bytes -> \"$l(bytes)\" (refCount=$l(refCount) length=$l(length))"
puts "cohérent : [expr {$l(bytes) eq $x}]"
