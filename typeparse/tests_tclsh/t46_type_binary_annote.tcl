# type binary : les interrogations sont des annotations locales sur l'argument
# de type ({what?} {sizeof?} {is?}) ; decode/encode/deref/space sont des verbes.
# type binary: questions are local annotations on the type argument
# ({what?} {sizeof?} {is?}); decode/encode/deref/space are verbs.
source [file join [file dirname [file normalize [info script]]] .. layout filebinary.tcl]
source [file join [file dirname [file normalize [info script]]] .. layout binaliases.tcl]

# FR : chaque essai tourne dans sa procédure : decode rend une valeur typée,
#      qu'une variable réutilisée garderait pour contrat.
proc attempt {label script} {
    set c [catch {uplevel 1 $script} r]
    puts "$label: $c: [lindex [split $r \n] 0]"
}

type def entete {{u16be}magic; {u16le}version; {u32le}taille}
set T entete
set bon  [binary decode hex 4d5a010010000000]
set long [binary decode hex 4d5a010010000000ff]
set court [binary decode hex 4d5a]

puts "-- la représentation"
attempt nue        {type binary u16be}
attempt what_lit   {type binary {what?}u16be}
attempt what_var   {type binary {what?}$T}
attempt what_extra {type binary {what?}$T $bon}

puts "-- la taille"
attempt sizeof     {type binary {sizeof?}$T}
attempt sizeof_al  {type binary {sizeof?}$T -align natural}
attempt sizeof_var {type binary {sizeof?}cstring}

puts "-- la validité des octets"
attempt is_ok      {type binary {is?}$T $bon}
attempt is_court   {type binary {is?}$T $court}
attempt is_prefixe {type binary {is?}$T $long}
attempt is_exact   {type binary {is?}$T $long -exact 1}
attempt is_exact_ok {type binary {is?}$T $bon -exact 1}
attempt is_offset  {type binary {is?}u16be $long -offset 7}
attempt is_badopt  {type binary {is?}$T $bon -bogus 1}
attempt is_nodata  {type binary {is?}$T}
attempt is_pas_type {type binary {is?}inconnu $bon}

puts "-- annotations inconnues"
attempt question   {type binary {oups?}$T}
attempt option     {type binary {-v}u16be}
attempt nom        {type binary {mm}u16be}

puts "-- les verbes"
attempt encode     {binary encode hex [type binary encode u16be 258]}
attempt decode     {type binary decode u16be [binary decode hex 0102]}
attempt consumed   {type binary decode u32le [binary decode hex 01000000ff] -consumed n; set n}
attempt space      {type binary space list}
attempt alias      {file binary sizeof $T}

puts "-- un type qui porte le nom d'un verbe"
type def decode u16be
attempt nom_verbe  {type binary decode}
attempt verbe_reste {type binary decode u16be [binary decode hex 0102]}

puts "-- l'annotation ne suit pas la valeur"
proc passe {x} {set y $x; return $y}
attempt fuite      {passe {what?}$T}
attempt rangee     {set z [type binary {what?}$T]; string length $z}

puts "-- sans la couche Tcl"
interp create nu
attempt sans_couche {nu eval {type binary decode u8 x}}
attempt nue_ok      {nu eval {type binary {what?}int(u,8)}}
interp delete nu
