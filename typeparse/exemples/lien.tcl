# lien.tcl --
#
# FR : Le type lien *(origine,repr).T : un entier du fichier qui désigne une autre
#      valeur. Même exemples que niveau2.tcl (PE et ELF), mais SANS calculer les
#      offsets à la main : à la lecture, le lien est suivi ; à l'écriture,
#      `type binary` place la cible et reporte l'offset.
#        *(//,u32le).pe      offset depuis le début des données
#        *(./,u16le).T       depuis le début de la structure englobante
#        *(../,u16le).T      depuis la structure au-dessus
#        *(.,i32le).T        depuis le champ lui-même
#      ptr32(T), ptr64(T), rel32(T), sub32(T) (binaliases.tcl) sont des noms courts.
#      Une table absente (offset 0) se dit opt(T) : valeur some ou none.
# EN : The link type *(origin,repr).T: an integer in the file that designates
#      another value. Same examples as niveau2.tcl (PE and ELF), but WITHOUT
#      computing the offsets by hand: when reading, the link is followed; when
#      writing, `type binary` places the target and patches the offset.
#
#   usage : tclsh lien.tcl ?executable?     (default: the running tclsh)

set racine [file join [file dirname [file normalize [info script]]] .. layout]
source [file join $racine filebinary.tcl]
source [file join $racine binaliases.tcl]

set chemin [expr {$argc > 0 ? [lindex $argv 0] : [info nameofexecutable]}]
set f [open $chemin rb]
set debut [read $f 4]
seek $f 0

if {[string range $debut 0 1] eq "MZ"} {
    puts "=== PE : [file tail $chemin] ==="
    type def mz {"MZ"}
    type def pesig {"PE\x00\x00"}
    type def section {
	{string(raw,8)}nom; {u32}vsize; {u32}vaddr; {u32}rawsize; {u32}rawptr;
	{u32}reloc; {u32}lines; {u16}nreloc; {u16}nlines; {u32}flags
    }
    type def pe {
	{pesig}signature = "PE\x00\x00"; {u16}machine; {u16}nsec; {u32}date;
	{u32}symtab; {u32}nsym; {u16}optsize; {u16}caracteristiques;
	{string(raw,$optsize)}optionnel; {list($nsec).section}sections
    }
    # FR : lfanew et at($lfanew) deviennent UN champ : l'en-tête PE est là où
    #      l'entier dit, et c'est un pe.
    # EN : lfanew and at($lfanew) become ONE field: the PE header is where the
    #      integer says, and it is a pe.
    type def exe {
	{mz}magic = MZ; {string(raw,58)}stub = [string repeat \x00 58];
	{ptr32(pe)}nt
    }
    set x [chan binary read $f exe]
    set nt $x(nt)
    puts [format "machine 0x%x, %d sections" $nt(machine) $nt(nsec)]
    foreach s $nt(sections) {
	puts [format "  %-8s va=0x%06x file=0x%06x" [string trimright $s(nom) \x00] $s(vaddr) $s(rawptr)]
    }
    # FR : réécriture : le pe est placé à la suite (offset 64), le lien est recalculé
    # EN : rewriting: the pe is placed right after (offset 64), the link is recomputed
    set neuf [type binary encode exe $x]
    set y [type binary decode exe $neuf]
    puts "pe réécrit à l'offset [expr {[binary scan $neuf @60i lf] ? $lf : -1}] ; relu identique : [expr {$y eq $x}]"
} elseif {$debut eq "\x7fELF"} {
    puts "=== ELF : [file tail $chemin] ==="
    type def elfmagic {"\x7fELF"}
    type def shdr {
	{u32}nom; {u32}genre; {u64}flags; {u64}addr; {u64}offset; {u64}taille;
	{u32}lien; {u32}info; {u64}align; {u64}entsize
    }
    type def phdr {
	{u32}genre; {u32}flags; {u64}offset; {u64}vaddr; {u64}paddr;
	{u64}filesz; {u64}memsz; {u64}align
    }
    # FR : e_phoff et e_shoff valent 0 quand la table n'existe pas : opt(...).
    #      Les nombres (phnum, shnum) SUIVENT les adresses dans l'en-tête ELF ;
    #      la cible d'un lien est lue après tous les champs, donc elle peut les citer.
    # EN : e_phoff and e_shoff are 0 when the table does not exist: opt(...).
    #      The counts (phnum, shnum) FOLLOW the addresses in the ELF header; a
    #      link's target is read after all the fields, so it may cite them.
    type def elf {
	{elfmagic}magic = "\x7fELF"; {u8}classe; {u8}donnee; {u8}version;
	{u8}osabi; {u8}abiversion; {string(raw,7)}bourrage = [string repeat \x00 7];
	{u16}genre; {u16}machine; {u32}version2; {u64}entry;
	{ptr64(opt(list($phnum).phdr))}segments;
	{ptr64(opt(list($shnum).shdr))}sections;
	{u32}flags; {u16}ehsize; {u16}phentsize; {u16}phnum;
	{u16}shentsize; {u16}shnum; {u16}shstrndx
    }
    set e [chan binary read $f elf]
    puts "segments : [type get $e(segments)]"
    puts "sections : [type get $e(sections)]"
    if {$e(shnum) > 0} {
	set strsec [lindex $e(sections.some) $e(shstrndx)]
	seek $f 0
	set tout [::read $f [file size $chemin]]
	set n 0
	foreach s $e(sections.some) {
	    set nom [type binary decode string(utf-8,z) $tout -offset [expr {$strsec(offset) + $s(nom)}]]
	    if {[incr n] <= 6} {
		puts [format "  %-16s type=%-2d offset=0x%06x" $nom $s(genre) $s(offset)]
	    }
	}
    }
} else {
    puts "ni PE ni ELF : $chemin"
}
close $f
