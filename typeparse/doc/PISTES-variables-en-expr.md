# Pistes : les variables déclarées, sans `$`, dans `expr` (22 novembre 2026)

Notes de discussion, nées d'une réponse sur la liste Tcl (l'exemple du
quaternion, avec la TIP 759 et la TIP 764). **Rien n'est décidé.** Essais faits
sur le tclsh de la branche `tip-759h-tcl91-user-op` (9.1.1) et sur celui de
typeparse (9.0.5, v17k).

## 1. Le but

Écrire une expression comme une formule :

```tcl
type def quat {{double}w; {double}x; {double}y; {double}z}

proc {quat}tcl::mathop::qmul {{quat}a; {quat}b} {(
    a(w)*b(w) - a(x)*b(x) - a(y)*b(y) - a(z)*b(z),
    a(w)*b(x) + a(x)*b(w) + a(y)*b(z) - a(z)*b(y),
    a(w)*b(y) - a(x)*b(z) + a(y)*b(w) + a(z)*b(x),
    a(w)*b(z) + a(x)*b(y) - a(y)*b(x) + a(z)*b(w)
)}

set k [( i \qmul j )]
```

La déclaration prend un sens visible : on ne déclare plus seulement pour
vérifier, on déclare pour écrire comme dans un manuel. Pour l'enseignement,
c'est la différence entre une formule et un script.

## 2. Ce qui marche déjà, ce qui manque

| | aujourd'hui |
|---|---|
| un corps qui n'est qu'un tuple, sans couche de parenthèses de plus : `{( $a + $b, $a - $b )}` | **marche** (759) |
| un tuple après des instructions : `{( s = …; d = …; $s, $d )}` | refusé : il faut `(($s, $d))` |
| un nom typé dans un espace de noms : `proc {quat}tcl::mathop::qmul …` | **marche** (764) |
| le résultat (une liste de 4 nombres) converti en `quat` | **marche** (764) |
| `$a(w)` lu comme nombre dans `expr` | **marche** (764, `loadFieldNum`) |
| `a + b`, `a(w)` sans `$` | refusé : « invalid bareword "a" » |
| 759 et 764 dans le même Tcl | **à faire** : 759 est sur 9.1.1, typeparse sur 9.0.5 |

La forme sans `$` est aujourd'hui une erreur de syntaxe. On peut donc la
donner sans changer le sens d'aucun script valide, comme `{x}mot` pour la 764.

## 3. Deux registres

* **Les commandes** (TIP 759) : `ParseLexeme` reconnaît un nom de commande nu,
  suivi d'une espace, dans une **liste fixe** (`concat`, `dict`, `for`,
  `foreach`, `if`, `lindex`, `lrange`…) et produit `TCL_TOKEN_CMD_IN_EXPR`
  (512). Le registre est global, statique et lexical : l'analyseur ne consulte
  rien.
* **Les variables** (proposé) : un nom nu désigne une variable s'il est
  **déclaré dans la portée** de l'expression. L'analyseur produit
  `TCL_TOKEN_VAR_IN_EXPR` (4096 : 512, 1024 et 2048 sont pris par
  `CMD_IN_EXPR`, `INDEX` et `ANNOT`), qui se compile comme `$a`, et `a(w)`
  comme une lecture de champ. Ce registre change avec chaque corps. Il est
  connu à la **compilation**, pas à l'analyse seule.

## 4. La procédure sait-elle ce que déclare sa signature ?

Oui :

1. La signature est analysée à la création (`TclCreateTypedProc`, point
   d'entrée unique de proc, apply et TclOO) et rangée sur la procédure
   (`typedSig`).
2. Le corps est compilé au premier appel (`TclProcCompileProc`) :
   l'environnement de compilation pointe la procédure (`envPtr->procPtr`), et
   les arguments existent déjà en variables locales compilées
   (`procPtr->firstLocalPtr`), avec leur contrat.
3. En compilant une expression du corps, le compilateur peut donc répondre :
   « `a` est-il déclaré ici, de quel type ? ».

**Le chaînon manquant** : `ParseExpr` reçoit l'interpréteur mais pas
l'environnement de compilation ; `ParseLexeme` ne reçoit ni l'un ni l'autre.
Il faut un pointeur de **portée courante** (dans `Interp`, ou passé en
paramètre), posé par `TclCompileExpr` et `TclCompileScript` pendant la
compilation et lu par l'analyseur sur un nom nu.

## 5. Les portées

| déclaration | portée |
|---|---|
| signature (`proc`, `apply`, méthode) | tout le corps |
| `x is T = v` (expr), `set {T}x …` littéral | de ce point à la fin du corps, y compris dans les corps compilés sur place (`if`, `foreach`, `while`…) ; pas avant, comme en C |
| ce qui n'est pas compilé avec le corps : rappels, `uplevel`, `eval $s`, `after`, expression fabriquée à l'exécution | **hors de portée** : le `$` reste obligatoire |
| le niveau namespace (globales, console, où chaque ligne est compilée à part) | **question ouverte** (§ 7) |

Le message d'une expression hors de portée peut le dire : « x est déclarée
dans la procédure, mais cette expression n'est pas compilée avec elle :
écrivez $x ».

## 6. Deux pièges

* **Variable ou fonction.** `a(w)` s'écrit aujourd'hui comme l'appel de
  `tcl::mathfunc::a`. Proposé : la variable déclarée l'emporte, avec un
  avertissement si elle masque une fonction existante (`max`, `sin`…), et une
  erreur à la définition de la procédure plutôt qu'un sens changé en silence.
* **Le cache.** Une expression qui n'est pas compilée sur place met son
  bytecode en cache sur l'objet littéral, et un littéral peut être partagé entre
  procédures. La validité du cache doit compter la portée. Tcl le fait déjà
  pour les corps : un corps partagé est recompilé quand `codePtr->procPtr`
  n'est pas la procédure qui l'appelle.

## 7. Questions ouvertes

* **Une procédure classique** (`proc f {a b}`) déclare-t-elle `a` et `b` ?
  Proposé : oui. Être déclaré, c'est être nommé dans une signature ; le type
  vient en plus. La 759 seule y gagnerait `{( a + b )}`.
* **Le niveau namespace.** Un registre par namespace, persistant comme les
  commandes, rendrait `x` utilisable sans `$` dans la console après `x is
  double = 1`. Il faut alors invalider le bytecode quand le registre change,
  comme Tcl le fait déjà pour les commandes (époques de compilation).
* **Une variable déclarée puis détruite** (`unset`) : la lecture échoue comme
  `$x` (« can't read »). Rien de neuf, puisque le jeton se lit comme `$x`.
* **L'écriture.** `a(w) = 0` sans `$` existe déjà (affectation de la 759) ;
  seule la lecture change.

## 8. Le bénéfice en plus : la vitesse

Le type étant connu à la compilation, `a(w)` peut devenir un accès direct au
champ (décalage connu, comme `loadFieldNum` en forme compacte), et `a(q)` (un
champ inexistant) une erreur de compilation. Les types servent alors la
lisibilité et la vitesse, sans assembleur public. C'est la réponse à
l'argument de performance de la liste.
