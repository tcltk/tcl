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

#ifndef ZIPFSAPI
#   define ZIPFSAPI extern
#endif

#ifdef ZIPFS_IN_TK
#define Zipfs_Mount    Tkzipfs_Mount
#define Zipfs_Unmount  Tkzipfs_Unmount
#define Zipfs_Init     Tkzipfs_Init
#define Zipfs_SafeInit Tkzipfs_SafeInit
#ifdef BUILD_tk
#   undef ZIPFSAPI
#   define ZIPFSAPI DLLEXPORT
#endif
#endif

#ifdef ZIPFS_IN_TCL
#define Zipfs_Mount    Tclzipfs_Mount
#define Zipfs_Unmount  Tclzipfs_Unmount
#define Zipfs_Init     Tclzipfs_Init
#define Zipfs_SafeInit Tclzipfs_SafeInit
#ifdef BUILD_tcl
#   undef ZIPFSAPI
#   define ZIPFSAPI DLLEXPORT
#endif
#endif

ZIPFSAPI int Zipfs_Mount(Tcl_Interp *interp, const char *zipname,
		       const char *mntpt, const char *passwd);
ZIPFSAPI int Zipfs_Unmount(Tcl_Interp *interp, const char *zipname);
ZIPFSAPI int Zipfs_Init(Tcl_Interp *interp);
ZIPFSAPI int Zipfs_SafeInit(Tcl_Interp *interp);

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
