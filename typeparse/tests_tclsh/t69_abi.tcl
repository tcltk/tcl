# t69 : tcl::unsupported::abi : ordre des octets, pointeurs, taille et
#   alignement (dans une structure) des types fixes, mesurés à la compilation.
set a [tcl::unsupported::abi]
puts "clés : [dict keys $a]"
puts "byteOrder = [dict get $a byteOrder] (tcl_platform : $tcl_platform(byteOrder))"
puts "pointer = [dict get $a pointer] (tcl_platform : $tcl_platform(pointerSize))"
foreach t {i8 i16 i32 i64 f32 f64 ptr} {
    set s [dict get $a size $t]; set al [dict get $a align $t]
    puts [format "%-4s taille %d  alignement %d  %s" $t $s $al [expr {$al <= $s && ($s % $al) == 0 ? "cohérent" : "INCOHÉRENT"}]]
}
