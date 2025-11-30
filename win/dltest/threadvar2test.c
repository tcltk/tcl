/*
 * threadvar2test.c --
 *
 *	This file contains a simple Tcl package "threadvar2test" that is intended for
 *	testing the Tcl dynamic loading facilities using MemoryModule.
 *
 * Copyright © 2024 Jan Nijtmans
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#include <windows.h>
#include <stdbool.h>
#include "tcl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* read-only thread-specify variables. Cannot be written to. */
extern int _tls_index;
extern int _tls_start;
extern int _tls_end;

#ifdef _MSC_VER
__declspec(thread)
#elif __GNUC__
__thread
#elif __cplusplus
thread_local
#endif
int threadVar;

BOOL APIENTRY
DllMain(
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    LPVOID unused)
{
    (void)hInst;
    (void)reason;
    (void)unused;

    switch (reason) {
    case DLL_PROCESS_ATTACH:
	break;
    case DLL_THREAD_ATTACH:
	break;
    default:
	break;
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif

/* Tvt_ThreadVar2 access a treaded (integer) variable.  */
/* Special values -end, -index and -start can access
 * the (read-only) special variables _tls_end, _tls_index
 * and _tls_start */
static int
Tvt_ThreadVar2(
    void *dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    (void)dummy;
    static const char *options[] = {
	    "-end", "-index", "-start", NULL
    };

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?int? | -end | -index | -start");
	return TCL_ERROR;
    }
    if (objc == 2) {
	int index;
	int value = -1;
	if (Tcl_GetIndexFromObj(NULL, objv[1], options, "options", 0, &index) == TCL_OK) {
	    switch (index) {
	    case 0: value = _tls_end; break;
	    case 1: value = _tls_index; break;
	    case 2: value = _tls_start; break;
	    }
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(value));
	    return TCL_OK;
	}
	if (Tcl_GetIntFromObj(interp, objv[1], &threadVar) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(threadVar));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Threadvartest_Init --
 *
 *	This is a package initialization procedure, which is called by Tcl
 *	when this package is to be added to an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Threadvar2test_Init(
    Tcl_Interp *interp)		/* Interpreter in which the package is to be
				 * made available. */
{
    int code;

    if (Tcl_InitStubs(interp, "8.7-", 0) == NULL) {
	/* Tcl 8.6 doesn't have Tcl_DStringToObj() */
	return TCL_ERROR;
    }
    code = Tcl_PkgProvide(interp, "threadvar2test", "1.0.0");
    if (code != TCL_OK) {
	return code;
    }
    Tcl_CreateObjCommand(interp, "ThreadVar2", Tvt_ThreadVar2, NULL, NULL);
    return TCL_OK;
}
