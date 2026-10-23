# geo_types.tcl --
#
# FR : Produit geo_types.h, l'en-tête C des types de la géométrie, depuis
#      les type def (type c) : les struct, et les _Static_assert qui vérifient
#      que le compilateur range comme Tcl.
#        tclsh geo_types.tcl > geo_types.h
# EN : Writes geo_types.h (the C header of the geometry types) from the type
#      defs, with the _Static_assert checking the C compiler lays them out as
#      Tcl does.
set ici [file dirname [file normalize [info script]]]
source [file join $ici .. .. layout binaliases.tcl]
type def point  {{double}x; {double}y}
type def pointC {{f64le}x; {f64le}y}
type def polyC  {{u32le}n; {list($n).pointC}pts}
puts -nonewline [type c point pointC polyC]
