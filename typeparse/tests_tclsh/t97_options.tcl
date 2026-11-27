# t97 : v16q. Options et drapeaux des procédures typées.
#  - un champ dont le nom commence par « - » est une option, donnée au début
#    de l'appel (jusqu'au premier mot qui n'en est pas une, ou « -- ») ; la
#    locale perd le « - » ; un préfixe unique suffit ;
#  - {flag}-x : un drapeau (1 ou 0) ; {T}-x ?= v : une option à valeur ;
#  - {"a", "b", {T}c}-m ?= a : options exclusives -a, -b, -c valeur (ou -m a) ;
#    la locale m reçoit le nom de la variante, la locale c sa valeur ; une
#    variante produit prend ses champs à la suite, ses champs « -x » sont des
#    options qui dépendent d'elle ;
#  - {T}-o, T un produit : un groupe ; ses champs sont les options, o une
#    valeur de T.
proc t {label script} {
    if {[catch {uplevel #0 $script} r]} { puts "$label: ERREUR $r" } else { puts "$label: $r" }
}
proc trier {
    {flag}-decreasing;
    {flag}-unique;
    {int}-index ?= -1;
    {"ascii", "dictionary", "integer", {string}command}-mode ?= ascii;
    {list}l
} {
    list dec=$decreasing uni=$unique idx=$index mode=$mode [expr {[info exists command] ? "cmd=$command" : "-"}] l=$l
}
t "rien"          {trier {3 1 2}}
t "drapeaux"      {trier -decreasing -unique {3 1 2}}
t "préfixe"       {trier -dec -ind 2 {3 1 2}}
t "ambigu"        {trier -d {3 1 2}}
t "inconnue"      {trier -zz {3 1 2}}
t "exclusives"    {trier -ascii -integer {3 1 2}}
t "même deux fois" {trier -integer -integer {3 1 2}}
t "-integer"      {trier -integer {3 1 2}}
t "-command"      {trier -command mycmp {3 1 2}}
t "-mode"         {trier -mode dictionary {3 1 2}}
t "-mode mauvais" {trier -mode zz {3 1 2}}
t "-mode command" {trier -mode command {3 1 2}}
t "valeur manque" {trier -index}
t "index mauvais" {trier -index x {3}}
t "--"            {trier -- -decreasing}
t "nombre"        {trier -5}
t "usage"         {trier}
t "info args"     {info args trier}
proc parcours {
    {"plat", {{string}cmd; {int}-depth ?= 1; {flag}-follow}command}-mode ?= plat;
    {string}dir
} {
    set r [list mode=$mode dir=$dir]
    foreach v {cmd depth follow} { if {[info exists $v]} { lappend r $v=[set $v] } }
    set r
}
t "dépendantes : rien"    {parcours /tmp}
t "dépendantes : -command" {parcours -command ls /tmp}
t "dépendantes : -depth"  {parcours -command ls -depth 3 -follow /tmp}
t "dépendante seule"      {parcours -depth 3 /tmp}
t "usage dép"             {parcours}
type def triopts {
    {flag}decreasing;
    {int}index ?= -1;
    {"ascii", "dictionary", {string}command}mode ?= "ascii"
}
proc trier2 {{triopts}-o; {list}l} { list $o [type get $o] $o(decreasing) $o(index) $o(mode) [type get $o(mode)] }
t "groupe : rien"         {trier2 {1 2}}
t "groupe : options"      {trier2 -decreasing -index 3 -dictionary {1 2}}
t "groupe : -command"     {trier2 -command foo {1 2}}
t "groupe : exclusives"   {trier2 -ascii -dictionary {1 2}}
t "groupe : -mode"        {trier2 -mode dictionary {1 2}}
t "groupe : usage"        {trier2}
t "doublon"               {proc dd {{flag}-a; {"a","b"}-m} {}}
t "défaut pas une variante" {proc dv {{"a","b"}-m ?= c} {}}
t "info default"          {list [info default trier -index v] $v [info default trier -decreasing v] $v}
t "type proc"             {dict get [type proc trier] args}
t "{flag} hors procédure" {set {flag}f 1; type get $f}
t "option et étiquette"   {proc oe {{flag}-v; {int}a; {int}b ?= 0} {list $v $a $b}; oe -v 1 {/b}5}
t "option et args"        {proc oa {{flag}-v; {list.int}args} {list $v $args}; oa -v 1 2 3}
t "argument -5"           {proc on {{flag}-v; {int}a} {list $v $a}; on -5}
t "valeur commençant par -" {on -- -x}
t "type de retour"        {proc {int}or {{flag}-double; {int}a} {expr {$double ? 2 * $a : $a}}; list [or 4] [or -d 4] [type get [or -d 4]]}
t "appel compilé"         {proc oc {} { list [trier -dec -command foo {1}] [parcours -c ls -dep 2 /] }; oc}
t "-- termine les options" {list [trier -- -decreasing] [trier -dec -- -x]}
t "-- seul"               {trier --}
t "-- en valeur"          {trier -index -- {1 2}}
t "sans options : -- est un argument" {proc so {{string}a; {string}b ?= B} {list $a $b}; so -- x}
