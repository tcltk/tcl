/*
 * tclZipfs.h --
 *
 *	This header file describes the interface of the ZIPFS filesystem
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

#ifdef BUILD_tcl
#   undef ZIPFSAPI
#   define ZIPFSAPI DLLEXPORT
#endif

ZIPFSAPI int Tclzipfs_Mount(Tcl_Interp *interp, const char *zipname,
		       const char *mntpt, const char *passwd);
ZIPFSAPI int Tclzipfs_Unmount(Tcl_Interp *interp, const char *zipname);
ZIPFSAPI int Tclzipfs_Init(Tcl_Interp *interp);
ZIPFSAPI int Tclzipfs_SafeInit(Tcl_Interp *interp);

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
