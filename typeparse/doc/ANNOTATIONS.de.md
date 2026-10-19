# Annotationen in Tcl — Dokumentation

*19. Oktober 2026 · Version v13 · Florent MERLET, mit Claude (Anthropic)*

*Andere Sprachen: [Français](ANNOTATIONS.fr.md) (Referenz) · [English](ANNOTATIONS.en.md) · [Nederlands](ANNOTATIONS.nl.md) · [Español](ANNOTATIONS.es.md) · [Português](ANNOTATIONS.pt.md) · [Italiano](ANNOTATIONS.it.md)*

## Vorwort

**Autoren**: Florent MERLET, mit Claude (Anthropic).

Dieses Dokument beschreibt die von TIP 764 vorgeschlagenen **Annotationen** in Tcl 9.0: Metadaten, die an ein Wort gehängt werden (`{x}wort`), den Wert nicht ändern, aber Variablen typisieren, Felder benennen und Binärformate beschreiben. Es ersetzt das während der Entwicklung entstandene Handbuch und beschreibt den Prototyp in **Version v13**.

Drei Teile für drei Leser:

- **Teil I — Konzepte**: was eine Annotation ist, der Unterschied zwischen Notiz und Typ, und ihre Regeln. Zuerst lesen.
- **Teil II — Referenz**: die Befehle `note` und `type`, die Typsprache, Pfade, der Aufbau von Werten, Binärtypen und Fehler. Für alle, die Tcl schreiben.
- **Teil III — Implementierung**: das annotierte Objekt, Parser, Bytecode, die betroffenen Dateien und Funktionen. Für alle, die den TIP oder den Code prüfen.

Es folgen Kompatibilität und Leistung, Entwurfsentscheidungen, Grenzen und Anhänge (Installation, Glossar, Entsprechungen in C, Rust und LLVM).

Die Beispiele sind auf dem Prototyp lauffähiges Tcl; die Kommentare `;#` geben das Ergebnis an. Fehlermeldungen werden so zitiert, wie Tcl sie erzeugt. Die Bezeichner in den Beispielen (`nom`, `fiche`, `couleur`, `grandeur`…) bleiben wie im französischen Original, damit sie zu den Beispielskripten passen.

## Teil I — Konzepte

### 1. Annotationen

#### 1.1 Herkunft des Wortes

*Annotieren* kommt vom lateinischen *ad notare*, „eine Notiz daneben setzen“. Das Wort *Protokoll* sagt dasselbe in anderer Form: Das griechische *prōtokollon* war das erste Blatt, das vorn auf eine Papyrusrolle geklebt wurde und ihren Inhalt beschrieb, ohne dazuzugehören. Eine Tcl-Annotation ist genau das: ein Blatt, das vorn an ein Wort geklebt wird.

```tcl
{mm}12          ;# der Wert bleibt 12; „mm“ ist davorgeklebt
{list.int}L     ;# der Name bleibt L; „list.int“ sagt, was er enthalten wird
```

#### 1.2 Metadaten

Eine Annotation ist **Metadatum**:

- sie **erscheint nicht im Text** des Werts: `{mm}12` ist `12` für `puts`, `expr`, `string length`;
- sie **liefert eine Information**, die der Wert allein nicht trägt: eine Einheit, einen Typ, einen Feldnamen, eine Option;
- sie lebt **außerhalb der Ebene der Rohwerte**: kein bestehender Tcl-Befehl sieht sie, und kein bestehendes Skript ändert seine Bedeutung.

#### 1.3 Eine Analogie: der Imaginärteil

Eine komplexe Zahl hat einen Real- und einen Imaginärteil. Ebenso hat ein annotierter Wert einen **Rohteil** (den Text, den Tcl schon immer verarbeitet) und einen **annotierten** Teil (das, was der Entwickler im Kopf hat).

| | komplexe Zahl | annotierter Wert |
|---|---|---|
| sichtbarer Teil | der Realteil | der Rohwert (`12`) |
| hinzugefügter Teil | der Imaginärteil | die Annotation (`mm`, `int`, `nom`) |
| was man gewöhnlich liest | den Realteil | die Zeichenkette |

Die Analogie endet beim Rechnen. Bei einer komplexen Zahl rechnet der Imaginärteil mit. Hier **verliert eine Berechnung die Annotation**: `expr {$x + 1}` liefert eine nackte Zahl. Nur Kopieren, `lindex`, `string cat` und die Übergabe als Argument erhalten sie. Das ist gewollt: Die Ebene der Rohwerte bleibt gewöhnliches Tcl, und genau das sichert die Kompatibilität.

#### 1.4 Die Ausführung lenken

Die Annotation macht den „Imaginärteil“ des Entwicklers explizit, also das, was er über seine Daten weiß und Tcl bisher nicht:

- ein **Typ** lässt jedes Schreiben in eine Variable prüfen (`set {int}n abc` schlägt fehl);
- ein **Feldname** bestimmt, wohin ein Wert geht (`set {pt}a {age}12 {nom}Jean`);
- eine **Binärdarstellung** sagt, wie Bytes gelesen und geschrieben werden (`{int(u,16,be)}version`);
- eine **Notiz** markiert einen Wert, damit eine Prozedur ihn erkennt (`note has $v mm`).

Das Skript behält seine Form: Man fügt Annotationen dort hinzu, wo man Garantien will, und sonst nirgends.

### 2. Notizen, Typen und lokale Annotationen

Eine Annotation wird immer `{text}wort` geschrieben. Je nachdem, was der Text sagt und **wer ihn liest**, spielt sie drei Rollen.

#### 2.1 Die Notiz: die rohe Annotation

Eine **Notiz** ist eine Annotation, die noch kein Konstruktor aufgelöst hat. Sie folgt dem Wert: Kopie, Prozedurargument, Listenelement.

```tcl
set x {mm}5
note get $x          ;# mm
type get $x          ;# (leer: kein aufgelöster Typ)
```

**Stapeln.** Man annotiert einen bereits annotierten Wert; die Notizen stapeln sich, von der ältesten zur jüngsten.

```tcl
set y {px}$x
note get $y          ;# mm.px      (px ist die jüngste)
```

Die Schreibweise `{a}{b}$x` (zwei aneinandergeklebte Annotationen) gibt es nicht: Sie wäre mehrdeutig mit `{int}{300}`, einer Annotation auf einem Literal in geschweiften Klammern. Man stapelt, indem man einen schon annotierten Wert annotiert.

#### 2.2 Der Typ: die von einem Konstruktor aufgelöste Annotation

Ein **Typ** ist eine Annotation, die in der dem Interpreter bekannten Typologie **aufgelöst** wird (Grundtypen, `type def`, `type alias`). Nur ein **Variablenkonstruktor** löst auf:

- `set {T}v …` (und `set v {T}$x`);
- die Bindung der Argumente von `proc`;
- `lassign`, `foreach`, und in `expr` `x = v` und `x is T = v`.

Auf den **Namen** einer Variablen gesetzt, gibt eine Annotation der Variablen einen **Vertrag**: Jedes spätere Schreiben wird geprüft.

```tcl
set {int}n 5
set n abc            ;# expected integer but got "abc"
set {list.int}L 1 2 3
type get $L          ;# list<int>
```

Außerhalb eines Konstruktors prüft `{T}$x` nichts und wandelt nichts um, selbst wenn `T` ein bekannter Typ ist: `T` wird einfach **gestapelt**, wie eine Notiz. `list {int}abc` löst keinen Fehler aus.

#### 2.3 Zwei Stapel und eine Abstapelregel

Ein annotierter Wert trägt **zwei Listen**: den **aufgelösten Typ** (höchstens einen) und den **Stapel der offenen Notizen**. Die Anzeige trennt sie mit `@`:

```
 list<int>@mm.hello
 └───────┘ └──────┘
 aufgelöster Typ   Notizen, von der ältesten (mm) zur jüngsten (hello)
```

Ein Konstruktor **stapelt ab**, von oben:

1. er nimmt die jüngste Notiz und versucht, sie aufzulösen;
2. er macht weiter, solange die Notizen sich auflösen; die erste, die sich nicht auflöst, beendet alles und bleibt auf dem Stapel;
3. die abgestapelten Typen müssen zusammenpassen: Der jüngste muss in jeden der anderen passen, sonst `contradictory annotations px and qq`;
4. der Wert wird in den jüngsten Typ umgewandelt („gecastet“);
5. der Vertrag der Variablen macht die letzte Prüfung.

```tcl
type def px int
set z {px}$y         ;# y = @mm.px: px löst sich auf, mm nicht
type get $z          ;# px      ; note get $z: mm
```

Eine Notiz, die **vor** ihrem `type def` gesetzt wurde, wird also vom nächsten Konstruktor aufgelöst: Ein späteres `type def mm double` gibt den schon gesetzten `{mm}` einen Sinn.

**Zuerst der Kontext.** Ein Konstruktor löst in seinem Kontext auf, vor den globalen Typen: bei einer Summe die Variantennamen (`{i}5` in `set {S}v {i}5`), bei einem Produkt die Feldnamen (`{age}12`). Ein globaler Typ gleichen Namens ändert nichts.

#### 2.4 Lokale Annotationen

Eine Annotation, deren Text ein Zeichen enthält, das Typnamen nicht erlauben (`-`, `?`, `:`…), kann **nie** ein Typ sein. Sie ist eine **lokale Annotation**: Sie lebt nur zwischen Aufrufer und aufgerufenem Befehl und folgt dem Wert nicht.

```tcl
type binary {sizeof?}$T          ;# eine Frage an type binary
type binary {is?}$T $octets
set x {-v}$mot                   ;# abgelehnt: local annotation {-v} cannot be stored by "set"
set {-v}q 1                      ;# abgelehnt: local annotation {-v} cannot declare a variable
```

Zwei natürliche Familien, ohne Kollisionsgefahr mit einem Typnamen: Optionen als `-name` und Fragen als `name?`. Ein Typname enthält nur ASCII-Buchstaben, Ziffern, `_` und Nicht-ASCII-Bytes und beginnt nicht mit einer Ziffer.

#### 2.5 Die leere Annotation `{}`

`{}` ist „der leere Typ“. Seine Wirkung hängt vom Ort ab:

| wo | Wirkung |
|---|---|
| auf einem Wert, in einem Konstruktor (`set v {}$x`) | entfernt den aufgelösten Typ, behält die Notizen |
| auf einem Namen (`set {}v $x`) | hebt den Vertrag der Variablen auf; der Wert wird mit seinen Notizen abgelegt, ohne etwas aufzulösen |
| auf einem Wert, außerhalb eines Konstruktors | der Leser sieht den nackten Inhalt (Typ und Notizen gelöscht) |

```tcl
set {int}n 5
set {}n abc          ;# der Vertrag ist aufgehoben: n ist abc
```

Der ursprüngliche Wert wird nie verändert: `{}` erzeugt eine neue Sicht, was das Teilen von Objekten ungefährlich macht.

## Teil II — Referenz

### 3. Der Befehl `note`

```
note get WERT                ;# die Notizen, von der ältesten zur jüngsten, durch „.“ verbunden
note has WERT NAME           ;# 1, wenn NAME eine der Notizen ist, sonst 0
note delete WERT ?NAME?      ;# der Wert ohne die Notiz NAME, oder ohne jede Notiz
```

Tcl-Werte sind unveränderlich: `note delete` **liefert** einen neuen Wert, es ändert keine Variable.

```tcl
set y {px}{mm}5              ;# extra characters after close-brace: kein angeklebtes Stapeln
set x {mm}5 ; set y {px}$x
note get $y                  ;# mm.px
note has $y mm               ;# 1
note get [note delete $y px] ;# mm
```

Typische Verwendung: eine Prozedur, die auf die Markierung eines Werts reagiert.

```tcl
proc longueur {v} {
    if {[note has $v mm]} {return "$v millimètres"}
    return $v
}
longueur {mm}12              ;# 12 millimètres
```

### 4. Der Befehl `type`

```
type def NAME DEFINITION       ;# definiert einen nominalen Typ (oder eine parametrisierte Familie)
type alias NAME DEFINITION     ;# gibt einer Definition einen strukturellen Namen
type get WERT                  ;# der aufgelöste Typ eines Werts (leer, wenn keiner)
type cast TYP WERT             ;# der Auflöser eines Konstruktors, ohne Variable
type convert VON NACH LAMBDA   ;# deklariert eine explizite Umwandlung
type deref WERT                ;# Momentaufnahme: jeder var-Verweis durch seinen Wert ersetzt
type binary TYP ...            ;# Binärdarstellung, Kodierung, Dekodierung (§ 9)
```

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12
type get $a                    ;# pt
type alias entier int
set {entier}e 3 ; type get $e  ;# int  (ein Alias ist kein neuer Typ)
type cast pt {Léa 7}          ;# Léa 7, vom Typ pt
```

In `expr` prüft der Operator **`is`** den Typ einer Variablen, und eine Zuweisung kann deklarieren:

```tcl
expr {a is pt}                 ;# 1
expr {z is pt = ("Zoé", 30)}   ;# deklariert z vom Typ pt und weist den Wert zu
expr {new is cmd}              ;# 1, wenn new ein Befehl ist; new(…) wird eine expr-Funktion
```

`is cmd` ist kein Typ: Es prüft, ob ein Befehl existiert, und macht ihn als Funktion von `expr` verfügbar.

### 5. Das Typsystem

#### 5.1 Grundtypen

| Typ | Werte |
|---|---|
| `int` | Ganzzahl (unbegrenzt, wie in Tcl) |
| `double` | Gleitkommazahl |
| `string` | jede Zeichenkette |
| `list` | gewöhnliche Tcl-Liste (untypisierte Elemente) |
| `dict` | gewöhnliches Tcl-Dictionary |
| `var` | Verweis auf eine Variable, über ihren Namen (§ 5.3) |
| `any` | Element einer heterogenen Liste (`list.any`): jedes Element behält seinen Typ |

**Verfeinerungen** schränken einen Grundtyp ein und tragen eine Binärdarstellung: `int(u|s, n ?, be|le?)`, `double(32|64 ?, be|le?)`, `string(ENC, N|z)`, `bitarray(n ?, lsb0|msb0?)` (§ 9).

Ein `type def` erzeugt einen **nominalen** Typ: `type def port int(u,16)` ist von `int(u,16)` verschieden. Ein `type alias` gibt einer Definition nur einen Namen.

#### 5.2 Die Kompositionssprache

Ein Typ wird wie ein Ausdruck geschrieben und nach dem Vorbild von `expr` analysiert (`tclTypeParse.c` übernimmt die Struktur von `tclCompExpr.c`).

**Drei Arten von Operanden:**

| Operand | wo | Beispiel |
|---|---|---|
| Typ | nacktes Wort, links von einer `}` oder an Operandenstelle | `int`, `list`, `pt` |
| Name | Wort rechts an einer `}` | `{int}age`: das Feld `age` |
| Wert | in Anführungszeichen, in geschweiften Klammern oder nach `=` | `"MZ"`, `= 0` |

**Operatoren**, vom stärksten zum schwächsten:

| Operator | Rolle | Beispiel |
|---|---|---|
| `.` | Verschachtelung (rechtsassoziativ) | `list.dict.int` |
| `( )` | Argumente, am Namen | `list(3).int`, `dict(x, y?)`, `pt(double)` |
| `=` oder `?=` | Vorgabewert eines Felds | `{int}age ?= 0` |
| `;` | Produkt | `{double}x; {double}y` |
| `,` | Summe | `{int}i, {string}s` |
| `{ }name` | Gruppierung, mit einem Namen an der schließenden Klammer | `{{int}x; {int}y}A` |

`;` und `,` haben dieselbe Präzedenz: Um ein Produkt und eine Summe zu mischen, gruppiert man mit geschweiften Klammern (`{int}a; {{int}b, {string}c}u`).

**Zeichen.** Ein Typname enthält nur ASCII-Buchstaben, Ziffern, `_` und Nicht-ASCII-Bytes und beginnt nicht mit einer Ziffer. Die Grammatik verwendet zusätzlich `{ } , ; . = " # ( ) * $`. Jedes andere ASCII-Zeichen (`! % & ' + - / : < > ? @ [ \ ] ^ | ~`) wird abgelehnt: `invalid character "!"`, Code `TCL PARSE TYPE BADCHAR`, außer nach `=` (ein Vorgabewert) und in den Klammern eines Konstruktors (`*(file://x, …)`). Diese Ablehnung macht lokale Annotationen (`{-v}`, `{is?}`) sicher.

Eine Definition ist **deklarativ**: Beim Lesen findet keine Ersetzung statt, außer in einem Vorgabewert, der bei jedem Aufbau ausgewertet wird.

#### 5.3 Felder und Container

**Aufzählung** — eine Summe von Literalen.

```tcl
type def couleur {"rouge", "vert", "bleu"}
set {couleur}c vert
set {couleur}c jaune           ;# value "jaune" matches no variant of couleur
type def mz {"MZ"}             ;# ein einziges Literal: eine Konstante (Dateisignatur)
```

**Summe** — jeweils nur eine Variante, die ihren Namen trägt.

```tcl
type def S {{int}i, {string}s}
set {S}v {i}5
type get $v                    ;# S.i
set {S.s}w abc                 ;# die Variante, gewählt über den Namen des Typs
```

**Produkt** — ein `struct`: benannte, geordnete Felder, jedes mit eigenem Typ.

```tcl
type def pt {{string}nom ?= "?"; {int}age ?= 0}
set {pt}a Jean 12              ;# positionell
set {pt}b {age}12 {nom}Jean    ;# Feldetiketten (§ 8)
$a(nom)                        ;# Jean
```

**Liste** — `list` (erweiterbar), `list.T` (Elemente vom Typ T), `list(N).T` (genau N).

```tcl
set {list.int}L 10 20 30
set {list(2).int}c 3 4
set {list(2).int}d 3           ;# list(2)<int> needs exactly 2 elements but got 1
set {list.any}H {int}1 {double}1.0 {string}{a b}
```

Die Elemente einer `list.T` sind atomar: `lindex`, `foreach`, `$L(i)` liefern typisierte Elemente, die nie wieder in Wörter zerlegt werden. Die nackte Liste `{list}` behält das Verhalten von Tcl.

**Dictionary** — `dict` (alle Schlüssel), `dict.T` (Werte vom Typ T), `dict(Schlüssel).T`.

```tcl
type def fiche {dict(nom, age?=0).string}   ;# nom Pflicht; age optional, standardmäßig 0
set {fiche}e nom Jean                       ;# nom Jean age 0
set {fiche}f age 3                          ;# key "nom" is missing in fiche
```

`dict(x, z?)`: z darf fehlen; `dict()`: nur der leere Schlüssel. Schlüssel sind immer Text; nur die Werte sind typisiert.

**Verweis `var`** — der NAME einer Variablen, bei jedem Lesen verfolgt.

```tcl
set A 1 ; set B 2
set {list.var.int}L A B        ;# ::A ::B
set A 7
$L(0)                          ;# 7
type deref $L                  ;# 7 2   (eine Momentaufnahme, vom Typ list.int)
```

**Typparameter `{type}T`** — in einem parametrisierten Typ ein Parameter, der einen Typ erhält (§ 5.5).

**Binärverweis `*(ursprung,repr).T`** — eine Adresse in Binärdaten, die zu einem Wert T verfolgt wird (§ 9.5).

#### 5.4 Vorgabewerte

`?=` (oder `=`, die ältere Schreibweise, in einem Produkt weiterhin akzeptiert) gibt einem Feld eine Vorgabe. Eine Vorgabe kann ein Literal sein, ein `[script]`, das bei jedem Aufbau ausgewertet wird, oder eine `$variable`, die bei jedem Aufbau gelesen wird.

```tcl
type def t {{int}n ?= [expr {6*7}]; {string}s ?= $::tcl_platform(os)}
set {t}v                       ;# 42 Linux
```

Ein Feld ohne Vorgabe muss angegeben werden: `field "x" of pt has no value and no default`.

#### 5.5 Parametrisierte Typen

Der Name eines `type def` kann Parameter tragen, die wie Felder deklariert werden: `{type}T` (ein Typ), `{int}n`, `{double}f`, `{string}s`. Im Rumpf werden sie mit `$` verwendet.

```tcl
type def {pt({type}T)} {{$T}x; {$T}y ?= 0}
set {pt(double)}p {x}1.5       ;# 1.5 0
type get $p                    ;# pt(double)
type def {tree({type}T)} {{$T}value; {list.tree($T)}children}   ;# rekursiv
```

#### 5.6 Phantomparameter: physikalische Einheiten

Ein Parameter, der im Rumpf nicht vorkommt, ist erlaubt: Er ändert die Werte nicht, unterscheidet aber die Instanzen, die verschiedene nominale Typen sind. Eine Familie von Größen liefert so Einheiten, deren Wert ein `double` bleibt:

```tcl
type def {grandeur({string}dim, {double}f = 1.0, {double}o = 0.0)} double
type def mm   {grandeur(L, 0.001)}
type def m    {grandeur(L)}
type def kg   {grandeur(M)}
type convert grandeur grandeur {{x de vers} {
    if {$de(dim) ne $vers(dim)} {return -code continue}   ;# Regel nicht anwendbar
    expr {($x * $de(f) + $de(o) - $vers(o)) / $vers(f)}
}}
set {mm}a 1500
set b {m}$a                    ;# 1.5  (explizite Umwandlung)
set {m}c $a                    ;# type mismatch: expected m but got mm (use an explicit cast)
set d {kg}$a                   ;# unknown way to cast the value from mm to kg
```

Eine `type convert`-Regel kann einen Typ oder eine **Familie** (den Namen eines parametrisierten `type def`) betreffen; die Lambda erhält dann die Parameter der Instanzen (`$de(f)`). `return -code continue` bedeutet „nicht anwendbar“. Umwandlungen bleiben explizit.

#### 5.7 Mitgelieferte Aliase

`layout/binaliases.tcl` definiert für Binärformate (Konventionen von Rust und Zig):

| Alias | Definition |
|---|---|
| `u8 u16 u32 u64`, `i8 i16 i32 i64` | `int(u,n)`, `int(s,n)` |
| Suffixe `be`, `le` (`u16be`, `i32le`) | feste Byte-Reihenfolge |
| `f32 f64` (und `f32be`…) | `double(32)`, `double(64)` |
| `cstring`, `wstringz`, `latin1z` | `string(utf-8,z)`, `string(utf-16le,z)`, `string(latin1,z)` |
| `ptr32(T)`, `ptr64(T)` | `*(//,u32le,null).T`, `*(//,u64le,null).T` |
| `rel32(T)`, `sub32(T)` | `*(.,i32le).T`, `*(./,u32le).T` |
| `unit`, `opt(T)` | `""` ; `{T}some, {unit}none` |

### 6. Typisierte Variablen und das Leben eines annotierten Werts

#### 6.1 Der Vertrag einer typisierten Variablen

Eine mit `set {T}v …` deklarierte Variable hat einen **Vertrag**: Jedes Schreiben wird geprüft, ob es von `set`, `lappend`, `lset`, `incr`, `dict set`, einem Pfadschreiben (`set v(x) …`) oder `expr {v = …}` kommt.

```tcl
set {list.int}M 1 2
lappend M 3                    ;# 1 2 3
lappend M x                    ;# expected integer but got "x"
```

**Automatische Erweiterung.** Eine Verfeinerung passt dorthin, wo ein breiterer Typ erwartet wird: `int(u,8)` in `int`, `int(s,16)` oder `double`; `double(32)` in `double(64)`. Verengung wird abgelehnt (sie verlangt einen expliziten Cast), ebenso zwischen `bitarray(…,lsb0)` und `bitarray(…,msb0)`.

```tcl
set {int(u,8)}o 200
set {int}w $o                  ;# akzeptiert: type get $w -> int
set {int(u,8)}o2 300           ;# value "300" does not fit in int(u,8)
```

**Nominale Typen.** Ein nominaler Typ weicht seiner Basis (`px` passt in `int` und ergibt `int`, ohne Spur). Zwei nominale Typen ohne Basisverbindung sind nie gleichwertig (`mm` und `kg`): Genau das ist ihre Aufgabe.

**Explizite Casts.** `set v {T}$x` oder `type cast T $x` wandeln um, in dieser Reihenfolge: eine spezifische `type convert`-Regel; die Kette der Basen; die Variante einer Summe; eine Familienregel; die eingebauten Umwandlungen zwischen Skalaren. Sonst: `unknown way to cast the value from A to B`.

**Den Vertrag aufheben.** `set {}v wert` (§ 2.5).

**Prüfen.** `expr {v is T}`: wahr, wenn die Variable typisiert ist und ihr Typ in `T` passt; bei einer untypisierten Variablen wahr, wenn ihr Wert für `T` gültig ist.

#### 6.2 Was die Annotation behält, was sie verliert

| Operation | Typ | Notizen |
|---|---|---|
| Kopie (`set a $x`), Argument, Rückgabewert | behalten | behalten |
| `lindex`, `foreach`, `$L(i)` auf einer `list.T` | Element vom Typ `T` | — |
| `string cat $x` (ein einziges Argument) | behalten | behalten |
| Berechnung (`expr`, `string toupper`, `dict set` auf einem Wert) | **verloren** | **verloren** |
| Cast `{T}$x` in einem Konstruktor | geändert | behalten |
| `set {T}a $x`, typisierte Variable | `T` | durch Erweiterung gelöscht |

**Übernahme.** Eine **untypisierte** Variable, die einen typisierten Wert erhält, **übernimmt** dessen Typ und damit dessen Vertrag:

```tcl
set {list.int}L 1 2 3
set e [lindex $L 1]            ;# e wird eine int-Variable
set e hello                    ;# expected integer but got "hello"
```

Das ist das überraschendste Verhalten des Modells. Die klassische Falle ist ein wiederverwendetes `catch … m`: Hat ein erstes `catch` einen typisierten Wert in `m` abgelegt, kann ein zweites `catch` dort seine Fehlermeldung nicht mehr ablegen. Abhilfe: eine Variable pro Verwendung, `unset` zwischen zwei Verwendungen, oder `set {}m …`. (Offener Punkt, § 17.)

**Prozeduren.** Ein Argument, das einen typisierten Wert erhält, ist in der Prozedur typisiert und streng; ein gewöhnliches Argument ändert sich nicht.

```tcl
proc f {x} {set x 1.5}
set {int}n 3
f $n                           ;# expected integer but got "1.5"
f 3                            ;# 1.5
```

**Schleifenvariablen.** `foreach`, `lmap`, `dict for`, `dict map` (und der Filter von `dict filter`) vergessen den Typ ihrer Variablen vor jeder Zuweisung; nach der Schleife behält die Variable den Typ des zuletzt erhaltenen Elements.

#### 6.3 `type deref`: eine Momentaufnahme der Verweise

`type deref V` liefert `V`, in dem jeder `var`-Verweis durch den Wert ersetzt ist, den die Variable in diesem Augenblick hat. Das Ergebnis hat den Typ „ohne Verweise“ (`list.var.int` → `list.int`), bei einem Produkt oder einer Summe `deref(<Typ>)` genannt. Pro Aufruf wird nur eine Ebene verfolgt; eine fehlende Variable ist ein Fehler, der die Stelle nennt (`type deref: at 2: can't read "::C": no such variable`), ohne Teilergebnis. Funktioniert in Listen, Dicts, Produkten, Summen und rekursiven Typen; ein Zyklus läuft nie endlos.

### 7. Auf Teile eines Werts zugreifen: Pfade

#### 7.1 Die Klammerform

Ein typisierter Wert wird mit der Syntax von Arrays gelesen und geschrieben, aber der Index ist ein **Pfad**:

```tcl
type def rect {{{int}x; {int}y}A; {{int}x; {int}y}B}
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# Aufbau (§ 8)
$R(A.x)                                    ;# 1
set R(B) {5 6}                             ;# Schreiben eines ganzen Felds
set R((A,B).x) 10 30                       ;# mehrfaches Schreiben
${R(A.x, B.y)}                             ;# Lesen, Leerzeichen erlaubt
```

Leerzeichen sind erlaubt in `$R(…)`, `${R(…)}` und in einem annotierten Wort `{T}R(…)`. In einem nackten, nicht annotierten Wort (`set R(A.x, A.y) …`) trennt Tcl wie immer an Leerzeichen: ohne Leerzeichen oder in Anführungszeichen schreiben.

#### 7.2 Die Pfadsprache

Die Sprache ist die der Tcl-Wiki-Seite „A better way to do indexation“, nach dem Vorbild von `expr` analysiert (`tclTypePath.c`).

**Operanden:**

| Operand | Bedeutung |
|---|---|
| `name` (nacktes Wort) | Feld, Dict-Schlüssel, Variantenname oder Listenindex (`3`, `end`, `end-1`): der Typ des Containers entscheidet |
| `'schlüssel'` | immer ein Schlüssel (`'end'` ist der Schlüssel „end“, kein Index) |
| `"etikett"` | literaler Text, ins Ergebnis kopiert |

**Operatoren**, vom schwächsten zum stärksten:

| Operator | Rolle | Beispiel |
|---|---|---|
| `,` | Sammlung (eine Gruppe) | `A,B` |
| `*` (unär) | Auffalten: die Gruppe geht in ihrer Elterngruppe auf | `*0:2,4` |
| `.` | Verschachtelung; verteilt sich über eine Gruppe | `A.(x,y)` = `A.x,A.y` |
| `:` | Listenbereich | `0:end-1` |
| `( )` | Gruppierung | `(A,B).x` |

**Form des Ergebnisses.** Ein Mehrfachlesen liefert gemäß den Gruppen verschachtelte Listen: `$R((A,B).(x,y))` ergibt `{1 2} {3 4}`; `$L(0:2,4)` ergibt `{10 20 30} 50`; `$L(*0:2,4)` ergibt `10 20 30 50`.

**Ein Pfad ersetzt nichts.** Tcl ersetzt `$x`, `[cmd]` und `[(expr)]` in `$R(…)` und in nackten Wörtern; ein ersetzter Wert ist **ein Blatt**, das nie wieder als Pfad gelesen wird:

```tcl
set k file.txt ; $D($k)        ;# der Schlüssel „file.txt“ (nicht D.file.txt)
set n 1        ; $D(f_$n)      ;# an Text geklebt, verlängert der Wert den Namen
set idx {0 2}  ; $L(*$idx)     ;# * faltet einen Listenwert auf: 10 30
set p {[exec ls]} ; $L($p)     ;# nichts wird ausgeführt: bad index "[exec ls]"
```

In geschweiften Klammern und in `${…}` sind `$` und `[` Fehler: `a path does not substitute "$"…`. Einen berechneten Pfad liest man, indem man die Variable mit ihm **benennt**: `set ref R($chemin) ; set $ref`.

#### 7.3 Container für Container

| Container | Lesen | Schreiben |
|---|---|---|
| Produkt | `$a(nom)`, `$R(A.x)`, `$R(A.(x,y))` | `set a(nom) Léa` (neu geprüft) |
| Summe | `$v(i)`: die aktuelle Variante | `set v(i) 6` |
| Liste | `$L(2)`, `$L(end)`, `$L(1:3)`, `$L(0,2)` | `set L(1) 25`, `set L(0,2) 5 6` |
| Dict | `$D(a)`, `$D('end')`, `$D(a,b)` | `set D(a) 3` |
| bitarray | `$b(3)` (ein Bit), `$b(1:4)` (ein Ausschnitt: eine Ganzzahl `bitarray(4,…)`) | `set b(0) 1`, `set b(4:7) 0` |
| Aufzählung | (Skalar: kein Pfad) | — |

Lesen und anschließendes Zurückschreiben desselben Pfads funktioniert immer: `set R(c) [set R(c)]`.

### 8. Einen Wert aufbauen

#### 8.1 Die Formen eines Produktkonstruktors

Ein Produkt mit *n* Feldern wird auf vier Arten aufgebaut, ohne dass etwas erraten wird:

| Form | Beispiel | Regel |
|---|---|---|
| positionell | `set {pt}a Jean 12` | genau *n* Werte, in der Reihenfolge der Felder |
| Feldetiketten | `set {pt}a {age}12 {nom}Jean` | alle Werte etikettiert; beliebige Reihenfolge; weggelassene Felder mit ihrer Vorgabe |
| ein einzelnes Feld | `set {pt}a {pt.nom}Jean` | die anderen Felder mit ihrer Vorgabe |
| Pfad + positionelle Werte | `set {pt}a(age,nom) 12 Jean` | ein Wert pro Blatt des Pfads (oder pro Gruppe, § 8.3) |

Hinzu kommen **kein Argument** (`set {pt}a`, alle Felder mit ihrer Vorgabe) und **ein Produkt desselben Typs** (Kopie). Alles andere ist ein Fehler:

```
wrong # args: pt needs 2 values; to name fields (the others take their default),
write {field}value or set {pt}var(field,...) value...
```

Die nackte Name/Wert-Form (`set {pt}a nom Jean age 12`) **gibt es nicht mehr**: Sie war mit der positionellen Form verwechselbar, sobald *n* Argumente zu *n* Feldern passten (§ 16).

#### 8.2 Feldetiketten

Das Etikett ist die **jüngste** Annotation jedes Arguments, im Kontext des Produkts gelesen: Ein Feldname geht einem globalen Typ gleichen Namens vor.

```tcl
type def paire {{string}nom; {int}age ?= 0}
type def age int                       ;# ändert nichts
set {paire}p {nom}jean {age}12          ;# jean 12
set {paire}p {nom}zoe                   ;# zoe 0
set v {age}21 ; set {paire}p {nom}max $v   ;# ein von einer Variablen getragenes Etikett zählt
```

**Alles oder nichts:**

```tcl
set {paire}p {nom}jean 12        ;# can't mix values named by a field annotation ({field}value)
                                 ;#   and positional values in a paire constructor: name every field, or none
set {paire}p {nom}a {taille}3    ;# unknown field "taille" in paire
set {paire}p {nom}a {nom}b       ;# duplicate field "nom" in paire
```

Auch ein Wert in geschweiften Klammern lässt sich etikettieren: `set {seg}s {a}{1 2} {b}{3 4}`.

#### 8.3 Pfad und positionelle Werte

`set {T}v(pfad) v1 v2 …` baut das Produkt (und seine verschachtelten Produkte) auf; die benannten Felder erhalten die Werte, die anderen ihre Vorgabe. Die **Anzahl** der Werte bestimmt die Verteilung, nie ihr Inhalt:

```tcl
set {rect}R(A.x,A.y,B.x,B.y) 1 2 3 4      ;# ein Wert pro Blatt
set {rect}R((A,B).(x,y))     1 2 3 4      ;# ebenso
set {rect}R(A.(x,y),B.(x,y)) {1 2} {3 4}  ;# ein Wert pro Element der Kopfgruppe
set {rect}R((A,B).(x,y))     {{1 2} {3 4}};# ein Wert für den ganzen Pfad
set {rect}Q((A,B).*(x,y))    {5 6 7 8}    ;# mit Auffalten: ein Wert, flach
```

Jeder Wert hat die Form, die sein Lesen liefern würde; die drei Anzahlen fallen nur zusammen, wenn die Lesungen zusammenfallen.

**Pfad + Etiketten gibt es nicht**: `set {rect}R(A) {y}2 {x}1` ist ein Fehler (`1 targets but 2 values`). Das ist nicht vorgesehen, solange keine Verwendung es verlangt.

#### 8.4 Summen und Listen

Eine Summe wird mit ihrer Variante als Etikett aufgebaut, oder über den Variantennamen im Typ:

```tcl
set {S}v {i}5                  ;# S.i
set {S.s}w abc                 ;# S.s
set {list.S}L {i}1 {s}a        ;# jedes Element behält seine Variante
```

Ohne Etikett wird die Variante aus dem Wert abgeleitet, in der Reihenfolge der Varianten: `set {S}u 5` ergibt `S.i`. Haben mehrere Varianten dieselbe Form (`{string}a, {string}b`), passt immer die erste: Zum Wählen etikettieren (`{b}y`).

Eine Liste erhält ihre Elemente einzeln: `set {list.int}L 1 2 3`, `set {list(2).int}c 3 4`. Ein einzelnes Argument ist **ein** Element.

### 9. Binärtypen

Ein Typ sagt selbst, wie er in Bytes geschrieben wird: Es gibt keine weitere Syntax zu lernen. Lesen und Schreiben übernimmt `layout/filebinary.tcl` (mit `source` geladen) über dem Kern, mit einem schnellen Weg in C für feste Layouts (§ 9.6).

#### 9.1 `bitarray`: eine Ganzzahl, deren Bits man liest

`bitarray(n, ordnung)` ist eine n-Bit-Ganzzahl (0 ≤ v < 2^n). Die Ordnung gibt an, wo Index 0 beginnt: `lsb0` (Vorgabe, das rechte Bit, wie `1 << i`: Register, Masken) oder `msb0` (das linke Bit, wie in RFC-Diagrammen). Es sind zwei verschiedene Typen.

```tcl
set {bitarray(8)}b 0xB2                ;# 10110010
set {bitarray(8,msb0)}c 0xB2
lmap i {0 1 2 3 4 5 6 7} {set b($i)}   ;# 0 1 0 0 1 1 0 1
lmap i {0 1 2 3 4 5 6 7} {set c($i)}   ;# 1 0 1 1 0 0 1 0
$b(1:4)                                ;# 9   (eine Ganzzahl bitarray(4,lsb0))
set b(0) 1 ; set b(4:7) 0
expr {"b(1:2)" = 3}                    ;# in expr ein Ausschnitt in Anführungszeichen
expr {popcount($b)} ; expr {bitlen($b)}
```

#### 9.2 Darstellungen

| Typ | Darstellung |
|---|---|
| `int(u,n)` / `int(s,n)` | vorzeichenlose / vorzeichenbehaftete n-Bit-Ganzzahl (Vielfaches von 8, 8 bis 1024) |
| `double(32)`, `double(64)` | IEEE-Gleitkommazahl |
| `string(raw,N)` | N rohe Bytes |
| `string(ENC,N)` | Text auf N Bytes, mit NUL aufgefüllt |
| `string(ENC,z)` | NUL-terminierter Text (2 Bytes in utf-16, ucs-2) |
| `bitarray(n)` | n Bits (Vielfaches von 8), als vorzeichenlose Ganzzahl gelesen |
| `"MZ"` (Summe gleich langer Literale) | eine Konstante, geprüft |
| `list(N).T`, ein Produkt dieser Typen | N Elemente; Verschachtelung ist erlaubt |

`ENC`: `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`. Ein Text, der sich nicht verlustfrei kodieren lässt (`é` in ascii) oder nicht in N Bytes passt, wird beim Aufbau abgelehnt.

**Byte-Reihenfolge.** Letztes Argument: `be` oder `le` (`int(u,16,be)`, `double(64,le)`, `bitarray(16,msb0,be)`). Ohne es gilt die des Aufrufs (`-endian`, standardmäßig little). Sie ändert die Werte nicht: `int(u,16,be)` und `int(u,16,le)` sind einander zuweisbar.

**Ausrichtung.** Standardmäßig sind Produkte kompakt (packed). `-align natural` (oder `-align N`) fügt das Padding von C hinzu: Jedes Feld beginnt an einem Vielfachen seiner Ausrichtung, die Größe eines Produkts wird aufgerundet. Padding wird beim Schreiben mit Nullen gefüllt und beim Lesen ignoriert, außer mit `-strictpad 1`.

#### 9.3 Befehle

```
type binary TYP                                    ;# die Darstellung aus Sicht des Kerns
type binary {what?}TYP                             ;# dasselbe
type binary {sizeof?}TYP ?-align A?                ;# Größe in Bytes (Fehler, wenn datenabhängig)
type binary {is?}TYP BYTES ?-exact 1?              ;# 1, wenn BYTES als TYP dekodierbar sind, sonst 0
type binary decode TYP BYTES ?-offset N? ?-endian E? ?-consumed VAR? ?-align A? ?-linkdepth D?
type binary encode TYP WERT ?-endian E? ?-align A? ?-dedupe 1?
type binary deref VERWEIS ?BYTES?                  ;# folgt einem nicht verfolgten Verweis um einen Schritt
type binary space file|memory|list|get|delete …    ;# benannte Räume
chan binary read  KANAL TYP ?optionen?             ;# der Kanal rückt vor
chan binary write KANAL TYP WERT ?optionen?
```

Die Fragen (`{what?}`, `{sizeof?}`, `{is?}`) sind **lokale Annotationen** am Typargument; Aktionen sind Verben. `file binary` bleibt ein Alias des alten Namens.

```tcl
type def mz {"MZ"}
type def entete {{mz}magic = MZ; {int(u,16,le)}version; {string(raw,4)}reserve; {cstring}nom}
set o [type binary encode entete [type cast entete {MZ 2 abcd hello}]]
type binary decode entete $o           ;# MZ 2 abcd hello
```

#### 9.4 Stufe 2: was die Datei über sich selbst sagt

Eine Größe, eine Anzahl, eine Position oder eine Variante können aus der Datei gelesen werden: `$feld` in den Argumenten des Typs (ein **bereits gelesenes** Feld des aktuellen oder eines umschließenden Produkts) oder ein vorzeichenloser Ganzzahltyp als **Präfix**.

| Schreibweise | Bedeutung |
|---|---|
| `string(utf-8,int(u,16,be))` | Text mit vorangestellter Größe in Bytes (Pascal-String) |
| `string(raw,$len)`, `string(utf-8,$len - 4)` | N Bytes, N aus gelesenen Feldern berechnet |
| `list(int(u,8)).T`, `list($n).T` | Liste mit vorangestellter Elementanzahl, oder n Elemente |
| `at($off).T` | der Wert liegt an Position `$off` (der Cursor bewegt sich nicht) |
| `corps($tag)` | die Summe `corps`, deren Variante das Feld `tag` wählt |
| `U(tp)`, `R($v)` (gebundener Parameter) | eine Summe, deren Variante bei der Definition festgelegt ist |

```tcl
type def corps {{pp}1, {pingmsg}2, {string(raw,$len)}3}   ;# Variantennamen = Werte des Etiketts
type def message {{u8}type; {u16be}len; {corps($type)}body}
```

Schon beim Aufbau wird eine Liste geprüft, deren Größe von einem Feld abhängt: `set {Batch}b(n,items) 3 {{1 1.0 0}}` wird abgelehnt (`field "items" of Batch holds 1 elements, but its size says 3 ($n)`).

#### 9.5 Verweise: `*(ursprung, repr, optionen).T`

Ein Feld enthält oft eine Adresse, nicht den Wert. Der Verweistyp liest die Adresse (`repr`, eine Ganzzahl) und dann den dort liegenden Wert `T`.

| Ursprung | die Adresse zählt ab |
|---|---|
| `//` (oder `file://`) | Anfang der Daten |
| `./` | Anfang der umschließenden Struktur |
| `../` | Anfang ihrer übergeordneten Struktur |
| `.` | dem Feld selbst |
| `//name/`, `memory://name/` | einem benannten Raum (Datei oder Speichersegmente) |

Optionen: `null` (Adresse 0 bedeutet „kein Ziel“), `null=V`. Mit `opt(T)` ergibt eine Nulladresse `none`, sonst `some`: Das ist der Unterschied zwischen „null“ und „nicht existent“. Beim Schreiben werden die Ziele am Ende angefügt, ausgerichtet, und die Adressen berechnet; `-dedupe 1` schreibt identische Ziele nur einmal. Ein Verweisfeld, das seiner Vorgabe gleicht, wird als Null-Verweis geschrieben.

**`-linkdepth`** regelt die Anzahl der verfolgten Verweise (standardmäßig `Inf`). Ein nicht verfolgter Verweis ist eine annotierte Ganzzahl (die absolute Position des Ziels); `type binary deref` folgt ihm um einen Schritt. So liest man nur den Kopf einer großen Datei, durchläuft eine verkettete Liste und schreibt ein Feld an Ort und Stelle neu.

```tcl
type def rec {{u8}a; {u8}b}
type def H {{u8}m; {*(//,u32le).rec}r}
set h [type binary decode H $d -linkdepth 0]
type get $h(r)                         ;# &*(//,int(u,32,le)).rec
type binary deref $h(r) $d             ;# 7 8: das Ziel
```

#### 9.6 C-Codecs

`type binary encode` und `decode` versuchen zuerst einen Weg in C, der feste Layouts abdeckt: Ganzzahlen von 1, 2, 4, 8 Bytes, Gleitkommazahlen von 4 und 8 Bytes, Produkte (mit `-align`), `list(N).T` und `list($n).T`. Außerhalb dieses Bereichs (Zeichenketten, Summen, Verweise, `at`, Bitfelder, berechnete Größen, `-dedupe`) oder bei einem Fehler übernimmt die Tcl-Schicht und liefert ihre Meldungen. `set ::tcl_typebinaryc 0` schaltet den C-Weg ab. Die erzeugten Bytes und Werte sind auf beiden Wegen identisch (Test `t51_codec_c`). Messungen: § 15.

### 10. Fehler

Die Meldungen folgen dem Tcl-Brauch: Sie nennen den betroffenen Wert, Typ oder das Feld. Die typspezifischen `errorCode`s beginnen mit `TCL TYPE` oder `TCL PARSE TYPE`.

| Situation | Meldung (Beispiel) | `errorCode` |
|---|---|---|
| verbotenes Zeichen in einem Typ | `invalid character "!" in type "{int}a!"` | `TCL PARSE TYPE BADCHAR` |
| unbekannter Typname an einem Variablennamen | `unknown type "zz"` | `TCL TYPE UNKNOWN` |
| lokale Annotation an einem Variablennamen | `local annotation {-v} cannot declare a variable: a variable name takes a type` | `TCL TYPE LOCALANNOT` |
| lokale Annotation von `set` abgelegt | `local annotation {-v} cannot be stored by "set": it only applies between a caller and the command it calls` | `TCL TYPE LOCALANNOT` |
| ungültiger Wert für den Typ | `expected integer but got "abc"`; `value "300" does not fit in int(u,8)` | `TCL TYPE INT` (je nach Grundtyp) |
| keine Variante passt | `value "jaune" matches no variant of couleur` | `TCL TYPE SUM` |
| verschiedene nominale Typen | `type mismatch: expected m but got mm (use an explicit cast)` | `TCL TYPE MISMATCH` |
| keine bekannte Umwandlung | `unknown way to cast the value from mm to kg` | — |
| widersprüchliche gestapelte Annotationen | `contradictory annotations px and qq` | `TCL TYPE CONTRADICT` |
| unbekanntes Feld | `unknown field "zz" in pt`; `pt has no field "zz"` (Pfad) | `TCL TYPE FIELD`; `TCL TYPE PATH` |
| Feld ohne Wert und ohne Vorgabe | `field "y" of rect has no value and no default` | `TCL TYPE FIELD` |
| falsche Anzahl von Werten | `wrong # args: pt needs 2 values; to name fields …` | `TCL WRONGARGS` |
| Etiketten und Positionen gemischt | `can't mix values named by a field annotation ({field}value) and positional values in a pt constructor: name every field, or none` | `TCL TYPE FIELD` |
| doppeltes Feld | `duplicate field "nom" in paire` | `TCL TYPE FIELD` |
| abhängige Größe nicht eingehalten | `field "items" of Batch holds 1 elements, but its size says 3 ($n)` | `TCL TYPE FIELD SIZE` |
| Dict-Schlüssel fehlt oder nicht erlaubt | `key "nom" is missing in fiche`; `key "zz" is not allowed in fiche` | `TCL TYPE DICT` |
| Listenarität | `list(2)<int> needs exactly 2 elements but got 1` | `TCL TYPE LIST` |
| Ersetzung in einem literalen Pfad | `a path does not substitute "$": …` | `TCL PARSE INDEX …` |
| ungültiger Listenindex in einem Pfad | `bad index "zz": must be integer?[+-]integer? or end?[+-]integer?` | `TCL VALUE INDEX` |
| Verweis auf eine verschwundene Variable | `can't access "…": the procedure call that owned this variable has returned` | `TCL TYPE STALE` |
| `type deref` auf einem kaputten Verweis | `type deref: at 2: can't read "::C": no such variable` | `TCL TYPE DEREF` |
| annotierter Wert außerhalb des Parsers erzeugt | `annotated values can only be created by the parser or by typed assignments` | `TCL VALUE ANNOTATED` |
| Binärschicht nicht geladen | `the binary layer is not loaded` | — |

## Teil III — Implementierung

### 11. Überblick

Die Implementierung besteht aus **sieben neuen Dateien** (rund 15 000 Zeilen C), die nach `generic/` kopiert werden, und aus **Einhängepunkten** in rund zwanzig Kerndateien. Die neuen Dateien werden nie gepatcht: Sie werden ganz geliefert; nur Kerndateien erhalten Patches.

### 12. Das annotierte Objekt

#### Struktur

Ein annotierter Wert ist ein gewöhnliches `Tcl_Obj`, dessen interner Typ `annotated` ist (`annotatedObjType`, in `tclAnnotObj.c`):

```
 Tcl_Obj  (typePtr = &annotatedObjType)
 ┌───────────────────────────────────────────────────────────────┐
 │ bytes / length   die Zeichenkette: die des Inhalts V (bei      │
 │                  Bedarf von UpdateStringOfAnnotated berechnet) │
 │ internalRep.twoPtrValue                                        │
 │   ptr1 ──► V   der Inhalt: ein gewöhnliches, geteiltes Tcl_Obj │
 │   ptr2 ──► T   die Annotation, in einer dieser Formen:         │
 │                · eine aufgelöste Typreferenz (typeRef)          │
 │                · der Rohtext der Annotation (noch nicht         │
 │                  aufgelöst: eine Notiz oder ein Typ)            │
 │                · ""   die Markierung der leeren Annotation {}   │
 │                · ein „annotstate“ {T, N}: aufgelöstes T (oder   │
 │                  NULL) und N, die Liste der Notizen, von der    │
 │                  ältesten zur jüngsten                          │
 └───────────────────────────────────────────────────────────────┘
```

#### Eigenschaften

- **Transparent**: Die Zeichenkette ist die von V. Für jeden Befehl, der Annotationen nicht kennt, ist der Wert V.
- **Unveränderlich und geteilt**: Der Zustand `{T, N}` ist selbst ein unveränderliches Objekt; `Tcl_DuplicateObj` teilt ihn. Annotieren, Abstapeln oder Entfernen einer Notiz erzeugt ein neues Objekt.
- **Kostenlos ohne Notiz**: Solange es keine Notiz gibt, ist `ptr2` allein T.
- **Abstrakte Liste** (Tcl 9, `TCL_OBJTYPE_V2`): `lindex`, `llength`, `lrange`, `foreach`, `in`, `lappend`, `lset`… laufen über die Listenprozeduren des Objekts (`AnnotListLength`, `AnnotListIndex`, `AnnotListSlice`, `AnnotListReverse`, `AnnotListGetElements`, `AnnotListSetElement`, `AnnotListReplace`, `AnnotListIn`). Die Elemente einer `list.T` kommen typisiert zurück; ein annotierter Skalar ist eine einelementige Liste, deren Element er selbst ist.

#### Routinen des `Tcl_ObjType`

| Routine | Rolle |
|---|---|
| `FreeAnnotated` | gibt V und T frei (und die „Selbst“-Zelle eines Skalars, Tabelle `SelfCell`) |
| `DupAnnotated` | kopiert V, teilt T oder den Zustand `{T, N}` |
| `UpdateStringOfAnnotated` | die Zeichenkette von V |
| `SetAnnotatedFromAny` | **lehnt ab**: Ein annotierter Wert entsteht aus dem Parser oder einer typisierten Zuweisung, nie aus einer Umwandlung (`TCL VALUE ANNOTATED`) |

#### Wichtigste Funktionen (`tclAnnotObj.c`)

`TclAnnotateValue` (eine Annotation setzen: Cast oder Notiz), `TclAnnotSettle` (das Abstapeln eines Konstruktors, § 2.3), `TclAnnotResolve`, `ClassifyAnnot` (Typ, Variante, unbekannt, lokal, ungültig; der Kontext vor den globalen Typen), `TclAnnotInfo`, `TclAnnotPending`, `TclAnnotPopTop`, `TclAnnotUnwrap` (der nackte Inhalt, für Umwandlungen des Kerns), `TclAnnotDisplay` (`list<int>@mm.hello`) sowie die Befehle `::tcl::annotate`, `::tcl::annotraw`, `::tcl::annotlocal`, `note`.

### 13. Im Parser und im Compiler

#### Das Präfix `{x}wort`

Im ursprünglichen Tcl 9 ist ein Wort in geschweiften Klammern, dem weitere Zeichen folgen, ein Syntaxfehler (`extra characters after close-brace`), außer `{*}`. Das Annotationspräfix belegt also eine Schreibweise, **die keinen Sinn hatte**: Kein gültiges Skript ändert seine Bedeutung.

- `tclParse.c`, `ParseTokens` / `Tcl_ParseCommand`: Ein Wort `{text}rest` erzeugt ein Token **`TCL_TOKEN_ANNOT`** (512, `tcl.h`), das den Text der Annotation trägt, gefolgt von den Tokens des restlichen Worts. Ein annotiertes Wort `{T}name(...)` reicht bis zur ausgeglichenen Klammer, Leerzeichen eingeschlossen, auf einer Zeile.
- `tclCompile.c`, `TclCompileTokens`: Ist der Rest des Worts literal (`TclAnnotSuffixIsLiteral`), wird das Wort zu `::tcl::annotraw text wort` kompiliert (eine rohe Notiz, ohne Ersetzung); sonst zu `::tcl::annotate text wert`.
- `tclCompCmdsSZ.c`, `TclCompileSetCmd`: Ein `set` mit annotiertem Namen läuft über `TclSetAnnotatedCmd`; eine auf den Wert geschriebene lokale Annotation (`set x {-v}$mot`) erzeugt einen Aufruf von `::tcl::annotlocal`, der den Fehler auslöst (`TclAnnotLocalWordText`).
- `tclVar.c`, `Tcl_SetObjCmd`: Ein annotiertes erstes Argument delegiert an `TclSetAnnotatedCmd` (`tclTypedVar.c`), den Konstruktor.

#### Der Index `$name(...)`: `TCL_TOKEN_INDEX`

- `tclParse.c`, `Tcl_ParseVarName`: Bei der `(` nach einem Variablennamen wird **`TclParseVarIndex`** aufgerufen, das den Index bis zur ausgeglichenen `)` liest: verschachtelte Klammern, gepaarte Etiketten `"..."`, Ersetzungen `$x`, `[cmd]`, `[(expr)]`, Backslashes; geschweifte Klammern abgelehnt. Ist der Index nach dieser Grammatik nicht wohlgeformt, erfolgt ein exakter **Rückfall** auf die Regel von Tcl 9 (`info complete {a "c $dd("}` bleibt wahr).
- Nur wenn die erweiterte Syntax genutzt wird, werden die Bestandteile in ein Token **`TCL_TOKEN_INDEX`** (1024, `tcl.h`) gehüllt: Ein in Tcl 9 gültiger Index behält genau seine Tokens. Nackte Wörter sind nicht betroffen: `puts a(b c)` bleibt zwei Wörter.
- `${name(...)}`: Der ganze Name ist literal; `Tcl_ObjGetVar2` und `INST_LOAD_STK` (über `TclTypedVarGetElementByName`) lesen den Pfad, wenn die Variable ein typisierter Skalar ist.
- `tclTest.c`: `testparser` zeigt das Token „index“.

#### Bytecode

| Instruktion | wo | Rolle |
|---|---|---|
| `INST_INDEX_CONCAT` („indexConcat“, 6 Bytes: Anzahl `uint1` + Maske `uint4`) | `tclCompile.c` (`TclCompileIndexTokens`, aufgerufen von `TclCompileVarSubst` und `TclPushVarName`), `tclExecute.c` | verkettet die Teile eines Index zu einem **pathindex**-Objekt, das die ersetzten Bereiche behält (`TclPathIndexNew`): ein ersetzter Wert bleibt ein Blatt des Pfads |
| `INST_TYPE_FORGET` | `tclCompCmds.c` (kompilierte `dict for`, `dict map`), `tclExecute.c` | vergisst den Typ einer Schleifenvariablen vor der Zuweisung |

Nicht kompilierter Weg: `TclSubstTokens` (`SubstIndexTokens`) erzeugt dasselbe pathindex-Objekt.

#### Weitere Einhängepunkte im Kern

| Datei | Änderung |
|---|---|
| `tclBasic.c` | `TclTypesInit`: erzeugt `type`, `note`, `::tcl::mathop::is`, `popcount`, `bitlen` |
| `tclExecute.c` | `INST_STORE_SCALAR` nimmt für einen annotierten Wert den langsamen Weg; `INST_FOREACH_STEP` vergisst den Typ; Lesen und Schreiben typisierter Elemente (`TclTypedVarGetElement`, `TclTypedVarSetElement`) |
| `tclVar.c` | `TclTypedVarPrepareWrite` und `TclTypedVarAfterStore` um jedes Schreiben: der Vertrag |
| `tclProc.c` | `TclTypedVarBindArg`: typisierte Argumentbindung |
| `tclCmdAH.c`, `tclDictObj.c` | Schleifenvariablen (`TclTypedVarForgetByName`); `TclAnnotUnwrap` beim Lesen von Dicts |
| `tclObj.c`, `tclStringObj.c`, `tclBinary.c` | Umwandlungen des Kerns auf dem Inhalt (`TclAnnotUnwrap`, Makro `TCL_ANNOT_CONTENT`): ein annotierter Wert wird nie an Ort und Stelle umgewandelt |
| `tclListObj.c` | `lappend` über die `replaceProc` einer abstrakten Liste; `lindex` mit mehreren Indizes ohne Abflachen |
| `tclCompExpr.c` | Operator `is` (Lexem `IS_TYPE`, `IsTypeAhead`), Zuweisungen `x = v`, `x is T = v`, `(a,b) = $L` |

### 14. Die hinzugefügten Dateien

| Datei | Zeilen | Inhalt |
|---|---|---|
| `tclTypes.c` | 4 500 | Typologie, `type def/alias/get/cast/convert/deref/binary`, Validierung, Erweiterung, Casts, C-Codecs |
| `tclTypeParse.c` | 2 700 | Parser und Compiler der Typausdrücke (Vorbild: `tclCompExpr.c`) |
| `tclTypeGeneric.c` | 1 400 | parametrisierte Typen, Instanzen, Phantomparameter, binäre Verfeinerungen |
| `tclAnnotObj.c` | 2 150 | das annotierte Objekt, der Zustand `{T, N}`, die Auflösung, `note` |
| `tclTypedVar.c` | 1 350 | typisierte Variablen, Konstruktoren (`TclSetAnnotatedCmd`, `BuildProductEx`, Feldetiketten) |
| `tclTypePath.c` | 2 900 | Pfadsprache, Lesen, Schreiben, Aufbau über Pfad, pathindex-Objekt |
| `tclVarRef.c` | 360 | `var`-Verweise (kanonischer Name, Zugriff, veraltete Verweise) |
| `tclTypes.h`, `tclTypesInt.h` | 550 | Strukturen (`TclType`, `TclTypeField`, `TclBinLink`…) und Schnittstellen |

Auf der Tcl-Seite: `layout/filebinary.tcl` (Binärschicht: `type binary decode/encode/deref/space`, `chan binary`) und `layout/binaliases.tcl` (Aliase).

## Kompatibilität, Entwurf, Grenzen

### 15. Kompatibilität und Leistung

**Kein gültiges Skript ändert seine Bedeutung.** Beide hinzugefügten Syntaxen belegen Schreibweisen, die Fehler waren: `{x}wort` („extra characters after close-brace“) und ein erweiterter Index `$name(...)`, der genau auf die Regel von Tcl 9 zurückfällt, sobald er nicht wohlgeformt ist. Prüfungen:

- Tcl-Tests `parse`, `parseExpr`, `parseOld`, `subst` (darunter 257 Tests von `testparser`), `var`, `compile`, `set`, `upvar`, `basic`, `execute`, `incr`, `append`, `lset`, `dict`, `info`: Ergebnisse identisch mit der Basis;
- Äquivalenz mit einem unveränderten Tcl 9 auf 18 000 erzeugten Fällen (`$z(…)`, `"<$z(…)>"`, `set z(…)`, `info complete`): kein gültiges Ergebnis geändert;
- AddressSanitizer ohne Fehler; Speicher stabil über 1 000 000 Aufbauten.

**Typen sind nicht der Feind der Geschwindigkeit.** Messungen am Beispiel `cstruct` (10 000 Messwerte, `-align natural`):

| Operation | Tcl-Schicht | C-Codec | klassisches Tcl von Hand, ohne Prüfung |
|---|---|---|---|
| Kodierung (`type binary encode`) | 333 ms | 1,3 ms | ≈ 3,3 ms (`binary format`; 330 µs gemessen für 1 000) |
| Dekodierung (`type binary decode`) | 485 ms | 3,0 bis 3,7 ms | 7,3 bis 10,5 ms (`binary scan`) |

Der C-Codec, der zusätzlich jeden Wert gegen seinen Typ **prüft**, ist 2- bis 3-mal schneller als klassisches, von Hand geschriebenes Tcl und 130- bis 250-mal schneller als die generische Tcl-Schicht.

### 16. Entwurfsentscheidungen

| Entscheidung | Grund |
|---|---|
| `{x}wort` belegt einen Syntaxfehler | volle Kompatibilität, keine neue Quotierungsregel |
| kein angeklebtes Stapeln `{a}{b}$x` | mehrdeutig mit `{int}{300}`; man stapelt, indem man einen annotierten Wert annotiert |
| nur ein Konstruktor löst einen Typ auf | `{T}$x` außerhalb eines Konstruktors kostet nichts und scheitert nie; geprüft wird dort, wo der Wert abgelegt wird |
| lokale Annotationen an ihren Zeichen erkannt | ein Typname kann weder `-` noch `?` enthalten: keine Kollision möglich |
| `type def` nominal, `type alias` strukturell | Einheiten (`mm`, `kg`) unterscheiden, ohne die Werte zu ändern |
| automatische Erweiterung, explizite Verengung | kein stiller Informationsverlust |
| explizite Umwandlungen zwischen nominalen Typen | `mm` → `m` verlangt `{m}$a` oder `type cast` |
| nackte Name/Wert-Form entfernt | `set {png}p chunks $l` war zweifach lesbar; die Namen stehen jetzt im Quelltext (`{chunks}…` oder `p(chunks)`), nie aus den Daten erraten |
| Feldetiketten: alles oder nichts | eine Mischung wäre eine verkleidete positionelle Form |
| ein Pfad ersetzt nichts | Daten mit `[script]` werden vom Pfadparser nie ausgeführt (die doppelte Ersetzung verschwindet); dieselbe Regel wie bei einem Array |
| atomare ersetzte Werte | `$D($k)` liest den Schlüssel `$k`, auch wenn er `.` oder `,` enthält |
| Pfad + Etiketten nicht vorgesehen | kein nachgewiesener Bedarf; die Praxis wird es zeigen |

### 17. Grenzen und offene Punkte

**Zu beheben oder zu entscheiden**

- **Übernahme durch eine untypisierte Variable.** `set e [lindex $L 1]` macht `e` typisiert und streng. Daraus folgt die Falle `catch … m` (§ 6.2), und der Fehler, der dabei entweicht, ist die ursprüngliche Meldung, was ihn schwer verständlich macht. Zu entscheiden: stille Übernahme, Übernahme ohne Vertrag oder eine explizite Meldung.
- **`;` und `,` ohne geschweifte Klammern gemischt** (`{int}a; {int}b, {int}c`) werden fehlerfrei akzeptiert und erzeugen ein anonymes Feld; die Regel der Spezifikation lehnt sie ab.
- **Das Lesen einer Variante, die nicht die aktuelle Variante** einer Summe ist, liefert `unknown way to cast the value from S to string` statt einer Meldung, die die Variante nennt.
- **Varianten gleicher Form**: Ohne Etikett wird ohne Warnung die erste genommen.
- `set {paire}s {aut.nom}x` (das Feld eines **anderen** Produkts) liefert `wrong # args`; eine eigene Meldung wäre klarer.
- `lset` mit einer Indexliste über `TclLsetList` behält eine Referenz zu viel (Leck, ohne Absturz).

**Noch nicht umgesetzt**

- Ein „nicht existenter“ Wert im Allgemeinen: `opt(T)` deckt binäre Verweise ab, eine allgemeine Form gibt es nicht.
- `union`, geteilte Bitfelder, eine Liste „bis zum Dateiende“ (die Ablehnung von `union` ist gewollt).
- Eine Kommentar-Annotation `{#}` auf Parserebene, die Annotation des ersten Worts (`{c}proc`), Deklarationsannotationen (`proc p {{?}{…} {!}arg}`).
- Notizen pro Element einer typisierten Liste (heute gilt eine Notiz für die ganze Liste); Löschen aller Notizen mit `{@}$x`.
- C-Codecs für Summen, Verweise und berechnete Größen; Zwischenspeicherung des analysierten Typs.
- Der C-Prüfstand mit Attrappe (`typeparse/test/`) muss aktualisiert werden; die Beispiele `arbre_var.tcl` und `defauts_calcules.tcl` müssen korrigiert werden.

## Anhänge

### A. Installation und Tests

Der Prototyp geht von Tcl `core-9-0-branch` mit dem TIP-759-Zweig aus. Vom Wurzelverzeichnis des Tcl-Baums (MSYS2 / MinGW64 oder Unix):

```
bash typeparse/maj.sh [typeparse.zip]   # entpackt, kopiert die Typquellen, wendet die Kernpatches an,
                                        # baut neu und vergleicht jede tests_tclsh/tNN_*.tcl mit ihrer .out
```

- Die Typquellen werden ganz in `typeparse/` geliefert und nach `generic/` kopiert; nur die in `noyau/ORDRE.txt` aufgeführten Kernpatches werden angewendet, ohne Fuzz (`-F0`), wobei ein bereits angewendeter Patch erkannt und übersprungen wird.
- Optionen von `maj.sh`: `--dry-run`, `--yes`, `--no-build`, `--no-test`, `--commit "nachricht"`.
- Notizen zu jedem Patch: `noyau/LISEZMOI.md`. Lauffähige Beispiele: `exemples/`.

### B. Glossar

| Begriff | Bedeutung |
|---|---|
| Annotation | Text in geschweiften Klammern vor einem Wort: `{x}wort` |
| Notiz | nicht aufgelöste Annotation, die dem Wert folgt |
| Typ | in der bekannten Typologie aufgelöste Annotation |
| lokale Annotation | Annotation, deren Text kein Typ sein kann (`{-v}`, `{is?}`); sie lebt nur zwischen Aufrufer und Befehl |
| Konstruktor | was einen Wert ablegt und seine Annotationen auflöst: `set {T}v`, Argumentbindung, `lassign`, `foreach`, Zuweisung in `expr` |
| Vertrag | der Typ einer Variablen, bei jedem Schreiben geprüft |
| Übernahme | eine untypisierte Variable nimmt den Typ des erhaltenen Werts an |
| Cast | explizite Umwandlung in einen Typ (`{T}$x` in einem Konstruktor, `type cast`) |
| nominal | durch `type def` erzeugter Typ, verschieden von seiner Definition; weicht seiner Basis |
| Verfeinerung | eingeschränkter Grundtyp mit Binärdarstellung (`int(u,8)`) |
| Erweiterung | automatischer Übergang einer Verfeinerung zu einem breiteren Typ |
| Produkt | `struct`: benannte, geordnete Felder |
| Summe | eine benannte Variante unter mehreren |
| Variante | einer der Zweige einer Summe |
| Feldetikett | Annotation, die das Feld eines Arguments benennt: `{age}12` |
| Pfad | Index eines typisierten Werts: `R(A.(x,y))` |
| pathindex | Objekt, das einen Index und seine ersetzten Teile trägt |
| Verweis | `var.T` (auf eine Variable) oder `*(…).T` (auf eine Binäradresse) |
| Phantomparameter | Typparameter, der die Werte nicht ändert, aber die Instanzen unterscheidet |

### C. Entsprechungen

| hier | C | Rust | LLVM |
|---|---|---|---|
| Produkt | `struct` | `struct` | `{ T1, T2 }` (packed: `<{ … }>`) |
| Summe | (`union` + Diskriminante von Hand) | `enum` mit Daten | — |
| Aufzählung (Summe von Literalen) | `enum` | `enum` ohne Daten | — |
| `list(N).T` | `T[N]` | `[T; N]` | `[N x T]` |
| `int(u,16)` / `u16` | `uint16_t` | `u16` | `i16` (ohne Vorzeichen) |
| `int(s,32)` / `i32` | `int32_t` | `i32` | `i32` |
| `double(64)` / `f64` | `double` | `f64` | `double` |
| `cstring` | NUL-terminiertes `char *` | `CStr` | `i8*` |
| `ptr32(T)` | `T *` (32 Bit) | `*const T` | `ptr` |
| `opt(T)` | Zeiger, der `NULL` sein kann | `Option<T>` | — |
| nominaler Typ (`type def mm double`) | `typedef` (ungeprüft) | Newtype `struct Mm(f64)` | — |
| `-align natural` / packed | Standardausrichtung / `__attribute__((packed))` | `#[repr(C)]` / `#[repr(packed)]` | Ausrichtung / `<{ }>` |
