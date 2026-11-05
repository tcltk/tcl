# zipmem.tcl -- lire une archive zip en mémoire, et la sourcer
#
# FR : le format zip décrit par des types binaires (§ 9 de ANNOTATIONS.fr.md),
#      lu par type binary decode, sans monter de système de fichiers :
#
#        set z [zipmem::open $octets]          ;# l'archive : son répertoire
#        zipmem::names $z                      ;# les membres
#        zipmem::read $z lib/util.tcl          ;# leurs octets (inflate, CRC)
#        zipmem::source app.zip                ;# main.tcl de l'archive
#        zipmem::install                       ;# puis : source app.zip
#
#      Une archive se reconnaît à son type : zipmem::is cherche la fin du
#      répertoire central et demande type binary {is?}::zipmem::eocd.
#      Pendant et après zipmem::source, un source d'un chemin de l'archive
#      (source [file join [file dirname [info script]] lib/x.tcl]) lit le
#      membre en mémoire.
#
#      Tcl 9 a zipfs (zipfs mount, zipfs mountdata), un vrai système de
#      fichiers virtuel : glob, open, file exists, load y marchent ; zipmem
#      n'en est pas un. Ce qu'il montre : le format, écrit en types, lisible
#      et vérifié ; une archive reçue en mémoire (un socket, une base de
#      données), lue sans montage ; source qui reconnaît une archive par son
#      type.
#
# EN : the zip format as binary types, read from memory with type binary
#      decode; zipmem::source / zipmem::install let "source app.zip" run the
#      archive's main.tcl. zipfs (Tcl 9) remains the real virtual file system.
#
# Limites : pas de zip64, de chiffrement, de méthode autre que stored (0) et
# deflate (8) ; les noms sont lus en utf-8 (un vieux nom cp437 non ASCII
# serait mal lu).

if {[info commands ::fb::filebinary] eq ""} {
    set _here [file dirname [file normalize [info script]]]
    source [file join $_here .. .. layout filebinary.tcl]
    source [file join $_here .. .. layout binaliases.tcl]
    unset _here
}

namespace eval ::zipmem {
    variable seq 0
    variable mounted {}		;# préfixe -> archive (pour source)

    # --- le format (APPNOTE.TXT de PKWARE, § 4.3) -------------------------

    type def sig_local {"PK\x03\x04"}
    type def sig_entry {"PK\x01\x02"}
    type def sig_eocd  {"PK\x05\x06"}
    type def sig_eocd64 {"PK\x06\x06"}
    type def sig_loc64  {"PK\x06\x07"}

    # 4.3.16 : la fin du répertoire central, à la fin de l'archive
    type def eocd {
	{sig_eocd}sig;
	{u16le}disk;   {u16le}cddisk;
	{u16le}nhere;  {u16le}n;		# entrées (ici, en tout)
	{u32le}cdsize; {u32le}cdoff;		# le répertoire central
	{u16le}clen;   {string(utf-8,$clen)}comment
    }

    # 4.3.14, 4.3.15 : zip64, la fin du répertoire en 64 bits, et son
    # repère, juste avant eocd (une archive écrite d'un trait, zip - -, en a
    # même quand les tailles tiennent en 32 bits)
    type def eocd64 {
	{sig_eocd64}sig; {u64le}size;
	{u16le}made;     {u16le}needed;
	{u32le}disk;     {u32le}cddisk;
	{u64le}nhere;    {u64le}n;
	{u64le}cdsize;   {u64le}cdoff
    }
    type def loc64 {{sig_loc64}sig; {u32le}disk; {u64le}off; {u32le}ndisks}

    # 4.3.7 : l'en-tête local, devant les données du membre
    type def local {
	{sig_local}sig;
	{u16le}needed; {u16le}flags; {u16le}method;
	{u16le}mtime;  {u16le}mdate;
	{u32le}crc;    {u32le}csize; {u32le}usize;
	{u16le}nlen;   {u16le}xlen;
	{string(utf-8,$nlen)}name; {string(raw,$xlen)}extra
    }

    # 4.3.12 : une entrée du répertoire central ; "local" est l'adresse de
    # l'en-tête local, depuis le début de l'archive. (Un lien
    # {*(//,u32le).local}local le dirait mieux, mais lu sans être suivi,
    # -linkdepth 0, il passe par la couche Tcl : 75 fois plus lent.)
    type def entry {
	{sig_entry}sig;
	{u16le}made;   {u16le}needed; {u16le}flags; {u16le}method;
	{u16le}mtime;  {u16le}mdate;
	{u32le}crc;    {u32le}csize;  {u32le}usize;
	{u16le}nlen;   {u16le}xlen;   {u16le}clen;
	{u16le}disk;   {u16le}iattr;  {u32le}xattr;
	{u32le}local;
	{string(utf-8,$nlen)}name; {string(raw,$xlen)}extra;
	{string(utf-8,$clen)}comment
    }
}

# Où est la fin du répertoire ? Elle est à la fin, suivie d'un commentaire
# d'au plus 65535 octets : la dernière signature qui se décode en eocd et
# dont le commentaire va jusqu'au bout.
proc ::zipmem::Locate {data} {
    # la recherche dans la fin seulement (string last sur 16 Mo d'octets
    # coûterait leur conversion)
    set len [string length $data]
    set from [expr {max(0, $len - 22 - 65535)}]
    set end [string range $data $from end]
    set p [string last "PK\x05\x06" $end]
    while {$p >= 0} {
	set pos [expr {$from + $p}]
	set tail [string range $end $p end]
	if {[type binary {is?}::zipmem::eocd $tail]} {
	    set e [type binary decode ::zipmem::eocd $tail]
	    if {22 + $e(clen) == [string length $tail]} {
		return [list $pos $e]
	    }
	}
	set p [string last "PK\x05\x06" $end [expr {$p - 1}]]
    }
    return {}
}

# 1 si DATA (des octets) est une archive zip
proc ::zipmem::is {data} {
    expr {[llength [Locate $data]] > 0}
}

# L'archive : un dict {data base comment entries}, entries : nom -> entrée
# (une valeur ::zipmem::entry ; son champ local, un lien non suivi).
proc ::zipmem::open {data} {
    set loc [Locate $data]
    if {$loc eq ""} {
	return -code error "zipmem: not a zip archive (no end of central\
		directory record)"
    }
    lassign $loc pos e
    set n $e(n); set cdsize $e(cdsize); set cdoff $e(cdoff)
    set cdend $pos
    # zip64 : le repère est juste avant eocd, la fin 64 bits avant lui
    if {$pos >= 20 && [type binary {is?}::zipmem::loc64 \
	    [string range $data $pos-20 $pos-1]]} {
	set p64 [string last "PK\x06\x06" \
		[string range $data [expr {max(0, $pos - 4096)}] $pos-21]]
	if {$p64 < 0} {
	    return -code error "zipmem: zip64 locator without its record"
	}
	incr p64 [expr {max(0, $pos - 4096)}]
	set e64 [type binary decode ::zipmem::eocd64 \
		[string range $data $p64 [expr {$p64 + 55}]]]
	set n $e64(n); set cdsize $e64(cdsize); set cdoff $e64(cdoff)
	set cdend $p64
    } elseif {$n == 0xFFFF || $cdoff == 0xFFFFFFFF} {
	return -code error "zipmem: bad zip64 archive"
    }
    if {$e(disk) != 0 || $e(nhere) != $e(n)} {
	return -code error "zipmem: multi-disk archives are not supported"
    }
    # une archive peut suivre autre chose (un exécutable) : ses adresses
    # partent alors de son propre début
    set base [expr {$cdend - $cdsize - $cdoff}]
    if {$base < 0} {
	return -code error "zipmem: bad central directory offset"
    }
    set entries [dict create]
    if {$n > 0} {
	# le répertoire seul (une tranche) ; dans un produit d'un champ : la
	# voie C du décodage (une liste seule passerait par la couche Tcl)
	set cd [string range $data [expr {$base + $cdoff}] \
		[expr {$base + $cdoff + $cdsize - 1}]]
	set dir [type binary decode "{list($n).::zipmem::entry}e" $cd]
	foreach x $dir(e) {
	    # "./main.tcl" (zipfs mkzip .) est main.tcl ; le nom est le champ
	    # 17 (un produit est la liste de ses champs, v16t)
	    set name [lindex $x 17]
	    if {[string match ./* $name]} {set name [regsub {^(\./)+} $name {}]}
	    dict set entries $name $x
	}
    }
    return [dict create data $data base $base comment $e(comment) \
	    entries $entries]
}

proc ::zipmem::names {z} {
    dict keys [dict get $z entries]
}

# Une entrée : {taille méthode crc compressée} pour l'affichage
proc ::zipmem::info {z name} {
    set x [Entry $z $name]
    dict create size $x(usize) compressed $x(csize) \
	    method [dict get {0 stored 8 deflate} $x(method)] crc $x(crc)
}

proc ::zipmem::Entry {z name} {
    if {![dict exists $z entries $name]} {
	return -code error "zipmem: no member \"$name\" in the archive"
    }
    dict get $z entries $name
}

# Les octets d'un membre : l'en-tête local (le lien de l'entrée), puis les
# données, décompressées et vérifiées.
proc ::zipmem::read {z name} {
    set x [Entry $z $name]
    if {$x(flags) & 1} {
	return -code error "zipmem: $name is encrypted"
    }
    set data [dict get $z data]
    # l'en-tête local, lu dans une tranche (décoder dans les 16 Mo d'une
    # grosse archive coûterait leur copie)
    set at [expr {[dict get $z base] + $x(local)}]
    set h [type binary decode ::zipmem::local \
	    [string range $data $at [expr {$at + 30 + 2 * 65535}]]]
    set start [expr {$at + 30 + $h(nlen) + $h(xlen)}]
    # les tailles de l'entrée centrale : celles de l'en-tête local valent 0
    # quand elles suivent les données (bit 3, archive écrite d'un trait)
    set raw [string range $data $start [expr {$start + $x(csize) - 1}]]
    switch -- $x(method) {
	0 {set out $raw}
	8 {set out [zlib inflate $raw]}
	default {
	    return -code error "zipmem: $name: compression method $x(method)\
		    is not supported (only stored and deflate)"
	}
    }
    if {[string length $out] != $x(usize) || [zlib crc32 $out] != $x(crc)} {
	return -code error "zipmem: $name: CRC or size mismatch"
    }
    return $out
}

proc ::zipmem::text {z name {encoding utf-8}} {
    encoding convertfrom $encoding [read $z $name]
}

# ----------------------------------------------------------------------
# source
# ----------------------------------------------------------------------

# Sourcer MEMBER (main.tcl par défaut) d'une archive : un chemin, ou des
# octets (-data). Le script voit info script = PREFIXE/membre ; un source
# d'un chemin sous PREFIXE lit le membre en mémoire.
proc ::zipmem::source {args} {
    variable seq
    variable mounted
    set member main.tcl
    if {[lindex $args 0] eq "-data"} {
	set data [lindex $args 1]
	set prefix zipmem[incr seq]
	set args [lrange $args 2 end]
    } else {
	set path [file normalize [lindex $args 0]]
	set f [::open $path rb]
	set data [::read $f]
	close $f
	set prefix $path
	set args [lrange $args 1 end]
    }
    if {[llength $args] > 0} {set member [lindex $args 0]}
    if {![dict exists $mounted $prefix]} {
	dict set mounted $prefix [open $data]
    }
    Install
    tailcall ::zipmem::Eval $prefix $member
}

# Évalue un membre, au niveau de l'appelant, comme source : return finit le
# script, info script donne son chemin.
proc ::zipmem::Eval {prefix member} {
    variable mounted
    set z [dict get $mounted $prefix]
    set script [text $z $member]
    set old [::info script]
    ::info script $prefix/$member
    set code [catch {uplevel 1 $script} r opts]
    ::info script $old
    if {$code == 2} {
	# return dans le script : la fin du script, comme source
	dict set opts -code [dict get $opts -code]
	return $r
    }
    dict incr opts -level
    return -options $opts $r
}

# source reconnaît les chemins des archives ouvertes, et (zipmem::install)
# une archive donnée par son chemin.
proc ::zipmem::Install {{detect 0}} {
    variable detecting
    if {$detect} {set detecting 1}
    if {[::info commands ::zipmem::RealSource] ne ""} return
    rename ::source ::zipmem::RealSource
    proc ::source {args} {
	set path [lindex $args end]
	set full [file normalize $path]
	dict for {prefix z} $::zipmem::mounted {
	    if {[string match $prefix/* $full] || [string match $prefix/* $path]} {
		set member [string range $full [string length $prefix/] end]
		if {![string match $prefix/* $full]} {
		    set member [string range $path [string length $prefix/] end]
		}
		tailcall ::zipmem::Eval $prefix $member
	    }
	}
	if {[::info exists ::zipmem::detecting] && [llength $args] == 1
		&& [file isfile $path] && [::zipmem::LooksZip $path]} {
	    tailcall ::zipmem::source $path
	}
	tailcall ::zipmem::RealSource {*}$args
    }
}

# zipmem::install : source app.zip sourcera main.tcl de l'archive
proc ::zipmem::install {} {
    Install 1
}

# Une archive ? Les premiers octets (PK\3\4), ou l'extension .zip, puis le
# type de la fin du répertoire.
proc ::zipmem::LooksZip {path} {
    set f [::open $path rb]
    set head [::read $f 4]
    if {$head ne "PK\x03\x04" && ![string equal -nocase [file extension $path] .zip]} {
	close $f
	return 0
    }
    seek $f 0
    set data [::read $f]
    close $f
    return [is $data]
}

package provide zipmem 0.1
