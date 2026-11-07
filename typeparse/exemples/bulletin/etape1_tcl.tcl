# Étape 1 : le prototype, en Tcl nu.
# Une ligne « Nom Prénom note » : la note est le dernier mot, le nom le reste.
# Le bogue classique : on prend la ligne (une chaîne) pour une liste.
proc bulletin {fichier} {
    set f [open $fichier]; set texte [read $f]; close $f
    set total 0; set n 0
    foreach l [split $texte \n] {
        if {$l eq ""} continue
        set nom  [lrange $l 0 end-1]
        set note [lindex $l end]
        puts [format "%-24s %5.1f" $nom $note]
        set total [expr {$total + $note}]; incr n
    }
    puts [format "%-24s %5.2f" moyenne [expr {$total / $n}]]
}
bulletin [file join [file dirname [info script]] classe.txt]
