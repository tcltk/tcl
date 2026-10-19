# convertir_images.tcl --
#
# Conversion PNG <-> GIF en passant par un raster typé commun.
#
#   octets PNG --png_lire--> {png} --png_vers_raster--> {raster}
#   {raster} --raster_vers_gif--> {gif} --gif_ecrire--> octets GIF   (et inverse)
#
# Chaque format est décrit par un type somme (un bloc = une variante) ; le
# type de chaque bloc dit ce que contient ce qui suit, comme l'en-tête d'un
# rouleau (prôtokollon). Limites assumées : PNG 8 bits (gris, RVB, RVBA ; les
# images à palette ne sont pas lues), GIF à une seule image, 256 couleurs.

# =========================== types communs ==================================
type def raster {{int}width; {int}height; {list.string}pixels}   ;# pixels "rrggbb"

# =========================== PNG =============================================
type def ihdr {
    {int}width; {int}height; {int}depth; {int}colortype;
    {int}compression = 0; {int}filter = 0; {int}interlace = 0
}
type def texte   {{string}keyword; {string}text}
type def donnees {{string}hex}
type def autre   {{string}name; {string}hex}
type def bloc {{ihdr}IHDR, {texte}tEXt, {donnees}IDAT, {string}IEND, {autre}inconnu}
type def png  {{string}signature = "\x89PNG\r\n\x1a\n"; {list.bloc}chunks}

proc png_bloc {nom donnees} {
    set corps $nom$donnees
    binary format Ia*Iu [string length $donnees] $corps [zlib crc32 $corps]
}

proc png_lire {octets} {
    binary scan $octets a8 sig
    set pos 8
    set {list.bloc}blocs
    while {$pos < [string length $octets]} {
	binary scan $octets @${pos}Iua4 len nom
	set data [string range $octets [expr {$pos+8}] [expr {$pos+7+$len}]]
	switch -- $nom {
	    IHDR {
		binary scan $data IuIucucucucucu w h dep ct comp fil inter
		set {ihdr}e {width}$w {height}$h {depth}$dep {colortype}$ct {compression}$comp {filter}$fil {interlace}$inter
		lappend blocs [set {bloc}b {IHDR}$e]
	    }
	    tEXt {
		lassign [split $data \x00] mot txt
		set {texte}t {keyword}$mot {text}$txt
		lappend blocs [set {bloc}b {tEXt}$t]
	    }
	    IDAT {
		set {donnees}d {hex}[binary encode hex $data]
		lappend blocs [set {bloc}b {IDAT}$d]
	    }
	    IEND {
		lappend blocs [set {bloc}b {IEND}{}]
	    }
	    default {
		set {autre}a {name}$nom {hex}[binary encode hex $data]
		lappend blocs [set {bloc}b {inconnu}$a]
	    }
	}
	incr pos [expr {12 + $len}]
    }
    set {png}p {signature}$sig {chunks}$blocs
    return $p
}

proc png_ecrire {p} {
    set out $p(signature)
    foreach b $p(chunks) {
	switch -- [type get $b] {
	    bloc.IHDR {
		append out [png_bloc IHDR [binary format IuIuc5 $b(IHDR.width) $b(IHDR.height) \
		    [list $b(IHDR.depth) $b(IHDR.colortype) $b(IHDR.compression) \
			 $b(IHDR.filter) $b(IHDR.interlace)]]]
	    }
	    bloc.tEXt    {append out [png_bloc tEXt $b(tEXt.keyword)\x00$b(tEXt.text)]}
	    bloc.IDAT    {append out [png_bloc IDAT [binary decode hex $b(IDAT.hex)]]}
	    bloc.IEND    {append out [png_bloc IEND ""]}
	    bloc.inconnu {append out [png_bloc $b(inconnu.name) [binary decode hex $b(inconnu.hex)]]}
	}
    }
    return $out
}

proc paeth {a b c} {
    set p [expr {$a + $b - $c}]
    set pa [expr {abs($p - $a)}]; set pb [expr {abs($p - $b)}]; set pc [expr {abs($p - $c)}]
    if {$pa <= $pb && $pa <= $pc} {return $a}
    if {$pb <= $pc} {return $b}
    return $c
}

proc png_vers_raster {p} {
    set idat ""
    foreach b $p(chunks) {
	switch -- [type get $b] {
	    bloc.IHDR {
		set w $b(IHDR.width); set h $b(IHDR.height)
		set ct $b(IHDR.colortype)
		if {$b(IHDR.depth) != 8 || $ct ni {0 2 6} || $b(IHDR.interlace) != 0} {
		    error "PNG non géré (8 bits, gris/RVB/RVBA, sans entrelacement)"
		}
	    }
	    bloc.IDAT {append idat [binary decode hex $b(IDAT.hex)]}
	}
    }
    set bpp [dict get {0 1 2 3 6 4} $ct]
    set brut [zlib decompress $idat]
    set stride [expr {$w * $bpp}]
    set prev [lrepeat $stride 0]
    set pixels {}
    for {set y 0} {$y < $h} {incr y} {
	set off [expr {$y * ($stride + 1)}]
	binary scan $brut @${off}cu ft
	binary scan $brut @[expr {$off + 1}]cu$stride ligne
	set cur {}
	for {set i 0} {$i < $stride} {incr i} {
	    set x [lindex $ligne $i]
	    set a [expr {$i >= $bpp ? [lindex $cur [expr {$i - $bpp}]] : 0}]
	    set up [lindex $prev $i]
	    set c [expr {$i >= $bpp ? [lindex $prev [expr {$i - $bpp}]] : 0}]
	    switch -- $ft {
		0 {set v $x}
		1 {set v [expr {$x + $a}]}
		2 {set v [expr {$x + $up}]}
		3 {set v [expr {$x + (($a + $up) >> 1)}]}
		4 {set v [expr {$x + [paeth $a $up $c]}]}
		default {error "filtre PNG inconnu : $ft"}
	    }
	    lappend cur [expr {$v & 255}]
	}
	set prev $cur
	for {set i 0} {$i < $stride} {incr i $bpp} {
	    switch -- $ct {
		0 {set g [format %02x [lindex $cur $i]]; lappend pixels $g$g$g}
		default {lappend pixels [format %02x%02x%02x {*}[lrange $cur $i [expr {$i + 2}]]]}
	    }
	}
    }
    set {raster}r {width}$w {height}$h {pixels}$pixels
    return $r
}

proc raster_vers_png {r} {
    set w $r(width); set h $r(height)
    set brut ""
    for {set y 0} {$y < $h} {incr y} {
	append brut \x00
	for {set x 0} {$x < $w} {incr x} {
	    append brut [binary decode hex [lindex $r(pixels) [expr {$y * $w + $x}]]]
	}
    }
    set {ihdr}e {width}$w {height}$h {depth}8 {colortype}2
    set {donnees}d {hex}[binary encode hex [zlib compress $brut]]
    set {list.bloc}blocs
    lappend blocs [set {bloc}b1 {IHDR}$e]
    lappend blocs [set {bloc}b2 {IDAT}$d]
    lappend blocs [set {bloc}b3 {IEND}{}]
    set {png}p(signature,chunks) "\x89PNG\r\n\x1a\n" $blocs;# (2 valeurs = positionnel : on nomme tout)
    return $p
}

# =========================== GIF =============================================
type def ecran {{int}width; {int}height; {int}flags; {int}background; {int}aspect = 0}
type def commentaire {{string}text}
type def controle    {{int}delay; {int}transparent; {int}disposal}
type def application {{string}id; {string}hex}
type def cadre {
    {int}left; {int}top; {int}width; {int}height; {int}flags;
    {int}mincode; {string}lzw
}
type def extinconnue {{int}label; {string}hex}
type def gblock {
    {commentaire}comment, {controle}control, {application}app,
    {cadre}frame, {extinconnue}extension, {string}trailer
}
type def gif {
    {string}signature = "GIF89a"; {ecran}screen; {string}palette = "";
    {list.gblock}blocks
}

proc gif_sous_blocs_lire {octets posVar} {
    upvar 1 $posVar pos
    set data ""
    while {1} {
	binary scan $octets @${pos}cu n
	incr pos
	if {$n == 0} break
	append data [string range $octets $pos [expr {$pos + $n - 1}]]
	incr pos $n
    }
    return $data
}

proc gif_sous_blocs_ecrire {data} {
    set out ""
    set n [string length $data]
    for {set i 0} {$i < $n} {incr i 255} {
	set morceau [string range $data $i [expr {$i + 254}]]
	append out [binary format c [string length $morceau]]$morceau
    }
    return $out\x00
}

proc gif_lire {octets} {
    binary scan $octets a6 sig
    binary scan $octets @6susucucucu w h fl bg asp
    set {ecran}e {width}$w {height}$h {flags}$fl {background}$bg {aspect}$asp
    set pos 13
    set palette ""
    if {$fl & 0x80} {
	set taille [expr {3 * (1 << (($fl & 7) + 1))}]
	binary scan [string range $octets $pos [expr {$pos+$taille-1}]] H* palette
	incr pos $taille
    }
    set {list.gblock}blocs
    while {$pos < [string length $octets]} {
	binary scan $octets @${pos}cu intro
	incr pos
	switch -- $intro {
	    33 {
		binary scan $octets @${pos}cu label
		incr pos
		switch -- $label {
		    249 {
			set d [gif_sous_blocs_lire $octets pos]
			binary scan $d cusucu pk delai transp
			set {controle}c {delay}$delai {transparent}[expr {($pk & 1) ? $transp : -1}] {disposal}[expr {($pk >> 2) & 7}]
			lappend blocs [set {gblock}b {control}$c]
		    }
		    254 {
			set {commentaire}c {text}[gif_sous_blocs_lire $octets pos]
			lappend blocs [set {gblock}b {comment}$c]
		    }
		    255 {
			set d [gif_sous_blocs_lire $octets pos]
			set {application}a {id}[string range $d 0 10] {hex}[binary encode hex [string range $d 11 end]]
			lappend blocs [set {gblock}b {app}$a]
		    }
		    default {
			set {extinconnue}x {label}$label {hex}[binary encode hex [gif_sous_blocs_lire $octets pos]]
			lappend blocs [set {gblock}b {extension}$x]
		    }
		}
	    }
	    44 {
		binary scan $octets @${pos}susususucu l t iw ih f
		incr pos 9
		binary scan $octets @${pos}cu mincode
		incr pos
		set {cadre}i {left}$l {top}$t {width}$iw {height}$ih {flags}$f {mincode}$mincode {lzw}[binary encode hex [gif_sous_blocs_lire $octets pos]]
		lappend blocs [set {gblock}b {frame}$i]
	    }
	    59 {lappend blocs [set {gblock}b {trailer}{}]}
	    default {error "octet de bloc GIF inattendu : $intro"}
	}
    }
    set {gif}g {signature}$sig {screen}$e {palette}$palette {blocks}$blocs
    return $g
}

proc gif_ecrire {g} {
    set out $g(signature)
    append out [binary format susucucucu $g(screen.width) $g(screen.height) \
	$g(screen.flags) $g(screen.background) $g(screen.aspect)]
    append out [binary decode hex $g(palette)]
    foreach b $g(blocks) {
	switch -- [type get $b] {
	    gblock.comment {append out \x21\xfe[gif_sous_blocs_ecrire $b(comment.text)]}
	    gblock.control {
		set t $b(control.transparent)
		set pk [expr {($b(control.disposal) << 2) | ($t >= 0)}]
		append out \x21\xf9[gif_sous_blocs_ecrire \
		    [binary format cusucu $pk $b(control.delay) [expr {max($t, 0)}]]]
	    }
	    gblock.app {
		append out \x21\xff[gif_sous_blocs_ecrire \
		    $b(app.id)[binary decode hex $b(app.hex)]]
	    }
	    gblock.frame {
		append out \x2c[binary format susususucuc $b(frame.left) $b(frame.top) \
		    $b(frame.width) $b(frame.height) $b(frame.flags) $b(frame.mincode)]
		append out [gif_sous_blocs_ecrire [binary decode hex $b(frame.lzw)]]
	    }
	    gblock.extension {
		append out \x21[binary format cu $b(extension.label)]
		append out [gif_sous_blocs_ecrire [binary decode hex $b(extension.hex)]]
	    }
	    gblock.trailer {append out \x3b}
	}
    }
    return $out
}

# --- compression LZW du GIF ----------------------------------------------------
proc lzw_compresser {indices mincode} {
    set clear [expr {1 << $mincode}]
    set eoi [expr {$clear + 1}]
    set size [expr {$mincode + 1}]
    set next [expr {$eoi + 1}]
    set acc 0; set n 0; set out {}
    set emit {
	set acc [expr {$acc | ($code << $n)}]
	incr n $size
	while {$n >= 8} {
	    lappend out [expr {$acc & 255}]
	    set acc [expr {$acc >> 8}]
	    incr n -8
	}
    }
    set code $clear; eval $emit
    set prefix [lindex $indices 0]
    foreach k [lrange $indices 1 end] {
	set key $prefix,$k
	if {[info exists tab($key)]} {
	    set prefix $tab($key)
	    continue
	}
	set code $prefix; eval $emit
	set tab($key) $next
	incr next
	if {$next > (1 << $size) && $size < 12} {incr size}
	if {$next >= 4096} {
	    set code $clear; eval $emit
	    unset tab
	    set size [expr {$mincode + 1}]
	    set next [expr {$eoi + 1}]
	}
	set prefix $k
    }
    set code $prefix; eval $emit
    set code $eoi; eval $emit
    if {$n > 0} {lappend out [expr {$acc & 255}]}
    return [binary format c* $out]
}

proc lzw_decompresser {octets mincode} {
    set clear [expr {1 << $mincode}]
    set eoi [expr {$clear + 1}]
    binary scan $octets cu* bytes
    set total [expr {8 * [llength $bytes]}]
    set acc 0; set n 0; set pos 0
    set size [expr {$mincode + 1}]
    set next [expr {$eoi + 1}]
    for {set i 0} {$i < $clear} {incr i} {set tab($i) [list $i]}
    set prev {}
    set out {}
    while {1} {
	while {$n < $size && $pos < [llength $bytes]} {
	    set acc [expr {$acc | ([lindex $bytes $pos] << $n)}]
	    incr pos; incr n 8
	}
	if {$n < $size} break
	set code [expr {$acc & ((1 << $size) - 1)}]
	set acc [expr {$acc >> $size}]
	incr n -$size
	if {$code == $clear} {
	    array unset tab
	    for {set i 0} {$i < $clear} {incr i} {set tab($i) [list $i]}
	    set size [expr {$mincode + 1}]
	    set next [expr {$eoi + 1}]
	    set prev {}
	    continue
	}
	if {$code == $eoi} break
	if {$prev eq {}} {
	    set entry $tab($code)
	} else {
	    if {[info exists tab($code)]} {
		set entry $tab($code)
	    } else {
		set entry [list {*}$prev [lindex $prev 0]]
	    }
	    set tab($next) [list {*}$prev [lindex $entry 0]]
	    incr next
	    if {$next == (1 << $size) && $size < 12} {incr size}
	}
	lappend out {*}$entry
	set prev $entry
    }
    return $out
}

proc gif_vers_raster {g} {
    set w $g(screen.width); set h $g(screen.height)
    set pal $g(palette)
    set couleurs [expr {[string length $pal] / 6}]
    foreach b $g(blocks) {
	if {[type get $b] eq "gblock.frame"} {
	    set mincode $b(frame.mincode)
	    set octets [binary decode hex $b(frame.lzw)]
	    set entrelace [expr {$b(frame.flags) & 0x40}]
	}
    }
    set indices [lzw_decompresser $octets $mincode]
    if {$entrelace} {
	# images entrelacées : les lignes arrivent par passes (0,8,.. 4,12,.. 2,6,.. 1,3,..)
	set ordre {}
	foreach {debut pas} {0 8 4 8 2 4 1 2} {
	    for {set y $debut} {$y < $h} {incr y $pas} {lappend ordre $y}
	}
	set rangees {}
	set k 0
	foreach y $ordre {
	    dict set rangees $y [lrange $indices [expr {$k * $w}] [expr {$k * $w + $w - 1}]]
	    incr k
	}
	set indices {}
	for {set y 0} {$y < $h} {incr y} {lappend indices {*}[dict get $rangees $y]}
    }
    set pixels {}
    foreach i $indices {
	lappend pixels [string range $pal [expr {6 * $i}] [expr {6 * $i + 5}]]
    }
    set {raster}r {width}$w {height}$h {pixels}$pixels
    return $r
}

proc raster_vers_gif {r} {
    set w $r(width); set h $r(height)
    set couleurs [lsort -unique $r(pixels)]
    if {[llength $couleurs] > 256} {error "trop de couleurs pour un GIF ([llength $couleurs])"}
    set bits 1
    while {(1 << $bits) < [llength $couleurs]} {incr bits}
    set palette ""
    set idx {}
    foreach c $couleurs {dict set idx $c [dict size $idx]; append palette $c}
    append palette [string repeat 000000 [expr {(1 << $bits) - [llength $couleurs]}]]
    set indices [lmap p $r(pixels) {dict get $idx $p}]
    set mincode [expr {max(2, $bits)}]
    set lzw [lzw_compresser $indices $mincode]
    set {ecran}e {width}$w {height}$h {flags}[expr {0x80 | ($bits - 1)}] {background}0
    set {cadre}f {left}0 {top}0 {width}$w {height}$h {flags}0 {mincode}$mincode {lzw}[binary encode hex $lzw]
    set {list.gblock}blocs
    lappend blocs [set {gblock}b1 {frame}$f]
    lappend blocs [set {gblock}b2 {trailer}{}]
    set {gif}g {screen}$e {palette}$palette {blocks}$blocs
    return $g
}

# --- les conversions demandées ------------------------------------------------
proc png_vers_gif {octets} {gif_ecrire [raster_vers_gif [png_vers_raster [png_lire $octets]]]}
proc gif_vers_png {octets} {png_ecrire [raster_vers_png [gif_vers_raster [gif_lire $octets]]]}

# =========================== démonstration ===================================
if {[file normalize $::argv0] eq [file normalize [info script]]} {
    # une image de 8x4 pixels : dégradé rouge/vert
    set pixels {}
    for {set y 0} {$y < 4} {incr y} {
	for {set x 0} {$x < 8} {incr x} {
	    lappend pixels [format %02x%02x40 [expr {$x * 36}] [expr {$y * 80}]]
	}
    }
    set {raster}depart {width}8 {height}4 {pixels}$pixels
    set png [png_ecrire [raster_vers_png $depart]]
    set gif [png_vers_gif $png]
    set png2 [gif_vers_png $gif]
    puts "PNG : [string length $png] octets, GIF : [string length $gif] octets, retour PNG : [string length $png2] octets"
    set a [png_vers_raster [png_lire $png]]
    set b [gif_vers_raster [gif_lire $gif]]
    set c [png_vers_raster [png_lire $png2]]
    puts "pixels identiques après PNG -> GIF -> PNG : [expr {$a(pixels) eq $b(pixels) && $b(pixels) eq $c(pixels)}]"
    set g [gif_lire $gif]
    puts "type du GIF produit : [type get $g], blocs : [lmap k $g(blocks) {type get $k}]"
    if {$argc} {
	set f [open [lindex $argv 0].png wb]; puts -nonewline $f $png; close $f
	set f [open [lindex $argv 0].gif wb]; puts -nonewline $f $gif; close $f
    }
}
