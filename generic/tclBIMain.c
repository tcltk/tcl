/*
 * tclMainBI.c --
 *
 *	Provides a batteries included version of tclsh.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tcl.h"
#include <windows.h>

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never returns
 *	either.
 *
 * Side effects:
 *	Just about anything, since from here we call arbitrary Tcl code.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int Tcl_AppInit(Tcl_Interp *); /* TODO - move to tclInt.h */

extern int Sqlite3_Init(Tcl_Interp *interp);

static int
TclBIPostInit(
    Tcl_Interp *interp,
    void *clientData)
{
    if (Sqlite3_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#ifdef _WIN32
int
wmain(
    int argc,
    WCHAR *argv[])
#else
int
main(
    int argc,
    char *argv[])
#endif
{
#ifdef _WIN32
    WCHAR *p;
#else
    char *p;
#endif
    for (p = argv[0]; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }

    TclZipfs_AppHook(&argc, &argv);
    Tcl_SetPanicProc(Tcl_ConsolePanic);
    Tcl_RegisterPostInitProc(TclBIPostInit, NULL);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_MainEx(argc, argv, Tcl_AppInit, interp); /* Does not return */
    return 0;					 /* Avoid compiler warning */
}

