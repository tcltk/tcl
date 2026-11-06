# sondes_v16w.tcl -- les mêmes sondes après la v16w (sommes balisées, unions)
# (doc/DIAGNOSTIC-SOMMES.md, § 7).   tclsh sondes_v16w.tcl
proc t {label script} {
    if {[catch {uplevel 1 "unset -nocomplain k k2 e l p r t u v x L M D; $script"} r]} {
	set r "ERREUR: $r"
    }
    puts [format "%-40s %s" $label $r]
}
proc fresh {x} {string range [string cat _ $x] 1 end}   ;# même texte, objet neuf

type def K {{int}a, {int}b}
type def S {{int}i, {string}s}
type def temp {{double}celsius, {double}fahrenheit}
type def lit {"on", "off"}
type def forme {{double}cercle, {{double}w; {double}h}rect}
type def E {{K}k, {string}s}
type def P {{K}k; {int}n}
proc {string}fs {{string}s} {return $s}
proc {int}fi {{int}n} {return $n}
proc {K}fk {{K}x} {return $x}

puts "== S1. la variante ne passe pas par la chaîne"
t {K.b, même objet}             {set {K}k {b:}5; set {K}k2 $k; type get $k2}
t {K.b, chaîne neuve}           {set {K}k {b:}5; set {K}k2 [fresh $k]; type get $k2}
t {list.K, chaîne neuve}        {set {list.K}L {a:}1 {b:}2; set {list.K}M {*}[fresh $L]}
t {temp.fahrenheit, chaîne}     {set {temp}t {fahrenheit:}50.0; set {temp}u [fresh $t]; type get $u}
t {S.s "5", chaîne}             {set {S}u {s:}5; set {S}x [fresh $u]; type get $x}
t {S.s "5", incr}               {set {S}u {s:}5; incr u; type get $u}

puts "== S2. une valeur typée invalide"
proc {P}h {} {set {K}k {b:}5; return [list $k 3]}
t {return [list $k 3] : accepté}  {type get [h]}
t {... puis son champ k}        {set r [h]; type get $r(k)}
t {type cast P : correct}       {set {K}k {b:}5; set r [type cast P [list $k 3]]; type get $r(k)}

puts "== S3. une étiquette relit la valeur par sa chaîne"
t {{k:} sur un K.b}             {set {E}e {k:}[fk {b:}5]; type get $e}
t {sans étiquette : déduit}     {set {K}k {b:}5; set {E}e $k; type get $e(k)}

puts "== S4. la durée de vie d'une étiquette dépend de la compilation"
proc {K}r1 {} {return {a:}1}
proc {K}r2 {} {eval {return {a:}1}}
proc {K}r3 {} {uplevel 0 {return {a:}1}}
proc {K}r4 {s} {switch -regexp -matchvar m -- $s {{^a(\d+)$} {return {a:}[lindex $m 1]} default {return {b:}0}}}
proc {K}r5 {s} {switch -regexp -- $s {{^a\d+$} {return {a:}5} default {return {b:}0}}}
t {return {a:}1}                {type get [r1]}
t {eval {return {a:}1}}         {type get [r2]}
t {uplevel 0 {return {a:}1}}    {type get [r3]}
t {switch -matchvar}            {type get [r4 a5]}
t {switch sans -matchvar}       {type get [r5 a5]}

puts "== S5. les commandes qui reçoivent une étiquette"
t {set {K}k {b:}5}              {set {K}k {b:}5; type get $k}
t {proc {{K}x} {b:}5}           {type get [fk {b:}5]}
t {lappend L {b:}3}             {set {list.K}L {a:}1; lappend L {b:}3; type get $L(1)}
t {linsert $L end {b:}3}        {set {list.K}L {a:}1; set L [linsert $L end {b:}3]; type get $L(1)}
t {dict set D b {b:}2}          {set {dict.K}D a {a:}1; dict set D b {b:}2; type get $D(b)}
t {list {b:}5 (perdue)}         {type get [lindex [list {b:}5] 0]}
t {set e {b:}5 (refus)}         {set e {b:}5}

puts "== S6. une somme ne se lit pas comme sa variante"
t {lit -> {string}}             {set {lit}l on; fs $l}
t {S.i -> {int}}                {set {S}v 5; fi $v}
t {type cast int S.i}           {set {S}v 5; type cast int $v}
t {type cast S K.a}             {set {K}k {a:}5; type get [type cast S $k]}
t {expr sur S.i : permis}       {set {S}v 5; expr {$v * 2}}
t {string length lit : permis}  {set {lit}l on; string length $l}

puts "== S7. le type d'une variante, à moitié"
t {set {forme.rect}f 2.0 3.0}   {set {forme.rect}f 2.0 3.0; type get $f}
t {... f(w)}                    {set {forme.rect}f 2.0 3.0; set f(w)}
t {... f(rect.w)}               {set {forme.rect}f 2.0 3.0; set f(rect.w)}

puts "== S8. égalité et clés"
t {K.a 5 eq K.b 5}              {set {K}k {a:}5; set {K}k2 {b:}5; expr {$k eq $k2}}
t {dict de deux clés}           {set {K}k {a:}5; set {K}k2 {b:}5; dict size [dict create $k 1 $k2 2]}
t {lsearch K.b}                 {set {K}k {a:}5; set {K}k2 {b:}5; set {list.K}L $k $k2; lsearch $L $k2}

puts "== S9. la variante collante"
t {temp.celsius : set t 50.0}   {set {temp}t {celsius:}100.0; set t 50.0; type get $t}
t {S.s abc : set v 5}           {set {S}v abc; set v 5; type get $v}

puts "== S10. introspection"
t {type get}                    {set {K}k {b:}5; type get $k}
t {type get list.K}             {set {list.K}L {a:}1; type get $L}
t {la variante seule}           {set {K}k {b:}5; type variant $k}
t {type is K.b \$k}             {set {K}k {b:}5; type is K.b $k}

puts "== S11. grammaire"
t {variante produit à un champ} {type def F1 {{{double}r}cercle, {string}autre}}
t {produit à un champ seul}     {type def F2 {{double}r}; type def F2}

puts "== S12. quatre régimes"
t {disjointe : 5, abc}          {type def D1 {{int}i, {list(2).int}c}; type is {disjoint?}D1}
t {recouvrante, sans option}    {type is {disjoint?}temp}
