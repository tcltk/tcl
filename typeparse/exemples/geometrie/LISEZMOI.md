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
type def {-compact}pointC {{f64le}x; {f64le}y}   typedef struct {double x, y;} Point;
type def polyC  {{u32le}n;                   typedef struct {uint32_t n; Point pts[];} Poly;
                 {list($n).pointC}pts}
```

Deux voies vers le C, au choix (`set geo::voie octets|objet`) :

- **octets** : le polygone devient un `polyC` par `type cast list.pointC $P` (écrit directement en cases, copié d'un
  bloc si le polygone est lui-même en cases), puis `type binary encode polyC … -align natural` copie ces cases ;
  C lit un `(const Poly *)` ;
- **objet** : C reçoit la valeur annotée elle-même, vérifie son type (`geo: expected a polygone but got "nuage"`) et lit ses points.

Fonctions en C : `aire`, `perimetre`, `englobant`, `dedans_tous` (un nuage de points dans un polygone), `bezier_echantillons` (n points d'une Bézier cubique).

## Lancer

```
tclsh ../geometrie.tcl     ;# les exemples commentés (étape 2)
tclsh verifier.tcl         ;# les 22 exemples dans les 4 versions : mêmes résultats ?
tclsh bench.tcl ?N?        ;# vitesses comparées avec timerate (N = 10000 par défaut)
```

`bench.tcl` prend le Tcl nu (étape 1) pour référence et écrit des tableaux markdown : à côté de chaque
temps, `(/ 2.2)` veut dire 2,2 fois plus lent que le Tcl nu, `(x 37.1)` 37,1 fois plus rapide. Mesure du
24 octobre (v15x, Linux) :

| calcul (N = 10000, µs par appel) | 1 brut | 2 annoté | 3 C, octets | 3 C, objet |
|---|---:|---:|---:|---:|
| aire | 4 340 | 9 650 (/ 2.2) | 562 (x 7.7) | 391 (x 11.1) |
| périmètre | 17 532 | 40 533 (/ 2.3) | 472 (x 37.1) | 440 (x 39.8) |
| englobant | 1 276 | 3 595 (/ 2.8) | 450 (x 2.8) | 411 (x 3.1) |
| dedans_tous | 73 590 | 162 420 (/ 2.2) | 508 (x 144.8) | 498 (x 147.9) |
| Bézier | 21 059 | 60 683 (/ 2.9) | 324 (x 64.9) | 274 (x 76.8) |

L'étape annotée, rapportée au Tcl nu, au fil des versions (les écarts de ± 0.3 d'un passage à l'autre sont du
bruit de mesure) :

| calcul | v15s | v15u | v15v | v16b | v16c |
|---|---:|---:|---:|---:|---:|
| aire | / 3.4 | / 2.2 | / 1.8 | / 1.8 | / 1.2 à 1.5 |
| périmètre | / 2.7 | / 1.9 | / 2.1 | / 1.9 à 2.3 | / 1.1 à 1.4 |
| englobant | / 6.7 | / 3.7 | / 3.0 | / 2.7 à 3.1 | / 2.4 à 2.8 |
| dedans_tous | / 3.6 | / 2.6 | / 2.1 | / 1.9 à 2.2 | / 1.3 à 1.4 |
| Bézier | / 2.7 | / 2.4 | / 2.4 | / 2.2 à 2.5 | / 2.0 à 2.1 |

En forme compacte (`TCL_TYPES_COMPACT=1`), v16b → v16c : aire / 3.1 → / 1.9, périmètre / 3.0 → / 1.7,
dedans_tous / 3.9 → / 2.2.

Les voies C, rapportées au Tcl nu :

| calcul | octets v15v | octets v15x | objet v15v | objet v15x |
|---|---:|---:|---:|---:|
| aire | x 2.9 | x 7.7 | x 9.3 | x 11.1 |
| périmètre | x 12.6 | x 37.1 | x 39.6 | x 39.8 |
| Bézier | x 9.7 | x 64.9 | x 8.6 | x 76.8 |

v15u : `foreach` parcourt une liste typée sans refabriquer ses éléments ; v15v : lecture `$a(x)` par un cache du
champ, nombres typés lus directement par `expr` ; v15w : `type binary encode` / `decode` copient les cases ;
v15x : cast vers une liste en cases sans objet par élément (voie octets), `Tcl_AnnotatedCreateCells` (Bézier) ;
v16c : `X is T = v` et `nom = v` sans recherche par le nom, `$a(x)` lu comme nombre dans `expr`.

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

L'étape 2 (Tcl annoté, `lindex` et `$a(x)` à chaque point) est plus lente en forme compacte : chaque `lindex`
fabrique une vue. Depuis la v16c, `$a(x)` dans `expr` est lu directement dans la case ; l'écart est passé de 1,8
à 1,5 fois (aire, instructions comptées par callgrind : 114 → 76 millions, contre 52 millions en forme ordinaire).

## Écrit avec les fonctions de stub prévues

La voie objet de `geo.c` n'appelle plus l'interpréteur elle-même : elle est écrite avec les fonctions C prévues
pour le TIP (`Tcl_AnnotatedGetType`, `Tcl_AnnotatedGetCells`, `Tcl_AnnotatedGetParts`, `Tcl_AnnotatedCreate`,
`Tcl_AnnotatedCreateCells`). `bezier_echantillons` remplit un tableau C de `Point` et le donne à
`Tcl_AnnotatedCreateCells`, qui en fait une `polyligne` en cases : pas d'objet par point.
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

`englobant` et `bezier_echantillons` rendent directement une valeur annotée (`rectangle`, `polyligne`) : C construit la liste des points et la fait typer (`type cast`, en attendant une fonction de stub). Depuis la v15x, la Bézier passe par `Tcl_AnnotatedCreateCells` : 2,3 ms → 0,27 ms. Pour la Bézier, mesurée seule dans un interpréteur neuf : 3,8 ms avant (liste plate, puis `lmap` et typage en Tcl), 1,8 ms après. Dans `bench.tcl`, après les autres mesures, le même appel prend 4,5 ms : le tas déjà chargé par les grandes données pèse sur les allocations.
