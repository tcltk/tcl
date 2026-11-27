# Sommes, identité des types et `num` : note de conception (v17z, implémentée)

*27 novembre 2026. Fait suite à `DIAGNOSTIC-SOMMES.md` (v16v) et à la décision de la v16w.
Les questions de la § 9 ont reçu leurs réponses le même jour ; implémenté en v17z (même jour),
sauf l'union (§ 3.5) et les notes (§ 6), chantiers à part. Écarts à l'implémentation : l'arrondi
d'un entier se demande par `type cast inexact $n` (`inexact`, nominal de `double`) plutôt que
`num.double.inexact` ; `type is {sum?}` prend un type (pour une valeur :
`type is {sum?}[type get $v]`, une annotation locale ne gardant que le contenu).*

## 1. En une phrase

**Ce qui décrit une valeur est une métadonnée, posée à côté d'elle : le type, la variante
d'une somme, les notes. La valeur, elle, reste la chaîne.**

La v16w avait fait l'inverse pour les sommes : la variante entrait dans la valeur (`i 5`,
`fahrenheit 50.0`). Elle réglait ainsi les pertes silencieuses du diagnostic (S1 à S9), mais
au prix d'une valeur qui n'est plus sa charge : `expr {$t + 1}` ne marche plus sur un `temp`.
Cette note revient à la variante en métadonnée, et remplace les pertes silencieuses par des
erreurs et par une forme chaîne explicite.

## 2. Deux couches de types

Une discussion avec DKF (tclquadcode) l'a rendu clair : il y a deux sortes de types.

* **La forme** : un sous-ensemble des chaînes. `int`, `double`, `int(u,8)`, les prédicats
  (`int(($%2==0))`) en sont. C'est ce qu'un compilateur cherche, et ce qu'on peut déduire des
  opérations.
* **Le sens** : une métavaleur posée par-dessus. `{mm}5` et `{px}5` sont la même chaîne ; aucun
  sous-ensemble ne les distingue. Les types nominaux, les unités et les variantes d'une somme
  recouvrante sont de cette couche.

Le TIP porte les deux. Les règles ci-dessous disent comment elles se combinent.

## 3. La variante d'une somme est une métadonnée

### 3.1 La règle

* `set {temp}t {/celsius}50.0` : la valeur est `50.0`, `type get $t` rend `temp/celsius`.
* `$t` se lit comme sa charge : `expr {$t + 1}` donne `51.0`. C'est la projection S6 : une
  somme se lit comme le type de sa variante active.
* `$t(celsius)` lit la charge en vérifiant la variante ; `$t(fahrenheit)` est une erreur qui
  nomme la variante active.
* `type variant $t` rend `celsius`.

### 3.1b Variantes et littéraux : tester, lire, poser

Une somme mêle deux natures, qu'on ne confond pas :

| | variante nommée (`{int}n`) | littéral (`"auto"`) |
|---|---|---|
| porte une charge | oui | non : il est sa propre valeur |
| s'atteint par un chemin | `$m(n)`, `set m(n) 5` | non : `$m(auto)` est une erreur (`auto is a literal of M, not a variant: it has no payload`) |
| s'étiquette | `{/n}5` | inutile |
| se perd par la chaîne | si elle recouvre une autre (métadonnée) | jamais : c'est la chaîne même |

| commande | rend |
|---|---|
| `type is {sum?}X` | 1 si `X` (un type, ou une valeur annotée) est une somme ; vrai aussi pour une union, une énumération, `num` |
| `type is {enum?}X` | 1 si `X` n'a que des littéraux |
| `type variant -of T` | les variantes nommées seulement (`i s` ; rien pour une énumération) |
| `type variant -of T NAME` | le type de la charge de `NAME` |
| `type literal -of T` | les littéraux seulement (`on off auto`) |
| `type variant $v` | la variante nommée de la valeur, vide pour un littéral |
| `type variant $v NAME` | la même charge sous la variante `NAME`, comme nouvelle valeur (charge vérifiée) |

* `type variant $v NAME` **réétiquette** (même charge, lue autrement : ce qu'on veut entre
  `{int}i` et `{int}j`) ; **convertir** est `type cast S/NAME $v`, par `type convert`
  (`50.0` celsius → `122.0` fahrenheit). Le message de `type variant` rappelle le cast quand
  une règle de conversion existe. Pour une variable : `set v [type variant $v s]`, ou le
  chemin `set v(s) $v`.
* `type get` d'un littéral rend le type seul (`M`, `E`), jamais `M.auto` : il n'y a rien à
  garder en métadonnée.
* Un littéral qui recouvre une variante (`{{string}s, "auto"}`) gagne : c'est le plus strict
  (une seule valeur). La chaîne `auto` comme `s` s'écrit `{/s}auto`.
* `opt(T)`, aujourd'hui réservé aux liens binaires, devient la somme `{T}some, "none"` pour
  tout type.

### 3.2 Quand la variante se perd : une erreur, jamais un choix

La variante suit l'objet Tcl, comme le type : copies, arguments, retours, éléments de liste,
variables. Elle se perd là où Tcl ne transmet que la chaîne : un fichier, un socket,
`interp`/`thread send`, une extension C qui lit le texte, une valeur refaite par des opérations
de chaîne.

Une valeur nue qui entre dans une somme :

* **une seule variante l'accepte** : c'est elle (somme disjointe, énumération) ;
* **un littéral l'accepte** : c'est lui (une seule valeur, le plus strict possible) ;
* **plusieurs variantes typées l'acceptent** : erreur, avec l'écriture qui lève l'ambiguïté :
  `value "5" fits variants i and s of S: write {/i}5, or set {S}v(i) 5`.

*Une règle « la plus stricte gagne » (pré-v16w) a été essayée le 27 novembre, puis écartée :
c'est exactement le danger relevé par l'exemple du texte enrichi en v16n. Une somme dont une
variante accepte presque tout (une liste de segments, une chaîne) accepterait n'importe quelle
valeur nue en silence (`te::document … "pas un bloc"` devenait un paragraphe de trois mots).
La déduction « la plus stricte » reste celle des unions `|`, dont les alternatives sont
disjointes, et donc de `num` (`{num}5` → `num/int`, `{num}5.0` → `num/double`, par la forme
du nombre).*

Conséquence : une étiquette suit la valeur comme métadonnée, et se perd comme elle ; relue
depuis la chaîne, une valeur ambiguë est une erreur, jamais un autre choix. `type canonic`
l'écrit (`{S/s}5`, et `{/s}5` pour un élément d'une structure), `type parse` la relit.

Plus rien n'est deviné en silence (fin de l'Open Issue « without a tag, the first is taken
silently »). La variante n'est pas collante non plus (S9) : `set t 50.0` sur une variable
`temp` est une erreur ; on écrit `set t(celsius) 50.0` ou `set t {/celsius}50.0`.

### 3.3 La forme chaîne : le mot préfixé

Les deux commandes existent déjà ; seule leur sortie change.

| commande | v16w | maintenant |
|---|---|---|
| `type canonic $t` | `{temp}{celsius 50.0}` | `{temp/celsius}50.0` |
| `type canonic $L` (`list.temp`) | `{list.temp}{{celsius 50.0} …}` | `{list.temp}{{/celsius}50.0 …}` |
| `type parse $s` | relit la forme | relit la forme |

Dans une structure, l'étiquette n'apparaît que là où la variante ne se déduit pas de la forme.
`type parse` ne substitue rien (ni `$` ni `[…]`) : une donnée ne peut pas exécuter de code.

### 3.4 Égalité en valeur, identité en métavaleur

* **`eq`, `==`, clés de dict, `lsearch`, `lsort -unique`** : la valeur, comme toujours. Un
  `50.0` en celsius et un `50.0` en fahrenheit sont égaux en valeur (S8 revient, assumé).
* **`type eq a b`** : les types seulement, ni la valeur ni les notes. Il compare ce que rend
  `type get`, variante comprise : `temp/celsius` et `temp/fahrenheit` ne sont pas `type eq`.
  Le type exact : `{mm}5` et `{double}5` ne le sont pas, même si `mm` a `double` pour base.
* **Les notes** : par la commande `note` (`[note get $a] eq [note get $b]`, § 6).
* L'identité complète se compose : `$a eq $b && [type eq $a $b]`.

Usage typique, l'addition d'unités :

```tcl
if {![type eq $a $b]} {
    set b [type cast [type get $a] $b]   ;# fahrenheit → celsius par type convert
}
expr {$a + $b}
```

Un opérateur `id` dans `expr` a été envisagé et laissé de côté : ce serait le seul opérateur
qui regarde la métadonnée, que le moindre calcul perd ; il n'a pas de négation naturelle ; il
touche à la grammaire de `expr` (conflit possible avec une fonction `id(…)`). À revoir à
l'usage, d'abord sous forme de fonction (`tcl::mathfunc::…`).

### 3.5 L'union `|`

**Décidé : chantier à part, plus tard.** L'union reste telle quelle pendant ce chantier.

Avec une valeur nue pour toutes les sommes, `|` ne diffère plus de `,` que par sa
vérification : ses alternatives doivent être disjointes, ce qui est vérifié à la définition.
Deux choix, pour plus tard :

* **garder `|`** comme une somme qui promet d'être disjointe : la variante se déduit
  toujours, jamais d'étiquette ;
* **la fondre dans `,`**, `type is {disjoint?}S` disant si une somme se déduit.

### 3.6 `opt(T)`

`opt(T)` = `{T}some, "none"`. Avec une valeur nue, `opt(string)` devient ambigu pour la chaîne
`none` : elle demande alors l'étiquette (`{/some}none`). Les autres `opt(T)` (nombres, produits)
ne changent pas d'écriture.

## 4. `type include`

`type include U T` : 1 si **toute** valeur de `T` entre dans `U` sans cast, sans vérification
qui puisse échouer, et en restant **égale à elle-même au sens de `==`** (un nombre peut
changer d'écriture, `5` → `5.0`, pas de valeur). Notes ignorées.

L'ordre se lit en anglais, « U includes T » : `type include int pair`.

| règle | exemple |
|---|---|
| réflexivité ; tout est inclus dans `string` | `type include string temp` |
| raffinement : la base inclut, un intervalle inclut un plus petit | `int` ⊇ `int(s,16)` ⊇ `int(u,8)` |
| nominal : la base inclut le nominal, jamais l'inverse ; deux nominaux sans lien, jamais | `double` ⊇ `mm` ; `mm` ⊉ `px` |
| prédicat : la base inclut ; entre prédicats, seulement s'ils sont identiques | `int` ⊇ `pair` ; `pair` ⊉ `int` |
| somme : elle inclut ses variantes ; `S` ⊇ `S'` si chaque variante de `S'` est dans `S`, même nom, charge incluse | `temp` ⊇ `temp/celsius` |
| énumération : littéraux inclus | `{"a","b","c"}` ⊇ `{"a","b"}` |
| conteneurs, en covariance (les valeurs sont immuables) | `list.int` ⊇ `list.pair` |
| produits : mêmes champs, mêmes noms, même ordre, champ par champ | |
| liens `var.T`, en invariance (lus et écrits) | `var.int` ⊉ `var.pair` |
| nombres : `double` inclut les entiers exacts en double | `double` ⊇ `int(s,53)`, `double` ⊉ `int` |

`type include` est la garantie statique : quand elle vaut 1, un cast est inutile, et un
compilateur (tclquadcode par exemple) peut supprimer la vérification.

## 5. `num`, le nombre

### 5.1 Un type sémantique intégré

`num` est aujourd'hui une union définie à la main (`type def num {{int}i | {double}d}`, v17b),
déjà connue des voies rapides (`list.num` normalisée, écriture de colonnes, création rapide).
Il devient intégré, comme `int` et `double` :

* **le sens** : « un nombre, quelle que soit sa représentation » ;
* **les représentations** : `int`, `double` et tous leurs raffinements (`int(u,8)`,
  `int(s,32)`, `double(32)`…) ;
* `type include num T` vaut 1 pour chacune ; c'est le NUMBER de tclquadcode.

Un nom commun évite que deux bibliothèques définissent chacune le leur (`num`, `number`),
qui ne s'incluraient pas (deux nominaux sans lien).

**Décidé : `num` est une somme ; `type get` d'un `{num}5` rend `num/int`**, d'un `{num}2.5`
`num/double`, et ainsi de chaque représentation.

### 5.2 `int` vers `double` : exact, ou marqué inexact

Le cast automatique de `int` en `double` est un changement de représentation à l'intérieur
de `num` ; il reste. Mais il n'est exact que jusqu'à 2^53 : au-delà,
`set {double}x {int}9007199254740993` donnait silencieusement un autre nombre.

**Décidé :** il passe quand le `double` vaut exactement l'entier ; sinon c'est une erreur. Pour
accepter l'arrondi, on le dit : un cast vers **`num.double.inexact`**, qui arrondit et garde
dans le type que la valeur n'est plus exacte. L'inexactitude devient une métadonnée, comme
l'exactitude des nombres de Scheme :

```tcl
set {double}x {int}9007199254740993        ;# erreur : pas de double exact
set x [type cast num/double.inexact $n]    ;# 9007199254740992.0, type num/double.inexact
type include num/double num/double.inexact ;# 1 : un inexact reste un double
```

À préciser à l'implémentation : ce que devient `inexact` dans un calcul (perdu comme toute
annotation par `expr`, ou propagé par `type map`), et si d'autres arrondis (`decimal` →
`double`) le posent aussi.

### 5.3 Le pont avec tcllib (après l'implémentation)

`math::decimal` représente un nombre par `signe mantisse exposant` (`0.1` → `0 1 -1`) ;
`math::bigfloat` par une mantisse, un exposant et une incertitude ; `math::exact` par des
réels calculables. Chacun serait une représentation de plus de `num` :

* **`num` devient une somme recouvrante** : `0.1` peut être un `double` (approché) ou un
  `decimal` (exact), et `0 1 -1` ressemble à une liste d'entiers. La variante vit dans la
  métadonnée (§ 3) : `{num/decimal}0.1`.
* **Inclusions exactes** : `decimal` ⊇ `int`, et `decimal` ⊇ `double` (tout `double` est un
  décimal fini). Dans l'autre sens, un arrondi, donc un cast et une règle `type convert`.
* **Le calcul** reste celui des modules (`::math::decimal::add`…). La contagion
  (exact + exact → exact, exact + approché → approché, comme la tour numérique de Scheme) et
  la surcharge des opérateurs de `expr` sont un sujet à part, en discussion seulement.

Exemple prévu : `exemples/num_tcllib/`.

## 6. Les notes : un ensemble, le point pour imbriquer

Aujourd'hui, le point sépare les notes d'une pile (`note get` → `mm.px`, deux notes) et on
voudrait qu'il nomme une note imbriquée (`unit.mm`, une seule). Proposition :

* une note est un chemin de noms (`unit.mm`, `src.capteur.3`) ; une valeur porte un
  **ensemble** de chemins, sans ordre ni doublon ;
* `note add VALUE NAME ?NAME …?` les ajoute ; `note delete VALUE ?NAME?` retire un chemin et
  ce qui est dessous (sans nom : toutes) ; toutes deux rendent une nouvelle valeur ;
* `note has VALUE NAME` : 1 si un chemin commence par NAME ; `note get VALUE ?NAME?` : la liste
  triée des chemins, ou ce qui est sous NAME (`note get $x unit` → `mm`) ;
* `note add` refuse un nom qui est un type : un cast s'écrit avec `type` ;
* **résolution** : une note qui est un type se résout ; deux notes qui sont des types
  différents sont une erreur d'ambiguïté (plus d'ordre d'empilement, que personne ne voit) ;
* l'affichage suit : `type@{src.capteur unit.mm}`.

**Décidé : chantier à part.** Les notes doivent pouvoir former des arbres, écrits comme le
langage des chemins, les parenthèses groupant des branches :

```
a.(b.(c,d,e),f.(g.h,l)),i.(j,k,l)
```

soit les chemins `a.b.c a.b.d a.b.e a.f.g.h a.f.l i.j i.k i.l`. La proposition ci-dessus
(ensemble de chemins, préfixes, `note add`/`delete`/`has`/`get`) en est la forme dépliée ; la
syntaxe groupée et sa relation avec les chemins de valeurs seront l'objet de ce chantier.

## 7. Les constats du diagnostic, de nouveau

| | constat | v16w | ce modèle |
|---|---|---|---|
| S1 | la variante ne passe pas par la chaîne | dans la chaîne | **erreur** si ambigu ; `type canonic`/`type parse` pour la chaîne |
| S2 | une voie rapide range le contenu nu | sans danger (la variante est dans le contenu) | **à revoir** : toute voie rapide qui déballe une somme doit garder l'annotation (revue, § 8) |
| S3 | une étiquette relit la valeur par sa chaîne | réglé | à garder réglé : l'étiquette sur une valeur déjà `K.b` garde sa variante |
| S4 | étiquette de retour et compilation | réglé (v16x) | inchangé |
| S5 | commandes qui reçoivent une étiquette | `lappend`, `lset` | idem ; `linsert`, `dict set`, `list` gardent l'annotation de l'élément s'il en a une |
| S6 | projection | une somme ne se lit jamais comme sa variante | **une somme se lit comme sa variante** (`expr {$t + 1}`) |
| S7 | `$f(w)` | réglé | inchangé |
| S8 | égalité et clés | `i 5` ≠ `s 5` | **égalité en valeur** ; identité par `type eq` (assumé) |
| S9 | variante collante | `set t 50.0` refusé | idem (ambiguïté = erreur) |
| S10 | introspection | `type variant`, `type is {union?}` | + `type eq`, `type include` ; `type is` sur une valeur reste à faire |
| S11 | produit à un champ | réglé | inchangé |
| S12 | régimes | deux constructions | une règle (déduite ou étiquetée) ; `|` selon la question 1 |

## 8. Ce que ça touche

**Essai du 27 novembre.** En neutralisant `TclSumTagged` (toutes les sommes traitées comme
avant la v16w : charge seule, variante déduite), le prototype compile et 91 tests sur 116
passent tels quels ; la machinerie pré-v16w (déduction, ambiguïté, étiquettes) est intacte.
Les 25 autres se partagent entre des sorties attendues (la forme `variante charge` disparaît ;
des tests écrivent encore `{i 5}` en entrée) et de vrais points à reprendre : la charge d'une
variante produit n'est plus normalisée (`{1 2}` au lieu de `1.0 2.0`, t100), le codec binaire
de `opt(N)` relit la forme balisée (t43), `deref` (t29).

* **Le modèle des sommes** (`tclTypes.c`, `tclTypedVar.c`) : construction, lecture, cast,
  messages ; le refus d'une valeur nue devient « déduite ou ambiguë ».
* **Les voies rapides qui déballent** (S2) : à passer en revue une par une, fuzzées contre
  `TCL_TYPE_OPS=off`.
* **Les listes de sommes en cases** : une colonne de variante à côté des cases de charge (comme
  les sommes binaires), ou des objets annotés.
* **Le côté C** : `cproc` et l'API reçoivent l'objet annoté ; une fonction pour lire sa
  variante.
* **`type canonic` / `type parse`** : la nouvelle forme.
* **`num` intégré**, `type eq`, `type include`, le cast `int` → `double` exact.
* Les notes (§ 6) : chantier à part.
* **Tests** : t-sommes, sondes `doc/sommes/` rejouées, `texte_enrichi`, `cproc`,
  `tcllib_geometry` (210/210, déjà en unions donc a priori inchangée).
* **Le TIP** : sections sums and unions, open issues ; `num`, `type eq`, `type include`.

Ordre retenu : sommes (§ 3) et revue des voies rapides ; `type eq`, `type include` et `num`
(§ 4, 5.1, 5.2) ; l'exemple tcllib (§ 5.3). L'union (§ 3.5) et les notes (§ 6) sont des
chantiers à part. Pas de compatibilité avec la forme v16w : c'est un prototype.

**Limite connue (v17z).** Une somme placée directement dans la charge d'une autre somme
(`{{S}k, {string}t}`, ou `opt(S)`) ne garde que la variante extérieure : la variante
intérieure est déduite à nouveau à la lecture (`$e(k)`), donc une erreur si elle est ambiguë.
Dans une liste ou un produit, chaque élément garde la sienne. Lever cette limite demande de
lire le type sous une étiquette en attente (`{/k}$k`) au moment de la résolution.

## 9. Questions et réponses (27 novembre)

1. **`|`** : garder comme somme promise disjointe, ou fondre dans `,` ? → **plus tard**,
   chantier à part.
2. **`num` et ses variantes** : `type get` d'un `{num}5` ? → **`num/int`** : `num` est une
   somme. Reste à voir si une bibliothèque peut lui ajouter une représentation (`decimal`),
   c'est-à-dire si `num` est une somme ouverte (avec l'exemple tcllib).
3. **Le cast `int` → `double` hors de 2^53** → **erreur** ; l'arrondi s'obtient par un cast vers
   `num.double.inexact` (§ 5.2).
4. **Les notes** → **chantier à part** : des arbres, avec la syntaxe groupée des chemins (§ 6).
5. **Compatibilité v16w** → **non**, c'est un prototype.

## 10. La notation des membres (v18a, 27 novembre)

Une somme est une collection de types alternatifs ; la variante sera donc un type, et les
membres doivent pouvoir s'enchaîner (`list.temp/celsius`). Le séparateur a été discuté :
`.` compose déjà les types et donnerait des collisions dans les produits compliqués ; `:`
est pris par les espaces de noms ; `~`, `@`, `°`, `#` écartés ; **`/`** retenu (aucun nom de
type ne le contient, et il passe dans `expr` après `is` avec un petit changement du lexeur).
Par cohérence, l'étiquette prend la même barre : `set {temp}t {/celsius}50` et
`set {temp/celsius}t 50` disent la même chose.

Fait en v18a (première étape) :

* étiquettes `{/nom}` (champs et variantes, arguments nommés, retours, `type canonic`) ;
  `{nom:}` refusé avec la nouvelle écriture dans le message ;
* `P/f` = le type du champ (`type def point/x`), `S/v` = la variante (`type get`, `type cast`,
  `type include`, `type variant -of`, `type def`, `{S/v}x`, `proc {S/v}f`), `{P/f}x` = étiquette
  qualifiée ; `S.v` refusé avec la nouvelle écriture ;
* `expr {x is S/v}` (lexeur : noms, `.`, `/membre` et groupes entre parenthèses alternés,
  ce qui règle aussi `x is list(2).int`) ;
* un nom de variante peut être un nom de type (`{{int}int, {string}string}`) : la restriction
  n'a plus de raison d'être, l'exception faite pour `num` disparaît.

Deuxième étape, faite en v18b (validée le 27 novembre : une variable `{S/v}x` n'accepte que
v ; `list.temp/celsius` = `list.(temp/celsius)` ; un littéral est une variante à valeur unique,
au sens de `string(($ eq "litteral"))`, idée de l'utilisateur) :

* **Le type de variante.** `S/v` est interné (`TclTypeVariantOf`) : une somme qui a les
  variantes de S, n'en permet qu'une (`variantOf`, `variantIdx`), et dont la charge peut être
  restreinte (`S/v/w`). La grammaire lit les membres collés au mot (`MemberSpan`,
  `TclTypeMember`), avant le point : `list.temp/celsius`, `opt(int)/some`, `binop/"+"`.
* **Les valeurs.** Une valeur de S/v est annotée S/v (son contrat, celui d'une variable, d'un
  argument) ; rangée dans un S (variable, élément), elle devient une valeur de S de variante v
  (`TypeCastEx`). La validation lit la variante d'une valeur déjà annotée (élément de liste,
  argument) au lieu de la redéduire ; une variante différente est refusée sans règle
  implicite, convertie par un cast écrit.
* **Sommes imbriquées.** La charge d'une somme peut être une valeur annotée d'une autre somme :
  elle garde sa variante (`{/some}$t`, `{/some/celsius}5`, la chaîne restreinte d'un type
  S/v/w). `type get` et `type canonic` écrivent la chaîne (`TclAppendPayloadChain`). La limite
  du § 8 est levée.
* **Inclusion.** `type include` et l'inclusion interne suivent la restriction ; `is` vérifie
  la variante et la charge.
* **Littéraux.** `type get` donne `M/auto` ; `type variant $v` reste vide pour un littéral,
  `type literal -of` et `type variant -of` restent distincts.

Ce qui reste : `type variant $v` d'un littéral (vide, ou le littéral ?) ; `type def chaud
temp/celsius` (un nominal d'un type de variante) n'a pas de nom propre dans `type get` ;
`type canonic` d'un élément imbriqué écrit `{/some}{{/celsius}1.0}` plutôt que
`{/some/celsius}1.0` (relu correctement).
