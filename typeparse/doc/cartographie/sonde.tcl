# sonde.tcl -- cartographie EIAS / EI2S : ce que devient une valeur typée
# (son contenu, son type) à travers les opérations de Tcl. Chaque ligne : un
# identifiant (repris dans CARTOGRAPHIE-EIAS.md), l'opération, le résultat.
#   tclsh sonde.tcl > sonde.out
proc t {id label script} {
    if {[catch {uplevel #0 $script} r]} { set r "ERREUR: $r" }
    puts [format "%-6s %-46s %s" $id $label $r]
}
proc ty {v} { set t [type get $v]; expr {$t eq "" ? "-" : $t} }
type def point {{double}x; {double}y}
type def rect {{point}nw; {point}se}
type def {-strict}S {{int}i, {string}s}
set {point}P 1 2
set Pn {1 2}
set {int}I 5
set {list.double}L 1 2 3
set {list.point}LP {1 2} {3 4}
set {S}V {s:}abc
set {dict.int}D a 1 b 2

puts "=== A. identité, égalité"
t A1 "chaîne de P"                          {list $P [string length $P] [ty $P]}
t A2 "P eq {1 2} / == "                     {list [expr {$P eq $Pn}] [string equal $P $Pn]}
t A3 "deux types, même chaîne : eq"         {type def vec {{double}x; {double}y}; set {vec}W 1 2; list [expr {$P eq $W}] [ty $P] [ty $W]}
t A4 "int 5 == double 5.0"                  {set {double}F 5.0; list [expr {$I == $F}] [expr {$I eq $F}]}
t A5 "clé de dict typée"                    {set d [dict create $P x]; list [dict exists $d $Pn] [ty [lindex [dict keys $d] 0]]}
t A6 "index de tableau typé"                {set arr($P) 1; list [array names arr] [info exists arr($Pn)]}
t A7 "lsort -unique mélange"                {lsort -unique [list $P $Pn]}
t A8 "string is double sur int typé"        {list [string is double $I] [string is integer $I]}

puts "=== B. vue liste"
t B1 "llength : produit typé / nu"          {list [llength $P] [llength $Pn]}
t B2 "lindex P 0"                           {list [lindex $P 0] [lindex $Pn 0]}
t B3 "foreach sur P"                        {set r {}; foreach e $P {lappend r $e}; list $r [llength $r]}
t B4 "{*} d'un produit dans un appel"       {proc n args {llength $args}; list [n {*}$P] [n {*}$Pn]}
t B5 "lassign P"                            {lassign $P a b; list $a $b}
t B6 "list.double : lindex / foreach"       {set r {}; foreach e $L {lappend r [ty $e]}; list [ty [lindex $L 0]] $r}
t B7 "list.point : éléments"                {list [llength $LP] [ty [lindex $LP 0]] [llength [lindex $LP 0]]}
t B8 "concat P P"                           {set c [concat $P $P]; list $c [llength $c] [ty $c]}
t B9 "list P P : éléments typés ?"          {set c [list $P $P]; list [llength $c] [ty [lindex $c 0]]}
t B10 "join / split"                        {list [join $P ,] [split $P " "]}
t B11 "lsearch L 2.0 / 2"                   {list [lsearch $L 2] [lsearch -real $L 2.0]}
t B12 "lappend typée list.double"           {set {list.double}M 1; lappend M 2; list $M [ty $M]}
t B13 "lappend sur une copie non typée"     {set m $L; lappend m x; list $m [ty $m]}
t B14 "lrange L"                            {set r [lrange $L 0 1]; list $r [ty $r] [ty [lindex $r 0]]}

puts "=== C. variables"
t C1 "adoption : set x P"                   {set x $P; list [ty $x] [catch {set x abc} e] $e}
t C2 "catch m réutilisé"                    {catch {set {int}q 1} m; list [catch {catch {error boom} m} e] $e}
t C3 "incr int typé / double typé"          {set {int}k 1; incr k; set {double}f 1.5; list $k [catch {incr f} e] $e}
t C4 "append string typée"                  {set {string}s ab; append s cd; list $s [ty $s]}
t C5 "append sur int typé"                  {set {int}k2 1; list [catch {append k2 x} e] $e}
t C6 "unset puis set"                       {set {int}u 1; unset u; set u abc; ty $u}
t C7 "upvar vers variable typée"            {proc w {} {upvar 1 I v; list [catch {set v x} e] $e}; w}
t C8 "global + contrat"                     {proc w2 {} {global I; list [catch {set I 2.5} e] $e}; w2}
t C9 "array set avec valeurs typées"        {array set A [list a $P]; list [ty $A(a)] [catch {set A(a) zz} e]}
t C10 "set {}x P : retirer le type"         {set {}z $P; list [ty $z] [llength $z]}
t C11 "trace write sur typée"               {set {int}tw 1; set ::seen {}; trace add variable tw write {apply {{a b c} {lappend ::seen [ty [set ::$a]]}}}; set tw 7; set ::seen}

puts "=== D. transport"
t D1 "aller-retour par fichier"             {set fh [file tempfile fn]; puts $fh $P; close $fh; set fh [open $fn]; set r [gets $fh]; close $fh; file delete $fn; list $r [ty $r] [llength $r]}
t D2 "subst"                                {set r [subst {$P}]; list $r [ty $r]}
t D3 "string cat / format"                  {list [ty [string cat $P]] [ty [format %s $P]]}
t D4 "eval d'une commande typée"            {proc g {p} {ty $p}; eval [list g $P]}
t D5 "eval d'une chaîne"                    {proc g {p} {ty $p}; eval "g {$P}"}
t D6 "interp enfant : set"                  {interp create ch; ch eval [list set x $P]; set r [ch eval {type get $x}]; interp delete ch; set r}
t D7 "interp enfant : type inconnu là-bas"  {interp create ch2; set r [ch2 eval [list catch [list type def point int] e]]; ch2 eval [list set x $P]; set r2 [ch2 eval {list [type get $x] [type def point]}]; interp delete ch2; list $r $r2}
t D8 "alias vers l'enfant"                  {interp create ch3; ch3 eval {proc q {v} {type get $v}}; interp alias {} q3 ch3 q; set r [q3 $P]; interp delete ch3; set r}
t D9 "dict for / dict get sur dict.int"     {set r {}; dict for {k v} $D {lappend r [ty $v]}; list $r [ty [dict get $D a]]}
t D10 "after (script)"                      {set ::af {}; after 0 [list apply {{p} {set ::af [ty $p]}} $P]; update; set ::af}
t D11 "binary format sur double typé"       {set {double}bd 1.5; binary encode hex [binary format d $bd]}
t D12 "regexp / regsub"                     {list [regexp {^1} $P] [ty [regsub {1} $P 9]]}
t D13 "affichage T@notes relisible ?"       {set n [concat {mm}5]; list [type get $n] [catch {set y {int@mm}5} e]}

puts "=== E. procédures"
t E1 "proc classique reçoit P"              {proc c1 {p} {list [ty $p] [catch {set p xx} e]}; c1 $P}
t E2 "proc typée {point}p reçoit {1 2}"     {proc c2 {{point}p; {int}n ?= 0} {ty $p}; c2 {1 2}}
t E3 "proc typée reçoit un vec"             {proc c3 {{point}p; {int}n ?= 0} {ty $p}; type def vec2 {{double}x; {double}y}; set {vec2}W2 1 2; c3 $W2}
t E4 "retour {point} vers appelant nu"      {proc {point}c4 {} {( 1, 2 )}; set r [c4]; list [llength $r] [ty $r]}
t E5 "retour {*}point"                      {proc {{*}point}c5 {} {( 1, 2 )}; set r [c5]; list [llength $r] [ty $r]}
t E6 "TclOO : méthode à signature typée"    {oo::class create K {method m {{point}p; {int}n ?= 0} {type get $p}}; set o [K new]; $o m {1 2}}
t E7 "apply à signature typée"              {apply {{{point}p; {int}n ?= 0} {type get $p}} {1 2}}
t E8 "info args proc typée"                 {info args c2}

puts "=== F. noms de types"
t F1 "namespace : type def dans ns"         {namespace eval nsA {type def pnt {{double}x; {double}y}}; list [catch {type def pnt int} e] $e}
t F2 "redéfinition d'un type existant"      {list [catch {type def point int} e] $e}
t F3 "type forget pendant usage"            {type def tmpT {{int}a; {int}b}; set {tmpT}TT 1 2; type forget tmpT; list [ty $TT] $TT(a)}

puts "=== G. compléments"
t C1a "adoption globale puis set abc"   {set x $P; set x abc; list $x [ty $x]}
t C1b "adoption dans une proc"          {proc a1 {} {global P; set y $P; list [catch {set y abc} e] $e}; a1}
t C1c "lindex typé dans une proc"       {proc a2 {} {set {list.double}M 1 2; set e [lindex $M 1]; list [ty $e] [catch {set e abc} m] $m}; a2}
t C2b "catch m après valeur typée"      {proc a3 {} {set {list.double}M 1 2; catch {lindex $M 1} m; list [ty $m] [catch {catch {error boom} m} e] $e $m}; a3}
t E1b "proc classique : set p xx"       {proc c1 {p} {set p xx; list $p [ty $p]}; c1 $P}
t B11b "lsearch -real plain / typé"     {list [lsearch -real {1 2 3} 2.0] [lsearch -real $L 2.0] [lsearch -exact $L 2]}
t B8b "concat nus / typés"              {list [concat {1 2} {1 2}] [concat $P $P] [string length [concat $P $P]]}
t D14 "dict.point : valeurs"            {set {dict.point}DP a {1 2}; list [ty [dict get $DP a]] [llength [dict get $DP a]]}
t D15 "lappend d'un point dans liste nue" {set l2 {}; lappend l2 $P; list $l2 [llength $l2] [ty [lindex $l2 0]]}
t D16 "string de liste nue contenant P" {set l3 [list 0 $P]; list $l3 [llength [lindex $l3 1]]}
t D17 "relire la chaîne de l3"          {set l3 [list 0 $P]; set s17 [string trimright "$l3 "]; list $s17 [llength [lindex $s17 1]]}
t B15 "string typée : vue liste"          {set {string}st "a b c"; list [llength $st] [lindex $st 0] [llength "a b c"]}
t B16 "int typé : vue liste"              {list [llength $I] [lindex $I 0]}
