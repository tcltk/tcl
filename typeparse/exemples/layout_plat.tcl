# layout_plat.tcl --
#
# FR : Démonstration du niveau 1 de « file binary » (layout/filebinary.tcl,
#      pur Tcl, avec les types binaires du noyau). AUCUNE syntaxe de plus : la
#      représentation binaire est portée par le TYPE du champ :
#      {int(u,16,be)}src, comme bitarray(16) porte déjà sa largeur.
#        1. un en-tête TCP (big-endian, bitarray msb0 : doff + drapeaux) ;
#        2. l'en-tête d'un vrai exécutable (ELF ou PE), lu DIRECTEMENT depuis
#           un canal binaire, puis ré-encodé octet pour octet ;
#        3. les refus : données tronquées, constante fausse, plage.
# EN : Demonstration of level 1 of "file binary" (layout/filebinary.tcl,
#      pure Tcl, with the core's binary types). NO extra syntax: the binary
#      representation is carried by the field's TYPE: {int(u,16,be)}src, just
#      as bitarray(16) already carries its width.
#        1. a TCP header (big-endian, msb0 bitarray: data offset + flags);
#        2. the header of a real executable (ELF or PE), read DIRECTLY from a
#           binary channel, then re-encoded byte for byte;
#        3. the refusals: truncated data, wrong constant, range.
#
#   usage : tclsh layout_plat.tcl ?executable?     (default: the running tclsh)

set racine [file join [file dirname [file normalize [info script]]] .. layout]
source [file join $racine filebinary.tcl]
# FR : alias courts (u8 u16 i32 f64 u16be ...) ; EN: short aliases
source [file join $racine binaliases.tcl]

# ===========================================================================
# 1. TCP
# ===========================================================================

puts "=== 1. TCP ==="

# FR : le type dit à la fois CE QUE C'EST (plages) et COMMENT C'EST ÉCRIT.
#      int(u,16,be) : entier non signé de 16 bits, big-endian ; be. et le.
#      qualifient la représentation sans changer les valeurs.
# EN : the type says both WHAT IT IS (ranges) and HOW IT IS WRITTEN.
#      int(u,16,be): 16-bit unsigned integer, big-endian; be. and le. qualify
#      the representation without changing the values.
# FR : alias structurels (layout/binaliases.tcl) : u16be EST int(u,16,be)
# EN : structural aliases (layout/binaliases.tcl): u16be IS int(u,16,be)
type def tcp {
    {u16be}src; {u16be}dst; {u32be}seq; {u32be}ack;
    {bitarray(16,msb0,be)}flags;    # bits 0-3 : data offset ; 11 ACK, 14 SYN, 15 FIN
    {u16be}window; {u16be}checksum; {u16be}urgent
}
puts "taille / size : [file binary sizeof tcp] octets"

# FR : un SYN de 40000 vers 80, data offset 5 (20 octets)
# EN : a SYN from 40000 to 80, data offset 5 (20 bytes)
set octets [binary decode hex "9c40 0050 000003e8 00000000 5002 ffff 0000 0000"]
set t [file binary decode tcp $octets]
puts "src=$t(src) dst=$t(dst) seq=$t(seq) window=$t(window)"
puts "data offset = [expr {$t(flags.0:3) * 4}] octets ; SYN=$t(flags.14) ACK=$t(flags.11) FIN=$t(flags.15)"
puts "aller-retour / round trip : [expr {[file binary encode tcp $t] eq $octets}]"

# FR : on construit un SYN+ACK sans octets, puis on l'encode
# EN : build a SYN+ACK without bytes, then encode it
set {tcp}r src 80 dst 40000 seq 7000 ack 1001 flags 0b0101000000010010 \
	window 65535 checksum 0 urgent 0
puts "SYN+ACK : [binary encode hex [file binary encode tcp $r]]"

# ===========================================================================
# 2. En-tête d'un exécutable, lu depuis un canal / header of an executable, read from a channel
# ===========================================================================

set chemin [expr {$argc ? [lindex $argv 0] : [info nameofexecutable]}]
set f [open $chemin rb]
set debut [read $f 4]
seek $f 0

if {$debut eq "\x7fELF"} {
    puts "\n=== 2. ELF : [file tail $chemin] ==="
    # FR : une CONSTANTE est un type : une somme d'un seul littéral
    # EN : a CONSTANT is a type: a sum of a single literal
    type def elfmagic {"\x7fELF"}
    type def elfhdr {
	{elfmagic}magic = "\x7fELF";
	{u8}classe; {u8}donnee; {u8}version; {u8}osabi; {u8}abiversion;
	{string(raw,7)}bourrage = [string repeat \x00 7];
	{u16}genre; {u16}machine; {u32}version2; {u64}entry;
	{u64}phoff; {u64}shoff; {bitarray(32)}flags;
	{u16}ehsize; {u16}phentsize; {u16}phnum;
	{u16}shentsize; {u16}shnum; {u16}shstrndx
    }
    puts "taille / size : [file binary sizeof elfhdr] octets"
    # FR : lecture DIRECTE depuis le canal ; il avance de 64 octets
    # EN : DIRECT read from the channel; it advances by 64 bytes
    set h [file binary read $f elfhdr]
    puts "position du canal après lecture / channel position after reading : [tell $f]"
    puts "classe $h(classe) (2 = 64 bits), machine $h(machine), entry [format 0x%x $h(entry)]"
    puts "$h(phnum) segments (en-têtes de programme) à l'offset $h(phoff)"
    puts "$h(shnum) sections à l'offset $h(shoff)"
    seek $f 0
    set brut [::read $f 64]
    puts "aller-retour / round trip : [expr {[file binary encode elfhdr $h] eq $brut}]"
} elseif {[string range $debut 0 1] eq "MZ"} {
    puts "\n=== 2. PE : [file tail $chemin] ==="
    # FR : l'en-tête DOS se réduit à « MZ » et au pointeur vers l'en-tête PE
    # EN : the DOS header boils down to "MZ" and the pointer to the PE header
    type def mz {"MZ"}
    type def pesig {"PE\x00\x00"}
    type def dosfin {
	{mz}magic = MZ; {string(raw,58)}stub = [string repeat \x00 58]; {u32}lfanew
    }
    type def coff {
	{pesig}signature = "PE\x00\x00";
	{u16}machine; {u16}nsections; {u32}timestamp;
	{u32}symtab; {u32}nsyms; {u16}optsize;
	{bitarray(16)}caracteristiques
    }
    set d [file binary read $f dosfin]
    # FR : on se place avec seek, puis on lit : le canal est un vrai canal
    # EN : position with seek, then read: it is a real channel
    seek $f $d(lfanew)
    set c [file binary read $f coff]
    puts "PE header at $d(lfanew), machine [format 0x%x $c(machine)], $c(nsections) sections"
    puts "DLL : $c(caracteristiques.13)   exécutable / executable image : $c(caracteristiques.1)"
    seek $f $d(lfanew)
    puts "aller-retour / round trip : [expr {[file binary encode coff $c] eq [::read $f 24]}]"
} else {
    puts "\n(format non reconnu / unrecognised format : $chemin)"
}
close $f

# ===========================================================================
# 3. Refus / refusals
# ===========================================================================

puts "\n=== 3. refus / refusals ==="
catch {file binary decode tcp [string range $octets 0 9]} m1
puts "tronqué / truncated : $m1"
type def mzc {"MZ"}
type def mzx {{mzc}magic = MZ; {int(u,16)}x}
catch {file binary decode mzx "XZ\x00\x00"} m2
puts "constante / constant : $m2"
catch {file binary encode tcp $r -endian middle} m3
puts "option : $m3"
catch {set {tcp}big src 70000 dst 1 seq 0 ack 0 flags 0 window 0 checksum 0 urgent 0} m4
puts "plage / range : $m4"
type def sansbin {{int}x}
catch {file binary sizeof sansbin} m5
puts "sans représentation / no representation : $m5"

# ===========================================================================
# 4. Élargissement automatique / automatic widening
# ===========================================================================

puts "\n=== 4. élargissement / widening ==="
# FR : un champ int(u,16,be) entre tel quel dans une variable int ou double, ou
#      dans un entier plus large ; l'inverse (rétrécir) est refusé.
# EN : a int(u,16,be) field goes as is into an int or double variable, or into
#      a wider integer; the reverse (narrowing) is refused.
set {int}n $t(src)
set {double}x $t(window)
set {int(u,32)}large $t(src)
puts "int <- int(u,16,be) : $n   double <- int(u,16,be) : $x   int(u,32) <- int(u,16,be) : $large"
set {int(u,8)}petit 7
catch {set petit $n} m6
puts "int(u,8) <- int : $m6"
