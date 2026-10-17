# minidll — mini-extension Tcl 9 / tiny Tcl 9 extension

Commandes : `mini::add`, `mini::reverse`, `mini::objinfo_raw` (liste), `mini::objinfo` (valeur annotée de type `objinfo`, fabriquée par `minidll.tcl`), `mini::peek` (voir `minidll.c`).
`mini::objinfo` donne les champs réels d'un `Tcl_Obj` (adresse, `refCount`, `bytes`, `length`,
`typePtr`, nom du type) ; `mini::peek` relit des octets du processus : on peut ainsi comparer
`tcl_obj.tcl` (`file binary`, `memory://`) à la mémoire vivante.

## Compiler

Linux (testé) :

    gcc -shared -fPIC -O2 -DUSE_TCL_STUBS -I<src>/generic -I<build> -I<src>/unix \
        -o minidll.so minidll.c <build>/libtclstub.a

Windows, MinGW64 (non testé ici, pas de compilateur croisé dans mon environnement) :

    gcc -shared -O2 -DUSE_TCL_STUBS -I<src>/generic -I<src>/win \
        -o minidll.dll minidll.c <build>/libtclstub90.a

(le nom exact de la bibliothèque de stubs dépend de votre build : cherchez `libtclstub*.a`).

## Charger

    source minidll.tcl; mini::init [chemin]     ;# charge la bibliothèque et définit le type objinfo
    tclsh test.tcl [chemin]                     ;# compare le modèle (tcl_obj.tcl) à la mémoire vivante

Note : dans `Minidll_Init`, les commandes sont créées avec des noms absolus (`::mini::add`) : un nom
relatif serait créé dans l'espace de noms courant, qui est `::mini` si `load` est appelé depuis une
procédure de cet espace.

Note : `format %d` est limité à 32 bits en Tcl 9 ; utiliser `%ld`/`%lld` pour les adresses.
