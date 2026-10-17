# Alignement, encodages de chaînes, rétrécissement des listes
# Alignment, string encodings, list narrowing
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]

proc attempt {script} {
    set c [catch {uplevel 1 $script} r]
    puts "$c: [lindex [split $r \n] 0]"
}
proc hex {s} {binary scan $s H* h; return $h}

# --- alignement naturel comme en C : a@0 b@4 c@8 d@16 e@24, taille 32
type def cs {{int(u,8)}a; {int(u,32)}b; {int(u,16)}c; {int(u,64)}d; {int(u,8)}e}
puts "packed  [file binary sizeof cs]"
puts "natural [file binary sizeof cs -align natural]"
puts "align4  [file binary sizeof cs -align 4]"
set {cs}v a 1 b 2 c 3 d 4 e 5
puts [hex [file binary encode cs $v]]
puts [hex [file binary encode cs $v -align natural]]
puts [hex [file binary encode cs $v -align 4]]
set d [file binary decode cs [file binary encode cs $v -align natural] -align natural -consumed n]
puts "$d consumed=$n"
# remplissage non nul : ignoré, sauf en mode strict
set raw [file binary encode cs $v -align natural]
set raw [string replace $raw 1 1 \x7f]
puts [file binary decode cs $raw -align natural]
attempt {file binary decode cs $raw -align natural -strictpad 1}
attempt {file binary sizeof cs -align 3}
attempt {file binary sizeof cs -align wide}

# --- produit imbriqué : alignement du plus grand champ
type def inner {{int(u,8)}x; {int(u,16)}y}
type def outer {{int(u,8)}p; {inner}q; {int(u,32)}r}
puts "outer packed [file binary sizeof outer] natural [file binary sizeof outer -align natural]"
set {outer}o p 1 q {2 3} r 4
puts [hex [file binary encode outer $o -align natural]]
puts [file binary decode outer [file binary encode outer $o -align natural] -align natural]

# --- tableau de produits : la taille de l'élément est un multiple de son alignement
type def elt {{int(u,32)}k; {int(u,8)}f}
type def tab {{list(3).elt}items}
puts "tab packed [file binary sizeof tab] natural [file binary sizeof tab -align natural]"

# --- canal avec alignement
set f [file tempfile name]
file binary write $f cs $v -align natural
close $f
set f [open $name rb]
puts [file binary read $f cs -align natural]
close $f
file delete $name

# --- encodages de chaîne
puts [type binary string(utf-8,8)]
puts [type binary string(utf-16le,z)]
puts [type binary string(raw,4)]
type def txt {{string(utf-16le,6)}w; {string(utf-16be,z)}z; {string(latin1,4)}l; {string(ascii,z)}a}
set {txt}t w "héé" z "日本" l "éà" a "plain"
set b [file binary encode txt $t]
puts [hex $b]
puts [file binary decode txt $b]
attempt {set {string(ascii,3)}bad é}
attempt {set {string(latin1,3)}bad €}
attempt {set {string(utf-8,2)}bad abc}
attempt {set {string(utf-16le,4)}bad abc}
attempt {set {string(utf-8,z)}bad "a\x00b"}
attempt {type binary string(raw,z)}
attempt {type binary string(klingon,3)}
attempt {type binary string(utf-8)}
attempt {type binary string(utf-8,0)}
attempt {file binary decode string(utf-8,z) "abc"}
attempt {file binary decode string(utf-16le,z) "a\x00b\x00"}
puts [file binary decode string(utf-16le,z) "a\x00b\x00\x00\x00rest" -consumed used]
puts "used=$used"
puts [file binary sizeof string(utf-16le,6)]

# --- double et flottants
puts [type binary double(32,le)]
attempt {type binary double(16)}
attempt {type binary double(64,mid)}
puts [hex [file binary encode double(32,be) 1.5]]
puts [file binary decode double(64,le) [file binary encode double(64,le) 0.1]]

# --- arguments invalides de int
attempt {type binary int(u)}
attempt {type binary int(x,8)}
attempt {type binary int(u,9)}
attempt {type binary int(u,2048)}
attempt {type binary int(u,8,be,extra)}
attempt {type binary int(u,8,wide)}
attempt {type binary int}

# --- pas de rétrécissement caché dans les listes
set {list.int}M 1 2 3
attempt {set {list.int(u,8)}L $M}
set {list.int(u,8)}N 1 2 3
attempt {set {list.int}L $N; puts [type get $L]}
attempt {set {list.int(s,16)}L2 $N; puts [type get $L2]}
attempt {set {list.int(u,16,be)}L3 $N; puts [type get $L3]}
attempt {set {list.int(u,8)}L4 1 2 300}
attempt {set {list.int(u,16,be)}L5 1 2; set L5}
