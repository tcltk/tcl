# demo.tcl -- à lancer avec le wish (ou le tclsh avec Tk) de l'arbre TIP 764.
package require Tk
source [file join [file dirname [file normalize [info script]]] modele.tcl]

te::styles {
    gras     {poids bold}
    italique {pente italic}
    titre1   {taille 18 poids bold couleur #1d2433}
    titre2   {taille 14 poids bold couleur #2b5c8a}
    code     {famille Courier taille 10 couleur #4a5468 marge 12}
    puce     {marge 16}
    alerte   {couleur #b4531f souligne 1}
}

set doc [te::document \
    [te::titre 1 "Texte enrichi et types"] \
    [te::para "Chaque passage est un " [te::m gras "segment"] " typé ; les styles " \
	"sont une " [te::m italique "somme de littéraux"] ", si bien qu'un nom mal " \
	"orthographié est " [te::m alerte "refusé à la construction"] "."] \
    [te::titre 2 "Ce que le type vérifie"] \
    [te::puces \
	[list "le nom de chaque style (" [te::m code "nomstyle"] ")"] \
	[list "le niveau d'un titre : 1, 2 ou 3"] \
	[list "la forme des blocs et des segments, imbriqués " \
	    [te::m italique "à toute " [te::m gras "profondeur"]]] \
	[list "un " [te::lien https://core.tcl-lang.org/tips "lien"] " vers les TIP"]] \
    [te::titre 2 "Un exemple"] \
    [te::code "set doc \[te::document \[te::para \"Un \" \[te::m gras texte\]\]\]"]]

proc ouvrir {cible} {
    tk_messageBox -title Lien -message "Lien vers $cible"
}

# À gauche le document, éditable : Ctrl-B et Ctrl-I posent gras et italique
# sur la sélection. « Relire » reconstruit un document typé à partir du
# widget (te::lire) et le rend à droite : on y voit ce que le type a gardé.
frame .b
button .b.relire -text "Relire  \u2192" -command relire
checkbutton .b.coupe -text "couper les chevauchements" -variable couper
set couper 1
pack .b.relire .b.coupe -side left -padx 6 -pady 4
pack .b -side top -fill x
panedwindow .p -orient horizontal
text .t -wrap word -width 60 -height 26 -padx 14 -pady 10 -relief flat -undo 1
text .r -wrap word -width 60 -height 26 -padx 14 -pady 10 -relief flat \
	-background #f6f4ee
.p add .t .r
pack .p -fill both -expand 1
te::rendre .t $doc ouvrir

# Ctrl-B / Ctrl-I : pose (ou retire, si toute la sélection l'a déjà) la marque
# sur la sélection, puis refait l'apparence (te::habiller).
proc basculer {style} {
    if {[.t tag ranges sel] eq ""} return
    set tag [te::Tag segment.marque style $style]
    set plages [.t tag nextrange $tag sel.first]
    if {[llength $plages] && [.t compare [lindex $plages 0] <= sel.first]
	    && [.t compare [lindex $plages 1] >= sel.last]} {
	.t tag remove $tag sel.first sel.last
    } else {
	.t tag add $tag sel.first sel.last
    }
    te::habiller .t
}
foreach {touche style} {b gras B gras i italique I italique} {
    bind .t <Control-$touche> [list basculer $style]
    bind .t <Control-$touche> +break
}

proc relire {} {
    global couper
    set mode [expr {$couper ? "couper" : "erreur"}]
    if {[catch {te::lire .t -chevauchement $mode} d]} {
	tk_messageBox -icon warning -title "Relecture" -message $d
	return
    }
    .r configure -state normal
    .r delete 1.0 end
    te::rendre .r $d ouvrir
    .r configure -state disabled
    puts "[llength $d] blocs : [lmap b $d {type get $b}]"
}
relire
