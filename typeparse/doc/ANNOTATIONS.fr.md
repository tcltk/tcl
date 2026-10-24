# Annotations en Tcl — documentation

*19 octobre 2026 · version v13 · Florent MERLET, avec Claude (Anthropic)*

*Traductions : [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md)*

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
- un **nom de champ** choisit où va une valeur (`set {pt}a {age}12 {nom}Jean`) ;
- une **représentation binaire** dit comment lire et écrire des octets (`{int(u,16,be)}version`) ;
- une **note** marque une valeur pour qu'une procédure la reconnaisse (`note has $v mm`).

Le script garde sa forme : on ajoute des annotations là où l'on veut des garanties, et nulle part ailleurs.

### 2. Notes, types et annotations locales

Une annotation s'écrit toujours `{texte}mot`. Selon ce que dit le texte et selon **qui le lit**, elle joue trois rôles.

#### 2.1 La note : l'annotation brute

Une **note** est une annotation qu'aucun constructeur n'a encore résolue. Elle suit la valeur : copie, argument de procédure, élément de liste.

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

L'écriture `{a}{b}$x` (deux annotations collées) n'existe pas : elle serait ambiguë avec `{int}{300}`, une annotation sur un littéral entre accolades. On empile en annotant une valeur qui l'est déjà.

#### 2.2 Le type : l'annotation résolue par un constructeur

Un **type** est une annotation **résolue** dans la typologie connue de l'interpréteur (types de base, `type def`, `type alias`). Seul un **constructeur de variable** résout :

- `set {T}v …` (et `set v {T}$x`) ;
- la liaison des arguments de `proc` ;
- `lassign`, `foreach`, et dans `expr`, `x = v` et `x is T = v`.

Annotée sur le **nom** d'une variable, une annotation donne à la variable un **contrat** : toute écriture ultérieure est vérifiée.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Hors constructeur, `{T}$x` ne vérifie rien et ne convertit rien, même si `T` est un type connu : `T` est simplement **empilé**, comme une note. `list {int}abc` ne lève pas d'erreur.

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

Une note posée **avant** son `type def` est donc résolue par le constructeur suivant : écrire `type def mm double` plus tard donne un sens aux `{mm}` déjà posés.

**Le contexte d'abord.** Un constructeur résout dans son contexte avant les types globaux : pour une somme, les noms de variantes (`{i}5` dans `set {S}v {i}5`) ; pour un produit, les noms de champs (`{age}12`). Un type global du même nom ne change rien.

#### 2.4 Les annotations locales

Une annotation dont le texte contient un caractère que les noms de types refusent (`-`, `?`, `:`…) ne peut **jamais** être un type. C'est une **annotation locale** : elle ne vit qu'entre l'appelant et la commande appelée, et ne suit pas la valeur.

```tcl
type binary {sizeof?}$T          ;# une question posée à type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# refusé : local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# refusé : local annotation {-v} cannot declare a variable
```

Deux familles naturelles, sans risque de collision avec un nom de type : les options en `-nom` et les questions en `nom?`. Un nom de type ne contient que des lettres ASCII, des chiffres, `_` et des octets non ASCII, et ne commence pas par un chiffre.

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

### 4. La commande `type`

```
type def NOM DÉFINITION        ;# définit un type nominal (ou une famille paramétrée)
type alias NOM DÉFINITION      ;# donne un nom structurel à une définition
type get VALEUR                ;# le type résolu d'une valeur (vide s'il n'y en a pas)
type cast TYPE VALEUR          ;# le résolveur d'un constructeur, sans variable
type convert DE VERS LAMBDA    ;# déclare une conversion explicite
type deref VALEUR              ;# instantané : chaque lien var remplacé par sa valeur
type binary TYPE ...           ;# représentation binaire, encodage, décodage (§ 9)
type def {-compact}NOM DÉF     ;# une liste toujours rangée en cases (§ 12.1)
type is {compact?}TYPE|VALEUR  ;# une question sur un type ou une valeur
type {c!}export TYPE ?TYPE ...? ;# en-tête C des types nommés (§ 12.2) ; {llvm!} prévu
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
| `,` | somme | `{int}i, {string}s` |
| `{ }nom` | groupement, avec un nom collé à l'accolade fermante | `{{int}x; {int}y}A` |

`;` et `,` ne se mêlent pas au même niveau (`{int}a; {int}b, {string}c` est refusé) : la partie intérieure se regroupe entre accolades et se nomme (`{int}a; {{int}b, {string}c}u`), ou se définit à part avec `type def`. Dans un produit, tout champ a un nom, un groupe aussi ; une somme garde ses variantes anonymes (`{int}, {string}`, `{{int}a; {int}b}, {string}c`).

**Caractères.** Un nom de type ne contient que des lettres ASCII, des chiffres, `_` et des octets non ASCII, sans commencer par un chiffre. La grammaire emploie en plus `{ } , ; . = " # ( ) * $`. Tout autre signe ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) est refusé : `invalid character "!"`, code `TCL PARSE TYPE BADCHAR`, sauf après `=` (une valeur par défaut) et dans les parenthèses d'un constructeur (`*(file://x, …)`). C'est ce refus qui rend les annotations locales (`{-v}`, `{is?}`) sûres.

Une définition est **déclarative** : aucune substitution n'y est faite à la lecture, sauf dans une valeur par défaut, évaluée à chaque construction.

#### 5.3 Champs et conteneurs

**Énumération** — une somme de littéraux.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# un seul littéral : une constante (signature de fichier)
```

**Somme** — une seule variante à la fois, qui porte son nom.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# la variante choisie par le nom du type
```

**Produit** — une `struct` : des champs nommés, ordonnés, chacun de son type.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positionnel
set {pt}b {age}12 {nom}Jean    ;# étiquettes de champ (§ 8)
$a(nom)                        ;# Jean
```

**Liste** — `list` (extensible), `list.T` (éléments de type T), `list(N).T` (exactement N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

Les éléments d'une `list.T` sont atomiques : `lindex`, `foreach`, `$L(i)` rendent des éléments typés, jamais redécoupés en mots. La liste nue `{list}` garde le comportement de Tcl.

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
set {pt(double)}p {x}1.5       ;# 1.5 0
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
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

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

**Produit vers collection.** Tout produit se convertit en liste ou en dict : `{list}$p` donne la liste de ses champs, nus (`foreach {x y} {list}$p`) ; `{list.any}$p` les garde annotés, comme `$p(*)` ; `{list(2).double}$p` les valide ; `{dict}$p` donne `x 3 y 4`. Une liste nominale se convertit de même en liste structurelle (`{list.point}$P`, `{list}$P`). Sur une valeur typée, `{list}$p` est un cast immédiat, même hors d'un constructeur.

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

**Adoption.** Une variable **non typée** qui reçoit une valeur typée **adopte** son type, et donc son contrat :

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e devient une variable int
set e hello                    ;# expected integer but got "hello"
```

C'est le comportement le plus surprenant du modèle. Le piège classique est `catch … m` réutilisé : si un premier `catch` range dans `m` une valeur typée, un second `catch` ne peut plus y ranger son message d'erreur. Utiliser une variable par usage, `unset` entre deux usages, ou `set {}m …`. (Point ouvert, § 17.)

**Procédures.** Un argument qui reçoit une valeur typée est typé et strict dans la procédure ; un argument ordinaire ne change pas.

**Conversions.** Une valeur typée se lit comme un nombre si sa chaîne en est un et que son type n'est pas une collection : `string`, une énumération (`type def pi {"3.14159"}` puis `expr {$PI * 2}`), un produit d'un seul champ (`{{int}i}`). Un produit de plusieurs champs, une liste ou un dict restent refusés. `string is` juge toujours la chaîne.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Variables de boucle.** `foreach`, `lmap`, `dict for`, `dict map` (et le filtre de `dict filter`) oublient le type de leur variable avant chaque affectation ; après la boucle, la variable garde le type du dernier élément reçu.

#### 6.3 `type deref` : un instantané des liens

`type deref V` rend `V` où chaque lien `var` est remplacé par la valeur qu'a la variable à cet instant. Le résultat a le type « sans les liens » (`list.var.int` → `list.int`), nommé `deref(<type>)` pour un produit ou une somme. Un seul niveau est suivi par appel ; une variable manquante est une erreur qui nomme la case (`type deref: at 2: can't read "::C": no such variable`), sans résultat partiel. Fonctionne dans les listes, dicts, produits, sommes et types récursifs ; un cycle ne boucle jamais.

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
| étiquettes de champ | `set {pt}a {age}12 {nom}Jean` | toutes les valeurs étiquetées ; ordre libre ; champs omis à leur défaut |
| un seul champ | `set {pt}a {pt.nom}Jean` | les autres champs à leur défaut |
| chemin + valeurs positionnelles | `set {pt}a(age,nom) 12 Jean` | une valeur par feuille du chemin (ou par groupe, § 8.3) |

S'y ajoutent : **aucun argument** (`set {pt}a`, tous les champs à leur défaut) et **un produit du même type** (copie). Tout le reste est une erreur :

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

La forme nom/valeur nue (`set {pt}a nom Jean age 12`) **n'existe plus** : elle se confondait avec la forme positionnelle dès que *n* arguments valaient pour *n* champs (§ 16).

#### 8.2 Les étiquettes de champ

L'étiquette est l'annotation **la plus récente** de chaque argument, lue dans le contexte du produit : un nom de champ passe avant un type global du même nom.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# ne change rien
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# une étiquette portée par une variable compte
```

**Tout ou rien :**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
set {paire}p {aut.nom}x         ;# aut.nom names a field of aut, not of paire
```

Une valeur entre accolades s'étiquette aussi : `set {seg}s {a}{1 2} {b}{3 4}`.

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

**La combinaison chemin + étiquettes n'existe pas** : `set {rect}R(A) {y}2 {x}1` est une erreur (`1 targets but 2 values`). Elle n'est pas prévue tant qu'un usage ne la demande pas.

#### 8.4 Sommes et listes

Une somme se construit avec sa variante en étiquette, ou par le nom de la variante dans le type :

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# chaque élément garde sa variante
```

Sans étiquette, la variante est déduite de la valeur : la plus stricte de celles qui l'acceptent (`set {S}u 5` donne `S.i`, `int` étant plus strict que `string`). Quand plusieurs variantes **également strictes** l'acceptent (même forme, `{int}i, {int}j`, ou formes sans ordre entre elles), c'est une erreur qui propose l'étiquette :

```tcl
type def A {{int}i, {int}j}
set {A}v 5      ;# value "5" fits variants i and j of A equally: write {i}5, or set {A}v(i) 5
set {A}v {j}5   ;# A.j
```

La variante choisie reste attachée à la valeur, y compris dans une liste ou un produit (`set {list.A}L $v`, `set {R}r $v 1` : `A.j`), et `type cast A {j}5` la lit aussi. Une modification (`set v 8`, `incr v`, `set L(0) 4`, `lset L 0 9`, `set r(a) 7`) garde la variante courante quand plusieurs conviennent également ; la valeur choisit toujours si une seule variante convient, ou si une est plus stricte. Variantes anonymes de même forme (`{int}, {int}`) : rien ne permet de choisir, `name the variants to choose one`.

**Conversions entre variantes.** Deux variantes d'une même somme peuvent recevoir une règle, sens par sens :

```tcl
type def temp {{double}celsius, {double}fahrenheit}
type convert temp.celsius temp.fahrenheit {{x} {expr {$x*9/5.0 + 32}}}
set {temp}t {celsius}100
set t(fahrenheit)              ;# 212.0 : lecture d'une variante inactive, par la règle
set u {temp.fahrenheit}$t      ;# cast explicite : u vaut 212.0, de type temp.fahrenheit
type cast temp.fahrenheit $t   ;# idem
```

La lecture d'une variante inactive et le cast `{S.v}` (ou `type cast S.v`) prennent, dans l'ordre : la règle ; refus si les deux variantes ont la même forme (un cast identité dirait qu'un celsius est un fahrenheit : `… both have the same form, so reading one as the other needs a rule: type convert temp.celsius temp.fahrenheit {{x} ...}`) ; sinon le cast ordinaire entre les types des deux variantes (`{int}i, {string}s` : `{i}5` lu par `$v(s)` donne `5`). Une règle n'intervient jamais dans la déduction (`set {temp}y 5` reste ambigu), ni dans l'écriture (`set t {fahrenheit}50` change de variante sans convertir). Pas d'enchaînement (une règle a → b et une règle b → c ne font pas a → c), pas de réciproque supposée.

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

Options : `null` (l'adresse 0 signifie « pas de cible »), `null=V`. Avec `opt(T)`, une adresse nulle donne `none`, sinon `some` : c'est la différence entre « nul » et « inexistant ». À l'écriture, les cibles sont ajoutées à la fin, alignées, et les adresses calculées ; `-dedupe 1` n'écrit qu'une fois les cibles identiques. Un champ lien qui vaut son défaut s'écrit comme un lien nul.

**`-linkdepth`** règle le nombre de liens suivis (`Inf` par défaut). Un lien non suivi est un entier annoté (la position absolue de la cible) ; `type binary deref` le suit d'un pas. Cela permet de lire seulement l'en-tête d'un gros fichier, de parcourir une liste chaînée, et de réécrire un champ sur place.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8 : la cible
```

#### 9.6 Codecs C

`type binary encode` et `decode` essaient d'abord une voie en C, qui couvre les dispositions fixes : entiers de 1, 2, 4, 8 octets, flottants de 4 et 8 octets, produits (avec `-align`), `list(N).T` et `list($n).T`. Hors de ce domaine (chaînes, sommes, liens, `at`, champs de bits, tailles calculées, `-dedupe`), ou en cas d'erreur, la couche Tcl prend la main et donne ses messages. `set ::tcl_typebinaryc 0` désactive la voie C. Les octets et les valeurs produits sont identiques dans les deux voies (test `t51_codec_c`). Mesures : § 15.

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
| étiquettes et positions mêlées | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
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

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand` : un mot `{texte}suite` produit un jeton **`TCL_TOKEN_ANNOT`** (512, `tcl.h`) qui porte le texte de l'annotation, suivi des jetons du reste du mot. Un mot annoté `{T}nom(...)` va jusqu'à la parenthèse équilibrée, espaces compris, sur une ligne.
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

Chemin non compilé : `TclSubstTokens` (`SubstIndexTokens`) produit le même objet pathindex.

#### Autres points d'accroche du noyau

| fichier | changement |
|---|---|
| `tclBasic.c` | `TclTypesInit` : crée `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` passe par le chemin lent pour une valeur annotée ; `INST_FOREACH_STEP` oublie le type ; lecture et écriture d'éléments typés (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` et `TclTypedVarAfterStore` autour de chaque écriture : le contrat |
| `tclProc.c` | `TclTypedVarBindArg` : liaison typée des arguments |
| `tclCmdAH.c`, `tclDictObj.c` | variables de boucle (`TclTypedVarForgetByName`) ; `TclAnnotUnwrap` dans les lectures de dict |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | conversions du noyau sur le contenu (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`) : une valeur annotée n'est jamais convertie en place |
| `tclListObj.c` | `lappend` par la `replaceProc` d'une liste abstraite ; `lindex` à plusieurs indices sans aplatir |
| `tclCompExpr.c` | opérateur `is` (lexème `IS_TYPE`, `IsTypeAhead`), affectations `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. Les fichiers ajoutés

| fichier | lignes | contenu |
|---|---|---|
| `tclTypes.c` | 4 500 | typologie, `type def/alias/get/cast/convert/deref/binary`, validation, élargissement, casts, codecs C |
| `tclTypeParse.c` | 2 700 | analyseur et compilateur des expressions de types (modèle de `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | types paramétrés, instances, paramètres fantômes, raffinements binaires |
| `tclAnnotObj.c` | 2 150 | l'objet annoté, l'état `{T, N}`, la résolution, `note` |
| `tclTypedVar.c` | 1 350 | variables typées, constructeurs (`TclSetAnnotatedCmd`, `BuildProductEx`, étiquettes de champ) |
| `tclTypePath.c` | 2 900 | langage de chemin, lecture, écriture, construction par chemin, objet pathindex |
| `tclVarRef.c` | 360 | liens `var` (nom canonique, accès, liens périmés) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | structures (`TclType`, `TclTypeField`, `TclBinLink`…) et interfaces |

Côté Tcl : `layout/filebinary.tcl` (couche binaire : `type binary decode/encode/deref/space`, `chan binary`) et `layout/binaliases.tcl` (alias).

#### 12.1 La forme compacte (prototype)

**Par type** : `type def {-compact}polygone list.point`. Les valeurs de ce type sont **toujours** en cases : les nombres sont normalisés à la construction (le constructeur est une conversion : `{0 0}` devient `{0.0 0.0}`, `0x10` devient `16`, un `double(32)` est arrondi au `float`), et une conversion du noyau (`string length`, `string range`, `dict`…) travaille sur une copie en chaîne gardée à côté des cases. Le type doit être une liste dont l'élément a une disposition : sinon `type def` refuse, et le nom reste libre. Pas encore pour une famille (`type def {-compact}{v({int}n)} …`) ; pas d'option pour un alias. `type is {compact?}polygone` dit si le type le demande, `type is {compact?}$P` si la valeur est en cases.

```tcl
type def {-compact}polygone list.point
set {polygone}P {0 0} {4 0} {4 3}     ;# {0.0 0.0} {4.0 0.0} {4.0 3.0}
type is {compact?}$P                   ;# 1, même après string length $P
```

**Pour tout le programme**, derrière `tcl::unsupported::compact on` (ou la variable d'environnement `TCL_TYPES_COMPACT=1`, pour passer les tests), le contenu V d'une liste `list.T` est rangé en **cases** quand T est un scalaire à taille fixe (`int(s,n)`, `int(u,n)` avec n = 8, 16, 32 ou 64 ; `double`, `double(32|64)`) ou un produit de tels champs, produits imbriqués compris. Une case par élément, disposée comme le struct C équivalent (alignement naturel, mesuré à la compilation : `tcl::unsupported::abi`). `int` sans taille n'entre pas (c'est un grand entier).

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

#### 12.2 `type {c!}export` : l'en-tête C des types

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

- tests de Tcl `parse`, `parseExpr`, `parseOld`, `subst` (dont 257 tests de `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info` : résultats identiques à la base ;
- équivalence avec un Tcl 9 d'origine sur 18 000 cas générés (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`) : aucun résultat valide changé ;
- AddressSanitizer sans erreur ; mémoire stable sur 1 000 000 de constructions.

**Les types ne sont pas l'ennemi de la vitesse.** Mesures sur l'exemple `cstruct` (10 000 mesures, `-align natural`) :

| opération | couche Tcl | codec C | Tcl classique écrit à la main, sans contrôle |
|---|---|---|---|
| encodage (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format` ; 330 µs mesurés pour 1 000) |
| décodage (`type binary decode`) | 485 ms | 3,0 à 3,7 ms | 7,3 à 10,5 ms (`binary scan`) |

Le codec C, qui **vérifie** en plus chaque valeur contre son type, est 2 à 3 fois plus rapide que le code Tcl classique écrit à la main, et 130 à 250 fois plus rapide que la couche Tcl générique.

### 16. Choix de conception

| choix | raison |
|---|---|
| `{x}mot` occupe une erreur de syntaxe | compatibilité totale, aucune nouvelle règle de citation |
| pas d'empilage collé `{a}{b}$x` | ambigu avec `{int}{300}` ; on empile en annotant une valeur annotée |
| seul un constructeur résout un type | `{T}$x` hors constructeur ne coûte rien et n'échoue jamais ; la vérification a lieu là où la valeur est rangée |
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

- **Adoption par une variable non typée.** `set e [lindex $L 1]` rend `e` typée et stricte. Le piège `catch … m` (§ 6.2) en découle, et l'erreur qui s'en échappe est le message d'origine, ce qui la rend difficile à comprendre. À décider : adoption silencieuse, adoption sans contrat, ou message explicite.

**Pas encore fait**

- Valeur « inexistante » en général : `opt(T)` couvre les liens binaires, il n'y a pas de forme générale.
- `union`, champs de bits partagés, liste « jusqu'à la fin du fichier » (le refus d'`union` est voulu).
- Annotation de commentaire `{#}` au niveau du parseur, annotation du premier mot (`{c}proc`), annotations de déclaration (`proc p {{?}{…} {!}arg}`).
- Notes par élément d'une liste typée (aujourd'hui la note porte sur la liste entière) ; effacement de toutes les notes par `{@}$x`.
- Codecs C pour les sommes, les liens et les tailles calculées ; cache du type analysé.
- Forme compacte : cases à pointeurs (`string`, listes imbriquées), lecture directe dans les cases par `expr` compilé, fonction de stub pour le C, `lappend` en temps constant (aujourd'hui une copie par ajout, comme pour une liste typée ordinaire).
- `type {c!}export` : pointeurs pour les types récursifs ; cible `{llvm!}` ; sens inverse (en-tête C vers `type def`).
- `type def {-compact}` pour une famille ; `type is` : une valeur dont le texte est aussi un nom de type (`type is {compact?}$x` avec `x` = `point`) est lue comme le type.
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
| adoption | une variable non typée prend le type de la valeur reçue |
| cast | conversion explicite vers un type (`{T}$x` dans un constructeur, `type cast`) |
| nominal | type créé par `type def`, distinct de sa définition ; s'efface devant sa base |
| raffinement | type de base restreint, avec une représentation binaire (`int(u,8)`) |
| élargissement | passage automatique d'un raffinement vers un type plus large |
| produit | `struct` : champs nommés et ordonnés |
| somme | une variante parmi plusieurs, nommée |
| variante | l'une des branches d'une somme |
| étiquette de champ | annotation qui nomme le champ d'un argument : `{age}12` |
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

