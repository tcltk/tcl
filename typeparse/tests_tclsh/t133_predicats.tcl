# Types restreints par un prédicat (v17u) : T((expression)), $ seul = la
# valeur. Vérifié à l'entrée (un visa) : set, paramètres, cast, écritures par
# chemin, listes ; un T typé entre sans cast si le prédicat est vrai.
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r o]} { set r "ERR $r" }
    puts "$nom : $r"
}
type def pair int(($%2==0))
essai "set pair 4"        {set {pair}x 4; list $x [type get $x]}
essai "set pair 3"        {set {pair}x 3}
essai "set pair abc"      {set {pair}x abc}
essai "typé int 6"        {set {int}i 6; set {pair}y $i; list $y [type get $y]}
essai "typé int 7"        {set {int}i 7; set {pair}y $i}
essai "pair -> int"       {set {pair}p 8; set {int}k $p; type get $k}
essai "en ligne"          {set {int(($ > 0))}z 5; type get $z}
essai "en ligne faux"     {set {int(($ > 0))}z -5}
essai "u8 pair"           {set {int(u,8,($%2==0))}b 200; type get $b}
essai "u8 pair 201"       {set {int(u,8,($%2==0))}b 201}
essai "u8 pair 300"       {set {int(u,8,($%2==0))}b 300}
essai "double"            {set {double((0 <= $ && $ <= 100))}q 55.5}
essai "double faux"       {set {double((0 <= $ && $ <= 100))}q 101}
set ::max 10
essai "globale"           {set {int(($ < $::max))}m 3}
essai "globale faux"      {set {int(($ < $::max))}m 30}
essai "visa"              {set {int(($ < $::max))}m 3; set ::max 1; set {int(($ < $::max))}m2 $m; set m2}
essai "non qualifiée"     {set {int(($ < $max))}m 3}
essai "chaîne"            {set {string(($ ne ""))}s abc}
essai "chaîne vide"       {set {string(($ ne ""))}s ""}
essai "chaîne {a\$b}"     {set {string(($ eq {a$b}))}s {a$b}}
essai "commande"          {set {string(([string length $] < 4))}s abc}
essai "commande faux"     {set {string(([string length $] < 4))}s abcdef}
proc f {{pair}n} {expr {$n/2}}
essai "proc 10"           {f 10}
essai "proc 11"           {f 11}
essai "list.pair"         {set {list.pair}L 2 4 6; list $L [type get $L]}
essai "list.pair 5"       {set {list.pair}L 2 5 6}
essai "cast liste"        {type cast list.pair {2 4 6}}
essai "cast liste 5"      {type cast list.pair {2 4 5}}
essai "écriture L(1) 3"   {set {list.pair}L 2 4 6; set L(1) 3}
essai "écriture L(1) 8"   {set {list.pair}L 2 4 6; set L(1) 8; set L}
essai "lset"              {set {list.pair}L 2 4 6; lset L 1 3}
essai "set x 5"           {set {pair}x 4; set x 5}
essai "incr x"            {set {pair}x 4; incr x}
essai "incr x 2"          {set {pair}x 4; incr x 2; set x}
proc k {} {set {pair}x 4; incr x; return $x}
essai "proc incr x"       {k}
essai "cases"             {set {list.int(u,8,($%2==0))}B {*}[lrepeat 10 4]; set B(3) 5}
essai "cases ok"          {set {list.int(u,8,($%2==0))}B {*}[lrepeat 10 4]; set B(3) 6; set B}
essai "tranche"           {set B [type repeat int(u,8,($%2==0)) 10 4]; set B(0:end:2) 7}
essai "tranche ok"        {set B [type repeat int(u,8,($%2==0)) 10 4]; set B(0:end:2) 8; set B}
essai "type map"          {type map {v} {1 2 3} {int(($%2==0))}{($v*2)}}
essai "type map faux"     {type map {v} {1 2 3} {int(($%2==0))}{($v+1)}}
essai "repeat"            {type repeat pair 3 4}
essai "repeat zéro"       {type repeat pair 3}
essai "repeat faux"       {type repeat pair 3 5}
essai "imbriqué"          {type def pos2 {pair(($ > 0))}; list [catch {set {pos2}a 4}] [catch {set {pos2}a -4}] [catch {set {pos2}a 3}]}
essai "liste sans élément" {set {list(($ ne {}))}x a}
