# executable_pe.tcl --
#
# FR : Un type « fichier exécutable » pour le format PE (Windows : .exe, .dll).
#      Même démarche que executable_elf.tcl : lecture avec binary scan,
#      types produit, bitarray pour les drapeaux, hypergraphe pour les liens.
# EN : An "executable file" type for the PE format (Windows: .exe, .dll).
#      Same approach as executable_elf.tcl: reading with binary scan,
#      product types, bitarray for flags, hypergraph for the relations.
#
#   usage : tclsh executable_pe.tcl file.exe
#
# FR : Différences avec ELF :
#   - les drapeaux sont plus nombreux : 16 bits (caractéristiques du
#     fichier, du DLL) et 32 bits (caractéristiques de section) ;
#   - l'hypergraphe est différent : les sommets sont les sections, les
#     hyperarêtes sont les 16 « répertoires de données » (imports, exports,
#     ressources, relocations...). Un répertoire peut couvrir plusieurs
#     sections, et une section en contient plusieurs ;
#   - PE32 (32 bits) et PE32+ (64 bits) ont des en-têtes de tailles
#     différentes, d'où un seul type pe(n) et deux chemins de lecture.
# EN: Differences from ELF:
#   - more flags: 16 bits (file and DLL characteristics) and 32 bits
#     (section characteristics);
#   - a different hypergraph: vertices are sections, hyperedges are the 16
#     "data directories" (imports, exports, resources, relocations...).
#     A directory can span several sections, and a section holds several;
#   - PE32 (32-bit) and PE32+ (64-bit) have headers of different sizes, hence
#     one type pe(n) and two reading paths.

# ---------------------------------------------------------------------------
# FR : Les types   /   EN: The types
# ---------------------------------------------------------------------------

type def sectionpe {
    {string}nom;
    {bitarray(32)}flags;      # bit5 CODE, bit6 INIT_DATA, bit7 UNINIT_DATA,
                              # bit29 EXECUTE, bit30 READ, bit31 WRITE
    {int}vaddr; {int}vsize; {int}rawptr; {int}rawsize
}
type def repertoire {{string}nom; {int}rva; {int}taille}
# FR : n = nombre de sections (les sommets) / EN: n = number of sections (the vertices)
type def {pe({int}n)} {
    {int}bits; {string}machine;
    {bitarray(16)}fichier;    # bit1 EXECUTABLE_IMAGE, bit5 LARGE_ADDRESS_AWARE, bit13 DLL
    {bitarray(16)}dllcar;     # bit5 HIGH_ENTROPY_VA, bit6 DYNAMIC_BASE (ASLR),
                              # bit8 NX_COMPAT (DEP), bit14 GUARD_CF
    {string}subsystem; {int}entry; {int}imagebase;
    {list($n).sectionpe}sections;
    {list.repertoire}repertoires;
    {list.bitarray($n)}couverture   ;# FR: un masque par répertoire / EN: one mask per directory
}

# ---------------------------------------------------------------------------
# FR : Lecture binaire   /   EN: Binary reading
# ---------------------------------------------------------------------------

# FR : lirele data off w1 w2... : entiers non signés little-endian (PE l'est toujours)
# EN : lirele data off w1 w2... : unsigned little-endian integers (PE always is)
proc lirele {data off args} {
    set fmt "@$off"
    set names {}
    set i 0
    foreach w $args {
	append fmt [dict get {1 cu 2 su 4 iu 8 wu} $w]
	lappend names v$i
	incr i
    }
    binary scan $data $fmt {*}$names
    lmap v $names {set $v}
}

set MACHINES_PE {332 x86 34404 x86-64 452 ARM 43620 ARM64 512 IA-64}
set SUBSYSTEMS {1 native 2 GUI 3 console 9 WinCE 10 EFI-app 14 Xbox}
set REPERTOIRES {Export Import Resource Exception Security BaseReloc Debug
    Architecture GlobalPtr TLS LoadConfig BoundImport IAT DelayImport CLR
    Reserved}

proc nom_de {table code} {
    expr {[dict exists $table $code] ? [dict get $table $code] : "0x[format %x $code]"}
}

# ---------------------------------------------------------------------------
# FR : Lecture d'un fichier PE   /   EN: Reading a PE file
# ---------------------------------------------------------------------------

proc lire_pe {chemin} {
    global MACHINES_PE SUBSYSTEMS REPERTOIRES
    set f [open $chemin rb]
    set data [read $f]
    close $f

    # FR : en-tête DOS puis signature « PE\0\0 » / EN: DOS header then "PE\0\0" signature
    if {[string range $data 0 1] ne "MZ"} {error "$chemin: not a PE file (no MZ)"}
    set pe [lindex [lirele $data 60 4] 0]
    if {[string range $data $pe [expr {$pe + 3}]] ne "PE\x00\x00"} {
	error "$chemin: no PE signature"
    }

    # FR : en-tête COFF / EN: COFF header
    lassign [lirele $data [expr {$pe + 4}] 2 2 4 4 4 2 2] \
	    machine nsec - - - taille_opt caract
    set opt [expr {$pe + 24}]
    set magic [lindex [lirele $data $opt 2] 0]
    switch $magic {
	267 {set bits 32}
	523 {set bits 64}
	default {error "$chemin: unknown optional header magic $magic"}
    }
    # FR : PE32 et PE32+ ne placent pas les champs aux mêmes endroits
    # EN : PE32 and PE32+ do not place the fields at the same offsets
    set entry [lindex [lirele $data [expr {$opt + 16}] 4] 0]
    if {$bits == 64} {
	set base [lindex [lirele $data [expr {$opt + 24}] 8] 0]
	set dirs [expr {$opt + 112}]
	set nd [lindex [lirele $data [expr {$opt + 108}] 4] 0]
    } else {
	set base [lindex [lirele $data [expr {$opt + 28}] 4] 0]
	set dirs [expr {$opt + 96}]
	set nd [lindex [lirele $data [expr {$opt + 92}] 4] 0]
    }
    lassign [lirele $data [expr {$opt + 68}] 2 2] sub dllcar

    # FR : répertoires de données / EN: data directories
    set repertoires {}
    for {set i 0} {$i < $nd && $i < 16} {incr i} {
	lassign [lirele $data [expr {$dirs + 8 * $i}] 4 4] rva tai
	lappend repertoires [list [lindex $REPERTOIRES $i] $rva $tai]
    }

    # FR : table des sections, juste après l'en-tête optionnel
    # EN : section table, right after the optional header
    set sections {}
    set st [expr {$opt + $taille_opt}]
    for {set i 0} {$i < $nsec} {incr i} {
	set o [expr {$st + 40 * $i}]
	binary scan $data "@${o}a8" nom
	set nom [string trimright $nom "\x00"]
	lassign [lirele $data [expr {$o + 8}] 4 4 4 4] vsize va rsize rptr
	set fl [lindex [lirele $data [expr {$o + 36}] 4] 0]
	lappend sections [list $nom $fl $va $vsize $rptr $rsize]
    }
    set couv [remplir_couverture_pe $sections $repertoires]
    return [list $nsec [list $bits [nom_de $MACHINES_PE $machine] $caract $dllcar \
	    [nom_de $SUBSYSTEMS $sub] $entry $base $sections $repertoires $couv]]
}

# FR : le répertoire r couvre la section s s'ils se recouvrent en mémoire
#      (RVA) : hyperarête = ensemble des sections touchées.
# EN : directory r covers section s if they overlap in memory (RVA):
#      hyperedge = set of touched sections.
proc remplir_couverture_pe {sections repertoires} {
    set masques {}
    foreach r $repertoires {
	lassign $r nom rva tai
	set m 0
	set i 0
	foreach s $sections {
	    lassign $s snom fl va vsize
	    if {$tai > 0 && $rva < $va + $vsize && $rva + $tai > $va} {
		set m [expr {$m | (1 << $i)}]
	    }
	    incr i
	}
	lappend masques $m
    }
    return $masques
}

# FR : le programme principal ne s'exécute que si ce fichier est lancé
#      directement (pas quand il est chargé par source depuis executable.tcl).
# EN : the main program only runs when this file is launched directly
#      (not when sourced from executable.tcl).
if {[file normalize [info script]] eq [file normalize $argv0]} {
# ---------------------------------------------------------------------------
# FR : Programme principal   /   EN: Main program
# ---------------------------------------------------------------------------

if {$argc < 1} {puts stderr "usage: tclsh executable_pe.tcl file.exe"; exit 2}
lassign [lire_pe [lindex $argv 0]] n valeurs

# FR : le nom du type contient n, donc on construit la commande en texte.
# EN : the type name contains n, so the command is built as text.
set cmd "set {pe($n)}E"
foreach champ {bits machine fichier dllcar subsystem entry imagebase
	sections repertoires couverture} v $valeurs {
    append cmd " " [list $champ $v]
}
eval $cmd

puts "type : [type get $E]"
puts "PE$E(bits) $E(machine), subsystem $E(subsystem), entry RVA [format 0x%x $E(entry)],\
	image base [format 0x%x $E(imagebase)]"
puts "[expr {$E(fichier.13) ? "DLL" : "EXE"}]\
	[expr {$E(fichier.5) ? "(large address aware)" : ""}]"

# FR : mitigations de sécurité, lues bit par bit dans dllcar
# EN : security mitigations, read bit by bit in dllcar
puts "\n--- FR: mitigations / EN: mitigations ---"
foreach {bit nom} {5 HIGH_ENTROPY_VA 6 "DYNAMIC_BASE (ASLR)" 8 "NX_COMPAT (DEP)" 14 GUARD_CF} {
    puts [format "  %-22s %s" $nom [expr {$E(dllcar.$bit) ? "yes" : "no"}]]
}

# FR : droits d'une section : les 3 bits hauts du bitarray(32)
# EN : section rights: the 3 high bits of the bitarray(32)
proc rwx {fl} {
    return "[expr {$fl(30) ? "R" : "-"}][expr {$fl(31) ? "W" : "-"}][expr {$fl(29) ? "X" : "-"}]"
}
proc noms_sections {E masque} {
    set out {}
    for {set i 0} {$i < [llength $E(sections)]} {incr i} {
	if {($masque >> $i) & 1} {lappend out $E(sections.$i.nom)}
    }
    return $out
}

puts "\n--- FR: sections / EN: sections ---"
for {set i 0} {$i < [llength $E(sections)]} {incr i} {
    puts [format "  %-8s %s  rva %#8x  size %#8x" $E(sections.$i.nom) \
	    [rwx $E(sections.$i.flags)] $E(sections.$i.vaddr) $E(sections.$i.vsize)]
}

puts "\n--- FR: répertoires -> sections / EN: directories -> sections ---"
for {set i 0} {$i < [llength $E(repertoires)]} {incr i} {
    if {$E(repertoires.$i.taille) > 0} {
	puts [format "  %-10s rva %#8x size %#6x : %s" $E(repertoires.$i.nom) \
		$E(repertoires.$i.rva) $E(repertoires.$i.taille) \
		[join [noms_sections $E $E(couverture.$i)] " "]]
    }
}

# FR : sections qui contiennent plus d'un répertoire (degré >= 2)
# EN : sections holding more than one directory (degree >= 2)
puts "\n--- FR: sections partagées / EN: shared sections ---"
for {set k 0} {$k < [llength $E(sections)]} {incr k} {
    set qui {}
    for {set i 0} {$i < [llength $E(couverture)]} {incr i} {
	if {$E(couverture.$i.$k)} {lappend qui $E(repertoires.$i.nom)}
    }
    if {[llength $qui] >= 2} {
	puts "  $E(sections.$k.nom) : [join $qui {, }]"
    }
}

# ---------------------------------------------------------------------------
# FR : contrôles entre champs / EN: cross-field checks
# ---------------------------------------------------------------------------

puts "\n--- FR: contrôles / EN: checks ---"

# FR : W^X sur les sections / EN: W^X on sections
set wx {}
for {set i 0} {$i < [llength $E(sections)]} {incr i} {
    # FR : bits 29 et 31 ensemble : EXECUTE et WRITE
    # EN : bits 29 and 31 together: EXECUTE and WRITE
    if {$E(sections.$i.flags.29) && $E(sections.$i.flags.31)} {
	lappend wx $E(sections.$i.nom)
    }
}
puts "W^X : [expr {[llength $wx] ? "VIOLATION $wx" : "ok"}]"

# FR : le point d'entrée tombe-t-il dans une section exécutable ?
# EN : does the entry point fall in an executable section?
set ok 0
for {set i 0} {$i < [llength $E(sections)]} {incr i} {
    set va $E(sections.$i.vaddr)
    if {$E(entry) >= $va && $E(entry) < $va + $E(sections.$i.vsize)} {
	set ok $E(sections.$i.flags.29)
	puts "entry point : in $E(sections.$i.nom) ([rwx $E(sections.$i.flags)])"
    }
}
if {!$ok && $E(entry) != 0} {puts "entry point : PROBLEM"}

puts "\n--- FR: refus du typage / EN: what typing refuses ---"
# FR : attention, ne pas réutiliser une variable typée pour un message
#      (ici m1, m2 sont des variables fraîches).
# EN : careful, do not reuse a typed variable for a message (m1, m2 are fresh).
catch {set {repertoire}R {nom}Import {rva}abc {taille}0} m1
puts "rva not an integer : $m1"
catch {set {sectionpe}S {nom}.text {flags}0x100000000 {vaddr}0 {vsize}0 {rawptr}0 {rawsize}0} m2
puts "flags too wide     : $m2"
}
