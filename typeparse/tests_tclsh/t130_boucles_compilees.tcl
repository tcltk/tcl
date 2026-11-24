# Boucles vectorisées compilées (v17s) : foreach/lmap à corps littéral dans
# une procédure passent d'abord par ::tcl::unsupported::vecloop ; incr s ?E?
# reconnu comme cumul d'entiers. Résultats, erreurs et variables : ceux de la
# boucle ordinaire (TCL_TYPE_NOVECLOOP=1 donne la même sortie).
type def {-compact}octets list.int(u,8)
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r o]} { set r "ERR $r | [lindex [split [dict get $o -errorinfo] \n] end]" }
    puts "$nom : $r"
}
set L [lrepeat 40 3]
set O [type cast octets [lrepeat 100 7]]
proc i1 {L} {set c 0; foreach v $L {incr c $v}; list $c $v}
proc i2 {L} {set c 0; foreach v $L {incr c}; set c}
proc i3 {L} {set c 0; foreach v $L {incr c -2}; set c}
proc i4 {L} {set c 0; foreach v $L {incr c [expr {$v*2}]}; set c}
proc i5 {L {c 0}} {foreach v $L {incr c $v}; set c}
proc f1 {U V} {set s 0.0; foreach u $U v $V {set s [expr {$s + $u*$v}]}; set s}
proc f2 {L} {set c 0; foreach v $L {(c = $c + $v)}; set c}
proc l1 {L} {lmap v $L {($v * 2)}}
proc l2 {U V} {lmap u $U v $V {expr {$u - $v}}}
proc court {L} {set c 0; foreach v $L {incr c $v}; set c}
proc brk {L} {set c 0; foreach v $L {incr c $v; if {$c > 10} break}; set c}
essai "incr \$v"             {i1 $L}
essai "incr"                 {i2 $L}
essai "incr -2"              {i3 $L}
essai "incr \[expr\]"        {i4 $L}
essai "incr octets"          {i1 $O}
essai "incr grand entier"    {i5 [lrepeat 20 [expr {2**62}]]}
essai "incr 1.5 (erreur)"    {i5 [concat $L 1.5 $L]}
essai "incr abc (erreur)"    {i5 [concat $L abc $L]}
essai "incr s non entier"    {i5 $L 1.5}
essai "incr 0x10"            {i5 $L 0x10}
essai "produit scalaire"     {f1 [lrepeat 30 0.5] [lrepeat 30 4]}
essai "(c = \$c + \$v)"      {f2 $L}
essai "lmap (E)"             {llength [set r [l1 $L]]}
essai "lmap valeurs"         {lrange [l1 $O] 0 3}
essai "lmap expr"            {lrange [l2 [lrepeat 20 5] [lrepeat 20 2.5]] 0 2}
essai "lmap longueurs"       {l2 [lrepeat 20 1] [lrepeat 19 1]}
essai "courte"               {court {1 2 3}}
essai "break (non)"          {brk $L}
essai "désassemblage"        {expr {[string first vecloop [tcl::unsupported::disassemble proc i1]] >= 0}}
essai "non vectorisable"     {expr {[string first vecloop [tcl::unsupported::disassemble proc brk]] >= 0}}
