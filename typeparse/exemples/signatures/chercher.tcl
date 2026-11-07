# La signature d'une procédure : une version simplifiée de lsearch.
# Tout ce que lsearch analyse à la main est dans la signature : options
# exclusives, drapeaux, options à valeur, un défaut calculé à chaque appel
# qui lit un autre argument, un lien vers une variable de l'appelant (upvar).
proc {list.int}chercher {
    {"exact", "glob", "regexp"}-mode ?= glob;   ;# -exact, -glob ou -regexp
    {flag}-nocase;
    {flag}-all;
    {int}-debut ?= 0;
    {int}-fin ?= [expr {[llength $liste] - 1}]; ;# calculé à l'appel
    {var.int}-compte;                           ;# upvar : le nombre trouvé
    {list}liste;
    {string}motif
} {
    set trouves {}
    if {$nocase} { set motif [string tolower $motif] }
    for {set i $debut} {$i <= $fin} {incr i} {
        set e [lindex $liste $i]
        if {$nocase} { set e [string tolower $e] }
        set ok [switch $mode {
            exact  { expr {$e eq $motif} }
            glob   { string match $motif $e }
            regexp { regexp -- $motif $e }
        }]
        if {$ok} {
            lappend trouves $i
            if {!$all} break
        }
    }
    set compte [llength $trouves]      ;# sans -compte : une simple locale
    return $trouves
}

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-52s -> %s" $script $r]
}

set fruits {Pomme poire Prune pêche abricot pomme}
montre {chercher $fruits p*}
montre {chercher -all $fruits p*}
montre {chercher -all -nocase $fruits p*}
montre {chercher -all -exact $fruits pomme}
montre {chercher -all -regexp $fruits {^[Pp]o}}
montre {chercher -all -debut 2 $fruits p*}
montre {chercher -all -fin 2 $fruits p*}
montre {chercher -all -nocase -compte n $fruits p*}
puts "n = $n"

puts "\n--- ce que la signature vérifie, sans une ligne de code"
montre {chercher}
montre {chercher -glob -exact $fruits p*}
montre {chercher -debut deux $fruits p*}
montre {chercher -fruit $fruits p*}
montre {set n abc; chercher -compte n $fruits p*}
montre {chercher -all -n -e $fruits pomme}      ;# préfixes : -nocase -exact
