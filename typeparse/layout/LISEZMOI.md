# type binary — niveau 1 / level 1

**FR** — `filebinary.tcl` (pur Tcl) lit et écrit des valeurs typées dans des fichiers ou canaux
binaires. **Aucune syntaxe de plus** : la représentation binaire d'un champ est portée par son
TYPE (`int(u,16,be)`, `string(raw,4)`, `string(utf-8,z)`, `bitarray(16)`, une constante `"MZ"`...), fournis
par le patch noyau `noyau/types-binaires.patch` ; `type binary T` décrit la représentation.

**EN** — `filebinary.tcl` (pure Tcl) reads and writes typed values in binary files or channels.
**No extra syntax**: a field's binary representation is carried by its TYPE, provided by the
core patch `noyau/types-binaires.patch`; `type binary T` describes the representation.

    source binaliases.tcl                       ;# u16be = int(u,16,be), u32be, f64le ... (alias courts)
    type def tcp {{u16be}src; {u16be}dst; ... {bitarray(16,msb0,be)}flags; ...}

    chan binary read   CANAL TYPE ?-endian E?
    chan binary write  CANAL TYPE VALEUR ?-endian E?
    type binary decode TYPE OCTETS ?-offset N? ?-endian E? ?-consumed VAR?
    type binary encode TYPE VALEUR ?-endian E?
    type binary {sizeof?}TYPE

Le canal est mis en mode binaire le temps de l'appel puis rétabli / the channel is put in binary
mode for the call, then restored. La même table servira à `chan binary` / the same table will
serve `chan binary`.

Voir `MANUEL.md` (chapitre « Types binaires »), `exemples/layout_plat.tcl`,
`tests_tclsh/t32_layout_plat.tcl`, `t33_types_binaires.tcl`.

## Nouveautés de cette version (0.4)

* Nouvelle notation : `int(u,16,be)`, `double(32,le)`, `string(utf-8,8)`, `string(utf-8,z)` ;
  le `.` ne sert plus qu'à composer (`list(3).int(u,16,be)`).
* Alignement : `-align packed|natural|N`, `-strictpad 1` (voir MANUEL.md).
* Encodages de chaînes : `raw ascii latin1 utf-8 utf-16le utf-16be ucs-2le ucs-2be`.
* `binaliases.tcl` : alias courts (`u8 … u64`, `i8 … i64`, `f32`, `f64`, suffixes `be`/`le`).

## Niveau 2 (0.5) / Level 2 (0.5)

* Tailles, nombres, positions et variantes lus dans le fichier : `string(raw,$len)`,
  `list($n).T`, `string(utf-8,int(u,16,be))`, `at($off).T`, `corps($tag)` (voir MANUEL.md).
* Sizes, counts, positions and variants read from the file: same notation, see MANUEL.md.
* Exemple / example: `exemples/niveau2.tcl` (PE, ELF, messages). Test : `t35_niveau2.tcl`.
