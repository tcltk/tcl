# foreach et lmap vectorisés par le registre (v17r) : le résultat, les
# erreurs et les variables sont ceux de la boucle ordinaire
type def {-compact}vd list.double
proc essai {nom script} {
    if {[catch {uplevel 1 $script} r o]} { set r "ERR $r | [lindex [split [dict get $o -errorinfo] \n] end]" }
    puts "$nom : $r"
}
set U {}; set V {}
for {set i 0} {$i < 40} {incr i} { lappend U [expr {$i * 0.5}]; lappend V [expr {$i - 3}] }
set CU [type cast vd $U]
proc corps {b} { return $b }
essai "lmap (E)"             {lmap u $U v $V [corps {($u*2 + $v)}]}
essai "lmap expr"            {lmap u $U v $V [corps {expr {$u*2 + $v}}]}
essai "lmap cases"           {lmap u $CU [corps {($u / 4)}]}
essai "variables après"      {list $u $v}
essai "fold (s = …)"         {set s 0; foreach u $U v $V [corps {(s = $s + $u*$v)}]; set s}
essai {fold set s [expr]}    {set s 0; foreach u $U v $V [corps {set s [expr {$s + $u*$v}]}]; set s}
essai "fold lissage"         {set s 0.0; foreach u $CU [corps {(s = 0.9*$s + 0.1*$u)}]; set s}
essai "fold ?: (non)"        {set s 0; foreach u [lreplace $U 3 3 1e308] [corps {(s = $s > $u ? $s : $u)}]; set s}
essai "longueurs différentes" {lmap u $U v [lrange $V 0 end-1] [corps {($u + 0)}]}
essai "erreur"               {lmap u $V [corps {(10 / $u)}]}
essai "s inexistante"        {unset -nocomplain z; foreach u $U [corps {(z = $z + $u)}]}
set n 0
trace add variable t write {apply {args {incr ::n}}}
essai "trace (non)"          {lmap t $U [corps {($t + 1)}]; set n}
trace remove variable t write {apply {args {incr ::n}}}
essai "variable typée (non)" {set {double}w 0.0; lmap w $U [corps {($w + 1)}]; set w}
essai "corps non pur (non)"  {lmap u $U [corps {($u + [llength $V])}]}
essai "court (non)"          {lmap u {1 2 3} [corps {($u * 2)}]}
essai "compilé (inchangé)"   {lmap u $U {expr {$u * 2}}}
