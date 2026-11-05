# sonde_interp.tcl -- tension T4 : une valeur typée qui survit à
# l'interpréteur qui a défini son type. Le descripteur de type est libéré avec
# l'interpréteur ; la valeur garde un pointeur vers lui (lecture après
# libération : valgrind la montre ; sans valgrind, le résultat est imprévisible).
#   valgrind tclsh sonde_interp.tcl
interp create c
c eval {type def zz {{int}a; {int}b}; set {zz}V 1 2}
set v [c eval {set V}]
interp delete c
puts [type get $v]
puts [llength $v]
