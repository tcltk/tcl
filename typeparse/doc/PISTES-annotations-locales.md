# Pistes : où et quand une annotation agit (2–3 novembre 2026)

Notes de discussion. **Décidé le 3 novembre : piste B, faite en v16m** (retrait du cast
immédiat, expiration, cast local des boucles en C, `type {T}mot` ; voir `noyau/LISEZMOI.md`).
Point de départ : la v16k, et l'essai v16l.

## Le constat

* `catch {concat {int}$s} r` (s = 3.0) : l'erreur naît en écrivant `r`, donc **hors** de la
  protection du `catch`. Cause : hors constructeur, l'annotation est empilée sur la valeur, et
  elle se résout au premier rangement, quel qu'il soit, même après être passée par d'autres
  commandes (`concat`, `return`, résultat d'une procédure). C'est une action à distance.
* L'essai v16l (cast immédiat de `{T}$x` pendant la substitution) casse les noms calculés :
  `set {int}$nom 5` caste la chaîne `compteur` au lieu de déclarer la variable, et
  `{T}"$nom"` aussi (mêmes tokens). Raison de fond : Tcl substitue tous les mots **avant**
  d'appeler la commande ; seule la commande sait si un mot est un nom ou une valeur.
* Pourquoi la pile avait été introduite (CONCEPTION-annotations.md §6–§7, 18 octobre) : notes =
  nominaux en attente qu'un `type def` tardif rend significatifs ; « le cast appartient aux
  constructeurs » (adoption, rôle du mot connu de la seule commande). L'adoption est réglée
  (`VAR_CONTRACT`) ; le rôle du mot reste la vraie raison du report. La **pile** elle-même (ordre,
  `contradictory annotations`, résolution tardive) sert peu.

## Ce qui est retenu

* **Étiquettes `{nom:}`** (v16l) pour les champs et les variantes : indépendantes du reste, elles
  règlent un vrai conflit (`{elf}$V` dans `exemples/executable.tcl`). À garder.
* **Retirer le cast immédiat de `{T}$x`** de la v16l (régression des noms calculés).
* Idée directrice : **généraliser l'annotation locale** — une annotation n'agit que sur la commande
  qui reçoit le mot ; un **type résolu** voyage avec la valeur, une **annotation** jamais.

## Les trois façons de borner le report

| | `concat {int}$s` | noms calculés, `upvar` | coût |
|---|---|---|---|
| **A.** cast à l'appel de la commande, sauf liste de commandes qui prennent des noms | casté, attrapé par `catch` | cassés hors liste et pour les procédures à `upvar` | liste longue et fragile (positions, ensembles `dict`/`array`/`info`) |
| **B.** l'annotation expire après la commande qui reçoit le mot (compteur de commandes noté à la pose) ; rien n'est casté avant l'appel | non vérifié : devient une note | marchent partout | simple, général |
| **C.** B + cast à l'appel pour une liste blanche de commandes qui ne prennent **que** des valeurs (`concat`, `list`, `puts`, `string`, `format`, `lindex`…) | casté, attrapé par `catch` | marchent partout | une liste, sans danger : un oubli retire un contrôle, ne casse rien |

Recommandation provisoire : **B**, puis **C** comme extension optionnelle. Avec B, la pile devient
inutile : au plus un type résolu, et des notes comme simples métadonnées (un ensemble), jamais
résolues plus tard.

À trancher avec B : le sort des notes posées avant leur `type def` (t48, t49 V12), qui ne
prendraient plus de sens tout seules.

## Forme texte d'une valeur typée (3 novembre)

Le type n'est jamais dans la chaîne d'une valeur (EIAS) : passer une valeur typée là où seul un
texte est gardé (un nom de tag Tk, un fichier, une clé) perd son type. Le mot annoté est une
forme texte naturelle : `{T}` suivi du contenu mis en forme de liste,
`{segment.lien}https://tcl.tk`, `{nomstyle}gras`, `{bloc.titre}{1 {…}}`. On le relit sans
`eval` : `regexp {^\{([^}]*)\}(.*)$}`, puis `type cast T [lindex $reste 0]`, qui valide contre le
type **actuel**.

C'est une forme canonique d'une valeur typée, au sens des listes et des expressions canoniques :
la mise en forme du contenu doit garantir qu'on retrouve exactement la valeur. Piste de commande
pour la TIP : `type canonic $v` → `{T}contenu`, et `type parse $texte` → la valeur typée (refus si
le type n'existe pas, ou si le contenu n'y entre pas). Premier usage : `exemples/texte_enrichi`,
où chaque tag du widget porte la tête d'un nœud du document.

Nom retenu (3 novembre) : `type canonic` plutôt que `type text`. Il dit la garantie et pas
seulement le format : une seule forme par valeur typée, et
`type canonic [type parse $t]` rend `$t` quand `$t` est déjà canonique. Il couvre les deux formes,
avec contenu (`{T}contenu`) et sans (`T@notes`, notes triées et factorisées), par exemple
`type canonic -notes $v`.

### La forme sans contenu : `T@notes`

L'affichage `type@notes` est l'autre forme texte : le type et ses métadonnées, sans le contenu.
`exemples/texte_enrichi` l'emploie maintenant pour ses tags (`bloc.titre@2`,
`segment.marque@gras`, `segment.lien@https%3A%2F%2Ftcl%2Etk`) : le contenu est le texte que le tag
couvre. Pour en faire une forme canonique :

* **notes triées** (avec la piste B, les notes deviennent un ensemble : l'ordre ne porte plus de
  sens) ;
* **encodage** de tout caractère hors lettres, chiffres, `_`, **point compris** puisqu'il sépare
  les notes. L'exemple prend l'encodage d'URL (`%XX` sur l'UTF-8).

Écart avec la v16k : `%`, `:`, `/` y sont BADCHAR, une note encodée rend donc l'annotation locale
et elle est perdue (`{segment.lien@https%3A%2F%2Ftcl%2Etk}x` n'a pas de note). Deux issues :
admettre `%XX` dans les notes (l'interpréteur les tient pour des octets encodés), ou prendre un
encodage en caractères déjà permis (`_XX`, `_` encodé lui-même).

### Les liens par une famille de types : `lien(url)` (3 novembre, essais en v16k)

Idée : la cible d'un lien n'est plus une note, mais un **paramètre** de type ; elle entre dans le
nom canonique, et le tag d'un lien est simplement `type get` du nœud : `lien({https://tcl.tk})`.
Plus d'encodage (un paramètre est une chaîne libre, pas de BADCHAR), un vrai nom de type que
`type cast` relit, et `type convert` sur la famille. Mieux encore, une **famille constante**
(`type def {cible({string}url)} {"$url"}`, un type à une seule valeur) mettrait la valeur entière
dans le nom du type : c'est ce qu'il faut à un tag, qui ne garde qu'un texte.

Ce que la v16k donne :

* paramètre fantôme : marche (`type def {lien({string}url)} {list.string}` →
  `lien({https://tcl.tk})`, `lien({Été 2026})`) ;
* famille constante : refusée, `substitutions are not allowed in a type definition` ;
* nom d'instance non injectif : une `,` → `too many arguments for lien` (même avec `[list $u]`) ;
  `x)y`, `a"b` → erreur de syntaxe ; `a}b`, `a\b`, `{a` acceptés mais
  `type get [type cast [type get $v] …]` ne redonne pas le même nom ;
* une variante de somme ne peut pas être une famille ouverte :
  `type def seg {{string}texte, {lien}l}` → `type "lien" takes parameters` ; dans
  `seg3(u)`, la variante `lien` heurte la famille globale `lien` ;
* `{lien($u)}` n'est pas substitué (paramètre littéral `$u`) : il faut `type cast "lien(…)"` ;
* pas de lecture des paramètres d'une valeur hors de `type convert` (`regexp` sur `type get`,
  accolades comprises).

À ajouter pour que ça marche :

1. substitution des paramètres dans les littéraux (familles constantes) ;
2. une mise entre guillemets canonique des paramètres, avec `type get` ∘ `type cast` = identité ;
3. des variantes de somme sur une famille (somme dépendante : « `lien`, pour une cible
   quelconque ») ;
4. une lecture des paramètres d'une valeur (`$v(@url)` ou `type params $v`).

Avec les quatre, `segment.lien(cible)` remplace `segment.lien@cible-encodée` dans
`exemples/texte_enrichi`, sans encodage d'URL.

### Syntaxe des notes : `,` pour l'ensemble, `.` pour le chemin (proposition du 3 novembre)

Avec la piste B, les notes deviennent un ensemble. Plutôt qu'un `.` qui empile, reprendre la
syntaxe des sommes et des chemins :

```
para@rouge,gras,italique                 un ensemble : l'ordre ne compte pas
para@font.helvetica,font.size.10         des chemins : des notes structurées
```

* **Cohérence** : `,` sépare déjà les variantes d'une somme et les branches d'un chemin
  (`$R((A,B).x)`), `.` descend dans un type ou une valeur. Une note devient un chemin, et les
  notes un ensemble de chemins, soit un arbre (`font` → `helvetica`, `size` → `10`).
* **Forme canonique** : chemins aplatis et triés. La forme factorisée des chemins
  (`para@font.(helvetica,size.10)`) peut être admise en entrée et dépliée.
* **Lecture** : `note get $v font.size` → `10` (les feuilles sous un préfixe). À trancher : un
  chemin à plusieurs feuilles (`font.size.10,font.size.12`) est-il permis (ensemble de chemins)
  ou refusé (un dict, une feuille par clé) ?
* **Changement d'affichage** : `T@a.b` voulait dire « deux notes empilées », il voudrait dire
  « une note, chemin a.b ». Sans gêne si la pile disparaît (piste B).
* **Feuilles** : une feuille qui contient `.`, `,` ou un caractère interdit doit encore être
  encodée (URL : `%XX`). C'est le même besoin que les paramètres de famille (`lien(url)`) : une
  seule règle de mise entre guillemets pour les deux serait bienvenue.

Essai dans `exemples/texte_enrichi` : les notes des tags sont séparées par `,`, et les tags
d'aspect sont nommés par chemins `champ.valeur` (`aspect@pente.italic,poids.bold`,
`aspect@couleur.%23b4531f,souligne.1`). Ces tags d'aspect existent parce que Tk ne cumule pas
les polices de deux tags : l'apparence d'un morceau est le cumul calculé des styles de ses tags.

#### Suite : `=` pour les feuilles, `"…"` pour les mots (3 novembre, matin)

* **`=` sépare la feuille de sa valeur** : `font.(family=helvetica,size=10)`. Une note sans `=`
  est un drapeau (`gras`). On distingue ainsi branche et feuille, et un chemin à valeur ressemble
  à un champ de produit : `segment.lien@cible="…"` ↔ `$s(lien.cible)`. (`=` sert déjà aux défauts
  dans `type def`, mais en position de champ, pas de note : pas de conflit de lecture.)
* **La dernière l'emporte**, comme une clé répétée dans un dict ; poser une feuille efface ce qui
  est dessous ou dessus (`font=x,font.size=10` → `font.size=10`). Les notes sont donc un dict
  arborescent, plus un ensemble de chemins.
* **Mots entre guillemets avec les substitutions backslash de Tcl** au lieu de l'encodage
  d'URL : `cible="https://tcl.tk/a,b.c?x=(1)"`, `"une clé"="\"q\" \\ \u00e9"`. Mot nu si
  `[\w-]+`, sinon guillemets ; dedans, seuls `\` et `"` (et les caractères de contrôle) sont
  échappés. Décodage : `subst -nocommands -novariables`. C'est la règle qui manquait aux
  paramètres de famille (`lien("x,y")` est déjà accepté, mais ni `"` ni `\` ne passent, et
  l'affichage revient en accolades) : une seule règle pour les deux.
* **`extra characters after close-quote`** : n'arrive que si un **mot Tcl** (ou un élément de
  liste) **commence** par `"`. Un nom `T@…` commence par le type, donc jamais. Mais un nom qui
  contient espace et guillemet n'est pas un élément de liste sûr : `lindex {a@c="x y"} 0` →
  `a@c="x`. Il faut toujours le passer par `list` (ce qu'on fait déjà pour les tags Tk). Dans une
  annotation `{T@c="x y"}mot`, les accolades protègent tout, rien à craindre côté parseur Tcl.
* **Forme canonique** : clés triées, branches factorisées, branche à un seul enfant sans
  parenthèses (`a.b=1`), guillemets seulement s'il le faut ; idempotente (testé).

Prototype complet en Tcl dans `exemples/texte_enrichi` (`te::LireNotes`, `te::EcrireNotes`,
environ 130 lignes), avec tests : factorisation, dernière l'emporte, guillemets et backslashes, erreurs
(`a=(`, `a.(b=1`, guillemet non fermé, `,a`).

En v16k, `T@notes` reste un **affichage** : `{S.a@style=gras}x` est refusé comme annotation
locale, et `=`, `"`, `(`, `\` sont BADCHAR dans un nom. Pour la TIP : faire de cette syntaxe la
forme écrite des notes (une seule annotation `{T@notes}` au lieu d'empiler `{n1}{n2}{T}`), et la
relire dans `type parse`.

## Le cast immédiat de `{list}` / `{dict}` (3 novembre)

Gardé pour son côté pratique (`foreach {x y} {list}$p`). Mais il n'est immédiat que si la valeur
est **déjà typée** ; sur une valeur nue, l'annotation est reportée comme `{int}`. Essais v16k :

| | résultat |
|---|---|
| `catch {concat {list}$brut} r` (brut = `a {b`) | « unmatched open brace in list » **sort** du `catch` : levée par le `catch` lui-même quand il range `r` |
| `catch {concat {dict}{a b c}} r` | idem, « missing value to go with key » |
| `proc f {} {return {list}$brut}` ; `catch f r` | idem, l'erreur éclate chez l'appelant |
| `set {string}nom compteur` ; `set {list}$nom 5` | `unknown way to cast the value from string to list` : c'est le **nom** qui est casté (régression v16l, en plus rare) ; idem `{list}[set nom]` |
| `foreach {a b} {list}$p {…}` (p produit typé) | `1/2` : le cas pratique marche |

Le même code a donc deux comportements selon que la valeur est typée ou non : l'erreur reste dans
le `catch` pour un produit typé, elle s'en échappe pour une chaîne nue.

Trous au passage : `{list}$e` (e élément d'une `list.string`) → `unknown way to cast the value
from string to list` (un scalaire typé ne devient pas une liste d'un élément) ; `{dict}$L` d'une
`list.string` n'est pas défini.

### Avec une commande

* **`[type cast list $P]` existe déjà** et règle tout : l'erreur naît dans la commande, donc dans
  le `catch` (`catch {type cast list $brut} r` → 1) ; pas de nom en jeu ; même résultat que
  `{list}$p` (`1 2`, `type get` → `list`), `type cast dict $p` → `x 1 y 2`,
  `type cast list.any $p` aussi. Coût : 2,3 µs contre 1,4 µs pour `foreach {a b} {list}$p {}`
  (appel de commande non compilé ; à compiler en bytecode si besoin). Mêmes trous
  (scalaire → list, `list.string` → dict).
* **`[type list $P]`, `[type dict $P]`** : n'existent pas (`bad subcommand "list"`). Raccourcis
  faciles, mais ils multiplient les sous-commandes : un par type collection.
* **`[type {list}$P]`** : aujourd'hui le cast immédiat agit avant `type`, qui reçoit `1 2` comme
  sous-commande (`bad subcommand "1 2"`). Avec la piste B, ce serait la forme la plus cohérente :
  `type` est la commande qui reçoit le mot, elle résout son annotation (`type` avec un seul
  argument annoté = « résous et rends »). L'écriture d'annotation est gardée, le cast est local à
  une commande, l'erreur reste dans le `catch`, et les noms ne sont jamais touchés. Elle
  **demande** de retirer le cast immédiat (sinon `type` ne voit jamais l'annotation).

Proposition : retirer le cast immédiat de `{list}`/`{dict}` avec celui de `{T}$x` ; garder
`foreach {x y} {list}$p` par la piste C (les arguments de valeur de `foreach`, `lmap`, `lassign`,
`dict for` castés à l'appel) ; ajouter `[type {T}$v]` comme cast explicite partout ailleurs
(`type cast T $v` reste la forme calculable, le type pouvant venir d'une variable).

### `dict map` (et `dict for`, `lmap`)

Essais v16k :

* `dict map {k v} {dict}$p {expr {$v*10}}` → `x 10 y 20` ; `dict for {k v} {dict}$p` → `x=1 y=2`.
* `catch {dict map {k v} {dict}$brut {set v}} r` (brut = `a b c`) → 1, « missing value to go with
  key » : **pas de fuite**. `dict map` et `dict for` lisent leur argument comme un dict tout de
  suite ; une valeur qui n'en est pas un échoue dans la commande, donc dans le `catch`.
* Le résultat du corps : `dict map {k v} {x 1 y 2.5} {string cat {int}$v}` → `x 1 y 2.5`, sans
  erreur, et `type get` de l'élément est vide ; idem `lmap`. L'annotation est **perdue en
  silence** : pas de fuite non plus, mais pas de contrôle (ce que la piste B rend explicite :
  l'annotation expire avec la commande qui la reçoit).

Règle qui s'en dégage : la fuite vient des commandes qui **rendent l'objet reçu tel quel**
(`concat` d'un seul argument, `return`, résultat d'une procédure, `set` en lecture) : l'annotation
non résolue voyage avec lui jusqu'au prochain rangement. Les commandes qui **consomment** leur
argument (`dict map`, `dict for`, `foreach`, `lindex`, `llength`…) échouent tout de suite ou
l'oublient. Pour la piste C, `dict map` va dans la liste avec `dict for` (argument dict casté à
l'appel), et la question du corps (caster ce qu'il rend ?) reste ouverte : `lmap` et `dict map`
pourraient caster chaque résultat du corps quand il porte une annotation, puisqu'ils en sont les
constructeurs.

#### Typer ce que rend `dict map` (essais v16k, `type cast int` pour le futur `[type {int}…]`)

* Un cast valide, il ne convertit pas : `type cast int 2.5` → `expected integer but got "2.5"`
  (et `2.0` aussi). Pour `x 1 y 2`, il faut `expr {int($v)}`.
* `dict map {k v} {x 1 y 2} {type cast int $v}` garde des éléments typés (`type get` → `int`,
  même après `dict set`), mais c'est la représentation interne : `string length $d` les fait
  perdre. L'affichage n'est jamais `x {int}1 y {int}2` (EIAS) ; ce serait la forme texte
  `type canonic` de la piste 7.
* `string cat` d'un seul argument rend l'objet typé tel quel : `type {int}[string cat [type {int}$v]]`
  est redondant.
* Le type du conteneur : `set {dict.int}D {*}[dict map …]` → `dict<int>`, chaque valeur vérifiée
  (2.5 refusé). Deux incohérences : `set {dict.int}D $d` (un dict en un seul argument) est
  refusé (`a dict needs an even number of elements`) ; et `dict get $D x` d'un `dict.int` rend un
  élément **sans** type, alors que `dict map` en rend de typés.
