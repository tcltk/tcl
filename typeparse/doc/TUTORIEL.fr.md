# Du prototype Tcl au C, en trois étapes

On écrit souvent un programme en trois temps : un **prototype** pour trouver
la bonne forme, puis une version **solide**, enfin une version **rapide** pour
ce qui compte. En Tcl, la deuxième et la troisième étape forcent d'ordinaire à
réécrire : des contrôles d'arguments à la main, puis une extension C dont la
moitié est de la colle entre Tcl et C.

Avec les types du prototype TIP 764, le même programme traverse les trois
étapes **sans que ses appelants changent** :

1. **Tcl** : le prototype ;
2. **Tcl typé** : les mêmes procédures, qui disent ce qu'elles prennent et ce
   qu'elles rendent ;
3. **Tcl et C** : les mêmes signatures, et les corps qui comptent écrits en C.

L'exemple est petit : des polygones (aire, périmètre, point intérieur,
déplacement, boîte englobante). Le code est dans `exemples/tutoriel` :

| fichier | rôle |
|---|---|
| `etape1_tcl.tcl`, `etape2_type.tcl`, `etape3_c.tcl` | les trois étapes |
| `essais.tcl 1\|2\|3` | les mêmes appels pour chaque étape (`essais_1.out`, `essais_2.out`) |
| `vitesse.tcl ?tcc?` | les trois étapes mesurées côte à côte |
| `chemins.tcl` | le coût d'un accès : `lindex`, `lassign`, `$p(x)`, `$s(a.x)`… |

## Étape 1 — le prototype

Un point est la liste `{x y}`, un polygone la liste de ses points :

```tcl
proc poly::aire {pg} {
    set a 0.0
    lassign [lindex $pg end] x0 y0
    foreach p $pg {
        lassign $p x y
        set a [expr {$a + $x0 * $y - $y0 * $x}]
        set x0 $x
        set y0 $y
    }
    expr {abs($a) / 2}
}
```

C'est du bon Tcl : court, lisible, et il marche. Deux choses lui manquent.

**Personne ne vérifie la forme des données.** Un point à trois valeurs passe
sans bruit, et l'aire est fausse :

```
poly::aire {{0.0 0.0 0.0} {1.0 0.0} {1.0 1.0}}   ;# 0.5, sans erreur
poly::aire {{0.0 0.0} {x 1.0} {1.0 1.0}}         ;# cannot use non-numeric string "x" as right operand of "*"
```

Le second message vient du fond de la boucle, sans dire quel argument était
fautif. Pour faire mieux, il faudrait écrire les contrôles soi-même : `llength
$p == 2`, `string is double`, sur chaque point de chaque appel.

**Une option coûte une page.** `poly::perimetre ?-ouvert? pg` demande d'analyser
`args` à la main : la boucle sur les options, `--`, le message d'une option
inconnue, celui d'un nombre d'arguments faux. Cela fait 29 lignes pour la
procédure, dont la moitié pour ses arguments.

## Étape 2 — le Tcl typé

### Nommer les données

```tcl
namespace eval poly {
    type def point    {{double}x; {double}y}
    type def polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}
```

- **`point` est un produit** : deux champs nommés, chacun un `double`.
- **`polygone` est une liste de points.**
- **Les types appartiennent à l'espace de noms** `::poly`, comme les procédures
  (`::poly::point`).

La valeur ne change pas de forme : un `point` est toujours la liste `{x y}`, et
un polygone la liste de ses points. Le type dit seulement ce qu'elle doit être.

### Des signatures

```tcl
proc {double}poly::aire {{polygone}pg} {(
    a = 0.0;  q = [lindex $pg end];
    foreach p $pg {( a = $a + $q(x) * $p(y) - $q(y) * $p(x);  q = $p )};
    abs($a) / 2
)}
```

- **`{double}poly::aire`** : le type du résultat. Il est vérifié au retour.
- **`{{polygone}pg}`** : la liste des arguments est elle-même un type (un
  produit). L'argument `pg` doit être un polygone. Une liste ordinaire qui en a
  la forme est acceptée et vérifiée, point par point, à l'appel.
- **`$p(x)`** : un champ se lit par son nom.
- **`( … )`** : le corps est une expression. On y écrit `a = …` pour affecter,
  `;` pour enchaîner, et la dernière expression est le résultat. `( x, y )`
  construit une liste, et `(xmin, ymin) = $p` la défait.

L'option qui coûtait une page s'écrit dans la signature :

```tcl
proc {double}poly::perimetre {{flag}-ouvert; {polygone}pg} {(
    l = 0.0;
    q = $ouvert ? [lindex $pg 0] : [lindex $pg end];
    foreach p [lrange $pg $ouvert end] {( l = $l + hypot($p(x) - $q(x), $p(y) - $q(y));  q = $p )};
    $l
)}
```

`{flag}-ouvert` est une option sans valeur, et la locale `ouvert` vaut 0 ou 1.
Avec cette seule ligne, on a aussi les préfixes uniques (`-ouv`), `--` et les
messages d'usage :

```
poly::perimetre -ferme $carre    ;# bad option "-ferme": must be -ouvert or --
poly::deplace $carre             ;# wrong # args: should be "poly::deplace pg dx ?dy?"
```

Un défaut s'écrit `?= valeur` : `{double}dy ?= 0.0`.

### Ce qu'on y gagne

Les deux erreurs de l'étape 1 sont arrêtées à l'appel, avec le nom de
l'argument :

```
poly::aire {{0.0 0.0 0.0} {1.0 0.0} {1.0 1.0}}   ;# argument "pg": ::poly::point needs 2 values but got 3
poly::aire {{0.0 0.0} {x 1.0} {1.0 1.0}}         ;# argument "pg": expected floating-point number but got "x"
```

Les **résultats sont les mêmes** qu'à l'étape 1 (`essais.tcl 1` et `essais.tcl 2`
ne diffèrent que par ces messages). Le fichier passe de 78 à 38 lignes.

Un appelant qui ne connaît pas les types n'y voit rien : un `point` reste une
liste (`lassign [poly::point 1.0 2.0] x y`), un polygone une liste de listes, et
une `boite` la liste de ses quatre champs.

### Ce que ça coûte

Dans le prototype, la vérification n'est pas gratuite. Ici, l'étape 2 est
**1,2 à 1,6 fois plus lente** que l'étape 1 (tableau ci-dessous) :

- **La liaison.** Une procédure typée coûte environ 0,4 µs de plus par appel.
- **La vérification.** Un polygone de 1 000 points donné en liste ordinaire
  est vérifié point par point à chaque appel (environ 80 µs). Une variable
  typée (`set {poly::polygone}G …`) ne l'est qu'une fois, quand on la remplit.
- **Les valeurs typées dans la boucle.** Chaque point lu porte son type, ce
  qui coûte un peu à chaque tour. La lecture elle-même est rapide : `$p(x)`
  coûte 65 ns contre 45 pour `lindex` ou `lassign` sur une liste ordinaire, et
  un chemin plus long comme `$s(a.x)` 110 ns, car il est analysé une fois puis
  gardé (`chemins.tcl`).

C'est le prix de la sûreté en Tcl. Il pèse peu sur la plupart des procédures,
et l'étape 3 le reprend là où il pèse.

## Étape 3 — le C, pour ce qui compte

```tcl
source ../cproc/cproc.tcl

namespace eval poly {
    type def point    {{double}x; {double}y}
    type def {-compact}polygone list.point
    type def boite    {{double}xmin; {double}ymin; {double}xmax; {double}ymax}
}

cproc {double}poly::aire {{polygone}pg} {
    double a = 0.0;
    point q = pg.at[pg.n - 1];

    for (Tcl_Size i = 0; i < pg.n; i++) {
        a = a + q.x * pg.at[i].y - q.y * pg.at[i].x;
        q = pg.at[i];
    }
    return fabs(a) / 2;
}
```

`cproc` a la syntaxe de `proc`. **La signature est celle de l'étape 2, mot pour
mot** ; seul le corps change de langue. Côté C :

| Tcl | C |
|---|---|
| `{point}P` | `point P` : la struct, avec ses champs `P.x`, `P.y` |
| `{polygone}pg` | `pg.at[i]`, `pg.n` : un tableau de `point` et sa longueur |
| `{double}dx`, `{flag}-ouvert` | `double dx`, `int ouvert` |
| `{boite}` en retour | `return (boite){…}` |
| `{polygone}` en retour | `return cproc_list_point(tableau, n)` |

La struct `point` vient du type `point` (`type {c!}export`). Tout ce qui fait
d'ordinaire une extension C est généré : la conversion des arguments, la
construction du résultat, la création des commandes, l'initialisation des
stubs.

**`{-compact}`** range un polygone **en cases**, comme un tableau C de `point`.
Le C le lit alors en place, sans copie ni conversion. C'est la seule
différence entre les types des étapes 2 et 3.

`poly::point` reste en Tcl : elle est trop simple pour gagner quoi que ce soit
en C.

**La compilation** se fait au premier appel, avec gcc par défaut. Pour tcc :
`cproc::config -cc tcc`, ou la variable d'environnement `CPROC_CC=tcc`. La
bibliothèque produite est gardée en cache et ne se recompile que si le C
change. Une erreur du compilateur renvoie au fichier `.tcl` et à la ligne du
corps.

Les **messages d'erreur sont ceux de l'étape 2**. La commande C lie seule les
appels simples ; pour tout le reste (une étiquette, une option, une valeur d'un
autre type, une erreur), elle passe la main à la procédure typée de même
signature. `essais.tcl 3` donne exactement la sortie de `essais.tcl 2`, avec gcc
comme avec tcc.

## Les trois étapes côte à côte

`tclsh vitesse.tcl tcc` (µs par appel, polygone de 1 000 sommets, Linux
x86-64) :

| | étape 1 : Tcl | étape 2 : Tcl typé | étape 3 : gcc | étape 3 : tcc |
|---|---:|---:|---:|---:|
| `point 1.0 2.0` | 0,26 | 0,68 | 0,70 | 0,70 |
| `aire`, liste ordinaire | 244 | 397 | 14 | 33 |
| `aire`, variable typée | 245 | 316 | **1,63** | 4,9 |
| `perimetre` | 329 | 405 | **6,7** | 10,1 |
| `contient` | 187 | 263 | **1,80** | 9,7 |
| `boite` | 451 | 530 | **11,7** | 11,9 |
| `deplace` | 281 | 365 | 217 | 234 |

- **Les calculs** (`aire`, `perimetre`, `contient`, `boite`) vont **40 à 150 fois
  plus vite qu'à l'étape 1**, quand le polygone est déjà typé. tcc est un peu
  plus lent que gcc, mais il compile en quelques dizaines de millisecondes.
- **Une liste ordinaire** passée à l'étape 3 est lue par le C, point par point
  (14 µs pour 1 000 points). Une variable typée l'est en confiance, et en place
  quand elle est en cases : c'est la récompense de l'étape 2.
- **`deplace`** ne gagne presque rien : le temps passe à fabriquer 1 000 points
  Tcl pour le résultat (120 µs), puis à les vérifier au retour (50 µs). Rendre
  directement des cases depuis le C est la prochaine amélioration de `cproc`.
- **`point`** : l'appel coûte plus que le calcul, à toutes les étapes typées.

| | étape 1 | étape 2 | étape 3 |
|---|---|---|---|
| lignes de code | 78 | 38 | 58 (dont 39 de C) |
| forme des arguments vérifiée | non | oui, à l'appel | oui, à l'appel |
| options, défauts, usage | à la main | par la signature | par la signature |
| appelants | — | inchangés | inchangés |

## La démarche

- **Typer d'abord les données** qui circulent (`point`, `polygone`) et les
  procédures d'interface. Les erreurs se déplacent vers l'appel, avec le nom de
  l'argument.
- **Garder dans des variables typées** ce que le programme manipule souvent.
  C'est ce qui rend le passage au C rentable.
- **Ne passer en C que les boucles** : les procédures où le calcul domine
  l'appel. On copie la signature, on écrit le corps en C, et on ajoute
  `{-compact}` aux listes que le C parcourt.

## Pour aller plus loin

- **Le langage de types** : `ANNOTATIONS.fr.md` (§ 5 les types, § 5.8 les
  espaces de noms, § 6.4 et 6.5 les procédures typées et leurs options, § 7 les
  chemins).
- **`cproc`** : `exemples/cproc/LISEZMOI.md` (correspondance complète des types,
  configuration, Windows).
- **Un exemple réel** : `exemples/tcllib_geometry`, le paquet `math::geometry`
  de tcllib passé par les trois étapes. Les 210 tests de tcllib passent à
  chacune.
