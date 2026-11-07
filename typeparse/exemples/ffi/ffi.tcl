# ffi.tcl -- appeler une fonction d'une bibliothèque partagée depuis sa
# signature typée (prototype)
#
# FR : la signature d'une procédure typée décrit le prototype C ; les types
#      binaires (layout/filebinary.tcl) décrivent les données. ffi::fn n'a
#      donc rien à apprendre de plus :
#
#        ffi::fn libc.so.6 {int(u,64)}strlen {{string(utf-8,z)}s}
#        ffi::fn libc.so.6 {int(s,32)}timespec_get {{var.timespec}ts; {int(s,32)}base}
#        timespec_get t 1 ; puts $t(sec)
#
#      La procédure Tcl de même nom lie et vérifie les arguments (défauts,
#      options, liens, messages d'usage : comme proc), encode ce qui passe par
#      adresse avec type binary encode, appelle un talon C, et décode ce qui
#      revient. Le talon est généré depuis la signature et compilé par le
#      compilateur de cproc (gcc ou tcc), au premier appel ; une bibliothèque
#      compilée est gardée en cache. Pas de libffi : c'est la voie « talon
#      compilé », comme critcl.
#
#      Correspondance (argument -> C) :
#        int(s,N), int(u,N)     intN_t, uintN_t        (valeur)
#        double, double(64)     double ; double(32) : float
#        flag                   int
#        ptr                    void *, une adresse (un entier, sans contrôle)
#        size_t, c_int, c_long  les tailles de la plate-forme
#        string(ENC,z), string  const char * : le texte encodé, NUL final
#        ref.T                  const T * : la valeur encodée (entrée)
#        list.T, list(N).T      const T * : le tableau encodé (entrée)
#        var.T, var.list.T      T * : la variable encodée, passée par
#                               adresse, puis décodée dans la variable
#                               (entrée et sortie ; absente : des zéros)
#        un type de rappel      un pointeur de fonction (ffi::callback)
#      Retour : un scalaire, ptr, string(utf-8,z) (char *, NULL : ""), ou
#      rien (pas d'annotation : void).
#
#      ffi::callback {R}nom {signature} : un type de pointeur de fonction.
#      L'argument est une lambda (apply, typée de préférence) ; un paramètre
#      ref.T lui arrive décodé (qsort : les deux éléments comparés).
#
# EN : the typed signature is the C prototype; binary types are the data.
#      A generated C stub, compiled with cproc's compiler, makes the call.
#
# Limites du prototype : pas de struct par valeur, pas de fonction
# variadique (printf), pas de type de retour struct ni de char * à libérer ;
# un rappel n'est pas réentrant d'un thread à l'autre ; les adresses (ptr)
# ne sont pas contrôlées : à ne jamais exposer dans un interpréteur sûr.

namespace eval ::ffi {variable here [file dirname [file normalize [info script]]]}
source [file join $::ffi::here .. cproc cproc.tcl]
source [file join $::ffi::here .. .. layout filebinary.tcl]

namespace eval ::ffi {
    variable seq 0		;# fonctions
    variable cbseq 0		;# types de rappel
    variable pending {}		;# fonctions à compiler
    variable callbacks {}	;# nom -> {id ret params}
    variable lastSource {}
    namespace eval c {}		;# les commandes C
    namespace eval cb {}	;# l'adaptation Tcl des rappels

    # les tailles de la plate-forme
    set w [expr {8 * $::tcl_platform(pointerSize)}]
    set lw [expr {$::tcl_platform(platform) eq "windows" ? 32 : $w}]
    uplevel #0 [list type def ptr int(u,$w)]
    uplevel #0 [list type def size_t int(u,$w)]
    uplevel #0 [list type def c_int int(s,32)]
    uplevel #0 [list type def c_long int(s,$lw)]
    unset w lw
}

# ----------------------------------------------------------------------
# Ce que voit le C d'un type : {kind ctype detail}
#   num   : un nombre passé par valeur (detail : sint, uint, float, flag)
#   cstr  : un texte encodé (detail : le type binaire string(ENC,z))
#   in    : un tampon d'entrée (detail : le type à encoder)
#   out   : un tampon d'entrée et de sortie (detail : le type)
#   cb    : un pointeur de fonction (detail : le nom du rappel)
# ----------------------------------------------------------------------

proc {list}::ffi::Scalar {{string}t} {
    switch -- $t {
	double - double(64) {return {num double float}}
	double(32) {return {num float float}}
	flag {return {num int flag}}
	string {return {cstr {const char *} string(utf-8,z)}}
    }
    if {[catch {type binary {what?}$t} w]} {
	return {}
    }
    switch -- [lindex $w 0] {
	sint {return [list num int[expr {8 * [lindex $w 1]}]_t sint]}
	uint {return [list num uint[expr {8 * [lindex $w 1]}]_t uint]}
	float {
	    return [list num [expr {[lindex $w 1] == 4 ? "float" : "double"}] \
		    float]
	}
	cstring {return [list cstr {const char *} $t]}
    }
    return {}
}

proc {list}::ffi::Classify {{string}t} {
    variable callbacks
    set s [Scalar $t]
    if {$s ne ""} {return $s}
    if {[dict exists $callbacks $t]} {
	return [list cb ffi_cb[dict get $callbacks $t id]_t $t]
    }
    if {[regexp {^ref\.(.+)$} $t -> e]} {
	return [list in {const void *} $e]
    }
    if {[regexp {^var\.(.+)$} $t -> e]} {
	return [list out {void *} $e]
    }
    if {[regexp {^list(\(\d+\))?\.(.+)$} $t]} {
	return [list in {const void *} $t]
    }
    return -code error "ffi: type \"$t\" has no C form here (a number, a\
	    string(ENC,z), ref.T, var.T, list.T or a callback type)"
}

# le type Tcl de la signature de la procédure : ref.T se lit T
proc {string}::ffi::TclType {{string}t} {
    regsub {^ref\.} $t {}
}

# ----------------------------------------------------------------------
# Les aides appelées par les procédures générées
# ----------------------------------------------------------------------

# Voie rapide : un scalaire, ou un tableau de scalaires, s'écrit avec
# binary format (codes natifs) ; le reste passe par type binary.
# Fmt : {code signe} ou {} ; gardé par type.
proc ::ffi::Fmt {t} {
    variable fmts
    if {[info exists fmts($t)]} {return $fmts($t)}
    set r {}
    if {![catch {type binary {what?}$t} w]} {
	switch -- [lindex $w 0] {
	    sint - uint {
		set c [dict get {1 c 2 t 4 n 8 m} [lindex $w 1]]
		set r [list $c [expr {[lindex $w 0] eq "uint" ? "u" : ""}]]
	    }
	    float {set r [list [expr {[lindex $w 1] == 4 ? "f" : "d"}] {}]}
	}
    } elseif {$t in {double double(64)}} {
	set r {d {}}
    }
    set fmts($t) $r
}
# l'élément d'un tableau list(N).T, ou {}
proc ::ffi::Elem {t} {
    if {[regexp {^list(?:\(\d+\))?\.(.+)$} $t -> e]} {return $e}
    return {}
}

# une valeur de T en octets (alignement du C)
proc ::ffi::Enc {t v} {
    set f [Fmt $t]
    if {$f ne ""} {return [binary format [lindex $f 0] $v]}
    set e [Elem $t]
    if {$e ne "" && [set f [Fmt $e]] ne ""} {
	return [binary format [lindex $f 0]* $v]
    }
    if {[lindex [type binary {what?}$t] 0] in {product array}} {
	set v [type cast $t $v]
    }
    type binary encode $t $v -align natural
}
proc ::ffi::Zero {t} {
    string repeat \0 [type binary {sizeof?}$t -align natural]
}
proc ::ffi::Dec {t b} {
    set f [Fmt $t]
    if {$f ne ""} {
	binary scan $b [join $f ""] v
	return $v
    }
    set e [Elem $t]
    if {$e ne "" && [set f [Fmt $e]] ne ""} {
	binary scan $b [lindex $f 0][lindex $f 1]* v
	return $v
    }
    type binary decode $t $b -align natural
}
# le type d'un tableau à N éléments : list.T, list(N).T -> list(N).T
proc ::ffi::Sized {t n} {
    regsub {^list(\(\d+\))?\.} $t "list($n)."
}
# un texte en octets, NUL final (string(ENC,z))
proc ::ffi::Str {t s} {
    if {$t eq "string(utf-8,z)"} {
	return "[encoding convertto utf-8 $s]\0"
    }
    type binary encode $t $s
}
# lire un T à une adresse (ref.T d'un rappel)
proc ::ffi::Peek {t addr} {
    variable sizes
    if {![info exists sizes($t)]} {
	set sizes($t) [type binary {sizeof?}$t -align natural]
    }
    Dec $t [::ffi::c::peek $addr $sizes($t)]
}

# ----------------------------------------------------------------------
# ffi::callback {R}nom {signature}
# ----------------------------------------------------------------------

proc ::ffi::callback {args} {
    variable callbacks
    variable cbseq
    if {[llength $args] != 2} {
	return -code error "wrong # args: should be \"ffi::callback {R}name\
		signature\""
    }
    set ret [note get [lindex $args 0]]
    set name [string range [string cat _ [lindex $args 0]] 1 end]
    set sig [string range [string cat _ [lindex $args 1]] 1 end]
    set params {}
    set call {}
    set lnames {}
    foreach f [::cproc::ParseSig $sig] {
	set k [Classify $f(ttext)]
	if {[lindex $k 0] ni {num in}} {
	    return -code error "ffi::callback $name: parameter $f(name) must\
		    be a number, ptr or ref.T"
	}
	lappend params [list $f(name) {*}$k]
	lappend lnames $f(name)
	if {[lindex $k 0] eq "in"} {
	    append call " \[::ffi::Peek [list [lindex $k 2]] \$$f(name)\]"
	} else {
	    append call " \$$f(name)"
	}
    }
    set rk {}
    if {$ret ne ""} {
	set rk [Scalar $ret]
	if {[lindex $rk 0] ne "num"} {
	    return -code error "ffi::callback $name: the result must be a number"
	}
    }
    dict set callbacks $name [dict create id [incr cbseq] ret $rk \
	    params $params]
    # le type de l'argument : une lambda (un texte)
    uplevel 1 [list type def $name string]
    proc ::ffi::cb::$name [list lam {*}$lnames] "apply \$lam$call"
    return
}

# ----------------------------------------------------------------------
# ffi::fn LIB {R}nom {signature}
# ----------------------------------------------------------------------

proc ::ffi::fn {args} {
    variable seq
    variable pending
    if {[llength $args] != 3} {
	return -code error "wrong # args: should be \"ffi::fn library\
		{R}name signature\""
    }
    lassign $args lib
    set ret [note get [lindex $args 1]]
    set name [string range [string cat _ [lindex $args 1]] 1 end]
    set sig [string range [string cat _ [lindex $args 2]] 1 end]
    set ns [uplevel 1 {namespace current}]
    if {![string match ::* $name]} {
	set name [expr {$ns eq "::" ? "::$name" : "${ns}::$name"}]
    }
    set sym [namespace tail $name]
    set id [incr seq]
    set cmd ::ffi::c::f$id

    set rk {}
    set tret ""
    if {$ret ne ""} {
	set rk [Scalar $ret]
	switch -- [lindex $rk 0] {
	    num {set tret $ret}
	    cstr {
		if {$ret ni {string string(utf-8,z)}} {
		    return -code error "ffi::fn $sym: a char * result is read\
			    as string(utf-8,z)"
		}
		set rk {cstr {const char *} string(utf-8,z)}
		set tret string
	    }
	    default {
		return -code error "ffi::fn $sym: the result must be a number,\
			ptr or string(utf-8,z)"
	    }
	}
    }

    # les paramètres, la signature Tcl, l'appel du talon, le retour
    set params {}
    set tsig {}
    set call $cmd
    set post {}
    set nout 0
    foreach f [::cproc::ParseSig $sig] {
	set local [string trimleft $f(name) -]
	set k [Classify $f(ttext)]
	lassign $k kind ctype detail
	lappend params [list $local {*}$k]
	# le champ de la procédure Tcl : ref.T s'y lit T
	set fd "{[TclType $f(ttext)]}$f(name)"
	set d $f(def)
	switch -- [::cproc::Variant $d] {
	    const {append fd " ?= [list $d(const)]"}
	    computed {append fd " ?= $d(computed)"}
	}
	lappend tsig $fd
	switch -- $kind {
	    num - cb {append call " \$$local"}
	    cstr {append call " \[::ffi::Str [list $detail] \$$local\]"}
	    in {
		if {[string match list* $detail]} {
		    append call " \[::ffi::Enc \[::ffi::Sized [list $detail]\
			    \[llength \$$local\]\] \$$local\]"
		} else {
		    append call " \[::ffi::Enc [list $detail] \$$local\]"
		}
	    }
	    out {
		incr nout
		if {[string match list* $detail]} {
		    # la taille est celle de la liste dans la variable
		    set t "\[::ffi::Sized [list $detail] \[llength \$$local\]\]"
		    append call " \[::ffi::Enc $t \$$local\]"
		    append post "    set $local \[::ffi::Dec $t\
			    \[lindex \$_r $nout\]\]\n"
		} else {
		    append call " \[if {\[info exists $local\]} {::ffi::Enc\
			    [list $detail] \$$local} else {::ffi::Zero\
			    [list $detail]}\]"
		    append post "    set $local \[::ffi::Dec [list $detail]\
			    \[lindex \$_r $nout\]\]\n"
		}
	    }
	}
    }
    if {$nout > 0} {
	set body "    set _r \[$call\]\n$post"
	if {$rk ne ""} {append body "    return \[lindex \$_r 0\]\n"}
    } else {
	set body "    $call\n"
    }
    set tsig [join $tsig "; "]
    if {$tret eq ""} {
	uplevel #0 [list proc $name $tsig $body]
    } else {
	uplevel #0 "proc {$tret}[list $name] [list $tsig] [list $body]"
    }
    # en attendant la compilation
    proc $cmd {args} [format {
	::ffi::build
	if {[info procs %s] ne ""} {
	    return -code error "ffi: %s was not compiled"
	}
	tailcall %s {*}$args
    } $cmd $name $cmd]
    lappend pending [dict create id $id lib $lib sym $sym cmd $cmd \
	    ret $rk params $params]
    return
}

# ----------------------------------------------------------------------
# Le C
# ----------------------------------------------------------------------

proc ::ffi::CStr {s} {
    return "\"[string map {\\ \\\\ \" \\\"} $s]\""
}

proc ::ffi::Gen {fns} {
    variable callbacks
    set c "/* Produit par ffi : ne pas modifier. */\n"
    append c "#include <tcl.h>\n#include <stdint.h>\n#include <stddef.h>\n"
    append c "#include <string.h>\n#include <stdio.h>\n#include <stdarg.h>\n"
    append c [::cproc::Prelude]
    append c {
/* les bibliothèques ouvertes, par chemin */
static struct { char *path; Tcl_LoadHandle h; } ffi_libs[64];
static int ffi_nlibs = 0;

static void *
FfiSym(Tcl_Interp *ip, const char *lib, const char *sym)
{
    static const char *none[] = { NULL };
    Tcl_LoadHandle h = NULL;
    Tcl_Obj *p;
    void *f;
    int i;

    for (i = 0; i < ffi_nlibs; i++) {
	if (strcmp(ffi_libs[i].path, lib) == 0) {
	    h = ffi_libs[i].h;
	    break;
	}
    }
    if (h == NULL) {
	if (ffi_nlibs == 64) {
	    Tcl_SetObjResult(ip, Tcl_NewStringObj("ffi: too many libraries", -1));
	    return NULL;
	}
	p = Tcl_NewStringObj(lib, -1);
	Tcl_IncrRefCount(p);
	if (Tcl_LoadFile(ip, p, none, 0, NULL, &h) != TCL_OK) {
	    Tcl_DecrRefCount(p);
	    return NULL;
	}
	Tcl_DecrRefCount(p);
	ffi_libs[ffi_nlibs].path = (char *) Tcl_Alloc(strlen(lib) + 1);
	strcpy(ffi_libs[ffi_nlibs].path, lib);
	ffi_libs[ffi_nlibs++].h = h;
    }
    f = Tcl_FindSymbol(ip, h, sym);
    return f;
}

/* à la sortie : les bibliothèques ouvertes sont refermées */
static void
FfiExit(void *cd)
{
    int i;

    (void) cd;
    for (i = 0; i < ffi_nlibs; i++) {
	Tcl_FSUnloadFile(NULL, ffi_libs[i].h);
	Tcl_Free(ffi_libs[i].path);
    }
    ffi_nlibs = 0;
}

/* ::ffi::c::peek adresse n : n octets lus à l'adresse */
static int
FfiPeek(void *cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv[])
{
    Tcl_WideUInt a;
    Tcl_WideInt n;

    (void) cd;
    if (objc != 3) {
	Tcl_WrongNumArgs(ip, 1, objv, "address n");
	return TCL_ERROR;
    }
    if (Tcl_GetWideUIntFromObj(ip, objv[1], &a) != TCL_OK
	    || Tcl_GetWideIntFromObj(ip, objv[2], &n) != TCL_OK) {
	return TCL_ERROR;
    }
    if (a == 0 || n < 0) {
	Tcl_SetObjResult(ip, Tcl_NewStringObj("ffi: bad address", -1));
	return TCL_ERROR;
    }
    Tcl_SetObjResult(ip, Tcl_NewByteArrayObj((const unsigned char *)
	    (uintptr_t) a, (Tcl_Size) n));
    return TCL_OK;
}
}
    # les rappels utilisés
    set used {}
    foreach f $fns {
	foreach p [dict get $f params] {
	    if {[lindex $p 1] eq "cb" && [lindex $p 3] ni $used} {
		lappend used [lindex $p 3]
	    }
	}
    }
    foreach name $used {
	append c [GenCallback $name [dict get $callbacks $name]]
    }
    foreach f $fns {
	append c [GenFn $f]
    }
    return $c
}

# un nombre Tcl -> C (DST de type CT ; détail sint, uint, float, flag)
proc ::ffi::GetNum {how ct obj dst} {
    switch -- $how {
	sint {
	    return "    { Tcl_WideInt w_; if (Tcl_GetWideIntFromObj(ip, $obj,\
		    &w_) != TCL_OK) goto fail; $dst = ($ct) w_; }\n"
	}
	uint {
	    return "    { Tcl_WideUInt w_; if (Tcl_GetWideUIntFromObj(ip, $obj,\
		    &w_) != TCL_OK) goto fail; $dst = ($ct) w_; }\n"
	}
	float {
	    return "    { double d_; if (Tcl_GetDoubleFromObj(ip, $obj, &d_)\
		    != TCL_OK) goto fail; $dst = ($ct) d_; }\n"
	}
	flag {
	    return "    { int i_; if (Tcl_GetBooleanFromObj(ip, $obj, &i_)\
		    != TCL_OK) goto fail; $dst = i_; }\n"
	}
    }
}
proc ::ffi::NewNum {how src} {
    switch -- $how {
	sint - flag {return "Tcl_NewWideIntObj((Tcl_WideInt) $src)"}
	uint {return "Tcl_NewWideUIntObj((Tcl_WideUInt) $src)"}
	float {return "Tcl_NewDoubleObj((double) $src)"}
    }
}

proc ::ffi::GenCallback {name d} {
    set k [dict get $d id]
    set rk [dict get $d ret]
    set rct [expr {$rk eq "" ? "void" : [lindex $rk 1]}]
    set ps [dict get $d params]
    set n [llength $ps]
    set protos [lmap p $ps {lindex $p 2}]
    if {$protos eq ""} {set protos void}
    set args {}
    set i 0
    foreach p $ps {lappend args "[lindex $p 2] x$i"; incr i}
    if {$args eq ""} {set args void}
    set c "\n/* rappel $name */\n"
    append c "typedef $rct (*ffi_cb${k}_t)([join $protos {, }]);\n"
    append c "static Tcl_Interp *ffi_cb${k}_ip = NULL;\n"
    append c "static Tcl_Obj *ffi_cb${k}_lam = NULL;\n"
    append c "static int ffi_cb${k}_err = 0;\n"
    append c "static $rct\nffi_cb${k}([join $args {, }])\n\{\n"
    append c "    Tcl_Obj *v\[[expr {$n + 2}]\];\n    int i_, rc_;\n"
    if {$rct ne "void"} {append c "    $rct r_ = 0;\n"}
    set rr [expr {$rct eq "void" ? "" : " r_"}]
    append c "    if (ffi_cb${k}_err) return$rr;\n"
    append c "    v\[0\] = Tcl_NewStringObj([CStr ::ffi::cb::$name], -1);\n"
    append c "    v\[1\] = ffi_cb${k}_lam;\n"
    set i 0
    foreach p $ps {
	lassign $p pn kind ct how
	if {$kind eq "in"} {
	    append c "    v\[[expr {$i + 2}]\] = Tcl_NewWideUIntObj((Tcl_WideUInt) (uintptr_t) x$i);\n"
	} else {
	    append c "    v\[[expr {$i + 2}]\] = [NewNum $how x$i];\n"
	}
	incr i
    }
    append c "    for (i_ = 0; i_ < [expr {$n + 2}]; i_++) Tcl_IncrRefCount(v\[i_\]);\n"
    append c "    rc_ = Tcl_EvalObjv(ffi_cb${k}_ip, [expr {$n + 2}], v, 0);\n"
    append c "    for (i_ = 0; i_ < [expr {$n + 2}]; i_++) Tcl_DecrRefCount(v\[i_\]);\n"
    append c "    if (rc_ != TCL_OK) { ffi_cb${k}_err = 1; return$rr; }\n"
    if {$rct ne "void"} {
	set ip ffi_cb${k}_ip
	set g [GetNum [lindex $rk 2] $rct "Tcl_GetObjResult($ip)" r_]
	set g [string map [list "(ip," "($ip,"] $g]
	append c $g
	append c "    return r_;\n  fail:\n    ffi_cb${k}_err = 1;\n    return 0;\n"
    }
    append c "\}\n"
    return $c
}

proc ::ffi::GenFn {f} {
    variable callbacks
    set id [dict get $f id]
    set rk [dict get $f ret]
    set rct [expr {$rk eq "" ? "void" : [lindex $rk 1]}]
    set ps [dict get $f params]
    set n [llength $ps]
    set protos [lmap p $ps {lindex $p 2}]
    if {$protos eq ""} {set protos void}
    set c "\n/* [dict get $f lib] : [dict get $f sym] */\n"
    append c "typedef $rct (*ffi_t$id)([join $protos {, }]);\n"
    append c "static ffi_t$id ffi_p$id = NULL;\n"
    append c "static int\nffi_cmd$id\(void *cd, Tcl_Interp *ip, int objc,\
	    Tcl_Obj *const objv\[\])\n\{\n"
    set i 0
    set outs {}
    set cbs {}
    foreach p $ps {
	lassign $p pn kind ct
	append c "    $ct a$i;\n"
	if {$kind eq "out"} {
	    append c "    Tcl_Obj *b$i = NULL;\n    Tcl_Size n$i;\n"
	    lappend outs $i
	}
	if {$kind eq "cb"} {
	    set k [dict get $callbacks [lindex $p 3] id]
	    append c "    Tcl_Interp *sip$i = ffi_cb${k}_ip;\n"
	    append c "    Tcl_Obj *slam$i = ffi_cb${k}_lam;\n"
	    append c "    int serr$i = ffi_cb${k}_err;\n"
	    lappend cbs [list $i $k]
	}
	incr i
    }
    if {$rct ne "void"} {append c "    $rct r;\n"}
    append c "    int ok = 0;\n"
    append c "    (void) cd;\n"
    append c "    if (objc != [expr {$n + 1}]) \{\n"
    append c "\tTcl_WrongNumArgs(ip, 1, objv, [CStr [lmap p $ps {lindex $p 0}]]);\n"
    append c "\treturn TCL_ERROR;\n    \}\n"
    append c "    if (ffi_p$id == NULL && (ffi_p$id = (ffi_t$id) FfiSym(ip,\
	    [CStr [dict get $f lib]], [CStr [dict get $f sym]])) == NULL) \{\n"
    append c "\treturn TCL_ERROR;\n    \}\n"
    set i 0
    foreach p $ps {
	lassign $p pn kind ct how
	set o "objv\[[expr {$i + 1}]\]"
	switch -- $kind {
	    num {append c [GetNum $how $ct $o a$i]}
	    cstr - in {
		append c "    a$i = ($ct) Tcl_GetBytesFromObj(ip, $o, NULL);\n"
		append c "    if (a$i == NULL) goto fail;\n"
	    }
	    out {
		append c "    b$i = Tcl_DuplicateObj($o);\n"
		append c "    Tcl_IncrRefCount(b$i);\n"
		append c "    if (Tcl_GetBytesFromObj(ip, b$i, &n$i) == NULL) goto fail;\n"
		append c "    a$i = Tcl_SetByteArrayLength(b$i, n$i);\n"
	    }
	}
	incr i
    }
    # les rappels en dernier : rien ne peut plus échouer avant l'appel
    foreach ck $cbs {
	lassign $ck i k
	append c "    ffi_cb${k}_ip = ip;\n"
	append c "    ffi_cb${k}_lam = objv\[[expr {$i + 1}]\];\n"
	append c "    ffi_cb${k}_err = 0;\n"
	append c "    a$i = ffi_cb$k;\n"
    }
    set call "ffi_p$id\([join [lmap i [lsearch -all $ps *] {string cat a $i}] {, }]\)"
    if {$rct eq "void"} {
	append c "    $call;\n"
    } else {
	append c "    r = $call;\n"
    }
    set errs {}
    foreach ck $cbs {
	lassign $ck i k
	lappend errs "ffi_cb${k}_err"
    }
    if {$errs ne ""} {
	append c "    if ([join $errs { || }]) goto restore;\n"
    }
    # le résultat
    switch -- [lindex $rk 0] {
	"" {set res "Tcl_NewObj()"}
	num {set res [NewNum [lindex $rk 2] r]}
	cstr {set res "(r ? Tcl_NewStringObj(r, -1) : Tcl_NewObj())"}
    }
    if {$outs eq ""} {
	append c "    Tcl_SetObjResult(ip, $res);\n"
    } else {
	append c "    \{\n\tTcl_Obj *l = Tcl_NewListObj(0, NULL);\n"
	append c "\tTcl_ListObjAppendElement(NULL, l, $res);\n"
	foreach i $outs {
	    append c "\tTcl_ListObjAppendElement(NULL, l, b$i);\n"
	}
	append c "\tTcl_SetObjResult(ip, l);\n    \}\n"
    }
    append c "    ok = 1;\n"
    append c "  restore:\n"
    foreach ck $cbs {
	lassign $ck i k
	append c "    ffi_cb${k}_ip = sip$i;\n"
	append c "    ffi_cb${k}_lam = slam$i;\n"
	append c "    ffi_cb${k}_err = serr$i;\n"
    }
    append c "  fail:\n"
    foreach i $outs {
	append c "    if (b$i != NULL) Tcl_DecrRefCount(b$i);\n"
    }
    append c "    return ok ? TCL_OK : TCL_ERROR;\n\}\n"
    return $c
}

proc ::ffi::Init {prefix fns} {
    set c "\nDLLEXPORT int\n${prefix}_Init(Tcl_Interp *ip)\n\{\n"
    append c "    if (!CprocInitStubs(ip)) return TCL_ERROR;\n"
    append c "    Tcl_CreateObjCommand(ip, \"::ffi::c::peek\", FfiPeek, NULL, NULL);\n"
    foreach f $fns {
	append c "    Tcl_CreateObjCommand(ip, [CStr [dict get $f cmd]],\
		ffi_cmd[dict get $f id], NULL, NULL);\n"
    }
    append c "    Tcl_CreateExitHandler(FfiExit, NULL);\n"
    append c "    return TCL_OK;\n\}\n"
}

# compile ce qui attend (le compilateur et le cache de cproc)
proc ::ffi::build {} {
    variable pending
    variable lastSource
    if {[llength $pending] == 0} return
    set fns $pending
    set c [Gen $fns]
    upvar #0 ::cproc::cfg cfg
    set cc $cfg(cc)
    set istcc [string match *tcc* [file tail $cc]]
    set flags $cfg(cflags)
    if {$flags eq ""} {
	set flags [expr {$istcc ? {} : {-O2}}]
    }
    set key [format %08x%08x [zlib crc32 "$cc $flags\n$c"] \
	    [zlib adler32 "$cc $flags\n$c"]]
    set prefix Ffi$key
    append c [Init $prefix $fns]
    set lastSource $c
    set lib [file join [::cproc::CacheDir] ffi_$key[info sharedlibextension]]
    if {![file exists $lib]} {
	::cproc::Compile $c $cc $flags $istcc $lib
    }
    load $lib $prefix
    set pending [lrange $pending [llength $fns] end]
    return
}

proc ::ffi::source {} {
    variable lastSource
    return $lastSource
}

package provide ffi 0.1
