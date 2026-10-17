/*
 * realtcl.c --
 *
 *	Remplace mocktcl.c : mêmes fonctions d'aide pour les tests (mock_*),
 *	mais au-dessus de la VRAIE bibliothèque Tcl 9.0 (libtcl9.0.a). Les
 *	variables, elles, restent celles de mockvars.c (résolveur TclVarRef*
 *	de référence) ; seul le moteur Tcl (listes, dictionnaires, analyseur,
 *	évaluateur, comptage de références) est le vrai.
 */
#include "tclInt.h"
#include "tclTypes.h"
#include <stdlib.h>
#include <string.h>

long mock_live_objs;
static Tcl_Interp *lastInterp;

Tcl_Interp *mock_interp(void)
{
    static int inited;

    if (!inited) {
	Tcl_FindExecutable("realtest");
	inited = 1;
    }
    lastInterp = Tcl_CreateInterp();
    return lastInterp;
}

const char *mock_result(Tcl_Interp *in)
{
    return Tcl_GetString(Tcl_GetObjResult(in));
}

const char *mock_errorcode(Tcl_Interp *in)
{
    Tcl_Obj *opts = Tcl_GetReturnOptions(in, TCL_ERROR), *code = NULL;
    Tcl_Obj *key = Tcl_NewStringObj("-errorcode", -1);

    Tcl_DictObjGet(NULL, opts, key, &code);
    return code ? Tcl_GetString(code) : "";
}

void mock_reset(Tcl_Interp *in)
{
    Tcl_ResetResult(in);
}

int mock_type_command(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[])
{
    return Tcl_EvalObjv(in, objc, objv, 0);
}

/* Commandes de script des tests : MockCmdProc -> Tcl_ObjCmdProc2. */
typedef int (MockCmdProc)(Tcl_Interp *interp, Tcl_Size argc, const char *argv[],
	    Tcl_Obj **resultPtr);

static int Trampoline(void *cd, Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[])
{
    const char *argv[16];
    Tcl_Obj *res = NULL;
    Tcl_Size i;
    int rc;

    if (objc > 16) {
	return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
	argv[i] = Tcl_GetString(objv[i]);
    }
    rc = ((MockCmdProc *) cd)(in, objc, argv, &res);
    Tcl_SetObjResult(in, res ? res : Tcl_NewStringObj("", -1));
    return rc;
}

void mock_register_command(const char *name, MockCmdProc *proc)
{
    /* les tests créent un seul interpréteur à la fois */
    extern Tcl_Interp *mock_current_interp(void);

    Tcl_CreateObjCommand2(mock_current_interp(), name, Trampoline,
	    (void *) proc, NULL);
}

/* L'interpréteur courant : le dernier créé. */
Tcl_Interp *mock_current_interp(void)
{
    return lastInterp;
}
