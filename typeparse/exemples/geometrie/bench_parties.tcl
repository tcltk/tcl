# bench_parties.tcl -- FR : coût des parties de l'étape annotée (lecture par chemin,
#   lindex, foreach, redéclaration), sur un polygone de 10 000 points.
#   EN : cost of the parts of the annotated stage.
type def point {{double}x; {double}y}
type def polygone list.point
proc cercle {n r} {lmap k [lseq $n] {list [expr {$r*cos(6.283185307179586*$k/$n)}] [expr {$r*sin(6.283185307179586*$k/$n)}]}}
set brut [cercle 10000 100.0]
set P [type cast polygone $brut]
proc decl {p} {( P is polygone = $p; 0 )}
proc decl_plain {p} { set q $p; return 0 }
proc idx {p} { set n [llength $p]; foreach i [lseq $n] { set a [lindex $p $i] } }
proc idxpath {p} { set n [llength $p]; foreach i [lseq $n] { set a [lindex $p $i]; set x $a(x); set y $a(y) } }
proc idxlassign {p} { set n [llength $p]; foreach i [lseq $n] { lassign [lindex $p $i] x y } }
proc fe {p} { foreach a $p { } }
proc fepath {p} { foreach a $p { set x $a(x); set y $a(y) } }
proc aire_brut {p} { set n [llength $p]; set s 0.0; foreach i [lseq $n] { lassign [lindex $p $i] xa ya; lassign [lindex $p [expr {($i+1)%$n}]] xb yb; set s [expr {$s + $xa*$yb - $xb*$ya}] }; expr {abs($s)/2.0} }
proc aire_ann {p} {( P is polygone = $p;  n = llength $P;  s = 0.0;
    foreach i [lseq $n] {( a = lindex $P $i;  b = lindex $P [(($i+1) % $n)]; s = $s + $a(x)*$b(y) - $b(x)*$a(y) )};
    abs($s) / 2.0 )}
foreach {lab cmd} {
  "aire brut (brut)"        {aire_brut $brut}
  "aire annotée"            {aire_ann $P}
  "P is polygone = \$p"     {decl $P}
  "lindex x N, brut"        {idx $brut}
  "lindex x N, typé"        {idx $P}
  "lindex + \$a(x) \$a(y)"  {idxpath $P}
  "lindex + lassign brut"   {idxlassign $brut}
  "foreach, brut"           {fe $brut}
  "foreach, typé"           {fe $P}
  "foreach + chemins"       {fepath $P}
} { puts [format "%-26s %10.0f µs" $lab [lindex [timerate $cmd 1500] 0]] }
