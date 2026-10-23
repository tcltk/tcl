# niveau2.tcl --
#
# FR : Démonstration du niveau 2 de « type binary » : des tailles, des nombres
#      et des positions LUS DANS LE FICHIER, et des sommes dont la variante
#      est choisie par un champ. Toujours AUCUNE syntaxe de plus : c'est la
#      notation $champ des types paramétrés, dans les arguments du type.
#        1. PE : les en-têtes entiers d'un .exe en UN type, avec at($lfanew) ;
#        2. ELF : la table des sections et des segments, avec at($shoff) ;
#        3. un protocole à messages : la variante dépend du champ type, la
#           taille du champ len.
# EN : Demonstration of level 2 of "type binary": sizes, counts and positions
#      READ FROM THE FILE, and sums whose variant is chosen by a field. Still
#      NO extra syntax: it is the $field notation of parameterized types, in
#      the arguments of the type.
#        1. PE: the whole headers of an .exe in ONE type, with at($lfanew);
#        2. ELF: the section and segment tables, with at($shoff);
#        3. a message protocol: the variant depends on the type field, the
#           size on the len field.
#
#   usage : tclsh niveau2.tcl ?executable?     (default: the running tclsh)
#
# FR : les arguments d'un type, dans l'ordre de ce que dit le fichier :
#      string(utf-8,int(u,16,be))  la taille est un préfixe (Pascal)
#      string(raw,$len)            la taille vient du champ len
#      string(utf-8,$len - 4)      une expression sur les champs déjà lus
#      list($n).T                  n éléments, n étant un champ
#      at($off).T                  la valeur est à la position $off
#      corps($tag)                 la somme corps, variante choisie par le champ tag
# EN : a type's arguments, in the order the file says it:
#      string(utf-8,int(u,16,be))  the size is a prefix (Pascal)
#      string(raw,$len)            the size comes from field len
#      string(utf-8,$len - 4)      an expression on the fields already read
#      list($n).T                  n elements, n being a field
#      at($off).T                  the value is at position $off
#      corps($tag)                 the sum corps, variant chosen by field tag

set racine [file join [file dirname [file normalize [info script]]] .. layout]
source [file join $racine filebinary.tcl]
source [file join $racine binaliases.tcl]

set chemin [expr {$argc > 0 ? [lindex $argv 0] : [info nameofexecutable]}]
set f [open $chemin rb]
set debut [read $f 4]
seek $f 0

proc hex {s} {binary scan $s H* h; return $h}

if {[string range $debut 0 1] eq "MZ"} {
    puts "=== 1. PE : [file tail $chemin] ==="
    type def mz {"MZ"}
    type def pesig {"PE\x00\x00"}
    # FR : une section du tableau (40 octets)  /  EN: a section of the table (40 bytes)
    type def section {
	{string(raw,8)}nom; {u32}vsize; {u32}vaddr; {u32}rawsize; {u32}rawptr;
	{u32}reloc; {u32}lines; {u16}nreloc; {u16}nlines; {u32}flags
    }
    # FR : l'en-tête PE : l'en-tête facultatif a la taille que dit optsize,
    #      puis nsec sections.
    # EN : the PE header: the optional header has the size optsize says,
    #      then nsec sections.
    type def pe {
	{pesig}signature = "PE\x00\x00"; {u16}machine; {u16}nsec; {u32}date;
	{u32}symtab; {u32}nsym; {u16}optsize; {u16}caracteristiques;
	{string(raw,$optsize)}optionnel; {list($nsec).section}sections
    }
    # FR : et le tout est situé là où dit lfanew.
    # EN : and all of it sits where lfanew says.
    type def exe {
	{mz}magic = MZ; {string(raw,58)}stub = [string repeat \x00 58];
	{u32}lfanew; {at($lfanew).pe}nt
    }
    set x [chan binary read $f exe]
    set nt $x(nt)
    puts "en-tête PE à l'offset $x(lfanew) / PE header at offset $x(lfanew)"
    puts [format "machine 0x%x, %d sections, en-tête facultatif de %d octets / optional header of %d bytes" \
	    $nt(machine) $nt(nsec) $nt(optsize) $nt(optsize)]
    puts "tell après lecture / tell after reading: [tell $f] (le curseur est resté après lfanew / the cursor stayed after lfanew)"
    foreach s $nt(sections) {
	puts [format "  %-8s va=0x%06x taille/size=0x%06x fichier/file=0x%06x" \
		[string trimright $s(nom) \x00] $s(vaddr) $s(vsize) $s(rawptr)]
    }
    # FR : ré-encodage : la région de l'en-tête PE est identique octet pour octet
    # EN : re-encoding: the PE header region is identical byte for byte
    seek $f 0
    set orig [::read $f [expr {$x(lfanew) + 24 + $nt(optsize) + 40 * $nt(nsec)}]]
    set neuf [type binary encode exe $x]
    puts "aller-retour de l'en-tête PE / PE header round trip : [expr {[string range $orig $x(lfanew) end] eq [string range $neuf $x(lfanew) end]}]"
} elseif {$debut eq "\x7fELF"} {
    puts "=== 2. ELF : [file tail $chemin] ==="
    type def elfmagic {"\x7fELF"}
    type def shdr {
	{u32}nom; {u32}genre; {u64}flags; {u64}addr; {u64}offset; {u64}taille;
	{u32}lien; {u32}info; {u64}align; {u64}entsize
    }
    type def phdr {
	{u32}genre; {u32}flags; {u64}offset; {u64}vaddr; {u64}paddr;
	{u64}filesz; {u64}memsz; {u64}align
    }
    # FR : l'en-tête ELF64, puis les deux tables situées là où il le dit
    # EN : the ELF64 header, then the two tables located where it says
    type def elf {
	{elfmagic}magic = "\x7fELF"; {u8}classe; {u8}donnee; {u8}version;
	{u8}osabi; {u8}abiversion; {string(raw,7)}bourrage = [string repeat \x00 7];
	{u16}genre; {u16}machine; {u32}version2; {u64}entry;
	{u64}phoff; {u64}shoff; {u32}flags;
	{u16}ehsize; {u16}phentsize; {u16}phnum;
	{u16}shentsize; {u16}shnum; {u16}shstrndx;
	{at($phoff).list($phnum).phdr}segments;
	{at($shoff).list($shnum).shdr}sections
    }
    set e [chan binary read $f elf]
    puts "$e(phnum) segments, $e(shnum) sections"
    # FR : les noms sont des chaînes terminées par NUL dans la section shstrndx
    # EN : names are NUL-terminated strings in section shstrndx
    set strsec [lindex $e(sections) $e(shstrndx)]
    seek $f 0
    set tout [::read $f [file size $chemin]]
    set n 0
    foreach s $e(sections) {
	set nom [type binary decode string(utf-8,z) $tout -offset [expr {$strsec(offset) + $s(nom)}]]
	if {[incr n] <= 8} {
	    puts [format "  %-18s type=%-2d offset=0x%06x taille/size=0x%x" $nom $s(genre) $s(offset) $s(taille)]
	}
    }
    puts "..."
} else {
    puts "ni PE ni ELF / neither PE nor ELF: $chemin"
}
close $f

# ===========================================================================
# 3. Un protocole à messages / A message protocol
# ===========================================================================

puts "\n=== 3. messages ==="

# FR : la variante du corps dépend du champ type ; la taille de l'octet
#      supplémentaire dépend du champ len. Les noms de variantes sont les
#      valeurs de l'étiquette (1, 2, 3).
# EN : the variant of the body depends on field type; the size of the extra
#      data depends on field len. Variant names are the tag values (1, 2, 3).
type def utilisateur {{string(utf-8,int(u,8))}nom; {u8}age}
type def pingmsg {{u32be}numero}
type def corps {{utilisateur}1, {pingmsg}2, {string(raw,$len)}3}
type def message {{u8}type; {u16be}len; {corps($type)}body}

set {utilisateur}u(nom,age) "Léa" 31
set {message}m1(type,len,body) 1 6 {1}$u
set b1 [type binary encode message $m1]
puts "utilisateur : [hex $b1]"
set d [type binary decode message $b1]
puts "  décodé / decoded: [type get $d(body)] nom=$d(body.1.nom) age=$d(body.1.age)"

set {pingmsg}pg {numero}4242
set {message}m2(type,len,body) 2 4 {2}$pg
set b2 [type binary encode message $m2]
puts "ping        : [hex $b2]"
set d2 [type binary decode message $b2]
puts "  décodé / decoded: [type get $d2(body)] numero=$d2(body.2.numero)"

set {message}m3(type,len,body) 3 5 {3}hello
set b3 [type binary encode message $m3]
puts "données     : [hex $b3]"
puts "  décodé / decoded: [type binary decode message $b3]"

# FR : l'étiquette et la variante doivent concorder / tag and variant must agree
#      (refusé dès la construction / refused as soon as it is built)
if {[catch {set {message}m4(type,len,body) 2 6 {1}$u} msg]} {puts "refus / refused : $msg"}
if {[catch {type binary decode message "\x09\x00\x00"} msg]} {puts "refus / refused : $msg"}
