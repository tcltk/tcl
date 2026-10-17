# binaliases.tcl --
#
# FR : Alias courts pour les types binaires. Ce n'est PAS une obligation :
#      `int(u,16,be)` s'écrit toujours en toutes lettres. Cette bibliothèque
#      donne des noms brefs aux représentations usuelles, et sert aussi de
#      MÉMO sur les formats binaires : elle dit ce que sont un octet, un mot,
#      un entier réseau, une chaîne C, une chaîne Windows.
#
#      Convention (celle de Rust et de Zig, proche du <stdint.h> du C) :
#        u8 u16 u32 u64     entiers non signés (uint8_t ...)
#        i8 i16 i32 i64     entiers signés     (int8_t ...)
#        f32 f64            flottants IEEE 754 (float, double)
#      suffixe « be » : big-endian (ordre réseau, « network byte order »),
#      suffixe « le » : little-endian (x86, ARM Linux/Windows, PE, ELF le).
#      Sans suffixe, le boutisme est celui de l'appel (option -endian).
#
#      Attention : les i8..i64 d'ici sont SIGNÉS, comme en Rust. Les iN de LLVM
#      n'ont pas de signe (c'est l'opération qui interprète) : il n'y a donc
#      pas de correspondance exacte, voir MANUEL.md.
#
#      Usage :  source binaliases.tcl     puis    {u16be}port
#
# EN : Short aliases for the binary types. They are NOT required:
#      `int(u,16,be)` can still be written in full. This library gives brief
#      names to the usual representations and doubles as a MEMO on binary
#      formats: it says what a byte, a word, a network integer, a C string and
#      a Windows string are.
#
#      Convention (Rust's and Zig's, close to C's <stdint.h>):
#        u8 u16 u32 u64     unsigned integers (uint8_t ...)
#        i8 i16 i32 i64     signed integers   (int8_t ...)
#        f32 f64            IEEE 754 floats   (float, double)
#      suffix "be": big-endian (network byte order), suffix "le":
#      little-endian (x86, ARM on Linux/Windows, PE, little-endian ELF).
#      With no suffix, the byte order is the call's (-endian option).
#
#      Beware: the i8..i64 here are SIGNED, as in Rust. LLVM's iN have no sign
#      (the operation interprets the bits): there is no exact match, see
#      MANUEL.md.
#
#      Usage:  source binaliases.tcl     then    {u16be}port

namespace eval ::binaliases {
    # FR : entiers : nom, signe, largeur en bits   /   EN: integers
    foreach {name sign bits} {
	u8 u 8   u16 u 16   u24 u 24   u32 u 32   u64 u 64
	i8 s 8   i16 s 16   i24 s 24   i32 s 32   i64 s 64
    } {
	::type alias $name int($sign,$bits)
	if {$bits > 8} {
	    ::type alias ${name}be int($sign,$bits,be)
	    ::type alias ${name}le int($sign,$bits,le)
	}
    }
    # FR : flottants   /   EN: floats
    foreach {name bits} {f32 32 f64 64} {
	::type alias $name double($bits)
	::type alias ${name}be double($bits,be)
	::type alias ${name}le double($bits,le)
    }
    # FR : chaînes terminées par un NUL   /   EN: NUL-terminated strings
    #   cstring  : la chaîne C, en UTF-8 (ou ASCII, qui en est un sous-ensemble)
    #   wstringz : la chaîne « large » de Windows (UTF-16 little-endian)
    #   latin1z  : texte ISO 8859-1, vieux formats
    ::type alias cstring  string(utf-8,z)
    ::type alias wstringz string(utf-16le,z)
    ::type alias latin1z  string(latin1,z)

    # FR : « rien » (0 octet) et « une valeur ou rien » : la présence d'un lien.
    #      opt(T) vaut {T}some ou none. Un lien nul vers opt(T) est none
    #      (voir noyau/CONCEPTION-lien.md).
    # EN : "nothing" (0 bytes) and "a value or nothing": the presence of a link.
    #      opt(T) is {T}some or none. A null link to opt(T) is none.
    ::type def unit {""}
    ::type def {opt({type}T)} {{$T}some, {unit}none}

    # FR : liens : l'adresse est un entier, la valeur est ce qu'il désigne.
    #      ptr32(T) / ptr64(T) : offset depuis le début des données ;
    #      rel32(T) : offset (signé) depuis le champ lui-même ;
    #      sub32(T) : offset depuis le début de la structure englobante.
    #      Pour un lien qui peut être nul : ptr32(opt(T)), valeur « some » ou none.
    # EN : links: the address is an integer, the value is what it designates.
    #      ptr32(T) / ptr64(T): offset from the start of the data;
    #      rel32(T): signed offset from the field itself;
    #      sub32(T): offset from the start of the enclosing structure.
    #      For a link that may be null: ptr32(opt(T)), value "some" or none.
    ::type alias {ptr32({type}T)} {*(//,u32le,null).$T}
    ::type alias {ptr64({type}T)} {*(//,u64le,null).$T}
    ::type alias {rel32({type}T)} {*(.,i32le).$T}
    ::type alias {sub32({type}T)} {*(./,u32le).$T}
}

package provide binaliases 0.2
