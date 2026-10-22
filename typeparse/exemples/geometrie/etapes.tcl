# etapes.tcl --
#
# FR : Charge une étape dans un interpréteur fils, pour que les trois
#      versions (mêmes noms, même espace geo) puissent cohabiter.
#        etape::creer brut|annote|octets|objet  -> nom de l'interpréteur
#        etape::donnee $i TYPE VALEUR           -> la valeur, typée sauf en brut
# EN : Loads a stage in a child interpreter, so the three versions (same
#      names, same geo namespace) can live side by side.

namespace eval etape {
    variable ici [file dirname [file normalize [info script]]]
    variable noms {brut "1 brut" annote "2 annoté" octets "3 C, octets" objet "3 C, objet"}
}

proc etape::creer {quoi} {
    variable ici
    set i [interp create]
    switch -- $quoi {
        brut   { $i eval [list source [file join $ici geo_brut.tcl]] }
        annote { $i eval [list source [file join $ici .. geometrie.tcl]] }
        octets - objet {
            $i eval [list source [file join $ici geo_c.tcl]]
            $i eval [list set geo::voie $quoi]
        }
        default { error "étape inconnue : $quoi" }
    }
    $i eval [list set etape $quoi]
    # FR : forme comparable d'un résultat : contenu brut, nombres arrondis
    $i eval {
        proc normalise {v} {
            if {[llength [info commands ::tcl::unsupported::content]]} {
                set v [::tcl::unsupported::content $v]
            }
            if {[string is double -strict $v]} {
                set r [format %.6g $v]
                return [expr {$r == 0 ? 0 : $r}]
            }
            if {[llength $v] > 1 || ([llength $v] == 1 && [lindex $v 0] ne $v)} {
                return [lmap e $v {normalise $e}]
            }
            return $v
        }
        proc donnee {type valeur} {
            if {$::etape eq "brut" || $type eq "-"} {return $valeur}
            type cast $type $valeur
        }
    }
    return $i
}

proc etape::nom {quoi} {variable noms; dict get $noms $quoi}
