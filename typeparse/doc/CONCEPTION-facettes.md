# Facettes : des sortes d'objets déclarées par les types

*Conception, 21 novembre 2026 (sur TIP 764 v17k). Rien de ce qui suit n'est encore
implémenté ; les notations marquées « à fixer » sont provisoires.*

## 1. Objet

Une classe TclOO se définit sur une seule dimension : ses méthodes (`oo::define`). Les objets
de VolɷDuctus en ont plusieurs, et leur liste dépend de la *sorte* d'objet :

* tout être (entité, relation, convolution) a des propriétés (sa fiche), des références
  vers d'autres êtres, et des convoluences (les convolutions où il peut entrer ; pour une
  instance, celles qu'il a subies) ;
* une relation a en plus ses étants, ceux dont elle est faite ;
* une convolution a en plus ses actants, ses situations (circonstants : lieu, moment,
  contexte), et ses conséquences (précondition `\conduit` postcondition, par issue).

Chaque sorte est un **métamodèle de classe** : elle fixe les domaines qu'une classe de cette
sorte doit déclarer à sa construction, en plus du bloc de méthodes usuel. Ces domaines sont
les **facettes** (au sens de la classification à facettes : une chose décrite selon plusieurs
axes indépendants).

Le modèle de la TIP 558 (`oo::configurable`, `property`, `configure`/`cget`, méthodes
`<ReadProp-…>`) n'est pas repris : les propriétés ne sont qu'une facette parmi d'autres, et
ce sont les types de la TIP 764 qui portent la déclaration, pas une métaclasse configurable.

## 2. Trois étages

| Étage | Exemple | Est une valeur de | Définit |
|---|---|---|---|
| sorte | `Entity` | `meta.class` | la liste des facettes (nom, sorte de facette, facultative ou non) |
| classe | `Article` | `Entity` | la déclaration de chaque facette, et les méthodes |
| instance | `visM12` | `Article` | l'état que chaque facette porte |

Les annotations sont des métadonnées ; à chaque étage, une valeur se fige en type pour
l'étage d'en dessous : la classe `Article` est une valeur du type `Entity`, et devient le type
de ses instances. Un quatrième niveau, `meta.facet`, définit les sortes de facettes.

```tcl
type def {meta.class}Being {
    {propriétés}propriétés; {références}références ?= {}; {convoluences}convoluences ?= {}
}
type def {meta.class}Entity      Being
type def {meta.class}Relation    {Being; {relatif}étants}
type def {meta.class}Convolution {
    Being; {actif}actants; {circonstanciel}situations ?= {date quand};
    {conséquences}conséquences
}

set {Entity}Article propriétés $proto def {
    method {int}ajouter {{int}n} { incr quantité $n }
}
set {Relation}fournit étants {fournisseur Fournisseur  article Article}
set {Article}visM12 propriétés {nom vis quantité 12}
set {fournit}f1 étants {fournisseur $acme  article $visM12}
$visM12(références)            ;# dérivée : {fournit article f1}
```

`meta.class` se lit comme une composition de la TIP 764 (`list.int`) : « méta de classe », un
type dont les valeurs sont des classes.

## 3. La vie d'un objet : `Lifecycle`

Les étapes sont celles que TclOO déroule (`tclOO.c`, `tclOOBasic.c`), fondues en six :

| Étape | Dans TclOO | Ce qui s'y passe |
|---|---|---|
| `name` | `TclNewObjectInstanceCommon` | nom donné ou `::oo::ObjN` ; refus si la commande existe |
| `alloc` | `AllocObject`, `TclOOAllocClass` | espace de noms, commande, `my`, chemin `::oo::Helpers` ; l'objet entre dans les instances de sa classe ; structure de classe si la classe est une métaclasse |
| `construct` | `TclNRNewObjectInstance`, `FinalizeAlloc` | chaîne des constructeurs (`next`) ; un échec détruit l'objet ; « object deleted in constructor » (mort-né) |
| `clone` | `Tcl_CopyObjectInstance`, `<cloned>` | allocation sans constructeur ; copie des méthodes propres (`cloneProc`), mixins, filtres, déclarations de variables, métadonnées (`cloneProc`), classe ; puis `<cloned>` (par défaut : procédures et variables de l'espace) |
| `reclass` | `oo::objdefine o class X` | changement de classe, sans constructeur |
| `destroy` | `destroy`, `ObjectNamespaceDeleted` | chaîne des destructeurs ; sortie des instances ; une classe emporte sous-classes et instances ; `deleteProc` des métadonnées ; espace supprimé |

Pour une classe, la *définition* (le script `oo::define`) a lieu pendant `construct` : c'est
le constructeur d'`oo::class` qui l'évalue.

```tcl
type def Lifecycle {
    {fn({class}cls; {string}name)}            name      ?= {};
    {fn({object}o)}                           alloc     ?= {};
    {fn({object}o; {list}words)}              construct ?= {};
    {fn({object}src; {object}dst)}            clone     ?= {};
    {fn({object}o; {class}from; {class}to)}   reclass   ?= {};
    {fn({object}o)}                           destroy   ?= {}
}
```

Chaque champ est un point d'accroche : une lambda typée (v16z) de la signature indiquée ; une
étape omise ne fait rien.

**Règle d'écriture.** Une valeur `Lifecycle` se construit **par étiquettes**
(`construct {…} destroy {…}`), jamais par position. On pourra ainsi séparer plus tard
`finalize` de `construct` et `release` de `destroy` (deux champs facultatifs de plus) sans
rien casser : l'ordre d'exécution est déjà le bon (`finalize` après `construct`, `release`
après `destroy`), un point d'accroche existant reste juste, seulement moins fin.

**Ancêtre en C.** `Tcl_ObjectMetadataType` (`{version, name, deleteProc, cloneProc}`) est
déjà un produit de deux de ces étapes : c'est le moyen par lequel TclOO laisse une extension
attacher un état à un objet. Une facette en est la généralisation à toutes les étapes, et
l'implémentation en C s'appuiera sur les métadonnées.

## 4. Les sortes de facettes : `Facet(D)`

Une sorte de facette est paramétrée par le type `D` de ce que la classe déclare :

```tcl
type def Facet({type}D) {
    {fn({D}d -> type)}carry;                ;# ce que porte l'instance (notation à fixer)
    {fn({D}sup; {D}sub -> D)}inherit ?= {}; ;# déclaration d'une sous-classe
    {fn({class}cls; {model}m -> D)}derive ?= {}; ;# facette dérivée : la vue inverse
                                            ;# des déclarations des autres classes
    {Lifecycle}life ?= {}                   ;# ce que la facette fait à chaque étape
}
```

* **Facettes dérivées.** Une facette qui a un `derive` ne fait pas foi : sa valeur se
  calcule depuis les déclarations des *autres* classes (§ 7, « Qui déclare »). Si la classe
  l'écrit quand même, c'est une **promesse**, confrontée à la dérivation (§ 7bis).
* **Générique, pas dépendant.** Les signatures des points d'accroche ne dépendent que de `D` :
  elles sont vérifiées une fois, à la définition de la sorte de facette. Aucun champ de
  `Facet` ne dépend de la valeur d'un autre champ.
* **`meta.facet`** nomme une sorte de facette : il lit `declare`, instancie `Facet(D)` et
  vérifie le reste contre cette instance (comme un sélecteur de variante des types binaires,
  une fois, à la définition).

```tcl
type def {meta.facet}propriétés {
    declare record
    carry   {{{record}d} { type get $d }}
    inherit {{{record}sup; {record}sub} { type extend $sup $sub }}
    life    {alloc {…} construct {…} clone {…}}
}
```

* **`carry` traverse un étage.** Le type d'une instance d'`Article` dépend de la *valeur* de
  sa déclaration (le prototype de ses propriétés). Il est calculé une fois, à la création de
  la classe, et donne un type nommé (`Article` ; `Entity(Article)` dans les messages), comme
  `deref(T)` ou le `record` de `type call`. Le type d'une instance est le produit, facette par
  facette, de `carry(déclaration)`.
* **Produits dépendants.** Ils existent déjà dans les types binaires (`list($n).T`,
  `corps($type)`, `at(EXPR).T` : un argument entier ou un sélecteur lu dans un champ déjà
  lu). Un champ dont le *type* serait la valeur d'un autre (`{type}t; {$t}v`) n'est pas
  nécessaire ici : une valeur annotée porte déjà son type. On ne généralisera `$champ` aux
  types que si un cas le demande.

### Déclarations en dialecte

Quand une déclaration se lit mal en valeur littérale (convoluences, automates), la facette
déclare un **dialecte** : `declare dialect(::vd::conv)`. Le script de la classe est évalué
une fois, à sa construction, dans cet espace de commandes de déclaration, et devient une
valeur de type `D`, qu'on peut inspecter, vérifier et fusionner à l'héritage. C'est le modèle
d'`oo::define`.

Le bloc de méthodes usuel n'est donc pas un cas à part : c'est la facette `def`, de
déclaration `dialect(oo::define)`, que `meta.class` fournit à toutes les sortes.

## 5. Les sortes d'objets : `meta.class`

```tcl
type def {meta.class}Being {
    {propriétés}propriétés; {références}références ?= {}; {convoluences}convoluences ?= {}
}
type def {meta.class}Entity      Being
type def {meta.class}Relation    {Being; {relatif}étants}
type def {meta.class}Convolution {
    Being; {actif}actants; {circonstanciel}situations ?= {date quand};
    {conséquences}conséquences
}
```

* **Noms.** Les sortes portent des noms anglais dans le code (`Being`, `Entity`,
  `Relation`, `Convolution`) ; les facettes du prototype aussi (`properties`, `references`,
  `convoluences`), la spécification gardant les noms français pour les autres (`relatif`,
  `actif`, `circonstanciel`, `conséquences`) jusqu'à leur implémentation.
* **Une sorte de base, `Being`.** Les trois sortes partagent propriétés, références et
  convoluences ; une sorte hérite d'une autre par prolongement du produit (`{Being; …}`),
  comme une sous-classe prolonge les propriétés de sa superclasse. Les noms accentués ne
  posent pas de problème à Tcl ni aux types (`{c!}export` les rend valides en C).
* **La liste des facettes est une signature de produit.** Une classe de la sorte `Entity`
  est une valeur de ce produit : une facette inconnue ou une facette obligatoire manquante est
  une erreur de construction, avec les messages ordinaires de la TIP 764. Le nombre de
  facettes est ainsi fixé par sorte.
* **`def` est toujours là**, hérité de `meta.class`.
* **Composition des étapes.** La sorte compose les `Lifecycle` de ses facettes dans l'ordre
  déclaré : `alloc` et `construct` de la première à la dernière, `destroy` de la dernière à la
  première. Le constructeur TclOO de la classe s'exécute **après** les facettes, son
  destructeur **avant** : le constructeur voit un objet aux facettes remplies, le destructeur
  un objet encore entier.
* **Construction.** Les mots de `set {Article}x …` sont étiquetés par facette
  (`propriétés {…} références {…}`) ; chaque facette prend les siens à `construct`, avant le
  constructeur, qui ne reçoit que ce qui reste.
* **Effets de la construction d'une classe.** `set {Entity}Article …` crée la classe TclOO
  `Article` dans l'espace de la variable, et le type `Article` ; la variable contient le lien
  vers la classe.

## 6. Les liens vers les objets

Un objet est une référence mutable ; une valeur `{Article}` est un **lien** vers lui (comme
les liens `var` et `*(…)`), et les chemins le traversent.

```tcl
$visM12(propriétés.quantité)              ;# lecture
incr visM12(propriétés.quantité)          ;# écriture : modifie l'objet
set {Article}autre $visM12                ;# même objet, deux liens
set autre $vis2                           ;# relie : autre désigne un autre objet (contrat Article)
set {Article}copie [type deref $visM12]   ;# reproduction : un nouvel objet
$f1(étants.fournisseur)(nom)              ;# un lien suivi jusqu'à un autre objet
```

* **Relier ou modifier.** Écrire la variable relie le lien ; écrire par chemin modifie
  l'objet (comme `p = q` et `p->f = x` en C).
* **Premier segment de chemin : la facette.** Deux facettes peuvent avoir un champ de même nom.
* **Sous-typage nominal.** `{Article}` accepte une instance d'une sous-classe d'`Article` :
  premier sous-typage par héritage dans la TIP 764.
* **`type deref`** rend le produit des facettes de l'objet, figé (une valeur
  `deref(Article)`).
* **Les deux formes d'appel restent** : `$visM12 ajouter 2` (la valeur est le nom de la
  commande de l'objet).

## 7. Les facettes de base

### `propriétés`, `références`, `convoluences` (toutes les sortes) et `def`

| Étape | `propriétés` | `références` | `convoluences` | `def` |
|---|---|---|---|---|
| déclare | un prototype (produit typé), avec ses clés | **dérivée** : les relations où l'être est étant, avec son rôle ; écrite = promesse | **dérivée** : les convolutions où l'être est actant ou situation, avec son rôle ; écrite = promesse | méthodes internes (`oo::define`) |
| porte | une variable d'objet par champ, typée | les liens de relation où l'instance figure (vue inverse des instances de relation) | le journal : les convolutions subies, chaque entrée en mode *actant* (avec son rôle) ou *situationnel* (avec le nom de la situation) | — |
| `alloc` | valeurs du prototype : chaque variable naît typée, son contrat est collant | aucun lien | journal vide | méthodes internes installées ; la facette `convoluences` installe les **méthodes publiques** (§ 7ter) |
| `construct` | ses mots étiquetés | — (les liens naissent des relations) | — | — |
| `clone` | reproduction des valeurs | aucun lien : une relation ne se copie pas avec un de ses étants | à surveiller (§ 10) ; journal vide dans le prototype | méthodes propres clonées |
| `reclass` | prolonger ou réduire le produit | refus si l'instance figure dans une relation que la nouvelle classe ne permet pas | refus si une convolution du journal n'est plus déclarée | — |
| `destroy` | — | l'alerte des relations dépendantes (§ 7, `relatif`) | — | — |
| `inherit` | `type extend` (champ en double de type différent : erreur) | dérivée : une relation qui cite la superclasse vaut pour la sous-classe | idem | `superclass` |

**Les propriétés vivent dans des variables d'objet**, une par champ, déclarées par la facette
(`oo::define variable`) : dans les méthodes, `incr quantité` ; de l'extérieur,
`$visM12(propriétés.quantité)` lit et écrit la même variable, sous le même contrat. Un objet
naît complet : il n'y a jamais de propriété déclarée et sans valeur, et le contrat d'une
variable est posé dès `alloc`. La question des variables typées sans valeur
(`variable {int}n`) disparaît pour les objets à facettes.

**Les clés** sont une marque sur les champs du prototype (notation à fixer, par exemple
`{string}nom ?clé`) ; l'étape `name` s'en sert pour l'identité stable.

**Les états ne sont pas une facette.** L'automate d'un être se déduit de ses convoluences :
les conséquences `pré \conduit post` des convolutions déclarées donnent les états (les
participes) et les transitions, comme l'état dérivé de VolɷDuctus.

### Qui déclare

Une relation déclare ses étants, une convolution ses actants et ses situations : c'est là que
l'information fait foi (`CONCEPTION-etants.md` de VolɷDuctus). Les facettes `références` et
`convoluences` d'une entité en sont la **vue inverse** : dérivées par défaut, une promesse si
on les écrit. On voit l'être entier au même endroit, sans doublon.

| Fait foi | Vue inverse, dans l'être cité |
|---|---|
| `relatif` (étants d'une relation) | `références` |
| `actif`, `circonstanciel` (actants, situations d'une convolution) | `convoluences` |

### `relatif`, `actif` : deux vies

Les deux déclarent rôle → classe et cardinalité (`dict.string.Role`) ; ce qui les distingue
est leur `Lifecycle`.

| | `relatif` (les étants d'une relation) | `actif` (les actants d'une convolution) |
|---|---|---|
| sens | la relation **est faite de** ses étants | des participants à un procès |
| identité | le n-uplet des étants est la clé : deux relations identiques sont un doublon | non |
| `construct` | tous les étants requis, unicité vérifiée | actants saisis, précondition vérifiée |
| `destroy` d'une cible | refus avec alerte, ou `destroy -cascade` (voir ci-dessous) | rien : un procès passé reste au journal |

**Destruction d'un étant.** `destroy` sur un être qui est étant d'une ou plusieurs relations
est refusé, avec une erreur qui renseigne (code `{VD DESTROY DEPENDENTS}`) et nomme la
**fermeture** des dépendants (une relation peut être l'étant d'une autre) :

```tcl
$palette destroy
;# PAL_1 est étant de 2 relations : Contient(CAM_AB123, PAL_1), Stocke(DÉPART_2, PAL_1)
$palette destroy -cascade      ;# détruire tout, relations comprises
info object dependents $palette ;# la liste, sans rien détruire
```

L'interface pose l'alternative (« détruire tout » / « ne pas détruire ») à partir du code
d'erreur ; un script choisit par l'option.

### 7bis. Promesses et fermeture du modèle

Une facette dérivée écrite par la classe est une **promesse**. Dans la phase d'essai, une
promesse est **exhaustive** : une participation qui n'y figure pas est une contradiction. On
pourra relâcher plus tard (demander confirmation sur une contradiction, permettre des
reconfigurations).

* **Au fil de l'eau** : à chaque définition (d'une relation, d'une convolution, d'une
  promesse), on cherche les **contradictions** de la classe qui vient d'être définie et des
  classes qu'elle cite. Erreur immédiate, à la ligne qui l'a causée. Coût : une recherche par
  rôle déclaré (un index classe → promesses).
* **À la fermeture** (`modèle clore`, ce que fait `/modele/verifier`) : tout est revérifié,
  y compris les **promesses non tenues** (une participation promise qu'aucune définition ne
  déclare), qu'on ne peut voir qu'une fois le modèle complet. C'est aussi le filet après une
  redéfinition. La fermeture est la même vérification, appliquée à toutes les classes.

Une étape de niveau **modèle** s'ajoute donc à `Lifecycle`, qui est de niveau objet : la
fermeture. C'est elle qui calcule les facettes dérivées et les fige.

### 7ter. Les méthodes publiques sont les convolutions

Ce qui change le monde passe par une convolution : typée, en phases, journalisée. La facette
`convoluences` installe sur l'être une **méthode publique par convolution où il entre**, selon
son rôle : si une Personne est le prêteur de `prêter`, `$p prêter objet à x` crée la
convolution. Les méthodes de `def` restent **internes** : elles ne modifient pas les
propriétés directement. C'est la distinction entre état public et mécanique interne qui
manque à TclOO.

### `circonstanciel` : les situations (convolutions)

Les compléments de situation de VolɷDuctus (18/11) : le contexte d'une convolution (lieu,
moment, société), qui n'est pas un actant — les *circonstants* de Tesnière, à côté des
actants (les champs `-x` des signatures d'attentrel).

| | `actif` | `circonstanciel` |
|---|---|---|
| déclare | rôle → classe | situation → classe ou type, et obtention (désignation, `quand`, `?`) |
| porte | les actants liés | les valeurs captées |
| `construct` | phase `à` ; rôle `one` absent → mort-né | phase `à`, **avant les conséquences** ; introuvable → « inconnue », sans échec |
| au journal de l'être lié | entrée *actant*, avec son rôle | entrée *situationnelle* (l'arc `situationnel`, genre relatif) |

* **Défaut de sorte.** Une situation déclarée pour toutes les convolutions (`date * quand`)
  est le défaut de la sorte : `?= {date quand}` dans `meta.class Convolution`, dont chaque
  classe de convolution hérite. Une sorte peut ainsi porter des déclarations par défaut.
* **Points de vue.** Ils ne sont pas une facette : la facette déclare les situations
  *possibles*, les points de vue choisissent celles qu'on capte (l'union des points de vue qui
  concernent le verbe). Ils restent une déclaration du modèle (`[Points de vue]`).

### `conséquences` (convolutions)

Les conséquences d'attentrel : *Produit* (`() \conduit X`), *Conduit* (changement d'état),
*Détruit* (`X \conduit ()`), écrites `pré \conduit post` sur les actants et les produits.

* **Déclaration** : par issue, un jeu de conséquences (dialecte d'`effets.tcl`) ; une
  convolution sans issue n'en a qu'un. Les issues de VolɷDuctus (`VERBE·ISSUE`) sont les noms
  de ces jeux.
* **Portée** : une conséquence peut viser plusieurs actants, et des produits, qui ne sont pas
  des actants fournis à la construction ; c'est pourquoi elle est une facette, et non une
  partie d'`actif`.
* **Instance** : l'issue choisie et les êtres produits.

### Une convolution qui a lieu est un objet qu'on construit

Les cinq phases d'attentrel se placent sur `Lifecycle` :

| Phase | Étape |
|---|---|
| `pour` (préparation) | `alloc` |
| `à` (précondition, actants saisis, situations captées) | `construct` d'`actif`, puis de `circonstanciel` |
| `par` (le corps) | le constructeur |
| `de` (affectations, postcondition) | `construct` de `conséquences`, après le constructeur |
| `fors` (journal) | l'écriture dans les `convoluences` de chaque actant |

« Un échec avant `fors` défait tout » est un objet mort-né. Une convolution portant elle-même
des convoluences (un achat approuvé ou rejeté) entre dans le même modèle : son actant est la
convolution réifiée.

## 8. Introspection

* `type get $visM12` → `Article` ; `type get $Article` → `Entity` ; `type get $Entity` →
  `meta.class`.
* `type def Entity` → la liste des facettes ; `type def Article` → le type calculé des
  instances (produit des `carry`).
* `info class` et `info object` restent valables sur les classes et objets créés.

## 9. Ce qu'il faut ajouter à la TIP 764

1. `fn(SIGNATURE -> R)` : le type d'une lambda typée de signature donnée (notation à fixer).
2. `class` et `object` comme types de liens vers TclOO, avec le sous-typage par héritage.
3. Le préfixe `meta.` : un type dont les valeurs sont des classes (`meta.class`) ou des
   sortes de facettes (`meta.facet`), et dont la construction a un effet (créer la classe,
   le type).
4. `dialect(NS)` : une déclaration écrite en script, évaluée dans un espace de commandes.
5. L'écriture par chemin à travers un lien d'objet.
6. `type extend` (prolonger un produit) pour l'héritage des propriétés et des sortes.
7. `destroy -cascade` et `info object dependents` (côté TclOO).

## 10. Questions ouvertes

* **Durée de vie.** Proposé : un objet vit jusqu'à `destroy`, comme en TclOO ; un lien vers
  un objet détruit vise une cible *inexistante* (erreur claire). Une possession par la
  variable demanderait un ramasse-miettes.
* **Nom des instances.** Anonyme (`::oo::ObjN`) ou tiré de la variable, ou tiré des clés
  des propriétés à l'étape `name` (identifiant stable et clés déclarées de VolɷDuctus).
* **`convoluences` à la copie (à surveiller).** Journal vide ou copié : la réponse dépend
  sans doute du caractère **générique** (un modèle, une sorte d'article) ou **spécifique**
  (un exemplaire) de l'être copié. L'étape `clone` recevra ce mode quand on saura le
  distinguer ; le prototype part d'un journal vide et on observe les cas réels.
* **Points de vue.** Déclaration du modèle, ou attachés au rôle de l'observateur ?
* **Relâcher les promesses.** Exhaustives dans la phase d'essai ; plus tard, confirmation
  sur contradiction, reconfigurations.
* **Écriture des promesses dans le `.vdm`.** Colonnes de la feuille Entités, ou section
  propre ?
* **Mixins.** Apportent-ils des facettes, ou seulement des méthodes ?

## 11. Prototype

*(21/11 : première étape faite dans VolɷDuctus — `moteur/facets.tcl`, sortes `Being` et
`Entity`, facettes `properties`, `references`, `convoluences` dérivées avec promesses et
fermeture ; `vdmtypes.tcl` crée une classe `Entity` par entité ; essai
`exemple/facettes_exemple.tcl`.)*

Un prototype en Tcl, dans le moteur de VolɷDuctus (premier client), avant toute modification
du C ; il remplace `vdmtypes.tcl` sans changer le format `.vdm` (le *typage unique* : la
classe de sorte `Entity` est le type), en commençant par les entités (propriétés et clés,
références et convoluences dérivées, promesses, fermeture) :

1. `Lifecycle`, `Facet(D)` en script ; `meta.class` comme une commande
   (`sorte create Entity {…}`), les facettes comme des valeurs de `Facet` ;
2. les facettes `propriétés`, `références`, `convoluences`, `relatif`, `actif`,
   `circonstanciel`, `conséquences`, `def`, au-dessus de TclOO (constructeur
   et destructeur engendrés, métadonnées en variables d'espace) ;
3. essai sur `logistique.vdm` et `achat.vdm`, et sur l'achat d'attentrel (conséquences,
   phases, journal) ;
4. puis, en C : `set {meta.class}…` / `set {Article}…`, les types `class`/`object`, les
   chemins à travers les liens, les facettes sur `Tcl_ObjectMetadataType`.

## 12. Réserves (à vérifier dans le prototype)

*Consignées le 21 novembre 2026, après la mise au point du modèle. Aucune ne le remet en
cause ; elles disent où regarder en premier.*

1. **La profondeur.** Quatre étages (`meta.facet`, `meta.class`, la classe, l'instance) : une
   erreur peut naître à l'un et se manifester à l'autre. Chaque message doit dire à quel étage
   et dans quelle facette elle se produit (par exemple `Entity Article, facette références,
   construct : …`), sinon le modèle sera difficile à déboguer.
2. **`set` qui crée une classe.** Faire d'une assignation un acte de création s'écarte de Tcl ;
   c'est là que la relecture de la TIP résistera. La forme commande reste canonique
   (`Entity create Article …`), `set {Entity}Article …` en est l'écriture équivalente.
3. **L'intégrité des liens.** Sans ramasse-miettes, les liens inverses (`références`,
   `relatif`, l'alerte de destruction et sa fermeture) seront la partie la plus délicate à
   tenir juste, plus que les types. À tester d'abord : destruction, cascade, copie,
   reclassement, sur des relations de relations.
4. **Le générique et le spécifique.** La question de la copie du journal touche une
   distinction plus profonde : l'article au catalogue et l'exemplaire en stock (le type et
   l'occurrence ; les *powertypes* des ontologies). Elle reviendra ailleurs que dans la copie ;
   il faudra peut-être qu'elle ait sa place dans le modèle (un être générique comme classe,
   un être spécifique comme instance ?). Le `.vdm` la porte déjà (colonne Identité :
   `spécifique`, `générique UNITÉ` — des exemplaires anonymes, comptés ou mesurés).
5. **Le coût.** Chaque convolution devient une création d'objet, qui passe par les étapes de
   chaque facette (des lambdas en Tcl dans le prototype). Pour les volumes de VolɷDuctus cela
   devrait aller, mais il faut le mesurer : création, chemin à travers un lien, destruction
   en cascade.

**Points d'appui.** Le protocole des métaobjets de CLOS repose sur les mêmes idées
(définitions de champs porteuses de comportement, création en étapes surchargeables :
`allocate-instance`, `initialize-instance`, et `change-class`, notre `reclass`) et a trente
ans d'usage ; on pourra s'y référer pour les cas délicats (reclassement, instances
obsolètes quand une classe change).

