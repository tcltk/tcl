# version.tcl -- quelle version du prototype TIP 764 ce tclsh contient-il ?
#                which TIP 764 prototype version is built into this tclsh?
#
#   tclsh typeparse/version.tcl
#
# Depuis la v16b, le tclsh le dit lui-même : tcl::unsupported::typesversion.
# Avant, la version se reconnaît à ce que chaque livraison a ajouté :
#   v16a  codec C des chaînes et des sommes balisées (encode bien plus rapide
#         qu'avec ::tcl_typebinaryc 0)
#   v15z  lappend dans une liste typée en temps linéaire (avant : quadratique)
#   v15x  tcl::unsupported::compact cells
#   v15x et v15y ne se distinguent pas de l'extérieur (v15y n'a fait qu'accélérer).

proc ms {script} {
    set t [lindex [time {uplevel 1 $script} 1] 0]
    expr {$t / 1000.0}
}

if {[info commands ::tcl::unsupported::typesversion] ne ""} {
    puts "version : [tcl::unsupported::typesversion]"
    return
}
if {[info commands ::type] eq ""} {
    puts "ce tclsh n'a pas le prototype (pas de commande type)"
    return
}

# lappend : 1000 puis 4000 ajouts ; linéaire -> rapport ~4, quadratique -> ~16
type def vpoint {{double}x; {double}y}
proc vremplit {N} {set {list.vpoint}L; for {set i 0} {$i < $N} {incr i} {lappend L [list $i 1]}; llength $L}
vremplit 200
set t1 [ms {vremplit 1000}]
set t4 [ms {vremplit 4000}]
set lineaire [expr {$t4 < 40.0 && $t4 < 8.0 * max($t1, 0.05)}]

# codec C des chaînes : encode avec et sans le codec C (la couche Tcl doit être chargée)
set codec 0
if {![catch {
    source [file join [file dirname [file normalize [info script]]] layout filebinary.tcl]
    type def ventete {{string(utf-8,8)}nom; {string(utf-8,z)}texte}
    set {ventete}e Été cstring
    type binary encode ventete $e
}]} {
    set tc [lindex [time {type binary encode ventete $e} 2000] 0]
    set ::tcl_typebinaryc 0
    set tt [lindex [time {type binary encode ventete $e} 2000] 0]
    set ::tcl_typebinaryc 1
    set codec [expr {$tt > 4.0 * max($tc, 0.01)}]
}

set cells [expr {![catch {tcl::unsupported::compact cells list.vpoint [binary format d2 {1 2}]}]}]

if {$codec && $lineaire} {
    set v v16a
} elseif {$lineaire} {
    set v v15z
} elseif {$cells} {
    set v "v15x ou v15y"
} else {
    set v "v15w ou antérieure"
}
puts "version : $v (déduite : lappend [format %.1f $t1] ms / [format %.1f $t4] ms,\
    codec C des chaînes [expr {$codec ? "oui" : "non"}], compact cells [expr {$cells ? "oui" : "non"}])"
