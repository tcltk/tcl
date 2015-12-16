/*
 * zipfs.h --
 *
 *	This header file describes the interface of the ZIPFS filesystem
 *	used in AndroWish.
 *
 * Copyright (c) 2013-2015 Christian Werner <chw@ch-werner.de>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _ZIPFS_H
#define _ZIPFS_H

#include "tcl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ZIPFS_IN_TK
#define Zipfs_Mount    Tkzipfs_Mount
#define Zipfs_Unmount  Tkzipfs_Unmount
#define Zipfs_Init     Tkzipfs_Init
#define Zipfs_SafeInit Tkzipfs_SafeInit
#endif

#ifdef ZIPFS_IN_TCL
#define Zipfs_Mount    Tclzipfs_Mount
#define Zipfs_Unmount  Tclzipfs_Unmount
#define Zipfs_Init     Tclzipfs_Init
#define Zipfs_SafeInit Tclzipfs_SafeInit
#endif

DLLEXPORT int Zipfs_Mount(Tcl_Interp *interp, CONST char *zipname,
		       CONST char *mntpt, CONST char *passwd);
DLLEXPORT int Zipfs_Unmount(Tcl_Interp *interp, CONST char *zipname);
DLLEXPORT int Zipfs_Init(Tcl_Interp *interp);
DLLEXPORT int Zipfs_SafeInit(Tcl_Interp *interp);

#ifdef __cplusplus
}
#endif

#endif /* _ZIPFS_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
