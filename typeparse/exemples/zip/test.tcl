# test.tcl -- zipmem : une archive zip lue en mémoire, et sourcée
#
#   tclsh test.tcl
#
# app.zip : main.tcl (deflate), lib/util.tcl (stored), données/é.txt, un
# commentaire. flux.zip : écrite d'un trait (zip - -), avec les
# enregistrements zip64. Les autres archives sont faites ici (zipfs mkzip,
# une archive derrière d'autres octets).

set here [file dirname [file normalize [info script]]]
source [file join $here zipmem.tcl]
proc slurp {f} {set c [open $f rb]; set d [read $c]; close $c; return $d}
proc t {label script} {
    if {[catch {uplevel 1 $script} r]} {set r "ERREUR: $r"}
    puts [format "%-30s %s" $label $r]
}

set d [slurp [file join $here app.zip]]
puts "--- le format, par ses types"
t "une archive ?"              {list [zipmem::is $d] [zipmem::is "pas une archive"]}
t "{is?} sur la fin"           {type binary {is?}::zipmem::eocd [string range $d end-36 end]}
t "{is?} sur le début"         {type binary {is?}::zipmem::local $d}
set z [zipmem::open $d]
t "membres"                    {zipmem::names $z}
t "commentaire"                {dict get $z comment}
t "une entrée (type)"          {type get [dict get $z entries main.tcl]}
t "ses champs par chemin"      {set x [dict get $z entries main.tcl]; list $x(method) $x(csize) $x(usize)}
t "info"                       {zipmem::info $z données/é.txt}
t "lire (stored)"              {string trim [zipmem::text $z lib/util.tcl]}
t "lire (deflate, utf-8)"      {string range [zipmem::text $z données/é.txt] 0 4}
t "membre absent"              {zipmem::read $z nulle/part}

puts "--- sourcer"
t "zipmem::source app.zip"     {zipmem::source [file join $here app.zip]}
t "zipmem::source -data"       {zipmem::source -data $d}
zipmem::install
t "source app.zip"             {source [file join $here app.zip]}
t "source d'un .tcl ordinaire" {
    set f [file join [file tempdir] ordinaire.tcl]
    set c [open $f w]; puts $c {return "un script ordinaire"}; close $c
    source $f
}

puts "--- d'autres archives"
set tmp [file tempdir]
file mkdir [file join $tmp src lib]
set c [open [file join $tmp src main.tcl] w]
puts $c {source [file join [file dirname [info script]] lib/util.tcl]; return [util::hello]}
close $c
set c [open [file join $tmp src lib util.tcl] w]
puts $c {namespace eval util {proc hello {} {return "bonjour (zipfs mkzip)"}}}
close $c
zipfs mkzip [file join $tmp mk.zip] [file join $tmp src] [file join $tmp src]
t "zipfs mkzip"                {source [file join $tmp mk.zip]}
set c [open [file join $tmp sfx.zip] wb]
puts -nonewline $c "[string repeat x 5000][slurp [file join $here app.zip]]"
close $c
t "derrière 5000 octets"       {set z2 [zipmem::open [slurp [file join $tmp sfx.zip]]]; list [dict get $z2 base] [zipmem::names $z2]}
t "source de cette archive"    {source [file join $tmp sfx.zip]}
set z3 [zipmem::open [slurp [file join $here flux.zip]]]
t "flux (zip64)"               {list [zipmem::names $z3] [string length [zipmem::read $z3 -]]}
t "vide"                       {zipmem::names [zipmem::open "PK\x05\x06[string repeat \x00 18]"]}
file delete -force $tmp
