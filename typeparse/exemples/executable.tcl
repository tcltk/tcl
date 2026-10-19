# executable.tcl --
#
# FR : Le type « fichier exécutable » complet : une SOMME de deux formats,
#      ELF (executable_elf.tcl) et PE (executable_pe.tcl). Le type de la
#      somme dit de quel format il s'agit ; le reste du code n'a besoin
#      que de la structure commune : des sections (les sommets) et des
#      hyperarêtes (segments ELF, répertoires PE) sous forme de masques de
#      bits. L'analyse d'hypergraphe est donc écrite UNE seule fois.
# EN : The complete "executable file" type: a SUM of two formats, ELF
#      (executable_elf.tcl) and PE (executable_pe.tcl). The sum's type says
#      which format it is; the rest of the code only needs the common
#      structure: sections (the vertices) and hyperedges (ELF segments, PE
#      directories) as bit masks. The hypergraph analysis is therefore
#      written ONCE.
#
#   usage : tclsh executable.tcl ?file ...?     (default: the running tclsh)
#   try    : tclsh executable.tcl /bin/ls some.exe other.dll

# FR : on charge les deux lecteurs (leur programme principal ne se lance pas)
# EN : load the two readers (their main program does not run)
set ici [file dirname [file normalize [info script]]]
source [file join $ici executable_elf.tcl]
source [file join $ici executable_pe.tcl]

# ---------------------------------------------------------------------------
# FR : Les types   /   EN: The types
# ---------------------------------------------------------------------------

# FR : la somme : un exécutable est SOIT un elf(n) SOIT un pe(n), avec le
#      même n (nombre de sections). Le nom de variante est le nom de champ.
# EN : the sum: an executable is EITHER an elf(n) OR a pe(n), with the same
#      n (number of sections). The variant name is the field name.
type def {executable({int}n)} {{elf($n)}elf, {pe($n)}pe}

# FR : l'hypergraphe générique de hypergraphe.tcl (labels = sommets,
#      edges = hyperarêtes), qui sert aux deux formats.
# EN : the generic hypergraph of hypergraphe.tcl (labels = vertices,
#      edges = hyperedges), shared by both formats.
type def {hg({int}n)} {{list($n).string}labels; {list.bitarray($n)}edges}

# ---------------------------------------------------------------------------
# FR : Lecture : détecte le format et rend une valeur executable(n)
# EN : Reading: detects the format and returns an executable(n) value
# ---------------------------------------------------------------------------

proc lire_executable {chemin} {
    set f [open $chemin rb]
    set debut [read $f 4]
    close $f
    if {$debut eq "\x7fELF"} {
	lassign [lire_elf $chemin] n valeurs
	set champs {bits endian machine entry sections segments couverture}
	set variante elf
    } elseif {[string range $debut 0 1] eq "MZ"} {
	lassign [lire_pe $chemin] n valeurs
	set champs {bits machine fichier dllcar subsystem entry imagebase
	    sections repertoires couverture}
	set variante pe
    } else {
	error "$chemin: format inconnu / unknown format"
    }
    # FR : le nom des types contient n : on construit les commandes en texte
    # EN : type names contain n: the commands are built as text
    # FR : construction par chemin : set {T}V(c1,c2,...) v1 v2 ...
    # EN : construction by path: set {T}V(f1,f2,...) v1 v2 ...
    set cmd "set {${variante}($n)}V([join $champs ,])"
    foreach v $valeurs {append cmd " " [list $v]}
    eval $cmd
    eval "set {executable($n)}X {$variante}\$V"
    return $X
}

# ---------------------------------------------------------------------------
# FR : Accès communs   /   EN: Common accessors
# ---------------------------------------------------------------------------

# FR : variante d'une valeur somme : « executable(31).elf » -> « elf »
# EN : variant of a sum value: "executable(31).elf" -> "elf"
proc variante {x} {
    lindex [split [type get $x] .] end
}

# FR : sections en lecture/écriture/exécution, selon le format
# EN : sections' read/write/execute rights, depending on the format
proc droits {x i} {
    set v [variante $x]
    if {$v eq "elf"} {
	# FR : ELF : le droit est porté par le segment LOAD qui contient la section
	# EN : ELF: the right belongs to the LOAD segment holding the section
	set d "---"
	for {set s 0} {$s < [llength $x($v.segments)]} {incr s} {
	    if {$x($v.segments.$s.genre) eq "LOAD" && $x($v.couverture.$s.$i)} {
		set p $x($v.segments.$s.perms)
		set d "[expr {$p(2) ? "R" : "-"}][expr {$p(1) ? "W" : "-"}][expr {$p(0) ? "X" : "-"}]"
	    }
	}
	return $d
    } else {
	# FR : PE : le droit est dans les drapeaux de la section
	# EN : PE: the right is in the section's flags
	set f $x($v.sections.$i.flags)
	return "[expr {$f(30) ? "R" : "-"}][expr {$f(31) ? "W" : "-"}][expr {$f(29) ? "X" : "-"}]"
    }
}

# FR : le nom commun des hyperarêtes : segment ELF ou répertoire PE
# EN : the common name of hyperedges: ELF segment or PE directory
proc nom_arete {x j} {
    set v [variante $x]
    if {$v eq "elf"} {return "$x($v.segments.$j.genre)#$j"}
    return $x($v.repertoires.$j.nom)
}

# FR : construit l'hypergraphe hg(n) des deux formats (une seule fois)
# EN : builds the hg(n) hypergraph for both formats (once)
proc hypergraphe_de {x} {
    set v [variante $x]
    set n [llength $x($v.sections)]
    set noms [lmap s $x($v.sections) {expr {[lindex $s 0] eq "" ? "(null)" : [lindex $s 0]}}]
    set cmd "set {hg($n)}H labels [list $noms] edges [list $x($v.couverture)]"
    eval $cmd
    return $H
}

# ---------------------------------------------------------------------------
# FR : Analyse d'hypergraphe : indépendante du format
# EN : Hypergraph analysis: independent of the format
# ---------------------------------------------------------------------------

proc analyser {x} {
    set v [variante $x]
    set H [hypergraphe_de $x]
    set n [llength $H(labels)]
    set m [llength $H(edges)]

    # FR : sections qui ne sont dans aucune hyperarête (non chargées / sans répertoire)
    # EN : sections in no hyperedge (not loaded / no directory)
    set union 0
    for {set j 0} {$j < $m} {incr j} {set union [expr {$union | $H(edges.$j)}]}
    set hors [expr {$union ^ ((1 << $n) - 1)}]
    set liste {}
    for {set i 0} {$i < $n} {incr i} {
	if {($hors >> $i) & 1} {lappend liste [lindex $H(labels) $i]}
    }
    puts "  [expr {popcount($hors)}] section(s) in no hyperedge: $liste"

    # FR : section dans le plus d'hyperarêtes = la plus « centrale »
    # EN : section in the most hyperedges = the most "central"
    set best -1
    set meilleure ""
    for {set i 0} {$i < $n} {incr i} {
	set deg 0
	for {set j 0} {$j < $m} {incr j} {incr deg $H(edges.$j.$i)}
	if {$deg > $best} {set best $deg; set meilleure [lindex $H(labels) $i]}
    }
    puts "  most central section: $meilleure (degree $best)"

    # FR : l'hyperarête la plus grande / EN: the biggest hyperedge
    set grand -1
    for {set j 0} {$j < $m} {incr j} {
	set t [expr {popcount($H(edges.$j))}]
	if {$t > $grand} {set grand $t; set jg $j}
    }
    puts "  biggest hyperedge: [nom_arete $x $jg] ($grand sections)"

    # FR : paires d'hyperarêtes qui partagent le plus de sections
    # EN : pair of hyperedges sharing the most sections
    set maxc 0
    set paire ""
    for {set a 0} {$a < $m} {incr a} {
	for {set b [expr {$a + 1}]} {$b < $m} {incr b} {
	    set c [expr {popcount($H(edges.$a) & $H(edges.$b))}]
	    if {$c > $maxc} {set maxc $c; set paire "[nom_arete $x $a] & [nom_arete $x $b]"}
	}
    }
    puts "  most overlapping pair: [expr {$paire eq "" ? "none" : "$paire ($maxc sections)"}]"
}

# FR : contrôles spécifiques d'un format, choisis par la VARIANTE de la somme
# EN : format-specific checks, selected by the sum's VARIANT
proc controles {x} {
    set v [variante $x]
    switch $v {
	elf {
	    set wx {}
	    for {set s 0} {$s < [llength $x(elf.segments)]} {incr s} {
		if {($x(elf.segments.$s.perms) & 3) == 3} {lappend wx $s}
	    }
	    puts "  W^X: [expr {[llength $wx] ? "VIOLATION $wx" : "ok"}]"
	}
	pe {
	    set aslr [expr {$x(pe.dllcar.6) ? "yes" : "no"}]
	    set dep  [expr {$x(pe.dllcar.8) ? "yes" : "no"}]
	    puts "  ASLR: $aslr   DEP: $dep   [expr {$x(pe.fichier.13) ? "DLL" : "EXE"}]"
	}
    }
}

# ---------------------------------------------------------------------------
# FR : Programme principal   /   EN: Main program
# ---------------------------------------------------------------------------

# FR : une procédure par fichier : x est typé executable(n), et un n différent
#      par fichier interdirait de réutiliser la même variable globale.
# EN : one procedure per file: x is typed executable(n), and a different n per
#      file would forbid reusing the same global variable.
proc rapporter {chemin} {
    set x [lire_executable $chemin]
    set v [variante $x]
    puts "\n=== [file tail $chemin] ==="
    puts "type: [type get $x]   format: [string toupper $v]$x($v.bits) $x($v.machine)"
    puts "entry: [format 0x%x $x($v.entry)]"
    controles $x
    puts "sections (first 6):"
    for {set i 0} {$i < 6 && $i < [llength $x($v.sections)]} {incr i} {
	puts [format "  %2d %-22s %s" $i $x($v.sections.$i.nom) [droits $x $i]]
    }
    puts "hypergraph:"
    analyser $x
}

set fichiers [expr {$argc ? $argv : [list [info nameofexecutable]]}]
foreach chemin $fichiers {rapporter $chemin}
