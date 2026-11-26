# Le crible d'Ératosthène : Tcl et les autres langages

Un même programme, écrit dans chaque langage avec ses moyens ordinaires.
N = 1 000 000 octets à 1 ; pour chaque i tel que i² < N et s[i] = 1, on
met à 0 s[i², i²+i, …] ; on compte les 1 (78 498 nombres premiers). Trois
étapes chronométrées à part, meilleur de 5, en millisecondes. L'étape
« cribler » commence par une copie du tableau, partout.

    sh comparer.sh /chemin/vers/tclsh     # crible.tcl, puis autres/*

## Résultats (une machine Linux x86-64, v17t ; deux séries)

| Langage, forme | Créer | Cribler | Compter | Total |
|---|---:|---:|---:|---:|
| C (gcc -O2) | 0,0 | 1,3 | 0,2 | **1,5** |
| Python + NumPy, tranches | 0,0 | 1,1–1,5 | 0,3 | **1,4–1,9** |
| **Tcl, octets en cases + tranches** | 0,0 | 2,3–2,8 | 0,1–0,2 | **2,5–3,0** |
| JavaScript (Node 22), `Uint8Array` | 0,4–0,5 | 2,8–3,0 | 1,2 | **4,4–4,7** |
| Python, `bytearray` + tranches | 0,0 | 1,1–1,4 | 3,6–4,6 | **4,7–6,0** |
| JavaScript (Node 22), `Array` | 5,7 | 10 | 2,9 | 19 |
| Tcl, `list.int` + tranches | 2,1–2,3 | 22 | 5,2–5,4 | 30 |
| PHP 8.3, tableau + boucle | 6,3 | 42 | 2,0 | 51 |
| Python, liste + boucle | 1,4 | 44 | 14 | 59 |
| Ruby 3.3, tableau + boucle | 1,2 | 65–90 | 1,1 | 68–93 |
| Tcl nu, liste + `lset` | 0,5 | 120–150 | 4,2 | 125–155 |
| Perl, tableau + boucle | 11–14 | 100–135 | 17 | 130–170 |
| Tcl nu, sans vectorisation (`TCL_TYPE_NOVECLOOP=1`) | 0,4 | 149 | 30 | 180 |

Le Tcl en octets rangés en cases va environ 50 fois plus vite que le Tcl
nu, devant Python `bytearray` et le `Uint8Array` de JavaScript, loin devant
les boucles de Python, PHP, Ruby et Perl, et à moins de deux fois C et
NumPy (l'écart restant est dans les tranches : 2,3 ms contre 1,1–1,3).

## Ce que montre chaque ligne Tcl

**La représentation.** Une liste Tcl ordinaire tient un objet par élément
(8 octets de pointeur, plus l'objet partagé). Une `list.int(u,8)` faite par
`type repeat` range les éléments dans des cases d'un octet, comme un
`bytearray` ou un `Uint8Array` : la liste d'un million d'éléments tient en
un mégaoctet, et le code C la parcourt sans toucher d'objets. La valeur
reste une liste Tcl (`llength`, `lindex`, `foreach`, sa chaîne sont les
mêmes).

**Les tranches à pas.** `set s($j:end:$i) 0` écrit d'un coup tous les
multiples de i, comme `s[i*i::i] = 0` en Python. La valeur est vérifiée et
codée une fois, puis recopiée de case en case, sur place (la variable est
seule à tenir la liste). C'est l'étape « cribler » : 2,3 ms en cases,
20 ms en `list.int` (un objet partagé posé dans chaque élément), contre
120–150 ms pour une boucle de `lset`, où chaque tour passe par
l'interpréteur.

**La vectorisation des boucles.** Le comptage est une boucle ordinaire :

    foreach v $s {incr c $v}

Son corps est pur (un cumul d'entiers) : à la compilation, Tcl place devant
la boucle un appel au registre des boucles vectorisables. Pour `s + $v` sur
une colonne d'entiers de 8, 16 ou 32 bits en cases, la somme se fait d'un
trait, comme une boucle C (sur 64 bits : aucune somme partielle ne peut
déborder, sinon le calcul repasse par blocs, avec contrôle à chaque
addition, et le résultat devient un grand entier comme dans la boucle) ; si
une valeur n'était pas entière, la boucle ordinaire serait refaite et
donnerait l'erreur d'`incr`. 30 ms → 0,1–0,2 ms en cases, 5 ms sur une
liste d'objets (par blocs de 256). Sans rien changer au programme.

**La création.** `type repeat int(u,8) $N 1` fait une liste de N fois
la valeur 1, toujours rangée en cases : la valeur est vérifiée une fois,
codée dans la première case, et les autres sont remplies d'un bloc (un
`memset` pour des octets), sans liste intermédiaire : 0,1 ms, comme
`bytearray(b'\x01')*N`. (`type cast` d'une liste faite par `lrepeat` donne
la même valeur en 2,5–3 ms : `lrepeat` fait d'abord un million de
pointeurs.) Il n'y a pas de type à déclarer ; pour un type nommé,
`type repeat -into octets $N 1`.

## Les limites de la comparaison

* Le crible mesure les boucles serrées et l'accès aux tableaux ; d'autres
  programmes classeraient les langages autrement.
* Les formes rapides de Python (NumPy, `bytearray`) et de Tcl (cases,
  tranches) demandent d'écrire le crible « par tranches » ; les formes en
  boucle sont celles qu'on écrit sans y penser.
* Une seule machine ; ces mesures varient de 10 % (plus pour Ruby, Perl et
  le Tcl nu) d'une série à l'autre.
