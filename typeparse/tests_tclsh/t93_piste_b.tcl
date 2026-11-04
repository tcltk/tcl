# t93 : v16m, piste B. Une annotation n'agit que sur la commande qui reçoit le
# mot ; quand cette commande finit, ce qui est encore en attente expire : une
# note, une métadonnée que plus aucun constructeur ne résout. Plus de cast
# immédiat ; le cast explicite est « type {T}mot ». foreach, lmap, dict for,
# dict map, lassign reçoivent leurs listes et les résolvent ({list}$p).
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
proc show {v} { list $v [type get $v] [note get $v] }
type def pt {{double}x; {double}y}
set {pt}p 1 2
set s 3.0
set brut "a {b"

puts "--- plus de fuite : l'erreur ne sort plus d'un catch"
proc f {} {global s; return {int}$s}
proc g {} {global s; concat {int}$s}
t "catch {concat {int}\$s} r"    {list [catch {concat {int}$s} r] [show $r]}
t "string cat"                   {list [catch {string cat {int}$s} r] [show $r]}
t "lindex"                       {list [catch {lindex {int}$s} r] [show $r]}
t "return {int}\$s"              {list [catch {f} r] [show $r]}
t "procédure, concat"            {list [catch {g} r] [show $r]}
t "return -level 0"              {list [catch {return -level 0 {int}$s} r] [show $r]}
t "if"                           {list [catch {if 1 {concat {int}$s}} r] [show $r]}
t "élément d'une liste"          {list [catch {lindex [list {int}$s] 0} r] [show $r]}
t "eval"                         {list [catch {eval {concat {int}$s}} r] [show $r]}
t "subst"                        {list [catch {subst {[concat {int}$s]}} r] [show $r]}
t "set y \[concat ...\]"         {list [catch {set y [concat {int}$s]} r] [show $y]}
t "{dict} non vérifié"           {list [catch {concat {dict}{a b c}} r] [show $r]}

puts "--- le constructeur qui reçoit le mot résout toujours"
t "set _ {int}\$s"               {list [catch {set _ {int}$s} r] $r}
t "set {int}i 5"                 {set {int}i 5; type get $i}
t "nom calculé : déclaration"    {set nom compteur; set {int}$nom 5; type get $compteur}
t "nom calculé : \[set nom\]"    {set nom c2; set {double}[set nom] 2; type get $c2}
t "étiquettes"                   {set {pt}q {y:}4 {x:}3; list $q(x) $q(y)}
t "contrat"                      {set {int}k 1; list [catch {set k {int}$s} e] $e}

puts "--- type {T}mot : le cast explicite"
t "type {int}\$s"                {list [catch {type {int}$s} r] $r}
t "type {double}\$s"             {show [type {double}$s]}
t "type {list}\$p"               {show [type {list}$p]}
t "type {dict}\$p"               {show [type {dict}$p]}
t "type {int}\[cmd\]"            {show [type {int}[expr {2 + 3}]]}
t "type {int}\[(expr)\]"         {show [type {int}[(2 + 3)]]}
t "type {int}\"…\""              {set a 4; show [type {int}"$a$a"]}
t "type {int}5"                  {show [type {int}5]}
t "nom inconnu"                  {list [catch {type {zz}5} r] $r}
t "annotation locale"            {list [catch {type {-v}5} r] $r}
t "sans annotation"              {list [catch {type 5} r] $r}
t "dans un catch"                {list [catch {type {list}$brut} r] $r}

puts "--- les boucles reçoivent leurs listes"
t "foreach {list}\$p"            {set r {}; foreach {a b} {list}$p {lappend r $a/$b}; set r}
t "lmap {list}\$p"               {lmap {a b} {list}$p {expr {$a + $b}}}
t "dict for {dict}\$p"           {set r {}; dict for {k v} {dict}$p {lappend r $k=$v}; set r}
t "dict map {dict}\$p"           {dict map {k v} {dict}$p {expr {$v * 10}}}
t "lassign {list}\$p"            {lassign {list}$p a b; list $a $b}
t "foreach : erreur attrapée"    {list [catch {foreach x {list}$brut {}} r] $r}
t "lassign : erreur attrapée"    {list [catch {lassign {list}$brut a} r] $r}
proc boucles {} {
    set {pt}q 3 4
    foreach {a b} {list}$q {lappend r $a-$b}
    lappend r {*}[lmap {a b} {list}$q {expr {$a * $b}}]
    dict for {k v} {dict}$q {lappend r $k}
    lappend r [dict map {k v} {dict}$q {incr v}]
    lassign {list}$q c d
    lappend r $c $d
    set mauvais "a \{b"
    lappend r [catch {foreach x {list}$mauvais {}} e] $e
}
t "compilées dans une procédure" {boucles}

puts "--- listes typées : lappend et lset reçoivent l'élément"
proc listes {} {
    set {list.any}A {int}1
    lappend A {string}x
    lset A 0 {double}2.5
    set r [lmap e $A {type get $e}]
    type def {-deduce}S {{int}i, {string}s}
    set {list.S}L {i:}1
    lset L 0 {s:}abc
    lappend L {i:}7
    lappend r {*}[lmap e $L {type get $e}]
}
t "list.any, list.S"             {listes}
t "au niveau global"             {set {list.any}G {int}1; lappend G {string}x; lmap e $G {type get $e}}

puts "--- notes expirées : des métadonnées"
t "note posée, passée par concat" {show [concat {mm}5]}
t "type def ensuite"              {set v [concat {mm2}5]; type def mm2 int; set w $v; show $w}
t "rangée par set"                {set v {mm3}5; type def mm3 int; set w $v; show $w}
t "note delete"                   {show [note delete [concat {a}[concat {b}5]] a]}

puts "--- boucles compilées : {T}mot casté une fois (annotCast)"
proc castes {} {
    global brut
    set {pt}q 5 6
    set r {}
    foreach {x y} {list.int}{1 2} {lappend r [type get $x]}
    lappend r [catch {foreach x {list.int}{1 a} {}} e] $e
    lappend r [catch {foreach x {list(3).int}{1 2} {}} e] $e
    lappend r [catch {foreach x {list}$brut {}} e] $e
    lappend r [dict map {k v} {dict.int}{a 1 b 2} {type get $v}]
    foreach x {mm}{1 2} {lappend r $x}
    lappend r [lmap {a b} {list}$q {expr {$a * $b}}]
}
t "castes"                       {castes}
