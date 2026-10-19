# Niveau 2 : tailles, nombres et positions lus dans le fichier ; sommes étiquetées
# Level 2: sizes, counts and positions read from the file; tagged sums
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# --- 1. préfixe de longueur (Pascal, TLV) / length prefix
type def pas {{string(utf-8,int(u,16,be))}name; {int(u,8)}k}
set {pas}p(name,k) "héllo" 7
set b [type binary encode pas $p]
puts [hex $b]
puts [type binary decode pas $b -consumed n]
puts "consumed $n"
attempt {type binary {sizeof?}pas}

# --- 2. nombre d'éléments en préfixe / count prefix
type def cnt {{list(int(u,8)).int(u,16,be)}vals}
set {cnt}c(vals) {1 2 515}
puts [hex [type binary encode cnt $c]]
puts [type binary decode cnt [type binary encode cnt $c]]
puts [type binary decode cnt "\x00"]

# --- 3. taille lue dans un autre champ / size from another field
type def tlv {{int(u,8)}tag; {int(u,16,be)}len; {string(raw,$len)}data}
set {tlv}t {tag}9 {len}3 {data}abc
puts [hex [type binary encode tlv $t]]
puts [type binary decode tlv [type binary encode tlv $t]]
attempt {type binary encode tlv [set {tlv}bad {tag}9 {len}4 {data}abc]}
attempt {type binary decode tlv "\x09\x00\x05ab"}

# --- expression : la longueur inclut l'en-tête / expression: length includes header
type def msg {{int(u,8)}len; {string(utf-8,$len - 1)}text}
puts [hex [type binary encode msg [set {msg}m {len}4 {text}abc]]]
puts [type binary decode msg "\x04abc"]

# --- le nombre vient d'un champ / count from a field
type def tab {{int(u,8)}n; {list($n).int(u,16,be)}v; {int(u,8)}last}
set {tab}tb(n,v,last) 2 {258 3} 9
puts [hex [type binary encode tab $tb]]
puts [type binary decode tab [type binary encode tab $tb]]

# --- champ non encore lu : refusé à la définition / field not yet read
type def mauvais {{string(raw,$len)}data; {int(u,8)}len}
attempt {type binary mauvais}

# --- 4. somme à étiquette / tagged sum
type def pp {{int(u,8)}x; {int(u,8)}y}
type def corps {{pp}1, {string(utf-8,z)}2, {list(int(u,8)).int(u,8)}3}
type def paquet {{int(u,8)}tag; {corps($tag)}body; {int(u,8)}end = 255}
set {pp}p1 {x}3 {y}4
set {paquet}q(tag,body) 1 {1}$p1
puts [hex [type binary encode paquet $q]]
set d [type binary decode paquet [type binary encode paquet $q]]
puts "$d [type get $d(body)] $d(body.1.x)"
set d2 [type binary decode paquet "\x02hi\x00\xff"]
puts "$d2 [type get $d2(body)]"
set d3 [type binary decode paquet "\x03\x02\x0a\x0b\xff"]
puts "$d3 [type get $d3(body)]"
attempt {type binary decode paquet "\x07\x00"}
set {pp}p2 {x}1 {y}2
set {paquet}bad(tag,body) 2 {1}$p2
attempt {type binary encode paquet $bad}
puts [type binary paquet]

# --- 5. position ailleurs / located value
type def entete {{int(u,32,be)}off; {int(u,16,be)}cnt; {at($off).list($cnt).int(u,16,be)}items; {int(u,8)}end}
set {entete}h(off,cnt,items,end) 10 3 {1 2 3} 255
set b [type binary encode entete $h]
puts [hex $b]
puts [type binary decode entete $b -consumed n]
puts "consumed $n"
# canal / channel
set f [file tempfile name]
puts -nonewline $f "XX"
chan configure $f -translation binary
puts -nonewline $f $b
close $f
set f [open $name rb]
read $f 2
puts [chan binary read $f entete]
puts "position [tell $f]"
close $f
file delete $name
attempt {type binary {sizeof?}entete}

# --- 6. imbriqué : $h(off) / nested
type def h2 {{int(u,8)}k; {int(u,8)}m}
type def blk {{h2}h; {string(raw,$h(m))}data}
set {blk}bl(h,data) {1 3} xyz
puts [hex [type binary encode blk $bl]]
puts [type binary decode blk [type binary encode blk $bl]]

# --- 7. erreurs de syntaxe / syntax errors
attempt {type binary string(utf-8,int(s,8))}
attempt {type binary string(utf-8,double(32))}
attempt {type binary {string(utf-8,$)}}
attempt {type binary at(4)}
attempt {type def sz {{int(u,8)}1, "X"}; type binary {sz($t)}}
attempt {type binary {int($x)}}
