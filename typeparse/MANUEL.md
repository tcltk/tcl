# Manuel des types annotés — chapitres

> **Remplacé par la documentation complète `doc/ANNOTATIONS.fr.md`** (et ses traductions `doc/ANNOTATIONS.{en,de,nl,es,pt,it}.md`). Ce fichier et les `MANUAL.*.md` restent pour l'historique.

*(Ce fichier accueille les chapitres du manuel au fur et à mesure ; les notes
de patch restent dans `noyau/LISEZMOI.md`.)*

*Traductions / translations : `MANUAL.en.md`, `MANUAL.de.md`, `MANUAL.nl.md`,
`MANUAL.es.md`, `MANUAL.pt.md`, `MANUAL.it.md`.*

## Dict et produit : deux façons de parler d'une structure

Pour qui vient du C : une **liste** Tcl est un tableau, un **dict** est une
structure à clés libres, un **type produit** est une `struct` déclarée. Les
deux derniers se ressemblent beaucoup, et on passe souvent de l'un à l'autre.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# produit
type def fiche  {dict(nom, age?=0).string}              ;# dict typé
```

### Ce qu'ils ont en commun

| | produit `pt` | dict `fiche` |
|---|---|---|
| construction par nom | `set {pt}a {nom}Jean {age}12` ou `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| accès à un champ | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| défaut d'un champ/clé | `age ?= 0` (ou `=`) | `age?=0` |
| champ inconnu | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| valeur sérialisée | liste Tcl | liste Tcl clé/valeur |

L'écriture du défaut est la même : `?=` s'emploie dans les deux cas, et un
défaut peut être un littéral, `[script]` (calculé à chaque construction) ou
`$variable`. Dans un produit, l'ancien `=` reste accepté.

### Ce qui les distingue

* **Champs présents.** Dans un produit, tous les champs existent toujours :
  un champ omis prend son défaut, et un champ sans défaut doit être donné
  (`field "x" of pt has no value and no default`). Dans un dict, une clé
  sans `?` est obligatoire (`key "nom" is missing in fiche`) et une clé
  marquée `?` peut être absente.
* **Ordre.** Un produit a des champs ordonnés ; il se construit aussi en
  **positionnel** (`set {pt}b Jean 12`, comme l'initialisation d'une struct
  C). Un dict n'a pas de forme positionnelle.
* **Un seul champ.** Un produit accepte le préfixe `{T.champ}` : 
  `set {pt}c {pt.nom}Jean` fixe `nom`, les autres champs prennent leur
  défaut (`Jean 0`). C'est la même écriture que `{Val.i}5` pour une somme.
* **Types des champs.** Chaque champ d'un produit a son propre type
  (`{string}nom; {int}age`) ; un `dict(...)` en a un seul pour toutes les
  valeurs (`.string`).
* **Taille.** Un produit a un nombre fixe de champs, fixé à la définition.

### Nommer les champs : étiquettes ou chemin (plus de nom/valeur nue)

La forme nom/valeur nue (`set {pt}a nom Jean age 12`) est **supprimée** : elle
se confondait avec le positionnel dès que *n* arguments valaient pour *n*
champs. Un produit se construit maintenant de trois façons, sans rien deviner :

* **positionnel** : autant de valeurs que de champs, dans l'ordre
  (`set {pt}b Jean 12`) ;
* **étiquettes de champ** : `set {pt}a {age}12 {nom}Jean`. L'étiquette est
  l'annotation la plus récente de chaque argument ; elle est lue dans le
  contexte du produit, donc un nom de champ passe avant un type global du même
  nom (`type def age int` ne change rien), comme le nom d'une variante pour
  une somme. Ordre libre, les champs omis prennent leur défaut. **Tout ou
  rien** : mélanger étiquettes et valeurs positionnelles est une erreur
  (`can't mix values named by a field annotation …`), une étiquette inconnue
  aussi (`unknown field "taille" in pt`). Une valeur qui porte son étiquette
  en variable compte (`set v {age}12; set {pt}a {nom}Jean $v`) ;
* **chemin** : `set {pt}a(age,nom) 12 Jean`, utile quand les noms sont
  calculés ou les valeurs entre accolades.

Le dict, lui, garde sa forme clé/valeur : un dict n'a pas de forme positionnelle.

### Choisir

* Un ensemble de champs **connu et stable**, avec des types variés : produit.
* Des clés **libres ou facultatives**, de même type : dict.
* Un format avec plusieurs formes possibles (PNG, GIF…) : une **somme**
  de produits (voir `exemples/png_somme.tcl`).

## Types restreints par un prédicat : `T((expression))`

Un prédicat restreint un type aux valeurs pour lesquelles une expression est
vraie. Il s'écrit comme dernier argument du type, entre parenthèses : c'est
la forme expression de TIP 759, et `$` seul y désigne la valeur.

```tcl
type def pair     int(($%2==0))                   ;# les entiers pairs
type def pct      {double((0 <= $ && $ <= 100))}
set {int(u,8,($%2==0))}b 200                      ;# un octet pair, en ligne
set {string(($ ne ""))}nom Jean                   ;# une chaîne non vide
set {string(([string length $] < 9))}code ABC     ;# une commande, aussi
proc moitie {{pair}n} {expr {$n/2}}
set {list.pair}L 2 4 6                            ;# chaque élément
```

* **Un visa, vérifié à l'entrée.** Le prédicat est évalué quand une valeur
  entre dans le type : `set {T}x`, un paramètre, `type cast`, une écriture
  par chemin (`set L(1) 3`, `lset`, une tranche), `incr`, la construction
  d'une liste, `type repeat`, le résultat typé de `type map`. Une valeur
  déjà de ce type y entre telle quelle : elle a son visa.
* **Sans cast.** Un `{int}6` entre dans `{pair}` si le prédicat est vrai ;
  un `pair` entre partout où l'on attend un `int`.
* **L'état peut varier.** Le prédicat peut lire des variables qualifiées :
  `int(($ < $::max))`. La valeur certifie que la condition était vraie à
  son entrée ; elle garde son type si `::max` change. Une variable non
  qualifiée est refusée à la définition : elle serait lue dans le cadre,
  quel qu'il soit, qui vérifie la valeur.
* **Rien à modifier.** Le prédicat ne doit rien changer (comme `assert` en
  C) : il n'est pas évalué en mode `off`, et une valeur qui a le visa n'est
  pas revérifiée. Un prédicat qui écrit donnerait un programme différent
  selon le mode.
* **Le type restreint d'abord.** `int(u,8,($%2==0))` refuse 300 parce
  qu'il ne tient pas dans un octet, 201 parce qu'il est impair. Les cases
  (listes compactes) gardent la représentation du type restreint.
* **Rapide quand c'est numérique.** Un prédicat qui ne lit que la valeur,
  des nombres et des variables qualifiées numériques est calculé sans
  interpréteur (environ 30 ns par valeur) ; sinon il passe par `expr`,
  la valeur y étant écrite en littéral.
* **Les prédicats s'empilent.** `type def pos2 {pair(($ > 0))}` : pair,
  puis positif.
* **Limite.** Un prédicat sur une liste entière (`list(($ ne {}))`) n'est
  pas encore accepté : on restreint chaque élément, `list.int(($ > 0))`.

Message : `value "3" is not of type pair: ($%2==0) is false`, code
`TCL TYPE PRED`. Test : `t133_predicats.tcl`.

## bitarray : un entier dont on lit les bits

`bitarray(n, ordre)` est un entier de n bits (0 ≤ v < 2^n). L'ordre dit d'où
part l'indice 0 :

| | `lsb0` (défaut) | `msb0` |
|---|---|---|
| bit d'indice 0 | celui de DROITE (`1 << 0`) | celui de GAUCHE |
| usage | registres, masques, C | schémas de protocoles (RFC) |
| indice d'un bit si la largeur change | inchangé | décalé |

Prenons l'octet `0xB2`, écrit `10110010` :

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Lecture.** `$b(i)` donne le bit (0 ou 1). `$b(i:j)` donne les bits i à j
comme UN entier, typé `bitarray(j-i+1, ordre)` : `$b(1:4)` vaut 9 pour lsb0 ;
`$c(0:3)` vaut 11 pour msb0 (les quatre bits de gauche, `1011`).

**Écriture.** `set b(0) 1`, `set b(4:7) 0`. Dans `expr`, une tranche
s'écrit entre guillemets, car le `:` appartient à l'analyseur de `expr` :
`expr {"b(1:2)" = 3}`. Une valeur qui ne tient pas dans les bits visés est
refusée.

**Tranches à pas.** `début:fin:pas`, à la Python, la fin restant incluse
comme partout en Tcl ; chaque partie peut manquer (`:fin`, `début:`,
`::pas` ; début 0, fin `end`, pas 1). Avec un pas, la tranche est une
sélection de bits : `$b(0:6:2)` rend les bits 0, 2, 4, 6 en un
`bitarray(4, ordre)` ; `set b(1::3) 1` met à 1 tous les bits choisis (0 ou
1 vaut pour tous), une liste `{1 0 1 1}` les donne un par un, un
`bitarray` de la bonne largeur sert de motif. Le crible d'Ératosthène :

    for {set i 2} {$i*$i < $N} {incr i} {
        if {!$b($i)} {set b([expr {$i*$i}]::$i) 1}
    }

Les listes ont les mêmes tranches : `$L(0:end:2)`, `$L(::-1)` (le pas
négatif retourne, en lecture), `set L(1::2) 0` (une valeur pour tous),
`set L(::4) 7 8` (une par élément ; la longueur ne change pas). Une liste
compacte ou un grand bitarray tenu par la seule variable est écrit sur
place.

**Fenêtres.** `(d0,d1,...):fin:pas` : une colonne par début, toutes
avançant du même pas ; la fin donnée borne le premier début, et on ne
garde que les fenêtres complètes (aucune colonne ne sort de la liste). Sur
une polyligne plate `x1 y1 x2 y2 ...` :

    $pl((0,1)::2)            ;# deux colonnes : les x, les y
    $pl((0,1,2,3)::2)        ;# x[i] y[i] x[i+1] y[i+1], pour chaque i
    type map -sum {x0 y0 x1 y1} $pl((0,1,2,3)::2) {($x0*$y1 - $x1*$y0)}
    set out((0,1)::2) $X $Y  ;# réentrelace deux colonnes

Les débuts sont des index simples (entier, `end-k`, ou une valeur
substituée : `$P(($i,2)::3)`). Une fenêtre d'un seul tour se lit comme une
tranche d'un élément (`$L((5,6)::2)` rend `5.0 6.0`). En écriture, une
valeur par colonne : une liste de la longueur de la colonne, ou une valeur
pour tous. Un index entièrement substitué reste un nom entier (`set s
1:3; $L($s)` cherche l'élément `1:3`) : une valeur ne fait pas de la
syntaxe de chemin.

**Vues.** Une tranche à pas d'une liste typée de scalaires n'est pas
copiée : c'est une *vue* (la liste de base, un début, un pas, un nombre),
en cases comme en objets. Pour une liste d'objets, la base est tenue par
une copie qui partage son stockage (comme `lrange` dans Tcl 9) : ni la
tranche ni sa libération ne touchent les éléments. `type map` lit une vue
directement dans sa base ; `llength`, `lindex` la lisent sans la défaire ;
les autres commandes la voient comme une liste (faite une fois, à la
demande). Une vue ne change pas quand la base change. `TCL_TYPE_NOVIEW=1`
fait des copies, pour comparer. Écrire des colonnes typées du type des
éléments (`set L(::2) $X`, `set L((0,1)::2) $X $Y`) dans une liste de
nombres (ou de `num`) tenue par la seule variable se fait sur place, sans
revérifier les éléments.

**Deux ordres, deux types.** `bitarray(8,lsb0)` et `bitarray(8,msb0)` ne se
mélangent pas : affecter l'un à l'autre est un `type mismatch`. Le défaut
`bitarray(8)` est `bitarray(8,lsb0)`.

**Fonctions.** `popcount(x)` : nombre de bits à 1 ; `bitlen(x)` : largeur d'un
bitarray, sinon nombre de bits de l'entier.

**Dans les autres types.** Un champ de produit (`{bitarray(8)}flags`), un
élément de liste (`list.bitarray(4)`) ou une variable de procédure sont des
bitarray comme les autres : `set r(flags.7) 1`, `set L(2.3)`.

**Cela ressemble à un dict ?** Non : pas de clés ni de champs, un seul entier.
Pour décrire les champs d'un en-tête binaire, on combine un produit de
bitarray (voir le chapitre précédent).

Exemples complets : `exemples/bitarray_exemples.tcl` (octet « packed » d'un
GIF et drapeaux TCP en `msb0`, permissions Unix en `lsb0`).

## type deref : prendre un instantané des liens

Un lien `var` contient le NOM d'une variable et suit sa valeur à chaque
lecture. `type deref` remplace chaque lien par la valeur qu'a la variable À
CET INSTANT : le résultat ne change plus ensuite.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : le lien suit B
    puts $v                             ;# 1 2 3 : l'instantané est figé

* Le résultat a le type « sans les liens » : `list.var.int` devient
  `list.int`, et se manipule comme n'importe quelle liste d'entiers.
* Une variable manquante arrête tout par une erreur qui nomme la case :
  `type deref: at 2: can't read "::C": no such variable`. Il n'y a pas de
  résultat partiel et la valeur d'origine n'est pas touchée.
* Un seul niveau est suivi : si une variable visée contient elle-même des
  liens, ils sont conservés ; un second `type deref` les suit.
* Marche dans les listes, dicts, produits et SOMMES, imbriqués ou non, et
  dans les types RÉCURSIFS (arbres comme `tree(var)`). La variante d'une
  somme est conservée.
* Les graphes se traitent par niveaux : si les voisins d'un nœud sont des
  liens (`{list.var.node}next`), `type deref` donne le nœud avec ses voisins
  remplacés par leurs valeurs, qui contiennent encore des liens ; un second
  `type deref` descend d'un niveau. Un cycle ne fait donc jamais boucler.
* Le type du résultat est un type propre, nommé `deref(<type d'origine>)`
  (`deref(tree(var))`, `deref(mix)`), calculé une fois puis réutilisé : deux
  instantanés du même type ont le même type.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b:}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# arbre dont les valeurs sont des liens
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

See also `MANUAL-deref.md` (English): the `var` link type and `type deref`, with examples in `exemples/deref_manual.tcl`.

## Somme et `union` du C : à ne pas confondre

Pour qui vient du C : une **somme** (`{{A}x, {B}y}`) ressemble à une `union`, mais ce n'est
pas la même chose.

| | `union` du C | somme |
|---|---|---|
| ce qu'elle contient | les **mêmes octets**, lus de plusieurs façons | **une seule** variante, qui porte son nom |
| qui sait laquelle est valide | personne : le programmeur doit le savoir | la valeur elle-même (`type get` donne `S.x`) |
| lire la mauvaise variante | permis, on lit des octets quelconques | impossible : on accède à la variante qu'on a |
| taille dans un fichier | celle de la plus grande vue | celle de la variante choisie |
| choix de la variante | aucun, il n'y en a pas | par un champ déjà lu (`S($tag)`) ou par son nom (`S(x)`) |

Conséquences :
* Il n'y a **pas d'`union` du C dans ce modèle**, volontairement : on garde les garde-fous.
  Une somme ne peut pas être lue « de travers ». Sa variante est une **métadonnée**, comme son
  type (v17z) : la valeur est la charge seule, `type get` donne `S.x`, et rien n'est deviné
  (voir ci-dessous). L'**union** `{A}x | {B}y` n'est pas non plus une union C : elle accepte une
  valeur de l'une des formes, ses alternatives doivent être disjointes, et la valeur montre
  elle-même laquelle elle est.
* Quand le format ne dit pas quelle variante est valide (la `Tcl_ObjInternalRep` de Tcl, dont
  le discriminant est l'adresse `typePtr`, extérieure à l'union), on choisit la variante
  **à l'appel** : `Tcl_ObjAs(twoPtrValue)` (voir `exemples/tcl_obj.tcl`). C'est l'appelant qui
  sait, comme en C, mais le type reste vérifié.
* Une somme dont plusieurs variantes ont la **même forme** n'est pas ambiguë dès que la variante
  est choisie par son nom (`S(y)`) : il n'y a plus rien à deviner.

### La variante, une métadonnée (v17z)

    type def S {{int}i, {string}s}
    set {S}v {s:}5            ;# la valeur est 5, type get $v donne S.s
    expr {$v * 2}             ;# 10 : une somme se lit comme sa variante
    set {S}w abc              ;# une seule variante l'accepte : S.s
    set {S}w 5                ;# erreur : fits variants i and s of S: write {i:}5

* **La valeur est la charge.** La variante suit la valeur comme son type (copies, arguments,
  retours, éléments de liste, champs) ; `$v(i)` lit la charge en vérifiant la variante.
* **Rien n'est deviné.** Une valeur nue n'est déduite que si une seule variante l'accepte ; un
  littéral (`"auto"`) se reconnaît toujours. Sinon, une erreur montre l'étiquette (`{i:}5`) ou
  le chemin (`set v(i) 5`). Rien n'est collant non plus : `set v 8` sur un `S.s` redéduit.
  (La règle « la plus stricte gagne » reste celle des unions, donc de `num`.)
* **Égalité en valeur, identité par les types.** `eq`, `==`, les clés de dict comparent la
  valeur : `{i:}5` et `{s:}5` sont égaux. `type eq $a $b` compare les types seuls, variante
  comprise (ni la valeur ni les notes) :

      if {![type eq $a $b]} {set b [type cast [type get $a] $b]}   ;# fahrenheit -> celsius
      expr {$a + $b}

* **Par la chaîne.** Hors de l'interpréteur (fichier, socket, C), la variante se perd comme le
  type ; relue, une valeur ambiguë est une erreur. `type canonic` écrit la variante quand la
  chaîne ne la dit pas (`{S.s}5`, et `{s:}5` pour un élément d'une structure :
  `{list.S}{{{s:}5} 6 x}`), `type parse` la relit.
* **Variantes et littéraux.** Une variante nommée porte une charge et s'atteint par un chemin ;
  un littéral est sa propre valeur (`type get` d'un littéral donne le type seul, `M`).

| commande | rend |
|---|---|
| `type variant $v` | la variante nommée de la valeur ; vide pour un littéral |
| `type variant $v NAME` | la même charge sous la variante NAME (réétiqueter ; convertir, c'est `type cast S.NAME $v`) |
| `type variant -of T ?NAME?` | les variantes nommées de T, ou le type de la charge de NAME |
| `type literal -of T` | les littéraux de T (les valeurs permises d'une énumération) |
| `type is {sum?}T`, `{enum?}T` | une somme ; une énumération (que des littéraux). Pour une valeur : `type is {sum?}[type get $v]` |

* **Limite** : une somme placée directement dans la charge d'une autre (`opt(S)`) ne garde que
  la variante extérieure ; l'intérieure est redéduite à la lecture.

### `type include`, `num`, `inexact` (v17z)

`type include U T` : toute valeur de T entre dans U sans cast, sans vérification qui puisse
échouer, et reste égale à elle-même (au sens de `==`). La base inclut ses raffinements, ses
nominaux et ses prédicats (`int` ⊇ `int(u,8)`, `double` ⊇ `mm`, `int` ⊇ `pair`), jamais
l'inverse ; deux nominaux sans lien ne s'incluent pas ; une somme inclut ses variantes
(`type include temp temp.celsius`) ; les listes et dicts sont covariants, les liens `var.T`
invariants ; `double` n'inclut que les entiers exacts (`int(s,32)` oui, `int` non).

`num` est intégré : une union de ses représentations, `num.int` et `num.double`, lue comme
`expr` lit un nombre (`{num}5` → `num.int`, `{num}0x10` → 16, `{num}5.0` → `num.double`).

Le passage d'un entier typé en `double` est **exact, ou une erreur** :
`set {double}x {int}9007199254740993` est refusé (2^53 + 1 n'a pas de double). L'arrondi se
demande : `type cast inexact $n`. `inexact` est un nominal de `double`, il y entre. Un texte
nu reste lu comme Tcl lit un nombre.

### Exister, être nul, ne pas exister

Ne pas confondre trois situations pour une valeur pointée par un lien (type `*(...)`, à venir) :
* **présente** : le lien désigne une valeur ;
* **présente mais nulle ou vide** : la valeur existe, elle vaut `0`, `{}` ou `""` ;
* **inexistante** : le lien est nul (offset 0, par exemple), il n'y a rien à lire.

Un défaut de champ (`= {}`) ne distingue pas les deux dernières. Une somme avec une variante
sans valeur (`{{T}some, {}none}`) les distingue. Ce n'est pas encore implémenté, voir la note
de conception.

## Types binaires : lire et écrire des octets (`type binary`, `chan binary`)

Un champ dit, par son TYPE, comment il s'écrit en octets. Rien d'autre à apprendre :
c'est le principe de `bitarray(16)`, qui porte déjà sa largeur. Le `.` ne sert qu'à
COMPOSER (`list(3).T`) ; la représentation est dans les parenthèses du type.

    type def mz {"MZ"}                           ;# une constante : somme d'un littéral
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| type | représentation |
|---|---|
| `int(u,n)` / `int(s,n)` | entier non signé / signé de n bits (multiple de 8, 8 à 1024) |
| `double(32)` `double(64)` | flottant IEEE |
| `string(raw,N)` | N octets bruts |
| `string(ENC,N)` | texte sur N octets, complété par des NUL |
| `string(ENC,z)` | texte terminé par un NUL (2 octets en utf-16, ucs-2) |
| `bitarray(n)` | n bits (multiple de 8), lus comme un entier non signé |
| `"MZ"` (somme de littéraux de même longueur) | une constante, vérifiée par le type |
| `list(N).T`, un produit de ces types | N éléments ; l'imbrication est permise |

`ENC` : `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un texte qui ne
s'encode pas sans perte (`é` en ascii) ou qui ne tient pas dans N octets est refusé.

**Boutisme.** Dernier argument : `be` (big-endian, ordre réseau) ou `le` : `int(u,16,be)`,
`double(64,le)`, `bitarray(16,msb0,be)`. Sans lui, c'est celui de l'appel (`-endian`, little
par défaut). Il ne change pas les valeurs : `int(u,16,be)` et `int(u,16,le)` sont
interchangeables.

**Élargissement automatique.** Un raffinement entre là où l'on attend un type plus large :

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok : tout int(u,8) est un int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok : 0..255 tient dans -32768..32767
    set {int(u,8)}v $n               ;# refusé : rétrécissement (cast explicite)
    set {list.int(u,8)}L $liste_int  ;# refusé aussi pour les listes

Un type défini par `type def port int(u,16)` reste nominal : `port` entre dans un int, mais
un simple `int(u,16)` n'entre pas dans un `port`.

**Alignement.** Par défaut les produits sont compacts (packed), comme les en-têtes réseau et
PE/ELF. `-align natural` (ou `-align N`) ajoute le remplissage du C et de LLVM : chaque champ
commence à un multiple de son alignement (sa taille, plafonnée à 8 ou N), la taille d'un
produit est arrondie à son alignement. Remplissage à zéro à l'écriture, ignoré à la lecture
sauf `-strictpad 1`.

    type binary entete                           ;# la représentation, vue du noyau
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# le canal avance
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

La lecture et l'écriture sont écrites en Tcl (`layout/filebinary.tcl`) au-dessus du noyau : `type binary`
décrit, décode et encode des octets en mémoire, `chan binary` lit et écrit sur un canal. Le nom
`file binary` (l'ancien) reste disponible comme alias.

**`type binary`, interrogations et verbes.** Les questions sur un type s'écrivent par une
annotation locale sur l'argument de type (le `?` final la distingue de tout nom de type) ;
les actions sur des octets restent des verbes :

    type binary entete                          ;# la représentation (forme nue)
    type binary {what?}$T                       ;# idem
    type binary {sizeof?}$T ?-align A?          ;# taille en octets (erreur si variable)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 si $octets est décodable en T, sinon 0
    type binary decode T $octets ...            ;# lit les octets en mémoire
    type binary encode T $valeur ...            ;# idem encode
    type binary deref ...                       ;# idem deref
    type binary space ...                       ;# idem space (référentiels nommés)

`{is?}` accepte par défaut un préfixe valide (reconnaître un en-tête) ; `-exact 1` exige que
les octets soient tous consommés. Une option invalide, un type inconnu ou sans représentation
binaire restent des erreurs : seul l'échec du décodage donne 0. Une annotation inconnue qui se
termine par `?` est une erreur (`unknown question "oups?"`) ; toute autre annotation est
ignorée. Un type qui porte le nom d'un verbe (`type def decode ...`) reste décrit par
`type binary decode`, tant qu'aucun autre argument ne suit. `decode`, `encode`, `deref` et
`space` demandent que `layout/filebinary.tcl` soit chargé (sinon : `the binary layer is not
loaded`).

**Alias courts.** `layout/binaliases.tcl` donne des noms brefs aux représentations usuelles
(convention de Rust et de Zig) : `u8 u16 u32 u64`, `i8 i16 i32 i64`, `f32 f64`, avec les
suffixes `be`/`le` (`u16be`, `i32le`, `f64be`), et `cstring`, `wstringz`, `latin1z`. Ce sont des
`type alias` : la bibliothèque sert aussi de mémo sur les formats binaires.
Les `i8…i64` d'ici sont signés ; les `iN` de LLVM n'ont pas de signe, il n'y a donc pas de
correspondance exacte (les tableaux `[N x T]` et les structures packed correspondent à
`list(N).T` et aux produits).


### Niveau 2 : tailles, nombres, positions et variantes lus dans le fichier

Toujours la même syntaxe : `$champ` dans les arguments du type, comme les paramètres des
types paramétrés, ou un type entier non signé qui sert de préfixe.

| écriture | sens |
|---|---|
| `string(utf-8,int(u,16,be))` | texte précédé de sa taille en octets (chaîne de Pascal) |
| `string(raw,$len)` | N octets, N étant la valeur du champ `len` |
| `string(utf-8,$len - 4)` | une expression Tcl sur les champs déjà lus |
| `list(int(u,8)).T` | liste précédée de son nombre d'éléments |
| `list($n).T` | n éléments, n étant un champ |
| `at($off).T` | la valeur est à la position `$off` du fichier (le curseur ne bouge pas) |
| `corps($tag)` | la somme `corps`, dont la variante est choisie par le champ `tag` |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# noms de variantes = étiquettes
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Règles : un `$champ` doit désigner un champ **déjà lu** du produit courant ou d'un produit
englobant (`type binary` le vérifie) ; un champ produit se lit `$h(off)`. À l'écriture, une
taille ou un nombre qui ne correspond pas au champ dont il dépend est une erreur, et
l'étiquette doit être celle de la variante. `type binary {sizeof?}` refuse un type dont la
taille dépend des données. Exemple complet (PE, ELF, messages) : `exemples/niveau2.tcl`.

Exemples : `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Limites : pas d'unions ni de
champs de bits partagés, pas de liste « jusqu'à la fin du fichier ».

#### Variante choisie : `S(nom)` et paramètre de type

Une somme `S` peut aussi avoir sa variante **choisie à la définition**, sans champ
sélecteur dans le fichier :

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# la variante tp, écrite en toutes lettres
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# ou donnée par un paramètre
type binary decode {R(tp)} $octets
```

`U(tp)` est un type d'une seule variante : sa taille est connue (`type binary {sizeof?}`),
la valeur lue est `U(tp).tp` et il n'y a rien à deviner, même si plusieurs variantes
ont la même forme (ce qui rendait la valeur ambiguë avant). `U($champ)`, lui, garde son
sens : variante choisie par un champ déjà lu. Si `$v` est un paramètre lié du type, c'est
le premier cas qui s'applique. Exemple : `exemples/tcl_obj.tcl` (`Tcl_ObjAs(twoPtrValue)`).
Test : `t36_somme_choisie.tcl`.

#### Le type lien `*(origine,repr).T`

Un champ de fichier contient souvent une adresse, pas la valeur. Le type lien lit l'adresse (`repr`,
un entier) puis la valeur `T` qui s'y trouve ; la valeur du champ est celle de la cible.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Origines, comme dans un système de fichiers : `//` (ou `file://`) début des données, `./` début de la
structure englobante, `../` sa parente, `.` le champ lui-même (voir ci-dessous). Options : `null` (l'adresse 0 signifie
« pas de cible »), `null=V`. Avec `opt(T)` (voir `unit`), une adresse nulle donne `none`, sinon
`some` : c'est la différence entre « nul » et « inexistant ».

Les alias génériques (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) donnent des noms courts :
`ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` dans `layout/binaliases.tcl`. À l'écriture, les cibles
sont ajoutées à la fin, alignées, et les adresses sont calculées. Les types récursifs, `memory://` et les
référentiels nommés sont décrits plus bas. Détails : `noyau/CONCEPTION-lien.md`.

**Le point `.` : l'adresse est relative à l'emplacement du champ.** `./` repère le *début de la
structure* ; `.` repère *le champ lui-même* (l'endroit où l'adresse est écrite). C'est l'origine des
décalages « à partir d'ici », fréquents dans les formats à décalages relatifs (`rel32`) :

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# le champ r est à l'offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# même résultat, mais avec ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Mémo : `//` début des données · `./` début de la structure · `../` début de la structure parente ·
`.` position du champ. La notation `.` reste volontairement courte ; un nom plus parlant n'est
envisagé qu'après usage.

#### Jusqu'où suivre les liens : `-linkdepth`

Un lien peut être suivi tout de suite ou laissé en l'état. L'option d'appel `-linkdepth` règle le
nombre de liens suivis (comme la portée d'un rayonnement : un rayon gamma traverse tout, un rayon
peu pénétrant s'arrête à la peau) :

    type binary decode H $d                   ;# 1 {7 8}   Inf, le défaut : tout suivre
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   un niveau de liens
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      aucun lien suivi
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Un lien non suivi est un entier annoté (position absolue de la cible dans les données). Copie,
`lindex` et `string cat` gardent l'annotation ; un calcul la perd, et le résultat ne peut plus être
suivi. On suit un lien à la demande, un pas à la fois :

    type binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 par défaut : un seul pas)

Ce qu'on en tire : lire seulement l'en-tête d'un gros fichier ; parcourir une liste chaînée ou un
arbre (le type peut être récursif : `type def N {{u8}x; {*(//,u8,null).opt(N)}next}`) ; localiser un
lien abîmé ; modifier un champ sur place, car `type binary encode` réécrit une adresse non suivie
telle quelle au lieu de réorganiser le fichier. Un cycle n'est une erreur qu'avec `Inf`.


## Référentiels nommés / Named spaces

FR : un lien peut désigner un référentiel enregistré par son nom plutôt que les données de l'appel.

```tcl
set sp [type binary space file $octets -name lib]          ;# valeur annotée de type space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segments {adresse offset taille}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ est équivalent
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# pas de DATA : le référentiel suffit
type binary space list | get NOM | delete NOM
```

Le nom par défaut est calculé (`space1`, …) ; la commande de création enregistre le référentiel ; un nom déjà pris est une erreur. `//nom/` exige un référentiel sans segments, `memory://nom/` un référentiel avec segments ; une adresse hors segments est une erreur. On ne peut pas écrire une vraie cible dans un référentiel nommé : seule l'adresse de frontière (`-linkdepth 0`) se réécrit.

EN: a link can name a registered space instead of the call's data (see the example above). The creation command registers it; the default name is computed; duplicate names are errors; real targets cannot be written into a named space, only frontier addresses.

## Écrire un lien nul / Writing a null link

FR : un champ lien qui a une valeur par défaut et dont la valeur est égale à ce défaut s'écrit comme un lien nul (l'adresse « nulle » du lien, aucune cible n'est écrite). La lecture fait l'inverse : adresse nulle → valeur par défaut. Sans défaut (et sans `opt`), lire un lien nul reste une erreur.

EN: a link field with a default whose value equals that default is written as a null link (the null address, no target written). Reading does the reverse. Without a default (and without `opt`), reading a null link is still an error.

## -dedupe : partager les cibles identiques / sharing identical targets

FR : `type binary encode TYPE VALEUR -dedupe 1` (aussi `write`) n'écrit qu'une fois les cibles de liens dont les octets sont identiques ; les liens concernés reçoivent la même adresse. Par défaut (`-dedupe 0`) chaque cible est écrite. Seules les cibles sans lien imbriqué sont mises en commun (les adresses internes d'une cible dépendent de sa position) ; la relecture donne la même valeur.

EN: `-dedupe 1` writes link targets with identical bytes once and gives the links the same address. Default is off. Only targets holding no nested link are shared; reading back gives the same value.

## Taille dépendante d'un champ : vérifiée à la construction / size depending on a field: checked at construction

FR : une liste dont le nombre d'éléments est une expression sur d'autres champs (`{list($n).Sample}items`,
`list($n - 1).u8`) est contrôlée dès la construction du produit : `set {Batch}b(n,items) 3 {{1 1.0 0}}`
est refusé (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`). Les champs omis
comptent avec leur valeur par défaut. Le contrôle ne couvre pas les affectations champ par champ
(`set b(n) 3` : un état intermédiaire est légitime) ; `type binary encode` revérifie à l'écriture.
Les tailles de chaîne en octets (`string(raw,$len)`) ne sont pas contrôlées à la construction.

EN: a list whose element count is an expression over other fields is checked as soon as the product
is built; omitted fields count with their default. Field-by-field assignments are not checked
(an intermediate state is legitimate); `type binary encode` checks again when writing. Byte sizes of
strings (`string(raw,$len)`) are not checked at construction.

## Opérations spécialisées / Specialised operations

Un type est connu dès sa définition : les opérations fréquentes n'ont pas à
le retrouver dans chaque valeur. Chaque type peut porter une table
d'opérations spécialisées, choisie dans un registre : intégrée (listes de
nombres, listes de produits) ou fournie par une bibliothèque chargée.

    type ops list.double        ;# list-scalar
    type ops pts                ;# list-product
    load ./libproduits.so Produits
    type ops list.pt            ;# produits-ordinaires (priorité 10)

Une spécialisation ne fait qu'aller plus vite : même valeur, même texte, même
type. `TCL_TYPE_OPS=off` les coupe toutes ; `TCL_TYPE_OPS=check` exécute
aussi le chemin général et compare (un écart est écrit sur stderr). Le
bytecode s'en sert pour `$P($i.champ)` (le champ lu sans former l'index), et
`{double}x`, `{int}x` reçoivent un résultat d'`expr` dans leur propre valeur
quand la variable est seule à la tenir. Exemple de bibliothèque :
`exemples/ops/` (`tclTypesOps.h`, `make`, `test_ops.tcl`).

## type map : une expression appliquée à chaque élément

    set d [type map {x y z} $P(*.(a,b,c)) {($x + $y * $z)}]
    set s [type map {x} $L {k} $poids {($x * $k)}]

Les listes avancent ensemble (même longueur) ; une liste de K variables
prend une liste de K colonnes, ce que rend un chemin à plusieurs champs. Le
corps est une expression entre parenthèses (la forme de la TIP 759). Faite
de nombres, de variables, d'arithmétique, de comparaisons, de `&&`, `||`,
`!`, `?:` et des fonctions mathématiques usuelles, elle est compilée une
fois et évaluée en C, sans objets ; sinon (ou pour un élément qui
déborderait, diviserait par zéro...) par `expr` : le résultat est toujours
celui d'`expr`. Comme `lmap`, les variables gardent le dernier élément. Avec `-sum`,
la somme des résultats (comme `::tcl::mathop::+`) ; avec `-min`, `-max`, le plus
petit, le plus grand (comme `::tcl::mathfunc::min`, `max`). Le
résultat est une `list.double` ou une `list.int` en cases quand tous les
résultats en sont. Quand l'expression ne calcule qu'en flottants, elle est
évaluée par blocs de 256 éléments : les valeurs de chaque colonne sont
d'abord rassemblées dans un petit tableau (cases, vues ou objets), puis
chaque opération est appliquée au bloc entier ; un élément qui n'est pas un
flottant simple, ou dont un calcul n'est pas fini, est repris seul par le
chemin ordinaire (le résultat reste celui d'`expr`). `TCL_TYPE_NOBLOCK=1`
désactive les blocs, pour comparer. De 10 000 à 100 000 éléments : 3 à 5 ns
par élément en cases, 9 à 10 ns en objets, contre 100 à 150 ns avec `lmap` ;
un million : 7 à 8 ns en cases, 17 à 20 en objets.

**Cumul.** `-fold {s init}` : `s` part de `init`, et le corps donne sa
nouvelle valeur à chaque élément ; la commande rend la dernière (et la
laisse dans `s`), comme

    set s init; foreach u $U v $V { set s [expr {corps}] }

mais sans repasser par l'interpréteur :

    type map -fold {s 0.0} {u} $U {v} $V {($s + $u*$v)}     ;# produit scalaire
    type map -fold {s 0.0} {u} $U {(0.9*$s + 0.1*$u)}       ;# lissage

Le résultat est celui de la boucle, au bit près : les opérations se font
dans le même ordre (pas de sommes partielles parallèles). Quand le sommet du
corps est une opération binaire dont un côté ne lit que `s` (et des valeurs
fixes) et l'autre pas `s`, cet autre côté est calculé par blocs, et seule la
combinaison reste élément par élément, en boucle serrée pour `s ⊕ e` et
`c*s ⊕ e` (⊕ : `+ - *`). Sinon (un `?:`, une fonction de `s` mêlée aux
éléments), le corps est évalué élément par élément en C, ou par `expr`.
Un million d'éléments : produit scalaire et lissage 2,5 à 3 ns par élément
en cases, 8 à 13 en objets, contre 80 ns pour la boucle ; un `?:` 25 ns.

**Sur place : `-in`.** `type map -in VAR(CHEMIN) varList ?varList liste ...?
corps` calcule comme `type map`, la première `varList` liée à ce que
désigne le chemin dans la variable, et écrit le résultat au même endroit :
une valeur par colonne du chemin (un tuple pour une fenêtre). La commande
rend la nouvelle valeur de la variable, comme `lset` ou `ledit`.

    type map -in pl((0,1)::2) {x y} {($x + $tx, $y + $ty)}      ;# translation
    type map -in pl((0,1)::2) {x y} {($c*$x - $s*$y, $s*$x + $c*$y)}
    type map -in L {x} {($x * 2)}                                  ;# toute la liste
    type map -in L(1::2) {y} {k} $coef {($y * $k)}                 ;# avec une autre liste

* Le type de la sortie est celui de la cible : chaque valeur est rangée
  comme par une affectation (validation, cases, prédicats). Une annotation
  du corps (`{T}{(…)}`) n'est qu'une déclaration, vérifiée une fois : `T`
  doit entrer dans le type des éléments de la cible.
* Une signature d'entrée reste permise (pour la cible, son type doit y
  entrer ; pour les listes ajoutées, elle vérifie comme dans `type map`).
* Tout ou rien : les colonnes sont calculées, puis écrites d'un coup.
* Avec un chemin, la variable doit être typée (comme pour `$v(chemin)`) ;
  sans chemin, toute variable.
* Une liste en cases partagée (un paramètre) est copiée une fois, ou pas du
  tout quand le chemin couvre chaque case (`(0,1)::2` sur une liste paire) ;
  les résultats sont écrits à leur pas dans les cases.

**foreach et lmap vectorisés.** `foreach` et `lmap` demandent, à chaque
appel, à un registre de boucles vectorisables si une entrée sait faire cet
appel-là à l'identique ; sinon, la boucle ordinaire. L'entrée intégrée
s'appuie sur `type map` et n'accepte que :

    lmap u $U v $V {($u*2 + $v)}              ;# ou {expr {$u*2 + $v}}
    foreach u $U v $V {(s = $s + $u*$v)}      ;# ou {set s [expr {...}]} : -fold
    foreach v $L {incr c $v}                  ;# ou {incr c}, {incr c 5}, {incr c [expr {...}]}

avec une variable par liste (ni tracée, ni typée, ni tableau), des listes de
même longueur (au moins 16 éléments), un corps fait d'une expression pure
(le sous-ensemble compilé de `type map`), `expr` et `set` intégrés, et, pour
le cumul, `s` existante. Le résultat, les erreurs (message et `errorInfo` :
en cas d'erreur, la boucle ordinaire est refaite depuis le début, le corps
étant pur) et les variables après la boucle sont ceux de la boucle
ordinaire. `(s = E)` garde un opérande tel quel quand `E` en rend un (`$u`,
`?:`, `min`, `max`...), sans le normaliser comme `expr` : ces corps-là
restent à la boucle ordinaire. `incr s ?E?` est un cumul d'entiers : `s`
entière au départ, `E` un entier écrit, `$v` ou `[expr {…}]` ; si une
valeur n'est pas entière, la boucle ordinaire refaite donne l'erreur
d'`incr`. `TCL_TYPE_NOVECLOOP=1` désactive le registre.

Le registre sert aussi aux boucles compilées en ligne (corps littéral dans
une procédure) : quand le corps a l'une de ces formes, la compilation place
devant la boucle un appel à `::tcl::unsupported::vecloop`, qui rend
`{1 résultat}` ou `{0 L1 L2 …}` (les listes, déjà évaluées, pour la boucle
ordinaire, qui suit). Les autres boucles ne changent pas. Un million
d'éléments : `foreach v $s {incr c $v}` 34 → 2,5 ms (octets en cases),
`foreach v $s {(c=$c+$v)}` 44 → 2,4 ms, `lmap v $s {($v*2)}` 73 → 19 ms,
produit scalaire `set d [expr {$d + $u*$v}]` sur deux listes ordinaires
84 → 10 ms ; une boucle courte (moins de 16 éléments) ne coûte rien de
mesurable de plus.

**Où est la falaise : `-report` et `mapreport`.** Les voies rapides se
retirent sans rien dire quand elles ne peuvent pas rendre le même résultat ;
le calcul reste juste, mais beaucoup plus lent. Deux moyens de voir ce qui
s'est passé (v17y).

`type map -report varName …` (en première option) fait le calcul comme
d'habitude et range dans `varName` un dictionnaire, même si la commande
échoue :

    type map -report r -in pl((0,1)::2) {x y} {($c*$x - $s*$y, $s*$x + $c*$y)}
    # elements 100000
    # inputs   x {cells double(64) stride 16} y {cells double(64) stride 16}
    # body     {compiled double} {compiled double}
    # pass     single
    # blocks 200000  elementwise 0  expr 0
    # write    direct
    # why      {}

* `inputs` : chaque variable et la forme de sa colonne (`cells …` avec le
  pas en octets, `objects`, `objects view stride N`).
* `body` : pour chaque composante, `compiled double`, `compiled int`,
  `compiled double int`, ou `expr: …` avec la raison (`not compiled near
  "foo($y)"`, `variable "k" is not a number`).
* `pass` : `single` (le tuple en une passe), `per-component`, `tuple by
  expr` (le corps entier, élément par élément), `column`, `fold`, `reduce`.
* `blocks`, `elementwise`, `expr` : les évaluations faites par blocs, une à
  une en C, et par `expr` (une par élément et par composante).
* `write` (avec `-in`) : `direct` (bloc par bloc dans des cases neuves),
  `direct (copied first)`, `columns into cells`, `columns into objects`,
  `set (by the path, each value checked)`.
* `why` : la première raison pour laquelle une voie plus rapide n'a pas
  servi (`single pass: element 3 is not a double in every input`, `direct
  write: target: overlapping columns`, `element 3: x = "abc" is not a
  number, by expr`), vide si rien ne s'est retiré.

`::tcl::unsupported::mapreport on|off|clear|get` tient le même relevé pour
tout un programme, par lieu d'appel (procédure et ligne, ou fichier et
ligne) : `type map`, et les `foreach`/`lmap` candidats du registre, avec
la raison d'un refus (`declined: short lists`, `declined: the expression
is not in the compiled subset`, `declined: an element errs: the ordinary
loop, from the start`…). `get` rend un dictionnaire lieu → statistiques
(`calls`, `pass …`, `elements`, `blocks`, `elementwise`, `expr`,
`write …`, le premier `body` et le premier `why` vus à ce lieu) :

    ::tcl::unsupported::mapreport on
    run-my-program
    dict for {where st} [::tcl::unsupported::mapreport get] {puts "$where: $st"}
    # ::normalize line 3: command {type map} calls 200 {pass column} 200 ...
    #     body {{expr: variable "k" is not a number}}

Une boucle qui n'a pas la forme d'une boucle vectorisable n'est pas
relevée (elle ne l'a jamais été). Une boucle compilée en ligne (corps
littéral dans une procédure) dont l'expression sort du sous-ensemble est
compilée en boucle ordinaire dès la compilation : elle n'est pas relevée
non plus ; `type map -report` sur la même expression dit pourquoi. Sans
relevé demandé, le coût est un test par appel.

Avec `-sum`, `-min` et `-max`, des résultats entiers restent en cases
(entiers de 64 bits) le temps du calcul : `type map -sum {v} $octets {($v)}`
40 → 4,7 ms pour un million. `type map` lui-même rend toujours une
`list.int` d'objets (un `int` peut être un grand entier : il n'a pas de
forme en cases) ; les petits entiers y sont partagés (23 → 12 ms).

Une tranche d'une liste en cases remplie d'une même valeur
(`set s($j:end:$i) 0`) est vérifiée et codée une fois, puis recopiée de
case en case : le crible d'Ératosthène sur un million d'octets en cases
prend 2,5 ms en tout (créer 0,0, cribler 2,3, compter 0,2 : `s + $v`
sur des entiers de 8 à 32 bits en cases se somme d'un trait), contre 125 à
180 ms pour une liste ordinaire et `lset` ; comparaison avec d'autres
langages : `exemples/crible/COMPARAISON.md`.

**type repeat.** `type repeat T N ?valeur?` rend une liste `list.T` faite
de `N` fois la valeur ; `type repeat -into L N ?valeur?` rend une valeur du
type de liste `L` (son élément, sa longueur fixe vérifiée) :

    set s [type repeat int(u,8) $N 1]          ;# 0,1 ms pour un million
    type repeat double 3                       ;# 0.0 0.0 0.0
    type repeat pt 2 {1 2}                     ;# {1.0 2.0} {1.0 2.0}
    type repeat list.int 3 {1 2}               ;# list<list<int>>
    type def {-compact}octets list.int(u,8)
    set s [type repeat -into octets $N 1]      ;# de type octets, sans cast

Sans `-into`, le premier argument est toujours le type de l'élément (comme
`lrepeat`, qui prend un élément) ; avec, le type de la liste à remplir.
La valeur est vérifiée une fois contre l'élément. Le résultat est toujours
rangé en cases quand l'élément en a une forme (entiers et flottants de
taille fixe, produits de ceux-ci), sans qu'il faille `-compact` : la
valeur est codée dans la première case et les autres sont remplies d'un
bloc, sans liste intermédiaire ; sinon la liste partage le même objet.
Sans valeur : le zéro de l'élément (`0`, ou `0.0` pour un flottant) ; une
valeur est demandée pour un autre élément (chaîne, produit).
`type cast` d'une liste qui répète le même objet (`lrepeat`) code de même
l'élément une fois.

**Typé.** Les variables peuvent se déclarer comme les paramètres d'une
procédure, et le corps peut dire ce qu'il rend :

    type map {{double}x; {double}y} $pl((0,1)::2) \
        {{double}u; {double}v}{($c*$x - $s*$y, $s*$x + $c*$y)}
    type map {{double}x} $L {double}{($x * 2)}
    type map {{double}x; {double}y} $cols {{double}y; {double}x}{}

- Une signature d'entrée (un `{T}nom` ou un `;`) : ses champs sont les
  variables ; chaque colonne est vérifiée, ou rangée dans `list.T`, une
  fois à l'entrée, comme un argument (`type map: column "x": expected
  floating-point number but got "abc"`). Le calcul suit alors les types
  déclarés : avec `{double}`, `$x / 2` divise en flottants, et `$x / 0.0`
  vaut `Inf` comme dans `expr`.
- Un corps annoté `{T}{(…)}` : `T` est le type de ce que le corps rend à
  chaque élément. Un scalaire : une colonne `list.T` (avec `-sum`, `-min`,
  `-max`, un `T`). Un produit écrit en ligne (`{{double}u; {double}v}`) :
  une colonne par champ, que le tuple du corps remplit dans l'ordre, prête
  pour `set out((0,1)::2) {*}[…]`. Un produit nommé (`{point}`) : une
  `list.point`. Chaque colonne est rangée dans le type de son champ (des
  doubles dans un `{int}` : `type mismatch`, comme une affectation).
- Un corps vide annoté `{T}{}` : la projection, par les noms ; les champs
  de `T` sont pris parmi les variables d'entrée (choisis, réordonnés,
  convertis ; pas renommés : un nom absent est une erreur).
- Sans annotation nulle part : `type map` comme avant (le sens d'`expr`).

Les composantes d'un tuple sont compilées chacune (et calculées par blocs)
quand elles le peuvent ; sinon le corps entier est évalué par `expr`,
élément par élément, puis ses tuples rangés en colonnes. Quand toutes les
composantes se calculent en flottants (entrées en cases ou en objets
doubles, valeurs fixes flottantes, aucun résultat non fini), le tuple se
fait en une passe : chaque bloc de 256 éléments est lu une fois, puis les K
expressions tournent dessus, chacune dans sa colonne (v17x). Sinon, une
passe par composante, avec le même résultat. Avec `-in` sur une liste en
cases de doubles, chaque bloc est écrit aussitôt à son pas dans des cases
neuves, installées à la fin (rien n'a changé en cas d'erreur). La rotation
d'une polyligne de 100 000 sommets en `type map -in` : 0,98 → 0,36 ms.

Exemple : `exemples/tcllib_geometry/vectorise.tcl` réécrit l'aire, la
longueur, la translation, la rotation et la boîte englobante d'une
polyligne (liste plate x1 y1 x2 y2 ...) avec des fenêtres
(`$pl((0,1,2,3)::2)`, `$pl((0,1)::2)`), `type map -sum` et l'écriture de
colonnes. Coordonnées double, 100 000 sommets, par rapport à geometry.tcl
typé : aire ×5,9, longueur ×7,1, translation ×2,8, rotation ×3,9, boîte
×3,2 ; en `num` : ×4,6, ×4,0, ×2,0, ×1,9, ×2,4.
`exemples/tcllib_geometry/COMPARAISON.md` compare la chaîne entière en dix
variantes (Tcl nu, typé scintillant ou de bout en bout, en Tcl, vectorisé
ou en C, en objets ou en cases), un principe par variante : ×0,5 pour le
Tcl typé élément par élément, ×33 pour le Tcl typé vectorisé en cases, ×45
pour le C en cases.

## Formes d'un type : `type def T DÉF {-compact}F`

    type def poly list.double {-compact}polyc

`poly` et `polyc` sont le même type sous deux dispositions (objets Tcl, ou
cases contiguës pour le C). Une forme entre telle quelle là où le type est
attendu (`{poly}p`, `set {poly}x`, réaffectation) ; entrer dans la forme est
un contrat écrit : `{polyc}p` refuse un `poly` (« use an explicit cast »),
et `type cast` passe de l'une à l'autre sans règle. Exemple de bout en bout :
`exemples/tcllib_geometry/e2e.tcl` (polygone typé une fois en `polylinec`,
puis translation, rotation, aire, longueur, boîte : environ 6 fois plus
rapide qu'en objets pour 100 000 sommets).

Vues : une tranche à pas d'une liste en cases (`$L(1::2)`) ou une colonne
d'une liste en cases de produits (`$P(*.x)`) est une vue sur les mêmes
cases (base, début, pas, nombre), sans copie ; la base partagée est copiée
avant toute écriture. `TCL_TYPE_NOVIEW=1` les remplace par des copies (pour
comparer).
