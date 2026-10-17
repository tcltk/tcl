# gif_somme.tcl --
#
# Même idée que png_somme.tcl, pour un GIF : l'en-tête (signature + écran
# logique), puis une suite de blocs dont la nature est donnée par leur
# premier octet (0x21 extension, 0x2C image, 0x3B fin). Chaque bloc est une
# variante d'un type somme ; le type dit ce que contient ce qui suit, comme
# l'en-tête d'un rouleau (prôtokollon).
# Le fichier est fabriqué ici même (1 pixel + un commentaire), relu, décrit.

# --- les types -------------------------------------------------------------
type def ecran {
    {int}width; {int}height; {int}flags; {int}background; {int}aspect = 0
}
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
    {string}signature; {ecran}screen; {string}palette = "";
    {list.gblock}blocks
}

# --- fabrication d'un petit GIF (aucun fichier externe) -----------------------
proc fabriquer_gif {} {
    set tete GIF89a
    set ecran [binary format susucucuc 1 1 0x80 0 0]       ;# 1x1, palette globale 2 couleurs
    set palette \xff\xff\xff\x00\x00\x00
    set controle \x21\xf9\x04\x01\x00\x00\x00\x00
    set comm \x21\xfe[binary format c 7]bonjour\x00
    set image \x2c[binary format susususuc 0 0 1 1 0]
    append image \x02\x02\x44\x01\x00                        ;# code minimal + données LZW
    return $tete$ecran$palette$controle$comm$image\x3b
}

# --- lecture : octets -> blocs typés -----------------------------------------
proc sous_blocs {octets posVar} {
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

proc lire_gif {octets} {
    binary scan $octets a6 sig
    binary scan $octets @6susucucucu w h fl bg asp
    set {ecran}e width $w height $h flags $fl background $bg aspect $asp
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
			set d [sous_blocs $octets pos]
			binary scan $d cusucu pk delai transp
			set {controle}c delay $delai \
			    transparent [expr {($pk & 1) ? $transp : -1}] \
			    disposal [expr {($pk >> 2) & 7}]
			lappend blocs [set {gblock}b {control}$c]
		    }
		    254 {
			set {commentaire}c text [sous_blocs $octets pos]
			lappend blocs [set {gblock}b {comment}$c]
		    }
		    255 {
			set d [sous_blocs $octets pos]
			set {application}a id [string range $d 0 10] \
			    hex [binary encode hex [string range $d 11 end]]
			lappend blocs [set {gblock}b {app}$a]
		    }
		    default {
			set {extinconnue}x label $label \
			    hex [binary encode hex [sous_blocs $octets pos]]
			lappend blocs [set {gblock}b {extension}$x]
		    }
		}
	    }
	    44 {
		binary scan $octets @${pos}susususucu l t iw ih f
		incr pos 9
		binary scan $octets @${pos}cu mincode
		incr pos
		set {cadre}i left $l top $t width $iw height $ih flags $f \
		    mincode $mincode lzw [binary encode hex [sous_blocs $octets pos]]
		lappend blocs [set {gblock}b {frame}$i]
	    }
	    59 {
		lappend blocs [set {gblock}b {trailer}{}]
	    }
	    default {
		error "octet de bloc inattendu : $intro"
	    }
	}
    }
    set {gif}g signature $sig screen $e palette $palette blocks $blocs
    return $g
}

# --- description : on aiguille sur le TYPE de chaque bloc ---------------------
proc decrire {b} {
    switch -- [type get $b] {
	gblock.comment   {format "commentaire : %s" $b(comment.text)}
	gblock.control   {format "contrôle graphique : délai %d, transparent %d, disposition %d" \
			      $b(control.delay) $b(control.transparent) $b(control.disposal)}
	gblock.app       {format "application : %s" $b(app.id)}
	gblock.frame     {format "image %d x %d en (%d,%d), %d octets LZW" \
			      $b(frame.width) $b(frame.height) $b(frame.left) $b(frame.top) \
			      [expr {[string length $b(frame.lzw)] / 2}]}
	gblock.extension {format "extension inconnue 0x%02x" $b(extension.label)}
	gblock.trailer   {format "fin du fichier"}
    }
}

set gif [lire_gif [fabriquer_gif]]
puts "type    : [type get $gif]  ($gif(signature))"
puts "écran   : $gif(screen.width) x $gif(screen.height), palette de [expr {[string length $gif(palette)] / 6}] couleurs"
foreach b $gif(blocks) {
    puts "  [decrire $b]"
}
puts "variantes : [lmap b $gif(blocks) {type get $b}]"
if {[catch {set {gblock}x {frame}{0 0 1}} m]} {puts "image incomplète : $m"}
