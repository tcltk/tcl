# REPRISE — état du prototype TIP 764 (valeurs annotées, variables typées)

*Fiche de reprise, 19 octobre 2026, version **v13**. À joindre (avec `typeparse.zip`) au
premier message d'une conversation qui reprend le travail.*

## 1. Où en est-on

* **v13 livrée** : forme nom/valeur nue d'un produit supprimée, étiquettes de champ
  `{champ}valeur`. Tests chez l'utilisateur jusqu'en v12 : `bash typeparse/tests.sh` →
  59 OK, 0 échec. v13 vérifiée ici : 32 tests avec `.out` OK, les autres sans erreur,
  AddressSanitizer propre, mémoire stable.
* **Manuel** : `MANUEL.md` (référence, français) et ses traductions `MANUAL.{en,de,nl,es,pt,it}.md`.
  Notes de patch : `noyau/LISEZMOI.md` (une section par patch, ordre dans `noyau/ORDRE.txt`).
* **Documents de résultats des codecs C** (2 pages, fr/en/de/nl/la, + SVG de l'objet annoté) :
  livrés pendant la conversation, hors archive.

## 2. Installation et tests (chez l'utilisateur, MinGW64)

    bash typeparse/maj.sh [typeparse.zip]     # extrait, copie les sources de types, applique
                                               # les patchs NOYAU de ORDRE.txt (-F0), recompile
    bash typeparse/tests.sh                    # tests_tclsh/tNN_*.tcl comparés à leur .out

Les sources de types (`tclTypes*.{c,h}`, `tclAnnotObj.c`, `tclTypedVar.c`, `tclTypePath.c`,
`tclTypeGeneric.c`, `tclTypeParse.c`, `tclVarRef.c`) sont livrées ENTIÈRES à la racine de
`typeparse/` et copiées dans `generic/` ; elles ne sont jamais patchées. Seules les sections
« noyau » des patchs s'appliquent (tcl.h, tclParse.c, tclCompile.c/.h, tclCompCmds.c,
tclExecute.c, tclVar.c, tclTest.c, …).

## 3. Reconstruire l'environnement de travail (côté Claude)

* Base : Tcl `core-9-0-branch`, commit git `f9e939e312bf12c501d09dd3abbc1c01b0812a5d`, plus
  la branche TIP 759 de l'utilisateur, puis les patchs de `noyau/ORDRE.txt` dans l'ordre.
* Build unix : `cd unix && ./configure && make -j8 tclsh tcltest`.
* Garder trois binaires : tclsh patché, une compilation hors arbre avec
  `-fsanitize=address,undefined`, et un tclsh 9.0 d'origine (équivalence).
* Vérifications habituelles :
  - tests du projet (`tests_tclsh`, chaque `.tcl` comparé à son `.out`, `LANG=C.UTF-8`) ;
  - sorties de tous les `exemples/*.tcl` comparées à la version précédente ;
  - tests de Tcl `parse parseExpr parseOld subst var compile set upvar basic execute incr
    append lset dict info` identiques à la base (seul échec connu : `info-20.2`, dû aux
    fonctions `popcount`/`bitlen`, présent aussi dans la base patchée) ;
  - ASan sur les tests touchés ; boucle de 1 000 000 d'opérations pour vérifier que la RSS
    reste stable.
* Le banc C sur maquette (`typeparse/test/`, `make`) ne compile plus depuis longtemps (la
  maquette ignore `TCL_ONE_WORD_KEYS`, `PopcountCmd`, `BitlenCmd`) : à remettre à jour ou à
  retirer. Ce n'est pas une régression de la v13.

## 4. Décisions de conception arrêtées

**Valeurs annotées**
* Tcl_Obj annoté : `ptr1` = contenu, `ptr2` = type résolu, texte brut, `""` (marqueur `{}`)
  ou état `{T, notes}`. Une valeur rangée dans une variable garde son annotation brute.
* Résolution d'une annotation : dans le contexte d'abord (variantes d'une somme, champs d'un
  produit), puis les types globaux ; sinon c'est une note. Une annotation ne vient jamais
  du contenu d'une chaîne.
* `{age}{mm}12` (annotations littérales empilées) n'est pas une syntaxe : écrire `{age}$v`.

**Construction d'un produit (v13)**
* Ordre des cas : 1 argument de même type = copie ; `{T.champ}valeur` ; *n* valeurs pour
  *n* champs = positionnel ; 0 argument = défauts ; construction nommée interne (chemin,
  étiquettes) ; tout le reste = `wrong # args … write {field}value or set {T}var(field,...)`.
* Étiquettes : l'annotation en attente la plus récente de CHAQUE argument ; tout ou rien
  (mélange = erreur ; annotation inconnue à côté d'étiquettes = `unknown field`) ; une
  étiquette portée par une variable compte.
* Le dict garde sa forme clé/valeur.

**Chemins** (`tclTypePath.c`, modèle du wiki « A better way to do indexation »)
* Opérateurs `,` `.` `:` `( )`, `*` unaire (dépliage, `L(*0:2,4)`) ; construction
  `set {rect}R((A,B).(x,y)) …`.
* **Le chemin ne substitue rien** : dans `{…}` et `${…}`, `$` et `[` sont des erreurs
  explicites. Les valeurs substituées par Tcl dans `$nom(...)` et dans les mots nus sont
  **atomiques** (type `pathindex`, instruction `INST_INDEX_CONCAT`). Un chemin calculé se lit
  en nommant la variable : `set ref trame($chemin); set $ref`.
* Côté noyau : `TCL_TOKEN_INDEX` produit par `Tcl_ParseVarIndex`, appelé seulement par
  `Tcl_ParseVarName` sur `(` (jamais pour un mot comme `puts a(b c)`) ; parenthèses
  équilibrées, `"étiquettes"`, espaces permis ; repli exact sur la règle Tcl 9 quand l'index
  n'est pas bien formé (`info complete` inchangé).
* `${R(chemin)}` lit le chemin littéral pour un scalaire typé ; symétrie assumée avec les
  tableaux : `${a($k)}` ne substitue pas `$k`.
* `{T}nom(...)` : mot annoté jusqu'à la parenthèse équilibrée, espaces compris, sur une ligne.

**Binaire**
* `type binary` / `chan binary` ; représentation portée par le type ; liens `*(origine,repr).T`,
  `-linkdepth`, référentiels nommés, `-dedupe`, tailles dépendantes contrôlées à la
  construction.
* Codecs C (encodage et décodage) : prototype qui couvre les cas mesurés (tableaux de
  structures) et retombe sur le codec Tcl sinon ; `set ::tcl_typebinaryc 0` force le Tcl.

## 5. Ce qui reste ouvert

* **Documentation** : `doc/ANNOTATIONS.{fr,en,de,nl,es,pt,it}.md` (français = référence ; aussi en doc
  partagée « Annotations en Tcl — documentation »). Son § 17 tient la liste à jour des points ouverts,
  dont trois relevés en la rédigeant :
  - une variable non typée **adopte** le type d'une valeur typée reçue ; piège : `catch … m` réutilisé
    échoue si `m` a reçu une valeur typée, et l'erreur qui s'échappe est le message d'origine ;
  - `;` et `,` mêlés sans accolades sont acceptés (`{int}a; {int}b, {int}c`) au lieu d'être refusés ;
  - lire une variante inactive d'une somme donne `unknown way to cast the value from S to string`.
* La forme « chemin + étiquettes » (`set {rect}R(A) {y}2 {x}1`) n'est **pas** prévue : à voir à l'usage.

* « Exister / être nul / ne pas exister » pour une valeur pointée : variante sans valeur
  (`{{T}some, {}none}`), pas encore implémentée (voir `noyau/CONCEPTION-lien.md`).
* Pas d'`union`, de champs de bits partagés ni de liste « jusqu'à la fin du fichier »
  (choix assumé pour `union`).
* Étendre les codecs C à des structures plus complexes (sommes, liens, tailles dépendantes).
* Banc C sur maquette (§3) à remettre en état.
* `exemples/arbre_var.tcl` (commentaires dans un paramètre de `type def`) et
  `exemples/defauts_calcules.tcl` (namespace `::test` absent) échouent déjà en v8 : à
  corriger.
* Message possible à affiner : `set {paire}s {aut.nom}x` (étiquette d'un AUTRE produit)
  donne aujourd'hui `wrong # args` ; un message dédié serait plus clair.

## 6. Pour reprendre

Joindre `typeparse.zip` (dernière version) et ce fichier, dire ce qui a été fait depuis
(patchs appliqués, résultat de `tests.sh`, modifications locales), puis donner la suite.
