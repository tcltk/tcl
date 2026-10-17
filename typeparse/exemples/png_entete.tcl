# png_entete.tcl --
#
# Un fichier PNG décrit avec nos annotations : la signature de 8 octets,
# l'en-tête IHDR (largeur, hauteur, profondeur, type de couleur…) et la
# suite des blocs (longueur, nom, données, CRC). Le fichier est fabriqué
# ici même (image rouge de 4x2), puis relu et typé.

# --- les types ------------------------------------------------------------
type def ihdr {
    {int}width; {int}height; {int}depth; {int}colortype;
    {int}compression = 0; {int}filter = 0; {int}interlace = 0
}
type def chunk {{int}length; {string}name; {string}data; {int}crc}
type def png {
    {string}signature = "\x89PNG\r\n\x1a\n";
    {ihdr}header;
    {list.chunk}chunks
}

# --- fabrication d'un petit PNG (aucun fichier externe) ---------------------
proc bloc {nom donnees} {
    set corps $nom$donnees
    binary format Ia*Iu [string length $donnees] $corps [zlib crc32 $corps]
}
proc fabriquer_png {} {
    set w 4; set h 2
    set ligne \x00[string repeat \xff\x00\x00 $w]            ;# filtre 0 + pixels rouges
    set brut [string repeat $ligne $h]
    return \x89PNG\r\n\x1a\n[bloc IHDR [binary format IuIuc5 $w $h {8 2 0 0 0}]][bloc tEXt "Comment\x00bonjour"][bloc IDAT [zlib compress $brut]][bloc IEND ""]
}

# --- lecture : octets -> valeurs typées -------------------------------------
proc lire_png {octets} {
    binary scan $octets a8 sig
    set pos 8
    set {list.chunk}blocs
    while {$pos < [string length $octets]} {
	binary scan $octets @${pos}Iua4 len nom
	set data [string range $octets [expr {$pos+8}] [expr {$pos+7+$len}]]
	binary scan $octets @[expr {$pos+8+$len}]Iu crc
	lappend blocs [set {chunk}k length $len name $nom data [binary encode hex $data] crc $crc]
	if {$nom eq "IHDR"} {
	    binary scan $data IuIucucucucucu w h dep ct comp fil inter
	    set {ihdr}entete width $w height $h depth $dep colortype $ct \
		compression $comp filter $fil interlace $inter
	}
	incr pos [expr {12 + $len}]
    }
    set {png}p signature $sig header $entete chunks $blocs
    return $p
}

set image [lire_png [fabriquer_png]]
puts "type            : [type get $image]"
puts "taille          : $image(header.width) x $image(header.height)"
puts "couleur         : type $image(header.colortype), $image(header.depth) bits"
puts "blocs           : [lmap b $image(chunks) {set b(name)}]"
puts "longueurs       : [lmap b $image(chunks) {set b(length)}]"

# --- ce que le système de types garantit ------------------------------------
if {[catch {set {ihdr}faux width abc height 1 depth 8 colortype 2} m]} {
    puts "largeur invalide : $m"
}
if {[catch {set {png}faux header $image(header) chunks {{1 2}}} m]} {
    puts "bloc incomplet   : $m"
}
