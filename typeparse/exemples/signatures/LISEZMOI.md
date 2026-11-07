# Les signatures de procédures

Plusieurs TIP ont proposé d'enrichir la liste d'arguments de `proc` ; aucune ne
s'est imposée. Dans TIP 764, une signature est simplement un **produit** : les
mêmes `{T}nom`, `?=`, sommes et `var.T` que partout ailleurs. Cet exemple montre
qu'elle couvre l'essentiel de ce que ces TIP demandaient.

| fichier | ce qu'il montre |
|---|---|
| `chercher.tcl` | une `lsearch` simplifiée : options exclusives, drapeaux, options à valeur, un défaut calculé qui lit un autre argument, un lien `-compte` vers une variable de l'appelant ; puis les erreurs que la signature produit seule |
| `chercher_classique.tcl` | la même procédure en Tcl classique, avec les mêmes contrôles écrits à la main |
| `liens.tcl` | l'exemple de TIP 460 ; `echanger` ; un journal avec un défaut `[commande]` et une variable par défaut |
| `bench_chercher.tcl` | le coût de l'analyse des arguments, signature contre analyse à la main |

## La signature

```tcl
proc {list.int}chercher {
    {"exact", "glob", "regexp"}-mode ?= glob;   ;# -exact, -glob ou -regexp
    {flag}-nocase;
    {flag}-all;
    {int}-debut ?= 0;
    {int}-fin ?= [expr {[llength $liste] - 1}]; ;# calculé à l'appel
    {var.int}-compte;                           ;# upvar : le nombre trouvé
    {list}liste;
    {string}motif
} { ... }
```

Ces huit lignes remplacent la cinquantaine de lignes d'analyse de
`chercher_classique.tcl` (préfixes, exclusion, contrôle des entiers, `upvar`,
message d'usage). Le message d'usage est produit par la signature :

```
wrong # args: should be "chercher ?-exact|-glob|-regexp? ?-nocase? ?-all? ?-debut value? ?-fin value? ?-compte varName? ?--? liste motif"
```

## Ce que demandaient les TIP

| besoin | TIP | ici |
|---|---|---|
| `args` ailleurs qu'en dernier (forme de `lsearch` : options, puis arguments) | 288 (rejetée) | les options `-x` en tête de l'appel, puis les arguments |
| valeurs par défaut, arguments obligatoires | 457 | `{int}n ?= 0` ; sans `?=`, obligatoire |
| arguments nommés | 457, 479 | `-debut 2`, ou une étiquette à l'appel : `decale $p {dy:}5` |
| drapeaux | 457 (`-switch`) | `{flag}-nocase` |
| options exclusives | — | une somme : `{"exact", "glob", "regexp"}-mode` |
| lien vers une variable de l'appelant | 457 (`-upvar`), 460 (`*a`) | `{var.int}a`, et en option `{var.int}-compte` |
| défaut calculé à l'appel | — | `?= [commande]`, qui peut lire les autres arguments |
| assertions de type | 480 | chaque argument a un type, vérifié à l'entrée |
| type du résultat | — | `proc {list.int}chercher` |
| message d'usage | — | produit par la signature |

**TIP 460** proposait `proc bar {*a *b} {incr a; incr b}`. Ici :

```tcl
proc bar {{var.int}a; {var.int}b} { incr a; incr b }
```

avec en plus le contrôle du contenu (`bar v1 s` avec `s = abc` échoue avant le
corps, sans rien modifier), et un nom par défaut (`{var.list}-dans ?= journal`),
que TIP 460 interdisait.

## Le coût

`bench_chercher.tcl` compare trois versions du même corps. Sur Linux, x86-64 :
signature typée 4,9 µs, Tcl classique 4,5 à 4,9 µs, sans analyse ni contrôle
1,5 µs. Les deux premières sont du même ordre : la signature ne coûte pas plus
que l'analyse qu'on écrirait de toute façon, et elle tient en huit lignes.
Deux pistes d'optimisation restent ouvertes : un défaut calculé est réévalué par
`Tcl_EvalObjEx` à chaque appel (0,4 µs ici), et une écriture dans une variable
typée (`set motif ...` sur un `{string}`) coûte nettement plus qu'une écriture
nue.

## Pas encore

- `apply` et les méthodes TclOO n'acceptent pas encore de signature typée.
- `{list.T}args` ailleurs qu'en dernier est accepté à la définition mais
  inutilisable à l'appel : il faudra le refuser, ou adopter la règle de TIP 288.
