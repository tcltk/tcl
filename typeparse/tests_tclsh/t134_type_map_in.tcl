# type map -in VAR(CHEMIN) varList ?varList liste ...? corps (v17w) : calculé
# et écrit sur place ; le type de la sortie est celui de la cible.
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r o]} { set r "ERR $r" }
    puts "$nom : $r"
}
essai "liste nue"        {set L {1 2 3}; type map -in L {x} {($x * 2)}; set L}
essai "typée"            {set {list.double}D 1 2 3 4; type map -in D {x} {($x / 2)}}
essai "tranche"          {set {list.double}D 1 2 3 4; type map -in D(1::2) {y} {($y * 10)}}
essai "fenêtre"          {set {list.double}D 1 2 3 4; type map -in D((0,1)::2) {x y} {($y, $x)}}
essai "fenêtre rot"      {set {list.double}D 1 0 0 1; type map -in D((0,1)::2) {x y} {(-$y, $x)}}
essai "autre liste"      {set {list.double}D 1 2 3; type map -in D {x} {k} {10 20 30} {($x * $k)}}
essai "longueurs"        {set {list.double}D 1 2 3; type map -in D {x} {k} {10 20} {($x * $k)}}
essai "nombre valeurs"   {set {list.double}D 1 2 3 4; type map -in D((0,1)::2) {x y} {($x)}}
essai "type cible"       {set {list.int}I 1 2 3; type map -in I {x} {($x / 2.0)}}
essai "annot ok"         {set {list.double}D 1 2 3; type map -in D {x} {int}{($x > 1)}}
essai "annot refusée"    {set {list.int}I 1 2 3; type map -in I {x} {double}{($x * 1.5)}}
essai "annot tuple"      {set {list.double}D 1 2 3 4; type map -in D((0,1)::2) {x y} {{double}a; {double}b}{($x+$y, $x-$y)}}
essai "signature"        {set {list.double}D 1 2 3 4; type map -in D {{double}x} {($x / 2)}}
essai "prédicat"         {set {list.int(($%2==0))}P 2 4 6; type map -in P {x} {($x + 1)}}
essai "prédicat ok"      {set {list.int(($%2==0))}P 2 4 6; type map -in P {x} {($x + 2)}}
essai "tout ou rien"     {set {list.double}D 1 0 4; catch {type map -in D {x} {(1.0 / sqrt($x - 0.5))}} m; list $m $D}
essai "pas de variable"  {type map -in nulle {x} {($x)}}
essai "chemin non typé"  {set U {1 2 3}; type map -in U(1::2) {x} {($x)}}
essai "type gardé"       {type def {-compact}vd list.double; set {vd}V 1 2 3; type map -in V {x} {($x*3)}; type get $V}
essai "arguments"        {type map -in D}
proc tr {{list.double}pl} { type map -in pl((0,1)::2) {x y} {($x + 1, $y + 2)}; return $pl }
essai "procédure"        {set {list.double}Q 1 2 3 4; list [tr $Q] $Q}
essai "cases partagées"  {set {list.double}S 1 2 3 4; set T $S; type map -in T((0,1)::2) {x y} {($y, $x)}; list $S $T}
essai "liste d'objets"   {set {list.int}O 1 2 3 4; set O2 $O; type map -in O2(0::2) {x} {($x * 100)}; list $O $O2}
essai "entiers 32 bits"  {type def {-compact}vi list.int(s,32); set {vi}W 1 2 3; type map -in W {x} {($x * 3000000000)}}
essai "nominal"          {type def coord double; type def pl list.coord; set {pl}C 1 2 3 4; type map -in C((0,1)::2) {x y} {($x + 1, $y + 1)}; list $C [type get $C]}
