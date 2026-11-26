# Une chaîne, dix variantes : ce que coûtent les types, et ce qu'ils rapportent

Une même chaîne de géométrie sur une polyligne plate `x1 y1 x2 y2 ...` :
translation, rotation, puis aire, longueur et boîte englobante. Elle est écrite de dix
façons, du Tcl nu de tcllib jusqu'à des valeurs typées de bout en bout traitées en C. Les
résultats sont identiques dans les dix variantes (l'aire et la longueur sont comparées à
celles de tcllib).

Chaque variante isole un principe. Lues dans l'ordre, elles en font le tour.

| | 1 000 sommets | 100 000 sommets | par rapport au Tcl nu |
|---|---:|---:|---:|
| 1. Tcl nu (tcllib) | 1 135 µs | 126 ms | ×1 |
| 2. Tcl nu + C | 169 µs | 20 ms | ×6,4 |
| 3. Typé scintillant, Tcl | 2 245 µs | 280 ms | ×0,5 |
| 4. Typé scintillant, vectorisé | 350 µs | 32 ms | ×3,9 |
| 5. Typé scintillant, C | 62 µs | 6,1 ms | ×21 |
| 6. Tout typé, Tcl | 2 216 µs | 267 ms | ×0,5 |
| 7. Tout typé, vectorisé (objets) | 337 µs | 31 ms | ×4,1 |
| 8. Tout typé, vectorisé (cases) | 102 µs | 5,2 ms | **×24** |
| 9. Tout typé, C (cases) | 28 µs | 2,8 ms | **×46** |
| 10. Tout typé, vectorisé + C (cases) | 66 µs | 3,7 ms | ×34 |

Conditions : Linux, gcc -O2, typeparse v17v (v17o : ×20 en 8, ×41 en 9, ×29 en 10), coordonnées `double`. Chaque variante tourne
dans son propre processus ; le temps retenu est le meilleur de trois processus, chacun
répétant la chaîne (200 fois à 1 000 sommets, 7 fois à 100 000). Les listes « ordinaires »
sont faites de doubles déjà convertis (calculés par `expr`) : des données lues en texte
coûteraient davantage aux variantes 1 à 5.

**Scintillant** veut dire : les données arrivent en listes ordinaires, et ce sont les
commandes typées qui les convertissent à leur entrée. **Tout typé** veut dire : la
polyligne est convertie une fois au départ (`type cast`), puis chaque commande reçoit le
résultat de la précédente.

## 1. Tcl nu : la référence

`math::geometry` de tcllib, tel quel : des listes, `foreach`, `lindex`, `expr`. Chaque
nombre est un `Tcl_Obj` (un double déjà converti, ici). Chaque élément de chaque boucle
passe par l'interpréteur de bytecode : chargement, calcul, rangement.

## 2. Tcl nu + C : le coût est dans la représentation

Les mêmes listes, mais l'aire, la longueur, la boîte, la translation et la rotation sont
des commandes C (`geom.c`). Le calcul lui-même devient presque gratuit, et pourtant on ne
gagne que ×6. Le C passe son temps à lire un objet par nombre (`Tcl_GetDoubleFromObj`) et
à en fabriquer un par résultat (`Tcl_NewDoubleObj`), puis Tcl les libère.

**Principe : au-delà d'un certain point, ce n'est plus le calcul qui coûte, c'est la
représentation des données.** C'est la façon classique d'accélérer Tcl, et elle bute ici.

## 3. Typé scintillant, Tcl : un type se paie à la frontière

La reformulation typée de `geometry.tcl` (`{polyline}pl` dans les signatures, corps en
expressions), appelée avec des listes ordinaires. Elle est deux fois plus lente que le Tcl
nu. À chaque appel, la signature vérifie les 200 000 nombres de la polyligne. Les corps,
eux, restent des boucles élément par élément, comme en 1.

**Principe : un type se vérifie là où une valeur entre dans une commande typée. Si les
valeurs franchissent cette frontière à chaque appel, on paie à chaque appel.** C'est ce
qui donne l'impression que « le Tcl typé est plus lent que le Tcl nu ». Écrit ainsi,
c'est vrai.

## 4. Typé scintillant, vectorisé : une commande par opération, pas par élément

Les mêmes signatures, mais des corps vectorisés (`vectorise.tcl`). Une seule commande
traite toute la liste : `type map` évalue une expression sur des colonnes entières, les
tranches à pas (`$pl(1::2)`) et les fenêtres (`$pl((0,1,2,3)::2)`) découpent sans
boucle, et `set out((0,1)::2) $X $Y` réentrelace. L'interpréteur de bytecode n'est plus
traversé qu'une fois par opération, au lieu d'une fois par élément. Et `type map` calcule
par blocs : les valeurs de 256 éléments sont rassemblées, puis chaque opération
s'applique au bloc entier.

Les procédures vectorisées rendent des valeurs typées. Seul le premier appel de la chaîne
convertit donc sa liste ordinaire ; les suivants reçoivent une valeur déjà typée.

**Principe : vectoriser, c'est sortir la boucle de l'interpréteur. Une syntaxe plus riche
dans une seule commande (pas, fenêtres) évite de repasser par le bytecode.**

## 5. Typé scintillant, C : convertir une fois, vers la bonne forme

Les commandes C, derrière des signatures typées en cases (`{polylinec}pl`, où
`polylinec` est la forme compacte de `polyline`). La liste ordinaire de départ est
convertie en cases par la première signature ; ensuite, tout reste en cases. Le C lit en
place et rend des cases (`geomcells.c`). On passe de ×6 (variante 2) à ×21 : la
conversion se paie une fois (environ 3,5 ms pour 200 000 nombres), au lieu d'un objet par
nombre à chaque commande.

**Principe : la conversion n'est pas le problème si elle a lieu une fois, à l'entrée, et
vers une forme dans laquelle les données restent.**

## 6. Tout typé, Tcl : le type doit survivre au résultat

La polyligne est typée une fois au départ, mais les procédures Tcl de la variante 3
rendent des listes ordinaires (`concat {*}[lmap ...]`). Dès le deuxième appel, la valeur a
perdu son type et doit être revérifiée. Le résultat est le même qu'en 3.

**Principe : typer de bout en bout, c'est aussi rendre des valeurs typées. Un type
perdu en route se repaie à la commande suivante, comme un scintillement (*shimmering*)
de représentation.**

## 7. Tout typé, vectorisé, en objets : les vues, mais encore un objet par nombre

Les procédures vectorisées sur une polyligne typée faite d'objets. Les tranches et les
fenêtres sont des *vues* : la liste de base, un début, un pas, un nombre. Elles se créent
en O(1), sans toucher les éléments, et `type map` lit directement dans la base. Ce que la
vue ne supprime pas, c'est l'objet par nombre : translation et rotation fabriquent
encore 200 000 objets, que Tcl libère ensuite. On reste à ×3,5, presque comme en 4 :
seule la première conversion a disparu.

**Principe : une vue évite de copier, mais pas de représenter chaque nombre par un objet.**

## 8. Tout typé, vectorisé, en cases : la bonne représentation

La même chaîne, la même syntaxe, mais la polyligne est en cases (`{-compact}`, la forme
compacte du même type). Plus d'objet par nombre : les vues sont des pas dans un tableau
de doubles, `type map` lit et écrit des cases, et ses blocs se vectorisent. La polyligne
résultat est créée d'un bloc (`type repeat -into polylinec`, sans liste intermédiaire), et
`type map -min {x} $X {($x)}` sur une colonne en cases lit les cases sans les recopier.
**×24, en Tcl, avec des valeurs typées.**

**Principe : la représentation fait l'essentiel du gain. Le même type, déclaré une fois
avec ses formes (`type def polyline list.coord {-compact}polylinec`), choisit la sienne.**

## 9. Tout typé, C, en cases : le plafond

Les mêmes cases, traitées entièrement en C : lues en place, rendues en cases. ×46. C'est
le plafond de cette chaîne. Comparé à la variante 2 (le même C sur des listes
d'objets), il montre que le C double l'apport des cases, mais seulement si ses données
sont déjà en cases.

**Principe : le C rapporte en proportion de la représentation qu'il lit.**

## 10. Tout typé, vectorisé + C : chaque chose à sa place

Les transformations en Tcl vectorisé, les mesures en C, sur les mêmes cases. ×34, entre
8 et 9. À 100 000 sommets, translation et rotation écrites en `type map` ne coûtent
qu'environ 1 ms de plus que leur version C. À 1 000 sommets, les coûts fixes d'une
commande Tcl se voient davantage (66 contre 28 µs).

**Principe : en cases, le Tcl vectorisé est assez proche du C pour qu'on n'écrive en C
que ce qui le mérite, et les deux se passent les mêmes valeurs sans conversion.**

## En résumé

- Le Tcl typé, écrit élément par élément, est plus lent que le Tcl nu (×0,5) : il vérifie
  ses types sans rien en tirer.
- Le typage paie quand il change la façon de calculer (vectorisation) et la façon de
  représenter (cases). Avec les deux, ×24 en Tcl, ×46 avec le C.
- L'ordre des gains : la représentation d'abord, la vectorisation ensuite, le C en
  dernier.

## Lancer

```sh
# les deux extensions de C (Linux) ; <src> = les sources de Tcl, <build> = leur construction
gcc -shared -fPIC -O2 -ffp-contract=off -DUSE_TCL_STUBS -I<src>/generic -I<build> \
    -I<src>/unix -I../.. -o geom.so geom.c <build>/libtclstub.a
gcc -shared -fPIC -O2 -ffp-contract=off -DUSE_TCL_STUBS -I<src>/generic -I<build> \
    -I<src>/unix -I../.. -o geomcells.so geomcells.c <build>/libtclstub.a

tclsh comparaison.tcl ?TCLLIB_MATH_DIR?
```

`TCLLIB_MATH_DIR` est le répertoire du `geometry.tcl` d'origine (`modules/math` des
sources de tcllib) ; par défaut, celui que trouve `package require math::geometry`. Sans
`geom.so` ni `geomcells.so`, les variantes en C sont sautées.

`tclsh comparaison.tcl TCLLIB_MATH_DIR N` mesure la seule variante N et affiche ses deux
temps.
