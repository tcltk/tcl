# t60 : construction par chemin d'une liste (indices) ou d'un dict (clés) ; une variable
#   typée est un scalaire : set {int}A(k) est refusé sans rien créer.
type def point {{double}x; {double}y}
type def segment list(2).point
set {point}A 0 0; set {point}B 4 4
proc t {l s} { set r [catch {uplevel 1 $s} m]; puts [format "%-40s %s %s" $l $r $m] }
t "set {segment}AB(0,1) \$A \$B"   {set {segment}AB(0,1) $A $B}
t "array exists AB"               {array exists AB}
t "info exists AB"                {info exists AB}
t "set {segment}AB \$A \$B"        {set {segment}AB $A $B}
t "array exists R"                {array exists R}
t "list.int : set {list.int}L(0,1) 5 6" {set {list.int}L(0,1) 5 6}
t "array exists L"                {array exists L}
proc p {} { catch {set {segment}S(0,1) {0 0} {1 1}} m; list $m [array exists S] [info exists S] }
t "dans une procédure"            {p}
type def poly list.point
proc t {l s} { set r [catch {uplevel 1 $s} m]; puts [format "%-44s %s %s" $l $r $m] }
t "segment ordre inverse AB(1,0)"     {set {segment}S1(1,0) {4 4} {0 0}; list $S1 [type get $S1]}
t "trou L(0,2)"                       {set {poly}S2(0,2) {0 0} {1 1}}
t "  S2 créé ?"                       {info exists S2}
t "doublon L(0,0)"                    {set {poly}S3(0,0) {0 0} {1 1}}
t "pas un indice L(a,b)"              {set {poly}S4(a,b) {0 0} {1 1}}
t "segment de 3 : S5(0,1,2)"          {set {segment}S5(0,1,2) {0 0} {1 1} {2 2}}
t "  S5 créé ?"                       {info exists S5}
t "chemin profond 0.x,0.y,1.x,1.y"    {set {segment}S6(0.x,0.y,1.x,1.y) 1 2 3 4; list $S6 [type get [lindex $S6 1]]}
t "élément invalide"                  {set {segment}S7(0,1) {0 0} abc}
t "dict.int D(a,b)"                   {set {dict.int}D(a,b) 1 2; list $D [type get $D]}
t "dict.int invalide"                 {set {dict.int}D2(a) x}
t "{int}I(k) refusé"                  {set {int}I(k) 5}
t "  I créé ?"                        {info exists I}
t "nombre de valeurs"                 {set {segment}S8(0,1) {0 0}}
