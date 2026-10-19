# Handbuch der annotierten Typen — Kapitel

*(Deutsche Übersetzung von `MANUEL.md`, das die Referenz bleibt. Diese Datei
sammelt die Kapitel des Handbuchs, sobald sie geschrieben sind; die
Patch-Notizen bleiben in `noyau/LISEZMOI.md`. Die Bezeichner in den Beispielen
sind wie im französischen Original belassen, damit sie zu den
Beispielskripten und Fehlermeldungen passen.)*

## Dict und Produkt: zwei Arten, eine Struktur zu beschreiben

Für C-Programmierer: Eine Tcl-**Liste** ist ein Array, ein **Dict** ist eine
Struktur mit freien Schlüsseln, ein **Produkttyp** ist ein deklariertes
`struct`. Die beiden letzten ähneln sich sehr, und man wechselt oft vom einen
zum anderen.

```tcl
type def pt     {{string}nom = "?"; {int}age ?= 0}     ;# Produkt
type def fiche  {dict(nom, age?=0).string}              ;# typisiertes Dict
```

### Was sie gemeinsam haben

| | Produkt `pt` | Dict `fiche` |
|---|---|---|
| Aufbau mit Namen | `set {pt}a {nom}Jean {age}12` oder `set {pt}a(nom,age) Jean 12` | `set {fiche}e nom Jean` |
| Zugriff auf ein Feld | `$a(nom)` | `dict get $e nom`, `$e(nom)` |
| Vorgabe eines Felds/Schlüssels | `age ?= 0` (oder `=`) | `age?=0` |
| unbekanntes Feld | `unknown field "zz" in pt` | `key "zz" is not allowed in fiche` |
| serialisierter Wert | Tcl-Liste | Tcl-Liste aus Schlüssel/Wert |

Vorgaben werden gleich geschrieben: `?=` gilt in beiden Fällen, und eine
Vorgabe kann ein Literal, `[script]` (bei jedem Aufbau berechnet) oder
`$variable` sein. In einem Produkt wird das alte `=` weiterhin akzeptiert.

### Was sie unterscheidet

* **Vorhandene Felder.** In einem Produkt existieren alle Felder immer: Ein
  weggelassenes Feld erhält seine Vorgabe, und ein Feld ohne Vorgabe muss
  angegeben werden (`field "x" of pt has no value and no default`). In einem
  Dict ist ein Schlüssel ohne `?` Pflicht (`key "nom" is missing in fiche`),
  und ein mit `?` markierter Schlüssel darf fehlen.
* **Reihenfolge.** Ein Produkt hat geordnete Felder; es lässt sich auch
  **positionell** aufbauen (`set {pt}b Jean 12`, wie die Initialisierung
  eines C-struct). Ein Dict hat keine positionelle Form.
* **Ein einzelnes Feld.** Ein Produkt akzeptiert das Präfix `{T.feld}`:
  `set {pt}c {pt.nom}Jean` setzt `nom`, die anderen Felder erhalten ihre
  Vorgabe (`Jean 0`). Das ist dieselbe Schreibweise wie `{Val.i}5` für eine
  Summe.
* **Feldtypen.** Jedes Feld eines Produkts hat seinen eigenen Typ
  (`{string}nom; {int}age`); ein `dict(...)` hat einen einzigen für alle
  Werte (`.string`).
* **Größe.** Ein Produkt hat eine feste Anzahl von Feldern, festgelegt in
  der Definition.

### Felder benennen: Etiketten oder Pfad (keine nackte Name/Wert-Form mehr)

Die nackte Name/Wert-Form (`set {pt}a nom Jean age 12`) ist **entfernt**:
Sie war mit der positionellen Form verwechselbar, sobald *n* Argumente zu
*n* Feldern passten. Ein Produkt wird jetzt auf drei Arten aufgebaut, ohne
dass etwas erraten wird:

* **positionell**: so viele Werte wie Felder, in Reihenfolge
  (`set {pt}b Jean 12`);
* **Feldetiketten**: `set {pt}a {age}12 {nom}Jean`. Das Etikett ist die
  jüngste Annotation jedes Arguments; es wird im Kontext des Produkts
  gelesen, daher geht ein Feldname einem globalen Typ gleichen Namens vor
  (`type def age int` ändert nichts), wie der Name einer Variante bei einer
  Summe. Beliebige Reihenfolge; weggelassene Felder erhalten ihre Vorgabe.
  **Alles oder nichts**: Etiketten und positionelle Werte zu mischen ist ein
  Fehler (`can't mix values named by a field annotation …`), ein unbekanntes
  Etikett ebenso (`unknown field "taille" in pt`). Ein Wert, der sein
  Etikett in einer Variablen mitbringt, zählt
  (`set v {age}12; set {pt}a {nom}Jean $v`);
* **Pfad**: `set {pt}a(age,nom) 12 Jean`, nützlich, wenn die Namen berechnet
  werden oder die Werte in geschweiften Klammern stehen.

Das Dict behält seine Schlüssel/Wert-Form: Ein Dict hat keine positionelle
Form.

### Auswählen

* Eine **bekannte, stabile** Menge von Feldern mit verschiedenen Typen:
  Produkt.
* **Freie oder optionale** Schlüssel desselben Typs: Dict.
* Ein Format mit mehreren möglichen Formen (PNG, GIF…): eine **Summe** von
  Produkten (siehe `exemples/png_somme.tcl`).

## bitarray: eine ganze Zahl, deren Bits man liest

`bitarray(n, ordnung)` ist eine n-Bit-Ganzzahl (0 ≤ v < 2^n). Die Ordnung
gibt an, wo der Index 0 beginnt:

| | `lsb0` (Vorgabe) | `msb0` |
|---|---|---|
| Bit mit Index 0 | das RECHTE (`1 << 0`) | das LINKE |
| Verwendung | Register, Masken, C | Protokolldiagramme (RFC) |
| Index eines Bits bei geänderter Breite | unverändert | verschoben |

Nehmen wir das Byte `0xB2`, geschrieben `10110010`:

    set {bitarray(8)}b 0xB2
    set {bitarray(8,msb0)}c 0xB2
    lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1   (lsb0)
    lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0   (msb0)

**Lesen.** `$b(i)` liefert das Bit (0 oder 1). `$b(i:j)` liefert die Bits i
bis j als EINE ganze Zahl vom Typ `bitarray(j-i+1, ordnung)`: `$b(1:4)` ist 9
bei lsb0; `$c(0:3)` ist 11 bei msb0 (die vier linken Bits, `1011`).

**Schreiben.** `set b(0) 1`, `set b(4:7) 0`. In `expr` wird ein Ausschnitt
in Anführungszeichen geschrieben, denn das `:` gehört dem Parser von `expr`:
`expr {"b(1:2)" = 3}`. Ein Wert, der nicht in die Ziel-Bits passt, wird
abgelehnt.

**Zwei Ordnungen, zwei Typen.** `bitarray(8,lsb0)` und `bitarray(8,msb0)`
mischen sich nicht: Die Zuweisung des einen an den anderen ist ein
`type mismatch`. Die Vorgabe `bitarray(8)` ist `bitarray(8,lsb0)`.

**Funktionen.** `popcount(x)`: Anzahl der Bits mit Wert 1; `bitlen(x)`:
Breite eines bitarray, sonst Anzahl der Bits der Ganzzahl.

**In anderen Typen.** Ein Produktfeld (`{bitarray(8)}flags`), ein
Listenelement (`list.bitarray(4)`) oder eine Prozedurvariable sind bitarrays
wie alle anderen: `set r(flags.7) 1`, `set L(2.3)`.

**Ähnelt das einem Dict?** Nein: keine Schlüssel und keine Felder, eine
einzige Ganzzahl. Um die Felder eines binären Kopfes zu beschreiben,
kombiniert man ein Produkt aus bitarrays (siehe voriges Kapitel).

Vollständige Beispiele: `exemples/bitarray_exemples.tcl` (das „packed“-Byte
eines GIF und TCP-Flags in `msb0`, Unix-Rechte in `lsb0`).

## type deref: eine Momentaufnahme der Verweise

Ein `var`-Verweis enthält den NAMEN einer Variablen und folgt ihrem Wert bei
jedem Lesen. `type deref` ersetzt jeden Verweis durch den Wert, den die
Variable IN DIESEM AUGENBLICK hat: Das Ergebnis ändert sich danach nicht
mehr.

    set A 1;  set B 2;  set C 3
    set {list.var.int}refs A B C
    set v [type deref $refs]            ;# 1 2 3   (list.int)
    set B 20
    puts $refs(1)                       ;# 20  : der Verweis folgt B
    puts $v                             ;# 1 2 3 : die Momentaufnahme bleibt

* Das Ergebnis hat den Typ „ohne Verweise“: `list.var.int` wird zu
  `list.int` und wird wie jede Liste von Ganzzahlen behandelt.
* Eine fehlende Variable bricht alles mit einem Fehler ab, der die Stelle
  nennt: `type deref: at 2: can't read "::C": no such variable`. Es gibt kein
  Teilergebnis, und der ursprüngliche Wert bleibt unberührt.
* Es wird nur eine Ebene verfolgt: Enthält eine Zielvariable selbst
  Verweise, bleiben sie erhalten; ein zweites `type deref` folgt ihnen.
* Funktioniert in Listen, Dicts, Produkten und SUMMEN, verschachtelt oder
  nicht, und in REKURSIVEN Typen (Bäume wie `tree(var)`). Die Variante einer
  Summe bleibt erhalten.
* Graphen werden ebenenweise behandelt: Sind die Nachbarn eines Knotens
  Verweise (`{list.var.node}next`), liefert `type deref` den Knoten mit durch
  ihre Werte ersetzten Nachbarn, die noch Verweise enthalten; ein zweites
  `type deref` steigt eine Ebene tiefer. Ein Zyklus führt daher nie zu einer
  Endlosschleife.
* Der Typ des Ergebnisses ist ein eigener Typ namens
  `deref(<ursprünglicher Typ>)` (`deref(tree(var))`, `deref(mix)`), einmal
  berechnet und dann wiederverwendet: Zwei Momentaufnahmen desselben Typs
  haben denselben Typ.

    type def mix {{int}a, {var.int}b, {string}s}
    set {mix}s1 {b}A
    type get [type deref $s1]           ;# deref(mix).b

    set {tree(var)}T A {{B {}} {C {}}}  ;# ein Baum, dessen Werte Verweise sind
    set d [type deref $T]               ;# 1 {{2 {}} {3 {}}}
    puts $d(children.0.value)           ;# 2

Siehe auch `MANUAL-deref.md` (Englisch): der Verweistyp `var` und
`type deref`, mit Beispielen in `exemples/deref_manual.tcl`.

## Summe und C-`union`: nicht verwechseln

Für C-Programmierer: Eine **Summe** (`{{A}x, {B}y}`) ähnelt einer `union`,
ist aber nicht dasselbe.

| | C-`union` | Summe |
|---|---|---|
| was sie enthält | **dieselben Bytes**, auf mehrere Arten gelesen | **eine einzige** Variante, die ihren Namen trägt |
| wer weiß, welche gültig ist | niemand: der Programmierer muss es wissen | der Wert selbst (`type get` liefert `S.x`) |
| die falsche Variante lesen | erlaubt, man liest beliebige Bytes | unmöglich: man greift auf die vorhandene Variante zu |
| Größe in einer Datei | die der größten Sicht | die der gewählten Variante |
| Wahl der Variante | keine, es gibt keine | durch ein bereits gelesenes Feld (`S($tag)`) oder ihren Namen (`S(x)`) |

Folgen:
* Es gibt in diesem Modell **bewusst keine `union`**: Die Schutzmechanismen
  bleiben. Eine Summe kann nicht „schief“ gelesen werden.
* Wenn das Format nicht sagt, welche Variante gültig ist (Tcls
  `Tcl_ObjInternalRep`, deren Diskriminante die Adresse `typePtr` außerhalb
  der union ist), wird die Variante **beim Aufruf** gewählt:
  `Tcl_ObjAs(twoPtrValue)` (siehe `exemples/tcl_obj.tcl`). Der Aufrufer weiß
  es, wie in C, aber der Typ wird weiterhin geprüft.
* Eine Summe, deren Varianten teilweise die **gleiche Form** haben, ist nicht
  mehrdeutig, sobald die Variante über ihren Namen gewählt wird (`S(y)`): Es
  gibt nichts mehr zu erraten.

### Existieren, null sein, nicht existieren

Drei Situationen für einen Wert, auf den ein Verweis zeigt (Typ `*(...)`,
kommt noch), nicht verwechseln:
* **vorhanden**: der Verweis bezeichnet einen Wert;
* **vorhanden, aber null oder leer**: der Wert existiert, er ist `0`, `{}`
  oder `""`;
* **nicht existent**: der Verweis ist null (zum Beispiel Offset 0), es gibt
  nichts zu lesen.

Eine Feldvorgabe (`= {}`) unterscheidet die beiden letzten nicht. Eine Summe
mit einer wertlosen Variante (`{{T}some, {}none}`) unterscheidet sie. Das ist
noch nicht umgesetzt, siehe die Entwurfsnotiz.

## Binäre Typen: Bytes lesen und schreiben (`type binary`, `chan binary`)

Ein Feld sagt durch seinen TYP, wie es in Bytes geschrieben wird. Sonst ist
nichts zu lernen: Es ist das Prinzip von `bitarray(16)`, das seine Breite
bereits trägt. Der `.` dient nur zum ZUSAMMENSETZEN (`list(3).T`); die
Darstellung steht in den Klammern des Typs.

    type def mz {"MZ"}                           ;# eine Konstante: Summe eines Literals
    type def entete {
        {mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve;
        {string(utf-8,z)}nom
    }

| Typ | Darstellung |
|---|---|
| `int(u,n)` / `int(s,n)` | vorzeichenlose / vorzeichenbehaftete n-Bit-Ganzzahl (Vielfaches von 8, 8 bis 1024) |
| `double(32)` `double(64)` | IEEE-Gleitkommazahl |
| `string(raw,N)` | N rohe Bytes |
| `string(ENC,N)` | Text auf N Bytes, mit NUL aufgefüllt |
| `string(ENC,z)` | NUL-terminierter Text (2 Bytes in utf-16, ucs-2) |
| `bitarray(n)` | n Bits (Vielfaches von 8), als vorzeichenlose Ganzzahl gelesen |
| `"MZ"` (Summe gleich langer Literale) | eine Konstante, vom Typ geprüft |
| `list(N).T`, ein Produkt aus diesen Typen | N Elemente; Verschachtelung ist erlaubt |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Ein Text,
der sich nicht verlustfrei kodieren lässt (`é` in ascii) oder nicht in N
Bytes passt, wird abgelehnt.

**Byte-Reihenfolge.** Letztes Argument: `be` (Big-Endian, Netzwerkordnung)
oder `le`: `int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`. Ohne es
gilt die des Aufrufs (`-endian`, standardmäßig little). Sie ändert die Werte
nicht: `int(u,16,be)` und `int(u,16,le)` sind austauschbar.

**Automatische Erweiterung.** Eine Verfeinerung passt dorthin, wo ein
breiterer Typ erwartet wird:

    set {int(u,8,be)}u 3
    set {int}n $u                    ;# ok: jedes int(u,8) ist ein int
    set {double}x $u                 ;# ok
    set {int(s,16)}s $u              ;# ok: 0..255 passt in -32768..32767
    set {int(u,8)}v $n               ;# abgelehnt: Verengung (expliziter Cast)
    set {list.int(u,8)}L $liste_int  ;# auch für Listen abgelehnt

Ein mit `type def port int(u,16)` definierter Typ bleibt nominal: `port`
passt in ein int, aber ein einfaches `int(u,16)` passt nicht in einen
`port`.

**Ausrichtung.** Standardmäßig sind Produkte kompakt (packed), wie Netzwerk-
und PE/ELF-Köpfe. `-align natural` (oder `-align N`) fügt das Padding von C
und LLVM hinzu: Jedes Feld beginnt an einem Vielfachen seiner Ausrichtung
(seine Größe, begrenzt auf 8 oder N), die Größe eines Produkts wird auf seine
Ausrichtung aufgerundet. Padding wird beim Schreiben mit Nullen gefüllt und
beim Lesen ignoriert, außer mit `-strictpad 1`.

    type binary entete                           ;# die Darstellung aus Sicht des Kerns
    type binary decode entete $octets ?-offset N? ?-endian E? ?-consumed VAR? ?-align A?
    type binary encode entete $valeur ?-endian E? ?-align A?
    chan binary read  $canal entete ?-endian E? ?-align A?   ;# der Kanal rückt vor
    chan binary write $canal entete $valeur ?-endian E? ?-align A?
    type binary {sizeof?}entete ?-align A?

Lesen und Schreiben sind in Tcl geschrieben (`layout/filebinary.tcl`), über
dem Kern: `type binary` beschreibt, dekodiert und kodiert Bytes im Speicher,
`chan binary` liest und schreibt auf einem Kanal. Der Name `file binary` (der
alte) bleibt als Alias verfügbar.

**`type binary`, Abfragen und Verben.** Fragen zu einem Typ werden als lokale
Annotation am Typargument geschrieben (das abschließende `?` unterscheidet
sie von jedem Typnamen); Aktionen auf Bytes bleiben Verben:

    type binary entete                          ;# die Darstellung (nackte Form)
    type binary {what?}$T                       ;# dasselbe
    type binary {sizeof?}$T ?-align A?          ;# Größe in Bytes (Fehler, wenn variabel)
    type binary {is?}$T $octets ?-exact 1?      ;# 1, wenn $octets als T dekodierbar ist, sonst 0
    type binary decode T $octets ...            ;# liest die Bytes im Speicher
    type binary encode T $valeur ...            ;# ebenso, encode
    type binary deref ...                       ;# ebenso, deref
    type binary space ...                       ;# ebenso, space (benannte Räume)

`{is?}` akzeptiert standardmäßig ein gültiges Präfix (einen Kopf erkennen);
`-exact 1` verlangt, dass alle Bytes verbraucht werden. Eine ungültige
Option, ein unbekannter Typ oder einer ohne binäre Darstellung bleiben
Fehler: Nur ein Scheitern der Dekodierung ergibt 0. Eine unbekannte
Annotation, die auf `?` endet, ist ein Fehler (`unknown question "oups?"`);
jede andere Annotation wird ignoriert. Ein Typ, der den Namen eines Verbs
trägt (`type def decode ...`), wird weiterhin von `type binary decode`
beschrieben, solange kein weiteres Argument folgt. `decode`, `encode`,
`deref` und `space` verlangen, dass `layout/filebinary.tcl` geladen ist
(sonst: `the binary layer is not loaded`).

**Kurze Aliase.** `layout/binaliases.tcl` gibt den üblichen Darstellungen
kurze Namen (Konvention von Rust und Zig): `u8 u16 u32 u64`,
`i8 i16 i32 i64`, `f32 f64`, mit den Suffixen `be`/`le` (`u16be`, `i32le`,
`f64be`), sowie `cstring`, `wstringz`, `latin1z`. Es sind `type alias`: Die
Bibliothek dient auch als Merkzettel für Binärformate. Die `i8…i64` hier sind
vorzeichenbehaftet; die `iN` von LLVM haben kein Vorzeichen, es gibt also
keine exakte Entsprechung (die Arrays `[N x T]` und die packed-Strukturen
entsprechen `list(N).T` und den Produkten).


### Stufe 2: Größen, Anzahlen, Positionen und Varianten aus der Datei

Immer dieselbe Syntax: `$feld` in den Argumenten des Typs, wie die Parameter
parametrisierter Typen, oder ein vorzeichenloser Ganzzahltyp als Präfix.

| Schreibweise | Bedeutung |
|---|---|
| `string(utf-8,int(u,16,be))` | Text mit vorangestellter Größe in Bytes (Pascal-String) |
| `string(raw,$len)` | N Bytes, N ist der Wert des Felds `len` |
| `string(utf-8,$len - 4)` | ein Tcl-Ausdruck über bereits gelesene Felder |
| `list(int(u,8)).T` | Liste mit vorangestellter Elementanzahl |
| `list($n).T` | n Elemente, n ist ein Feld |
| `at($off).T` | der Wert liegt an Position `$off` der Datei (der Cursor bewegt sich nicht) |
| `corps($tag)` | die Summe `corps`, deren Variante das Feld `tag` wählt |

    type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# Variantennamen = Etiketten
    type def message {{u8}type; {u16be}len; {corps($type)}body}
    type def exe {{mz}magic = MZ; ...; {u32}lfanew; {at($lfanew).pe}nt}

Regeln: Ein `$feld` muss ein **bereits gelesenes** Feld des aktuellen oder
eines umschließenden Produkts bezeichnen (`type binary` prüft das); ein
Produktfeld liest man als `$h(off)`. Beim Schreiben ist eine Größe oder
Anzahl, die nicht zum Feld passt, von dem sie abhängt, ein Fehler, und das
Etikett muss das der Variante sein. `type binary {sizeof?}` lehnt einen Typ
ab, dessen Größe von den Daten abhängt. Vollständiges Beispiel (PE, ELF,
Nachrichten): `exemples/niveau2.tcl`.

Beispiele: `exemples/layout_plat.tcl`, `exemples/niveau2.tcl`. Grenzen:
keine unions und keine geteilten Bitfelder, keine Liste „bis zum Dateiende“.

#### Gewählte Variante: `S(name)` und Typparameter

Eine Summe `S` kann ihre Variante auch **bei der Definition gewählt**
bekommen, ohne Auswahlfeld in der Datei:

```tcl
type def rep_twoPtr {{ptr}ptr1; {ptr}ptr2}
type def U {{rep_long}l, {rep_twoPtr}tp}

type def Q {{U(tp)}s}               ;# die Variante tp, ausgeschrieben
type def {R({string}v)} {{u8}k; {U($v)}s}   ;# oder durch einen Parameter gegeben
type binary decode {R(tp)} $octets
```

`U(tp)` ist ein Typ mit einer einzigen Variante: Seine Größe ist bekannt
(`type binary {sizeof?}`), der gelesene Wert ist `U(tp).tp`, und es gibt
nichts zu erraten, selbst wenn mehrere Varianten die gleiche Form haben (was
den Wert früher mehrdeutig machte). `U($feld)` behält seine Bedeutung:
Variante, gewählt durch ein bereits gelesenes Feld. Ist `$v` ein gebundener
Parameter des Typs, gilt der erste Fall. Beispiel: `exemples/tcl_obj.tcl`
(`Tcl_ObjAs(twoPtrValue)`). Test: `t36_somme_choisie.tcl`.

#### Der Verweistyp `*(ursprung,repr).T`

Ein Dateifeld enthält oft eine Adresse, nicht den Wert. Der Verweistyp liest
die Adresse (`repr`, eine Ganzzahl) und dann den dort liegenden Wert `T`;
der Wert des Felds ist der des Ziels.

    type def pe {{u8}a; {u8}b}
    type def H {{u8}m; {*(//,u32le).pe}r}

Ursprünge, wie in einem Dateisystem: `//` (oder `file://`) Anfang der Daten,
`./` Anfang der umschließenden Struktur, `../` ihre übergeordnete, `.` das
Feld selbst (siehe unten). Optionen: `null` (die Adresse 0 bedeutet „kein
Ziel“), `null=V`. Mit `opt(T)` (siehe `unit`) ergibt eine Nulladresse
`none`, sonst `some`: Das ist der Unterschied zwischen „null“ und „nicht
existent“.

Generische Aliase (`type alias {ptr32({type}T)} {*(//,u32le).$T}`) geben
kurze Namen: `ptr32(T)`, `ptr64(T)`, `rel32(T)`, `sub32(T)` in
`layout/binaliases.tcl`. Beim Schreiben werden die Ziele am Ende angefügt,
ausgerichtet, und die Adressen berechnet. Rekursive Typen, `memory://` und
benannte Räume werden weiter unten beschrieben. Details:
`noyau/CONCEPTION-lien.md`.

**Der Punkt `.`: Die Adresse ist relativ zum Ort des Felds.** `./`
bezeichnet den *Anfang der Struktur*; `.` bezeichnet *das Feld selbst* (die
Stelle, an der die Adresse steht). Das ist der Ursprung von „ab hier“-
Abständen, häufig in Formaten mit relativen Offsets (`rel32`):

    type def H {{u8}m; {u8}pad; {*(.,u8,null).rec}r}   ;# das Feld r liegt bei Offset 2
    type binary decode H "\x01\x00\x01\x07\x08"        ;# 1 0 {7 8} : 2 + 1 = 3

    type def I {{u8}m; {u8}pad; {*(./,u8,null).rec}r}  ;# dasselbe Ergebnis, aber mit ./
    type binary decode I "\x01\x00\x03\x07\x08"        ;# 1 0 {7 8} : 0 + 3 = 3

Merkhilfe: `//` Anfang der Daten · `./` Anfang der Struktur · `../` Anfang
der übergeordneten Struktur · `.` Position des Felds. Die Schreibweise `.`
bleibt bewusst kurz; ein sprechenderer Name wird erst nach praktischer
Erfahrung erwogen.

#### Wie weit Verweisen folgen: `-linkdepth`

Einem Verweis kann sofort gefolgt oder er kann belassen werden. Die
Aufrufoption `-linkdepth` regelt die Anzahl der verfolgten Verweise (wie die
Reichweite einer Strahlung: Gammastrahlung durchdringt alles, eine schwach
durchdringende Strahlung bleibt an der Haut stehen):

    type binary decode H $d                   ;# 1 {7 8}   Inf, die Vorgabe: allem folgen
    type binary decode H $d -linkdepth 1      ;# 1 {7 8}   eine Ebene von Verweisen
    set h [type binary decode H $d -linkdepth 0]
    puts "$h(m) $h(r)"                        ;# 1 5      keinem Verweis gefolgt
    type get $h(r)                            ;# &*(//,int(u,32,le),null=0).rec

Ein nicht verfolgter Verweis ist eine annotierte Ganzzahl (absolute Position
des Ziels in den Daten). Kopie, `lindex` und `string cat` behalten die
Annotation; eine Berechnung verliert sie, und dem Ergebnis kann nicht mehr
gefolgt werden. Man folgt einem Verweis bei Bedarf, Schritt für Schritt:

    type binary deref $h(r) $d                ;# 7 8   (standardmäßig -linkdepth 0: ein einziger Schritt)

Was man davon hat: nur den Kopf einer großen Datei lesen; eine verkettete
Liste oder einen Baum durchlaufen (der Typ kann rekursiv sein:
`type def N {{u8}x; {*(//,u8,null).opt(N)}next}`); einen beschädigten
Verweis finden; ein Feld an Ort und Stelle ändern, denn `type binary encode`
schreibt eine nicht verfolgte Adresse unverändert zurück, statt die Datei
neu zu ordnen. Ein Zyklus ist nur mit `Inf` ein Fehler.


## Benannte Räume

Ein Verweis kann einen unter seinem Namen registrierten Raum statt der Daten
des Aufrufs bezeichnen.

```tcl
set sp [type binary space file $octets -name lib]          ;# annotierter Wert vom Typ space
type binary space memory $octets {{4096 4 2}} -name mem    ;# Segmente {Adresse Offset Größe}
type def H {{u8}m; {*(//lib/,u8,null).rec}r}               ;# file://lib/ ist gleichwertig
type def M {{*(memory://mem/,u32le,null).rec}p}
type binary deref $h0(r)                                    ;# keine DATEN: der Raum genügt
type binary space list | get NAME | delete NAME
```

Der Standardname wird berechnet (`space1`, …); der Erzeugungsbefehl
registriert den Raum; ein bereits vergebener Name ist ein Fehler. `//name/`
verlangt einen Raum ohne Segmente, `memory://name/` einen Raum mit
Segmenten; eine Adresse außerhalb der Segmente ist ein Fehler. In einen
benannten Raum lässt sich kein echtes Ziel schreiben: Nur die Grenzadresse
(`-linkdepth 0`) wird neu geschrieben.

## Einen Null-Verweis schreiben

Ein Verweisfeld mit Vorgabewert, dessen Wert dieser Vorgabe gleicht, wird als
Null-Verweis geschrieben (die „Null“-Adresse des Verweises, kein Ziel wird
geschrieben). Das Lesen macht das Umgekehrte: Nulladresse → Vorgabewert. Ohne
Vorgabe (und ohne `opt`) bleibt das Lesen eines Null-Verweises ein Fehler.

## -dedupe: identische Ziele teilen

`type binary encode TYP WERT -dedupe 1` (auch `write`) schreibt Verweisziele
mit identischen Bytes nur einmal; die betroffenen Verweise erhalten dieselbe
Adresse. Standardmäßig (`-dedupe 0`) wird jedes Ziel geschrieben. Nur Ziele
ohne verschachtelten Verweis werden geteilt (die internen Adressen eines Ziels
hängen von seiner Position ab); das Wiedereinlesen liefert denselben Wert.

## Von einem Feld abhängige Größe: beim Aufbau geprüft

Eine Liste, deren Elementanzahl ein Ausdruck über andere Felder ist
(`{list($n).Sample}items`, `list($n - 1).u8`), wird schon beim Aufbau des
Produkts geprüft: `set {Batch}b(n,items) 3 {{1 1.0 0}}` wird abgelehnt
(`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).
Weggelassene Felder zählen mit ihrem Vorgabewert. Die Prüfung deckt keine
feldweisen Zuweisungen ab (`set b(n) 3`: ein Zwischenzustand ist legitim);
`type binary encode` prüft beim Schreiben erneut. Bytegrößen von Zeichenketten
(`string(raw,$len)`) werden beim Aufbau nicht geprüft.
