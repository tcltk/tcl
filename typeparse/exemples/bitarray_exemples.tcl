# bitarray_exemples.tcl --
#
# Trois usages de bitarray(n, ordre) :
#
#   1. l'octet « packed » d'un GIF (écran logique) : msb0, car la
#      spécification décrit les bits de gauche à droite ;
#   2. les drapeaux d'un segment TCP : msb0, comme le schéma de la RFC 9293
#      (le bit 0 est le premier bit transmis, le plus à gauche) ;
#   3. les permissions Unix (rwxr-xr-x) : lsb0, comme 1 << i en C.
#
# On voit pourquoi les deux ordres existent : le bon est celui du document
# que l'on recopie. Aucun fichier externe n'est nécessaire.

# ===========================================================================
# 1. L'octet packed d'un GIF
#
#      7   6 5 4   3     2 1 0
#     +---+-----+------+------+
#     | G | res | sort | size |      G    : table de couleurs globale présente
#     +---+-----+------+------+      res  : résolution - 1 (3 bits)
#                                    sort : palette triée
#                                    size : la palette a 2^(size+1) couleurs
#
# En msb0 on écrit les indices dans l'ordre où la spécification présente les
# champs : 0 | 1:3 | 4 | 5:7.
# ===========================================================================

puts "=== 1. octet packed d'un GIF ==="
type def gifpacked {{bitarray(8,msb0)}bits}

proc decrire_packed {octet} {
    set {bitarray(8,msb0)}p $octet
    puts [format "  0x%02X = %s" $octet [join [lmap i {0 1 2 3 4 5 6 7} {set p($i)}] ""]]
    puts "    table globale : [expr {$p(0) ? "oui" : "non"}]"
    puts "    résolution    : [expr {$p(1:3) + 1}] bit(s) par couleur primaire"
    puts "    palette triée : [expr {$p(4) ? "oui" : "non"}]"
    puts "    couleurs      : [expr {1 << ($p(5:7) + 1)}]"
}
decrire_packed 0xF7        ;# GCT, 8 bits, non triée, 256 couleurs
decrire_packed 0x80        ;# GCT, 1 bit, 2 couleurs (le GIF 1x1 de gif_somme.tcl)

# Construire l'octet champ par champ
set {bitarray(8,msb0)}neuf 0
set neuf(0) 1               ;# palette globale
set neuf(1:3) 5             ;# 6 bit(s) par couleur primaire
set neuf(5:7) 3             ;# 16 couleurs
puts [format "  construit : 0x%02X" $neuf]

# Le même octet lu en lsb0 : les indices sont à l'envers, c'est le piège.
set {bitarray(8,lsb0)}l 0xF7
puts "  lsb0 : l'indicateur de table globale est l(7)=$l(7), la taille l(0:2)=$l(0:2)"

# ===========================================================================
# 2. Les drapeaux d'un segment TCP (octets 12 et 13 de l'en-tête)
#
#      0       3 4     6 7  8  9  10  11  12  13  14  15
#     +---------+-------+--+---+---+---+---+---+---+---+---+
#     | offset  | rsv   |NS|CWR|ECE|URG|ACK|PSH|RST|SYN|FIN|
#     +---------+-------+--+---+---+---+---+---+---+---+---+
# ===========================================================================

puts "\n=== 2. drapeaux TCP ==="
set noms_drapeaux {NS CWR ECE URG ACK PSH RST SYN FIN}   ;# bits 7 à 15

proc drapeaux {mot} {
    global noms_drapeaux
    set actifs {}
    set i 7
    foreach nom $noms_drapeaux {
	if {$mot($i)} {lappend actifs $nom}
	incr i
    }
    expr {[llength $actifs] ? [join $actifs +] : "-"}
}

# fabrication d'un SYN+ACK, en-tête de 5 mots de 32 bits
set {bitarray(16,msb0)}mot 0
set mot(0:3) 5              ;# data offset
set mot(14) 1               ;# SYN
set mot(11) 1               ;# ACK
puts [format "  mot construit : 0x%04X  offset=%d  drapeaux=%s" $mot $mot(0:3) [drapeaux $mot]]

# décodage depuis les deux octets bruts, comme dans un vrai paquet
set octets [binary format S $mot]
binary scan $octets Su valeur
set {bitarray(16,msb0)}lu $valeur
puts "  relu : offset=$lu(0:3) drapeaux=[drapeaux $lu]  (bits à 1 : [expr {popcount($lu)}])"

foreach {nom hex} {syn 0x5002 synack 0x5012 fin_ack 0x5011 rst 0x5004 push_ack 0x5018} {
    set {bitarray(16,msb0)}w $hex
    puts [format "  %-9s 0x%04X  %s" $nom $w [drapeaux $w]]
}

# Le même mot lu avec le MAUVAIS ordre : une erreur de type, pas un résultat faux.
if {[catch {set {bitarray(16,lsb0)}faux $lu} msg]} {
    puts "  lsb0 <- msb0 : $msg"
}

# ===========================================================================
# 3. Les permissions Unix : rwxr-xr-x
#
#   bit    8 7 6   5 4 3   2 1 0
#          r w x   r w x   r w x
#          user    group   other
#
# En lsb0, 'other' occupe les bits 0:2, 'group' 3:5, 'user' 6:8 : c'est la
# numération octale (chmod 755 = trois chiffres de 3 bits).
# ===========================================================================

puts "\n=== 3. permissions Unix ==="

proc symbolique {m} {
    set s ""
    foreach lettre {r w x r w x r w x} i {8 7 6 5 4 3 2 1 0} {
	append s [expr {$m($i) ? $lettre : "-"}]
    }
    return $s
}

proc chmod_octal {m} {
    format %o%o%o $m(6:8) $m(3:5) $m(0:2)
}

set {bitarray(9)}perm 0o755
puts [format "  0o755 -> %s  (octal %s, %d droits accordés)" \
	[symbolique $perm] [chmod_octal $perm] [expr {popcount($perm)}]]

# chmod g+w   : un bit
set perm(4) 1
puts "  g+w    -> [symbolique $perm]  [chmod_octal $perm]"
# chmod o=    : une tranche
set perm(0:2) 0
puts "  o=     -> [symbolique $perm]  [chmod_octal $perm]"
# chmod u=rw  : une tranche
set perm(6:8) 6
puts "  u=rw   -> [symbolique $perm]  [chmod_octal $perm]"
# un umask 022 appliqué à 0666
set {bitarray(9)}fichier 0o666
set {bitarray(9)}masque 0o22
set resultat [expr {$fichier & ~$masque & 0o777}]
set {bitarray(9)}final $resultat
puts "  0o666 & ~0o22 -> [symbolique $final]  [chmod_octal $final]"

# La valeur ne peut pas dépasser 9 bits
if {[catch {set {bitarray(9)}trop 0o1000} msg]} {puts "  0o1000 : $msg"}
