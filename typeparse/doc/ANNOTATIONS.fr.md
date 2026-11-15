# Annotations en Tcl — documentation

*7 novembre 2026 · version v16z · Florent MERLET, avec Claude (Anthropic)*

*Traductions : [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md) — les traductions allemande, néerlandaise, espagnole, portugaise et italienne décrivent encore la v13*

## Avant-propos

**Auteurs** : Florent MERLET, avec Claude (Anthropic).

Ce document décrit les **annotations** de Tcl 9.0 proposées par le TIP 764 : une métadonnée attachée à un mot (`{x}mot`) qui ne change pas la valeur, mais qui permet de typer des variables, de nommer des champs et de décrire des formats binaires. Il remplace le manuel écrit au fil du développement ; il décrit l'état du prototype en **version v13**.

Trois parties, pour trois lecteurs :

- **Partie I — Concepts** : ce qu'est une annotation, la différence entre une note et un type, et leurs règles. À lire en premier.
- **Partie II — Référence** : les commandes `note` et `type`, le langage des types, les chemins, la construction des valeurs, les types binaires et les erreurs. Pour qui écrit du Tcl.
- **Partie III — Implémentation** : l'objet annoté, le parseur, le bytecode, les fichiers et fonctions touchés. Pour qui relit le TIP ou le code.

Suivent la compatibilité et les performances, les choix de conception, les limites, et des annexes (installation, glossaire, correspondances avec C, Rust et LLVM).

Les exemples sont du Tcl exécutable sur le prototype ; les commentaires `;#` donnent le résultat. Les messages d'erreur sont cités tels que Tcl les produit, en anglais.

## Partie I — Concepts

### 1. Les annotations

#### 1.1 Origine du mot

*Annoter* vient du latin *ad notare*, « mettre une note à côté ». Le mot *protocole* dit la même chose sous une autre forme : le *prôtokollon* grec était la première feuille collée en tête d'un rouleau de papyrus, qui décrivait son contenu sans en faire partie. Une annotation Tcl est exactement cela : une feuille collée en tête d'un mot.

```tcl
{mm}12          ;# la valeur reste 12 ; « mm » est collé devant
{list.int}L     ;# le nom reste L ; « list.int » dit ce qu'il contiendra
```

#### 1.2 Une métadonnée

Une annotation est une **métadonnée** :

- elle **ne paraît pas dans le texte** de la valeur : `{mm}12` vaut `12` pour `puts`, `expr`, `string length` ;
- elle **apporte une information** que la valeur seule ne porte pas : une unité, un type, le nom d'un champ, une option ;
- elle vit **hors du plan des valeurs brutes** : aucune commande Tcl existante ne la voit, et aucun script existant ne change de sens.

#### 1.3 Une analogie : la partie imaginaire

Un nombre complexe a une partie réelle et une partie imaginaire. De la même façon, une valeur annotée a une partie **brute** (le texte que Tcl manipule depuis toujours) et une partie **annotée** (ce que le développeur a en tête).

| | nombre complexe | valeur annotée |
|---|---|---|
| partie visible | la partie réelle | la valeur brute (`12`) |
| partie ajoutée | la partie imaginaire | l'annotation (`mm`, `int`, `nom`) |
| ce qu'on lit d'habitude | la partie réelle | la chaîne |

L'analogie s'arrête au calcul. Dans un complexe, la partie imaginaire participe à l'arithmétique. Ici, **un calcul perd l'annotation** : `expr {$x + 1}` rend un nombre nu. Seuls la copie, `lindex`, `string cat` et le passage en argument la gardent. C'est voulu : le plan des valeurs brutes reste du Tcl ordinaire, et c'est ce qui garantit la compatibilité.

#### 1.4 Guider l'exécution

L'annotation rend explicite l'« imaginaire » du développeur, ce qu'il sait de ses données et que Tcl ignorait :

- un **type** fait vérifier chaque écriture dans une variable (`set {int}n abc` échoue) ;
- une **étiquette** choisit où va une valeur (`set {pt}a {age:}12 {nom:}Jean`) ;
- une **représentation binaire** dit comment lire et écrire des octets (`{int(u,16,be)}version`) ;
- une **note** marque une valeur pour qu'une procédure la reconnaisse (`note has $v mm`).

Le script garde sa forme : on ajoute des annotations là où l'on veut des garanties, et nulle part ailleurs.

### 2. Notes, types et annotations locales

Une annotation s'écrit toujours `{texte}mot`. Selon ce que dit le texte et selon **qui le lit**, elle joue trois rôles.

#### 2.1 La note : l'annotation brute

Une **note** est une annotation qu'aucun constructeur n'a résolue. Elle suit la valeur : copie, argument de procédure, élément de liste. Depuis la v16m, c'est une **métadonnée** : elle se lit (`note get`, `type get` avec `@`), mais une annotation n'agit que sur la commande qui reçoit le mot (§ 2.2) ; passé cette commande, ce qui n'a pas été résolu reste une note pour toujours.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (vide : aucun type résolu)
```

**Empilage.** On annote une valeur déjà annotée ; les notes s'empilent, de la plus ancienne à la plus récente.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px est la plus récente)
```

**Deux usages des notes (v16y).** Une note est un *proto-type* : un nom posé sur des valeurs avant que le type existe (`{vitesse}12.5`), entre le Tcl nu et le Tcl typé. Une note qui commence par `@` (`{@trace}`, `{@step}`) est une **note d'instrument** : jamais un type, elle sert à la mise au point. Les deux se lisent par `type debug` (§ 3.1).

L'écriture `{a}{b}$x` (deux annotations collées) n'existe pas : elle serait ambiguë avec `{int}{300}`, une annotation sur un littéral entre accolades. On empile en annotant une valeur qui l'est déjà.

#### 2.2 Le type : l'annotation résolue par un constructeur

Un **type** est une annotation **résolue** dans la typologie connue de l'interpréteur (types de base, `type def`, `type alias`). Seul un **constructeur de variable** résout :

- `set {T}v …` (et `set v {T}$x`) ;
- la liaison des arguments de `proc` ;
- `lassign`, `foreach`, `lmap`, `dict for`, `dict map` pour leurs listes et dicts (`foreach {x y} {list}$p`) ;
- `lappend`, `lset`, `linsert`, `lreplace` sur une variable typée, pour les éléments reçus ;
- dans `expr`, `x = v` et `x is T = v` ;
- **`type {T}mot`**, le cast explicite (v16m) : `type {int}$s`, `type {list}$p`, `type {int}[cmd]`, `type {int}[(expr)]`.

Annotée sur le **nom** d'une variable, une annotation donne à la variable un **contrat** : toute écriture ultérieure est vérifiée.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

**Où agit une annotation (v16m, piste B).** Une annotation n'agit que sur la **commande qui reçoit le mot**. Rien n'est résolu pendant la substitution : Tcl substitue tous les mots avant d'appeler la commande, et seule la commande sait si un mot est une valeur ou un nom de variable (`set {int}$nom 5` déclare la variable nommée par `$nom`). Si cette commande est un constructeur, elle résout l'annotation ; sinon l'annotation ne fait rien. Quand la commande finit, ce qui est encore en attente **expire** : une note, que plus aucun constructeur ne résout, même si la valeur a été rendue (`concat`, `return`), rangée dans une liste ou dans une variable, ou si le type est défini plus tard. Une étiquette `{nom:}` ou une annotation locale disparaît.

```tcl
set s 3.0
catch {concat {int}$s} r      ;# 0 : concat ne vérifie rien ; r vaut 3.0, note int
catch {set y {int}$s} r       ;# 1 : set reçoit le mot, il vérifie : expected integer...
catch {type {int}$s} r        ;# 1 : le cast explicite
set q [type {list}$p]         ;# un produit vers la liste de ses champs
```

L'erreur d'un cast naît donc toujours dans la commande qui contient le mot, et un `catch` qui l'entoure l'attrape (avant la v16m, `catch {concat {int}$s} r` laissait l'annotation en attente, et l'erreur naissait en rangeant `r`, hors du `catch`). La v16l faisait un cast immédiat de `{T}$x` ; il est retiré : il castait le nom de `set {int}$nom 5`.

#### 2.3 Deux piles et une règle de dépilage

Une valeur annotée porte **deux listes** : le **type résolu** (au plus un) et la **pile des notes** en attente. L'affichage les sépare par `@` :

```
 list<int>@mm.hello
 └───────┘ └──────┘
 type résolu   notes, de la plus ancienne (mm) à la plus récente (hello)
```

Un constructeur **dépile** depuis le sommet :

1. il prend la note la plus récente et tente de la résoudre ;
2. il continue tant que les notes se résolvent ; la première qui ne se résout pas arrête tout et reste sur la pile ;
3. les types dépilés doivent être cohérents : la plus récente doit entrer dans chacune des autres, sinon `contradictory annotations px and qq` ;
4. la valeur est convertie (« castée ») vers le type le plus récent ;
5. le contrat de la variable fait le dernier contrôle.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px : px se résout, mm non
type get $z          ;# px      ; note get $z : mm
```

Cette pile n'existe que pour les annotations d'**un même mot**, reçues par un même constructeur. Une note posée avant son `type def` ne prend pas sens plus tard (v16m) : la commande qui l'a reçue l'a fait expirer.

**Les étiquettes (v16l).** Un champ de produit ou une variante de somme se nomme par une **étiquette**, `{nom:}valeur` : `set {pt}a {age:}12`, `set {S}v {i:}5`. Une étiquette est une annotation locale : elle ne peut jamais être un type, donc un type global du même nom ne change rien, et `{nom}` sans deux-points est toujours un type ou une note. Elle est lue par le constructeur qui reçoit le mot ; à travers une autre commande (`set {pt}a [concat {age:}12]`), elle a expiré (v16m) et l'argument est positionnel. Une étiquette qu'aucun constructeur ne lit ne se range pas. L'ancienne forme `{age}12`, quand `age` nomme un champ et aucun type, est une erreur qui donne la nouvelle écriture.

#### 2.4 Les annotations locales

Une annotation dont le texte contient un caractère que les noms de types refusent (`-`, `?`, `:`…) ne peut **jamais** être un type. C'est une **annotation locale** : elle ne vit qu'entre l'appelant et la commande appelée, et ne suit pas la valeur.

```tcl
type binary {sizeof?}$T          ;# une question posée à type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# refusé : local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# refusé : local annotation {-v} cannot declare a variable
```

Trois familles naturelles, sans risque de collision avec un nom de type : les options en `-nom`, les questions en `nom?` et les étiquettes en `nom:`. Un nom de type ne contient que des lettres ASCII, des chiffres, `_` et des octets non ASCII, et ne commence pas par un chiffre.

#### 2.5 L'annotation vide `{}`

`{}` est « le type vide ». Son effet dépend de l'endroit :

| où | effet |
|---|---|
| sur une valeur, dans un constructeur (`set v {}$x`) | retire le type résolu, garde les notes |
| sur un nom (`set {}v $x`) | lève le contrat de la variable ; la valeur est rangée avec ses notes, sans rien résoudre |
| sur une valeur, hors constructeur | le lecteur voit le contenu nu (type et notes effacés) |

```tcl
set {int}n 5
set {}n abc          ;# le contrat est levé : n vaut abc
```

La valeur d'origine n'est jamais modifiée : `{}` produit une nouvelle vue, ce qui rend le partage des objets sans danger.

## Partie II — Référence

### 3. La commande `note`

```
note get VALEUR              ;# les notes, de la plus ancienne à la plus récente, jointes par « . »
note has VALEUR NOM          ;# 1 si NOM est l'une des notes, sinon 0
note delete VALEUR ?NOM?     ;# la valeur sans la note NOM, ou sans aucune note
```

Les valeurs Tcl sont immuables : `note delete` **rend** une nouvelle valeur, il ne modifie pas une variable.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace : pas d'empilage collé
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Usage typique : une procédure qui réagit à la marque d'une valeur.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

#### 3.1 La mise au point : `type debug` (v16y)

Une note suit sa valeur : copie, argument, élément de liste, variable de boucle, paramètre de procédure. C'est exactement ce qu'il faut pour suivre une donnée à travers un programme. `type debug on` pose une trace sur chaque commande (les commandes ne sont plus mises en ligne tant qu'elle est posée) ; avant chaque commande, il regarde les notes de ses arguments.

```
type debug ?on|off?        ;# allume, éteint ; sans argument : l'état
type debug next            ;# arrêt à la prochaine commande (un point d'arrêt écrit dans le code)
type debug when ?NOTE ?EXPR?? ;# une condition : {@NOTE} ne se signale que si EXPR est vraie
type debug console ?on|off? ;# la console Tk (sinon l'invite sur stdin)
type debug hook ?CMD?      ;# le crochet appelé (par défaut ::tcl::typedebug::hit)
type debug notes           ;# l'inventaire des proto-types (un dict)
type debug report          ;# le même, lisible, avec suggestions et vérification
type debug clear           ;# vide l'inventaire
```

**Notes d'instrument.** `{@trace}` signale chaque commande qui reçoit la valeur ; `{@step}` s'y arrête aussi. Éteint, `type debug` ne coûte rien et les notes `@` ne font rien : on peut les laisser dans le code.

```tcl
type def pt {{double}x; {double}y}
proc {double}norme {{pt}p} {
    set x2 [expr {$p(x) ** 2}]
    return [expr {sqrt($x2 + [lindex $p 1] ** 2)}]
}
proc chemin {L} {
    set s 0.0
    foreach p $L { set s [expr {$s + [norme $p]}] }
    return $s
}
type debug on
set P {@step}{3.0 4.0}
chemin [list $P {1.0 0.0}]
```

```
@step  set argument 2 = 3.0 4.0   (d.tcl:12, in (global))
step> c
@step  list argument 1 = 3.0 4.0   (d.tcl:13, in (global))
step> c
@step  ::norme p = 3.0 4.0   (d.tcl:8, in chemin)
step> s
@next  expr {$p(x) ** 2}   (d.tcl:3, in norme)
step> p p
  3.0 4.0
step> s
@next  set x2 9.0   (d.tcl:3, in norme)
step> r
@step  lindex argument 1 = 3.0 4.0   (d.tcl:4, in norme)
```

La valeur est suivie dans `list`, dans le `foreach` de `chemin`, jusqu'au paramètre `p` de `norme` (le nom du paramètre est donné). L'invite :

| touche | effet |
|---|---|
| `c` ou Entrée | continuer |
| `s` | pas à pas : arrêt à la commande suivante (dans la procédure appelée s'il y en a une) |
| `r` | continuer sans plus s'arrêter (les `@trace`, `@step` s'affichent encore) |
| `o` | éteindre `type debug` |
| `q` | arrêter : la commande rend une erreur |
| `w` | la pile des appels |
| `v` | les variables locales |
| `p NOM` | la valeur d'une variable |
| autre | une commande Tcl, exécutée là où l'on est arrêté |

L'arrêt sur une note précède la commande : on est chez l'appelant. `s` entre dans la procédure, où ses paramètres sont visibles. `type debug next` écrit dans le code est un point d'arrêt sans note.

**Notes conditionnelles.** Toute note `@nom` autre que `@trace` s'arrête comme `@step`. `type debug when nom EXPR` lui donne une condition : elle ne se signale que si l'expression est vraie. L'expression voit `$value` (la valeur notée), `$cmd` (la commande), `$arg` (le paramètre ou « argument N ») et `$hits` (le nombre de passages de cette note). Une condition sur `trace` filtre les `@trace`.

```tcl
type debug when neg {$value < 0}      ;# s'arrête seulement sur une valeur négative
type debug when cinq {$hits == 5}     ;# au cinquième passage
type debug when trace {$cmd eq "::norme"}
racine {@neg}$v
type debug when neg {}                ;# retire la condition
```

**La console Tk.** `type debug console` remplace l'invite par une fenêtre : l'arrêt courant (note, commande, lieu, valeur suivie avec son type), des boutons (Continuer F5, Pas F10, Courir F8, Arrêter F12, Couper), la pile des appels, les variables du cadre choisi dans la pile avec leur **type et leurs notes**, le journal des passages, et une ligne de commande exécutée dans le cadre choisi. Pendant un arrêt, la console fait tourner la boucle d'événements ; fermer la fenêtre revient à l'invite sur stdin. (`exemples/mise_au_point/console.tcl`, capture `console.png`.)

**Le crochet.** `type debug hook CMD` remplace l'affichage : `CMD note mots indice` est appelé (sans trace) avec la note, les mots de la commande et l'indice de l'argument noté (0 pour un arrêt `next`). Une erreur du crochet empêche la commande de s'exécuter ; c'est ainsi qu'on fait une assertion, ou un journal. Un crochet personnel reçoit toutes les notes `@` : les conditions de `type debug when` sont celles du crochet par défaut.

```tcl
proc rec {note words index} { lappend ::log [list $note [lindex $words 0] $index] }
type debug hook rec
```

**Les proto-types.** Pendant que `type debug` est allumé, toute note dont le nom n'est pas (encore) un type est inventoriée : combien de fois elle a été vue, où la première fois, par quelles commandes, et jusqu'à trois valeurs, chacune avec son lieu. Quand le type est défini ensuite, `type debug report` vérifie après coup les valeurs vues ; tant qu'il ne l'est pas, il propose le type défini le plus proche (une faute de frappe).

```tcl
type def point {{double}x; {double}y}
type debug on
set a {pointt}{3 4}
set v {vitesse}12.5
set w {vitesse}abc
type debug off
type def vitesse double
puts [type debug report]
```

```
pointt: seen 1 time, first at e.tcl:3, via set
    values: 3 4
    not a type; did you mean point?
vitesse: seen 2 times, first at e.tcl:4, via set
    values: 12.5 | abc
    now a type, but:
      abc (e.tcl:5): expected floating-point number but got "abc"
```

`type debug notes` rend la même chose en dict : `nom {count N where F:L commands {...} samples {{valeur lieu}...} status note|ok|mismatch ?errors {{valeur lieu message}...}?}`.

### 4. La commande `type`

```
type def NOM DÉFINITION        ;# définit un type nominal (ou une famille paramétrée)
type def NOM                   ;# rend la définition (de S, d'une variante S.v, d'un champ P.f)
type alias NOM DÉFINITION      ;# donne un nom structurel à une définition
type get VALEUR                ;# le type résolu d'une valeur (vide s'il n'y en a pas)
type cast TYPE VALEUR          ;# le résolveur d'un constructeur, sans variable
type {T}MOT                    ;# le même cast, écrit comme une annotation (v16m)
type convert DE VERS LAMBDA    ;# déclare une conversion explicite
type deref VALEUR              ;# instantané : chaque lien var remplacé par sa valeur
type canonic ?-notes? VALEUR   ;# le texte canonique : {T}contenu, {T@notes}contenu (v17c)
type parse TEXTE               ;# relit un texte canonique en valeur typée (v17c)
type binary TYPE ...           ;# représentation binaire, encodage, décodage (§ 9)
type def {-compact}NOM DÉF     ;# une liste (ou toute liste d'un produit) toujours en cases (§ 14.1)
type def {-a,-b}NOM DÉF        ;# plusieurs options, dans la syntaxe des notes
type is {compact?}TYPE|VALEUR  ;# une question sur un type ou une valeur
type is {disjoint?}S           ;# les paires de variantes qui peuvent accepter une même valeur
type is {union?}S              ;# 1 si S est une union (|) (v16w, § 8.4)
type variant VALEUR            ;# la variante d'une valeur d'une somme ou d'une union (v16w)
type proc NOM                  ;# la signature d'une procédure typée : args, return, record (§ 6.4, 6.5)
type call ?TYPE?               ;# l'appel courant comme valeur : l'appel réifié (§ 6.4, v16z)
type {{*}T}MOT, {{**}T}MOT     ;# le cast explicite, rendu déplié en liste ou en dict (§ 6.4)
type {c!}export TYPE ?TYPE ...? ;# en-tête C des types nommés (§ 14.2) ; {llvm!} prévu
type debug ?on|off|next|when|console|hook|notes|report|clear? ;# mise au point par les notes (§ 3.1, v16y)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (un alias n'est pas un nouveau type)
type cast pt {Léa 7}          ;# Léa 7, de type pt
```

Dans `expr`, l'opérateur **`is`** teste le type d'une variable, et une affectation peut déclarer :

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# déclare z de type pt et lui affecte la valeur
expr {new is cmd}              ;# 1 si new est une commande ; new(…) devient une fonction de expr
```

`is cmd` n'est pas un type : il teste l'existence d'une commande et l'expose comme fonction d'`expr`.

### 5. Le système de types

#### 5.1 Types de base

| type | valeurs |
|---|---|
| `int` | entier (sans borne, comme Tcl) |
| `double` | flottant |
| `string` | toute chaîne |
| `list` | liste Tcl ordinaire (éléments non typés) |
| `dict` | dictionnaire Tcl ordinaire |
| `var` | lien vers une variable, par son nom (§ 5.3) |
| `any` | élément d'une liste hétérogène (`list.any`) : chaque élément garde son type |

Les **raffinements** restreignent un type de base et portent une représentation binaire : `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Un `type def` crée un type **nominal** : `type def port int(u,16)` est distinct de `int(u,16)`. Un `type alias` donne seulement un nom à une définition.

#### 5.2 Le langage de composition

Un type s'écrit comme une expression, analysée sur le modèle d'`expr` (`tclTypeParse.c` reprend la structure de `tclCompExpr.c`).

**Trois sortes d'opérandes :**

| opérande | où | exemple |
|---|---|---|
| type | mot nu, à gauche d'une `}` ou en position d'opérande | `int`, `list`, `pt` |
| nom | mot collé à droite d'une `}` | `{int}age` : le champ `age` |
| valeur | entre guillemets, entre accolades, ou après `=` | `"MZ"`, `= 0` |

**Opérateurs**, du plus fort au plus faible :

| opérateur | rôle | exemple |
|---|---|---|
| `.` | imbrication (associatif à droite) | `list.dict.int` |
| `( )` | arguments, collés au nom | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` ou `?=` | valeur par défaut d'un champ | `{int}age ?= 0` |
| `;` | produit | `{double}x; {double}y` |
| `,` | somme (balisée) | `{int}i, {string}s` |
| `\|` | union (v16w) | `{int}n \| {list(2).int}paire` |
| `{ }nom` | groupement, avec un nom collé à l'accolade fermante | `{{int}x; {int}y}A` |

`,` et `|` sont au même rang et ne se mêlent pas non plus (§ 8.4). `;` et `,` ne se mêlent pas au même niveau (`{int}a; {int}b, {string}c` est refusé) : la partie intérieure se regroupe entre accolades et se nomme (`{int}a; {{int}b, {string}c}u`), ou se définit à part avec `type def`. Dans un produit, tout champ a un nom, un groupe aussi. Dans une somme, toute variante typée a un nom, qui s'écrit dans sa valeur (v16w) ; une union peut garder ses alternatives anonymes (`{int} | {list(2).int}`). Un champ seul entre accolades et nommé est un produit à un champ : `{{double}r}cercle`.

**Caractères.** Un nom de type ne contient que des lettres ASCII, des chiffres, `_` et des octets non ASCII, sans commencer par un chiffre. La grammaire emploie en plus `{ } , ; | . = " # ( ) * $`. Tout autre signe ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ ~`) est refusé : `invalid character "!"`, code `TCL PARSE TYPE BADCHAR`, sauf après `=` (une valeur par défaut) et dans les parenthèses d'un constructeur (`*(file://x, …)`). C'est ce refus qui rend les annotations locales (`{-v}`, `{is?}`) sûres.

Une définition est **déclarative** : aucune substitution n'y est faite à la lecture, sauf dans une valeur par défaut, évaluée à chaque construction.

**Grammaire (EBNF, notation ISO).** Précédence, de la plus faible à la plus forte : `;`, puis `,` et `|`, puis `=`, puis `.`. `;`, `,` et `|` ne se mêlent pas à un même niveau.

```ebnf
type        = item , { ";" , item }            (* produit *)
            | item , { "," , item }            (* somme balisée *)
            | item , { "|" , item } ;          (* union disjointe *)
item        = chain , [ default ] ;
default     = ( "=" | "?=" ) , value ;
chain       = primary , [ "." , chain ] ;      (* imbrication, associative à droite *)
primary     = group
            | typename , [ "(" , args , ")" ]  (* list(3), int(u,16), pt(double) *)
            | "$" , name                       (* paramètre, dans le corps d'une famille *)
            | "*(" , linkargs , ")"            (* lien binaire *(origine, repr, options) *)
            | quoted ;                         (* littéral : "rouge", "MZ" *)
group       = "{" , type , "}" , [ fieldname ] ;   (* {T}nom : un champ ; {T} seul : un groupement *)
fieldname   = [ "-" ] , name , [ "..." ] ;         (* -nom : une option ; nom... : variadique *)
typename    = [ "::" ] , name , { "::" , name } ;
name        = namestart , { namechar } ;
namestart   = letter | "_" | nonascii ;
namechar    = namestart | digit ;
value       = quoted | braced | bareword       (* un défaut littéral *)
            | "[" , script , "]"               (* calculé à chaque construction *)
            | "$" , name ;                     (* un paramètre, ou un argument déjà lié *)
comment     = "#" , { any - newline } ;

args        = count                                              (* list(N) *)
            | ( "u" | "s" ) , "," , count , [ "," , endian ]     (* int *)
            | ( "32" | "64" ) , [ "," , endian ]                 (* double *)
            | encoding , "," , ( count | "z" | expr )            (* string(utf-8,z), string(raw,$len - 4) *)
            | count , [ "," , ( "lsb0" | "msb0" ) ] , [ "," , endian ]   (* bitarray *)
            | [ key , { "," , key } ]                            (* dict(x, z?=0); dict() *)
            | expr                                               (* at($off) *)
            | type , { "," , type } ;                            (* une instance de famille : pt(double) *)
key         = name , [ "?" , [ "=" , value ] ] ;
endian      = "be" | "le" ;
family-head = name , "(" , param , { "," , param } , ")" ;      (* type def {pt({type}T)} ... *)
param       = "{" , ( "type" | "int" | "double" | "string" ) , "}" , name ;
```

Ce que la grammaire ne dit pas : dans un produit, tout champ et tout groupe a un nom ; dans une somme, toute variante typée ; `-nom` et `nom...` n'existent que dans une signature de procédure.

#### 5.3 Champs et conteneurs

**Énumération** — une somme de littéraux.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# un seul littéral : une constante (signature de fichier)
```

**Somme** — une seule variante à la fois, dont la valeur **écrit le nom** (v16w) : `variante charge`, ou le littéral seul.

```tcl
type def S {{int}i, {string}s}
set {S}v {i:}5                 ;# v vaut "i 5"
set {S}v i 5                   ;# la même chose, écrite en clair
type get $v                    ;# S.i
set {S.s}w abc                 ;# w vaut "s abc" : la variante choisie par le nom du type
```

**Union** — une valeur de l'une des formes, sans balise : la valeur est la charge seule, et l'alternative se calcule. Les alternatives doivent être disjointes (v16w, § 8.4).

```tcl
type def U {{int}n | {list(2).int}paire}
set {U}u 3 4                   ;# u vaut "3 4", de type U.paire
```

**Produit** — une `struct` : des champs nommés, ordonnés, chacun de son type.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positionnel
set {pt}b {age:}12 {nom:}Jean  ;# étiquettes de champ (§ 8)
$a(nom)                        ;# Jean
```

**Liste** — `list` (extensible), `list.T` (éléments de type T), `list(N).T` (exactement N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

**Scalaires et conteneurs (v16t).** La structure d'une valeur typée vient de son type, jamais d'une relecture de sa chaîne :

- un **scalaire** typé (`string`, nombre, littéral d'une somme) est **un seul** élément de liste : `set {string}s "a b c"` donne `llength $s` = 1. Les éléments d'une `list.string` restent des chaînes à tous les niveaux d'imbrication : `foreach r $M {foreach s $r …}` ne redécoupe jamais `{hello world}` en deux mots ;
- un **conteneur** (`list`, `dict`, **produit**) se déplie : un produit est la liste de ses champs, chacun dans son type. `set {point}P 1 2` donne `llength $P` = 2, `lassign $P x y`, `foreach {x y} $P`, `{*}$P`, `concat $P $P` = `1 2 1 2`, comme pour la liste `{1 2}`. Un champ `string` reste atomique.

La liste nue `{list}` garde le comportement de Tcl. (Jusqu'à la v16s, un produit était lui aussi un seul élément, « règle B » : deux valeurs de même chaîne avaient alors deux structures, voir `doc/CARTOGRAPHIE-EIAS.md`.)

**Dictionnaire** — `dict` (toutes clés), `dict.T` (valeurs de type T), `dict(clés).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom obligatoire ; age facultatif, 0 par défaut
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)` : z peut manquer ; `dict()` : seule la clé vide. Les clés sont toujours du texte ; seules les valeurs sont typées.

**Lien `var`** — le NOM d'une variable, suivi à chaque lecture.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (un instantané, de type list.int)
```

**Lire un lien seul.** C'est le principe habituel de Tcl (`set A 1; set r A` : `$r` donne `A`, `set $r` donne `1`). Un lien sorti de son conteneur, ou une variable de type `var.int`, vaut le **nom canonique** de la variable ; `set` en donne la valeur. Dans un conteneur, la lecture par chemin suit le lien, et l'écriture par chemin écrit **dans** la variable (son contrat est vérifié).

```tcl
set r [lindex $L 0]            ;# ::A   (type var<int>)
set $r                         ;# 7
set {var.int}R A               ;# ::A ; set $R donne 7
set L(0) 8                     ;# écrit 8 dans ::A
```

Toute écriture d'un lien range le nom canonique : construction, `lset L 1 C`, `lappend L C` (`::C`). Un nom vide est refusé ; tout nom de variable Tcl est accepté (`lappend L 5` range `::5`). Le lien vers une variable locale de procédure a un nom canonique propre au cadre (`frame:1:x`) : `$M(0)` le suit, mais ce n'est pas un nom que `set` connaît.

**Paramètre de type `{type}T`** — dans un type paramétré, un paramètre qui reçoit un type (§ 5.5).

**Lien binaire `*(origine,repr).T`** — une adresse dans des données binaires, suivie vers une valeur T (§ 9.5).

#### 5.4 Valeurs par défaut

`?=` (ou `=`, l'ancienne écriture, encore acceptée dans un produit) donne un défaut à un champ. Un défaut peut être un littéral, un `[script]` évalué à chaque construction, ou une `$variable` lue à chaque construction.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Un champ sans défaut doit être donné : `field "x" of pt has no value and no default`.

#### 5.5 Types paramétrés

Le nom d'un `type def` peut porter des paramètres, déclarés comme des champs : `{type}T` (un type), `{int}n`, `{double}f`, `{string}s`. Ils s'emploient dans le corps avec `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x:}1.5      ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# récursif
```

#### 5.6 Paramètres fantômes : les unités physiques

Un paramètre absent du corps est permis : il ne change pas les valeurs, mais distingue les instances, qui sont des types nominaux différents. Une famille de grandeurs donne ainsi des unités dont la valeur reste un `double` :

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# règle non applicable
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (conversion explicite)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Une règle `type convert` peut viser un type ou une **famille** (le nom d'un `type def` paramétré) ; la lambda reçoit alors les paramètres des instances (`$de(f)`). `return -code continue` dit « non applicable ». Les conversions restent explicites.

**Un seul ordre de résolution (v17d)**, le même pour `set`, `type cast` et les arguments d'une procédure. Une valeur *typée* entre dans un type T telle quelle si elle est de type T, si elle s'y élargit (`int` → `double`, `Celsius` → `double`), ou si T est une somme dont une variante a exactement son type (`{S}` avec `{int}i` prend `{int}5` en `i 5`, avant toute règle). Une règle (`type convert`) ou une conversion intégrée (troncature, `double` → `Celsius`, `string` → `int`) est un cast, et un cast s'écrit : `type cast`, ou `{T}$x`, à l'appel compris (`VeutEntier {int}$d`) ; un argument typé n'est pas un cast. Une valeur *nue* (ou seulement notée) est lue par le lecteur de T ; un type nominal qui a une règle depuis son type sous-jacent (`type convert string Jour`, `type convert double Pos`) a là sa validation, appliquée à toute valeur nue ; quand la lecture native échoue, une règle `type convert any T` (ou `string T`) lit le texte. `set {Pt}p $x`, un seul argument pour un produit de plusieurs champs, prend `$x` comme la valeur entière.

```tcl
proc VeutEntier {{int}i} {...}
VeutEntier 2.5                 ;# erreur : expected integer but got "2.5"
VeutEntier $d                  ;# ($d un double) type mismatch ... (use an explicit cast)
VeutEntier {int}$d             ;# 2
type convert string Jour {{s} {if {![regexp {^\d{4}-\d\d-\d\d$} $s]} {error "pas une date"}; set s}}
set {Jour}j 10/11              ;# pas une date
```

#### 5.7 Alias fournis

`layout/binaliases.tcl` définit, pour les formats binaires (conventions de Rust et de Zig) :

| alias | définition |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| suffixes `be`, `le` (`u16be`, `i32le`) | boutisme fixé |
| `f32 f64` (et `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, "none"` (valeurs `some X` ou `none`, v16w) |

Ces alias sont globaux (`::u8`…) : le fichier les définit avec leur nom qualifié depuis son espace de noms (§ 5.8).

#### 5.8 Espaces de noms (v16u)

Un type appartient à l'espace de noms où il est défini, comme une procédure :

```tcl
namespace eval geom {
    type def point {{double}x; {double}y}      ;# ::geom::point
}
type def point {{string}nom}                    ;# point, global
type def ::geom::seg {{geom::point}a; {geom::point}b}
```

- **Définition.** `type def NOM` range le type dans l'espace courant ; `type def ::NOM` dans l'espace global ; `type def a::NOM` dans l'espace `a` (relatif à l'espace courant, puis global, comme `proc`), qui doit exister. Les types prédéfinis (`int`, `string`, `list`…) ne se redéfinissent pas dans un espace.
- **Résolution.** Un nom se résout comme une commande : `point` dans l'espace courant, puis dans son chemin (`namespace path`), puis dans l'espace global ; `geom::point` dans l'espace courant puis global ; `::geom::point` tel quel. Le corps d'un type est lu dans l'espace où il est défini ; le corps d'une famille (`type def {grille({int}n)} …`) dans l'espace de la famille, quel que soit l'endroit où elle est instanciée ; la signature d'une procédure (arguments et retour) dans l'espace de la procédure, comme son corps : `proc {point}::geom::mk {…}` rend un `::geom::point`, même définie depuis l'espace global.
- **Nom affiché.** `type get`, `type def` et les messages donnent le nom qualifié d'un type d'un espace (`::geom::point`, `list<::geom::point>`, `::geom::S.at`), le nom simple d'un type global. Ce nom se relit. Pour comparer à un nom court, `namespace tail` : `switch [namespace tail [type get $b]] {bloc.para {…}}`.
- **Bibliothèques.** Un nom de type passé en chaîne à une procédure d'un autre espace s'y résout dans cet espace (comme un nom de commande) : passer le nom qualifié. Un paquet qui veut des types globaux les nomme `::u8` (`binaliases.tcl`, `cstruct.tcl`).
- **Suppression.** `namespace delete geom` retire les noms de ses types (et ses règles `type convert`) ; les valeurs gardent leur type, sous son nom qualifié. Un nouveau `::geom::point` est un autre type.
- **C.** `type {c!}export` nomme la struct C d'après le dernier mot (`::math::geometry::point` → `point`) ; si deux types exportés ensemble ont le même, le second prend le nom qualifié (`ce2_cp`).
- **Durée de vie.** Une valeur typée garde en vie la table de types de l'interpréteur qui l'a créée : passée à un autre interpréteur, elle survit à `interp delete` (v16u ; avant, sa lecture lisait de la mémoire libérée).

### 6. Variables typées et vie d'une valeur annotée

#### 6.1 Le contrat d'une variable typée

Une variable déclarée par `set {T}v …` a un **contrat** : chaque écriture est vérifiée, qu'elle vienne de `set`, `lappend`, `lset`, `incr`, `dict set`, d'une écriture de chemin (`set v(x) …`) ou d'`expr {v = …}`. Toutes ces commandes écrivent aussi par chemin : `incr v(age)`, `append v(nom) X`, `lappend v(notes) 3`, `dict set v(d) k 1` lisent la partie, la modifient et l'écrivent, avec les mêmes vérifications que `set v(age) …`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Élargissement automatique.** Un raffinement entre là où l'on attend un type plus large : `int(u,8)` dans `int`, `int(s,16)` ou `double` ; `double(32)` dans `double(64)`. Le rétrécissement est refusé (il demande un cast explicite), de même qu'entre `bitarray(…,lsb0)` et `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# accepté : type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominaux.** Un type nominal s'efface devant sa base (`px` entre dans `int` et donne `int`, sans trace). Pour une collection, la base est sa structure : `type def polygone list.point` entre dans `list.point`, `list(4).point` ou un champ `{list($n).point}` (le nombre d'éléments est vérifié). Deux nominaux sans lien de base ne sont jamais équivalents (`mm` et `kg`) : c'est précisément leur rôle.

**Casts explicites.** `set v {T}$x` ou `type cast T $x` convertissent selon, dans l'ordre : une règle `type convert` spécifique ; la chaîne des bases ; la variante d'une somme ; une règle de famille ; les conversions intégrées entre scalaires. Sinon : `unknown way to cast the value from A to B`.

**Produit vers collection.** Tout produit se convertit en liste ou en dict : `{list}$p` donne la liste de ses champs, nus (depuis la v16t, `foreach {x y} $p` suffit) ; `{list.any}$p` les garde annotés, comme `$p(*)` ; `{list(2).double}$p` les valide ; `{dict}$p` donne `x 3 y 4`. Une liste nominale se convertit de même en liste structurelle (`{list.point}$P`, `{list}$P`). Le cast se fait dans la commande qui reçoit le mot : `foreach {x y} {list}$p`, `lassign {list}$p x y`, `dict for {k v} {dict}$p`, ou `type {list}$p` ailleurs (`llength [type {list}$p]`) ; `llength {list}$p` ne caste rien (v16m).

**Lever le contrat.** `set {}v valeur` (§ 2.5).

**Tester.** `expr {v is T}` : vrai si la variable est typée et que son type entre dans `T` ; pour une variable non typée, vrai si sa valeur est valide pour `T`.

#### 6.2 Ce qui garde l'annotation, ce qui la perd

| opération | type | notes |
|---|---|---|
| copie (`set a $x`), argument, valeur de retour | gardé | gardées |
| `lindex`, `foreach`, `$L(i)` sur une `list.T` | élément typé `T` | — |
| `string cat $x` (un seul argument) | gardé | gardées |
| calcul (`expr`, `string toupper`, `dict set` sur une valeur) | **perdu** | **perdues** |
| cast `{T}$x` dans un constructeur | changé | gardées |
| `set {T}a $x`, variable typée | `T` | effacées par élargissement |

**Pas d'adoption (v16l).** Une variable **non typée** qui reçoit une valeur typée garde la valeur et son type, mais ne prend **aucun contrat** : elle accepte ensuite n'importe quelle valeur. Seul `set {T}v` (ou une déclaration) pose un contrat.

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e vaut 2, de type int
set e hello                    ;# accepté : e n'a pas de contrat
```

Le piège de `catch … m` des versions antérieures n'existe donc plus.

**Procédures.** Un argument ordinaire (liste d'arguments classique) reçoit la valeur et son type, sans contrat : `proc f {x} {set x 1.5}` puis `f $n`, avec `n` un `int`, rend 1.5. Pour un argument vérifié, utiliser une signature typée (§ 6.4).

**Conversions.** Une valeur typée se lit comme un nombre si sa chaîne en est un et que son type n'est pas une collection : `string`, une énumération (`type def pi {"3.14159"}` puis `expr {$PI * 2}`), un produit d'un seul champ (`{{int}i}`). Un produit de plusieurs champs, une liste ou un dict restent refusés. `string is` juge toujours la chaîne.

**Variables de boucle.** `foreach`, `lmap`, `dict for`, `dict map` (et le filtre de `dict filter`) oublient le type de leur variable avant chaque affectation ; après la boucle, la variable garde le type du dernier élément reçu.

**Listes de nombres (v16s, v16t).** Les éléments d'une `list.int` ou `list.double` (types simples : ni nominaux, ni binaires), et les champs `int` ou `double` d'un produit, restent nus dans la liste : ils ont été vérifiés avec elle. `foreach`, `lmap`, `lassign` (le reste) et `lrange` les rendent nus ; `lindex $L i` et `$L(i)` en font une valeur typée à la demande. Une variable ou une procédure typée qui reçoit un élément nu le revérifie. Les listes de chaînes, de produits, de sommes, de `any` ou de types nominaux gardent leurs éléments typés. (Envelopper chaque élément d'une liste neuve coûtait 2,5 fois la boucle. Un nombre nu ne peut pas être relu comme une liste ; une chaîne nue le pourrait : la v16s rendait aussi les éléments d'une `list.string` nus, la v16t les garde typés.)

#### 6.3 `type deref` : un instantané des liens

`type deref V` rend `V` où chaque lien `var` est remplacé par la valeur qu'a la variable à cet instant. Le résultat a le type « sans les liens » (`list.var.int` → `list.int`), nommé `deref(<type>)` pour un produit ou une somme. Un seul niveau est suivi par appel ; une variable manquante est une erreur qui nomme la case (`type deref: at 2: can't read "::C": no such variable`), sans résultat partiel. Fonctionne dans les listes, dicts, produits, sommes et types récursifs ; un cycle ne boucle jamais.

#### 6.3b `type canonic` et `type parse` : la forme texte d'une valeur typée (v17c)

Le type n'est jamais dans la chaîne d'une valeur (EIAS). Là où seul un texte est gardé (un fichier, un tag Tk, une clé), `type canonic V` donne un texte qui le garde : `{T}contenu`, `T` étant le texte du type (son nom s'il est nommé, qualifié pour un type d'espace ; sans la variante : une somme étiquetée la porte dans son contenu, une union la lit dans le sien) et le contenu sa forme canonique (nombres normalisés ; listes, dicts, produits, sommes reconstruits selon le type), mis en forme d'un élément de liste. Les notes sont un ensemble : `{double@alpha,zeta}5.0`, triées, sans doublon, séparées par des virgules ; dans une note, tout caractère ASCII hors lettres, chiffres, `_` et `.` s'écrit `%XX`. Une valeur seulement notée donne `{@vitesse}5` ; une valeur nue est refusée. `-notes` donne `T@notes`, sans le contenu.

`type parse TEXTE` relit `{T}contenu`, `{T@notes}contenu` ou `{@notes}contenu` : le type doit exister (vérifié contre sa définition actuelle), le contenu doit y entrer, puis les notes sont posées. Deux lois : `type parse [type canonic $v]` est `$v`, et `type canonic [type parse $t]` est `$t` quand `$t` est canonique.

```tcl
type canonic [type cast int 0x10]                 ;# {int}16
type canonic [type cast list.int "1   2"]         ;# {list.int}{1 2}
type canonic {vitesse}[type cast double 5]        ;# {double@vitesse}5.0
type get [type parse {{pt}{1 2.5}}]               ;# pt
```

Pas gardés : les notes des éléments d'un conteneur, les types des éléments d'une `list.any`, l'option `{-compact}` (une représentation).

#### 6.4 Procédures typées (v16p)

La liste d'arguments d'une procédure peut être un **type produit**, écrit dans la syntaxe des types ; l'annotation du nom donne le **type de retour**.

```tcl
type def pt {{double}x; {double}y}
proc {pt}decale {{pt}p; {double}dx ?= 0; {double}dy ?= 0} {
    set {pt}q [expr {$p(x) + $dx}] [expr {$p(y) + $dy}]
    return $q
}
decale $a 1 1                  ;# positionnel
decale $a {dy:}5               ;# positionnels, puis étiquettes
decale {dy:}5 {p:}$a           ;# étiquettes seules, ordre libre
```

- **Champs.** Chaque champ devient une variable locale qui porte son contrat (`set dx abc` dans le corps est refusé). Un défaut `?= v` rend le champ facultatif ; `?= [cmd]` est évalué à chaque appel, dans la procédure : il peut lire les champs précédents (`{int}b ?= [expr {$a * 2}]`).
- **Appel.** Des arguments positionnels, puis des étiquettes `{nom:}` ; un positionnel après une étiquette est refusé, un champ donné deux fois aussi. Une étiquette qui ne nomme pas un champ reste sur la valeur si le champ visé est une somme (`ps {s:}abc` choisit la variante `s`), sinon c'est une erreur (`unknown argument label {zz:}: must be p:, dx: or dy:`). Messages : `wrong # args: should be "decale p ?dx? ?dy?"`, `missing argument "p"`, `argument "dx": expected floating-point number but got "abc"`.
- **Types écrits dans la signature.** Le type d'un champ peut être un produit anonyme : la signature se lit d'elle-même, et une liste nue est vérifiée et devient la valeur du champ. Un champ dont le type est un produit **dont tous les champs ont un défaut** est facultatif : absent, il prend ces défauts. Une liste incomplète reste refusée (la règle des constructeurs : pour ne donner que certains champs, les nommer).

```tcl
proc {pt}decale {
    {{double}x; {double}y}p;
    {{double}x ?= 0; {double}y ?= 0}dp
} {(
    $p(x) + $dp(x), $p(y) + $dp(y)
)}
decale {100 200} {50 30}       ;# 150 230, de type pt
decale {100 200}               ;# 100 200 : dp prend ses défauts
decale {100 200} {50}          ;# argument "dp": {{double}x ?= 0; {double}y ?= 0} needs 2 values but got 1
```

- **Nommer les types qui circulent.** Un produit écrit dans la signature est un type à lui seul : une valeur `pt` de même forme y est refusée (`unknown way to cast the value from pt to {{double}x; {double}y}`), il faudrait `{list}$a` ou un `type convert`. Et une valeur déjà du type d'un champ nommé y entre sans être revérifiée (`{pt}p; {vec}dp` : 1,38 µs avec des valeurs typées, 1,95 µs avec des listes nues, comme la forme anonyme). Les types qui passent de procédure en procédure gagnent donc à être nommés (`type def pt …`) ; la forme anonyme convient à ce qui n'entre que sous forme de liste ou de texte.
- **Le reste.** Un dernier champ `{list.T}args` reçoit les arguments en trop, chacun validé (`proc somme {{int}a; {list.int}args} …`).
- **Variadiques n'importe où (v16y).** Un champ dont le nom finit par `...` (`{T}args...`) prend un nombre variable de mots, où qu'il soit : sans défaut, **au moins une fois** ; avec `?=` (souvent `?= {}`), zéro fois ou plus, et le défaut est une liste de `T`. Les champs d'avant se remplissent de gauche à droite (un champ à défaut ne prend un mot que s'il en reste assez pour la suite), ceux d'après se prennent depuis la fin, comme dans TIP 288 ; ils ne peuvent pas avoir de défaut. Si `T` est un produit, chaque répétition prend autant de mots qu'il a de champs. La locale (`args`) est une liste de `T`. Un seul champ variadique par signature, et pas un `var.T`. `{list.T}args` en dernier garde son sens (zéro ou plus, un mot par `T`).

```tcl
proc chacun {{{list}vars; {list}valeurs}args...; {string}corps} { … }   ;# foreach
chacun {a b} {1 2 3 4} c {x y} { … }        ;# usage : chacun vars valeurs ?vars valeurs ...? corps
proc dict_set {{var.dict}d; {string}args...; {string}valeur} { dict set d {*}$args $valeur }
proc dict_create {{{string}cle; {string}valeur}args... ?= {}} { … }
proc longueur {{pt}args...} { … }           ;# longueur 0 0 3 4 3 0 : les points à plat
```
- **Upvar automatique.** Un champ `{var.T}n` reçoit un nom de variable : `n` est liée à la variable de l'appelant, dont la valeur courante (si elle existe) doit être un `T`.

```tcl
proc incrv {{var.int}n; {int}d ?= 1} { incr n $d }
incrv k 10                     ;# k est la variable de l'appelant
```

- **Retour.** `proc {T}nom` : le résultat d'un retour normal (fin du corps, `return`, `return -code ok`) est casté en `T`, comme `type {T}mot` ; un échec est une erreur `return value {T}: …`. Une erreur, `break`, `return -level 2`, `tailcall` passent sans cast. Un `return {U}$x` dans le corps n'ajoute rien : `{U}` n'est qu'une note locale, qui expire avec la procédure (pas de cumul de types sur le retour).
- **Étiquettes de retour.** Quand le type de retour est une somme, `return {v:}$x` choisit la variante `v` : `$x` est sa charge. Une étiquette qui n'est pas une variante, ou sur un type de retour qui n'est pas une somme, est une erreur. `proc {S.v}f` rend toujours la variante `v`. `return {v 4}` (la valeur écrite en clair, v16w) marche aussi. L'étiquette est lue aussi dans un corps non compilé (`eval`, `uplevel`, `switch -matchvar`, v16x) : le retour y est rendu dans le type de la procédure avant que l'étiquette expire.

```tcl
type def R {{pt}point, {int}n}
proc {R}g {w} {
    if {$w eq "p"} { set {pt}z 1 2; return {point:}$z }
    return {n:}4
}
type get [g p]                 ;# R.point
```

- **Introspection.** `type proc nom` rend `args {…} return T` (la signature canonique, vide pour une procédure classique) ; `info args` rend les noms des champs, `info default` leurs défauts (`[cmd]` pour un défaut calculé).
- **Coût.** Une procédure typée va aussi vite que la même procédure classique qui ferait tous ces contrôles à la main (1,15 µs contre 1,17 µs pour `decale $a 1 1`, sans contrôle 0,47 µs) ; une étiquette coûte environ 0,5 µs. Voir `exemples/bench_procs.tcl`.
- **Compatibilité.** Une liste d'arguments qui est une liste Tcl valide reste classique (`{a {b 2} args}`, et même `{a;}`) ; elle est typée quand elle n'est pas une liste et contient un `{T}nom`, ou quand elle contient des `;` et se lit comme un produit. Une procédure classique peut avoir un type de retour (`proc {int}f {a} …`).

- **Dépliage du résultat (v16r)** : `proc {{*}T}nom` vérifie le résultat comme un `T`, puis le rend **déplié**, en données Tcl ordinaires ; `{{**}T}` le rend en dict. Le `{*}` déplie comme celui de Tcl (et comme `$P(*)`), `{**}` donne les noms, comme `*args` et `**kwargs` en Python. Le dépliage est profond : un produit de produits devient une liste de listes (ou des dicts imbriqués), une `list.point` une liste de listes ; une somme rend le contenu de sa variante (avec `{**}`, un dict `{variante valeur}` ; une variante littérale reste elle-même). Depuis la v16t, un produit typé se déplie déjà dans la vue liste (`lassign $P x y` marche sur un `point`) : `{*}T` ne sert plus qu'à rendre des données **sans type**, et `{**}T` un dict.

```tcl
proc {{*}point}v  {{double}h} {( 0, $h )}     ;# 0 5      (vérifié comme point)
proc {{**}point}vd {{double}h} {( 0, $h )}    ;# x 0 y 5
type def rect {{point}nw; {point}se}
proc {{*}rect}r {} {( (0, 0), (10, 10) )}     ;# {0 0} {10 10}
proc {{**}rect}rd {} {( (0, 0), (10, 10) )}   ;# nw {x 0 y 0} se {x 10 y 10}
```

  Le cast explicite suit la même règle : `type {{*}point}$p` rend la liste `{x y}` vérifiée, `type {{**}point}$p` le dict. Le préfixe n'est reconnu qu'en tête d'une annotation de livraison (dans le langage des types, `{*}point` se lirait comme un champ nommé `point`). Coût : 0,3 à 0,9 µs par appel pour vérifier et déplier un point (`exemples/tcllib_geometry`).

- **L'appel réifié : `type call` (v16z).** Une signature est un produit : les arguments liés d'un appel en sont une valeur. `type call`, dans une procédure, rend l'appel courant comme valeur de l'**enregistrement** de sa signature : chaque champ sous le nom de sa locale (`-mode` : `mode`), avec son type ; un champ variadique y est une liste (`{int}args...` : `list.int`), un lien `{var.T}n` la valeur de la variable. Un champ sans valeur à cet appel (option absente sans défaut, lien vers une variable qui n'existe pas) n'y est pas. `type call TYPE` construit plutôt une valeur de `TYPE`, un produit nommé, dont chaque champ prend la locale de même nom, ou son défaut. `type proc NOM` donne le texte de l'enregistrement (`record`), qu'on peut nommer : `type def Demande [dict get [type proc demander] record]`. Une procédure classique a l'enregistrement `{any}a; …; {list}args`.

```tcl
proc {Rec}deplacer {{pt}p; {double}dx; {string}-note ?= "?"} {
    set evt [type call Rec]          ;# Rec : {{pt}p; {double}dx; {string}note}
    lappend ::journal $evt
    return $evt
}
deplacer -note retour {1 0} -1      ;# {1 0} -1 retour, de type Rec
dict get [type proc deplacer] record ;# {pt}p; {double}dx; {string}note
```

  C'est l'événement comme donnée : un appel qu'on range, qu'on journalise, ou qu'un autre appel reçoit en argument (`exemples/attentrel`, où une convolution en vise une autre).

- **Lambdas : `apply` (v16z).** Une lambda est une procédure sans nom : sa liste d'arguments accepte la même signature, et l'annotation du mot lambda est son type de retour, comme celle du nom dans `proc {T}nom`. Les types sont lus dans l'espace de noms de la lambda (son troisième élément). L'annotation peut porter sur une substitution : `apply {T}$f …` donne un type de retour à une lambda gardée dans une variable. Une règle de `type convert` étant une lambda, elle peut typer son argument.

```tcl
set somme [apply {double}{
   {
      {double}a;
      {double}b
   } {(
      $a + $b
   )}
} 5 10]                                          ;# 15
apply {int}{{x} {return $x}} 2.5                 ;# return value {int}: expected integer but got "2.5"
apply {int}$f 20.5 5                             ;# de même, sur une lambda gardée dans f
lmap x $L { apply {int}{{{int}x} {expr {$x * $x}}} $x }
type convert celsius kelvin {{{celsius}c} {expr {$c + 273.15}}}
```

  Le littéral `{T}{…}` est refait à chaque exécution ; l'interpréteur garde une Proc par texte `{T}lambda` (au plus 512, puis la table est vidée), pour que le corps ne soit pas recompilé à chaque appel. Coût : 0,8 µs par appel contre 0,5 µs pour la même lambda sans type de retour. Une note (`{@trace}{…}`) n'est pas un type de retour.

#### 6.5 Options et drapeaux (v16q)

Un champ dont le nom commence par `-` est une **option**. Les options se donnent au début de l'appel, dans n'importe quel ordre, jusqu'au premier mot qui n'en est pas une (ou `--`) ; un préfixe unique suffit (`-dec` pour `-decreasing`) ; la variable locale perd le `-`.

```tcl
proc {list}trier {
    {flag}-decreasing;
    {int}-index ?= -1;
    {"ascii", "dictionary", "integer", {string}command}-mode ?= ascii;
    {list}l
} {
    set o [list -$mode]                ;# mode : ascii, dictionary, integer ou command
    if {$mode eq "command"} {set o [list -command $command]}
    if {$decreasing} {lappend o -decreasing}
    if {$index >= 0} {lappend o -index $index}
    lsort {*}$o $l
}
trier -dec -index 1 -integer $L
```

- **Drapeau** : `{flag}-x` ; la locale vaut `1` si l'option est donnée, sinon `0` (ou son défaut). `flag` est un type prédéfini (un entier).
- **Option à valeur** : `{T}-x ?= v` ; `-x valeur`, vérifiée comme un argument. Sans défaut et absente, la locale reste indéfinie (`info exists x`). Un défaut calculé (`?= [cmd]`, `?= $var`) n'est évalué que si l'option manque, et après les arguments positionnels : il peut les lire (`{int}-fin ?= [expr {[llength $l] - 1}]`, v16y). Un défaut calculé d'argument voit les options données et celles à défaut littéral.
- **Option lien** : `{var.T}-o` ; `-o nom` lie la locale `o` à la variable `nom` de l'appelant, comme `{var.T}o` (v16y). `?= nom` donne la variable par défaut ; sans défaut et absente, `o` est une simple locale. L'usage l'écrit `?-o varName?`.
- **Options exclusives** : une somme. Chaque variante est une option : un littéral (`"ascii"`) est une option sans valeur, une variante typée (`{string}command`) prend une valeur. La locale du champ (`mode`) reçoit le **nom** de la variante choisie, la locale de la variante (`command`) sa valeur ; `-mode ascii` choisit aussi une variante sans valeur. Deux variantes différentes : `options -ascii and -integer are exclusive`. Le défaut nomme une variante.
- **Options dépendantes** : une variante dont le type est un produit. Ses champs sans `-` sont les valeurs qui suivent l'option, ses champs `-x` sont des options qui n'ont de sens qu'avec elle ; tous deviennent des locales.

```tcl
proc parcours {
    {"plat", {{string}cmd; {int}-depth ?= 1; {flag}-follow}command}-mode ?= plat;
    {string}dir
} {...}
parcours -command ls -depth 3 -follow /tmp   ;# mode=command cmd=ls depth=3 follow=1
parcours -depth 3 /tmp                       ;# option "-depth" requires -command
```

- **Groupe** : `{T}-o`, `T` un produit (nommé, réutilisable entre procédures). Les champs de `T` sont les options (`-decreasing`, `-index`, et les variantes d'un champ somme) ; la locale `o` est une valeur de `T` (`$o(index)`).

```tcl
type def triopts {{flag}decreasing; {int}index ?= -1; {"ascii", "dictionary"}mode ?= ascii}
proc trier2 {{triopts}-o; {list}l} { ... $o(decreasing) ... }
trier2 -decreasing -index 3 -dictionary $L
```

- **Messages** : `bad option "-zz": must be -ascii, -command, -decreasing, … or --`, `ambiguous option "-d": must be …`, `option "-index" needs a value`, `option "-index": expected integer but got "x"` ; l'usage les montre (`?-decreasing? ?-index value? ?-ascii|-dictionary|-integer|-command value? ?--? l`).
- **`--`** termine les options et n'est pas transmis (`trier -dec -- -x` : `l` vaut `-x`). Il n'est reconnu que dans la zone des options, en entier (pas d'abréviation) ; après les options, ou en position de valeur (`-index --`), c'est un mot ordinaire ; une procédure sans option ne le traite pas à part. Un nombre négatif (`-5`) termine aussi les options ; une autre valeur qui commence par `-` demande `--`.
- **Avec le reste** : les options se combinent avec les positionnels, les étiquettes, `{list.T}args` et le type de retour. `info args` rend les champs tels qu'écrits (`-decreasing`), `info default` leurs défauts (`0` pour un drapeau).
- **Coût** : `trier -dec -index 1 -integer $L` 2,2 µs, contre 3,3 µs pour la même analyse écrite en Tcl (`tcl::prefix match`, contrôles, exclusivité) ; sans option, 1,5 µs contre 1,1 µs (`exemples/bench_options.tcl`).

### 7. Accéder aux parties d'une valeur : les chemins

#### 7.1 La forme parenthésée

Une valeur typée se lit et s'écrit avec la syntaxe des tableaux, mais l'index est un **chemin** :

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# construction (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# écriture d'un champ entier
set R((A,B).x) 10 30                       ;# écriture multiple
${R(A.x, B.y)}                             ;# lecture, espaces permis
```

La construction par chemin vaut aussi pour une liste, par ses indices (tous, de 0 à N-1, dans un ordre quelconque) : `set {segment}AB(0,1) $A $B`, `set {segment}S(0.x,0.y,1.x,1.y) 1 2 3 4` ; et pour un dict, par ses clés : `set {dict.int}D(a,b) 1 2`. Pour un autre type, `set {int}A(k) 5` est refusé sans rien créer : une variable typée est un scalaire, pas un élément de tableau.

Les espaces sont permis dans `$R(…)`, `${R(…)}` et dans un mot annoté `{T}R(…)`. Dans un mot nu non annoté (`set R(A.x, A.y) …`), Tcl coupe aux espaces comme toujours : écrire sans espaces ou entre guillemets.

#### 7.2 Le langage de chemin

Le langage est celui de la page du wiki Tcl « A better way to do indexation », analysé sur le modèle d'`expr` (`tclTypePath.c`).

**Opérandes :**

| opérande | sens |
|---|---|
| `nom` (mot nu) | champ, clé de dict, nom de variante ou index de liste (`3`, `end`, `end-1`) : le type du conteneur tranche |
| `'clé'` | toujours une clé (`'end'` est la clé « end », pas un index) |
| `"étiquette"` | texte littéral copié dans le résultat |

**Opérateurs**, du plus faible au plus fort :

| opérateur | rôle | exemple |
|---|---|---|
| `,` | collecte (un groupe) | `A,B` |
| `*` (unaire) | dépliage : le groupe se fond dans son parent | `*0:2,4` |
| `.` | imbrication ; distribue sur un groupe | `A.(x,y)` = `A.x,A.y` |
| `:` | intervalle de liste | `0:end-1` |
| `( )` | groupement | `(A,B).x` |

**Forme du résultat.** Une lecture multiple rend des listes imbriquées selon les groupes : `$R((A,B).(x,y))` donne `{1 2} {3 4}` ; `$L(0:2,4)` donne `{10 20 30} 50` ; `$L(*0:2,4)` donne `10 20 30 50`.

**Le chemin ne substitue rien.** C'est Tcl qui substitue `$x`, `[cmd]` et `[(expr)]` dans `$R(…)` et dans les mots nus ; une valeur substituée est **une feuille**, jamais relue comme un chemin :

```tcl
set k file.txt ; $D($k)        ;# la clé « file.txt » (pas D.file.txt)
set n 1        ; $D(f_$n)      ;# collée à du texte, la valeur prolonge le nom
set idx {0 2}  ; $L(*$idx)     ;# * déplie une valeur liste : 10 30
set p {[exec ls]} ; $L($p)     ;# rien n'est exécuté : bad index "[exec ls]"
```

Entre accolades et dans `${…}`, `$` et `[` sont des erreurs : `a path does not substitute "$"…`. Un chemin calculé se lit en **nommant** la variable avec lui : `set ref R($chemin) ; set $ref`.

**Grammaire (EBNF).** Précédence, de la plus faible à la plus forte : `,`, puis `*`, puis `.`, puis `:`, puis `( )`.

```ebnf
path        = spread , { "," , spread } ;      (* collecte : un groupe *)
spread      = [ "*" ] , nest ;                 (* dépliage : le groupe se fond dans son parent *)
nest        = range , [ "." , nest ] ;         (* imbrication ; distribue : A.(x,y) = A.x,A.y *)
range       = step , [ ":" , step ] ;          (* intervalle de liste : 0:end-1 *)
step        = word                             (* champ, clé, variante ou index : le type tranche *)
            | "'" , key , "'"                  (* toujours une clé : 'end' *)
            | '"' , label , '"'                (* étiquette, copiée telle quelle *)
            | "(" , path , ")" ;
index       = integer | "end" , [ "-" , integer ] ;

read        = "$" , varname , "(" , path , ")"
            | "${" , varname , "(" , path , ")" , "}" ;
write       = "set " , varname , "(" , path , ")" , value , { value } ;
construct   = "set {" , type , "}" , varname , "(" , path , ")" , value , { value } ;
```

#### 7.3 Cas par conteneur

| conteneur | lecture | écriture |
|---|---|---|
| produit | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (revalidé) |
| somme | `$v(i)` : la variante courante ; une autre variante, `$v(s)`, passe par la règle `type convert S.i S.s` s'il y en a une, sinon par le cast de la valeur courante (`{i}5` donne `5`) ; variantes de même forme sans règle, ou cast impossible : une erreur qui nomme les deux variantes (§ 8) | `set v(i) 6` ; `set v(s) abc` change la variante ; une partie d'une variante inactive (`set u(p.x) 1`) est refusée |
| liste | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (un bit), `$b(1:4)` (une tranche : un entier `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| énumération | (scalaire : pas de chemin) | — |

Une lecture puis une réécriture du même chemin marche toujours : `set R(c) [set R(c)]`.

### 8. Construire une valeur

#### 8.1 Les formes d'un constructeur de produit

Un produit de *n* champs se construit de quatre façons, sans rien deviner :

| forme | exemple | règle |
|---|---|---|
| positionnelle | `set {pt}a Jean 12` | exactement *n* valeurs, dans l'ordre des champs |
| étiquettes de champ | `set {pt}a {age:}12 {nom:}Jean` | toutes les valeurs étiquetées ; ordre libre ; champs omis à leur défaut |
| un seul champ | `set {pt}a {pt.nom}Jean` | les autres champs à leur défaut |
| chemin + valeurs positionnelles | `set {pt}a(age,nom) 12 Jean` | une valeur par feuille du chemin (ou par groupe, § 8.3) |

S'y ajoutent : **aucun argument** (`set {pt}a`, tous les champs à leur défaut) et **un produit du même type** (copie). Tout le reste est une erreur :

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

La forme nom/valeur nue (`set {pt}a nom Jean age 12`) **n'existe plus** : elle se confondait avec la forme positionnelle dès que *n* arguments valaient pour *n* champs (§ 16).

#### 8.2 Les étiquettes de champ

Chaque argument nomme son champ par une étiquette, `{champ:}valeur` (v16l). Une étiquette ne peut pas être confondue avec un type global du même nom.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# ne change rien
set {paire}p {nom:}jean {age:}12        ;# jean 12
set {paire}p {nom:}zoe                  ;# zoe 0
set {paire}p {nom}jean                  ;# {nom} is not a type: ... write {nom:}
set v {age:}21                          ;# local annotation {age:} cannot be stored by "set"
```

**Tout ou rien :**

```tcl
set {paire}p {nom:}jean 12       ;# can't mix values named by a field label ({field:}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom:}a {taille:}3  ;# unknown field "taille:" in paire
set {paire}p {nom:}a {nom:}b     ;# duplicate field "nom" in paire
set {paire}p {aut.nom}x         ;# aut.nom names a field of aut, not of paire
```

Une valeur entre accolades s'étiquette aussi : `set {seg}s {a:}{1 2} {b:}{3 4}`.

#### 8.3 Chemin et valeurs positionnelles

`set {T}v(chemin) v1 v2 …` construit le produit (et ses produits imbriqués) ; les champs nommés reçoivent les valeurs, les autres leur défaut. Le **nombre** de valeurs choisit la répartition, jamais leur contenu :

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# une valeur par feuille
set {rect}R((A,B).(x,y))     1 2 3 4      ;# idem
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# une valeur par élément du groupe de tête
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# une valeur pour tout le chemin
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# avec dépliage : une valeur, à plat
```

Chaque valeur a la forme que sa lecture rendrait ; les trois nombres ne coïncident que si les lectures coïncident.

**La combinaison chemin + étiquettes n'existe pas** : `set {rect}R(A) {y:}2 {x:}1` est une erreur (`1 targets but 2 values`). Elle n'est pas prévue tant qu'un usage ne la demande pas.

#### 8.4 Sommes, unions et listes

**Deux constructions (v16w).** La forme de la déclaration dit si la valeur porte sa variante :

| | somme `,` | union `\|` |
|---|---|---|
| exemple | `{double}celsius, {double}fahrenheit` | `{int}n \| {list(2).int}paire` |
| valeur | `variante charge` : `fahrenheit 50.0` ; une variante littérale : son texte | la charge seule : `5`, `3 4` |
| la variante | écrite dans la valeur | calculée à partir de la valeur |
| à la définition | toute variante typée a un nom (sa balise) | les alternatives doivent être disjointes |
| `expr` direct | non : `$t(celsius)` | oui |

Une somme ne devine jamais rien : sa valeur dit sa variante, par la chaîne, dans un fichier, d'un interpréteur à l'autre. Deux variantes de même forme (des unités, `{int}a, {int}b`) ne posent donc aucun problème. Une union ne retient rien : ses alternatives ne peuvent pas se recouvrir, la valeur montre toujours laquelle elle est. `type is {disjoint?}S` donne les paires qui peuvent accepter une même valeur ; une union qui se recouvre est refusée à la définition :

```tcl
type def U2 {{int}i | {string}s}
  ;# union: alternatives i and s can take the same value, so the value could not tell
  ;# which one it is; a union must be disjoint. Write a sum with "," to tag the alternatives
type def P {{pt}p | {vec}v}            ;# refusée aussi : deux produits de même forme
type def P {{pt}p, {vec}v}             ;# la somme : "p {1 2}" ou "v {1 2}"
```

Une énumération (`{"on", "off"}`, ou `{"on" | "off"}`) a la même valeur dans les deux cas : une variante littérale est son propre nom. Les options `{-strict}` et `{-deduce}` des v16n–v16v n'existent plus.

**Construire une somme.** Toutes ces écritures donnent `s 5` :

```tcl
set {S}v {s:}5                 ;# l'étiquette : un raccourci pour la variante
set {S}v s 5                   ;# en clair : la variante, puis sa charge
set {S.s}v 5                   ;# par le type de la variante
set {S}v(s) 5                  ;# par le chemin
set {S}v [read $f]             ;# par la chaîne "s 5"
set {S}v 5                     ;# refusé : value "5" names no variant of the sum S: a value of
                               ;# a sum is the name of its variant, then its value, as {i 5}, or {i:}5
set {list.S}L {i:}1 {s:}a      ;# chaque élément : "i 1", "s a"
lappend L {s:}x                ;# lappend et lset reçoivent l'étiquette de l'élément
```

Une valeur déjà typée du type d'une variante entre dans la somme (`set {int}n 5; set {W}w $n` donne `i 5` si `W` n'a qu'une variante `int`, sinon il faut l'étiquette).

**Une variante qui est un produit (v16o)** se construit par ses champs, comme un produit nommé :

```tcl
type def segment {{string}texte, {{string}cible; {list.segment}contenu}lien}
set {segment.lien}s {cible:}$url {contenu:}$segs     ;# étiquettes, ordre libre, défauts
set {segment.lien}s $url $segs                       ;# positionnel
set {segment}s(lien.(cible,contenu)) $url $segs      ;# chemin à travers la somme
set {segment}s($v.($champs)) {*}$valeurs             ;# le même, calculé
type def segment.lien                                ;# {string}cible; {list.segment}contenu
```

La valeur est `lien {URL SEGMENTS}`. Le premier pas d'un chemin choisit la variante (une seule pour tous les chemins), la suite construit la valeur de son type ; un chemin traverse aussi une somme champ d'un produit (`set {P}r(n.i,m) 4 5`). Les messages nomment le produit anonyme d'une variante `S.v`.

**Lire.** `$v(s)` rend la charge, typée ; `$f(w)` lit directement un champ de la charge quand le nom n'est pas une variante (`forme.rect` : `$f(w)` comme `$f(rect.w)`). `type variant $v` rend le nom de la variante (son texte pour un littéral). Une valeur de somme est une liste de deux éléments : `lassign $v nom charge` marche aussi sans les types.

```tcl
expr {$v(i) * 2}               ;# la charge, comme nombre
switch [type variant $v] { i {…} s {…} }
```

**Modifier.** La valeur porte sa variante, une écriture la dit donc toujours : `set v 8` est refusé, `set v(i) 8` écrit `i 8`, `set v(s) x` change de variante (`s x`), `incr v(i)` incrémente la charge.

**Casts.** Toute valeur se lit comme une `string` : son texte (`on`, `i 5`). Une union se lit comme le type de son alternative (`{U}5` passe à un argument `{int}`). Une somme jamais : `a value of the sum S is not of type int: it is "i" then its value; read that value by its path, as $v(i)`. Ce qu'elle dit (l'unité, le genre) ne se perd pas en silence.

**Conversions entre variantes.** Deux variantes d'une même somme peuvent recevoir une règle, sens par sens :

```tcl
type def temp {{double}celsius, {double}fahrenheit}
type convert temp.celsius temp.fahrenheit {{x} {expr {$x*9/5.0 + 32}}}
set {temp}t {celsius:}100      ;# t vaut "celsius 100"
set t(fahrenheit)              ;# 212.0 : lecture d'une variante inactive, par la règle
set u {temp.fahrenheit}$t      ;# cast explicite : u vaut "fahrenheit 212.0"
type cast temp.fahrenheit $t   ;# idem
```

La lecture d'une variante inactive et le cast `{S.v}` (ou `type cast S.v`) prennent, dans l'ordre : la règle ; refus si les deux variantes ont la même forme (un cast identité dirait qu'un celsius est un fahrenheit : `… both have the same form, so reading one as the other needs a rule: type convert temp.celsius temp.fahrenheit {{x} ...}`) ; sinon le cast ordinaire entre les types des deux variantes (`{int}i, {string}s` : `{i}5` lu par `$v(s)` donne `5`). Une règle n'intervient jamais dans l'écriture (`set t(fahrenheit) 50` change de variante sans convertir). Pas d'enchaînement (une règle a → b et une règle b → c ne font pas a → c), pas de réciproque supposée.

Une liste reçoit ses éléments un par un : `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Un seul argument est **un** élément.

### 9. Types binaires

Un type dit, par lui-même, comment il s'écrit en octets : aucune syntaxe de plus n'est à apprendre. La lecture et l'écriture sont faites par `layout/filebinary.tcl` (chargé par `source`), au-dessus du noyau, avec une voie rapide en C pour les dispositions fixes (§ 9.6).

#### 9.1 `bitarray` : un entier dont on lit les bits

`bitarray(n, ordre)` est un entier de n bits (0 ≤ v < 2^n). L'ordre dit d'où part l'indice 0 : `lsb0` (défaut, le bit de droite, comme `1 << i` : registres, masques) ou `msb0` (le bit de gauche, comme les schémas des RFC). Ce sont deux types distincts.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (un entier bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# dans expr, une tranche entre guillemets
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Représentations

| type | représentation |
|---|---|
| `int(u,n)` / `int(s,n)` | entier non signé / signé de n bits (multiple de 8, de 8 à 1024) |
| `double(32)`, `double(64)` | flottant IEEE |
| `string(raw,N)` | N octets bruts |
| `string(ENC,N)` | texte sur N octets, complété par des NUL |
| `string(ENC,z)` | texte terminé par un NUL (2 octets en utf-16, ucs-2) |
| `bitarray(n)` | n bits (multiple de 8), lus comme un entier non signé |
| `"MZ"` (somme de littéraux de même longueur) | une constante, vérifiée |
| `list(N).T`, un produit de ces types | N éléments ; l'imbrication est permise |

`ENC` : `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un texte qui ne s'encode pas sans perte (`é` en ascii) ou qui ne tient pas dans N octets est refusé à la construction.

**Boutisme.** Dernier argument : `be` ou `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Sans lui, c'est celui de l'appel (`-endian`, little par défaut). Il ne change pas les valeurs : `int(u,16,be)` et `int(u,16,le)` s'affectent l'un à l'autre.

**Alignement.** Par défaut les produits sont compacts (packed). `-align natural` (ou `-align N`) ajoute le remplissage du C : chaque champ commence à un multiple de son alignement, la taille d'un produit est arrondie. Remplissage à zéro à l'écriture, ignoré à la lecture sauf `-strictpad 1`.

#### 9.3 Commandes

```
type binary TYPE                                   ;# la représentation, vue du noyau
type binary {what?}TYPE                            ;# idem
type binary {sizeof?}TYPE ?-align A?               ;# taille en octets (erreur si elle dépend des données)
type binary {is?}TYPE OCTETS ?-exact 1?            ;# 1 si OCTETS se décode en TYPE, sinon 0
type binary decode TYPE OCTETS ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TYPE VALEUR ?-endian E? ?-align A? ?-dedupe 1?
type binary deref LIEN ?OCTETS?                    ;# suit un lien non suivi, d'un pas
type binary space file|memory|list|get|delete …    ;# référentiels nommés
chan binary read  CANAL TYPE ?options?             ;# le canal avance
chan binary write CANAL TYPE VALEUR ?options?
```

Les questions (`{what?}`, `{sizeof?}`, `{is?}`) sont des **annotations locales** sur l'argument de type ; les actions sont des verbes. `file binary` reste un alias de l'ancien nom.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Niveau 2 : ce que le fichier dit de lui-même

Une taille, un nombre, une position ou une variante peuvent être lus dans le fichier : `$champ` dans les arguments du type (un champ **déjà lu** du produit courant ou d'un produit englobant), ou un type entier non signé qui sert de **préfixe**.

| écriture | sens |
|---|---|
| `string(utf-8,int(u,16,be))` | texte précédé de sa taille en octets (chaîne de Pascal) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N octets, N calculé sur des champs lus |
| `list(int(u,8)).T`, `list($n).T` | liste précédée de son nombre d'éléments, ou de n éléments |
| `at($off).T` | la valeur est à la position `$off` (le curseur ne bouge pas) |
| `corps($tag)` | la somme `corps`, dont la variante est choisie par le champ `tag` |
| `U(tp)`, `R($v)` (paramètre lié) | une somme dont la variante est fixée à la définition |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# noms de variantes = valeurs de l'étiquette
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

La variante d'un champ `corps($type)` est toujours celle que nomme le sélecteur, au décodage comme à la lecture (`$m(body)`), à la construction et à l'écriture par chemin, même si plusieurs variantes ont la même forme. Une valeur qui porte une autre variante (`{2}...`, ou une valeur typée d'une autre variante) est refusée : `selector type is 1 but field body of message holds variant 2 of corps($type)`. Changer le sélecteur revalide le champ.

À la construction déjà, une liste dont la taille dépend d'un champ est contrôlée : `set {Batch}b(n,items) 3 {{1 1.0 0}}` est refusé (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Liens : `*(origine, repr, options).T`

Un champ contient souvent une adresse, pas la valeur. Le type lien lit l'adresse (`repr`, un entier), puis la valeur `T` qui s'y trouve.

| origine | l'adresse part de |
|---|---|
| `//` (ou `file://`) | début des données |
| `./` | début de la structure englobante |
| `../` | début de sa parente |
| `.` | le champ lui-même |
| `//nom/`, `memory://nom/` | un référentiel nommé (fichier, ou segments mémoire) |

Options : `null` (l'adresse 0 signifie « pas de cible »), `null=V`. Avec `opt(T)`, une adresse nulle donne `none`, sinon `some X` (la cible X) : c'est la différence entre « nul » et « inexistant ». À l'écriture, les cibles sont ajoutées à la fin, alignées, et les adresses calculées ; `-dedupe 1` n'écrit qu'une fois les cibles identiques. Un champ lien qui vaut son défaut s'écrit comme un lien nul.

**`-linkdepth`** règle le nombre de liens suivis (`Inf` par défaut). Un lien non suivi est un entier annoté (la position absolue de la cible) ; `type binary deref` le suit d'un pas. Cela permet de lire seulement l'en-tête d'un gros fichier, de parcourir une liste chaînée, et de réécrire un champ sur place.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8 : la cible
```

#### 9.6 Codecs C

`type binary encode` et `decode` essaient d'abord une voie en C : entiers de 1, 2, 4, 8 octets, flottants de 4 et 8 octets, produits (avec `-align`), `list(N).T`, chaînes fixes et à zéro final, sommes de littéraux et balisées, tailles et nombres dynamiques (préfixe `string(utf-8,int(u,16,be))`, `list(int(u,8)).T`, ou expression entière sur les champs déjà lus : `$len`, `$len - 4`, `($a + $b) * 2`), `at(EXPR).T`, liens `*(ORIGINE,REPR).T` (origines `//`, `.`, `./`, `../`, cible `opt(T)` comprise). Hors de ce domaine (champs de bits, référentiels nommés, `-linkdepth` fini, `-dedupe`, lien nul d'un champ à défaut, `$h(off)` dans une expression), ou en cas d'erreur, la couche Tcl prend la main et donne ses messages. `set ::tcl_typebinaryc 0` désactive la voie C. Les octets et les valeurs produits sont identiques dans les deux voies (test `t51_codec_c`). Mesures : § 15.

### 10. Erreurs

Les messages suivent l'usage de Tcl : en anglais, et ils nomment la valeur, le type ou le champ en cause. Les codes `errorCode` propres aux types commencent par `TCL TYPE` ou `TCL PARSE TYPE`.

| situation | message (exemple) | `errorCode` |
|---|---|---|
| caractère interdit dans un type | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| nom de type inconnu sur un nom de variable | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| annotation locale sur un nom de variable | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| annotation locale rangée par `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| valeur invalide pour le type | `expected integer but got "abc"` ; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (selon le type de base) |
| aucune variante ne convient | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| types nominaux différents | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| aucune conversion connue | `unknown way to cast the value from mm to kg` | — |
| annotations empilées incohérentes | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| champ inconnu | `unknown field "zz" in pt` ; `pt has no field "zz"` (chemin) | `TCL TYPE FIELD` ; `TCL TYPE PATH` |
| champ sans valeur ni défaut | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| mauvais nombre de valeurs | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| étiquettes et positions mêlées | `can't mix values named by a field label ({field:}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| champ en double | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| taille dépendante non respectée | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| clé de dict manquante ou interdite | `key "nom" is missing in fiche` ; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| arité de liste | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| substitution dans un chemin littéral | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| index de liste invalide dans un chemin | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| lien vers une variable disparue | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` sur un lien cassé | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| valeur annotée créée hors du parseur | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| couche binaire absente | `the binary layer is not loaded` | — |

## Partie III — Implémentation

### 11. Vue d'ensemble

L'implémentation tient en **sept fichiers nouveaux** (environ 15 000 lignes de C), copiés dans `generic/`, et en **points d'accroche** dans une vingtaine de fichiers du noyau. Les fichiers nouveaux ne sont jamais patchés : ils sont livrés entiers ; seuls les fichiers du noyau reçoivent des patchs.

### 12. L'objet annoté

#### Structure

Une valeur annotée est un `Tcl_Obj` ordinaire dont le type interne est `annotated` (`annotatedObjType`, dans `tclAnnotObj.c`) :

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   la chaîne : celle du contenu V (calculée      │
 │                  à la demande par UpdateStringOfAnnotated)    │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   le contenu : un Tcl_Obj ordinaire, partagé      │
 │   ptr2 ──► T   l'annotation, sous l'une de ces formes :        │
 │                · une référence de type résolue (typeRef)       │
 │                · le texte brut de l'annotation (pas encore      │
 │                  résolue : une note, ou un type à résoudre)     │
 │                · ""   le marqueur de l'annotation vide {}       │
 │                · un état « annotstate » {T, N} : T résolu       │
 │                  (ou NULL) et N, la liste des notes, de la      │
 │                  plus ancienne à la plus récente                │
 └───────────────────────────────────────────────────────────────┘
```

#### Propriétés

- **Transparente** : la chaîne est celle de V. Pour toute commande qui ne connaît pas les annotations, la valeur est V.
- **Immuable et partagée** : l'état `{T, N}` est lui-même un objet immuable ; `Tcl_DuplicateObj` le partage. Annoter, dépiler ou retirer une note produit un nouvel objet.
- **Sans coût sans note** : tant qu'il n'y a pas de note, `ptr2` est T seul.
- **Liste abstraite** (Tcl 9, `TCL_OBJTYPE_V2`) : `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… passent par les procédures de liste de l'objet (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). Les éléments d'une `list.T` sont rendus typés ; un scalaire annoté est une liste d'un élément qui est lui-même.

#### Routines du `Tcl_ObjType`

| routine | rôle |
|---|---|
| `FreeAnnotated` | libère V et T (et la cellule « soi-même » d'un scalaire, table `SelfCell`) |
| `DupAnnotated` | copie V, partage T ou l'état `{T, N}` |
| `UpdateStringOfAnnotated` | la chaîne de V |
| `SetAnnotatedFromAny` | **refuse** : une valeur annotée naît du parseur ou d'une affectation typée, jamais d'une conversion (`TCL VALUE ANNOTATED`) |

#### Fonctions principales (`tclAnnotObj.c`)

`TclAnnotateValue` (poser une annotation : cast ou note), `TclAnnotSettle` (le dépilage d'un constructeur, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (type, variante, inconnu, local, invalide ; le contexte avant les types globaux), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (le contenu nu, pour les conversions du noyau), `TclAnnotDisplay` (`list<int>@mm.hello`), et les commandes `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. Dans le parseur et le compilateur

#### Le préfixe `{x}mot`

Dans Tcl 9 d'origine, un mot entre accolades suivi d'autres caractères est une erreur de syntaxe (`extra characters after close-brace`), sauf `{*}`. Le préfixe d'annotation occupe donc une écriture **qui n'avait aucun sens** : aucun script valide ne change de sens.

**Pas d'annotation devant un caractère fermant ou un opérateur (v16y).** Une valeur ne commence jamais par `] } ) = < > ! & | ^ ? , ; ~ %` : après une accolade fermante, ces caractères gardent l'erreur de Tcl 9, `extra characters after close-brace`. C'est ce qui garde le diagnostic de l'accolade mal placée, l'erreur de frappe la plus courante en Tcl : `proc p {} {set w foo}]` reste une erreur, au lieu de définir sans bruit une procédure dont le corps serait `]`. Les débuts de valeur utiles restent permis : `{flag}-x`, `{int}::n`, `{path}/tmp`, `{T}$x`, `{T}[cmd]`, `{T}"texte"`.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand` : un mot `{texte}suite` produit un jeton **`TCL_TOKEN_ANNOT`** (2048, `tcl.h`) qui porte le texte de l'annotation, suivi des jetons du reste du mot. Un mot annoté `{T}nom(...)` va jusqu'à la parenthèse équilibrée, espaces compris, sur une ligne.
- `tclCompile.c`, `TclCompileTokens` : si le reste du mot est littéral (`TclAnnotSuffixIsLiteral`), le mot compile en `::tcl::annotraw texte mot` (une note brute, sans substitution) ; sinon en `::tcl::annotate texte valeur`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd` : un `set` dont le nom est annoté passe par `TclSetAnnotatedCmd` ; une annotation locale écrite sur la valeur (`set x {-v}$mot`) émet un appel à `::tcl::annotlocal`, qui lève l'erreur (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd` : un premier argument annoté délègue à `TclSetAnnotatedCmd` (`tclTypedVar.c`), le constructeur.

#### L'index `$nom(...)` : `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName` : sur la `(` qui suit un nom de variable, appelle **`TclParseVarIndex`**, qui lit l'index jusqu'à la `)` équilibrée : parenthèses imbriquées, étiquettes `"..."` appariées, substitutions `$x`, `[cmd]`, `[(expr)]`, antislashs ; accolades refusées. Si l'index n'est pas bien formé selon cette grammaire, **repli** exact sur la règle de Tcl 9 (`info complete {a "c $dd("}` reste vrai).
- Seulement quand la syntaxe étendue sert, les composants sont enveloppés dans un jeton **`TCL_TOKEN_INDEX`** (1024, `tcl.h`) : un index valide en Tcl 9 garde exactement ses jetons. Les mots nus ne sont pas concernés : `puts a(b c)` reste deux mots.
- `${nom(...)}` : le nom entier est littéral ; `Tcl_ObjGetVar2` et `INST_LOAD_STK` (via `TclTypedVarGetElementByName`) lisent le chemin quand la variable est un scalaire typé.
- `tclTest.c` : `testparser` affiche le jeton « index ».

#### Bytecode

| instruction | où | rôle |
|---|---|---|
| `INST_INDEX_CONCAT` (« indexConcat », 6 octets : nombre `uint1` + masque `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, appelé par `TclCompileVarSubst` et `TclPushVarName`), `tclExecute.c` | concatène les parties d'un index en un objet **pathindex** qui retient les portées substituées (`TclPathIndexNew`) : une valeur substituée reste une feuille du chemin |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (`dict for`, `dict map` compilés), `tclExecute.c` | oublie le type d'une variable de boucle avant l'affectation |
| `INST_ANNOT_MARK`, `INST_ANNOT_EXPIRE` (« annotMark », « annotExpire », v16m) | `tclCompile.c` (`CompileCommandTokens`), `tclExecute.c` | encadrent une commande qui a un mot annoté : la marque de la liste des annotations posées, puis l'expiration de ce qui a été posé depuis (§ 2.2) |
| `INST_ANNOT_CAST` (« annotCast », v16m) | `tclCompCmds.c` (`foreach`, `lmap`, `dict for`, `dict map` compilés), `tclExecute.c` | une liste ou un dict reçu par la boucle sous la forme `{T}mot` : la valeur du mot est castée vers `T` une fois, sans valeur annotée intermédiaire (`TclAnnotCastText`) |
| `INST_ANNOT_SETTLE` (« annotSettle », v16m) | `tclExecute.c` | résout l'annotation en attente d'une valeur (`TclAnnotSettleValueArg`) ; défini, non émis pour l'instant |

**Expiration (v16m, piste B).** Chaque valeur annotée par un mot (`::tcl::annotate`, `::tcl::annotraw`, `TclSubstTokens`), et chaque valeur neuve d'un constructeur qui garde une annotation en attente, est inscrite dans une liste par interpréteur (`TclAnnotRegisterFresh`). L'expiration change **en place** l'état de l'objet (le `ptr2`, partagé par ses copies) : le texte brut et les notes en attente deviennent des notes expirées, sous une marque (un élément vide de la liste des notes), que `TclAnnotSettle` ne lit plus. Elle a lieu à la fin de la commande (`annotExpire` ; `TclEvalEx` pour un script non compilé), et à la sortie d'un bytecode (`TEBCresume` : `return {T}$x`, une commande sortie en erreur). Un inscrit que plus rien ne tient, ni lui ni son état, part sans expirer.

Chemin non compilé : `TclSubstTokens` (`SubstIndexTokens`) produit le même objet pathindex.

#### Autres points d'accroche du noyau

| fichier | changement |
|---|---|
| `tclBasic.c` | `TclTypesInit` : crée `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` passe par le chemin lent pour une valeur annotée ; `INST_FOREACH_STEP` oublie le type ; lecture et écriture d'éléments typés (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` et `TclTypedVarAfterStore` autour de chaque écriture : le contrat |
| `tclProc.c` | `TclTypedVarBindArg` : liaison typée des arguments ; signature typée de `proc` et d'`apply` (`TclTypedProcSig`, `TclTypedLambdaRet`) |
| `tclCmdAH.c`, `tclDictObj.c` | variables de boucle (`TclTypedVarForgetByName`) ; `TclAnnotUnwrap` dans les lectures de dict |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | conversions du noyau sur le contenu (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`) : une valeur annotée n'est jamais convertie en place |
| `tclListObj.c` | `lappend` par la `replaceProc` d'une liste abstraite ; `lindex` à plusieurs indices sans aplatir |
| `tclCompExpr.c` | opérateur `is` (lexème `IS_TYPE`, `IsTypeAhead`), affectations `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. Les fichiers ajoutés

| fichier | lignes | contenu |
|---|---|---|
| `tclTypes.c` | 12 100 | typologie, `type def/alias/get/cast/convert/deref/binary`, validation, élargissement, casts, codecs C, signatures de procédures et de lambdas |
| `tclAnnotObj.c` | 6 200 | l'objet annoté, l'état `{T, N}`, la résolution, `note`, `type debug` (trace, inventaire des proto-types) |
| `tclTypePath.c` | 4 300 | langage de chemin, lecture, écriture, construction par chemin, objet pathindex |
| `tclTypeParse.c` | 2 800 | analyseur et compilateur des expressions de types (modèle de `tclCompExpr.c`) |
| `tclTypedVar.c` | 2 200 | variables typées, constructeurs (`TclSetAnnotatedCmd`, `BuildProductEx`, étiquettes de champ) |
| `tclTypeGeneric.c` | 1 400 | types paramétrés, instances, paramètres fantômes, raffinements binaires |
| `tclVarRef.c` | 370 | liens `var` (nom canonique, accès, liens périmés) |
| `tclTypes.h`, `tclTypesInt.h`, `tclAnnotatedApi.h`, `tclCompact.h` | 1 050 | structures (`TclType`, `TclTypeField`, `TclBinLink`…), interfaces, API C publique, forme compacte |
| `tclTypeDebug.tcl` | 410 | `type debug` côté Tcl : l'invite, les conditions, la console Tk, le rapport ; inclus dans `tclTypeDebug.h` par `gen_typedebug.tcl` |

Côté Tcl : `layout/filebinary.tcl` (couche binaire : `type binary decode/encode/deref/space`, `chan binary`) et `layout/binaliases.tcl` (alias).

**Exemples** (`exemples/`), outre les petits scripts :

| dossier | ce qu'il montre |
|---|---|
| `signatures` | les signatures de procédures face aux TIP 288, 457, 460, 479, 480 ; options, liens, variadiques, lambdas typées |
| `bulletin` | une procédure boguée, mise au point, typée, puis passée en C, en quatre étapes |
| `mise_au_point` | `type debug` pas à pas, les proto-types, la console Tk |
| `cproc` | des procédures au corps C, compilées au premier appel (gcc ou tcc) |
| `ffi` | appeler une fonction d'une bibliothèque partagée depuis sa signature typée, par un talon C, sans libffi |
| `tcllib_geometry` | `math::geometry` de tcllib en Tcl, en Tcl typé et en C ; ses 210 tests passent dans les trois |
| `cstruct`, `geometrie`, `matrices` | codecs C, forme compacte, mesures |
| `zip`, `journal`, `texte_enrichi`, `minidll`, `arduino` | des formats binaires réels lus et écrits par leurs types |

#### 14.1 La forme compacte (prototype)

**Par type** : `type def {-compact}polygone list.point`. Les valeurs de ce type sont **toujours** en cases : les nombres sont normalisés à la construction (le constructeur est une conversion : `{0 0}` devient `{0.0 0.0}`, `0x10` devient `16`, un `double(32)` est arrondi au `float`), et une conversion du noyau (`string length`, `string range`, `dict`…) travaille sur une copie en chaîne gardée à côté des cases. Le type doit être une liste dont l'élément a une disposition : sinon `type def` refuse, et le nom reste libre. Pas encore pour une famille (`type def {-compact}{v({int}n)} …`) ; pas d'option pour un alias. `type is {compact?}polygone` dit si le type le demande, `type is {compact?}$P` si la valeur est en cases.

```tcl
type def {-compact}polygone list.point
set {polygone}P {0 0} {4 0} {4 3}     ;# {0.0 0.0} {4.0 0.0} {4.0 3.0}
type is {compact?}$P                   ;# 1, même après string length $P
```

**Sur un produit**, `{-compact}` veut dire « toute liste de ce produit est en cases », comme un tableau de `struct` en C, que la liste soit une variable, l'élément d'une autre valeur ou le champ d'un produit. Le produit doit avoir une disposition (que des scalaires à taille fixe, ou des produits de tels champs).

```tcl
type def {-compact}pointC {{f64le}x; {f64le}y}
type def polyC {{u32le}n; {list($n).pointC}pts}
set {polyC}p 2 {{1 2} {3 4}}           ;# 2 {{1.0 2.0} {3.0 4.0}}
type is {compact?}pointC               ;# 1
type is {compact?}$p(pts)              ;# 1 : le champ pts est en cases
```

Une liste qui est le champ d'un produit (ou d'un produit imbriqué) est mise en cases dans les mêmes cas qu'une liste seule : son type ou son élément le demande, ou la forme compacte est activée partout. Elle le reste après une écriture par chemin, un `lappend p(pts)` ou un `type binary decode`.

**Listes de taille fixe : matrices et tableaux à N dimensions** (v16h). Un élément `list(n).T`, où T a lui-même une disposition en cases (scalaire, produit, ou à son tour une liste de taille fixe), est rangé dans la case comme le tableau C `T[n]` : `list(3).list(3).double` est une liste de 3 cases de 24 octets, `list(2).list(3).list(2).int(s,32)` un tableau `int32_t[2][3][2]`, et un produit peut avoir un champ `{list(2).double}v` dans ses cases. Une case a au plus 4 096 parties (une matrice `list(100).list(100).double` passe : 100 cases de 800 octets).

```tcl
type def {-compact}matrix3 list(3).list(3).double
set {matrix3}M {1 2 3} {4 5 6} {7 8 9}   ;# normalisés : {1.0 2.0 3.0} ...
type is {compact?}$M                      ;# 1
expr {$M(0.0) * $M(1.1) + $M(2.2)}        ;# 14.0, lu dans les cases
set M(1.2) 60.5                           ;# écrit dans la case
lindex $M 1 2                             ;# 60.5 : chaque ligne est une liste
tcl::unsupported::compact layout list(2).list(3).int(s,16)
    ;# size 12 align 2 fields {{0.0 int 2 0} {0.1 int 2 2} ... {1.2 int 2 10}}
```

Une famille passe par une instance nommée : `type def {-compact}mat3 mat(3,3)`. En C, une matrice en cases se lit par `Tcl_AnnotatedGetCells` avec le type de ses lignes (nommer la ligne : `type def ligne3 list(3).double`, `type def {-compact}matrix3 list(3).ligne3`), et se fabrique par `Tcl_AnnotatedCreateCells` ; un lot `list.matrix3` est un tableau `matrix3[n]` : voir `exemples/matrices/`. Comme une ligne de produit, une ligne d'une matrice est une liste ordinaire pour le script (`lindex`, `foreach`, `lset`, arité vérifiée) ; seule la rangée change. `$M(i.j)` se lit directement dans la case (≈ 0,2 µs, contre 0,9 µs par le chemin général), et `set M(i.j) v` d'un nombre écrit la case sur place quand rien d'autre ne voit la valeur (ni copie, ni vue vivante : sinon le tampon est copié comme avant) : 0,14 µs contre 2,2 µs. Produit de deux matrices 3×3 (`exemples`, boucles Tcl et `$A($i.$k)`) : 33 µs, contre 75 µs en forme ordinaire et 9 µs pour des listes brutes avec `lindex`/`lset`.

**Cases et octets.** Quand la disposition des cases est celle des octets demandés (mêmes scalaires aux mêmes décalages sous `-align`, boutisme de la machine), `type binary encode` copie les cases et `type binary decode` remplit directement des cases, sans objet par élément : pour 10 000 points, 15 µs et 10 µs au lieu de 0,5 et 3 ms. Les octets sont les mêmes que sans cases ; sinon, le codec suit son chemin ordinaire. Un cast vers une liste en cases (`type cast list.pointC $P`) copie d'un bloc les cases d'une source de même disposition, ou écrit directement les éléments d'une liste ordinaire dans les cases. Depuis C, `Tcl_AnnotatedCreateCells` fait d'un tableau de `struct` une valeur en cases, sans objet par élément (`tcl::unsupported::compact cells TYPE OCTETS ?STRIDE?` en Tcl).

**Pour tout le programme**, derrière `tcl::unsupported::compact on` (ou la variable d'environnement `TCL_TYPES_COMPACT=1`, pour passer les tests), le contenu V d'une liste `list.T` est rangé en **cases** quand T est un scalaire à taille fixe (`int(s,n)`, `int(u,n)` avec n = 8, 16, 32 ou 64 ; `double`, `double(32|64)`) ou un produit de tels champs, produits imbriqués et listes de taille fixe `list(n).T` compris. Une case par élément, disposée comme le struct C équivalent (alignement naturel, mesuré à la compilation : `tcl::unsupported::abi`). `int` sans taille n'entre pas (c'est un grand entier).

```tcl
tcl::unsupported::compact on
type def point {{double}x; {double}y}
set {list.point}P {0.5 1.5} {2.5 3.5}
tcl::unsupported::compact info $P     ;# n 2 stride 16 elem point shared 1
tcl::unsupported::compact layout point ;# size 16 align 8 fields {{x float 8 0} {y float 8 8}}
```

- **Rien ne change pour le script** : V reste un `Tcl_Obj`, de type `compactlist`, liste abstraite (longueur, index, remplacement, éléments). Tous les tests et exemples donnent la même sortie forme compacte ou non (`t70_compact` compare chaque cas dans les deux formes).
- **EIAS** (interrupteur global seulement ; un type `{-compact}` normalise) : la forme compacte n'est prise que si chaque nombre est écrit sous sa forme canonique (celle que Tcl regénère : `0.5`, pas `1` pour un `double`, ni `0x10`, ni `1e3`) et chaque élément aussi (espaces) ; sinon, liste ordinaire. La chaîne d'origine de la liste entière est gardée telle quelle jusqu'à la première modification. Écrire une valeur non canonique (`set P(1.y) 9`) rend la liste ordinaire.
- **Lecture** : `lindex`, `foreach` rendent une **vue** sur la case (type `compactelem`), sans recopier l'élément ; `$P(i.x)` lit le nombre directement dans la case. Un nombre lu reste une allocation.
- **Écriture** : `set P(1.y) …`, `lset`, `lappend` écrivent dans les cases (copie du tampon s'il est partagé : une vue ou une copie garde la valeur qu'elle a lue). Une valeur invalide est refusée avant d'être écrite.
- **Ce qui demande tous les éléments à la fois** (`lsort`, `lrange`, `{*}`) fabrique une fois la liste ordinaire, gardée à côté des cases tant que la valeur ne change pas. Les conversions du noyau (`string length $P`, `string range`, `dict`) travaillent sur une copie en chaîne, gardée de même : les cases restent. `tcl::unsupported::content $P` rend une copie qui partage les cases.
- **Pour le C** : `typeparse/tclCompact.h` donne `TclCompactViewOf(obj)` (données, n, pas, nom du type des éléments), par les types d'objets enregistrés (`Tcl_GetObjType`). Le pointeur est valable tant que la valeur n'est ni modifiée ni convertie. Une fonction de stub remplacera ce passage.

Mesures (Linux x86-64) :

| | forme ordinaire | forme compacte |
|---|---|---|
| mémoire, 200 000 points (`mallinfo`, build PURIFY) | 264 o/point | 16 o/point |
| aire de 10 000 points en C, voie objet (`geo.c`) | 564 µs | 29 µs |
| `lappend` de 20 000 points à une `list.point` | 8,7 s | 0,25 s |
| aire de 10 000 points en Tcl annoté (`lindex` + `$a(x)`) | 15 ms | 23 ms |

La lecture élément par élément depuis Tcl est plus lente (une vue et un nombre fabriqués à chaque accès, au lieu de l'élément gardé) ; le gain est en mémoire, à la frontière C et dans les modifications.

#### 14.2 `type {c!}export` : l'en-tête C des types

`type {c!}export T ?T ...?` rend le texte d'un en-tête C (la cible est une annotation locale sur le verbe ; `{llvm!}` est réservé) : un `typedef` par type nommé, dans l'ordre des dépendances, et des `_Static_assert` sur `sizeof` et `offsetof` calculés par Tcl avec les mêmes alignements que la forme compacte. Si le compilateur de l'extension range autrement (Linux i386 : `double` aligné sur 4), la compilation échoue au lieu de lire des octets faux.

| type Tcl | C |
|---|---|
| `int(s,8/16/32/64)`, `int(u,…)` | `int8_t` … `uint64_t` |
| `double`, `double(32)` | `double`, `float` |
| produit | `typedef struct {…} nom;` (produit anonyme : `struct {…}` en place) |
| `list(N).T` | `T champ[N]` ; `list(N).list(M).T` : `T champ[N][M]` |
| `list($n).T` en dernier champ | membre flexible `T champ[]` |
| somme de littéraux | `enum { nom_a, nom_b }` |
| somme | `struct { int tag; union {…} u; }` (le commentaire donne l'ordre des balises) |
| `S($champ)` | l'`union` seule (le champ frère choisit) |
| `string(enc,N)`, `string(raw,N)` | `char champ[N]`, `uint8_t champ[N]` |
| `string`, `list.T`, `dict`, lien | `Tcl_Obj *` (avec `#include <tcl.h>`) |
| défauts numériques de tous les champs | `#define NOM_INIT { 1, 2.5 }` |

Refus motivés : `int` sans taille, tableau de taille variable qui n'est pas le dernier champ, ordre des octets étranger à la machine, type récursif (pas encore de pointeur), `bitarray`. Les noms sont rendus valides en C (`carré` → `carre`, mot réservé → `int_`).

```tcl
type def point {{double}x; {double}y}
puts [type {c!}export point]
;# typedef struct {
;#     double x;
;#     double y;
;# } point;
;# _Static_assert(sizeof(point) == 16, "point: size");
;# _Static_assert(offsetof(point, x) == 0, "point.x: offset");
;# _Static_assert(offsetof(point, y) == 8, "point.y: offset");
```

`tcl::unsupported::abi` donne ce que ces calculs utilisent : `byteOrder`, `pointer`, et pour `i8 … i64 f32 f64 ptr` la taille et l'alignement dans une structure (ce que `tcl_platform` ne dit pas).

## Compatibilité, conception, limites

### 15. Compatibilité et performances

**Aucun script valide ne change de sens.** Les deux syntaxes ajoutées occupent des écritures qui étaient des erreurs : `{x}mot` (« extra characters after close-brace ») et un index `$nom(...)` étendu, qui retombe exactement sur la règle de Tcl 9 dès qu'il n'est pas bien formé. Vérifications :

- suite complète de Tcl (`make test`, 51 095 tests, v16z) : les seuls échecs sont ceux d'un Tcl 9.0 d'origine dans le même environnement (8 tests `httpProxy`, sans réseau). Les tests qui supposaient que `{x}mot` est une erreur ont été mis à jour (patch `tests-syntaxe.patch`) : quand ils vérifiaient le message d'erreur, ils écrivent `{x}=mot`, qui le garde ; ceux de `testparser` montrent le jeton `annot` ;
- équivalence avec un Tcl 9 d'origine sur 18 000 cas générés (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`) : aucun résultat valide changé ;
- AddressSanitizer sans erreur ; mémoire stable sur 1 000 000 de constructions.

**Les types ne sont pas l'ennemi de la vitesse.** Mesures sur l'exemple `cstruct` (10 000 mesures, `-align natural`) :

| opération | couche Tcl | codec C | Tcl classique écrit à la main, sans contrôle |
|---|---|---|---|
| encodage (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format` ; 330 µs mesurés pour 1 000) |
| décodage (`type binary decode`) | 485 ms | 3,0 à 3,7 ms | 7,3 à 10,5 ms (`binary scan`) |

Le codec C, qui **vérifie** en plus chaque valeur contre son type, est 2 à 3 fois plus rapide que le code Tcl classique écrit à la main, et 130 à 250 fois plus rapide que la couche Tcl générique.

**Procédures typées.** Le texte d'un type n'est analysé qu'une fois : l'objet qui le porte garde son classement tant que la table des types de l'interpréteur n'a pas changé (un `type def` invalide tous les classements). Dans une procédure, le nom affecté par `x = …` ou `x is T = …` est une variable locale compilée, rangée sans recherche par son nom (`storeScalar`, et l'instruction `declareLocal` pour `X is T = v`, v16c), et `X is T = v` range une valeur déjà conforme sans repasser par une note. Mesures (exemple `geometrie`, 10 000 points) :

| opération | annotée | Tcl classique |
|---|---|---|
| `X is point = $a` | 0,6 µs | — |
| `geo::distance` | 4,7 µs | 1,9 µs |
| périmètre | ≈ 38 ms | ≈ 18 ms |
| point dans un polygone (`dedans_tous`) | ≈ 141 ms | ≈ 68 ms |

**Boucles.** `foreach` et `lmap` sur une liste typée parcourent son contenu, dont les éléments sont typés une fois pour toutes : la variable de boucle reçoit l'élément typé sans qu'il soit refabriqué à chaque tour. Sur 10 000 points, `foreach q $P {}` prend 0,24 ms contre 0,16 ms en Tcl nu (2,6 ms avant la v15u). Il en va de même pour une liste en cases (v15y). Le champ d'un produit lu par `$a(x)` est, lui aussi, typé une fois pour toutes dans le contenu quand celui-ci n'est pas partagé.

**Lecture d'un champ.** Une lecture compilée `$a(x)` garde, dans l'objet de son index, le type produit vu la dernière fois et la place du champ : la lecture suivante sur le même type va droit à l'élément. Dans `expr`, un opérande typé `int` ou `double` est lu directement dans son contenu. Depuis la v16c, `$a(x)` opérande d'un opérateur arithmétique ou d'une comparaison numérique est compilé en `loadFieldNum` : le nombre du champ, sans enveloppe de type (l'opérateur n'en lirait que le contenu), lu directement dans la case pour une vue en forme compacte ; ≈ 60 ns, contre 90 ns avant (et 0,14 µs en v15x). Depuis la v16g, il en va de même pour les opérateurs de bits (`&`, `|`, `^`, `<<`, `>>`, `~`).

**Déclaration par `set`.** `set {T}x a b…` d'un produit dont les champs sont des scalaires (`int`, `double`, `string`), avec autant de valeurs nues que de champs, valide chaque valeur sur place et la range dans le contenu sans copie ; dans une procédure, `x` (nom simple) est une variable locale compilée, et la valeur est rangée sans seconde recherche du nom. `set {point}A 1.0 2.0` : 0,6 µs, contre 1,3 µs en v16f. Toute autre forme (étiquettes de champ, valeurs annotées, produits de produits, valeur refusée) suit la voie générale, qui ne copie plus le contenu qu'elle vient de construire.

### 16. Choix de conception

| choix | raison |
|---|---|
| `{x}mot` occupe une erreur de syntaxe | compatibilité totale, aucune nouvelle règle de citation |
| pas d'empilage collé `{a}{b}$x` | ambigu avec `{int}{300}` ; on empile en annotant une valeur annotée |
| seul un constructeur résout un type | `{T}$x` hors constructeur ne coûte rien et n'échoue jamais |
| une annotation expire avec la commande qui reçoit le mot (v16m) | pas d'action à distance : l'erreur d'un cast naît dans la commande qui contient le mot (un `catch` l'attrape) ; une note ne change pas de sens quand un type est défini plus tard |
| annotations locales reconnues à leurs caractères | un nom de type ne peut pas contenir `-` ou `?` : aucune collision possible |
| `type def` nominal, `type alias` structurel | distinguer des unités (`mm`, `kg`) sans changer les valeurs |
| élargissement automatique, rétrécissement explicite | aucune perte d'information silencieuse |
| conversions explicites entre nominaux | `mm` → `m` demande `{m}$a` ou `type cast` |
| forme nom/valeur nue supprimée | `set {png}p chunks $l` se lisait de deux façons ; désormais les noms sont écrits dans le source (`{chunks}…` ou `p(chunks)`), jamais devinés dans les données |
| étiquettes de champ : tout ou rien | un mélange serait une forme positionnelle déguisée |
| le chemin ne substitue rien | une donnée contenant `[script]` n'est jamais exécutée par le parseur de chemins (la double substitution disparaît) ; même règle qu'un tableau |
| valeurs substituées atomiques | `$D($k)` lit la clé `$k`, même si elle contient `.` ou `,` |
| chemin + étiquettes non prévu | pas de besoin démontré ; on verra à l'usage |

### 17. Limites et points ouverts

**À corriger ou à décider**

- **Tensions avec EIAS.** Type perdu par la chaîne (pas de forme texte relisible), égalité, valeurs nues d'un `dict.point`, types propres à un interpréteur (un même nom désigne deux types dans deux interpréteurs) : voir `doc/CARTOGRAPHIE-EIAS.md`. Réglés : l'atomicité des produits (v16t), la lecture après libération et les noms globaux (v16u), la variante d'une somme, désormais dans sa valeur (v16w, `doc/DIAGNOSTIC-SOMMES.md`).
- **`namespace path` et caches.** La résolution d'un nom suit le chemin de l'espace courant ; un changement de chemin invalide les caches (`resolverEpoch`). Un espace d'objet TclOO a ses types comme un autre ; ils disparaissent avec l'objet.

**Pas encore fait**

- Valeur « inexistante » en général : `opt(T)` couvre les liens binaires, il n'y a pas de forme générale.
- Union binaire (deux lectures des mêmes octets, l'`union` du C : refus voulu, à ne pas confondre avec l'union `|` des types), champs de bits partagés, liste « jusqu'à la fin du fichier ».
- Annotation de commentaire `{#}` au niveau du parseur, annotation du premier mot (`{c}proc`), annotations de déclaration (`proc p {{?}{…} {!}arg}`).
- `type debug` (v16y) : pas de débogueur distant ni d'intégration à un éditeur ; pas de point d'arrêt par ligne ; le lieu d'une commande d'un corps écrit en syntaxe d'expression est approximatif ; une note posée sur une liste porte sur la liste, pas sur ses éléments (`foreach`, `lindex`, `$L(0)` ne la propagent pas : on note les éléments) ; une condition d'arrêt s'écrit dans un crochet.
- Notes par élément d'une liste typée (aujourd'hui la note porte sur la liste entière) ; effacement de toutes les notes par `{@}$x`.
- Codec C : référentiels nommés, `-linkdepth` fini, `-dedupe`, lien nul d'un champ à défaut, champs de bits (le reste est couvert depuis la v16d).
- Forme compacte : cases à pointeurs (`string`, listes imbriquées de taille variable), `lindex` sans vue neuve, fonction de stub pour le C, `lappend` en temps constant (aujourd'hui une copie par ajout, comme pour une liste typée ordinaire).
- `type {c!}export` : pointeurs pour les types récursifs ; cible `{llvm!}` ; sens inverse (en-tête C vers `type def`).
- `type def {-compact}` pour une famille elle-même (une instance nommée passe : `type def {-compact}mat3 mat(3,3)`) ; `type is` : une valeur dont le texte est aussi un nom de type (`type is {compact?}$x` avec `x` = `point`) est lue comme le type.
- Signatures typées pour les méthodes TclOO ; commandes à deux formes d'appel (`switch chaîne motif corps …` ou `switch chaîne {motif corps …}`), qui demanderaient une union de signatures.
- `cproc` : listes de chaînes, sommes. `ffi` (prototype) : structs par valeur, fonctions variadiques, propriété de la mémoire, `errno`, pointeurs contrôlés ; l'encodage des structs passe encore par la couche Tcl.
- Optimisations : l'écriture dans une variable typée (environ 7 fois une écriture nue) ; un défaut calculé, réévalué à chaque appel.
- Banc de test C sur maquette (`typeparse/test/`) à remettre à jour ; exemples `arbre_var.tcl` et `defauts_calcules.tcl` à corriger.

## Annexes

### A. Installation et tests

Le prototype part de Tcl `core-9-0-branch` avec la branche TIP 759. Depuis la racine de l'arbre Tcl (MSYS2 / MinGW64 ou Unix) :

```
bash typeparse/maj.sh [typeparse.zip]   # extrait, copie les sources de types, applique les patchs du noyau,
                                        # recompile, puis compare chaque tests_tclsh/tNN_*.tcl à son .out
```

- Les sources de types sont livrées entières dans `typeparse/` et copiées dans `generic/` ; seuls les patchs du noyau, listés dans `noyau/ORDRE.txt`, sont appliqués, sans fuzz (`-F0`), un patch déjà appliqué étant détecté et sauté.
- Options de `maj.sh` : `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "message"`.
- Notes de chaque patch : `noyau/LISEZMOI.md`. Exemples exécutables : `exemples/`.

### B. Glossaire

| terme | sens |
|---|---|
| annotation | texte entre accolades collé devant un mot : `{x}mot` |
| note | annotation non résolue, qui suit la valeur |
| type | annotation résolue dans la typologie connue |
| annotation locale | annotation dont le texte ne peut pas être un type (`{-v}`, `{is?}`) ; elle ne vit qu'entre l'appelant et la commande |
| constructeur | ce qui range une valeur et résout ses annotations : `set {T}v`, liaison d'arguments, `lassign`, `foreach`, affectation dans `expr` |
| contrat | le type d'une variable, vérifié à chaque écriture |
| cast | conversion explicite vers un type (`{T}$x` dans un constructeur, `type cast`) |
| nominal | type créé par `type def`, distinct de sa définition ; s'efface devant sa base |
| raffinement | type de base restreint, avec une représentation binaire (`int(u,8)`) |
| élargissement | passage automatique d'un raffinement vers un type plus large |
| produit | `struct` : champs nommés et ordonnés |
| somme | une variante parmi plusieurs, nommée |
| variante | l'une des branches d'une somme |
| étiquette | annotation locale qui nomme le champ ou la variante d'un argument : `{age:}12`, `{i:}5` (v16l) |
| chemin | index d'une valeur typée : `R(A.(x,y))` |
| pathindex | objet qui porte un index et ses parties substituées |
| lien | `var.T` (vers une variable) ou `*(…).T` (vers une adresse binaire) |
| paramètre fantôme | paramètre d'un type qui ne change pas les valeurs mais distingue les instances |

### C. Correspondances

| ici | C | Rust | LLVM |
|---|---|---|---|
| produit | `struct` | `struct` | `{ T1, T2 }` (packed : `<{ … }>`) |
| somme | (`union` + discriminant à la main) | `enum` avec données | — |
| énumération (somme de littéraux) | `enum` | `enum` sans données | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (sans signe) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | `char *` terminé par NUL | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 bits) | `*const T` | `ptr` |
| `opt(T)` | pointeur pouvant être `NULL` | `Option<T>` | — |
| type nominal (`type def mm double`) | `typedef` (sans contrôle) | *newtype* `struct Mm(f64)` | — |
| `-align natural` / packed | alignement par défaut / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | alignement / `<{ }>` |

