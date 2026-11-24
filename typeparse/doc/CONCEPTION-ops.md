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

## La suite

* Plus d'opérations : écriture d'un élément ou d'un champ, parcours
  (`foreach`), somme, remplissage d'une tranche, lecture sans objet pour
  `expr` (`indexDouble` existe, pas encore branché).
* Le coût restant d'une lecture typée : l'enveloppe (deux objets). C'est là
  que l'objet abstrait (valeur typée sans Tcl_Obj de contenu) servira.
* `{double}[expr …]` (1,2 µs) : la pile d'annotations faite de listes Tcl.
* Une interface complète (stubs, plus d'opérations) dans une TIP séparée.
