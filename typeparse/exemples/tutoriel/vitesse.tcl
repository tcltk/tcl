# vitesse.tcl -- les trois étapes, mêmes appels : µs par appel (le meilleur
# de 10 tours)
#
#   tclsh vitesse.tcl ?TCC?
#
# Colonnes : étape 1 (Tcl), étape 2 (Tcl typé), étape 3 (C, gcc), et étape 3
# avec tcc si TCC est donné. Le polygone est passé de deux façons : une liste
# ordinaire (ce que donne un appelant non typé), ou une variable typée
# (set {poly::polygone}G ...), ce que garde un programme typé.

set here [file dirname [file normalize [info script]]]
set tcc [lindex $argv 0]
set cols {e1 etape1_tcl.tcl {} e2 etape2_type.tcl {} e3_gcc etape3_c.tcl gcc}
if {$tcc ne ""} {lappend cols e3_tcc etape3_c.tcl $tcc}

set n 1000
set grand [lmap i [lseq $n] {
    list [expr {cos(2*3.141592653589793*$i/$n)}] [expr {sin(2*3.141592653589793*$i/$n)}]
}]
set names {}
foreach {c f cc} $cols {
    interp create $c
    if {$cc ne ""} {$c eval [list set ::env(CPROC_CC) $cc]}
    $c eval [list source [file join $here $f]]
    $c eval [list set grand $grand]
    if {$c ne "e1"} {
	$c eval {set {poly::polygone}G {*}$grand; set {poly::point}P 0.5 0.25}
    } else {
	$c eval {set G $grand; set P {0.5 0.25}}
    }
    $c eval {proc measure {cmd n} {uplevel #0 [list time $cmd $n]}}
    lappend names $c
}
catch {unset ::env(CPROC_CC)}

proc best {c cmd n} {
    set b 1e9
    for {set r 0} {$r < 10} {incr r} {
	set t [lindex [$c eval [list measure $cmd $n]] 0]
	if {$t < $b} {set b $t}
    }
    return $b
}

puts [format "%-34s%s" "" [join [lmap c $names {format %10s $c}] ""]]
foreach {label cmd k} {
    "point 1.0 2.0"                    {poly::point 1.0 2.0}               20000
    "aire (1000 sommets, liste)"       {poly::aire $grand}                 100
    "aire (1000 sommets, typé)"        {poly::aire $G}                     500
    "périmètre (typé)"                 {poly::perimetre $G}                500
    "contient (typé)"                  {poly::contient $G $P}              500
    "boîte (typé)"                     {poly::boite $G}                    500
    "déplace (typé)"                   {poly::deplace $G 1.0 2.0}          100
} {
    set line [format "%-34s" $label]
    foreach c $names {
	append line [format %10.2f [best $c $cmd $k]]
    }
    puts $line
}
