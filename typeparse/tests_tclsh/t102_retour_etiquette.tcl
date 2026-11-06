# t102 -- v16x : l'étiquette d'un retour, return {v:}$x, dans un corps non
# compilé (eval, uplevel, switch -matchvar) : elle est lue par le contrôle du
# retour de la procédure typée, comme dans un corps compilé.
proc t {label script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-34s %s" $label $r]
}
type def K {{int}a, {int}b}
proc {K}c1 {} {return {a:}1}
proc {K}r1 {} {eval {return {a:}1}}
proc {K}r2 {} {uplevel 0 {return {a:}1}}
proc {K}r3 {} {uplevel 1 {return {a:}1}}
proc {K}r4 {s} {switch -regexp -matchvar m -- $s {{^a(\d+)$} {return {a:}[lindex $m 1]} default {return {b:}0}}}
proc {K}r5 {} {foreach x {1} {eval {return {b:}7}}}
proc {K}r6 {} {eval {eval {return {a:}2}}}
proc {K}w1 {} {eval {return {a 1}}}
proc {K}w2 {s} {switch -regexp -matchvar m -- $s {{^a(\d+)$} {return [list a [lindex $m 1]]} default {return {b 0}}}}
t "compilé"                      {set v [c1]; list $v [type get $v]}
t "eval"                         {set v [r1]; list $v [type get $v]}
t "uplevel 0"                    {set v [r2]; list $v [type get $v]}
t "uplevel 1"                    {set v [r3]; list $v [type get $v]}
t "switch -matchvar a5"          {set v [r4 a5]; list $v [type get $v]}
t "switch -matchvar zz"          {set v [r4 zz]; list $v [type get $v]}
t "foreach puis eval"            {set v [r5]; list $v [type get $v]}
t "eval imbriqué"                {set v [r6]; list $v [type get $v]}
t "en clair, eval"               {set v [w1]; list $v [type get $v]}
t "en clair, switch"             {set v [w2 a3]; list $v [type get $v]}
puts "--- ce qui ne change pas"
proc f {} {eval {return {a:}1}}
proc {int}g {} {eval {return {a:}1}}
proc {K}h {} {catch {eval {return {a:}1}} r o; return -options $o $r}
proc {K}z {} {eval {return {z:}1}}
t "procédure classique"          {set v [f]; list $v [type get $v]}
t "retour qui n'est pas une somme" {g}
t "catch, puis return -options"  {set v [h]; list $v [type get $v]}
t "variante inconnue"            {z}
