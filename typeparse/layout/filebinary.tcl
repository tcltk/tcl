# filebinary.tcl --
#
# FR : « type binary » et « chan binary » : lecture et écriture de valeurs typées
#      dans des octets en mémoire, des fichiers ou des canaux binaires, en pur Tcl
#      au-dessus du noyau. (L'ancien nom « file binary » reste un alias.)
#
#      AUCUNE syntaxe supplémentaire : la représentation binaire d'un champ
#      est portée par son TYPE, comme bitarray(n) porte déjà sa largeur.
#      Le noyau fournit ces types (patch types-binaires) et `type binary`,
#      qui décrit la représentation d'un type ; ce fichier n'en fait que
#      la lecture/écriture.
#
#      Types à représentation binaire :
#        int(u,n)  int(s,n)  entier non signé / signé de n bits (n multiple de 8)
#        double(32|64)       flottant IEEE
#        int(u,16,be)        boutisme en dernier argument : be (big) ou le
#                            (little) ; double(64,le), bitarray(16,msb0,be)
#        string(raw,N)       N octets bruts (chaîne binaire, caractères <= 255)
#        string(ENC,N)       texte sur N octets, complété par des NUL ;
#                            ENC : ascii latin1 utf-8 utf-16le utf-16be
#                            ucs-2le ucs-2be
#        string(ENC,z)       texte terminé par un NUL (deux octets en utf-16)
#        bitarray(n)         n bits (multiple de 8), lu comme un entier non signé
#        "MZ"                une somme de littéraux de même longueur = une CONSTANTE
#                            (type def mz {"MZ"})
#        list(N).T           N éléments de T, un produit de ces types (imbrication)
#      Sans boutisme, c'est celui de l'appel (-endian, little par défaut).
#      Un champ sans représentation binaire est admis s'il a une valeur par
#      défaut : il n'est pas dans le fichier.
#
#      Niveau 2 : une taille, un nombre ou une position peut venir du fichier :
#        string(raw,$len)  string(utf-8,$len - 4)  list($n).T  at($off).T
#        string(utf-8,int(u,16,be))  list(int(u,8)).T  (préfixe)  corps($tag)
#      ($champ : un champ déjà lu du produit courant ou d'un produit englobant ;
#      corps($tag) : la variante de la somme corps est celle dont le nom égale le
#      champ tag). Voir MANUEL.md et exemples/niveau2.tcl.
#
#      Alignement : par défaut les produits sont compacts (packed). L'option
#      -align natural (ou -align N) insère le remplissage d'alignement du C et de
#      LLVM : chaque champ est placé à un multiple de son alignement (sa taille,
#      plafonnée à 8 ou N), la taille d'un produit est arrondie à son alignement.
#      Le remplissage vaut zéro à l'écriture et est ignoré à la lecture, sauf
#      avec -strictpad 1.
#
# EN : "type binary" and "chan binary": reading and writing typed values in bytes
#      held in memory, binary files or channels, in pure Tcl on top of the core.
#      (The former name "file binary" remains as an alias.)
#
#      NO extra syntax: the binary representation of a field is carried by
#      its TYPE, just as bitarray(n) already carries its width. The core
#      provides those types (patch types-binaires) and `type binary`, which
#      describes the representation of a type; this file only does the
#      reading/writing.
#
#      Types with a binary representation: int(u,n), int(s,n), double(32|64),
#      the byte order being the last argument (be or le: int(u,16,be),
#      double(64,le), bitarray(16,msb0,be)); string(raw,N), string(ENC,N),
#      string(ENC,z) (ENC: ascii latin1 utf-8 utf-16le utf-16be ucs-2le
#      ucs-2be), bitarray(n), a sum of same-length literals (= a CONSTANT,
#      `type def mz {"MZ"}`), list(N).T, and products of these (nesting).
#      Without a byte order, that of the call is used (-endian, little by
#      default). A field with no binary representation is allowed if it has
#      a default value: it is not in the file.
#
#      Level 2: a size, a count or a position can come from the file:
#        string(raw,$len)  string(utf-8,$len - 4)  list($n).T  at($off).T
#        string(utf-8,int(u,16,be))  list(int(u,8)).T  (prefix)  corps($tag)
#      ($field: an already-read field of the current product or an enclosing
#      one; corps($tag): the variant of the sum corps is the one whose name
#      equals field tag). See MANUEL.md and exemples/niveau2.tcl.
#
#      Alignment: products are packed by default. -align natural (or -align N)
#      inserts the alignment padding of C and LLVM: each field starts at a
#      multiple of its alignment (its size, capped at 8 or N) and the size of
#      a product is rounded up to its alignment. Padding is written as zeros
#      and ignored on reading, unless -strictpad 1.
#
# Commandes / Commands :
#
#   chan binary read   CHANNEL TYPE ?-endian little|big? ?-align A? ?-strictpad B?
#   chan binary write  CHANNEL TYPE VALUE ?-endian E? ?-align A?
#   type binary decode TYPE BYTES ?-offset N? ?-endian E? ?-consumed VAR?
#                                 ?-align A? ?-strictpad B?
#   type binary encode TYPE VALUE ?-endian E? ?-align A?
#   type binary {sizeof?}TYPE ?-align A?
#
#   A : packed (défaut / default), natural, ou une puissance de deux / or a power of two

namespace eval ::fb {
    variable cache [dict create]
    variable late {}
    variable pend {}
    variable seen {}
    # FR : valeur rendue par un lien nul (voir le champ à défaut d'un produit)
    # EN : value returned by a null link (see a product field with a default)
    variable nullLink "\u0000null-link\u0000"
}

# ---------------------------------------------------------------------------
# FR : Représentation d'un type (demandée au noyau, mise en cache)
# EN : A type's representation (asked from the core, cached)
# ---------------------------------------------------------------------------

proc ::fb::repr {type} {
    variable cache
    if {![dict exists $cache $type]} {
	dict set cache $type [::type binary $type]
    }
    return [dict get $cache $type]
}

# FR : le contexte ctx est un dictionnaire : endian (little|big), align
#      (plafond d'alignement, 1 = compact), strict (0|1)
# EN : the context ctx is a dict: endian (little|big), align (alignment cap,
#      1 = packed), strict (0|1)

# FR : boutisme effectif : celui du type s'il en a un, sinon celui de l'appel
# EN : effective byte order: the type's if it has one, else the call's
proc ::fb::endianOf {typeEnd ctx} {
    switch $typeEnd {
	le {return little}
	be {return big}
	default {return [dict get $ctx endian]}
    }
}

# ---------------------------------------------------------------------------
# FR : Entiers et flottants   /   EN: Integers and floats
# ---------------------------------------------------------------------------

proc ::fb::fmt {kind size endian} {
    set big [expr {$endian eq "big"}]
    if {$kind eq "float"} {
	return [dict get [expr {$big ? {4 R 8 Q} : {4 r 8 q}}] $size]
    }
    set l [dict get {1 c 2 s 4 i 8 w} $size]
    if {$big && $size > 1} {set l [string toupper $l]}
    return $l
}

# FR : octets -> entier (toute largeur)   /   EN: bytes -> integer (any width)
proc ::fb::getInt {raw signed size endian} {
    if {$size in {1 2 4 8}} {
	set f [fmt int $size $endian]
	if {!$signed} {append f u}
	binary scan $raw $f v
	return $v
    }
    binary scan $raw cu* bytes
    if {$endian eq "little"} {set bytes [lreverse $bytes]}
    set v 0
    foreach b $bytes {set v [expr {($v << 8) | $b}]}
    if {$signed && $v >= (1 << (8 * $size - 1))} {
	set v [expr {$v - (1 << (8 * $size))}]
    }
    return $v
}

proc ::fb::putInt {v size endian} {
    if {$size in {1 2 4 8}} {
	# FR : binary format accepte un non signé qui déborde du signé (wrap)
	# EN : binary format accepts an unsigned that overflows the signed (wrap)
	return [binary format [fmt int $size $endian] $v]
    }
    if {$v < 0} {set v [expr {$v + (1 << (8 * $size))}]}
    set bytes {}
    for {set i 0} {$i < $size} {incr i} {
	lappend bytes [expr {$v & 255}]
	set v [expr {$v >> 8}]
    }
    if {$endian eq "big"} {set bytes [lreverse $bytes]}
    return [binary format c* $bytes]
}

# ---------------------------------------------------------------------------
# FR : Alignement   /   EN: Alignment
# ---------------------------------------------------------------------------

# FR : alignement naturel d'une représentation, plafonné par ctx
# EN : natural alignment of a representation, capped by ctx
# FR : {linkref NOM} : un lien qui se retrouve dans sa propre cible (liste
#      chaînée) ; on le développe à la demande.
# EN : {linkref NAME}: a link that appears again inside its own target (linked
#      list); it is expanded on demand.
proc ::fb::unref {r} {
    if {[lindex $r 0] eq "linkref"} {return [repr [lindex $r 1]]}
    return $r
}

proc ::fb::alignOf {r ctx} {
    set r [unref $r]
    set cap [dict get $ctx align]
    if {$cap <= 1} {return 1}
    lassign $r kind a b c
    switch $kind {
	uint - sint - float - bits {
	    set p 1
	    while {$p * 2 <= $a} {set p [expr {$p * 2}]}
	    return [expr {min($p, $cap)}]
	}
	array {return [alignOf $b $ctx]}
	link {return [alignOf $b $ctx]}
	at - tagged {return 1}
	product {
	    set m 1
	    foreach f $b {
		lassign $f fname frepr hasdef
		set frepr [unref $frepr]
		if {$frepr eq "none"} continue
		set m [expr {max($m, [alignOf $frepr $ctx])}]
	    }
	    return $m
	}
    }
    return 1
}

# FR : octets de remplissage pour amener off à un multiple de al
# EN : padding bytes needed to bring off to a multiple of al
proc ::fb::padFor {off al} {
    return [expr {(-$off) % $al}]
}

# FR : lit (et vérifie si strict) le remplissage avant l'offset aligné
# EN : reads (and checks when strict) the padding up to the aligned offset
proc ::fb::skipPad {sName al ctx what} {
    upvar 1 $sName s
    if {$al <= 1} return
    set n [padFor [expr {$s(pos) - $s(base)}] $al]
    if {$n == 0} return
    set got [take s $n "padding before $what"]
    if {[dict get $ctx strict] && $got ne [string repeat \x00 $n]} {
	error "type binary: non-zero padding at offset [expr {$s(pos) - $n}] before $what"
    }
}

proc ::fb::putPad {outName al} {
    upvar 1 $outName out
    if {$al <= 1} return
    append out [string repeat \x00 [padFor [string length $out] $al]]
}

# FR : octets du terminateur NUL d'un encodage
# EN : bytes of the NUL terminator of an encoding
proc ::fb::nulWidth {enc} {
    expr {[string match utf-16* $enc] || [string match ucs-2* $enc] ? 2 : 1}
}


# ---------------------------------------------------------------------------
# FR : Niveau 2 : tailles, nombres et positions lus dans le fichier
# EN : Level 2: sizes, counts and positions read from the file
# ---------------------------------------------------------------------------

# FR : une expression de la forme "$len - 4" évaluée sur les champs déjà lus
#      (scope : dictionnaire nom -> valeur). Un champ produit est lu avec
#      $h(off), comme partout.
# EN : an expression such as "$len - 4" evaluated on the fields already read
#      (scope: dict name -> value). A product field is read with $h(off), as
#      everywhere else.
proc ::fb::evalExpr {text scope what} {
    if {[catch {
	apply {{__text __scope} {
	    dict for {__k __v} $__scope {set $__k $__v}
	    expr $__text
	}} $text $scope
    } r]} {
	error "type binary: cannot compute \"$text\" for $what: $r"
    }
    return $r
}


# FR : valeur du sélecteur d'une somme : « =nom » (constant), un nom de champ
#      déjà lu, ou une expression sur les champs ($shoff != 0)
# EN : value of a sum's selector: "=name" (constant), the name of an
#      already-read field, or an expression on fields ($shoff != 0)
proc ::fb::tagValue {a ctx what verb} {
    set scope [dict get $ctx scope]
    if {[string index $a 0] eq "="} {
	return [string range $a 1 end]
    }
    if {[dict exists $scope $a]} {
	return [dict get $scope $a]
    }
    if {[regexp {^[A-Za-z_][A-Za-z0-9_]*$} $a]} {
	error "type binary: tag field $a of $what has not been $verb"
    }
    return [evalExpr "\$$a" $scope $what]
}

# FR : une taille ou un nombre : entier, {prefix REPR} ou {expr TEXTE}
# EN : a size or count: integer, {prefix REPR} or {expr TEXT}
proc ::fb::isFixed {spec} {
    string is integer -strict $spec
}

proc ::fb::readSize {spec sName ctx what} {
    upvar 1 $sName s
    if {[isFixed $spec]} {return $spec}
    lassign $spec how arg
    if {$how eq "prefix"} {
	set n [dec $arg s $ctx "$what (length prefix)"]
    } else {
	set n [evalExpr $arg [dict get $ctx scope] $what]
    }
    if {![string is integer -strict $n] || $n < 0} {
	error "type binary: the size or count of $what is \"$n\", not a natural number"
    }
    return $n
}

# FR : (nombre, préfixe à écrire) pour écrire une taille ou un nombre réel n
# EN : (prefix to write) when writing an actual size or count n
proc ::fb::writeSize {spec n ctx what outName} {
    upvar 1 $outName out
    if {[isFixed $spec]} {return}
    lassign $spec how arg
    if {$how eq "prefix"} {
	enc $arg $n $ctx "$what (length prefix)" out
    } else {
	set want [evalExpr $arg [dict get $ctx scope] $what]
	if {$want != $n} {
	    error "type binary: $what holds $n, but the field it depends on says $want ($arg)"
	}
    }
}


# ---------------------------------------------------------------------------
# FR : Liens *(origine,repr).T  /  EN: Links *(origin,repr).T
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# FR : Les référentiels : les octets où vivent les adresses, avec leur base et,
#      pour une zone mémoire, la table qui traduit une adresse virtuelle en
#      position. Un lien `*(//nom/,...)` ou `*(memory://nom/,...)` désigne
#      le référentiel enregistré sous ce nom.
# EN : Spaces: the bytes where addresses live, with their base and, for a
#      memory area, the table that turns a virtual address into a position.
#      A link `*(//name/,...)` or `*(memory://name/,...)` designates the space
#      registered under that name.
# ---------------------------------------------------------------------------

namespace eval ::fb {
    variable spaces {}
    variable spaceSeq 0
}

proc ::fb::spaceName {} {
    variable spaceSeq
    return "space[incr spaceSeq]"
}

# FR : le nom vient du défaut calculé du champ ; on peut aussi le donner
# EN : the name comes from the field's computed default; it can also be given
catch {type def space {
    {string}name = [::fb::spaceName];
    {string}data;
    {int}base = 0;
    {list}segments = {}
}}

# FR : {nom genre} d'une origine nommée, ou {} (données de l'appel)
# EN : {name kind} of a named origin, or {} (the call's data)
proc ::fb::originSpace {origin} {
    if {[string match memory://*/ $origin]} {
	return [list [string range $origin 9 end-1] memory]
    }
    if {[string length $origin] > 2 && [string match //*/ $origin]} {
	return [list [string range $origin 2 end-1] file]
    }
    return {}
}

proc ::fb::spaceLookup {name what} {
    variable spaces
    if {![dict exists $spaces $name]} {
	error "type binary: unknown space \"$name\" ($what)"
    }
    return [dict get $spaces $name]
}

# FR : position (dans les octets du référentiel) de l'adresse addr
# EN : position (in the space's bytes) of address addr
proc ::fb::spaceTarget {name kind addr what} {
    set sp [spaceLookup $name $what]
    set segs $sp(segments)
    if {$kind eq "memory"} {
	if {[llength $segs] == 0} {
	    error "type binary: space \"$name\" has no segments: memory://$name/ needs a space made by \"type binary space memory\" ($what)"
	}
	foreach seg $segs {
	    lassign $seg vaddr offset size
	    if {$addr >= $vaddr && $addr < $vaddr + $size} {
		return [expr {$sp(base) + $offset + $addr - $vaddr}]
	    }
	}
	error [format "type binary: unmapped address 0x%x in space \"%s\" (%s)" $addr $name $what]
    }
    if {[llength $segs] != 0} {
	error "type binary: space \"$name\" is a memory area: write memory://$name/ ($what)"
    }
    return [expr {$sp(base) + $addr}]
}

# FR : l'inverse : l'adresse à stocker pour la position pos
# EN : the inverse: the address to store for position pos
proc ::fb::spaceAddress {name kind pos what} {
    set sp [spaceLookup $name $what]
    if {$kind eq "memory"} {
	foreach seg $sp(segments) {
	    lassign $seg vaddr offset size
	    set rel [expr {$pos - $sp(base) - $offset}]
	    if {$rel >= 0 && $rel < $size} {return [expr {$vaddr + $rel}]}
	}
	error "type binary: position $pos is outside the segments of space \"$name\" ($what)"
    }
    return [expr {$pos - $sp(base)}]
}

# --- type binary space ...
proc ::fb::spaceMake {data segs args} {
    variable spaces
    set o [dict create -base 0 -name ""]
    foreach {k v} $args {
	if {$k ni {-base -name}} {error "bad option \"$k\": must be -base or -name"}
	dict set o $k $v
    }
    set base [dict get $o -base]
    if {![string is integer -strict $base] || $base < 0} {
	error "bad base \"$base\": must be a non-negative integer"
    }
    set extra ""
    set name [dict get $o -name]
    if {$name ne ""} {
	if {![regexp {^[A-Za-z0-9_.:-]+$} $name]} {
	    error "bad space name \"$name\": letters, digits and _ - . : only"
	}
	set extra " name \$name"
    }
    eval "set {space}sp data \$data base \$base segments \$segs$extra"
    set nm $sp(name)
    if {[dict exists $spaces $nm]} {
	error "type binary: space \"$nm\" already exists (use \"type binary space delete $nm\")"
    }
    dict set spaces $nm $sp
    return $sp
}

proc ::fb::spaceFile {data args} {
    return [spaceMake $data {} {*}$args]
}

proc ::fb::spaceMemory {data segments args} {
    if {[llength $segments] == 0} {
	error "type binary space memory: -segments needs at least one {address offset size}"
    }
    foreach seg $segments {
	if {[llength $seg] != 3} {
	    error "bad segment \"$seg\": must be {address offset size}"
	}
	foreach x $seg {
	    if {![string is integer -strict $x] || $x < 0} {
		error "bad segment \"$seg\": address, offset and size are non-negative integers"
	    }
	}
    }
    return [spaceMake $data $segments {*}$args]
}

proc ::fb::spaceDelete {name} {
    variable spaces
    if {![dict exists $spaces $name]} {error "type binary space delete: unknown space \"$name\""}
    set spaces [dict remove $spaces $name]
    return
}

proc ::fb::spaceList {} {
    variable spaces
    return [lsort [dict keys $spaces]]
}

proc ::fb::spaceGet {name} {
    return [spaceLookup $name "type binary space get"]
}

namespace eval ::fb {
    namespace ensemble create -command ::fb::spaceCmd -map {
	file ::fb::spaceFile memory ::fb::spaceMemory delete ::fb::spaceDelete
	list ::fb::spaceList get ::fb::spaceGet
    }
}

# FR : position absolue de l'origine. starts = débuts des structures qui
#      englobent le champ (la plus proche en dernier), here = position du champ
#      lui-même, top = début des données ("//").
# EN : absolute position of the origin. starts = starts of the structures that
#      enclose the field (nearest last), here = the field's own position, top =
#      start of the data ("//").
proc ::fb::originPos {origin starts here top what} {
    switch -exact -- $origin {
	// {return $top}
	. {return $here}
	./ {
	    if {[llength $starts] == 0} {return $top}
	    return [lindex $starts end]
	}
    }
    set n [expr {[string length $origin] / 3}]
    set i [expr {[llength $starts] - 1 - $n}]
    if {$i < 0} {
	error "type binary: $what uses the origin $origin, but it is not nested that deep"
    }
    return [lindex $starts $i]
}


# FR : lit l'adresse d'un lien et en déduit la position de la cible. Rend un
#      dict : null (0|1), e (somme opt ou vide), d (repr de la cible), target
#      (position absolue), starts.
# EN : reads a link's address and works out where its target is. Returns a
#      dict: null (0|1), e (opt sum or empty), d (target repr), target
#      (absolute position), starts.
proc ::fb::linkRead {r sName ctx what} {
    upvar 1 $sName s
    lassign $r kind a b c d e f g
    set here $s(pos)
    set off [dec $b s [dict replace $ctx nullOk 0] "address of $what"]
    set info [dict create null 0 e $e d $d target 0 ref $f space "" starts [dict getdef $ctx starts {}]]
    if {$c ne "none" && $off == $c} {
	dict set info null 1
	return $info
    }
    set sp [originSpace $a]
    if {$sp ne ""} {
	# FR : l'adresse est lue dans CES données, la cible est dans le référentiel
	# EN : the address is read in THESE data, the target is in the space
	lassign $sp spName spKind
	dict set info space $spName
	dict set info target [spaceTarget $spName $spKind $off $what]
	return $info
    }
    set target [expr {[originPos $a [dict get $info starts] $here $s(base) $what] + $off}]
    if {$target < $s(base)} {
	error "type binary: the target of $what is at $target, before the start of the data"
    }
    dict set info target $target
    return $info
}

# FR : valeur d'un lien nul (none, ou la valeur nulle d'un champ à défaut)
# EN : value of a null link (none, or the null value of a field with a default)
proc ::fb::linkNull {info ctx what} {
    variable nullLink
    if {[dict get $info e] ne ""} {return [mkSum [dict get $info e] none ""]}
    if {![dict getdef $ctx nullOk 0]} {
	error "type binary: $what is a null link (give the field a default value, or make the target an opt(...))"
    }
    return $nullLink
}

# FR : vrai si le champ lien FNAME (qui a un défaut) vaut son défaut et que le
#      lien a une valeur nulle : on peut alors écrire le lien nul. Le défaut
#      est obtenu en construisant le produit sans ce champ.
# EN : true if the link field FNAME (which has a default) holds its default and
#      the link has a null value: the null link can then be written. The
#      default is obtained by building the product without that field.
proc ::fb::isDefaultLink {tname fields fname frepr x fval} {
    set frepr [unref $frepr]
    if {[lindex $frepr 0] ne "link"} {return 0}
    lassign $frepr kind a b c d e f g
    if {$c eq "none" || $e ne ""} {return 0}
    if {$f ne "" && [type get $fval] eq $f} {return 0}
    set cmd "set {$tname}dflt"
    set i 0
    foreach fl $fields {
	set n [lindex $fl 0]
	if {$n eq $fname} continue
	if {[lindex [unref [lindex $fl 1]] 0] eq "none" && [lindex $fl 2]} continue
	set ov$i $x($n)
	append cmd " [list $n] \$ov$i"
	incr i
    }
    if {[catch {eval $cmd} err]} {return 0}
    set dv $dflt($fname)
    return [expr {[string range $dv 0 end] eq [string range $fval 0 end]}]
}

# FR : va lire la cible, puis revient où l'on était
# EN : reads the target, then comes back to where we were
proc ::fb::linkFollow {info sName ctx what} {
    upvar 1 $sName s
    set target [dict get $info target]
    set depth [dict getdef $ctx depth -1]
    if {$depth == 0} {
	# FR : lien de frontière : on rend la position absolue de la cible,
	#      annotée par le type adresse du lien (-linkdepth).
	# EN : frontier link: the target's absolute position, annotated with the
	#      link's address type (-linkdepth).
	eval "set {[dict get $info ref]}__a $target"
	return $__a
    }
    if {$depth > 0} {dict set ctx depth [expr {$depth - 1}]}
    set chain [dict getdef $ctx chain {}]
    set spName [dict getdef $info space ""]
    set key [expr {$spName eq "" ? $target : "$spName@$target"}]
    if {$depth < 0 && $key in $chain} {
	error "type binary: link cycle at offset $target ($what)"
    }
    set saved [array get s]
    if {$spName ne ""} {
	# FR : la cible est dans le référentiel nommé : on y lit, puis on revient
	# EN : the target is in the named space: read there, then come back
	set sp [spaceLookup $spName $what]
	set s(kind) str
	set s(data) $sp(data)
	set s(base) $sp(base)
	set s(origin) 0
    }
    set s(pos) $target
    if {$s(kind) eq "chan"} {seek $s(chan) [expr {$s(origin) + $s(pos)}] start}
    set v [dec [dict get $info d] s [dict replace $ctx nullOk 0 starts {} chain [concat $chain $key]] $what]
    array unset s
    array set s $saved
    if {$s(kind) eq "chan"} {seek $s(chan) [expr {$s(origin) + $s(pos)}] start}
    if {[dict get $info e] ne ""} {return [mkSum [dict get $info e] some $v]}
    return $v
}

# FR : écrit les cibles des liens en attente, à la suite de out (alignées),
#      puis reporte leur offset dans le champ. Les cibles peuvent en créer
#      d'autres, qui sont traitées à leur tour.
# EN : writes the targets of pending links after out (aligned), then patches
#      their offset into the field. Targets may create more links, which are
#      handled in turn.
proc ::fb::flushLinks {outName ctx} {
    upvar 1 $outName out
    variable pend
    set wbase [dict getdef $ctx wbase 0]
    while {[llength $pend]} {
	set item [lindex $pend 0]
	set pend [lrange $pend 1 end]
	dict with item {}
	# patch, origin, repr, target, payload, what
	set len0 [string length $out]
	putPad out [alignOf $target $ctx]
	set tpos [expr {$wbase + [string length $out]}]
	set dup 0
	if {[dict getdef $ctx dedupe 0]} {
	    # FR : essai : la cible sans lien imbriqué ni sous-structure, déjà écrite
	    #      à l'identique, n'est pas réécrite : on réutilise son adresse
	    # EN : trial: a target without nested links or sub-structures already
	    #      written identically is not written again: its address is reused
	    variable seen
	    variable late
	    set np [llength $pend]
	    set nl [llength $late]
	    set tmp ""
	    enc $target $payload [dict replace $ctx wbase $tpos starts {} scope $scope] $what tmp
	    if {[llength $pend] == $np && [llength $late] == $nl} {
		set key [list $wbase [alignOf $target $ctx] $tmp]
		if {[dict exists $seen $key]} {
		    set tpos [dict get $seen $key]
		    set out [string range $out 0 [expr {$len0 - 1}]]
		    set dup 1
		} else {
		    dict set seen $key $tpos
		}
	    } else {
		set pend [lrange $pend 0 [expr {$np - 1}]]
		set late [lrange $late 0 [expr {$nl - 1}]]
	    }
	}
	if {!$dup} {
	    enc $target $payload [dict replace $ctx starts {} scope $scope] $what out
	}
	set rel [expr {$tpos - $origin}]
	if {$rel < 0 && [lindex $repr 0] eq "uint"} {
	    error "type binary: the target of $what is at $tpos, before its origin ($origin): use a signed address"
	}
	set bytes ""
	enc $repr $rel $ctx "address of $what" bytes
	set out [string range $out 0 [expr {$patch - 1}]]$bytes[string range $out [expr {$patch + [string length $bytes]}] end]
    }
}

# FR : les étiquettes se comparent comme nombres si possible, sinon comme texte
# EN : tags compare as numbers when possible, else as text
proc ::fb::tagEq {a b} {
    if {[string is integer -strict $a] && [string is integer -strict $b]} {
	return [expr {$a == $b}]
    }
    return [expr {$a eq $b}]
}

# FR : valeur d'une somme : {variante}contenu
# EN : sum value: {variant}payload
proc ::fb::mkSum {sumName variant payload} {
    eval "set {$sumName}__r {$variant}\$payload"
    return $__r
}

# ---------------------------------------------------------------------------
# FR : Sources d'octets (chaîne ou canal)   /   EN: Byte sources (string or channel)
# ---------------------------------------------------------------------------

# FR : s est un tableau : kind (str|chan), data, chan, pos (position), base
#      (position du début de la valeur, origine de l'alignement)
# EN : s is an array: kind (str|chan), data, chan, pos (position), base
#      (position of the start of the value, origin of the alignment)
proc ::fb::take {sName n what} {
    upvar 1 $sName s
    if {$s(kind) eq "str"} {
	set got [string range $s(data) $s(pos) [expr {$s(pos) + $n - 1}]]
    } else {
	set got [::read $s(chan) $n]
    }
    if {[string length $got] < $n} {
	error "type binary: data truncated at offset $s(pos) reading $what (needs $n bytes, got [string length $got])"
    }
    incr s(pos) $n
    return $got
}

# FR : lit jusqu'à un terminateur de w octets nuls (aligné sur w) et le consomme
# EN : reads up to a terminator of w NUL bytes (aligned on w) and consumes it
proc ::fb::takez {sName w what} {
    upvar 1 $sName s
    set zero [string repeat \x00 $w]
    if {$s(kind) eq "str"} {
	set i $s(pos)
	while 1 {
	    set j [string first $zero $s(data) $i]
	    if {$j < 0} {error "type binary: unterminated string at offset $s(pos) reading $what"}
	    if {($j - $s(pos)) % $w == 0} break
	    set i [expr {$j + 1}]
	}
	set got [string range $s(data) $s(pos) [expr {$j - 1}]]
	set s(pos) [expr {$j + $w}]
	return $got
    }
    set got ""
    while {1} {
	set c [::read $s(chan) $w]
	if {[string length $c] < $w} {error "type binary: unterminated string at offset $s(pos) reading $what"}
	incr s(pos) $w
	if {$c eq $zero} break
	append got $c
    }
    return $got
}

# ---------------------------------------------------------------------------
# FR : Décodage   /   EN: Decoding
# ---------------------------------------------------------------------------

# FR : retourne une valeur Tcl simple (ou une valeur typée pour un produit)
# EN : returns a plain Tcl value (or a typed value for a product)
proc ::fb::dec {r sName ctx what} {
    upvar 1 $sName s
    set r [unref $r]
    lassign $r kind a b c
    switch $kind {
	uint - sint {
	    return [getInt [take s $a $what] [expr {$kind eq "sint"}] $a \
		    [endianOf $b $ctx]]
	}
	bits {return [getInt [take s $a $what] 0 $a [endianOf $b $ctx]]}
	float {
	    binary scan [take s $a $what] [fmt float $a [endianOf $b $ctx]] x
	    return $x
	}
	lit {return [take s $a $what]}
	bytes {return [take s [readSize $a s $ctx $what] $what]}
	str {
	    set n [readSize $a s $ctx $what]
	    set text [encoding convertfrom $b [take s $n $what]]
	    if {[isFixed $a]} {set text [string trimright $text \x00]}
	    return $text
	}
	cstring {return [encoding convertfrom $a [takez s [nulWidth $a] $what]]}
	array {
	    set n [readSize $a s $ctx $what]
	    set v {}
	    for {set i 0} {$i < $n} {incr i} {
		lappend v [dec $b s [dict replace $ctx nullOk 0] "$what\[$i\]"]
	    }
	    return $v
	}
	at {
	    # FR : la valeur est ailleurs ; le curseur séquentiel ne bouge pas
	    # EN : the value is elsewhere; the sequential cursor does not move
	    set off [evalExpr $a [dict get $ctx scope] $what]
	    if {![string is integer -strict $off] || $off < 0} {
		error "type binary: position of $what is \"$off\""
	    }
	    set save $s(pos)
	    set s(pos) [expr {$s(base) + $off}]
	    if {$s(kind) eq "chan"} {seek $s(chan) [expr {$s(origin) + $s(pos)}] start}
	    set v [dec $b s $ctx $what]
	    set s(pos) $save
	    if {$s(kind) eq "chan"} {seek $s(chan) [expr {$s(origin) + $s(pos)}] start}
	    return $v
	}
	link {
	    set info [linkRead $r s $ctx $what]
	    if {[dict get $info null]} {return [linkNull $info $ctx $what]}
	    return [linkFollow $info s $ctx $what]
	}
	tagged {
	    # a = champ étiquette, b = variantes {nom repr}, c = nom du type somme
	    set tag [tagValue $a $ctx $what read]
	    foreach vr $b {
		lassign $vr vname vrepr
		if {[tagEq $tag $vname]} {
		    return [mkSum $c $vname [dec $vrepr s $ctx "variant $vname of $what"]]
		}
	    }
	    error "type binary: tag $tag of $what matches no variant of $c"
	}
	product {
	    # a = nom du type, b = champs {nom repr avecdefaut}
	    set scope [dict get $ctx scope]
	    set cmd "set {$a}V"
	    set i 0
	    set starts [dict getdef $ctx starts {}]
	    set mine 0
	    set deferred {}
	    foreach f $b {
		lassign $f fname frepr hasdef
		set frepr [unref $frepr]
		if {$frepr eq "none"} {
		    if {$hasdef} continue
		    error "type binary: field $fname of $a has no binary representation and no default value"
		}
		skipPad s [alignOf $frepr $ctx] $ctx "field $fname of $a"
		if {!$mine} {
		    # FR : le début de la structure = son premier champ, après remplissage
		    # EN : the structure starts at its first field, after padding
		    lappend starts $s(pos)
		    set mine 1
		}
		set cx [dict replace $ctx scope $scope starts $starts nullOk $hasdef]
		if {[lindex $frepr 0] eq "link"} {
		    # FR : la cible d'un lien est lue APRÈS tous les champs du produit,
		    #      pour que ses tailles puissent citer des champs qui suivent
		    #      (ELF : e_shoff avant e_shnum).
		    # EN : a link's target is read AFTER all the product's fields, so
		    #      its sizes can cite fields that follow (ELF: e_shoff before
		    #      e_shnum).
		    set info [linkRead $frepr s $cx "field $fname of $a"]
		    if {[dict get $info null]} {
			set fv$i [linkNull $info $cx "field $fname of $a"]
		    } else {
			set fv$i ""
			lappend deferred $i $fname $info
		    }
		} else {
		    set fv$i [dec $frepr s $cx "field $fname of $a"]
		}
		variable nullLink
		if {[set fv$i] eq $nullLink} {
		    # FR : lien nul : le champ prend sa valeur par défaut
		    # EN : null link: the field takes its default value
		    continue
		}
		dict set scope $fname [set fv$i]
		# FR : par variable, pour garder le type des sommes et des produits
		# EN : by variable, to keep the type of sums and products
		append cmd " " [list $fname] " \$fv$i"
		incr i
	    }
	    # FR : les cibles des liens, maintenant que tous les champs sont lus
	    # EN : the links' targets, now that all the fields have been read
	    foreach {k fname info} $deferred {
		set fv$k [linkFollow $info s [dict replace $ctx scope $scope starts $starts nullOk 0] "field $fname of $a"]
	    }
	    skipPad s [alignOf $r $ctx] $ctx "the end of $a"
	    # FR : le nom du type peut contenir des parenthèses : commande en texte
	    # EN : the type name may contain parentheses: the command is built as text
	    eval $cmd
	    return $V
	}
    }
    error "type binary: unsupported representation \"$r\""
}

# ---------------------------------------------------------------------------
# FR : Encodage (ajoute à la variable out de l'appelant)
# EN : Encoding (appends to the caller's variable out)
# ---------------------------------------------------------------------------

proc ::fb::enc {r value ctx what outName} {
    upvar 1 $outName out
    set r [unref $r]
    lassign $r kind a b c
    switch $kind {
	uint - sint - bits {append out [putInt $value $a [endianOf $b $ctx]]}
	float {append out [binary format [fmt float $a [endianOf $b $ctx]] $value]}
	lit {append out [string range $value 0 end]}
	bytes {
	    # FR : copie simple : une valeur typée ajoutée à une variable la
	    #      rendrait typée (une somme, ici) et bloquerait l'append suivant.
	    # EN : plain copy: a typed value appended to a variable would make it
	    #      typed (a sum, here) and block the next append.
	    set bs [string range $value 0 end]
	    if {[isFixed $a]} {
		append out $bs
	    } else {
		writeSize $a [string length $bs] $ctx $what out
		append out $bs
	    }
	}
	str {
	    set bs [encoding convertto $b $value]
	    if {[isFixed $a]} {
		append out $bs [string repeat \x00 [expr {$a - [string length $bs]}]]
	    } else {
		writeSize $a [string length $bs] $ctx $what out
		append out $bs
	    }
	}
	cstring {
	    append out [encoding convertto $a $value] \
		    [string repeat \x00 [nulWidth $a]]
	}
	array {
	    if {![isFixed $a]} {writeSize $a [llength $value] $ctx $what out}
	    set i 0
	    foreach e $value {
		enc $b $e $ctx "$what\[$i\]" out
		incr i
	    }
	}
	at {
	    # FR : encodé à part, placé à la fin à la position demandée
	    # EN : encoded apart, placed at the end at the requested position
	    variable late
	    set off [evalExpr $a [dict get $ctx scope] $what]
	    set sub ""
	    variable pend
	    set savedPend $pend
	    set pend {}
	    set subctx [dict replace $ctx wbase $off starts {}]
	    enc $b $value $subctx $what sub
	    flushLinks sub $subctx
	    set pend $savedPend
	    lappend late $off $sub
	}
	link {
	    lassign $r kind a b c d e f g
	    set wbase [dict getdef $ctx wbase 0]
	    if {$f ne "" && [type get $value] eq $f} {
		# FR : lien de frontière : on écrit l'adresse, sans la cible
		# EN : frontier link: write the address, not the target
		set here [expr {$wbase + [string length $out]}]
		set sp [originSpace $a]
		if {$sp ne ""} {
		    set rel [spaceAddress [lindex $sp 0] [lindex $sp 1] [string range $value 0 end] $what]
		} else {
		    set origin [originPos $a [dict getdef $ctx starts {}] $here 0 $what]
		    set rel [expr {[string range $value 0 end] - $origin}]
		    if {$rel < 0 && [lindex $b 0] eq "uint"} {
			error "type binary: the target of $what is at $value, before its origin ($origin): use a signed address"
		    }
		}
		enc $b $rel $ctx "address of $what" out
		return
	    }
	    if {[originSpace $a] ne ""} {
		error "type binary: cannot write the target of $what into the space [lindex [originSpace $a] 0]: leave the link unfollowed (-linkdepth 0) to rewrite its address"
	    }
	    if {$e ne ""} {
		set tg [type get $value]
		set vname [string range $tg [expr {[string last . $tg] + 1}] end]
		if {$vname eq "none"} {
		    enc $b $c $ctx "address of $what" out
		    return
		}
		if {$vname ne "some"} {
		    error "type binary: $what is a $tg, not an opt value (some or none)"
		}
		set payload $value(some)
	    } else {
		set payload $value
	    }
	    set patch [string length $out]
	    set here [expr {$wbase + $patch}]
	    set origin [originPos $a [dict getdef $ctx starts {}] $here 0 $what]
	    enc $b 0 $ctx "address of $what" out
	    variable pend
	    lappend pend [dict create patch $patch origin $origin repr $b \
		    target $d payload $payload what $what \
		    scope [dict get $ctx scope] fin 0]
	}
	tagged {
	    set tag [tagValue $a $ctx $what written]
	    set tg [type get $value]
	    set prefix "$c."
	    if {[string first $prefix $tg] != 0} {
		error "type binary: $what is a $tg, not a $c"
	    }
	    set vname [string range $tg [string length $prefix] end]
	    if {![tagEq $tag $vname]} {
		error "type binary: selector $a is $tag but $what holds variant $vname of $c"
	    }
	    foreach vr $b {
		lassign $vr n vrepr
		if {$n eq $vname} {
		    set payload $value($vname)
		    enc $vrepr $payload $ctx "variant $vname of $what" out
		    return
		}
	    }
	    error "type binary: $c has no variant $vname"
	}
	product {
	    set scope [dict get $ctx scope]
	    set starts [dict getdef $ctx starts {}]
	    set mine 0
	    variable pend
	    set n0 [llength $pend]
	    foreach f $b {
		lassign $f fname frepr hasdef
		set frepr [unref $frepr]
		if {$frepr eq "none"} {
		    if {$hasdef} continue
		    error "type binary: field $fname of $a has no binary representation and no default value"
		}
		putPad out [alignOf $frepr $ctx]
		if {!$mine} {
		    lappend starts [expr {[dict getdef $ctx wbase 0] + [string length $out]}]
		    set mine 1
		}
		set x $value
		unset -nocomplain fval
		set fval $x($fname)
		if {$hasdef && [isDefaultLink $a $b $fname $frepr $x $fval]} {
		    # FR : la valeur est la valeur par défaut : on écrit le lien nul
		    # EN : the value is the default value: write the null link
		    enc [lindex $frepr 2] [lindex $frepr 3] [dict replace $ctx scope $scope starts $starts] "address of field $fname of $a" out
		    dict set scope $fname $fval
		    continue
		}
		enc $frepr $fval [dict replace $ctx scope $scope starts $starts] "field $fname of $a" out
		dict set scope $fname $fval
	    }
	    # FR : les cibles des liens voient TOUS les champs du produit (ELF : une
	    #      taille qui cite un champ situé après l'adresse)
	    # EN : the links' targets see ALL the product's fields (ELF: a size that
	    #      cites a field located after the address)
	    set full $scope
	    for {set k $n0} {$k < [llength $pend]} {incr k} {
		set it [lindex $pend $k]
		if {![dict get $it fin]} {
		    dict set it scope $full
		    dict set it fin 1
		    lset pend $k $it
		}
	    }
	    putPad out [alignOf $r $ctx]
	}
	default {error "type binary: unsupported representation \"$r\""}
    }
}

# FR : taille en octets à partir de off (avec remplissage), ou erreur si elle
#      est variable
# EN : size in bytes starting at off (with padding), or an error if variable
proc ::fb::sizeAt {r off ctx} {
    set r [unref $r]
    lassign $r kind a b c
    switch $kind {
	uint - sint - float - bits - lit {return [expr {$off + $a}]}
	bytes - str {
	    if {![isFixed $a]} {error "type binary sizeof?: the size depends on the data ($kind)"}
	    return [expr {$off + $a}]
	}
	cstring {error "type binary sizeof?: variable size (string(...,z))"}
	tagged {
	    # FR : sélecteur constant (=nom, paramètre de type) : taille connue
	    # EN : constant selector (=name, type parameter): size is known
	    if {[string index $a 0] eq "="} {
		set tag [string range $a 1 end]
		foreach vr $b {
		    lassign $vr vname vrepr
		    if {[tagEq $tag $vname]} {return [sizeAt $vrepr $off $ctx]}
		}
		error "type binary: tag $tag matches no variant of $c"
	    }
	    error "type binary sizeof?: the size depends on the data ($kind)"
	}
	at {error "type binary sizeof?: the size depends on the data ($kind)"}
	link {
	    # FR : la taille EN PLACE : celle de l'adresse ; la cible est ailleurs
	    # EN : the size IN PLACE: that of the address; the target is elsewhere
	    lassign $r kind a b
	    incr off [padFor $off [alignOf $b $ctx]]
	    return [sizeAt $b $off $ctx]
	}
	array {
	    if {![isFixed $a]} {error "type binary sizeof?: the element count depends on the data"}
	    for {set i 0} {$i < $a} {incr i} {set off [sizeAt $b $off $ctx]}
	    return $off
	}
	product {
	    foreach f $b {
		lassign $f fname frepr hasdef
		set frepr [unref $frepr]
		if {$frepr eq "none"} {
		    if {$hasdef} continue
		    error "type binary: field $fname of $a has no binary representation and no default value"
		}
		incr off [padFor $off [alignOf $frepr $ctx]]
		set off [sizeAt $frepr $off $ctx]
	    }
	    incr off [padFor $off [alignOf $r $ctx]]
	    return $off
	}
    }
}

# ---------------------------------------------------------------------------
# FR : Les commandes « type binary ... » et « chan binary ... »   /   EN: The "type binary ..." and "chan binary ..." commands
# ---------------------------------------------------------------------------

proc ::fb::options {argv known} {
    set opts [dict create -offset 0 -endian little -consumed "" \
	    -align packed -strictpad 0 -linkdepth Inf -dedupe 0]
    foreach {k v} $argv {
	if {$k ni $known} {error "bad option \"$k\": must be [join $known {, }]"}
	dict set opts $k $v
    }
    if {[dict get $opts -endian] ni {little big}} {
	error "bad endianness \"[dict get $opts -endian]\": must be little or big"
    }
    set a [dict get $opts -align]
    switch -- $a {
	packed {set cap 1}
	natural {set cap 8}
	default {
	    if {![string is integer -strict $a] || $a < 1 || ($a & ($a - 1))} {
		error "bad alignment \"$a\": must be packed, natural or a power of two"
	    }
	    set cap $a
	}
    }
    set ld [dict get $opts -linkdepth]
    if {$ld eq "Inf"} {
	set ld -1
    } elseif {![string is integer -strict $ld] || $ld < 0} {
	error "bad link depth \"$ld\": must be a non-negative integer or Inf"
    }
    dict set opts ctx [dict create endian [dict get $opts -endian] depth $ld \
	    align $cap scope {} strict [string is true -strict [dict get $opts -strictpad]] \
	    dedupe [string is true -strict [dict get $opts -dedupe]]]
    return $opts
}

# FR : le résultat d'un type non produit est lui aussi une valeur typée
# EN : the result of a non-product type is a typed value too
proc ::fb::typed {type v} {
    if {[lindex [repr $type] 0] eq "product"} {return $v}
    eval "set {$type}V [list $v]"
    return $V
}

proc ::fb::decode {type data args} {
    set o [options $args {-offset -endian -consumed -align -strictpad -linkdepth}]
    set off [dict get $o -offset]
    array set s [list kind str data $data pos $off base $off origin 0]
    set v [dec [repr $type] s [dict get $o ctx] "$type"]
    if {[dict get $o -consumed] ne ""} {
	upvar 1 [dict get $o -consumed] consumed
	set consumed $s(pos)
    }
    return [typed $type $v]
}

# FR : suit un lien de frontière (valeur rendue par decode -linkdepth N) :
#      décode sa cible dans DATA. Par défaut -linkdepth 0 : un seul pas.
# EN : follows a frontier link (a value returned by decode -linkdepth N):
#      decodes its target in DATA. Default -linkdepth 0: a single step.
proc ::fb::deref {value args} {
    if {[llength $args] % 2} {
	set data [lindex $args 0]
	set args [lrange $args 1 end]
    } else {
	set data ""
	set noData 1
    }
    set tp [type get $value]
    if {[string index $tp 0] ne "&"} {
	error "type binary deref: \"$value\" is not a link left by -linkdepth (its type is \"$tp\")"
    }
    set o [options [concat {-linkdepth 0} $args] {-offset -endian -align -strictpad -linkdepth}]
    set r [repr [string range $tp 1 end]]
    lassign $r kind a b c d e f g
    set off [dict get $o -offset]
    set sp [originSpace $a]
    if {$sp ne ""} {
	# FR : lien nommé : les octets viennent du référentiel, pas de DATA
	# EN : named link: the bytes come from the space, no DATA needed
	set spv [spaceLookup [lindex $sp 0] "link $tp"]
	array set s [list kind str data $spv(data) pos [string range $value 0 end] base $spv(base) origin 0]
    } else {
	if {[info exists noData]} {
	    error "type binary deref: the link \"$tp\" is anonymous: DATA is required"
	}
	array set s [list kind str data $data pos [string range $value 0 end] base $off origin 0]
    }
    set v [dec $d s [dict get $o ctx] "link $tp"]
    if {$e ne ""} {return [mkSum $e some $v]}
    return [typed $g $v]
}

proc ::fb::encode {type value args} {
    set o [options $args {-endian -align -dedupe}]
    return [encodeValue $type $value [dict get $o ctx]]
}

# FR : encode, puis place les valeurs at(...) à leur position
# EN : encode, then place the at(...) values at their position
proc ::fb::encodeValue {type value ctx} {
    variable late
    variable pend
    variable seen
    set late {}
    set pend {}
    set seen {}
    set out ""
    enc [repr $type] $value [dict replace $ctx wbase 0 starts {}] $type out
    flushLinks out $ctx
    foreach {off sub} $late {
	if {$off > [string length $out]} {
	    append out [string repeat \x00 [expr {$off - [string length $out]}]]
	}
	set out [string range $out 0 [expr {$off - 1}]]$sub[string range $out [expr {$off + [string length $sub]}] end]
    }
    set late {}
    return $out
}

# FR : le canal est mis en mode binaire le temps de l'appel, puis rétabli
# EN : the channel is put in binary mode for the call, then restored
proc ::fb::withBinary {chan script} {
    set tr [chan configure $chan -translation]
    set en [chan configure $chan -encoding]
    chan configure $chan -translation binary
    try {
	uplevel 1 $script
    } finally {
	chan configure $chan -translation $tr -encoding $en
    }
}

proc ::fb::readChan {chan type args} {
    set o [options $args {-endian -align -strictpad -linkdepth}]
    set r [repr $type]
    array set s [list kind chan chan $chan pos 0 base 0 origin [tell $chan]]
    withBinary $chan {set v [dec $r s [dict get $o ctx] $type]}
    return [typed $type $v]
}

proc ::fb::writeChan {chan type value args} {
    set o [options $args {-endian -align -dedupe}]
    set bytes [encodeValue $type $value [dict get $o ctx]]
    withBinary $chan {puts -nonewline $chan $bytes}
    return [string length $bytes]
}

proc ::fb::sizeof {type args} {
    set o [options $args {-align}]
    return [sizeAt [repr $type] 0 [dict get $o ctx]]
}

# FR : DATA est-elle valide pour TYPE ? 1 ou 0, sans produire la valeur.
#      Par défaut un préfixe valide suffit (on peut reconnaître un en-tête) ;
#      -exact 1 exige en plus que DATA soit entièrement consommée. Une option
#      invalide ou un type sans représentation binaire reste une erreur : seul
#      l'échec du décodage donne 0.
#      Appelée par « type binary {is?}TYPE DATA ».
# EN : is DATA valid for TYPE? 1 or 0, without producing the value. By
#      default a valid prefix is enough (to recognise a header); -exact 1 also
#      requires DATA to be fully consumed. A bad option or a type without binary
#      representation is still an error: only a decoding failure gives 0.
#      Called by "type binary {is?}TYPE DATA".
proc ::fb::isvalid {type data args} {
    set exact 0
    set rest {}
    foreach {k v} $args {
	if {$k eq "-exact"} {
	    set exact [string is true -strict $v]
	} else {
	    lappend rest $k $v
	}
    }
    if {[llength $args] % 2} {
	error "wrong # args: options must be -name value pairs"
    }
    set o [options $rest {-offset -endian -align -strictpad -linkdepth}]
    repr $type
    if {[catch {decode $type $data {*}$rest -consumed used}]} {
	return 0
    }
    if {$exact && $used != [string length $data]} {
	return 0
    }
    return 1
}

# FR : « type binary » (octets en mémoire : decode encode deref space, plus les
#      interrogations {what?} {sizeof?} {is?} traitées par le noyau) et
#      « chan binary » (read write sur un canal). L'ancien « file binary »
#      garde toute la table, par compatibilité.
# EN : "type binary" (bytes in memory: decode encode deref space, plus the
#      {what?} {sizeof?} {is?} questions handled by the core) and "chan binary"
#      (read write on a channel). The former "file binary" keeps the whole
#      table, for compatibility.
namespace eval ::fb {
    namespace ensemble create -command ::fb::filebinary -map {
	read ::fb::readChan write ::fb::writeChan decode ::fb::decode
	encode ::fb::encode sizeof ::fb::sizeof deref ::fb::deref
	space ::fb::spaceCmd
    }
    namespace ensemble create -command ::fb::chanbinary -map {
	read ::fb::readChan write ::fb::writeChan
    }
}
# FR : « type binary » est une commande du noyau : elle passe decode, encode,
#      deref et space à ::fb::filebinary (voir TypeBinaryCmd).
# EN : "type binary" is a core command: it hands decode, encode, deref and
#      space to ::fb::filebinary (see TypeBinaryCmd).
namespace ensemble configure chan -map \
	[dict merge [namespace ensemble configure chan -map] {binary ::fb::chanbinary}]
namespace ensemble configure file -map \
	[dict merge [namespace ensemble configure file -map] {binary ::fb::filebinary}]

package provide filebinary 0.8
