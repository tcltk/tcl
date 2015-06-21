#ifndef _ZIPFS_H
#define _ZIPFS_H

#ifdef ZIPFS_IN_TK
#include "tkInt.h"
#define Zipfs_Mount    Tkzipfs_Mount
#define Zipfs_Unmount  Tkzipfs_Unmount
#define Zipfs_Init     Tkzipfs_Init
#define Zipfs_SafeInit Tkzipfs_SafeInit
#endif

#ifdef ZIPFS_IN_TCL
#include "tclPort.h"
#define Zipfs_Mount    Tclzipfs_Mount
#define Zipfs_Unmount  Tclzipfs_Unmount
#define Zipfs_Init     Tclzipfs_Init
#define Zipfs_SafeInit Tclzipfs_SafeInit
#endif

#ifndef EXTERN
#define EXTERN extern
#endif

#ifdef BUILD_tcl
#undef  EXTERN
#define EXTERN
#endif

EXTERN int Zipfs_Mount(Tcl_Interp *interp, CONST char *zipname,
		       CONST char *mntpt, CONST char *passwd);
EXTERN int Zipfs_Unmount(Tcl_Interp *interp, CONST char *mountname);
EXTERN int Zipfs_Init(Tcl_Interp *interp);
EXTERN int Zipfs_SafeInit(Tcl_Interp *interp);

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
