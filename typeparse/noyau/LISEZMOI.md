# Intégration au noyau Tcl 9.0 (branche core-9-0-branch)

1. Copier les fichiers `tclTypes*.{c,h}`, `tclAnnotObj.c`, `tclTypedVar.c`,
   `tclTypePath.c`, `tclTypeGeneric.c`, `tclTypeParse.c`, `tclVarRef.c`
   (dossier parent) dans `generic/`.
2. Dans la racine des sources : `patch -p1 < noyau/integration-noyau.patch`
   (tcl.h, tclInt.h, tclBasic.c, tclParse.c, tclCompile.c, tclCompCmdsSZ.c,
   tclExecute.c, tclVar.c, tclNamesp.c, unix/Makefile.in).
3. `./configure && make && ./tclsh tests_tclsh/t4_chemins_liens.tcl`.

Suites standard (set, var, upvar, parse, compile, expr, append, incr,
namespace, proc) : 3388 réussis / 13 en échec. Les 13 échecs testent tous
l'ancienne erreur « extra characters after close-brace », devenue
l'annotation `{type}mot` : à réécrire dans la suite de tests.

## Arbre TIP 759 (tcl-9-0-expr-cmd)

Utiliser `integration-noyau-tip759.patch` (même procédure, `patch -p1`) :
l'ancien patch visait core-9-0-branch et son contexte diffère dans tcl.h
(TCL_TOKEN_CMD_IN_EXPR). Si `TCL_TOKEN_ANNOT` est « non déclaré », le hunk de
tcl.h n'a pas été appliqué : ajouter à la main, après TCL_TOKEN_EXPAND_WORD,
`#define TCL_TOKEN_ANNOT 512`.

## Listes abstraites (Tcl 9)

Le type d'objet `annotated` est un `Tcl_ObjType` V2 : `lindex`, `llength`,
`lrange`, `lsearch`, `foreach`, `in`, `lappend`, `lset`, `lreplace`... passent
par ses procédures de liste. Un scalaire typé est un seul élément ; les
modifications sont revalidées contre le type.
`listes-abstraites.patch` (tclListObj.c, `patch -p1`, valable pour les deux
arbres) fait passer `lappend` par la `replaceProc` d'un objet qui en possède
une. Tests : `tests_tclsh/t9_*.tcl`, `t10_*.tcl`.
## Conversions du noyau (TclAnnotUnwrap)

`conversions-annotees.patch` (après `integration-noyau*.patch`, `patch -p1`)
branche `TclAnnotUnwrap` dans `tclObj.c` (nombres, booléens), `tclStringObj.c`
et `tclBinary.c` (chaînes, octets), `tclExecute.c` (`TclIncrObj`) et
`tclDictObj.c` (lecture sur le contenu ; les commandes qui réécrivent une
variable travaillent sur une copie, que la variable typée revalide). Avant, la
plupart des commandes (`expr`, `string length`, `if`, `incr`, `dict get`...)
convertissaient l'objet annoté en place et la variable perdait son type.
Ordre : `integration-noyau*.patch`, `conversions-annotees.patch`,
`listes-abstraites.patch`. Test : `tests_tclsh/t11_conversions.tcl`.

La fuite de `lset` (référence en trop, via `TclLsetList`) est corrigée par
`lset-reference.patch` (v15o).

## Windows : MSYS2 / MinGW-w64

Fichiers : `windows-mingw.patch` (à appliquer **après** les autres patches) ajoute
les 7 objets (`tclTypes`, `tclTypeParse`, `tclTypeGeneric`, `tclAnnotObj`,
`tclTypedVar`, `tclTypePath`, `tclVarRef`) à `win/Makefile.in` (mingw) et
`win/makefile.vc` (nmake/MSVC). Les `.c` de `generic/` sont compilés par la
règle implicite via `VPATH`, aucune règle supplémentaire n'est nécessaire.

Dans un shell « MSYS2 MINGW64 » (paquets : `pacman -S make patch autoconf
mingw-w64-x86_64-gcc mingw-w64-x86_64-zlib`) :

    cd tcl-9-0-expr-cmd
    cp /chemin/typeparse/{tclAnnotObj.c,tclTypes.c,tclTypeParse.c,tclTypeGeneric.c,\
    tclTypedVar.c,tclTypePath.c,tclVarRef.c,tclTypes.h,tclTypesInt.h} generic/
    patch --binary -p1 < noyau/integration-noyau-tip759.patch
    patch --binary -p1 < noyau/conversions-annotees.patch
    patch --binary -p1 < noyau/listes-abstraites.patch
    patch --binary -p1 < noyau/windows-mingw.patch
    cd win && ./configure --enable-64bit && make -j4 tclsh

Notes :
- Le patch `integration-noyau*.patch` modifie aussi `unix/Makefile.in` : sans
  conséquence sous Windows. Si `patch` signale des fins de ligne, utiliser
  `--binary` ou `dos2unix` sur les `.patch` (git `core.autocrlf=false`).
- Le banc de test sans noyau : `cd test && make -f Makefile.mingw` (pas
  d'ASan/UBSan sous mingw ; l'ancien `Makefile` est réservé à Linux).
- Les formats `printf` du code utilisent `%lld` + cast : sûrs avec le `long` 32 bits de Win64.
- Les tests `tests_tclsh/*.tcl` se lancent avec `./tclsh.exe` en définissant `TCL_LIBRARY`.

## Correctif TIP 759 : appels de fonction dans ( ... )

`tip759-fonctions.patch` (indépendant des autres) corrige un plantage de
l'instantané TIP 759 : dans `tclCompExpr.c`, `CompileExprTree`, le cas
`NULL_FUNC` faisait `funcObjv++` alors qu'aucun nom n'est ajouté à `funcList`
pour une parenthèse ; `[(sqrt(4.0))]` lisait donc un nom de fonction hors
tableau (segfault, ou `invalid command name "tcl::mathfunc::"`).

## Affectation destructurante dans expr : `(a,b,c) = $L`

`tip759-lassign.patch` (à appliquer après `tip759-fonctions.patch`, sur
`generic/tclCompExpr.c`) ajoute au lexème LIST l'équivalent de `lassign` :

    expr {(a,b,c,d) = $L}          ;# a,b,c,d reçoivent les 4 premiers éléments
    expr {(h,t) = [list 1 2 3]}    ;# les éléments en trop sont ignorés
    expr {(p,q,r) = {1 2}}         ;# les éléments manquants donnent ""

* Valeur de l'expression : la liste affectée (la valeur de droite).
* Analyse : à l'ouverture d'une parenthèse, `FindTupleTarget` cherche la
  parenthèse fermante et regarde si elle est suivie d'un `=` isolé (pas `==`).
  Si oui, les mots nus à l'intérieur sont des noms de variables.
* Compilation : `ASSIGN` dont l'opérande gauche est un LIST émet, pour chaque
  nom, `OVER/LIST_INDEX_IMM/OVER/LIST_INDEX_IMM/STORE_STK/POP`, puis
  `REVERSE 2; POP` (sémantique de `lindex`, donc "" si l'élément manque).
* Les variables typées valident chaque affectation (voir t12_lassign_expr.tcl).
* Imbrication : `((a,b),c,(d,(e,f))) = $N` avec `$N` = `{{1 2} 3 {4 {5 6}}}`
  (profondeur max 32). `EmitTupleStores` parcourt l'arbre cible (COMMA,
  LIST, OPEN_PAREN) et émet, pour chaque nom, le chemin d'indices
  `LIST_INDEX_IMM i j ...` dans la liste de noms et dans la valeur.
* Dans un corps de procédure, les instructions s'enchaînent avec `;` :
  `proc f {P} {( (x,y) = $P; [expr {hypot($x,$y)}] )}`.

## L'opérateur `is` : `expr {z is Complex}`

`tip759-is.patch` (après `tip759-lassign.patch`, sur `generic/tclCompExpr.c`) +
la commande `::tcl::mathop::is` (dans `tclTypes.c`, fournie avec les sources).

    set {Complex}z re 3 im 4
    expr {z is Complex}         ;# 1
    expr {n is int}             ;# variable {int}n
    if {z is Complex && w is Complex} { ... }
    expr {$name is int}         ;# $name contient le NOM de la variable

* Gauche : le NOM d'une variable (mot nu, ou `$v`/chaîne qui contient le nom).
  Droite : un type, écrit tel quel (`int`, `list.int`, `::ns::t`, `tree(int)`)
  ou obtenu par `$T`.
* Vrai si la variable est typée et que son type déclaré est inclus dans le
  type demandé ; si elle n'est pas typée, vrai quand sa valeur est valide pour
  le type. Variable absente : faux. Type inconnu : erreur.
* Lexème `IS_TYPE` (binaire, précédence de `==`/`in`, non constant). Après
  `is`, l'analyseur lit le type comme un texte littéral ; avant `is`, un mot nu
  est accepté comme nom de variable (`IsTypeAhead`).
* Compilation : `[nom type]` + `::tcl::mathop::is`, `REVERSE 3`, `REVERSE 2`,
  `INVOKE_STK 3`, donc sans nouvelle instruction.
* Limites : pas d'élément de tableau à gauche (`a(1) is int` serait un appel de
  fonction) ; `is` seul, pas de `isnt` (écrire `!(x is T)`).

## Affectation typée dans expr

`tip759-affectation-typee.patch` (après `tip759-is.patch` ; touche
`tclCompExpr.c` et `tclExecute.c`) + `::tcl::unsupported::declare` (dans
`tclTypes.c`).

    expr {z is Complex = (3,4)}   ;# déclare z : Complex ET lui affecte (3,4)
    expr {z = (5,6)}              ;# z déjà typée : valeur validée par son type
    expr {z(re) = 10}             ;# affectation d'un champ (ou d'un élément de tableau)
    expr {n is int = 42}

* `x is T = v` : l'affectation dont la gauche est un `is` n'évalue pas le test ;
  elle appelle `declare nom T v`, équivalent de `set {T}nom v`. Une variable
  déjà d'un autre type refuse (typage strict).
* `z(re) = v` : un mot nu suivi de `(...)` puis d'un `=` isolé (ou de `is`) est
  un nom de variable, pas un appel de fonction. À l'exécution, `INST_STORE_STK`
  sépare `z(re)` et passe par `TclTypedVarSetElement` (le champ est revalidé).
* Pas de tuple nommé `(re:3, im:4)` : `:` est déjà le `?:` de expr.

## `nom is cmd` : une commande appelée comme fonction de expr

Pas de patch noyau : seul `tclTypes.c` change (`IsCmdOp`, dans
`::tcl::mathop::is`).

    proc new {re im} { list [expr {double($re)}] [expr {double($im)}] }
    expr {new is cmd}                  ;# 1, et new devient une fonction de expr
    expr {new(3,4)}                    ;# 3.0 4.0
    expr {z is Complex = new(5,6)}
    expr {(new is cmd; (a,b) = new(7,8); $a+$b)}

* Vrai si NOM est une commande (cherchée depuis l'espace de noms courant puis
  le global), faux sinon.
* Si elle existe, un alias `::tcl::mathfunc::<queue du nom>` la désigne : c'est
  le mécanisme normal des fonctions de expr, donc `new(3,4)` appelle
  `new 3 4`. Un alias déjà présent n'est pas écrasé. L'alias est global (pas
  limité à l'espace de noms) et suit la commande par son nom complet.

## type-get.patch : `type get`

Ajoute la sous-commande `type get valeur` (generic/tclTypes.c seul) : elle
rend le nom du type d'une valeur, donc d'une variable typée avec `type get $x`.

    set {int}n 5            ; type get $n   ;# int
    set {list.int}L 1 2 3   ; type get $L   ;# list<int>
    set M {list}{}          ; type get $M   ;# list
    expr {z is Complex = (3,4)} ; type get $z ;# Complex

Une valeur sans annotation rend la chaîne vide. Pour tester le type d'une
variable par son nom, l'opérateur `is` de expr reste l'outil. À appliquer
après tous les autres patches (-p1, depuis la racine de l'arbre).

## elements-types.patch : éléments de liste typés, `{i}5`, `Val.i`

À appliquer après type-get.patch et avertissements.patch (-p1, racine de
l'arbre ; 4 fichiers : tclTypes.c, tclAnnotObj.c, tclTypedVar.c, tclTypePath.c).

- `foreach`, `lmap`, `lindex`, `lrange`... sur une liste `list.T` rendent des
  éléments enveloppés dans le type T : `type get $e` -> int, Complex, Val.i.
  L'enveloppe est posée une fois, dans le contenu de la liste, à la
  première lecture (chaîne inchangée). Les listes de strings (`{list}`,
  `list.string`) ne sont PAS enveloppées : une valeur string est un seul
  élément pour les commandes de liste, ce qui casserait `lindex $L 2 1`
  sur {a b {c d}}. Leur type se lit par `$L(i)`.
- Liste de sommes : `set {list.Val}L {i}5 {s}hello` est accepté ; la balise
  est conservée (type get -> Val.i / Val.s), y compris par `$L(i)`.
  Sans balise, la variante est déduite par revalidation.
- `Val.i` s'écrit aussi `{Val.i}5`, comme `{Val}{i}5` ; `set {Val.s}w 7`
  choisit la variante s. `type get` rend `Val.i` pour une somme.
- Conséquence : une variable qui reçoit une valeur typée prend ce type
  (comme avec `set`). Réutiliser le même nom de variable de boucle pour
  des éléments de types différents -> "type mismatch" ; `unset e` entre
  les boucles.

## lindex-abstrait.patch : lindex à plusieurs indices

Correctif du noyau (generic/tclListObj.c, TclLindexFlat). `lindex $Q 2 1`
convertissait en liste ordinaire la valeur de la variable Q (SetListFromAny
sur l'objet lui-même) : l'annotation était détruite en silence (`type get
$Q` rendait ensuite "" et `$Q(0)` échouait). Pour un objet dont le type
fournit une procédure d'index (listes abstraites : valeurs annotées, suites
de lseq), on passe maintenant par cette procédure. Indépendant des autres
patches (le fichier est celui de Tcl 9.0.5). Tests noyau lindex, lset,
listObj, lseq, list, lrange, foreach : aucun échec.

## liste-string.patch : `list.string` à éléments atomiques

À appliquer après elements-types.patch (6 fichiers : tclTypes.c, tclTypes.h,
tclTypesInt.h, tclTypeParse.c, tclTypedVar.c, tclAnnotObj.c).

`list.string` écrit explicitement est désormais un type distinct de la liste
nue `{list}` : son nom est `list<string>` (TclTypesMakeListExplicit). Ses
éléments sont enveloppés à la lecture comme ceux de `list.int` : ils sont
atomiques, un élément n'est pas redécoupé en mots.

    set {list.string}L {hello world} {coucou à vous}
    foreach e $L {foreach f $e {puts $f}}   ;# 2 écritures
    lindex $L 1 1                           ;# vide (hors limites)

La liste nue `{list}` garde le comportement de Tcl (5 mots ci-dessus ;
`lindex $L 2 1` sur {a b {c d}} donne d). Les deux types s'incluent
mutuellement (`L is list`, `A is list.string`) et s'affectent l'un à
l'autre (TclTypeSame). Pour redécouper un élément : `split` ou `lindex`
sur la chaîne d'une liste nue.

## avertissements-shadow.patch

tclTypeGeneric.c : variable `q` renommée `qp` (-Wshadow).

## atomique-profond.patch et foreach-abstrait.patch : atomicité à tous les niveaux

À appliquer après liste-string.patch. atomique-profond.patch : tclAnnotObj.c ;
foreach-abstrait.patch : tclCmdAH.c (noyau, indépendant).

Un scalaire annoté (élément atomique d'une `list.T`, `{string}s`, `{int}n`)
est une liste d'UN élément qui est LUI-MÊME, annotation comprise : `foreach f
$e`, `lindex $e 0`, `lmap` rendent e. Le type se conserve (`type get $f` ->
string) et l'atomicité aussi, à n'importe quelle profondeur d'imbrication.
Avant, l'élément rendu était le contenu nu : le type était perdu au 2e niveau
et le texte redécoupé en mots au 3e.

    set {list.string}L {hello world} {how are you ?}
    foreach e $L {foreach f $e {foreach g $f {puts [type get $g]:$g}}}
    ;# string:hello world / string:how are you ?

getElementsProc doit rendre un tableau qui vive autant que l'objet : une
case par objet dans une table propre au thread (SelfCell), libérée avec
l'objet (FreeAnnotated).

foreach-abstrait.patch corrige un défaut du noyau 9.0 révélé par ce
changement (reproductible dès qu'une liste abstraite rend un élément qui
dépend de sa copie) : EachloopCmd crée aCopyList = Tcl_DuplicateObj(liste
abstraite) SANS référence, alors que ForeachCleanup fait un DecrRefCount. Si
la variable de boucle a retenu la copie, elle est libérée sous elle
(lecture après libération, plantage tardif dans l'allocateur). Correctif :
un Tcl_IncrRefCount. Vérifié avec valgrind sur un noyau compilé -DPURIFY
(aucune erreur) et les tests noyau foreach, lmap, lindex, lset, list,
listObj, lseq.

## variables-boucle.patch  (à appliquer après foreach-abstrait.patch)

Les variables de boucle perdent leur type juste avant chaque affectation
(`foreach`, `lmap`, `dict for`, `dict map`, et le filtre de `dict filter`),
au niveau global comme dans une procédure :

* `tclTypedVar.c` : `TclTypedVarForget` (remplace la valeur annotée par son
  contenu brut et retire `VAR_TYPED`) et `TclTypedVarForgetByName`.
* `tclCmdAH.c`, `tclDictObj.c` : appel avant chaque `Tcl_ObjSetVar2` des
  variables de boucle (chemin commande, utilisé hors procédure).
* `tclExecute.c` : `INST_FOREACH_STEP` oublie le type, et une valeur annotée
  passe désormais par `TclPtrSetVarIdx` (même comportement que le chemin
  commande : la variable adopte le type de l'élément reçu).
* Nouvel opcode `INST_TYPE_FORGET` (tclCompile.h/.c, tclExecute.c) émis par
  `dict for` / `dict map` compilés (tclCompCmds.c).

Après la boucle, la variable garde le type du dernier élément reçu (comme
après un `set`). `for` n'a pas de variable de boucle propre : ses variables
sont affectées par vos scripts d'initialisation et de pas, donc non
concernées. Test : `tests_tclsh/t19_variables_de_boucle.tcl`.

## list-any.patch  (à appliquer après variables-boucle.patch)

Nouveau type prédéfini `any` et listes hétérogènes `list.any` : chaque
élément conserve sa propre annotation (int, double, string, list, somme…),
est atomique, et ses éléments plats sont typés `any`.

    set {list.any}L {int}1 {double}1.0 {string}{a b} {list}{e f g} plain
    foreach e $L {puts "[type get $e]:$e"}    ;# int:1 double:1.0 string:a b …

* `$L(i)`, `lindex`, `foreach`, `lmap` rendent l'annotation de l'élément ;
  `lappend L {int}7` la conserve ; `{int}abc` est refusé à la construction.
* Un argument annoté qui est lui-même une liste n'est pas déplié : c'est
  un seul élément (`set {list.any}Q $N` donne une liste d'un élément).
* `{list}` nu reste la liste classique de Tcl (éléments sans type).
* Fichiers : tclTypes.c/.h (`TclTypeIsAny`, builtin `any`), tclTypedVar.c
  (BuildList, TclTypedConform, lappend), tclTypePath.c (WrapSlot).
* Test : `tests_tclsh/t20_list_any.tcl`.

## valeurs-en-procedure.patch  (à appliquer après list-any.patch)

Une valeur typée gardait son type au niveau global mais le perdait dans une
procédure compilée : `proc f {t} {puts $t(x)}` appelée avec un objet typé
échouait (« variable isn't array »), de même `set r $q` dans une procédure.
Cause : les chemins rapides du compilateur (liaison des arguments dans
`InitArgsAndLocals`, `INST_STORE_SCALAR`) n'activaient pas VAR_TYPED.

* `tclProc.c` : un argument annoté (ou le dernier argument) est marqué typé
  à la liaison (`TclTypedVarAfterStore`).
* `tclExecute.c` : `INST_STORE_SCALAR` passe par le chemin lent quand la
  valeur est annotée.

Conséquence voulue (comme `set` au niveau global) : un argument typé est
strict dans la procédure ; `set t abc` sur un `int` échoue. Les arguments
ordinaires ne changent pas. Test : `tests_tclsh/t21_valeurs_typees_en_procedure.tcl`.
Exemple : `exemples/reseau_couches.tcl`.

## somme-balisee.patch  (à appliquer après valeurs-en-procedure.patch)

Une somme dont deux variantes ont la même forme (par ex. `{texte}tEXt` et
`{autre}inconnu`, toutes deux « deux chaînes ») ne pouvait pas vivre dans
une liste : la balise explicite `{tEXt}$t` était oubliée à la validation de
l'élément (« ambiguous variant … give the variant name »), et `lappend`
la supprimait.

* `tclTypes.c` (ValidateG) : un élément déjà balisé garde sa variante.
* `tclTypedVar.c` : `TclTypedConform` transmet la variante choisie, et
  `lappend` à une liste de sommes conserve l'annotation de l'élément.

Test : `tests_tclsh/t22_somme_balisee.tcl`. Exemple : `exemples/png_somme.tcl`
(PNG décrit par un type somme : IHDR, tEXt, IDAT, IEND, inconnu).

## produit-ambigu.patch  (à appliquer après somme-balisee.patch)

> **v13 : remplacé** par `types-produit-etiquettes.patch` (forme nom/valeur nue supprimée).

Un type produit appelé avec n arguments pour n champs (n pair) peut se lire
en positionnel (une valeur par champ) ou en nom/valeur (n/2 couples, champs
omis par défaut). `set {png}p chunks $blocs` donnait silencieusement
signature = "chunks". Désormais, `BuildProduct` essaie les deux lectures
sans effet de bord (`ProductTryPositional`, `ProductTryNameValue`) :

* une seule valide : on la prend (`set {trace}t texte a` est lu nom/valeur,
  `niveau` prenant son défaut, au lieu d'échouer en positionnel) ;
* les deux valides : erreur « ambiguous arguments for T … name every field » ;
* aucune : lecture positionnelle, avec son message d'erreur habituel.

La règle positionnelle est conservée. Test : `tests_tclsh/t23_ambiguite_produit.tcl`.

## produit-champ.patch  (à appliquer après produit-ambigu.patch)

Même écriture pour les champs d'un produit que pour les variantes d'une somme :
`set {paire}p {paire.nom}Jean` fixe le seul champ `nom` ; les autres champs
prennent leur défaut (`Jean 0`). Le préfixe `{T.champ}` n'est reconnu que si
`T` est le type construit et seulement pour UN argument ; champ inexistant :
« unknown field "zz" in paire » ; un autre type (`{aut.nom}x`) : message
dédié depuis v15l. Le préfixe n'a jamais de sens de champ en dehors d'un
produit. Test : `tests_tclsh/t24_produit_champ.tcl` (+ `.out`).

## produit-optionnel.patch  (à appliquer après produit-champ.patch)

Dans une définition de produit, `?=` est un synonyme de `=` pour la valeur par
défaut d'un champ : `{int}age ?= 3`, comme `dict(x, z?=0)`. L'ancien `=`
reste accepté. Changement local à l'analyseur lexical (`ParseTypeLexeme`
dans tclTypeParse.c). Test : `tests_tclsh/t25_champ_optionnel.tcl`.

Le chapitre « Dict et produit » du manuel est dans `MANUEL.md`.

## bitarray.patch  (à appliquer après produit-optionnel.patch)

Touche `tclTypes.c/.h`, `tclTypesInt.h`, `tclTypeGeneric.c`, `tclTypePath.c`
et `tclCompExpr.c`.

Type paramétré prédéfini `bitarray(n, ordre)` : un ENTIER de n bits (0 ≤ v <
2^n, bignum si n > 64), dont on lit et écrit les bits. L'ordre est `lsb0` (par
défaut : l'indice 0 est le bit de droite, comme `1 << i`) ou `msb0` (l'indice
0 est le bit de gauche, comme les schémas des RFC). Ce sont deux types
distincts : on ne peut pas affecter l'un à l'autre sans cast explicite.

    set {bitarray(8)}b 0xB2          ;# type get $b -> bitarray(8,lsb0)
    set {bitarray(8,msb0)}c 0xB2
    $b(1)  $b(end)                    ;# un bit : 0 ou 1 (int)
    $b(1:4)                           ;# tranche : bitarray(4,lsb0), UN entier
    set b(0) 1 ;  set b(4:7) 0        ;# écriture d'un bit / d'une tranche
    expr {"b(1:2)" = 3}               ;# dans expr : entre guillemets
    expr {popcount($b)}  expr {bitlen($b)}

* La tranche `i:j` désigne les bits i..j DANS LE SENS DE L'INDEX (msb0 : de
  gauche à droite). Le résultat est un bitarray de j-i+1 bits de même ordre.
* Hors de la largeur, la valeur est refusée : `value "256" does not fit in
  bitarray(8,lsb0) (8 bits)`. Largeur ≥ 1 ; ordre lsb0|msb0, sinon erreur.
* Implémentation : `TclType` reçoit `bitWidth`/`bitMsb0` (kind reste INT, donc
  `expr`, les conversions et `type` ordinaire marchent) ; le générique est
  défini à l'initialisation (corps `int # n ord`, drapeau `bits`) et
  `TclTypesInstantiate` pose les deux attributs. Lecture : `Step`/`StepRange`
  de tclTypePath.c ; écriture : `SetIn` (une tranche n'est admise qu'en
  dernier pas, sur un bitarray). Fonctions `::tcl::mathfunc::popcount` et
  `bitlen`.
* Correction jointe dans tclCompExpr.c : `nodes[nodePtr->left]` lisait hors
  tableau quand la gauche de `=` est une feuille (OT_LITERAL < 0), p. ex.
  `expr {"c(0:1)" = 3}` (trouvé par valgrind).

Test : `tests_tclsh/t26_bitarray.tcl` (+ `.out`).

## avertissements-hashkey.patch  (à appliquer après bitarray.patch)

tclTypeGeneric.c : `Tcl_GetHashKey` renvoie `void *` ; conversion explicite en
`const char *` (avertissement -Wc++-compat dans `Rollback`).

## type-deref.patch  (à appliquer après avertissements-hashkey.patch)

Touche `tclTypes.c/.h` et `tclTypePath.c`. Nouvelle sous-commande
`type deref VALEUR` : renvoie la valeur où chaque lien `var` est remplacé par
la valeur ACTUELLE de la variable visée (un instantané).

    set {list.var.int}refs A B C        ;# refs = ::A ::B ::C
    set v [type deref $refs]            ;# 1 2 3, de type list.int

* Le type du résultat est calculé (`TclTypeDerefType`) : `var.T` devient `T`
  (`var` seul : `string`), au travers des listes (arité conservée), des dicts
  (clés conservées) et des produits (le résultat est alors un produit
  anonyme). Une valeur sans lien est rendue telle quelle.
* Lien cassé (variable absente, appel de procédure terminé) : erreur
  franche qui nomme la case, `type deref: at 2: can't read "::C": no such
  variable`. Tout ou rien : la valeur d'origine n'est jamais modifiée.
* On ne suit pas les liens que contient la valeur d'une variable visée : un
  second `type deref` descend d'un niveau de plus.
* Non pris en charge (erreur claire) : un lien à l'intérieur d'une somme, et
  un type récursif contenant des liens (`tree(var)`).

Test : `tests_tclsh/t28_type_deref.tcl` (+ `.out`).

## deref-complet.patch  (à appliquer après type-deref.patch)

Touche `tclTypes.c`, `tclTypesInt.h` et `tclTypePath.c`. `type deref` suit
maintenant les liens à l'intérieur des SOMMES et des types RÉCURSIFS.

* `TypeData` reçoit la table `derefs` (TclType* -> type sans liens). Pour un
  produit ou une somme, le nouveau descripteur `deref(<nom>)` est enregistré
  AVANT son contenu : un type récursif (`tree(var)`, `bt`) retombe sur ce même
  descripteur au lieu de boucler. Le résultat est réutilisé d'un appel à
  l'autre (pas de croissance de la table de types).
* Somme : la variante en cours est retrouvée dans le nouveau type par son nom
  (ou par son type/sa valeur littérale si elle n'a pas de nom), car le tri
  des variantes peut avoir changé leur ordre. Un élément de liste ou de dict
  de type somme garde son annotation (sa variante).
* Seule une collection récursive sans produit ni somme (`type def L list.L`
  avec des liens) reste refusée.
* Les graphes n'ont pas de cas particulier : un lien `var.node` donne
  `node` sans descendre, donc un cycle ne boucle pas ; chaque `type deref`
  descend d'un niveau.
* Le type d'un produit rendu par `type deref` s'appelle désormais
  `deref(<nom>)` au lieu d'un nom anonyme `prod<...>`.

Tests : `t29_deref_arbres_sommes.tcl` ; `t28` ne change que pour ce nom.

## types-binaires.patch  (à appliquer après deref-complet.patch)

Touche `tclTypes.c`, `tclTypes.h`, `tclTypesInt.h`, `tclTypeGeneric.c`, `tclTypedVar.c`
et `tclTypeParse.c`. Version COMPLÈTE (dernière notation). Si tu avais déjà appliqué la
version précédente (celle avec `be.uint(16)`), applique plutôt `types-binaires-v3.patch`
(incrémental). Si tu avais la toute première (`uint(16,be)`), pars de `deref-complet`
et applique le patch complet.

Aucune syntaxe nouvelle pour les champs : un type PORTE sa représentation binaire,
comme `bitarray(n)` porte déjà sa largeur. Le `.` ne sert qu'à COMPOSER (`list(3).T`) ;
la représentation est dans les parenthèses du type lui-même.

* `int(u|s, n ?, be|le?)` : entier non signé / signé de n bits (multiple de 8, de 8 à 1024).
  `double(32|64 ?, be|le?)` : flottant IEEE. `string(ENC, N)` : texte sur N octets complété
  par des NUL ; `string(ENC, z)` : texte terminé par un NUL (2 octets en utf-16/ucs-2) ;
  `string(raw, N)` : N octets bruts. ENC : `raw ascii latin1 utf-8 utf-16le utf-16be
  ucs-2le ucs-2be`. `bitarray(n, ordre, be|le)` (le boutisme est le troisième argument,
  omis par défaut). Sans boutisme, c'est celui de l'appel (`-endian`).
* Ces formes sont construites par `TclTypesBinaryType` (tclTypeGeneric.c), appelée par le
  parseur quand `int`, `double` ou `string` reçoit des arguments ; `int` seul reste le type
  de base sans contrainte. `bitarray` reste un type générique (3e paramètre `e = auto`,
  absent du nom d'affichage).
* `TclType` reçoit `binKind`, `binSize`, `binEnd`, `binEnc` (nom d'encodage Tcl) et `refine` ;
  `CopyInto` copie les quatre premiers (pas `refine`), donc un `type def` sur un
  raffinement reste NOMINAL. Plus de noms réservés `be`/`le`, plus de qualificatifs
  `be.T` : le patch précédent est retiré.
* Validation (`ValidateG`) : plage exacte pour `int(u|s,n)` (bignum), encodage STRICT
  (`Tcl_UtfToExternalDStringEx` avec `TCL_ENCODING_PROFILE_STRICT`) et taille en octets
  pour `string(ENC,N)`, caractères <= 255 pour `string(raw,N)`, pas de NUL dans un texte.
* `TclTypeAssignable(attendu, reçu)` : ÉLARGISSEMENT automatique par inclusion des
  ensembles de valeurs. Les raffinements (`int(u|s,n)`, `double(n)`, `bitarray(n)`) entrent
  dans `int`, `double` ou un raffinement plus large (`int(u,8)` -> `int(s,16)`, `double(32)`
  -> `double(64)`, tout entier -> `double`) ; `int(u,16,be)` et `int(u,16,le)` sont
  interchangeables (mêmes valeurs). Le rétrécissement (`int` -> `int(u,8)`) reste refusé,
  ainsi que bitarray msb0 <-> lsb0. Un type défini par `type def` est nominal ; `type alias`
  donne un nom structurel (`type alias u16be int(u,16,be)`).
* Listes : le dépliage d'une liste typée dans une autre ne cache plus de rétrécissement :
  `list<int>` n'entre pas dans `list.int(u,8)` (`tclTypedVar.c`, `BuildList`). Une liste de
  `string` (non typée) est toujours validée valeur par valeur.
* `type binary TYPE` rend la représentation : `uint 2 be`, `sint 4 auto`, `float 8 le`,
  `bits 2 be`, `bytes N`, `str N ENC`, `cstring ENC`, `lit N` (somme de littéraux de même
  longueur = CONSTANTE, `type def mz {"MZ"}`), `array N R`, `product NOM {{champ R aunDéfaut} ...}`.
* Le message de sous-commande inconnue devient `... deref, get or binary`.

Tests : `t33_types_binaires.tcl` (types, élargissement, `type binary`),
`t34_alignement_chaines.tcl` (alignement, encodages, listes), `t32_layout_plat.tcl`.
Vérifié : suites du noyau sans nouvel échec, valgrind propre (build PURIFY), `-Wshadow`
`-Wc++-compat` sans avertissement.

## types-binaires-v3.patch  (incrémental)

Passe de la version précédente de `types-binaires.patch` (qualificatifs `be.T`/`le.T`,
`uint(n)`, `sint(n)`, `float(n)`, `bytes(n)`, `str(n)`, `cstring`) à la notation ci-dessus.
À n'utiliser que si cette version est déjà appliquée.

## types-binaires-niveau2.patch  (incrémental, niveau 2)

À appliquer APRÈS `types-binaires.patch` (ou `types-binaires-v3.patch`) tel que livré
avant le niveau 2. Les trois fichiers `types-binaires*.patch` aboutissent aux mêmes
sources : `types-binaires.patch` (complet, depuis `deref-complet`), `-v3` (depuis la
version avec `be.uint(16)`), `-niveau2` (depuis la version précédente, notation
`int(u,16,be)` sans niveau 2).

Tailles, nombres et positions lus dans le fichier. Une seule notation : `$champ` dans les
arguments du type (comme les paramètres `$n` des types paramétrés), ou un type entier
non signé qui sert de PRÉFIXE.

* `string(ENC, N | z | PRÉFIXE | $expr)`, `list(PRÉFIXE | $expr).T` : taille (octets) ou
  nombre d'éléments. `PRÉFIXE` : `int(u,16,be)` ou un alias (`u16be`), lu juste avant.
  `$expr` : une expression Tcl sur des champs déjà lus (`$len`, `$len - 4`, `$h(m)`).
* `at(EXPR).T` : la valeur est à la position EXPR (depuis le début de la valeur lue) ;
  le curseur séquentiel ne bouge pas.
* `S($champ)` : la somme S dont la variante est choisie par la valeur du champ (les noms
  de variantes sont les étiquettes : `type def S {{pp}1, {string(utf-8,z)}2}`).
* `TclBinDyn` (préfixe ou expression), `binAt`, `binTag`, `binBase` dans `TclType`. Ces
  types sont des COPIES de leur base avec une décoration partagée (arène `TypeData.blobs`,
  libérée à la fin) ; `TclTypeAssignable` compare les bases : `at(8).int(u,8)`, `S($tag)`,
  `list($n).T` ont les mêmes valeurs que `int(u,8)`, `S`, `list.T`.
* `$champ` dans un corps de type paramétré désigne d'abord un PARAMÈTRE du type (comme
  avant) ; sinon c'est un champ du fichier.
* `type binary` vérifie que chaque `$champ` cité est un champ DÉJÀ LU du produit courant
  ou d'un produit englobant, et décrit : `str {prefix R} ENC`, `str {expr TEXTE} ENC`,
  `array COUNT R`, `at EXPR R`, `tagged CHAMP {{variante R} ...} TYPE`.
* `at` n'est pas un nom de type réservé : seul `at(...)` suivi de `.T` est reconnu.

* **Somme à variante choisie** (patch séparé `types-binaires-somme-choisie.patch`, à appliquer
  APRÈS `types-binaires-niveau2.patch` ; `types-binaires-niveau2.patch` complet l'inclut déjà) : `S(nom)`, ou `S($p)` avec `p` paramètre de
  valeur lié, donne un type d'une seule variante. `binTag` vaut alors `=nom` : pas de champ
  sélecteur, taille connue, et `ValidateG` n'a rien à deviner (variantes de même forme).
  `type binary` lit `tagged =nom`.

Tests : `t35_niveau2.tcl`, `t36_somme_choisie.tcl`. Vérifié : valgrind propre (build PURIFY),
`-Wshadow` `-Wc++-compat` sans avertissement.


## types-binaires-somme-choisie.patch  (incrémental, après niveau 2)

Si `types-binaires-niveau2.patch` (version sans variante choisie) est déjà appliqué, appliquer
seulement celui-ci. Il touche `tclTypes.c`, `tclTypeParse.c`, `tclTypesInt.h` (et `tclTypes.h`
inchangé). Si l'arbre part d'avant le niveau 2, utiliser `types-binaires-niveau2.patch`, qui
contient les deux. Côté Tcl, remplacer `layout/filebinary.tcl` (sélecteur `=nom`, taille connue).

## Type lien `*(origine,repr,options).T` (patch types-binaires-lien)

* `tclTypes.h` : `TclBinLink` (origine, nombre de `../`, repr, null) dans `TclType.binLink`.
* `tclTypesInt.h` / `tclTypes.c` : constructeur `TclTypesMakeLink`, type `tLink`, `LinkRepr`
  (dispatch dans `BinRepr`), `lit 0` pour le littéral vide (`unit`), chemin rapide de
  `TclTypeParse` pour les noms canoniques commençant par `*(`.
* `tclTypeParse.c` : lexème `*(`, `ParseLinkArgs`, sélecteur `S(nom)` / paramètre lié.
* `tclTypeGeneric.c` : `type alias` avec paramètres (instance structurelle = le corps).
* Tests : t36 à t39. Limites : voir `CONCEPTION-lien.md` section 7bis.

### `-linkdepth` et liens de frontière

* `tclTypes.h` : `TclBinLink.ref` (type entier nominal `&`+nom du lien) et `TclType.refOf`.
  Créé par `TclTypesMakeLink`. `ValidateG`, `TclTypeAssignable`, `TclTypedConform` (tclTypedVar.c)
  et `WrapSlot` (tclTypePath.c) acceptent cette adresse à la place de la cible et gardent son annotation.
* `LinkRepr` rend `link ORIGINE REPR NUL CIBLE OPT REF NOMCIBLE` et, pour un lien déjà en cours de
  développement (type récursif), `{linkref NOM}` (pile `TypeData.linkStack`).
* `layout/filebinary.tcl` 0.6 : option `-linkdepth`, `type binary deref`, `unref`.


## Annotations locales (patch types-binaires-annotations-locales)

* `tclAnnotObj.c` : `TclAnnotResolve` traite comme valeur ordinaire (`TCLA_PLAIN`) une
  annotation brute que l'analyse des types rejette pour un caractère invalide (`BADCHAR`),
  p. ex. `{-v}$mot`, `{what?}$mot`. Le résultat de l'interpréteur est sauvé autour de l'analyse
  (il peut être la valeur à ranger). Nom inconnu, parenthèse non fermée : erreurs inchangées.
* `tclTypedVar.c` : `TclTypedVarPrepareWrite` range le contenu seul pour `TCLA_PLAIN` (variable
  non typée) et ne conserve pas l'annotation dans `append`/`lappend` sur variable typée.
* Test : `t45_annotations_locales.tcl`. Conception : `CONCEPTION-annotations.md`.

## type binary annoté (patch types-binaires-type-binary-annote)

* `tclTypes.c` : `TypeBinaryCmd` lit l'annotation brute de son argument de type
  (`TclAnnotInfo`) : `{what?}` décrit, `{sizeof?}` appelle `::fb::sizeof`, `{is?}` appelle
  `::fb::isvalid`. Une annotation inconnue terminée par `?` est une erreur ; les autres sont
  ignorées. Les verbes `decode`, `encode`, `deref`, `space` passent à `::fb::filebinary` (couche
  Tcl, qui doit être chargée) ; un type nommé comme un verbe reste décrit tant qu'aucun autre
  argument ne suit.
* `layout/filebinary.tcl` : `::fb::isvalid` (décodage à blanc, `-exact`).
* Test : `t46_type_binary_annote.tcl`. `file binary` reste en alias.

## Nom type binary / chan binary (patch types-binaires-nom-type-binary)

* Commentaires seulement côté C : la commande s'appelle `type binary` (les sources ne sont plus
  seulement des fichiers : chaînes d'octets, canaux, espaces mémoire).
* `layout/filebinary.tcl` 0.8 : ensembles `::fb::filebinary` (read write decode encode sizeof
  deref space) et `::fb::chanbinary` (read write), fusionnés dans les maps de `file` et `chan` :
  `chan binary read/write` sur un canal ; `file binary ...` reste en alias.

## set refuse une annotation locale écrite (patch types-binaires-set-annotation-locale)

* `set x {-v}$mot` (ou `{what?}`, ou `set {int}n {-v}5`) est refusé à la compilation de `set` :
  `TclCompileSetCmd` émet un appel à `::tcl::annotlocal`, qui lève
  `local annotation {-v} cannot be stored by "set": it only applies between a caller and the
  command it calls` (errorCode `TCL TYPE LOCALANNOT`). Le test se fait sur le texte du mot
  (`TclAnnotLocalWordText` : le texte est refusé par la grammaire des types avec BADCHAR).
* Une annotation locale reçue par un argument (`set b $a`) n'est pas écrite par `set` : elle
  reste tolérée et ignorée par la variable (t45). Les annotations de nom (`{mm}$x`) restent
  des casts ; un type valide fonctionne comme avant.
* Limite : la vérification se fait dans le code compilé (corps de proc, scripts, `eval`),
  pas dans un `Tcl_EvalEx` C direct.
* Test : `t47_set_annotation_locale.tcl` ; `t45` adapté (la valeur arrive par un argument).

## Notes : l'objet à deux listes (patch types-binaires-notes)

* `tclAnnotObj.c` : état `{T, N}` (objet « annotstate », immuable, partagé) derrière `ptr2` ;
  `TclAnnotNotes`, `TclAnnotNormalize`, `TclAnnotIsNoteOnly`, `TclAnnotDisplay` ;
  `TclAnnotateValue` pose une note quand le nom ne se résout pas, garde les notes lors d'un
  cast ; `TclAnnotInfo` lit la note la plus récente quand il n'y a pas de type résolu.
* `tclTypedVar.c` : une variable non typée adopte une note seule.
* `tclTypes.c` : `type get` ne rend que le type résolu ; commande `note get|has|delete`
  pour les notes.
* Test : `t48_notes.tcl` ; `t45` (`nom_inconnu`) et `t47` (`nominal`) changent : un nom
  inconnu rangé dans une variable non typée est désormais gardé comme note, plus une erreur.
* Pas d'empilage `{a}{b}$x` ; `{#}` reste à part. Détail et limites : CONCEPTION-annotations.md §6.7.

## type get ne rend que le type résolu (patch types-binaires-notes-typeget)

* Petit patch après `types-binaires-notes` : `TclAnnotDisplay(interp, v, withNotes)` ; `type get`
  l'appelle avec `withNotes = 0` (`list<int>`, vide pour une note seule). Les notes se lisent
  avec `note get`. Test `t48_notes` mis à jour (procédure `show` = type@notes).

## Le cast se fait dans les constructeurs (patch types-binaires-cast-constructeur)

À appliquer après `types-binaires-notes-typeget`. Touche `tclAnnotObj.c`, `tclTypedVar.c`,
`tclTypes.c`, `tclTypes.h` et, côté noyau, `tclProc.c` (liaison des arguments). Règles et
tableau complet des cas : doc « Constructeurs de variables : annotations, types et casts »,
résumé dans CONCEPTION-annotations.md §7.

* **Hors constructeur**, `{A}$x` ne valide ni ne caste plus rien : `A` est empilé
  (`AnnotateCore` → `TclAnnotPush`). `list {px}$x` avec `x = abc` ne lève plus d'erreur.
* **Constructeurs** (`set`, liaison des arguments de `proc`, `lassign`, `foreach`, `x = v` et
  `x is T = v` dans `expr`) : `TclAnnotSettle` dépile depuis la plus récente tant que
  l'annotation se résout, s'arrête à la première qui ne se résout pas, vérifie la cohérence
  (`contradictory annotations px and qq`), caste vers la plus récente. Une note posée avant son
  `type def` est donc résolue par le constructeur suivant.
* **Contrat** (`TclTypeEnters`) : un type entre s'il est assignable (élargissement d'un
  raffinement), ou si un type de sa chaîne de bases l'est : un nominal s'efface devant sa base
  (`dist` → `mm` → `double`, `px` → `int`). Deux nominaux sans lien de chaîne ne sont jamais
  équivalents, même de même générique (`mm` et `kg`, `couple` et `energie`). La chaîne s'arrête
  aux raffinements (`int(u,8)`, `bitarray(8)`), qui restent stricts. Une annotation non résolue
  n'empêche plus l'écriture dans une variable typée : `set {int}Position {px}100`, `px` inconnu,
  donne `int@px`.
* **Casts** (`TclTypeCast`) : `type convert` spécifique, chaîne des bases (tous niveaux),
  variante, conversion déclarée entre les génériques, puis conversions intégrées entre
  scalaires si l'un des deux côtés est structurel (`mm` → `int`, `double` → `mm`). Entre deux
  nominaux sans lien, seule une conversion déclarée vaut : sinon `unknown way to cast the value
  from mm to kg` (plus de troncature implicite `mm` → `px`).
* **Paramètres fantômes** (`tclTypeGeneric.c`) : un paramètre de `type def` absent du corps est
  permis ; il distingue les instances, qui sont des nominaux différents. Exemple d'unités :
  `type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double`, puis
  `type def mm {grandeur(L, 0.001)}` ; `mm` → `m` demande une conversion. Un `type alias`
  paramétré garde l'erreur `parameter "u" is never used` (son instance est le corps lui-même).
* **`{}`** : sur une valeur en constructeur, retire le type résolu et garde la pile ; sur le
  nom (`set {}y $x`), lève le contrat et range la pile sans rien résoudre ; hors constructeur,
  le lecteur voit le contenu nu (type et pile effacés).
* **Nom de variable** : `set {zz}y 5` → `unknown type "zz"` ; `set {-v}y 5` → `local annotation
  {-v} cannot declare a variable` (plantage corrigé : `tp` NULL dans `TclSetAnnotatedCmd`).
* **`type cast T v`** : le résolveur d'un constructeur, sans variable.
* `type get` ne montre que le type résolu ; un type connu pas encore résolu est dans la pile
  (`note get {px}$x` → `px`). `TclAnnotResolve` lit une note de nom inconnu comme son contenu,
  et sans interpréteur (`Tcl_GetNumberFromObj(NULL, ...)`) toute annotation en attente (avant :
  plantage dans `TclTypeParse`).
* Tests : `t49_cast_constructeur.tcl` (dont les unités à paramètres fantômes). Sorties
  changées, voulues : t28 et t33 (liste des sous-commandes, `age` → `int`), t45 (`cast_refus` : plus de validation
  hors constructeur), t48 (`typee` : la note reste sur la pile ; `local` : une locale n'est ni
  type ni note).
* Limite : la marque `{}` posée hors constructeur voyage avec la valeur ; si celle-ci est rangée
  dans une liste puis relue par un constructeur, la pile réapparaît.

## maj.sh : sources de types copiées, patchs du noyau sans fuzz (18 octobre)

* Incident : `types-binaires-taille-dynamique.patch` n'ajoute que des lignes. Une fois
  `tclTypedVar.c` modifié par les patchs suivants, `patch -N --dry-run` le trouvait applicable
  avec du fuzz (décalage de 142 lignes) au lieu de « déjà appliqué », et l'appliquait une seconde
  fois : `CheckDynCounts` en double, compilation en échec (`redefinition`).
* Désormais les sources de types (`tclTypes*.{c,h}`, `tclAnnotObj.c`, `tclTypedVar.c`,
  `tclTypePath.c`, `tclTypeGeneric.c`, `tclTypeParse.c`, `tclVarRef.c`), livrées entières dans
  `typeparse/`, sont COPIÉES dans `generic/` (étape 2). Des patchs, seule la partie qui vise les
  fichiers du noyau est appliquée (étape 3), sans fuzz (`-F0`), « déjà appliqué » étant testé en
  premier (`patch -R --dry-run -F0`). Les patchs restent livrés entiers, pour l'historique.
* `maj.sh` se remplace lui-même à l'étape 1 : la dernière ligne (`main "$@"; exit $?`) est lue
  d'un coup, bash ne relit donc pas le nouveau fichier.
* Réparer un arbre où l'ancien `maj.sh` a doublé le patch : extraire d'abord le nouveau script,
  `unzip -o typeparse.zip typeparse/maj.sh`, puis `bash typeparse/maj.sh` ; il recopie
  `generic/tclTypedVar.c` et voit `tclProc.c` comme déjà patché.

## Règles de conversion de famille (patch types-binaires-convert-famille)

Après `types-binaires-cast-constructeur`. Ne touche que des sources de types (`tclTypes.c`,
`tclTypes.h`, `tclTypesInt.h`, `tclTypeGeneric.c`) : `maj.sh` les copie, le patch sert
d'historique. Test : `t50_convert_famille.tcl`.

    type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
    type def mm   {grandeur(L, 0.001)}
    type def m    {grandeur(L)}
    type def degC {grandeur(Θ, 1.0, 273.15)}
    type convert grandeur grandeur {{x de vers} {
        if {$de(dim) ne $vers(dim)} {return -code continue}
        expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
    }}
    set {mm}a 1500 ; set b {m}$a          ;# 1.5
    set c {kg}$a                          ;# unknown way to cast the value from mm to kg

* `type convert FROM TO lambda` : FROM et TO sont des types définis ou des FAMILLES (le nom
  d'un `type def` paramétré). Côté famille, la lambda reçoit en plus les paramètres de
  l'instance : `{x de vers}` (famille → famille), `{x de}` (famille → type), `{x vers}`
  (type → famille).
* Les paramètres sont une valeur d'un produit anonyme construit d'après la déclaration
  (`{{string}dim; {double}f; {double}o}`) : `$de(f)`, `type get $de`. Un paramètre `{type}T` ou
  `{cmd}c` y est un champ `string` ; un paramètre d'un autre type qu'int, double ou string aussi.
* `return -code continue` : la règle ne s'applique pas, la recherche continue (et finit, faute
  de mieux, par `unknown way`). Une erreur de la lambda remonte telle quelle.
* Ordre dans `TclTypeCast` : même type ; règle spécifique ; chaîne des bases ; variante ;
  **règle de famille, seulement entre deux instances différentes** (deux nominaux sur la même
  instance, `couple` et `energie`, n'en relèvent pas) ; conversion entre génériques ;
  conversions intégrées si un côté est structurel ; sinon `unknown way`.
* `TclType.family` / `familyArgs` : posés à l'instanciation d'un `type def` paramétré (pas d'un
  `type alias`, pas de `bitarray`), hérités par un nominal défini sur l'instance
  (`type def mm {grandeur(L, 0.001)}`). `TypeGeneric.paramsType` : le produit, construit à la
  première règle qui en a besoin.
* Les conversions restent explicites : `set {m}b $a` (a : mm) est refusé ; `set b {m}$a` ou
  `type cast m $a` convertissent.

## layout/filebinary.tcl : remplissage en octets (18 octobre)

`putPad`, le remplissage des chaînes de taille fixe, le terminateur NUL et le comblement avant
`at(...)` ajoutent maintenant `binary format x$n` au lieu de `string repeat \x00 $n`. Mêler un
tableau d'octets et une chaîne dans `append`, puis appeler `string length`, fait reconvertir toute
la sortie par Tcl 9 (y compris un tclsh d'origine) : l'encodage devenait quadratique (cstruct,
1000 mesures : 222 ms, 33 ms après correction ; 2000 mesures : 786 ms, 98 ms). Octets produits
inchangés ; tests et exemples identiques. Mesures complètes : `exemples/cstruct/LISEZMOI.md`.

## Codec C pour `type binary encode` (patch types-binaires-codec-c)

Après `types-binaires-convert-famille`. Ne touche que `tclTypes.c` (copié par `maj.sh` ; le
patch sert d'historique). Prototype : une voie rapide en C, essayée avant la couche Tcl
(`layout/filebinary.tcl`) ; dès qu'un cas sort de son domaine, elle rend la main
(`TCL_CONTINUE`) et la couche Tcl fait le travail et produit les messages d'erreur, comme avant.

Domaine couvert (les dispositions fixes, celles de `cstruct`) :

* scalaires entiers signés ou non de 1, 2, 4, 8 octets ; flottants de 4 et 8 octets ;
  boutisme du type ou de `-endian little|big` ;
* produits (remplissage avant chaque champ et en fin de produit, selon `-align
  packed|natural|2^k`) ;
* listes de taille fixe `list(3).T`, et `list($n).T` où `n` est un champ déjà écrit du produit
  englobant (la valeur de `n` doit valoir le nombre d'éléments, sinon couche Tcl et son message).

Hors domaine, donc couche Tcl : champs de bits, `at(...)`, liens, sommes balisées, chaînes,
tailles calculées par une expression, `-dedupe` et toute autre option, u64 au-delà de 2^63,
valeur non annotée ou annotée d'un autre type que celui demandé. `set ::tcl_typebinaryc 0`
désactive la voie C (comparaison, mise au point).

Vérifications : octets identiques à la couche Tcl en `natural`, `packed`, `-endian big -align
4` et `-align 2`, et identiques aux octets écrits par `cstruct.c` ; tests 27 OK (et les tests
sans `.out` inchangés) ; exemples identiques ; `cstruct/test.tcl` identique sous
AddressSanitizer, sans erreur.

Mesures (cstruct, `-align natural`, µs par encodage) :

| n | couche Tcl | codec C | rapport |
|---|---|---|---|
| 1 000 | 31 000 | 80 | × 380 |
| 10 000 | 333 000 | 1 300 | × 250 |

`binary format` écrit à la main (Tcl classique, sans contrôle) : 330 µs pour 1 000 mesures ; le
codec C, qui contrôle en plus le type, est 4 × plus rapide. Restent à faire : le décodage en C
(même domaine), mettre en cache le type analysé par `TclTypeParse` (refait à chaque appel) et
éviter `Tcl_ListObjGetElements` sur les listes abstraites annotées.

## Codec C pour `type binary decode` (patch types-binaires-codec-c-decode)

Après `types-binaires-codec-c`. Touche `tclTypes.c`, `tclTypes.h`, `tclAnnotObj.c` (sources de
types, copiées par `maj.sh`). Même domaine que l'encodeur (dispositions fixes), avec un
résultat de type produit ; options `-offset`, `-endian`, `-align`, `-strictpad`,
`-linkdepth` (sans effet sans lien) et `-consumed`. Tout le reste, et toute erreur (données
tronquées, remplissage non nul sous `-strictpad`, taille absurde), passe à la couche Tcl, qui
donne ses messages. Le décodage n'a d'effet qu'en cas de succès (`-consumed` compris).

La valeur produite a exactement la forme de celle de la couche Tcl (comparée par ses
représentations internes, pas seulement son texte) : scalaires nus, listes nues, produits
annotés de leur type, sauf un produit valeur d'un champ, rendu nu comme le fait le
constructeur. Deux fonctions sont ajoutées dans `tclAnnotObj.c` : `TclTypeRefFor` (référence
de type, gardée pour toute une série de valeurs) et `TclNewAnnotatedOwn` (valeur typée qui
prend son contenu, sans copie).

Test : `t51_codec_c.tcl` fait chaque cas deux fois (couche Tcl, voie C) et compare octets,
valeur, types des parties, `-consumed` et messages d'erreur. Tests, exemples et
`cstruct/test.tcl` identiques avec et sans la voie C ; sous AddressSanitizer, sans erreur ;
mémoire stable sur 200 décodages de 10 000 mesures.

Mesures (cstruct, 10 000 mesures, `-align natural`) : décodage 3,0 à 3,7 ms en C, contre
485 ms par la couche Tcl (× 130 à 160) et 7,3 à 10,5 ms par `binary scan` écrit à la main.

Profil du décodage : 40 % du temps est la libération du résultat précédent ;
`FreeAnnotated` y cherche chaque valeur annotée dans la table des cellules « soi-même »
(`SelfCell`), dès que cette table n'est pas vide. Un drapeau sur la valeur éviterait cette
recherche, pour toutes les valeurs annotées et pas seulement ici.

### layout/filebinary.tcl : `typed` (résultat d'un type non produit)

`type binary decode {list(2).u8} ...` échouait (`expected integer but got "1 2"`) : `typed`
passait la liste en UN argument à `set {T}V`, qui en faisait un seul élément, et par son
texte, ce qui perdait le type des éléments. La liste est maintenant passée élément par
élément, par variable. Seule la sortie de `t51` (cas ajoutés) en dépend ; tests et exemples
identiques.

## Constructeur par chemin et lecture imbriquée (patch types-chemins-construction)

Après `types-binaires-codec-c-decode`. Touche `tclTypePath.c`, `tclTypedVar.c`, `tclTypes.h`
(sources de types, copiées par `maj.sh`). Répond à l'ambiguïté de la notation
positionnelle/nom-valeur : le choix des champs se lit dans le SOURCE, plus dans les données.

    type def rect {{{int}x; {int}y}A ; {{int}x; {int}y}B }
    set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4          ;# une valeur par feuille
    set {rect}R((A,B).(x,y))     1 2 3 4          ;# idem (produit de groupes)
    set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}      ;# une valeur par élément du groupe
    set {rect}R((A,B).(x,y))     {{1 2} {3 4}}    ;# une valeur pour tout le chemin
    set {paire}p(age) 12                          ;# les autres champs : leur défaut
    set R((A,B).(x,y))                            ;# lecture : {1 2} {3 4}

* **Forme d'un chemin.** L'ensemble de chemins compilé porte un arbre : `,` fait un groupe,
  les parenthèses le ferment (`(a,b),c` donne `{{a b} c}`), `.` remplace chaque feuille de
  gauche par la forme de droite. C'est le modèle de la page « A better way to do indexation ».
* **Lecture imbriquée.** Une lecture multiple rend des listes imbriquées selon la forme :
  `R((A,B).(x,y))` donne `{1 2} {3 4}` (avant : `1 2 3 4`). Un intervalle `a:b` est un
  sous-groupe : `L(0:2,4)` donne `{10 20 30} 50` (avant : `10 20 30 50`, sortie de
  `t5_index` modifiée d'autant). Limite : la taille d'un intervalle n'est connue qu'à la
  lecture, donc `(0:1).(x,y)` rend la colonne des x puis celle des y.
* **Répartition des valeurs** (écriture et construction), par leur NOMBRE seulement : une par
  feuille, une par élément du groupe de tête, ou une seule pour tout le chemin ; chaque valeur
  a la forme de ce que sa lecture rendrait. Les nombres ne coïncident que quand les lectures
  coïncident : aucune ambiguïté. `set R(chemin) [set R(chemin)]` marche toujours.
* **Construction.** `set {T}R(chemin) v...` (T produit) groupe les chemins par champ ; un
  groupe qui descend construit le produit du champ ; chaque niveau passe par le constructeur
  en nom/valeur imposé (`TclBuildProductNamed`, sans essai positionnel) : défauts, contrôles,
  « field "y" of … has no value and no default », « unknown field », « assigned more than
  once », champ intermédiaire qui n'est pas un produit, pas d'intervalle ni d'étiquette.
* **Espaces** : permis dans `{T}R(chemin)` (mot annoté) et dans `$R(...)`, `${R(...)}` ; dans
  un mot nu non annoté `set R(A.x, A.y) …`, Tcl coupe aux espaces : écrire
  `set "R(A.x, A.y)" …` ou sans espaces. Voir `index-chemins-noyau.patch` ci-dessous.

Test : `t52_constructeur_chemin.tcl` (+ `.out`). Tests et exemples identiques sauf
`t5_index` (ci-dessus) ; AddressSanitizer sans erreur ni fuite dans le nouveau code ; mémoire
stable sur 200 000 constructions.

## Opérateur de dépliage `*` dans les chemins (patch types-chemins-depliage)

Après `types-chemins-construction`. Touche `tclTypePath.c`. `*X` fond le groupe (ou l'intervalle) X
dans le groupe qui le contient ; précédence entre « , » et « . » (`*A.(x,y)` déplie
`A.(x,y)` en entier) :

    set {list.int}L 10 20 30 40 50
    set L(0:2,4)            ;# {10 20 30} 50
    set L(*0:2,4)           ;# 10 20 30 50
    set R(*A.(x,y),B.x)     ;# 1 2 3
    set R((A,B).*(x,y))     ;# 1 2 3 4
    set {rect}Q((A,B).*(x,y)) {5 6 7 8}   ;# construction : une valeur, à plat

Le dépliage change la forme, donc aussi le nombre d'éléments que compte la répartition des
valeurs. À la racine, sans groupe parent, `*` n'a pas d'effet. `1*2`, `*` seul : erreurs
de syntaxe. Test : `t52_constructeur_chemin.tcl` (cas ajoutés).

## Index de `$nom(...)` et valeurs substituées atomiques (patchs index-chemins-noyau, types-chemins-atomes)

Après `types-chemins-depliage`. `index-chemins-noyau.patch` est un patch du NOYAU
(`tcl.h`, `tclParse.c`, `tclCompile.c/.h`, `tclCompCmds.c`, `tclExecute.c`, `tclVar.c`,
`tclTest.c`), appliqué par `maj.sh` sans fuzz ; `types-chemins-atomes.patch` touche
`tclTypePath.c`, `tclTypes.h`, `tclTypedVar.c` (copiés par `maj.sh`, le patch sert
d'historique). `tcl.h` change : tout est recompilé.

**1. `Tcl_ParseVarIndex` et `TCL_TOKEN_INDEX` (tclParse.c).** `Tcl_ParseVarName`, sur la
`(` qui suit un nom de variable, appelle `TclParseVarIndex` : grammaire de l'index jusqu'à
la `)` équilibrée — parenthèses imbriquées, étiquettes `"..."` appariées (les parenthèses y
sont du texte), substitutions `$x`, `[cmd]`, `[(expr)]`, antislashs ; accolades refusées.
Si l'index n'est pas bien formé selon cette grammaire, REPLI sur la règle de Tcl 9, avec ses
messages : dans `"c $dd("`, le `"` ferme le mot et `info complete` reste vrai. Seulement
quand la syntaxe étendue sert, les composants sont enveloppés dans un jeton
`TCL_TOKEN_INDEX` (1024, tcl.h ; « index » dans `testparser`) ; un index valide en Tcl 9 garde
exactement ses jetons. Les mots nus ne changent pas : `puts a(b c)` reste deux mots.

    list $R(A.(x,y)) $R((A,B).(x, y))   "<$L(0,"|",1)>"   $tab(b(c))

**2. Valeurs substituées atomiques.** Une valeur substituée dans l'index est UNE feuille du
chemin, jamais relue comme un chemin. L'index qui en contient est un objet « pathindex » :
sa chaîne est l'index (un tableau ordinaire ne voit qu'elle), sa représentation interne la
liste des portées substituées. Produit par la nouvelle instruction `indexConcat` (compilé :
`TclCompileIndexTokens`, utilisé par `TclCompileVarSubst` et `TclPushVarName`) et par
`TclSubstTokens` (non compilé) ; lu par le parseur de chemins.

    set k file.txt ; $D($k)          ;# la clé « file.txt » (avant : D.file.txt)
    set n 1 ; $D(f_$n)               ;# collée à du texte, la valeur prolonge le nom
    set k A,B ; $R($k.x)             ;# rect has no field "A,B"
    set p {[exec …]} ; $L($p)        ;# rien n'est exécuté : bad index "[exec …]"
    set idx {0 2} ; $L(*$idx)        ;# * déplie une valeur liste : 10 30

La DOUBLE SUBSTITUTION disparaît : avant, une donnée contenant `[script]` était exécutée par
le parseur de chemins. Limite : l'atomicité suit les jetons, donc `$R(...)` et `set R(...) v`
compilé ; une écriture multiple `set R($k.x,$k.y) 1 2` et le constructeur `set {T}R(...)`
reçoivent une chaîne déjà substituée (utiliser `'$k'` pour forcer une clé).

**3. Le chemin ne substitue plus rien.** Même règle qu'un tableau : `${a($k)}` et
`${R($k.x)}` sont littéraux. Dans un chemin entre accolades ou dans `${...}`, `$` et `[`
donnent l'erreur « a path does not substitute "$": Tcl substitutes $x and [cmd] in
$name(...) and in bare words, not inside braces or ${...} ». Les étiquettes sont
littérales. Un chemin construit dans une variable se lit en NOMMANT la variable avec
lui : `set ref trame($chemin); set $ref` (exemple `reseau_couches.tcl`, adapté).
`t5_index`, `t6_substitutions` réécrits en conséquence.

**4. `${R(chemin)}`.** `Tcl_ObjGetVar2` et `INST_LOAD_STK` : quand le nom entier porte
l'index (part2 absent) et que la variable est un scalaire typé, le chemin littéral est lu ;
sinon le message de Tcl est inchangé. `${R(A.x, B.y)}`, `"<${L(0,"|",1)}>"`.

**5. Mot annoté avec espaces.** `{T}nom(...)` va jusqu'à la parenthèse équilibrée, espaces
et tabulations compris, sur une ligne : `set {rect}R(A.x, A.y, B) 1 2 {3 4}`. Tcl d'origine
refuse tout mot `{T}suite` : aucun script valide ne change de sens. Mots nus non annotés :
inchangés.

**Vérifications.** Tests de Tcl `parse`, `parseExpr`, `parseOld`, `subst` (avec `tcltest`,
257 tests de `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`,
`append`, `lset`, `dict`, `info` : résultats identiques à la base. Équivalence avec un
Tcl 9 d'origine sur 18 000 cas générés (`$z(…)`, `"<$z(…)>"`, `set z(…)`, et
`info complete`) : aucun résultat valide changé ; seules des erreurs deviennent autre chose
(`a""end)[` : l'index `a""end` est valide, le `[` qui suit rend la commande incomplète).
Tests du projet 30 OK ; AddressSanitizer sans erreur ; mémoire stable.

**Fuites corrigées au passage** (elles existaient en v8) : `Tcl_DictObjPut` ne garde pas la
clé qu'on lui passe quand elle existe déjà. L'écriture `set D(a) 3` d'un dictionnaire typé
(`SetIn`) et la construction avec clés en double (`BuildDict`) perdaient un objet à chaque
fois (~75 octets ; 31 Mo pour 200 000 constructions) : la clé est maintenant rendue.

## types-produit-etiquettes.patch  (v13, à appliquer après types-chemins-atomes.patch)

Sources de types seulement (`tclTypedVar.c`, `tclAnnotObj.c`, `tclTypes.h`, livrées entières
dans `typeparse/`). **La forme nom/valeur nue d'un produit est supprimée**, et les champs se
nomment par **étiquette** :

    type def paire {{string}nom; {int}age ?= 0}
    set {paire}p {nom}jean {age}12      ;# jean 12
    set {paire}p {age}13 {nom}paul      ;# ordre libre
    set {paire}p {nom}zoe               ;# zoe 0 (défaut)
    set {paire}p jean 14                ;# positionnel, inchangé
    set {paire}p(age,nom) 15 lea        ;# chemin, inchangé

* **Constructeur** (`BuildProductEx`) : copie d'un produit du même type (1 argument),
  `{T.champ}valeur`, *n* valeurs pour *n* champs = positionnel, 0 argument = tous les
  défauts, construction nommée interne (chemin, étiquettes). Tout le reste :
  « wrong # args: T needs n values; to name fields (the others take their default), write
  {field}value or set {T}var(field,...) value... ». L'essai des deux lectures et l'erreur
  « ambiguous arguments » disparaissent.
* **Étiquettes** (`TclSetAnnotatedCmd`) : pour un produit, l'annotation en attente la plus
  récente de chaque argument est comparée aux noms de champs ; le contexte du produit passe
  avant les types globaux (`type def age int` ne change rien). Tous étiquetés : la valeur
  sans son étiquette (`TclAnnotPopTop`) est résolue dans le contexte du type du champ, puis
  `TclBuildProductNamed` (doublons, champs sans défaut : messages habituels). Aucun :
  positionnel. Mélange : « can't mix values named by a field annotation ({field}value) and
  positional values in a T constructor » ; une annotation inconnue à côté d'étiquettes :
  « unknown field "x" in T ».
* Une valeur qui porte son étiquette en variable compte : `set v {age}21` puis
  `set {paire}p {nom}max $v`. `{age}$w` avec `w` = `{mm}12` : l'étiquette est la plus
  récente, `mm` reste une note.
* **Usages convertis** : décodeur de `layout/filebinary.tcl` (produit et défaut d'un lien :
  construction par chemin `set {T}V(c1,c2,...) $fv0 $fv1 ...`), `spaceMake` (étiquettes),
  exemples (`png`, `gif`, `elf`, `pe`, `executable`, `complexes`, `reseau_couches`,
  `cstruct`, …) et tests. `t23_ambiguite_produit` réécrit (avec `.out`) : l'ancien piège
  `set {png}p chunks {a b}` est désormais positionnel, sans erreur ni devinette. Nouveau
  `t54_etiquettes_champ`.

**Vérifications.** Tests du projet 32 OK (dont t23 et t54 nouveaux avec `.out`), les autres
sans erreur ; sorties des exemples identiques à la v8 sauf les messages d'erreur attendus
(et ce qui dépend du binaire ELF lu) ; AddressSanitizer sans erreur ; mémoire stable sur
1 000 000 de constructions étiquetées ; tests de Tcl `set`, `var`, `dict`, `info`, `compile`
identiques à la base.

## v14 — contrat par le nom (20 octobre 2026)

Sources de types livrées entières : `tclTypedVar.c`, `tclTypes.c`, `tclTypes.h`, `tclAnnotObj.c`.
Patch noyau : `contrat-par-nom.patch` (`tclInt.h`, `tclVar.c`, `tclExecute.c`).

- Plus d'adoption : une variable non déclarée reçoit une valeur typée, jamais de contrat.
  Bit `VAR_CONTRACT`, posé seulement par une déclaration (`set {T}v`, `x is T = v`).
- Déclaration dans toute commande qui écrit : `lappend {list}M x`, `lset {T}M`, `incr {int}n`,
  `dict set {dict.int}D`… redéclarent la variable dans `TclObjLookupVarEx` (comme `set {T}M $M`).
- Une modification est un calcul : sans contrat, `lappend`, `lset`, `incr`… rendent une valeur
  ordinaire et n'échouent jamais à cause du type de la valeur. Sous contrat, le résultat est
  vérifié au rangement et jamais modifié en place : un refus laisse la variable intacte.
- Tests : `t19` (variable de boucle libre après la boucle), `t48` (`lset` sur une copie),
  `t51` (valeur incohérente fabriquée par un cast) mis à jour.

## v15 — collections et éléments (20 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypePath.c`, `tclTypes.c`. Patch noyau : `elements-atomiques.patch` (`tclCompExpr.c`).

- Règle B : seules les collections (`list`, `dict`) se déplient dans la vue liste (`llength`, `lindex`,
  `foreach`, `lassign`, `{*}`). Un scalaire, un produit, un lien est UN élément ; une somme prend
  la nature de sa variante active. `llength $P` vaut 1 pour un `Point`.
- Chemin `*` seul : toutes les parties dans l'ordre (champs d'un produit, éléments d'une liste,
  valeurs d'un dict) : `$P(*)`, `{*}$P(*)`, `$T(*.y)`. Lecture seule.
- Une lecture multiple de types différents rend une `list.any` (chaque élément garde son type).
- `(x,y) = $P` dans expr lit le contenu (les champs), y compris imbriqué : `((a,b),(c,d)) = $T`.
- `::tcl::unsupported::content V` : contenu profond d'une valeur annotée (champs, éléments décodés
  rendus bruts) ; sert au tuple et à l'exemple `cstruct` (voie objet), à recompiler.
- Exemples réécrits : `complexes.tcl`, `complexes_tip759.tcl` (`lassign $a(*) …`), `arbre_graphe.tcl`
  (`$t(value)`, `$t(children)`), `hypergraphe.tcl` ; tests `t10`, `t22`, `t30`, `t51`, `t52`.

### v15b — cast d'un produit vers une collection (22 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypes.c`, `tclTypes.h`. Patch noyau : `cast-collection.patch` (`tclParse.c`), après `elements-atomiques.patch`.

- Cast de base de tout produit vers une collection : `{list}$p` rend la liste de ses champs, nus ;
  `{list.any}$p` les rend annotés de leur type (comme `$p(*)`) ; `{list.T}$p`, `{list(N).T}$p`
  les valident ; `{dict}$p`, `{dict.any}$p`, `{dict.T}$p` rendent le dict nom -> champ. Une somme part
  de sa variante active. La chaîne ne change pas.
- Liste nominale -> liste structurelle (et dict -> dict) : `polygone` -> `list.point`, `list(4).point`,
  `list` (éléments nus) ; deux nominaux sans lien restent refusés.
- `{list}$p` sur une valeur typée est un cast immédiat, au moment de l'annotation (`::tcl::annotate`
  et `TclSubstTokens`) : `foreach {x y} {list}$p`, `lassign {list}$p x y`, `dict for {k v} {dict}$p`.
  Un nom inconnu reste une note, comme avant.
- Test `t55_cast_collection`. Tests du projet, exemples et tests de Tcl `parse`, `subst`, `list`,
  `lassign`, `foreach`, `dict` : identiques à la v15.

### v15c — conversions EIAS (23 octobre 2026)

Source de types : `tclAnnotObj.c`. Patch noyau : `eias-string-is.patch` (`tclCmdMZ.c`), après `cast-collection.patch`.

- Cause : les conversions du noyau (`Tcl_GetDoubleFromObj`, `Tcl_GetIntFromObj`, nombres de `expr`…)
  passent par `TclAnnotUnwrap`, qui décidait selon le GENRE du type (`KindAllows`) et non selon la
  chaîne : une énumération `{"3.14159"}`, un `{string}` qui vaut `12`, un produit `{{int}i}` étaient
  refusés comme nombres (`cannot use non-numeric string`, `can't use pi value as floating-point number`).
- Une valeur de genre chaîne (string, littéral d'énumération, nominal sur string) se lit comme un nombre
  si son texte en est un ; sinon le message habituel de Tcl.
- Un produit d'UN SEUL champ se lit comme ce champ pour les conversions scalaires (nombre, entier,
  flottant, booléen), y compris imbriqué. La vue liste garde la règle B (`llength` vaut 1).
- `string is` juge la chaîne, quel que soit le type (`string is list $P` vaut 1 pour un point).
- Inchangé : un produit de plusieurs champs, une liste ou un dict restent refusés comme nombres ; une
  écriture sous contrat reste validée (`incr` d'une énumération hors de ses variantes : erreur).
- Test `t56_eias_conversions`. Tests du projet et exemples identiques ; tests de Tcl `expr`,
  `expr-old`, `string`, `stringObj`, `incr`, `obj`, `parse`, `subst`, `list`, `lassign`, `foreach`,
  `dict` : 5110 tests, mêmes résultats que la base.

### v15d — somme choisie par un champ, S($champ) (23 octobre 2026)

Sources de types : `tclTypes.c`, `tclTypes.h`, `tclTypePath.c`, `tclTypedVar.c`, `tclAnnotObj.c`. Pas de patch noyau.

- Cause : le décodeur choisissait la bonne variante, mais un produit range ses champs nus ; à la
  relecture (`$r(corps)`, validation du produit, construction), la variante d'un champ `corps($genre)`
  était redevinée d'après la forme de la valeur, sans regarder `genre` : `ambiguous variant` quand
  deux variantes ont la même forme (`{u8; u8}`), ou pire, la mauvaise variante en silence
  (`3 {10 11}` lu en variante 1 dans `t35`).
- `TclTypeTagVariantFrom` / `TclTypeTagVariant` (`tclTypes.c`) : la variante dont le nom est la valeur
  du champ sélecteur (comparaison de chaînes, puis numérique). Utilisée par la validation des produits,
  la lecture et l'écriture par chemin (`WrapSlotHint`, `tclTypePath.c`) et la construction
  (`TclTypedConformHint`, `tclTypedVar.c`). Construction nom/valeur en deux passes : le sélecteur peut
  être donné après le champ qu'il choisit.
- Changer le sélecteur par chemin revalide le champ selon la nouvelle variante.
- `TclTypeTagCheck` : une valeur qui porte elle-même une variante (`{4}{9 1}`, ou une valeur typée
  `c_pwm`) doit concorder avec le sélecteur, sinon `selector genre is 1 but field corps of req holds
  variant 4 of corps($genre)` dès la construction (avant : à l'encodage, ou relu en silence dans
  l'autre variante).
- `ClassifyAnnot` et le cast vers une collection sauvent et restaurent tout l'état de l'interpréteur
  (`Tcl_SaveInterpState`) : une analyse de type ratée ne laisse plus d'`errorInfo` (`{1}$p` laissait
  « unknown type "1" », qui remplaçait le message d'une erreur suivante au niveau global).
- Tests : nouveau `t57_selecteur` ; `t35` (variante `.3` au lieu de `.1`, refus à la construction) et
  `t37` (même message, sans le préfixe `type binary:`) régénérés. Tests de Tcl (22 fichiers, 6931
  tests) : mêmes résultats que la base.

### v15e — incr, append, lappend, dict set par chemin (23 octobre 2026)

Sources de types : `tclTypePath.c`, `tclTypes.h`. Patch noyau : `chemins-modification.patch`
(`tclVar.c`, `tclExecute.c`), après `eias-string-is.patch`.

- Cause : seuls `Tcl_ObjGetVar2`, `Tcl_ObjSetVar2` et les instructions de lecture et d'écriture
  d'élément connaissaient les chemins. `incr v(age)`, `append v(nom) X`, `lappend v(notes) 3`,
  `dict set v(d) k 1` cherchaient un élément de tableau et échouaient (`variable isn't array`),
  partout. (Le bug noté « set v(age) dans une procédure » n'existait plus depuis la v13.)
- `TclTypedVarModifyElementVar` / `TclTypedVarModifyElement` (`tclTypePath.c`) : lire la partie par
  chemin, la modifier (ajout de texte, d'éléments, incrément), l'écrire par chemin : le contrat et le
  type de la partie sont vérifiés comme pour `set v(age) ...` (`lappend c(notes) x` sur un
  `list.int` : `expected integer but got "x"`).
- Branché dans les commandes (`TclIncrObjVar2`, `append`, `lappend`, `Tcl_ObjSetVar2` avec
  `TCL_APPEND_VALUE` / `TCL_LIST_ELEMENT`, et `Tcl_ObjSetVar2` sur un nom `v(chemin)` en un mot, pour
  `dict set`) et dans les instructions compilées (`INST_INCR_ARRAY*`, `INST_INCR_*_STK`,
  `INST_APPEND_ARRAY*`, `INST_LAPPEND_ARRAY*`, `INST_LAPPEND_LIST_ARRAY*`) : même comportement
  compilé ou non.
- Les vrais tableaux et les variables non typées ne changent pas.
- Test `t58_modif_chemin`.

**Vérification de v15b à v15e, refaite contre une vraie v15.** L'arbre de référence de mes comparaisons
précédentes compilait, par son Makefile copié, les sources de l'arbre de travail : il ne servait pas de
référence. Refait contre une v15 recompilée depuis ses propres sources : tests du projet et exemples
identiques hormis les écarts attendus (`t35`, `t37`, nouveaux `t55` à `t58`, exemples qui lisent le
`tclsh` lui-même, temps de `bench_matrices`) ; 29 fichiers de tests de Tcl (7542 tests) : mêmes
échecs, test par test.

### v15f — un nominal de collection entre dans sa base (23 octobre 2026)

Source de types : `tclTypes.c`. Pas de patch noyau.

- État avant : depuis la v15b, les casts explicites marchaient (`type cast list.point $P`,
  `{list.point}$P`), et l'affectation d'une variable aussi (`set {list.point}L $P`, par la règle de
  dépliage). Restait le champ d'un produit : `set {polyC}c 4 $P` avec `{list($n).point}pts` donnait
  `type mismatch: expected list($n)<point> but got polygone`.
- Cause : `TclTypeEnters` suit la chaîne `under` d'un nominal, qui n'existe que pour un nominal de
  scalaire (`px` -> `int`). Un `type def polygone list.point` n'en a pas : sa base, sa propre
  structure, était invisible.
- Correction : un nominal de liste ou de dict entre dans une collection STRUCTURELLE de même genre
  dont les éléments l'acceptent (`list.point`, `list(4).point`, `list($n).point`, `list`) ; le nombre
  d'éléments est vérifié ensuite. Deux nominaux restent distincts (`nuage` refuse un `polygone`),
  et deux tailles fixes différentes aussi (`list(4).point` refuse un `list(3).point` nominal).
- `geometrie/geo_c.tcl` garde son passage par le contenu, pour une autre raison : `point` et `pointC`
  sont deux nominaux distincts (commentaire ajouté).
- Test `t59_nominal_base`.

### v15g — performances de l'étape annotée (23 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypes.h`, `tclTypePath.c`. Pas de patch noyau.

Mesuré avec `exemples/geometrie/bench_parties.tcl` (polygone de 10 000 points) et callgrind. Avant :
l'aire annotée prenait 50,9 ms contre 4,0 ms en Tcl nu ; la lecture `$a(x)` coûtait environ 1 µs,
la redéclaration `P is polygone = $p` 4 µs (négligeable).

- Lecture par chemin d'un seul nom (`$a(x)`, `$L(3)`) : un seul pas, sans compiler l'index
  (`CompileIndex` faisait 21 % du temps).
- Typeref mis en cache dans le descripteur de type (`refCache`) : plus de clé `snprintf` ni de
  hachage à chaque valeur créée (13 %) ; les éléments enveloppés par `lindex` le partagent aussi.
- `TclAnnotUnwrap` : un scalaire typé sans note (int, double, string) donne son contenu sans
  résolution (les opérandes de `expr`).
- `TclAnnotSettle` : rien à faire pour une valeur au type résolu et sans note (la plupart des
  affectations `a = lindex ...`).
- Libération : la table des cases d'auto-élément n'est consultée que si une case a été créée.
- Lecture d'une partie : le contenu est partagé au lieu d'être copié (une valeur est immuable ; qui
  la modifie travaille sur une copie).

| polygone de 10 000 points (µs) | v15 | v15g |
|---|---:|---:|
| aire annotée (Tcl nu : environ 4 000) | 50 867 | 14 110 |
| `$a(x)` + `$a(y)` sur 10 000 points | 23 241 | 6 117 |
| `lindex` sur 10 000 points typés | 3 086 | 2 166 |
| `foreach` + chemins | 22 395 | 5 611 |

`geometrie/bench.tcl`, étape 2 : aire 39,8 -> 15,7 ms, englobant 36,1 -> 11,8 ms, dedans_tous
841 -> 314 ms, Bézier 149 -> 99 ms, périmètre 254 -> 202 ms (dominé par les appels de procédures
typées : `geo::distance` -> `geo::vec`, avec leurs déclarations ; prochaine cible).

Vérifié contre la v15 : tests du projet, exemples (dont `cstruct`, `arduino`, `geometrie/verifier.tcl`)
identiques ; 23 fichiers de tests de Tcl (6615 tests), mêmes échecs ; valgrind sans erreur sur
`geometrie.tcl`, `t22`, `t30`, `t52`, `t55`, `t57`, `t58`.

### v15h — construction par chemin d'une liste ou d'un dict (23 octobre 2026)

Sources de types : `tclTypePath.c`, `tclTypedVar.c`. Pas de patch noyau.

- Cause : la construction par chemin (`set {T}v(a,b) x y`) n'existait que pour les produits. Pour
  tout autre type, le nom `AB(0,1)` partait dans le chemin des tableaux : Tcl créait le tableau `AB`,
  puis échouait (`no such element in array`), et le `set {segment}AB $A $B` suivant donnait
  `typed variables must be scalars`.
- Liste : construite par ses indices, tous de 0 à N-1, dans un ordre quelconque (une liste n'a pas
  de défaut) : `set {segment}AB(0,1) $A $B`, `set {segment}S(1,0) ...`, chemins profonds
  `S(0.x,0.y,1.x,1.y) 1 2 3 4`. Trou, doublon, nom qui n'est pas un indice : message clair.
- Dict : construit par ses clés : `set {dict.int}D(a,b) 1 2`.
- Autre type (scalaire...) : `set {int}A(k) 5` refusé AVANT toute recherche de variable, rien n'est
  créé : `a typed variable is a scalar, not an array element; a constructor path (k) names fields of
  products, indices of lists or keys of dicts, and int is none of these`.
- Tests : nouveau `t60_construction_liste` ; `t52` régénéré (message élargi aux listes et dicts).

### v15i — `set` à un seul argument et nom substitué (23 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypes.h`. Patch noyau : `set-nom-substitue.patch`
(`tclVar.c`), après `chemins-modification.patch`.

- Le cas noté au planning (`set $r`, `$r` un lien `var<int>`) ne se reproduisait plus depuis la v13 :
  il lit bien `::A`. Restait un cas voisin : `set $nm` avec `nm` qui vaut `{mm}n` (une note) donnait
  `unknown type "mm"` : le nom substitué était pris pour une déclaration, par `set` (constructeur) et
  par la redéclaration de la v14 dans `TclObjLookupVarEx`.
- Règle : seule une annotation ÉCRITE dans le source déclare (`set {pt}p` construit avec les défauts,
  `lappend {list}M x` redéclare). Un `set` à un seul argument dont le nom substitué porte un type ou
  des notes lit ; une lecture (`"read"`) par un tel nom ne redéclare rien.
- `TclAnnotIsSourceRaw` (`tclAnnotObj.c`) : l'annotation est un texte de type brut, pas encore résolu.
- Inchangé : `set {rect}Q(A, $k) ...` (annotation dans le source, suffixe substitué) construit
  toujours ; `set $nm 9` écrit.
- Test `t61_set_nom_substitue`.

### Liens `var` : vérification du 23 octobre

- `lset L 1 C` et `lappend L C` rangent bien le nom canonique (`::C`) depuis la v14 ; `lappend L 5`
  range `::5`, comme la construction (`5` est un nom de variable Tcl valide) ; un nom vide est refusé.
- Lecture d'un lien seul documentée dans `doc/ANNOTATIONS.fr.md` (§ 5.3) ; limite notée : un lien vers
  une variable locale (`frame:1:x`) se suit par chemin, pas par `set $r`.
- Test `t62_liens_ecriture`.

### v15j — `;` et `,` mêlés (23 octobre 2026)

Source de types : `tclTypeParse.c`. Pas de patch noyau.

- Avant : `{int}a; {int}b, {string}c` et `{int}a, {int}b; {string}c` étaient acceptés et donnaient
  un champ sans nom (une somme anonyme, lue seulement par son numéro), avec un regroupement différent
  selon l'ordre ; `{int}a; {{int}b, {string}c}` aussi, alors que `{int}a; {int}` était refusé.
- Maintenant : `;` et `,` au même niveau sont refusés (`mixing ";" and "," at the same level: group
  and name the inner part, as {int}a; {{int}b, {string}c}u, or define it with type def`) ; dans un
  produit, un groupe sans nom est refusé comme tout champ sans nom. `SumToField` (la somme devenue
  champ anonyme) est retirée.
- Inchangé (décision du 23 octobre) : une somme garde ses variantes anonymes (`{int}, {string}`,
  `{{int}a; {int}b}, {string}c`).
- Test `t63_separateurs`. Aucun test ni exemple du projet n'utilisait les formes refusées.

### v15k — lire une variante inactive (23 octobre 2026)

Source de types : `tclTypePath.c`. Pas de patch noyau.

- Avant : `$v(s)` sur un `S` dont la variante courante est `i` tentait de caster la SOMME vers
  `string` : `unknown way to cast the value from S to string`.
- La règle (lire une autre variante = cast explicite) part maintenant de la valeur de la variante
  COURANTE : `{i}5` lu par `$v(s)` donne `5`, de type `string`. Sans cast possible :
  `the current variant of U is "n", not "p", and its value "3" can't be read as pt`.
- Écrire dans une partie d'une variante inactive (`set u(p.x) 1`) : `the current variant of U is "n",
  not "p": to change the variant, write the whole value ({p}...)`. Écrire la variante entière
  (`set v(s) abc`) change la variante, comme avant.
- Test `t64_variante_inactive`.

### v15l — étiquette du champ d'un autre produit (23 octobre 2026)

Source de types : `tclTypedVar.c`. Pas de patch noyau.

- Avant : `set {paire}s {aut.nom}x` donnait `wrong # args: paire needs 2 values…` ; avec
  toutes les valeurs étiquetées d'un autre produit (`{aut.nom}x {aut.age}3`), elles étaient prises
  par position, sans rien dire (`x 3`).
- Maintenant : `aut.nom names a field of aut, not of paire`, que l'étiquette étrangère soit seule,
  mêlée à des étiquettes de `paire` (`{aut.nom}x {n}3`) ou partout. Champ inexistant dans l'autre
  produit : `aut.zz: aut has no field "zz", and the value being built is a paire`.
- Inchangé : `{nom}x {n}3`, `{paire.nom}x {paire.n}3`, `x 3`.
- Tests : `t65_champ_etranger` ; `t24_produit_champ.out` régénéré (ligne 7 : le nouveau message).
- Exemple `niveau2.tcl` : le refus « selector type is 2 but … variant 1 » a lieu dès la construction
  (depuis la correction du sélecteur `S($champ)`) ; le `catch` entoure maintenant le `set`.

### v15m — variantes de même forme (23 octobre 2026)

Sources de types : `tclTypes.c`, `tclTypes.h`, `tclTypedVar.c`, `tclTypePath.c`, `tclAnnotObj.c`. Pas de patch noyau.

- Avant : `set {A}v 5` avec `A = {int}i, {int}j` prenait `i` sans rien dire. Pire, une variante choisie
  (`{j}5`) se perdait dans une liste (`set {list.A}L $v`), un produit (`set {R}r $v 1`), une
  modification (`incr v`, `set v 8`, `lset`, `set L(0) …`) et dans `type cast A {j}5` : relue `A.i`.
- Déduction (`ValidateG`, `tclTypes.c`) : parmi les variantes qui acceptent la valeur, celles qu'aucune
  autre ne bat (strictement plus spécifique). Une seule : c'est elle (`{int}i, {string}s` : `5` donne
  `i`, comme avant). Plusieurs : `value "5" fits variants i and j of A equally: write {i}5`, et
  `set` ajoute `, or set {A}v(i) 5` avec le nom de la variable. Variantes anonymes : `#1 (int) and
  #2 (int) … name the variants to choose one`. Code d'erreur `TCL TYPE AMBIGUOUS A i`.
- La variante survit : une partie de liste ou de produit dont le contenu ne redit pas la variante
  (`TclTypeSumNeedsTag`) garde son annotation, que la validation relit déjà (`TclTypedConformHint`).
- Modifications : la variante courante départage des variantes également strictes (variable :
  `MakeTyped` ; chemin : `SetIn` ; `lset` : `AnnotListSetElement`, via `TclTypeSumCurrent`). Un indice
  `-2 - v` passé à la validation code cette préférence.
- `type cast S {v}x` : l'étiquette sous le type d'une somme nomme la variante (`TclAnnotSettle`).
  Avant : ignorée (`type cast B {s}5` donnait `B.i`).
- Test `t66_variantes_meme_forme`. Les autres tests et les exemples sont inchangés.

### v15n — règles de conversion entre variantes (23 octobre 2026)

Sources de types : `tclTypes.c`, `tclTypes.h`, `tclTypePath.c`, `tclAnnotObj.c`. Pas de patch noyau.

- `type convert S.a S.b {{x} ...}` : une règle d'une variante vers une autre de la même somme (clé
  `S.a\1S.b` dans la table des conversions). Refus : sommes différentes, variante inconnue, même
  variante, variante mêlée à un type.
- `TclTypeVariantConvert` (`tclTypes.c`) : la règle ; sinon, variantes de même forme : erreur qui propose
  la règle ; sinon le cast ordinaire entre les types des variantes. Utilisée par la lecture d'une
  variante inactive (`$t(fahrenheit)`, `tclTypePath.c`, remplace le cast de v15k) et par le cast
  `{S.b}$v` (`TclAnnotSettle`), qui ignorait jusqu'ici la variante demandée (`{temp.fahrenheit}$t`
  restait `temp.celsius`).
- `type cast S.v valeur` : accepté (avant : `unknown type "v"`).
- Tranche la question ouverte de v15k : cast pour des variantes de formes différentes, règle ou refus
  pour des variantes de même forme.
- Test `t67_conversion_variantes`.

### v15o — fuite de `lset`, type perdu par `lpop` (23 octobre 2026)

Source de types : `tclAnnotObj.c`. Patch noyau : `lset-reference.patch` (tclCmdIL.c, tclExecute.c,
tclVar.c), à appliquer après `set-nom-substitue.patch` (dernier de `ORDRE.txt`).

- Cause : Tcl 9.0 n'a pas UNE convention pour la référence que rend un `setElementProc` de liste
  abstraite. `TclLsetList` (donc `lset L 0 9` compilé, et `lset L {0} 9`) et `lset` non compilé à
  plusieurs indices en ajoutent une : la valeur rendue doit être sans référence. `INST_LSET_FLAT`
  (`lset M 0 1 9` compilé) et `lpop L i` l'utilisent telle quelle : elle doit en avoir une. Notre
  `AnnotListSetElement` suivait la seconde, d'où une référence en trop sur chaque `lset` à un indice
  (~170 octets ; 17 Mo pour 100 000 `lset`). La liste abstraite de test de Tcl (`tclTestABSList.c`)
  suit la première : la changer aurait cassé `abstractlist.test`.
- Correction : une seule convention, celle de `TclLsetList` (sans référence). `AnnotListSetElement`
  la suit ; `INST_LSET_FLAT` (`tclExecute.c`) et `lpop` à indices (`tclCmdIL.c`) ajoutent la
  référence qu'ils supposaient.
- Trouvé en route : `lpop L` sans indice modifiait EN PLACE la valeur non partagée d'une liste typée ;
  la variable voyait alors une valeur sans type et perdait son contrat (`type get $L` vide). Une liste
  abstraite est maintenant copiée avant modification (`tclCmdIL.c`), comme pour `lset`.
- Et : une valeur confiée sans référence à `Tcl_ObjSetVar2` puis remplacée par sa version retypée
  n'était jamais libérée (`tclVar.c`, après `TclTypedVarPrepareWrite`) ; et en cas d'échec du
  contrat, le nettoyage visait la valeur intermédiaire au lieu de celle de l'appelant.
- Tests : `t68_lset_reference` (mesure la mémoire sous Linux). Suites Tcl `lset*`, `lpop`, `list*`,
  `cmdIL`, `abstractlist`, `lseq`, `set*`, `var`, `upvar`, `append*`, `incr*`, `trace` : 1903
  réussis / 84 sautés / 0 échec, comme la v15.

### v15p — forme compacte (prototype), `type c`, `tcl::unsupported::abi` (23 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypes.c`, `tclTypes.h`, `tclTypePath.c`, `tclTypedVar.c` ; nouveau
`typeparse/tclCompact.h` (pour les extensions, pas copié dans `generic/`). Patch noyau :
`tip759-memoire.patch` (tclCompExpr.c, tclParse.c), dernier de `ORDRE.txt`.

**`TclType` change de taille** (deux champs) : tout ce qui inclut `tclTypes.h` doit être recompilé. `maj.sh`
fait `make clean` ; à la main, faire de même.

- `tcl::unsupported::abi` : `byteOrder`, `pointer`, taille et alignement dans une structure de `i8 … i64 f32 f64
  ptr`, mesurés à la compilation (`offsetof` sur `struct {char c; T x;}`). `TclTypeAbiAlign`.
- Forme compacte (doc § 12.1) : `tcl::unsupported::compact ?on|off?`, `… info V`, `… layout T` ; variable
  d'environnement `TCL_TYPES_COMPACT=1`. Types d'objets `compactlist` (V d'une `list.T`) et `compactelem`
  (vue sur une case), enregistrés, ainsi que `annotated`. Branchements : `TclNewAnnotated` et
  `TclNewAnnotatedOwn` (création), `AnnotListIndex` (vue typée), `AnnotListGetElements` (liste gardée à côté
  des cases), `ValidateG` (cases valides par construction), `SetIn` (écriture), `ReadSteps` (lecture directe de
  `$P(i.x)`).
- `type c T …` (doc § 12.2, devenu `type {c!}export` en v15q) : en-tête C, `_Static_assert` compris. Vérifié avec gcc (C11) ; avec `-m32`, les
  assertions d'un `{int(s,32); double}` échouent comme prévu (`double` aligné sur 4).
- Exemple géométrie : `geo_types.h` est produit par `tclsh geo_types.tcl > geo_types.h` ; `geo.c` l'inclut et lit
  les points d'une forme compacte en place (aire de 10 000 points, voie objet : 564 → 29 µs).
- Tests : `t69_abi`, `t70_compact` (chaque cas dans les deux formes : identiques), `t71_type_c`. Tous les tests et
  exemples donnent la même sortie avec `TCL_TYPES_COMPACT=1` et sans.

**Bugs trouvés en route** (valgrind sur un tclsh construit avec `-DPURIFY`, où les `Tcl_Obj` passent par
`malloc` ; la plupart existaient en v15) :

- `$P(0.x)` après `set e [lindex $P 0]` : plantage ; `set P(0.x) 7.5` remplaçait alors tout le point par `7.5`.
  `lindex` enveloppe l'élément dans V, et le chemin posait une annotation sur cet objet annoté (un A dans un A,
  lu comme un seul élément). `WrapSlotHint` annote maintenant le contenu (`tclTypePath.c`).
- Message « fits variants … equally » (v15m) : la liste qui citait la valeur la libérait quand elle n'avait pas de
  référence (`tclTypes.c`, `tclTypedVar.c`).
- `x is T = v` et `set {T}x …` refusés : le drapeau du contrat était remis sur une variable déjà détruite
  (`TclTypedVarDeclare`, `TclSetAnnotatedCmd` : la variable est relue).
- `type binary decode`, voie C : fuite des champs déjà lus quand le codec passe la main à la couche Tcl
  (`BinFastDec`).
- `SetIn` : le contenu d'une partie écrite était libéré deux fois quand le conteneur ne le garde pas (forme
  compacte).
- Code TIP 759 (`tip759-memoire.patch`) : `ParseExpr` lisait `nestedPtr->term` après `TclStackFree(nestedPtr)`
  (corps `expr` de procédure) ; `TclSubstTokens` gardait la référence rendue par `Tcl_ExprObj` pour un mot `(…)`
  (fuite à chaque évaluation).

Restent (déjà en v15, hors types) : à la sortie par `exit` en plein script, quelques objets de la ligne en cours
ne sont pas libérés (`executable_pe.tcl`), sans conséquence.

### v15q — `type def {-compact}T`, `type is {compact?}`, `type {c!}export` (23 octobre 2026)

Sources de types : `tclAnnotObj.c`, `tclTypes.c`, `tclTypes.h`. Patch noyau : `compact-conversions.patch`
(tclStringObj.c, tclBinary.c), dernier de `ORDRE.txt`. `TclType` grandit encore d'un champ : `make clean`.

- `type def {-compact}T DÉF` : option (annotation locale) sur le NOM du type ; marque `compactWanted` dans le
  descripteur. Refus : type qui n'est pas une liste d'éléments à disposition (la définition est annulée, le nom
  reste libre), option inconnue, alias, famille.
- Les valeurs d'un tel type sont toujours en cases : `TclCompactFrom` normalise au lieu de refuser (`norm` dans le
  tampon, gardé par les copies et les écritures) et ne garde pas la chaîne d'origine.
- Conversions sur une copie : `TCL_ANNOT_CONV` (tclTypes.h) donne, pour un contenu compact, une copie en chaîne
  gardée à côté des cases (`TclCompactConv`) ; utilisé par `Tcl_GetCharLength`, `Tcl_GetUniChar`,
  `Tcl_GetUnicodeFromObj`, `Tcl_GetRange`, `TclStringReverse`, `Tcl_GetBytesFromObj` (patch) et par
  `TclAnnotUnwrap` pour toute demande autre que liste (dict, nombres…). `TclStringReverse` ne retourne plus en
  place le contenu d'une valeur annotée. `tcl::unsupported::content` rend une copie qui partage les cases.
  Pour l'interrupteur global aussi : `string length $P` ne défait plus la forme compacte.
- `type is {compact?}X` : X valeur en cases → 1 ; sinon, nom de type → la marque ; sinon 0. Questions inconnues
  refusées.
- `type {c!}export T …` remplace `type c` ; `{llvm!}export` : « not available yet » ; sans cible ou cible
  inconnue : refus.
- Tests : `t72_compact_type` ; `t71_type_c` passe par `{c!}export` ; `t70_compact.out` : `string length` garde
  désormais la forme compacte. `geo_types.tcl` emploie `{c!}export`.
