# matrices — des matrices typées en cases, lues et fabriquées en C / typed matrices in cells, read and built in C

Application : des matrices 3×3 (rotations, produits, déterminants) déclarées en Tcl, rangées
comme des tableaux C, et traitées en C sans conversion élément par élément. Demande la v16h
(listes de taille fixe en forme compacte).

| Tcl (`matrices.tcl`) | C (`mat_types.h`, produit par `type {c!}export`) |
|---|---|
| `type def ligne3 list(3).double` | `typedef double ligne3[3];` (24 octets) |
| `type def {-compact}matrix3 list(3).ligne3` | `typedef ligne3 matrix3[3];` (72 octets) |
| `type def {-compact}lot list.matrix3` | un tableau `matrix3[n]`, lu en place |

`{-compact}` range toujours la valeur en cases : `set {matrix3}M {1 2 3} {4 5 6} {7 8 9}` donne
la matrice `{1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}`, dont les 9 doubles sont côte à côte.
Pour le script, rien ne change : `$M(1.2)`, `lindex $M 1 2`, `foreach`, `set M(1.2) 60.5`.

**Nommer la ligne** (`ligne3`) n'est pas obligatoire, mais c'est le nom que C donne pour lire les
cases : sans lui, le type des éléments s'appellerait `list(3)<double>`.

## Lire une matrice en C

Une matrice en cases est une liste de 3 cases, une par ligne : on demande les cases de type
`ligne3`, de `sizeof(ligne3)` octets.

```c
#include "tclAnnotatedApi.h"
#include "mat_types.h"

const void *cells;
Tcl_Size n;
Tcl_Obj *t = Tcl_NewStringObj("ligne3", -1);

Tcl_IncrRefCount(t);
rc = Tcl_AnnotatedGetCells(interp, obj, t, sizeof(ligne3), &cells, &n);
Tcl_DecrRefCount(t);
if (rc == TCL_OK && cells != NULL && n == 3) {
    const ligne3 *m = cells;          /* m[i][j], lecture seule */
    ...
}
```

Le pointeur vise les octets de la valeur (pas de copie) ; il vaut tant que la valeur n'est ni
modifiée ni convertie. `cells == NULL` : la valeur n'est pas en cases (une liste ordinaire, un
type sans `{-compact}`) ; `GetMatrix` dans `mat.c` la lit alors par l'API des listes.

Un **lot** (`list.matrix3`) se lit de même avec le type `matrix3` et `sizeof(matrix3)` : un
tableau de `n` matrices (`mat::lot_det`).

## Fabriquer une matrice en C

```c
matrix3 m = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
Tcl_Obj *t = Tcl_NewStringObj("matrix3", -1), *res;

Tcl_IncrRefCount(t);
rc = Tcl_AnnotatedCreateCells(interp, t, m, 3, sizeof(ligne3), &res);
Tcl_DecrRefCount(t);       /* res : une matrix3 en cases, refcount 0 */
```

Tcl vérifie l'arité (3 lignes) et la taille d'une ligne (24 octets).

## Les commandes de `mat.c`

| commande | rôle |
|---|---|
| `mat::trace M`, `mat::det M` | lecture d'une matrice (en cases ou non) |
| `mat::mul A B` | produit, rendu en matrix3 en cases |
| `mat::identite` | matrice identité fabriquée en C |
| `mat::lot_det L` | déterminant de chaque matrice d'un lot |
| `mat::layout` | `sizeof` vus par le compilateur |

`matrices.tcl` donne les types, `mat::init` (types et chargement), `mat::header` (l'en-tête
attendu) et les mêmes calculs en Tcl (`mat::tcl_trace`, `mat::tcl_det`, `mat::tcl_mul`,
`mat::rotz`).

## Ce que vérifie `test.tcl`

1. `mat_types.h` est bien `type {c!}export matrix3`, et les tailles de C et de Tcl sont égales.
2. Matrices construites en Tcl : C et Tcl donnent la même trace, le même déterminant, le même
   produit.
3. Matrices fabriquées en C : de type `matrix3`, en cases, lues en Tcl, écrites en Tcl puis
   relues en C.
4. Un lot de 1 002 matrices : en cases (72 octets par matrice), mêmes déterminants en C et en Tcl.
5. Hors cases (liste ordinaire, type sans `{-compact}`) : mêmes résultats par les listes.
6. Les erreurs : matrice 2×3, nombre invalide, ligne trop courte.

## Compiler et lancer

    gcc -shared -fPIC -O2 -DUSE_TCL_STUBS -I<src>/generic -I<build> -I<src>/unix \
        -o mat.so mat.c <build>/libtclstub.a
    tclsh test.tcl [chemin/vers/mat.so]
    tclsh bench.tcl [chemin/vers/mat.so]

Windows (MinGW64) : `gcc -shared -O2 -DUSE_TCL_STUBS -I<src>/generic -I<src>/win -o mat.dll mat.c
<build>/libtclstub.a`, puis `tclsh test.tcl` (le chargement prend `mat[info sharedlibextension]`
à côté du script).

Régénérer l'en-tête après un changement de types :

    tclsh <<< 'source matrices.tcl; mat::types; puts -nonewline [mat::header]' > mat_types.h

## Mesures (v16i, Linux x86-64, `-O2`, µs par opération)

| opération | µs |
|---|---:|
| déterminant, Tcl brut (`lassign`) | 1,0 |
| déterminant, Tcl typé en cases (`$M(i.j)`) | 1,6 |
| déterminant, Tcl typé ordinaire | 1,8 |
| déterminant, C sur les cases | 0,22 |
| déterminant, C sur une liste brute | 0,27 |
| produit, Tcl brut (`lindex`/`lset`) | 5,6 |
| produit, Tcl typé en cases | 23 |
| produit, Tcl typé ordinaire | 36 |
| produit, C (résultat en cases) | 1,1 |
| 10 000 déterminants, Tcl brut | 26 000 |
| 10 000 déterminants, Tcl typé en cases | 21 000 |
| 10 000 déterminants, C sur le lot en cases | 350 |
| 10 000 déterminants, C sur une liste brute | 4 100 |

Lu en C, un lot en cases va 12 fois plus vite qu'une liste brute : pas de conversion des 90 000
nombres. En Tcl, un déterminant sur une matrice typée coûte 1,6 fois le Tcl brut. Le produit
en Tcl typé reste 4 fois plus lent que le brut : il construit ses indices (`$A($i.0)`) et sa
matrice résultat à chaque appel.

## Limites

Les fonctions de `tclAnnotatedApi.h` sont des cales sur l'API publique : `Tcl_AnnotatedGetCells`
lit la forme compacte par les types d'objets enregistrés, et `Tcl_AnnotatedCreateCells` passe par
un tableau d'octets (une copie de plus). Le code C ne changera pas quand ce seront de vraies
fonctions de stub. Hypothèse : hôte little-endian, `double` de 8 octets (vérifié par les
`_Static_assert` de `mat_types.h`).
