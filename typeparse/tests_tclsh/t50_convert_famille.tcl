# t50 : règles de conversion de FAMILLE (type convert famille famille).
# Une famille est un type def paramétré ; ses paramètres fantômes (dim, f, o)
# distinguent les instances sans changer la représentation (un double). Une
# règle de famille reçoit la valeur et les paramètres de l'instance source et
# de l'instance cible, chacun comme valeur d'un produit ($de(f)) ; elle répond
# « non applicable » par return -code continue. Les conversions restent
# explicites : set {m}b $a (a : mm) est refusé, set b {m}$a convertit.
proc T {label script} { set r [catch {apply [list {} $script]} m]; puts [format "%-34s %s" $label [expr {$r ? "ERREUR: [lindex [split $m \n] 0]" : "ok: $m"}]] }
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def m       {grandeur(L)}
type def mm      {grandeur(L, 0.001)}
type def km      {grandeur(L, 1000.0)}
type def kg      {grandeur(M)}
type def K       {grandeur(Θ)}
type def degC    {grandeur(Θ, 1.0, 273.15)}
type def couple  {grandeur(M.L2.T-2)}
type def energie {grandeur(M.L2.T-2)}
T "règle de famille"         {type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}}
T "mm -> m"                  {set {mm}a 1500; set b {m}$a; list [type get $b] $b}
T "mm -> km"                 {set {mm}a 1500; set b {km}$a; list [type get $b] $b}
T "K -> degC"                {set {K}t 300; set c {degC}$t; list [type get $c] $c}
T "degC -> K"                {set {degC}t 25; set c {K}$t; list [type get $c] $c}
T "mm -> kg (continue)"      {set {mm}a 1500; set c {kg}$a}
T "couple -> energie"        {set {couple}x 3; set e {energie}$x}
T "instance directe"         {set {grandeur(L,0.01)}cm 12; set b {m}$cm; list [type get $b] $b}
T "type cast"                {set {mm}a 1500; type cast km $a}
T "contrat reste explicite"  {set {mm}a 1500; set {m}b 1; set b $a}
T "de est un produit"        {type convert grandeur int {{x de} {return -code error [list [type get $de] $de(dim) $de(f) $de(o)]}}; set {mm}a 2; set d {int}$a}
T "règle qui échoue"         {type convert grandeur kg {{x de} {error "pas de masse pour $de(dim)"}}; set {mm}a 2; set k {kg}$a}
T "spécifique prioritaire"   {type convert mm m {x {expr {$x / 1000. + 0.5}}}; set {mm}a 1000; set b {m}$a}
T "inconnu"                  {type convert zz grandeur {x {}}}
