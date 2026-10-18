# Note de conception : le type lien `*(...)`

*État : discussion, **rien n'est implémenté**. Ce fichier consigne ce qui est décidé, ce qui
est proposé, et ce qui reste ouvert, pour pouvoir y revenir.*

## 1. Pourquoi

Aujourd'hui, un offset lu dans un fichier est un entier ordinaire, et la cible se lit avec
`at($off).T` (niveau 2 de `type binary`). La lecture est complète. L'écriture oblige à calculer
les offsets soi-même. Un type lien met l'adresse et la cible dans le même type, pour que
`type binary` calcule les offsets à l'écriture.

## 2. Notation (décidée)

```
{*(ORIGINE, REPR ?, OPTIONS?).CIBLE}nom
```

* `.` ne fait que composer (comme `list(3).T`, `at($off).T`) ; la représentation de l'adresse
  est dans les parenthèses (`u32le`, `i32le`, …).
* `*` est un constructeur du noyau avec **sa propre grammaire** d'arguments (comme `at(...)`),
  pas un type générique : les paramètres nommés ne passent pas dans les types génériques
  (vérifié : `m=3` est lu comme une valeur positionnelle).
* Pointeur vers pointeur : `*(...).*(...).T`.

### Origines (décidé)

| Écriture | Origine |
|---|---|
| `file://` | le début des données (fichier) |
| `memory://` ou `memory://NOM` | un espace d'adresses virtuelles, traduit par une table fournie à l'appel |
| `//` | racine de l'espace courant (synonyme court de `file://` pour un fichier) |
| `./` | début de la structure englobante = position de son premier champ, **après** son éventuel remplissage |
| `../` | début de la structure au-dessus (`../../` : deux niveaux) |
| `.` | l'emplacement du champ lui-même |

Un schéma (`file://`, `memory://`) ne fait **aucune entrée-sortie** : il dit à quel espace
l'adresse se rapporte. Les octets sont toujours ceux fournis à l'appel.

Le début de la structure se compte après le remplissage (convention du C et des spécifications
de formats). La différence avec « avant » n'existe que si le parent n'aligne pas le produit
(variante de somme, `at(...)`, produit de tête avec `-offset` non aligné).

### Options du type (décidé : grammaire propre à `*`)

`null=VALEUR` (valeur
nulle ; 0 par défaut si `null` est donné). Elles viennent après la représentation, dans
n'importe quel ordre.

## 3. Options d'appel de `type binary` (proposées)

* `-space TABLE` : table de traduction `{{vaddr offset taille} ...}` pour `memory://`.
* `-dedupe on|off` (écriture) : deux liens vers des valeurs égales donnent une copie ou deux.
  Proposé : `off`.
* `-into OCTETS` (piste, pas retenue pour l'instant) : superposer l'écriture à un fichier
  existant, pour modifier sur place. Ne concerne que les liens de frontière (`-linkdepth` fini).

`-place` a été **abandonné** : `keep` (écrire aux offsets d'origine) est redondant avec le mode
un lien de frontière connaît déjà son offset. L'écriture a une seule politique : cibles écrites à la
suite, alignées.

## 4. Présence : existant, nul, inexistant (décidé dans son principe)

Trois situations : **présente**, **présente mais nulle ou vide** (`0`, `{}`, `""`),
**inexistante** (lien nul). Un défaut de champ (`= {}`) ne distingue pas les deux dernières.

Décision : la distinction passe par une **somme avec une variante sans valeur** :

```tcl
type def {opt({type}T)} {{$T}some, {}none}
{*(//,u64le,null=0).opt(list($shnum).section)}sections   ;# e_shoff = 0 : none
```

* Un lien nul donne `none`, sinon `some` enveloppant la cible. On accède par
  `$h(sections.some.0.nom)`. Le `.some` est **voulu** : il force à traiter l'absence
  (garde-fou). Il ne concerne que les champs réellement optionnels.
* Les liens obligatoires restent transparents, sans enveloppe.
* Un cas où 0 est un offset valide (`sh_name` d'ELF : index 0 = chaîne vide) se traite avec
  un défaut de champ (`= ""`), pas avec `opt`.

À faire dans le noyau, en deux étapes :
1. **Variante sans valeur** dans une somme (aujourd'hui refusée pour la sélection par champ).
2. **Sélecteur-expression** : `S($champ)` aujourd'hui, `S(expr)` (ex. `$shoff != 0`) comme
   le `{expr TEXTE}` des tailles.

## 5. Comportements par défaut (proposés, à confirmer)

* Le mode **transparent** est le défaut : la valeur est la cible.
* Relire puis réécrire un fichier peut réorganiser les zones (conséquence du mode transparent).
* Un **cycle** en mode transparent est une erreur nette ; avec un `-linkdepth` fini, il n'y en a plus.
* Une cible placée avant l'origine est une erreur avec un entier non signé.
* Un élément de liste est sa propre structure englobante.
* `//` est le début des données décodées, déplacé par `-offset`.
* `type binary {sizeof?}` d'un type contenant un lien donne la taille en place (celle de l'entier),
  pas celle de la cible.

## 6. Hors sujet, tranché

* **Pas d'`union` C.** Une union superpose les mêmes octets sans balise ; on garde les
  garde-fous. Voir le chapitre « Somme et `union` du C » du manuel. Pour `Tcl_ObjInternalRep`,
  la variante est choisie à l'appel : `Tcl_ObjAs(twoPtrValue)` (`exemples/tcl_obj.tcl`).
* **Condition statique** dans un corps générique (`$cas ? {$T}some : NULL`, `$bits == 32 ?
  {u32le} : {u64le}`) : utile pour paramétrer une disposition (ELF 32/64), mais **distincte** de
  la présence dynamique ci-dessus. À faire quand un format l'exigera.
* Les paramètres nommés des types génériques ne sont pas nécessaires ici.

## 7. Plan proposé

1. Variante sans valeur dans les sommes ; sélecteur-expression.
2. `*(//,REPR).T` et `*(./,REPR).T` en lecture, puis en écriture automatique (cibles à la suite,
   alignées) ; `null=` ; `opt`.
3. `..`, `.` (point courant), puis `-linkdepth` (liens de frontière annotés).
4. `memory://` avec `-space`.
5. Alias dans `binaliases.tcl` : `ptr32(T)`, `ptr64(T)`, `rel32(T)`.

## 7bis. État de l'implémentation

Faits : `//`, `file://`, `./`, `../`, `.` ; `null`/`null=V` ; `opt` ; cibles lues après tous les champs
du produit, écrites à la fin (alignées) avec correction de l'adresse ; cycles, cibles avant l'origine
refusés ; alias génériques (`ptr32`, `ptr64`, `rel32`, `sub32`) ; tests t36 à t39.
Faits aussi : `-linkdepth` (decode, read), `type binary deref`, types récursifs par `{linkref NOM}`,
adresse de frontière = entier annoté `&*(...).T` (position absolue de la cible), test t40.
Non faits : référentiels nommés `//nom/`, `memory://` et `-space`, `-dedupe`, types récursifs
(listes chaînées) dans la représentation binaire, écriture d'un lien nul vers une cible non `opt`.

## 7ter. Profondeur des liens : `-linkdepth` (discussion du 16 octobre)

*Transparent* et *opaque* qualifient un milieu ; ici le milieu (fichier, mémoire) ne change pas, c'est
la sonde qui change. On remplace donc l'option de type `opaque` par une **option d'appel** :

    type binary decode ?-linkdepth N? TYPE DATA        ;# N = 0, 1, 2, ... ou Inf (défaut)

* `0` : aucun lien suivi (surface) ; `1` : un seul niveau ; `Inf` : jusqu'au bout (l'ancien transparent).
* Un lien situé à la frontière reste un **entier annoté** par le type lien (origine, représentation,
  nul, type cible). Copie, `lindex`, `string cat` gardent l'annotation ; tout calcul la perd
  (vérifié) : on peut comparer (`== 0`) mais pas déréférencer le résultat.
* Déréférencer un lien de frontière = décoder sa cible avec `-linkdepth 1` : pas de commande à part.
* Les types récursifs (listes chaînées) deviennent décodables, bornés par la profondeur ; les cycles
  ne sont une erreur qu'avec `Inf`.
* L'écriture renvoie un lien de frontière tel quel : on peut modifier sur place sans réorganiser.
* Référentiel nommé : `//` seul = les données de l'appel ; `//nom/` (aussi `file://nom/`,
  `memory://nom/`) = un référentiel enregistré, ce qui rend la valeur annotée autosuffisante.
* Réserve : une profondeur propre à un champ (lien « parent » à ne jamais suivre) serait une option
  du lien ; non ajoutée tant que le besoin ne se présente pas. `sizeof` d'un type récursif reste
  sans réponse (la taille en place est celle des entiers).

## 8. Ouvert

* Où écrire une cible : tout de suite après le champ qui la cite, ou à la fin de la structure
  englobante, et l'ordre pour des liens imbriqués.
* La commande d'enregistrement des référentiels `//nom/` et leur durée de vie (que devient un lien
  dont le référentiel a disparu ?). Le nom exact de l'option : `-linkdepth` (retenu).
* La notation du point courant `.` côte à côte avec le `.` de composition (lisibilité).

## 7quater. Référentiels nommés / Named spaces

Produit `space {name, data, base, segments}` ; registre `::fb::spaces`, alimenté par `type binary space file|memory`. Analyse C : `ParseOrigin` normalise `file://nom/`→`//nom/`. Lecture : `linkRead` calcule la position via `spaceTarget` ; `linkFollow` échange temporairement l'état de lecture (données, base) puis le restaure ; la clé de cycle est `nom@position`. Écriture : adresse de frontière via `spaceAddress` ; cible réelle = erreur. Tests : t41.
