# t45 : une annotation LOCALE ({-v}, {what?}) n'est pas un type. Elle
# s'adresse à la commande appelée : elle ne doit pas suivre la valeur dans
# les variables, les retours, les conteneurs.
set mot toto

proc essai {titre script} {
    if {[catch {uplevel 1 $script} r]} {
	puts "$titre: ERREUR $r"
    } else {
	puts "$titre: $r"
    }
}

# 1. Commandes qui ne s'y intéressent pas : la valeur passe telle quelle.
essai list         {list {-v}$mot {what?}$mot}
essai string       {string length {-v}$mot}
essai expr         {expr {[string length {-v}$mot] + 1}}

# 2. Dans une procédure, la valeur est rangée de toutes les façons.
proc r1 {a} {return $a}
proc r2 {a} {set b $a; return $b}
proc r3 {a} {lappend L $a; return $L}
proc r4 {a} {append s $a x; return $s}
proc r5 {a} {dict set d k $a; return $d}
proc r6 {a} {set b "$a"; return $b}
proc r7 {a} {set b [string trim $a]; return $b}
proc r8 {a} {foreach e [list $a] {set b $e}; return $b}
foreach p {r1 r2 r3 r4 r5 r6 r7 r8} {
    essai $p [list $p {-v}$mot]
}

# 3. Au retour, et dans un conteneur.
essai retour       {set b [r1 {-v}$mot]; set b}
essai conteneur    {set l [list {-v}$mot]; set c [lindex $l 0]; set c}
essai catch        {catch {r1 {-v}$mot} res; set res}

# 4. La variable ne devient pas typée à cause de l'annotation locale.
proc rangee {v} {set x $v; list $x [type get x]}
proc rangee_s {v} {set {string}s $v; set s}
proc rangee_i {v} {set {int}n $v}
essai non_typee    {rangee {-v}$mot}

# 5. Une annotation locale sur une variable typée n'est pas un type non
#    plus : le contenu est validé comme si elle n'existait pas.
essai typee_ok     {rangee_s {-v}$mot}
essai typee_refus  {rangee_i {-v}$mot}
# (écrite directement sur la valeur d'un set, elle est refusée : voir t47)

# 6. Un vrai type, lui, garde son effet (cast sur une substitution).
set n 300
essai cast_refus   {list {int(u,8)}$n}
essai cast_ok      {list {int(s,16)}$n}

# 7. Un nom qui ressemble à un type (sans caractère invalide) reste une
#    erreur de frappe pour une variable : pas d'indulgence.
essai nom_inconnu  {set y {inconnu}$mot; set y}
