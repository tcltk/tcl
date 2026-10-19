# t53 : l'index de $nom(...) (Tcl_ParseVarIndex, TCL_TOKEN_INDEX) et les
# valeurs substituées atomiques.
#  - $R(...) accepte parenthèses équilibrées, étiquettes "...", espaces ; un
#    index déjà valide en Tcl 9 garde ses jetons et sa chaîne ;
#  - une valeur substituée ($x, [cmd], [(expr)]) est UNE feuille du chemin ;
#    *$x la déplie en alternatives ; le chemin lui-même ne substitue rien ;
#  - ${R(chemin)} et un nom donné comme chaîne (set $ref) : chemin littéral ;
#  - {T}R(a, b) : mot annoté jusqu'à la parenthèse équilibrée.
proc e {label script} {
    if {[catch {uplevel #0 $script} r]} {set out "ERREUR: [lindex [split $r \n] 0]"} else {set out "-> [list [string range $r 0 end]]"}
    puts [format "%-34s | %s" $label $out]
}
type def rect {{{int}x; {int}y}A ; {{int}x; {int}y}B }
set {rect}R(A,B) {1 2} {3 4}
set {list.int}L 10 20 30
set {dict.int}D file.txt 1 a 2 f_1 4
# 1. index étendu
e {$R(A.(x,y))}                   {list $R(A.(x,y))}
e {$R((A,B).(x, y))}              {list $R((A,B).(x, y))}
e {"<$R(A.x, B.y)>"}              {set s "<$R(A.x, B.y)>"}
e {$L(0,"|",1)}                   {list $L(0,"|",1)}
e {tableau $tab(b(c))}            {array set tab {{b(c)} 1 {"q"} 2}; list $tab(b(c)) $tab("q")}
e {$tab(b(c) non fermé}           {list $tab(b(c)}
# 2. atomicité
e {k=file.txt : $D($k)}           {set k file.txt; list $D($k)}
e {n=1 : $D(f_$n)}                {set n 1; list $D(f_$n)}
e {k=A,B : $R($k.x)}              {set k A,B; list $R($k.x)}
e {k=A : $R($k.(x,y))}            {set k A; list $R($k.(x,y))}
e {[cmd] atomique}                {list $D([string cat file . txt])}
e {injection neutralisée}         {set p {[error INJECTION]}; list $L($p)}
e {*$idx}                         {set idx {0 2}; list $L(*$idx)}
e {*$idx, 1}                      {set idx {0 2}; list $L(*$idx, 1)}
e {écriture set D($k) v}          {set k file.txt; set D($k) 5; list $D($k)}
e {tableau ordinaire a($k)}       {array set a {file.txt 1}; set k file.txt; list $a($k)}
# 3. littéral : ${...}, accolades, nom donné comme chaîne
e {${R(A.x, B.y)}}                {list ${R(A.x, B.y)}}
e {"<${L(0,"|",1)}>"}             {set s "<${L(0,"|",1)}>"}
e {${R($k)} littéral}             {list ${R($k)}}
e {set {L($i)} littéral}          {set {L($i)}}
e {chemin dans une variable}      {set c A.y; set ref R($c); set $ref}
e {${nope(x)} : message de Tcl}   {list ${nope(x)}}
# 4. mot annoté avec espaces
e {set {rect}Q(A.x, A.y, B)}      {set {rect}Q(A.x, A.y, B) 1 2 {3 4}}
e {set {rect}Q((A, B).*(x, y))}   {set {rect}Q2((A, B).*(x, y)) {5 6 7 8}}
e {k=B : set {rect}Q(A, $k)}      {set k B; set {rect}Q3(A, $k) {1 1} {2 2}}
e {mot nu inchangé}               {llength [list f(a b)]}
# 5. dans une procédure (bytecode : indexConcat)
proc g {k idx} {
    set {dict.int}D file.txt 1 a 2
    set {list.int}L 10 20 30
    set D($k) 9
    list $D($k) $L(*$idx) ${L(0,"-",2)} "<$L(0,"|",1)>"
}
e {procédure}                     {g file.txt {0 2}}
e {bytecode}                      {expr {[string first indexConcat [tcl::unsupported::disassemble proc g]] >= 0}}
