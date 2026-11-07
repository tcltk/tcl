# Étape 2 : la mise au point. Le prototype, plus trois lignes (marquées ###).
# Une ligne n'est sûre comme liste que si la relire comme liste ne la change
# pas : la condition de la note @ligne arrête sur les autres.
type debug on                                                ;###
type debug when ligne {[catch {lrange $value 0 end} v] || $v ne $value}  ;###
proc bulletin {fichier} {
    set f [open $fichier]; set texte [read $f]; close $f
    set total 0; set n 0
    foreach l [split $texte \n] {
        if {$l eq ""} continue
        set l {@ligne}$l                                     ;###
        set nom  [lrange $l 0 end-1]
        set note [lindex $l end]
        puts [format "%-24s %5.1f" $nom $note]
        set total [expr {$total + $note}]; incr n
    }
    puts [format "%-24s %5.2f" moyenne [expr {$total / $n}]]
}
bulletin [file join [file dirname [info script]] classe.txt]
