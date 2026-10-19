# png_somme.tcl --
#
# PNG avec un TYPE SOMME : chaque bloc est d'une variante différente selon
# son nom (IHDR, tEXt, IDAT, IEND, ou inconnu). Le type de chaque bloc dit
# ce que contient ce qui suit, comme l'en-tête d'un rouleau (prôtokollon).
# Le fichier est fabriqué ici même (image rouge de 4x2), relu, puis décrit.

# --- les types -------------------------------------------------------------
type def ihdr {
    {int}width; {int}height; {int}depth; {int}colortype;
    {int}compression = 0; {int}filter = 0; {int}interlace = 0
}
type def texte   {{string}keyword; {string}text}
type def donnees {{string}hex}
type def autre   {{string}name; {string}hex}
type def bloc {{ihdr}IHDR, {texte}tEXt, {donnees}IDAT, {string}IEND, {autre}inconnu}
type def png  {{string}signature = "\x89PNG\r\n\x1a\n"; {list.bloc}chunks}

# --- fabrication d'un petit PNG (aucun fichier externe) ----------------------
proc bloc_brut {nom donnees} {
    set corps $nom$donnees
    binary format Ia*Iu [string length $donnees] $corps [zlib crc32 $corps]
}
proc fabriquer_png {} {
    set w 4; set h 2
    set ligne \x00[string repeat \xff\x00\x00 $w]
    set brut [string repeat $ligne $h]
    return \x89PNG\r\n\x1a\n[bloc_brut IHDR [binary format IuIuc5 $w $h {8 2 0 0 0}]][bloc_brut tEXt "Comment\x00bonjour"][bloc_brut IDAT [zlib compress $brut]][bloc_brut IEND ""]
}

# --- lecture : octets -> blocs typés ----------------------------------------
proc lire_png {octets} {
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

# --- description : on aiguille sur le TYPE de chaque bloc --------------------
proc decrire {b} {
    switch -- [type get $b] {
	bloc.IHDR    {format "IHDR   image %d x %d, %d bits, type de couleur %d" \
			  $b(IHDR.width) $b(IHDR.height) $b(IHDR.depth) $b(IHDR.colortype)}
	bloc.tEXt    {format "tEXt   %s = %s" $b(tEXt.keyword) $b(tEXt.text)}
	bloc.IDAT    {format "IDAT   %d octets de pixels compressés" \
			  [expr {[string length $b(IDAT.hex)] / 2}]}
	bloc.IEND    {format "IEND   fin de l'image"}
	bloc.inconnu {format "????   bloc %s ignoré" $b(inconnu.name)}
    }
}

set image [lire_png [fabriquer_png]]
puts "type : [type get $image]"
foreach b $image(chunks) {
    puts "  [decrire $b]"
}

# --- ce que le système de types garantit --------------------------------------
puts "variantes : [lmap b $image(chunks) {type get $b}]"
if {[catch {set {bloc}x {IHDR}{4 2 8}} m]} {puts "IHDR incomplet : $m"}
if {[catch {set {bloc}y {inconnu}{seulement-un-champ}} m]} {puts "bloc inconnu mal formé : $m"}
