# Les sommes : premier diagnostic (v16v)

*6 novembre 2026. Les sondes sont dans `doc/sommes/sondes.tcl` ; leur sortie est dans
`sondes.out`. Chaque constat ci-dessous renvoie à une section de cette sortie.*

## 1. En une phrase

**La variante d'une somme vit dans l'annotation (T), pas dans la valeur (V).**

Quand la variante peut se recalculer à partir de la chaîne, cela ne gêne pas. Quand elle ne le
peut pas, c'est une information que la valeur Tcl ne porte pas. Tout ce qui fait passer une
valeur par son contenu nu la perd alors : une chaîne, un fichier, une copie, une voie rapide qui
déballe, le côté C. Les étiquettes `{v:}` servent à transporter cette information à côté de la
valeur, et la plupart des fragilités viennent de ce transport.

## 2. Ce qui marche

- **Les énumérations** (`{"on", "off"}`) et **les sommes disjointes** : la variante se lit dans
  la valeur.
- **La construction** :
  - par étiquette : `set {K}k {b:}5` ;
  - par le type de la variante : `{K.b}5` ;
  - par les champs d'une variante produit : `set {forme}f(rect.(w,h)) 2.0 3.0`.
- **La variante gardée** dans une liste typée : `foreach`, `lmap`, `lsort` et `$L(i)` la rendent.
- **Les conversions entre variantes** (`type convert temp.celsius temp.fahrenheit …`).
- **Les options exclusives** des procédures typées (v16q).
- **`type is {disjoint?}S`**, qui dit si une somme peut se déduire.

## 3. Les constats

| | constat | exemple (`sondes.out`) |
|---|---|---|
| S1 | **La variante ne passe pas par la chaîne.** Selon la somme, on a une erreur, ou un changement de sens sans erreur. | `K.b` vu à travers une chaîne neuve : *names no variant of the strict sum K*. `S.s "5"` revient en `S.i`. |
| S2 | **Une valeur typée invalide.** Le retour d'une procédure accepte un produit dont le champ somme a perdu sa variante. L'erreur n'apparaît qu'à la lecture du champ. | `proc {P}h {} {… return [list $k 3]}` : `type get [h]` = `P`, puis `$r(k)` donne une erreur. |
| S3 | **Une étiquette relit la valeur par sa chaîne.** `{k:}` posée sur une valeur déjà typée `K.b` jette sa variante. Placée dans une somme englobante, la variante intérieure se perd aussi. | `set {E}e {k:}[fk {b:}5]` : erreur. Sans étiquette, `$e(k)` est de type `K`, pas `K.b`. |
| S4 | **La durée de vie d'une étiquette dépend de la compilation.** `return {a:}1` marche dans un corps compilé, mais échoue sous `eval`, sous `uplevel`, et dans un `switch -matchvar`. | `r1` : `K.a` ; `r2`, `r3`, `r4` : erreur. |
| S5 | **Peu de commandes reçoivent une étiquette.** Seuls `set {T}`, les arguments et le `return` d'une procédure typée la prennent. `lappend`, `linsert` et `dict set` la refusent, `list` la perd sans rien dire. Une liste `list.K` d'une somme stricte ne s'allonge donc qu'avec des valeurs déjà typées. | `lappend L {b:}3` : erreur. `[list {b:}5]` : élément sans type. |
| S6 | **Une somme ne se lit pas comme sa variante.** Il n'y a pas de projection vers le type de la variante active : un littéral n'est pas une `string`, un `S.i` n'est pas un `int`. On ne peut pas passer une somme à une procédure qui attend `{string}` ou `{int}`. Paradoxe : `expr` et `string length` l'acceptent. | `fs $l` : *unknown way to cast the value from lit to string*. |
| S7 | **Le type d'une variante n'est qu'à moitié un type.** `{forme.rect}` construit la valeur et `type get` le rend, mais le chemin `f(w)` est refusé : il faut écrire `f(rect.w)`. | *forme has no variant "w"* |
| S8 | **L'égalité ignore la variante.** `K.a 5` et `K.b 5` sont égales, font une seule clé de dict et se confondent dans `lsearch`. | `eq` → 1 ; `dict size` → 1 |
| S9 | **La variante est collante.** Après `set t 50.0` sur un `temp.celsius`, la valeur reste en celsius parce que les deux variantes conviennent. Le sens d'une affectation dépend donc de l'histoire de la variable. | `temp.celsius` |
| S10 | **L'introspection est pauvre.** Il n'y a pas de `type variant` ; il faut découper la chaîne de `type get` (`K.b`, `::g::K2.b`). Et `type is` ne teste pas l'appartenance d'une valeur à un type. | *bad subcommand "variant"* |
| S11 | **Grammaire : une variante produit à un seul champ** ne s'écrit pas. Elle sert pourtant : elle nomme la charge (`$f(cercle.r)`) et permet d'ajouter un champ plus tard sans changer les chemins. | `{{double}r}cercle` : *"cercle" can only name a type* |
| S12 | **Quatre régimes** : disjointe, `{-deduce}`, `{-strict}`, et le défaut, qui déduit avec un avertissement sur `stderr`. | `temp` : avertissement à la définition |

Les trois points relevés en typant `cproc` en font partie :
- la somme vers `string`, c'est S6 ;
- `return {v:}[list …]`, c'est S2 et S3 ;
- `switch -matchvar`, c'est S4.

## 4. Le diagnostic : deux sortes de sommes confondues

Le prototype traite toutes les sommes de la même façon. Il y en a pourtant deux sortes.

**Les sommes où la variante est une fonction de la chaîne.** Ce sont les sommes disjointes, les
énumérations, et les sommes `{-deduce}`, où la règle « la plus stricte gagne » est
déterministe. La variante n'y est qu'un **cache** : elle se recalcule toujours à partir de V, et
EIAS tient.

**Les sommes où la variante est une information en plus.** Ce sont les sommes `{-strict}`, et
les sommes qui se recouvrent sans option, comme `temp`. Deux valeurs de même chaîne y ont deux
sens. La variante n'est nulle part dans la valeur Tcl.

| constat | cause |
|---|---|
| S1, S8, S9 | la variante d'une somme recouvrante n'est pas dans la chaîne |
| S2, S3 | du code qui suppose qu'une valeur typée vaut son contenu (déballer, puis revalider) ; c'est vrai pour tout type, sauf une somme recouvrante |
| S4, S5 | les étiquettes transportent cette information à côté de la valeur ; leur durée de vie est forcément locale, et elle est fixée par le bytecode |
| S6, S7, S10, S11, S12 | des manques indépendants, à corriger quel que soit le choix |

Pour S2, la cause précise est `ScalarProductPlain` (`tclTypes.c`, voie rapide v16s). Elle
accepte un élément déjà typé du type du champ et **range son contenu nu**. C'est sans danger
pour un nombre, pas pour une somme stricte. D'autres voies rapides font peut-être la même
supposition. Il faudra les passer en revue.

Les étiquettes sont la réponse la plus complexe du modèle (§ 2.4 et 8.4 des annotations, piste B
de la v16m). Elles servent surtout à une chose : donner à une somme recouvrante une information
que sa valeur n'a pas. Si la valeur portait elle-même sa variante, elles deviendraient un simple
raccourci d'écriture, et S4 et S5 cesseraient d'être des problèmes.

## 5. Pistes, à discuter

**Piste A — une somme recouvrante porte sa variante dans sa valeur.** La règle serait la suivante :
- quand la variante se calcule à partir de la chaîne (somme disjointe, énumération,
  `{-deduce}`), la valeur reste la charge seule, comme aujourd'hui ;
- sinon, la valeur est la liste `variante charge` : `b 5`, `fahrenheit 50.0`,
  `lien {https://… {…}}`.

Ce qu'on y gagne :
- EIAS est rétabli, et S1, S8 et S9 disparaissent ;
- S2 et S3 deviennent bénins, puisque déballer ne perd plus rien ;
- le côté C reçoit la balise, et cproc peut prendre des sommes ;
- la forme texte des sommes est réglée.

Ce qu'on y perd :
- la chaîne n'est plus la charge : `expr {$t + 1}` sur un `temp` ne marche plus, il faut
  `$t(celsius)` ;
- `texte_enrichi` et `cproc` changent de forme. C'est d'ailleurs la forme classique d'un texte
  enrichi en Tcl : `texte {…}`, `lien {url {…}}`.

**Piste B — garder la variante dans l'annotation, et réparer site par site.** Il faudrait
corriger S2, faire accepter les étiquettes par `lappend`, `linsert` et `dict set`, et rendre leur
durée de vie indépendante de la compilation. EIAS resterait rompu pour les sommes strictes, et
leur forme texte (`{T}C`) deviendrait obligatoire pour qu'une telle valeur sorte de
l'interpréteur.

**À faire dans les deux cas :**
- corriger S2, et passer en revue les autres voies rapides qui déballent ;
- S6, la projection : une somme se lit comme le type de sa variante active, et un littéral comme
  une `string`. C'est une règle de cast, pas une déduction ;
- S7 : `f(w)` sur une valeur `forme.rect` ;
- S10 : `type variant $v`, et `type is` avec une valeur ;
- S11 : accepter le produit à un champ, comme variante (`{{double}r}cercle`) et comme groupe ;
- S12 : supprimer le régime par défaut, ou l'aligner sur `{-strict}`, pour qu'il n'en reste que
  trois (disjointe, `{-deduce}`, `{-strict}`).

## 6. Questions

1. **La piste A te paraît-elle acceptable ?** Elle dit qu'une somme recouvrante n'a pas la même
   chaîne que sa charge. C'est le même parti pris que la v16t : la structure vient du type, et la
   chaîne la dit.
2. **Avec la piste A, que deviennent les étiquettes ?** On peut les garder comme raccourci
   (`{b:}5` écrit `b 5`), ou les retirer pour les sommes et ne les garder que pour les champs.
3. **`{-deduce}` doit-il interdire l'étiquette d'une variante moins stricte ?** Aujourd'hui
   `{s:}5` donne un `S.s` qui redevient `S.i` à la première modification ou à la première
   chaîne.
4. **`temp` (deux variantes de même forme)** : faut-il refuser de le définir sans `{-strict}` ?

## 7. La décision et la v16w

Décidé le 6 novembre : toute somme porte sa variante dans sa valeur, et une construction
distincte, l'**union**, couvre les valeurs qui montrent d'elles-mêmes leur forme.

- **Somme `,`** : la valeur est `variante charge`, ou le littéral seul. Toute variante typée a
  un nom, qui est sa balise. Rien n'est deviné : une valeur nue est refusée, avec un message qui
  montre l'écriture (`as {i 5}, or {i:}5`). C'est le « tag » d'une union C dans une `struct`.
- **Union `|`** : la valeur est la charge seule ; les alternatives doivent être disjointes,
  vérifié à la définition (`union: alternatives i and s can take the same value…`).
  L'alternative se calcule, et `type variant` la donne.
- `{-strict}`, `{-deduce}`, le régime par défaut et son avertissement disparaissent.
- `opt(T)` devient `{T}some, "none"` : valeurs `some X` et `none`.

Les sondes rejouées sur la v16w sont dans `sommes/sondes_v16w.tcl` (`sondes_v16w.out`). Les
définitions ont perdu leurs options ; les appels sont les mêmes, et ceux qui donnaient une valeur
nue à une somme sont maintenant refusés, comme prévu.

| | constat | v16w |
|---|---|---|
| S1 | la variante ne passe pas par la chaîne | **réglé** : elle est dans la chaîne (`K.b`, `temp.fahrenheit` relus tels quels) |
| S2 | une valeur typée invalide | **réglé** : le champ garde `b 5` |
| S3 | une étiquette relit la valeur par sa chaîne | **réglé** : `{k:}` sur un `K.b` donne `E.k`, et `$e(k)` est `K.b` |
| S4 | étiquette de retour et compilation | **réglé en v16x** : sous `eval`, `uplevel` ou `switch -matchvar`, le retour étiqueté est rendu dans le type de la procédure avant que l'étiquette expire (patch `retour-etiquette.patch`, test t102) ; `return [list a 1]` marchait déjà partout |
| S5 | commandes qui reçoivent une étiquette | `lappend` et `lset` la reçoivent ; `linsert`, `dict set`, `list` non (elles fabriquent une valeur sans contrat) : on écrit la valeur en clair, `b 3` |
| S6 | projection | **décidé** : toute valeur se lit en `string` (son texte) ; une union se lit comme son alternative ; une somme jamais (elle se lit par `$v(i)`) |
| S7 | type d'une variante à moitié | **réglé** : `$f(w)` lit la charge courante |
| S8 | égalité et clés | **réglé** : `i 5` et `s 5` sont deux chaînes différentes |
| S9 | variante collante | **réglé** : `set t 50.0` est refusé, `set t(celsius) 50.0` dit la variante |
| S10 | introspection | `type variant $v`, `type is {union?}S` ; `type is` sur une valeur reste à faire |
| S11 | produit à un champ | **réglé** : `{{double}r}cercle` |
| S12 | quatre régimes | **réglé** : deux constructions, chacune avec une règle |

Les unions sont venues naturellement dans les exemples : `inter` de la géométrie
(`"aucune" | {point}en`) et `intersection`, `found` de tcllib (`{point}at | "coincident" | "none"`)
rendent une valeur que tcllib attend telle quelle. Les 210 tests de tcllib passent.
