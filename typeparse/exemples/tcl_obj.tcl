# tcl_obj.tcl --
#
# FR : La structure Tcl_Obj de Tcl 9 (generic/tcl.h) décrite avec nos types
#      binaires. Disposition LP64 little-endian (Linux/macOS x86-64, arm64) :
#      Tcl_Size = ptrdiff_t = 8 octets signés, pointeur = 8 octets, long = 8.
#      (Sous Windows, long fait 4 octets : seul longValue changerait.)
#
#        typedef struct Tcl_Obj {
#            Tcl_Size refCount;            0
#            char *bytes;                  8
#            Tcl_Size length;             16
#            const Tcl_ObjType *typePtr;  24
#            union { ... } internalRep;   32   (16 octets)
#        } Tcl_Obj;                       taille 48
#
#      Deux limites du modèle, dites franchement :
#       - un pointeur est un simple u64le : pas de type lien (voir discussion) ;
#       - l'union internalRep n'a pas de balise dans la structure : le
#         discriminant est typePtr, qui est HORS de l'union et indirect. On ne
#         peut donc pas la décrire comme une somme balisée. On la lit en
#         16 octets bruts, et on fournit une VUE par variante.
#
# EN : Tcl 9's Tcl_Obj (generic/tcl.h) described with our binary types.
#      LP64 little-endian layout. Two limits of the model: a pointer is a plain
#      u64le (no link type), and internalRep is an untagged union (its
#      discriminant, typePtr, is outside it and indirect), so it is read as
#      16 raw bytes with one VIEW per variant.
#
#   usage : tclsh tcl_obj.tcl

set racine [file join [file dirname [file normalize [info script]]] .. layout]
source [file join $racine filebinary.tcl]
source [file join $racine binaliases.tcl]

# --- Types de base C (LP64 little-endian) -----------------------------------
type alias Tcl_Size    i64le      ;# ptrdiff_t
type alias ptr         u64le      ;# void *, char *, Tcl_ObjType *
type alias Tcl_WideInt i64le

# --- Tcl_ObjInternalRep : l'union de tcl.h (externalisée dans son propre type)
#     Les variantes sont les membres de l'union, avec leurs noms C.
type def rep_longValue     {{i64le}longValue}                    ;# long
type def rep_doubleValue   {{f64le}doubleValue}                  ;# double
type def rep_otherValuePtr {{ptr}otherValuePtr}                  ;# void *
type def rep_wideValue     {{Tcl_WideInt}wideValue}              ;# Tcl_WideInt
type def rep_twoPtrValue   {{ptr}ptr1; {ptr}ptr2}                ;# struct
type def rep_ptrAndLongRep {{ptr}ptr; {u64le}value}              ;# struct
type def rep_ptrAndSize    {{ptr}ptr; {Tcl_Size}size}            ;# struct

# FR : la SOMME : une valeur est UNE de ces variantes (comme l'union C).
# EN : the SUM: a value is ONE of these variants (like the C union).
type def Tcl_ObjInternalRep {
    {rep_longValue}longValue, {rep_doubleValue}doubleValue,
    {rep_otherValuePtr}otherValuePtr, {rep_wideValue}wideValue,
    {rep_twoPtrValue}twoPtrValue, {rep_ptrAndLongRep}ptrAndLongRep,
    {rep_ptrAndSize}ptrAndSize
}

# --- La structure elle-même -------------------------------------------------
# FR : Pour `type binary`, une somme a besoin d'un SÉLECTEUR : un champ déjà lu
#      dont la valeur est le nom de la variante (S($champ)). Ici le seul
#      discriminant est typePtr, une adresse, qui ne désigne pas un nom de
#      variante. Le champ est donc lu en octets bruts (16 = taille de la
#      plus grande variante) et on applique la vue voulue.
# EN : For `type binary` a sum needs a SELECTOR: an already-read field whose
#      value is the variant's name (S($field)). Here the only discriminant is
#      typePtr, an address, which names no variant. So the field is read as
#      raw bytes (16 = size of the largest variant) and the wanted view is applied.
type def Tcl_Obj {
    {Tcl_Size}refCount;
    {ptr}bytes;                    # NULL = pas de représentation chaîne
    {Tcl_Size}length;
    {ptr}typePtr;                  # NULL = pas de représentation interne
    {string(raw,16)}internalRep    # = Tcl_ObjInternalRep, vue à choisir
}

# FR : Variante CHOISIE par paramètre : Tcl_ObjAs(twoPtrValue) fixe la variante de
#      l'union, donc la taille (48) et la lecture. Le nom entre parenthèses est
#      celui d'une variante de Tcl_ObjInternalRep ; c'est à l'appelant de savoir
#      laquelle est valide (d'après typePtr).
# EN : Variant CHOSEN by a parameter: Tcl_ObjAs(twoPtrValue) fixes the union's
#      variant, hence the size (48) and the reading. The caller must know
#      which one is valid (from typePtr).
type def {Tcl_ObjAs({string}rep)} {
    {Tcl_Size}refCount; {ptr}bytes; {Tcl_Size}length; {ptr}typePtr;
    {Tcl_ObjInternalRep($rep)}internalRep
}

# --- Démonstration : un objet « liste » fabriqué à la main ------------------
if {[info exists argv0] && [file tail $argv0] eq "tcl_obj.tcl"} {
    set oct [binary format {w w w w w w} \
        2 0x5000 11 0x7000 0x9000 0x9100]
    set o [type binary decode Tcl_Obj $oct]
    puts "sizeof Tcl_Obj = [type binary {sizeof?}Tcl_Obj]"
    puts "refCount=$o(refCount) length=$o(length)"
    puts [format "bytes=0x%x typePtr=0x%x" $o(bytes) $o(typePtr)]
    set v [type binary decode rep_twoPtrValue $o(internalRep)]
    puts [format "twoPtr: ptr1=0x%x ptr2=0x%x" $v(ptr1) $v(ptr2)]
    # aller-retour / round trip
    puts "aller-retour: [expr {[type binary encode Tcl_Obj $o] eq $oct}]"

    # la somme directement, par paramètre / the sum directly, by parameter
    puts "sizeof Tcl_ObjAs(twoPtrValue) = [type binary {sizeof?}{Tcl_ObjAs(twoPtrValue)}]"
    set t [type binary decode {Tcl_ObjAs(twoPtrValue)} $oct]
    puts [format "internalRep.twoPtrValue.ptr2 = 0x%x" \
        $t(internalRep.twoPtrValue.ptr2)]
    puts "aller-retour: [expr {[type binary encode {Tcl_ObjAs(twoPtrValue)} $t] eq $oct}]"
    set w [type binary decode {Tcl_ObjAs(wideValue)} $oct]
    puts "wideValue = $w(internalRep.wideValue.wideValue)"
}
