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

# Le document s'écrit en gabarits, dans un interpréteur sûr (te::ecrire) :
# des blocs courts (h, p, ul, code), du texte tel qu'on le lit, les passages
# balisés entre crochets, [tête -- contenu].
set doc [te::ecrire {
    h 1 {Texte enrichi et types}
    p {Chaque passage est un [gras -- segment] typé ; les styles sont une
       [italique -- somme de littéraux], si bien qu'un nom mal orthographié est
       [alerte -- refusé à la construction].}
    h 2 {Ce que le type vérifie}
    ul {le nom de chaque style ([code -- nomstyle])} \
       {le niveau d'un titre : 1, 2 ou 3} \
       {la forme des blocs et des segments, imbriqués
        [italique -- à toute [gras -- profondeur]]} \
       {un [lien@cible="https://core.tcl-lang.org/tips" -- lien] vers les TIP}
    h 2 {Un exemple}
    code {p {Un [gras -- texte] et un [lien@cible="https://tcl.tk" -- lien].}}
}]

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
    set tag [te::Tag segment/marque style $style]
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
