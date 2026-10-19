# Annotaties in Tcl — documentatie

*19 oktober 2026 · versie v13 · Florent MERLET, met Claude (Anthropic)*

*Andere talen: [Français](ANNOTATIONS.fr.md) (referentie) · [English](ANNOTATIONS.en.md) · [Deutsch](ANNOTATIONS.de.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md)*

## Voorwoord

**Auteurs**: Florent MERLET, met Claude (Anthropic).

Dit document beschrijft de **annotaties** van Tcl 9.0 die TIP 764 voorstelt: metagegevens die aan een woord worden gehecht (`{x}woord`), de waarde niet veranderen, maar het mogelijk maken variabelen te typeren, velden te benoemen en binaire formaten te beschrijven. Het vervangt de handleiding die tijdens de ontwikkeling gaandeweg is ontstaan en beschrijft het prototype in **versie v13**.

Drie delen, voor drie lezers:

- **Deel I — Concepten**: wat een annotatie is, het verschil tussen een notitie en een type, en hun regels. Lees dit eerst.
- **Deel II — Referentie**: de commando's `note` en `type`, de typetaal, paden, het opbouwen van waarden, binaire typen en fouten. Voor wie Tcl schrijft.
- **Deel III — Implementatie**: het geannoteerde object, de parser, de bytecode, de betrokken bestanden en functies. Voor wie de TIP of de code beoordeelt.

Daarna volgen compatibiliteit en prestaties, ontwerpkeuzes, grenzen en bijlagen (installatie, woordenlijst, overeenkomsten met C, Rust en LLVM).

De voorbeelden zijn Tcl dat op het prototype draait; het commentaar `;#` geeft het resultaat. Foutmeldingen worden geciteerd zoals Tcl ze geeft. De namen in de voorbeelden (`nom`, `fiche`, `couleur`, `grandeur`…) zijn gelijk gehouden aan het Franse origineel, zodat ze overeenkomen met de voorbeeldscripts.

## Deel I — Concepten

### 1. Annotaties

#### 1.1 Herkomst van het woord

*Annoteren* komt van het Latijnse *ad notare*, „een notitie ernaast zetten”. Het woord *protocol* zegt hetzelfde in een andere vorm: het Griekse *prōtokollon* was het eerste blad dat vooraan op een papyrusrol werd geplakt en de inhoud ervan beschreef zonder er deel van uit te maken. Een Tcl-annotatie is precies dat: een blad dat vooraan een woord is geplakt.

```tcl
{mm}12          ;# de waarde blijft 12; „mm” is ervoor geplakt
{list.int}L     ;# de naam blijft L; „list.int” zegt wat hij zal bevatten
```

#### 1.2 Metagegevens

Een annotatie is een **metagegeven**:

- ze **verschijnt niet in de tekst** van de waarde: `{mm}12` is `12` voor `puts`, `expr`, `string length`;
- ze **geeft informatie** die de waarde alleen niet draagt: een eenheid, een type, een veldnaam, een optie;
- ze leeft **buiten het vlak van de ruwe waarden**: geen bestaand Tcl-commando ziet haar, en geen bestaand script verandert van betekenis.

#### 1.3 Een analogie: het imaginaire deel

Een complex getal heeft een reëel en een imaginair deel. Zo heeft een geannoteerde waarde een **ruw** deel (de tekst die Tcl altijd al verwerkt) en een **geannoteerd** deel (wat de ontwikkelaar in gedachten heeft).

| | complex getal | geannoteerde waarde |
|---|---|---|
| zichtbaar deel | het reële deel | de ruwe waarde (`12`) |
| toegevoegd deel | het imaginaire deel | de annotatie (`mm`, `int`, `nom`) |
| wat men gewoonlijk leest | het reële deel | de tekenreeks |

De analogie houdt op bij rekenen. Bij een complex getal rekent het imaginaire deel mee. Hier **verliest een berekening de annotatie**: `expr {$x + 1}` geeft een kaal getal. Alleen kopiëren, `lindex`, `string cat` en doorgeven als argument behouden haar. Dat is bewust: het vlak van de ruwe waarden blijft gewoon Tcl, en dat garandeert de compatibiliteit.

#### 1.4 De uitvoering sturen

De annotatie maakt het „imaginaire deel” van de ontwikkelaar expliciet, wat hij over zijn gegevens weet en Tcl niet:

- een **type** laat elke schrijfactie naar een variabele controleren (`set {int}n abc` mislukt);
- een **veldnaam** bepaalt waar een waarde heen gaat (`set {pt}a {age}12 {nom}Jean`);
- een **binaire representatie** zegt hoe bytes gelezen en geschreven worden (`{int(u,16,be)}version`);
- een **notitie** markeert een waarde zodat een procedure haar herkent (`note has $v mm`).

Het script behoudt zijn vorm: je voegt annotaties toe waar je garanties wilt, en nergens anders.

### 2. Notities, typen en lokale annotaties

Een annotatie wordt altijd `{tekst}woord` geschreven. Afhankelijk van wat de tekst zegt en van **wie hem leest**, speelt ze drie rollen.

#### 2.1 De notitie: de ruwe annotatie

Een **notitie** is een annotatie die nog door geen enkele constructor is opgelost. Ze volgt de waarde: kopie, procedureargument, lijstelement.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (leeg: geen opgelost type)
```

**Stapelen.** Je annoteert een waarde die al geannoteerd is; de notities stapelen zich op, van de oudste naar de recentste.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px is de recentste)
```

De schrijfwijze `{a}{b}$x` (twee aan elkaar geplakte annotaties) bestaat niet: ze zou dubbelzinnig zijn met `{int}{300}`, een annotatie op een letterlijke waarde tussen accolades. Je stapelt door een waarde te annoteren die het al is.

#### 2.2 Het type: de annotatie die een constructor oplost

Een **type** is een annotatie die wordt **opgelost** in de typologie die de interpreter kent (basistypen, `type def`, `type alias`). Alleen een **variabeleconstructor** lost op:

- `set {T}v …` (en `set v {T}$x`);
- het binden van de argumenten van `proc`;
- `lassign`, `foreach`, en in `expr` `x = v` en `x is T = v`.

Op de **naam** van een variabele geeft een annotatie de variabele een **contract**: elke latere schrijfactie wordt gecontroleerd.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Buiten een constructor controleert `{T}$x` niets en zet niets om, zelfs als `T` een bekend type is: `T` wordt gewoon **gestapeld**, zoals een notitie. `list {int}abc` geeft geen fout.

#### 2.3 Twee stapels en een afstapelregel

Een geannoteerde waarde draagt **twee lijsten**: het **opgeloste type** (hoogstens één) en de **stapel van openstaande notities**. De weergave scheidt ze met `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 opgelost type   notities, van de oudste (mm) naar de recentste (hello)
```

Een constructor **stapelt af** vanaf de top:

1. hij neemt de recentste notitie en probeert haar op te lossen;
2. hij gaat door zolang de notities zich oplossen; de eerste die zich niet oplost, stopt alles en blijft op de stapel;
3. de afgestapelde typen moeten samenhangen: de recentste moet in elk van de andere passen, anders `contradictory annotations px and qq`;
4. de waarde wordt omgezet („gecast”) naar het recentste type;
5. het contract van de variabele doet de laatste controle.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px lost op, mm niet
type get $z          ;# px      ; note get $z: mm
```

Een notitie die **vóór** haar `type def` is gezet, wordt dus door de volgende constructor opgelost: later `type def mm double` schrijven geeft betekenis aan de al gezette `{mm}`.

**Eerst de context.** Een constructor lost op in zijn context vóór de globale typen: bij een som de namen van de varianten (`{i}5` in `set {S}v {i}5`); bij een product de veldnamen (`{age}12`). Een globaal type met dezelfde naam verandert niets.

#### 2.4 Lokale annotaties

Een annotatie waarvan de tekst een teken bevat dat typenamen weigeren (`-`, `?`, `:`…), kan **nooit** een type zijn. Het is een **lokale annotatie**: ze leeft alleen tussen de aanroeper en het aangeroepen commando, en volgt de waarde niet.

```tcl
type binary {sizeof?}$T          ;# een vraag aan type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# geweigerd: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# geweigerd: local annotation {-v} cannot declare a variable
```

Twee natuurlijke families, zonder kans op botsing met een typenaam: opties als `-naam` en vragen als `naam?`. Een typenaam bevat alleen ASCII-letters, cijfers, `_` en niet-ASCII-bytes, en begint niet met een cijfer.

#### 2.5 De lege annotatie `{}`

`{}` is „het lege type”. Het effect hangt af van de plaats:

| waar | effect |
|---|---|
| op een waarde, in een constructor (`set v {}$x`) | verwijdert het opgeloste type, behoudt de notities |
| op een naam (`set {}v $x`) | heft het contract van de variabele op; de waarde wordt met haar notities opgeslagen, zonder iets op te lossen |
| op een waarde, buiten een constructor | de lezer ziet de kale inhoud (type en notities gewist) |

```tcl
set {int}n 5
set {}n abc          ;# het contract is opgeheven: n is abc
```

De oorspronkelijke waarde wordt nooit gewijzigd: `{}` maakt een nieuwe weergave, wat het delen van objecten veilig maakt.

## Deel II — Referentie

### 3. Het commando `note`

```
note get WAARDE              ;# de notities, van de oudste naar de recentste, verbonden door „.”
note has WAARDE NAAM         ;# 1 als NAAM een van de notities is, anders 0
note delete WAARDE ?NAAM?    ;# de waarde zonder de notitie NAAM, of zonder enige notitie
```

Tcl-waarden zijn onveranderlijk: `note delete` **geeft** een nieuwe waarde terug, het wijzigt geen variabele.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: geen aaneengeplakt stapelen
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Typisch gebruik: een procedure die reageert op de markering van een waarde.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. Het commando `type`

```
type def NAAM DEFINITIE        ;# definieert een nominaal type (of een geparametriseerde familie)
type alias NAAM DEFINITIE      ;# geeft een definitie een structurele naam
type get WAARDE                ;# het opgeloste type van een waarde (leeg als er geen is)
type cast TYPE WAARDE          ;# de oplosser van een constructor, zonder variabele
type convert VAN NAAR LAMBDA   ;# declareert een expliciete omzetting
type deref WAARDE              ;# momentopname: elke var-koppeling vervangen door haar waarde
type binary TYPE ...           ;# binaire representatie, codering, decodering (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (een alias is geen nieuw type)
type cast pt {Léa 7}          ;# Léa 7, van type pt
```

In `expr` test de operator **`is`** het type van een variabele, en een toekenning kan declareren:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# declareert z van type pt en kent de waarde toe
expr {new is cmd}              ;# 1 als new een commando is; new(…) wordt een expr-functie
```

`is cmd` is geen type: het test of een commando bestaat en stelt het beschikbaar als functie van `expr`.

### 5. Het typesysteem

#### 5.1 Basistypen

| type | waarden |
|---|---|
| `int` | geheel getal (onbegrensd, zoals in Tcl) |
| `double` | zwevendekommagetal |
| `string` | elke tekenreeks |
| `list` | gewone Tcl-lijst (ongetypeerde elementen) |
| `dict` | gewone Tcl-dictionary |
| `var` | koppeling naar een variabele, via haar naam (§ 5.3) |
| `any` | element van een heterogene lijst (`list.any`): elk element behoudt zijn type |

**Verfijningen** beperken een basistype en dragen een binaire representatie: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Een `type def` maakt een **nominaal** type: `type def port int(u,16)` verschilt van `int(u,16)`. Een `type alias` geeft een definitie alleen een naam.

#### 5.2 De compositietaal

Een type wordt geschreven als een expressie, geanalyseerd naar het model van `expr` (`tclTypeParse.c` volgt de structuur van `tclCompExpr.c`).

**Drie soorten operanden:**

| operand | waar | voorbeeld |
|---|---|---|
| type | kaal woord, links van een `}` of op een operandpositie | `int`, `list`, `pt` |
| naam | woord rechts tegen een `}` | `{int}age`: het veld `age` |
| waarde | tussen aanhalingstekens, tussen accolades, of na `=` | `"MZ"`, `= 0` |

**Operatoren**, van de sterkste naar de zwakste:

| operator | rol | voorbeeld |
|---|---|---|
| `.` | nesting (rechtsassociatief) | `list.dict.int` |
| `( )` | argumenten, tegen de naam | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` of `?=` | standaardwaarde van een veld | `{int}age ?= 0` |
| `;` | product | `{double}x; {double}y` |
| `,` | som | `{int}i, {string}s` |
| `{ }naam` | groepering, met een naam tegen de sluitaccolade | `{{int}x; {int}y}A` |

`;` en `,` hebben dezelfde prioriteit: om een product en een som te mengen, groepeer je met accolades (`{int}a; {{int}b, {string}c}u`).

**Tekens.** Een typenaam bevat alleen ASCII-letters, cijfers, `_` en niet-ASCII-bytes, en begint niet met een cijfer. De grammatica gebruikt daarnaast `{ } , ; . = " # ( ) * $`. Elk ander ASCII-teken (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) wordt geweigerd: `invalid character "!"`, code `TCL PARSE TYPE BADCHAR`, behalve na `=` (een standaardwaarde) en tussen de haakjes van een constructor (`*(file://x, …)`). Die weigering maakt lokale annotaties (`{-v}`, `{is?}`) veilig.

Een definitie is **declaratief**: bij het lezen wordt niets gesubstitueerd, behalve in een standaardwaarde, die bij elke opbouw wordt geëvalueerd.

#### 5.3 Velden en containers

**Opsomming** — een som van letterlijke waarden.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# één letterlijke waarde: een constante (bestandshandtekening)
```

**Som** — telkens één variant, die haar naam draagt.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# de variant gekozen via de naam van het type
```

**Product** — een `struct`: benoemde, geordende velden, elk met een eigen type.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positioneel
set {pt}b {age}12 {nom}Jean    ;# veldlabels (§ 8)
$a(nom)                        ;# Jean
```

**Lijst** — `list` (uitbreidbaar), `list.T` (elementen van type T), `list(N).T` (precies N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

De elementen van een `list.T` zijn atomair: `lindex`, `foreach`, `$L(i)` geven getypeerde elementen, nooit opnieuw in woorden gesplitst. De kale lijst `{list}` behoudt het gedrag van Tcl.

**Dictionary** — `dict` (alle sleutels), `dict.T` (waarden van type T), `dict(sleutels).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom verplicht; age optioneel, standaard 0
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z mag ontbreken; `dict()`: alleen de lege sleutel. Sleutels zijn altijd tekst; alleen de waarden zijn getypeerd.

**Koppeling `var`** — de NAAM van een variabele, bij elke lezing gevolgd.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (een momentopname, van type list.int)
```

**Typeparameter `{type}T`** — in een geparametriseerd type een parameter die een type ontvangt (§ 5.5).

**Binaire koppeling `*(oorsprong,repr).T`** — een adres in binaire gegevens, gevolgd naar een waarde T (§ 9.5).

#### 5.4 Standaardwaarden

`?=` (of `=`, de oudere schrijfwijze, in een product nog aanvaard) geeft een veld een standaardwaarde. Een standaardwaarde kan een letterlijke waarde zijn, een `[script]` dat bij elke opbouw wordt geëvalueerd, of een `$variabele` die bij elke opbouw wordt gelezen.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Een veld zonder standaardwaarde moet worden opgegeven: `field "x" of pt has no value and no default`.

#### 5.5 Geparametriseerde typen

De naam van een `type def` kan parameters dragen, gedeclareerd als velden: `{type}T` (een type), `{int}n`, `{double}f`, `{string}s`. In de body gebruik je ze met `$`.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# recursief
```

#### 5.6 Fantoomparameters: fysische eenheden

Een parameter die niet in de body voorkomt, is toegestaan: hij verandert de waarden niet, maar onderscheidt de instanties, die verschillende nominale typen zijn. Een familie van grootheden levert zo eenheden waarvan de waarde een `double` blijft:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# regel niet van toepassing
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (expliciete omzetting)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Een `type convert`-regel kan een type of een **familie** betreffen (de naam van een geparametriseerde `type def`); de lambda ontvangt dan de parameters van de instanties (`$de(f)`). `return -code continue` betekent „niet van toepassing”. Omzettingen blijven expliciet.

#### 5.7 Meegeleverde aliassen

`layout/binaliases.tcl` definieert, voor binaire formaten (conventies van Rust en Zig):

| alias | definitie |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| achtervoegsels `be`, `le` (`u16be`, `i32le`) | vaste bytevolgorde |
| `f32 f64` (en `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Getypeerde variabelen en het leven van een geannoteerde waarde

#### 6.1 Het contract van een getypeerde variabele

Een variabele die met `set {T}v …` is gedeclareerd, heeft een **contract**: elke schrijfactie wordt gecontroleerd, of ze nu komt van `set`, `lappend`, `lset`, `incr`, `dict set`, een padschrijfactie (`set v(x) …`) of `expr {v = …}`.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Automatische verbreding.** Een verfijning past waar een breder type verwacht wordt: `int(u,8)` in `int`, `int(s,16)` of `double`; `double(32)` in `double(64)`. Versmalling wordt geweigerd (ze vraagt een expliciete cast), net als tussen `bitarray(…,lsb0)` en `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# aanvaard: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominale typen.** Een nominaal type wijkt voor zijn basis (`px` past in `int` en wordt `int`, zonder spoor). Twee nominale typen zonder basisverband zijn nooit gelijkwaardig (`mm` en `kg`): dat is precies hun rol.

**Expliciete casts.** `set v {T}$x` of `type cast T $x` zetten om volgens, in deze volgorde: een specifieke `type convert`-regel; de keten van basissen; de variant van een som; een familieregel; de ingebouwde omzettingen tussen scalairen. Anders: `unknown way to cast the value from A to B`.

**Het contract opheffen.** `set {}v waarde` (§ 2.5).

**Testen.** `expr {v is T}`: waar als de variabele getypeerd is en haar type in `T` past; voor een ongetypeerde variabele waar als haar waarde geldig is voor `T`.

#### 6.2 Wat de annotatie behoudt, wat ze verliest

| bewerking | type | notities |
|---|---|---|
| kopie (`set a $x`), argument, retourwaarde | behouden | behouden |
| `lindex`, `foreach`, `$L(i)` op een `list.T` | element van type `T` | — |
| `string cat $x` (één argument) | behouden | behouden |
| berekening (`expr`, `string toupper`, `dict set` op een waarde) | **verloren** | **verloren** |
| cast `{T}$x` in een constructor | gewijzigd | behouden |
| `set {T}a $x`, getypeerde variabele | `T` | gewist door verbreding |

**Overname.** Een **ongetypeerde** variabele die een getypeerde waarde ontvangt, **neemt** haar type **over**, en dus haar contract:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e wordt een int-variabele
set e hello                    ;# expected integer but got "hello"
```

Dit is het verrassendste gedrag van het model. De klassieke valkuil is een hergebruikte `catch … m`: als een eerste `catch` een getypeerde waarde in `m` heeft gezet, kan een tweede `catch` daar zijn foutmelding niet meer kwijt. Gebruik één variabele per toepassing, `unset` tussen twee toepassingen, of `set {}m …`. (Open punt, § 17.)

**Procedures.** Een argument dat een getypeerde waarde ontvangt, is in de procedure getypeerd en strikt; een gewoon argument verandert niet.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Lusvariabelen.** `foreach`, `lmap`, `dict for`, `dict map` (en het filter van `dict filter`) vergeten het type van hun variabele vóór elke toekenning; na de lus houdt de variabele het type van het laatst ontvangen element.

#### 6.3 `type deref`: een momentopname van de koppelingen

`type deref V` geeft `V` terug waarin elke `var`-koppeling is vervangen door de waarde die de variabele op dat moment heeft. Het resultaat heeft het type „zonder koppelingen” (`list.var.int` → `list.int`), `deref(<type>)` genoemd voor een product of een som. Per aanroep wordt één niveau gevolgd; een ontbrekende variabele is een fout die de plaats noemt (`type deref: at 2: can't read "::C": no such variable`), zonder gedeeltelijk resultaat. Werkt in lijsten, dicts, producten, sommen en recursieve typen; een cyclus loopt nooit vast.

### 7. Bij de delen van een waarde komen: paden

#### 7.1 De vorm met haakjes

Een getypeerde waarde wordt gelezen en geschreven met de syntaxis van arrays, maar de index is een **pad**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# opbouw (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# een heel veld schrijven
set R((A,B).x) 10 30                       ;# meervoudig schrijven
${R(A.x, B.y)}                             ;# lezen, spaties toegestaan
```

Spaties zijn toegestaan in `$R(…)`, `${R(…)}` en in een geannoteerd woord `{T}R(…)`. In een kaal, niet-geannoteerd woord (`set R(A.x, A.y) …`) splitst Tcl zoals altijd bij spaties: schrijf zonder spaties of tussen aanhalingstekens.

#### 7.2 De padtaal

De taal is die van de Tcl-wikipagina „A better way to do indexation”, geanalyseerd naar het model van `expr` (`tclTypePath.c`).

**Operanden:**

| operand | betekenis |
|---|---|
| `naam` (kaal woord) | veld, dict-sleutel, variantnaam of lijstindex (`3`, `end`, `end-1`): het type van de container beslist |
| `'sleutel'` | altijd een sleutel (`'end'` is de sleutel „end”, geen index) |
| `"label"` | letterlijke tekst, in het resultaat gekopieerd |

**Operatoren**, van de zwakste naar de sterkste:

| operator | rol | voorbeeld |
|---|---|---|
| `,` | verzameling (een groep) | `A,B` |
| `*` (unair) | uitvouwen: de groep gaat op in zijn ouder | `*0:2,4` |
| `.` | nesting; verdeelt over een groep | `A.(x,y)` = `A.x,A.y` |
| `:` | lijstbereik | `0:end-1` |
| `( )` | groepering | `(A,B).x` |

**Vorm van het resultaat.** Een meervoudige lezing geeft lijsten die volgens de groepen genest zijn: `$R((A,B).(x,y))` geeft `{1 2} {3 4}`; `$L(0:2,4)` geeft `{10 20 30} 50`; `$L(*0:2,4)` geeft `10 20 30 50`.

**Een pad substitueert niets.** Tcl substitueert `$x`, `[cmd]` en `[(expr)]` in `$R(…)` en in kale woorden; een gesubstitueerde waarde is **één blad**, nooit opnieuw als pad gelezen:

```tcl
set k file.txt ; $D($k)        ;# de sleutel „file.txt” (niet D.file.txt)
set n 1        ; $D(f_$n)      ;# tegen tekst geplakt verlengt de waarde de naam
set idx {0 2}  ; $L(*$idx)     ;# * vouwt een lijstwaarde uit: 10 30
set p {[exec ls]} ; $L($p)     ;# er wordt niets uitgevoerd: bad index "[exec ls]"
```

Tussen accolades en in `${…}` zijn `$` en `[` fouten: `a path does not substitute "$"…`. Een berekend pad lees je door de variabele ermee te **benoemen**: `set ref R($chemin) ; set $ref`.

#### 7.3 Per container

| container | lezen | schrijven |
|---|---|---|
| product | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (opnieuw gecontroleerd) |
| som | `$v(i)`: de huidige variant | `set v(i) 6` |
| lijst | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (één bit), `$b(1:4)` (een deel: een geheel getal `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| opsomming | (scalair: geen pad) | — |

Hetzelfde pad lezen en daarna terugschrijven werkt altijd: `set R(c) [set R(c)]`.

### 8. Een waarde opbouwen

#### 8.1 De vormen van een productconstructor

Een product met *n* velden wordt op vier manieren opgebouwd, zonder iets te raden:

| vorm | voorbeeld | regel |
|---|---|---|
| positioneel | `set {pt}a Jean 12` | precies *n* waarden, in de volgorde van de velden |
| veldlabels | `set {pt}a {age}12 {nom}Jean` | alle waarden gelabeld; vrije volgorde; weggelaten velden krijgen hun standaardwaarde |
| één veld | `set {pt}a {pt.nom}Jean` | de andere velden krijgen hun standaardwaarde |
| pad + positionele waarden | `set {pt}a(age,nom) 12 Jean` | één waarde per blad van het pad (of per groep, § 8.3) |

Daarbij komen **geen argument** (`set {pt}a`, alle velden op hun standaardwaarde) en **een product van hetzelfde type** (kopie). Al het andere is een fout:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

De kale naam/waarde-vorm (`set {pt}a nom Jean age 12`) **bestaat niet meer**: ze viel samen met de positionele vorm zodra *n* argumenten bij *n* velden pasten (§ 16).

#### 8.2 Veldlabels

Het label is de **recentste** annotatie van elk argument, gelezen in de context van het product: een veldnaam gaat voor op een globaal type met dezelfde naam.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# verandert niets
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# een label dat een variabele meedraagt telt mee
```

**Alles of niets:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

Ook een waarde tussen accolades kan een label krijgen: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Pad en positionele waarden

`set {T}v(pad) v1 v2 …` bouwt het product (en zijn geneste producten) op; de benoemde velden krijgen de waarden, de andere hun standaardwaarde. Het **aantal** waarden bepaalt de verdeling, nooit hun inhoud:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# één waarde per blad
set {rect}R((A,B).(x,y))     1 2 3 4      ;# idem
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# één waarde per element van de kopgroep
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# één waarde voor het hele pad
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# met uitvouwen: één waarde, plat
```

Elke waarde heeft de vorm die haar lezing zou geven; de drie aantallen vallen alleen samen als de lezingen samenvallen.

**Pad + labels bestaat niet**: `set {rect}R(A) {y}2 {x}1` is een fout (`1 targets but 2 values`). Het is niet gepland zolang geen toepassing erom vraagt.

#### 8.4 Sommen en lijsten

Een som wordt opgebouwd met haar variant als label, of via de naam van de variant in het type:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# elk element behoudt zijn variant
```

Zonder label wordt de variant uit de waarde afgeleid, in de volgorde van de varianten: `set {S}u 5` geeft `S.i`. Als meerdere varianten dezelfde vorm hebben (`{string}a, {string}b`), past altijd de eerste: label om te kiezen (`{b}y`).

Een lijst ontvangt haar elementen één voor één: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Eén argument is **één** element.

### 9. Binaire typen

Een type zegt zelf hoe het als bytes wordt geschreven: er is geen extra syntaxis te leren. Lezen en schrijven gebeuren in `layout/filebinary.tcl` (geladen met `source`), boven op de kern, met een snelle weg in C voor vaste indelingen (§ 9.6).

#### 9.1 `bitarray`: een geheel getal waarvan je de bits leest

`bitarray(n, volgorde)` is een geheel getal van n bits (0 ≤ v < 2^n). De volgorde zegt waar index 0 begint: `lsb0` (standaard, het rechtse bit, zoals `1 << i`: registers, maskers) of `msb0` (het linkse bit, zoals in RFC-diagrammen). Het zijn twee verschillende typen.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (een geheel getal bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# in expr staat een deel tussen aanhalingstekens
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Representaties

| type | representatie |
|---|---|
| `int(u,n)` / `int(s,n)` | geheel getal zonder / met teken van n bits (veelvoud van 8, 8 tot 1024) |
| `double(32)`, `double(64)` | IEEE-zwevendekommagetal |
| `string(raw,N)` | N ruwe bytes |
| `string(ENC,N)` | tekst op N bytes, aangevuld met NUL |
| `string(ENC,z)` | tekst afgesloten met een NUL (2 bytes in utf-16, ucs-2) |
| `bitarray(n)` | n bits (veelvoud van 8), gelezen als geheel getal zonder teken |
| `"MZ"` (som van letterlijke waarden van gelijke lengte) | een constante, gecontroleerd |
| `list(N).T`, een product van deze typen | N elementen; nesting is toegestaan |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Een tekst die niet verliesloos te coderen is (`é` in ascii) of niet in N bytes past, wordt bij de opbouw geweigerd.

**Bytevolgorde.** Laatste argument: `be` of `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Zonder dat geldt die van de aanroep (`-endian`, standaard little). Ze verandert de waarden niet: `int(u,16,be)` en `int(u,16,le)` zijn aan elkaar toe te kennen.

**Uitlijning.** Standaard zijn producten compact (packed). `-align natural` (of `-align N`) voegt de opvulling van C toe: elk veld begint op een veelvoud van zijn uitlijning, de grootte van een product wordt naar boven afgerond. Opvulling met nullen bij het schrijven, genegeerd bij het lezen behalve met `-strictpad 1`.

#### 9.3 Commando's

```
type binary TYPE                                   ;# de representatie, zoals de kern ze ziet
type binary {what?}TYPE                            ;# idem
type binary {sizeof?}TYPE ?-align A?               ;# grootte in bytes (fout als ze van de gegevens afhangt)
type binary {is?}TYPE BYTES ?-exact 1?             ;# 1 als BYTES als TYPE te decoderen zijn, anders 0
type binary decode TYPE BYTES ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TYPE WAARDE ?-endian E? ?-align A? ?-dedupe 1?
type binary deref KOPPELING ?BYTES?                ;# volgt een niet-gevolgde koppeling één stap
type binary space file|memory|list|get|delete …    ;# benoemde ruimten
chan binary read  KANAAL TYPE ?opties?             ;# het kanaal schuift op
chan binary write KANAAL TYPE WAARDE ?opties?
```

De vragen (`{what?}`, `{sizeof?}`, `{is?}`) zijn **lokale annotaties** op het typeargument; acties zijn werkwoorden. `file binary` blijft een alias van de oude naam.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Niveau 2: wat het bestand over zichzelf zegt

Een grootte, een aantal, een positie of een variant kan uit het bestand worden gelezen: `$veld` in de argumenten van het type (een **reeds gelezen** veld van het huidige product of van een omvattend product), of een geheel type zonder teken dat als **voorvoegsel** dient.

| schrijfwijze | betekenis |
|---|---|
| `string(utf-8,int(u,16,be))` | tekst voorafgegaan door zijn grootte in bytes (Pascal-string) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N bytes, N berekend uit gelezen velden |
| `list(int(u,8)).T`, `list($n).T` | lijst voorafgegaan door haar aantal elementen, of n elementen |
| `at($off).T` | de waarde staat op positie `$off` (de cursor beweegt niet) |
| `corps($tag)` | de som `corps`, waarvan de variant door het veld `tag` gekozen wordt |
| `U(tp)`, `R($v)` (gebonden parameter) | een som waarvan de variant bij de definitie vastligt |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# variantnamen = waarden van het label
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Al bij de opbouw wordt een lijst gecontroleerd waarvan de grootte van een veld afhangt: `set {Batch}b(n,items) 3 {{1 1.0 0}}` wordt geweigerd (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Koppelingen: `*(oorsprong, repr, opties).T`

Een veld bevat vaak een adres, niet de waarde. Het koppelingstype leest het adres (`repr`, een geheel getal) en daarna de waarde `T` die daar staat.

| oorsprong | het adres telt vanaf |
|---|---|
| `//` (of `file://`) | het begin van de gegevens |
| `./` | het begin van de omvattende structuur |
| `../` | het begin van haar ouder |
| `.` | het veld zelf |
| `//naam/`, `memory://naam/` | een benoemde ruimte (bestand, of geheugensegmenten) |

Opties: `null` (adres 0 betekent „geen doel”), `null=V`. Met `opt(T)` geeft een nuladres `none`, anders `some`: dat is het verschil tussen „nul” en „onbestaand”. Bij het schrijven worden de doelen achteraan toegevoegd, uitgelijnd, en worden de adressen berekend; `-dedupe 1` schrijft identieke doelen maar één keer. Een koppelingsveld dat gelijk is aan zijn standaardwaarde, wordt als nulkoppeling geschreven.

**`-linkdepth`** regelt het aantal gevolgde koppelingen (standaard `Inf`). Een niet-gevolgde koppeling is een geannoteerd geheel getal (de absolute positie van het doel); `type binary deref` volgt haar één stap. Zo lees je alleen de kop van een groot bestand, doorloop je een gekoppelde lijst en herschrijf je een veld ter plaatse.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: het doel
```

#### 9.6 C-codecs

`type binary encode` en `decode` proberen eerst een weg in C, die vaste indelingen dekt: gehele getallen van 1, 2, 4, 8 bytes, zwevendekommagetallen van 4 en 8 bytes, producten (met `-align`), `list(N).T` en `list($n).T`. Buiten dat domein (tekenreeksen, sommen, koppelingen, `at`, bitvelden, berekende groottes, `-dedupe`), of bij een fout, neemt de Tcl-laag het over en geeft haar meldingen. `set ::tcl_typebinaryc 0` schakelt de C-weg uit. De geproduceerde bytes en waarden zijn op beide wegen identiek (test `t51_codec_c`). Metingen: § 15.

### 10. Fouten

De meldingen volgen het gebruik van Tcl: ze noemen de betrokken waarde, het type of het veld. De typespecifieke `errorCode`s beginnen met `TCL TYPE` of `TCL PARSE TYPE`.

| situatie | melding (voorbeeld) | `errorCode` |
|---|---|---|
| verboden teken in een type | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| onbekende typenaam op een variabelenaam | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| lokale annotatie op een variabelenaam | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| lokale annotatie opgeslagen door `set` | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| ongeldige waarde voor het type | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (afhankelijk van het basistype) |
| geen variant past | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| verschillende nominale typen | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| geen bekende omzetting | `unknown way to cast the value from mm to kg` | — |
| tegenstrijdige gestapelde annotaties | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| onbekend veld | `unknown field "zz" in pt`; `pt has no field "zz"` (pad) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| veld zonder waarde en zonder standaardwaarde | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| verkeerd aantal waarden | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| labels en posities gemengd | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| dubbel veld | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| afhankelijke grootte niet gerespecteerd | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| dict-sleutel ontbreekt of is niet toegestaan | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| lijstariteit | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| substitutie in een letterlijk pad | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| ongeldige lijstindex in een pad | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| koppeling naar een verdwenen variabele | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` op een kapotte koppeling | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| geannoteerde waarde buiten de parser gemaakt | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| binaire laag niet geladen | `the binary layer is not loaded` | — |

## Deel III — Implementatie

### 11. Overzicht

De implementatie bestaat uit **zeven nieuwe bestanden** (ongeveer 15 000 regels C), gekopieerd naar `generic/`, en uit **aanhaakpunten** in een twintigtal kernbestanden. De nieuwe bestanden worden nooit gepatcht: ze worden in hun geheel geleverd; alleen kernbestanden krijgen patches.

### 12. Het geannoteerde object

#### Structuur

Een geannoteerde waarde is een gewoon `Tcl_Obj` waarvan het interne type `annotated` is (`annotatedObjType`, in `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   de tekenreeks: die van de inhoud V (op        │
 │                  verzoek berekend door UpdateStringOfAnnotated)│
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   de inhoud: een gewoon, gedeeld Tcl_Obj          │
 │   ptr2 ──► T   de annotatie, in een van deze vormen:           │
 │                · een opgeloste typereferentie (typeRef)         │
 │                · de ruwe tekst van de annotatie (nog niet       │
 │                  opgelost: een notitie, of een type)            │
 │                · ""   de markering van de lege annotatie {}     │
 │                · een „annotstate” {T, N}: opgeloste T (of NULL) │
 │                  en N, de lijst van notities, van de oudste     │
 │                  naar de recentste                              │
 └───────────────────────────────────────────────────────────────┘
```

#### Eigenschappen

- **Transparant**: de tekenreeks is die van V. Voor elk commando dat geen annotaties kent, is de waarde V.
- **Onveranderlijk en gedeeld**: de toestand `{T, N}` is zelf een onveranderlijk object; `Tcl_DuplicateObj` deelt hem. Annoteren, afstapelen of een notitie verwijderen maakt een nieuw object.
- **Kosteloos zonder notitie**: zolang er geen notitie is, is `ptr2` alleen T.
- **Abstracte lijst** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… gaan via de lijstprocedures van het object (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). De elementen van een `list.T` komen getypeerd terug; een geannoteerde scalair is een lijst van één element dat hijzelf is.

#### Routines van het `Tcl_ObjType`

| routine | rol |
|---|---|
| `FreeAnnotated` | geeft V en T vrij (en de „zelf”-cel van een scalair, tabel `SelfCell`) |
| `DupAnnotated` | kopieert V, deelt T of de toestand `{T, N}` |
| `UpdateStringOfAnnotated` | de tekenreeks van V |
| `SetAnnotatedFromAny` | **weigert**: een geannoteerde waarde ontstaat uit de parser of een getypeerde toekenning, nooit uit een omzetting (`TCL VALUE ANNOTATED`) |

#### Belangrijkste functies (`tclAnnotObj.c`)

`TclAnnotateValue` (een annotatie zetten: cast of notitie), `TclAnnotSettle` (het afstapelen van een constructor, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (type, variant, onbekend, lokaal, ongeldig; de context vóór de globale typen), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (de kale inhoud, voor omzettingen van de kern), `TclAnnotDisplay` (`list<int>@mm.hello`), en de commando's `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. In de parser en de compiler

#### Het voorvoegsel `{x}woord`

In het oorspronkelijke Tcl 9 is een woord tussen accolades dat door andere tekens wordt gevolgd een syntaxfout (`extra characters after close-brace`), behalve `{*}`. Het annotatievoorvoegsel neemt dus een schrijfwijze in **die geen betekenis had**: geen geldig script verandert van betekenis.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: een woord `{tekst}rest` levert een token **`TCL_TOKEN_ANNOT`** (512, `tcl.h`) dat de tekst van de annotatie draagt, gevolgd door de tokens van de rest van het woord. Een geannoteerd woord `{T}naam(...)` loopt tot het sluitende haakje, spaties inbegrepen, op één regel.
- `tclCompile.c`, `TclCompileTokens`: als de rest van het woord letterlijk is (`TclAnnotSuffixIsLiteral`), wordt het woord gecompileerd tot `::tcl::annotraw tekst woord` (een ruwe notitie, zonder substitutie); anders tot `::tcl::annotate tekst waarde`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: een `set` met een geannoteerde naam gaat via `TclSetAnnotatedCmd`; een lokale annotatie op de waarde (`set x {-v}$mot`) levert een aanroep van `::tcl::annotlocal` op, die de fout geeft (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: een geannoteerd eerste argument delegeert naar `TclSetAnnotatedCmd` (`tclTypedVar.c`), de constructor.

#### De index `$naam(...)`: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: bij de `(` na een variabelenaam wordt **`TclParseVarIndex`** aangeroepen, dat de index leest tot de bijbehorende `)`: geneste haakjes, gepaarde labels `"..."`, substituties `$x`, `[cmd]`, `[(expr)]`, backslashes; accolades geweigerd. Is de index volgens die grammatica niet goed gevormd, dan volgt een exacte **terugval** op de regel van Tcl 9 (`info complete {a "c $dd("}` blijft waar).
- Alleen wanneer de uitgebreide syntaxis gebruikt wordt, worden de onderdelen in een token **`TCL_TOKEN_INDEX`** (1024, `tcl.h`) verpakt: een index die in Tcl 9 geldig is, behoudt precies zijn tokens. Kale woorden zijn niet betrokken: `puts a(b c)` blijft twee woorden.
- `${naam(...)}`: de hele naam is letterlijk; `Tcl_ObjGetVar2` en `INST_LOAD_STK` (via `TclTypedVarGetElementByName`) lezen het pad als de variabele een getypeerde scalair is.
- `tclTest.c`: `testparser` toont het token „index”.

#### Bytecode

| instructie | waar | rol |
|---|---|---|
| `INST_INDEX_CONCAT` („indexConcat”, 6 bytes: aantal `uint1` + masker `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, aangeroepen door `TclCompileVarSubst` en `TclPushVarName`), `tclExecute.c` | voegt de delen van een index samen tot een **pathindex**-object dat de gesubstitueerde stukken bewaart (`TclPathIndexNew`): een gesubstitueerde waarde blijft één blad van het pad |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (gecompileerde `dict for`, `dict map`), `tclExecute.c` | vergeet het type van een lusvariabele vóór de toekenning |

Niet-gecompileerde weg: `TclSubstTokens` (`SubstIndexTokens`) maakt hetzelfde pathindex-object.

#### Andere aanhaakpunten in de kern

| bestand | wijziging |
|---|---|
| `tclBasic.c` | `TclTypesInit`: maakt `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` neemt de trage weg voor een geannoteerde waarde; `INST_FOREACH_STEP` vergeet het type; lezen en schrijven van getypeerde elementen (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` en `TclTypedVarAfterStore` rond elke schrijfactie: het contract |
| `tclProc.c` | `TclTypedVarBindArg`: getypeerd binden van argumenten |
| `tclCmdAH.c`, `tclDictObj.c` | lusvariabelen (`TclTypedVarForgetByName`); `TclAnnotUnwrap` bij het lezen van dicts |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | omzettingen van de kern op de inhoud (`TclAnnotUnwrap`, macro `TCL_ANNOT_CONTENT`): een geannoteerde waarde wordt nooit ter plaatse omgezet |
| `tclListObj.c` | `lappend` via de `replaceProc` van een abstracte lijst; `lindex` met meerdere indices zonder afvlakken |
| `tclCompExpr.c` | operator `is` (lexeem `IS_TYPE`, `IsTypeAhead`), toekenningen `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. De toegevoegde bestanden

| bestand | regels | inhoud |
|---|---|---|
| `tclTypes.c` | 4 500 | typologie, `type def/alias/get/cast/convert/deref/binary`, validatie, verbreding, casts, C-codecs |
| `tclTypeParse.c` | 2 700 | parser en compiler van type-expressies (model: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | geparametriseerde typen, instanties, fantoomparameters, binaire verfijningen |
| `tclAnnotObj.c` | 2 150 | het geannoteerde object, de toestand `{T, N}`, het oplossen, `note` |
| `tclTypedVar.c` | 1 350 | getypeerde variabelen, constructors (`TclSetAnnotatedCmd`, `BuildProductEx`, veldlabels) |
| `tclTypePath.c` | 2 900 | padtaal, lezen, schrijven, opbouw via pad, pathindex-object |
| `tclVarRef.c` | 360 | `var`-koppelingen (canonieke naam, toegang, verouderde koppelingen) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | structuren (`TclType`, `TclTypeField`, `TclBinLink`…) en interfaces |

Aan de Tcl-kant: `layout/filebinary.tcl` (binaire laag: `type binary decode/encode/deref/space`, `chan binary`) en `layout/binaliases.tcl` (aliassen).

## Compatibiliteit, ontwerp, grenzen

### 15. Compatibiliteit en prestaties

**Geen geldig script verandert van betekenis.** Beide toegevoegde syntaxen nemen schrijfwijzen in die fouten waren: `{x}woord` („extra characters after close-brace”) en een uitgebreide index `$naam(...)`, die precies terugvalt op de regel van Tcl 9 zodra hij niet goed gevormd is. Controles:

- Tcl-tests `parse`, `parseExpr`, `parseOld`, `subst` (waaronder 257 tests van `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: resultaten identiek aan de basis;
- equivalentie met een ongewijzigde Tcl 9 op 18 000 gegenereerde gevallen (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): geen geldig resultaat veranderd;
- AddressSanitizer zonder fouten; geheugen stabiel over 1 000 000 opbouwen.

**Typen zijn niet de vijand van snelheid.** Metingen op het voorbeeld `cstruct` (10 000 metingen, `-align natural`):

| bewerking | Tcl-laag | C-codec | klassiek Tcl met de hand, zonder controle |
|---|---|---|---|
| codering (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format`; 330 µs gemeten voor 1 000) |
| decodering (`type binary decode`) | 485 ms | 3,0 tot 3,7 ms | 7,3 tot 10,5 ms (`binary scan`) |

De C-codec, die bovendien elke waarde tegen haar type **controleert**, is 2 tot 3 keer sneller dan klassiek met de hand geschreven Tcl, en 130 tot 250 keer sneller dan de generieke Tcl-laag.

### 16. Ontwerpkeuzes

| keuze | reden |
|---|---|
| `{x}woord` neemt een syntaxfout in | volledige compatibiliteit, geen nieuwe quoteringsregel |
| geen aaneengeplakt stapelen `{a}{b}$x` | dubbelzinnig met `{int}{300}`; je stapelt door een geannoteerde waarde te annoteren |
| alleen een constructor lost een type op | `{T}$x` buiten een constructor kost niets en mislukt nooit; gecontroleerd wordt waar de waarde wordt opgeslagen |
| lokale annotaties herkend aan hun tekens | een typenaam kan geen `-` of `?` bevatten: geen botsing mogelijk |
| `type def` nominaal, `type alias` structureel | eenheden (`mm`, `kg`) onderscheiden zonder de waarden te veranderen |
| automatische verbreding, expliciete versmalling | geen stil verlies van informatie |
| expliciete omzettingen tussen nominale typen | `mm` → `m` vraagt `{m}$a` of `type cast` |
| kale naam/waarde-vorm verwijderd | `set {png}p chunks $l` was op twee manieren te lezen; de namen staan nu in de bron (`{chunks}…` of `p(chunks)`), nooit geraden uit de gegevens |
| veldlabels: alles of niets | een mengeling zou een verkapte positionele vorm zijn |
| een pad substitueert niets | gegevens met `[script]` worden nooit door de padparser uitgevoerd (de dubbele substitutie verdwijnt); dezelfde regel als bij een array |
| atomaire gesubstitueerde waarden | `$D($k)` leest de sleutel `$k`, ook als die `.` of `,` bevat |
| pad + labels niet voorzien | geen aangetoonde behoefte; het gebruik zal het leren |

### 17. Grenzen en open punten

**Te corrigeren of te beslissen**

- **Overname door een ongetypeerde variabele.** `set e [lindex $L 1]` maakt `e` getypeerd en strikt. Daaruit volgt de valkuil `catch … m` (§ 6.2), en de fout die daarbij ontsnapt is de oorspronkelijke melding, wat haar moeilijk te begrijpen maakt. Te beslissen: stille overname, overname zonder contract, of een expliciete melding.
- **`;` en `,` gemengd zonder accolades** (`{int}a; {int}b, {int}c`) worden zonder fout aanvaard en leveren een anoniem veld op; de regel van de specificatie weigert ze.
- **Een variant lezen die niet de huidige variant** van een som is, geeft `unknown way to cast the value from S to string` in plaats van een melding die de variant noemt.
- **Varianten van dezelfde vorm**: zonder label wordt zonder waarschuwing de eerste genomen.
- `set {paire}s {aut.nom}x` (het veld van een **ander** product) geeft `wrong # args`; een eigen melding zou duidelijker zijn.
- `lset` met een indexlijst via `TclLsetList` houdt een referentie te veel vast (lek, zonder crash).

**Nog niet gedaan**

- Een „onbestaande” waarde in het algemeen: `opt(T)` dekt binaire koppelingen, een algemene vorm bestaat niet.
- `union`, gedeelde bitvelden, een lijst „tot het einde van het bestand” (het weigeren van `union` is bewust).
- Een commentaarannotatie `{#}` op parserniveau, annotatie van het eerste woord (`{c}proc`), declaratieannotaties (`proc p {{?}{…} {!}arg}`).
- Notities per element van een getypeerde lijst (nu geldt een notitie voor de hele lijst); alle notities wissen met `{@}$x`.
- C-codecs voor sommen, koppelingen en berekende groottes; caching van het geanalyseerde type.
- De C-testbank met nabootsing (`typeparse/test/`) moet worden bijgewerkt; de voorbeelden `arbre_var.tcl` en `defauts_calcules.tcl` moeten worden gecorrigeerd.

## Bijlagen

### A. Installatie en tests

Het prototype vertrekt van Tcl `core-9-0-branch` met de TIP 759-tak. Vanuit de wortel van de Tcl-boom (MSYS2 / MinGW64 of Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # pakt uit, kopieert de typebronnen, past de kernpatches toe,
                                        # bouwt opnieuw en vergelijkt elke tests_tclsh/tNN_*.tcl met haar .out
```

- De typebronnen worden in hun geheel geleverd in `typeparse/` en naar `generic/` gekopieerd; alleen de kernpatches die in `noyau/ORDRE.txt` staan, worden toegepast, zonder fuzz (`-F0`), waarbij een al toegepaste patch wordt herkend en overgeslagen.
- Opties van `maj.sh`: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "bericht"`.
- Notities bij elke patch: `noyau/LISEZMOI.md`. Uitvoerbare voorbeelden: `exemples/`.

### B. Woordenlijst

| term | betekenis |
|---|---|
| annotatie | tekst tussen accolades vóór een woord geplakt: `{x}woord` |
| notitie | niet-opgeloste annotatie, die de waarde volgt |
| type | annotatie opgelost in de bekende typologie |
| lokale annotatie | annotatie waarvan de tekst geen type kan zijn (`{-v}`, `{is?}`); ze leeft alleen tussen de aanroeper en het commando |
| constructor | wat een waarde opslaat en haar annotaties oplost: `set {T}v`, binden van argumenten, `lassign`, `foreach`, toekenning in `expr` |
| contract | het type van een variabele, bij elke schrijfactie gecontroleerd |
| overname | een ongetypeerde variabele neemt het type van de ontvangen waarde aan |
| cast | expliciete omzetting naar een type (`{T}$x` in een constructor, `type cast`) |
| nominaal | type gemaakt door `type def`, verschillend van zijn definitie; wijkt voor zijn basis |
| verfijning | beperkt basistype, met een binaire representatie (`int(u,8)`) |
| verbreding | automatische overgang van een verfijning naar een breder type |
| product | `struct`: benoemde, geordende velden |
| som | één benoemde variant uit meerdere |
| variant | een van de takken van een som |
| veldlabel | annotatie die het veld van een argument benoemt: `{age}12` |
| pad | index van een getypeerde waarde: `R(A.(x,y))` |
| pathindex | object dat een index en zijn gesubstitueerde delen draagt |
| koppeling | `var.T` (naar een variabele) of `*(…).T` (naar een binair adres) |
| fantoomparameter | typeparameter die de waarden niet verandert maar de instanties onderscheidt |

### C. Overeenkomsten

| hier | C | Rust | LLVM |
|---|---|---|---|
| product | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| som | (`union` + discriminant met de hand) | `enum` met gegevens | — |
| opsomming (som van letterlijke waarden) | `enum` | `enum` zonder gegevens | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (zonder teken) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | met NUL afgesloten `char *` | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 bits) | `*const T` | `ptr` |
| `opt(T)` | pointer die `NULL` kan zijn | `Option<T>` | — |
| nominaal type (`type def mm double`) | `typedef` (zonder controle) | newtype `struct Mm(f64)` | — |
| `-align natural` / packed | standaarduitlijning / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | uitlijning / `<{ }>` |
