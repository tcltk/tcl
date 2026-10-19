# Handleiding geannoteerde typen — hoofdstukken

*(Nederlandse vertaling van `MANUEL.md`, dat de referentie blijft. Dit bestand
verzamelt de hoofdstukken van de handleiding naarmate ze geschreven worden;
de patchnotities blijven in `noyau/LISEZMOI.md`. De namen in de voorbeelden
zijn gelijk gehouden aan het Franse origineel, zodat ze overeenkomen met de
voorbeeldscripts en de foutmeldingen.)*

## Dict en product: twee manieren om een structuur te beschrijven

Voor wie uit C komt: een Tcl-**lijst** is een array, een **dict** is een
structuur met vrije sleutels, een **producttype** is een gedeclareerde
`struct`. De laatste twee lijken sterk op elkaar, en men gaat vaak van het
een naar het ander.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# product
type def fiche  {dict(nom, age?=0).string}              ;# getypeerde dict
```

### Wat ze gemeen hebben

| | product `pt` | dict `fiche` |
|---|---|---|
| opbouw met namen | `set {pt}a {nom}Jean {age}12` of `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| toegang tot een veld | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| standaardwaarde van veld/sleutel | `age ?= 0` (of `=`) | `age?=0` |
| onbekend veld | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| geserialiseerde waarde | Tcl-lijst | Tcl-lijst van sleutel/waarde |

Een standaardwaarde schrijf je op dezelfde manier: `?=` geldt in beide
gevallen, en een standaardwaarde kan een letterlijke waarde zijn, `[script]`
(bij elke opbouw berekend) of `$variabele`. In een product wordt de oude `=`
nog steeds aanvaard.

### Wat ze onderscheidt

* **Aanwezige velden.** In een product bestaan alle velden altijd: een
  weggelaten veld krijgt zijn standaardwaarde, en een veld zonder
  standaardwaarde moet worden opgegeven
  (`field "x" of pt has no value and no default`). In een dict is een
  sleutel zonder `?` verplicht (`key "nom" is missing in fiche`) en mag een
  sleutel met `?` ontbreken.
* **Volgorde.** Een product heeft geordende velden; het kan ook
  **positioneel** worden opgebouwd (`set {pt}b Jean 12`, zoals de
  initialisatie van een C-struct). Een dict heeft geen positionele vorm.
* **Eén enkel veld.** Een product aanvaardt het voorvoegsel `{T.veld}`:
  `set {pt}c {pt.nom}Jean` zet `nom`, de andere velden krijgen hun
  standaardwaarde (`Jean 0`). Het is dezelfde schrijfwijze als `{Val.i}5`
  voor een som.
* **Veldtypen.** Elk veld van een product heeft zijn eigen type
  (`{string}nom; {int}age`); een `dict(...)` heeft er één voor alle waarden
  (`.string`).
* **Grootte.** Een product heeft een vast aantal velden, vastgelegd bij de
  definitie.

### Velden benoemen: labels of pad (geen kale naam/waarde meer)

De kale naam/waarde-vorm (`set {pt}a nom Jean age 12`) is **verwijderd**: ze
viel samen met de positionele vorm zodra *n* argumenten bij *n* velden
pasten. Een product wordt nu op drie manieren opgebouwd, zonder iets te
raden:

* **positioneel**: evenveel waarden als velden, in volgorde
  (`set {pt}b Jean 12`);
* **veldlabels**: `set {pt}a {age}12 {nom}Jean`. Het label is de meest
  recente annotatie van elk argument; het wordt gelezen in de context van het
  product, dus een veldnaam gaat voor op een globaal type met dezelfde naam
  (`type def age int` verandert niets), zoals de naam van een variant bij een
  som. Vrije volgorde; weggelaten velden krijgen hun standaardwaarde.
  **Alles of niets**: labels en positionele waarden mengen is een fout
  (`can't mix values named by a field annotation …`), een onbekend label ook
  (`unknown field "taille" in pt`). Een waarde die haar label in een
  variabele meedraagt telt mee (`set v {age}12; set {pt}a {nom}Jean $v`);
* **pad**: `set {pt}a(age,nom) 12 Jean`, handig wanneer de namen berekend
  worden of de waarden tussen accolades staan.

De dict houdt zijn sleutel/waarde-vorm: een dict heeft geen positionele vorm.

### Kiezen

* Een **bekende, stabiele** verzameling velden met verschillende typen:
  product.
* **Vrije of optionele** sleutels van hetzelfde type: dict.
* Een formaat met meerdere mogelijke vormen (PNG, GIF…): een **som** van
  producten (zie `exemples/png_somme.tcl`).

## bitarray: een geheel getal waarvan je de bits leest

`bitarray(n, volgorde)` is een geheel getal van n bits (0 ≤ v < 2^n). De
volgorde zegt waar index 0 begint:

| | `lsb0` (standaard) | `msb0` |
|---|---|---|
| bit met index 0 | het RECHTSE (`1 << 0`) | het LINKSE |
| gebruik | registers, maskers, C | protocoldiagrammen (RFC) |
| index van een bit als de breedte verandert | ongewijzigd | verschoven |

Neem de byte `0xB2`, geschreven `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Lezen.** `$b(i)` geeft het bit (0 of 1). `$b(i:j)` geeft de bits i tot j
als ÉÉN geheel getal, van type `bitarray(j-i+1, volgorde)`: `$b(1:4)` is 9
bij lsb0; `$c(0:3)` is 11 bij msb0 (de vier linkse bits, `1011`).

**Schrijven.** `set b(0) 1`, `set b(4:7) 0`. In `expr` schrijf je een
deelreeks tussen aanhalingstekens, want de `:` hoort bij de parser van
`expr`: `expr {"b(1:2)" = 3}`. Een waarde die niet in de beoogde bits past,
wordt geweigerd.

**Twee volgordes, twee typen.** `bitarray(8,lsb0)` en `bitarray(8,msb0)`
mengen niet: het ene aan het andere toekennen is een `type mismatch`. De
standaard `bitarray(8)` is `bitarray(8,lsb0)`.

**Functies.** `popcount(x)`: aantal bits op 1; `bitlen(x)`: breedte van een
bitarray, anders het aantal bits van het gehele getal.

**In andere typen.** Een productveld (`{bitarray(8)}flags`), een
lijstelement (`list.bitarray(4)`) of een procedurevariabele zijn bitarrays
zoals alle andere: `set r(flags.7) 1`, `set L(2.3)`.

**Lijkt het op een dict?** Nee: geen sleutels of velden, één enkel geheel
getal. Om de velden van een binaire kop te beschrijven, combineer je een
product van bitarrays (zie het vorige hoofdstuk).

Volledige voorbeelden: `exemples/bitarray_exemples.tcl` (de „packed“-byte
van een GIF en TCP-vlaggen in `msb0`, Unix-rechten in `lsb0`).

## type deref: een momentopname van de koppelingen

Een `var`-koppeling bevat de NAAM van een variabele en volgt haar waarde bij
elke lezing. `type deref` vervangt elke koppeling door de waarde die de
variabele OP DAT MOMENT heeft: het resultaat verandert daarna niet meer.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : de koppeling volgt B
    puts $v                             ;# 1 2 3 : de momentopname ligt vast

* Het resultaat heeft het type „zonder koppelingen“: `list.var.int` wordt
  `list.int`, en gedraagt zich als elke lijst van gehele getallen.
* Een ontbrekende variabele stopt alles met een fout die de plaats noemt:
  `type deref: at 2: can't read "::C": no such variable`. Er is geen
  gedeeltelijk resultaat en de oorspronkelijke waarde blijft onaangeroerd.
* Er wordt maar één niveau gevolgd: als een doelvariabele zelf koppelingen
  bevat, blijven die behouden; een tweede `type deref` volgt ze.
* Werkt in lijsten, dicts, producten en SOMMEN, genest of niet, en in
  RECURSIEVE typen (bomen zoals `tree(var)`). De variant van een som blijft
  behouden.
* Grafen worden per niveau behandeld: als de buren van een knoop koppelingen
  zijn (`{list.var.node}next`), geeft `type deref` de knoop met zijn buren
  vervangen door hun waarden, die nog koppelingen bevatten; een tweede
  `type deref` daalt één niveau af. Een cyclus loopt dus nooit vast.
* Het type van het resultaat is een eigen type, `deref(<oorspronkelijk type>)`
  genoemd (`deref(tree(var))`, `deref(mix)`), één keer berekend en daarna
  hergebruikt: twee momentopnamen van hetzelfde type hebben hetzelfde type.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# een boom waarvan de waarden koppelingen zijn
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

Zie ook `MANUAL-deref.md` (Engels): het koppelingstype `var` en
`type deref`, met voorbeelden in `exemples/deref_manual.tcl`.

## Som en C-`union`: niet verwarren

Voor wie uit C komt: een **som** (`{{A}x, {B}y}`) lijkt op een `union`, maar
het is niet hetzelfde.

| | C-`union` | som |
|---|---|---|
| wat ze bevat | **dezelfde bytes**, op meerdere manieren gelezen | **één enkele** variant, die haar naam draagt |
| wie weet welke geldig is | niemand: de programmeur moet het weten | de waarde zelf (`type get` geeft `S.x`) |
| de verkeerde variant lezen | toegestaan, je leest willekeurige bytes | onmogelijk: je opent de variant die je hebt |
| grootte in een bestand | die van de grootste weergave | die van de gekozen variant |
| keuze van de variant | geen, die bestaat niet | door een reeds gelezen veld (`S($tag)`) of door haar naam (`S(x)`) |

Gevolgen:
* Er is **bewust geen `union` in dit model**: de beveiligingen blijven. Een
  som kan niet „scheef“ gelezen worden.
* Wanneer het formaat niet zegt welke variant geldig is (de
  `Tcl_ObjInternalRep` van Tcl, waarvan de discriminant het adres `typePtr`
  is, buiten de union), wordt de variant **bij de aanroep** gekozen:
  `Tcl_ObjAs(twoPtrValue)` (zie `exemples/tcl_obj.tcl`). De aanroeper weet
  het, zoals in C, maar het type blijft gecontroleerd.
* Een som waarvan meerdere varianten **dezelfde vorm** hebben, is niet
  dubbelzinnig zodra de variant op naam gekozen wordt (`S(y)`): er valt
  niets meer te raden.

### Bestaan, nul zijn, niet bestaan

Verwar niet drie situaties voor een waarde waarnaar een koppeling wijst (type
`*(...)`, komt nog):
* **aanwezig**: de koppeling wijst een waarde aan;
* **aanwezig maar nul of leeg**: de waarde bestaat, ze is `0`, `{}` of `""`;
* **onbestaand**: de koppeling is nul (offset 0, bijvoorbeeld), er valt
  niets te lezen.

Een standaardwaarde van een veld (`= {}`) onderscheidt de laatste twee niet.
Een som met een variant zonder waarde (`{{T}some, {}none}`) onderscheidt ze
wel. Dat is nog niet geïmplementeerd, zie de ontwerpnotitie.

## Binaire typen: bytes lezen en schrijven (`type binary`, `chan binary`)

Een veld zegt, via zijn TYPE, hoe het als bytes geschreven wordt. Verder
niets te leren: het is het principe van `bitarray(16)`, dat zijn breedte al
draagt. De `.` dient alleen om SAMEN TE STELLEN (`list(3).T`); de
representatie staat tussen de haakjes van het type.

    type def mz {"MZ"}                           ;# een constante: som van één letterlijke waarde
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| type | representatie |
|---|---|
| `int(u,n)` / `int(s,n)` | geheel getal zonder / met teken van n bits (veelvoud van 8, 8 tot 1024) |
| `double(32)` `double(64)` | IEEE-zwevendekommagetal |
| `string(raw,N)` | N ruwe bytes |
| `string(ENC,N)` | tekst op N bytes, aangevuld met NUL |
| `string(ENC,z)` | tekst afgesloten met een NUL (2 bytes in utf-16, ucs-2) |
| `bitarray(n)` | n bits (veelvoud van 8), gelezen als geheel getal zonder teken |
| `"MZ"` (som van letterlijke waarden van gelijke lengte) | een constante, gecontroleerd door het type |
| `list(N).T`, een product van deze typen | N elementen; nesting is toegestaan |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Een tekst
die niet verliesloos te coderen is (`é` in ascii) of niet in N bytes past,
wordt geweigerd.

**Bytevolgorde.** Laatste argument: `be` (big-endian, netwerkvolgorde) of
`le`: `int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Zonder dat
geldt die van de aanroep (`-endian`, standaard little). Ze verandert de
waarden niet: `int(u,16,be)` en `int(u,16,le)` zijn uitwisselbaar.

**Automatische verbreding.** Een verfijning past waar een breder type
verwacht wordt:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: elke int(u,8) is een int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 past in -32768..32767
    set {int(u,8)}v $n               ;# geweigerd: versmalling (expliciete cast)
    set {list.int(u,8)}L $liste_int  ;# ook voor lijsten geweigerd

Een type gedefinieerd met `type def port int(u,16)` blijft nominaal: `port`
past in een int, maar een gewone `int(u,16)` past niet in een `port`.

**Uitlijning.** Standaard zijn producten compact (packed), zoals netwerk- en
PE/ELF-koppen. `-align natural` (of `-align N`) voegt de opvulling van C en
LLVM toe: elk veld begint op een veelvoud van zijn uitlijning (zijn grootte,
begrensd op 8 of N), de grootte van een product wordt naar boven afgerond op
zijn uitlijning. Opvulling met nullen bij het schrijven, genegeerd bij het
lezen behalve met `-strictpad 1`.

    type binary entete                           ;# de representatie, zoals de kern ze ziet
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# het kanaal schuift op
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

Lezen en schrijven zijn in Tcl geschreven (`layout/filebinary.tcl`) boven op
de kern: `type binary` beschrijft, decodeert en codeert bytes in het
geheugen, `chan binary` leest en schrijft op een kanaal. De naam
`file binary` (de oude) blijft beschikbaar als alias.

**`type binary`, vragen en werkwoorden.** Vragen over een type schrijf je als
een lokale annotatie op het typeargument (de afsluitende `?` onderscheidt ze
van elke typenaam); acties op bytes blijven werkwoorden:

    type binary entete                          ;# de representatie (kale vorm)
    type binary {what?}$T                       ;# idem
    type binary {sizeof?}$T ?-align A?          ;# grootte in bytes (fout als variabel)
    type binary {is?}$T $octets ?-exact 1?      ;# 1 als $octets als T te decoderen is, anders 0
    type binary decode T $octets ...            ;# leest de bytes in het geheugen
    type binary encode T $valeur ...            ;# idem, encode
    type binary deref ...                       ;# idem, deref
    type binary space ...                       ;# idem, space (benoemde ruimten)

`{is?}` aanvaardt standaard een geldig voorvoegsel (een kop herkennen);
`-exact 1` eist dat alle bytes verbruikt worden. Een ongeldige optie, een
onbekend type of een type zonder binaire representatie blijven fouten: alleen
het mislukken van de decodering geeft 0. Een onbekende annotatie die op `?`
eindigt, is een fout (`unknown question "oups?"`); elke andere annotatie
wordt genegeerd. Een type met de naam van een werkwoord
(`type def decode ...`) wordt nog steeds beschreven door
`type binary decode`, zolang er geen ander argument volgt. `decode`,
`encode`, `deref` en `space` vereisen dat `layout/filebinary.tcl` geladen is
(anders: `the binary layer is not loaded`).

**Korte aliassen.** `layout/binaliases.tcl` geeft korte namen aan de
gebruikelijke representaties (de conventie van Rust en Zig):
`u8 u16 u32 u64`, `i8 i16 i32 i64`, `f32 f64`, met de achtervoegsels
`be`/`le` (`u16be`, `i32le`, `f64be`), en `cstring`, `wstringz`, `latin1z`.
Het zijn `type alias`: de bibliotheek dient ook als geheugensteun voor
binaire formaten. De `i8…i64` hier hebben een teken; de `iN` van LLVM hebben
geen teken, er is dus geen exacte overeenkomst (de arrays `[N x T]` en de
packed-structuren komen overeen met `list(N).T` en met de producten).


### Niveau 2: groottes, aantallen, posities en varianten uit het bestand

Altijd dezelfde syntaxis: `$veld` in de argumenten van het type, zoals de
parameters van geparametriseerde typen, of een geheel type zonder teken dat
als voorvoegsel dient.

| schrijfwijze | betekenis |
|---|---|
| `string(utf-8,int(u,16,be))` | tekst voorafgegaan door zijn grootte in bytes (Pascal-string) |
| `string(raw,$len)` | N bytes, waarbij N de waarde van het veld `len` is |
| `string(utf-8,$len - 4)` | een Tcl-expressie over reeds gelezen velden |
| `list(int(u,8)).T` | lijst voorafgegaan door haar aantal elementen |
| `list($n).T` | n elementen, waarbij n een veld is |
| `at($off).T` | de waarde staat op positie `$off` van het bestand (de cursor beweegt niet) |
| `corps($tag)` | de som `corps`, waarvan de variant door het veld `tag` gekozen wordt |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# variantnamen = labels
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Regels: een `$veld` moet een **reeds gelezen** veld aanwijzen van het huidige
product of van een omvattend product (`type binary` controleert dat); een
productveld lees je als `$h(off)`. Bij het schrijven is een grootte of
aantal dat niet past bij het veld waarvan het afhangt een fout, en het label
moet dat van de variant zijn. `type binary {sizeof?}` weigert een type
waarvan de grootte van de gegevens afhangt. Volledig voorbeeld (PE, ELF,
berichten): `exemples/niveau2.tcl`.

Voorbeelden: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Grenzen:
geen unions of gedeelde bitvelden, geen lijst „tot het einde van het
bestand“.

#### Gekozen variant: `S(naam)` en typeparameter

Een som `S` kan haar variant ook **bij de definitie gekozen** krijgen,
zonder selectieveld in het bestand:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# de variant tp, voluit geschreven
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# of gegeven door een parameter
type binary decode {R(tp)} $octets
```

`U(tp)` is een type met één variant: zijn grootte is bekend
(`type binary {sizeof?}`), de gelezen waarde is `U(tp).tp` en er valt niets
te raden, zelfs als meerdere varianten dezelfde vorm hebben (wat de waarde
vroeger dubbelzinnig maakte). `U($veld)` behoudt zijn betekenis: variant
gekozen door een reeds gelezen veld. Als `$v` een gebonden parameter van het
type is, geldt het eerste geval. Voorbeeld: `exemples/tcl_obj.tcl`
(`Tcl_ObjAs(twoPtrValue)`). Test: `t36_somme_choisie.tcl`.

#### Het koppelingstype `*(oorsprong,repr).T`

Een bestandsveld bevat vaak een adres, niet de waarde. Het koppelingstype
leest het adres (`repr`, een geheel getal) en dan de waarde `T` die daar
staat; de waarde van het veld is die van het doel.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Oorsprongen, zoals in een bestandssysteem: `//` (of `file://`) begin van de
gegevens, `./` begin van de omvattende structuur, `../` de ouder ervan, `.`
het veld zelf (zie hieronder). Opties: `null` (adres 0 betekent „geen
doel“), `null=V`. Met `opt(T)` (zie `unit`) geeft een nuladres `none`,
anders `some`: dat is het verschil tussen „nul“ en „onbestaand“.

Generieke aliassen (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) geven
korte namen: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` in
`layout/binaliases.tcl`. Bij het schrijven worden de doelen achteraan
toegevoegd, uitgelijnd, en worden de adressen berekend. Recursieve typen,
`memory://` en benoemde ruimten worden verderop beschreven. Details:
`noyau/CONCEPTION-lien.md`.

**De punt `.`: het adres is relatief ten opzichte van de plaats van het
veld.** `./` markeert het *begin van de structuur*; `.` markeert *het veld
zelf* (de plaats waar het adres geschreven staat). Het is de oorsprong van
„vanaf hier“-afstanden, gebruikelijk in formaten met relatieve offsets
(`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# het veld r staat op offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# zelfde resultaat, maar met ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Geheugensteun: `//` begin van de gegevens · `./` begin van de structuur ·
`../` begin van de ouderstructuur · `.` positie van het veld. De notatie `.`
blijft bewust kort; een sprekender naam wordt pas na gebruik overwogen.

#### Hoe ver koppelingen volgen: `-linkdepth`

Een koppeling kan meteen gevolgd of zo gelaten worden. De aanroepoptie
`-linkdepth` regelt het aantal gevolgde koppelingen (zoals het
doordringingsvermogen van straling: gammastraling gaat overal doorheen, een
zwak doordringende straal stopt bij de huid):

    type binary decode H $d                   ;# 1 {7 8}   Inf, de standaard: alles volgen
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   één niveau koppelingen
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      geen koppeling gevolgd
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Een niet-gevolgde koppeling is een geannoteerd geheel getal (absolute positie
van het doel in de gegevens). Kopiëren, `lindex` en `string cat` behouden de
annotatie; een berekening verliest ze, en het resultaat kan niet meer gevolgd
worden. Je volgt een koppeling op verzoek, stap voor stap:

    type binary deref $h(r) $d                ;# 7 8   (standaard -linkdepth 0: één enkele stap)

Wat het oplevert: alleen de kop van een groot bestand lezen; een gekoppelde
lijst of een boom doorlopen (het type kan recursief zijn:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); een beschadigde koppeling
opsporen; een veld ter plaatse wijzigen, want `type binary encode` schrijft
een niet-gevolgd adres ongewijzigd terug in plaats van het bestand te
herschikken. Een cyclus is alleen met `Inf` een fout.


## Benoemde ruimten

Een koppeling kan een op naam geregistreerde ruimte aanwijzen in plaats van
de gegevens van de aanroep.

```tcl
set sp [type binary space file $octets -name lib]          ;# geannoteerde waarde van type space
type binary space memory $octets {{4096 4 2}} -name mem    ;# segmenten {adres offset grootte}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ is gelijkwaardig
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# geen GEGEVENS: de ruimte volstaat
type binary space list | get NAAM | delete NAAM
```

De standaardnaam wordt berekend (`space1`, …); het aanmaakcommando
registreert de ruimte; een reeds gebruikte naam is een fout. `//naam/` eist
een ruimte zonder segmenten, `memory://naam/` een ruimte met segmenten; een
adres buiten de segmenten is een fout. In een benoemde ruimte kan geen echt
doel geschreven worden: alleen het grensadres (`-linkdepth 0`) wordt
herschreven.

## Een nulkoppeling schrijven

Een koppelingsveld met een standaardwaarde, waarvan de waarde gelijk is aan
die standaardwaarde, wordt geschreven als een nulkoppeling (het „nul“-adres
van de koppeling, er wordt geen doel geschreven). Lezen doet het omgekeerde:
nuladres → standaardwaarde. Zonder standaardwaarde (en zonder `opt`) blijft
het lezen van een nulkoppeling een fout.

## -dedupe: identieke doelen delen

`type binary encode TYPE WAARDE -dedupe 1` (ook `write`) schrijft
koppelingsdoelen met identieke bytes maar één keer; de betrokken koppelingen
krijgen hetzelfde adres. Standaard (`-dedupe 0`) wordt elk doel geschreven.
Alleen doelen zonder geneste koppeling worden gedeeld (de interne adressen
van een doel hangen af van zijn positie); opnieuw inlezen geeft dezelfde
waarde.

## Grootte die van een veld afhangt: gecontroleerd bij de opbouw

Een lijst waarvan het aantal elementen een expressie over andere velden is
(`{list($n).Sample}items`, `list($n - 1).u8`), wordt gecontroleerd zodra het
product wordt opgebouwd: `set {Batch}b(n,items) 3 {{1 1.0 0}}` wordt
geweigerd (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).
Weggelaten velden tellen mee met hun standaardwaarde. De controle dekt geen
toekenningen veld per veld (`set b(n) 3`: een tussentoestand is legitiem);
`type binary encode` controleert opnieuw bij het schrijven. Bytegroottes van
tekenreeksen (`string(raw,$len)`) worden bij de opbouw niet gecontroleerd.
