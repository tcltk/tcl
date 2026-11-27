# Fenêtres $L((s0,s1,...):fin:pas), vues d'objets, écriture de colonnes (v17n)
type def {-compact}cd list.double
# num est intégré (v17z)
proc montre {nom script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERR $r" }
    puts "$nom : $r"
}
proc lecture {{list.double}P} {
    montre "fenêtre (0,1,2,3)::2"  {list $P((0,1,2,3)::2)}
    montre "avec la fin"           {list $P((0,1,2,3):end-2:2)}
    montre "x et y"                {list $P((0,1)::2)}
    montre "pas -2"                {list $P((7,6)::-2)}
    montre "glissante"             {list $P((0,2,4):3)}
    montre "début substitué"       {set i 1; list $P(($i,2)::3)}
    montre "un seul tour"          {list $P((5,6)::2)}
    montre "aucun"                 {list $P((6,7):5:2)}
    montre "pas nul"               {list $P((0,1)::0)}
    montre "pas un index"          {list $P((a,1)::2)}
    montre "index substitué entier" {set s "1:3"; list $P($s)}
    montre "groupe sans :"         {list $P((1,3))}
}
lecture {0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0}
proc cases {} {
    set {cd}C 0.0 1.0 2.0 3.0 4.0 5.0
    montre "en cases"        {list $C((0,1)::2)}
    montre "type map"        {type map -sum {x0 y0 x1 y1} $C((0,1,2,3)::2) {($x0*$y1 - $x1*$y0)}}
    set C((0,1)::2) [type cast list.double {10 20 30}] [type cast list.double {-1 -2 -3}]
    montre "écrit en cases"  {list $C [type get $C]}
}
cases
proc vues {{list.double}P} {
    set v $P(1::2)
    montre "llength lindex"  {list [llength $v] [lindex $v 2] [lindex $v end]}
    montre "foreach lmap"    {set s 0; foreach x $v {set s [expr {$s+$x}]}; list $s [lmap x $v {expr {$x*2}}]}
    montre "lsort join"      {list [lsort -real -decreasing $v] [join $v ,]}
    montre "lappend copie"   {set w $v; lappend w 99.0; list $w $v}
    montre "vue de vue"      {set u $v(0::2); set u}
    set P(1) 100.0
    montre "base changée"    {list $P(0:3) $v}
    montre "type map vue"    {type map {x} $v {y} $P(0::2) {($x - $y)}}
}
vues {0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0}
proc ecritures {} {
    set {list.double}L 0.0 1.0 2.0 3.0 4.0 5.0
    set X [type cast list.double {10 20 30}]; set Y [type cast list.double {-1 -2 -3}]
    set L((0,1)::2) $X $Y
    montre "entrelacées"     {set L}
    set K $L
    set L((1,0)::2) $X $Y
    montre "partagée"        {list $L $K}
    set L((0,1)::2) 7.0 8.0
    montre "une pour tous"   {set L}
    montre "longueur"        {set L((0,1)::2) [type cast list.double {1 2}] $Y}
    montre "mauvais type"    {set L((0,1)::2) [type cast list.int {1 2 3}] $Y}
    montre "texte"           {set L((0,1)::2) {1 2 x} $Y}
    set {list.num}N 1 2.5 3 4.5
    set N((0,1)::2) [type cast list.int {10 30}] [type cast list.double {2.0 4.0}]
    montre "num"             {list $N [type canonic $N]}
    set {list.num}Z {*}[lrepeat 4 0.0]
    montre "num rapide"      {type canonic $Z}
    set {list.double}D 1.0 2.0
    set {list.double}E $D
    montre "même type"       {list $E [type get $E]}
}
ecritures
