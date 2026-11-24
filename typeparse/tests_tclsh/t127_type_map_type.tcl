# type map typé (v17p) : signatures d'entrée, corps annoté, tuples, projection
proc essai {nom script} { if {[catch {uplevel 1 $script} r]} { set r "ERR $r" }; puts "$nom : $r" }
set X {1 2 3}; set Y {0.5 1.5 2.5}
set c [expr {cos(0.5)}]; set s [expr {sin(0.5)}]
essai "entrée typée"        {type map {{double}x} $X {($x / 2)}}
essai "sans type"           {type map {x} $X {($x / 2)}}
essai "deux colonnes"       {type map {{double}x; {double}y} [list $X $Y] {($x + $y)}}
essai "colonne fautive"     {type map {{double}x} {1 abc 3} {($x)}}
essai "sortie scalaire"     {type map {{double}x} $X {double}{($x * 2)}}
essai "sortie int"          {type map {{int}x} $X {int}{($x * 2)}}
essai "int depuis double"   {type map {{double}x} $X {int}{($x * 2)}}
essai "tuple"               {type map {{double}x; {double}y} [list $X $Y] {{double}u; {double}v}{($x + $y, $x - $y)}}
essai "rotation"            {type map {{double}x; {double}y} [list $X $Y] {{double}u; {double}v}{($c*$x - $s*$y, $s*$x + $c*$y)}}
essai "projection"          {type map {{double}x; {double}y} [list $X $Y] {{double}y; {double}x}{}}
essai "projection absente"  {type map {{double}x; {double}y} [list $X $Y] {{double}z}{}}
essai "somme typée"         {type map -sum {{double}x} $X {double}{($x)}}
essai "somme produit"       {type map -sum {{double}x} $X {{double}u; {double}v}{($x, $x)}}
essai "nombre de valeurs"   {type map {{double}x} $X {{double}u; {double}v}{[list $x]}}
essai "tuple imbriqué"      {type map {{double}x} $X {{double}u; {double}v}{(max($x, 2), [expr {$x+1}])}}
type def point {{double}x; {double}y}
essai "produit nommé"       {type map {{double}x; {double}y} [list $X $Y] {point}{($y, $x)}}
essai "type du produit"     {type get [type map {{double}x; {double}y} [list $X $Y] {point}{($y, $x)}]}
essai "types des colonnes"  {lmap c [type map {{double}x; {double}y} [list $X $Y] {{double}u; {int}v}{($x, round($y))}] {type get $c}}
essai "mêlé"                {type map {{double}x} $X {y} $Y {($x + $y)}}
essai "variables après"     {type map {{double}x} $X {($x)}; set x}
essai "noms entre accolades" {type map {{x} {y}} [list $X $Y] {($x + $y)}}
essai "erreur au milieu"    {type map {{double}x; {double}y} [list $X {1 0 2}] {{double}u; {double}v}{($x / $y, $x)}}
essai "erreur ordinaire"    {type map {x y} [list $X {1 0 2}] {($x / $y, $x)}}
essai "cases"               {type map {{double}x} [type cast list.double {1 2 3}] {{double}u; {double}v}{($x*2, -$x)}}
