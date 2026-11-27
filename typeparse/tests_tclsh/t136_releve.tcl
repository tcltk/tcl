# type map -report varName ... et ::tcl::unsupported::mapreport (v17y) :
# ce que type map a fait, et pourquoi une voie plus rapide n'a pas servi.
# Le relevé dit ce que font les voies rapides : le test tourne sans leurs
# interrupteurs (relancé sans eux s'il le faut ; TCL_TYPE_OPS est lu une fois).
set sw 0
foreach e {TCL_TYPE_NOBLOCK TCL_TYPE_NOTUPLE TCL_TYPE_NOVECLOOP TCL_TYPE_OPS} {
    if {[info exists ::env($e)]} { unset ::env($e); set sw 1 }
}
if {$sw} {
    puts [exec [info nameofexecutable] [info script] 2>@1]
    exit
}
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r o]} { set r "ERR $r" }
    puts "$nom : $r"
}
proc releve {nom script {cles {pass body blocks elementwise expr write why}}} {
    upvar 1 r r
    unset -nocomplain r
    catch {uplevel 1 $script}
    set out {}
    foreach k $cles { if {[dict exists $r $k]} { lappend out $k [dict get $r $k] } }
    puts "$nom : $out"
}
type def {-compact}pd list.double
set c 0.6; set s 0.8; set k abc
set X {}; set Y {}
for {set i 0} {$i < 300} {incr i} { lappend X [expr {$i * 0.5}]; lappend Y [expr {$i * 0.25}] }
set {pd}P {*}[concat {*}[lmap a $X b $Y {list $a $b}]]
proc tcl::mathfunc::foo {a} { expr {$a + 1} }

releve "colonne"        {type map -report r {x} $X {($x * 2)}} {elements inputs pass body blocks why}
releve "tuple"          {type map -report r {x y} [list $X $Y] {{double}u; {double}v}{($c*$x - $s*$y, $s*$x + $c*$y)}}
releve "-in direct"     {type map -report r -in P((0,1)::2) {x y} {($y, $x)}} {inputs pass write why}
releve "fonction"       {type map -report r {x y} [list $X $Y] {{double}u; {double}v}{($x, foo($y))}}
releve "fixe texte"     {type map -report r {x} $X {($x * $k)}}
set X2 $X; lset X2 3 abc
releve "un texte"       {type map -report r {x} $X2 {($x * 2)}}
set X3 $X; lset X3 3 7
releve "un entier"      {type map -report r {x y} [list $X3 $Y] {{double}u; {double}v}{($x, $y)}}
releve "-sum"           {type map -report r -sum {x} $X {($x)}}
releve "recouvrement"   {type map -report r -in P((0,1)::1) {x y} {($x, $y)}} {pass write why}
set {list.double}O {*}$X
releve "-in objets"     {type map -report r -in O(0::2) {x} {($x + 1)}} {pass write why}
releve "sans chemin"    {type map -report r -in X {x} {($x + 1)}} {pass write why}
essai "résultat gardé"  {type map -report r {x} {1.0 2.0} {($x * 2)}}
essai "erreur gardée"   {catch {type map -report r {x} {1.0 abc} {($x * 2)}} m; list $m [dict get $r why]}
essai "option seule"    {type map -report}

# le relevé global, par lieu d'appel
::tcl::unsupported::mapreport on
proc f {L} { set t 0; foreach v $L {set t [expr {$t + $v}]}; return $t }
proc h {L} { lmap v $L {($v * 2)} }
proc m {L} { type map {x} $L {($x + $::k)} }
proc n {L} { set t 0; foreach v $L {incr t $v}; return $t }
set XI {}; for {set i 0} {$i < 300} {incr i} { lappend XI $i }
proc a {L} { set b {($v + $::g(1))}; lmap v $L $b }
set g(1) 2
proc b {L} { set t 0.5; foreach v $L {incr t $v}; return $t }
proc d {L} { unset -nocomplain s; catch {foreach v $L {(s = $s + $v)}} }
f $X; f $X; h $X; catch {m $X}; n $XI; n {1 2}; a $X; catch {b $XI}; d $X
foreach {where st} [::tcl::unsupported::mapreport get] {
    regsub {^.*/} $where {} where
    set out {}
    foreach key [lsort [dict keys $st]] { lappend out $key [dict get $st $key] }
    puts "@ $where : $out"
}
::tcl::unsupported::mapreport clear
essai "vidé"            {::tcl::unsupported::mapreport get}
::tcl::unsupported::mapreport off
f $X
essai "arrêté"          {::tcl::unsupported::mapreport get}
essai "option"          {::tcl::unsupported::mapreport bof}
