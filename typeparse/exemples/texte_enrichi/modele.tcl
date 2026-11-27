# modele.tcl --
#
#   Texte enrichi pour un widget text de Tk, décrit par des types (TIP 764).
#
#   Un document est une liste de blocs ; un bloc est un paragraphe, un titre,
#   une liste à puces ou du code ; le texte d'un bloc est une liste de
#   segments, qui s'imbriquent : du texte, un passage marqué d'un style, un
#   lien. Les styles sont une table nommée ; leurs noms deviennent un type
#   (une somme de littéraux), si bien qu'un style mal orthographié est refusé
#   à la construction du document, pas au rendu.
#
#   N'emploie que des formes communes à la v16k et à la v16l : constructeurs
#   positionnels, chemins, variantes nommées par le type (`{segment/lien}`).

namespace eval te {
    variable styles {}		;# nom -> style (valeur typée)
    variable champs {}		;# nom -> champs écrits dans la table
    variable aspects {}		;# widget -> tag d'aspect -> déjà configuré
}

# ------------------------------------------------------------------
# Styles
# ------------------------------------------------------------------

type def poids   {"normal", "bold"}
type def pente   {"roman", "italic"}
type def oui_non {"0", "1"}

type def style {
    {string}famille ?= "Helvetica";
    {int(u,8)}taille ?= 11;
    {poids}poids ?= "normal";
    {pente}pente ?= "roman";
    {string}couleur ?= "black";
    {oui_non}souligne ?= 0;
    {int(u,8)}marge ?= 0
}

# Un style par ses champs nommés, les autres à leur défaut :
#   te::style poids bold couleur #803000
proc te::style {args} {
    if {[llength $args] == 0} {
	set {style}s
	return $s
    }
    # le chemin se calcule : les champs nommés, les autres à leur défaut
    set {style}s([join [dict keys $args] ,]) {*}[dict values $args]
    return $s
}

# ------------------------------------------------------------------
# La table des styles, et les types qui en dépendent
# ------------------------------------------------------------------
#
# Les noms de style sont un type : nomstyle = somme des noms de la table.
# Changer la table redéfinit nomstyle, donc aussi les types qui s'en servent
# (segment, bloc, document) : type forget les oublie ensemble.

proc te::styles {table} {
    variable styles
    variable champs
    variable aspects
    set nouveau {}
    dict for {nom c} $table {
	dict set nouveau $nom [te::style {*}$c]
    }
    set styles $nouveau
    # les champs écrits, sans les défauts : c'est ce qui se cumule (te::Aspect)
    set champs [dict merge {lien {couleur #2b5c8a souligne 1}} $table]
    set aspects {}
    catch {type forget document bloc segment nomstyle niveau}
    type def nomstyle "\"[join [dict keys $styles] {", "}]\""
    type def niveau {"1", "2", "3"}
    type def segment {
	{string}texte,
	{{nomstyle}style; {list.segment}contenu}marque,
	{{string}cible; {list.segment}contenu}lien
    }
    type def bloc {
	{list.segment}para,
	{{niveau}niveau; {list.segment}contenu}titre,
	{list.list.segment}puces,
	{string}code
    }
    type def document list.bloc
    return [dict keys $styles]
}

# ------------------------------------------------------------------
# Construire un document
# ------------------------------------------------------------------
#
# Une chaîne nue devient un segment de texte ; un segment déjà typé passe tel
# quel. Chaque constructeur rend une valeur typée : une faute (style inconnu,
# niveau 4) est refusée ici, avec le nom de ce qui manque.

proc te::seg {x} {
    if {[type get $x] eq ""} {
	set {segment}s {segment/texte}$x
	return $s
    }
    set {segment}s $x
    return $s
}
proc te::segs {liste} {
    set {list.segment}L
    foreach x $liste {
	lappend L [te::seg $x]
    }
    return $L
}

proc te::m {style args} {
    set {segment}s {segment/marque}[list $style [te::segs $args]]
    return $s
}
proc te::lien {cible args} {
    set {segment}s {segment/lien}[list $cible [te::segs $args]]
    return $s
}

proc te::para {args} {
    set {bloc}b {bloc/para}[te::segs $args]
    return $b
}
proc te::titre {n args} {
    set {bloc}b {bloc/titre}[list $n [te::segs $args]]
    return $b
}
proc te::puces {args} {
    set items [lmap item $args {te::segs $item}]
    set {bloc}b {bloc/puces}$items
    return $b
}
proc te::code {texte} {
    set {bloc}b {bloc/code}$texte
    return $b
}
proc te::document {args} {
    set {document}d {*}$args
    return $d
}

# ------------------------------------------------------------------
# Écrire du texte sans guillemets : le gabarit
# ------------------------------------------------------------------
#
# Un seul argument, le texte tel qu'on le lit ; les passages balisés sont
# entre crochets, une tête puis le contenu :
#
#   te::p {Voir [lien@cible="https://tcl.tk" -- la page de la TIP], puis
#          [marque@style=alerte -- un texte [gras -- très] important].}
#
#   [variante@attribut=valeur,... -- contenu]   une variante de segment
#   [style -- contenu]                          raccourci de [marque@style=style -- …]
#
# La tête est la forme des noms de tags (te::LireNotes : « , » sépare les
# attributs, "…" cite une valeur). Le contenu est lu comme le texte autour :
# espaces et ponctuation gardés, crochets imbriqués. \[ \] \\ échappent un
# crochet ou une barre. Comme en HTML, une suite de blancs (fin de ligne
# comprise) compte pour un espace.
#
# Les attributs sont les champs de la variante, le contenu son champ
# {list.segment}contenu : la valeur est construite par un chemin à travers la
# somme, set {segment}s(lien.(cible,contenu)) ..., calculé à partir de la
# balise. Le type refuse une variante inconnue, un attribut inconnu ou
# manquant, un style hors de nomstyle : aucune table ici.

proc te::texte {gabarit} {
    set res {}
    set buf ""
    set n [string length $gabarit]
    for {set i 0} {$i < $n} {incr i} {
	set c [string index $gabarit $i]
	if {$c eq "\\" && [string index $gabarit $i+1] in {\[ \] \\}} {
	    append buf [string index $gabarit [incr i]]
	} elseif {$c eq "\["} {
	    set j [te::Fermant $gabarit $i]
	    if {$buf ne ""} {
		lappend res [te::seg [regsub -all {\s+} $buf " "]]
		set buf ""
	    }
	    lappend res [te::Balise [string range $gabarit $i+1 $j-1]]
	    set i $j
	} elseif {$c eq "\]"} {
	    return -code error -errorcode {TE GABARIT} \
		"\"\]\" without \"\[\" at $i in \"$gabarit\""
	} else {
	    append buf $c
	}
    }
    if {$buf ne ""} {
	lappend res [te::seg [regsub -all {\s+} $buf " "]]
    }
    return $res
}

# L'indice du crochet fermant qui répond à celui de i.
proc te::Fermant {texte i} {
    set prof 0
    set n [string length $texte]
    for {} {$i < $n} {incr i} {
	switch -- [string index $texte $i] {
	    \\ {incr i}
	    \[ {incr prof}
	    \] {
		if {[incr prof -1] == 0} {
		    return $i
		}
	    }
	}
    }
    return -code error -errorcode {TE GABARIT} \
	"\"\[\" without \"\]\" in \"$texte\""
}

# [tête -- contenu] -> un segment typé.
proc te::Balise {interieur} {
    variable styles
    # le premier « -- » hors des crochets imbriqués et des guillemets
    set prof 0
    set guill 0
    set coupe -1
    set n [string length $interieur]
    for {set i 0} {$i < $n && $coupe < 0} {incr i} {
	set c [string index $interieur $i]
	if {$c eq "\\"} {
	    incr i
	} elseif {$c eq "\""} {
	    set guill [expr {!$guill}]
	} elseif {!$guill && $c eq "\["} {
	    incr prof
	} elseif {!$guill && $c eq "\]"} {
	    incr prof -1
	} elseif {!$guill && $prof == 0 && $c eq "-"
		&& [string range $interieur $i $i+1] eq "--"
		&& ($i == 0 || [string is space [string index $interieur $i-1]])
		&& [string is space [string index $interieur $i+2]]} {
	    set coupe $i
	}
    }
    if {$coupe < 0} {
	return -code error -errorcode {TE GABARIT} \
	    "\[$interieur\]: write \[head -- content\]"
    }
    set tete [string trim [string range $interieur 0 $coupe-1]]
    set contenu [te::texte [string trimleft [string range $interieur $coupe+2 end]]]
    if {![regexp {^([A-Za-z_]\w*)(?:@(.*))?$} $tete -> nom notes]} {
	return -code error -errorcode {TE GABARIT} "bad head \"$tete\""
    }
    if {$notes eq "" && [dict exists $styles $nom]} {
	set notes style=$nom			;# [gras -- …]
	set nom marque
    } elseif {[catch {type def segment/$nom}]} {
	# type def S.v rend la définition d'une variante : ici, elle n'existe pas
	return -code error -errorcode {TE GABARIT} "unknown tag \"$nom\":\
	    must be a variant of segment or a style ([join [dict keys $styles] {, }])"
    }
    set champs {}
    set valeurs {}
    dict for {chemin val} [te::LireNotes $notes] {
	if {[llength $chemin] != 1 || [llength $val] != 1} {
	    return -code error -errorcode {TE GABARIT} \
		"\[$tete\]: an attribute is written name=value"
	}
	lappend champs [lindex $chemin 0]
	lappend valeurs [lindex $val 0]
    }
    lappend champs contenu
    lappend valeurs $contenu
    # la variante et ses champs par un chemin calculé : le type vérifie
    if {[catch {
	set {segment}s($nom.([join $champs ,])) {*}$valeurs
    } msg]} {
	return -code error -errorcode {TE GABARIT} "\[$tete\]: $msg"
    }
    return $s
}

# Un document entier dans un interpréteur à part : te::ecrire SCRIPT.
#
#   te::ecrire {
#       h 1 {Texte enrichi}
#       p {Un [gras -- texte] et un [lien@cible="https://tcl.tk" -- lien].}
#       ul {premier point} {second point}
#       code {set x 1}
#   }
#
# Le script s'exécute dans un interpréteur sûr (interp create -safe) : les
# noms de blocs sont courts (p, h, ul, code) sans rien masquer ici, et le
# document ne peut ni lire de fichier ni lancer de commande. Chaque bloc est
# un alias vers te::p, te::h, te::ul, te::code, qui range le bloc typé ICI :
# seuls des textes traversent ; le document est construit et vérifié dans cet
# interpréteur, avec ses types.

proc te::ecrire {script} {
    variable blocsEcrits
    set blocsEcrits {}
    set i [interp create -safe]
    try {
	foreach {nom cible} {p te::p h te::h ul te::ul code te::code} {
	    interp alias $i $nom {} te::Ranger $cible
	}
	interp eval $i $script
    } finally {
	interp delete $i
    }
    set doc [te::document {*}$blocsEcrits]
    set blocsEcrits {}
    return $doc
}
proc te::Ranger {cible args} {
    variable blocsEcrits
    lappend blocsEcrits [$cible {*}$args]
    return
}

# Blocs écrits en gabarit : te::p, te::h, te::ul (un gabarit par puce).
proc te::p {gabarit} {
    set {bloc}b {bloc/para}[te::texte [string trim $gabarit]]
    return $b
}
proc te::h {niveau gabarit} {
    set {bloc}b {bloc/titre}[list $niveau [te::texte [string trim $gabarit]]]
    return $b
}
proc te::ul {args} {
    set items [lmap g $args {te::texte [string trim $g]}]
    set {bloc}b {bloc/puces}$items
    return $b
}

# ------------------------------------------------------------------
# Des noms de tags « type@notes »
# ------------------------------------------------------------------
#
# Un tag Tk est l'annotation d'une plage de texte. Son nom suit l'affichage
# des valeurs typées de la TIP 764, T@notes : le nom canonique de la variante
# du nœud, puis ses attributs comme notes.
#
#   bloc/para   bloc/puces   bloc/code   bloc/titre@niveau=2
#   segment/marque@style=gras   segment/lien@cible="https://tcl.tk"
#
# Les notes sont un ensemble de chemins :
#   ,          sépare les notes (l'ordre ne compte pas)
#   .          descend d'un nœud à l'autre (une branche)
#   =          donne la valeur d'une feuille ; sans =, la note est un drapeau
#   (…)        factorise : font.(family=helvetica,size=10)
#   "…"        une clé ou une valeur hors de [\w-] : entre guillemets, avec
#              les substitutions backslash de Tcl (\" \\ \n é…)
# Une note répétée : la dernière l'emporte, comme une clé de dict ; poser une
# feuille efface ce qui était dessous ou dessus (font=x puis font.size=10).
#
# Forme canonique (te::EcrireNotes) : clés triées, branches factorisées, une
# branche à un seul enfant écrite sans parenthèses (a.b=1), guillemets
# seulement s'il le faut. Un même sens donne un même nom, et Tk fusionne bien
# deux plages voisines de même sens.

# Les notes lues sont un dict plat : chemin (liste de clés) -> {} (drapeau)
# ou {valeur} (feuille).

proc te::Mot {texte} {
    if {[regexp {^[\w-]+$} $texte]} {
	return $texte
    }
    set res ""
    foreach c [split $texte ""] {
	switch -- $c {
	    \\ {append res \\\\}
	    \" {append res \\\"}
	    \n {append res \\n}
	    \t {append res \\t}
	    default {
		if {[scan $c %c] < 32} {
		    append res [format \\u%04x [scan $c %c]]
		} else {
		    append res $c
		}
	    }
	}
    }
    return \"$res\"
}

proc te::EcrireNotes {notes} {
    set groupes [dict create]
    foreach chemin [lsort -dictionary [dict keys $notes]] {
	dict lappend groupes [lindex $chemin 0] \
	    [list [lrange $chemin 1 end] [dict get $notes $chemin]]
    }
    set res {}
    foreach cle [lsort [dict keys $groupes]] {
	set g [dict get $groupes $cle]
	lassign [lindex $g 0] reste val
	if {[llength $g] == 1 && [llength $reste] == 0} {
	    lappend res [expr {[llength $val] ? "[te::Mot $cle]=[te::Mot [lindex $val 0]]" : [te::Mot $cle]}]
	    continue
	}
	set sous [dict create]
	foreach e $g {
	    dict set sous {*}[list [lindex $e 0]] [lindex $e 1]
	}
	set texte [te::EcrireNotes $sous]
	set premiers [lsort -unique [lmap e $g {lindex $e 0 0}]]
	if {[llength $premiers] == 1} {
	    lappend res [te::Mot $cle].$texte
	} else {
	    lappend res [te::Mot $cle].($texte)
	}
    }
    return [join $res ,]
}

proc te::LireNotes {texte} {
    set i 0
    set notes [dict create]
    if {$texte eq ""} {
	return $notes
    }
    te::Suite $texte i {} notes
    if {$i < [string length $texte]} {
	return -code error "bad notes \"$texte\": unexpected \"[string index $texte $i]\" at $i"
    }
    return $notes
}
proc te::Suite {texte iVar prefixe notesVar} {
    upvar 1 $iVar i $notesVar notes
    while 1 {
	te::Note $texte i $prefixe notes
	if {[string index $texte $i] ne ","} {
	    return
	}
	incr i
    }
}
proc te::Note {texte iVar prefixe notesVar} {
    upvar 1 $iVar i $notesVar notes
    set chemin [linsert $prefixe end [te::Jeton $texte i]]
    while {[string index $texte $i] eq "."} {
	incr i
	if {[string index $texte $i] eq "("} {
	    incr i
	    te::Suite $texte i $chemin notes
	    if {[string index $texte $i] ne ")"} {
		return -code error "bad notes \"$texte\": missing \")\" at $i"
	    }
	    incr i
	    return
	}
	lappend chemin [te::Jeton $texte i]
    }
    set val {}
    if {[string index $texte $i] eq "="} {
	incr i
	set val [list [te::Jeton $texte i]]
    }
    # la dernière l'emporte : retirer ce qui est dessus ou dessous
    set n [llength $chemin]
    foreach c [dict keys $notes] {
	set m [expr {min($n, [llength $c])}]
	if {[lrange $c 0 $m-1] eq [lrange $chemin 0 $m-1]} {
	    dict unset notes $c
	}
    }
    dict set notes $chemin $val
}
proc te::Jeton {texte iVar} {
    upvar 1 $iVar i
    if {[string index $texte $i] eq "\""} {
	set j [expr {$i + 1}]
	while {$j < [string length $texte]} {
	    set c [string index $texte $j]
	    if {$c eq "\\"} {
		incr j 2
	    } elseif {$c eq "\""} {
		set mot [subst -nocommands -novariables \
			[string range $texte $i+1 $j-1]]
		set i [expr {$j + 1}]
		return $mot
	    } else {
		incr j
	    }
	}
	return -code error "bad notes \"$texte\": missing close-quote"
    }
    if {![regexp -start $i -indices {\A[\w-]+} $texte r]} {
	return -code error "bad notes \"$texte\": key expected at $i"
    }
    set i [expr {[lindex $r 1] + 1}]
    return [string range $texte {*}$r]
}

# te::Tag bloc/titre niveau 2  ->  bloc/titre@niveau=2
# Les clés peuvent être des chemins : te::Tag aspect police.taille 10
proc te::Tag {T args} {
    if {[llength $args] == 0} {
	return $T
    }
    set notes [dict create]
    foreach {cle val} $args {
	dict set notes [split $cle .] [list $val]
    }
    return $T@[te::EcrireNotes $notes]
}

# Le nom d'un tag -> {sorte attribut}, ou {} si ce n'est pas un tag du
# document (sel, un tag de l'application). Erreur si c'est un tag du
# document mal formé.
proc te::Tete {tag} {
    if {![regexp {^(bloc|segment)/([a-z]+)(?:@(.*))?$} $tag -> T sorte notes]} {
	return {}
    }
    set attributs {bloc {para {} titre niveau puces {} code {}}
		   segment {marque style lien cible}}
    if {![dict exists $attributs $T $sorte]} {
	return {}
    }
    set cle [dict get $attributs $T $sorte]
    set n [te::LireNotes $notes]
    if {$cle eq ""} {
	return [list $sorte]
    }
    if {![dict exists $n [list $cle]] || [dict get $n [list $cle]] eq ""} {
	return -code error "no value for \"$cle\""
    }
    return [list $sorte [lindex [dict get $n [list $cle]] 0]]
}

# ------------------------------------------------------------------
# Rendu dans un widget text
# ------------------------------------------------------------------
#
# Chaque bloc est couvert par le tag de sa variante (bloc/para, bloc/titre@2…),
# chaque passage marqué par segment/marque@style, chaque lien par
# segment/lien@cible. Les blocs sont séparés par une ligne vide sans tag.
#
# Ces tags disent ce qu'est le texte, pas son apparence : ils ne sont pas
# configurés. Tk ne cumule pas les polices de deux tags (la police du tag de
# plus haute priorité remplace l'autre) : gras dans italique ne donnerait que
# l'un des deux. L'apparence est donc calculée : chaque morceau de texte reçoit
# un seul tag d'aspect, le cumul des styles de ses tags, du bloc vers le
# passage le plus intérieur. Son nom est l'ensemble des champs écrits :
#
#   aspect@police.(pente=italic,poids=bold)
#   aspect@couleur="#b4531f",souligne=1
#   aspect                               (aucun champ : le style par défaut)

proc te::Police {s} {
    set police [list $s(famille) $s(taille) $s(poids)]
    if {$s(pente) eq "italic"} {
	lappend police italic
    }
    return $police
}

# Les tags d'un morceau de texte -> le nom de son tag d'aspect, configuré au
# besoin. Les blocs d'abord, puis les passages dans l'ordre donné (de
# l'extérieur vers l'intérieur) : l'intérieur l'emporte sur un même champ.
proc te::Aspect {w tags} {
    variable champs
    variable aspects
    set blocs {}
    set passages {}
    foreach tag $tags {
	if {[catch {te::Tete $tag} t] || $t eq ""} continue
	lassign $t sorte note
	switch -- $sorte {
	    titre   {lappend blocs titre$note}
	    code    {lappend blocs code}
	    puces   {lappend blocs puce}
	    marque  {lappend passages $note}
	    lien    {lappend passages lien}
	}
    }
    set d {}
    foreach nom [concat $blocs $passages] {
	if {[dict exists $champs $nom]} {
	    set d [dict merge $d [dict get $champs $nom]]
	}
    }
    # les champs de la police sous une branche : police.(famille=…,taille=…)
    set nom [te::Tag aspect {*}[concat {*}[lmap {k v} $d {
	if {$k in {famille taille poids pente}} {set k police.$k}
	list $k $v
    }]]]
    if {![dict exists $aspects $w $nom]} {
	set s [te::style {*}$d]
	$w tag configure $nom -font [te::Police $s] -foreground $s(couleur) \
	    -underline $s(souligne) -lmargin1 $s(marge) -lmargin2 $s(marge)
	dict set aspects $w $nom 1
    }
    return $nom
}

proc te::configurer {w} {
    $w configure -font [te::Police [te::style]]
}

# Refait les tags d'aspect d'après les tags du document, après une édition
# (Ctrl-B sur une sélection…).
proc te::habiller {w} {
    foreach t [$w tag names] {
	if {$t eq "aspect" || [string match aspect@* $t]} {
	    $w tag remove $t 1.0 end
	}
    }
    set ouverts {}
    set pos 0
    foreach {cle valeur index} [$w dump -text -tag 1.0 end] {
	switch -- $cle {
	    tagon  {lappend ouverts $valeur}
	    tagoff {set ouverts [lsearch -all -inline -exact -not $ouverts $valeur]}
	    text {
		set fin [expr {$pos + [string length $valeur]}]
		$w tag add [te::Aspect $w $ouverts] "1.0 + $pos chars" "1.0 + $fin chars"
		set pos $fin
	    }
	}
    }
}

proc te::rendre {w doc {commandeLien {}}} {
    set {document}doc $doc
    te::configurer $w
    foreach b $doc {
	switch -- [namespace tail [type get $b]] {
	    bloc/para {
		te::Segments $w $b(para) [list [te::Tag bloc/para]] $commandeLien
	    }
	    bloc/titre {
		te::Segments $w $b(titre.contenu) \
			[list [te::Tag bloc/titre niveau $b(titre.niveau)]] $commandeLien
	    }
	    bloc/puces {
		set tag [te::Tag bloc/puces]
		set premier 1
		foreach item $b(puces) {
		    set tags [list $tag [te::Aspect $w [list $tag]]]
		    if {!$premier} {
			$w insert end \n $tags
		    }
		    set premier 0
		    $w insert end "• " $tags
		    te::Segments $w $item [list $tag] $commandeLien
		}
	    }
	    bloc/code {
		set tags [list [te::Tag bloc/code]]
		$w insert end $b(code) [linsert $tags end [te::Aspect $w $tags]]
	    }
	}
	$w insert end \n\n
    }
}

proc te::Segments {w segments tags commandeLien} {
    foreach s $segments {
	switch -- [namespace tail [type get $s]] {
	    segment/texte {
		$w insert end $s(texte) [linsert $tags end [te::Aspect $w $tags]]
	    }
	    segment/marque {
		te::Segments $w $s(marque.contenu) \
			[linsert $tags end [te::Tag segment/marque style $s(marque.style)]] \
			$commandeLien
	    }
	    segment/lien {
		set tag [te::Tag segment/lien cible $s(lien.cible)]
		if {$commandeLien ne ""} {
		    $w tag bind $tag <1> [list {*}$commandeLien $s(lien.cible)]
		}
		te::Segments $w $s(lien.contenu) [linsert $tags end $tag] \
			$commandeLien
	    }
	}
    }
}

# ------------------------------------------------------------------
# Relire le widget : du texte à plages vers un document typé
# ------------------------------------------------------------------
#
# « $w dump » rend une suite plate de textes et de bascules de tags. te::lire
# en refait les plages, ne garde que les tags du document (bloc.…, segment/…),
# puis :
#   - les blocs sont les plages de para, titre, puces et code ; le texte hors
#     de tout bloc (tapé par l'utilisateur) devient des paragraphes ;
#   - dans chaque bloc, les plages de marque et de lien sont emboîtées.
# Deux plages qui se chevauchent ne forment pas un arbre :
#   -chevauchement couper   la plage intérieure est coupée en deux (défaut)
#   -chevauchement erreur   erreur, avec les positions
# Un tag étranger est ignoré ; les attributs (style, niveau, cible) passent
# par les constructeurs typés, qui refusent ce qui n'entre pas dans le type.

proc te::lire {w args} {
    set chevauchement couper
    foreach {opt val} $args {
	switch -- $opt {
	    -chevauchement {
		if {$val ni {couper erreur}} {
		    return -code error "bad -chevauchement \"$val\": must be couper or erreur"
		}
		set chevauchement $val
	    }
	    default {
		return -code error "bad option \"$opt\": must be -chevauchement"
	    }
	}
    }

    # 1. Le texte, et les plages des tags du document : {sorte note début fin}.
    set texte ""
    set ouverts [dict create]
    set plages {}
    set connus [dict create]
    foreach {cle valeur index} [$w dump -text -tag 1.0 end] {
	set pos [string length $texte]
	switch -- $cle {
	    text   {append texte $valeur}
	    tagon  {dict set ouverts $valeur $pos}
	    tagoff {
		if {![dict exists $ouverts $valeur]} continue
		if {![dict exists $connus $valeur]} {
		    if {[catch {te::Tete $valeur} b]} {
			return -code error -errorcode {TE TAG} \
			    "tag \"$valeur\" at [te::Index $texte\
			    [dict get $ouverts $valeur]]: $b"
		    }
		    # la note d'un titre est un niveau, celle d'une marque un
		    # nomstyle : le type actuel les valide
		    set T [dict get {{} {} para {} titre niveau puces {} code {}
			marque nomstyle lien string} [lindex $b 0]]
		    if {$T ne "" &&
			    [catch {type cast $T [lindex $b 1]} e]} {
			return -code error -errorcode {TE TAG} \
			    "tag \"$valeur\" at [te::Index $texte\
			    [dict get $ouverts $valeur]]: $e"
		    }
		    dict set connus $valeur $b
		}
		set b [dict get $connus $valeur]
		if {$b ne ""} {
		    lassign $b sorte attribut
		    lappend plages [list $sorte $attribut \
			    [dict get $ouverts $valeur] $pos]
		}
		dict unset ouverts $valeur
	    }
	}
    }
    set texte [string range $texte 0 end-1]	;# la fin de ligne de « end »

    # 2. Les blocs, dans l'ordre ; le texte hors bloc fait des paragraphes.
    set blocs [lsort -integer -index 2 [lmap p $plages {
	if {[lindex $p 0] ni {para titre puces code}} continue
	set p
    }]]
    set doc {}
    set pos 0
    foreach p [concat $blocs [list [list fin {} [string length $texte] {}]]] {
	lassign $p variante valeur a b
	foreach morceau [te::Libre $texte $pos $a] {
	    lassign $morceau x y
	    lappend doc [te::para {*}[te::Arbre $texte $plages $x $y $chevauchement]]
	}
	if {$variante eq "fin"} break
	set contenu [te::Arbre $texte $plages $a $b $chevauchement]
	switch -- $variante {
	    para  {lappend doc [te::para {*}$contenu]}
	    titre {lappend doc [te::titre $valeur {*}$contenu]}
	    code  {lappend doc [te::code [string range $texte $a [expr {$b - 1}]]]}
	    puces {
		set items {}
		set i $a
		while {$i < $b} {
		    set nl [string first \n $texte $i]
		    if {$nl < 0 || $nl > $b} {set nl $b}
		    set x $i
		    if {[string range $texte $x [expr {$x + 1}]] eq "• "} {
			incr x 2
		    }
		    lappend items [te::Arbre $texte $plages $x $nl $chevauchement]
		    set i [expr {$nl + 1}]
		}
		lappend doc [te::puces {*}$items]
	    }
	}
	set pos $b
    }
    return [te::document {*}$doc]
}

# Le texte libre de [a, b) : des morceaux séparés par des lignes vides, sans
# les fins de ligne autour.
proc te::Libre {texte a b} {
    set res {}
    set morceau [string range $texte $a [expr {$b - 1}]]
    set pos $a
    foreach partie [split [string map [list \n\n \u0001] $morceau] \u0001] {
	set x $pos
	set y [expr {$pos + [string length $partie]}]
	set pos [expr {$y + 2}]
	set t [string trim $partie \n]
	if {$t ne ""} {
	    set x [expr {$x + [string first $t $partie]}]
	    lappend res [list $x [expr {$x + [string length $t]}]]
	}
    }
    return $res
}

proc te::Index {texte pos} {
    set avant [string range $texte 0 [expr {$pos - 1}]]
    set ligne [expr {[regexp -all \n $avant] + 1}]
    set col [expr {$pos - ([string last \n $avant] + 1)}]
    return $ligne.$col
}

proc te::Ordre {p q} {
    lassign $p - - a1 b1
    lassign $q - - a2 b2
    expr {$a1 != $a2 ? $a1 - $a2 : $b2 - $b1}
}

# Les segments du morceau [a, b) : emboîte les plages de marque et de lien.
proc te::Arbre {texte plages a b chevauchement} {
    set ps {}
    foreach p $plages {
	lassign $p variante valeur x y
	if {$variante ni {marque lien}} continue
	set x [expr {max($x, $a)}]
	set y [expr {min($y, $b)}]
	if {$x < $y} {
	    lappend ps [list $variante $valeur $x $y]
	}
    }
    set ps [lsort -command te::Ordre $ps]
    set noeuds [dict create 0 [list racine {} $a $b]]
    set enfants [dict create 0 {}]
    set ids {0}
    set id 0
    while {[llength $ps] > 0} {
	set ps [lassign $ps p]
	lassign $p variante valeur x y
	while {[lindex $ids end] != 0
		&& $x >= [lindex [dict get $noeuds [lindex $ids end]] 3]} {
	    set ids [lrange $ids 0 end-1]
	}
	set haut [lindex $ids end]
	lassign [dict get $noeuds $haut] hv hval hx hy
	if {$y > $hy} {
	    if {$chevauchement eq "erreur"} {
		return -code error -errorcode {TE CHEVAUCHEMENT} \
		    "$hv \"$hval\" ([te::Index $texte $hx]-[te::Index $texte $hy])\
		    and $variante \"$valeur\" ([te::Index $texte $x]-[te::Index\
		    $texte $y]) overlap: they do not nest"
	    }
	    lappend ps [list $variante $valeur $hy $y]
	    set ps [lsort -command te::Ordre $ps]
	    set y $hy
	}
	incr id
	dict set noeuds $id [list $variante $valeur $x $y]
	dict set enfants $id {}
	dict lappend enfants $haut $id
	lappend ids $id
    }
    return [te::Segs $texte $noeuds $enfants 0]
}

proc te::Segs {texte noeuds enfants id} {
    lassign [dict get $noeuds $id] - - a b
    set res {}
    set pos $a
    foreach e [dict get $enfants $id] {
	lassign [dict get $noeuds $e] variante valeur ea eb
	if {$ea > $pos} {
	    lappend res [string range $texte $pos [expr {$ea - 1}]]
	}
	set dedans [te::Segs $texte $noeuds $enfants $e]
	if {$variante eq "lien"} {
	    lappend res [te::lien $valeur {*}$dedans]
	} else {
	    lappend res [te::m $valeur {*}$dedans]
	}
	set pos $eb
    }
    if {$b > $pos} {
	lappend res [string range $texte $pos [expr {$b - 1}]]
    }
    return $res
}
