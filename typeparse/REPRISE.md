# REPRISE — état du prototype TIP 764 (valeurs annotées, variables typées)

*Fiche de reprise, 24 octobre 2026, version **v15v**. À joindre (avec `typeparse.zip`) au
premier message d'une conversation qui reprend le travail.*

## 1. Où en est-on

* **v15v livrée** (24 octobre) : lecture `$a(champ)` par un cache du champ, nombres typés lus directement par `expr`. Le détail de chaque version est
  dans `noyau/LISEZMOI.md`, une section par version, de la v14 à la v15v en fin de fichier.
* **Vérifié pour la v15v** (côté Claude, Linux) :
  - les 54 tests de `tests_tclsh` qui ont une sortie attendue passent (Linux ; MinGW64 chez
    l'utilisateur pour la v15s) ; les autres tournent sans erreur ;
  - sorties de tous les tests et exemples identiques avec et sans forme compacte
    (`TCL_TYPES_COMPACT=0/1`) ;
  - valgrind propre dans les deux modes sur tous les tests et exemples, sauf les fuites
    bénignes à la sortie de `exemples/executable_pe.tcl` (appel à `exit`) ;
  - suite complète de Tcl (`make test`) : 61 échecs, exactement ceux de la v15 de référence
    (51 090 tests, 45 056 passés, 5 973 sautés).
* **Planning** : document en ligne « TIP 764 — planning de travail »
  (https://claude.ai/artifact/2npXGRaYukdjpbXcwH3pne), liste à cocher des points. C'est la
  référence pour la suite.
* **Documentation** :
  - `doc/ANNOTATIONS.{fr,en,de,nl,es,pt,it}.md`. Le français est la référence ; son § 17 tient la
    liste des points ouverts, son § 15 les mesures de performances ;
  - `MANUEL.md` et ses traductions `MANUAL.*.md`, plus anciens ;
  - notes de patch dans `noyau/LISEZMOI.md`, ordre d'application dans `noyau/ORDRE.txt`.

### Versions depuis la v13

| version | contenu |
|---|---|
| v14 | contrat par le nom : plus d'adoption, bit `VAR_CONTRACT` posé seulement par une déclaration |
| v15 | collections et éléments : seules `list` et `dict` se déplient ; chemin `*` |
| v15b–v15f | cast produit → collection ; conversions EIAS ; somme choisie par un champ ; `incr`/`append`/`lappend`/`dict set` par chemin ; nominal de collection |
| v15g | performances de l'étape annotée (aire 50,9 → 14,1 ms) |
| v15h–v15l | construction par chemin ; `set` à un argument ; `;` et `,` mêlés ; variante inactive ; étiquette d'un autre produit |
| v15m | variantes de même forme : erreur `TCL TYPE AMBIGUOUS`, la variante courante départage |
| v15n | règles de conversion entre variantes d'une somme |
| v15o | fuite de `lset` via `TclLsetList`, type perdu par `lpop` |
| v15p | forme compacte (prototype), `tcl::unsupported::abi` |
| v15q | `type def {-compact}T`, `type is {compact?}T`, `type {c!}export T` |
| v15r | `exemples/geometrie/geo.c` écrit avec les fonctions de stub prévues (en-tête `tclAnnotatedApi.h`) |
| v15s | procédures typées : cache du classement, déclaration directe, variables locales compilées |
| v15t | `type def {-compact}P` sur un produit ; listes champs d'un produit en cases ; `bench.tcl` rapporté au Tcl nu |
| v15u | `foreach`/`lmap` parcourent le contenu d'une liste typée (patch `boucles-typees.patch`) ; `bench.tcl` en markdown |
| v15v | cache du champ pour `$a(x)` ; nombres typés lus directement par `expr` (patch `expr-nombres-types.patch`) |

## 2. Installation et tests (chez l'utilisateur, MinGW64)

    bash typeparse/maj.sh [typeparse.zip]     # extrait, copie les sources de types, applique
                                               # les patchs NOYAU de ORDRE.txt (-F0), make clean,
                                               # recompile, puis compare tests_tclsh/*.tcl à leur .out
    bash typeparse/maj.sh --help               # options : -n, -y, --no-build, --no-test, --commit "msg"

Les sources de types (`tclTypes*.{c,h}`, `tclAnnotObj.c`, `tclTypedVar.c`, `tclTypePath.c`,
`tclTypeGeneric.c`, `tclTypeParse.c`, `tclVarRef.c`) sont livrées ENTIÈRES à la racine de
`typeparse/` et copiées dans `generic/` ; elles ne sont jamais patchées. Seules les sections
« noyau » des patchs s'appliquent (tcl.h, tclInt.h, tclParse.c, tclCompile.c/.h, tclCompCmds.c,
tclCompExpr.c, tclExecute.c, tclVar.c, tclStringObj.c, tclBinary.c, tclTest.c, …).
`tclCompact.h` et `tclAnnotatedApi.h` sont pour les extensions (non copiés dans `generic/`).

Quand une structure de `tclTypes.h` / `tclTypesInt.h` change (v15p, v15s), tout doit être
recompilé : `maj.sh` fait `make clean`. Des objets périmés donnent une corruption du tas.

## 3. Reconstruire l'environnement de travail (côté Claude)

* **Base** : `https://github.com/tcltk/tcl.git`, branche `tip-759b_tcl90` (TIP 759 de
  l'utilisateur), commit `f9e939e312bf12c501d09dd3abbc1c01b0812a5d`. Puis les sources de types du
  zip copiées dans `generic/`, et les patchs de `noyau/ORDRE.txt` dans l'ordre (sections noyau).
* **Arbres gardés** (un clone par usage, sous `tclsrc/`) :
  - `atom` : version de travail ; `cd unix && ./configure && make -j8` ;
  - `atom_ref` : version de référence (v15) pour comparer sorties et suite Tcl. À défaut,
    la livraison précédente ;
  - `pur`, `pur_ref` : mêmes sources, `./configure --disable-shared 'CFLAGS=-O0 -g -DPURIFY'`,
    pour valgrind (sans PURIFY, l'allocateur de Tcl masque les erreurs).
* **Vérifications pour chaque point** :
  1. reproduire, expliquer la cause dans les sources, corriger ;
  2. tests de `tests_tclsh` comparés à leur sortie attendue, `tNN_x.out` **ou** `tNN_x.tcl.out`
     (t24 à t40, t57), avec `LANG=C.UTF-8`, comme le fait `maj.sh` ; une sortie attendue ne
     doit pas dépendre du système (pas de `/proc`, de chemins, de tailles de pointeur) ;
  3. sorties de tous les tests et `exemples/*.tcl` comparées à celles de `atom_ref` (seuls
     les écarts voulus) ;
  4. mêmes sorties avec `TCL_TYPES_COMPACT=0` et `=1` ;
  5. valgrind (`-q --leak-check=full`, arbre `pur`) sur tous les tests et exemples dans les deux
     modes ; compter `Invalid|definitely|uninitialised` ;
  6. `make test` complet : la liste des échecs doit rester celle de la référence (61) ;
  7. nouveau test `tNN_*.tcl` + `.out` ; patch noyau éventuel (diff contre une copie
     `.avant_…` du fichier) ajouté en fin de `ORDRE.txt` ;
  8. section dans `noyau/LISEZMOI.md`, mise à jour de `doc/ANNOTATIONS.fr.md` si le comportement
     visible change, point coché ou complété dans le planning ;
  9. archive : `zip -qr typeparse.zip typeparse -x '*.so' '*.o'`.
* Le banc C sur maquette (`typeparse/test/`) ne compile plus depuis longtemps ; ce n'est pas une
  régression.

## 4. Décisions de conception arrêtées

**Valeurs annotées**
* Valeur annotée A = (V contenu, T référence de type). Une annotation locale (`{-x}`, `{x?}`,
  `{x!}`) ne vit qu'entre l'appelant et la commande ; posée sur une valeur, elle ne garde que le
  contenu.
* Résolution d'une annotation : dans le contexte d'abord (variantes d'une somme, champs d'un
  produit), puis les types globaux ; sinon c'est une note. Une annotation ne vient jamais du
  contenu d'une chaîne.
* **Contrat par le nom (v14)** : seule une déclaration (`set {T}v`, `x is T = v`, `lappend {T}M`…)
  pose un contrat ; une variable non déclarée reçoit une valeur typée sans contrat. Sans contrat,
  une modification est un calcul ordinaire ; sous contrat, le résultat est vérifié au rangement
  et un refus laisse la variable intacte.
* **Collections (v15)** : seules `list` et `dict` se déplient dans la vue liste ; un produit, un
  scalaire, un lien est UN élément ; une somme prend la nature de sa variante active.

**Sommes**
* Déduction de la variante : la plus stricte. Plusieurs variantes également strictes = erreur
  `TCL TYPE AMBIGUOUS T v`, le message propose `{variante}valeur` et `set {S}v(variante) valeur`.
  **Décision implicite gardée** : à égalité, la variante courante de la variable départage.
* Conversions entre variantes d'une même somme (v15n), faites sur une copie.

**Construction d'un produit (v13)**
* 1 argument de même type = copie ; `{T.champ}valeur` ; *n* valeurs pour *n* champs =
  positionnel ; 0 argument = défauts ; sinon `wrong # args … write {field}value or set {T}var(field,...)`.
* Étiquettes : tout ou rien ; une étiquette portée par une variable compte.

**Chemins** (`tclTypePath.c`)
* Opérateurs `,` `.` `:` `( )`, `*` unaire et `*` seul (toutes les parties) ; construction
  `set {rect}R((A,B).(x,y)) …`.
* Le chemin ne substitue rien : dans `{…}` et `${…}`, `$` et `[` sont des erreurs. Les valeurs
  substituées par Tcl dans `$nom(...)` sont atomiques (`pathindex`, `INST_INDEX_CONCAT`).
* Côté noyau : `TCL_TOKEN_INDEX` par `Tcl_ParseVarIndex`, repli exact sur la règle Tcl 9 quand
  l'index n'est pas bien formé (`info complete` inchangé).

**Forme compacte (v15p, v15q)**
* Seulement les formes canoniques ; `int(s,n)`, `int(u,n)`, `double` ; alignement naturel
  (mesuré par `tcl::unsupported::abi`) ; commandes sous `tcl::unsupported::compact`.
* `type def {-compact}T` : l'annotation est sur le nom du type ; la valeur est normalisée à la
  construction ; les conversions se font sur une copie. Interrogation `type is {compact?}T`
  (ou `$v`). Export : `type {c!}export T` (en-tête C avec `_Static_assert`), `{llvm!}` prévu.
* API C pour les extensions : `tclAnnotatedApi.h`, fonctions `static inline` en attendant des
  entrées de stub (`Tcl_AnnotatedGetType`, `Tcl_AnnotatedCreate`, `Tcl_AnnotatedGetParts`,
  `Tcl_AnnotatedGetCells`). Exemple : `exemples/geometrie/geo.c`.

**Binaire**
* `type binary` / `chan binary` ; liens `*(origine,repr).T`, `-linkdepth`, référentiels nommés,
  `-dedupe` ; codecs C pour les cas mesurés, repli sur le codec Tcl (`set ::tcl_typebinaryc 0`).

## 5. Prochaine étape

**Performances, côté C.** Après la v15v, l'étape annotée de l'exemple `geometrie` est à 1,8 à 3 fois
le Tcl nu ; une lecture `$a(x)` dans `expr` coûte ≈ 0,14 µs (la valeur typée du champ, créée puis
libérée). Suite proposée : `type binary encode`/`decode` depuis et vers les cases, une valeur en cases
rendue par C sans passer par le texte (`Tcl_AnnotatedCreateCells`), puis de vraies entrées de stub.
`exemples/geometrie/bench.tcl` et `bench_parties.tcl` ; profil avec callgrind.

Les autres points ouverts sont dans le planning et au § 17 de `doc/ANNOTATIONS.fr.md`, entre
autres :
* valeur « inexistante » en général (`opt(T)` ne couvre que les liens binaires) ;
* annotation de commentaire `{#}`, du premier mot, de déclaration dans `proc` ;
* notes par élément d'une liste typée ;
* forme compacte : cases à pointeurs, lecture directe par `expr` compilé, `lappend` en temps
  constant, `{-compact}` pour une famille ;
* `type {c!}export` pour les types récursifs, cible `{llvm!}`, sens inverse (en-tête C →
  `type def`) ;
* codecs C pour sommes, liens et tailles calculées ;
* exemples `arbre_var.tcl` et `defauts_calcules.tcl` à corriger ; banc C sur maquette.

## 6. Pour reprendre

Joindre `typeparse.zip` (dernière version) et ce fichier. Dire ce qui a été fait depuis
(résultat de `maj.sh`, modifications locales), puis le point du planning à traiter.
