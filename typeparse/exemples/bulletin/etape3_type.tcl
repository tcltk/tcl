# Étape 3 : le typage. Une ligne est une chaîne : on la découpe en chaîne
# (le dernier blanc), jamais en liste. Ce qu'on en tire est un élève, un
# produit {nom; note} : le nom reste d'un seul tenant, la note est un nombre.
type def eleve {{string}nom; {double}note}

proc {eleve}lire {{string}ligne} {
    set ligne [string trim $ligne]
    set k [string last " " $ligne]
    if {$k < 0} { return -code error "ligne sans note : $ligne" }
    return [list [string trimright [string range $ligne 0 $k-1]] \
                 [string range $ligne $k+1 end]]
}

proc {double}moyenne {{list.double}notes} {
    set t 0.0
    foreach x $notes { set t [expr {$t + $x}] }
    return [expr {$t / [llength $notes]}]
}

proc bulletin {fichier} {
    set f [open $fichier]; set texte [read $f]; close $f
    set {list.eleve}classe          ;# une liste d'élèves, vide
    foreach l [split $texte \n] {
        if {[string trim $l] eq ""} continue
        lappend classe [lire $l]
    }
    foreach e $classe { puts [format "%-24s %5.1f" $e(nom) $e(note)] }
    puts [format "%-24s %5.2f" moyenne [moyenne [lmap e $classe {set e(note)}]]]
}
bulletin [file join [file dirname [info script]] classe.txt]
