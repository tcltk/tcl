# tcllib math::geometry, reformulé, et son cœur en C (v16q)

`geometry.tcl` reprend **tout** le fichier `geometry.tcl` de tcllib (math::geometry 1.4.2, 52
procédures) avec les procédures typées (v16p/v16q) et les corps en expressions (TIP 759). Mêmes
noms, mêmes arguments (des listes nues, comme les appelants de tcllib), mêmes résultats.
`geometry_circle.tcl` et `geometry_ext.tcl` sont ceux de tcllib, **inchangés** : ils appellent les
procédures reformulées.

`geom.c` (avec `geometry_c.tcl`) fait passer en C les commandes lourdes, à la manière de
l'exemple `matrices` : les structures C viennent des types Tcl (`type {c!}export point line` →
`geom_types.h`), et les commandes C prennent la place des procédures.

- `run_tests.tcl ?-c? TCLLIB` : la suite de tests de tcllib (`geometry.test`, 210 tests) sur la
  reformulation, ou avec `-c` sur la variante C (copie temporaire, rien n'est modifié dans
  tcllib). **210/210** dans les deux cas, résultats identiques au caractère près.
- `bench.tcl ?TCLLIB/modules/math? ?geom.so?` : les trois versions dans le même tclsh, mêmes
  appels, résultats comparés, temps par appel.

## L'écriture

```tcl
type def point    {{double}x; {double}y}
type def line     {{double}x1; {double}y1; {double}x2; {double}y2}
type def polyline list.double
type alias vector point;  type alias segment line;  type alias polygon polyline

proc ::math::geometry::distance {{point}pa; {point}pb} {( hypot($pb(x)-$pa(x), $pb(y)-$pa(y)) )}

proc ::math::geometry::findLineIntersection {{line}l1; {line}l2} {(
    d  = ($l2(y2) - $l2(y1)) * ($l1(x2) - $l1(x1)) - ($l2(x2) - $l2(x1)) * ($l1(y2) - $l1(y1));
    na = ($l2(x2) - $l2(x1)) * ($l1(y1) - $l2(y1)) - ($l2(y2) - $l2(y1)) * ($l1(x1) - $l2(x1));
    $d == 0 ? ($na == 0 ? "coincident" : "none")
            : ($l1(x1) + $na * ($l1(x2) - $l1(x1)) / $d, $l1(y1) + $na * ($l1(y2) - $l1(y1)) / $d)
)}
```

au lieu de

```tcl
proc ::math::geometry::findLineIntersection {line1 line2} {
    set l1x1 [lindex $line1 0]
    set l1y1 [lindex $line1 1]
    ...                                    ;# 8 lindex
    set d [expr {($l2y2 - $l2y1) * ($l1x2 - $l1x1) - ($l2x2 - $l2x1) * ($l1y2 - $l1y1)}]
    set na [expr {...}]
    if {$d == 0} { if {$na == 0} { return "coincident" } else { return "none" } }
    set r [list [expr {...}] [expr {...}]]
    return $r
}
```

- **Les signatures disent ce qu'est chaque argument** (`{point}P; {segment}seg`), et le
  vérifient : `distance {0 0} {a 4}` → `argument "pb": expected floating-point number but got "a"`
  au lieu d'une erreur de `expr` au milieu du calcul.
- **Les champs ont des noms** (`$l1(x2)`, `(Ax, Ay, Bx, By) = $line`) : plus de `lindex $line 2`.
- **Les corps sont des expressions** quand le calcul est une formule : affectations, tuples,
  `?:` à la place des `if`/`return` imbriqués, boucles `foreach … {( … )}`.
- Les procédures à contrôle (retours anticipés, boucles imbriquées : `lineSegmentsIntersect`,
  `findLineSegmentIntersection`, `polylinesBoundingIntersect`, `rectangleInsidePolygon`) gardent
  un corps Tcl, avec la signature typée.
- Les arithmétiques sont recopiées à l'identique (même ordre des opérations), pour des résultats
  identiques au bit près.

## Longueur

| | tcllib | reformulé |
|---|---|---|
| lignes du fichier | 1 570 | 392 |
| lignes de code (sans commentaires ni lignes vides) | 660 (633 sans le premier `pointInsidePolygon`, remplacé plus bas et jamais utilisé) | 289 |
| caractères de code | 17 838 | 11 891 (−33 %) |
| procédures | 53 | 52 |

Les lignes reformulées sont plus longues (41 caractères en moyenne contre 27) : le gain réel est
mieux mesuré en caractères, un tiers. Les commentaires de tcllib (769 lignes, la moitié du fichier :
en-têtes « Arguments / Results / Examples ») sont en grande partie rendus inutiles par les
signatures ; il en reste une ligne par procédure.

## Le cœur en C : `geom.c`

```
type def point {{double}x; {double}y}            typedef struct { double x; double y; } point;
type def line  {{double}x1; {double}y1;          typedef struct { double x1; double y1;
                {double}x2; {double}y2}                           double x2; double y2; } line;
```

(`geom_types.h`, produit par `type {c!}export point line`, avec ses `_Static_assert` de tailles et
de positions ; `geometry_c.tcl` vérifie au chargement que le C voit les mêmes tailles.)

- **Du C pur** : `geom_distance`, `geom_area_polygon`, `geom_length_polyline`,
  `geom_point_inside_polygon_alt`, `geom_distance_to_segment`, `geom_distance_to_polyline`,
  `geom_segments_intersect`, `geom_polylines_bounding_intersect`, `geom_rotate`, `geom_reflect`
  travaillent sur des `point`, des `line` et des tableaux `const point *` ; aucun appel à Tcl, on
  peut les appeler depuis un autre programme C (symboles exportés de `geom.so` / `geom.dll`).
- **Les commandes Tcl** (`::math::geometry::c::*`, 15) lisent leurs arguments et appellent ces
  fonctions. Un polygone **en cases** (une liste `{-compact}` de `double`) est lu **en place** par
  `Tcl_AnnotatedGetCells` : le tableau de doubles de Tcl est directement le `const point *` du C,
  sans copie. Une liste ordinaire est lue par l'API des listes ; une valeur typée (un `point` est
  un seul élément de liste) par `Tcl_AnnotatedGetParts`.
- **`geometry_c.tcl`** charge `geometry.tcl`, puis **remplace** les 15 procédures par les
  commandes C (`rename`) : un appel à `::math::geometry::areaPolygon` va directement à la fonction
  C. Les autres procédures (et `geometry_circle.tcl`, `geometry_ext.tcl`) les appellent sans le
  savoir : `rectangleInsidePolygon`, restée en Tcl, passe de 198 à 20 µs.
- **Résultats identiques à tcllib**, y compris là où tcllib calcule en entiers : avec des entrées
  entières, `pointInsidePolygon` fait une division entière, `translate` une somme entière,
  `findLineIntersection` rend `5 5` et non `5.0 5.0`. Le C reproduit l'arithmétique de `expr`
  (`struct Num` : entier op entier reste entier, division arrondie vers −∞) ; `bbox` et
  `findClosestPointOnPolyline` rendent les nombres d'origine. Compilé avec `-ffp-contract=off`
  (pas de multiplication-addition fusionnée : mêmes arrondis que `expr`).

Compiler (Linux ; MinGW64 : `-o geom.dll`, `-I<src>/win`) :

    gcc -shared -fPIC -O2 -ffp-contract=off -DUSE_TCL_STUBS -I<src>/generic -I<build> \
        -I<src>/unix -I../.. -o geom.so geom.c <build>/libtclstub.a

Régénérer l'en-tête après un changement des types :

    tclsh <<< 'type def point {{double}x; {double}y}
               type def line {{double}x1; {double}y1; {double}x2; {double}y2}
               puts -nonewline [type {c!}export point line]' > geom_types.h

## Vitesse

`bench.tcl`, µs par appel (meilleur de 10 passes alternées, appel compilé). Avec les listes nues
de tcllib : **tcllib**, **Tcl typé** (`geometry.tcl`), **C** (`geometry_c.tcl`). Avec des
valeurs déjà typées par l'appelant (`set {point}A 0 0`) : **Tcl typé\***, et **C\*** dont les
polygones sont en cases (`type def {-compact}polygon_c list.double`). Le rapport à tcllib est
entre parenthèses.

| | tcllib | Tcl typé | C | Tcl typé\* | C\* |
|---|---:|---:|---:|---:|---:|
| `distance {0 0} {3 4}` | 0,51 | 1,43 (2,8) | **0,15 (0,30)** | 1,05 (2,1) | 0,88 (1,7) |
| `findLineIntersection` | 1,06 | 2,32 (2,2) | **0,28 (0,26)** | 2,17 (2,1) | 1,09 (1,0) |
| `lineSegmentsIntersect` | 10,9 | 23,4 (2,2) | **0,21 (0,02)** | 23,4 (2,2) | 0,96 (0,09) |
| `areaPolygon` (carré) | 1,20 | 2,98 (2,5) | **0,23 (0,19)** | 2,32 (1,9) | 0,33 (0,27) |
| `rectangleInsidePolygon` (carré) | 198 | 380 (1,9) | **20 (0,10)** | | |
| 1 000 sommets : `areaPolygon` | 193 | 435 (2,3) | 7,0 (0,04) | 309 (1,6) | **0,93 (0,005)** |
| `lengthOfPolyline` | 266 | 516 (1,9) | 12,1 (0,05) | 404 (1,5) | **6,0 (0,02)** |
| `bbox` | 173 | 440 (2,6) | 11,3 (0,07) | 249 (1,4) | **6,0 (0,03)** |
| `pointInsidePolygon` | 275 | 435 (1,6) | 25,6 (0,09) | 286 (1,0) | **4,9 (0,02)** |
| `calculateDistanceToPolyline` | 6 934 | 4 124 (0,6) | 20,9 (0,003) | 3 911 (0,6) | **15,6 (0,002)** |
| `findClosestPointOnPolyline` | 2 665 | 5 054 (1,9) | 14,6 (0,005) | 5 017 (1,9) | **8,5 (0,003)** |
| `rotate 30` | 384 | 841 (2,2) | 101 (0,26) | 744 (1,9) | **90 (0,23)** |
| 2 × 200 sommets : `polylinesIntersect` | 4 551 | 8 947 (2,0) | **31 (0,007)** | 8 402 (1,8) | 33 (0,007) |

(Tableau complet : `tclsh bench.tcl`.)

- **Le Tcl typé** est environ deux fois plus lent que tcllib (causes ci-dessous).
- **Le C** va de 4 fois (`rotate`) à 300 fois plus vite que tcllib sur les polygones, de 3 à
  50 fois sur les points et les segments. Ce qui reste est la lecture des arguments : 2 000 `Tcl_GetDoubleFromObj` pour un
  polygone de 1 000 sommets en liste (7 µs pour `areaPolygon`) ; **en cases, plus rien** :
  0,93 µs, 200 fois moins que tcllib et 7 fois moins que la même liste ordinaire.
- **La fabrication du résultat** domine `rotate` et `translate` (2 000 `Tcl_Obj` doubles à créer
  pour une liste ordinaire), pas le calcul. Rendre une liste en cases
  (`Tcl_AnnotatedCreateCells`) l'éviterait, mais ce ne serait plus le résultat de tcllib (des
  doubles normalisés : `1.0` au lieu de `1`).
- **C\* sur des points typés** : `distance $A $B` (0,88 µs) est plus lent qu'avec des listes
  (0,15) : un produit typé est lu par la cale `Tcl_AnnotatedGetParts` (`tclAnnotatedApi.h`), qui
  passe par l'interpréteur. La vraie fonction de stub, prévue, lira les parties en place.

### D'où vient le coût (mesures séparées)

| | classique | typé |
|---|---:|---:|
| 5 arguments scalaires (`{double}a; …`), corps vide | 0,25 µs | 1,24 µs |
| liaison d'un `{list.double}` de 2 000 nombres | 0,5 µs | 17 µs |
| boucle `foreach {a b}` sur ces 2 000 nombres, une multiplication par tour | 101 µs | 358 µs |
| `intervalsOverlap` : corps Tcl / corps expression avec échanges en tuple | 0,38 / 0,78 µs | 1,13 / 2,83 µs |

1. **La vérification à l'entrée** : ~0,2 µs par argument scalaire, ~0,3 à 0,5 µs par point fait
   d'une liste, ~8 ns par nombre d'une liste. tcllib ne vérifie rien ; la même sûreté écrite à la
   main coûte plus cher : `string is double -strict` sur les 2 000 nombres prend 91 µs, contre
   17 µs pour la liaison typée (et `exemples/bench_procs.tcl` : à contrôles égaux, une procédure
   typée va aussi vite).
2. **Les boucles sur une liste typée** : les éléments sont rendus typés (enveloppés) avant la
   boucle, puis l'arithmétique déballe chaque élément ; la liste étant une copie neuve à chaque
   appel (liste nue de l'appelant), l'enveloppement est refait à chaque appel. Avec des arguments
   typés, il n'est fait qu'une fois : d'où la colonne de droite (`pointInsidePolygon` 1,1×).
3. **Les affectations à des locales typées** dans une expression (`(y1, y2) = $y1 > $y2 ? … : …`
   sur des `{double}`) : chaque affectation revérifie et réannote. `rectanglesOverlap` (4 points,
   2 appels à `intervalsOverlap`) cumule les trois.

## Ce que l'exercice apprend

- **« Script + compilé » fonctionne avec les mêmes types** : les types Tcl qui vérifient les
  arguments en Tcl donnent aussi les `struct` du C (`type {c!}export`), et une valeur en cases
  passe de l'un à l'autre sans conversion. Le C remplace une procédure sans que ses appelants (ni
  les tests de tcllib) le voient.

- **La compatibilité est complète** : 210 tests de tcllib passent, et le code de tcllib qui
  n'a pas été touché (`geometry_circle.tcl`, `geometry_ext.tcl`) appelle la reformulation avec ses
  listes nues sans rien voir. C'est l'intérêt des signatures typées : l'API ne change pas.
- **Les produits sont atomiques** (règle B) : une valeur `point` est un seul élément de liste.
  Rendre un `point` à un appelant de tcllib qui fait `lassign $P x y` casserait l'API. D'où le
  **dépliage** (v16r) : les procédures qui rendent un point déclarent `proc {{*}point}nom` — le
  résultat est vérifié comme un `point`, puis rendu en liste `{x y}`. De même `{*}line` (`rect`),
  `{*}list(2).point` (`nwse`), `{*}intersection` (`findLineIntersection`, une somme
  `{{point}at, "coincident", "none"}` : le point déplié, ou le mot). La vérification au retour
  coûte 0,3 à 0,9 µs par appel (`p 1 2` : 0,99 → 1,46 µs).
- **Les noms de types n'ont pas d'espace de noms** : `point`, `line`, `polyline` sont globaux à
  l'interpréteur ; un paquet qui les définit les impose à son hôte (ou les nomme `geometry_point`).
  À traiter avant d'utiliser les types dans une bibliothèque.
- **Les priorités d'optimisation** (prototype) : la liaison des arguments scalaires et des produits
  faits de listes ; l'enveloppement des éléments d'une `list.double` (inutile pour une boucle qui
  ne fait que de l'arithmétique) ; l'affectation à une locale `{double}` d'une valeur déjà
  vérifiée.
