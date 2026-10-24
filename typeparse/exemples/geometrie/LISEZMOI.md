# Géométrie en trois étapes

Le même petit morceau de `math::geometry` (tcllib), écrit trois fois, selon le principe « script + compilé » :

| Étape | Fichier | Ce que c'est |
|---|---|---|
| 1. prototype, Tcl nu | `geo_brut.tcl` | listes plates à la tcllib : un point est `{x y}`, rien ne distingue un segment d'une droite |
| 2. prototype annoté | `../geometrie.tcl` | chaque objet a son type (`point`, `segment`, `droite`, `polygone`…) ; corps `expr` (TIP 759) |
| 3. application | `geo_c.tcl` + `geo.c` | l'étape 2, dont cinq calculs lourds passent en C ; le reste est inchangé |

Les trois étapes ont les mêmes noms (`geo::aire`, `geo::projete`…), les mêmes arguments et les mêmes résultats.

## À l'étape 3

Le type Tcl décrit la structure C :

```
type def pointC {{f64le}x; {f64le}y}         typedef struct {double x, y;} Point;
type def polyC  {{u32le}n;                   typedef struct {uint32_t n; Point pts[];} Poly;
                 {list($n).pointC}pts}
```

Deux voies vers le C, au choix (`set geo::voie octets|objet`) :

- **octets** : le polygone est encodé par `type binary encode polyC … -align natural`, et C lit un `(const Poly *)` ;
- **objet** : C reçoit la valeur annotée elle-même, vérifie son type (`geo: expected a polygone but got "nuage"`) et lit ses points.

Fonctions en C : `aire`, `perimetre`, `englobant`, `dedans_tous` (un nuage de points dans un polygone), `bezier_echantillons` (n points d'une Bézier cubique).

## Lancer

```
tclsh ../geometrie.tcl     ;# les exemples commentés (étape 2)
tclsh verifier.tcl         ;# les 22 exemples dans les 4 versions : mêmes résultats ?
tclsh bench.tcl ?N?        ;# vitesses comparées avec timerate (N = 10000 par défaut)
```

`bench.tcl` prend le Tcl nu (étape 1) pour référence et écrit des tableaux markdown : à côté de chaque
temps, `(/ 2.2)` veut dire 2,2 fois plus lent que le Tcl nu, `(x 17.5)` 17,5 fois plus rapide. Mesure du
24 octobre (v15u, Linux) :

| calcul (N = 10000, µs par appel) | 1 brut | 2 annoté | 3 C, octets | 3 C, objet |
|---|---:|---:|---:|---:|
| aire | 4 722 | 10 406 (/ 2.2) | 1 281 (x 3.7) | 470 (x 10.0) |
| périmètre | 23 506 | 44 503 (/ 1.9) | 1 346 (x 17.5) | 477 (x 49.3) |
| englobant | 1 382 | 5 163 (/ 3.7) | 1 180 (x 1.2) | 510 (x 2.7) |
| dedans_tous | 79 026 | 203 659 (/ 2.6) | 698 (x 113.3) | 538 (x 146.9) |
| Bézier | 21 271 | 50 341 (/ 2.4) | 2 101 (x 10.1) | 2 341 (x 9.1) |

En v15s, l'étape annotée était à / 3.4 (aire), / 2.7 (périmètre), / 6.7 (englobant), / 3.6 (dedans_tous),
/ 2.7 (Bézier) : la v15u parcourt une liste typée sans refabriquer ses éléments à chaque boucle.

## L'en-tête C vient des types

`geo_types.h` est produit par `type {c!}export` depuis les `type def` (`point`, `pointC`, `polyC`), avec des
`_Static_assert` qui arrêtent la compilation si le compilateur range autrement que Tcl :

```
tclsh geo_types.tcl > geo_types.h
```

## Forme compacte

Avec `tcl::unsupported::compact on` (ou `TCL_TYPES_COMPACT=1 tclsh bench.tcl`), ou un type déclaré `type def {-compact}polygone list.point`, un polygone est rangé en
cases de 16 octets, exactement un tableau de `point` : la voie objet de `geo.c` le lit en place
(`tclCompact.h`), sans relire chaque point.

| Calcul (N = 10000, µs par appel) | voie objet, forme ordinaire | voie objet, forme compacte |
|---|---:|---:|
| aire | 564 | 29 |
| périmètre | 629 | 94 |
| englobant | 568 | 27 |

L'étape 2 (Tcl annoté, `lindex` et `$a(x)` à chaque point) est plus lente en forme compacte (aire : 15 → 23 ms) :
chaque accès fabrique une vue et un nombre.

## Écrit avec les fonctions de stub prévues

La voie objet de `geo.c` n'appelle plus l'interpréteur elle-même : elle est écrite avec les fonctions C prévues
pour le TIP (`Tcl_AnnotatedGetType`, `Tcl_AnnotatedGetCells`, `Tcl_AnnotatedGetParts`, `Tcl_AnnotatedCreate`).
En attendant qu'elles soient de vraies fonctions de stub, `typeparse/tclAnnotatedApi.h` les fournit avec l'API
publique de Tcl 9 ; ce jour-là, on retire la ligne `#include "../../tclAnnotatedApi.h"`, rien d'autre.

## Compiler `geo.c`

Linux :

```
gcc -shared -fPIC -O2 -DUSE_TCL_STUBS -I<src>/generic -I<src>/unix -o geo.so geo.c <build>/libtclstub.a -lm
```

Windows (MinGW) :

```
gcc -shared -O2 -DUSE_TCL_STUBS -I<src>/generic -I<src>/win -o geo.dll geo.c <build>/libtclstub.a
```

## Mesures (Linux, N = 10000, µs par appel)

| Calcul | 1 brut | 2 annoté | 3 C, octets | 3 C, objet |
|---|---:|---:|---:|---:|
| aire (N points) | 3 288 | 32 702 | 1 002 | 461 |
| périmètre (N points) | 14 345 | 190 730 | 1 296 | 555 |
| englobant (N points) | 1 526 | 26 520 | 897 | 516 |
| dedans_tous (1000 points × polygone de 200) | 57 339 | 766 846 | 561 | 532 |
| Bézier, N points | 22 716 | 114 739 | 4 458 | 4 805 |

Voie octets, aire : 1 029 µs d'encodage en Tcl pour 19 µs de calcul en C. La frontière coûte plus que le calcul : il faut envoyer au C des opérations « grosses ».

`englobant` et `bezier_echantillons` rendent directement une valeur annotée (`rectangle`, `polyligne`) : C construit la liste des points et la fait typer (`type cast`, en attendant une fonction de stub). Pour la Bézier, mesurée seule dans un interpréteur neuf : 3,8 ms avant (liste plate, puis `lmap` et typage en Tcl), 1,8 ms après. Dans `bench.tcl`, après les autres mesures, le même appel prend 4,5 ms : le tas déjà chargé par les grandes données pèse sur les allocations.
