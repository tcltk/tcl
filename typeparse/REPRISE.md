# REPRISE — état du prototype TIP 764 (valeurs annotées, variables typées)

*Fiche de reprise, 20 novembre 2026, version **v17j**. À joindre (avec `typeparse.zip`) au
premier message d'une conversation qui reprend le travail.*

## 1. Où en est-on

* **v17j** (20 novembre) : les méthodes TclOO acceptent les signatures typées des procédures (`method ajouter {{int}n} {…}`, `constructor {{string}nom; {int}stock} {…}`, `method {double}moitié …`, `oo::objdefine`) ; `info class definition`/`constructor`, `info object definition` rendent noms et défauts. Patch noyau `oo-methodes-typees.patch` (`tclOOMethod.c`, `tclOOInfo.c`), test t116. Reste : `variable {int}n` dans `oo::define` (accepté, ignoré ; typer dans le constructeur).
* **v17i** (15 novembre) : `T(chemin)`, le type d'une partie (`Emplacement(propriétés)`, `Liste(*.propriétés.max)`), comme `$v(chemin)` en est la valeur ; l'analogie list ↔ tableau, dict/produit ↔ struct. Pour VolɷDuctus : une instance est une valeur dont le type est le produit de ses blocs. Test t115.
* **v17h** (15 novembre) : `note get` rend les notes en ensemble, triées, jointes par « , » (`mm,px`, `list<int>@hello,mm`), comme `type canonic` et `{T@notes}`. Suite : VolɷDuctus selon CONCEPTION.
* **v17g** (15 novembre) : `list(LO:HI).T`, `list(1:Inf).T` (le « : » des chemins ; `Inf` sans borne ; `list(N:N)` = `list(N)`) — la cardinalité `1..n` de VolɷDuctus. Vérifiée à la construction et à chaque écriture. Test t114. **Le plan de la TIP (étapes 1–6) est fait** ; suite : VolɷDuctus selon CONCEPTION (journal des convolutions du suivi, référence typée unique, carnet typé…).
* **v17f** (15 novembre) : `{T@notes}valeur` dans un constructeur (`set v {int@this}5`, `{double@a,b}5`, `%XX`, le point dans la note) ; `type canonic` règle le mot reçu. Test t113 ; sondes 43/43. 
* **v17e** (15 novembre) : les méta-types `bare`, `typed`, `noted`, `typednoted` (et leurs unions) ; `any` leur union, devenu l'identité (`type cast any`, `{any}a`, `list.any`, `dict.any` gardent type et notes, un élément nu reste nu). Une exigence vérifie l'état sans convertir ni poser de contrat. `type meta $v` : `{int}typed`, `{list}bare`… (représentation = nom du `Tcl_ObjType`). `type canonic` d'une valeur nue : `{bare}texte`. Test t112 ; sondes 42/43. Suite : `{T@notes}` dans les constructeurs, `{-min n}` (étape 6).
* **v17d** (15 novembre) : un seul ordre de résolution pour `set`, `type cast` et les arguments. Typée : même type, élargissement, variante (avant les règles) ; règles et conversions intégrées seulement par un cast écrit (`type cast`, `{T}$x`, à l'appel compris) — un argument typé n'est plus un cast (`VeutEntier $d`, d double 2.5 : refusé, au lieu de tronquer). Nue : lecteur natif, validation d'un nominal par sa règle depuis le sous-jacent, puis règle `any T` / `string T` (`set {tson}T $texte` marche). `set {Pt}p $x` : un argument = la valeur entière. Test t111 ; sondes 38 OK sur 43 (le reste : étapes 5 et 6). Suite : méta-types (étape 5).
* **v17c** (14 novembre) : `type canonic ?-notes? V` → `{T}contenu`, `{T@n1,n2}contenu` (notes en ensemble trié, `%XX` hors lettres, chiffres, `_`, `.`), `{@notes}contenu` pour une valeur seulement notée ; contenu reconstruit selon le type ; `type parse` relit (type existant, contenu valide, notes posées). Aller-retour vérifié sur scalaires, conteneurs, produits nommés et anonymes, types d'espace, sommes, unions, nominaux, liens. Pas gardés : notes des éléments, types des éléments de `list.any`, `{-compact}`. Test t110 ; sondes 32 OK sur 43. Suite : ordre de résolution unique (étape 4).
* **v17b** (14 novembre) : l'union `{int}i | {double}d` acceptée, les formes canoniques étant disjointes (`16` / `16.0`) ; lecture comme `expr` (`0x10` → `i` 16, `2e1` → `d` 20.0) ; normalisation des éléments de `list.num` ; lecture rapide (boucles, `$P(x)` dans `expr`). `int | string` reste refusée. tcllib_geometry passe en coordonnées `num` : **210/210** (variantes C en `double`). Test t109. Détail : `noyau/LISEZMOI.md`. Suite prévue de la TIP : `type canonic` / `type parse`, un ordre de résolution unique, les méta-types (`bare`, `typed`, `noted`, `typednoted`), `{T@notes}`.
* **v17a** (14 novembre) : normalisation des nombres à la construction (`0x10` → `16`, `2e1` → `20.0`, `1` en double → `1.0` ; listes, dicts, produits, sommes, `lappend` sous contrat ; un `double` calcule désormais en double : `5 / 2` = 2.5), et notes gardées par les constructeurs de listes et de dicts (lues avec le type de la liste : `{double vitesse}`). Sans patch noyau. Test t108 ; sondes `doc/any/` 25 OK sur 36. tcllib_geometry : 199/210 (tcllib compare des textes et divise des entiers). Détail : `noyau/LISEZMOI.md`, discussion : `doc/PISTES-any-string.md`.
* **v16z** (7 novembre) : `apply` typé. Une lambda est une procédure sans nom : `apply {T}{{signature} corps ?ns?} mots…`, la liste d'arguments accepte la signature de `proc` (types, défauts, options, liens, variadiques), l'annotation du mot lambda est le type de retour ; les types sont lus dans l'espace de la lambda. Une règle de `type convert` peut typer son argument. Le littéral `{T}{…}` étant refait à chaque exécution, une table par interpréteur garde une Proc par texte (512 au plus). Patch noyau `apply-type.patch` (`tclProc.c`), test t106 ; `apply {T}$f` pose aussi le type de retour d'une lambda gardée dans une variable. Puis `type call ?TYPE?` : l'appel courant comme valeur (l'appel réifié), l'enregistrement de la signature (options sans tiret, variadiques en listes, liens en valeurs ; un champ sans valeur absent) ou un produit nommé construit depuis les locales de même nom ; `type proc` donne `record` ; test t107, utilisé par `exemples/attentrel` (attributs, entités TclOO, convolutions : le processus d'achat BPMN). Dans `attentrel`, l'effet d'une convolution sur chaque actant est, dans le type de l'actant, une précondition `\conduit` une postcondition (Hoare lu comme un verbe, d'après la typologie de l'analyse d'énoncé) : `{(Achat(état) eq "créé" \conduit Achat(état) = "demandé")}objet` ; `eq ne == != < <= > >= in ni && || ! + - * /`, `=` affecte après `\conduit`, `\avant X` la valeur d'avant, `()` l'inexistence, les unaires `\produit`, `\déduit`, `\lit` (`effets.tcl`, analyseur de Pratt). Chemins, valeurs et opérateurs vérifiés à la définition ; précondition vérifiée avant le corps, affectations appliquées et postcondition vérifiée après ; `processus` compose une chaîne par interprétation abstraite (ensembles pour les énumérations, intervalles pour les nombres) : chaque condition est nécessaire, requise (sur l'état de départ : elle devient la précondition de la chaîne), contingente ou impossible, une alternative ouvre des branches, et l'effet de la chaîne s'écrit lui-même comme un effet (`Budget(reste) >= 0 \conduit Budget(reste) >= -5000`) ; `effet`, `phrase`, `possibles` lisent les définitions. D'après l'analyse du procès (P(t), ses dérivées, les prépositions), une convolution passe par cinq phases, `pour` (préparation), `à` (inchoative : précondition, actants saisis), `par` (le corps), `de` (égressive : affectations, postcondition), `fors` (achèvement, journal) ; `phase` les observe, `quand PHASE NOM CMD` s'y accroche ; un échec avant `fors` défait tout (fiches des actants restaurées, produits détruits, rien au journal). `processus` range chaque entité selon son existence aux deux bouts de la chaîne : traversante, fait, consommée, flux (`() \conduit ()`, né et clos dans la chaîne) ou flux resté ouvert. Puis `exemples/attentrel/chantier` : un portefeuille de projets télécom chargé tel quel d'un classeur Excel (`xlsx.tcl`, lecture par `zipfs`), les identifiants en types lus par chemins (`CodeSite`, `CodeProjet` : `$c(site.dept)`, `$c(pi)`, par `type convert string …`), les jalons des « datés clés » en convolutions (l'ordre tiré des dates réelles, un locuteur par jalon vérifié à la phase `à`), et l'accueil d'un mail : l'expéditeur (carnet, nom affiché, ou question sur le rôle), le projet (fil, code, Pi, code-site, nom ou alias, lieu, commune), ce qui est attendu de cette personne, puis une proposition ou les questions qui restent. L'algèbre gagne le domaine « tout sauf » (`≠ ""`), celui d'un type qui refuse le texte vide. Puis `chantier/mad.tcl`, d'après le schéma de la page 62 du mémoire de F. Merlet (CNAM, 2024) : le processus de mise à disposition TOTEM au grain des verbes, les livrables (CRVT, APS, APD) en entités à automate (produit, à valider TOTEM, à valider client, validé, refusé), validations à deux niveaux et reprises (version + 1, par `\avant`), chaque losange étant deux verbes (valider, refuser) ; la livraison date le jalon, la validation par le client date `*_ok` ; plusieurs locuteurs par verbe (le chef de projet TOTEM relaie le client) ; `Abandonner`, verbe rare offert sans être proposé ; le losange « bailleur ? » (`Qualifier_site`, deux branches) décide de l'APS (esquisse, accord, sans note de calcul) ; l'algèbre resserre `a || b` quand un côté est impossible et situe une contingence dans sa branche ; `possible` vérifie les livrables d'un projet, `appeler` trouve les actants, `corbeille` donne ce que chaque rôle peut faire. Puis le rejeu d'un récit : `tableur.tcl` (xlsx, xlsm, ods), `hypergraphe.tcl` (une matrice d'incidence : sommets en lignes, une phrase par colonne, la place du mot dans la cellule ; les cadres des verbes), `rejeu.tcl` (rejouer la matrice sur un modèle ; un refus est une cause, avec la convolution qui manquerait, ou une cascade, ou un doublon ; les obligations au sens de Leibniz, est obligatoire ce que la norme rend nécessaire), indépendants du domaine ; `chantier/mad_rejeu.tcl` en est le lexique pour la mise à disposition TOTEM, sur la matrice de 74 événements de F. Merlet (`scenario_mad.ods`) : tel quel, 10 joués et deux causes de refus (l'EBF absente du récit, l'ordre du bureau d'études) ; complété de cinq hypothèses, jusqu'au go travaux. Tout ce qui suit l'exemple d'achat (le chantier, l'accueil, le rejeu, les lecteurs de classeurs) est sorti de `typeparse` le 09/11/2026 pour une archive séparée, VolɷDuctus (`volvoductus.zip`), hors fossil : `exemples/attentrel` ne garde que le moteur (`attentrel.tcl`, `effets.tcl`) et l'achat. Feuille de route décidée : 1. les tables comme jeux d'essai pour construire ontologie, processus, normes (en cours) ; 2. le flux des mails dans la corbeille, classés aussitôt, avec les questions ; 3. les vues à la demande (tableaux Excel, tableau de bord, journaux en phrases, planning, répertoire, pièces et documents liés). Constat sur TIP 764 : une valeur non typée passée à un paramètre `{Jour}d` n'entre pas par la règle `type convert string Jour` (le cast intégré par la base l'accepte sans la valider) ; une valeur typée `{string}` y passe. Prochaines étapes envisagées : `induit` (file d'événements, cause au journal), le temps et les promesses, les relations, puis un type `obj` par prototype (propriétés typées lues par chemins, méthodes à signature, type constaté après coup par `type portrait`, copie = reproduction). Puis le prototype `exemples/ffi` : `ffi::fn LIB {R}nom {signature}` appelle une fonction de bibliothèque partagée depuis sa signature typée (types binaires pour les données, `{var.T}` pour un paramètre de sortie, `ffi::callback` et une lambda typée pour un rappel), par un talon C compilé avec le compilateur et le cache de cproc, sans libffi ; `libc.tcl` : `hypot`, `strlen`, `getenv`, `timespec_get`, `modf`, `qsort`. Reste : TclOO, les commandes à deux formes d'appel ; pour ffi, les structs par valeur, l'encodage des structs dans le talon, `errno`.
* **v16y** (6 novembre) : `type debug`, la mise au point par les notes. Une note `@nom` est une note d'instrument (jamais un type) : `{@trace}` signale chaque commande qui reçoit la valeur (procédures, listes, `foreach`, paramètres nommés, `fichier:ligne`), `{@step}` s'y arrête avec une invite (`c s r o q w v p`, commande Tcl sur place) ; `s` et `type debug next` font le pas à pas ; `type debug hook` pour un journal ou une assertion ; `type debug when NOM EXPR` pour une note conditionnelle ; `type debug console`, une fenêtre Tk (pile, variables avec type et notes, journal, commande dans le cadre choisi). Le crochet par défaut est dans `tclTypeDebug.tcl`, mis en chaîne C dans `tclTypeDebug.h` par `gen_typedebug.tcl`. Puis les 61 échecs de `make test` triés et résolus (4 patchs noyau) : plus d'annotation devant `] } ) = < > ! & | ^ ? , ; ~ %` (l'accolade mal placée redevient une erreur), le résultat d'une fonction de `expr` à nouveau converti (`abs(0x10)` rend 16), l'arbre de `Tcl_ParseExpr` corrigé pour les parenthèses et les tuples de TIP 759, tests mis à jour. Il ne reste que les 8 échecs `httpProxy` d'un Tcl 9.0 d'origine. Exemple `exemples/bulletin` (les quatre étapes). Signatures : une option peut être un lien (`{var.T}-o`), le défaut calculé d'une option voit les arguments ; exemple `exemples/signatures` (une `lsearch` en huit lignes de signature, l'exemple de TIP 460, comparaison avec les TIP 288, 457, 460, 479, 480), test t104. Puis les variadiques n'importe où : `{T}args...` (sans défaut au moins un, avec `?=` zéro ou plus, les champs d'après depuis la fin, un produit s'étale sur ses champs : `foreach`, `dict set`, `dict create` s'écrivent en signature), test t105. Restent : `apply` et TclOO, les commandes à deux formes d'appel. Les notes ordinaires deviennent des proto-types : `type debug notes`/`report` les inventorient (lieu, commandes, valeurs), proposent le type voisin d'une faute de frappe, et vérifient après coup les valeurs vues quand le type est défini. Test t103. Pas de patch noyau.
* **v16x** (6 novembre) : `return {v:}$x` sous `eval`, `uplevel`, `switch -matchvar` : le retour étiqueté est rendu dans le type de la procédure typée avant que l'étiquette expire (`TclAnnotExpireFromRet`, patch `retour-etiquette.patch`, test t102). Aussi : cproc sous Windows avec tcc 0.9.27 (`_Static_assert` de secours dans `type {c!}export` et dans le C de cproc, contrôle 32/64 bits).
* **v16w** (6 novembre) : sommes et unions. Une somme (`,`) écrit sa variante dans sa valeur (`i 5`, `fahrenheit 50.0`, le littéral seul) : rien n'est deviné, la variante passe par la chaîne, l'égalité la voit ; toute variante typée a un nom. Une union (`|`) est la charge seule, ses alternatives doivent être disjointes (refus à la définition). `{-strict}`, `{-deduce}` et l'avertissement disparaissent ; `type variant`, `type is {union?}` ; `opt(T)` = `some X` / `none` ; toute valeur se lit en `string` ; `$f(w)` lit la charge courante ; produit à un champ `{{double}r}cercle`. Diagnostic et décision : `doc/DIAGNOSTIC-SOMMES.md`. Unions dans la géométrie et tcllib (210/210). Reste : `return {v:}` hors compilation.
* **v16v** (6 novembre) : chemins compilés gardés (dans l'objet index, ou une table par thread), cache (type, indice) pour `$p(x)`, pas d'enveloppe aux étapes intermédiaires ; `lindex`/`lassign` lus directement dans le contenu d'une valeur typée (`TclAnnotFastIndex`), rangement direct d'une valeur typée réglée dans une variable sans contrat ; fonctions mathématiques sur `$p(x)` en `loadFieldNum`. `$s(a.x)` 869 → 111 ns, `lassign $p x y` 389 → 66 ns ; étape 2 du tutoriel 1,2–1,6 fois l'étape 1. Patchs `expr-fonctions-champs.patch`, `execute-lecture-typee.patch` ; test t101 ; `exemples/tutoriel/chemins.tcl`. Sans changement de sens.
* **cproc en trois étapes** (6 novembre) : `exemples/cproc/cproc1_tcl.tcl` (Tcl), `cproc2_type.tcl` (générateur typé : sommes `kind`, `defval`, produits `field`, `cblock`, `cdef`, `cconfig` sous contrat ; même C produit), `cproc.tcl` (liaison des appels en C : positionnels, défauts constants, valeurs nues ou du bon type, résultat typé par (V, T) ; repli sur la procédure typée cachée `_cprocN_nom` avec les mêmes mots, donc les mêmes messages). `distance` 0,98 → 0,28 µs. Trois points du modèle relevés : une somme ne se caste pas en `string` (`{}$v`) ; `return {v:}[list …]` perd une somme stricte imbriquée ; `switch -regexp -matchvar` fait expirer l'étiquette de `return {v:}…`.
* **Tutoriel** (6 novembre) : `doc/TUTORIEL.fr.md` et `exemples/tutoriel` — des polygones en trois étapes (Tcl, Tcl typé, Tcl + C par `cproc`), mêmes appels et mêmes résultats (`essais.tcl 1|2|3`), mesures (`vitesse.tcl`). `cproc` corrigé : nom relatif (`cproc poly::aire` depuis l'espace global), fonction C du nom d'une struct (renommée `cproc_boite`), plus de boucle infinie après un échec de compilation.
* **Exemple `zip`** (6 novembre, sur v16u) : `exemples/zip/zipmem.tcl` — le format zip en types binaires (`::zipmem::eocd`, `entry`, `local`, zip64), `zipmem::open/names/read/text`, `zipmem::source`, `zipmem::install` (puis `source app.zip` exécute `main.tcl`, les `source` relatifs lisent l'archive en mémoire) ; tcllib en zip (5 455 membres) : ouverture 14 ms, membre 0,16 ms (zipfs : 26 ms, 0,21 ms). Correction de `filebinary.tcl` : `at(N).list(...)` en tête.
* **Exemple `cproc`** (5 novembre, sur v16u) : `exemples/cproc` — `cproc {T}nom {signature typée} {corps C}` ; la signature est liée par Tcl, la colle Tcl↔C (structs par `type {c!}export`, spans pour les listes, en place sur les cases) est générée, compilée par gcc ou tcc dans une bibliothèque en cache, stubs initialisés sans `libtclstub`. `tcllib_geometry/geometry_cproc.tcl` : 210 tests de tcllib passent (gcc et tcc). Limites : pas de sommes/dicts en argument ; liaison en Tcl (≈0,6 µs par appel).
* **v16u livrée** (5 novembre) : espaces de noms des types — `type def` range le type dans l'espace courant (`::geom::point`), un nom se résout comme une commande (courant, `namespace path`, global), la signature d'une procédure dans l'espace de la procédure, le corps d'une famille dans le sien ; `type get` donne le nom qualifié (simple pour un type global) ; `namespace delete` retire les noms. Une valeur typée garde en vie la table de types de son interpréteur (plus de lecture après libération après `interp delete`). `type {c!}export` : struct nommée par le dernier mot.
* **v16t** (5 novembre) : une chaîne est un scalaire, un produit un conteneur — un produit typé se déplie en ses champs dans la vue liste (`llength $P` = 2, `lassign $P x y`, `concat`), un scalaire typé reste UN élément, les éléments d'une `list.string` restent typés dans les parcours (la v16s les rendait nus : `{hello world}` redécoupé dans une boucle imbriquée) ; `doc/CARTOGRAPHIE-EIAS.md` (tensions avec EIAS, sondes dans `doc/cartographie/`) ; géométrie sans `{*}`, `geom.c` sans `Tcl_AnnotatedGetParts`.
* **v16s** (5 novembre) : optimisations — champs des signatures typées en locales compilées, types résolus une fois, pas de liste `args`, produits de scalaires vérifiés champ par champ ; éléments des listes de scalaires nus dans les boucles, `lassign`, `lrange` (`lindex` reste typé) ; `lrange` ne traite plus que la tranche ; écritures sous contrat sans copie. Patch `procs-typees-locales.patch`. math::geometry typé à parité avec tcllib sur les polygones.
* **v16r** (5 novembre) : dépliage — `proc {{*}T}nom` vérifie le résultat comme un `T` et le rend en liste ordinaire, `{{**}T}` en dict (profond ; `type {{*}T}mot` de même) ; appliqué aux procédures de `exemples/tcllib_geometry` qui rendent des points.
* **v16q** (5 novembre) : options et drapeaux des procédures typées — un champ `-x` est une option (`{flag}-decreasing`, `{int}-index ?= -1`), préfixes uniques, `--` ; une somme donne des options exclusives (`{"ascii", "dictionary", {string}command}-mode ?= ascii`), une variante produit des options dépendantes ; `{T}-o` un groupe réutilisable. Pas de patch noyau. (Exemples `bench_procs.tcl`, `bench_options.tcl`, et `tcllib_geometry` : math::geometry de tcllib reformulé, les 210 tests de tcllib passent ; son cœur en C, `geom.c`, avec les structs de `type {c!}export`.)
* **v16p** (5 novembre) : procédures typées — liste d'arguments en type produit (`proc {pt}decale {{pt}p; {double}dx ?= 0} {…}`), positionnels puis étiquettes, `{list.T}args`, `{var.T}n` (upvar automatique), type de retour par l'annotation du nom (cast au retour normal, `return {v:}$x` choisit la variante d'une somme, `return {T}$x` n'est qu'une note locale), `type proc`, `info args`/`info default` ; patch `procs-typees.patch`. 
* **v16o** (4 novembre) : une variante dont le type est un produit se construit par ses champs (étiquettes, positionnel, chemin à travers la somme `set {S}x(v.(f,g)) …`, calculable) ; `type def NOM` rend la définition (de `S`, `S.v`, `P.f`) ; texte enrichi sans table ni `eval` (voir `noyau/LISEZMOI.md`). (v16n : `type is {disjoint?}S`, `type def {-strict}S` / `{-deduce}S`, avertissement pour une somme qui se recouvre sans option, options multiples `{-a,-b}`.) (v16m, 3 novembre : piste B, une annotation expire avec la commande qui reçoit le mot ; plus de cast immédiat (ni `{T}$x`, ni `{list}`/`{dict}`) ; `foreach`, `lmap`, `dict for`, `dict map`, `lassign` résolvent leurs listes (instruction `annotSettle`) ; `type {T}mot`, le cast explicite ; patch `piste-b-expiration.patch`, et `doc/PISTES-annotations-locales.md` pour la discussion.) (v16l, 2 novembre : cast immédiat de `{T}$x` (variable seule), étiquettes `{nom:}` pour les champs et les variantes, patch `cast-immediat-etiquettes.patch`.) (v16k : `TCL_TOKEN_ANNOT` passe de 512 à 2048 (512 est `TCL_TOKEN_CMD_IN_EXPR` dans la série TIP 759 pour 9.1), patch `jeton-annot-2048.patch`.) (v16j : `type forget T ?T ...?`, oublier la définition d'un type (voir `noyau/LISEZMOI.md`).) (v16i, 26 octobre : exemple `exemples/matrices` (matrices en cases lues et fabriquées en C, lot lu comme un tableau `matrix3[n]`) ; `$M(i.j)` lu directement depuis une vue (matrice d'un lot) et dans une liste typée ordinaire (déterminant 16,7 -> 1,8 µs).) (v16h : listes de taille fixe en cases — `type def {-compact}m list(3).list(3).double`, et à N dimensions ; `$M(i.j)` lu et écrit directement dans la case (0,9 -> 0,2 µs ; 2,2 -> 0,14 µs) ; correction de `lset L i j v` sur une liste en cases (mémoire libérée).) (v16g : `set {T}x a b…` d'un produit de scalaires deux fois plus rapide (1,3 -> 0,6 µs : voie rapide sans copie, `x` variable locale compilée, patch `set-type-local-compile.patch`) ; `$a(x)` lu comme nombre aussi pour `& | ^ << >> ~` (patch `expr-champs-bits.patch`).) (v16f : `$P(*)` rapide (2,4 -> 0,7 µs) ; `cstruct.c` : voie objet par `tclAnnotatedApi.h`, lecture en place en forme compacte (2,5 ms -> 24 µs) ; banc cstruct mis à jour.) (v16e : affectation par tuple `(a, (b, c)) = v` compilée en une instruction (`tupleAssign`, patch `expr-tuple.patch`) ; `maj.sh` : patchs déjà appliqués reconnus même quand un patch suivant a retouché leur contexte, variable `TMP` renommée (MSYS2).) (v16d : codec C pour les tailles calculées et les préfixes, `at(EXPR).T` et les liens `*(ORIGINE,REPR).T` (opt(T), listes chaînées) ; `TclType` gagne `binAtBody`.) (v16c : `X is T = v` compilé (instruction `declareLocal`), `$a(x)` lu comme nombre dans `expr` (instruction `loadFieldNum`, directement dans la case en forme compacte), `nom = v` par `storeScalar` ; patch `expr-declaration-champs.patch`.) (v16b : numéro de version lisible (`typeparse/VERSION`, `tcl::unsupported::typesversion`, affiché et vérifié par `maj.sh` ; `version.tcl` devine celle d'un tclsh plus ancien). (v16a : codec C pour les chaînes, les sommes de littéraux et les sommes balisées.) (v15z : `lappend` sous contrat en temps linéaire.) (v15y : Tcl annoté, patch `donnees-interp.patch`, la structure `Interp` change.) Le détail de chaque version est
  dans `noyau/LISEZMOI.md`, une section par version, de la v14 à la v16i en fin de fichier.
* **Vérifié pour la v16i** (côté Claude, Linux) :
  - les 68 tests de `tests_tclsh` qui ont une sortie attendue passent (Linux ; MinGW64 chez
    l'utilisateur pour la v16b) ; les autres tournent sans erreur ;
  - sorties de tous les tests et exemples identiques avec et sans forme compacte
    (`TCL_TYPES_COMPACT=0/1`) ;
  - valgrind propre dans les deux modes sur tous les tests et exemples, sauf les fuites
    bénignes à la sortie de `exemples/executable_pe.tcl` (appel à `exit`) ;
  - suite complète de Tcl (`make test`) : depuis la v16y, 8 échecs, exactement ceux d'un Tcl 9.0
    d'origine ici (`httpProxy`, sans réseau) ; 51 095 tests, 45 114 passés, 5 973 sautés.
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
| v15w | `type binary encode`/`decode` par copie des cases quand la disposition est celle des octets |
| v15x | cast vers une liste en cases sans objet par élément ; `Tcl_AnnotatedCreateCells` (Bézier en C x 77) |
| v15y | boucles en cases, champs typés sur place, `X is T = v` direct, pointeurs de données dans `Interp`, mémo des types |
| v15z | `lappend` sous contrat sur place (7,7 s → 15 ms pour 20 000 points) |
| v16a | codec C : chaînes fixes et `z`, littéraux, sommes balisées ; `type binary decode` ne convertit plus les données inutilement |
| v16b | `typeparse/VERSION`, `tcl::unsupported::typesversion`, versions affichées et vérifiées par `maj.sh`, `version.tcl` |
| v16c | instructions `declareLocal` (`X is T = v`) et `loadFieldNum` (`$a(x)` nombre dans `expr`), `nom = v` par `storeScalar` (patch `expr-declaration-champs.patch`) |
| v16d | codec C : tailles calculées et préfixes, `at(EXPR).T`, liens `*(…)` (opt, listes chaînées) |
| v16e | affectation par tuple : instruction `tupleAssign` (patch `expr-tuple.patch`) ; `maj.sh` plus sûr |
| v16f | `$P(*)` rapide ; `cstruct.c` par `tclAnnotatedApi.h` (cases lues en place) ; banc cstruct mis à jour |
| v16g | `set {T}x a b…` rapide (produit de scalaires, variable locale compilée) ; `loadFieldNum` pour les opérateurs de bits (patchs `expr-champs-bits.patch`, `set-type-local-compile.patch`) |
| v16h | listes de taille fixe en cases (matrices, N dimensions) ; `$M(i.j)` lu et écrit dans la case ; `lset` profond sur une liste en cases corrigé |
| v16i | exemple `exemples/matrices` (C) ; lecture directe de `$M(i.j)` depuis une vue et dans une liste ordinaire |
| v16j | `type forget T ?T ...?` : le nom redevient libre ; valeurs et variables déjà typées gardent l'ancienne définition |
| v16k | `TCL_TOKEN_ANNOT` = 2048 (patch `jeton-annot-2048.patch`), pour laisser 512 à `TCL_TOKEN_CMD_IN_EXPR` de la TIP 759 |
| v16l | `{T}$x` : cast immédiat ; étiquettes `{nom:}` pour champs et variantes, plus de « contexte d'abord » |
| v16m | piste B : expiration des annotations à la fin de la commande qui reçoit le mot (instructions `annotMark`, `annotExpire`) ; plus de cast immédiat ; boucles et `lassign` résolvent leurs listes (`annotSettle`) ; `type {T}mot` |
| v16n | sommes : `{disjoint?}`, `{-strict}`, `{-deduce}`, avertissement ; options multiples `{-a,-b}` |
| v16o | variantes construites par leurs champs (étiquettes, chemin à travers une somme) ; `type def NOM` rend la définition |
| v16p | procédures typées : arguments en produit, étiquettes, `{list.T}args`, `{var.T}` (upvar), type de retour, `type proc` (patch `procs-typees.patch`) |
| v16q | options et drapeaux : champs `-x`, `{flag}`, préfixes, options exclusives (somme), dépendantes (variante produit), groupe `{T}-o` |
| v16r | dépliage du résultat : `{*}T` (liste), `{**}T` (dict), en retour de procédure et dans `type {…}mot` |
| v16s | optimisations : liaison des arguments, listes de scalaires nues dans les parcours, `lrange` en tranche, écritures sous contrat (patch `procs-typees-locales.patch`) |
| v16t | produits ouverts dans la vue liste, chaînes atomiques (fin de la règle B) ; cartographie EIAS |
| v16u | espaces de noms des types ; descripteurs comptés (une valeur survit à son interpréteur) |
| v16v | chemins compilés gardés, lecture typée rapide (`lindex`, `lassign`, rangement) ; patchs `expr-fonctions-champs.patch`, `execute-lecture-typee.patch` |
| v16w | sommes balisées (`variante charge`), unions `\|` disjointes ; plus de `{-strict}`/`{-deduce}` ; `type variant` |
| v16x | étiquette d'un retour lue hors compilation (patch `retour-etiquette.patch`) ; `_Static_assert` de secours (tcc 0.9.27) |

## 2. Installation et tests (chez l'utilisateur, MinGW64)

    bash typeparse/maj.sh [typeparse.zip]     # extrait, copie les sources de types, applique
                                               # les patchs NOYAU de ORDRE.txt (-F0), make clean,
                                               # recompile, puis compare tests_tclsh/*.tcl à leur .out
    bash typeparse/maj.sh --help               # options : -n, -y, --no-build, --no-test, --commit "msg"
    app/bin/tclsh90.exe typeparse/version.tcl  # version du tclsh installé (devinée avant la v16b)

À chaque livraison : changer `TCL_TYPES_VERSION` (`tclTypes.h`) ET `typeparse/VERSION` ensemble.

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
  6. `make test` complet : seuls les 8 échecs `httpProxy` (réseau) doivent rester ;
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
* **Scalaires et conteneurs (v16t)** : `list`, `dict` et les produits se déplient dans la vue
  liste (un produit en ses champs, chacun typé) ; un scalaire typé (chaîne, nombre, littéral), un
  lien est UN élément ; une somme prend la nature de sa variante active. (v15 à v16s : « règle
  B », un produit était UN élément.)

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

**Performances.** Après la v16c, l'étape annotée de l'exemple `geometrie` est à 1,1 à 2,8 fois
le Tcl nu, les voies C de 3 à 150 fois plus rapides que lui. `X is T = v` est une instruction
(`declareLocal`), `$a(x)` opérande d'un opérateur arithmétique est lu comme nombre (`loadFieldNum`,
≈ 60 ns). Le codec C couvre depuis la v16d les tailles calculées, les préfixes, `at(...)` et les
liens (restent : référentiels nommés, `-linkdepth` fini, `-dedupe`, lien nul à défaut). Restent aussi : de vraies entrées de stub (les fonctions de `tclAnnotatedApi.h`), les appels de
procédure (avec les signatures), la vue fabriquée par chaque `lindex` d'une liste en cases.
`set {T}x a b…` est passé en v16g de 1,3 à 0,6 µs pour un point (`X is point = (…)` : 0,2 µs) ; le reste
est l'appel de `::tcl::annotraw` (objet annoté créé puis libéré à chaque tour, texte du type relu) :
le compiler sans cet appel serait l'étape suivante.
Bancs : `exemples/geometrie/bench.tcl` et `bench_parties.tcl` ; profil avec callgrind.

Les autres points ouverts sont dans le planning et au § 17 de `doc/ANNOTATIONS.fr.md`, entre
autres :
* valeur « inexistante » en général (`opt(T)` ne couvre que les liens binaires) ;
* annotation de commentaire `{#}`, du premier mot, de déclaration dans `proc` ;
* notes par élément d'une liste typée ;
* forme compacte : cases à pointeurs (listes de taille variable, chaînes), `lindex` sans vue neuve, `lappend` en temps
  constant, `{-compact}` pour une famille ;
* `type {c!}export` pour les types récursifs, cible `{llvm!}`, sens inverse (en-tête C →
  `type def`) ;
* codecs C pour sommes, liens et tailles calculées ;
* exemples `arbre_var.tcl` et `defauts_calcules.tcl` à corriger ; banc C sur maquette.

## 6. Pour reprendre

Joindre `typeparse.zip` (dernière version) et ce fichier. Dire ce qui a été fait depuis
(résultat de `maj.sh`, modifications locales), puis le point du planning à traiter.
