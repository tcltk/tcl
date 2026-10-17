# cstruct — d'un produit typé Tcl à une structure C / from a typed Tcl product to a C struct

Application : traiter en C des lots de mesures de capteurs (statistiques sur des `Sample`),
avec les données construites, vérifiées et relues en Tcl.

| Tcl | C |
|---|---|
| `type def Sample {{u32le}id; {f64le}value; {u8}flags}` | `struct Sample { uint32_t id; double value; uint8_t flags; }` (24 octets) |
| `type def Batch {{u32le}n; {list($n).Sample}items}` | `struct Batch { uint32_t n; Sample items[]; }` (8 + 24 n) |
| `type def Summary {{u32le}count; {f64le}min; {f64le}max; {f64le}mean}` | `struct Summary {...}` (32 octets) |

**Le contrat : `-align natural`** donne au produit Tcl la disposition du compilateur C (remplissage
compris). `cs::layout` renvoie `sizeof` et `offsetof` vus par C ; `test.tcl` les compare à ce que
mesure Tcl.

## Deux voies pour passer le produit à C

* **Voie rapide — `cs::stats_raw OCTETS`.** Tcl encode (`file binary encode Batch $b -align
  natural`) ; C lit les octets à travers un `(const Batch *)`. Pas de conversion champ par champ.
  C **vérifie** le contrat (longueur = `8 + 24 n`, alignement) au lieu de le supposer.
* **Voie objet — `cs::stats_obj BATCH`.** C reçoit la **valeur annotée** elle-même. Il vérifie son
  type, lit ses champs par l'API publique (pour elle, un produit annoté est la liste de ses
  champs, dans l'ordre du type) et remplit ses structures. Plus lent, mais sans encodage.

Dans l'autre sens, `cs::generate N SEED` fabrique un `Batch` en C ; `file binary decode Batch ...
-align natural` le relit en produit annoté (`$b(items)`, `$it(value)`...).

## Ce que montre `test.tcl`

1. Les tailles et les décalages de C et de Tcl sont identiques.
2. Un lot construit en Tcl : les deux voies donnent le même `Summary` ; la mesure invalide est
   ignorée.
3. 1000 mesures fabriquées par C, relues en Tcl, recalculées en Tcl pur : mêmes résultats.
4. Les erreurs : lot encodé sans `-align natural`, valeur qui n'est pas un `Batch`, `n`
   incohérent (vérifié à l'encodage côté Tcl, et par C lui-même sur la voie objet).

## Limite actuelle (et ce que ferait une commande dans la table des stubs)

C ne peut pas, aujourd'hui, lire le type d'une valeur annotée : `TypeIs()` dans `cstruct.c`
interroge Tcl (`type get`). Avec `Tcl_GetAnnotation(obj)` dans la table des stubs, ce serait un
appel C direct ; avec `Tcl_NewAnnotatedObj(interp, valeur, type)`, C pourrait aussi *renvoyer* des
produits annotés au lieu d'octets (voir le commentaire de `exemples/minidll/minidll.c`).

Hypothèses : hôte little-endian, `Tcl_Size` de 8 octets. Les décalages et les tailles sont
vérifiés à l'exécution par `cs::layout`, pas seulement documentés.

## Compiler et lancer

    gcc -shared -fPIC -O2 -DUSE_TCL_STUBS -I<src>/generic -I<build> -I<src>/unix \
        -o cstruct.so cstruct.c <build>/libtclstub.a
    tclsh test.tcl [chemin/vers/cstruct.so]

Windows (MinGW64, non testé ici) : `gcc -shared -O2 -DUSE_TCL_STUBS -I<src>/generic -I<src>/win
-o cstruct.dll cstruct.c <build>/libtclstub*.a`, puis `tclsh test.tcl cstruct.dll`. Avec MSVC,
`_Alignof` et les membres flexibles demandent C11 : préférer MinGW.
