# cproc1_tcl.tcl -- cproc, étape 1 : le générateur en Tcl ordinaire (la version
# de départ ; cproc.tcl est l'étape 3, cproc2_type.tcl l'étape 2)
#
#
# FR : cproc a la syntaxe de proc ; seul le corps change de langue :
#
#        proc  {point}decale {{point}p; {double}dx ?= 0} {( $p(x) + $dx, $p(y) )}
#        cproc {point}decale {{point}p; {double}dx ?= 0} {
#            return (point){p.x + dx, p.y};
#        }
#
#      La signature est celle d'une procédure typée (v16p, options v16q) :
#      elle est liée et vérifiée par Tcl, exactement comme pour proc (mêmes
#      défauts, étiquettes, options, messages). cproc génère le reste : la
#      fonction C pure (point decale(point p, double dx)), les structs (type
#      {c!}export), la commande Tcl qui convertit les arguments, appelle la
#      fonction et rend le résultat, et l'initialisation de la bibliothèque.
#      Le C est compilé par gcc ou tcc (cproc::config -cc tcc) en une
#      bibliothèque partagée, gardée en cache (une source = une bibliothèque),
#      au premier appel d'une des procédures définies depuis la dernière
#      compilation (ou par cproc::build).
#
#      Correspondance des types (arguments ; le retour de même) :
#        double, double(64)         double
#        double(32)                 float
#        int                        Tcl_WideInt
#        int(s,N), int(u,N)         intN_t, uintN_t
#        flag                       int
#        string                     const char *
#        un produit (point)         la struct de type {c!}export, par valeur
#        list.T, list(n).T (T : un  cproc_span_T : { const T *at; Tcl_Size n; }
#          nombre ou un produit)      en place si la liste est en cases
#                                     ({-compact}), sinon une copie
#        autre type (retour), ou     Tcl_Obj * (à construire, par exemple avec
#          pas de type de retour      cproc_list_point(tableau, n) ; NULL :
#                                     la chaîne vide)
#      Une erreur : cproc_error("format", ...) puis return ; le message
#      devient l'erreur Tcl.
#
# EN : cproc has proc's syntax; only the body's language changes. The typed
#      signature is bound and checked by Tcl; cproc generates the pure C
#      function, the structs, the glue command and the init function, and
#      compiles them with gcc or tcc into a cached shared library.
#
# Configuration :
#   cproc::config -cc gcc|tcc|chemin  -cflags {...}  -includes {dir ...}
#                 -cachedir DIR  -verbose 0|1
#   cproc::code {C}     du C commun (fonctions, #include) avant les corps
#   cproc::build        compile tout de suite les procédures en attente
#   cproc::source       le C de la dernière compilation (pour le lire)

namespace eval ::cproc {
    variable here [file dirname [file normalize [info script]]]
    variable cfg [dict create -cc gcc -cflags {} -includes {} -cachedir {} \
	    -verbose 0]
    variable pending {}		;# les procédures en attente de compilation
    variable codes {}		;# cproc::code
    variable seq 0
    variable lastSource {}
    namespace eval c {}		;# les commandes C : ::cproc::c::<id>
}

# ----------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------

proc ::cproc::config {args} {
    variable cfg
    if {[llength $args] == 0} {return $cfg}
    if {[llength $args] == 1} {return [dict get $cfg [lindex $args 0]]}
    foreach {k v} $args {
	if {![dict exists $cfg $k]} {
	    return -code error "cproc::config: unknown option \"$k\": must be\
		    [join [dict keys $cfg] {, }]"
	}
	dict set cfg $k $v
    }
    return $cfg
}

proc ::cproc::code {text} {
    variable codes
    lappend codes $text
    return
}

proc ::cproc::source {} {
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
    # le namespace des types de la signature : celui de la procédure
    set pns [namespace qualifiers $name]
    if {$pns eq ""} {set pns ::}

    set fields [ParseSig $sig]
    set params {}
    foreach f $fields {
	lassign $f ttext fname
	set local [string trimleft $fname -]
	set k [Classify $pns $ttext]
	lappend params [dict create local $local ctype [dict get $k ctype] \
		kind $k ttext $ttext]
    }
    # sans type de retour : le C rend un Tcl_Obj * (NULL : la chaîne vide)
    set rk [expr {$rt eq "" ? [dict create how obj ctype {Tcl_Obj *}]
	    : [Classify $pns $rt 1]}]

    set id [incr seq]
    set cmd ::cproc::c::f$id
    # la procédure Tcl : la signature, puis l'appel de la commande C
    set wbody $cmd
    foreach p $params {
	append wbody " \$[dict get $p local]"
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
    # où est le corps (pour #line : les erreurs du compilateur y renvoient)
    set file ""; set line 0
    if {[dict exists $frame file] && [dict exists $frame line]} {
	set file [dict get $frame file]
	set line [dict get $frame line]
	if {[dict exists $frame cmd]} {
	    set c [dict get $frame cmd]
	    set i [string first $body $c]
	    if {$i >= 0} {
		incr line [regexp -all {\n} [string range $c 0 $i-1]]
	    }
	}
    }
    lappend pending [dict create id $id name $name cname $tail cmd $cmd \
	    ret $rk params $params body $body file $file line $line ns $pns]
    return
}

# Les champs d'une signature : {T}nom ?= défaut ; ... -> {T nom}
proc ::cproc::ParseSig {sig} {
    set fields {}
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
	if {![regexp {^-?[A-Za-z_][A-Za-z0-9_]*} [string range $f $j+1 end] nm]} {
	    return -code error "cproc: field \"$f\": no name after the type"
	}
	lappend fields [list $t $nm]
    }
    return $fields
}

# ----------------------------------------------------------------------
# Les types : ce que voit le C
#
# Classify NS T -> dict : how (double float wide int fix str struct span obj)
#   ctype (le type C), et selon how : name (C de la struct), qual (nom
#   qualifié, pour lire les cases), elem (le dict de l'élément d'un span).
# ----------------------------------------------------------------------

proc ::cproc::Classify {ns t {isRet 0}} {
    set t [string trim $t]
    switch -regexp -matchvar m -- $t {
	{^double(\(64\))?$}  {return [dict create how double ctype double]}
	{^double\(32\)$}     {return [dict create how float ctype float]}
	{^int$}              {return [dict create how wide ctype Tcl_WideInt]}
	{^flag$}             {return [dict create how int ctype int]}
	{^string$}           {return [dict create how str ctype {const char *}]}
	{^int\(([su]),(8|16|32|64)\)$} {
	    set c [expr {[lindex $m 1] eq "u" ? "uint" : "int"}][lindex $m 2]_t
	    return [dict create how fix ctype $c]
	}
	{^list(\(\d+\))?\.(.+)$} {
	    if {$isRet} {
		# une liste rendue : construite par le C (cproc_list_T)
		return [dict create how obj ctype {Tcl_Obj *}]
	    }
	    set e [Classify $ns [lindex $m 2]]
	    if {[dict get $e how] ni {double float wide fix struct}} {
		if {$isRet} {return [dict create how obj ctype {Tcl_Obj *}]}
		return -code error "cproc: list.[lindex $m 2]: the elements of\
			a list given to C must be numbers or products"
	    }
	    set en [expr {[dict get $e how] eq "struct" ? [dict get $e name]
		    : [string map {{ } _} [dict get $e ctype]]}]
	    return [dict create how span ctype cproc_span_$en elem $e \
		    ename $en]
	}
    }
    # un type défini : son texte (un alias ou un nominal de scalaire se
    # résout), ou une struct
    if {[catch {namespace eval $ns [list ::type def $t]} def]} {
	if {$isRet} {return [dict create how obj ctype {Tcl_Obj *}]}
	return -code error "cproc: unknown type \"$t\" (in $ns)"
    }
    if {[string index [string trim $def] 0] ne "\{"} {
	return [Classify $ns $def $isRet]
    }
    if {[catch {namespace eval $ns "::type {c!}export [list $t]"} h]} {
	if {$isRet} {return [dict create how obj ctype {Tcl_Obj *}]}
	return -code error "cproc: type \"$t\" has no C form: $h"
    }
    set blocks [Blocks $h]
    set last [lindex $blocks end]
    if {[dict get $last kind] ne "struct"} {
	if {$isRet} {return [dict create how obj ctype {Tcl_Obj *}]}
	return -code error "cproc: type \"$t\" is not a product (a sum or a\
		list cannot be given to C yet)"
    }
    return [dict create how struct ctype [dict get $last name] \
	    name [dict get $last name] qual [dict get $last qual] \
	    ns $ns tname $t]
}

# Le texte de type {c!}export, découpé en blocs : un par type, avec son nom
# qualifié (le commentaire), son nom C, sa sorte et ses champs.
proc ::cproc::Blocks {h} {
    set out {}
    set cur {}
    foreach line [split $h \n] {
	if {[regexp {^/\* (\S+) \*/$} $line -> q]} {
	    if {$cur ne ""} {lappend out [Block $cur]}
	    set cur [dict create qual $q text "$line\n"]
	    continue
	}
	if {$cur ne ""} {dict append cur text "$line\n"}
    }
    if {$cur ne ""} {lappend out [Block $cur]}
    return $out
}

proc ::cproc::Block {b} {
    set text [dict get $b text]
    set kind other
    set name ""
    set fields {}
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
    dict set b kind $kind
    dict set b name $name
    dict set b fields $fields
    return $b
}

# ----------------------------------------------------------------------
# Génération
# ----------------------------------------------------------------------

proc ::cproc::Gen {procs} {
    variable codes
    # les structs : chaque type, exporté dans son namespace ; un bloc par
    # nom qualifié
    set blocks [dict create]
    set spans [dict create]
    set structs {}
    foreach p $procs {
	set ks {}
	foreach a [dict get $p params] {lappend ks [dict get $a kind]}
	if {[dict get $p ret] ne ""} {lappend ks [dict get $p ret]}
	foreach k $ks {
	    if {[dict get $k how] eq "span"} {
		dict set spans [dict get $k ename] [dict get $k elem]
		set k [dict get $k elem]
	    }
	    if {[dict get $k how] eq "struct"} {
		set h [namespace eval [dict get $k ns] \
			"::type {c!}export [list [dict get $k tname]]"]
		foreach b [Blocks $h] {
		    set q [dict get $b qual]
		    if {![dict exists $blocks $q]} {
			foreach o [dict values $blocks] {
			    if {[dict get $o name] eq [dict get $b name]} {
				return -code error "cproc: types [dict get $o qual]\
					and $q have the same C name\
					\"[dict get $b name]\""
			    }
			}
			dict set blocks $q $b
		    }
		}
	    }
	}
    }
    # une fonction ne peut pas porter le nom d'une struct (proc
    # {boite}poly::boite) : elle devient cproc_boite
    set snames [lmap b [dict values $blocks] {dict get $b name}]
    set procs [lmap p $procs {
	if {[dict get $p cname] in $snames} {
	    dict set p cname cproc_[dict get $p cname]
	}
	set p
    }]
    set c {}
    append c "/* Produit par cproc : ne pas modifier. */\n"
    append c "#include <tcl.h>\n#include <stdint.h>\n#include <stddef.h>\n"
    append c "#include <string.h>\n#include <stdio.h>\n#include <stdarg.h>\n"
    append c "#include \"tclAnnotatedApi.h\"\n\n"
    append c [Prelude]
    # les structs, dans l'ordre de leurs dépendances (celui de l'export)
    dict for {q b} $blocks {
	append c [dict get $b text]
    }
    # spans
    dict for {en e} $spans {
	append c "typedef struct { const [dict get $e ctype] *at;\
		Tcl_Size n; } cproc_span_$en;\n"
    }
    append c "\n"
    # lecture / écriture de chaque struct
    dict for {q b} $blocks {
	if {[dict get $b kind] ne "struct"} continue
	append c [StructGetPut $b $blocks]
    }
    dict for {en e} $spans {
	append c [SpanGet $en $e]
    }
    # les prototypes, le code commun, les fonctions
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
	if {[dict get $p file] ne ""} {
	    append c "#line [dict get $p line] \"[string map {\\ / \" \\\"} \
		    [dict get $p file]]\"\n"
	}
	append c [dict get $p body]
	append c "\n\}\n"
	if {[dict get $p file] ne ""} {
	    # la suite renvoie au fichier C produit
	    append c "#line [expr {[regexp -all \n $c] + 2}] \"cproc.c\"\n"
	}
    }
    # la colle
    append c "\n/* la colle : Tcl -> C -> Tcl */\n"
    foreach p $procs {
	append c [Glue $p]
    }
    return $c
}

proc ::cproc::Proto {p} {
    set rk [dict get $p ret]
    set r [expr {$rk eq "" ? "void" : [dict get $rk ctype]}]
    set args {}
    foreach a [dict get $p params] {
	lappend args "[dict get $a ctype] [dict get $a local]"
    }
    if {$args eq ""} {set args void}
    return "$r [dict get $p cname]([join $args {, }])"
}

proc ::cproc::Prelude {} {
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

static int
CprocWrong(Tcl_Interp *interp, const char *what, Tcl_Size i)
{
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("cproc: argument %"
	    TCL_SIZE_MODIFIER "d: %s", i, what));
    return TCL_ERROR;
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
proc ::cproc::StructGetPut {b blocks} {
    set n [dict get $b name]
    set fs [dict get $b fields]
    set nf [llength $fs]
    set g "static int\ncp_get_$n\(Tcl_Interp *ip, Tcl_Obj *o, $n *r)\n\{\n"
    append g "    Tcl_Size n;\n    Tcl_Obj **e;\n\n"
    append g "    if (Tcl_ListObjGetElements(ip, o, &n, &e) != TCL_OK) return TCL_ERROR;\n"
    append g "    if (n != $nf) {\n\tTcl_SetObjResult(ip, Tcl_ObjPrintf(\"expected $nf fields for $n, got %\" TCL_SIZE_MODIFIER \"d\", n));\n\treturn TCL_ERROR;\n    }\n"
    set p "static Tcl_Obj *\ncp_put_$n\(const $n *v)\n\{\n    Tcl_Obj *e\[$nf\];\n\n"
    set i 0
    foreach f $fs {
	lassign $f ct fn dim
	if {$dim ne ""} {
	    append g "    {\n\tTcl_Size m, k;\n\tTcl_Obj **d;\n"
	    append g "\tif (Tcl_ListObjGetElements(ip, e\[$i\], &m, &d) != TCL_OK) return TCL_ERROR;\n"
	    append g "\tif (m != $dim) { Tcl_SetObjResult(ip, Tcl_NewStringObj(\"$n.$fn: expected $dim values\", -1)); return TCL_ERROR; }\n"
	    append g "\tfor (k = 0; k < $dim; k++) {\n[GetScalar $ct d\[k\] r->$fn\[k\] $blocks \t\t]\t}\n    }\n"
	    append p "    {\n\tTcl_Obj *d\[$dim\];\n\tint k;\n\tfor (k = 0; k < $dim; k++) d\[k\] = [PutScalar $ct v->$fn\[k\] $blocks];\n"
	    append p "\te\[$i\] = Tcl_NewListObj($dim, d);\n    }\n"
	} else {
	    append g [GetScalar $ct e\[$i\] r->$fn $blocks "    "]
	    append p "    e\[$i\] = [PutScalar $ct v->$fn $blocks];\n"
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

proc ::cproc::GetScalar {ct obj dst blocks ind} {
    switch -glob -- $ct {
	double {return "${ind}if (Tcl_GetDoubleFromObj(ip, $obj, &$dst) != TCL_OK) return TCL_ERROR;\n"}
	float {return "${ind}{ double t; if (Tcl_GetDoubleFromObj(ip, $obj, &t) != TCL_OK) return TCL_ERROR; $dst = (float) t; }\n"}
	*int*_t - Tcl_WideInt - int {
	    return "${ind}{ Tcl_WideInt t; if (Tcl_GetWideIntFromObj(ip, $obj, &t) != TCL_OK) return TCL_ERROR; $dst = ($ct) t; }\n"
	}
	Tcl_Obj* {return "${ind}$dst = $obj;\n"}
	default {
	    foreach b [dict values $blocks] {
		if {[dict get $b name] eq $ct && [dict get $b kind] eq "struct"} {
		    return "${ind}if (cp_get_$ct\(ip, $obj, &$dst) != TCL_OK) return TCL_ERROR;\n"
		}
	    }
	    return -code error "cproc: C field type \"$ct\" is not supported"
	}
    }
}

proc ::cproc::PutScalar {ct src blocks} {
    switch -glob -- $ct {
	double - float {return "Tcl_NewDoubleObj($src)"}
	*int*_t - Tcl_WideInt - int {return "Tcl_NewWideIntObj((Tcl_WideInt) $src)"}
	Tcl_Obj* {return "($src != NULL ? $src : Tcl_NewObj())"}
	default {return "cp_put_$ct\(&$src)"}
    }
}

# un span : en place si la valeur est en cases du même type d'élément,
# sinon une copie (libérée après l'appel)
proc ::cproc::SpanGet {en e} {
    set ct [dict get $e ctype]
    set tn [expr {[dict get $e how] eq "struct" ? [dict get $e qual]
	    : [dict get {double double float double(32) wide int fix x} \
		[dict get $e how]]}]
    if {[dict get $e how] eq "fix"} {
	set tn [string map {uint int(u, int int(s, _t )} $ct]
	regsub {\(([su]),} $tn {(\1,} tn
    }
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
    if {[dict get $e how] eq "struct"} {
	append s "\tif (cp_get_$ct\(ip, e\[i\], &a\[i\]) != TCL_OK) return TCL_ERROR;\n"
    } else {
	append s [GetScalar $ct e\[i\] a\[i\] {} "\t"]
    }
    append s "    }\n    s->at = a;\n    s->n = n;\n    return TCL_OK;\n\}\n\n"
    return $s
}

proc ::cproc::Glue {p} {
    set id [dict get $p id]
    set params [dict get $p params]
    set np [llength $params]
    set g "static int\ncp_cmd_$id\(void *cd, Tcl_Interp *ip, int objc, Tcl_Obj *const objv\[\])\n\{\n"
    set decl ""
    set get ""
    set free ""
    set args {}
    set i 0
    foreach a $params {
	set k [dict get $a kind]
	set v a$i
	set o "objv\[[expr {$i + 1}]\]"
	set ct [dict get $k ctype]
	switch -- [dict get $k how] {
	    double {
		append decl "    double $v;\n"
		append get "    if (Tcl_GetDoubleFromObj(ip, $o, &$v) != TCL_OK) return TCL_ERROR;\n"
	    }
	    float - wide - int - fix {
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
		append get "    if (cp_span_[dict get $k ename]\(ip, $o, &$v, &own$i) != TCL_OK) goto fail;\n"
		append free "    if (own$i != NULL) Tcl_Free(own$i);\n"
	    }
	}
	lappend args $v
	incr i
    }
    append g "    (void) cd;\n"
    append g "    if (objc != [expr {$np + 1}]) {\n\tTcl_WrongNumArgs(ip, 1, objv, \"[lmap a $params {dict get $a local}]\");\n\treturn TCL_ERROR;\n    }\n"
    append g $decl
    set rk [dict get $p ret]
    if {$rk ne ""} {append g "    [dict get $rk ctype] r;\n"}
    append g "    Tcl_Obj *res = NULL;\n\n"
    # les spans libèrent avant de sortir : les autres lectures ne
    # réservent rien, on peut sortir directement avant le premier span
    set getf [string map {"return TCL_ERROR;" "goto fail;"} $get]
    append g $getf
    append g "    cproc_failed = 0;\n"
    set call "[dict get $p cname]([join $args {, }])"
    if {$rk eq ""} {
	append g "    $call;\n"
    } else {
	append g "    r = $call;\n"
    }
    append g "    if (cproc_failed) {\n\tTcl_SetObjResult(ip, Tcl_NewStringObj(cproc_errbuf, -1));\n\tgoto fail;\n    }\n"
    if {$rk ne ""} {
	switch -- [dict get $rk how] {
	    double - float {append g "    res = Tcl_NewDoubleObj(r);\n"}
	    wide - int - fix {append g "    res = Tcl_NewWideIntObj((Tcl_WideInt) r);\n"}
	    str {append g "    res = Tcl_NewStringObj(r != NULL ? r : \"\", -1);\n"}
	    struct {append g "    res = cp_put_[dict get $rk ctype]\(&r);\n"}
	    default {append g "    res = (r != NULL) ? r : Tcl_NewObj();\n"}
	}
	append g "    Tcl_SetObjResult(ip, res);\n"
    }
    append g $free
    append g "    return TCL_OK;\n  fail:\n"
    append g $free
    append g "    return TCL_ERROR;\n\}\n\n"
    return $g
}

# ----------------------------------------------------------------------
# Compilation, cache, chargement
# ----------------------------------------------------------------------

proc ::cproc::build {} {
    variable pending
    variable cfg
    variable here
    variable lastSource
    if {[llength $pending] == 0} return
    set procs $pending
    set c [Gen $procs]
    # l'initialisation : les commandes ::cproc::c::f<id>
    set cc [dict get $cfg -cc]
    set istcc [string match *tcc* [file tail $cc]]
    set flags [dict get $cfg -cflags]
    if {$flags eq ""} {
	# -ffp-contract=off : pas de a*b+c fusionné, les mêmes résultats que
	# expr (tcc ne fusionne jamais)
	set flags [expr {$istcc ? {} : {-O2 -ffp-contract=off}}]
    }
    set key [format %08x%08x [zlib crc32 "$cc $flags\n$c"] \
	    [zlib adler32 "$cc $flags\n$c"]]
    set prefix Cproc$key
    append c "\nDLLEXPORT int\n${prefix}_Init(Tcl_Interp *ip)\n\{\n"
    append c "    if (!CprocInitStubs(ip)) return TCL_ERROR;\n"
    foreach p $procs {
	append c "    Tcl_CreateObjCommand(ip, \"[dict get $p cmd]\",\
		cp_cmd_[dict get $p id], NULL, NULL);\n"
    }
    append c "    return TCL_OK;\n\}\n"
    set lastSource $c
    set dir [CacheDir]
    set lib [file join $dir cproc_$key[info sharedlibextension]]
    if {![file exists $lib]} {
	set src [file join $dir cproc_$key.c]
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
	if {[dict get $cfg -verbose]} {puts stderr "cproc: $cmd"}
	if {[catch {exec {*}$cmd 2>@1} out]} {
	    file delete -force $lib
	    # les procédures restent en attente : l'appel suivant redonne
	    # l'erreur du compilateur
	    return -code error "cproc: the C compiler failed:\n$out"
	}
	if {[dict get $cfg -verbose] && $out ne ""} {puts stderr $out}
    }
    load $lib $prefix
    set pending [lrange $pending [llength $procs] end]
    return
}

proc ::cproc::CacheDir {} {
    variable cfg
    set d [dict get $cfg -cachedir]
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
proc ::cproc::Includes {} {
    variable cfg
    variable here
    set out [dict get $cfg -includes]
    set cands {}
    catch {lappend cands [::tcl::pkgconfig get includedir,runtime]}
    set lib [info library]
    lappend cands [file join [file dirname [file dirname $lib]] include] \
	    [file join [file dirname $lib] generic]
    foreach d $cands {
	if {[file exists [file join $d tcl.h]]} {lappend out $d; break}
    }
    # l'API des valeurs annotées : à la racine de typeparse
    foreach d [list [file join $here .. ..] $here] {
	if {[file exists [file join $d tclAnnotatedApi.h]]} {
	    lappend out [file normalize $d]
	    break
	}
    }
    return $out
}

package provide cproc 0.1
