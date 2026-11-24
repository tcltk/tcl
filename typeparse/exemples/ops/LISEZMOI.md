# Exemple : une bibliothèque de spécialisations (TIP 764)

`produits_ordinaires.c` spécialise la lecture `P(i.champ)` dans les listes de
produits ordinaires (non compactes) dont les champs sont des nombres ou des
chaînes, avec la priorité 10 (les intégrées ont 0).

    make TCL=/chemin/vers/les/sources/tcl     # generic/ et unix/ construits
    tclsh9.0 test_ops.tcl

`test_ops.tcl` montre `type ops` avant et après le chargement, puis des
lectures, et le nombre d'appels de la spécialisation. Avec
`TCL_TYPE_OPS=check`, chaque lecture est aussi faite par le chemin général et
comparée.
