/* 
 * tclFileSystem.h --
 *
 *	This file contains the common defintions and prototypes for
 *	use by Tcl's filesystem and path handling layers.
 *
 * Copyright (c) 2003 Vince Darley.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclFileSystem.h,v 1.3 2003/08/23 12:16:49 vasiljevic Exp $
 */

/* 
 * struct FilesystemRecord --
 * 
 * A filesystem record is used to keep track of each
 * filesystem currently registered with the core,
 * in a linked list.  Pointers to these structures
 * are also kept by each "path" Tcl_Obj, and we must
 * retain a refCount on the number of such references.
 */
typedef struct FilesystemRecord {
    ClientData	     clientData;  /* Client specific data for the new
				   * filesystem (can be NULL) */
    Tcl_Filesystem *fsPtr;        /* Pointer to filesystem dispatch
				   * table. */
    int fileRefCount;             /* How many Tcl_Obj's use this
				   * filesystem. */
    struct FilesystemRecord *nextPtr;  
				  /* The next filesystem registered
				   * to Tcl, or NULL if no more. */
    struct FilesystemRecord *prevPtr;
				  /* The previous filesystem registered
				   * to Tcl, or NULL if no more. */
} FilesystemRecord;

/*
 * This structure holds per-thread private copy of the
 * current directory maintained by the global cwdPathPtr.
 * This structure holds per-thread private copies of
 * some global data. This way we avoid most of the
 * synchronization calls which boosts performance, at
 * cost of having to update this information each
 * time the corresponding epoch counter changes.
 */
typedef struct ThreadSpecificData {
    int initialized;
    int cwdPathEpoch;
    int filesystemEpoch;
    Tcl_Obj *cwdPathPtr;
    FilesystemRecord *filesystemList;
} ThreadSpecificData;

/* 
 * The internal TclFS API provides routines for handling and
 * manipulating paths efficiently, taking direct advantage of
 * the "path" Tcl_Obj type.
 * 
 * These functions are not exported at all at present.
 */

int      TclFSCwdPointerEquals _ANSI_ARGS_((Tcl_Obj* objPtr));
int	 TclFSMakePathFromNormalized _ANSI_ARGS_((Tcl_Interp *interp, 
		Tcl_Obj *objPtr, ClientData clientData));
int      TclFSNormalizeToUniquePath _ANSI_ARGS_((Tcl_Interp *interp, 
		Tcl_Obj *pathPtr, int startAt, ClientData *clientDataPtr));
Tcl_Obj* TclFSMakePathRelative _ANSI_ARGS_((Tcl_Interp *interp, 
		Tcl_Obj *objPtr, Tcl_Obj *cwdPtr));
Tcl_Obj* TclFSInternalToNormalized _ANSI_ARGS_((
		Tcl_Filesystem *fromFilesystem, ClientData clientData,
		FilesystemRecord **fsRecPtrPtr));
int      TclFSEnsureEpochOk _ANSI_ARGS_((Tcl_Obj* pathObjPtr,
		Tcl_Filesystem **fsPtrPtr));
void     TclFSSetPathDetails _ANSI_ARGS_((Tcl_Obj *pathObjPtr, 
		FilesystemRecord *fsRecPtr, ClientData clientData ));
Tcl_Obj* TclFSNormalizeAbsolutePath _ANSI_ARGS_((Tcl_Interp* interp, 
		Tcl_Obj *pathPtr, ClientData *clientDataPtr));

/* 
 * Private shared variables for use by tclIOUtil.c and tclPathObj.c
 */
extern Tcl_Filesystem tclNativeFilesystem;
extern int theFilesystemEpoch;
extern Tcl_ThreadDataKey fsDataKey;

/* 
 * Private shared functions for use by tclIOUtil.c and tclPathObj.c
 */
Tcl_PathType     FSGetPathType  _ANSI_ARGS_((Tcl_Obj *pathObjPtr, 
			    Tcl_Filesystem **filesystemPtrPtr, 
			    int *driveNameLengthPtr));
Tcl_PathType     GetPathType  _ANSI_ARGS_((Tcl_Obj *pathObjPtr, 
			    Tcl_Filesystem **filesystemPtrPtr, 
			    int *driveNameLengthPtr, Tcl_Obj **driveNameRef));
Tcl_FSPathInFilesystemProc NativePathInFilesystem;
