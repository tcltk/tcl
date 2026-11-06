# liaison.tcl -- la liaison directe en C (cproc.tcl) contre la liaison par la
# procédure typée (cproc2_type.tcl) : mêmes résultats, mêmes types, mêmes
# erreurs, sur des appels qui empruntent chaque chemin.
#
#   tclsh liaison.tcl ?COMPILATEUR?
#
# Les définitions sont celles de l'étape 3 du tutoriel (../tutoriel).

set here [file dirname [file normalize [info script]]]
set cc [lindex $argv 0]
set tests {
    {poly::aire {{0.0 0.0} {4.0 0.0} {4.0 4.0}}}
    {poly::aire $G}
    {poly::aire $L}
    {poly::aire {{0 0} {4 0} {4 4}}}
    {poly::aire {{0.0 0.0} {x 0.0}}}
    {poly::aire {{0.0 0.0 0.0}}}
    {poly::aire}
    {poly::aire a b}
    {poly::aire {pg:}$G}
    {poly::contient $G $P}
    {poly::contient $G $A}
    {poly::contient $G {2.0 1.0}}
    {poly::contient $G {2.0}}
    {poly::perimetre $G}
    {poly::perimetre -ouvert $G}
    {poly::perimetre -ou $G}
    {poly::perimetre -- $G}
    {poly::deplace $G 1.0}
    {poly::deplace $G 1.0 2.0}
    {poly::deplace $G -1.0 -2.0}
    {poly::deplace $G x}
    {poly::deplace $G {dy:}3.0 {dx:}1.0}
    {poly::boite $G}
    {poly::point 1 2}
}
foreach v {cproc2_type cproc} {
    interp create $v
    if {$cc ne ""} {$v eval [list set ::env(CPROC_CC) $cc]}
    # l'étape 3 du tutoriel, avec cette version de cproc
    set src [read [set f [open [file join $here .. tutoriel etape3_c.tcl]]]][close $f]
    regsub {source \[file join[^\n]*cproc\.tcl\]} $src \
	    [list source [file join $here $v.tcl]] src
    $v eval $src
    $v eval {
	namespace eval poly {type def autre {{double}x; {double}y}}
	set {poly::polygone}G {0.0 0.0} {4.0 0.0} {4.0 4.0}
	set {poly::point}P 1.0 1.0
	set {poly::autre}A 1.0 1.0
	set {list.poly::point}L {0.0 0.0} {4.0 0.0} {4.0 4.0}
    }
}
catch {unset ::env(CPROC_CC)}
set diff 0
foreach t $tests {
    set out {}
    foreach v {cproc2_type cproc} {
	set c [$v eval [list catch $t r]]
	set r [$v eval {set r}]
	set ty [expr {$c ? "" : [$v eval {type get $r}]}]
	lappend out "$c $ty | $r"
    }
    set same [expr {[lindex $out 0] eq [lindex $out 1]}]
    if {!$same} {incr diff}
    puts [format "%-40s %s %s" $t [expr {$same ? "=" : "DIFFÉRENT"}] [lindex $out 1]]
}
puts "[llength $tests] appels, $diff différence(s)"
