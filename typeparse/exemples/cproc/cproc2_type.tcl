# cproc2_type.tcl -- cproc, étape 2 : le générateur en Tcl typé
#
# FR : le même générateur que cproc1_tcl.tcl, et le même C produit ; ses
#      données ont maintenant un type, et ses procédures une signature :
#
#        un champ de signature   field {ttext; name; def}, def une somme :
#                                rien, une constante, un script
#        ce que voit le C        kind, une somme : struct, span, num, str, obj
#        une struct exportée     cblock {qual; text; kind; name; fields}
#        une procédure à faire   cdef {id; name; cname; ...; params; ret}
#        la configuration        cconfig, une variable sous contrat
#
#      Ce que le typage change : les dicts dont la forme n'était écrite nulle
#      part (dict get $k how, dict get $k ctype...) deviennent des types
#      vérifiés ; le classement d'un type est une somme, lue par sa variante ;
#      cproc::config vérifie ses valeurs (-verbose x : expected integer).
#
# EN : the same generator as cproc1_tcl.tcl, producing the same C; its data
#      are typed, its procedures have signatures.
#
# Configuration :
#   cproc::config ?-cc C? ?-cflags L? ?-includes L? ?-cachedir D? ?-verbose 0|1?
#                       (sans argument : la configuration, en dict)
#   cproc::code {C}     du C commun (fonctions, #include) avant les corps
#   cproc::build        compile tout de suite les procédures en attente
#   cproc::source       le C de la dernière compilation (pour le lire)

namespace eval ::cproc {
    variable here [file dirname [file normalize [info script]]]
    variable seq 0
    variable codes {}		;# cproc::code
    variable lastSource {}
    namespace eval c {}		;# les commandes C : ::cproc::c::<id>

    # --- les données du générateur ----------------------------------------

    # un champ de la signature : {T}nom ?= défaut
    type def defval {"none", {string}const, {string}computed}
    type def field   {{string}ttext; {string}name; {defval}def}

    # ce que voit le C d'un type Tcl
    type def cstruct {{string}name; {string}qual; {string}ns; {string}tname}
    type def cnum    {{"double", "float", "wide", "int", "fix"}how; {string}ctype}
    type def celem {{cstruct}struct, {cnum}num}
    type def cspan   {{celem}elem; {string}ename}
    type def kind {{cstruct}struct, {cspan}span, {cnum}num, "str", "obj"}
    type def param   {{string}local; {kind}kind; {field}field}

    # un bloc de type {c!}export : une struct, une union, un scalaire
    type def cfield  {{string}ctype; {string}name; {string}dim}
    type def cblock  {{string}qual; {string}text;
		      {"struct", "union", "scalar", "other"}kind; {string}name;
		      {list.cfield}fields}

    # une procédure en attente de compilation
    type def cdef    {{int}id; {string}name; {string}cname; {string}cmd;
		      {kind}ret; {list.param}params; {string}body;
		      {string}file; {int}line; {string}ns}

    type def cconfig {{string}cc = gcc; {list.string}cflags = {};
		      {list.string}includes = {}; {string}cachedir = "";
		      {int}verbose = 0}

    # l'état, sous contrat
    set {cconfig}cfg {cc:}gcc
    set {list.cdef}pending {*}{}
}

# La variante d'une somme : struct, span... (une variante littérale se
# nomme par sa valeur)
proc {string}::cproc::Variant {v} {
    set t [type get $v]
    set i [string last . $t]
    if {$i >= 0} {return [string range $t $i+1 end]}
    # une variante littérale : sa valeur, sans le type de la somme (une
    # somme ne se caste pas en string)
    return {}$v
}

proc {string}::cproc::CType {{kind}k} {
    switch -- [Variant $k] {
	struct {return $k(struct.name)}
	span   {return cproc_span_$k(span.ename)}
	num    {return $k(num.ctype)}
	str    {return "const char *"}
	obj    {return "Tcl_Obj *"}
    }
}

# ----------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------

proc ::cproc::config {{string}-cc; {list.string}-cflags; {list.string}-includes;
	{string}-cachedir; {int}-verbose} {
    variable cfg
    foreach f {cc cflags includes cachedir verbose} {
	if {[info exists $f]} {
	    set cfg($f) [set $f]		;# vérifié par le contrat
	}
    }
    dict create -cc $cfg(cc) -cflags $cfg(cflags) -includes $cfg(includes) \
	    -cachedir $cfg(cachedir) -verbose $cfg(verbose)
}

proc ::cproc::code {{string}text} {
    variable codes
    lappend codes $text
    return
}

proc {string}::cproc::source {} {
    variable lastSource
    return $lastSource
}

# ----------------------------------------------------------------------
# cproc NOM SIGNATURE CORPS
#
# Une procédure « args » : le mot {point}decale arrive avec sa note (une
# procédure aux arguments nommés la résoudrait, et casterait "decale" en
# point) ; elle est rendue telle quelle à proc.
# ----------------------------------------------------------------------

proc ::cproc {args} {
    if {[llength $args] != 3} {
	return -code error "wrong # args: should be \"cproc name signature\
		body\""
    }
    set ns [uplevel 1 {namespace current}]
    set frame [info frame -1]
    ::cproc::Define $ns $frame {*}$args
}

# ARGS : le mot du nom (avec sa note), la signature, le corps. Le mot du nom
# ne passe jamais par un constructeur (set, argument nommé...), qui
# résoudrait sa note.
proc ::cproc::Define {ns frame args} {
    variable pending
    variable seq

    # (string cat _ ... : un objet neuf, sans la note ; format %s ou
    # string cat "" rendraient l'objet lui-même)
    set sig [string range [string cat _ [lindex $args 1]] 1 end]
    set body [string range [string cat _ [lindex $args 2]] 1 end]
    set ret [note get [lindex $args 0]]
    set name [string range [string cat _ [lindex $args 0]] 1 end]
    if {![string match ::* $name]} {
	set name [expr {$ns eq "::" ? "::$name" : "${ns}::$name"}]
    }
    # le nom C : le dernier mot, s'il est un identificateur C
    set tail [namespace tail $name]
    if {![regexp {^[A-Za-z_][A-Za-z0-9_]*$} $tail]} {
	return -code error "cproc $tail: the name must be a C identifier\
		(letters, digits, _)"
    }
    # le type de retour : {*}T et {**}T déplient côté Tcl ; le C rend un T
    set rt [regsub {^\{\*\*?\}} $ret {}]
    # les types de la signature se lisent dans l'espace de la procédure
    set pns [namespace qualifiers $name]
    if {$pns eq ""} {set pns ::}

    set {list.param}params {*}{}
    foreach f [ParseSig $sig] {
	lappend params [list [string trimleft $f(name) -] \
		[Classify $pns $f(ttext)] $f]
    }
    # sans type de retour : le C rend un Tcl_Obj * (NULL : la chaîne vide)
    if {$rt eq ""} {
	set {kind}rk obj
    } else {
	set {kind}rk [Classify -ret $pns $rt]
    }

    set id [incr seq]
    set cmd ::cproc::c::f$id
    # la procédure Tcl : la signature, puis l'appel de la commande C
    set wbody $cmd
    foreach p $params {
	append wbody " \$$p(local)"
    }
    # créée depuis l'espace de l'appelant, comme proc : le nom s'y résout ;
    # les types de la signature, dans l'espace de la procédure
    uplevel #0 [list namespace eval $ns [list proc [lindex $args 0] $sig $wbody]]
    # en attendant la compilation : la commande C compile ce qui attend
    # (si la compilation n'a pas remplacé cette procédure par la commande C,
    # une erreur, et non un nouvel appel d'elle-même)
    proc $cmd {args} [format {
	::cproc::build
	if {[info procs %s] ne ""} {
	    return -code error "cproc: %s was not compiled"
	}
	tailcall %s {*}$args
    } $cmd $name $cmd]
    lassign [BodyLine $frame $body] file line
    lappend pending [list $id $name $tail $cmd $rk $params $body $file $line \
	    $pns]
    return
}

# Où est le corps (pour #line : les erreurs du compilateur y renvoient)
proc {list}::cproc::BodyLine {{dict}frame; {string}body} {
    if {![dict exists $frame file] || ![dict exists $frame line]} {
	return {{} 0}
    }
    set line [dict get $frame line]
    if {[dict exists $frame cmd]} {
	set c [dict get $frame cmd]
	set i [string first $body $c]
	if {$i >= 0} {
	    incr line [regexp -all {\n} [string range $c 0 $i-1]]
	}
    }
    list [dict get $frame file] $line
}

# Les champs d'une signature : {T}nom ?= défaut ; ...
proc {list.field}::cproc::ParseSig {{string}sig} {
    set {list.field}fields {*}{}
    set i 0
    set n [string length $sig]
    while {$i < $n} {
	# un champ va jusqu'au ";" de niveau 0
	set depth 0
	set start $i
	while {$i < $n} {
	    set ch [string index $sig $i]
	    if {$ch eq "\{"} {incr depth} elseif {$ch eq "\}"} {incr depth -1}
	    if {$ch eq ";" && $depth == 0} break
	    incr i
	}
	set f [string trim [string range $sig $start $i-1]]
	incr i
	if {$f eq ""} continue
	if {[string index $f 0] ne "\{"} {
	    return -code error "cproc: field \"$f\" has no type: write {T}name"
	}
	# le type : jusqu'à l'accolade fermante de même niveau
	set depth 0
	for {set j 0} {$j < [string length $f]} {incr j} {
	    set ch [string index $f $j]
	    if {$ch eq "\{"} {incr depth} elseif {$ch eq "\}"} {
		incr depth -1
		if {$depth == 0} break
	    }
	}
	set t [string range $f 1 $j-1]
	set rest [string range $f $j+1 end]
	if {![regexp {^(-?[A-Za-z_][A-Za-z0-9_]*)\s*(?:\??=\s*(.*))?$} $rest -> nm d]} {
	    return -code error "cproc: field \"$f\": no name after the type"
	}
	lappend fields [list $t $nm [DefVal $d]]
    }
    return $fields
}

# Un défaut : rien, une constante (un mot, entre accolades ou guillemets
# simples), ou un script [cmd] (ou ce qu'on ne lit pas sans Tcl)
proc {defval}::cproc::DefVal {{string}d} {
    set d [string trim $d]
    if {$d eq ""} {return none}
    if {[string index $d 0] eq "\["} {return {computed:}$d}
    if {[string index $d 0] eq "\{" && [string index $d end] eq "\}"} {
	return {const:}[string range $d 1 end-1]
    }
    if {[string index $d 0] eq "\"" && [string index $d end] eq "\""} {
	set d [string range $d 1 end-1]
    }
    if {[string first "\\" $d] >= 0 || [string first "\$" $d] >= 0
	    || [string first "\[" $d] >= 0} {
	return {computed:}$d
    }
    return {const:}$d
}

# ----------------------------------------------------------------------
# Les types : ce que voit le C
# ----------------------------------------------------------------------

proc {kind}::cproc::Classify {{flag}-ret; {string}ns; {string}t} {
    set t [string trim $t]
    # (des if, pas switch -regexp -matchvar : son corps, non compilé, ferait
    # expirer l'étiquette de return {num:}... avant le contrôle du retour)
    if {[regexp {^double(\(64\))?$} $t]} {return {num:}{double double}}
    if {$t eq "double(32)"} {return {num:}{float float}}
    if {$t eq "int"}        {return {num:}{wide Tcl_WideInt}}
    if {$t eq "flag"}       {return {num:}{int int}}
    if {$t eq "string"}     {return str}
    if {[regexp {^int\(([su]),(8|16|32|64)\)$} $t -> su bits]} {
	return {num:}[list fix [expr {$su eq "u" ? "uint" : "int"}]${bits}_t]
    }
    if {[regexp {^list(\(\d+\))?\.(.+)$} $t -> - et]} {
	if {$ret} {
	    # une liste rendue : construite par le C (cproc_list_T)
	    return obj
	}
	set {kind}e [Classify $ns $et]
	switch -- [Variant $e] {
	    struct {
		set {celem}el {struct:}$e(struct)
		set en $e(struct.name)
	    }
	    num {
		set {celem}el {num:}$e(num)
		set en [string map {{ } _} $e(num.ctype)]
	    }
	    default {
		return -code error "cproc: list.$et: the elements of a list\
			given to C must be numbers or products"
	    }
	}
	# (construit d'abord : return {span:}[list $el $en] perdrait la
	# variante de $el, une somme stricte dans la charge)
	set {cspan}sp $el $en
	return {span:}$sp
    }
    # un type défini : son texte (un alias ou un nominal de scalaire se
    # résout), ou une struct
    if {[catch {namespace eval $ns [list ::type def $t]} def]} {
	if {$ret} {return obj}
	return -code error "cproc: unknown type \"$t\" (in $ns)"
    }
    if {[string index [string trim $def] 0] ne "\{"} {
	if {$ret} {return [Classify -ret $ns $def]}
	return [Classify $ns $def]
    }
    if {[catch {namespace eval $ns "::type {c!}export [list $t]"} h]} {
	if {$ret} {return obj}
	return -code error "cproc: type \"$t\" has no C form: $h"
    }
    set {cblock}last [lindex [Blocks $h] end]
    if {$last(kind) ne "struct"} {
	if {$ret} {return obj}
	return -code error "cproc: type \"$t\" is not a product (a sum or a\
		list cannot be given to C yet)"
    }
    return {struct:}[list $last(name) $last(qual) $ns $t]
}

# Le texte de type {c!}export, découpé en blocs : un par type, avec son nom
# qualifié (le commentaire), son nom C, sa sorte et ses champs.
proc {list.cblock}::cproc::Blocks {{string}h} {
    set {list.cblock}out {*}{}
    set q ""
    set text ""
    foreach line [split $h \n] {
	if {[regexp {^/\* (\S+) \*/$} $line -> nq]} {
	    if {$q ne ""} {lappend out [Block $q $text]}
	    set q $nq
	    set text "$line\n"
	    continue
	}
	if {$q ne ""} {append text "$line\n"}
    }
    if {$q ne ""} {lappend out [Block $q $text]}
    return $out
}

proc {cblock}::cproc::Block {{string}qual; {string}text} {
    set kind other
    set name ""
    set {list.cfield}fields {*}{}
    if {[regexp {\} ([A-Za-z_][A-Za-z0-9_]*);\n} $text -> name]} {
	set kind [expr {[string match *union* $text] ? "union" : "struct"}]
	set in 0
	foreach line [split $text \n] {
	    if {[string match "typedef struct \{" $line]} {set in 1; continue}
	    if {[string match "\} *" $line]} {set in 0; continue}
	    if {!$in} continue
	    set l [regsub {/\*.*?\*/} $line {}]
	    if {[regexp {^\s+([A-Za-z_][A-Za-z0-9_]*)\s*(\*?)\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[(\d+)\])?\s*;\s*$} \
		    $l -> ct star fn dim]} {
		lappend fields [list "$ct$star" $fn $dim]
	    }
	}
    } elseif {[regexp {typedef .*?([A-Za-z_][A-Za-z0-9_]*)\s*(/\*.*\*/)?;} $text -> name]} {
	set kind scalar
    }
    list $qual $text $kind $name $fields
}

# ----------------------------------------------------------------------
# Génération
# ----------------------------------------------------------------------

# Les blocs dont ont besoin les procédures : un par nom qualifié, dans
# l'ordre de leurs dépendances (celui de l'export)
proc {list.cblock}::cproc::NeededBlocks {{list.cdef}procs} {
    set seen [dict create]
    set {list.cblock}out {*}{}
    foreach p $procs {
	foreach k [list $p(ret) {*}[lmap a $p(params) {set a(kind)}]] {
	    set {kind}k $k
	    switch -- [Variant $k] {
		struct {set {cstruct}s $k(struct)}
		span {
		    if {[Variant $k(span.elem)] ne "struct"} continue
		    set {cstruct}s $k(span.elem.struct)
		}
		default continue
	    }
	    set h [namespace eval $s(ns) "::type {c!}export [list $s(tname)]"]
	    foreach b [Blocks $h] {
		if {[dict exists $seen $b(qual)]} continue
		foreach o $out {
		    if {$o(name) eq $b(name)} {
			return -code error "cproc: types $o(qual) and $b(qual)\
				have the same C name \"$b(name)\""
		    }
		}
		dict set seen $b(qual) 1
		lappend out $b
	    }
	}
    }
    return $out
}

# Les spans, un par nom C d'élément
proc {list.cspan}::cproc::NeededSpans {{list.cdef}procs} {
    set {list.cspan}spans {*}{}
    set seen {}
    foreach p $procs {
	foreach a $p(params) {
	    set {kind}k $a(kind)
	    if {[Variant $k] eq "span" && $k(span.ename) ni $seen} {
		lappend seen $k(span.ename)
		lappend spans $k(span)
	    }
	}
    }
    return $spans
}

proc {string}::cproc::Gen {{list.cdef}procs} {
    variable codes
    set {list.cblock}blocks {*}[NeededBlocks $procs]
    set {list.cspan}spans {*}[NeededSpans $procs]
    # une fonction ne peut pas porter le nom d'une struct (proc
    # {boite}poly::boite) : elle devient cproc_boite
    set snames [lmap b $blocks {set b(name)}]
    set {list.cdef}procs {*}[lmap p $procs {
	if {$p(cname) in $snames} {set p(cname) cproc_$p(cname)}
	set p
    }]
    set c {}
    append c "/* Produit par cproc : ne pas modifier. */\n"
    append c "#include <tcl.h>\n#include <stdint.h>\n#include <stddef.h>\n"
    append c "#include <string.h>\n#include <stdio.h>\n#include <stdarg.h>\n"
    append c "#include \"tclAnnotatedApi.h\"\n\n"
    append c [Prelude]
    foreach b $blocks {
	append c $b(text)
    }
    foreach s $spans {
	append c "typedef struct { const [CType [ElemKind $s(elem)]] *at;\
		Tcl_Size n; } cproc_span_$s(ename);\n"
    }
    append c "\n"
    foreach b $blocks {
	if {$b(kind) eq "struct"} {append c [StructGetPut $b $blocks]}
    }
    foreach s $spans {
	append c [SpanGet $s(ename) $s(elem)]
    }
    append c "\n/* les fonctions */\n"
    foreach p $procs {
	append c "static [Proto $p];\n"
    }
    append c "\n"
    foreach t $codes {
	append c $t\n
    }
    foreach p $procs {
	append c "\nstatic [Proto $p]\n\{\n"
	if {$p(file) ne ""} {
	    append c "#line $p(line) \"[string map {\\ / \" \\\"} $p(file)]\"\n"
	}
	append c $p(body)
	append c "\n\}\n"
	if {$p(file) ne ""} {
	    # la suite renvoie au fichier C produit
	    append c "#line [expr {[regexp -all \n $c] + 2}] \"cproc.c\"\n"
	}
    }
    append c "\n/* la colle : Tcl -> C -> Tcl */\n"
    foreach p $procs {
	append c [Glue $p]
    }
    return $c
}

# un élément de span, vu comme un kind (pour son type C)
proc {kind}::cproc::ElemKind {{celem}e} {
    if {[Variant $e] eq "struct"} {return {struct:}$e(struct)}
    return {num:}$e(num)
}

proc {string}::cproc::Proto {{cdef}p} {
    set args [lmap a $p(params) {string cat [CType $a(kind)] " " $a(local)}]
    if {$args eq ""} {set args void}
    return "[CType $p(ret)] $p(cname)([join $args {, }])"
}

proc {string}::cproc::Prelude {} {
    return {
/* Les stubs de Tcl, sans bibliothèque à lier (gcc comme tcc) : le pointeur
 * de la table est lu dans l'interpréteur, comme le fait Tcl_InitStubs. */
const TclStubs *tclStubsPtr = NULL;
typedef struct { const char *r; void (*f)(void); int e;
	const TclStubs *stubTable; } CprocInterp;

static int
CprocInitStubs(Tcl_Interp *interp)
{
    const TclStubs *s = ((CprocInterp *) interp)->stubTable;

    if (s == NULL || s->magic != TCL_STUB_MAGIC) {
	return 0;
    }
    tclStubsPtr = s;
    return s->tcl_PkgRequireEx(interp, "tcl", "9.0", 0, NULL) != NULL;
}

/* cproc_error("format", ...) : le message de l'erreur Tcl */
static char cproc_errbuf[512];
static int cproc_failed = 0;

static void
cproc_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(cproc_errbuf, sizeof(cproc_errbuf), fmt, ap);
    va_end(ap);
    cproc_failed = 1;
}

static Tcl_Obj *
cproc_list_double(const double *a, Tcl_Size n)
{
    Tcl_Obj *l = Tcl_NewListObj(0, NULL);
    Tcl_Size i;

    for (i = 0; i < n; i++) {
	Tcl_ListObjAppendElement(NULL, l, Tcl_NewDoubleObj(a[i]));
    }
    return l;
}

}
}

# lecture (cp_get_T) et écriture (cp_put_T) d'une struct, et la liste
# cproc_list_T pour le code de l'utilisateur
proc {string}::cproc::StructGetPut {{cblock}b; {list.cblock}blocks} {
    set n $b(name)
    set nf [llength $b(fields)]
    set g "static int\ncp_get_$n\(Tcl_Interp *ip, Tcl_Obj *o, $n *r)\n\{\n"
    append g "    Tcl_Size n;\n    Tcl_Obj **e;\n\n"
    append g "    if (Tcl_ListObjGetElements(ip, o, &n, &e) != TCL_OK) return TCL_ERROR;\n"
    append g "    if (n != $nf) {\n\tTcl_SetObjResult(ip, Tcl_ObjPrintf(\"expected $nf fields for $n, got %\" TCL_SIZE_MODIFIER \"d\", n));\n\treturn TCL_ERROR;\n    }\n"
    set p "static Tcl_Obj *\ncp_put_$n\(const $n *v)\n\{\n    Tcl_Obj *e\[$nf\];\n\n"
    set i 0
    foreach f $b(fields) {
	set ct $f(ctype)
	set fn $f(name)
	set dim $f(dim)
	if {$dim ne ""} {
	    append g "    {\n\tTcl_Size m, k;\n\tTcl_Obj **d;\n"
	    append g "\tif (Tcl_ListObjGetElements(ip, e\[$i\], &m, &d) != TCL_OK) return TCL_ERROR;\n"
	    append g "\tif (m != $dim) { Tcl_SetObjResult(ip, Tcl_NewStringObj(\"$n.$fn: expected $dim values\", -1)); return TCL_ERROR; }\n"
	    append g "\tfor (k = 0; k < $dim; k++) {\n[GetScalar $ct d\[k\] r->$fn\[k\] $blocks \t\t]\t}\n    }\n"
	    append p "    {\n\tTcl_Obj *d\[$dim\];\n\tint k;\n\tfor (k = 0; k < $dim; k++) d\[k\] = [PutScalar $ct v->$fn\[k\]];\n"
	    append p "\te\[$i\] = Tcl_NewListObj($dim, d);\n    }\n"
	} else {
	    append g [GetScalar $ct e\[$i\] r->$fn $blocks "    "]
	    append p "    e\[$i\] = [PutScalar $ct v->$fn];\n"
	}
	incr i
    }
    append g "    return TCL_OK;\n\}\n\n"
    append p "    return Tcl_NewListObj($nf, e);\n\}\n\n"
    set l "static Tcl_Obj *\ncproc_list_$n\(const $n *a, Tcl_Size n)\n\{\n"
    append l "    Tcl_Obj *l = Tcl_NewListObj(0, NULL);\n    Tcl_Size i;\n\n"
    append l "    for (i = 0; i < n; i++) Tcl_ListObjAppendElement(NULL, l, cp_put_$n\(&a\[i\]));\n"
    append l "    return l;\n\}\n\n"
    return "$g$p$l"
}

proc {string}::cproc::GetScalar {{string}ct; {string}obj; {string}dst;
	{list.cblock}blocks; {string}ind} {
    switch -glob -- $ct {
	double {return "${ind}if (Tcl_GetDoubleFromObj(ip, $obj, &$dst) != TCL_OK) return TCL_ERROR;\n"}
	float {return "${ind}{ double t; if (Tcl_GetDoubleFromObj(ip, $obj, &t) != TCL_OK) return TCL_ERROR; $dst = (float) t; }\n"}
	*int*_t - Tcl_WideInt - int {
	    return "${ind}{ Tcl_WideInt t; if (Tcl_GetWideIntFromObj(ip, $obj, &t) != TCL_OK) return TCL_ERROR; $dst = ($ct) t; }\n"
	}
	Tcl_Obj* {return "${ind}$dst = $obj;\n"}
	default {
	    foreach b $blocks {
		if {$b(name) eq $ct && $b(kind) eq "struct"} {
		    return "${ind}if (cp_get_$ct\(ip, $obj, &$dst) != TCL_OK) return TCL_ERROR;\n"
		}
	    }
	    return -code error "cproc: C field type \"$ct\" is not supported"
	}
    }
}

proc {string}::cproc::PutScalar {{string}ct; {string}src} {
    switch -glob -- $ct {
	double - float {return "Tcl_NewDoubleObj($src)"}
	*int*_t - Tcl_WideInt - int {return "Tcl_NewWideIntObj((Tcl_WideInt) $src)"}
	Tcl_Obj* {return "($src != NULL ? $src : Tcl_NewObj())"}
	default {return "cp_put_$ct\(&$src)"}
    }
}

# Le nom de type Tcl d'un élément en cases : celui de la struct, ou du nombre
proc {string}::cproc::CellName {{celem}e} {
    if {[Variant $e] eq "struct"} {return $e(struct.qual)}
    switch -- $e(num.how) {
	double {return double}
	float  {return double(32)}
	fix {
	    regexp {^(u?)int(\d+)_t$} $e(num.ctype) -> u bits
	    return [expr {$u eq "u" ? "int(u,$bits)" : "int(s,$bits)"}]
	}
	default {return x}
    }
}

# un span : en place si la valeur est en cases du même type d'élément,
# sinon une copie (libérée après l'appel)
proc {string}::cproc::SpanGet {{string}en; {celem}e} {
    set ct [CType [ElemKind $e]]
    set tn [CellName $e]
    set s "static int\ncp_span_$en\(Tcl_Interp *ip, Tcl_Obj *o, cproc_span_$en *s, void **own)\n\{\n"
    append s "    const void *cells = NULL;\n    Tcl_Size n, i;\n    Tcl_Obj **e, *t;\n    $ct *a;\n\n"
    append s "    *own = NULL;\n"
    append s "    t = Tcl_NewStringObj(\"$tn\", -1);\n    Tcl_IncrRefCount(t);\n"
    append s "    (void) Tcl_AnnotatedGetCells(ip, o, t, sizeof($ct), &cells, &n);\n"
    append s "    Tcl_DecrRefCount(t);\n"
    append s "    if (cells != NULL) { s->at = (const $ct *) cells; s->n = n; return TCL_OK; }\n"
    append s "    if (Tcl_ListObjGetElements(ip, o, &n, &e) != TCL_OK) return TCL_ERROR;\n"
    append s "    a = ($ct *) Tcl_Alloc(sizeof($ct) * (n > 0 ? n : 1));\n"
    append s "    *own = a;\n"
    append s "    for (i = 0; i < n; i++) {\n"
    if {[Variant $e] eq "struct"} {
	append s "\tif (cp_get_$ct\(ip, e\[i\], &a\[i\]) != TCL_OK) return TCL_ERROR;\n"
    } else {
	append s [GetScalar $ct e\[i\] a\[i\] {} "\t"]
    }
    append s "    }\n    s->at = a;\n    s->n = n;\n    return TCL_OK;\n\}\n\n"
    return $s
}

proc {string}::cproc::Glue {{cdef}p} {
    set np [llength $p(params)]
    set g "static int\ncp_cmd_$p(id)\(void *cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv\[\])\n\{\n"
    set decl ""
    set get ""
    set free ""
    set args {}
    set i 0
    foreach a $p(params) {
	set {kind}k $a(kind)
	set v a$i
	set o "objv\[[expr {$i + 1}]\]"
	set ct [CType $k]
	switch -- [Variant $k] {
	    num {
		append decl "    $ct $v;\n"
		append get [GetScalar $ct $o $v {} "    "]
	    }
	    str {
		append decl "    const char *$v;\n"
		append get "    $v = Tcl_GetString($o);\n"
	    }
	    struct {
		append decl "    $ct $v;\n"
		append get "    if (cp_get_$ct\(ip, $o, &$v) != TCL_OK) return TCL_ERROR;\n"
	    }
	    span {
		append decl "    $ct $v;\n    void *own$i = NULL;\n"
		append get "    if (cp_span_$k(span.ename)\(ip, $o, &$v, &own$i) != TCL_OK) goto fail;\n"
		append free "    if (own$i != NULL) Tcl_Free(own$i);\n"
	    }
	    obj {
		return -code error "cproc: argument $a(local): this type cannot\
			be given to C"
	    }
	}
	lappend args $v
	incr i
    }
    append g "    (void) cd;\n"
    append g "    if (objc != [expr {$np + 1}]) {\n\tTcl_WrongNumArgs(ip, 1, objv, \"[lmap a $p(params) {set a(local)}]\");\n\treturn TCL_ERROR;\n    }\n"
    append g $decl
    append g "    [CType $p(ret)] r;\n"
    append g "    Tcl_Obj *res = NULL;\n\n"
    # les spans libèrent avant de sortir
    append g [string map {"return TCL_ERROR;" "goto fail;"} $get]
    append g "    cproc_failed = 0;\n"
    append g "    r = $p(cname)([join $args {, }]);\n"
    append g "    if (cproc_failed) {\n\tTcl_SetObjResult(ip, Tcl_NewStringObj(cproc_errbuf, -1));\n\tgoto fail;\n    }\n"
    append g "    res = [Result $p(ret) r];\n"
    append g "    Tcl_SetObjResult(ip, res);\n"
    append g $free
    append g "    return TCL_OK;\n  fail:\n"
    append g $free
    append g "    return TCL_ERROR;\n\}\n\n"
    return $g
}

# l'objet Tcl du résultat r
proc {string}::cproc::Result {{kind}k; {string}r} {
    switch -- [Variant $k] {
	num {
	    if {$k(num.how) in {double float}} {return "Tcl_NewDoubleObj($r)"}
	    return "Tcl_NewWideIntObj((Tcl_WideInt) $r)"
	}
	str    {return "Tcl_NewStringObj($r != NULL ? $r : \"\", -1)"}
	struct {return "cp_put_$k(struct.name)\(&$r)"}
	default {return "(($r) != NULL ? ($r) : Tcl_NewObj())"}
    }
}

# ----------------------------------------------------------------------
# Compilation, cache, chargement
# ----------------------------------------------------------------------

proc ::cproc::build {} {
    variable pending
    variable cfg
    variable lastSource
    if {[llength $pending] == 0} return
    set {list.cdef}procs {*}$pending
    set c [Gen $procs]
    set cc $cfg(cc)
    set istcc [string match *tcc* [file tail $cc]]
    set flags $cfg(cflags)
    if {$flags eq ""} {
	# -ffp-contract=off : pas de a*b+c fusionné, les mêmes résultats que
	# expr (tcc ne fusionne jamais)
	set flags [expr {$istcc ? {} : {-O2 -ffp-contract=off}}]
    }
    set key [format %08x%08x [zlib crc32 "$cc $flags\n$c"] \
	    [zlib adler32 "$cc $flags\n$c"]]
    set prefix Cproc$key
    append c [Init $prefix $procs]
    set lastSource $c
    set lib [file join [CacheDir] cproc_$key[info sharedlibextension]]
    if {![file exists $lib]} {
	Compile $c $cc $flags $istcc $lib
    }
    load $lib $prefix
    set pending [lrange $pending [llength $procs] end]
    return
}

# l'initialisation : les commandes ::cproc::c::f<id>
proc {string}::cproc::Init {{string}prefix; {list.cdef}procs} {
    set c "\nDLLEXPORT int\n${prefix}_Init(Tcl_Interp *ip)\n\{\n"
    append c "    if (!CprocInitStubs(ip)) return TCL_ERROR;\n"
    foreach p $procs {
	append c "    Tcl_CreateObjCommand(ip, \"$p(cmd)\", cp_cmd_$p(id), NULL, NULL);\n"
    }
    append c "    return TCL_OK;\n\}\n"
}

proc ::cproc::Compile {{string}c; {string}cc; {list.string}flags; {int}istcc;
	{string}lib} {
    variable cfg
    set src [file rootname $lib].c
    set f [open $src w]
    fconfigure $f -encoding utf-8
    puts -nonewline $f $c
    close $f
    set cmd [list $cc -shared {*}$flags]
    if {$::tcl_platform(platform) ne "windows" && !$istcc} {
	lappend cmd -fPIC
    }
    foreach i [Includes] {lappend cmd -I$i}
    lappend cmd -DUSE_TCL_STUBS -o $lib $src
    if {$cfg(verbose)} {puts stderr "cproc: $cmd"}
    if {[catch {exec {*}$cmd 2>@1} out]} {
	file delete -force $lib
	# les procédures restent en attente : l'appel suivant redonne
	# l'erreur du compilateur
	return -code error "cproc: the C compiler failed:\n$out"
    }
    if {$cfg(verbose) && $out ne ""} {puts stderr $out}
}

proc {string}::cproc::CacheDir {} {
    variable cfg
    set d $cfg(cachedir)
    if {$d eq ""} {
	if {[info exists ::env(CPROC_CACHE)]} {
	    set d $::env(CPROC_CACHE)
	} elseif {[info exists ::env(LOCALAPPDATA)]} {
	    set d [file join $::env(LOCALAPPDATA) cproc]
	} else {
	    set d [file join [file home] .cache cproc]
	}
    }
    file mkdir $d
    return $d
}

# tcl.h (et tclDecls.h...), tclAnnotatedApi.h et tclCompact.h
proc {list.string}::cproc::Includes {} {
    variable cfg
    variable here
    set out $cfg(includes)
    set cands {}
    catch {lappend cands [::tcl::pkgconfig get includedir,runtime]}
    set lib [info library]
    lappend cands [file join [file dirname [file dirname $lib]] include] \
	    [file join [file dirname $lib] generic]
    foreach d $cands {
	if {[file exists [file join $d tcl.h]]} {lappend out $d; break}
    }
    foreach d [list [file join $here .. ..] $here] {
	if {[file exists [file join $d tclAnnotatedApi.h]]} {
	    lappend out [file normalize $d]
	    break
	}
    }
    return $out
}

package provide cproc 0.2
