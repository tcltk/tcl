/* 
 * tclPathObj.c --
 *
 *	This file contains the implementation of Tcl's "path" object
 *	type used to represent and manipulate a general (virtual)
 *	filesystem entity in an efficient manner.
 *
 * Copyright (c) 2003 Vince Darley.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclPathObj.c,v 1.3.2.1 2003/08/07 21:36:00 dgp Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
#ifdef MAC_TCL
#include "tclMacInt.h"
#endif
#include "tclFileSystem.h"

/*
 * Prototypes for procedures defined later in this file.
 */

static void		DupFsPathInternalRep _ANSI_ARGS_((Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr));
static void		FreeFsPathInternalRep _ANSI_ARGS_((Tcl_Obj *listPtr));
static void             UpdateStringOfFsPath _ANSI_ARGS_((Tcl_Obj *objPtr));
static int		SetFsPathFromAny _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Obj *objPtr));
static int 		FindSplitPos _ANSI_ARGS_((char *path, char *separator));



/*
 * Define the 'path' object type, which Tcl uses to represent
 * file paths internally.
 */
Tcl_ObjType tclFsPathType = {
    "path",				/* name */
    FreeFsPathInternalRep,		/* freeIntRepProc */
    DupFsPathInternalRep,	        /* dupIntRepProc */
    UpdateStringOfFsPath,		/* updateStringProc */
    SetFsPathFromAny			/* setFromAnyProc */
};

/* 
 * struct FsPath --
 * 
 * Internal representation of a Tcl_Obj of "path" type.  This
 * can be used to represent relative or absolute paths, and has
 * certain optimisations when used to represent paths which are
 * already normalized and absolute.
 * 
 * Note that 'normPathPtr' can be a circular reference to the
 * container Tcl_Obj of this FsPath.
 */
typedef struct FsPath {
    Tcl_Obj *translatedPathPtr; /* Name without any ~user sequences.
				 * If this is NULL, then this is a 
				 * pure normalized, absolute path
				 * object, in which the parent Tcl_Obj's
				 * string rep is already both translated
				 * and normalized. */
    Tcl_Obj *normPathPtr;       /* Normalized absolute path, without 
				 * ., .. or ~user sequences. If the 
				 * Tcl_Obj containing 
				 * this FsPath is already normalized, 
				 * this may be a circular reference back
				 * to the container.  If that is NOT the
				 * case, we have a refCount on the object. */
    Tcl_Obj *cwdPtr;            /* If null, path is absolute, else
				 * this points to the cwd object used
				 * for this path.  We have a refCount
				 * on the object. */
    int flags;                  /* Flags to describe interpretation */
    ClientData nativePathPtr;   /* Native representation of this path,
				 * which is filesystem dependent. */
    int filesystemEpoch;        /* Used to ensure the path representation
				 * was generated during the correct
				 * filesystem epoch.  The epoch changes
				 * when filesystem-mounts are changed. */ 
    struct FilesystemRecord *fsRecPtr;
				/* Pointer to the filesystem record 
				 * entry to use for this path. */
} FsPath;

/* 
 * Define some macros to give us convenient access to path-object
 * specific fields.
 */
#define PATHOBJ(objPtr) (objPtr->internalRep.otherValuePtr)
#define PATHFLAGS(objPtr) \
 (((FsPath*)(objPtr->internalRep.otherValuePtr))->flags)

#define TCLPATH_APPENDED 1
#define TCLPATH_RELATIVE 2

/*
 *---------------------------------------------------------------------------
 *
 * TclFSNormalizeAbsolutePath --
 *
 * Description:
 *	Takes an absolute path specification and computes a 'normalized'
 *	path from it.
 *	
 *	A normalized path is one which has all '../', './' removed.
 *	Also it is one which is in the 'standard' format for the native
 *	platform.  On MacOS, Unix, this means the path must be free of
 *	symbolic links/aliases, and on Windows it means we want the
 *	long form, with that long form's case-dependence (which gives
 *	us a unique, case-dependent path).
 *	
 *	The behaviour of this function if passed a non-absolute path
 *	is NOT defined.
 *
 * Results:
 *	The result is returned in a Tcl_Obj with a refCount of 1,
 *	which is therefore owned by the caller.  It must be
 *	freed (with Tcl_DecrRefCount) by the caller when no longer needed.
 *
 * Side effects:
 *	None (beyond the memory allocation for the result).
 *
 * Special note:
 *	This code is based on code from Matt Newman and Jean-Claude
 *	Wippler, with additions from Vince Darley and is copyright 
 *	those respective authors.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj*
TclFSNormalizeAbsolutePath(interp, pathPtr, clientDataPtr)
    Tcl_Interp* interp;    /* Interpreter to use */
    Tcl_Obj *pathPtr;      /* Absolute path to normalize */
    ClientData *clientDataPtr;
{
    int splen = 0, nplen, eltLen, i;
    char *eltName;
    Tcl_Obj *retVal;
    Tcl_Obj *split;
    Tcl_Obj *elt;
    
    /* Split has refCount zero */
    split = Tcl_FSSplitPath(pathPtr, &splen);

    /* 
     * Modify the list of entries in place, by removing '.', and
     * removing '..' and the entry before -- unless that entry before
     * is the top-level entry, i.e. the name of a volume.
     */
    nplen = 0;
    for (i = 0; i < splen; i++) {
	Tcl_ListObjIndex(NULL, split, nplen, &elt);
	eltName = Tcl_GetStringFromObj(elt, &eltLen);

	if ((eltLen == 1) && (eltName[0] == '.')) {
	    Tcl_ListObjReplace(NULL, split, nplen, 1, 0, NULL);
	} else if ((eltLen == 2)
		&& (eltName[0] == '.') && (eltName[1] == '.')) {
	    if (nplen > 1) {
		nplen--;
		Tcl_ListObjReplace(NULL, split, nplen, 2, 0, NULL);
	    } else {
		Tcl_ListObjReplace(NULL, split, nplen, 1, 0, NULL);
	    }
	} else {
	    nplen++;
	}
    }
    if (nplen > 0) {
	ClientData clientData = NULL;
	
	retVal = Tcl_FSJoinPath(split, nplen);
	/* 
	 * Now we have an absolute path, with no '..', '.' sequences,
	 * but it still may not be in 'unique' form, depending on the
	 * platform.  For instance, Unix is case-sensitive, so the
	 * path is ok.  Windows is case-insensitive, and also has the
	 * weird 'longname/shortname' thing (e.g. C:/Program Files/ and
	 * C:/Progra~1/ are equivalent).  MacOS is case-insensitive.
	 * 
	 * Virtual file systems which may be registered may have
	 * other criteria for normalizing a path.
	 */
	Tcl_IncrRefCount(retVal);
	TclFSNormalizeToUniquePath(interp, retVal, 0, &clientData);
	/* 
	 * Since we know it is a normalized path, we can
	 * actually convert this object into an FsPath for
	 * greater efficiency 
	 */
	TclFSMakePathFromNormalized(interp, retVal, clientData);
	if (clientDataPtr != NULL) {
	    *clientDataPtr = clientData;
	}
    } else {
	/* Init to an empty string */
	retVal = Tcl_NewStringObj("",0);
	Tcl_IncrRefCount(retVal);
    }
    /* 
     * We increment and then decrement the refCount of split to free
     * it.  We do this right at the end, in case there are
     * optimisations in Tcl_FSJoinPath(split, nplen) above which would
     * let it make use of split more effectively if it has a refCount
     * of zero.  Also we can't just decrement the ref count, in case
     * 'split' was actually returned by the join call above, in a
     * single-element optimisation when nplen == 1.
     */
    Tcl_IncrRefCount(split);
    Tcl_DecrRefCount(split);

    /* This has a refCount of 1 for the caller */
    return retVal;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSGetPathType --
 *
 *	Determines whether a given path is relative to the current
 *	directory, relative to the current volume, or absolute.  
 *
 * Results:
 *	Returns one of TCL_PATH_ABSOLUTE, TCL_PATH_RELATIVE, or
 *	TCL_PATH_VOLUME_RELATIVE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_PathType
Tcl_FSGetPathType(pathObjPtr)
    Tcl_Obj *pathObjPtr;
{
    return FSGetPathType(pathObjPtr, NULL, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * FSGetPathType --
 *
 *	Determines whether a given path is relative to the current
 *	directory, relative to the current volume, or absolute.  If the
 *	caller wishes to know which filesystem claimed the path (in the
 *	case for which the path is absolute), then a reference to a
 *	filesystem pointer can be passed in (but passing NULL is
 *	acceptable).
 *
 * Results:
 *	Returns one of TCL_PATH_ABSOLUTE, TCL_PATH_RELATIVE, or
 *	TCL_PATH_VOLUME_RELATIVE.  The filesystem reference will
 *	be set if and only if it is non-NULL and the function's 
 *	return value is TCL_PATH_ABSOLUTE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_PathType
FSGetPathType(pathObjPtr, filesystemPtrPtr, driveNameLengthPtr)
    Tcl_Obj *pathObjPtr;
    Tcl_Filesystem **filesystemPtrPtr;
    int *driveNameLengthPtr;
{
    if (Tcl_FSConvertToPathType(NULL, pathObjPtr) != TCL_OK) {
	return GetPathType(pathObjPtr, filesystemPtrPtr, 
			   driveNameLengthPtr, NULL);
    } else {
	FsPath *fsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
	if (fsPathPtr->cwdPtr != NULL) {
	    if (PATHFLAGS(pathObjPtr) == 0) {
		return TCL_PATH_RELATIVE;
	    }
	    return FSGetPathType(fsPathPtr->cwdPtr, filesystemPtrPtr, 
				 driveNameLengthPtr);
	} else {
	    return GetPathType(pathObjPtr, filesystemPtrPtr, 
			       driveNameLengthPtr, NULL);
	}
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSJoinPath --
 *
 *      This function takes the given Tcl_Obj, which should be a valid
 *      list, and returns the path object given by considering the
 *      first 'elements' elements as valid path segments.  If elements < 0,
 *      we use the entire list.
 *      
 * Results:
 *      Returns object with refCount of zero, (or if non-zero, it has
 *      references elsewhere in Tcl).  Either way, the caller must
 *      increment its refCount before use.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj* 
Tcl_FSJoinPath(listObj, elements)
    Tcl_Obj *listObj;
    int elements;
{
    Tcl_Obj *res;
    int i;
    Tcl_Filesystem *fsPtr = NULL;
    
    if (elements < 0) {
	if (Tcl_ListObjLength(NULL, listObj, &elements) != TCL_OK) {
	    return NULL;
	}
    } else {
	/* Just make sure it is a valid list */
	int listTest;
	if (Tcl_ListObjLength(NULL, listObj, &listTest) != TCL_OK) {
	    return NULL;
	}
	/* 
	 * Correct this if it is too large, otherwise we will
	 * waste our time joining null elements to the path 
	 */
	if (elements > listTest) {
	    elements = listTest;
	}
    }
    
    if (elements == 2) {
	/* 
	 * This is a special case where we can be much more
	 * efficient
	 */
	Tcl_Obj *base;
	
	Tcl_ListObjIndex(NULL, listObj, 0, &base);
	/* 
	 * There is only any value in doing this if the first object is
	 * of path type, otherwise we'll never actually get any
	 * efficiency benefit elsewhere in the code (from re-using the
	 * normalized representation of the base object).
	 */
	if (base->typePtr == &tclFsPathType
		&& !(base->bytes != NULL && base->bytes[0] == '\0')) {
	    Tcl_Obj *tail;
	    Tcl_PathType type;
	    Tcl_ListObjIndex(NULL, listObj, 1, &tail);
	    type = GetPathType(tail, NULL, NULL, NULL);
	    if (type == TCL_PATH_RELATIVE) {
		CONST char *str;
		int len;
		str = Tcl_GetStringFromObj(tail,&len);
		if (len == 0) {
		    /* 
		     * This happens if we try to handle the root volume
		     * '/'.  There's no need to return a special path
		     * object, when the base itself is just fine!
		     */
		    return base;
		}
		if (str[0] != '.') {
		    return TclNewFSPathObj(base, str, len);
		}
		/* 
		 * Otherwise we don't have an easy join, and
		 * we must let the more general code below handle
		 * things
		 */
	    } else {
		return tail;
	    }
	}
    }
    
    res = Tcl_NewObj();
    
    for (i = 0; i < elements; i++) {
	Tcl_Obj *elt;
	int driveNameLength;
	Tcl_PathType type;
	char *strElt;
	int strEltLen;
	int length;
	char *ptr;
	Tcl_Obj *driveName = NULL;
	
	Tcl_ListObjIndex(NULL, listObj, i, &elt);
	strElt = Tcl_GetStringFromObj(elt, &strEltLen);
	type = GetPathType(elt, &fsPtr, &driveNameLength, &driveName);
	if (type != TCL_PATH_RELATIVE) {
	    /* Zero out the current result */
	    Tcl_DecrRefCount(res);
	    if (driveName != NULL) {
		res = Tcl_DuplicateObj(driveName);
		Tcl_DecrRefCount(driveName);
	    } else {
		res = Tcl_NewStringObj(strElt, driveNameLength);
	    }
	    strElt += driveNameLength;
	}
	
	ptr = Tcl_GetStringFromObj(res, &length);
	
	/* 
	 * Strip off any './' before a tilde, unless this is the
	 * beginning of the path.
	 */
	if (length > 0 && strEltLen > 0) {
	    if ((strElt[0] == '.') && (strElt[1] == '/') 
	      && (strElt[2] == '~')) {
		strElt += 2;
	    }
	}

	/* 
	 * A NULL value for fsPtr at this stage basically means
	 * we're trying to join a relative path onto something
	 * which is also relative (or empty).  There's nothing
	 * particularly wrong with that.
	 */
	if (*strElt == '\0') continue;
	
	if (fsPtr == &tclNativeFilesystem || fsPtr == NULL) {
	    TclpNativeJoinPath(res, strElt);
	} else {
	    char separator = '/';
	    int needsSep = 0;
	    
	    if (fsPtr->filesystemSeparatorProc != NULL) {
		Tcl_Obj *sep = (*fsPtr->filesystemSeparatorProc)(res);
		if (sep != NULL) {
		    separator = Tcl_GetString(sep)[0];
		}
	    }

	    if (length > 0 && ptr[length -1] != '/') {
		Tcl_AppendToObj(res, &separator, 1);
		length++;
	    }
	    Tcl_SetObjLength(res, length + (int) strlen(strElt));
	    
	    ptr = Tcl_GetString(res) + length;
	    for (; *strElt != '\0'; strElt++) {
		if (*strElt == separator) {
		    while (strElt[1] == separator) {
			strElt++;
		    }
		    if (strElt[1] != '\0') {
			if (needsSep) {
			    *ptr++ = separator;
			}
		    }
		} else {
		    *ptr++ = *strElt;
		    needsSep = 1;
		}
	    }
	    length = ptr - Tcl_GetString(res);
	    Tcl_SetObjLength(res, length);
	}
    }
    return res;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSConvertToPathType --
 *
 *      This function tries to convert the given Tcl_Obj to a valid
 *      Tcl path type, taking account of the fact that the cwd may
 *      have changed even if this object is already supposedly of
 *      the correct type.
 *      
 *      The filename may begin with "~" (to indicate current user's
 *      home directory) or "~<user>" (to indicate any user's home
 *      directory).
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */
int 
Tcl_FSConvertToPathType(interp, objPtr)
    Tcl_Interp *interp;		/* Interpreter in which to store error
				 * message (if necessary). */
    Tcl_Obj *objPtr;		/* Object to convert to a valid, current
				 * path type. */
{
    /* 
     * While it is bad practice to examine an object's type directly,
     * this is actually the best thing to do here.  The reason is that
     * if we are converting this object to FsPath type for the first
     * time, we don't need to worry whether the 'cwd' has changed.
     * On the other hand, if this object is already of FsPath type,
     * and is a relative path, we do have to worry about the cwd.
     * If the cwd has changed, we must recompute the path.
     */
    if (objPtr->typePtr == &tclFsPathType) {
	FsPath *fsPathPtr = (FsPath*) PATHOBJ(objPtr);
	if (fsPathPtr->filesystemEpoch != theFilesystemEpoch) {
	    if (objPtr->bytes == NULL) {
		UpdateStringOfFsPath(objPtr);
	    }
	    FreeFsPathInternalRep(objPtr);
	    objPtr->typePtr = NULL;
	    return Tcl_ConvertToType(interp, objPtr, &tclFsPathType);
	}
	return TCL_OK;
	/* 
	 * This code is intentionally never reached.  Once fs-optimisation
	 * is complete, it will be removed/replaced
	 */
#if 0
	if (fsPathPtr->cwdPtr == NULL) {
	    return TCL_OK;
	} else {
	    if (TclFSCwdPointerEquals(fsPathPtr->cwdPtr)) {
		return TCL_OK;
	    } else {
		if (objPtr->bytes == NULL) {
		    UpdateStringOfFsPath(objPtr);
		}
		FreeFsPathInternalRep(objPtr);
		objPtr->typePtr = NULL;
		return Tcl_ConvertToType(interp, objPtr, &tclFsPathType);
	    }
	}
#endif
    } else {
	return Tcl_ConvertToType(interp, objPtr, &tclFsPathType);
    }
}

/* 
 * Helper function for SetFsPathFromAny.  Returns position of first
 * directory delimiter in the path.
 */
static int
FindSplitPos(path, separator)
    char *path;
    char *separator;
{
    int count = 0;
    switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	case TCL_PLATFORM_MAC:
	    while (path[count] != 0) {
		if (path[count] == *separator) {
		    return count;
		}
		count++;
	    }
	    break;

	case TCL_PLATFORM_WINDOWS:
	    while (path[count] != 0) {
		if (path[count] == *separator || path[count] == '\\') {
		    return count;
		}
		count++;
	    }
	    break;
    }
    return count;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclNewFSPathObj --
 *
 *      Creates a path object whose string representation is 
 *      '[file join dirPtr addStrRep]', but does so in a way that
 *      allows for more efficient caching of normalized paths.
 *      
 * Assumptions:
 *      'dirPtr' must be an absolute path.  
 *      'len' may not be zero.
 *      
 * Results:
 *      The new Tcl object, with refCount zero.
 *
 * Side effects:
 *	Memory is allocated.  'dirPtr' gets an additional refCount.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj*
TclNewFSPathObj(Tcl_Obj *dirPtr, CONST char *addStrRep, int len)
{
    FsPath *fsPathPtr;
    Tcl_Obj *objPtr;
    
    objPtr = Tcl_NewObj();
    fsPathPtr = (FsPath*)ckalloc((unsigned)sizeof(FsPath));
    
    if (tclPlatform == TCL_PLATFORM_MAC) { 
	/* 
	 * Mac relative paths may begin with a directory separator ':'. 
	 * If present, we need to skip this ':' because we assume that 
	 * we can join dirPtr and addStrRep by concatenating them as 
	 * strings (and we ensure that dirPtr is terminated by a ':'). 
	 */ 
	if (addStrRep[0] == ':') { 
	    addStrRep++; 
	    len--; 
	} 
    } 
    /* Setup the path */
    fsPathPtr->translatedPathPtr = NULL;
    fsPathPtr->normPathPtr = Tcl_NewStringObj(addStrRep, len);
    Tcl_IncrRefCount(fsPathPtr->normPathPtr);
    fsPathPtr->cwdPtr = dirPtr;
    Tcl_IncrRefCount(dirPtr);
    fsPathPtr->nativePathPtr = NULL;
    fsPathPtr->fsRecPtr = NULL;
    fsPathPtr->filesystemEpoch = theFilesystemEpoch;

    PATHOBJ(objPtr) = (VOID *) fsPathPtr;
    PATHFLAGS(objPtr) = TCLPATH_RELATIVE | TCLPATH_APPENDED;
    objPtr->typePtr = &tclFsPathType;
    objPtr->bytes = NULL;
    objPtr->length = 0;
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSMakePathRelative --
 *
 *      Like SetFsPathFromAny, but assumes the given object is an
 *      absolute normalized path. Only for internal use.
 *      
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj*
TclFSMakePathRelative(interp, objPtr, cwdPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object we have. */
    Tcl_Obj *cwdPtr;		/* Make it relative to this. */
{
    int cwdLen, len;
    CONST char *tempStr;
    
    if (objPtr->typePtr == &tclFsPathType) {
	FsPath* fsPathPtr = (FsPath*) PATHOBJ(objPtr);
	if (PATHFLAGS(objPtr) != 0 
		&& fsPathPtr->cwdPtr == cwdPtr) {
	    objPtr = fsPathPtr->normPathPtr;
	    /* Free old representation */
	    if (objPtr->typePtr != NULL) {
		if (objPtr->bytes == NULL) {
		    if (objPtr->typePtr->updateStringProc == NULL) {
			if (interp != NULL) {
			    Tcl_ResetResult(interp);
			    Tcl_AppendResult(interp, "can't find object",
					     "string representation", (char *) NULL);
			}
			return NULL;
		    }
		    objPtr->typePtr->updateStringProc(objPtr);
		}
		if ((objPtr->typePtr->freeIntRepProc) != NULL) {
		    (*objPtr->typePtr->freeIntRepProc)(objPtr);
		}
	    }

	    fsPathPtr = (FsPath*)ckalloc((unsigned)sizeof(FsPath));

	    /* Circular reference, by design */
	    fsPathPtr->translatedPathPtr = objPtr;
	    fsPathPtr->normPathPtr = NULL;
	    fsPathPtr->cwdPtr = cwdPtr;
	    Tcl_IncrRefCount(cwdPtr);
	    fsPathPtr->nativePathPtr = NULL;
	    fsPathPtr->fsRecPtr = NULL;
	    fsPathPtr->filesystemEpoch = theFilesystemEpoch;

	    PATHOBJ(objPtr) = (VOID *) fsPathPtr;
	    PATHFLAGS(objPtr) = 0;
	    objPtr->typePtr = &tclFsPathType;

	    return objPtr;
	}
    }
    /* 
     * We know the cwd is a normalised object which does
     * not end in a directory delimiter, unless the cwd
     * is the name of a volume, in which case it will
     * end in a delimiter!  We handle this situation here.
     * A better test than the '!= sep' might be to simply
     * check if 'cwd' is a root volume.
     * 
     * Note that if we get this wrong, we will strip off
     * either too much or too little below, leading to
     * wrong answers returned by glob.
     */
    tempStr = Tcl_GetStringFromObj(cwdPtr, &cwdLen);
    /* 
     * Should we perhaps use 'Tcl_FSPathSeparator'?
     * But then what about the Windows special case?
     * Perhaps we should just check if cwd is a root
     * volume.
     */
    switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	    if (tempStr[cwdLen-1] != '/') {
		cwdLen++;
	    }
	    break;
	case TCL_PLATFORM_WINDOWS:
	    if (tempStr[cwdLen-1] != '/' 
		    && tempStr[cwdLen-1] != '\\') {
		cwdLen++;
	    }
	    break;
	case TCL_PLATFORM_MAC:
	    if (tempStr[cwdLen-1] != ':') {
		cwdLen++;
	    }
	    break;
    }
    tempStr = Tcl_GetStringFromObj(objPtr, &len);
    return Tcl_NewStringObj(tempStr + cwdLen, len - cwdLen);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSMakePathFromNormalized --
 *
 *      Like SetFsPathFromAny, but assumes the given object is an
 *      absolute normalized path. Only for internal use.
 *      
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

int
TclFSMakePathFromNormalized(interp, objPtr, nativeRep)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
    ClientData nativeRep;	/* The native rep for the object, if known
				 * else NULL. */
{
    FsPath *fsPathPtr;

    if (objPtr->typePtr == &tclFsPathType) {
	return TCL_OK;
    }
    
    /* Free old representation */
    if (objPtr->typePtr != NULL) {
	if (objPtr->bytes == NULL) {
	    if (objPtr->typePtr->updateStringProc == NULL) {
		if (interp != NULL) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "can't find object",
				     "string representation", (char *) NULL);
		}
		return TCL_ERROR;
	    }
	    objPtr->typePtr->updateStringProc(objPtr);
	}
	if ((objPtr->typePtr->freeIntRepProc) != NULL) {
	    (*objPtr->typePtr->freeIntRepProc)(objPtr);
	}
    }

    fsPathPtr = (FsPath*)ckalloc((unsigned)sizeof(FsPath));
    /* It's a pure normalized absolute path */
    fsPathPtr->translatedPathPtr = NULL;
    fsPathPtr->normPathPtr = objPtr;
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = nativeRep;
    fsPathPtr->fsRecPtr = NULL;
    fsPathPtr->filesystemEpoch = theFilesystemEpoch;

    PATHOBJ(objPtr) = (VOID *) fsPathPtr;
    PATHFLAGS(objPtr) = 0;
    objPtr->typePtr = &tclFsPathType;

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSNewNativePath --
 *
 *      This function performs the something like that reverse of the 
 *      usual obj->path->nativerep conversions.  If some code retrieves
 *      a path in native form (from, e.g. readlink or a native dialog),
 *      and that path is to be used at the Tcl level, then calling
 *      this function is an efficient way of creating the appropriate
 *      path object type.
 *      
 *      Any memory which is allocated for 'clientData' should be retained
 *      until clientData is passed to the filesystem's freeInternalRepProc
 *      when it can be freed.  The built in platform-specific filesystems
 *      use 'ckalloc' to allocate clientData, and ckfree to free it.
 *
 * Results:
 *      NULL or a valid path object pointer, with refCount zero.
 *
 * Side effects:
 *	New memory may be allocated.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSNewNativePath(fromFilesystem, clientData)
    Tcl_Filesystem* fromFilesystem;
    ClientData clientData;
{
    Tcl_Obj *objPtr;
    FsPath *fsPathPtr;

    FilesystemRecord *fsFromPtr;
    int epoch;
    
    objPtr = TclFSInternalToNormalized(fromFilesystem, clientData, 
				       &fsFromPtr, &epoch);

    if (objPtr == NULL) {
	return NULL;
    }
    
    /* 
     * Free old representation; shouldn't normally be any,
     * but best to be safe. 
     */
    if (objPtr->typePtr != NULL) {
	if (objPtr->bytes == NULL) {
	    if (objPtr->typePtr->updateStringProc == NULL) {
		return NULL;
	    }
	    objPtr->typePtr->updateStringProc(objPtr);
	}
	if ((objPtr->typePtr->freeIntRepProc) != NULL) {
	    (*objPtr->typePtr->freeIntRepProc)(objPtr);
	}
    }
    
    fsPathPtr = (FsPath*)ckalloc((unsigned)sizeof(FsPath));
    fsPathPtr->translatedPathPtr = NULL;
    /* Circular reference, by design */
    fsPathPtr->normPathPtr = objPtr;
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = clientData;
    fsPathPtr->fsRecPtr = fsFromPtr;
    /* We must increase the refCount for this filesystem. */
    fsPathPtr->fsRecPtr->fileRefCount++;
    fsPathPtr->filesystemEpoch = epoch;

    PATHOBJ(objPtr) = (VOID *) fsPathPtr;
    PATHFLAGS(objPtr) = 0;
    objPtr->typePtr = &tclFsPathType;
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetTranslatedPath --
 *
 *      This function attempts to extract the translated path
 *      from the given Tcl_Obj.  If the translation succeeds (i.e. the
 *      object is a valid path), then it is returned.  Otherwise NULL
 *      will be returned, and an error message may be left in the
 *      interpreter (if it is non-NULL)
 *
 * Results:
 *      NULL or a valid Tcl_Obj pointer.
 *
 * Side effects:
 *	Only those of 'Tcl_FSConvertToPathType'
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj* 
Tcl_FSGetTranslatedPath(interp, pathPtr)
    Tcl_Interp *interp;
    Tcl_Obj* pathPtr;
{
    register FsPath* srcFsPathPtr;
    if (Tcl_FSConvertToPathType(interp, pathPtr) != TCL_OK) {
	return NULL;
    }
    srcFsPathPtr = (FsPath*) PATHOBJ(pathPtr);
    if (srcFsPathPtr->translatedPathPtr == NULL) {
	if (PATHFLAGS(pathPtr) != 0) {
	    return Tcl_FSGetNormalizedPath(interp, pathPtr);
	}
	/* 
	 * It is a pure absolute, normalized path object.
	 * This is something like being a 'pure list'.  The
	 * object's string, translatedPath and normalizedPath
	 * are all identical.
	 */
	return srcFsPathPtr->normPathPtr;
    } else {
	/* It is an ordinary path object */
	return srcFsPathPtr->translatedPathPtr;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetTranslatedStringPath --
 *
 *      This function attempts to extract the translated path
 *      from the given Tcl_Obj.  If the translation succeeds (i.e. the
 *      object is a valid path), then the path is returned.  Otherwise NULL
 *      will be returned, and an error message may be left in the
 *      interpreter (if it is non-NULL)
 *
 * Results:
 *      NULL or a valid string.
 *
 * Side effects:
 *	Only those of 'Tcl_FSConvertToPathType'
 *
 *---------------------------------------------------------------------------
 */
CONST char*
Tcl_FSGetTranslatedStringPath(interp, pathPtr)
    Tcl_Interp *interp;
    Tcl_Obj* pathPtr;
{
    Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(interp, pathPtr);
    if (transPtr == NULL) {
	return NULL;
    } else {
	return Tcl_GetString(transPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetNormalizedPath --
 *
 *      This important function attempts to extract from the given Tcl_Obj
 *      a unique normalised path representation, whose string value can
 *      be used as a unique identifier for the file.
 *
 * Results:
 *      NULL or a valid path object pointer.
 *
 * Side effects:
 *	New memory may be allocated.  The Tcl 'errno' may be modified
 *      in the process of trying to examine various path possibilities.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj* 
Tcl_FSGetNormalizedPath(interp, pathObjPtr)
    Tcl_Interp *interp;
    Tcl_Obj* pathObjPtr;
{
    register FsPath* fsPathPtr;
    if (Tcl_FSConvertToPathType(interp, pathObjPtr) != TCL_OK) {
	return NULL;
    }
    fsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);

    if (PATHFLAGS(pathObjPtr) != 0) {
	/* 
	 * This is a special path object which is the result of
	 * something like 'file join' 
	 */
	Tcl_Obj *dir, *copy;
	int cwdLen;
	int pathType;
	CONST char *cwdStr;
	ClientData clientData = NULL;
	
	pathType = Tcl_FSGetPathType(fsPathPtr->cwdPtr);
	dir = Tcl_FSGetNormalizedPath(interp, fsPathPtr->cwdPtr);
	if (dir == NULL) {
	    return NULL;
	}
	if (pathObjPtr->bytes == NULL) {
	    UpdateStringOfFsPath(pathObjPtr);
	}
	copy = Tcl_DuplicateObj(dir);
	Tcl_IncrRefCount(copy);
	Tcl_IncrRefCount(dir);
	/* We now own a reference on both 'dir' and 'copy' */
	
	cwdStr = Tcl_GetStringFromObj(copy, &cwdLen);
	/* 
	 * Should we perhaps use 'Tcl_FSPathSeparator'?
	 * But then what about the Windows special case?
	 * Perhaps we should just check if cwd is a root volume.
	 * We should never get cwdLen == 0 in this code path.
	 */
	switch (tclPlatform) {
	    case TCL_PLATFORM_UNIX:
		if (cwdStr[cwdLen-1] != '/') {
		    Tcl_AppendToObj(copy, "/", 1);
		    cwdLen++;
		}
		break;
	    case TCL_PLATFORM_WINDOWS:
		if (cwdStr[cwdLen-1] != '/' 
			&& cwdStr[cwdLen-1] != '\\') {
		    Tcl_AppendToObj(copy, "/", 1);
		    cwdLen++;
		}
		break;
	    case TCL_PLATFORM_MAC:
		if (cwdStr[cwdLen-1] != ':') {
		    Tcl_AppendToObj(copy, ":", 1);
		    cwdLen++;
		}
		break;
	}
	Tcl_AppendObjToObj(copy, fsPathPtr->normPathPtr);
	/* 
	 * Normalize the combined string, but only starting after
	 * the end of the previously normalized 'dir'.  This should
	 * be much faster!  We use 'cwdLen-1' so that we are
	 * already pointing at the dir-separator that we know about.
	 * The normalization code will actually start off directly
	 * after that separator.
	 */
	TclFSNormalizeToUniquePath(interp, copy, cwdLen-1, 
	  (fsPathPtr->nativePathPtr == NULL ? &clientData : NULL));
	/* Now we need to construct the new path object */
	
	if (pathType == TCL_PATH_RELATIVE) {
	    register FsPath* origDirFsPathPtr;
	    Tcl_Obj *origDir = fsPathPtr->cwdPtr;
	    origDirFsPathPtr = (FsPath*) PATHOBJ(origDir);
	    
	    fsPathPtr->cwdPtr = origDirFsPathPtr->cwdPtr;
	    Tcl_IncrRefCount(fsPathPtr->cwdPtr);
	    
	    Tcl_DecrRefCount(fsPathPtr->normPathPtr);
	    fsPathPtr->normPathPtr = copy;
	    /* That's our reference to copy used */
	    Tcl_DecrRefCount(dir);
	    Tcl_DecrRefCount(origDir);
	} else {
	    Tcl_DecrRefCount(fsPathPtr->cwdPtr);
	    fsPathPtr->cwdPtr = NULL;
	    Tcl_DecrRefCount(fsPathPtr->normPathPtr);
	    fsPathPtr->normPathPtr = copy;
	    /* That's our reference to copy used */
	    Tcl_DecrRefCount(dir);
	}
	if (clientData != NULL) {
	    fsPathPtr->nativePathPtr = clientData;
	}
	PATHFLAGS(pathObjPtr) = 0;
    }
    /* Ensure cwd hasn't changed */
    if (fsPathPtr->cwdPtr != NULL) {
	if (!TclFSCwdPointerEquals(fsPathPtr->cwdPtr)) {
	    if (pathObjPtr->bytes == NULL) {
		UpdateStringOfFsPath(pathObjPtr);
	    }
	    FreeFsPathInternalRep(pathObjPtr);
	    pathObjPtr->typePtr = NULL;
	    if (Tcl_ConvertToType(interp, pathObjPtr, 
				  &tclFsPathType) != TCL_OK) {
		return NULL;
	    }
	    fsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
	} else if (fsPathPtr->normPathPtr == NULL) {
	    int cwdLen;
	    Tcl_Obj *copy;
	    CONST char *cwdStr;
	    ClientData clientData = NULL;
	    
	    copy = Tcl_DuplicateObj(fsPathPtr->cwdPtr);
	    Tcl_IncrRefCount(copy);
	    cwdStr = Tcl_GetStringFromObj(copy, &cwdLen);
	    /* 
	     * Should we perhaps use 'Tcl_FSPathSeparator'?
	     * But then what about the Windows special case?
	     * Perhaps we should just check if cwd is a root volume.
	     * We should never get cwdLen == 0 in this code path.
	     */
	    switch (tclPlatform) {
		case TCL_PLATFORM_UNIX:
		    if (cwdStr[cwdLen-1] != '/') {
			Tcl_AppendToObj(copy, "/", 1);
			cwdLen++;
		    }
		    break;
		case TCL_PLATFORM_WINDOWS:
		    if (cwdStr[cwdLen-1] != '/' 
			    && cwdStr[cwdLen-1] != '\\') {
			Tcl_AppendToObj(copy, "/", 1);
			cwdLen++;
		    }
		    break;
		case TCL_PLATFORM_MAC:
		    if (cwdStr[cwdLen-1] != ':') {
			Tcl_AppendToObj(copy, ":", 1);
			cwdLen++;
		    }
		    break;
	    }
	    Tcl_AppendObjToObj(copy, pathObjPtr);
	    /* 
	     * Normalize the combined string, but only starting after
	     * the end of the previously normalized 'dir'.  This should
	     * be much faster!
	     */
	    TclFSNormalizeToUniquePath(interp, copy, cwdLen-1, 
	      (fsPathPtr->nativePathPtr == NULL ? &clientData : NULL));
	    fsPathPtr->normPathPtr = copy;
	    if (clientData != NULL) {
		fsPathPtr->nativePathPtr = clientData;
	    }
	}
    }
    if (fsPathPtr->normPathPtr == NULL) {
	ClientData clientData = NULL;
	Tcl_Obj *useThisCwd = NULL;
	/* 
	 * Since normPathPtr is NULL, but this is a valid path
	 * object, we know that the translatedPathPtr cannot be NULL.
	 */
	Tcl_Obj *absolutePath = fsPathPtr->translatedPathPtr;
	char *path = Tcl_GetString(absolutePath);
	
	/* 
	 * We have to be a little bit careful here to avoid infinite loops
	 * we're asking Tcl_FSGetPathType to return the path's type, but
	 * that call can actually result in a lot of other filesystem
	 * action, which might loop back through here.
	 */
	if ((path[0] != '\0') && 
	  (Tcl_FSGetPathType(pathObjPtr) == TCL_PATH_RELATIVE)) {
	    useThisCwd = Tcl_FSGetCwd(interp);

	    if (useThisCwd == NULL) {
		return NULL;
	    }

	    absolutePath = Tcl_FSJoinToPath(useThisCwd, 1, &absolutePath);
	    Tcl_IncrRefCount(absolutePath);
	    /* We have a refCount on the cwd */
	}
	/* Already has refCount incremented */
	fsPathPtr->normPathPtr = TclFSNormalizeAbsolutePath(interp, absolutePath, 
		       (fsPathPtr->nativePathPtr == NULL ? &clientData : NULL));
	if (0 && (clientData != NULL)) {
	    fsPathPtr->nativePathPtr = 
	      (*fsPathPtr->fsRecPtr->fsPtr->dupInternalRepProc)(clientData);
	}
	if (!strcmp(Tcl_GetString(fsPathPtr->normPathPtr),
		    Tcl_GetString(pathObjPtr))) {
	    /* 
	     * The path was already normalized.  
	     * Get rid of the duplicate.
	     */
	    Tcl_DecrRefCount(fsPathPtr->normPathPtr);
	    /* 
	     * We do *not* increment the refCount for 
	     * this circular reference 
	     */
	    fsPathPtr->normPathPtr = pathObjPtr;
	}
	if (useThisCwd != NULL) {
	    /* This was returned by Tcl_FSJoinToPath above */
	    Tcl_DecrRefCount(absolutePath);
	    fsPathPtr->cwdPtr = useThisCwd;
	}
    }
    return fsPathPtr->normPathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetInternalRep --
 *
 *      Extract the internal representation of a given path object,
 *      in the given filesystem.  If the path object belongs to a
 *      different filesystem, we return NULL.
 *      
 *      If the internal representation is currently NULL, we attempt
 *      to generate it, by calling the filesystem's 
 *      'Tcl_FSCreateInternalRepProc'.
 *
 * Results:
 *      NULL or a valid internal representation.
 *
 * Side effects:
 *	An attempt may be made to convert the object.
 *
 *---------------------------------------------------------------------------
 */

ClientData 
Tcl_FSGetInternalRep(pathObjPtr, fsPtr)
    Tcl_Obj* pathObjPtr;
    Tcl_Filesystem *fsPtr;
{
    register FsPath* srcFsPathPtr;
    
    if (Tcl_FSConvertToPathType(NULL, pathObjPtr) != TCL_OK) {
	return NULL;
    }
    srcFsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
    
    /* 
     * We will only return the native representation for the caller's
     * filesystem.  Otherwise we will simply return NULL. This means
     * that there must be a unique bi-directional mapping between paths
     * and filesystems, and that this mapping will not allow 'remapped'
     * files -- files which are in one filesystem but mapped into
     * another.  Another way of putting this is that 'stacked'
     * filesystems are not allowed.  We recognise that this is a
     * potentially useful feature for the future.
     * 
     * Even something simple like a 'pass through' filesystem which
     * logs all activity and passes the calls onto the native system
     * would be nice, but not easily achievable with the current
     * implementation.
     */
    if (srcFsPathPtr->fsRecPtr == NULL) {
	/* 
	 * This only usually happens in wrappers like TclpStat which
	 * create a string object and pass it to TclpObjStat.  Code
	 * which calls the Tcl_FS..  functions should always have a
	 * filesystem already set.  Whether this code path is legal or
	 * not depends on whether we decide to allow external code to
	 * call the native filesystem directly.  It is at least safer
	 * to allow this sub-optimal routing.
	 */
	Tcl_FSGetFileSystemForPath(pathObjPtr);
	
	/* 
	 * If we fail through here, then the path is probably not a
	 * valid path in the filesystsem, and is most likely to be a
	 * use of the empty path "" via a direct call to one of the
	 * objectified interfaces (e.g. from the Tcl testsuite).
	 */
	srcFsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
	if (srcFsPathPtr->fsRecPtr == NULL) {
	    return NULL;
	}
    }

    if (fsPtr != srcFsPathPtr->fsRecPtr->fsPtr) {
	/* 
	 * There is still one possibility we should consider; if the
	 * file belongs to a different filesystem, perhaps it is
	 * actually linked through to a file in our own filesystem
	 * which we do care about.  The way we can check for this
	 * is we ask what filesystem this path belongs to.
	 */
	Tcl_Filesystem *actualFs = Tcl_FSGetFileSystemForPath(pathObjPtr);
	if (actualFs == fsPtr) {
	    return Tcl_FSGetInternalRep(pathObjPtr, fsPtr);
	}
	return NULL;
    }

    if (srcFsPathPtr->nativePathPtr == NULL) {
	Tcl_FSCreateInternalRepProc *proc;
	proc = srcFsPathPtr->fsRecPtr->fsPtr->createInternalRepProc;

	if (proc == NULL) {
	    return NULL;
	}
	srcFsPathPtr->nativePathPtr = (*proc)(pathObjPtr);
    }
    return srcFsPathPtr->nativePathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSEnsureEpochOk --
 *
 *      This will ensure the pathObjPtr is up to date and can be
 *      converted into a "path" type, and that we are able to generate a
 *      complete normalized path which is used to determine the
 *      filesystem match.
 *
 * Results:
 *      Standard Tcl return code.
 *
 * Side effects:
 *	An attempt may be made to convert the object.
 *
 *---------------------------------------------------------------------------
 */

int 
TclFSEnsureEpochOk(pathObjPtr, theEpoch, fsPtrPtr)
    Tcl_Obj* pathObjPtr;
    int theEpoch;
    Tcl_Filesystem **fsPtrPtr;
{
    FsPath* srcFsPathPtr;

    /* 
     * SHOULD BE ABLE TO IMPROVE EFFICIENCY HERE.
     */

    if (Tcl_FSGetNormalizedPath(NULL, pathObjPtr) == NULL) {
	return TCL_ERROR;
    }

    srcFsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);

    /* 
     * Check if the filesystem has changed in some way since
     * this object's internal representation was calculated.
     */
    if (srcFsPathPtr->filesystemEpoch != theEpoch) {
	/* 
	 * We have to discard the stale representation and 
	 * recalculate it 
	 */
	if (pathObjPtr->bytes == NULL) {
	    UpdateStringOfFsPath(pathObjPtr);
	}
	FreeFsPathInternalRep(pathObjPtr);
	pathObjPtr->typePtr = NULL;
	if (SetFsPathFromAny(NULL, pathObjPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	srcFsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
    }
    /* Check whether the object is already assigned to a fs */
    if (srcFsPathPtr->fsRecPtr != NULL) {
	*fsPtrPtr = srcFsPathPtr->fsRecPtr->fsPtr;
    }
    return TCL_OK;
}

void 
TclFSSetPathDetails(pathObjPtr, fsRecPtr, clientData, theEpoch) 
    Tcl_Obj *pathObjPtr;
    FilesystemRecord *fsRecPtr;
    ClientData clientData;
    int theEpoch;
{
    /* We assume pathObjPtr is already of the correct type */
    FsPath* srcFsPathPtr;
    
    srcFsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);
    srcFsPathPtr->fsRecPtr = fsRecPtr;
    srcFsPathPtr->nativePathPtr = clientData;
    srcFsPathPtr->filesystemEpoch = theEpoch;
    fsRecPtr->fileRefCount++;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSEqualPaths --
 *
 *      This function tests whether the two paths given are equal path
 *      objects.  If either or both is NULL, 0 is always returned.
 *
 * Results:
 *      1 or 0.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int 
Tcl_FSEqualPaths(firstPtr, secondPtr)
    Tcl_Obj* firstPtr;
    Tcl_Obj* secondPtr;
{
    if (firstPtr == secondPtr) {
	return 1;
    } else {
	char *firstStr, *secondStr;
	int firstLen, secondLen, tempErrno;

	if (firstPtr == NULL || secondPtr == NULL) {
	    return 0;
	}
	firstStr  = Tcl_GetStringFromObj(firstPtr, &firstLen);
	secondStr = Tcl_GetStringFromObj(secondPtr, &secondLen);
	if ((firstLen == secondLen) && (strcmp(firstStr, secondStr) == 0)) {
	    return 1;
	}
	/* 
	 * Try the most thorough, correct method of comparing fully
	 * normalized paths
	 */

	tempErrno = Tcl_GetErrno();
	firstPtr = Tcl_FSGetNormalizedPath(NULL, firstPtr);
	secondPtr = Tcl_FSGetNormalizedPath(NULL, secondPtr);
	Tcl_SetErrno(tempErrno);

	if (firstPtr == NULL || secondPtr == NULL) {
	    return 0;
	}
	firstStr  = Tcl_GetStringFromObj(firstPtr, &firstLen);
	secondStr = Tcl_GetStringFromObj(secondPtr, &secondLen);
	if ((firstLen == secondLen) && (strcmp(firstStr, secondStr) == 0)) {
	    return 1;
	}
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetFsPathFromAny --
 *
 *      This function tries to convert the given Tcl_Obj to a valid
 *      Tcl path type.
 *      
 *      The filename may begin with "~" (to indicate current user's
 *      home directory) or "~<user>" (to indicate any user's home
 *      directory).
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

static int
SetFsPathFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    int len;
    FsPath *fsPathPtr;
    Tcl_Obj *transPtr;
    char *name;
    
    if (objPtr->typePtr == &tclFsPathType) {
	return TCL_OK;
    }
    
    /* 
     * First step is to translate the filename.  This is similar to
     * Tcl_TranslateFilename, but shouldn't convert everything to
     * windows backslashes on that platform.  The current
     * implementation of this piece is a slightly optimised version
     * of the various Tilde/Split/Join stuff to avoid multiple
     * split/join operations.
     * 
     * We remove any trailing directory separator.
     * 
     * However, the split/join routines are quite complex, and
     * one has to make sure not to break anything on Unix, Win
     * or MacOS (fCmd.test, fileName.test and cmdAH.test exercise
     * most of the code).
     */
    name = Tcl_GetStringFromObj(objPtr,&len);

    /*
     * Handle tilde substitutions, if needed.
     */
    if (name[0] == '~') {
	char *expandedUser;
	Tcl_DString temp;
	int split;
	char separator='/';
	
	if (tclPlatform==TCL_PLATFORM_MAC) {
	    if (strchr(name, ':') != NULL) separator = ':';
	}
	
	split = FindSplitPos(name, &separator);
	if (split != len) {
	    /* We have multiple pieces '~user/foo/bar...' */
	    name[split] = '\0';
	}
	/* Do some tilde substitution */
	if (name[1] == '\0') {
	    /* We have just '~' */
	    CONST char *dir;
	    Tcl_DString dirString;
	    if (split != len) { name[split] = separator; }
	    
	    dir = TclGetEnv("HOME", &dirString);
	    if (dir == NULL) {
		if (interp) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "couldn't find HOME environment ",
			    "variable to expand path", (char *) NULL);
		}
		return TCL_ERROR;
	    }
	    Tcl_DStringInit(&temp);
	    Tcl_JoinPath(1, &dir, &temp);
	    Tcl_DStringFree(&dirString);
	} else {
	    /* We have a user name '~user' */
	    Tcl_DStringInit(&temp);
	    if (TclpGetUserHome(name+1, &temp) == NULL) {	
		if (interp != NULL) {
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "user \"", (name+1), 
				     "\" doesn't exist", (char *) NULL);
		}
		Tcl_DStringFree(&temp);
		if (split != len) { name[split] = separator; }
		return TCL_ERROR;
	    }
	    if (split != len) { name[split] = separator; }
	}
	
	expandedUser = Tcl_DStringValue(&temp);
	transPtr = Tcl_NewStringObj(expandedUser, Tcl_DStringLength(&temp));

	if (split != len) {
	    /* Join up the tilde substitution with the rest */
	    if (name[split+1] == separator) {

		/*
		 * Somewhat tricky case like ~//foo/bar.
		 * Make use of Split/Join machinery to get it right.
		 * Assumes all paths beginning with ~ are part of the
		 * native filesystem.
		 */

		int objc;
		Tcl_Obj **objv;
		Tcl_Obj *parts = TclpNativeSplitPath(objPtr, NULL);
		Tcl_ListObjGetElements(NULL, parts, &objc, &objv);
		/* Skip '~'.  It's replaced by its expansion */
		objc--; objv++;
		while (objc--) {
		    TclpNativeJoinPath(transPtr, Tcl_GetString(*objv++));
		}
		Tcl_DecrRefCount(parts);
	    } else {
		/* Simple case. "rest" is relative path.  Just join it. */
		Tcl_Obj *rest = Tcl_NewStringObj(name+split+1,-1);
		transPtr = Tcl_FSJoinToPath(transPtr, 1, &rest);
	    }
	}
	Tcl_DStringFree(&temp);
    } else {
	transPtr = Tcl_FSJoinToPath(objPtr,0,NULL);
    }

#if defined(__CYGWIN__) && defined(__WIN32__)
    {
    extern int cygwin_conv_to_win32_path 
	_ANSI_ARGS_((CONST char *, char *));
    char winbuf[MAX_PATH+1];

    /*
     * In the Cygwin world, call conv_to_win32_path in order to use the
     * mount table to translate the file name into something Windows will
     * understand.  Take care when converting empty strings!
     */
    name = Tcl_GetStringFromObj(transPtr, &len);
    if (len > 0) {
	cygwin_conv_to_win32_path(name, winbuf);
	TclWinNoBackslash(winbuf);
	Tcl_SetStringObj(transPtr, winbuf, -1);
    }
    }
#endif /* __CYGWIN__ && __WIN32__ */

    /* 
     * Now we have a translated filename in 'transPtr'.  This will have
     * forward slashes on Windows, and will not contain any ~user
     * sequences.
     */
    
    fsPathPtr = (FsPath*)ckalloc((unsigned)sizeof(FsPath));
    fsPathPtr->translatedPathPtr = transPtr;
    Tcl_IncrRefCount(fsPathPtr->translatedPathPtr);
    fsPathPtr->normPathPtr = NULL;
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = NULL;
    fsPathPtr->fsRecPtr = NULL;
    fsPathPtr->filesystemEpoch = theFilesystemEpoch;

    /*
     * Free old representation before installing our new one.
     */
    if (objPtr->typePtr != NULL && objPtr->typePtr->freeIntRepProc != NULL) {
	(objPtr->typePtr->freeIntRepProc)(objPtr);
    }
    PATHOBJ(objPtr) = (VOID *) fsPathPtr;
    PATHFLAGS(objPtr) = 0;
    objPtr->typePtr = &tclFsPathType;

    return TCL_OK;
}

static void
FreeFsPathInternalRep(pathObjPtr)
    Tcl_Obj *pathObjPtr;	/* Path object with internal rep to free. */
{
    register FsPath* fsPathPtr = (FsPath*) PATHOBJ(pathObjPtr);

    if (fsPathPtr->translatedPathPtr != NULL) {
	if (fsPathPtr->translatedPathPtr != pathObjPtr) {
	    Tcl_DecrRefCount(fsPathPtr->translatedPathPtr);
	}
    }
    if (fsPathPtr->normPathPtr != NULL) {
	if (fsPathPtr->normPathPtr != pathObjPtr) {
	    Tcl_DecrRefCount(fsPathPtr->normPathPtr);
	}
	fsPathPtr->normPathPtr = NULL;
    }
    if (fsPathPtr->cwdPtr != NULL) {
	Tcl_DecrRefCount(fsPathPtr->cwdPtr);
    }
    if (fsPathPtr->nativePathPtr != NULL) {
	if (fsPathPtr->fsRecPtr != NULL) {
	    if (fsPathPtr->fsRecPtr->fsPtr->freeInternalRepProc != NULL) {
		(*fsPathPtr->fsRecPtr->fsPtr
		   ->freeInternalRepProc)(fsPathPtr->nativePathPtr);
		fsPathPtr->nativePathPtr = NULL;
	    }
	}
    }
    if (fsPathPtr->fsRecPtr != NULL) {
	fsPathPtr->fsRecPtr->fileRefCount--;
	if (fsPathPtr->fsRecPtr->fileRefCount <= 0) {
	    /* It has been unregistered already */
	    ckfree((char *)fsPathPtr->fsRecPtr);
	}
    }

    ckfree((char*) fsPathPtr);
}


static void
DupFsPathInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Path obj with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Path obj with internal rep to set. */
{
    register FsPath* srcFsPathPtr = (FsPath*) PATHOBJ(srcPtr);
    register FsPath* copyFsPathPtr = 
      (FsPath*) ckalloc((unsigned)sizeof(FsPath));
    Tcl_FSDupInternalRepProc *dupProc;
    
    PATHOBJ(copyPtr) = (VOID *) copyFsPathPtr;

    if (srcFsPathPtr->translatedPathPtr != NULL) {
	copyFsPathPtr->translatedPathPtr = srcFsPathPtr->translatedPathPtr;
	if (copyFsPathPtr->translatedPathPtr != copyPtr) {
	    Tcl_IncrRefCount(copyFsPathPtr->translatedPathPtr);
	}
    } else {
	copyFsPathPtr->translatedPathPtr = NULL;
    }
    
    if (srcFsPathPtr->normPathPtr != NULL) {
	copyFsPathPtr->normPathPtr = srcFsPathPtr->normPathPtr;
	if (copyFsPathPtr->normPathPtr != copyPtr) {
	    Tcl_IncrRefCount(copyFsPathPtr->normPathPtr);
	}
    } else {
	copyFsPathPtr->normPathPtr = NULL;
    }
    
    if (srcFsPathPtr->cwdPtr != NULL) {
	copyFsPathPtr->cwdPtr = srcFsPathPtr->cwdPtr;
	Tcl_IncrRefCount(copyFsPathPtr->cwdPtr);
    } else {
	copyFsPathPtr->cwdPtr = NULL;
    }

    copyFsPathPtr->flags = srcFsPathPtr->flags;
    
    if (srcFsPathPtr->fsRecPtr != NULL 
      && srcFsPathPtr->nativePathPtr != NULL) {
	dupProc = srcFsPathPtr->fsRecPtr->fsPtr->dupInternalRepProc;
	if (dupProc != NULL) {
	    copyFsPathPtr->nativePathPtr = 
	      (*dupProc)(srcFsPathPtr->nativePathPtr);
	} else {
	    copyFsPathPtr->nativePathPtr = NULL;
	}
    } else {
	copyFsPathPtr->nativePathPtr = NULL;
    }
    copyFsPathPtr->fsRecPtr = srcFsPathPtr->fsRecPtr;
    copyFsPathPtr->filesystemEpoch = srcFsPathPtr->filesystemEpoch;
    if (copyFsPathPtr->fsRecPtr != NULL) {
	copyFsPathPtr->fsRecPtr->fileRefCount++;
    }

    copyPtr->typePtr = &tclFsPathType;
}

/*
 *---------------------------------------------------------------------------
 *
 * UpdateStringOfFsPath --
 *
 *      Gives an object a valid string rep.
 *      
 * Results:
 *      None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *---------------------------------------------------------------------------
 */

static void
UpdateStringOfFsPath(objPtr)
    register Tcl_Obj *objPtr;	/* path obj with string rep to update. */
{
    register FsPath* fsPathPtr = (FsPath*) PATHOBJ(objPtr);
    CONST char *cwdStr;
    int cwdLen;
    Tcl_Obj *copy;
    
    if (PATHFLAGS(objPtr) == 0 || fsPathPtr->cwdPtr == NULL) {
	panic("Called UpdateStringOfFsPath with invalid object");
    }
    
    copy = Tcl_DuplicateObj(fsPathPtr->cwdPtr);
    Tcl_IncrRefCount(copy);
    
    cwdStr = Tcl_GetStringFromObj(copy, &cwdLen);
    /* 
     * Should we perhaps use 'Tcl_FSPathSeparator'?
     * But then what about the Windows special case?
     * Perhaps we should just check if cwd is a root volume.
     * We should never get cwdLen == 0 in this code path.
     */
    switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	    if (cwdStr[cwdLen-1] != '/') {
		Tcl_AppendToObj(copy, "/", 1);
		cwdLen++;
	    }
	    break;
	case TCL_PLATFORM_WINDOWS:
	    /* 
	     * We need the extra 'cwdLen != 2', and ':' checks because 
	     * a volume relative path doesn't get a '/'.  For example 
	     * 'glob C:*cat*.exe' will return 'C:cat32.exe'
	     */
	    if (cwdStr[cwdLen-1] != '/'
		    && cwdStr[cwdLen-1] != '\\') {
		if (cwdLen != 2 || cwdStr[1] != ':') {
		    Tcl_AppendToObj(copy, "/", 1);
		    cwdLen++;
		}
	    }
	    break;
	case TCL_PLATFORM_MAC:
	    if (cwdStr[cwdLen-1] != ':') {
		Tcl_AppendToObj(copy, ":", 1);
		cwdLen++;
	    }
	    break;
    }
    Tcl_AppendObjToObj(copy, fsPathPtr->normPathPtr);
    objPtr->bytes = Tcl_GetStringFromObj(copy, &cwdLen);
    objPtr->length = cwdLen;
    copy->bytes = tclEmptyStringRep;
    copy->length = 0;
    Tcl_DecrRefCount(copy);
}

/*
 *---------------------------------------------------------------------------
 *
 * NativePathInFilesystem --
 *
 *      Any path object is acceptable to the native filesystem, by
 *      default (we will throw errors when illegal paths are actually
 *      tried to be used).
 *      
 *      However, this behavior means the native filesystem must be
 *      the last filesystem in the lookup list (otherwise it will
 *      claim all files belong to it, and other filesystems will
 *      never get a look in).
 *
 * Results:
 *      TCL_OK, to indicate 'yes', -1 to indicate no.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
int 
NativePathInFilesystem(pathPtr, clientDataPtr)
    Tcl_Obj *pathPtr;
    ClientData *clientDataPtr;
{
    /* 
     * A special case is required to handle the empty path "". 
     * This is a valid path (i.e. the user should be able
     * to do 'file exists ""' without throwing an error), but
     * equally the path doesn't exist.  Those are the semantics
     * of Tcl (at present anyway), so we have to abide by them
     * here.
     */
    if (pathPtr->typePtr == &tclFsPathType) {
	if (pathPtr->bytes != NULL && pathPtr->bytes[0] == '\0') {
	    /* We reject the empty path "" */
	    return -1;
	}
	/* Otherwise there is no way this path can be empty */
    } else {
	/* 
	 * It is somewhat unusual to reach this code path without
	 * the object being of tclFsPathType.  However, we do
	 * our best to deal with the situation.
	 */
	int len;
	Tcl_GetStringFromObj(pathPtr,&len);
	if (len == 0) {
	    /* We reject the empty path "" */
	    return -1;
	}
    }
    /* 
     * Path is of correct type, or is of non-zero length, 
     * so we accept it.
     */
    return TCL_OK;
}
