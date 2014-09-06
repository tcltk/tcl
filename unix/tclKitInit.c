/*
** This file implements the main routine for a standalone Tcl/Tk shell.
*/
#include <tcl.h>
#include "tclInt.h"
#define TCLKIT_INIT     "main.tcl"
#define TCLKIT_VFSMOUNT "/zvfs"

#define TCL_LOCAL_APPINIT Tclkit_AppInit
MODULE_SCOPE int TCL_LOCAL_APPINIT(Tcl_Interp *);
MODULE_SCOPE int main(int, char **);

/*
** This routine runs first.  
*/
int main(int argc, char **argv){
  Tcl_FindExecutable(argv[0]);
  Tcl_SetStartupScript(Tcl_NewStringObj("noop",-1),NULL);
  Tcl_Main(argc,argv,&Tclkit_AppInit);
  return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tclkit_AppInit --
 *
 *	This procedure performs application-specific initialization. Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tclkit_AppInit(
    Tcl_Interp *interp)		/* Interpreter for application. */
{
    Tcl_Zvfs_Boot(interp,TCLKIT_VFSMOUNT,TCLKIT_INIT);

    if ((Tcl_Init)(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Call the init procedures for included packages. Each call should look
     * like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module. (Dynamically-loadable packages
     * should have the same entry-point name.)
     */

    /*
     * Call Tcl_CreateCommand for application-specific commands, if they
     * weren't already created by the init procedures called above.
     */

    /*
     * Specify a user-specific startup file to invoke if the application is
     * run interactively. Typically the startup file is "~/.apprc" where "app"
     * is the name of the application. If this line is deleted then no
     * user-specific startup file will be run under any conditions.
     */

#ifdef DJGPP
    (Tcl_ObjSetVar2)(interp, Tcl_NewStringObj("tcl_rcFileName", -1), NULL,
	    Tcl_NewStringObj("~/tclsh.rc", -1), TCL_GLOBAL_ONLY);
#else
    (Tcl_ObjSetVar2)(interp, Tcl_NewStringObj("tcl_rcFileName", -1), NULL,
	    Tcl_NewStringObj("~/.tclshrc", -1), TCL_GLOBAL_ONLY);
#endif

    return TCL_OK;
}