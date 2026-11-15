# t114 (v17g) : list(LO:HI).T, une liste de LO à HI éléments ; HI = Inf
#   pour « sans borne ». Le « : » des intervalles de chemin ($L(1:3)).
#   list(N) reste exactement N ; list(N:N) est list(N), list(0:Inf) est
#   list.T. Vérifiée à chaque construction et à chaque écriture.
proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-30s %s" $nom $r]
}
puts "== construction"
t "au moins un"                  {set {list(1:Inf).int}L 1 2 3; list $L [type get $L]}
t "vide : refusée"               {set {list(1:Inf).int}L}
t "type cast vide"               {type cast list(1:Inf).int {}}
t "de 2 à 3"                     {type cast list(2:3).int {1 2}}
t "de 2 à 3 : 4"                 {type cast list(2:3).int {1 2 3 4}}
t "au plus 2"                    {type cast list(0:2).int {}}
t "list(3:3) = list(3)"          {type get [type cast list(3:3).int {1 2 3}]}
t "list(0:Inf) = list.int"       {type get [type cast list(0:Inf).int {}]}
t "imbriquée"                    {set {list(1:Inf).list(2).int}P {1 2}; type get $P}
puts "== écritures"
t "lappend jusqu'à la borne"     {set {list(2:3).int}M 1 2; lappend M 3}
t "lappend au-delà"              {set {list(2:3).int}M 1 2 3; lappend M 4}
t "chemin : ajouter"             {set {list(2:3).int}M 1 2; set M(2) 9; set M}
t "chemin : au-delà"             {set {list(2:3).int}M 1 2 3; set M(3) 9}
t "lpop du dernier"              {set {list(1:Inf).int}L 1; lpop L}
t "lset vide"                    {set {list(1:Inf).int}L 1; lset L {}}
t "affectation vide"             {set {list(1:Inf).int}L 1; set L {}}
t "lreplace"                     {set {list(1:Inf).int}L 1; set L [lreplace $L 0 0]}
puts "== types"
t "argument"                     {proc f {{list(1:Inf).int}l} {llength $l}; f {}}
t "champ d'un produit"           {type def rel114 {{list(1:Inf).string}c; {int}n}; set {rel114}x {} 3}
t "type def"                     {type def contenus114 list(1:Inf).string; type cast contenus114 a}
t "vers list.int"                {set {list(1:Inf).int}L 1 2; set {list.int}K $L; type get $K}
t "depuis list.int"              {set {list.int}K 1; set {list(1:Inf).int}L $K; type get $L}
t "type canonic"                 {type canonic [type cast list(2:5).int {1 0x2}]}
t "type parse"                   {type get [type parse {{list(1:Inf).int}{1 2}}]}
puts "== refus de la syntaxe"
t "end"                          {type cast list(1:end).int 1}
t "bornes inversées"             {type cast list(3:1).int 1}
t "borne basse"                  {type cast list(a:2).int 1}
t "un nom qualifié reste un préfixe" {catch {type cast list(::nexiste::pas).int 1} m; string match "*nexiste*" $m}
