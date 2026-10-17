# minidll.tcl --
# FR : charge la bibliothèque et annote le résultat de objinfo_raw en valeur
#      de type `objinfo` (produit). C ne sait pas fabriquer une valeur annotée :
#      il renvoie la liste des champs, dans l'ordre, et Tcl l'annote.
# EN : loads the library and annotates objinfo_raw's result as an `objinfo`
#      product value. C cannot build an annotated value: it returns the field
#      list, in order, and Tcl annotates it.
namespace eval mini {variable dir [file dirname [file normalize [info script]]]}
proc mini::init {{lib ""}} {
    if {$lib eq ""} {set lib [file join $::mini::dir minidll[info sharedlibextension]]}
    ::load $lib Minidll
    type def objinfo {{int}objAddr; {int}refCount; {int}bytesPtr; {int}length; {int}typePtr; {string}typeName}
    proc ::mini::objinfo {v} {
        return [set {objinfo}r {*}[::mini::objinfo_raw $v]]
    }
}
