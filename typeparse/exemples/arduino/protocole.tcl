# protocole.tcl --
#
# FR : Le protocole série entre Tcl et l'Arduino, décrit par des TYPES. Le
#      même découpage d'octets est écrit en C dans arduino.ino (struct
#      packed) : un champ de type ici = un champ de struct là-bas.
#
#      Requête  : AA genre corps... somme      (somme = XOR de genre et du corps)
#      Réponse  : 55 genre statut valeur(u16le) somme
#
# EN : The serial protocol between Tcl and the Arduino, described by TYPES.
#      The same byte layout is written in C in arduino.ino (packed structs):
#      one typed field here = one struct field there.
#
#      Request  : AA kind body... checksum      (checksum = XOR of kind and body)
#      Reply    : 55 kind status value(u16le) checksum

set _ici [file dirname [file normalize [info script]]]
source [file join $_ici .. .. layout filebinary.tcl]
source [file join $_ici .. .. layout binaliases.tcl]

namespace eval ard {
    # FR : nom -> code du genre de requête / EN: name -> request kind code
    variable GENRES {mode 1 ecrire 2 lire 3 pwm 4}
    variable STATUTS {0 ok 1 "broche invalide" 2 "commande inconnue" 3 "somme fausse"}
    variable trace 0
}

# ---------------------------------------------------------------------------
# FR : Les types   /   EN: The types
# ---------------------------------------------------------------------------

# FR : une CONSTANTE est un type : une somme d'un seul littéral. Un octet de
#      début faux est refusé par le décodage lui-même.
# EN : a CONSTANT is a type: a sum of a single literal. A wrong start byte is
#      refused by decoding itself.
type def debut_req {"\xaa"}
type def debut_rep {"\x55"}

type def entete  {{debut_req}debut = "\xaa"; {u8}genre}

# FR : un corps par genre de requête (struct C de même nom dans arduino.ino)
# EN : one body per request kind (C struct of the same name in arduino.ino)
type def c_mode   {{u8}broche; {u8}mode}       ;# 0 INPUT, 1 OUTPUT, 2 INPUT_PULLUP
type def c_ecrire {{u8}broche; {u8}niveau}     ;# digitalWrite : 0 LOW, 1 HIGH
type def c_lire   {{u8}broche}                 ;# analogRead
type def c_pwm    {{u8}broche; {u8}rapport}    ;# analogWrite : 0..255

type def reponse {
    {debut_rep}debut = "\x55";
    {u8}genre; {u8}statut;
    {u16le}valeur;                             # analogRead : 0..1023
    {u8}somme
}

# ---------------------------------------------------------------------------
# FR : Somme de contrôle et trames   /   EN: Checksum and frames
# ---------------------------------------------------------------------------

proc ard::xor {octets} {
    set s 0
    binary scan $octets cu* vals
    foreach v $vals {set s [expr {$s ^ $v}]}
    return $s
}

# FR : genre (nom), corps (valeur typée c_...) -> octets de la trame
# EN : kind (name), body (typed c_... value) -> frame bytes
proc ard::trame {genre corps} {
    variable GENRES
    set code [dict get $GENRES $genre]
    set {entete}e {genre:}$code
    set utile [type binary encode c_$genre $corps]
    set octets [type binary encode entete $e]$utile
    append octets [binary format cu [ard::xor [binary format cu $code]$utile]]
    return $octets
}

# FR : lit une réponse sur le canal et la vérifie ; rend la valeur
# EN : reads a reply from the channel and checks it; returns the value
proc ard::reponse {chan genre} {
    variable GENRES; variable STATUTS
    set brut [read $chan [type binary {sizeof?}reponse]]
    if {[string length $brut] < [type binary {sizeof?}reponse]} {
	error "réponse tronquée / truncated reply: [ard::hex $brut]"
    }
    set r [type binary decode reponse $brut]   ;# refuse un octet de début faux
    set attendu [ard::xor [string range $brut 1 end-1]]
    if {$r(somme) != $attendu} {
	error "somme de contrôle fausse / bad checksum: reçu $r(somme), attendu $attendu ([ard::hex $brut])"
    }
    if {$r(genre) != [dict get $GENRES $genre]} {
	error "réponse au genre $r(genre), attendu $genre"
    }
    if {$r(statut) != 0} {
	error "arduino: [dict get $STATUTS $r(statut)]"
    }
    return $r(valeur)
}

proc ard::hex {octets} {
    regsub -all {..} [binary encode hex $octets] {& }
}

# ---------------------------------------------------------------------------
# FR : Transport   /   EN: Transport
#   ard::lien sortie entree ?pompe?
#     sortie : canal vers l'Arduino ; entree : canal depuis l'Arduino ;
#     pompe  : commande appelée après chaque envoi (le simulateur), ou "".
# ---------------------------------------------------------------------------

proc ard::lien {sortie entree {pompe ""}} {
    variable lien [list $sortie $entree $pompe]
    foreach c [list $sortie $entree] {
	fconfigure $c -translation binary -buffering none
    }
}

proc ard::demander {genre corps} {
    variable lien; variable trace
    lassign $lien sortie entree pompe
    set octets [ard::trame $genre $corps]
    if {$trace} {puts "  -> [ard::hex $octets]"}
    puts -nonewline $sortie $octets
    flush $sortie
    if {$pompe ne ""} {uplevel #0 $pompe}
    ard::reponse $entree $genre
}

# ---------------------------------------------------------------------------
# FR : L'API, aux noms de l'Arduino   /   EN: The API, with Arduino names
#      Chaque corps est construit dans une variable typée : une broche 300
#      ou un rapport 512 sont refusés AVANT d'être envoyés.
# EN : Each body is built in a typed variable: pin 300 or duty 512 are
#      refused BEFORE being sent.
# ---------------------------------------------------------------------------

proc ard::pinMode {broche mode} {
    set m [dict get {INPUT 0 OUTPUT 1 INPUT_PULLUP 2} $mode]
    set {c_mode}c {broche:}$broche {mode:}$m
    ard::demander mode $c
}
proc ard::digitalWrite {broche niveau} {
    set n [dict get {LOW 0 HIGH 1 0 0 1 1} $niveau]
    set {c_ecrire}c {broche:}$broche {niveau:}$n
    ard::demander ecrire $c
}
proc ard::analogRead {broche} {
    set {c_lire}c {broche:}$broche
    ard::demander lire $c
}
proc ard::analogWrite {broche rapport} {
    set {c_pwm}c {broche:}$broche {rapport:}$rapport
    ard::demander pwm $c
}
