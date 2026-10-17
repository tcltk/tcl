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

Connu : `lset` avec une liste d'index passant par `TclLsetList` laisse une
référence en trop sur l'objet (fuite mémoire, sans plantage).

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
« unknown field "zz" in paire » ; un autre type (`{aut.nom}x`) retombe sur
l'erreur habituelle. Le préfixe n'a jamais de sens de champ en dehors d'un
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
  `file binary` lit `tagged =nom`.

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
* `layout/filebinary.tcl` 0.6 : option `-linkdepth`, `file binary deref`, `unref`.


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
