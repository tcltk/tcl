# type map -fold (v17q) : un cumul ; le corps donne la nouvelle valeur de s
type def {-compact}vd list.double
proc essai {nom script} { if {[catch {uplevel 1 $script} r]} { set r "ERR $r" }; puts "$nom : $r" }
proc boucle {init body U V} { set s $init; foreach u $U v $V { set s [expr $body] }; return $s }
set U {1.5 2.5 -3.0 4.0 0.25}; set V {2.0 0.5 1.0 -1.0 8.0}
set CU [type cast vd $U]; set CV [type cast vd $V]
essai "produit scalaire"     {type map -fold {s 0} {u} $U {v} $V {($s + $u*$v)}}
essai "même, boucle"         {boucle 0 {$s + $u*$v} $U $V}
essai "s après"              {set s}
essai "en cases"             {type map -fold {s 0.0} {u} $CU {v} $CV {($u*$v + $s)}}
essai "lissage"              {type map -fold {s 0.0} {u} $U {(0.9*$s + 0.1*$u)}}
essai "lissage, boucle"      {boucle 0.0 {0.9*$s + 0.1*$u} $U $V}
essai "récurrence"           {type map -fold {s 1.0} {u} $U {($u - $s / 2)}}
essai "entiers exacts"       {type map -fold {s 1} {u} {10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160 170 180 190 200} {($s * $u)}}
essai "liste vide"           {type map -fold {s 42} {u} {} {($s + $u)}}
essai "texte"                {type map -fold {s ""} {u} {a b c} {([string cat $s $u])}}
essai "max par ?:"           {type map -fold {s -Inf} {u} $U {($s > $u ? $s : $u)}}
essai "débordement"          {type map -fold {s 1.0} {u} {1e200 1e200 0.5} {($s * $u)}}
essai "division par zéro"    {type map -fold {s 0} {u} {1 0 2} {($s + 1/$u)}}
essai "s variable de boucle" {type map -fold {u 0} {u} $U {($u)}}
essai "-fold incomplet"      {type map -fold {s} {u} $U {($u)}}
essai "signature typée"      {type map -fold {s 0.0} {{double}u; {double}v} [list $U $V] {double}{($s + $u*$v)}}
set k 0.5
essai "valeur fixe"          {type map -fold {s 0.0} {u} $U {($s + $k * $u)}}
set big {}; for {set i 0} {$i < 1000} {incr i} { lappend big [expr {sin($i)}] }
essai "1000, blocs = boucle" {expr {[type map -fold {s 0.0} {u} [type cast vd $big] {(0.5*$s + $u)}] eq [boucle 0.0 {0.5*$s + $u} $big $big]}}
