# cstruct_classique.tcl --
# FR : la même chose que cstruct.tcl, en Tcl 9.0 classique (sans annotations) :
#      un Batch est la liste {n items}, un Sample la liste {id value flags} ; la
#      disposition C est écrite à la main avec binary format / binary scan.
# EN : the same as cstruct.tcl, in classic Tcl 9.0 (no annotations): a Batch is
#      the list {n items}, a Sample the list {id value flags}; the C layout is
#      written by hand with binary format / binary scan.
#
#   struct Sample { uint32_t id; double value; uint8_t flags; }  -> i x4 q c x7  (24 octets)
#   struct Batch  { uint32_t n; Sample items[]; }                -> i x4 + items  (8 + 24 n)
#   struct Summary{ uint32_t count; double min, max, mean; }     -> iu x4 q q q   (32 octets)
#   Hypothèse : x86-64 little-endian, comme cstruct.c.

namespace eval csc {}

proc csc::init {{lib ""}} {
    if {$lib eq ""} {set lib [file join [file dirname [info script]] cstruct[info sharedlibextension]]}
    ::load $lib Cstruct
}

# --- Variante NUE : aucun contrôle (ce que l'on écrit d'habitude) ---------------
proc csc::encode {batch} {
    lassign $batch n items
    set flat {}
    foreach it $items {lappend flat {*}$it}
    binary format "ix4[string repeat ix4qcx7 [llength $items]]" $n {*}$flat
}

proc csc::decode {bytes} {
    binary scan $bytes iu n
    set items {}
    for {set i 0; set off 8} {$i < $n} {incr i; incr off 24} {
        binary scan $bytes @${off}iux4qcu id v fl
        lappend items [list $id $v $fl]
    }
    list $n $items
}

proc csc::summary {bytes} {
    binary scan $bytes iux4qqq count min max mean
    dict create count $count min $min max $max mean $mean
}

proc csc::stats {batch} {
    summary [cs::stats_raw [encode $batch]]
}

# --- Variante CONTRÔLÉE : les contrôles que fait le type Batch -------------------
# u32 pour id et n, double pour value, u8 pour flags, n = nombre d'éléments.
proc csc::check {batch} {
    if {[llength $batch] != 2} {error "a Batch is {n items}"}
    lassign $batch n items
    if {![string is entier -strict $n] || $n < 0 || $n > 0xFFFFFFFF} {error "n: expected u32, got \"$n\""}
    if {[llength $items] != $n} {error "Batch holds [llength $items] items, but n says $n"}
    foreach it $items {
        if {[llength $it] != 3} {error "a Sample is {id value flags}"}
        lassign $it id v fl
        if {![string is entier -strict $id] || $id < 0 || $id > 0xFFFFFFFF} {error "id: expected u32, got \"$id\""}
        if {![string is double -strict $v]} {error "value: expected double, got \"$v\""}
        if {![string is entier -strict $fl] || $fl < 0 || $fl > 255} {error "flags: expected u8, got \"$fl\""}
    }
    return $batch
}

proc csc::stats_checked {batch} {
    summary [cs::stats_raw [encode [check $batch]]]
}

# --- Calcul en Tcl pur, sur la liste ---------------------------------------------
proc csc::compute {batch} {
    set sum 0.0; set cnt 0; set mn ""; set mx ""
    foreach it [lindex $batch 1] {
        lassign $it id v fl
        if {$fl & 1} continue
        set sum [expr {$sum + $v}]; incr cnt
        if {$mn eq "" || $v < $mn} {set mn $v}
        if {$mx eq "" || $v > $mx} {set mx $v}
    }
    list $cnt $mn $mx [expr {$sum / $cnt}]
}
