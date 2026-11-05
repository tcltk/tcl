# cproc — des procédures typées au corps en C

`cproc` a la syntaxe de `proc` ; seul le corps change de langue :

```tcl
proc  {point}decale {{point}p; {double}dx ?= 0} {( $p(x) + $dx, $p(y) )}

cproc {point}decale {{point}p; {double}dx ?= 0} {
    return (point){p.x + dx, p.y};
}
```

C'est la troisième étape de la démarche : un prototype en Tcl, puis le Tcl
typé, puis le C pour ce qui doit aller vite, **sans toucher aux appelants**. La
signature reste celle d'une procédure typée : liaison des arguments, défauts,
étiquettes (`decale {dx:}5 {p:}{0 0}`), options et drapeaux (`{flag}-strict`),
`--`, messages d'erreur, type du résultat, espace de noms des types. Tout cela
est fait par Tcl, comme pour `proc`.

`cproc` génère le reste, qu'on écrivait à la main avec `geom.c` :

- la fonction C pure : `static point decale(point p, double dx)`, que le C
  d'à côté peut appeler directement ;
- les structs, par `type {c!}export` (`point` est la struct du type `point`) ;
- la commande Tcl qui convertit chaque argument, appelle la fonction et rend le
  résultat ;
- l'initialisation de la bibliothèque, sans bibliothèque de stubs à lier.

Le C est compilé par **gcc** (MinGW-w64 sous Windows, clang convient aussi) ou
par **tcc** (`cproc::config -cc tcc`). Il est compilé au premier appel d'une des
procédures définies depuis la dernière compilation, ou par `cproc::build`. Le
résultat est mis en cache : une source donne une bibliothèque, et la même source
ne se recompile pas.

## Correspondance des types

| Tcl | C |
|---|---|
| `double`, `double(64)` | `double` |
| `double(32)` | `float` |
| `int` | `Tcl_WideInt` |
| `int(s,N)`, `int(u,N)` | `intN_t`, `uintN_t` |
| `flag` | `int` |
| `string` | `const char *` |
| un produit (`point`, `line`…) | sa struct (`type {c!}export`), par valeur |
| `list.T`, `list(n).T`, T un nombre ou un produit | `cproc_span_T`, soit `{ const T *at; Tcl_Size n; }` |
| un autre type, ou pas de type, **en retour** | `Tcl_Obj *` (NULL : la chaîne vide) |

Un alias ou un nominal se résout : `{polygon}pg`, où `polygon` est un alias de
`list.double`, donne un `cproc_span_double`. Un produit de champs de taille
fixe (`int(s,32)`, `double`, des produits, `list(3).double`) a sa struct ;
`int` n'a pas de taille fixe dans une struct, il faut `int(s,64)`.

**Les listes en cases** (`{-compact}`, ou l'interrupteur global
`tcl::unsupported::compact on`) sont lues **en place** : `pg.at` pointe sur les
cases, sans copie. Une liste ordinaire est copiée dans un tableau, libéré après
l'appel.

**Les résultats** de type liste se construisent avec les fonctions générées :
`cproc_list_double(tableau, n)` et `cproc_list_point(tableau, n)` (une par
struct utilisée).

**Les erreurs** : `cproc_error("format", ...)` puis `return`. Le message devient
l'erreur Tcl.

**Le C commun** (`#include`, fonctions d'aide) : `cproc::code {...}`, placé
avant les corps.

Les erreurs du compilateur renvoient au fichier Tcl et à la ligne du corps
(`#line`) :

```
test.tcl:33:88: error: incompatible types when returning type 'void *' ...
```

## Configuration

```tcl
cproc::config -cc gcc            ;# ou tcc, ou un chemin complet
cproc::config -cflags {-O2 -ffp-contract=off}   ;# le défaut pour gcc
cproc::config -includes {...}    ;# si tcl.h n'est pas trouvé
cproc::config -cachedir DIR      ;# défaut : CPROC_CACHE, %LOCALAPPDATA%\cproc ou ~/.cache/cproc
cproc::config -verbose 1         ;# affiche la commande de compilation
cproc::source                    ;# le C de la dernière compilation
```

`tcl.h` est cherché par `tcl::pkgconfig` (Tcl installé), puis près de `info
library` (Tcl compilé sur place). `tclAnnotatedApi.h` et `tclCompact.h`, pour
lire les cases, sont pris à la racine de `typeparse`.

Sous Windows avec MSYS2 : si `tclsh` n'est pas lancé depuis le shell MinGW64,
`gcc` peut ne pas être dans le PATH. Il faut alors donner le chemin complet :
`cproc::config -cc C:/msys64/mingw64/bin/gcc.exe`. Avec tcc, rien à lier : le
code généré initialise lui-même la table des stubs de Tcl (comme
`Tcl_InitStubs`). C'est ce qui permet à tcc d'éviter `libtclstub.a`, une archive
MinGW qu'il ne sait pas lire.

## Essais

```
tclsh test.tcl            # gcc
tclsh test.tcl tcc        # tcc : la même sortie (test.out)
```

Dans `../tcllib_geometry` :

- **`geometry_cproc.tcl`** : `geometry.tcl`, dont dix commandes lourdes sont des
  `cproc`. Ce sont 104 lignes de C, au lieu des 919 de `geom.c`, dont la moitié
  était de la colle. Les **210 tests de tcllib** passent, avec gcc comme avec tcc
  (`tclsh run_tests.tcl -cproc TCLLIB`, `CPROC_CC=tcc` pour tcc).
- **`bench_cproc.tcl`** : les vitesses.

Mesures en µs par appel, Linux x86-64. La colonne « C à la main » est `geom.c`,
dont les commandes remplacent les procédures et ne vérifient donc pas les types.

| | Tcl typé | C à la main | cproc gcc | cproc tcc |
|---|---:|---:|---:|---:|
| `distance` | 0,83 | 0,36 | 0,98 | 1,00 |
| `findLineIntersection` | 2,60 | 0,55 | 1,59 | 1,59 |
| `lineSegmentsIntersect` | 18,3 | 0,46 | 1,09 | 1,24 |
| `areaPolygon` 1 000 sommets, liste | 175 | 6,2 | 19,4 | 24,2 |
| `areaPolygon` 1 000 sommets, typé | 163 | 6,3 | 6,6 | 11,2 |
| `areaPolygon` 1 000 sommets, en cases | 165 | 0,83 | 1,35 | 3,2 |
| `lengthOfPolyline` (cases) | 235 | 5,3 | 5,8 | 8,1 |
| `pointInsidePolygonAlt` (cases) | 441 | 1,6 | 2,2 | 5,0 |
| `calculateDistanceToPolyline` (cases) | 2 878 | 13,5 | 14,3 | 39,3 |
| `rotate 30` (1 000 sommets) | 524 | 70 | 86 | 87 |

- **Fonctions lourdes.** Le gain est de 50 à 200 fois sur le Tcl typé. cproc
  compilé par gcc est au niveau du C écrit à la main. tcc est 1,5 à 3 fois plus
  lent que gcc dans les boucles, et il compile en quelques dizaines de ms au
  lieu de 200.
- **Fonctions minuscules** (`distance`). L'appel coûte plus que le calcul : la
  procédure typée lie et vérifie ses arguments (environ 0,6 µs), puis appelle la
  commande C. cproc n'y gagne rien sur le Tcl typé. Une liaison faite
  directement en C, pour les signatures simples, est la prochaine étape.
- **Liste ordinaire contre liste typée.** Une liste ordinaire est vérifiée à la
  liaison (2 000 nombres), puis copiée pour le C. Une liste déjà typée n'est que
  copiée. Une liste en cases n'est ni vérifiée ni copiée.

**Doubles.** Le C calcule en `double` : `5.0`, là où `expr`, sur des valeurs
écrites `0` et `10`, calcule en entiers et donne `5`. Un champ `{double}` garde
le texte qu'on lui donne. `geom.c` reproduisait cette règle d'`expr` (une
`struct Num`) ; un corps `cproc` est du C ordinaire. Les deux formes sont des
doubles égaux (les tests de tcllib passent), mais leurs chaînes diffèrent.

## Limites (prototype)

- Pas encore de somme ni de dict en argument (une somme en retour passe par un
  `Tcl_Obj *`), ni de `{var.T}`.
- Une compilation ne voit que les `cproc` en attente. Une fonction déjà compilée
  ne peut pas être appelée depuis le C d'une compilation suivante. Il faut
  définir ensemble ce qui s'appelle.
- tcc en mémoire (libtcc, sans fichier) : pas encore. Ici, tcc produit une
  bibliothèque partagée, comme gcc, mais vingt fois plus vite.
- Vérifié sous Linux, avec gcc 13 et tcc 0.9.28rc. Sous Windows, la même
  démarche (`-shared`, `load`, stubs initialisés par le code) est à confirmer.
