# Manuel des types annotés — chapitres

*(Ce fichier accueille les chapitres du manuel au fur et à mesure ; les notes
de patch restent dans `noyau/LISEZMOI.md`.)*

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
| construction nom/valeur | `set {pt}a nom Jean age 12` | `set {fiche}e nom Jean` |
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

### Le piège de l'ambiguïté

Un produit de *n* champs appelé avec *n* arguments (n pair) peut se lire en
positionnel ou en nom/valeur. Si les deux lectures sont valides, c'est une
erreur (`ambiguous arguments for T … name every field`) ; si une seule l'est,
on la prend. Nommer tous les champs lève toujours le doute.

### Choisir

* Un ensemble de champs **connu et stable**, avec des types variés : produit.
* Des clés **libres ou facultatives**, de même type : dict.
* Un format avec plusieurs formes possibles (PNG, GIF…) : une **somme**
  de produits (voir `exemples/png_somme.tcl`).

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
    set {mix}s1 {b}A
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
* Il n'y a **pas d'`union` dans ce modèle**, volontairement : on garde les garde-fous. Une
  somme ne peut pas être lue « de travers ».
* Quand le format ne dit pas quelle variante est valide (la `Tcl_ObjInternalRep` de Tcl, dont
  le discriminant est l'adresse `typePtr`, extérieure à l'union), on choisit la variante
  **à l'appel** : `Tcl_ObjAs(twoPtrValue)` (voir `exemples/tcl_obj.tcl`). C'est l'appelant qui
  sait, comme en C, mais le type reste vérifié.
* Une somme dont plusieurs variantes ont la **même forme** n'est pas ambiguë dès que la variante
  est choisie par son nom (`S(y)`) : il n'y a plus rien à deviner.

### Exister, être nul, ne pas exister

Ne pas confondre trois situations pour une valeur pointée par un lien (type `*(...)`, à venir) :
* **présente** : le lien désigne une valeur ;
* **présente mais nulle ou vide** : la valeur existe, elle vaut `0`, `{}` ou `""` ;
* **inexistante** : le lien est nul (offset 0, par exemple), il n'y a rien à lire.

Un défaut de champ (`= {}`) ne distingue pas les deux dernières. Une somme avec une variante
sans valeur (`{{T}some, {}none}`) les distingue. Ce n'est pas encore implémenté, voir la note
de conception.

## Types binaires et `file binary` : lire et écrire des fichiers

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
    file binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    file binary encode entete $valeur ?-endian E? ?-align A?
    file binary read  $canal entete ?-endian E? ?-align A?   ;# le canal avance
    file binary write $canal entete $valeur ?-endian E? ?-align A?
    file binary sizeof entete ?-align A?

`file binary` est écrit en Tcl (`layout/filebinary.tcl`) au-dessus de `type binary` ; la
même table de sous-commandes pourra servir à `chan binary`.

**`type binary`, interrogations et verbes.** Les questions sur un type s'écrivent par une
annotation locale sur l'argument de type (le `?` final la distingue de tout nom de type) ;
les actions sur des octets restent des verbes :

    type binary entete                          ;# la représentation (forme nue)
    type binary {what?}$T                       ;# idem
    type binary {sizeof?}$T ?-align A?          ;# taille en octets (erreur si variable)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 si $octets est décodable en T, sinon 0
    type binary decode T $octets ...            ;# comme « file binary decode »
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
loaded`). `file binary` reste disponible comme alias pendant la transition.

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
l'étiquette doit être celle de la variante. `file binary sizeof` refuse un type dont la
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
file binary decode {R(tp)} $octets
```

`U(tp)` est un type d'une seule variante : sa taille est connue (`file binary sizeof`),
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
    file binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# même résultat, mais avec ./
    file binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Mémo : `//` début des données · `./` début de la structure · `../` début de la structure parente ·
`.` position du champ. La notation `.` reste volontairement courte ; un nom plus parlant n'est
envisagé qu'après usage.

#### Jusqu'où suivre les liens : `-linkdepth`

Un lien peut être suivi tout de suite ou laissé en l'état. L'option d'appel `-linkdepth` règle le
nombre de liens suivis (comme la portée d'un rayonnement : un rayon gamma traverse tout, un rayon
peu pénétrant s'arrête à la peau) :

    file binary decode H $d                   ;# 1 {7 8}   Inf, le défaut : tout suivre
    file binary decode H $d -linkdepth 1      ;# 1 {7 8}   un niveau de liens
    set h [file binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      aucun lien suivi
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Un lien non suivi est un entier annoté (position absolue de la cible dans les données). Copie,
`lindex` et `string cat` gardent l'annotation ; un calcul la perd, et le résultat ne peut plus être
suivi. On suit un lien à la demande, un pas à la fois :

    file binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 par défaut : un seul pas)

Ce qu'on en tire : lire seulement l'en-tête d'un gros fichier ; parcourir une liste chaînée ou un
arbre (le type peut être récursif : `type def N {{u8}x; {*(//,u8,null).opt(N)}next}`) ; localiser un
lien abîmé ; modifier un champ sur place, car `file binary encode` réécrit une adresse non suivie
telle quelle au lieu de réorganiser le fichier. Un cycle n'est une erreur qu'avec `Inf`.


## Référentiels nommés / Named spaces

FR : un lien peut désigner un référentiel enregistré par son nom plutôt que les données de l'appel.

```tcl
set sp [file binary space file $octets -name lib]          ;# valeur annotée de type space
file binary space memory $octets {{4096 4 2}} -name mem    ;# segments {adresse offset taille}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ est équivalent
type def M {{*(memory://mem/,u32le,null).rec}p}
file binary deref $h0(r)                                    ;# pas de DATA : le référentiel suffit
file binary space list | get NOM | delete NOM
```

Le nom par défaut est calculé (`space1`, …) ; la commande de création enregistre le référentiel ; un nom déjà pris est une erreur. `//nom/` exige un référentiel sans segments, `memory://nom/` un référentiel avec segments ; une adresse hors segments est une erreur. On ne peut pas écrire une vraie cible dans un référentiel nommé : seule l'adresse de frontière (`-linkdepth 0`) se réécrit.

EN: a link can name a registered space instead of the call's data (see the example above). The creation command registers it; the default name is computed; duplicate names are errors; real targets cannot be written into a named space, only frontier addresses.

## Écrire un lien nul / Writing a null link

FR : un champ lien qui a une valeur par défaut et dont la valeur est égale à ce défaut s'écrit comme un lien nul (l'adresse « nulle » du lien, aucune cible n'est écrite). La lecture fait l'inverse : adresse nulle → valeur par défaut. Sans défaut (et sans `opt`), lire un lien nul reste une erreur.

EN: a link field with a default whose value equals that default is written as a null link (the null address, no target written). Reading does the reverse. Without a default (and without `opt`), reading a null link is still an error.

## -dedupe : partager les cibles identiques / sharing identical targets

FR : `file binary encode TYPE VALEUR -dedupe 1` (aussi `write`) n'écrit qu'une fois les cibles de liens dont les octets sont identiques ; les liens concernés reçoivent la même adresse. Par défaut (`-dedupe 0`) chaque cible est écrite. Seules les cibles sans lien imbriqué sont mises en commun (les adresses internes d'une cible dépendent de sa position) ; la relecture donne la même valeur.

EN: `-dedupe 1` writes link targets with identical bytes once and gives the links the same address. Default is off. Only targets holding no nested link are shared; reading back gives the same value.

## Taille dépendante d'un champ : vérifiée à la construction / size depending on a field: checked at construction

FR : une liste dont le nombre d'éléments est une expression sur d'autres champs (`{list($n).Sample}items`,
`list($n - 1).u8`) est contrôlée dès la construction du produit : `set {Batch}b n 3 items {{1 1.0 0}}`
est refusé (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`). Les champs omis
comptent avec leur valeur par défaut. Le contrôle ne couvre pas les affectations champ par champ
(`set b(n) 3` : un état intermédiaire est légitime) ; `file binary encode` revérifie à l'écriture.
Les tailles de chaîne en octets (`string(raw,$len)`) ne sont pas contrôlées à la construction.

EN: a list whose element count is an expression over other fields is checked as soon as the product
is built; omitted fields count with their default. Field-by-field assignments are not checked
(an intermediate state is legitimate); `file binary encode` checks again when writing. Byte sizes of
strings (`string(raw,$len)`) are not checked at construction.
