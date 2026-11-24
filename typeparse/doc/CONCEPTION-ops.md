# Opérations spécialisées (v17m, 23 novembre 2026)

## Pourquoi

Le type d'une valeur est connu dès sa définition, mais chaque opération le
redécouvrait : de la valeur à son annotation, au type, au type des éléments,
au genre, puis le traitement. Mesures avant (ns par opération) : `lindex`
d'une list.double 274 (nu 50), `$P($i.x)` sur une liste compacte de points
550, `{double}x` qui reçoit `[expr]` 204 (nu 65).

## Ce qui est fait

* `TclTypeOps` : la table d'opérations d'un type (`index`, `elemField`,
  `indexDouble` pour la version 1). Absente, ou qui rend NULL / 0 : le
  chemin général.
* `TclTypeSpec` : une déclaration (nom, priorité, fonction `match`, table).
  `TclTypesRegisterOps` l'ajoute au registre de l'interpréteur et fait monter
  sa génération ; `TclTypeOpsOf(tp)` refait le choix d'un type quand la
  génération a changé (la plus haute priorité qui reconnaît le type).
* Intégrées : `list-scalar` (listes de nombres simples, nues ou en cases),
  `list-product` (champ F de l'élément I lu dans la case).
* Branchements : `AnnotListIndex` (lindex et vue liste), la lecture
  `P(i.champ)` (texte « chiffres.nom »), et le bytecode : `indexConcat 2`
  suivi de `loadArray` sur une variable typée lit le champ sans former
  l'index (patch `champ-indexe-specialise.patch`).
* Hors table, même idée : `{double}x` / `{int}x` (types simples) qui reçoivent
  un nombre nu de leur genre l'écrivent dans leur valeur si la variable est
  seule à la tenir (`TclTypedVarPrepareWrite`).
* Mode de contrôle : `TCL_TYPE_OPS=off|check`. La suite passe dans les trois
  modes ; `check` ne relève aucun écart.
* Entrée externe simplifiée (TIP) : `Tcl_GetAssocData(interp, "tclTypesOps",
  NULL)` donne un `TclTypesOpsApi` (déclarer, lire un type opaque, `wrap`).
  Exemple : `exemples/ops/produits_ordinaires.c`.

## Mesures après (ns, meilleur de 3, un million de tours)

| opération | off | on |
|---|---:|---:|
| `$P($i.x)`, liste compacte de points | 432 | 186 |
| `lindex` d'une list.double | 117 | 117–150 (bruit) |
| `set x [expr]`, `{double}x` | 204 avant | 94 (nu 60) |

## type map (même jour)

`type map VARS LISTE ?VARS LISTE ...? {(EXPR)}` : l'expression compilée une
fois en un petit programme sur des nombres (`MExpr` : arbre ; `MDProg` :
programme postfixe sur des doubles quand tout est flottant), évaluée en C
pour chaque élément ; un élément que le programme ne sait pas traiter
exactement comme `expr` passe par `expr` (`Tcl_ExprObj`, variables posées
dans le cadre de l'appelant). {x y z} prend trois colonnes (ce que rend
`$P(*.(a,b,c))`). Entrées en cases lues directement ; résultat en cases si
tous les résultats sont des doubles (ou des entiers). Vérifié par
différence avec `TCL_TYPE_OPS=off` (tout par `expr`) : 3 000 expressions
aléatoires × 225 couples de valeurs, entrées ordinaires et en cases, et des
listes en cases de toutes largeurs : aucun écart. `$P(*.a)` et
`$P(*.(a,b,c))` sur une liste en cases de produits copient les colonnes
d'un coup (`TclCompactFieldColumn`) : 1 800 → 2 à 17 ns par élément.

| un million d'éléments, `$x + $y * $z` | ns / élément |
|---|---:|
| `lmap` + `lassign` (lignes) | 144–155 |
| `lmap` sur trois colonnes | 123–126 |
| `type map`, colonnes ordinaires | 39–40 |
| `type map`, colonnes en cases | 24–28 |
| `type map {x y z} $P(*.(a,b,c))` (chemin compris) | 39 |

## La suite

* Plus d'opérations : écriture d'un élément ou d'un champ, parcours
  (`foreach`), somme, remplissage d'une tranche, lecture sans objet pour
  `expr` (`indexDouble` existe, pas encore branché).
* Le coût restant d'une lecture typée : l'enveloppe (deux objets). C'est là
  que l'objet abstrait (valeur typée sans Tcl_Obj de contenu) servira.
* `{double}[expr …]` (1,2 µs) : la pile d'annotations faite de listes Tcl.
* Une interface complète (stubs, plus d'opérations) dans une TIP séparée.
