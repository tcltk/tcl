# Manual de los tipos anotados — capítulos

*(Traducción al español de `MANUEL.md`, que sigue siendo la referencia. Este
archivo reúne los capítulos del manual a medida que se escriben; las notas de
parche siguen en `noyau/LISEZMOI.md`. Los identificadores de los ejemplos se
mantienen como en el original francés, para que coincidan con los scripts de
ejemplo y los mensajes de error.)*

## Dict y producto: dos maneras de describir una estructura

Para quien viene de C: una **lista** de Tcl es un array, un **dict** es una
estructura de claves libres, un **tipo producto** es un `struct` declarado.
Los dos últimos se parecen mucho, y a menudo se pasa del uno al otro.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# producto
type def fiche  {dict(nom, age?=0).string}              ;# dict tipado
```

### Lo que tienen en común

| | producto `pt` | dict `fiche` |
|---|---|---|
| construcción por nombre | `set {pt}a {nom}Jean {age}12` o `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| acceso a un campo | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| valor por defecto de un campo/clave | `age ?= 0` (o `=`) | `age?=0` |
| campo desconocido | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| valor serializado | lista Tcl | lista Tcl clave/valor |

El valor por defecto se escribe igual: `?=` se usa en los dos casos, y un
valor por defecto puede ser un literal, `[script]` (calculado en cada
construcción) o `$variable`. En un producto, el antiguo `=` sigue aceptado.

### Lo que los distingue

* **Campos presentes.** En un producto, todos los campos existen siempre:
  un campo omitido toma su valor por defecto, y un campo sin valor por
  defecto debe darse (`field "x" of pt has no value and no default`). En un
  dict, una clave sin `?` es obligatoria (`key "nom" is missing in fiche`) y
  una clave marcada con `?` puede faltar.
* **Orden.** Un producto tiene campos ordenados; también se construye de
  forma **posicional** (`set {pt}b Jean 12`, como la inicialización de un
  struct de C). Un dict no tiene forma posicional.
* **Un solo campo.** Un producto acepta el prefijo `{T.campo}`:
  `set {pt}c {pt.nom}Jean` fija `nom`, los demás campos toman su valor por
  defecto (`Jean 0`). Es la misma escritura que `{Val.i}5` para una suma.
* **Tipos de los campos.** Cada campo de un producto tiene su propio tipo
  (`{string}nom; {int}age`); un `dict(...)` tiene uno solo para todos los
  valores (`.string`).
* **Tamaño.** Un producto tiene un número fijo de campos, fijado en la
  definición.

### Nombrar los campos: etiquetas o camino (se acabó el nombre/valor desnudo)

La forma nombre/valor desnuda (`set {pt}a nom Jean age 12`) queda
**suprimida**: se confundía con la posicional en cuanto *n* argumentos
valían para *n* campos. Un producto se construye ahora de tres maneras, sin
adivinar nada:

* **posicional**: tantos valores como campos, en orden (`set {pt}b Jean 12`);
* **etiquetas de campo**: `set {pt}a {age}12 {nom}Jean`. La etiqueta es la
  anotación más reciente de cada argumento; se lee en el contexto del
  producto, así que un nombre de campo prevalece sobre un tipo global del
  mismo nombre (`type def age int` no cambia nada), como el nombre de una
  variante en una suma. Orden libre; los campos omitidos toman su valor por
  defecto. **Todo o nada**: mezclar etiquetas y valores posicionales es un
  error (`can't mix values named by a field annotation …`), y también lo es
  una etiqueta desconocida (`unknown field "taille" in pt`). Un valor que
  lleva su etiqueta en una variable cuenta
  (`set v {age}12; set {pt}a {nom}Jean $v`);
* **camino**: `set {pt}a(age,nom) 12 Jean`, útil cuando los nombres se
  calculan o los valores van entre llaves.

El dict, en cambio, conserva su forma clave/valor: un dict no tiene forma
posicional.

### Elegir

* Un conjunto de campos **conocido y estable**, con tipos variados: producto.
* Claves **libres u opcionales**, del mismo tipo: dict.
* Un formato con varias formas posibles (PNG, GIF…): una **suma** de
  productos (véase `exemples/png_somme.tcl`).

## bitarray: un entero cuyos bits se leen

`bitarray(n, orden)` es un entero de n bits (0 ≤ v < 2^n). El orden indica
desde dónde parte el índice 0:

| | `lsb0` (por defecto) | `msb0` |
|---|---|---|
| bit de índice 0 | el de la DERECHA (`1 << 0`) | el de la IZQUIERDA |
| uso | registros, máscaras, C | esquemas de protocolos (RFC) |
| índice de un bit si cambia el ancho | sin cambio | desplazado |

Tomemos el byte `0xB2`, escrito `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Lectura.** `$b(i)` da el bit (0 o 1). `$b(i:j)` da los bits i a j como UN
entero, de tipo `bitarray(j-i+1, orden)`: `$b(1:4)` vale 9 en lsb0;
`$c(0:3)` vale 11 en msb0 (los cuatro bits de la izquierda, `1011`).

**Escritura.** `set b(0) 1`, `set b(4:7) 0`. En `expr`, un tramo se escribe
entre comillas, porque el `:` pertenece al analizador de `expr`:
`expr {"b(1:2)" = 3}`. Un valor que no cabe en los bits indicados se
rechaza.

**Dos órdenes, dos tipos.** `bitarray(8,lsb0)` y `bitarray(8,msb0)` no se
mezclan: asignar uno al otro es un `type mismatch`. El `bitarray(8)` por
defecto es `bitarray(8,lsb0)`.

**Funciones.** `popcount(x)`: número de bits a 1; `bitlen(x)`: ancho de un
bitarray, si no, número de bits del entero.

**Dentro de otros tipos.** Un campo de producto (`{bitarray(8)}flags`), un
elemento de lista (`list.bitarray(4)`) o una variable de procedimiento son
bitarrays como los demás: `set r(flags.7) 1`, `set L(2.3)`.

**¿Se parece a un dict?** No: ni claves ni campos, un solo entero. Para
describir los campos de una cabecera binaria, se combina un producto de
bitarrays (véase el capítulo anterior).

Ejemplos completos: `exemples/bitarray_exemples.tcl` (byte «packed» de un
GIF y banderas TCP en `msb0`, permisos Unix en `lsb0`).

## type deref: tomar una instantánea de los enlaces

Un enlace `var` contiene el NOMBRE de una variable y sigue su valor en cada
lectura. `type deref` sustituye cada enlace por el valor que tiene la
variable EN ESE INSTANTE: el resultado ya no cambia después.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : el enlace sigue a B
    puts $v                             ;# 1 2 3 : la instantánea queda fija

* El resultado tiene el tipo «sin enlaces»: `list.var.int` pasa a ser
  `list.int`, y se maneja como cualquier lista de enteros.
* Una variable que falta lo detiene todo con un error que nombra la casilla:
  `type deref: at 2: can't read "::C": no such variable`. No hay resultado
  parcial y el valor original no se toca.
* Solo se sigue un nivel: si una variable apuntada contiene a su vez
  enlaces, se conservan; un segundo `type deref` los sigue.
* Funciona en listas, dicts, productos y SUMAS, anidados o no, y en tipos
  RECURSIVOS (árboles como `tree(var)`). La variante de una suma se conserva.
* Los grafos se tratan por niveles: si los vecinos de un nodo son enlaces
  (`{list.var.node}next`), `type deref` da el nodo con sus vecinos sustituidos
  por sus valores, que aún contienen enlaces; un segundo `type deref` baja un
  nivel. Un ciclo, por tanto, nunca provoca un bucle.
* El tipo del resultado es un tipo propio, llamado `deref(<tipo original>)`
  (`deref(tree(var))`, `deref(mix)`), calculado una vez y luego reutilizado:
  dos instantáneas del mismo tipo tienen el mismo tipo.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# árbol cuyos valores son enlaces
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

Véase también `MANUAL-deref.md` (en inglés): el tipo de enlace `var` y
`type deref`, con ejemplos en `exemples/deref_manual.tcl`.

## Suma y `union` de C: no confundir

Para quien viene de C: una **suma** (`{{A}x, {B}y}`) se parece a una
`union`, pero no es lo mismo.

| | `union` de C | suma |
|---|---|---|
| lo que contiene | los **mismos bytes**, leídos de varias formas | **una sola** variante, que lleva su nombre |
| quién sabe cuál es válida | nadie: el programador debe saberlo | el propio valor (`type get` da `S.x`) |
| leer la variante equivocada | permitido, se leen bytes cualesquiera | imposible: se accede a la variante que se tiene |
| tamaño en un archivo | el de la vista más grande | el de la variante elegida |
| elección de la variante | ninguna, no la hay | por un campo ya leído (`S($tag)`) o por su nombre (`S(x)`) |

Consecuencias:
* **No hay `union` en este modelo**, a propósito: se mantienen las
  salvaguardas. Una suma no puede leerse «de través».
* Cuando el formato no dice qué variante es válida (la `Tcl_ObjInternalRep`
  de Tcl, cuyo discriminante es la dirección `typePtr`, exterior a la union),
  la variante se elige **en la llamada**: `Tcl_ObjAs(twoPtrValue)` (véase
  `exemples/tcl_obj.tcl`). Es quien llama quien lo sabe, como en C, pero el
  tipo sigue comprobado.
* Una suma en la que varias variantes tienen la **misma forma** no es ambigua
  en cuanto la variante se elige por su nombre (`S(y)`): ya no hay nada que
  adivinar.

### Existir, ser nulo, no existir

No hay que confundir tres situaciones para un valor apuntado por un enlace
(tipo `*(...)`, por venir):
* **presente**: el enlace designa un valor;
* **presente pero nulo o vacío**: el valor existe, vale `0`, `{}` o `""`;
* **inexistente**: el enlace es nulo (offset 0, por ejemplo), no hay nada
  que leer.

Un valor por defecto de campo (`= {}`) no distingue las dos últimas. Una suma
con una variante sin valor (`{{T}some, {}none}`) sí las distingue. Aún no
está implementado; véase la nota de diseño.

## Tipos binarios: leer y escribir bytes (`type binary`, `chan binary`)

Un campo dice, por su TIPO, cómo se escribe en bytes. Nada más que aprender:
es el principio de `bitarray(16)`, que ya lleva su ancho. El `.` solo sirve
para COMPONER (`list(3).T`); la representación está en los paréntesis del
tipo.

    type def mz {"MZ"}                           ;# una constante: suma de un literal
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| tipo | representación |
|---|---|
| `int(u,n)` / `int(s,n)` | entero sin signo / con signo de n bits (múltiplo de 8, de 8 a 1024) |
| `double(32)` `double(64)` | flotante IEEE |
| `string(raw,N)` | N bytes en bruto |
| `string(ENC,N)` | texto en N bytes, completado con NUL |
| `string(ENC,z)` | texto terminado en NUL (2 bytes en utf-16, ucs-2) |
| `bitarray(n)` | n bits (múltiplo de 8), leídos como entero sin signo |
| `"MZ"` (suma de literales de igual longitud) | una constante, comprobada por el tipo |
| `list(N).T`, un producto de estos tipos | N elementos; se permite anidar |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un texto
que no se codifica sin pérdida (`é` en ascii) o que no cabe en N bytes se
rechaza.

**Orden de bytes.** Último argumento: `be` (big-endian, orden de red) o
`le`: `int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Sin él, se
usa el de la llamada (`-endian`, little por defecto). No cambia los valores:
`int(u,16,be)` e `int(u,16,le)` son intercambiables.

**Ampliación automática.** Un refinamiento entra donde se espera un tipo más
amplio:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: todo int(u,8) es un int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 cabe en -32768..32767
    set {int(u,8)}v $n               ;# rechazado: estrechamiento (cast explícito)
    set {list.int(u,8)}L $liste_int  ;# también rechazado para las listas

Un tipo definido con `type def port int(u,16)` sigue siendo nominal: `port`
entra en un int, pero un simple `int(u,16)` no entra en un `port`.

**Alineación.** Por defecto los productos son compactos (packed), como las
cabeceras de red y PE/ELF. `-align natural` (o `-align N`) añade el relleno
de C y de LLVM: cada campo empieza en un múltiplo de su alineación (su
tamaño, limitado a 8 o N), el tamaño de un producto se redondea a su
alineación. Relleno a cero al escribir, ignorado al leer salvo con
`-strictpad 1`.

    type binary entete                           ;# la representación, vista desde el núcleo
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# el canal avanza
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

La lectura y la escritura están escritas en Tcl (`layout/filebinary.tcl`)
sobre el núcleo: `type binary` describe, decodifica y codifica bytes en
memoria, `chan binary` lee y escribe en un canal. El nombre `file binary`
(el antiguo) sigue disponible como alias.

**`type binary`, consultas y verbos.** Las preguntas sobre un tipo se
escriben con una anotación local sobre el argumento de tipo (el `?` final la
distingue de cualquier nombre de tipo); las acciones sobre bytes siguen
siendo verbos:

    type binary entete                          ;# la representación (forma desnuda)
    type binary {what?}$T                       ;# ídem
    type binary {sizeof?}$T ?-align A?          ;# tamaño en bytes (error si es variable)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 si $octets se puede decodificar como T, si no 0
    type binary decode T $octets ...            ;# lee los bytes en memoria
    type binary encode T $valeur ...            ;# ídem, encode
    type binary deref ...                       ;# ídem, deref
    type binary space ...                       ;# ídem, space (espacios con nombre)

`{is?}` acepta por defecto un prefijo válido (reconocer una cabecera);
`-exact 1` exige que se consuman todos los bytes. Una opción no válida, un
tipo desconocido o sin representación binaria siguen siendo errores: solo el
fallo de la decodificación da 0. Una anotación desconocida que termina en `?`
es un error (`unknown question "oups?"`); cualquier otra anotación se ignora.
Un tipo que lleva el nombre de un verbo (`type def decode ...`) sigue
descrito por `type binary decode`, mientras no le siga ningún otro argumento.
`decode`, `encode`, `deref` y `space` exigen que `layout/filebinary.tcl` esté
cargado (si no: `the binary layer is not loaded`).

**Alias cortos.** `layout/binaliases.tcl` da nombres breves a las
representaciones habituales (convención de Rust y de Zig): `u8 u16 u32 u64`,
`i8 i16 i32 i64`, `f32 f64`, con los sufijos `be`/`le` (`u16be`, `i32le`,
`f64be`), y `cstring`, `wstringz`, `latin1z`. Son `type alias`: la
biblioteca sirve también de chuleta sobre los formatos binarios. Los
`i8…i64` de aquí tienen signo; los `iN` de LLVM no tienen signo, así que no
hay correspondencia exacta (los arrays `[N x T]` y las estructuras packed
corresponden a `list(N).T` y a los productos).


### Nivel 2: tamaños, cantidades, posiciones y variantes leídos en el archivo

Siempre la misma sintaxis: `$campo` en los argumentos del tipo, como los
parámetros de los tipos parametrizados, o un tipo entero sin signo que sirve
de prefijo.

| escritura | significado |
|---|---|
| `string(utf-8,int(u,16,be))` | texto precedido de su tamaño en bytes (cadena de Pascal) |
| `string(raw,$len)` | N bytes, siendo N el valor del campo `len` |
| `string(utf-8,$len - 4)` | una expresión Tcl sobre los campos ya leídos |
| `list(int(u,8)).T` | lista precedida de su número de elementos |
| `list($n).T` | n elementos, siendo n un campo |
| `at($off).T` | el valor está en la posición `$off` del archivo (el cursor no se mueve) |
| `corps($tag)` | la suma `corps`, cuya variante elige el campo `tag` |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nombres de variantes = etiquetas
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Reglas: un `$campo` debe designar un campo **ya leído** del producto actual o
de un producto que lo englobe (`type binary` lo comprueba); un campo de
producto se lee `$h(off)`. Al escribir, un tamaño o una cantidad que no
corresponde al campo del que depende es un error, y la etiqueta debe ser la
de la variante. `type binary {sizeof?}` rechaza un tipo cuyo tamaño depende
de los datos. Ejemplo completo (PE, ELF, mensajes): `exemples/niveau2.tcl`.

Ejemplos: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Límites: ni
unions ni campos de bits compartidos, ni lista «hasta el final del archivo».

#### Variante elegida: `S(nombre)` y parámetro de tipo

Una suma `S` también puede tener su variante **elegida en la definición**,
sin campo selector en el archivo:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# la variante tp, escrita con todas sus letras
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# o dada por un parámetro
type binary decode {R(tp)} $octets
```

`U(tp)` es un tipo de una sola variante: su tamaño se conoce
(`type binary {sizeof?}`), el valor leído es `U(tp).tp` y no hay nada que
adivinar, aunque varias variantes tengan la misma forma (lo que antes volvía
ambiguo el valor). `U($campo)` conserva su sentido: variante elegida por un
campo ya leído. Si `$v` es un parámetro ligado del tipo, se aplica el primer
caso. Ejemplo: `exemples/tcl_obj.tcl` (`Tcl_ObjAs(twoPtrValue)`). Prueba:
`t36_somme_choisie.tcl`.

#### El tipo enlace `*(origen,repr).T`

Un campo de archivo contiene a menudo una dirección, no el valor. El tipo
enlace lee la dirección (`repr`, un entero) y luego el valor `T` que se
encuentra allí; el valor del campo es el del destino.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Orígenes, como en un sistema de archivos: `//` (o `file://`) inicio de los
datos, `./` inicio de la estructura que lo engloba, `../` su padre, `.` el
propio campo (véase más abajo). Opciones: `null` (la dirección 0 significa
«sin destino»), `null=V`. Con `opt(T)` (véase `unit`), una dirección nula da
`none`, si no `some`: es la diferencia entre «nulo» e «inexistente».

Los alias genéricos (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) dan
nombres cortos: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` en
`layout/binaliases.tcl`. Al escribir, los destinos se añaden al final,
alineados, y se calculan las direcciones. Los tipos recursivos, `memory://` y
los espacios con nombre se describen más abajo. Detalles:
`noyau/CONCEPTION-lien.md`.

**El punto `.`: la dirección es relativa a la ubicación del campo.** `./`
marca el *inicio de la estructura*; `.` marca *el propio campo* (el lugar
donde está escrita la dirección). Es el origen de los desplazamientos «a
partir de aquí», frecuentes en los formatos con desplazamientos relativos
(`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# el campo r está en el offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# mismo resultado, pero con ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Recordatorio: `//` inicio de los datos · `./` inicio de la estructura ·
`../` inicio de la estructura padre · `.` posición del campo. La notación `.`
se mantiene corta a propósito; un nombre más elocuente solo se considerará
tras el uso.

#### Hasta dónde seguir los enlaces: `-linkdepth`

Un enlace puede seguirse enseguida o dejarse tal cual. La opción de llamada
`-linkdepth` fija el número de enlaces seguidos (como el alcance de una
radiación: un rayo gamma lo atraviesa todo, un rayo poco penetrante se
detiene en la piel):

    type binary decode H $d                   ;# 1 {7 8}   Inf, por defecto: seguirlo todo
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   un nivel de enlaces
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      ningún enlace seguido
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Un enlace no seguido es un entero anotado (posición absoluta del destino en
los datos). La copia, `lindex` y `string cat` conservan la anotación; un
cálculo la pierde, y el resultado ya no puede seguirse. Un enlace se sigue a
demanda, paso a paso:

    type binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 por defecto: un solo paso)

Lo que se obtiene: leer solo la cabecera de un archivo grande; recorrer una
lista enlazada o un árbol (el tipo puede ser recursivo:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); localizar un enlace
dañado; modificar un campo en su sitio, pues `type binary encode` reescribe
tal cual una dirección no seguida en lugar de reorganizar el archivo. Un
ciclo solo es un error con `Inf`.


## Espacios con nombre

Un enlace puede designar un espacio registrado por su nombre en lugar de los
datos de la llamada.

```tcl
set sp [type binary space file $octets -name lib]          ;# valor anotado de tipo space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segmentos {dirección offset tamaño}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ es equivalente
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# sin DATOS: el espacio basta
type binary space list | get NOMBRE | delete NOMBRE
```

El nombre por defecto se calcula (`space1`, …); el comando de creación
registra el espacio; un nombre ya usado es un error. `//nombre/` exige un
espacio sin segmentos, `memory://nombre/` un espacio con segmentos; una
dirección fuera de los segmentos es un error. No se puede escribir un destino
real en un espacio con nombre: solo se reescribe la dirección de frontera
(`-linkdepth 0`).

## Escribir un enlace nulo

Un campo enlace que tiene un valor por defecto y cuyo valor es igual a ese
valor por defecto se escribe como un enlace nulo (la dirección «nula» del
enlace, no se escribe ningún destino). La lectura hace lo contrario:
dirección nula → valor por defecto. Sin valor por defecto (y sin `opt`), leer
un enlace nulo sigue siendo un error.

## -dedupe: compartir los destinos idénticos

`type binary encode TIPO VALOR -dedupe 1` (también `write`) escribe una sola
vez los destinos de enlaces cuyos bytes son idénticos; los enlaces afectados
reciben la misma dirección. Por defecto (`-dedupe 0`) se escribe cada
destino. Solo se comparten los destinos sin enlace anidado (las direcciones
internas de un destino dependen de su posición); la relectura da el mismo
valor.

## Tamaño que depende de un campo: comprobado en la construcción

Una lista cuyo número de elementos es una expresión sobre otros campos
(`{list($n).Sample}items`, `list($n - 1).u8`) se comprueba desde la
construcción del producto: `set {Batch}b(n,items) 3 {{1 1.0 0}}` se rechaza
(`field "items" of Batch holds 1 elements, but its size says 3 ($n)`). Los
campos omitidos cuentan con su valor por defecto. La comprobación no cubre
las asignaciones campo a campo (`set b(n) 3`: un estado intermedio es
legítimo); `type binary encode` vuelve a comprobar al escribir. Los tamaños
en bytes de las cadenas (`string(raw,$len)`) no se comprueban en la
construcción.
