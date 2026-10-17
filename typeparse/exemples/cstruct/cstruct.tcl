# cstruct.tcl --
# FR : les types Tcl qui décrivent les structures de cstruct.c, et les
#      commandes de confort qui passent un produit typé à C.
# EN : the Tcl types describing cstruct.c's structs, and the convenience
#      commands that hand a typed product to C.
#
#   Le contrat : « -align natural » = la disposition du compilateur C.
#   The contract: "-align natural" = the C compiler's layout.

namespace eval cs {variable dir [file dirname [file normalize [info script]]]}

proc cs::init {{lib ""}} {
    variable dir
    if {$lib eq ""} {set lib [file join $dir cstruct[info sharedlibextension]]}
    ::load $lib Cstruct
    # struct Sample { uint32_t id; double value; uint8_t flags; };
    type def Sample {{u32le}id; {f64le}value; {u8}flags}
    # struct Batch  { uint32_t n; Sample items[]; };
    type def Batch {{u32le}n; {list($n).Sample}items}
    # struct Summary { uint32_t count; double min, max, mean; };
    type def Summary {{u32le}count; {f64le}min; {f64le}max; {f64le}mean}
}

# FR : voie rapide : le produit est encodé selon la disposition C et lu tel quel
#      par C ; le résultat revient en produit Summary annoté.
# EN : fast path: the product is encoded in the C layout and read as is by C;
#      the result comes back as an annotated Summary product.
proc cs::stats {batch} {
    set bytes [file binary encode Batch $batch -align natural]
    return [file binary decode Summary [cs::stats_raw $bytes] -align natural]
}

# FR : voie « objet » : C reçoit la valeur annotée elle-même.
# EN : "object" path: C receives the annotated value itself.
proc cs::stats_typed {batch} {
    return [file binary decode Summary [cs::stats_obj $batch] -align natural]
}

# FR : C fabrique un Batch ; Tcl le relit comme produit annoté.
# EN : C builds a Batch; Tcl reads it back as an annotated product.
proc cs::make {n seed} {
    return [file binary decode Batch [cs::generate $n $seed] -align natural]
}
