# executable_elf.tcl --
#
# FR : Un type « fichier exécutable » pour le format ELF (Linux, BSD...).
#      On lit le vrai fichier avec binary scan, on remplit des types, puis
#      on analyse avec des masques de bits.
# EN : An "executable file" type for the ELF format (Linux, BSD...).
#      We read the real file with binary scan, fill typed values, then
#      analyse them with bit masks.
#
#   usage : tclsh executable_elf.tcl ?file?    (default: the running tclsh)
#
# FR : Ce que l'exemple illustre :
#   - des produits (segment, section) et un type paramétré elf(n) ;
#   - bitarray pour les drapeaux : permissions d'un segment (bit 0 = X,
#     bit 1 = W, bit 2 = R, comme p_flags : convention lsb0) ;
#   - la structure d'hypergraphe : les sections sont les sommets, les
#     segments sont les hyperarêtes (une section peut être dans plusieurs
#     segments). La « couverture » est une liste de masques, comme edges
#     dans hg(n) (voir hypergraphe.tcl) ;
#   - les contrôles entre champs (W^X, point d'entrée) restent des
#     procédures : le type vérifie les formes et les plages, pas les liens
#     logiques entre champs.
# EN: What the example shows:
#   - products (segment, section) and a parameterized type elf(n);
#   - bitarray for flags: segment permissions (bit 0 = X, bit 1 = W,
#     bit 2 = R, as in p_flags: lsb0 convention);
#   - the hypergraph structure: sections are the vertices, segments are
#     the hyperedges (a section can belong to several segments). The
#     "coverage" is a list of masks, like edges in hg(n) (see
#     hypergraphe.tcl);
#   - cross-field checks (W^X, entry point) stay in procedures: the type
#     checks shapes and ranges, not logical relations between fields.

# ---------------------------------------------------------------------------
# FR : Les types   /   EN: The types
# ---------------------------------------------------------------------------

type def segment {
    {string}genre;                # LOAD, DYNAMIC, NOTE... (segment kind)
    {bitarray(3)}perms;           # bit0 = X, bit1 = W, bit2 = R
    {int}offset; {int}vaddr; {int}filesz; {int}memsz
}
type def section {
    {string}nom; {string}genre;   # name, kind
    {bitarray(64)}flags;          # bit0 = WRITE, bit1 = ALLOC, bit2 = EXEC
    {int}addr; {int}offset; {int}taille
}
# FR : n = nombre de sections = nombre de sommets de l'hypergraphe.
# EN : n = number of sections = number of vertices of the hypergraph.
type def {elf({int}n)} {
    {int}bits; {string}endian; {string}machine; {int}entry;
    {list($n).section}sections;
    {list.segment}segments;
    {list.bitarray($n)}couverture   ;# FR: un masque par segment / EN: one mask per segment
}

# ---------------------------------------------------------------------------
# FR : Lecture binaire   /   EN: Binary reading
# ---------------------------------------------------------------------------

# FR : lire data off endian w1 w2... : lit des entiers non signés de largeur
#      1, 2, 4 ou 8 octets à partir de off, et rend la liste des valeurs.
# EN : lire data off endian w1 w2... : reads unsigned integers of width
#      1, 2, 4 or 8 bytes starting at off, and returns the list of values.
proc lire {data off endian args} {
    set big [expr {$endian eq "big"}]
    set fmt "@$off"
    set names {}
    set i 0
    foreach w $args {
	append fmt [dict get [expr {$big
		? {1 cu 2 Su 4 Iu 8 Wu}
		: {1 cu 2 su 4 iu 8 wu}}] $w]
	lappend names v$i
	incr i
    }
    binary scan $data $fmt {*}$names
    lmap v $names {set $v}
}

# FR : lecture d'une chaîne terminée par un zéro (table de noms)
# EN : reads a NUL-terminated string (name table)
proc chaine {data off} {
    set fin [string first \x00 $data $off]
    encoding convertfrom ascii [string range $data $off [expr {$fin - 1}]]
}

# FR : tables de noms / EN: name tables
set TYPES_SEG {0 NULL 1 LOAD 2 DYNAMIC 3 INTERP 4 NOTE 5 SHLIB 6 PHDR 7 TLS
    1685382480 GNU_EH_FRAME 1685382481 GNU_STACK 1685382482 GNU_RELRO
    1685382483 GNU_PROPERTY}
set TYPES_SEC {0 NULL 1 PROGBITS 2 SYMTAB 3 STRTAB 4 RELA 5 HASH 6 DYNAMIC
    7 NOTE 8 NOBITS 9 REL 11 DYNSYM 14 INIT_ARRAY 15 FINI_ARRAY
    1879048182 GNU_HASH 1879048190 VERNEED 1879048191 VERSYM}
set MACHINES {3 x86 40 ARM 62 x86-64 183 AArch64 243 RISC-V}

proc nom_de {table code} {
    expr {[dict exists $table $code] ? [dict get $table $code] : "0x[format %x $code]"}
}

# ---------------------------------------------------------------------------
# FR : Lecture d'un fichier ELF   /   EN: Reading an ELF file
# ---------------------------------------------------------------------------

# FR : rend un nom de variable-type : le résultat est une valeur elf(n).
# EN : returns a value of type elf(n), as a list {n description}.
proc lire_elf {chemin} {
    global TYPES_SEG TYPES_SEC MACHINES
    set f [open $chemin rb]
    set data [read $f]
    close $f

    binary scan $data a4cc magic classe donnee
    if {$magic ne "\x7fELF"} {error "$chemin: not an ELF file"}
    set bits [expr {$classe == 2 ? 64 : 32}]
    set endian [expr {$donnee == 2 ? "big" : "little"}]
    set a [expr {$bits == 64 ? 8 : 4}]      ;# FR: largeur d'une adresse / EN: address width

    # FR : en-tête du fichier / EN: file header
    lassign [lire $data 16 $endian 2 2] genre machine
    lassign [lire $data [expr {24}] $endian $a $a $a] entry phoff shoff
    set apres [expr {24 + 3 * $a}]
    lassign [lire $data $apres $endian 4 2 2 2 2 2 2] \
	    - ehsize phentsize phnum shentsize shnum shstrndx

    # FR : en-têtes de section / EN: section headers
    set sec {}
    for {set i 0} {$i < $shnum} {incr i} {
	set o [expr {$shoff + $i * $shentsize}]
	if {$bits == 64} {
	    lassign [lire $data $o $endian 4 4 8 8 8 8] nm gen fl addr off tai
	} else {
	    lassign [lire $data $o $endian 4 4 4 4 4 4] nm gen fl addr off tai
	}
	lappend sec [list $nm $gen $fl $addr $off $tai]
    }
    # FR : la table des noms est elle-même une section / EN: the name table is itself a section
    set tabnoms [lindex $sec $shstrndx 4]
    set sections {}
    foreach s $sec {
	lassign $s nm gen fl addr off tai
	lappend sections [list [chaine $data [expr {$tabnoms + $nm}]] \
		[nom_de $TYPES_SEC $gen] $fl $addr $off $tai]
    }

    # FR : en-têtes de programme (segments) / EN: program headers (segments)
    set segments {}
    for {set i 0} {$i < $phnum} {incr i} {
	set o [expr {$phoff + $i * $phentsize}]
	if {$bits == 64} {
	    lassign [lire $data $o $endian 4 4 8 8 8 8 8] gen fl off va pa fs ms
	} else {
	    lassign [lire $data $o $endian 4 4 4 4 4 4 4] gen off va pa fs ms fl
	}
	lappend segments [list [nom_de $TYPES_SEG $gen] [expr {$fl & 7}] $off $va $fs $ms]
    }

    set n [llength $sections]
    set couverture [remplir_couverture $sections $segments]
    return [list $n [list $bits $endian [nom_de $MACHINES $machine] $entry \
	    $sections $segments $couverture]]
}

# FR : couverture = hypergraphe section/segment. Le bit i du masque d'un
#      segment vaut 1 si la section i y est contenue (comme readelf :
#      section allouée, de taille non nulle, entièrement dans le segment).
# EN : coverage = section/segment hypergraph. Bit i of a segment's mask is
#      1 if section i lies in it (like readelf: allocated section, non-
#      empty, fully inside the segment).
proc remplir_couverture {sections segments} {
    set masques {}
    foreach seg $segments {
	lassign $seg gen perms off va fs ms
	set m 0
	set i 0
	foreach s $sections {
	    lassign $s nom sgen fl addr soff tai
	    if {($fl & 2) && $tai > 0 && $addr >= $va && $addr + $tai <= $va + $ms} {
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

set chemin [expr {$argc ? [lindex $argv 0] : [info nameofexecutable]}]
lassign [lire_elf $chemin] n valeurs

# FR : le type est choisi à l'exécution : elf(31), elf(28)...
# EN : the type is chosen at run time: elf(31), elf(28)...
# FR : le nom du type contient n, donc on construit la commande en texte.
# EN : the type name contains n, so the command is built as text.
set cmd "set {elf($n)}E"
foreach champ {bits endian machine entry sections segments couverture} v $valeurs {
    append cmd " " [list $champ $v]
}
eval $cmd

puts "type : [type get $E]"
puts "ELF$E(bits) $E(endian), machine $E(machine), entry [format 0x%x $E(entry)]"
puts "[llength $E(sections)] sections, [llength $E(segments)] segments"

# FR : noms des sections dont le bit est à 1 dans un masque
# EN : names of the sections whose bit is 1 in a mask
proc noms_sections {E masque} {
    set out {}
    for {set i 0} {$i < [llength $E(sections)]} {incr i} {
	if {($masque >> $i) & 1} {lappend out $E(sections.$i.nom)}
    }
    return $out
}

# FR : permissions lisibles : lecture bit par bit de bitarray(3)
# EN : readable permissions: bit-by-bit reading of a bitarray(3)
proc rwx {p} {
    return "[expr {$p(2) ? "R" : "-"}][expr {$p(1) ? "W" : "-"}][expr {$p(0) ? "X" : "-"}]"
}

puts "\n--- FR: segments -> sections / EN: segments -> sections ---"
for {set i 0} {$i < [llength $E(segments)]} {incr i} {
    set seg $E(segments.$i)
    puts [format "%2d %-13s %s  vaddr %#8x  %2d sections: %s" $i \
	    $E(segments.$i.genre) [rwx $E(segments.$i.perms)] $E(segments.$i.vaddr) \
	    [expr {popcount($E(couverture.$i))}] \
	    [join [noms_sections $E $E(couverture.$i)] " "]]
}

# FR : sections hors de tout segment (le complément de l'union)
# EN : sections outside any segment (the complement of the union)
set n [llength $E(sections)]
set union 0
for {set i 0} {$i < [llength $E(couverture)]} {incr i} {
    set union [expr {$union | $E(couverture.$i)}]
}
set hors [expr {$union ^ ((1 << $n) - 1)}]
puts "\nFR: sections non chargées en mémoire / EN: sections not loaded in memory ([expr {popcount($hors)}]):"
puts "  [join [noms_sections $E $hors] " "]"

# FR : intersection de deux hyperarêtes : sections communes à deux segments
# EN : intersection of two hyperedges: sections shared by two segments
puts "\nFR: sections partagées par deux segments / EN: sections shared by two segments"
for {set i 0} {$i < [llength $E(couverture)]} {incr i} {
    for {set j [expr {$i + 1}]} {$j < [llength $E(couverture)]} {incr j} {
	set c [expr {$E(couverture.$i) & $E(couverture.$j)}]
	if {$c} {
	    puts "  $E(segments.$i.genre)#$i & $E(segments.$j.genre)#$j : [join [noms_sections $E $c] " "]"
	}
    }
}

# FR : degré d'une section = nombre de segments qui la contiennent
# EN : degree of a section = number of segments containing it
puts "\nFR: sections dans au moins 3 segments / EN: sections in at least 3 segments"
for {set k 0} {$k < $n} {incr k} {
    set deg 0
    for {set i 0} {$i < [llength $E(couverture)]} {incr i} {
	incr deg $E(couverture.$i.$k)
    }
    if {$deg >= 3} {puts "  $E(sections.$k.nom) : $deg"}
}

# ---------------------------------------------------------------------------
# FR : contrôles entre champs (hors du type) / EN: cross-field checks (outside the type)
# ---------------------------------------------------------------------------

puts "\n--- FR: contrôles / EN: checks ---"

# W^X : aucun segment à la fois inscriptible et exécutable
# W^X: no segment both writable and executable
set wx {}
for {set i 0} {$i < [llength $E(segments)]} {incr i} {
    if {($E(segments.$i.perms) & 3) == 3} {lappend wx $i}
}
puts "W^X : [expr {[llength $wx] ? "VIOLATION segments $wx" : "ok"}]"

# FR : le point d'entrée doit tomber dans un segment LOAD exécutable
# EN : the entry point must fall in an executable LOAD segment
set trouve ""
for {set i 0} {$i < [llength $E(segments)]} {incr i} {
    set va $E(segments.$i.vaddr)
    if {$E(segments.$i.genre) eq "LOAD" && $E(entry) >= $va
	    && $E(entry) < $va + $E(segments.$i.memsz)} {
	set trouve $i
    }
}
if {$trouve ne "" && $E(segments.$trouve.perms.0)} {
    puts "entry point : ok (segment $trouve, [rwx $E(segments.$trouve.perms)])"
} else {
    puts "entry point : PROBLEM"
}

# FR : garanties du typage / EN: guarantees of the typing
puts "\n--- FR: refus du typage / EN: what typing refuses ---"
puts "perms=9 : [catch {set {segment}P genre LOAD perms 9 offset 0 vaddr 0 filesz 0 memsz 0} msg] $msg"
puts "bad count : [catch {set {elf(3)}Z bits 64 endian little machine x entry 0 sections {} segments {} couverture {}} msg] $msg"
}
