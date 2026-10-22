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

* **Voie rapide — `cs::stats_raw OCTETS`.** Tcl encode (`type binary encode Batch $b -align
  natural`) ; C lit les octets à travers un `(const Batch *)`. Pas de conversion champ par champ.
  C **vérifie** le contrat (longueur = `8 + 24 n`, alignement) au lieu de le supposer.
* **Voie objet — `cs::stats_obj BATCH`.** C reçoit la **valeur annotée** elle-même. Il vérifie son
  type, lit ses champs par l'API publique (pour elle, un produit annoté est la liste de ses
  champs, dans l'ordre du type) et remplit ses structures. Plus lent, mais sans encodage.

Dans l'autre sens, `cs::generate N SEED` fabrique un `Batch` en C ; `type binary decode Batch ...
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

## Performances : version annotée et Tcl classique (18 octobre)

`cstruct_classique.tcl` fait la même chose en Tcl 9.0 classique : un `Batch` est la liste
`{n items}`, la disposition C est écrite à la main (`binary format ix4qcx7`). Deux variantes :
nue (aucun contrôle) et contrôlée (les contrôles du type `Batch` : u32, double, u8, `n` = nombre
d'éléments). `bench_cstruct.tcl annote|classique` mesure chaque étape sur 1000 mesures (µs par
opération ; Linux, `-O2`) :

| Étape | Annoté | Classique | Rapport |
|---|---|---|---|
| Construction | 120 (`set {Batch}`, contrôlée) | 0,4 nue ; 405 contrôlée | annoté 3,4 × plus rapide que le classique contrôlé |
| Encodage | 33 400 (222 000 avant le correctif de `putPad`) | 350 | × 95 |
| C sur les octets | 1,8 | 1,8 | = |
| Décodage | 48 800 | 790 | × 62 |
| Calcul en Tcl : `$it(champ)` / `lassign` | 2 530 / 830 | 300 (`lassign`) | × 8 / × 2,8 |
| Voie objet `cs::stats_obj` (C lit la valeur annotée) | 111 | — (chaîne classique complète : 350) | annoté 3 × plus rapide |

Le Tcl classique tourne à la même vitesse sur un tclsh d'origine et sur le tclsh patché (écarts de
0 à 10 %, de l'ordre du bruit entre deux tours).

Diagnostic : le codec de `type binary` est écrit en Tcl (`layout/filebinary.tcl`) et interprète
la description du type champ par champ : 260 à 340 commandes Tcl par mesure, contre 1 à 6 en
classique. Le correctif de `putPad` (des octets, plus des chaînes) supprime un coût quadratique
dû à Tcl lui-même (mélange tableau d'octets / chaîne suivi de `string length`).

### Avec les codecs C de `type binary` (patchs types-binaires-codec-c et -decode)

Les dispositions fixes de `cstruct` sont maintenant encodées et décodées en C (voir
`noyau/LISEZMOI.md`). `bench_cstruct.tcl` prend la taille du lot en 3e argument (10 000 par
défaut) et le mode `annote-tcl` (codecs en Tcl seulement, `set ::tcl_typebinaryc 0`) :

    tclsh bench_cstruct.tcl annote|annote-tcl|classique ?typeparse? ?N?

Lot de 10 000 mesures, µs par opération, même machine pour les trois colonnes (fourchette
de trois tours) :

| Étape | annoté, couche Tcl | annoté, codec C | Tcl classique |
|---|---|---|---|
| Construction | 870 à 1 200 (contrôlée) | idem | 0,4 nue ; 3 700 à 4 400 contrôlée |
| Encodage | 330 000 | 950 à 1 200 | 3 600 à 4 000 (nu) |
| C sur les octets | 15 | 15 | 16 à 20 |
| Décodage | 485 000 | 3 000 à 3 700 | 7 300 à 10 500 |
| Calcul Tcl `$it(champ)` / `lassign` | 25 000 / 7 000 à 8 700 | idem | 2 600 à 3 000 (`lassign`) |
| Chaîne complète `cs::stats` | 338 000 | 850 à 900 | 3 600 nue ; 7 400 contrôlée |
| Voie objet `cs::stats_obj` | 440 à 570 | idem | — |

Avec les codecs C, la version annotée, qui contrôle tout, va plus vite que le Tcl classique
qui ne contrôle rien : 4 × pour l'encodage, 2 à 3 × pour le décodage, 4 × pour la chaîne
complète (8 × face au classique contrôlé). Le coût restant est côté Tcl : lire un champ par
`$it(champ)` (25 ms pour 10 000 mesures) reste 3 × plus lent que `lassign`, et 10 × plus lent
que le `lassign` classique.

## v15 : la voie objet lit le CONTENU du produit

Avec la règle B (v15), un produit annoté est UN élément pour l'API de listes : `Tcl_ListObjGetElements`
sur un `Batch` rend 1 élément. `cs::stats_obj` demande donc à Tcl le contenu brut du produit
(`::tcl::unsupported::content`, profond : les `Sample` d'un lot décodé deviennent bruts aussi), puis
lit ses champs comme avant. **Recompiler `cstruct.so` (ou `cstruct.dll`) après la mise à jour.**
Avec une fonction de stub (contenu d'une valeur annotée), ce serait un appel C direct.
