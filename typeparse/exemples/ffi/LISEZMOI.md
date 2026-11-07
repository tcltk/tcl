# ffi — appeler une fonction de bibliothèque depuis sa signature (prototype)

Une FFI a besoin de trois choses : la forme des données C, le prototype de la
fonction, et l'appel lui-même. TIP 764 fournit déjà les deux premières :

- **les données** : les types binaires (`int(s,32)`, `double(64)`,
  `string(utf-16le,z)`, un produit avec `-align natural` pour une `struct`,
  `list(N).T` pour un tableau) ;
- **le prototype** : la signature d'une procédure typée. Le type de retour est
  `{T}nom` ; un paramètre de sortie est un `{var.T}`, le lien vers la variable
  de l'appelant.

`ffi.tcl` ajoute l'appel, sans libffi : un talon C généré depuis la signature
et compilé par le compilateur de `cproc` (gcc ou tcc), au premier appel, avec
le cache de cproc. C'est la voie de critcl.

```tcl
source ffi.tcl
ffi::fn libm.so.6 {double}hypot {{double}x; {double}y}
hypot 3 4                                   ;# 5.0

type def timespec {{int(s,64)}sec; {c_long}nsec}
ffi::fn libc.so.6 {c_int}timespec_get {{var.timespec}ts; {c_int}base ?= 1}
timespec_get t
puts $t(sec)                                ;# t est un timespec

ffi::callback {c_int}compare {{ref.c_int}a; {ref.c_int}b}
ffi::fn libc.so.6 qsort {{var.list.c_int}base; {size_t}-n ?= [llength $base];
                         {size_t}-size ?= [type binary {sizeof?}c_int];
                         {compare}f}
set L {5 3 9 1 7}
qsort L {{{c_int}a; {c_int}b} {expr {$a - $b}}}
set L                                       ;# 1 3 5 7 9
```

La procédure Tcl de même nom est une procédure typée ordinaire : elle lie les
arguments (défauts, options, défauts calculés, liens), les vérifie et produit le
message d'usage, comme `proc`. Elle encode ce qui passe par adresse, appelle le
talon, puis décode ce qui revient dans les variables. Une valeur fausse est
refusée avant le C (`argument "y": expected floating-point number but got "x"`).
La signature Tcl peut être plus riche que le prototype C : dans `qsort`, `n` et
`size` deviennent des options calculées depuis la liste.

| fichier | |
|---|---|
| `ffi.tcl` | `ffi::fn`, `ffi::callback`, le générateur du talon |
| `libc.tcl` | `hypot`, `strlen`, `getenv`, `timespec_get`, `modf`, `qsort` avec une lambda typée (`libc.out`) |

```
tclsh libc.tcl ?COMPILATEUR?      # gcc par défaut ; tcc, ou un chemin
```

## Correspondance

| signature | C | passage |
|---|---|---|
| `int(s,N)`, `int(u,N)` | `intN_t`, `uintN_t` | valeur |
| `double`, `double(64)` ; `double(32)` | `double` ; `float` | valeur |
| `flag` | `int` | valeur |
| `ptr`, `size_t`, `c_int`, `c_long` | une adresse, `size_t`, `int`, `long` | valeur (types fournis, à la taille de la plate-forme) |
| `string(ENC,z)`, `string` | `const char *` | le texte encodé, NUL final |
| `ref.T` | `const T *` | la valeur encodée (entrée) |
| `list.T`, `list(N).T` | `const T *` | le tableau encodé (entrée) |
| `var.T`, `var.list.T` | `T *` | la variable encodée, passée par adresse, puis décodée dans la variable (entrée et sortie ; une variable absente : des zéros) |
| un type de `ffi::callback` | un pointeur de fonction | une lambda, typée de préférence |

Retour : un nombre, `ptr`, `string(utf-8,z)` (un `char *` ; `NULL` donne la
chaîne vide), ou rien (`void`, sans annotation sur le nom).

**Un rappel** est une lambda appelée par un trampoline C. Un paramètre `ref.T`
lui arrive décodé : le comparateur de `qsort` reçoit deux entiers, pas deux
adresses. Une erreur dans le rappel arrête les appels suivants du rappel ; elle
est rendue après le retour de la fonction C, et les variables de sortie ne sont
pas modifiées.

`long` mesure 32 bits sous Windows et 64 sous Linux et macOS : `c_long` le
prend en compte, `timespec` s'écrit donc une seule fois.

## Le coût

Linux x86-64, gcc, µs par appel :

| | |
|---|---:|
| `hypot 3 4` | 0,72 |
| `strlen abc` (le texte encodé en UTF-8) | 1,35 |
| `timespec_get t` (une struct encodée, puis décodée) | 9,7 |
| `qsort` de 1 000 entiers, rappel Tcl | 40 000, soit 4 par comparaison |

Les nombres, les textes UTF-8 et les tableaux de nombres passent par
`binary format` et `binary scan`. Les structs passent par `type binary`, écrit
en Tcl (`layout/filebinary.tcl`). C'est le prochain chemin à raccourcir : générer
l'encodage des structs dans le talon, comme cproc le fait déjà pour ses
arguments.

## Ce qui manque

- les structs passées ou rendues par valeur (cproc sait les écrire, à reprendre) ;
- les fonctions variadiques (`printf`) ;
- un `char *` rendu qu'il faut libérer, et en général la propriété de la mémoire ;
- `errno` et `GetLastError`, à lire juste après l'appel ;
- un rappel n'est pas réentrant d'un thread à l'autre (son état est une
  variable statique du talon) ;
- `ptr` est un entier sans contrôle : à ne jamais exposer dans un interpréteur
  sûr. Une vraie FFI marquerait ses pointeurs (le type de la cible, le nul).

Testé sous Linux (glibc). Sous Windows, `libc.tcl` prend `ucrtbase.dll`, qui
a toutes ces fonctions ; sous macOS, `libSystem.dylib`. Ces deux variantes
n'ont pas encore été essayées.

## Et cffi ?

L'extension cffi d'Ashok Nadkarni fait tout cela, avec libffi ou dyncall, sans
compilateur. Elle a son propre langage de déclaration : des types, et des
annotations sur les paramètres (`in`, `out`, `inout`, `nullok`, `enum`,
`bitmask`, `onerror`…). Ce prototype montre que les signatures de TIP 764
peuvent servir de langage de déclaration : une FFI comme cffi pourrait les
lire, au lieu d'en définir un.
