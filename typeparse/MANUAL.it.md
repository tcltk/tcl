# Manuale dei tipi annotati — capitoli

*(Traduzione italiana di `MANUEL.md`, che resta il riferimento. Questo file
raccoglie i capitoli del manuale man mano che vengono scritti; le note di
patch restano in `noyau/LISEZMOI.md`. Gli identificatori degli esempi sono
lasciati come nell'originale francese, perché corrispondano agli script di
esempio e ai messaggi di errore.)*

## Dict e prodotto: due modi di descrivere una struttura

Per chi viene dal C: una **lista** Tcl è un array, un **dict** è una
struttura a chiavi libere, un **tipo prodotto** è uno `struct` dichiarato.
Gli ultimi due si somigliano molto, e spesso si passa dall'uno all'altro.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# prodotto
type def fiche  {dict(nom, age?=0).string}              ;# dict tipizzato
```

### Che cosa hanno in comune

| | prodotto `pt` | dict `fiche` |
|---|---|---|
| costruzione per nome | `set {pt}a {nom}Jean {age}12` o `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| accesso a un campo | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| valore predefinito di campo/chiave | `age ?= 0` (o `=`) | `age?=0` |
| campo sconosciuto | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| valore serializzato | lista Tcl | lista Tcl chiave/valore |

Il valore predefinito si scrive allo stesso modo: `?=` vale nei due casi, e
un valore predefinito può essere un letterale, `[script]` (calcolato a ogni
costruzione) o `$variabile`. In un prodotto, il vecchio `=` è ancora
accettato.

### Che cosa li distingue

* **Campi presenti.** In un prodotto tutti i campi esistono sempre: un campo
  omesso prende il suo valore predefinito, e un campo senza valore
  predefinito va fornito (`field "x" of pt has no value and no default`). In
  un dict, una chiave senza `?` è obbligatoria
  (`key "nom" is missing in fiche`) e una chiave marcata `?` può mancare.
* **Ordine.** Un prodotto ha campi ordinati; si costruisce anche in modo
  **posizionale** (`set {pt}b Jean 12`, come l'inizializzazione di uno
  struct C). Un dict non ha forma posizionale.
* **Un solo campo.** Un prodotto accetta il prefisso `{T.campo}`:
  `set {pt}c {pt.nom}Jean` imposta `nom`, gli altri campi prendono il loro
  valore predefinito (`Jean 0`). È la stessa scrittura di `{Val.i}5` per una
  somma.
* **Tipi dei campi.** Ogni campo di un prodotto ha il suo tipo
  (`{string}nom; {int}age`); un `dict(...)` ne ha uno solo per tutti i
  valori (`.string`).
* **Dimensione.** Un prodotto ha un numero fisso di campi, stabilito dalla
  definizione.

### Nominare i campi: etichette o percorso (niente più nome/valore nudo)

La forma nome/valore nuda (`set {pt}a nom Jean age 12`) è stata
**soppressa**: si confondeva con quella posizionale non appena *n*
argomenti valevano per *n* campi. Un prodotto si costruisce ora in tre modi,
senza indovinare nulla:

* **posizionale**: tanti valori quanti campi, in ordine
  (`set {pt}b Jean 12`);
* **etichette di campo**: `set {pt}a {age}12 {nom}Jean`. L'etichetta è
  l'annotazione più recente di ogni argomento; è letta nel contesto del
  prodotto, quindi un nome di campo prevale su un tipo globale con lo stesso
  nome (`type def age int` non cambia nulla), come il nome di una variante
  per una somma. Ordine libero; i campi omessi prendono il loro valore
  predefinito. **Tutto o niente**: mescolare etichette e valori posizionali è
  un errore (`can't mix values named by a field annotation …`), e così
  un'etichetta sconosciuta (`unknown field "taille" in pt`). Un valore che
  porta la sua etichetta in una variabile conta
  (`set v {age}12; set {pt}a {nom}Jean $v`);
* **percorso**: `set {pt}a(age,nom) 12 Jean`, utile quando i nomi sono
  calcolati o i valori sono tra graffe.

Il dict, invece, conserva la sua forma chiave/valore: un dict non ha forma
posizionale.

### Scegliere

* Un insieme di campi **noto e stabile**, con tipi diversi: prodotto.
* Chiavi **libere o facoltative**, dello stesso tipo: dict.
* Un formato con più forme possibili (PNG, GIF…): una **somma** di prodotti
  (vedi `exemples/png_somme.tcl`).

## bitarray: un intero di cui si leggono i bit

`bitarray(n, ordine)` è un intero di n bit (0 ≤ v < 2^n). L'ordine dice da
dove parte l'indice 0:

| | `lsb0` (predefinito) | `msb0` |
|---|---|---|
| bit di indice 0 | quello a DESTRA (`1 << 0`) | quello a SINISTRA |
| uso | registri, maschere, C | schemi di protocolli (RFC) |
| indice di un bit se cambia la larghezza | invariato | spostato |

Prendiamo il byte `0xB2`, scritto `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Lettura.** `$b(i)` dà il bit (0 o 1). `$b(i:j)` dà i bit da i a j come UN
intero, di tipo `bitarray(j-i+1, ordine)`: `$b(1:4)` vale 9 in lsb0;
`$c(0:3)` vale 11 in msb0 (i quattro bit a sinistra, `1011`).

**Scrittura.** `set b(0) 1`, `set b(4:7) 0`. In `expr`, una fetta si scrive
tra virgolette, perché il `:` appartiene all'analizzatore di `expr`:
`expr {"b(1:2)" = 3}`. Un valore che non sta nei bit indicati viene
rifiutato.

**Due ordini, due tipi.** `bitarray(8,lsb0)` e `bitarray(8,msb0)` non si
mescolano: assegnare l'uno all'altro è un `type mismatch`. Il `bitarray(8)`
predefinito è `bitarray(8,lsb0)`.

**Funzioni.** `popcount(x)`: numero di bit a 1; `bitlen(x)`: larghezza di un
bitarray, altrimenti numero di bit dell'intero.

**Dentro altri tipi.** Un campo di prodotto (`{bitarray(8)}flags`), un
elemento di lista (`list.bitarray(4)`) o una variabile di procedura sono
bitarray come gli altri: `set r(flags.7) 1`, `set L(2.3)`.

**Somiglia a un dict?** No: né chiavi né campi, un solo intero. Per
descrivere i campi di un'intestazione binaria, si combina un prodotto di
bitarray (vedi il capitolo precedente).

Esempi completi: `exemples/bitarray_exemples.tcl` (byte «packed» di un GIF e
flag TCP in `msb0`, permessi Unix in `lsb0`).

## type deref: scattare un'istantanea dei collegamenti

Un collegamento `var` contiene il NOME di una variabile e ne segue il valore
a ogni lettura. `type deref` sostituisce ogni collegamento con il valore che
la variabile ha IN QUELL'ISTANTE: il risultato non cambia più in seguito.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : il collegamento segue B
    puts $v                             ;# 1 2 3 : l'istantanea è fissata

* Il risultato ha il tipo «senza collegamenti»: `list.var.int` diventa
  `list.int`, e si usa come qualunque lista di interi.
* Una variabile mancante ferma tutto con un errore che nomina la casella:
  `type deref: at 2: can't read "::C": no such variable`. Non c'è risultato
  parziale e il valore originale non viene toccato.
* Si segue un solo livello: se una variabile puntata contiene a sua volta
  collegamenti, questi sono conservati; un secondo `type deref` li segue.
* Funziona in liste, dict, prodotti e SOMME, annidati o no, e nei tipi
  RICORSIVI (alberi come `tree(var)`). La variante di una somma è
  conservata.
* I grafi si trattano per livelli: se i vicini di un nodo sono collegamenti
  (`{list.var.node}next`), `type deref` dà il nodo con i vicini sostituiti
  dai loro valori, che contengono ancora collegamenti; un secondo
  `type deref` scende di un livello. Un ciclo, quindi, non fa mai girare
  all'infinito.
* Il tipo del risultato è un tipo a sé, chiamato `deref(<tipo originale>)`
  (`deref(tree(var))`, `deref(mix)`), calcolato una volta e poi
  riutilizzato: due istantanee dello stesso tipo hanno lo stesso tipo.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# albero i cui valori sono collegamenti
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

Vedi anche `MANUAL-deref.md` (in inglese): il tipo di collegamento `var` e
`type deref`, con esempi in `exemples/deref_manual.tcl`.

## Somma e `union` del C: da non confondere

Per chi viene dal C: una **somma** (`{{A}x, {B}y}`) somiglia a una `union`,
ma non è la stessa cosa.

| | `union` del C | somma |
|---|---|---|
| che cosa contiene | gli **stessi byte**, letti in più modi | **una sola** variante, che porta il suo nome |
| chi sa quale è valida | nessuno: il programmatore deve saperlo | il valore stesso (`type get` dà `S.x`) |
| leggere la variante sbagliata | permesso, si leggono byte qualsiasi | impossibile: si accede alla variante che si ha |
| dimensione in un file | quella della vista più grande | quella della variante scelta |
| scelta della variante | nessuna, non esiste | da un campo già letto (`S($tag)`) o dal suo nome (`S(x)`) |

Conseguenze:
* **Non c'è `union` in questo modello**, di proposito: si mantengono le
  protezioni. Una somma non può essere letta «di traverso».
* Quando il formato non dice quale variante sia valida (la
  `Tcl_ObjInternalRep` di Tcl, il cui discriminante è l'indirizzo
  `typePtr`, esterno alla union), la variante si sceglie **alla chiamata**:
  `Tcl_ObjAs(twoPtrValue)` (vedi `exemples/tcl_obj.tcl`). È il chiamante a
  saperlo, come in C, ma il tipo resta verificato.
* Una somma in cui più varianti hanno la **stessa forma** non è ambigua non
  appena la variante viene scelta per nome (`S(y)`): non c'è più niente da
  indovinare.

### Esistere, essere nullo, non esistere

Non confondere tre situazioni per un valore puntato da un collegamento (tipo
`*(...)`, in arrivo):
* **presente**: il collegamento designa un valore;
* **presente ma nullo o vuoto**: il valore esiste, vale `0`, `{}` o `""`;
* **inesistente**: il collegamento è nullo (offset 0, per esempio), non c'è
  nulla da leggere.

Un valore predefinito di campo (`= {}`) non distingue gli ultimi due. Una
somma con una variante senza valore (`{{T}some, {}none}`) li distingue. Non
è ancora implementato; vedi la nota di progetto.

## Tipi binari: leggere e scrivere byte (`type binary`, `chan binary`)

Un campo dice, tramite il suo TIPO, come si scrive in byte. Nient'altro da
imparare: è il principio di `bitarray(16)`, che porta già la sua larghezza.
Il `.` serve solo a COMPORRE (`list(3).T`); la rappresentazione sta tra le
parentesi del tipo.

    type def mz {"MZ"}                           ;# una costante: somma di un letterale
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| tipo | rappresentazione |
|---|---|
| `int(u,n)` / `int(s,n)` | intero senza segno / con segno di n bit (multiplo di 8, da 8 a 1024) |
| `double(32)` `double(64)` | virgola mobile IEEE |
| `string(raw,N)` | N byte grezzi |
| `string(ENC,N)` | testo su N byte, completato con NUL |
| `string(ENC,z)` | testo terminato da NUL (2 byte in utf-16, ucs-2) |
| `bitarray(n)` | n bit (multiplo di 8), letti come intero senza segno |
| `"MZ"` (somma di letterali di uguale lunghezza) | una costante, verificata dal tipo |
| `list(N).T`, un prodotto di questi tipi | N elementi; l'annidamento è permesso |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Un testo
che non si codifica senza perdita (`é` in ascii) o che non sta in N byte
viene rifiutato.

**Ordine dei byte.** Ultimo argomento: `be` (big-endian, ordine di rete) o
`le`: `int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Senza di
esso vale quello della chiamata (`-endian`, little per impostazione
predefinita). Non cambia i valori: `int(u,16,be)` e `int(u,16,le)` sono
intercambiabili.

**Allargamento automatico.** Un raffinamento entra dove ci si aspetta un
tipo più ampio:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: ogni int(u,8) è un int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 sta in -32768..32767
    set {int(u,8)}v $n               ;# rifiutato: restringimento (cast esplicito)
    set {list.int(u,8)}L $liste_int  ;# rifiutato anche per le liste

Un tipo definito con `type def port int(u,16)` resta nominale: `port` entra
in un int, ma un semplice `int(u,16)` non entra in un `port`.

**Allineamento.** Per impostazione predefinita i prodotti sono compatti
(packed), come le intestazioni di rete e PE/ELF. `-align natural` (o
`-align N`) aggiunge il riempimento del C e di LLVM: ogni campo comincia a un
multiplo del suo allineamento (la sua dimensione, limitata a 8 o N), la
dimensione di un prodotto è arrotondata al suo allineamento. Riempimento a
zero in scrittura, ignorato in lettura salvo con `-strictpad 1`.

    type binary entete                           ;# la rappresentazione, vista dal nucleo
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# il canale avanza
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

La lettura e la scrittura sono scritte in Tcl (`layout/filebinary.tcl`)
sopra il nucleo: `type binary` descrive, decodifica e codifica byte in
memoria, `chan binary` legge e scrive su un canale. Il nome `file binary`
(quello vecchio) resta disponibile come alias.

**`type binary`, interrogazioni e verbi.** Le domande su un tipo si scrivono
con un'annotazione locale sull'argomento di tipo (il `?` finale la distingue
da qualunque nome di tipo); le azioni sui byte restano verbi:

    type binary entete                          ;# la rappresentazione (forma nuda)
    type binary {what?}$T                       ;# idem
    type binary {sizeof?}$T ?-align A?          ;# dimensione in byte (errore se variabile)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 se $octets è decodificabile come T, altrimenti 0
    type binary decode T $octets ...            ;# legge i byte in memoria
    type binary encode T $valeur ...            ;# idem, encode
    type binary deref ...                       ;# idem, deref
    type binary space ...                       ;# idem, space (spazi con nome)

`{is?}` accetta per impostazione predefinita un prefisso valido (riconoscere
un'intestazione); `-exact 1` esige che tutti i byte siano consumati.
Un'opzione non valida, un tipo sconosciuto o privo di rappresentazione
binaria restano errori: solo il fallimento della decodifica dà 0.
Un'annotazione sconosciuta che termina con `?` è un errore
(`unknown question "oups?"`); qualunque altra annotazione è ignorata. Un
tipo che porta il nome di un verbo (`type def decode ...`) resta descritto da
`type binary decode`, finché non segue nessun altro argomento. `decode`,
`encode`, `deref` e `space` richiedono che `layout/filebinary.tcl` sia
caricato (altrimenti: `the binary layer is not loaded`).

**Alias brevi.** `layout/binaliases.tcl` dà nomi brevi alle rappresentazioni
usuali (convenzione di Rust e di Zig): `u8 u16 u32 u64`, `i8 i16 i32 i64`,
`f32 f64`, con i suffissi `be`/`le` (`u16be`, `i32le`, `f64be`), e
`cstring`, `wstringz`, `latin1z`. Sono `type alias`: la libreria serve anche
da promemoria sui formati binari. Gli `i8…i64` qui hanno segno; gli `iN` di
LLVM non hanno segno, quindi non c'è corrispondenza esatta (gli array
`[N x T]` e le strutture packed corrispondono a `list(N).T` e ai prodotti).


### Livello 2: dimensioni, quantità, posizioni e varianti lette nel file

Sempre la stessa sintassi: `$campo` negli argomenti del tipo, come i
parametri dei tipi parametrizzati, o un tipo intero senza segno che fa da
prefisso.

| scrittura | significato |
|---|---|
| `string(utf-8,int(u,16,be))` | testo preceduto dalla sua dimensione in byte (stringa Pascal) |
| `string(raw,$len)` | N byte, dove N è il valore del campo `len` |
| `string(utf-8,$len - 4)` | un'espressione Tcl sui campi già letti |
| `list(int(u,8)).T` | lista preceduta dal suo numero di elementi |
| `list($n).T` | n elementi, dove n è un campo |
| `at($off).T` | il valore si trova alla posizione `$off` del file (il cursore non si sposta) |
| `corps($tag)` | la somma `corps`, la cui variante è scelta dal campo `tag` |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# nomi delle varianti = etichette
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Regole: un `$campo` deve designare un campo **già letto** del prodotto
corrente o di un prodotto che lo contiene (`type binary` lo verifica); un
campo di prodotto si legge `$h(off)`. In scrittura, una dimensione o una
quantità che non corrisponde al campo da cui dipende è un errore, e
l'etichetta deve essere quella della variante. `type binary {sizeof?}`
rifiuta un tipo la cui dimensione dipende dai dati. Esempio completo (PE,
ELF, messaggi): `exemples/niveau2.tcl`.

Esempi: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Limiti: niente
union né campi di bit condivisi, niente lista «fino alla fine del file».

#### Variante scelta: `S(nome)` e parametro di tipo

Una somma `S` può anche avere la sua variante **scelta nella definizione**,
senza campo selettore nel file:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# la variante tp, scritta per esteso
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# o data da un parametro
type binary decode {R(tp)} $octets
```

`U(tp)` è un tipo a una sola variante: la sua dimensione è nota
(`type binary {sizeof?}`), il valore letto è `U(tp).tp` e non c'è niente da
indovinare, anche se più varianti hanno la stessa forma (cosa che prima
rendeva ambiguo il valore). `U($campo)` mantiene il suo senso: variante
scelta da un campo già letto. Se `$v` è un parametro legato del tipo, si
applica il primo caso. Esempio: `exemples/tcl_obj.tcl`
(`Tcl_ObjAs(twoPtrValue)`). Test: `t36_somme_choisie.tcl`.

#### Il tipo collegamento `*(origine,repr).T`

Un campo di file contiene spesso un indirizzo, non il valore. Il tipo
collegamento legge l'indirizzo (`repr`, un intero) e poi il valore `T` che vi
si trova; il valore del campo è quello della destinazione.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Origini, come in un file system: `//` (o `file://`) inizio dei dati, `./`
inizio della struttura che lo contiene, `../` la sua genitrice, `.` il campo
stesso (vedi sotto). Opzioni: `null` (l'indirizzo 0 significa «nessuna
destinazione»), `null=V`. Con `opt(T)` (vedi `unit`), un indirizzo nullo dà
`none`, altrimenti `some`: è la differenza tra «nullo» e «inesistente».

Gli alias generici (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) danno
nomi brevi: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` in
`layout/binaliases.tcl`. In scrittura, le destinazioni sono aggiunte in
fondo, allineate, e gli indirizzi vengono calcolati. I tipi ricorsivi,
`memory://` e gli spazi con nome sono descritti più avanti. Dettagli:
`noyau/CONCEPTION-lien.md`.

**Il punto `.`: l'indirizzo è relativo alla posizione del campo.** `./`
indica l'*inizio della struttura*; `.` indica *il campo stesso* (il punto in
cui è scritto l'indirizzo). È l'origine degli spostamenti «da qui», frequenti
nei formati a spostamenti relativi (`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# il campo r è all'offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# stesso risultato, ma con ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Promemoria: `//` inizio dei dati · `./` inizio della struttura · `../`
inizio della struttura genitrice · `.` posizione del campo. La notazione `.`
resta volutamente breve; un nome più espressivo sarà preso in considerazione
solo dopo l'uso.

#### Fin dove seguire i collegamenti: `-linkdepth`

Un collegamento può essere seguito subito o lasciato così com'è. L'opzione
di chiamata `-linkdepth` regola il numero di collegamenti seguiti (come la
portata di una radiazione: un raggio gamma attraversa tutto, un raggio poco
penetrante si ferma alla pelle):

    type binary decode H $d                   ;# 1 {7 8}   Inf, il predefinito: seguire tutto
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   un livello di collegamenti
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      nessun collegamento seguito
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Un collegamento non seguito è un intero annotato (posizione assoluta della
destinazione nei dati). La copia, `lindex` e `string cat` conservano
l'annotazione; un calcolo la perde, e il risultato non può più essere
seguito. Si segue un collegamento su richiesta, un passo alla volta:

    type binary deref $h(r) $d                ;# 7 8   (-linkdepth 0 per impostazione predefinita: un solo passo)

Che cosa se ne ricava: leggere solo l'intestazione di un file grande;
percorrere una lista concatenata o un albero (il tipo può essere ricorsivo:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); individuare un
collegamento danneggiato; modificare un campo sul posto, perché
`type binary encode` riscrive così com'è un indirizzo non seguito invece di
riorganizzare il file. Un ciclo è un errore solo con `Inf`.


## Spazi con nome

Un collegamento può designare uno spazio registrato con il suo nome invece
dei dati della chiamata.

```tcl
set sp [type binary space file $octets -name lib]          ;# valore annotato di tipo space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segmenti {indirizzo offset dimensione}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ è equivalente
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# niente DATI: lo spazio basta
type binary space list | get NOME | delete NOME
```

Il nome predefinito è calcolato (`space1`, …); il comando di creazione
registra lo spazio; un nome già usato è un errore. `//nome/` richiede uno
spazio senza segmenti, `memory://nome/` uno spazio con segmenti; un
indirizzo fuori dai segmenti è un errore. Non si può scrivere una vera
destinazione in uno spazio con nome: si riscrive solo l'indirizzo di
frontiera (`-linkdepth 0`).

## Scrivere un collegamento nullo

Un campo collegamento che ha un valore predefinito e il cui valore è uguale a
quel valore predefinito si scrive come collegamento nullo (l'indirizzo
«nullo» del collegamento, nessuna destinazione viene scritta). La lettura fa
l'inverso: indirizzo nullo → valore predefinito. Senza valore predefinito (e
senza `opt`), leggere un collegamento nullo resta un errore.

## -dedupe: condividere le destinazioni identiche

`type binary encode TIPO VALORE -dedupe 1` (anche `write`) scrive una sola
volta le destinazioni di collegamenti i cui byte sono identici; i
collegamenti interessati ricevono lo stesso indirizzo. Per impostazione
predefinita (`-dedupe 0`) ogni destinazione viene scritta. Si condividono
solo le destinazioni senza collegamento annidato (gli indirizzi interni di
una destinazione dipendono dalla sua posizione); la rilettura dà lo stesso
valore.

## Dimensione che dipende da un campo: verificata alla costruzione

Una lista il cui numero di elementi è un'espressione su altri campi
(`{list($n).Sample}items`, `list($n - 1).u8`) è verificata fin dalla
costruzione del prodotto: `set {Batch}b(n,items) 3 {{1 1.0 0}}` è rifiutato
(`field "items" of Batch holds 1 elements, but its size says 3 ($n)`). I
campi omessi contano con il loro valore predefinito. La verifica non copre
le assegnazioni campo per campo (`set b(n) 3`: uno stato intermedio è
legittimo); `type binary encode` verifica di nuovo in scrittura. Le
dimensioni in byte delle stringhe (`string(raw,$len)`) non sono verificate
alla costruzione.
