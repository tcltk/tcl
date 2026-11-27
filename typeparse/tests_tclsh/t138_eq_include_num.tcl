# t138 (v17z) : type eq, type include, num intégré, int -> double exact ou en erreur, inexact.
type def mm double
type def px double
type def pair int(($%2==0))
type def point {{double}x; {double}y}
type def pt2 {{double}x; {double}y}
type def temp {{double}celsius, {double}fahrenheit}
type def S {{int}i, {string}s}
type def E3 {"a", "b", "c"}
type def E2 {"a", "b"}
type def opt2 {{int}some, "none"}
foreach {U T} {
 string temp  int int  int int(s,16)  int(s,16) int(u,8)  int(u,8) int
 double mm  mm double  mm px  int pair  pair int  pair pair
 double int  double int(s,32)  double int(s,64)  double int(u,8)  double double(32)  double(32) double
 num int  num double  num int(u,8)  num mm  double num  int num  string num
 temp temp.celsius  double temp.celsius  double temp
 S int  S string  E3 E2  E2 E3
 list.int list.pair  list.pair list.int  list.double list.int  list(2).int list.int  list.int list(2).int
 point pt2  point point  var.int var.pair  var.int var.int
 opt2 int
 double inexact  inexact double
} {
  if {[catch {type include $U $T} r]} {set r "ERR $r"}
  puts [format "%-14s ⊇ %-14s %s" $U $T $r]
}



proc t {nom script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR $r"}
    puts [format "%-40s %s" $nom $r]
}
puts "== type eq : les types seuls"
type convert temp.fahrenheit temp.celsius {{f} {expr {($f - 32) * 5 / 9.0}}}
set {temp}a {celsius:}10.0; set {temp}b {celsius:}20.0; set {temp}c {fahrenheit:}10.0
t "même variante, valeurs différentes"   {type eq $a $b}
t "même valeur, variantes différentes"   {list [expr {$a eq $c}] [type eq $a $c]}
t "nu et typé"                           {type eq 5 [type {int}5]}
t "deux nus"                             {type eq 5 6}
t "nominal et sa base"                   {type eq [type {mm}5] [type {double}5]}
t "les notes ne comptent pas"            {type eq {note}[type {int}5] [type {int}6]}
t "addition d'unités"                    {if {![type eq $a $c]} {set c [type cast [type get $a] $c]}; list $c [expr {$a + $c}]}
puts "== num"
t "num 5"                                {set {num}n 5; list $n [type get $n]}
t "num 5.0"                              {set {num}n 5.0; list $n [type get $n]}
t "num 0x10"                             {set {num}n 0x10; list $n [type get $n]}
t "num abc"                              {set {num}n abc}
t "variantes"                            {type variant -of num}
t "redéfinir num"                        {type def num {{int}i | {double}d}}
t "oublier num"                          {type forget num}
t "{double:}5"                           {set {num}n {double:}5; list $n [type get $n]}
puts "== int -> double : exact, ou une erreur"
t "5"                                    {set {double}x {int}5; set x}
t "2^53"                                 {set {double}x {int}9007199254740992; set x}
t "2^53+1"                               {set {double}x {int}9007199254740993}
t "2^53+2 (exact)"                       {set {double}x {int}9007199254740994; set x}
t "-(2^53+1)"                            {set {double}x {int}-9007199254740993}
t "10^20 (exact)"                        {set {double}x {int}100000000000000000000; set x}
t "10^20+1"                              {set {double}x {int}100000000000000000001}
t "cast"                                 {type cast double [type {int}9007199254740993]}
t "argument"                             {proc f {{double}d} {return $d}; f [type {int}9007199254740993]}
t "liste"                                {set {list.double}L [type {list.int}{1 2 9007199254740993}]}
t "texte nu : lu par Tcl"                {set {double}x 9007199254740993; set x}
t "inexact"                              {set y [type cast inexact [type {int}9007199254740993]]; list $y [type get $y]}
t "inexact entre dans double"            {set {double}w [type cast inexact [type {int}9007199254740993]]; list $w [type get $w]}
t "double n'entre pas dans inexact"      {set {inexact}z [type {double}1.5]}
