# geo_c.tcl --
#
# FR : Étape 3 : l'application = ../geometrie.tcl (étape 2, annotée) dont les
#      calculs lourds passent en C (geo.c). Les autres procédures restent
#      celles de l'étape 2, inchangées. Les procédures remplacées gardent leur
#      nom, leurs arguments et leurs types de résultat : le reste du programme
#      ne voit pas la différence.
#
#      Deux voies de passage vers C (variable geo::voie) :
#        octets  le polygone est encodé selon la disposition C (polyC, ci-
#                dessous) ; C lit un (const Poly *) ;
#        objet   C reçoit la valeur annotée, vérifie son type, lit ses points.
#
# EN : Stage 3: the application = ../geometrie.tcl (stage 2, annotated) whose
#      heavy computations move to C (geo.c). Replaced procedures keep their
#      name, arguments and result types. Two ways into C (variable geo::voie):
#      "octets" (bytes in the C layout) or "objet" (the annotated value).

set geo_ici [file dirname [file normalize [info script]]]
set geo_racine [file normalize [file join $geo_ici .. ..]]
source [file join $geo_racine layout filebinary.tcl]
source [file join $geo_racine layout binaliases.tcl]
source [file join $geo_ici .. geometrie.tcl]
load [file join $geo_ici geo[info sharedlibextension]] Geo

# FR : la disposition C (geo.c) : struct Poly {uint32_t n; Point pts[];}.
#      {-compact} : toute liste de pointC est rangée en cases, exactement le
#      tableau de Point du C ; type binary encode les copie d'un bloc.
# EN : the C layout (geo.c); {-compact}: lists of pointC are stored as cells.
type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC  {{u32le}n; {list($n).pointC}pts}

namespace eval geo {variable voie octets}

# FR : un polygone, un nuage, une Bézier -> les octets d'un polyC. point et
#      pointC sont deux nominaux distincts (même forme, types différents) : un
#      polygone n'entre pas tel quel dans list($n).pointC, il y entre par un
#      cast vers list.pointC, écrit directement en cases (copié d'un bloc si
#      le polygone est lui-même en cases).
# EN : point and pointC are distinct nominals: cast to list.pointC first.
proc geo::octets {v} {
    set {polyC}p [llength $v] [type cast list.pointC $v]
    type binary encode polyC $p -align natural
}

# ---------------------------------------------------------------------------
# FR : les procédures remplacées / EN: the replaced procedures

proc geo::aire {p} {(
    P is polygone = $p;
    $::geo::voie eq "octets" ? [geo::c::aire_raw [geo::octets $P]]
                             : [geo::c::aire_obj $P]
)}

proc geo::perimetre {p} {(
    P is polygone = $p;
    $::geo::voie eq "octets" ? [geo::c::perimetre_raw [geo::octets $P]]
                             : [geo::c::perimetre_obj $P]
)}

# FR : C rend directement un rectangle annoté / EN: C returns an annotated rectangle
proc geo::englobant {p} {(
    P is polygone = $p;
    $::geo::voie eq "octets" ? [geo::c::englobant_raw [geo::octets $P]]
                             : [geo::c::englobant_obj $P]
)}

proc geo::dedans_tous {pts q} {(
    N is nuage = $pts;  Q is polygone = $q;
    $::geo::voie eq "octets" ? [geo::c::dedans_raw [geo::octets $N] [geo::octets $Q]]
                             : [geo::c::dedans_obj $N $Q]
)}

# FR : C rend directement une polyligne annotée / EN: C returns an annotated polyline
proc geo::bezier_echantillons {b n} {(
    B is bezier = $b;
    $::geo::voie eq "octets" ? [geo::c::bezier_raw [geo::octets $B] $n]
                             : [geo::c::bezier_obj $B $n]
)}
