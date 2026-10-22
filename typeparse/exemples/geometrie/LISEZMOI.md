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
| aire (N points) | 2 976 | 34 671 | 1 226 | 451 |
| périmètre (N points) | 13 875 | 157 374 | 1 126 | 588 |
| englobant (N points) | 1 299 | 32 455 | 1 272 | 577 |
| dedans_tous (1000 points × polygone de 200) | 62 420 | 651 110 | 548 | 459 |
| Bézier, N points | 20 716 | 107 738 | 4 759 | 4 707 |

Voie octets, aire : 968 µs d'encodage en Tcl pour 20 µs de calcul en C. La frontière coûte plus que le calcul : il faut envoyer au C des opérations « grosses ».
