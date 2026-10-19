# usage : tclsh test.tcl [chemin/vers/cstruct.so|.dll]
set here [file dirname [file normalize [info script]]]
source [file join $here .. .. layout filebinary.tcl]
source [file join $here .. .. layout binaliases.tcl]
source [file join $here cstruct.tcl]
cs::init {*}$argv

proc check {label cond} {
    set ok [uplevel 1 [list expr $cond]]
    puts [format "%-52s %s" $label [expr {$ok ? "OK" : "ÉCHEC"}]]
}

puts "--- 1. La disposition : le compilateur C et nos types décrivent les mêmes octets"
set c [cs::layout]
check "sizeof Sample : C=[dict get $c sizeof(Sample)]" \
    {[type binary {sizeof?}Sample -align natural] == [dict get $c sizeof(Sample)]}
check "sizeof Summary : C=[dict get $c sizeof(Summary)]" \
    {[type binary {sizeof?}Summary -align natural] == [dict get $c sizeof(Summary)]}
# FR : décalages mesurés côté Tcl en encodant des valeurs repères
# EN : offsets measured on the Tcl side by encoding landmark values
proc offsetOf {type probe marker} {
    set bytes [type binary encode $type $probe -align natural]
    return [string first $marker $bytes]
}
set s [set {Sample}p {id}0xAABBCCDD {value}0.0 {flags}0xEE]
check "Sample.id    : C=[dict get $c Sample.id]" \
    {[offsetOf Sample $s \xDD] == [dict get $c Sample.id]}
check "Sample.flags : C=[dict get $c Sample.flags]" \
    {[offsetOf Sample $s \xEE] == [dict get $c Sample.flags]}
set s2 [set {Sample}q {id}0 {value}1.0 {flags}0]
check "Sample.value : C=[dict get $c Sample.value]" \
    {[expr {[offsetOf Sample $s2 \xF0] - 6}] == [dict get $c Sample.value]}
set bt [set {Batch}b0(n,items) 1 {{0xAABBCCDD 1.0 0}}]
check "Batch.items  : C=[dict get $c Batch.items]" \
    {[offsetOf Batch $bt \xDD] == [dict get $c Batch.items] + [dict get $c Sample.id]}

puts "--- 2. Tcl -> C : un lot de mesures construit en Tcl"
set {Batch}b(n,items) 5 {
    {1 20.5 0} {2 18.0 0} {3 99.9 1} {4 22.5 0} {5 19.0 0}
}
puts "type : [type get $b]   taille encodée : [string length [type binary encode Batch $b -align natural]] octets"
set r1 [cs::stats $b]
set r2 [cs::stats_typed $b]
puts "voie rapide (octets)  : $r1"
puts "voie objet (annoté)   : $r2"
check "les deux voies donnent le même résultat" {$r1 eq $r2}
check "la mesure invalide (flags=1) est ignorée : count=4" {$r1(count) == 4}
check "min/max/mean attendus (18.0 / 22.5 / 20.0)" \
    {$r1(min) == 18.0 && $r1(max) == 22.5 && $r1(mean) == 20.0}
puts "le résultat est un produit annoté : [type get $r1] ; min=$r1(min)"

puts "--- 3. C -> Tcl : C fabrique 1000 mesures"
set big [cs::make 1000 42]
puts "type : [type get $big] ; n=$big(n) ; premier : [lindex $big(items) 0]"
set sum 0.0; set cnt 0; set mn ""; set mx ""
foreach it $big(items) {
    if {$it(flags) & 1} continue
    set v $it(value); set sum [expr {$sum + $v}]; incr cnt
    if {$mn eq "" || $v < $mn} {set mn $v}
    if {$mx eq "" || $v > $mx} {set mx $v}
}
set rc [cs::stats $big]
check "C et un calcul en Tcl pur concordent (count=$rc(count))" \
    {$rc(count) == $cnt && $rc(min) == $mn && $rc(max) == $mx && abs($rc(mean) - $sum/$cnt) < 1e-9}
check "stats sur le lot relu = stats sur ses octets bruts" \
    {[cs::stats_typed $big] eq $rc}

puts "--- 4. Les erreurs sont attrapées, pas ignorées"
proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "  $c: [lindex [split $r \n] 0]"
}
puts "- un Batch encodé sans -align natural (disposition packed) :"
attempt {cs::stats_raw [type binary encode Batch $b]}
puts "- une valeur qui n'est pas un Batch :"
attempt {cs::stats_obj [set {Sample}z {id}1 {value}2.0 {flags}0]}
attempt {cs::stats_obj {n 1 items {}}}
puts "- n incohérent avec le nombre d'éléments : refusé dès la construction :"
attempt {set {Batch}bad(n,items) 3 {{1 1.0 0}}}
puts "- si on modifie n après coup (état intermédiaire légitime), l'encodage et C le vérifient :"
set {Batch}ok(n,items) 1 {{1 1.0 0}}
set ok(n) 3
puts "  voie rapide :"
attempt {cs::stats $ok}
puts "  voie objet :"
attempt {cs::stats_obj $ok}
