# Anotaciones en Tcl — documentación

*19 de octubre de 2026 · versión v13 · Florent MERLET, con Claude (Anthropic)*

*Otros idiomas: [Français](ANNOTATIONS.fr.md) (referencia) · [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md)*

## Prólogo

**Autores**: Florent MERLET, con Claude (Anthropic).

Este documento describe las **anotaciones** de Tcl 9.0 que propone el TIP 764: un metadato adjunto a una palabra (`{x}palabra`) que no cambia el valor, pero permite tipar variables, nombrar campos y describir formatos binarios. Sustituye al manual escrito a medida que avanzaba el desarrollo y describe el prototipo en su **versión v13**.

Tres partes, para tres lectores:

- **Parte I — Conceptos**: qué es una anotación, la diferencia entre una nota y un tipo, y sus reglas. Para leer primero.
- **Parte II — Referencia**: los comandos `note` y `type`, el lenguaje de tipos, los caminos, la construcción de valores, los tipos binarios y los errores. Para quien escribe Tcl.
- **Parte III — Implementación**: el objeto anotado, el analizador, el bytecode, los archivos y funciones implicados. Para quien revisa el TIP o el código.

Siguen la compatibilidad y el rendimiento, las decisiones de diseño, los límites y los anexos (instalación, glosario, equivalencias con C, Rust y LLVM).

Los ejemplos son Tcl ejecutable en el prototipo; los comentarios `;#` dan el resultado. Los mensajes de error se citan tal como los produce Tcl. Los identificadores de los ejemplos (`nom`, `fiche`, `couleur`, `grandeur`…) se mantienen como en el original francés, para que coincidan con los scripts de ejemplo.

## Parte I — Conceptos

### 1. Las anotaciones

#### 1.1 Origen de la palabra

*Anotar* viene del latín *ad notare*, «poner una nota al lado». La palabra *protocolo* dice lo mismo de otra forma: el *prōtokollon* griego era la primera hoja pegada al principio de un rollo de papiro, que describía su contenido sin formar parte de él. Una anotación Tcl es exactamente eso: una hoja pegada al principio de una palabra.

```tcl
{mm}12          ;# el valor sigue siendo 12; «mm» va pegado delante
{list.int}L     ;# el nombre sigue siendo L; «list.int» dice lo que contendrá
```

#### 1.2 Un metadato

Una anotación es un **metadato**:

- **no aparece en el texto** del valor: `{mm}12` vale `12` para `puts`, `expr`, `string length`;
- **aporta una información** que el valor solo no lleva: una unidad, un tipo, el nombre de un campo, una opción;
- vive **fuera del plano de los valores en bruto**: ningún comando Tcl existente la ve, y ningún script existente cambia de sentido.

#### 1.3 Una analogía: la parte imaginaria

Un número complejo tiene una parte real y una parte imaginaria. Del mismo modo, un valor anotado tiene una parte **en bruto** (el texto que Tcl ha manejado siempre) y una parte **anotada** (lo que el desarrollador tiene en mente).

| | número complejo | valor anotado |
|---|---|---|
| parte visible | la parte real | el valor en bruto (`12`) |
| parte añadida | la parte imaginaria | la anotación (`mm`, `int`, `nom`) |
| lo que se lee normalmente | la parte real | la cadena |

La analogía se detiene en el cálculo. En un complejo, la parte imaginaria participa en la aritmética. Aquí, **un cálculo pierde la anotación**: `expr {$x + 1}` devuelve un número desnudo. Solo la copia, `lindex`, `string cat` y el paso como argumento la conservan. Es deliberado: el plano de los valores en bruto sigue siendo Tcl ordinario, y eso es lo que garantiza la compatibilidad.

#### 1.4 Guiar la ejecución

La anotación hace explícita la «parte imaginaria» del desarrollador, lo que sabe de sus datos y Tcl ignoraba:

- un **tipo** hace verificar cada escritura en una variable (`set {int}n abc` falla);
- un **nombre de campo** elige adónde va un valor (`set {pt}a {age}12 {nom}Jean`);
- una **representación binaria** dice cómo leer y escribir bytes (`{int(u,16,be)}version`);
- una **nota** marca un valor para que un procedimiento lo reconozca (`note has $v mm`).

El script conserva su forma: se añaden anotaciones donde se quieren garantías, y en ningún otro sitio.

### 2. Notas, tipos y anotaciones locales

Una anotación se escribe siempre `{texto}palabra`. Según lo que dice el texto y según **quién lo lee**, desempeña tres papeles.

#### 2.1 La nota: la anotación en bruto

Una **nota** es una anotación que ningún constructor ha resuelto todavía. Sigue al valor: copia, argumento de procedimiento, elemento de lista.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (vacío: ningún tipo resuelto)
```

**Apilamiento.** Se anota un valor ya anotado; las notas se apilan, de la más antigua a la más reciente.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px es la más reciente)
```

La escritura `{a}{b}$x` (dos anotaciones pegadas) no existe: sería ambigua con `{int}{300}`, una anotación sobre un literal entre llaves. Se apila anotando un valor que ya lo está.

#### 2.2 El tipo: la anotación resuelta por un constructor

Un **tipo** es una anotación **resuelta** en la tipología que conoce el intérprete (tipos de base, `type def`, `type alias`). Solo un **constructor de variable** resuelve:

- `set {T}v …` (y `set v {T}$x`);
- la vinculación de los argumentos de `proc`;
- `lassign`, `foreach`, y en `expr`, `x = v` y `x is T = v`.

Puesta sobre el **nombre** de una variable, una anotación da a la variable un **contrato**: toda escritura posterior se verifica.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Fuera de un constructor, `{T}$x` no verifica nada ni convierte nada, aunque `T` sea un tipo conocido: `T` simplemente se **apila**, como una nota. `list {int}abc` no produce ningún error.

#### 2.3 Dos pilas y una regla de desapilado

Un valor anotado lleva **dos listas**: el **tipo resuelto** (como mucho uno) y la **pila de notas** pendientes. La visualización las separa con `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 tipo resuelto   notas, de la más antigua (mm) a la más reciente (hello)
```

Un constructor **desapila** desde la cima:

1. toma la nota más reciente e intenta resolverla;
2. continúa mientras las notas se resuelvan; la primera que no se resuelve lo detiene todo y queda en la pila;
3. los tipos desapilados deben ser coherentes: el más reciente debe entrar en cada uno de los demás; si no, `contradictory annotations px and qq`;
4. el valor se convierte («castea») al tipo más reciente;
5. el contrato de la variable hace el último control.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px se resuelve, mm no
type get $z          ;# px      ; note get $z: mm
```

Una nota puesta **antes** de su `type def` la resuelve, por tanto, el constructor siguiente: escribir `type def mm double` más tarde da sentido a los `{mm}` ya puestos.

**Primero el contexto.** Un constructor resuelve en su contexto antes que en los tipos globales: en una suma, los nombres de las variantes (`{i}5` en `set {S}v {i}5`); en un producto, los nombres de los campos (`{age}12`). Un tipo global con el mismo nombre no cambia nada.

#### 2.4 Las anotaciones locales

Una anotación cuyo texto contiene un carácter que los nombres de tipo rechazan (`-`, `?`, `:`…) **nunca** puede ser un tipo. Es una **anotación local**: solo vive entre quien llama y el comando llamado, y no sigue al valor.

```tcl
type binary {sizeof?}$T          ;# una pregunta dirigida a type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# rechazado: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# rechazado: local annotation {-v} cannot declare a variable
```

Dos familias naturales, sin riesgo de colisión con un nombre de tipo: las opciones en `-nombre` y las preguntas en `nombre?`. Un nombre de tipo solo contiene letras ASCII, cifras, `_` y bytes no ASCII, y no empieza por una cifra.

#### 2.5 La anotación vacía `{}`

`{}` es «el tipo vacío». Su efecto depende del lugar:

| dónde | efecto |
|---|---|
| sobre un valor, en un constructor (`set v {}$x`) | retira el tipo resuelto, conserva las notas |
| sobre un nombre (`set {}v $x`) | levanta el contrato de la variable; el valor se guarda con sus notas, sin resolver nada |
| sobre un valor, fuera de un constructor | el lector ve el contenido desnudo (tipo y notas borrados) |

```tcl
set {int}n 5
set {}n abc          ;# el contrato se levanta: n vale abc
```

El valor original nunca se modifica: `{}` produce una nueva vista, lo que hace inofensivo compartir objetos.

## Parte II — Referencia

### 3. El comando `note`

```
note get VALOR               ;# las notas, de la más antigua a la más reciente, unidas por «.»
note has VALOR NOMBRE        ;# 1 si NOMBRE es una de las notas, si no 0
note delete VALOR ?NOMBRE?   ;# el valor sin la nota NOMBRE, o sin ninguna nota
```

Los valores Tcl son inmutables: `note delete` **devuelve** un nuevo valor, no modifica una variable.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: no hay apilamiento pegado
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Uso típico: un procedimiento que reacciona a la marca de un valor.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. El comando `type`

```
type def NOMBRE DEFINICIÓN     ;# define un tipo nominal (o una familia parametrizada)
type alias NOMBRE DEFINICIÓN   ;# da un nombre estructural a una definición
type get VALOR                 ;# el tipo resuelto de un valor (vacío si no hay)
type cast TIPO VALOR           ;# el resolutor de un constructor, sin variable
type convert DE A LAMBDA       ;# declara una conversión explícita
type deref VALOR               ;# instantánea: cada enlace var sustituido por su valor
type binary TIPO ...           ;# representación binaria, codificación, decodificación (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (un alias no es un tipo nuevo)
type cast pt {Léa 7}          ;# Léa 7, de tipo pt
```

En `expr`, el operador **`is`** comprueba el tipo de una variable, y una asignación puede declarar:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# declara z de tipo pt y le asigna el valor
expr {new is cmd}              ;# 1 si new es un comando; new(…) pasa a ser una función de expr
```

`is cmd` no es un tipo: comprueba que un comando existe y lo expone como función de `expr`.

### 5. El sistema de tipos

#### 5.1 Tipos de base

| tipo | valores |
|---|---|
| `int` | entero (sin límite, como en Tcl) |
| `double` | flotante |
| `string` | cualquier cadena |
| `list` | lista Tcl ordinaria (elementos sin tipo) |
| `dict` | diccionario Tcl ordinario |
| `var` | enlace a una variable, por su nombre (§ 5.3) |
| `any` | elemento de una lista heterogénea (`list.any`): cada elemento conserva su tipo |

Los **refinamientos** restringen un tipo de base y llevan una representación binaria: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Un `type def` crea un tipo **nominal**: `type def port int(u,16)` es distinto de `int(u,16)`. Un `type alias` solo da un nombre a una definición.

#### 5.2 El lenguaje de composición

Un tipo se escribe como una expresión, analizada según el modelo de `expr` (`tclTypeParse.c` retoma la estructura de `tclCompExpr.c`).

**Tres clases de operandos:**

| operando | dónde | ejemplo |
|---|---|---|
| tipo | palabra desnuda, a la izquierda de una `}` o en posición de operando | `int`, `list`, `pt` |
| nombre | palabra pegada a la derecha de una `}` | `{int}age`: el campo `age` |
| valor | entre comillas, entre llaves, o tras `=` | `"MZ"`, `= 0` |

**Operadores**, del más fuerte al más débil:

| operador | papel | ejemplo |
|---|---|---|
| `.` | anidamiento (asociativo por la derecha) | `list.dict.int` |
| `( )` | argumentos, pegados al nombre | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` o `?=` | valor por defecto de un campo | `{int}age ?= 0` |
| `;` | producto | `{double}x; {double}y` |
| `,` | suma | `{int}i, {string}s` |
| `{ }nombre` | agrupación, con un nombre pegado a la llave de cierre | `{{int}x; {int}y}A` |

`;` y `,` tienen la misma precedencia: para mezclar un producto y una suma, se agrupa con llaves (`{int}a; {{int}b, {string}c}u`).

**Caracteres.** Un nombre de tipo solo contiene letras ASCII, cifras, `_` y bytes no ASCII, y no empieza por una cifra. La gramática usa además `{ } , ; . = " # ( ) * $`. Cualquier otro signo ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) se rechaza: `invalid character "!"`, código `TCL PARSE TYPE BADCHAR`, salvo tras `=` (un valor por defecto) y entre los paréntesis de un constructor (`*(file://x, …)`). Este rechazo es lo que hace seguras las anotaciones locales (`{-v}`, `{is?}`).

Una definición es **declarativa**: no se hace ninguna sustitución al leerla, salvo en un valor por defecto, evaluado en cada construcción.

#### 5.3 Campos y contenedores

**Enumeración** — una suma de literales.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# un solo literal: una constante (firma de archivo)
```

**Suma** — una sola variante a la vez, que lleva su nombre.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# la variante elegida por el nombre del tipo
```

**Producto** — un `struct`: campos con nombre, ordenados, cada uno de su tipo.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# posicional
set {pt}b {age}12 {nom}Jean    ;# etiquetas de campo (§ 8)
$a(nom)                        ;# Jean
```

**Lista** — `list` (extensible), `list.T` (elementos de tipo T), `list(N).T` (exactamente N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

Los elementos de una `list.T` son atómicos: `lindex`, `foreach`, `$L(i)` devuelven elementos tipados, que nunca se vuelven a partir en palabras. La lista desnuda `{list}` conserva el comportamiento de Tcl.

**Diccionario** — `dict` (cualquier clave), `dict.T` (valores de tipo T), `dict(claves).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom obligatorio; age opcional, 0 por defecto
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z puede faltar; `dict()`: solo la clave vacía. Las claves son siempre texto; solo los valores están tipados.

**Enlace `var`** — el NOMBRE de una variable, seguido en cada lectura.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (una instantánea, de tipo list.int)
```

**Parámetro de tipo `{type}T`** — en un tipo parametrizado, un parámetro que recibe un tipo (§ 5.5).

**Enlace binario `*(origen,repr).T`** — una dirección en datos binarios, seguida hasta un valor T (§ 9.5).

#### 5.4 Valores por defecto

`?=` (o `=`, la escritura antigua, aún aceptada en un producto) da un valor por defecto a un campo. Puede ser un literal, un `[script]` evaluado en cada construcción, o una `$variable` leída en cada construcción.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Un campo sin valor por defecto debe darse: `field "x" of pt has no value and no default`.

#### 5.5 Tipos parametrizados

El nombre de un `type def` puede llevar parámetros, declarados como campos: `{type}T` (un tipo), `{int}n`, `{double}f`, `{string}s`. Se usan en el cuerpo con `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# recursivo
```

#### 5.6 Parámetros fantasma: las unidades físicas

Un parámetro ausente del cuerpo está permitido: no cambia los valores, pero distingue las instancias, que son tipos nominales distintos. Una familia de magnitudes da así unidades cuyo valor sigue siendo un `double`:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# regla no aplicable
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (conversión explícita)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Una regla `type convert` puede apuntar a un tipo o a una **familia** (el nombre de un `type def` parametrizado); la lambda recibe entonces los parámetros de las instancias (`$de(f)`). `return -code continue` significa «no aplicable». Las conversiones siguen siendo explícitas.

#### 5.7 Alias incluidos

`layout/binaliases.tcl` define, para los formatos binarios (convenciones de Rust y de Zig):

| alias | definición |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| sufijos `be`, `le` (`u16be`, `i32le`) | orden de bytes fijo |
| `f32 f64` (y `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Variables tipadas y vida de un valor anotado

#### 6.1 El contrato de una variable tipada

Una variable declarada con `set {T}v …` tiene un **contrato**: cada escritura se verifica, venga de `set`, `lappend`, `lset`, `incr`, `dict set`, de una escritura por camino (`set v(x) …`) o de `expr {v = …}`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Ampliación automática.** Un refinamiento entra donde se espera un tipo más amplio: `int(u,8)` en `int`, `int(s,16)` o `double`; `double(32)` en `double(64)`. El estrechamiento se rechaza (exige un cast explícito), igual que entre `bitarray(…,lsb0)` y `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# aceptado: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominales.** Un tipo nominal se borra ante su base (`px` entra en `int` y da `int`, sin rastro). Dos nominales sin vínculo de base nunca son equivalentes (`mm` y `kg`): ese es precisamente su papel.

**Casts explícitos.** `set v {T}$x` o `type cast T $x` convierten según, en este orden: una regla `type convert` específica; la cadena de bases; la variante de una suma; una regla de familia; las conversiones integradas entre escalares. Si no: `unknown way to cast the value from A to B`.

**Levantar el contrato.** `set {}v valor` (§ 2.5).

**Comprobar.** `expr {v is T}`: verdadero si la variable está tipada y su tipo entra en `T`; para una variable sin tipo, verdadero si su valor es válido para `T`.

#### 6.2 Lo que conserva la anotación, lo que la pierde

| operación | tipo | notas |
|---|---|---|
| copia (`set a $x`), argumento, valor de retorno | conservado | conservadas |
| `lindex`, `foreach`, `$L(i)` sobre una `list.T` | elemento de tipo `T` | — |
| `string cat $x` (un solo argumento) | conservado | conservadas |
| cálculo (`expr`, `string toupper`, `dict set` sobre un valor) | **perdido** | **perdidas** |
| cast `{T}$x` en un constructor | cambiado | conservadas |
| `set {T}a $x`, variable tipada | `T` | borradas por ampliación |

**Adopción.** Una variable **sin tipo** que recibe un valor tipado **adopta** su tipo, y por tanto su contrato:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e pasa a ser una variable int
set e hello                    ;# expected integer but got "hello"
```

Es el comportamiento más sorprendente del modelo. La trampa clásica es un `catch … m` reutilizado: si un primer `catch` guarda en `m` un valor tipado, un segundo `catch` ya no puede guardar allí su mensaje de error. Usar una variable por uso, `unset` entre dos usos, o `set {}m …`. (Punto abierto, § 17.)

**Procedimientos.** Un argumento que recibe un valor tipado queda tipado y estricto dentro del procedimiento; un argumento ordinario no cambia.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Variables de bucle.** `foreach`, `lmap`, `dict for`, `dict map` (y el filtro de `dict filter`) olvidan el tipo de su variable antes de cada asignación; tras el bucle, la variable conserva el tipo del último elemento recibido.

#### 6.3 `type deref`: una instantánea de los enlaces

`type deref V` devuelve `V` con cada enlace `var` sustituido por el valor que tiene la variable en ese instante. El resultado tiene el tipo «sin enlaces» (`list.var.int` → `list.int`), llamado `deref(<tipo>)` para un producto o una suma. Solo se sigue un nivel por llamada; una variable que falta es un error que nombra la casilla (`type deref: at 2: can't read "::C": no such variable`), sin resultado parcial. Funciona en listas, dicts, productos, sumas y tipos recursivos; un ciclo nunca provoca un bucle.

### 7. Acceder a las partes de un valor: los caminos

#### 7.1 La forma entre paréntesis

Un valor tipado se lee y se escribe con la sintaxis de los arrays, pero el índice es un **camino**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# construcción (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# escritura de un campo entero
set R((A,B).x) 10 30                       ;# escritura múltiple
${R(A.x, B.y)}                             ;# lectura, espacios permitidos
```

Los espacios están permitidos en `$R(…)`, `${R(…)}` y en una palabra anotada `{T}R(…)`. En una palabra desnuda no anotada (`set R(A.x, A.y) …`), Tcl corta en los espacios como siempre: escribir sin espacios o entre comillas.

#### 7.2 El lenguaje de caminos

El lenguaje es el de la página de la wiki de Tcl «A better way to do indexation», analizado según el modelo de `expr` (`tclTypePath.c`).

**Operandos:**

| operando | sentido |
|---|---|
| `nombre` (palabra desnuda) | campo, clave de dict, nombre de variante o índice de lista (`3`, `end`, `end-1`): decide el tipo del contenedor |
| `'clave'` | siempre una clave (`'end'` es la clave «end», no un índice) |
| `"etiqueta"` | texto literal copiado en el resultado |

**Operadores**, del más débil al más fuerte:

| operador | papel | ejemplo |
|---|---|---|
| `,` | colección (un grupo) | `A,B` |
| `*` (unario) | despliegue: el grupo se funde en su padre | `*0:2,4` |
| `.` | anidamiento; se distribuye sobre un grupo | `A.(x,y)` = `A.x,A.y` |
| `:` | intervalo de lista | `0:end-1` |
| `( )` | agrupación | `(A,B).x` |

**Forma del resultado.** Una lectura múltiple devuelve listas anidadas según los grupos: `$R((A,B).(x,y))` da `{1 2} {3 4}`; `$L(0:2,4)` da `{10 20 30} 50`; `$L(*0:2,4)` da `10 20 30 50`.

**El camino no sustituye nada.** Es Tcl quien sustituye `$x`, `[cmd]` y `[(expr)]` en `$R(…)` y en las palabras desnudas; un valor sustituido es **una hoja**, nunca releída como camino:

```tcl
set k file.txt ; $D($k)        ;# la clave «file.txt» (no D.file.txt)
set n 1        ; $D(f_$n)      ;# pegado a un texto, el valor prolonga el nombre
set idx {0 2}  ; $L(*$idx)     ;# * despliega un valor lista: 10 30
set p {[exec ls]} ; $L($p)     ;# no se ejecuta nada: bad index "[exec ls]"
```

Entre llaves y en `${…}`, `$` y `[` son errores: `a path does not substitute "$"…`. Un camino calculado se lee **nombrando** la variable con él: `set ref R($chemin) ; set $ref`.

#### 7.3 Caso por contenedor

| contenedor | lectura | escritura |
|---|---|---|
| producto | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (revalidado) |
| suma | `$v(i)`: la variante actual | `set v(i) 6` |
| lista | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (un bit), `$b(1:4)` (un tramo: un entero `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| enumeración | (escalar: sin camino) | — |

Leer y luego reescribir el mismo camino funciona siempre: `set R(c) [set R(c)]`.

### 8. Construir un valor

#### 8.1 Las formas de un constructor de producto

Un producto de *n* campos se construye de cuatro maneras, sin adivinar nada:

| forma | ejemplo | regla |
|---|---|---|
| posicional | `set {pt}a Jean 12` | exactamente *n* valores, en el orden de los campos |
| etiquetas de campo | `set {pt}a {age}12 {nom}Jean` | todos los valores etiquetados; orden libre; los campos omitidos toman su valor por defecto |
| un solo campo | `set {pt}a {pt.nom}Jean` | los demás campos toman su valor por defecto |
| camino + valores posicionales | `set {pt}a(age,nom) 12 Jean` | un valor por hoja del camino (o por grupo, § 8.3) |

Se añaden **ningún argumento** (`set {pt}a`, todos los campos con su valor por defecto) y **un producto del mismo tipo** (copia). Todo lo demás es un error:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

La forma nombre/valor desnuda (`set {pt}a nom Jean age 12`) **ya no existe**: se confundía con la posicional en cuanto *n* argumentos valían para *n* campos (§ 16).

#### 8.2 Las etiquetas de campo

La etiqueta es la anotación **más reciente** de cada argumento, leída en el contexto del producto: un nombre de campo prevalece sobre un tipo global del mismo nombre.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# no cambia nada
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# cuenta una etiqueta llevada por una variable
```

**Todo o nada:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

Un valor entre llaves también se etiqueta: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Camino y valores posicionales

`set {T}v(camino) v1 v2 …` construye el producto (y sus productos anidados); los campos nombrados reciben los valores, los demás su valor por defecto. El **número** de valores elige el reparto, nunca su contenido:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# un valor por hoja
set {rect}R((A,B).(x,y))     1 2 3 4      ;# ídem
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# un valor por elemento del grupo de cabeza
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# un valor para todo el camino
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# con despliegue: un valor, plano
```

Cada valor tiene la forma que devolvería su lectura; los tres números solo coinciden si coinciden las lecturas.

**La combinación camino + etiquetas no existe**: `set {rect}R(A) {y}2 {x}1` es un error (`1 targets but 2 values`). No está prevista mientras ningún uso la pida.

#### 8.4 Sumas y listas

Una suma se construye con su variante como etiqueta, o con el nombre de la variante en el tipo:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# cada elemento conserva su variante
```

Sin etiqueta, la variante se deduce del valor, en el orden de las variantes: `set {S}u 5` da `S.i`. Cuando varias variantes tienen la misma forma (`{string}a, {string}b`), la primera siempre encaja: para elegir, etiquetar (`{b}y`).

Una lista recibe sus elementos uno a uno: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Un solo argumento es **un** elemento.

### 9. Tipos binarios

Un tipo dice, por sí mismo, cómo se escribe en bytes: no hay más sintaxis que aprender. La lectura y la escritura las hace `layout/filebinary.tcl` (cargado con `source`), sobre el núcleo, con una vía rápida en C para las disposiciones fijas (§ 9.6).

#### 9.1 `bitarray`: un entero cuyos bits se leen

`bitarray(n, orden)` es un entero de n bits (0 ≤ v < 2^n). El orden dice desde dónde parte el índice 0: `lsb0` (por defecto, el bit de la derecha, como `1 << i`: registros, máscaras) o `msb0` (el bit de la izquierda, como los esquemas de los RFC). Son dos tipos distintos.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (un entero bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# en expr, un tramo entre comillas
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Representaciones

| tipo | representación |
|---|---|
| `int(u,n)` / `int(s,n)` | entero sin signo / con signo de n bits (múltiplo de 8, de 8 a 1024) |
| `double(32)`, `double(64)` | flotante IEEE |
| `string(raw,N)` | N bytes en bruto |
| `string(ENC,N)` | texto en N bytes, completado con NUL |
| `string(ENC,z)` | texto terminado en NUL (2 bytes en utf-16, ucs-2) |
| `bitarray(n)` | n bits (múltiplo de 8), leídos como entero sin signo |
| `"MZ"` (suma de literales de igual longitud) | una constante, verificada |
| `list(N).T`, un producto de estos tipos | N elementos; se permite anidar |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un texto que no se codifica sin pérdida (`é` en ascii) o que no cabe en N bytes se rechaza en la construcción.

**Orden de bytes.** Último argumento: `be` o `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Sin él, se usa el de la llamada (`-endian`, little por defecto). No cambia los valores: `int(u,16,be)` e `int(u,16,le)` se asignan uno a otro.

**Alineación.** Por defecto los productos son compactos (packed). `-align natural` (o `-align N`) añade el relleno de C: cada campo empieza en un múltiplo de su alineación, el tamaño de un producto se redondea. Relleno a cero al escribir, ignorado al leer salvo con `-strictpad 1`.

#### 9.3 Comandos

```
type binary TIPO                                   ;# la representación, vista desde el núcleo
type binary {what?}TIPO                            ;# ídem
type binary {sizeof?}TIPO ?-align A?               ;# tamaño en bytes (error si depende de los datos)
type binary {is?}TIPO BYTES ?-exact 1?             ;# 1 si BYTES se decodifica como TIPO, si no 0
type binary decode TIPO BYTES ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TIPO VALOR ?-endian E? ?-align A? ?-dedupe 1?
type binary deref ENLACE ?BYTES?                   ;# sigue un enlace no seguido, un paso
type binary space file|memory|list|get|delete …    ;# espacios con nombre
chan binary read  CANAL TIPO ?opciones?            ;# el canal avanza
chan binary write CANAL TIPO VALOR ?opciones?
```

Las preguntas (`{what?}`, `{sizeof?}`, `{is?}`) son **anotaciones locales** sobre el argumento de tipo; las acciones son verbos. `file binary` sigue siendo un alias del nombre antiguo.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Nivel 2: lo que el archivo dice de sí mismo

Un tamaño, una cantidad, una posición o una variante pueden leerse en el archivo: `$campo` en los argumentos del tipo (un campo **ya leído** del producto actual o de un producto que lo engloba), o un tipo entero sin signo que sirve de **prefijo**.

| escritura | sentido |
|---|---|
| `string(utf-8,int(u,16,be))` | texto precedido de su tamaño en bytes (cadena de Pascal) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N bytes, N calculado a partir de campos leídos |
| `list(int(u,8)).T`, `list($n).T` | lista precedida de su número de elementos, o de n elementos |
| `at($off).T` | el valor está en la posición `$off` (el cursor no se mueve) |
| `corps($tag)` | la suma `corps`, cuya variante elige el campo `tag` |
| `U(tp)`, `R($v)` (parámetro ligado) | una suma cuya variante se fija en la definición |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nombres de variantes = valores de la etiqueta
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Ya en la construcción se comprueba una lista cuyo tamaño depende de un campo: `set {Batch}b(n,items) 3 {{1 1.0 0}}` se rechaza (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Enlaces: `*(origen, repr, opciones).T`

Un campo contiene a menudo una dirección, no el valor. El tipo enlace lee la dirección (`repr`, un entero) y luego el valor `T` que se encuentra allí.

| origen | la dirección parte de |
|---|---|
| `//` (o `file://`) | el inicio de los datos |
| `./` | el inicio de la estructura que lo engloba |
| `../` | el inicio de su padre |
| `.` | el propio campo |
| `//nombre/`, `memory://nombre/` | un espacio con nombre (archivo, o segmentos de memoria) |

Opciones: `null` (la dirección 0 significa «sin destino»), `null=V`. Con `opt(T)`, una dirección nula da `none`, si no `some`: es la diferencia entre «nulo» e «inexistente». Al escribir, los destinos se añaden al final, alineados, y se calculan las direcciones; `-dedupe 1` escribe una sola vez los destinos idénticos. Un campo enlace igual a su valor por defecto se escribe como un enlace nulo.

**`-linkdepth`** fija el número de enlaces seguidos (`Inf` por defecto). Un enlace no seguido es un entero anotado (la posición absoluta del destino); `type binary deref` lo sigue un paso. Esto permite leer solo la cabecera de un archivo grande, recorrer una lista enlazada y reescribir un campo en su sitio.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: el destino
```

#### 9.6 Códecs C

`type binary encode` y `decode` prueban primero una vía en C, que cubre las disposiciones fijas: enteros de 1, 2, 4, 8 bytes, flotantes de 4 y 8 bytes, productos (con `-align`), `list(N).T` y `list($n).T`. Fuera de este dominio (cadenas, sumas, enlaces, `at`, campos de bits, tamaños calculados, `-dedupe`), o en caso de error, la capa Tcl toma el relevo y da sus mensajes. `set ::tcl_typebinaryc 0` desactiva la vía C. Los bytes y los valores producidos son idénticos por las dos vías (prueba `t51_codec_c`). Medidas: § 15.

### 10. Errores

Los mensajes siguen el uso de Tcl: nombran el valor, el tipo o el campo en cuestión. Los códigos `errorCode` propios de los tipos empiezan por `TCL TYPE` o `TCL PARSE TYPE`.

| situación | mensaje (ejemplo) | `errorCode` |
|---|---|---|
| carácter prohibido en un tipo | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| nombre de tipo desconocido sobre un nombre de variable | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| anotación local sobre un nombre de variable | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| anotación local guardada por `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| valor no válido para el tipo | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (según el tipo de base) |
| ninguna variante encaja | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| tipos nominales distintos | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| ninguna conversión conocida | `unknown way to cast the value from mm to kg` | — |
| anotaciones apiladas incoherentes | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| campo desconocido | `unknown field "zz" in pt`; `pt has no field "zz"` (camino) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| campo sin valor ni valor por defecto | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| número de valores incorrecto | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| etiquetas y posiciones mezcladas | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| campo duplicado | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| tamaño dependiente no respetado | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| clave de dict ausente o no permitida | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| aridad de lista | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| sustitución en un camino literal | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| índice de lista no válido en un camino | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| enlace a una variable desaparecida | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` sobre un enlace roto | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| valor anotado creado fuera del analizador | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| capa binaria no cargada | `the binary layer is not loaded` | — |

## Parte III — Implementación

### 11. Vista general

La implementación cabe en **siete archivos nuevos** (unas 15 000 líneas de C), copiados en `generic/`, y en **puntos de enganche** en una veintena de archivos del núcleo. Los archivos nuevos nunca se parchean: se entregan completos; solo los archivos del núcleo reciben parches.

### 12. El objeto anotado

#### Estructura

Un valor anotado es un `Tcl_Obj` ordinario cuyo tipo interno es `annotated` (`annotatedObjType`, en `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   la cadena: la del contenido V (calculada      │
 │                  a petición por UpdateStringOfAnnotated)       │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   el contenido: un Tcl_Obj ordinario, compartido  │
 │   ptr2 ──► T   la anotación, en una de estas formas:           │
 │                · una referencia de tipo resuelta (typeRef)      │
 │                · el texto en bruto de la anotación (aún no      │
 │                  resuelta: una nota, o un tipo por resolver)    │
 │                · ""   la marca de la anotación vacía {}         │
 │                · un estado «annotstate» {T, N}: T resuelto      │
 │                  (o NULL) y N, la lista de notas, de la más     │
 │                  antigua a la más reciente                      │
 └───────────────────────────────────────────────────────────────┘
```

#### Propiedades

- **Transparente**: la cadena es la de V. Para cualquier comando que no conozca las anotaciones, el valor es V.
- **Inmutable y compartido**: el estado `{T, N}` es a su vez un objeto inmutable; `Tcl_DuplicateObj` lo comparte. Anotar, desapilar o retirar una nota produce un objeto nuevo.
- **Sin coste sin notas**: mientras no hay nota, `ptr2` es solo T.
- **Lista abstracta** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… pasan por los procedimientos de lista del objeto (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). Los elementos de una `list.T` se devuelven tipados; un escalar anotado es una lista de un elemento que es él mismo.

#### Rutinas del `Tcl_ObjType`

| rutina | papel |
|---|---|
| `FreeAnnotated` | libera V y T (y la celda «sí mismo» de un escalar, tabla `SelfCell`) |
| `DupAnnotated` | copia V, comparte T o el estado `{T, N}` |
| `UpdateStringOfAnnotated` | la cadena de V |
| `SetAnnotatedFromAny` | **rechaza**: un valor anotado nace del analizador o de una asignación tipada, nunca de una conversión (`TCL VALUE ANNOTATED`) |

#### Funciones principales (`tclAnnotObj.c`)

`TclAnnotateValue` (poner una anotación: cast o nota), `TclAnnotSettle` (el desapilado de un constructor, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (tipo, variante, desconocido, local, no válido; el contexto antes que los tipos globales), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (el contenido desnudo, para las conversiones del núcleo), `TclAnnotDisplay` (`list<int>@mm.hello`), y los comandos `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. En el analizador y el compilador

#### El prefijo `{x}palabra`

En Tcl 9 original, una palabra entre llaves seguida de otros caracteres es un error de sintaxis (`extra characters after close-brace`), salvo `{*}`. El prefijo de anotación ocupa pues una escritura **que no tenía sentido**: ningún script válido cambia de sentido.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: una palabra `{texto}resto` produce un token **`TCL_TOKEN_ANNOT`** (512, `tcl.h`) que lleva el texto de la anotación, seguido de los tokens del resto de la palabra. Una palabra anotada `{T}nombre(...)` llega hasta el paréntesis equilibrado, espacios incluidos, en una línea.
- `tclCompile.c`, `TclCompileTokens`: si el resto de la palabra es literal (`TclAnnotSuffixIsLiteral`), la palabra se compila en `::tcl::annotraw texto palabra` (una nota en bruto, sin sustitución); si no, en `::tcl::annotate texto valor`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: un `set` cuyo nombre está anotado pasa por `TclSetAnnotatedCmd`; una anotación local escrita sobre el valor (`set x {-v}$mot`) emite una llamada a `::tcl::annotlocal`, que lanza el error (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: un primer argumento anotado delega en `TclSetAnnotatedCmd` (`tclTypedVar.c`), el constructor.

#### El índice `$nombre(...)`: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: en el `(` que sigue a un nombre de variable, llama a **`TclParseVarIndex`**, que lee el índice hasta el `)` equilibrado: paréntesis anidados, etiquetas `"..."` emparejadas, sustituciones `$x`, `[cmd]`, `[(expr)]`, barras invertidas; llaves rechazadas. Si el índice no está bien formado según esta gramática, **recurre** exactamente a la regla de Tcl 9 (`info complete {a "c $dd("}` sigue siendo verdadero).
- Solo cuando se usa la sintaxis extendida, los componentes se envuelven en un token **`TCL_TOKEN_INDEX`** (1024, `tcl.h`): un índice válido en Tcl 9 conserva exactamente sus tokens. Las palabras desnudas no se ven afectadas: `puts a(b c)` sigue siendo dos palabras.
- `${nombre(...)}`: el nombre entero es literal; `Tcl_ObjGetVar2` e `INST_LOAD_STK` (vía `TclTypedVarGetElementByName`) leen el camino cuando la variable es un escalar tipado.
- `tclTest.c`: `testparser` muestra el token «index».

#### Bytecode

| instrucción | dónde | papel |
|---|---|---|
| `INST_INDEX_CONCAT` («indexConcat», 6 bytes: número `uint1` + máscara `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, llamado por `TclCompileVarSubst` y `TclPushVarName`), `tclExecute.c` | concatena las partes de un índice en un objeto **pathindex** que conserva los tramos sustituidos (`TclPathIndexNew`): un valor sustituido sigue siendo una hoja del camino |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (`dict for`, `dict map` compilados), `tclExecute.c` | olvida el tipo de una variable de bucle antes de la asignación |

Vía no compilada: `TclSubstTokens` (`SubstIndexTokens`) produce el mismo objeto pathindex.

#### Otros puntos de enganche del núcleo

| archivo | cambio |
|---|---|
| `tclBasic.c` | `TclTypesInit`: crea `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` toma la vía lenta para un valor anotado; `INST_FOREACH_STEP` olvida el tipo; lectura y escritura de elementos tipados (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` y `TclTypedVarAfterStore` alrededor de cada escritura: el contrato |
| `tclProc.c` | `TclTypedVarBindArg`: vinculación tipada de los argumentos |
| `tclCmdAH.c`, `tclDictObj.c` | variables de bucle (`TclTypedVarForgetByName`); `TclAnnotUnwrap` en las lecturas de dict |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | conversiones del núcleo sobre el contenido (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): un valor anotado nunca se convierte en su sitio |
| `tclListObj.c` | `lappend` por la `replaceProc` de una lista abstracta; `lindex` con varios índices sin aplanar |
| `tclCompExpr.c` | operador `is` (lexema `IS_TYPE`, `IsTypeAhead`), asignaciones `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. Los archivos añadidos

| archivo | líneas | contenido |
|---|---|---|
| `tclTypes.c` | 4 500 | tipología, `type def/alias/get/cast/convert/deref/binary`, validación, ampliación, casts, códecs C |
| `tclTypeParse.c` | 2 700 | analizador y compilador de las expresiones de tipos (modelo: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | tipos parametrizados, instancias, parámetros fantasma, refinamientos binarios |
| `tclAnnotObj.c` | 2 150 | el objeto anotado, el estado `{T, N}`, la resolución, `note` |
| `tclTypedVar.c` | 1 350 | variables tipadas, constructores (`TclSetAnnotatedCmd`, `BuildProductEx`, etiquetas de campo) |
| `tclTypePath.c` | 2 900 | lenguaje de caminos, lectura, escritura, construcción por camino, objeto pathindex |
| `tclVarRef.c` | 360 | enlaces `var` (nombre canónico, acceso, enlaces caducados) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | estructuras (`TclType`, `TclTypeField`, `TclBinLink`…) e interfaces |

Del lado Tcl: `layout/filebinary.tcl` (capa binaria: `type binary decode/encode/deref/space`, `chan binary`) y `layout/binaliases.tcl` (alias).

## Compatibilidad, diseño, límites

### 15. Compatibilidad y rendimiento

**Ningún script válido cambia de sentido.** Las dos sintaxis añadidas ocupan escrituras que eran errores: `{x}palabra` («extra characters after close-brace») y un índice `$nombre(...)` extendido, que recurre exactamente a la regla de Tcl 9 en cuanto no está bien formado. Verificaciones:

- pruebas de Tcl `parse`, `parseExpr`, `parseOld`, `subst` (incluidas 257 pruebas de `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: resultados idénticos a la base;
- equivalencia con un Tcl 9 original en 18 000 casos generados (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): ningún resultado válido cambiado;
- AddressSanitizer sin errores; memoria estable en 1 000 000 de construcciones.

**Los tipos no son enemigos de la velocidad.** Medidas sobre el ejemplo `cstruct` (10 000 medidas, `-align natural`):

| operación | capa Tcl | códec C | Tcl clásico escrito a mano, sin control |
|---|---|---|---|
| codificación (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format`; 330 µs medidos para 1 000) |
| decodificación (`type binary decode`) | 485 ms | 3,0 a 3,7 ms | 7,3 a 10,5 ms (`binary scan`) |

El códec C, que además **verifica** cada valor contra su tipo, es de 2 a 3 veces más rápido que el Tcl clásico escrito a mano, y de 130 a 250 veces más rápido que la capa Tcl genérica.

### 16. Decisiones de diseño

| decisión | razón |
|---|---|
| `{x}palabra` ocupa un error de sintaxis | compatibilidad total, ninguna regla de citado nueva |
| sin apilamiento pegado `{a}{b}$x` | ambiguo con `{int}{300}`; se apila anotando un valor anotado |
| solo un constructor resuelve un tipo | `{T}$x` fuera de un constructor no cuesta nada y nunca falla; la verificación ocurre donde se guarda el valor |
| anotaciones locales reconocidas por sus caracteres | un nombre de tipo no puede contener `-` ni `?`: ninguna colisión posible |
| `type def` nominal, `type alias` estructural | distinguir unidades (`mm`, `kg`) sin cambiar los valores |
| ampliación automática, estrechamiento explícito | ninguna pérdida de información silenciosa |
| conversiones explícitas entre nominales | `mm` → `m` exige `{m}$a` o `type cast` |
| forma nombre/valor desnuda suprimida | `set {png}p chunks $l` se leía de dos maneras; ahora los nombres se escriben en el fuente (`{chunks}…` o `p(chunks)`), nunca se adivinan en los datos |
| etiquetas de campo: todo o nada | una mezcla sería una forma posicional disfrazada |
| el camino no sustituye nada | un dato que contiene `[script]` nunca lo ejecuta el analizador de caminos (desaparece la doble sustitución); misma regla que un array |
| valores sustituidos atómicos | `$D($k)` lee la clave `$k`, aunque contenga `.` o `,` |
| camino + etiquetas no previsto | sin necesidad demostrada; el uso dirá |

### 17. Límites y puntos abiertos

**A corregir o decidir**

- **Adopción por una variable sin tipo.** `set e [lindex $L 1]` vuelve `e` tipada y estricta. De ahí la trampa `catch … m` (§ 6.2), y el error que se escapa es el mensaje original, lo que lo hace difícil de entender. A decidir: adopción silenciosa, adopción sin contrato, o mensaje explícito.
- **`;` y `,` mezclados sin llaves** (`{int}a; {int}b, {int}c`) se aceptan sin error y producen un campo anónimo; la regla de la especificación los rechaza.
- **Leer una variante que no es la actual** de una suma da `unknown way to cast the value from S to string` en lugar de un mensaje que nombre la variante.
- **Variantes de la misma forma**: sin etiqueta, se toma la primera sin aviso.
- `set {paire}s {aut.nom}x` (el campo de **otro** producto) da `wrong # args`; un mensaje propio sería más claro.
- `lset` con una lista de índices que pasa por `TclLsetList` guarda una referencia de más (fuga, sin bloqueo).

**Aún no hecho**

- Valor «inexistente» en general: `opt(T)` cubre los enlaces binarios, no hay forma general.
- `union`, campos de bits compartidos, lista «hasta el final del archivo» (el rechazo de `union` es deliberado).
- Anotación de comentario `{#}` a nivel del analizador, anotación de la primera palabra (`{c}proc`), anotaciones de declaración (`proc p {{?}{…} {!}arg}`).
- Notas por elemento de una lista tipada (hoy la nota se aplica a la lista entera); borrado de todas las notas con `{@}$x`.
- Códecs C para sumas, enlaces y tamaños calculados; caché del tipo analizado.
- Banco de pruebas C sobre maqueta (`typeparse/test/`) por actualizar; ejemplos `arbre_var.tcl` y `defauts_calcules.tcl` por corregir.

## Anexos

### A. Instalación y pruebas

El prototipo parte de Tcl `core-9-0-branch` con la rama del TIP 759. Desde la raíz del árbol Tcl (MSYS2 / MinGW64 o Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # extrae, copia las fuentes de tipos, aplica los parches del núcleo,
                                        # recompila y compara cada tests_tclsh/tNN_*.tcl con su .out
```

- Las fuentes de tipos se entregan completas en `typeparse/` y se copian en `generic/`; solo se aplican los parches del núcleo, listados en `noyau/ORDRE.txt`, sin fuzz (`-F0`), y un parche ya aplicado se detecta y se salta.
- Opciones de `maj.sh`: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "mensaje"`.
- Notas de cada parche: `noyau/LISEZMOI.md`. Ejemplos ejecutables: `exemples/`.

### B. Glosario

| término | sentido |
|---|---|
| anotación | texto entre llaves pegado delante de una palabra: `{x}palabra` |
| nota | anotación no resuelta, que sigue al valor |
| tipo | anotación resuelta en la tipología conocida |
| anotación local | anotación cuyo texto no puede ser un tipo (`{-v}`, `{is?}`); solo vive entre quien llama y el comando |
| constructor | lo que guarda un valor y resuelve sus anotaciones: `set {T}v`, vinculación de argumentos, `lassign`, `foreach`, asignación en `expr` |
| contrato | el tipo de una variable, verificado en cada escritura |
| adopción | una variable sin tipo toma el tipo del valor recibido |
| cast | conversión explícita a un tipo (`{T}$x` en un constructor, `type cast`) |
| nominal | tipo creado por `type def`, distinto de su definición; se borra ante su base |
| refinamiento | tipo de base restringido, con una representación binaria (`int(u,8)`) |
| ampliación | paso automático de un refinamiento a un tipo más amplio |
| producto | `struct`: campos con nombre y ordenados |
| suma | una variante entre varias, con nombre |
| variante | una de las ramas de una suma |
| etiqueta de campo | anotación que nombra el campo de un argumento: `{age}12` |
| camino | índice de un valor tipado: `R(A.(x,y))` |
| pathindex | objeto que lleva un índice y sus partes sustituidas |
| enlace | `var.T` (a una variable) o `*(…).T` (a una dirección binaria) |
| parámetro fantasma | parámetro de un tipo que no cambia los valores pero distingue las instancias |

### C. Equivalencias

| aquí | C | Rust | LLVM |
|---|---|---|---|
| producto | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| suma | (`union` + discriminante a mano) | `enum` con datos | — |
| enumeración (suma de literales) | `enum` | `enum` sin datos | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (sin signo) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | `char *` terminado en NUL | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 bits) | `*const T` | `ptr` |
| `opt(T)` | puntero que puede ser `NULL` | `Option<T>` | — |
| tipo nominal (`type def mm double`) | `typedef` (sin control) | newtype `struct Mm(f64)` | — |
| `-align natural` / packed | alineación por defecto / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | alineación / `<{ }>` |
