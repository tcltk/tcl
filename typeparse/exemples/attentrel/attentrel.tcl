# attentrel.tcl -- attributs, entités, relations, convolutions, sur TIP 764
#
# Une réécriture de la bibliothèque attentrel (Florent Merlet, 2025) :
#
#   attribut     une valeur : un type TIP 764 (aucun code à écrire)
#   entité       une identité : une classe TclOO, dont la fiche est un produit
#                typé (chaque écriture est vérifiée)
#   convolution  un événement : une procédure typée dont l'appel est gardé
#                comme une valeur (la convolution réifiée : type call, v16z),
#                dans un journal
#
# Les actants d'une convolution sont ses arguments, les circonstants ses
# options : {Service}demandeur est un actant 1,1 ; {string}-but ?= "" un
# circonstant 0,1 ; {T}args... un actant 1,+.
#
# Ce que la convolution fait à un actant est déclaré dans son type : une
# précondition et une postcondition, reliées par \conduit (effets.tcl) :
#
#   {(Achat(état) eq "créé" \conduit Achat(état) = "demandé")}x
#   {(Achat(état) eq "demandé" \conduit Achat(état) in {"validé" "refusé"})}x
#   {(Budget(reste) >= 0 \conduit Budget(reste) = \avant Budget(reste) - Achat(montant))}b
#   {(\lit Facture)}x        Facture \conduit Facture, inchangée
#   {(\produit Commande)}x   () \conduit Commande : le corps la crée
#   {(\déduit Commande)}x    Commande \conduit () : elle cesse d'exister
#
# Les chemins et les valeurs sont vérifiés à la définition. Le moteur vérifie
# l'avant avant le corps ; après, il applique les affectations que le corps
# n'a pas faites et vérifie le reste ; un échec défait tout. Une convolution
# passe par cinq phases (pour, à, par, de, fors), observables et
# accrochables. « processus » compose une chaîne de
# convolutions sans l'exécuter : chaque étape est nécessaire, contingente ou
# impossible, et les alternatives ouvrent des branches.

namespace eval ::attentrel {
    variable journal {}		;# les convolutions, dans l'ordre
    variable champs		;# convolution -> {champ type ...} (l'enregistrement)
    variable transitions {}	;# type d'état -> dict état -> suivants
    variable compte {}		;# classe -> nombre d'objets produits
    variable attributs		;# classe -> {attribut type ...}
    variable fiches		;# classe -> sa fiche (le texte du produit)
    variable effets		;# convolution -> {spec ...} (effets.tcl)
    variable classesConv	;# convolution -> {actant classe ...}
    variable circonstants	;# convolution -> ses options
    variable pile {}		;# les convolutions en cours : {nom phase} ...
    variable quand {}		;# "phase convolution" -> commande
    namespace export entite convolution transitions conduit produit \
	    detruit journal phrase possibles processus effet phase quand
}
source [file join [file dirname [info script]] effets.tcl]

# ----------------------------------------------------------------------
# entite NOM FICHE
#   Une classe TclOO ::NOM. Ses attributs sont un produit typé NOM_fiche ;
#   le type NOM désigne une référence à une entité (le nom de l'objet).
#     Service create ::achats {/nom}"Service achat" {/mel}achat@societe.org
#     achats get nom ; achats set mel x@y ; achats fiche
# ----------------------------------------------------------------------
proc ::attentrel::entite {nom fiche} {
    variable attributs
    variable fiches
    dict set attributs ::$nom {}
    dict set fiches ::$nom $fiche
    foreach f [Champs $fiche] {
	lassign [Champ $f] t n
	dict lappend attributs ::$nom $n $t
    }
    uplevel #0 [list type def ${nom}_fiche $fiche]
    uplevel #0 [list type def $nom string]
    oo::class create ::$nom [string map [list @T@ ${nom}_fiche] {
	variable fiche
	constructor {args} { set {@T@}fiche {*}$args }
	method get {champ} { set fiche($champ) }
	method set {champ valeur} {
	    ::attentrel::Permis [self] $champ $fiche($champ) $valeur
	    set fiche($champ) $valeur
	}
	method fiche {} { set fiche }
	method restaurer {f} { set fiche $f }
    }]
    return ::$nom
}

# ----------------------------------------------------------------------
# convolution NOM SIGNATURE CONSÉQUENCES
#   Définit la procédure NOM et le type NOM (la convolution réifiée : le
#   produit de ses actants, circonstants et produits), que la procédure
#   rend. Dans le corps, les actants sont des locales ; $evt n'existe
#   qu'après (type call, à la fin).
# ----------------------------------------------------------------------
proc ::attentrel::convolution {nom signature consequences} {
    variable champs
    variable effets
    variable classesConv
    variable circonstants
    set sig {}
    set produits {}
    set specs {}
    set classes {}
    set locaux {}
    set controles {}
    set circ {}
    # premier passage : les champs, les classes des actants
    foreach f [Champs $signature] {
	lassign [Champ $f] t n reste
	set local [string trimright [string trimleft $n -] .]
	lappend locaux $local
	if {[string match -* $n]} {lappend circ $local}
	if {[string index $t 0] ne "("} {
	    lappend sig $f
	    if {[info object isa class ::$t]} {
		dict set classes $local $t
		lappend controles [list $local $t]
	    }
	    continue
	}
	if {[catch {Effet $t} e]} {
	    return -code error "convolution $nom: $local: $e"
	}
	lassign $e cls av ap
	dict set classes $local $cls
	lappend specs [dict create local $local cls $cls avant $av apres $ap \
		texte $t res {}]
	if {$av eq "absent"} {
	    # () \conduit X : un produit, qui n'est pas un argument
	    if {$reste ne ""} {
		return -code error "convolution $nom: $local: a product has no\
			default"
	    }
	    lappend produits "{$cls}$local"
	} else {
	    lappend sig "{$cls}$n$reste"
	}
    }
    # second passage : chaque chemin à son actant, et la validation
    set r {}
    foreach spec $specs {
	set res {}
	foreach cote {avant apres} {
	    set a [dict get $spec $cote]
	    if {$a in {absent same}} continue
	    foreach p [Chemins $a] {
		if {[catch {Cible $nom $spec $p $classes} c]} {
		    return -code error $c
		}
		dict set res [Cle $p] [lindex $c 1]
	    }
	}
	dict set spec res $res
	ValiderEffet $nom $spec $classes $locaux
	lappend r $spec
    }
    dict set effets $nom $r
    dict set classesConv $nom $classes
    dict set circonstants $nom $circ
    set sig [join $sig "; "]
    # la procédure, pour lire son enregistrement ; les produits s'y ajoutent
    uplevel #0 [list proc $nom $sig {}]
    set record [dict get [type proc $nom] record]
    if {$produits ne ""} {
	append record "; " [join $produits "; "]
    }
    uplevel #0 [list type def $nom $record]
    dict set champs $nom {}
    foreach f [Champs $record] {
	lassign [Champ $f] t n
	dict lappend champs $nom $n $t
    }
    # le corps : les cinq phases du procès
    set act "\[[list ::attentrel::Act $locaux]\]"
    set corps "try \{\n"
    append corps "    [list ::attentrel::Phase $nom pour] $act\n"
    foreach c $controles {
	lassign $c local t
	append corps "    [list ::attentrel::Est $nom $local ::$t] \${$local}\n"
    }
    append corps "    set __av \[[list ::attentrel::Avant $nom] $act\]\n"
    append corps "    [list ::attentrel::Phase $nom à] $act\n"
    append corps "    [list ::attentrel::Phase $nom par] $act\n"
    append corps $consequences "\n"
    append corps "    [list ::attentrel::Phase $nom de] $act\n"
    append corps "    [list ::attentrel::Apres $nom] $act \$__av\n"
    append corps "\} on error \{__m __o\} \{\n"
    append corps "    if \{\[info exists __av\]\} \{[list ::attentrel::Defaire $nom] \$__av\}\n"
    append corps "    ::attentrel::Fermer\n"
    append corps "    return -options \$__o \$__m\n\}\n"
    append corps "set evt \[type call [list $nom]\]\n"
    append corps "::attentrel::Journaliser \$evt\n"
    append corps "::attentrel::Fermer [list $nom] \$evt\n"
    append corps "return \$evt\n"
    uplevel #0 "proc {$nom}[list $nom] [list $sig] [list $corps]"
    return $nom
}

# les actants de l'appelant : nom -> valeur ("" : pas encore produit)
proc ::attentrel::Act {noms} {
    set a {}
    foreach n $noms {
	upvar 1 $n v
	dict set a $n [expr {[info exists v] ? $v : ""}]
    }
    return $a
}

# --- les phases ---------------------------------------------------------
#
#   pour  préparation : l'appel est fait, les actants liés
#   à     transition inchoative : la précondition tient, les actants sont saisis
#   par   opération : le corps
#   de    transition égressive : les affectations, la postcondition
#   fors  achèvement : la convolution est un fait, au journal (irréversible)
#
# Un échec pendant pour, à, par ou de défait tout : les fiches des actants
# reprennent leur valeur, les produits disparaissent, rien n'entre au journal.

# entrer dans une phase ; la première ouvre la convolution
proc ::attentrel::Phase {conv ph act} {
    variable pile
    if {$ph eq "pour"} {
	lappend pile [list $conv pour]
    } else {
	lset pile end 1 $ph
    }
    Crochets $conv $ph $act
}

# l'achèvement (fors), puis la convolution sort de la pile ; sans argument :
# un échec, elle en sort seulement
proc ::attentrel::Fermer {{conv {}} {evt {}}} {
    variable pile
    if {$conv eq ""} {
	set pile [lrange $pile 0 end-1]
	return
    }
    lset pile end 1 fors
    try {
	Crochets $conv fors [dict create evt $evt]
    } finally {
	set pile [lrange $pile 0 end-1]
    }
}

proc ::attentrel::Crochets {conv ph act} {
    variable quand
    foreach k [list "$ph $conv" "$ph *"] {
	if {[dict exists $quand $k]} {
	    uplevel #0 [list {*}[dict get $quand $k] $conv $act]
	}
    }
}

# phase ?CONVOLUTION? : la phase où en est une convolution en cours ("" : elle
# ne l'est pas) ; sans argument, toutes les convolutions en cours
proc ::attentrel::phase {{conv {}}} {
    variable pile
    if {$conv eq ""} {return [join $pile]}
    for {set i [llength $pile]} {[incr i -1] >= 0} {} {
	if {[lindex $pile $i 0] eq $conv} {return [lindex $pile $i 1]}
    }
    return ""
}

# quand PHASE CONVOLUTION ?COMMANDE? : COMMANDE CONV ACTANTS est appelée à
# l'entrée de la phase (CONVOLUTION peut être *) ; ACTANTS : nom -> valeur,
# ou, à fors, evt -> la convolution réifiée. Une erreur avant fors fait
# échouer la convolution. Une commande vide retire le crochet.
proc ::attentrel::quand {ph conv args} {
    variable quand
    if {$ph ni {pour à par de fors}} {
	return -code error "bad phase \"$ph\": must be pour, à, par, de or fors"
    }
    if {![llength $args]} {
	return [expr {[dict exists $quand "$ph $conv"] ? [dict get $quand "$ph $conv"] : ""}]
    }
    set cmd [lindex $args 0]
    if {$cmd eq ""} {
	dict unset quand "$ph $conv"
    } else {
	dict set quand "$ph $conv" $cmd
    }
    return
}

# --- ce que la définition dit -----------------------------------------

# un côté d'un effet, lisible
proc ::attentrel::Cote {a cls} {
    switch -- $a {
	absent {return ()}
	same - {num 1} {return $cls}
    }
    return [Unparse $a]
}

# effet CONVOLUTION ?ACTANT? : ses effets, une ligne par actant
proc ::attentrel::effet {conv {champ {}}} {
    variable effets
    set r {}
    foreach spec [dict get $effets $conv] {
	dict with spec {}
	if {$champ ne "" && $local ne $champ} continue
	lappend r "$local: [Cote $avant $cls] \\conduit [Cote $apres $cls]"
    }
    join $r \n
}

# phrase CONVOLUTION : ce qu'elle fait, lu dans sa définition
proc ::attentrel::phrase {conv} {
    variable effets
    variable champs
    variable circonstants
    set dit {}
    set parts {}
    foreach spec [dict get $effets $conv] {
	dict with spec {}
	dict set dit $local 1
	if {$avant eq "absent"} {
	    set p "produit $local ($cls)"
	    if {$apres ne {num 1}} {append p ", tel que [Unparse $apres]"}
	} elseif {$apres eq "absent"} {
	    set p "déduit $local ($cls), qui cesse d'exister"
	} elseif {$apres eq "same"} {
	    set p "lit $local ($cls), sans le changer"
	} else {
	    set p "conduit $local ($cls)"
	    if {$avant ne {num 1}} {append p " : si [Unparse $avant]"}
	    append p ", alors [Unparse $apres]"
	}
	lappend parts $p
    }
    set avec {}
    set circ [dict get $circonstants $conv]
    foreach {n t} [dict get $champs $conv] {
	if {![dict exists $dit $n] && $n ni $circ} {
	    lappend avec "$n ($t)"
	}
    }
    set r "$conv [join $parts {, }]"
    if {$avec ne ""} {append r " ; avec [join $avec {, }]"}
    if {$circ ne ""} {append r " ; circonstances : [join $circ {, }]"}
    return "$r."
}

# possibles OBJET : les convolutions qui peuvent conduire cet objet, dans
# son état présent. Une condition sur un autre actant reste à voir : elle
# est donnée entre parenthèses.
proc ::attentrel::possibles {obj} {
    variable effets
    set r {}
    dict for {conv specs} $effets {
	foreach spec $specs {
	    dict with spec {}
	    if {$avant in {absent {num 1}} || $apres in {absent same}
		    || ![info object isa typeof $obj ::$cls]} continue
	    set env [dict create act [dict create $local $obj] res $res snap {}]
	    set ok 1
	    set si {}
	    foreach c [Conjoints $avant] {
		set siens 1
		foreach p [Chemins $c] {
		    if {[dict get $res [Cle $p]] ne $local} {set siens 0}
		}
		if {[regexp {\{var } $c]} {set siens 0}
		if {!$siens} {
		    lappend si [Unparse $c]
		} elseif {![Val $c $env]} {
		    set ok 0
		    break
		}
	    }
	    if {!$ok} continue
	    set l "$conv → [Unparse $apres]"
	    if {$si ne ""} {append l " (si [join $si { && }])"}
	    lappend r $l
	}
    }
    return $r
}

# un champ {TYPE}nom ?reste? -> {TYPE nom reste} (accolades équilibrées)
proc ::attentrel::Champ {f} {
    set d 0
    set n [string length $f]
    for {set i 0} {$i < $n} {incr i} {
	switch -- [string index $f $i] {
	    "\{" {incr d}
	    "\}" {if {[incr d -1] == 0} break}
	}
    }
    set t [string range $f 1 [expr {$i - 1}]]
    set r [string range $f [expr {$i + 1}] end]
    regexp {^(\S+)(.*)$} $r -> nom reste
    list $t $nom $reste
}

# les champs d'une signature, séparés par « ; » hors des accolades
proc ::attentrel::Champs {sig} {
    set r {}
    set cur {}
    set d 0
    foreach c [split $sig ""] {
	switch -- $c {
	    "\{" - "\[" {incr d}
	    "\}" - "\]" {incr d -1}
	}
	if {$c eq ";" && $d == 0} {
	    lappend r [string trim $cur]
	    set cur {}
	} else {
	    append cur $c
	}
    }
    if {[string trim $cur] ne ""} {lappend r [string trim $cur]}
    return $r
}

# un actant entité : un objet de la bonne classe (vide : circonstant absent)
proc ::attentrel::Est {conv champ classe v} {
    if {$v eq ""} return
    if {![info object isa object $v] || ![info object isa typeof $v $classe]} {
	return -code error "$conv: actant \"$champ\" must be\
		[string trimleft $classe :], got \"$v\""
    }
}

# ----------------------------------------------------------------------
# Les conséquences, le journal
# ----------------------------------------------------------------------

proc ::attentrel::Journaliser {evt} {
    variable journal
    lappend journal $evt
}

# ----------------------------------------------------------------------
# Les conséquences
# ----------------------------------------------------------------------

# transitions TYPE {état suivants état suivants ...} : l'automate d'un
# attribut d'état (une somme de littéraux)
proc ::attentrel::transitions {type table} {
    variable transitions
    dict set transitions $type $table
}

# conduit OBJET CHAMP ÉTAT : un changement d'état. L'automate est vérifié
# par la fiche elle-même : « $obj set état … » passe par la même règle.
proc ::attentrel::conduit {obj champ etat} {
    $obj set $champ $etat
}

# un changement d'état permis par l'automate du type de l'attribut
proc ::attentrel::Permis {obj champ de etat} {
    variable transitions
    set type [lindex [split [namespace tail [type get $de]] /] 0]
    if {$type eq "" || ![dict exists $transitions $type]} return
    if {[catch {type cast $type $etat}]} return	;# le type refusera
    set suivants {}
    if {[dict exists $transitions $type $de]} {
	set suivants [dict get $transitions $type $de]
    }
    if {$etat ni $suivants} {
	if {$suivants eq ""} {set suivants {none}}
	return -code error "[string trimleft $obj :]: $champ cannot go\
		from \"$de\" to \"$etat\" (allowed: [join $suivants {, }])"
    }
}

# produit CLASSE ARGS... : une entité nouvelle, nommée ::CLASSE1, ::CLASSE2…
proc ::attentrel::produit {classe args} {
    variable compte
    dict incr compte $classe
    $classe create ::$classe[dict get $compte $classe] {*}$args
}

# detruit OBJET : l'entité disparaît (le journal garde ses convolutions)
proc ::attentrel::detruit {obj} {
    $obj destroy
}

# journal : une ligne par convolution
proc ::attentrel::journal {} {
    variable journal
    variable champs
    set lignes {}
    foreach e $journal {
	set t [type get $e]
	set l $t
	foreach {n ty} [dict get $champs $t] {
	    set v $e($n)
	    if {[dict exists $champs [type get $v]]} {
		set v "<[type get $v]>"	;# une convolution dans une autre
	    } elseif {$v eq ""} {
		continue
	    }
	    append l " $n=[string trimleft $v :]"
	}
	lappend lignes $l
    }
    join $lignes \n
}

namespace import ::attentrel::*
