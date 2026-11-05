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
  sans copie. Une liste ordinaire est lue par l'API des listes ; une valeur typée aussi : depuis la
  v16t, un `point` typé est, pour l'API des listes, la liste de ses champs (en v16s, il fallait
  `Tcl_AnnotatedGetParts`).
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
| `distance {0 0} {3 4}` | 0,62 | 1,16 (1,9) | **0,15 (0,25)** | 1,27 (2,0) | 1,08 (1,7) |
| `findLineIntersection` | 0,99 | 2,40 (2,4) | **0,28 (0,28)** | 2,62 (2,6) | 1,03 (1,0) |
| `lineSegmentsIntersect` | 10,5 | 19,8 (1,9) | **0,22 (0,02)** | 20,0 (1,9) | 0,93 (0,09) |
| `areaPolygon` (carré) | 1,20 | 2,08 (1,7) | **0,23 (0,19)** | 1,94 (1,6) | 0,27 (0,23) |
| `rectangleInsidePolygon` (carré) | 205 | 378 (1,8) | **25 (0,12)** | | |
| 1 000 sommets : `areaPolygon` | 192 | 209 (1,1) | 7,7 (0,04) | 172 (0,9) | **1,5 (0,008)** |
| `lengthOfPolyline` | 262 | 271 (1,0) | 12,7 (0,05) | 260 (1,0) | **11,3 (0,04)** |
| `bbox` | 173 | 142 (0,8) | 11,3 (0,07) | 135 (0,8) | **5,4 (0,03)** |
| `pointInsidePolygon` | 248 | 222 (0,9) | 26,4 (0,11) | 211 (0,9) | **8,2 (0,03)** |
| `calculateDistanceToPolyline` | 5 693 | 3 305 (0,6) | 21,7 (0,004) | 3 143 (0,6) | **13,4 (0,002)** |
| `findClosestPointOnPolyline` | 2 733 | 4 606 (1,7) | 15,3 (0,006) | 4 797 (1,8) | **9,6 (0,004)** |
| `rotate 30` | 406 | 637 (1,6) | 96 (0,24) | 605 (1,5) | **94 (0,23)** |
| 2 × 200 sommets : `polylinesIntersect` | 3 207 | 6 321 (2,0) | **23 (0,007)** | 6 055 (1,9) | 25 (0,008) |

(v16s ; tableau complet : `tclsh bench.tcl`.)

- **Le Tcl typé** (v16s) va à peu près aussi vite que tcllib sur les polygones (0,8 à 1,1 ; deux
  fois plus lent en v16r), et reste 1,6 à 2,4 fois plus lent sur les petites procédures (liaison
  des arguments, vérification du retour).
- **Le C** va de 4 fois (`rotate`) à 300 fois plus vite que tcllib sur les polygones, de 3 à
  50 fois sur les points et les segments. Ce qui reste est la lecture des arguments : 2 000 `Tcl_GetDoubleFromObj` pour un
  polygone de 1 000 sommets en liste (7 µs pour `areaPolygon`) ; **en cases, plus rien** :
  0,93 µs, 200 fois moins que tcllib et 7 fois moins que la même liste ordinaire.
- **La fabrication du résultat** domine `rotate` et `translate` (2 000 `Tcl_Obj` doubles à créer
  pour une liste ordinaire), pas le calcul. Rendre une liste en cases
  (`Tcl_AnnotatedCreateCells`) l'éviterait, mais ce ne serait plus le résultat de tcllib (des
  doubles normalisés : `1.0` au lieu de `1`).
- **C\* sur des points typés** : en v16s, `distance $A $B` (0,88 µs) était plus lent qu'avec
  des listes (0,15) : un produit typé était lu par la cale `Tcl_AnnotatedGetParts`, qui passe par
  l'interpréteur. Depuis la v16t, l'API des listes lit ses champs : 0,41 µs
  (`findLineIntersection` 1,08 → 0,62 ; `lineSegmentsIntersect` 1,02 → 0,53).

### D'où vient le coût (mesures séparées), et ce qu'en a fait la v16s

| | classique | typé v16r | typé v16s |
|---|---:|---:|---:|
| 5 arguments scalaires (`{double}a; …`), corps vide | 0,23 µs | 1,14 µs | 0,68 µs |
| 2 points faits de listes | 0,20 µs | 0,86 µs | 0,56 µs |
| boucle `foreach {a b}` sur 2 000 nombres (`{list.double}`), une multiplication par tour | 97 µs | 272 µs | 111 µs |
| `intervalsOverlap`, corps expression avec échanges en tuple | 0,36 µs | 2,73 µs | 1,92 µs |
| retour `{*}point` | 0,25 µs | 0,95 µs | 0,60 µs |

1. **La liaison des arguments** : les champs deviennent des locales compilées (plus de recherche
   par le nom, plus de table de variables du cadre), le type de chaque champ est résolu une fois,
   la liste `args` n'est plus fabriquée, un produit de scalaires (`{1 2}` pour un point) est
   vérifié champ par champ, sans le validateur général.
2. **Les listes de scalaires** (`list.double`, `list.int`, `list.string`) : leurs éléments
   restent nus dans le contenu ; `foreach`, `lmap`, `lassign`, `lrange` les rendent nus
   (vérifiés à l'entrée), `lindex` et `$L(i)` en font une valeur typée à la demande. Avant, chaque
   élément était enveloppé avant la boucle (2 000 objets créés et libérés à chaque appel avec une
   liste neuve). Et `lrange $L end-1 end` n'enveloppait pas seulement la tranche, mais toute la
   liste (184 µs pour 2 000 nombres).
3. **Les affectations à des locales typées** : une valeur déjà du type de la variable est rangée
   telle quelle, un nombre nu sous contrat `int`/`double` sans copie.
4. **Le retour** : un produit de scalaires rendu en liste est vérifié champ par champ ; `{*}T` le
   rend tel quel.

Ce qui reste, sur les petites procédures, est le prix d'une valeur typée : la créer (une
enveloppe par argument) et la libérer.

## Ce que l'exercice apprend

- **« Script + compilé » fonctionne avec les mêmes types** : les types Tcl qui vérifient les
  arguments en Tcl donnent aussi les `struct` du C (`type {c!}export`), et une valeur en cases
  passe de l'un à l'autre sans conversion. Le C remplace une procédure sans que ses appelants (ni
  les tests de tcllib) le voient.

- **La compatibilité est complète** : 210 tests de tcllib passent, et le code de tcllib qui
  n'a pas été touché (`geometry_circle.tcl`, `geometry_ext.tcl`) appelle la reformulation avec ses
  listes nues sans rien voir. C'est l'intérêt des signatures typées : l'API ne change pas.
- **Les produits sont des conteneurs** (v16t) : une valeur `point` est, pour les commandes de
  liste, la liste de ses deux champs ; un appelant de tcllib qui fait `lassign $P x y` sur un
  `point` rendu par `proc {point}nom` le lit comme la liste `{x y}`. Jusqu'à la v16s, un produit
  était un seul élément de liste (« règle B »), et les procédures publiques devaient rendre
  `{*}point` (dépliage, v16r) ; ce n'est plus nécessaire, et les 18 `{*}` ont été retirés. La
  vérification au retour coûte 0,3 à 0,9 µs par appel (`p 1 2` : 0,99 → 1,46 µs).
- **Les types sont dans l'espace du paquet** (v16u) : `::math::geometry::point`, `line`,
  `polyline`… ; les procédures du paquet les nomment `point`, un programme les nomme
  `math::geometry::point` ou fait `namespace path ::math::geometry` (comme `bench.tcl`). Jusqu'à
  la v16t, ils étaient globaux à l'interpréteur et s'imposaient à l'hôte. `type {c!}export point
  line` donne toujours les structs `point` et `line` (le dernier mot du nom).
- **Les priorités d'optimisation** (prototype) : la liaison des arguments scalaires et des produits
  faits de listes ; l'enveloppement des éléments d'une `list.double` (inutile pour une boucle qui
  ne fait que de l'arithmétique) ; l'affectation à une locale `{double}` d'une valeur déjà
  vérifiée.
