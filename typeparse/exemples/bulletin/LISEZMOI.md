# Le bulletin : un programme en quatre étapes

Un fichier `classe.txt` donne une ligne par élève, « Nom Prénom note ». Le
programme affiche les notes et la moyenne. Le prototype prend la ligne (une
chaîne) pour une liste : le bogue le plus classique de Tcl.

| fichier | étape |
|---|---|
| `etape1_tcl.tcl` | 1. le prototype en Tcl nu : `lrange $l 0 end-1`, `lindex $l end` |
| `etape2_debug.tcl` | 2. la mise au point : trois lignes ajoutées (`###`) |
| `etape3_type.tcl` | 3. le typage : `eleve {{string}nom; {double}note}`, découpe en chaîne |
| `etape4_c.tcl ?CC?` | 4. le C : `lire` et `moyenne` en `cproc`, rien d'autre ne change |

**Étape 1.** Deux défauts, sur les deux dernières lignes de `classe.txt` :
`Garnier "Gigi" Gisèle 13` s'affiche `Garnier Gigi Gisèle` (les guillemets sont
mangés, sans erreur) ; `Bernard {Junior 8` arrête tout sur « unmatched open
brace in list », sans dire quelle ligne du fichier est en cause.

**Étape 2.** Une ligne n'est sûre comme liste que si la relire comme liste ne
la change pas. La note `{@ligne}` posée sur chaque ligne, avec

```tcl
type debug when ligne {[catch {lrange $value 0 end} v] || $v ne $value}
```

s'arrête sur les deux lignes fautives, et seulement sur elles. À l'invite :
`p n` dit combien de lignes précèdent, `lrange $l 0 end` montre ce que Tcl en
fait, `q` arrête.

**Étape 3.** La ligne est découpée en chaîne (au dernier blanc), jamais en
liste ; le résultat est un `eleve`. Le nom reste d'un seul tenant, quels que
soient ses guillemets ou ses accolades ; la note est vérifiée à la lecture : une
note `9,5` est refusée par la signature de `lire`, au lieu d'une erreur de
`expr` plus loin.

**Étape 4.** `lire` et `moyenne` passent en C par `cproc`, avec les mêmes
signatures ; `bulletin` ne change pas. En C, un `eleve` est la struct
`{Tcl_Obj *nom; double note;}` donnée par `type {c!}export`.
