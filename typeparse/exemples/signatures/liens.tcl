# Les liens vers les variables de l'appelant (upvar) dans la signature, et
# les défauts calculés à chaque appel.

proc montre {script} {
    if {[catch {uplevel 1 $script} r]} { set r "ERREUR $r" }
    puts [format "%-40s -> %s" $script $r]
}

puts "--- l'exemple de TIP 460 (proc bar {*a *b} {incr a; incr b})"
# {var.int}a : a est la variable de l'appelant, et elle doit contenir un
# entier ; le contrôle a lieu avant le corps.
proc bar {{var.int}a; {var.int}b} {
    incr a
    incr b
}
proc foo {{var.int}a; {var.int}b} {
    bar a b                    ;# le lien se transmet : a est déjà un lien
}
set v1 0; set v2 1
foo v1 v2
puts "v1 = $v1, v2 = $v2"
montre {set s abc; bar v1 s}
montre {set v1}                       ;# rien n'a bougé : erreur avant le corps

puts "\n--- échanger deux variables, de n'importe quel type"
proc echanger {{var}a; {var}b} {
    lassign [list $b $a] a b
}
set x {1 2}; set y bonjour
echanger x y
puts "x = $x, y = $y"

puts "\n--- un journal : défauts calculés, et une variable par défaut"
# -numero : un défaut [commande], évalué à chaque appel ;
# -dans : la variable de l'appelant où écrire, « journal » par défaut ;
# -niveau : une option exclusive (-info, -alerte, -erreur).
set ::numero 0
proc journaliser {
    {"info", "alerte", "erreur"}-niveau ?= info;
    {int}-numero ?= [incr ::numero];
    {var.list}-dans ?= journal;
    {string}message
} {
    lappend dans [format "%03d %-6s %s" $numero $niveau $message]
    return $numero
}
proc traiter {fichier} {
    journaliser "ouverture de $fichier"
    journaliser -alerte "ligne 3 vide"
    journaliser -erreur -numero 999 "fin inattendue"
    return $journal            ;# « journal » est une locale de traiter
}
puts [join [traiter donnees.txt] \n]
journaliser -dans global "au niveau global"
puts $global
montre {journaliser -niveau debug x}
montre {journaliser}
