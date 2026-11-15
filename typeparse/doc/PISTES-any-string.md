# Pistes : valeur nue, méta-types, `any` (13 novembre 2026)

Notes de discussion, nées de l'usage de la TIP dans VolɷDuctus (son
`JOURNAL-TIP764.md`, points 1, 2, 6, 18, 20, 24, 25). **Rien n'est décidé.**
Sondes : `doc/any/sondes.tcl`, sortie sur la version actuelle dans
`doc/any/sondes.out`.

*Réécrite le 13 novembre après-midi : la première version proposait de
renommer `any` ; la discussion a montré que `any` est juste et que ce qui
manquait, ce sont les méta-types.*

## 1. Le constat

Une valeur porte, à côté de son contenu, deux choses (ANNOTATIONS §2.1–2.2) :
au plus un **type résolu**, et une **pile de notes** (des proto-types).
D'où deux axes indépendants, et quatre états :

```tcl
set f 3              ;# nue            : ni type, ni note ; elle scintille
set {int}b 3         ;# typée          : int
set a {mm}3          ;# notée          : la note mm, pas de type
set {int}g {mm}3     ;# typée et notée : int, et la note mm
```

Le C les confond. `TclAnnotInfo` (tclAnnotObj.c) rend trois cas : « 0 : pas
annoté ; 1 : typé ; 2 : brut ».

* « typé » et « typé et noté » tombent tous deux dans le cas 1 : les notes
  sont perdues pour qui ne regarde que ce code. Les constructeurs de listes
  les effacent : `set {list}P 5 {this}6` rend deux éléments sans note ;
  `{list.double}` aussi.
* `any` n'est pas aujourd'hui « indifférent » : il **remplace** le type.
  `type cast any $g` (g : int, note mm) rend une valeur de type `any` (le
  `int` est perdu, la note gardée) ; dans `set {list.any}P 5 {this}6`,
  l'élément nu devient de type `any`. Avec § 3, `any` n'étant que l'union
  des quatre méta-types, le cast vers `any` est l'identité.
* une valeur seulement notée voit sa note la plus récente « lue comme un nom
  à la consommation » : si elle nomme un type, la valeur est traitée comme
  typée. La frontière se décide en route au lieu d'être un état connu.

Et `set`, `type cast` et le passage d'argument ne tranchent pas pareil (voir
§ 6).

## 2. Trois axes : état, et exigence

L'hésitation sur `any` venait de ce qu'un même mot servait pour un **état**
(la valeur nue) et pour une **exigence** (ce qu'un type pose sur ce qu'il
reçoit). Sur chaque axe de l'état (type, notes), une exigence prend trois
positions :

| | exige | indifférent (garde) | retire |
|---|---|---|---|
| **type** | `T`, ou `typed` (un type quelconque) | `any` | `bare`, ou `{}` sur une valeur |
| **notes** | `noted`, `typednoted` | (par défaut) | `note delete` |

## 3. Les méta-types

*(Faits en v17e, §§ 3–5 sauf `{T@notes}` (étape 6) et `type fix` (plus
tard). Une précision : un méta-type est une exigence partout, constructeurs
compris — `set {list.bare}L` refuse un élément typé au lieu de le rendre nu ;
rendre nu reste `{}`. `set {typed}v` ne pose pas de contrat. Voir
`noyau/LISEZMOI.md` et ANNOTATIONS § 2.6.)*

Quatre méta-types, disjoints par construction :

| méta-type | type résolu | notes |
|---|---|---|
| `bare` | non | non |
| `typed` | oui | non |
| `noted` | non | oui |
| `typednoted` | oui | oui |

**`any` est leur somme** — l'absence d'exigence n'est pas un primitif, c'est
l'union des quatre états :

```tcl
type def any {bare | typed | noted | typednoted}
```

Une union est permise puisqu'ils sont disjoints ; d'autres unions ont un
sens (`type def annotee {noted | typednoted}` : au moins une note, typée ou
non). Les deux emplois actuels de `any` dans la TIP sont alors cohérents :
`list.any` (des éléments qui gardent ce qu'ils portent) et `{any}a`, l'argument
d'une procédure classique (« an argument receives typed values without
becoming strict », 764.md).

**Pas d'effet d'observateur à la frontière d'un constructeur.** Un type ne
peut pas tester l'état d'une valeur en la castant vers une variante (le cast
la typerait). Mais une commande qui reçoit un mot voit le `Tcl_Obj` tel qu'il
arrive, *avant* d'agir : elle peut vérifier un méta-type. Les méta-types
servent donc d'exigences :

```tcl
proc f {{bare}x} …         ;# n'accepte qu'une valeur nue
proc g {{typed}x} …        ;# une valeur typée, de n'importe quel type
```

**Côté C**, une énumération à la place du 0/1/2 de `TclAnnotInfo` :

```c
typedef enum {
    TCL_META_BARE,        /* nue : aucun type, aucune note ; elle scintille */
    TCL_META_TYPED,       /* typée : un type résolu, fixé                  */
    TCL_META_NOTED,       /* notée : des notes, pas de type (proto-type)   */
    TCL_META_TYPED_NOTED  /* typée et notée                                */
} TclMeta;
```

Chaque fonction de réception dirait ce qu'elle fait de chacun. Pour
`TclTypeCast` : BARE, le lecteur natif puis la règle depuis la valeur nue ;
NOTED, résoudre ou non la note (décision explicite), puis comme BARE ; TYPED,
même type, variante exacte, puis règle ; TYPED_NOTED, comme TYPED, et les
notes sont transmises. Les corrections des § 5–7 en découlent.

## 4. Observer : `type meta`

`type meta $v` rend la variante du méta-type, **annotée** de ce qu'on sait du
type :

```tcl
set m [type meta $v]
type variant $m      ;# bare, typed, noted ou typednoted
note get $m          ;# le type (valeur typée), ou la représentation (valeur nue)
```

* une valeur typée : `{int}typed` (et `typednoted`, ses notes à elle à part) ;
* une valeur **nue** : sa représentation, le **nom de son `Tcl_ObjType`**,
  quel qu'il soit, tel qu'il est (`typePtr->name`) : `{int}bare`,
  `{list}bare`, `{dict}bare`, `{bytearray}bare`, mais aussi `{cmdName}bare`,
  `{index}bare`, `{assemblecode}bare`, `{dictIterator}bare`… Une chaîne pure
  (`typePtr == NULL`) donne `bare` sans note — à ne pas confondre avec
  `{string}bare`, le type `string` de Tcl (la chaîne indexée).

C'est la version intégrée de `tcl::unsupported::representation`, et pour la
même raison : la représentation porte l'**intention** de la commande qui l'a
produite (`lappend` : une liste ; `incr` : un entier), et des commandes la
défont en passant, selon des heuristiques locales invisibles. Voir la
représentation permet de choisir, parmi les écritures d'un script, celle qui
convient au moteur.

Exemples (vérifiés, Tcl 9 de cette branche) :

1. `set x 123` (chaîne pure), puis `incr x 0` (int) : même texte, deux
   histoires ; un encodeur JSON qui regarde la représentation rendrait
   `"123"` puis `123`.
2. `set d {a 1 a 2}` : `llength` 4 (list) ou `dict size` 1 (dict), même
   texte ; `lindex $d 1` vaut 1, `dict get $d a` vaut 2.
3. `foreach i {1 2 3} {lappend L $i}` (list) ; `append L " 4"` (string) ;
   `lindex $L 0` (list, tout le texte relu) : en boucle, la lenteur
   quadratique classique.
4. `binary format c3 {65 66 67}` (bytearray) : rester binaire.
5. `set h 0x10; expr {$h + 0}` : int, mais le texte reste `0x10`
   (`$h eq 16` faux, `$h == 16` vrai).

Limites :

* les noms de `Tcl_ObjType` sont **internes et changent** d'une version à
  l'autre : `type meta` rapporte la représentation *du moment, dans ce Tcl* ;
* certains ne se relisent pas comme annotation (`end-offset`, `clock-format`,
  `str-idx-tree` contiennent `-` ; `TclOO method name` des espaces). La note,
  posée par le C, se range telle quelle et `note get` la rend exactement ;
  seul son affichage ne se relit pas ;
* `annotated` n'apparaît jamais (une valeur annotée n'est pas nue).

**Forcer** le type est un acte explicite : `set {int}i $i`, ou un raccourci
`type fix $v` qui scelle la représentation du moment en type résolu — pour
les représentations qui ont un type de la TIP. Jamais automatiquement : on
recréerait le problème des variables qu'un type préexistant empêche de
réemployer.

**L'inventaire.** `type debug notes` et `type debug report` recensent déjà
les notes ; ils pourraient recenser aussi les représentations des variables
nues : « `i` : toujours int (124 lectures) → `{int}i` ; `L` : list, mais
string 3 fois (ligne 42, `append`) ». Le rapport désignerait les types à
déclarer et les endroits où le script fait scintiller ses valeurs : un pont
entre un script Tcl ordinaire et un script typé. (La dernière conversion
n'est pas toujours l'intention — `if {$nom == 1}` convertit par `expr` ; le
compte des lectures compense.)

## 5. Conteneurs

L'exigence porte sur les éléments (les valeurs d'un dict ; ses clés restent
des chaînes) :

```tcl
set {list.bare}LB bare0 bare1 bare2      ;# chaque élément rendu nu
$LB(1)                                   ;# bare1, il scintille
set {list.typed}LT {int}5 {double}2.5    ;# chacun le sien, et il le garde
set {list.noted}LA {this}ti {that}ta     ;# des proto-types (type debug report)
set {list.typednoted}LTA {int@this}5 {double@that}2.5
set {list.any}L …                        ;# les quatre, tout est gardé
set {list.int}I …                        ;# un type précis ; les notes gardées
set {dict.typed}D a {int}1 b {double}2.5
```

**Correction** (faite en v17a) : tous les constructeurs de conteneurs gardent les notes de
leurs éléments (aujourd'hui ils les effacent, contre ANNOTATIONS §2.1 : « une
note suit la valeur : … élément de liste »).

**`{T@notes}`** *(fait en v17f)* : `@` est le séparateur que la TIP emploie pour *afficher* le
type et les notes, et la forme `T@notes` est déjà décrite dans
`PISTES-annotations-locales.md` (3 novembre, `exemples/texte_enrichi`) : les
notes forment un **ensemble** (triées), `,` les sépare, `.` imbrique (un
chemin), `=` donne sa valeur à une feuille — `{int@a.b,c,d.e.f}`,
`aspect@pente.italic,poids.bold`. Aujourd'hui `{int@this}5` est lu comme une annotation
locale (« cannot be stored by "set" ») ; une valeur typée et notée se fait en
deux temps (`set a {this}5; set {int}v $a`). Proposition : l'accepter dans un
constructeur, pour pouvoir écrire ce qu'on lit. Conséquence : `note get` joint aujourd'hui
les notes par `.` (« mm.px ») ; avec `.` réservé à l'imbrication, il les
joindrait par `,`.

**Hors constructeur, `{int}` n'est qu'une note** (`list {int}5` : la note
`int`, pas le type). C'est un choix, pas un oubli : pas de cast immédiat sur
une valeur, pour ne pas casser les noms calculés (`set {int}$nom 5`) — voir
`PISTES-annotations-locales.md` (piste B, v16m). Les méta-types ne le
changent pas ; ils rendent l'état visible (`type meta [list {int}5]` :
`noted`).

**Rendre nu** : l'annotation vide le fait déjà sur une valeur (`set v {}$x` :
retire le type, garde les notes) ; sur une String, c'est la relecture
explicite (`llength {}$s` vaut 3 pour « a b c »), à préférer au détour
`string range $s 0 end`.

## 6. Un seul ordre de résolution

*(Fait en v17d, avec une correction décidée le 15 novembre : l'étape
« règle » ne vaut que pour un cast écrit — `type cast`, `{T}$x`, à l'appel
compris. `set`, les contrats et les arguments d'une procédure s'en tiennent
au même type, à l'élargissement et à la variante (ANNOTATIONS § 5.6 :
« les conversions restent explicites »). Les règles depuis une valeur nue
et la validation d'un nominal valent partout. Test t111.)*

Le commentaire de `TclTypeCast` donne l'ordre actuel : « même type ;
conversion déclarée ; nominal <-> sous-jacent ; conversions intégrées ;
variante d'une somme ». `set`, lui, prend la variante exacte avant la règle.
Proposition, la même partout (`set`, `type cast`, argument), quand une valeur
de méta-type M et de type U doit devenir un T :

1. U est T : rien.
2. T est une somme dont une seule variante est de type U : cette variante —
   *une valeur qui convient déjà n'est jamais convertie*.
3. Une règle `type convert U T` : on l'applique.
4. M est `bare` (ou `noted` dont la note ne se résout pas) : le lecteur
   natif de T ; s'il échoue et qu'une règle depuis la valeur nue existe, on
   l'applique. (Aujourd'hui `type convert any T {…}` est accepté mais jamais
   consulté.) Avec § 3, cette règle s'écrirait `type convert bare T`.
5. Sinon, refus.

## 7. Forme canonique et normalisation

Précédent : la **forme compacte** normalise déjà à la construction
(ANNOTATIONS §14.1 : « `{0 0}` becomes `{0.0 0.0}`, `0x10` becomes `16` »).
Il s'agit de l'étendre à toute valeur typée.

*(Fait en v17a, voir `noyau/LISEZMOI.md`. Conséquence en v17b : l'union
`{int}i | {double}d` devient disjointe sur les formes canoniques, et
acceptée ; test t109.)*

**Dès la construction**, un `int` ou un `double` validé perd sa
représentation texte, comme le fait `expr` (`INST_TRY_CVT_TO_NUMERIC`,
tclExecute.c : « to make sure that "expr {0001}" yields "1" ») : jeter
`bytes` si l'objet n'est pas partagé, sinon le dupliquer sans son texte. Le
texte se régénère à la demande, canonique. Place : là où la valeur typée est
produite (`TclTypeCast`, la mise en variable typée), pas dans la vérification
(`tclTypes.c`, TCLT_INT / TCLT_DOUBLE), qui ne doit pas toucher une valeur
peut-être partagée. Pour `list.T` et `dict.T` : les éléments, et le texte du
conteneur jeté (la forme compacte s'y prête).

Cas : `" 3 "`, `+3`, `0o10`, `1_000` donnent `3`, `3`, `8`, `1000` ; `1` dans
un `double` donne `1.0` ; à vérifier : bignums, `-0.0`, `Inf`, `NaN`. Une
`string` n'est jamais normalisée (qui veut garder `0.10` déclare une chaîne).

*(`type canonic` et `type parse` : faits en v17c, voir `noyau/LISEZMOI.md`
et ANNOTATIONS § 6.3b.)*

**`type canonic $v`** — nom déjà retenu le 3 novembre
(`PISTES-annotations-locales.md` : « `type canonic $v` → `{T}contenu`, et
`type parse $texte` → la valeur typée », avec la forme sans contenu
`T@notes`, notes triées) : rendre la forme canonique — `type cast any` ne le peut pas, puisque `any` garde tout.
Deux lois :

    type cast T [type canonic $v]  ≡  $v                 (aller-retour)
    type canonic [type cast T $x]  =  forme canonique de x  (normalisation)

`type deref` tient déjà l'aller-retour pour toutes les valeurs essayées ; avec
la normalisation dès la construction, `type canonic` se réduit presque à `{}`.
Précisions : la forme canonique est celle du lecteur natif (jamais d'une
règle) ; une somme porte sa variante ; doubles : l'écriture la plus courte
exacte ; dicts : l'ordre des clés ; liens : comme `type deref`, avec
`-linkdepth`.

## 8. Types nominaux validés

`type def Jour string` avec une règle `string -> Jour` qui vérifie le format :
une String invalide est refusée, une valeur nue invalide passe
(`type cast Jour 10/11`). Le lecteur natif d'un nominal devrait appliquer la
règle de validation du type : la validation fait partie du type.

## Décisions et questions (13 novembre, fin d'après-midi)

* Les noms `bare`, `typed`, `noted`, `typednoted` : retenus (pas d'autre
  idée pour `typed`).
* `type meta` d'une valeur typée : le type résolu suffit. (La question était
  de rapporter aussi la représentation du *contenu* d'une valeur typée — un
  `int` dont le contenu est encore la chaîne `0x10`, une liste en cellules
  compactes. La normalisation à la construction rend le premier cas sans
  objet ; le second a déjà `type is {compact?}$v`.)
* `type fix` : plus tard.
* `{T@notes}` : oui, collection séparée par des virgules, le point pour
  l'imbrication — `{int@a.b,c,d.e.f}` —, comme `texte_enrichi`.
