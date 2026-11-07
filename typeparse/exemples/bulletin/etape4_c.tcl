# Étape 4 : le passage au C. Les signatures ne changent pas, bulletin non
# plus : seuls les corps de lire et moyenne passent en C (cproc).
#   tclsh etape4_c.tcl ?COMPILATEUR?     (gcc par défaut ; tcc)
source [file join [file dirname [file normalize [info script]]] .. cproc cproc.tcl]
if {$argc > 0} {cproc::config -cc [lindex $argv 0]}

type def eleve {{string}nom; {double}note}

cproc::code {#include <ctype.h>}

# Le nom : tout avant le dernier blanc ; la note : le dernier mot. En C, un
# élève est une struct {Tcl_Obj *nom; double note;} (type {c!}export eleve).
cproc {eleve}lire {{string}ligne} {
    const char *deb = ligne, *fin = ligne + strlen(ligne), *k, *fn;
    eleve e = {NULL, 0};
    while (*deb && isspace((unsigned char) *deb)) deb++;
    while (fin > deb && isspace((unsigned char) fin[-1])) fin--;
    for (k = fin; k > deb && !isspace((unsigned char) k[-1]); k--) {}
    if (k == deb) {
        cproc_error("ligne sans note : %s", ligne);
        return e;
    }
    for (fn = k; fn > deb && isspace((unsigned char) fn[-1]); fn--) {}
    Tcl_Obj *n = Tcl_NewStringObj(k, fin - k);
    Tcl_IncrRefCount(n);
    if (Tcl_GetDoubleFromObj(NULL, n, &e.note) != TCL_OK) {
        cproc_error("note invalide : %s", Tcl_GetString(n));
    } else {
        e.nom = Tcl_NewStringObj(deb, fn - deb);
    }
    Tcl_DecrRefCount(n);
    return e;
}

cproc {double}moyenne {{list.double}notes} {
    double t = 0;
    for (Tcl_Size i = 0; i < notes.n; i++) t += notes.at[i];
    return t / notes.n;
}

proc bulletin {fichier} {
    set f [open $fichier]; set texte [read $f]; close $f
    set {list.eleve}classe          ;# une liste d'élèves, vide
    foreach l [split $texte \n] {
        if {[string trim $l] eq ""} continue
        lappend classe [lire $l]
    }
    foreach e $classe { puts [format "%-24s %5.1f" $e(nom) $e(note)] }
    puts [format "%-24s %5.2f" moyenne [moyenne [lmap e $classe {set e(note)}]]]
}
bulletin [file join [file dirname [info script]] classe.txt]
