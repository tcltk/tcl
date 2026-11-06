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

**L'appel** est lié en C quand il est simple : arguments positionnels, défauts
constants, valeurs nues valides ou déjà du bon type. Le résultat est rendu
typé. Pour tout le reste, la commande C passe la main à la procédure typée de
même signature, avec les mêmes mots, qui lie, vérifie et donne les messages
exactement comme `proc`. Cela couvre les étiquettes, les options données, une
valeur d'un autre type et toute erreur (voir « cproc en trois étapes »).

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
cproc::config                    ;# la configuration (un dict)
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
| `distance` | 0,82 | 0,33 | **0,28** | 0,31 |
| `findLineIntersection` | 2,46 | 0,53 | 1,09 | 1,11 |
| `lineSegmentsIntersect` | 18,1 | 0,47 | **0,32** | 0,44 |
| `areaPolygon` 1 000 sommets, liste | 166 | 6,0 | 8,2 | 18,3 |
| `areaPolygon` 1 000 sommets, typé | 159 | 6,5 | 8,5 | 18,1 |
| `areaPolygon` 1 000 sommets, en cases | 161 | 1,65 | 1,66 | 4,4 |
| `lengthOfPolyline` (cases) | 230 | 5,3 | 5,3 | 6,7 |
| `pointInsidePolygonAlt` (cases) | 434 | 1,6 | 1,6 | 4,1 |
| `calculateDistanceToPolyline` (cases) | 2 861 | 13,5 | 13,1 | 38,4 |
| `rotate 30` (1 000 sommets) | 516 | 72 | 88 | 90 |

- **Fonctions lourdes.** Le gain est de 50 à 200 fois sur le Tcl typé. cproc
  compilé par gcc est au niveau du C écrit à la main. tcc est 1,5 à 3 fois plus
  lent que gcc dans les boucles, et il compile en quelques dizaines de ms au
  lieu de 200.
- **Fonctions minuscules** (`distance`, `lineSegmentsIntersect`). Avec la
  liaison directe, cproc va aussi vite que `geom.c`, voire un peu plus vite, et
  il vérifie les types (une valeur d'un autre type passe par la procédure
  typée, qui la refuse avec son message). Avant la liaison directe, la
  procédure typée coûtait 0,6 µs par appel : `distance` prenait 0,98 µs.
- **Résultat liste ou somme** (`findLineIntersection`, `rotate`). Le résultat
  est construit en objets Tcl, puis contrôlé par une procédure qui n'a que le
  type de retour. C'est le prochain chemin à raccourcir : rendre des cases
  depuis le C.
- **Liste ordinaire contre liste typée.** Une liste ordinaire est lue
  strictement par le C, élément par élément. Une liste du bon type est lue en
  confiance. Une liste en cases est lue en place.

**Doubles.** Le C calcule en `double` : `5.0`, là où `expr`, sur des valeurs
écrites `0` et `10`, calcule en entiers et donne `5`. Un champ `{double}` garde
le texte qu'on lui donne. `geom.c` reproduisait cette règle d'`expr` (une
`struct Num`) ; un corps `cproc` est du C ordinaire. Les deux formes sont des
doubles égaux (les tests de tcllib passent), mais leurs chaînes diffèrent.

## cproc en trois étapes

`cproc` est lui-même passé par les trois étapes du tutoriel
(`doc/TUTORIEL.fr.md`). Les trois versions sont dans ce répertoire :

| fichier | étape |
|---|---|
| `cproc1_tcl.tcl` | 1. Tcl : des dicts (`dict get $k how`), des arguments sans type |
| `cproc2_type.tcl` | 2. Tcl typé : le même générateur, le même C produit |
| `cproc.tcl` | 3. la liaison des appels en C, avec repli sur la procédure typée |
| `liaison.tcl ?CC?` | les étapes 2 et 3 comparées, appel par appel (`liaison.out`) |

**Étape 2.** Les données du générateur reçoivent un type :

- **un champ de signature** : `field {ttext; name; def}`, où `def` est une
  somme : `none`, une constante ou un script ;
- **ce que voit le C** : `kind`, une somme `{cstruct}struct`, `{cspan}span`,
  `{cnum}num`, `"str"` ou `"obj"`, lue par sa variante
  (`switch [Variant $k]`, puis `$k(struct.name)`, `$k(span.elem.struct.qual)`) ;
- **un bloc de `type {c!}export`** : `cblock`. **Une procédure à compiler** :
  `cdef`. **La configuration** : `cconfig`, dans une variable sous contrat.

Chaque procédure interne a une signature typée.
`proc {kind}Classify {{flag}-ret; {string}ns; {string}t}` remplace l'ancien
argument positionnel `isRet`. `cproc::config` prend ses options par sa signature
et vérifie ses valeurs : `-verbose x` donne `option "-verbose": expected integer
but got "x"`, et `-foo` donne `bad option "-foo": must be -cachedir, -cc,
-cflags, -includes, -verbose or --`. Le C produit est le même qu'à l'étape 1, aux
détails près.

L'étape 2 a payé à l'étape 3. Pendant l'écriture de la liaison directe, une
variable déclarée `{kind}k` a été réutilisée comme compteur : le contrat l'a
signalé à la première exécution (`value "0" names no variant of the strict sum
::cproc::kind`), au lieu d'un C faux plus loin.

Typer `cproc` a aussi fait apparaître **trois points du modèle**, notés pour
le noyau :

- **Une somme ne se convertit pas en `string`.** Une variante littérale qu'on
  veut rendre comme simple texte s'écrit `{}$v`.
- **`return {v:}[list …]` perd une somme stricte imbriquée.** Si la charge de
  la variante contient une somme stricte (`return {span:}[list $el $en]`), la
  variante de `$el` ne passe pas. `set {cspan}s $el $en; return {span:}$s` la
  garde.
- **`switch -regexp -matchvar` fait expirer l'étiquette.** Son corps n'est pas
  compilé, et l'étiquette d'un `return {v:}…` expire avant le contrôle du
  retour. Sans `-matchvar`, tout va bien.

**Étape 3.** La partie de `cproc` qui tourne souvent n'est pas le générateur
(11 ms pour dix procédures, quand la compilation en prend 50 à 200), mais
l'appel. Celui-ci passait par la procédure typée, environ 0,6 µs, avant la
commande C. Désormais, la commande du nom de la procédure est en C :

- **elle lit seule** les appels simples : arguments positionnels, défauts
  constants (`?= 0.0`), valeurs nues valides ou annotées du bon type. Une valeur
  annotée se reconnaît à sa représentation, le couple (V, T) où T a pour chaîne
  le nom du type. Une valeur du bon type est lue en confiance ; une valeur nue
  l'est strictement (le nombre de champs, des nombres, les bornes des entiers) ;
- **elle rend un résultat typé** : la même représentation (V, T), avec le T
  d'une valeur exemple du type de retour ;
- **pour tout le reste, elle passe la main** à la procédure typée de même
  signature (un nom caché, `::poly::_cproc3_aire`), avec les mêmes mots, et
  donc le même nom d'appel dans les messages. Cela couvre les étiquettes, les
  options données (`-ouvert`, `--`), une valeur d'un autre type, un argument
  qui manque ou de trop, et toute valeur que la lecture stricte refuse.

Les résultats, leur type et les erreurs sont identiques à ceux de l'étape 2,
sur 24 appels choisis pour emprunter chaque chemin (positionnels, défauts,
étiquettes, options et préfixes, valeurs nues, typées, d'un autre type,
erreurs) : `tclsh liaison.tcl` les rejoue. Le tutoriel et les 210 tests de tcllib passent, avec gcc comme avec
tcc, et valgrind est propre : les objets gardés par le C sont libérés à la
sortie de Tcl.

Les gains se voient dans les deux tableaux de mesures : celui ci-dessus, pour la
géométrie, et `doc/TUTORIEL.fr.md`, où `aire` sur une liste ordinaire passe de
50 à 13 µs.

## Limites (prototype)

- Pas encore de somme ni de dict en argument (une somme en retour passe par un
  `Tcl_Obj *`), ni de `{var.T}`.
- La liaison directe lit `o->internalRep` d'une valeur annotée : c'est ce que
  fera la vraie API C des valeurs annotées (`Tcl_AnnotatedGetType`…), quand
  elle sera dans les stubs.
- Une option d'une `cproc` doit avoir un défaut constant (ou être un `flag`).
- Une compilation ne voit que les `cproc` en attente. Une fonction déjà compilée
  ne peut pas être appelée depuis le C d'une compilation suivante. Il faut
  définir ensemble ce qui s'appelle.
- tcc en mémoire (libtcc, sans fichier) : pas encore. Ici, tcc produit une
  bibliothèque partagée, comme gcc, mais vingt fois plus vite.
- Vérifié sous Linux, avec gcc 13 et tcc 0.9.28rc. Sous Windows, la même
  démarche (`-shared`, `load`, stubs initialisés par le code) est à confirmer.
