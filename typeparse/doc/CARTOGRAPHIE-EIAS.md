# Cartographie des tensions avec EIAS (v16s)

*5 novembre 2026. Mesuré sur le prototype v16s par `doc/cartographie/sonde.tcl` (sortie :
`sonde.out` ; les identifiants A1, B8… renvoient à ses lignes) et `sonde_interp.tcl` (sous
valgrind).*

## 1. EIAS, et ce que serait EI2S

**EIAS** (*everything is a string*) : une valeur Tcl **est** sa chaîne. Deux valeurs de même
chaîne sont interchangeables ; toute opération ne dépend que de la chaîne ; écrire une valeur dans
un fichier et la relire la restitue entièrement. Les représentations internes (liste, nombre,
dict) ne sont que des caches de la chaîne.

Une valeur typée du prototype a **deux** composantes : son **contenu** (une valeur Tcl ordinaire,
donc une chaîne) et son **type** (un descripteur, plus des notes). Le type n'est pas dans la
chaîne. C'est la source de toutes les tensions ci-dessous.

**EI2S** (*everything is two strings*) : une valeur est un couple de deux chaînes, son contenu
`C` et son type `T`, et **les deux** voyagent ensemble. EI2S garde EIAS pour les valeurs sans type
(`T` vide) et l'étend aux autres. Pour qu'EI2S tienne, il faudrait au minimum :

- **R1** une forme texte canonique du couple, relisible : `{T}C` (l'écriture des annotations), et
  les commandes qui l'écrivent et la relisent ;
- **R2** une règle qui dise quelles opérations voient `C` seul (la chaîne de Tcl) et lesquelles
  voient `(C, T)` ;
- **R3** que la **structure** d'une valeur (sa vue liste, ses éléments) se déduise de ses deux
  chaînes, jamais d'un état caché ;
- **R4** que `T` soit un **nom**, résolu là où la valeur arrive (un autre interpréteur, un autre
  thread), pas un pointeur vers un descripteur qui peut disparaître.

## 2. Vue d'ensemble

| | tension | opérations (sonde) | EIAS | EI2S tel que défini | gravité |
|---|---|---|---|---|---|
| T1 | une valeur typée est **atomique** dans la vue liste | B1–B5, B8, B15, B16, D15–D17 | ✗ | ✗ (R3) | **haute** : touche le cœur |
| T2 | la chaîne **ne porte pas le type** | D1, D5, D13, D17 | ✓ | ✗ (R1) | haute |
| T3 | l'égalité et les clés **ignorent le type** | A2, A3, A5–A7 | ✓ | à décider (R2) | moyenne |
| T4 | les types sont **propres à un interpréteur** ; une valeur peut leur survivre | D6–D8, `sonde_interp.tcl` | — | ✗ (R4) | **haute** : lecture après libération |
| T5 | les noms de types sont **globaux** (pas d'espace de noms) | F1, F2 | — | ✗ (R4) | haute pour les bibliothèques |
| T6 | **où vit le type** : le conteneur ou l'élément ? | B6, B7, B14, D9, D14 | — | ✗ (R3) | moyenne : incohérences |
| T7 | typage **nominal** strict | A3, E3 | ✓ | ✓ | faible (choix assumé) |
| T8 | **couverture** : TclOO, `apply`, threads | E6, E7 | — | — | moyenne |
| T9 | la doc décrit encore l'**adoption** | C1, C2, E1 | — | — | corrigé ici |

Ce qui est **cohérent** aujourd'hui : les contrats des variables (C3–C8, `upvar`, `global`, traces :
C11) ; le passage tel quel d'une valeur (`set`, argument, `subst`, `string cat`, `format`,
`eval` d'une liste, `after`, alias vers un autre interpréteur : C1, D2–D4, D8, D10) garde le type ;
un **calcul** rend une valeur ordinaire (`regsub`, `expr` : D12), comme Tcl ; `string is` et
`binary format` jugent la chaîne (A8, D11) ; `set {}x $P` retire le type (C10).

## 3. Les tensions

### T1. L'atomicité : même chaîne, autre structure

```tcl
set {point}P 1 2;  set Pn {1 2}
llength $P ;# 1        llength $Pn ;# 2         (B1)
lassign $P a b         ;# a = {1 2}, b = {}       (B5)
concat $P $P           ;# {1 2} {1 2}  (2 éléments)   concat $Pn $Pn ;# 1 2 1 2   (B8)
set {string}st "a b c"; llength $st ;# 1        (B15)
```

Un produit typé, mais aussi **tout scalaire typé** (B15, B16), est un seul élément de liste. C'est
la « règle B » : un point est un élément, ses champs se lisent par chemin (`$P(x)`, `$P(*)`).
Conséquences :

- **deux valeurs de même chaîne ont deux structures** (B1, B2, B3) ;
- **une opération sur deux valeurs de mêmes chaînes rend deux chaînes différentes** : `concat`
  (B8). C'est la violation la plus nette d'EIAS : ce n'est plus seulement le type qui se perd, c'est
  le **texte** qui change selon un état invisible ;
- la structure **change quand la valeur passe par une chaîne** : une liste nue qui contient `P`
  voit `P` comme un élément (D16) ; la même chaîne relue le voit comme deux (D17) ;
- `{*}$P` passe un argument au lieu de deux (B4) ;
- c'est elle qui a imposé `{*}T` (v16r) pour rendre une valeur à un appelant non typé.

C'est la **décision centrale** : sans toucher à l'atomicité, R3 ne peut pas tenir, et ni EIAS ni
EI2S ne tiennent. Deux voies :

- **(a) garder l'atomicité** et l'assumer comme une sortie d'EIAS : un `point` n'est pas une liste ;
  il faut alors que la forme texte (T2) la porte, sinon une valeur relue n'a plus la même
  structure ;
- **(b) l'abandonner** : la vue liste d'une valeur typée est celle de son contenu (un `point` est
  la liste `{1 2}`, `llength` 2), le type ne sert qu'aux chemins (`$P(x)`), à la validation et aux
  contrats. `concat`, `lassign`, `{*}`, `foreach` redeviennent ceux de Tcl ; `{*}T` devient
  inutile pour les listes. Une `list.point` reste une liste de listes de deux éléments, la forme
  naturelle en Tcl. À examiner : ce que l'atomicité protège aujourd'hui (une chaîne typée avec des
  espaces dans une `list.string` est de toute façon un élément entre accolades ; les sommes ; les
  chemins à travers des valeurs imbriquées).

### T2. La chaîne ne porte pas le type

```tcl
puts $f $P;  gets $f r     ;# r = {1 2}, sans type, llength 2   (D1)
eval "g {$P}"              ;# g reçoit une valeur sans type    (D5)
```

Tout passage par du texte (fichier, socket, `eval` d'une chaîne construite, base de données)
perd `T`. C'est conforme à EIAS (la chaîne est la valeur), mais un programme typé change de
comportement après un aller-retour (combiné à T1, sa **structure** change aussi : D17). La seule
forme texte qui montre le type, `T@notes`, est un **affichage** : elle ne se relit pas (D13).

EI2S demanderait (R1) une forme canonique relisible, par exemple :

```tcl
type repr $P               ;# {point}{1 2}      (à créer)
set Q [type read $texte]   ;# Q de type point   (à créer ; ou type {…} sur le texte)
```

et une décision sur les canaux, les sockets, `interp transfer`, `thread::send` : texte nu (comme
aujourd'hui), ou forme canonique sur demande.

### T3. L'égalité et les clés ignorent le type

```tcl
expr {$P eq {1 2}}         ;# 1                (A2)
set {vec}W 1 2; expr {$P eq $W}   ;# 1 : point et vec égaux   (A3)
dict create $P x           ;# la clé garde le type point, invisible   (A5)
lsort -unique [list $P {1 2}]     ;# {1 2} : fusionnés          (A7)
```

C'est EIAS : `eq` compare les chaînes. Avec EI2S, il faut choisir (R2) : `eq` compare `C` seul
(compatibilité, ma recommandation) et une comparaison des deux chaînes s'ajoute (`type same $a $b`
ou équivalent). Point à noter : une clé de dict ou un index de tableau **garde** le type de la
valeur qui l'a créé (A5), sans que rien ne le montre.

### T4. Les types appartiennent à un interpréteur, les valeurs non

```tcl
interp create c
c eval {type def zz {{int}a; {int}b}; set {zz}V 1 2}
set v [c eval {set V}]
interp delete c
type get $v                 ;# lecture après libération (valgrind)   (sonde_interp.tcl)
```

Un descripteur de type est créé par un interpréteur et libéré avec lui ; une valeur typée garde un
**pointeur** vers son descripteur et peut passer dans un autre interpréteur (D6, D8). Deux
défauts :

- **sûreté mémoire** : si l'interpréteur qui a défini le type disparaît, la valeur pointe vers de
  la mémoire libérée. C'est un **bogue à corriger**, quelle que soit la suite ;
- **sens** : un enfant qui définit son propre `point` (un `int`) reçoit le `point` du parent, et
  `type get` dit `point` des deux côtés pour deux types différents (D7).

R4 (le type est un nom, résolu là où la valeur arrive) règle les deux ; à défaut, des descripteurs
comptés (une valeur garde son descripteur en vie) règlent la sûreté, pas le sens.

### T5. Des noms globaux

```tcl
namespace eval nsA {type def pnt {{double}x; {double}y}}
type def pnt int            ;# type "pnt" is already defined   (F1)
```

Les noms de types sont globaux à l'interpréteur ; deux bibliothèques qui définissent `point` se
heurtent. À faire : des types qualifiés par l'espace de noms (`::geom::point`), résolus comme les
commandes (espace courant, puis global), exportables. Lié à R4 : le nom qualifié est ce qui
voyage.

### T6. Où vit le type : dans le conteneur ou dans l'élément ?

| valeur | `lindex` / `$L(i)` | `foreach` | `lrange` | `dict get` / `dict for` |
|---|---|---|---|---|
| `list.double` | typé (B6) | **nu** (B6, v16s) | nu (B14) | — |
| `list.point` | typé (B7) | typé | — | — |
| `dict.int` | — | — | — | **nu** (D9) |
| `dict.point` | — | — | — | **nu** (D14) |

Le type est porté par le **conteneur** (`list.double`), et chaque opération décide si l'élément
qu'elle rend reçoit le type de l'élément. Aujourd'hui, la décision varie selon la commande et
selon le type des éléments. C'est le résultat d'optimisations successives (v15u, v16s), pas d'une
règle. Une règle unique serait l'une de :

- **l'élément est typé partout** (cohérent, coûteux pour les scalaires : v16s a mesuré 2,5 fois) ;
- **l'élément est nu partout**, le type ne servant qu'au conteneur (validation à l'entrée,
  chemins) ; avec la voie (b) de T1, c'est le modèle le plus simple ;
- **typé à l'accès, nu au parcours** (v16s pour les scalaires), étendu aux dicts et aux produits.

### T7. Typage nominal

`{point}p` refuse un `vec2` de même forme (E3) ; deux types de même forme sont égaux par `eq`
(A3). C'est un choix (le nominal distingue un segment d'une droite) ; il coûte des conversions
explicites (`type convert`) et des messages qui surprennent. À garder, mais à documenter comme tel.

### T8. Couverture

- **TclOO** : une méthode ne peut pas avoir de signature typée (E6) : `oo::define … method` passe
  par `TclCreateProc`, pas par `proc`.
- **`apply`** : idem (E7).
- **Threads** : non testés (pas de paquet Thread dans ce build) ; T4 s'y pose plus fortement
  (`thread::send`, `tsv`).

### T9. La doc décrit encore l'adoption

La v16l a retiré l'adoption (« une variable non déclarée reçoit une valeur typée, jamais de
contrat ») ; `ANNOTATIONS.fr.md` § 6.2 la décrivait encore, avec le piège de `catch … m`. La sonde
le confirme (C1, C1a–C1c, C2b, E1b) : une variable sans contrat accepte toute valeur ensuite. Le
§ 6.2 est corrigé dans cette livraison.

## 4. Ce que cela demande au modèle

| décision | options | dépend de |
|---|---|---|
| **D1. Atomicité** (T1) | (a) la garder et la porter dans la forme texte ; (b) l'abandonner : la vue liste est celle du contenu | — |
| **D2. Forme texte** (T2) | `{T}C` canonique, `type repr` / `type read` ; canaux et `interp transfer` : nu ou canonique | D1 |
| **D3. Égalité** (T3) | `eq` sur `C` seul + `type same` ; ou `eq` sur le couple | — |
| **D4. Identité des types** (T4, T5) | noms qualifiés résolus à l'arrivée ; descripteurs comptés ; espaces de noms | — |
| **D5. Type des éléments** (T6) | typé partout ; nu partout ; typé à l'accès, nu au parcours | D1 |
| **D6. Couverture** (T8) | signatures typées pour `method` et `apply` | — |

D1 conditionne D2 et D5. Mon avis :

- **D1 (b)** : il rend EIAS vrai pour la structure, supprime la plus grande source de surprises, et
  ne retire rien d'essentiel. Les chemins et les contrats suffisent à ce que faisait l'atomicité. Le
  coût est une révision de la règle B dans le code, les tests et les exemples ;
- **D3** : `eq` sur `C` seul ;
- **D4** : à faire en premier, parce que T4 est un bogue mémoire et que les espaces de noms sont
  demandés de toute façon ;
- **D2** et **D5** : à décider après D1.

Avec D1 (b), D2 et D4, le prototype serait EI2S au sens de la section 1 : une valeur est son
contenu (une chaîne, avec sa structure de liste habituelle) et son type (un nom), qui voyagent
ensemble quand on le demande et se perdent comme aujourd'hui quand on écrit le texte nu.

## 5. Suite : v16t, D1 tranchée

**Décision.** Une chaîne est un scalaire, un produit est un conteneur. L'atomicité visait un
problème réel de Tcl : `set L [list {hello world} coucou]`, parcourue par deux boucles
imbriquées, finit par voir `{hello world}` comme une liste. Elle était obtenue en rendant atomique
**toute** valeur typée, produits compris ; c'est l'atomicité des produits qui violait EIAS (T1),
pas celle des chaînes. La règle devient : **la structure d'une valeur vient de son type, jamais
d'une relecture de sa chaîne** (R3) :

- un scalaire typé (`string`, nombre, littéral d'une somme) est un seul élément de liste, à tous
  les niveaux d'imbrication ;
- un conteneur (`list`, `dict`, produit) se déplie ; un produit en ses champs, chacun dans son
  type (un champ `string` reste atomique, un champ `double` est rendu nu).

**Le code.** Une ligne dans `AnnotListView` (le produit se déplie), le type d'un élément par
indice (`AnnotListElemTypeAt` : le type du champ), l'enveloppe des champs (`AnnotWrapFields`), et
`string` retiré des éléments rendus nus (v16s). Aucun patch noyau.

**Ce que la sonde voit maintenant** (`sonde.tcl`, mêmes identifiants) :

| | v16s | v16t |
|---|---|---|
| B1 `llength` typé / nu | `1 2` | `2 2` |
| B3 `foreach` sur P | `{{1 2}} 1` | `{1 2} 2` |
| B4 `{*}$P` | 1 argument | 2 |
| B5 `lassign $P a b` | `{1 2} {}` | `1 2` |
| B8 `concat $P $P` | `{1 2} {1 2}` | `1 2 1 2`, comme les listes nues |
| D16 / D17 même chaîne, même structure | non | oui |
| B15 `{string}` "a b c" | 1 élément | 1 élément |
| `list.list.string` parcourue deux fois | `hello` `world` (v16s) | `{hello world}` |

T1 est résolue pour les produits ; les chaînes restent atomiques, ce qui est le but. Il reste
pour EI2S : la forme texte relisible (D2, sinon une chaîne écrite puis relue redevient une
liste), l'égalité (D3), l'identité des types (D4) et une règle unique pour les éléments des dicts
(D5 : `dict.point` rend encore ses valeurs nues).

**Ce qui se simplifie.**

- Les procédures publiques n'ont plus besoin de `{*}T` pour rendre un produit à un appelant non
  typé : la géométrie perd ses 18 `{*}` et passe les 210 tests de tcllib (Tcl et C), aux mêmes
  vitesses. `{*}T` ne sert plus qu'à rendre des données sans type, `{**}T` un dict.
- Le C lit un produit typé par l'API des listes (`Tcl_ListObjGetElements`) : `geom.c` n'a plus
  besoin de `Tcl_AnnotatedGetParts`.
- `foreach {x y} $p` et `lassign $p x y` marchent sans `{list}$p`.
- Deux exemples se corrigent d'eux-mêmes : `executable.tcl` voulait le nom d'une section
  (`lindex $s 0`) et affichait la section entière ; `t51` voulait remplacer le champ `n`
  (`lreplace $faux 0 0 5`) et remplaçait tout l'en-tête.
- Les tests `t18` et `t19`, qui n'avaient pas de sortie attendue, échouaient depuis la v16s sur
  les chaînes (et sur des `list.int` qu'ils supposaient typées au parcours) ; ils passent et ont
  maintenant leur `.out`.

## 6. Suite : v16u, D4 en partie

**Espaces de noms (T5).** Un type appartient à l'espace de noms où il est défini (`::geom::point`)
et se résout comme une commande (espace courant, `namespace path`, global). La sonde F1 passe :
`pnt` défini dans `nsA` ne bloque plus un `pnt` global. Voir `ANNOTATIONS.fr.md` § 5.8.

**Sûreté mémoire (T4, premier défaut).** Une valeur typée garde en vie la table de types de
l'interpréteur qui l'a créée (compteur sur la table, tenu par chaque référence de type et chaque
tampon de cases) : `sonde_interp.tcl` lit `::k::kp {1 2}` après `interp delete`, sans erreur sous
valgrind.

**Reste de T4 (le sens).** Une valeur garde un pointeur vers son descripteur, pas un nom résolu
à l'arrivée : un enfant qui a son propre `point` voit toujours le `point` du parent (D7). Le nom
qualifié rend la confusion plus rare (`::geom::point` d'un côté, `point` de l'autre) sans la
supprimer. À reprendre avec la forme texte (D2), qui demande de toute façon de résoudre un nom
à l'arrivée.
