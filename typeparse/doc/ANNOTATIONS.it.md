# Annotazioni in Tcl — documentazione

*19 ottobre 2026 · versione v13 · Florent MERLET, con Claude (Anthropic)*

*Altre lingue: [Français](ANNOTATIONS.fr.md) (riferimento) · [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md)*

## Premessa

**Autori**: Florent MERLET, con Claude (Anthropic).

Questo documento descrive le **annotazioni** di Tcl 9.0 proposte dal TIP 764: un metadato legato a una parola (`{x}parola`) che non cambia il valore, ma permette di tipizzare le variabili, nominare i campi e descrivere formati binari. Sostituisce il manuale scritto man mano durante lo sviluppo e descrive il prototipo alla **versione v13**.

Tre parti, per tre lettori:

- **Parte I — Concetti**: che cos'è un'annotazione, la differenza tra una nota e un tipo, e le loro regole. Da leggere per prima.
- **Parte II — Riferimento**: i comandi `note` e `type`, il linguaggio dei tipi, i percorsi, la costruzione dei valori, i tipi binari e gli errori. Per chi scrive Tcl.
- **Parte III — Implementazione**: l'oggetto annotato, l'analizzatore, il bytecode, i file e le funzioni coinvolti. Per chi esamina il TIP o il codice.

Seguono compatibilità e prestazioni, scelte di progetto, limiti e appendici (installazione, glossario, corrispondenze con C, Rust e LLVM).

Gli esempi sono Tcl eseguibile sul prototipo; i commenti `;#` danno il risultato. I messaggi di errore sono citati come li produce Tcl. Gli identificatori degli esempi (`nom`, `fiche`, `couleur`, `grandeur`…) restano come nell'originale francese, perché corrispondano agli script di esempio.

## Parte I — Concetti

### 1. Le annotazioni

#### 1.1 Origine della parola

*Annotare* viene dal latino *ad notare*, «mettere una nota accanto». La parola *protocollo* dice la stessa cosa in un'altra forma: il *prōtokollon* greco era il primo foglio incollato in testa a un rotolo di papiro, che ne descriveva il contenuto senza farne parte. Un'annotazione Tcl è esattamente questo: un foglio incollato in testa a una parola.

```tcl
{mm}12          ;# il valore resta 12; «mm» è incollato davanti
{list.int}L     ;# il nome resta L; «list.int» dice che cosa conterrà
```

#### 1.2 Un metadato

Un'annotazione è un **metadato**:

- **non compare nel testo** del valore: `{mm}12` vale `12` per `puts`, `expr`, `string length`;
- **aggiunge un'informazione** che il valore da solo non porta: un'unità, un tipo, il nome di un campo, un'opzione;
- vive **fuori dal piano dei valori grezzi**: nessun comando Tcl esistente la vede, e nessuno script esistente cambia significato.

#### 1.3 Un'analogia: la parte immaginaria

Un numero complesso ha una parte reale e una parte immaginaria. Allo stesso modo, un valore annotato ha una parte **grezza** (il testo che Tcl ha sempre gestito) e una parte **annotata** (ciò che lo sviluppatore ha in mente).

| | numero complesso | valore annotato |
|---|---|---|
| parte visibile | la parte reale | il valore grezzo (`12`) |
| parte aggiunta | la parte immaginaria | l'annotazione (`mm`, `int`, `nom`) |
| ciò che si legge di solito | la parte reale | la stringa |

L'analogia si ferma al calcolo. In un complesso, la parte immaginaria partecipa all'aritmetica. Qui **un calcolo perde l'annotazione**: `expr {$x + 1}` restituisce un numero nudo. Solo la copia, `lindex`, `string cat` e il passaggio come argomento la conservano. È voluto: il piano dei valori grezzi resta Tcl ordinario, ed è questo che garantisce la compatibilità.

#### 1.4 Guidare l'esecuzione

L'annotazione rende esplicita la «parte immaginaria» dello sviluppatore, ciò che sa dei suoi dati e che Tcl ignorava:

- un **tipo** fa verificare ogni scrittura in una variabile (`set {int}n abc` fallisce);
- un **nome di campo** sceglie dove va un valore (`set {pt}a {age}12 {nom}Jean`);
- una **rappresentazione binaria** dice come leggere e scrivere i byte (`{int(u,16,be)}version`);
- una **nota** contrassegna un valore perché una procedura lo riconosca (`note has $v mm`).

Lo script conserva la sua forma: si aggiungono annotazioni dove si vogliono garanzie, e da nessun'altra parte.

### 2. Note, tipi e annotazioni locali

Un'annotazione si scrive sempre `{testo}parola`. A seconda di ciò che dice il testo e di **chi lo legge**, svolge tre ruoli.

#### 2.1 La nota: l'annotazione grezza

Una **nota** è un'annotazione che nessun costruttore ha ancora risolto. Segue il valore: copia, argomento di procedura, elemento di lista.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (vuoto: nessun tipo risolto)
```

**Impilamento.** Si annota un valore già annotato; le note si impilano, dalla più vecchia alla più recente.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px è la più recente)
```

La scrittura `{a}{b}$x` (due annotazioni incollate) non esiste: sarebbe ambigua con `{int}{300}`, un'annotazione su un letterale tra graffe. Si impila annotando un valore che già lo è.

#### 2.2 Il tipo: l'annotazione risolta da un costruttore

Un **tipo** è un'annotazione **risolta** nella tipologia nota all'interprete (tipi di base, `type def`, `type alias`). Solo un **costruttore di variabile** risolve:

- `set {T}v …` (e `set v {T}$x`);
- il legame degli argomenti di `proc`;
- `lassign`, `foreach`, e in `expr` `x = v` e `x is T = v`.

Posta sul **nome** di una variabile, un'annotazione dà alla variabile un **contratto**: ogni scrittura successiva viene verificata.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Fuori da un costruttore, `{T}$x` non verifica né converte nulla, anche se `T` è un tipo noto: `T` viene semplicemente **impilato**, come una nota. `list {int}abc` non dà errore.

#### 2.3 Due pile e una regola di spilamento

Un valore annotato porta **due liste**: il **tipo risolto** (al più uno) e la **pila delle note** in attesa. La visualizzazione le separa con `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 tipo risolto   note, dalla più vecchia (mm) alla più recente (hello)
```

Un costruttore **spila** dalla cima:

1. prende la nota più recente e cerca di risolverla;
2. continua finché le note si risolvono; la prima che non si risolve ferma tutto e resta sulla pila;
3. i tipi spilati devono essere coerenti: il più recente deve entrare in ciascuno degli altri, altrimenti `contradictory annotations px and qq`;
4. il valore viene convertito («castato») verso il tipo più recente;
5. il contratto della variabile fa l'ultimo controllo.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px si risolve, mm no
type get $z          ;# px      ; note get $z: mm
```

Una nota posta **prima** del suo `type def` viene quindi risolta dal costruttore successivo: scrivere più tardi `type def mm double` dà senso ai `{mm}` già posti.

**Prima il contesto.** Un costruttore risolve nel suo contesto prima dei tipi globali: per una somma, i nomi delle varianti (`{i}5` in `set {S}v {i}5`); per un prodotto, i nomi dei campi (`{age}12`). Un tipo globale con lo stesso nome non cambia nulla.

#### 2.4 Le annotazioni locali

Un'annotazione il cui testo contiene un carattere che i nomi di tipo rifiutano (`-`, `?`, `:`…) non può **mai** essere un tipo. È un'**annotazione locale**: vive solo tra chi chiama e il comando chiamato, e non segue il valore.

```tcl
type binary {sizeof?}$T          ;# una domanda rivolta a type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# rifiutato: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# rifiutato: local annotation {-v} cannot declare a variable
```

Due famiglie naturali, senza rischio di collisione con un nome di tipo: le opzioni in `-nome` e le domande in `nome?`. Un nome di tipo contiene solo lettere ASCII, cifre, `_` e byte non ASCII, e non comincia con una cifra.

#### 2.5 L'annotazione vuota `{}`

`{}` è «il tipo vuoto». Il suo effetto dipende dal punto in cui si trova:

| dove | effetto |
|---|---|
| su un valore, in un costruttore (`set v {}$x`) | toglie il tipo risolto, conserva le note |
| su un nome (`set {}v $x`) | toglie il contratto della variabile; il valore è salvato con le sue note, senza risolvere nulla |
| su un valore, fuori da un costruttore | il lettore vede il contenuto nudo (tipo e note cancellati) |

```tcl
set {int}n 5
set {}n abc          ;# il contratto è tolto: n vale abc
```

Il valore originale non viene mai modificato: `{}` produce una nuova vista, il che rende innocua la condivisione degli oggetti.

## Parte II — Riferimento

### 3. Il comando `note`

```
note get VALORE              ;# le note, dalla più vecchia alla più recente, unite da «.»
note has VALORE NOME         ;# 1 se NOME è una delle note, altrimenti 0
note delete VALORE ?NOME?    ;# il valore senza la nota NOME, o senza alcuna nota
```

I valori Tcl sono immutabili: `note delete` **restituisce** un nuovo valore, non modifica una variabile.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: niente impilamento incollato
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Uso tipico: una procedura che reagisce al contrassegno di un valore.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. Il comando `type`

```
type def NOME DEFINIZIONE      ;# definisce un tipo nominale (o una famiglia parametrizzata)
type alias NOME DEFINIZIONE    ;# dà un nome strutturale a una definizione
type get VALORE                ;# il tipo risolto di un valore (vuoto se non c'è)
type cast TIPO VALORE          ;# il risolutore di un costruttore, senza variabile
type convert DA A LAMBDA       ;# dichiara una conversione esplicita
type deref VALORE              ;# istantanea: ogni collegamento var sostituito dal suo valore
type binary TIPO ...           ;# rappresentazione binaria, codifica, decodifica (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (un alias non è un tipo nuovo)
type cast pt {Léa 7}          ;# Léa 7, di tipo pt
```

In `expr`, l'operatore **`is`** verifica il tipo di una variabile, e un'assegnazione può dichiarare:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# dichiara z di tipo pt e gli assegna il valore
expr {new is cmd}              ;# 1 se new è un comando; new(…) diventa una funzione di expr
```

`is cmd` non è un tipo: verifica che un comando esista e lo espone come funzione di `expr`.

### 5. Il sistema dei tipi

#### 5.1 Tipi di base

| tipo | valori |
|---|---|
| `int` | intero (senza limiti, come in Tcl) |
| `double` | virgola mobile |
| `string` | qualsiasi stringa |
| `list` | lista Tcl ordinaria (elementi non tipizzati) |
| `dict` | dizionario Tcl ordinario |
| `var` | collegamento a una variabile, tramite il suo nome (§ 5.3) |
| `any` | elemento di una lista eterogenea (`list.any`): ogni elemento conserva il suo tipo |

I **raffinamenti** restringono un tipo di base e portano una rappresentazione binaria: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Un `type def` crea un tipo **nominale**: `type def port int(u,16)` è distinto da `int(u,16)`. Un `type alias` dà soltanto un nome a una definizione.

#### 5.2 Il linguaggio di composizione

Un tipo si scrive come un'espressione, analizzata sul modello di `expr` (`tclTypeParse.c` riprende la struttura di `tclCompExpr.c`).

**Tre tipi di operandi:**

| operando | dove | esempio |
|---|---|---|
| tipo | parola nuda, a sinistra di una `}` o in posizione di operando | `int`, `list`, `pt` |
| nome | parola attaccata a destra di una `}` | `{int}age`: il campo `age` |
| valore | tra virgolette, tra graffe, o dopo `=` | `"MZ"`, `= 0` |

**Operatori**, dal più forte al più debole:

| operatore | ruolo | esempio |
|---|---|---|
| `.` | annidamento (associativo a destra) | `list.dict.int` |
| `( )` | argomenti, attaccati al nome | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` o `?=` | valore predefinito di un campo | `{int}age ?= 0` |
| `;` | prodotto | `{double}x; {double}y` |
| `,` | somma | `{int}i, {string}s` |
| `{ }nome` | raggruppamento, con un nome attaccato alla graffa di chiusura | `{{int}x; {int}y}A` |

`;` e `,` hanno la stessa precedenza: per mescolare un prodotto e una somma si raggruppa con le graffe (`{int}a; {{int}b, {string}c}u`).

**Caratteri.** Un nome di tipo contiene solo lettere ASCII, cifre, `_` e byte non ASCII, e non comincia con una cifra. La grammatica usa in più `{ } , ; . = " # ( ) * $`. Ogni altro segno ASCII (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) è rifiutato: `invalid character "!"`, codice `TCL PARSE TYPE BADCHAR`, tranne dopo `=` (un valore predefinito) e tra le parentesi di un costruttore (`*(file://x, …)`). È questo rifiuto che rende sicure le annotazioni locali (`{-v}`, `{is?}`).

Una definizione è **dichiarativa**: alla lettura non si fa alcuna sostituzione, tranne in un valore predefinito, valutato a ogni costruzione.

#### 5.3 Campi e contenitori

**Enumerazione** — una somma di letterali.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# un solo letterale: una costante (firma di file)
```

**Somma** — una sola variante alla volta, che porta il suo nome.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# la variante scelta dal nome del tipo
```

**Prodotto** — una `struct`: campi con nome, ordinati, ciascuno del suo tipo.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# posizionale
set {pt}b {age}12 {nom}Jean    ;# etichette di campo (§ 8)
$a(nom)                        ;# Jean
```

**Lista** — `list` (estensibile), `list.T` (elementi di tipo T), `list(N).T` (esattamente N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

Gli elementi di una `list.T` sono atomici: `lindex`, `foreach`, `$L(i)` restituiscono elementi tipizzati, mai più spezzati in parole. La lista nuda `{list}` conserva il comportamento di Tcl.

**Dizionario** — `dict` (qualsiasi chiave), `dict.T` (valori di tipo T), `dict(chiavi).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom obbligatorio; age facoltativo, 0 di default
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z può mancare; `dict()`: solo la chiave vuota. Le chiavi sono sempre testo; solo i valori sono tipizzati.

**Collegamento `var`** — il NOME di una variabile, seguito a ogni lettura.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (un'istantanea, di tipo list.int)
```

**Parametro di tipo `{type}T`** — in un tipo parametrizzato, un parametro che riceve un tipo (§ 5.5).

**Collegamento binario `*(origine,repr).T`** — un indirizzo in dati binari, seguito fino a un valore T (§ 9.5).

#### 5.4 Valori predefiniti

`?=` (o `=`, la scrittura più vecchia, ancora accettata in un prodotto) dà un valore predefinito a un campo. Può essere un letterale, uno `[script]` valutato a ogni costruzione, o una `$variabile` letta a ogni costruzione.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Un campo senza valore predefinito va fornito: `field "x" of pt has no value and no default`.

#### 5.5 Tipi parametrizzati

Il nome di un `type def` può portare parametri, dichiarati come campi: `{type}T` (un tipo), `{int}n`, `{double}f`, `{string}s`. Nel corpo si usano con `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# ricorsivo
```

#### 5.6 Parametri fantasma: le unità fisiche

Un parametro assente dal corpo è permesso: non cambia i valori, ma distingue le istanze, che sono tipi nominali diversi. Una famiglia di grandezze dà così unità il cui valore resta un `double`:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# regola non applicabile
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (conversione esplicita)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Una regola `type convert` può riguardare un tipo o una **famiglia** (il nome di un `type def` parametrizzato); la lambda riceve allora i parametri delle istanze (`$de(f)`). `return -code continue` significa «non applicabile». Le conversioni restano esplicite.

#### 5.7 Alias forniti

`layout/binaliases.tcl` definisce, per i formati binari (convenzioni di Rust e di Zig):

| alias | definizione |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| suffissi `be`, `le` (`u16be`, `i32le`) | ordine dei byte fissato |
| `f32 f64` (e `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Variabili tipizzate e vita di un valore annotato

#### 6.1 Il contratto di una variabile tipizzata

Una variabile dichiarata con `set {T}v …` ha un **contratto**: ogni scrittura viene verificata, che venga da `set`, `lappend`, `lset`, `incr`, `dict set`, da una scrittura per percorso (`set v(x) …`) o da `expr {v = …}`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Allargamento automatico.** Un raffinamento entra dove ci si aspetta un tipo più ampio: `int(u,8)` in `int`, `int(s,16)` o `double`; `double(32)` in `double(64)`. Il restringimento è rifiutato (richiede un cast esplicito), così come tra `bitarray(…,lsb0)` e `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# accettato: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominali.** Un tipo nominale si cancella davanti alla sua base (`px` entra in `int` e dà `int`, senza traccia). Due nominali senza legame di base non sono mai equivalenti (`mm` e `kg`): è proprio il loro ruolo.

**Cast espliciti.** `set v {T}$x` o `type cast T $x` convertono secondo, nell'ordine: una regola `type convert` specifica; la catena delle basi; la variante di una somma; una regola di famiglia; le conversioni integrate tra scalari. Altrimenti: `unknown way to cast the value from A to B`.

**Togliere il contratto.** `set {}v valore` (§ 2.5).

**Verificare.** `expr {v is T}`: vero se la variabile è tipizzata e il suo tipo entra in `T`; per una variabile non tipizzata, vero se il suo valore è valido per `T`.

#### 6.2 Che cosa conserva l'annotazione, che cosa la perde

| operazione | tipo | note |
|---|---|---|
| copia (`set a $x`), argomento, valore di ritorno | conservato | conservate |
| `lindex`, `foreach`, `$L(i)` su una `list.T` | elemento di tipo `T` | — |
| `string cat $x` (un solo argomento) | conservato | conservate |
| calcolo (`expr`, `string toupper`, `dict set` su un valore) | **perso** | **perse** |
| cast `{T}$x` in un costruttore | cambiato | conservate |
| `set {T}a $x`, variabile tipizzata | `T` | cancellate dall'allargamento |

**Adozione.** Una variabile **non tipizzata** che riceve un valore tipizzato ne **adotta** il tipo, e quindi il contratto:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e diventa una variabile int
set e hello                    ;# expected integer but got "hello"
```

È il comportamento più sorprendente del modello. La trappola classica è un `catch … m` riutilizzato: se un primo `catch` salva in `m` un valore tipizzato, un secondo `catch` non può più salvarci il suo messaggio di errore. Usare una variabile per ogni uso, `unset` tra due usi, oppure `set {}m …`. (Punto aperto, § 17.)

**Procedure.** Un argomento che riceve un valore tipizzato è tipizzato e rigoroso nella procedura; un argomento ordinario non cambia.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Variabili di ciclo.** `foreach`, `lmap`, `dict for`, `dict map` (e il filtro di `dict filter`) dimenticano il tipo della loro variabile prima di ogni assegnazione; dopo il ciclo, la variabile conserva il tipo dell'ultimo elemento ricevuto.

#### 6.3 `type deref`: un'istantanea dei collegamenti

`type deref V` restituisce `V` con ogni collegamento `var` sostituito dal valore che la variabile ha in quell'istante. Il risultato ha il tipo «senza collegamenti» (`list.var.int` → `list.int`), chiamato `deref(<tipo>)` per un prodotto o una somma. Si segue un solo livello per chiamata; una variabile mancante è un errore che nomina la casella (`type deref: at 2: can't read "::C": no such variable`), senza risultato parziale. Funziona in liste, dict, prodotti, somme e tipi ricorsivi; un ciclo non gira mai all'infinito.

### 7. Accedere alle parti di un valore: i percorsi

#### 7.1 La forma tra parentesi

Un valore tipizzato si legge e si scrive con la sintassi degli array, ma l'indice è un **percorso**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# costruzione (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# scrittura di un campo intero
set R((A,B).x) 10 30                       ;# scrittura multipla
${R(A.x, B.y)}                             ;# lettura, spazi permessi
```

Gli spazi sono permessi in `$R(…)`, `${R(…)}` e in una parola annotata `{T}R(…)`. In una parola nuda non annotata (`set R(A.x, A.y) …`), Tcl spezza sugli spazi come sempre: scrivere senza spazi o tra virgolette.

#### 7.2 Il linguaggio dei percorsi

Il linguaggio è quello della pagina del wiki di Tcl «A better way to do indexation», analizzato sul modello di `expr` (`tclTypePath.c`).

**Operandi:**

| operando | significato |
|---|---|
| `nome` (parola nuda) | campo, chiave di dict, nome di variante o indice di lista (`3`, `end`, `end-1`): decide il tipo del contenitore |
| `'chiave'` | sempre una chiave (`'end'` è la chiave «end», non un indice) |
| `"etichetta"` | testo letterale copiato nel risultato |

**Operatori**, dal più debole al più forte:

| operatore | ruolo | esempio |
|---|---|---|
| `,` | raccolta (un gruppo) | `A,B` |
| `*` (unario) | dispiegamento: il gruppo si fonde nel suo genitore | `*0:2,4` |
| `.` | annidamento; si distribuisce su un gruppo | `A.(x,y)` = `A.x,A.y` |
| `:` | intervallo di lista | `0:end-1` |
| `( )` | raggruppamento | `(A,B).x` |

**Forma del risultato.** Una lettura multipla restituisce liste annidate secondo i gruppi: `$R((A,B).(x,y))` dà `{1 2} {3 4}`; `$L(0:2,4)` dà `{10 20 30} 50`; `$L(*0:2,4)` dà `10 20 30 50`.

**Il percorso non sostituisce nulla.** È Tcl che sostituisce `$x`, `[cmd]` e `[(expr)]` in `$R(…)` e nelle parole nude; un valore sostituito è **una foglia**, mai riletto come percorso:

```tcl
set k file.txt ; $D($k)        ;# la chiave «file.txt» (non D.file.txt)
set n 1        ; $D(f_$n)      ;# attaccato a un testo, il valore prolunga il nome
set idx {0 2}  ; $L(*$idx)     ;# * dispiega un valore lista: 10 30
set p {[exec ls]} ; $L($p)     ;# non viene eseguito nulla: bad index "[exec ls]"
```

Tra graffe e in `${…}`, `$` e `[` sono errori: `a path does not substitute "$"…`. Un percorso calcolato si legge **nominando** la variabile con esso: `set ref R($chemin) ; set $ref`.

#### 7.3 Contenitore per contenitore

| contenitore | lettura | scrittura |
|---|---|---|
| prodotto | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (riverificato) |
| somma | `$v(i)`: la variante corrente | `set v(i) 6` |
| lista | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (un bit), `$b(1:4)` (una fetta: un intero `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| enumerazione | (scalare: nessun percorso) | — |

Leggere e poi riscrivere lo stesso percorso funziona sempre: `set R(c) [set R(c)]`.

### 8. Costruire un valore

#### 8.1 Le forme di un costruttore di prodotto

Un prodotto di *n* campi si costruisce in quattro modi, senza indovinare nulla:

| forma | esempio | regola |
|---|---|---|
| posizionale | `set {pt}a Jean 12` | esattamente *n* valori, nell'ordine dei campi |
| etichette di campo | `set {pt}a {age}12 {nom}Jean` | tutti i valori etichettati; ordine libero; i campi omessi prendono il loro valore predefinito |
| un solo campo | `set {pt}a {pt.nom}Jean` | gli altri campi prendono il loro valore predefinito |
| percorso + valori posizionali | `set {pt}a(age,nom) 12 Jean` | un valore per foglia del percorso (o per gruppo, § 8.3) |

Si aggiungono **nessun argomento** (`set {pt}a`, tutti i campi al loro valore predefinito) e **un prodotto dello stesso tipo** (copia). Tutto il resto è un errore:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

La forma nome/valore nuda (`set {pt}a nom Jean age 12`) **non esiste più**: si confondeva con quella posizionale non appena *n* argomenti valevano per *n* campi (§ 16).

#### 8.2 Le etichette di campo

L'etichetta è l'annotazione **più recente** di ogni argomento, letta nel contesto del prodotto: un nome di campo prevale su un tipo globale con lo stesso nome.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# non cambia nulla
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# conta un'etichetta portata da una variabile
```

**Tutto o niente:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

Anche un valore tra graffe si può etichettare: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Percorso e valori posizionali

`set {T}v(percorso) v1 v2 …` costruisce il prodotto (e i suoi prodotti annidati); i campi nominati ricevono i valori, gli altri il loro valore predefinito. Il **numero** dei valori sceglie la ripartizione, mai il loro contenuto:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# un valore per foglia
set {rect}R((A,B).(x,y))     1 2 3 4      ;# idem
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# un valore per elemento del gruppo di testa
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# un valore per tutto il percorso
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# con dispiegamento: un valore, piatto
```

Ogni valore ha la forma che restituirebbe la sua lettura; i tre numeri coincidono solo se coincidono le letture.

**La combinazione percorso + etichette non esiste**: `set {rect}R(A) {y}2 {x}1` è un errore (`1 targets but 2 values`). Non è prevista finché nessun uso la richiede.

#### 8.4 Somme e liste

Una somma si costruisce con la sua variante come etichetta, o con il nome della variante nel tipo:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# ogni elemento conserva la sua variante
```

Senza etichetta, la variante è dedotta dal valore, nell'ordine delle varianti: `set {S}u 5` dà `S.i`. Quando più varianti hanno la stessa forma (`{string}a, {string}b`), la prima va sempre bene: per scegliere, etichettare (`{b}y`).

Una lista riceve i suoi elementi uno per uno: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Un solo argomento è **un** elemento.

### 9. Tipi binari

Un tipo dice, da sé, come si scrive in byte: non c'è altra sintassi da imparare. La lettura e la scrittura le fa `layout/filebinary.tcl` (caricato con `source`), sopra il nucleo, con una via rapida in C per le disposizioni fisse (§ 9.6).

#### 9.1 `bitarray`: un intero di cui si leggono i bit

`bitarray(n, ordine)` è un intero di n bit (0 ≤ v < 2^n). L'ordine dice da dove parte l'indice 0: `lsb0` (predefinito, il bit di destra, come `1 << i`: registri, maschere) o `msb0` (il bit di sinistra, come negli schemi delle RFC). Sono due tipi distinti.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (un intero bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# in expr, una fetta tra virgolette
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Rappresentazioni

| tipo | rappresentazione |
|---|---|
| `int(u,n)` / `int(s,n)` | intero senza segno / con segno di n bit (multiplo di 8, da 8 a 1024) |
| `double(32)`, `double(64)` | virgola mobile IEEE |
| `string(raw,N)` | N byte grezzi |
| `string(ENC,N)` | testo su N byte, completato con NUL |
| `string(ENC,z)` | testo terminato da NUL (2 byte in utf-16, ucs-2) |
| `bitarray(n)` | n bit (multiplo di 8), letti come intero senza segno |
| `"MZ"` (somma di letterali di uguale lunghezza) | una costante, verificata |
| `list(N).T`, un prodotto di questi tipi | N elementi; l'annidamento è permesso |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un testo che non si codifica senza perdita (`é` in ascii) o che non sta in N byte è rifiutato alla costruzione.

**Ordine dei byte.** Ultimo argomento: `be` o `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Senza di esso vale quello della chiamata (`-endian`, little di default). Non cambia i valori: `int(u,16,be)` e `int(u,16,le)` si assegnano l'uno all'altro.

**Allineamento.** Di default i prodotti sono compatti (packed). `-align natural` (o `-align N`) aggiunge il riempimento del C: ogni campo comincia a un multiplo del suo allineamento, la dimensione di un prodotto è arrotondata. Riempimento a zero in scrittura, ignorato in lettura salvo con `-strictpad 1`.

#### 9.3 Comandi

```
type binary TIPO                                   ;# la rappresentazione, vista dal nucleo
type binary {what?}TIPO                            ;# idem
type binary {sizeof?}TIPO ?-align A?               ;# dimensione in byte (errore se dipende dai dati)
type binary {is?}TIPO BYTE ?-exact 1?              ;# 1 se BYTE si decodifica come TIPO, altrimenti 0
type binary decode TIPO BYTE ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TIPO VALORE ?-endian E? ?-align A? ?-dedupe 1?
type binary deref COLLEGAMENTO ?BYTE?              ;# segue un collegamento non seguito, di un passo
type binary space file|memory|list|get|delete …    ;# spazi con nome
chan binary read  CANALE TIPO ?opzioni?            ;# il canale avanza
chan binary write CANALE TIPO VALORE ?opzioni?
```

Le domande (`{what?}`, `{sizeof?}`, `{is?}`) sono **annotazioni locali** sull'argomento di tipo; le azioni sono verbi. `file binary` resta un alias del vecchio nome.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Livello 2: ciò che il file dice di sé

Una dimensione, una quantità, una posizione o una variante si possono leggere nel file: `$campo` negli argomenti del tipo (un campo **già letto** del prodotto corrente o di un prodotto che lo contiene), oppure un tipo intero senza segno che fa da **prefisso**.

| scrittura | significato |
|---|---|
| `string(utf-8,int(u,16,be))` | testo preceduto dalla sua dimensione in byte (stringa Pascal) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N byte, con N calcolato da campi letti |
| `list(int(u,8)).T`, `list($n).T` | lista preceduta dal suo numero di elementi, o di n elementi |
| `at($off).T` | il valore si trova alla posizione `$off` (il cursore non si sposta) |
| `corps($tag)` | la somma `corps`, la cui variante è scelta dal campo `tag` |
| `U(tp)`, `R($v)` (parametro legato) | una somma la cui variante è fissata alla definizione |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nomi delle varianti = valori dell'etichetta
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Già alla costruzione si verifica una lista la cui dimensione dipende da un campo: `set {Batch}b(n,items) 3 {{1 1.0 0}}` è rifiutato (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Collegamenti: `*(origine, repr, opzioni).T`

Un campo contiene spesso un indirizzo, non il valore. Il tipo collegamento legge l'indirizzo (`repr`, un intero) e poi il valore `T` che vi si trova.

| origine | l'indirizzo parte da |
|---|---|
| `//` (o `file://`) | l'inizio dei dati |
| `./` | l'inizio della struttura che lo contiene |
| `../` | l'inizio della sua genitrice |
| `.` | il campo stesso |
| `//nome/`, `memory://nome/` | uno spazio con nome (file, o segmenti di memoria) |

Opzioni: `null` (l'indirizzo 0 significa «nessuna destinazione»), `null=V`. Con `opt(T)`, un indirizzo nullo dà `none`, altrimenti `some`: è la differenza tra «nullo» e «inesistente». In scrittura, le destinazioni sono aggiunte in fondo, allineate, e gli indirizzi calcolati; `-dedupe 1` scrive una sola volta le destinazioni identiche. Un campo collegamento uguale al suo valore predefinito si scrive come collegamento nullo.

**`-linkdepth`** regola il numero di collegamenti seguiti (`Inf` di default). Un collegamento non seguito è un intero annotato (la posizione assoluta della destinazione); `type binary deref` lo segue di un passo. Così si legge solo l'intestazione di un file grande, si percorre una lista concatenata e si riscrive un campo sul posto.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: la destinazione
```

#### 9.6 Codec C

`type binary encode` e `decode` provano prima una via in C, che copre le disposizioni fisse: interi di 1, 2, 4, 8 byte, numeri in virgola mobile di 4 e 8 byte, prodotti (con `-align`), `list(N).T` e `list($n).T`. Fuori da questo dominio (stringhe, somme, collegamenti, `at`, campi di bit, dimensioni calcolate, `-dedupe`), o in caso di errore, subentra lo strato Tcl, che dà i suoi messaggi. `set ::tcl_typebinaryc 0` disattiva la via C. I byte e i valori prodotti sono identici nelle due vie (test `t51_codec_c`). Misure: § 15.

### 10. Errori

I messaggi seguono l'uso di Tcl: nominano il valore, il tipo o il campo in questione. I codici `errorCode` propri dei tipi cominciano con `TCL TYPE` o `TCL PARSE TYPE`.

| situazione | messaggio (esempio) | `errorCode` |
|---|---|---|
| carattere vietato in un tipo | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| nome di tipo sconosciuto su un nome di variabile | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| annotazione locale su un nome di variabile | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| annotazione locale salvata da `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| valore non valido per il tipo | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (secondo il tipo di base) |
| nessuna variante va bene | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| tipi nominali diversi | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| nessuna conversione nota | `unknown way to cast the value from mm to kg` | — |
| annotazioni impilate incoerenti | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| campo sconosciuto | `unknown field "zz" in pt`; `pt has no field "zz"` (percorso) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| campo senza valore né valore predefinito | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| numero di valori sbagliato | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| etichette e posizioni mescolate | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| campo duplicato | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| dimensione dipendente non rispettata | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| chiave di dict mancante o non ammessa | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| arietà di lista | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| sostituzione in un percorso letterale | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| indice di lista non valido in un percorso | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| collegamento a una variabile scomparsa | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` su un collegamento rotto | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| valore annotato creato fuori dall'analizzatore | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| strato binario non caricato | `the binary layer is not loaded` | — |

## Parte III — Implementazione

### 11. Panoramica

L'implementazione sta in **sette file nuovi** (circa 15 000 righe di C), copiati in `generic/`, e in **punti di aggancio** in una ventina di file del nucleo. I file nuovi non vengono mai patchati: sono consegnati interi; solo i file del nucleo ricevono patch.

### 12. L'oggetto annotato

#### Struttura

Un valore annotato è un `Tcl_Obj` ordinario il cui tipo interno è `annotated` (`annotatedObjType`, in `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   la stringa: quella del contenuto V (calcolata │
 │                  su richiesta da UpdateStringOfAnnotated)      │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   il contenuto: un Tcl_Obj ordinario, condiviso   │
 │   ptr2 ──► T   l'annotazione, in una di queste forme:          │
 │                · un riferimento di tipo risolto (typeRef)       │
 │                · il testo grezzo dell'annotazione (non ancora   │
 │                  risolta: una nota, o un tipo da risolvere)     │
 │                · ""   il marcatore dell'annotazione vuota {}    │
 │                · uno stato «annotstate» {T, N}: T risolto       │
 │                  (o NULL) e N, la lista delle note, dalla più   │
 │                  vecchia alla più recente                       │
 └───────────────────────────────────────────────────────────────┘
```

#### Proprietà

- **Trasparente**: la stringa è quella di V. Per ogni comando che non conosce le annotazioni, il valore è V.
- **Immutabile e condiviso**: lo stato `{T, N}` è a sua volta un oggetto immutabile; `Tcl_DuplicateObj` lo condivide. Annotare, spilare o togliere una nota produce un oggetto nuovo.
- **Senza costo senza note**: finché non ci sono note, `ptr2` è solo T.
- **Lista astratta** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… passano per le procedure di lista dell'oggetto (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). Gli elementi di una `list.T` sono restituiti tipizzati; uno scalare annotato è una lista di un elemento che è sé stesso.

#### Routine del `Tcl_ObjType`

| routine | ruolo |
|---|---|
| `FreeAnnotated` | libera V e T (e la cella «sé stesso» di uno scalare, tabella `SelfCell`) |
| `DupAnnotated` | copia V, condivide T o lo stato `{T, N}` |
| `UpdateStringOfAnnotated` | la stringa di V |
| `SetAnnotatedFromAny` | **rifiuta**: un valore annotato nasce dall'analizzatore o da un'assegnazione tipizzata, mai da una conversione (`TCL VALUE ANNOTATED`) |

#### Funzioni principali (`tclAnnotObj.c`)

`TclAnnotateValue` (porre un'annotazione: cast o nota), `TclAnnotSettle` (lo spilamento di un costruttore, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (tipo, variante, sconosciuto, locale, non valido; il contesto prima dei tipi globali), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (il contenuto nudo, per le conversioni del nucleo), `TclAnnotDisplay` (`list<int>@mm.hello`), e i comandi `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. Nell'analizzatore e nel compilatore

#### Il prefisso `{x}parola`

Nel Tcl 9 originale, una parola tra graffe seguita da altri caratteri è un errore di sintassi (`extra characters after close-brace`), tranne `{*}`. Il prefisso di annotazione occupa dunque una scrittura **che non aveva senso**: nessuno script valido cambia significato.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: una parola `{testo}resto` produce un token **`TCL_TOKEN_ANNOT`** (512, `tcl.h`) che porta il testo dell'annotazione, seguito dai token del resto della parola. Una parola annotata `{T}nome(...)` arriva fino alla parentesi bilanciata, spazi compresi, su una riga.
- `tclCompile.c`, `TclCompileTokens`: se il resto della parola è letterale (`TclAnnotSuffixIsLiteral`), la parola si compila in `::tcl::annotraw testo parola` (una nota grezza, senza sostituzione); altrimenti in `::tcl::annotate testo valore`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: un `set` il cui nome è annotato passa per `TclSetAnnotatedCmd`; un'annotazione locale scritta sul valore (`set x {-v}$mot`) emette una chiamata a `::tcl::annotlocal`, che solleva l'errore (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: un primo argomento annotato delega a `TclSetAnnotatedCmd` (`tclTypedVar.c`), il costruttore.

#### L'indice `$nome(...)`: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: sulla `(` che segue un nome di variabile, chiama **`TclParseVarIndex`**, che legge l'indice fino alla `)` bilanciata: parentesi annidate, etichette `"..."` appaiate, sostituzioni `$x`, `[cmd]`, `[(expr)]`, barre rovesciate; graffe rifiutate. Se l'indice non è ben formato secondo questa grammatica, si **ricade** esattamente sulla regola di Tcl 9 (`info complete {a "c $dd("}` resta vero).
- Solo quando si usa la sintassi estesa i componenti sono avvolti in un token **`TCL_TOKEN_INDEX`** (1024, `tcl.h`): un indice valido in Tcl 9 conserva esattamente i suoi token. Le parole nude non sono toccate: `puts a(b c)` resta due parole.
- `${nome(...)}`: l'intero nome è letterale; `Tcl_ObjGetVar2` e `INST_LOAD_STK` (tramite `TclTypedVarGetElementByName`) leggono il percorso quando la variabile è uno scalare tipizzato.
- `tclTest.c`: `testparser` mostra il token «index».

#### Bytecode

| istruzione | dove | ruolo |
|---|---|---|
| `INST_INDEX_CONCAT` («indexConcat», 6 byte: numero `uint1` + maschera `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, chiamato da `TclCompileVarSubst` e `TclPushVarName`), `tclExecute.c` | concatena le parti di un indice in un oggetto **pathindex** che conserva i tratti sostituiti (`TclPathIndexNew`): un valore sostituito resta una foglia del percorso |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (`dict for`, `dict map` compilati), `tclExecute.c` | dimentica il tipo di una variabile di ciclo prima dell'assegnazione |

Via non compilata: `TclSubstTokens` (`SubstIndexTokens`) produce lo stesso oggetto pathindex.

#### Altri punti di aggancio del nucleo

| file | modifica |
|---|---|
| `tclBasic.c` | `TclTypesInit`: crea `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` prende la via lenta per un valore annotato; `INST_FOREACH_STEP` dimentica il tipo; lettura e scrittura di elementi tipizzati (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` e `TclTypedVarAfterStore` attorno a ogni scrittura: il contratto |
| `tclProc.c` | `TclTypedVarBindArg`: legame tipizzato degli argomenti |
| `tclCmdAH.c`, `tclDictObj.c` | variabili di ciclo (`TclTypedVarForgetByName`); `TclAnnotUnwrap` nelle letture di dict |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | conversioni del nucleo sul contenuto (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): un valore annotato non viene mai convertito sul posto |
| `tclListObj.c` | `lappend` tramite la `replaceProc` di una lista astratta; `lindex` con più indici senza appiattire |
| `tclCompExpr.c` | operatore `is` (lessema `IS_TYPE`, `IsTypeAhead`), assegnazioni `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. I file aggiunti

| file | righe | contenuto |
|---|---|---|
| `tclTypes.c` | 4 500 | tipologia, `type def/alias/get/cast/convert/deref/binary`, validazione, allargamento, cast, codec C |
| `tclTypeParse.c` | 2 700 | analizzatore e compilatore delle espressioni di tipo (modello: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | tipi parametrizzati, istanze, parametri fantasma, raffinamenti binari |
| `tclAnnotObj.c` | 2 150 | l'oggetto annotato, lo stato `{T, N}`, la risoluzione, `note` |
| `tclTypedVar.c` | 1 350 | variabili tipizzate, costruttori (`TclSetAnnotatedCmd`, `BuildProductEx`, etichette di campo) |
| `tclTypePath.c` | 2 900 | linguaggio dei percorsi, lettura, scrittura, costruzione per percorso, oggetto pathindex |
| `tclVarRef.c` | 360 | collegamenti `var` (nome canonico, accesso, collegamenti scaduti) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | strutture (`TclType`, `TclTypeField`, `TclBinLink`…) e interfacce |

Lato Tcl: `layout/filebinary.tcl` (strato binario: `type binary decode/encode/deref/space`, `chan binary`) e `layout/binaliases.tcl` (alias).

## Compatibilità, progetto, limiti

### 15. Compatibilità e prestazioni

**Nessuno script valido cambia significato.** Le due sintassi aggiunte occupano scritture che erano errori: `{x}parola` («extra characters after close-brace») e un indice `$nome(...)` esteso, che ricade esattamente sulla regola di Tcl 9 appena non è ben formato. Verifiche:

- test di Tcl `parse`, `parseExpr`, `parseOld`, `subst` (compresi 257 test di `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: risultati identici alla base;
- equivalenza con un Tcl 9 originale su 18 000 casi generati (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): nessun risultato valido cambiato;
- AddressSanitizer senza errori; memoria stabile su 1 000 000 di costruzioni.

**I tipi non sono nemici della velocità.** Misure sull'esempio `cstruct` (10 000 misure, `-align natural`):

| operazione | strato Tcl | codec C | Tcl classico scritto a mano, senza controllo |
|---|---|---|---|
| codifica (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format`; 330 µs misurati per 1 000) |
| decodifica (`type binary decode`) | 485 ms | da 3,0 a 3,7 ms | da 7,3 a 10,5 ms (`binary scan`) |

Il codec C, che in più **verifica** ogni valore rispetto al suo tipo, è da 2 a 3 volte più veloce del Tcl classico scritto a mano, e da 130 a 250 volte più veloce dello strato Tcl generico.

### 16. Scelte di progetto

| scelta | motivo |
|---|---|
| `{x}parola` occupa un errore di sintassi | compatibilità totale, nessuna nuova regola di quotatura |
| niente impilamento incollato `{a}{b}$x` | ambiguo con `{int}{300}`; si impila annotando un valore annotato |
| solo un costruttore risolve un tipo | `{T}$x` fuori da un costruttore non costa nulla e non fallisce mai; la verifica avviene dove il valore viene salvato |
| annotazioni locali riconosciute dai loro caratteri | un nome di tipo non può contenere `-` o `?`: nessuna collisione possibile |
| `type def` nominale, `type alias` strutturale | distinguere unità (`mm`, `kg`) senza cambiare i valori |
| allargamento automatico, restringimento esplicito | nessuna perdita silenziosa di informazione |
| conversioni esplicite tra nominali | `mm` → `m` richiede `{m}$a` o `type cast` |
| forma nome/valore nuda soppressa | `set {png}p chunks $l` si leggeva in due modi; ora i nomi sono scritti nel sorgente (`{chunks}…` o `p(chunks)`), mai indovinati nei dati |
| etichette di campo: tutto o niente | un miscuglio sarebbe una forma posizionale mascherata |
| il percorso non sostituisce nulla | un dato che contiene `[script]` non viene mai eseguito dall'analizzatore dei percorsi (la doppia sostituzione scompare); stessa regola di un array |
| valori sostituiti atomici | `$D($k)` legge la chiave `$k`, anche se contiene `.` o `,` |
| percorso + etichette non previsto | nessun bisogno dimostrato; lo dirà l'uso |

### 17. Limiti e punti aperti

**Da correggere o da decidere**

- **Adozione da parte di una variabile non tipizzata.** `set e [lindex $L 1]` rende `e` tipizzata e rigorosa. Ne deriva la trappola `catch … m` (§ 6.2), e l'errore che ne sfugge è il messaggio originale, il che lo rende difficile da capire. Da decidere: adozione silenziosa, adozione senza contratto, o messaggio esplicito.
- **`;` e `,` mescolati senza graffe** (`{int}a; {int}b, {int}c`) sono accettati senza errore e producono un campo anonimo; la regola della specifica li rifiuta.
- **Leggere una variante che non è quella corrente** di una somma dà `unknown way to cast the value from S to string` invece di un messaggio che nomini la variante.
- **Varianti della stessa forma**: senza etichetta, viene presa la prima senza avviso.
- `set {paire}s {aut.nom}x` (il campo di un **altro** prodotto) dà `wrong # args`; un messaggio dedicato sarebbe più chiaro.
- `lset` con una lista di indici che passa per `TclLsetList` trattiene un riferimento di troppo (perdita, senza crash).

**Non ancora fatto**

- Valore «inesistente» in generale: `opt(T)` copre i collegamenti binari, non esiste una forma generale.
- `union`, campi di bit condivisi, lista «fino alla fine del file» (il rifiuto di `union` è voluto).
- Annotazione di commento `{#}` a livello dell'analizzatore, annotazione della prima parola (`{c}proc`), annotazioni di dichiarazione (`proc p {{?}{…} {!}arg}`).
- Note per elemento di una lista tipizzata (oggi la nota vale per l'intera lista); cancellazione di tutte le note con `{@}$x`.
- Codec C per somme, collegamenti e dimensioni calcolate; cache del tipo analizzato.
- Banco di prova C su simulazione (`typeparse/test/`) da aggiornare; esempi `arbre_var.tcl` e `defauts_calcules.tcl` da correggere.

## Appendici

### A. Installazione e test

Il prototipo parte da Tcl `core-9-0-branch` con il ramo del TIP 759. Dalla radice dell'albero Tcl (MSYS2 / MinGW64 o Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # estrae, copia i sorgenti dei tipi, applica le patch del nucleo,
                                        # ricompila e confronta ogni tests_tclsh/tNN_*.tcl con il suo .out
```

- I sorgenti dei tipi sono consegnati interi in `typeparse/` e copiati in `generic/`; si applicano solo le patch del nucleo elencate in `noyau/ORDRE.txt`, senza fuzz (`-F0`), e una patch già applicata viene riconosciuta e saltata.
- Opzioni di `maj.sh`: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "messaggio"`.
- Note di ogni patch: `noyau/LISEZMOI.md`. Esempi eseguibili: `exemples/`.

### B. Glossario

| termine | significato |
|---|---|
| annotazione | testo tra graffe incollato davanti a una parola: `{x}parola` |
| nota | annotazione non risolta, che segue il valore |
| tipo | annotazione risolta nella tipologia nota |
| annotazione locale | annotazione il cui testo non può essere un tipo (`{-v}`, `{is?}`); vive solo tra chi chiama e il comando |
| costruttore | ciò che salva un valore e ne risolve le annotazioni: `set {T}v`, legame degli argomenti, `lassign`, `foreach`, assegnazione in `expr` |
| contratto | il tipo di una variabile, verificato a ogni scrittura |
| adozione | una variabile non tipizzata prende il tipo del valore ricevuto |
| cast | conversione esplicita verso un tipo (`{T}$x` in un costruttore, `type cast`) |
| nominale | tipo creato da `type def`, distinto dalla sua definizione; si cancella davanti alla sua base |
| raffinamento | tipo di base ristretto, con una rappresentazione binaria (`int(u,8)`) |
| allargamento | passaggio automatico da un raffinamento a un tipo più ampio |
| prodotto | `struct`: campi con nome e ordinati |
| somma | una variante tra più, con nome |
| variante | uno dei rami di una somma |
| etichetta di campo | annotazione che nomina il campo di un argomento: `{age}12` |
| percorso | indice di un valore tipizzato: `R(A.(x,y))` |
| pathindex | oggetto che porta un indice e le sue parti sostituite |
| collegamento | `var.T` (verso una variabile) o `*(…).T` (verso un indirizzo binario) |
| parametro fantasma | parametro di un tipo che non cambia i valori ma distingue le istanze |

### C. Corrispondenze

| qui | C | Rust | LLVM |
|---|---|---|---|
| prodotto | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| somma | (`union` + discriminante a mano) | `enum` con dati | — |
| enumerazione (somma di letterali) | `enum` | `enum` senza dati | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (senza segno) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | `char *` terminato da NUL | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 bit) | `*const T` | `ptr` |
| `opt(T)` | puntatore che può essere `NULL` | `Option<T>` | — |
| tipo nominale (`type def mm double`) | `typedef` (senza controllo) | newtype `struct Mm(f64)` | — |
| `-align natural` / packed | allineamento predefinito / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | allineamento / `<{ }>` |
