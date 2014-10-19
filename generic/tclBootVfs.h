#include <tcl.h>
#include "tclInt.h"
#include "tclFileSystem.h"

#ifndef MODULE_SCOPE
#   define MODULE_SCOPE extern
#endif

#define TCLVFSBOOT_INIT     "main.tcl"
#define TCLVFSBOOT_MOUNT    "/zvfs"

/* Make sure the stubbed variants of those are never used. */
#undef Tcl_ObjSetVar2
#undef Tcl_NewStringObj
#undef Tk_Init
#undef Tk_MainEx
#undef Tk_SafeInit

MODULE_SCOPE int Tcl_Zvfs_Boot(const char *,const char *,const char *);
MODULE_SCOPE int Zvfs_Init(Tcl_Interp *);
MODULE_SCOPE int Zvfs_SafeInit(Tcl_Interp *);
MODULE_SCOPE int Tclkit_Packages_Init(Tcl_Interp *);


