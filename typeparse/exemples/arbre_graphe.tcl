# arbre_graphe.tcl --
#
#   Illustration des valeurs annotées et des types paramétrés :
#   un arbre générique et un graphe pondéré générique.
#
#   Les types ci-dessous (définitions et valeurs d'exemple) ont été vérifiés
#   avec le moteur de types ; le reste du script est du Tcl ordinaire, que
#   je n'ai pas pu exécuter faute d'un Tcl 9.0 patché.

# ------------------------------------------------------------------
# Types
# ------------------------------------------------------------------

# Un arbre dont les valeurs sont de type T. La récursion passe par une
# liste (les enfants), donc l'arbre sans fils est une valeur finie.
type def {tree(
    {type}T
)} {
    {$T}value;
    {list.tree($T)}children
}

# Une arête vers un sommet, de poids W (int, double, ...).
type def {edge(
    {type}W
)} {
    {string}to;
    {$W}weight
}

# Un graphe : sommet -> liste d'arêtes sortantes. Les clés sont du texte,
# les valeurs sont typées.
type def {graph(
    {type}W
)} {
    dict.list.edge($W)
}

# ------------------------------------------------------------------
# Arbre
# ------------------------------------------------------------------

#         1
#        / \
#       2   3
#          / \
#         4   5

set {tree(int)}t value 1 children {
    {2 {}}
    {3 {{4 {}} {5 {}}}}
}

proc tree_sum {t} {
    lassign $t value children
    set total $value
    foreach c $children {
        incr total [tree_sum $c]
    }
    return $total
}

proc tree_depth {t} {
    set deepest 0
    foreach c [lindex $t 1] {
        set deepest [expr {max($deepest, [tree_depth $c])}]
    }
    return [expr {$deepest + 1}]
}

proc tree_print {t {indent 0}} {
    lassign $t value children
    puts "[string repeat {  } $indent]- $value"
    foreach c $children {
        tree_print $c [expr {$indent + 1}]
    }
}

tree_print $t
puts "somme   = [tree_sum $t]"       ;# 15
puts "hauteur = [tree_depth $t]"     ;# 3

# Le même type, avec un autre paramètre : un arbre de textes.
set {tree(string)}dirs value /usr children {
    {bin {}}
    {lib {{tcl9.0 {}}}}
}
tree_print $dirs

# Une valeur qui ne respecte pas le type est refusée à la construction.
if {[catch {set {tree(int)}bad value 1 children {{x {}}}} msg]} {
    puts "refusé : $msg"            ;# expected integer but got "x"
}

# ------------------------------------------------------------------
# Graphe pondéré
# ------------------------------------------------------------------

#   A --4--> B --1--> D
#   A --1--> C --2--> B
#            C --5--> D

# (les clés et les valeurs du dict sont des arguments séparés, sans accolades
# englobantes, comme pour tout constructeur de type)
set {graph(int)}g \
    A {{B 4} {C 1}} \
    B {{D 1}} \
    C {{B 2} {D 5}} \
    D {}

# Ajoute une arête (la variable globale g reste un graph(int) : une arête
# au poids non entier serait refusée).
proc add_edge {from to weight} {
    global g
    dict lappend g $from [list $to $weight]
    if {![dict exists $g $to]} {
        dict set g $to {}
    }
}

# Plus courts chemins depuis source (Dijkstra, file simple).
proc dijkstra {graph source} {
    set dist [dict create $source 0]
    set todo [list $source]
    set done {}
    while {[llength $todo] > 0} {
        # sommet en attente de distance minimale
        set best -1
        set at 0
        for {set i 0} {$i < [llength $todo]} {incr i} {
            set d [dict get $dist [lindex $todo $i]]
            if {$best < 0 || $d < $best} {
                set best $d
                set at $i
            }
        }
        set u [lindex $todo $at]
        set todo [lreplace $todo $at $at]
        if {[dict exists $done $u]} continue
        dict set done $u 1

        foreach edge [dict get $graph $u] {
            lassign $edge v w
            set nd [expr {$best + $w}]
            if {![dict exists $dist $v] || $nd < [dict get $dist $v]} {
                dict set dist $v $nd
                lappend todo $v
            }
        }
    }
    return $dist
}

puts "depuis A : [dijkstra $g A]"    ;# A 0 B 3 C 1 D 4

add_edge D E 2
puts "depuis A : [dijkstra $g A]"    ;# A 0 B 3 C 1 D 4 E 6

# Graphe(int) et graph(double) sont deux types distincts.
set {graph(double)}route \
    Paris {{Lyon 465.5}} \
    Lyon  {{Nice 470.2}} \
    Nice  {}
puts "depuis Paris : [dijkstra $route Paris]"   ;# Paris 0 Lyon 465.5 Nice 935.7

if {[catch {set {graph(int)}h A {{B 1.5}}} msg]} {
    puts "refusé : $msg"            ;# expected integer but got "1.5"
}
