# arbre_var.tcl --
#
#   Le type var : un champ lié au NOM d'une variable.
#   (Non exécuté sur un vrai Tcl 9.0 : les types et les liens sont vérifiés
#   sur la maquette de test.)

type def {tree(
    # type en paramètre :
    {type}T
)} {
    # valeur du noeud :
    {$T}value;
    # liste des noeuds fils :
    {list.tree($T)}children
}

set A 1
set B 2
set C 3

set {tree(var)}T A {
    {B {}}
    {C {}}
}

puts $T                       ;# ::A {{::B {}} {::C {}}}   les noms, pas les valeurs
puts $T(0)                    ;# 1      valeur de A
puts $T(1.0.0)                ;# 2      valeur de B
puts [expr {$T(1.0.0) eq $B}] ;# 1

set B 4
puts [expr {$T(1.0.0) eq $B}] ;# 1      toujours : le lien suit la variable

set T(1.0.0) 7                ;# écrit dans B (T ne change pas)
puts $B                       ;# 7

# var.int exige un entier ; var(typed).int exige en plus une variable typée.
set {int}n 5
set {list.var(typed).int}refs n
puts $refs(0)                 ;# 5
set refs(0) 6                 ;# n vaut 6
catch {set refs(0) 1.5} msg   ;# refusé par n elle-même

# Une variable locale vit tant que l'appel vit.
proc demo {} {
    set local 42
    set {list.var.int}r local ;# lien "frame:N:local"
    return $r
}
set r [demo]
catch {puts $r(0)} msg        ;# erreur : l'appel est terminé
