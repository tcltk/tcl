# Annotations : qui les consomme, quand, et choisir le mot d'une annotation de commande

Note de conception. Elle répond à une question d'auteur de commande : *quel mot choisir
pour l'annotation d'une commande, afin qu'il ne soit jamais pris pour le typage des
variables ?* Elle ne restreint pas les noms d'annotation en général : un auteur reste libre
du mot, la note dit seulement ce qui le protège.

Compatibilité : `{annotation}mot` n'a de sens que parce que, dans Tcl actuel, un mot entre
accolades suivi directement d'autres caractères est une erreur de syntaxe
(« extra characters after close-brace »), sauf `{*}`. Aucun script existant ne change de sens.

## 1. Un seul axe : qui consomme l'annotation, quand, et survit-elle ?

| Genre | Consommée par | Quand | Survit à la commande ? | État du prototype |
|---|---|---|---|---|
| **Parseur** (`{*}`, `{#}` prévu) | `Tcl_ParseCommand` | au parsing, avant toute substitution | non : elle disparaît du mot | `{*}` est du Tcl standard ; `{#}` pas fait |
| **Cast** (`{T}$x`, `{T}[cmd]`, `{T}"..$x"`) | l'évaluation, sur tout mot issu d'une substitution | à la substitution | oui : la valeur devient typée | fait |
| **Typage par la commande** (`set {T}x`, `lappend`, `type def`...) | la commande, sur un mot littéral (le nom de la variable) | à l'exécution | oui : la variable devient typée | fait |
| **Locale** (`{-v}`, `{what?}`, `{c-code}`) | la commande appelée, et elle seule | à l'exécution | **non** : elle n'existe qu'entre l'appelant et la commande appelée | fait (§4) |
| **Déclaration** (`proc p {{?}{...} {!}arg}`) | la commande qui relit un texte entre accolades | à l'exécution, par un analyseur propre | selon la commande | pas fait : `list` refuse `{#}{x}` dans un élément |
| **Premier mot** (`{c}proc`) | la résolution de la commande | à l'exécution | sans objet | pas fait : le prototype lit `{#}...` en premier mot comme un type |

Le typage a **deux points d'entrée** distincts : le cast générique (le noyau de l'évaluation
l'applique sans la commande) et la résolution par la commande. Sur un mot littéral
(`{int(u,8)}300`, `{int(u,8)}{300}`) il n'y a pas de cast : l'annotation reste brute tant qu'une
commande ne la résout pas.

## 2. Ce qui peut être un nom de type

`type def` et `type alias` n'acceptent que : lettres ASCII, chiffres, `_` et octets non ASCII
(UTF-8), sans commencer par un chiffre. Essais (`bad type name`) : `what?` `a-b` `a:b` `a.b`
`a/b` `12` `1x` `#x` `*x` `&x` `x$` `x*` `x#`. Acceptés : `_x`, `éé`.

Dans une *expression* de type, la grammaire emploie en plus `{ } , ; . = " # ( ) * $` ; tout
autre signe de ponctuation ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ ` | ~`) y est refusé
(`invalid character`, code d'erreur `TCL PARSE TYPE BADCHAR`), sauf après un `=` (valeur par
défaut) ou entre parenthèses d'un constructeur (`*(file://x,...)`).

## 3. Règle pour choisir le mot d'une annotation de commande

1. Une annotation locale n'est menacée que si elle peut s'appliquer à une **substitution**
   (`{x}$v`, `{x}[cmd]`), ou si la commande la lit comme un nom de variable typée.
2. Un mot fait **uniquement** de lettres, chiffres, `_` et octets non ASCII (`c`, `list`, `fast`)
   peut être un type, donc un cast. Exemple vérifié : avec `type def c int(u,8)`,
   `list {c}$n` (n = 300) échoue : `value "300" does not fit in c`.
3. Un mot qui contient un caractère que la grammaire des types refuse ne peut jamais être
   un type : `{-v}` (option), `{what?}` `{is?}` `{sizeof?}` (questions), `{c-code}`. Les deux
   familles naturelles sont les options en `-nom` et les interrogations en `nom?`.
4. Un mot nu (`{c}proc`) reste possible si la commande n'accepte l'annotation que sur des
   **mots littéraux** et le documente : un mot littéral n'est jamais l'objet d'un cast.

## 4. Une annotation locale ne suit pas la valeur

Le prototype portait l'annotation sur l'objet-valeur. Résultat constaté : passée à une
procédure, `{-v}$mot` traversait la liaison des arguments, les valeurs de retour et les
conteneurs, puis faisait échouer, bien plus loin, le premier `set`, `lappend` ou `append`
qui rangeait la valeur (`invalid character "-" in type "-v"`). Seules les valeurs
reconstruites (`string toupper`, `dict set`) échappaient au mal.

Correction (`types-binaires-annotations-locales.patch`) : à la résolution
(`TclAnnotResolve`), un texte d'annotation que l'analyse des types rejette **pour un
caractère invalide** (`BADCHAR`) est une annotation locale ; la valeur est alors traitée
comme ordinaire (`TCLA_PLAIN`, contenu seul) et rien n'est typé. Les autres échecs restent
des erreurs : nom inconnu (`{inconnu}$mot`), parenthèse non fermée, etc. : faute de
frappe sur un type ou une variante, pas d'indulgence.

Détail d'implémentation à retenir : l'échec de l'analyse écrit son message dans le résultat
de l'interpréteur, qui peut être justement la valeur à ranger (`catch ... m`,
`set x [cmd]`). Le résultat est donc sauvé et rétabli autour de l'analyse.

Test : `t45_annotations_locales.tcl` (couvre toutes les voies de fuite ci-dessus ; valgrind
propre).

## 5. Reste à faire / à décider

* `{#}` au niveau du parseur (commentaire d'un mot, y compris sur plusieurs lignes) : il
  supprime un mot, pas une ligne ; `{#}puts hi` ferait de `hi` la commande.
* Annotation sur le premier mot d'une commande (`{c}proc`) : qui la lit ?
* Annotations de déclaration : un analyseur de liste annotée est un chantier à part.
* Refuser à la compilation une annotation littérale invalide sur un mot de `set`
  (`set x {-v}$mot` est aujourd'hui accepté, annotation ignorée).
* `type binary` : **fait** (patch `types-binaires-type-binary-annote.patch`, test t46) :
  `type binary {what?}T`, `{is?}T $data ?-exact 1?`, `{sizeof?}T`, avec `encode`, `decode`,
  `deref`, `space` comme verbes d'action. Renommage fait : `type binary` / `chan binary` (patch types-binaires-nom-type-binary), `file binary` reste en alias.
  `set x {-v}$mot` est refusé (patch types-binaires-set-annotation-locale, t47).
* Une annotation locale survit-elle à `Tcl_DuplicateObj` ? À vérifier avant de la donner
  à lire à une commande C (`Tcl_GetAnnotation`).
* Unifier types nominaux et annotations (notes qui suivent la valeur) : voir §6.

## 6. Piste décidée sur le papier : unifier types nominaux et annotations

Codée en prototype (patch types-binaires-notes, t48) : voir §6.7. Elle prolonge §4 : le correctif d'aujourd'hui n'en est que la moitié (les
annotations qui ne sont pas des noms restent éphémères).

### 6.1 Principe

Une annotation qui est un **nom** *est* un type nominal. Un nominal est un nom, avec ou sans
base :

| | avec base (`type def mm double`) | sans base : **en attente** (la « note ») |
|---|---|---|
| Rôle | contrainte de valeurs + identité | simple étiquette |
| `{mm}$x` | cast : valide `x` contre la base | pose l'étiquette, rien à valider |
| `type def mm …` écrit plus tard | — | donne un sens aux étiquettes déjà posées |

Cast et note sont donc **la même opération** (poser une couche nominale) ; seule la
vérification change. Plus de collision entre un mot de commande et un type : il n'y a qu'un
espace de noms, et `type def` donne un sens à des `{mm}` déjà écrits au lieu de les casser.
L'erreur éventuelle (`can't cast list<int> to mm`) apparaît à la consommation, comme
aujourd'hui.

Partition finale des annotations :

| Écriture | Genre | Durée |
|---|---|---|
| un nom (`mm`, `px`, `hello`), défini ou non | nominal / note | persistante, suit la valeur |
| un texte avec un caractère que les noms refusent (`-v`, `what?`) | locale (§3, §4) | éphémère |

### 6.2 Structure de l'objet

Aujourd'hui `ptr1` = contenu `V`, `ptr2` = type `T` (typeRef résolu, ou texte brut). Avec la
piste : `ptr2` pointe une petite structure **immuable et partagée** `{T, N}` :

* `T` : chaîne des nominaux et du type structurel **résolus** (`list.int`, `port` → `int(u,16)`) ;
* `N` : liste des **notes** (nominaux en attente), de la plus ancienne à la plus récente.

Deux listes parallèles : l'ordre relatif d'une couche résolue et d'une note n'est pas conservé
(`@` les sépare). Quand `type def mm …` est écrit, `mm` passe de `N` à `T`.

### 6.3 Affichage

Symétrique autour de `@` : l'extérieur aux extrémités, l'intérieur au centre.

```
 list.int@mm.hello
 └──────┘ └──────┘
 résolu, de gauche à droite     notes, lues de droite à gauche
 (extérieur → intérieur)        (hello = la plus récente, extérieure ; mm = la plus ancienne)
```

Sans note : `list.int`. Sans type résolu : `@mm`. `type get $v` rend la partie gauche ;
`note get $v` rend les notes. Test cible (exemple de l'auteur) :

```
set {list.int}L 10 20 30 40
proc test {l} { switch [note get $l] {mm {…} px {…}} }
test {mm}$L ; test {px}$L          ;# type get : list.int
set A [test {mm}$L]                ;# A garde la note mm
essai {hello}$A                    ;# note get : mm.hello (hello empilée sur mm)
```

(`switch` sur `note get` n'est plus exact dès qu'il y a plusieurs notes : prévoir
`note has $v mm`.)

### 6.4 Opérations

| Écriture | Type résolu | Notes |
|---|---|---|
| `{mm}$x`, `mm` défini | cast (valide) | inchangées |
| `{mm}$x`, `mm` en attente | inchangé | `mm` ajoutée |
| `{}$x` (le type vide) | retiré | **inchangées** |
| `note delete $x mm` | inchangé | `mm` retirée ; `note delete $x` : toutes |
| `note get $x` / `note has $x mm` | | lues |
| `set a $x`, variable non typée | gardé | gardées (exemple : `A` garde `mm`) |
| `set {T}a $x`, variable typée | `T` | effacées par élargissement, comme `port` → `int` (voir 6.6) |
| `set {}a …` | `a` n'a plus de contrat | les notes de la valeur rangée restent |
| `Tcl_DuplicateObj` | partagé | partagées (`{T,N}` immuable) |
| valeur reconstruite (`string toupper`, `dict set`) | perdu | perdues |

`note delete` devient nécessaire parce que la note suit la valeur : les valeurs étant
immuables, c'est une fonction qui rend une valeur (une forme sur variable pourra venir
ensuite). `{}` est « le type vide » : il agit sur le type et rien d'autre. Aujourd'hui
`TCLA_EMPTY` recopie le contenu seul (tclTypedVar.c, adoption d'une valeur) : avec deux canaux
il faut le scinder.

### 6.5 Constats sur le prototype (essais du 18 octobre)

* `type get {zz}$p` rend `zz` : un nom non défini est déjà un nominal en attente.
* `type def port int(u,16)` : `port` entre dans `{int}n` (qui prend le type `int`) ; mais
  `type def mm double` : `mm` est refusé dans `{double}y` sans cast (`{double}$x` passe).
  **Asymétrie à corriger** : la règle actuelle semble « s'efface si la base est strictement
  plus large ». Il faut « s'efface toujours vers sa base ».
* `set m $l` dans une procédure, avec `l` portant `{mm}` non défini, échoue
  (`unknown type "mm"`) : l'adoption par une variable non typée exige un type résolu. Avec
  la piste, elle garderait la note.
* `{mm}{port}$p` est une erreur de syntaxe. L'empilement `{a}{b}$x` **n'est pas nécessaire**
  (`essai {hello}$A` empile en annotant une valeur déjà annotée) et serait ambigu avec
  `{int}{300}` (annotation sur un littéral entre accolades) : écarté.

### 6.6 Travail et questions ouvertes

Travail : (1) structure `{T, N}` et son affichage ; (2) uniformiser l'effacement d'un nominal
vers sa base ; (3) adoption d'une note par une variable non typée ; (4) scinder `{}` ;
(5) commandes `note get`, `note has`, `note delete` ; (6) test calqué sur l'exemple de 6.3.

Ouvert :

* une variable **typée** qui reçoit une valeur à notes : effacer sans bruit (`port` → `int`),
  ou refuser ?
* `set {}a $x` sur une variable auparavant typée : ranger la valeur telle quelle (notes
  comprises), proposition retenue ;
* `{@}$x` pour effacer toutes les notes (le vide à droite de `@`) : seulement si
  `note delete` paraît trop lourd, car `@` deviendrait un marqueur de note persistante,
  distinct des autres signes non-noms, qui restent éphémères ;
* une note posée sur une liste typée (`list.int`) : sur la liste entière ou sur chaque
  élément ? (proposition : sur la liste) ;
* une note sans type résolu (`@mm`) : `T` vide est-il un état légitime, ou une note force-t-elle
  `string` ? ;
* le nom de la commande de lecture (`note`), face à une vue de `type` (p. ex. `type layers`).

### 6.7 Prototype codé (patch types-binaires-notes, test t48)

Décisions de l'auteur : **pas d'empilage** `{a}{b}$x` (on annote une valeur déjà annotée) ;
`{#}` (commentaire du parseur) reste à part.

Fait :

* **Objet à deux listes.** Sans note, `ptr2` reste T seul (aucun coût). Avec notes, c'est un
  objet « annotstate » immuable et partagé `{T, N}` (T peut être NULL : note seule `@mm`).
  `Tcl_DuplicateObj` partage l'état.
* **Poser une note** : `{nom}$x` où `nom` ne se résout pas (ni type, ni variante, ni caractère
  local) ajoute `nom` à N et garde le type résolu. Une annotation littérale `{mm}5` non
  résolue est vue comme une note par `type get`, `note` et par l'adoption.
* **Cast** (`{T}$x`, `T` défini) : le type change, les **notes sont gardées**. `lset` aussi.
  `{}$x` retire le type, garde les notes.
* **Adoption** : une variable non typée garde une note seule (avant : `unknown type "mm"`).
  Une variable typée qui reçoit un nom inconnu refuse toujours (`unknown type`), réponse
  provisoire à la question ouverte « effacer ou refuser ».
* **Consommation** : une note seule se lit comme un nom à la consommation (la plus récente) :
  `type def` donne donc un sens à des notes déjà posées. Une note qui nomme une variante de la
  somme cible garde son rôle (`{d}$v`).
* **Commandes** : `note get V` (notes jointes par « . », de la plus ancienne à la plus récente),
  `note has V nom`, `note delete V ?nom?` (rend la valeur sans la note, ou sans aucune).
* **Affichage** : `type get V` rend `list<int>@mm.hello` (type résolu, `@`, notes) ; sans note
  il rend le type seul comme avant ; sans type, `@mm`.

Pas fait : `{@}$x`, note par élément d'une liste typée (la note est sur la liste entière),
effacement des notes par un `set {T}a` sur variable typée (elles sont perdues, comme
`port` → `int`), uniformisation « un nominal s'efface toujours vers sa base » (6.5).
Seule la note la plus récente est lue comme nom à la consommation.
