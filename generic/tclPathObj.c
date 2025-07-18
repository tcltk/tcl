/*
 * tclPathObj.c --
 *
 *	This file contains the implementation of Tcl's "path" object type used
 *	to represent and manipulate a general (virtual) filesystem entity in
 *	an efficient manner.
 *
 * Copyright © 2003 Vince Darley.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclFileSystem.h"
#include <assert.h>

/*
 * Prototypes for functions defined later in this file.
 */

static Tcl_Obj *	AppendPath(Tcl_Obj *head, Tcl_Obj *tail);
static void		DupFsPathInternalRep(Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr);
static void		FreeFsPathInternalRep(Tcl_Obj *pathPtr);
static void		UpdateStringOfFsPath(Tcl_Obj *pathPtr);
static int		SetFsPathFromAny(Tcl_Interp *interp, Tcl_Obj *pathPtr);
static Tcl_Size		FindSplitPos(const char *path, int separator);
static int		IsSeparatorOrNull(int ch);
static Tcl_Obj *	GetExtension(Tcl_Obj *pathPtr);
static int		MakePathFromNormalized(Tcl_Interp *interp,
			    Tcl_Obj *pathPtr);
static int		MakeTildeRelativePath(Tcl_Interp *interp,
			    const char *user, const char *subPath,
			    Tcl_DString *dsPtr);

/*
 * Define the 'path' object type, which Tcl uses to represent file paths
 * internally.
 */

static const Tcl_ObjType fsPathType = {
    "path",			/* name */
    FreeFsPathInternalRep,	/* freeIntRepProc */
    DupFsPathInternalRep,	/* dupIntRepProc */
    UpdateStringOfFsPath,	/* updateStringProc */
    SetFsPathFromAny,		/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

/*
 * struct FsPath --
 *
 * Internal representation of a Tcl_Obj of fsPathType
 */

typedef struct {
    Tcl_Obj *translatedPathPtr;	/* If the path has been normalized (flags ==
				 * 0), this is NULL.  Otherwise it is a path
				 * in which any ~user sequences have been
				 * translated away. */
    Tcl_Obj *normPathPtr;	/* If the path has been normalized (flags ==
				 * 0), this is an absolute path without ., ..
				 * or ~user components.  Otherwise it is a
				 * path, possibly absolute, to normalize
				 * relative to cwdPtr. */
    Tcl_Obj *cwdPtr;		/* If NULL, either translatedPtr exists or
				 * normPathPtr exists and is absolute. */
    int flags;			/* Flags to describe interpretation - see
				 * below. */
    void *nativePathPtr;	/* Native representation of this path, which
				 * is filesystem dependent. */
    size_t filesystemEpoch;	/* Used to ensure the path representation was
				 * generated during the correct filesystem
				 * epoch. The epoch changes when
				 * filesystem-mounts are changed. */
    const Tcl_Filesystem *fsPtr;/* The Tcl_Filesystem that claims this path */
} FsPath;

/*
 * Flag values for FsPath->flags.
 */

#define TCLPATH_APPENDED 1
#define TCLPATH_NEEDNORM 4

/*
 * Define some macros to give us convenient access to path-object specific
 * fields.
 */

#define PATHOBJ(pathPtr) \
	((FsPath *) (TclFetchInternalRep((pathPtr), &fsPathType)->twoPtrValue.ptr1))
#define SETPATHOBJ(pathPtr,fsPathPtr) \
    do {								\
	Tcl_ObjInternalRep ir;						\
	ir.twoPtrValue.ptr1 = (void *) (fsPathPtr);			\
	ir.twoPtrValue.ptr2 = NULL;					\
	Tcl_StoreInternalRep((pathPtr), &fsPathType, &ir);		\
    } while (0)
#define PATHFLAGS(pathPtr) (PATHOBJ(pathPtr)->flags)

/*
 *---------------------------------------------------------------------------
 *
 * TclFSNormalizeAbsolutePath --
 *
 *	Takes an absolute path specification and computes a 'normalized' path
 *	from it.
 *
 *	A normalized path is one which has all '../', './' removed. Also it is
 *	one which is in the 'standard' format for the native platform. On
 *	Unix, this means the path must be free of symbolic links/aliases, and
 *	on Windows it means we want the long form, with that long form's
 *	case-dependence (which gives us a unique, case-dependent path).
 *
 *	The behaviour of this function if passed a non-absolute path is NOT
 *	defined.
 *
 *	pathPtr may have a refCount of zero, or may be a shared object.
 *
 * Results:
 *	The result is returned in a Tcl_Obj with a refCount already
 *	incremented, which gives the caller ownership of it.  The caller must
 *	arrange for Tcl_DecRefCount to be called when the object is no-longer
 *	needed.
 *
 * Side effects:
 *	None (beyond the memory allocation for the result).
 *
 * Special note:
 *	Originally based on code from Matt Newman and Jean-Claude Wippler.
 *	Totally rewritten later by Vince Darley to handle symbolic links.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclFSNormalizeAbsolutePath(
    Tcl_Interp *interp,		/* Interpreter to use */
    Tcl_Obj *pathPtr)		/* Absolute path to normalize */
{
    const char *dirSep, *oldDirSep;
    int first = 1;		/* Set to zero once we've passed the first
				 * directory separator - we can't use '..' to
				 * remove the volume in a path. */
    Tcl_Obj *retVal = NULL;
    int zipVolumeLen;
    dirSep = TclGetString(pathPtr);

    zipVolumeLen = TclIsZipfsPath(dirSep);
    if (zipVolumeLen) {
	/*
	 * NOTE: file normalization for zipfs is very specific to
	 * format of zipfs volume being of the form //xxx:/
	 */
	dirSep += zipVolumeLen-1; /* Start parse after : */
    } else if (tclPlatform == TCL_PLATFORM_WINDOWS) {
	if (   (dirSep[0] == '/' || dirSep[0] == '\\')
	    && (dirSep[1] == '/' || dirSep[1] == '\\')
	    && (dirSep[2] == '?')
	    && (dirSep[3] == '/' || dirSep[3] == '\\')) {
	    /* NT extended path */
	    dirSep += 4;

	    if (   (dirSep[0] == 'U' || dirSep[0] == 'u')
		&& (dirSep[1] == 'N' || dirSep[1] == 'n')
		&& (dirSep[2] == 'C' || dirSep[2] == 'c')
		&& (dirSep[3] == '/' || dirSep[3] == '\\')) {
		/* NT extended UNC path */
		dirSep += 4;
	    }
	}
	if (dirSep[0] != 0 && dirSep[1] == ':' &&
		(dirSep[2] == '/' || dirSep[2] == '\\')) {
	    /* Do nothing */
	} else if ((dirSep[0] == '/' || dirSep[0] == '\\')
		&& (dirSep[1] == '/' || dirSep[1] == '\\')) {
	    /*
	     * UNC style path, where we must skip over the first separator,
	     * since the first two segments are actually inseparable.
	     */

	    dirSep += 2;
	    dirSep += FindSplitPos(dirSep, '/');
	    if (*dirSep != 0) {
		dirSep++;
	    }
	}
    }

    /*
     * Scan forward from one directory separator to the next, checking for
     * '..' and '.' sequences which must be handled specially. In particular
     * handling of '..' can be complicated if the directory before is a link,
     * since we will have to expand the link to be able to back up one level.
     */

    while (*dirSep != 0) {
	oldDirSep = dirSep;
	if (!first) {
	    dirSep++;
	}
	dirSep += FindSplitPos(dirSep, '/');
	if (dirSep[0] == 0 || dirSep[1] == 0) {
	    if (retVal != NULL) {
		Tcl_AppendToObj(retVal, oldDirSep, dirSep - oldDirSep);
	    }
	    break;
	}
	if (dirSep[1] == '.') {
	    if (retVal != NULL) {
		Tcl_AppendToObj(retVal, oldDirSep, dirSep - oldDirSep);
		oldDirSep = dirSep;
	    }
	again:
	    if (IsSeparatorOrNull(dirSep[2])) {
		/*
		 * Need to skip '.' in the path.
		 */
		Tcl_Size curLen;

		if (retVal == NULL) {
		    const char *path = TclGetString(pathPtr);
		    retVal = Tcl_NewStringObj(path, dirSep - path);
		    Tcl_IncrRefCount(retVal);
		}
		(void)TclGetStringFromObj(retVal, &curLen);
		if (curLen == 0) {
		    Tcl_AppendToObj(retVal, dirSep, 1);
		}
		dirSep += 2;
		oldDirSep = dirSep;
		if (dirSep[0] != 0 && dirSep[1] == '.') {
		    goto again;
		}
		continue;
	    }
	    if (dirSep[2] == '.' && IsSeparatorOrNull(dirSep[3])) {
		Tcl_Obj *linkObj;
		Tcl_Size curLen;
		char *linkStr;

		/*
		 * Have '..' so need to skip previous directory.
		 */

		if (retVal == NULL) {
		    const char *path = TclGetString(pathPtr);

		    retVal = Tcl_NewStringObj(path, dirSep - path);
		    Tcl_IncrRefCount(retVal);
		}
		(void)TclGetStringFromObj(retVal, &curLen);
		if (curLen == 0) {
		    Tcl_AppendToObj(retVal, dirSep, 1);
		}
		if (!first || (tclPlatform == TCL_PLATFORM_UNIX)) {
		    if (zipVolumeLen) {
			linkObj = NULL;
		    } else {
			linkObj = Tcl_FSLink(retVal, NULL, 0);

			/* Safety check in case driver caused sharing */
			if (Tcl_IsShared(retVal)) {
			    TclDecrRefCount(retVal);
			    retVal = Tcl_DuplicateObj(retVal);
			    Tcl_IncrRefCount(retVal);
			}
		    }

		    if (linkObj != NULL) {
			/*
			 * Got a link. Need to check if the link is relative
			 * or absolute, for those platforms where relative
			 * links exist.
			 */

			if (tclPlatform != TCL_PLATFORM_WINDOWS
				&& Tcl_FSGetPathType(linkObj) == TCL_PATH_RELATIVE) {
			    /*
			     * We need to follow this link which is relative
			     * to retVal's directory. This means concatenating
			     * the link onto the directory of the path so far.
			     */

			    const char *path =
				    TclGetStringFromObj(retVal, &curLen);

			    while (curLen-- > 0) {
				if (IsSeparatorOrNull(path[curLen])) {
				    break;
				}
			    }

			    /*
			     * We want the trailing slash.
			     */

			    Tcl_SetObjLength(retVal, curLen+1);
			    Tcl_AppendObjToObj(retVal, linkObj);
			    TclDecrRefCount(linkObj);
			    linkStr = TclGetStringFromObj(retVal, &curLen);
			} else {
			    /*
			     * Absolute link.
			     */

			    TclDecrRefCount(retVal);
			    if (Tcl_IsShared(linkObj)) {
				retVal = Tcl_DuplicateObj(linkObj);
				TclDecrRefCount(linkObj);
			    } else {
				retVal = linkObj;
			    }
			    linkStr = TclGetStringFromObj(retVal, &curLen);

			    /*
			     * Convert to forward-slashes on windows.
			     */

			    if (tclPlatform == TCL_PLATFORM_WINDOWS) {
				Tcl_Size i;

				for (i = 0; i < curLen; i++) {
				    if (linkStr[i] == '\\') {
					linkStr[i] = '/';
				    }
				}
			    }
			}
		    } else {
			linkStr = TclGetStringFromObj(retVal, &curLen);
		    }

		    /*
		     * Either way, we now remove the last path element (but
		     * not the first character of the path). In the case of
		     * zipfs, make sure not to go beyond the zipfs volume.
		     */

		    int minLen = zipVolumeLen ? zipVolumeLen - 1 : 0;
		    while (--curLen >= minLen) {
			if (IsSeparatorOrNull(linkStr[curLen])) {
			    if (curLen) {
				Tcl_SetObjLength(retVal, curLen);
			    } else {
				Tcl_SetObjLength(retVal, 1);
			    }
			    break;
			}
		    }
		}
		dirSep += 3;
		oldDirSep = dirSep;

		if ((curLen == 0) && (dirSep[0] != 0)) {
		    Tcl_SetObjLength(retVal, 0);
		}

		if (dirSep[0] != 0 && dirSep[1] == '.') {
		    goto again;
		}
		continue;
	    }
	}
	first = 0;
	if (retVal != NULL) {
	    Tcl_AppendToObj(retVal, oldDirSep, dirSep - oldDirSep);
	}
    }

    /*
     * If we didn't make any changes, just use the input path.
     */

    if (retVal == NULL) {
	retVal = pathPtr;
	Tcl_IncrRefCount(retVal);

	if (Tcl_IsShared(retVal)) {
	    /*
	     * Unfortunately, the platform-specific normalization code which
	     * will be called below has no way of dealing with the case where
	     * an object is shared. It is expecting to modify an object in
	     * place. So, we must duplicate this here to ensure an object with
	     * a single ref-count.
	     *
	     * If that changes in the future (e.g. the normalize proc is given
	     * one object and is able to return a different one), then we
	     * could remove this code.
	     */

	    TclDecrRefCount(retVal);
	    retVal = Tcl_DuplicateObj(pathPtr);
	    Tcl_IncrRefCount(retVal);
	}
    }

    /*
     * Ensure a windows drive like C:/ has a trailing separator.
     * Likewise for zipfs volumes.
     */
    if (zipVolumeLen || (tclPlatform == TCL_PLATFORM_WINDOWS)) {
	int needTrailingSlash = 0;
	Tcl_Size len;
	const char *path = TclGetStringFromObj(retVal, &len);
	if (zipVolumeLen) {
	    if (len == (zipVolumeLen - 1)) {
		needTrailingSlash = 1;
	    }
	} else {
	    if (len == 2 && path[0] != 0 && path[1] == ':') {
		needTrailingSlash = 1;
	    }
	}
	if (needTrailingSlash) {
	    if (Tcl_IsShared(retVal)) {
		TclDecrRefCount(retVal);
		retVal = Tcl_DuplicateObj(retVal);
		Tcl_IncrRefCount(retVal);
	    }
	    Tcl_AppendToObj(retVal, "/", 1);
	}
    }

    /*
     * Now we have an absolute path, with no '..', '.' sequences, but it still
     * may not be in 'unique' form, depending on the platform. For instance,
     * Unix is case-sensitive, so the path is ok. Windows is case-insensitive,
     * and also has the weird 'longname/shortname' thing (e.g. C:/Program
     * Files/ and C:/Progra~1/ are equivalent).
     *
     * Virtual file systems which may be registered may have other criteria
     * for normalizing a path.
     */

    TclFSNormalizeToUniquePath(interp, retVal, 0);

    /*
     * Since we know it is a normalized path, we can actually convert this
     * object into an FsPath for greater efficiency
     */

    MakePathFromNormalized(interp, retVal);

    /*
     * This has a refCount of 1 for the caller, unlike many Tcl_Obj APIs.
     */

    return retVal;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSGetPathType --
 *
 *	Determines whether a given path is relative to the current directory,
 *	relative to the current volume, or absolute.
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
Tcl_FSGetPathType(
    Tcl_Obj *pathPtr)
{
    return TclFSGetPathType(pathPtr, NULL, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclFSGetPathType --
 *
 *	Determines whether a given path is relative to the current directory,
 *	relative to the current volume, or absolute. If the caller wishes to
 *	know which filesystem claimed the path (in the case for which the path
 *	is absolute), then a reference to a filesystem pointer can be passed
 *	in (but passing NULL is acceptable).
 *
 * Results:
 *	Returns one of TCL_PATH_ABSOLUTE, TCL_PATH_RELATIVE, or
 *	TCL_PATH_VOLUME_RELATIVE. The filesystem reference will be set if and
 *	only if it is non-NULL and the function's return value is
 *	TCL_PATH_ABSOLUTE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_PathType
TclFSGetPathType(
    Tcl_Obj *pathPtr,
    const Tcl_Filesystem **filesystemPtrPtr,
    Tcl_Size *driveNameLengthPtr)
{
    FsPath *fsPathPtr;

    if (Tcl_FSConvertToPathType(NULL, pathPtr) != TCL_OK) {
	return TclGetPathType(pathPtr, filesystemPtrPtr, driveNameLengthPtr,
		NULL);
    }

    fsPathPtr = PATHOBJ(pathPtr);
    if (fsPathPtr->cwdPtr == NULL) {
	return TclGetPathType(pathPtr, filesystemPtrPtr, driveNameLengthPtr,
		NULL);
    }

    if (PATHFLAGS(pathPtr) == 0) {
	/* The path is not absolute... */
#ifdef _WIN32
	/* ... on Windows we must make another call to determine whether
	 * it's relative or volumerelative [Bug 2571597]. */
	return TclGetPathType(pathPtr, filesystemPtrPtr, driveNameLengthPtr,
		NULL);
#else
	/* On other systems, quickly deduce !absolute -> relative */
	return TCL_PATH_RELATIVE;
#endif
    }
    return TclFSGetPathType(fsPathPtr->cwdPtr, filesystemPtrPtr,
	    driveNameLengthPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclPathPart
 *
 *	This function calculates the requested part of the given path, which
 *	can be:
 *
 *	- the directory above ('file dirname')
 *	- the tail            ('file tail')
 *	- the extension       ('file extension')
 *	- the root            ('file root')
 *
 *	The 'portion' parameter dictates which of these to calculate. There
 *	are a number of special cases both to be more efficient, and because
 *	the behaviour when given a path with only a single element is defined
 *	to require the expansion of that single element, where possible.
 *
 *	Should look into integrating 'FileBasename' in tclFCmd.c into this
 *	function.
 *
 * Results:
 *	NULL if an error occurred, otherwise a Tcl_Obj owned by the caller
 *	(i.e. most likely with refCount 1).
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclPathPart(
    TCL_UNUSED(Tcl_Interp *),	/* Used for error reporting */
    Tcl_Obj *pathPtr,		/* Path to take dirname of */
    Tcl_PathPart portion)	/* Requested portion of name */
{
    if (TclHasInternalRep(pathPtr, &fsPathType)) {
	FsPath *fsPathPtr = PATHOBJ(pathPtr);

	if (PATHFLAGS(pathPtr) != 0) {
	    switch (portion) {
	    case TCL_PATH_DIRNAME: {
		/*
		 * Check if the joined-on bit has any directory delimiters in
		 * it. If so, the 'dirname' would be a joining of the main
		 * part with the dirname of the joined-on bit. We could handle
		 * that special case here, but we don't, and instead just use
		 * the standardPath code.
		 */

		Tcl_Size numBytes;
		const char *rest = TclGetStringFromObj(fsPathPtr->normPathPtr, &numBytes);

		if (strchr(rest, '/') != NULL) {
		    goto standardPath;
		}
		/*
		 * If the joined-on bit is empty, then [file dirname] is
		 * documented to return all but the last non-empty element
		 * of the path, so we need to split apart the main part to
		 * get the right answer.  We could do that here, but it's
		 * simpler to fall back to the standardPath code.
		 * [Bug 2710920]
		 */
		if (numBytes == 0) {
		    goto standardPath;
		}
		if (tclPlatform == TCL_PLATFORM_WINDOWS
			&& strchr(rest, '\\') != NULL) {
		    goto standardPath;
		}

		/*
		 * The joined-on path is simple, so we can just return here.
		 */

		Tcl_IncrRefCount(fsPathPtr->cwdPtr);
		return fsPathPtr->cwdPtr;
	    }
	    case TCL_PATH_TAIL: {
		/*
		 * Check if the joined-on bit has any directory delimiters in
		 * it. If so, the 'tail' would be only the part following the
		 * last delimiter. We could handle that special case here, but
		 * we don't, and instead just use the standardPath code.
		 */

		Tcl_Size numBytes;
		const char *rest = TclGetStringFromObj(fsPathPtr->normPathPtr, &numBytes);

		if (strchr(rest, '/') != NULL) {
		    goto standardPath;
		}
		/*
		 * If the joined-on bit is empty, then [file tail] is
		 * documented to return the last non-empty element
		 * of the path, so we need to split off the last element
		 * of the main part to get the right answer.  We could do
		 * that here, but it's simpler to fall back to the
		 * standardPath code.  [Bug 2710920]
		 */
		if (numBytes == 0) {
		    goto standardPath;
		}
		if (tclPlatform == TCL_PLATFORM_WINDOWS
			&& strchr(rest, '\\') != NULL) {
		    goto standardPath;
		}
		Tcl_IncrRefCount(fsPathPtr->normPathPtr);
		return fsPathPtr->normPathPtr;
	    }
	    case TCL_PATH_EXTENSION:
		return GetExtension(fsPathPtr->normPathPtr);
	    case TCL_PATH_ROOT: {
		const char *fileName, *extension;
		Tcl_Size length;

		fileName = TclGetStringFromObj(fsPathPtr->normPathPtr,
			&length);
		extension = TclGetExtension(fileName);
		if (extension == NULL) {
		    /*
		     * There is no extension so the root is the same as the
		     * path we were given.
		     */

		    Tcl_IncrRefCount(pathPtr);
		    return pathPtr;
		} else {
		    /*
		     * Need to return the whole path with the extension
		     * suffix removed.  Do that by joining our "head" to
		     * our "tail" with the extension suffix removed from
		     * the tail.
		     */

		    Tcl_Obj *resultPtr =
			    TclNewFSPathObj(fsPathPtr->cwdPtr, fileName,
			    length - strlen(extension));

		    Tcl_IncrRefCount(resultPtr);
		    return resultPtr;
		}
	    }
	    default:
		TCL_UNREACHABLE();
	    }
	} else if (fsPathPtr->cwdPtr != NULL) {
	    /* Relative path */
	    goto standardPath;
	} else {
	    /* Absolute path */
	    goto standardPath;
	}
    } else {
	Tcl_Size splitElements;
	Tcl_Obj *splitPtr, *resultPtr;

    standardPath:
	resultPtr = NULL;
	if (portion == TCL_PATH_EXTENSION) {
	    return GetExtension(pathPtr);
	} else if (portion == TCL_PATH_ROOT) {
	    Tcl_Size length;
	    const char *fileName, *extension;

	    fileName = TclGetStringFromObj(pathPtr, &length);
	    extension = TclGetExtension(fileName);
	    if (extension == NULL) {
		Tcl_IncrRefCount(pathPtr);
		return pathPtr;
	    } else {
		Tcl_Obj *root = Tcl_NewStringObj(fileName,
			length - strlen(extension));

		Tcl_IncrRefCount(root);
		return root;
	    }
	}

	/*
	 * Tcl_FSSplitPath in the handling of home directories;
	 * Tcl_FSSplitPath preserves the "~",  but this code computes the
	 * actual full path name, if we had just a single component.
	 */

	splitPtr = Tcl_FSSplitPath(pathPtr, &splitElements);
	Tcl_IncrRefCount(splitPtr);

	if (portion == TCL_PATH_TAIL) {
	    /*
	     * Return the last component, unless it is the only component, and
	     * it is the root of an absolute path.
	     */

	    if ((splitElements > 0) && ((splitElements > 1) ||
		    (Tcl_FSGetPathType(pathPtr) == TCL_PATH_RELATIVE))) {
		Tcl_ListObjIndex(NULL, splitPtr, splitElements-1, &resultPtr);
	    } else {
		TclNewObj(resultPtr);
	    }
	} else {
	    /*
	     * Return all but the last component. If there is only one
	     * component, return it if the path was non-relative, otherwise
	     * return the current directory.
	     */

	    if (splitElements > 1) {
		resultPtr = Tcl_FSJoinPath(splitPtr, splitElements - 1);
	    } else if (splitElements == 0 ||
		    (Tcl_FSGetPathType(pathPtr) == TCL_PATH_RELATIVE)) {
		TclNewLiteralStringObj(resultPtr, ".");
	    } else {
		Tcl_ListObjIndex(NULL, splitPtr, 0, &resultPtr);
	    }
	}
	Tcl_IncrRefCount(resultPtr);
	TclDecrRefCount(splitPtr);
	return resultPtr;
    }
}

/*
 * Simple helper function
 */

static Tcl_Obj *
GetExtension(
    Tcl_Obj *pathPtr)
{
    const char *tail, *extension;
    Tcl_Obj *ret;

    tail = TclGetString(pathPtr);
    extension = TclGetExtension(tail);
    if (extension == NULL) {
	TclNewObj(ret);
    } else {
	ret = Tcl_NewStringObj(extension, -1);
    }
    Tcl_IncrRefCount(ret);
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSJoinPath --
 *
 *	This function takes the given Tcl_Obj, which should be a valid list,
 *	and returns the path object given by considering the first 'elements'
 *	elements as valid path segments (each path segment may be a complete
 *	path, a partial path or just a single possible directory or file
 *	name). If any path segment is actually an absolute path, then all
 *	prior path segments are discarded.
 *
 *	If elements < 0, we use the entire list that was given.
 *
 *	It is possible that the returned object is actually an element of the
 *	given list, so the caller should be careful to store a refCount to it
 *	before freeing the list.
 *
 * Results:
 *	Returns object with refCount of zero, (or if non-zero, it has
 *	references elsewhere in Tcl). Either way, the caller must increment
 *	its refCount before use. Note that in the case where the caller has
 *	asked to join zero elements of the list, the return value will be an
 *	empty-string Tcl_Obj.
 *
 *	If the given listObj was invalid, then the calling routine has a bug,
 *	and this function will just return NULL.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSJoinPath(
    Tcl_Obj *listObj,		/* Path elements to join, may have a zero
				 * reference count. */
    Tcl_Size elements)		/* Number of elements to use (-1 = all) */
{
    Tcl_Obj *res;
    Tcl_Size objc;
    Tcl_Obj **objv;

    if (TclListObjLength(NULL, listObj, &objc) != TCL_OK) {
	return NULL;
    }

    elements = ((elements >= 0) && (elements <= objc)) ? elements : objc;
    TclListObjGetElements(NULL, listObj, &objc, &objv);
    res = TclJoinPath(elements, objv, 0);
    return res;
}

Tcl_Obj *
TclJoinPath(
    Tcl_Size elements,		/* Number of elements to use */
    Tcl_Obj * const objv[],	/* Path elements to join */
    int forceRelative)		/* If non-zero, assume all more paths are
				 * relative (e.g. simple normalization) */
{
    Tcl_Obj *res = NULL;
    Tcl_Size i;
    const Tcl_Filesystem *fsPtr = NULL;

    if (elements == 0) {
	TclNewObj(res);
	return res;
    }

    assert ( elements > 0 );

    if (elements == 2) {
	Tcl_Obj *elt = objv[0];
	Tcl_ObjInternalRep *eltIr = TclFetchInternalRep(elt, &fsPathType);

	/*
	 * This is a special case where we can be much more efficient, where
	 * we are joining a single relative path onto an object that is
	 * already of path type. The 'TclNewFSPathObj' call below creates an
	 * object which can be normalized more efficiently. Currently we only
	 * use the special case when we have exactly two elements, but we
	 * could expand that in the future.
	 *
	 * Bugfix [a47641a0]. TclNewFSPathObj requires first argument
	 * to be an absolute path. Added a check to ensure that elt is absolute.
	 */

	if ((eltIr)
		&& !((elt->bytes != NULL) && (elt->bytes[0] == '\0'))
		&& TclGetPathType(elt, NULL, NULL, NULL) == TCL_PATH_ABSOLUTE) {
	    Tcl_Obj *tailObj = objv[1];
	    Tcl_PathType type;

	    /* if forceRelative - second path is relative */
	    type = forceRelative ? TCL_PATH_RELATIVE :
		    TclGetPathType(tailObj, NULL, NULL, NULL);
	    if (type == TCL_PATH_RELATIVE) {
		const char *str;
		Tcl_Size len;

		str = TclGetStringFromObj(tailObj, &len);
		if (len == 0) {
		    /*
		     * This happens if we try to handle the root volume '/'.
		     * There's no need to return a special path object, when
		     * the base itself is just fine!
		     */

		    return elt;
		}

		/*
		 * If it doesn't begin with '.' and is a Unix path or it a
		 * windows path without backslashes, then we can be very
		 * efficient here. (In fact even a windows path with
		 * backslashes can be joined efficiently, but the path object
		 * would not have forward slashes only, and this would
		 * therefore contradict our 'file join' documentation).
		 */

		if (str[0] != '.' && ((tclPlatform != TCL_PLATFORM_WINDOWS)
			|| (strchr(str, '\\') == NULL))) {
		    /*
		     * Finally, on Windows, 'file join' is defined to convert
		     * all backslashes to forward slashes, so the base part
		     * cannot have backslashes either.
		     */

		    if ((tclPlatform != TCL_PLATFORM_WINDOWS)
			    || (strchr(TclGetString(elt), '\\') == NULL)) {

			if (PATHFLAGS(elt)) {
			    return TclNewFSPathObj(elt, str, len);
			}
			if (TCL_PATH_ABSOLUTE != Tcl_FSGetPathType(elt)) {
			    return TclNewFSPathObj(elt, str, len);
			}
			(void) Tcl_FSGetNormalizedPath(NULL, elt);
			if (elt == PATHOBJ(elt)->normPathPtr) {
			    return TclNewFSPathObj(elt, str, len);
			}
		    }
		}

		/*
		 * Otherwise we don't have an easy join, and we must let the
		 * more general code below handle things.
		 */
	    } else if (tclPlatform == TCL_PLATFORM_UNIX) {
		return tailObj;
	    } else {
		const char *str = TclGetString(tailObj);

		if (tclPlatform == TCL_PLATFORM_WINDOWS) {
		    if (strchr(str, '\\') == NULL) {
			return tailObj;
		    }
		}
	    }
	}
    }

    assert ( res == NULL );

    for (i = 0; i < elements; i++) {
	Tcl_Size driveNameLength;
	Tcl_Size strEltLen, length;
	Tcl_PathType type;
	char *strElt, *ptr;
	Tcl_Obj *driveName = NULL;
	Tcl_Obj *elt = objv[i];

	strElt = TclGetStringFromObj(elt, &strEltLen);
	driveNameLength = 0;
	/* if forceRelative - all paths excepting first one are relative */
	type = (forceRelative && (i > 0)) ? TCL_PATH_RELATIVE :
		TclGetPathType(elt, &fsPtr, &driveNameLength, &driveName);
	if (type != TCL_PATH_RELATIVE) {
	    /*
	     * Zero out the current result.
	     */

	    if (res != NULL) {
		TclDecrRefCount(res);
	    }

	    if (driveName != NULL) {
		/*
		 * We've been given a separate drive-name object, because the
		 * prefix in 'elt' is not in a suitable format for us (e.g. it
		 * may contain irrelevant multiple separators, like
		 * C://///foo).
		 */

		res = Tcl_DuplicateObj(driveName);
		TclDecrRefCount(driveName);

		/*
		 * Do not set driveName to NULL, because we will check its
		 * value below (but we won't access the contents, since those
		 * have been cleaned-up).
		 */
	    } else {
		res = Tcl_NewStringObj(strElt, driveNameLength);
	    }
	    strElt += driveNameLength;
	} else if (driveName != NULL) {
	    Tcl_DecrRefCount(driveName);
	}

	/*
	 * Optimisation block: if this is the last element to be examined, and
	 * it is absolute or the only element, and the drive-prefix was ok (if
	 * there is one), it might be that the path is already in a suitable
	 * form to be returned. Then we can short-cut the rest of this
	 * function.
	 */

	if ((driveName == NULL) && (i == (elements - 1))
		&& (type != TCL_PATH_RELATIVE || res == NULL)) {
	    /*
	     * It's the last path segment. Perform a quick check if the path
	     * is already in a suitable form.
	     */

	    if (tclPlatform == TCL_PLATFORM_WINDOWS) {
		if (strchr(strElt, '\\') != NULL) {
		    goto noQuickReturn;
		}
	    }
	    ptr = strElt;
	    /* [Bug f34cf83dd0] */
	    if (driveNameLength > 0) {
		if (ptr[0] == '/' && ptr[-1] == '/') {
		    goto noQuickReturn;
		}
	    }
	    while (*ptr != '\0') {
		if (*ptr == '/' && (ptr[1] == '/' || ptr[1] == '\0')) {
		    /*
		     * We have a repeated file separator, which means the path
		     * is not in normalized form
		     */

		    goto noQuickReturn;
		}
		ptr++;
	    }
	    if (res != NULL) {
		TclDecrRefCount(res);
	    }

	    /*
	     * This element is just what we want to return already; no further
	     * manipulation is requred.
	     */

	    return elt;
	}

	/*
	 * The path element was not of a suitable form to be returned as is.
	 * We need to perform a more complex operation here.
	 */

    noQuickReturn:
	if (res == NULL) {
	    TclNewObj(res);
	}
	ptr = TclGetStringFromObj(res, &length);

	/*
	 * A NULL value for fsPtr at this stage basically means we're trying
	 * to join a relative path onto something which is also relative (or
	 * empty). There's nothing particularly wrong with that.
	 */

	if (*strElt == '\0') {
	    continue;
	}

	if (fsPtr == &tclNativeFilesystem || fsPtr == NULL) {
	    TclpNativeJoinPath(res, strElt);
	} else {
	    char separator = '/';
	    int needsSep = 0;

	    if (fsPtr->filesystemSeparatorProc != NULL) {
		Tcl_Obj *sep = fsPtr->filesystemSeparatorProc(res);

		if (sep != NULL) {
		    separator = TclGetString(sep)[0];
		    TclDecrRefCount(sep);
		}
		/* Safety check in case the VFS driver caused sharing */
		if (Tcl_IsShared(res)) {
		    TclDecrRefCount(res);
		    res = Tcl_DuplicateObj(res);
		    Tcl_IncrRefCount(res);
		}
	    }

	    if (length > 0 && ptr[length -1] != '/') {
		Tcl_AppendToObj(res, &separator, 1);
		(void)TclGetStringFromObj(res, &length);
	    }
	    Tcl_SetObjLength(res, length + strlen(strElt));

	    ptr = TclGetString(res) + length;
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
	    length = ptr - TclGetString(res);
	    Tcl_SetObjLength(res, length);
	}
    }
    assert ( res != NULL );
    return res;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSConvertToPathType --
 *
 *	This function tries to convert the given Tcl_Obj to a valid Tcl path
 *	type, taking account of the fact that the cwd may have changed even if
 *	this object is already supposedly of the correct type.
 *
 *	The filename may begin with "~" (to indicate current user's home
 *	directory) or "~<user>" (to indicate any user's home directory).
 *
 * Results:
 *	Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSConvertToPathType(
    Tcl_Interp *interp,		/* Interpreter in which to store error message
				 * (if necessary). */
    Tcl_Obj *pathPtr)		/* Object to convert to a valid, current path
				 * type. */
{
    /*
     * While it is bad practice to examine an object's type directly, this is
     * actually the best thing to do here. The reason is that if we are
     * converting this object to FsPath type for the first time, we don't need
     * to worry whether the 'cwd' has changed. On the other hand, if this
     * object is already of FsPath type, and is a relative path, we do have to
     * worry about the cwd. If the cwd has changed, we must recompute the
     * path.
     */

    if (TclHasInternalRep(pathPtr, &fsPathType)) {
	if (TclFSEpochOk(PATHOBJ(pathPtr)->filesystemEpoch)) {
	    return TCL_OK;
	}

	TclGetString(pathPtr);
	Tcl_StoreInternalRep(pathPtr, &fsPathType, NULL);
    }

    return SetFsPathFromAny(interp, pathPtr);
}

/*
 * Helper function for normalization.
 */

static int
IsSeparatorOrNull(
    int ch)
{
    if (ch == 0) {
	return 1;
    }
    switch (tclPlatform) {
    case TCL_PLATFORM_UNIX:
	return (ch == '/' ? 1 : 0);
    case TCL_PLATFORM_WINDOWS:
	return ((ch == '/' || ch == '\\') ? 1 : 0);
    }
    return 0;
}

/*
 * Helper function for SetFsPathFromAny. Returns position of first directory
 * delimiter in the path. If no separator is found, then returns the position
 * of the end of the string.
 */

static Tcl_Size
FindSplitPos(
    const char *path,
    int separator)
{
    int count = 0;
    switch (tclPlatform) {
    case TCL_PLATFORM_UNIX:
	while (path[count] != 0) {
	    if (path[count] == separator) {
		return count;
	    }
	    count++;
	}
	break;

    case TCL_PLATFORM_WINDOWS:
	while (path[count] != 0) {
	    if (path[count] == separator || path[count] == '\\') {
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
 *	Creates a path object whose string representation is '[file join
 *	dirPtr addStrRep]', but does so in a way that allows for more
 *	efficient creation and caching of normalized paths, and more efficient
 *	'file dirname', 'file tail', etc.
 *
 * Assumptions:
 *	'dirPtr' must be an absolute path. 'len' may not be zero.
 *
 * Results:
 *	The new Tcl object, with refCount zero.
 *
 * Side effects:
 *	Memory is allocated. 'dirPtr' gets an additional refCount.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclNewFSPathObj(
    Tcl_Obj *dirPtr,
    const char *addStrRep,
    Tcl_Size len)
{
    FsPath *fsPathPtr;
    Tcl_Obj *pathPtr;
    const char *p;
    int state = 0, count = 0;

    /*
     * This comment is kept from the days of tilde expansion because
     * it is illustrative of a more general problem.
     * [Bug 2806250] - this is only a partial solution of the problem.
     * The PATHFLAGS != 0 representation assumes in many places that
     * the "tail" part stored in the normPathPtr field is itself a
     * relative path.  Strings that begin with "~" are not relative paths,
     * so we must prevent their storage in the normPathPtr field.
     *
     * More generally we ought to be testing "addStrRep" for any value
     * that is not a relative path, but in an unconstrained VFS world
     * that could be just about anything, and testing could be expensive.
     * Since this routine plays a big role in [glob], anything that slows
     * it down would be unwelcome.  For now, continue the risk of further
     * bugs when some Tcl_Filesystem uses otherwise relative path strings
     * as absolute path strings.  Sensible Tcl_Filesystems will avoid
     * that by mounting on path prefixes like foo:// which cannot be the
     * name of a file or directory read from a native [glob] operation.
     */

    TclNewObj(pathPtr);
    fsPathPtr = (FsPath *)Tcl_Alloc(sizeof(FsPath));

    /*
     * Set up the path.
     */

    fsPathPtr->translatedPathPtr = NULL;
    fsPathPtr->normPathPtr = Tcl_NewStringObj(addStrRep, len);
    Tcl_IncrRefCount(fsPathPtr->normPathPtr);
    fsPathPtr->cwdPtr = dirPtr;
    Tcl_IncrRefCount(dirPtr);
    fsPathPtr->nativePathPtr = NULL;
    fsPathPtr->fsPtr = NULL;
    fsPathPtr->filesystemEpoch = 0;

    SETPATHOBJ(pathPtr, fsPathPtr);
    PATHFLAGS(pathPtr) = TCLPATH_APPENDED;
    TclInvalidateStringRep(pathPtr);

    /*
     * Look for path components made up of only "."
     * This is overly conservative analysis to keep simple. It may mark some
     * things as needing more aggressive normalization that don't actually
     * need it. No harm done.
     */
    for (p = addStrRep; len > 0; p++, len--) {
	switch (state) {
	case 0:		/* So far only "." since last dirsep or start */
	    switch (*p) {
	    case '.':
		count = 1;
		break;
	    case '/':
	    case '\\':
	    case ':':
		if (count) {
		    PATHFLAGS(pathPtr) |= TCLPATH_NEEDNORM;
		    len = 0;
		}
		break;
	    default:
		count = 0;
		state = 1;
	    }
	    break;
	case 1:		/* Scanning for next dirsep */
	    switch (*p) {
	    case '/':
	    case '\\':
	    case ':':
		state = 0;
		break;
	    }
	}
    }
    if (len == 0 && count) {
	PATHFLAGS(pathPtr) |= TCLPATH_NEEDNORM;
    }

    return pathPtr;
}

static Tcl_Obj *
AppendPath(
    Tcl_Obj *head,
    Tcl_Obj *tail)
{
    const char *bytes;
    Tcl_Obj *copy = Tcl_DuplicateObj(head);
    Tcl_Size length;

    /*
     * This is likely buggy when dealing with virtual filesystem drivers
     * that use some character other than "/" as a path separator.  I know
     * of no evidence that such a foolish thing exists.  This solution was
     * chosen so that "JoinPath" operations that pass through either path
     * internalrep produce the same results; that is, bugward compatibility.  If
     * we need to fix that bug here, it needs fixing in TclJoinPath() too.
     */
    bytes = TclGetStringFromObj(tail, &length);
    if (length == 0) {
	Tcl_AppendToObj(copy, "/", 1);
    } else {
	TclpNativeJoinPath(copy, bytes);
    }
    return copy;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSMakePathRelative --
 *
 *	Only for internal use.
 *
 *	Takes a path and a directory, where we _assume_ both path and
 *	directory are absolute, normalized and that the path lies inside the
 *	directory. Returns a Tcl_Obj representing filename of the path
 *	relative to the directory.
 *
 * Results:
 *	NULL on error, otherwise a valid object, typically with refCount of
 *	zero, which it is assumed the caller will increment.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclFSMakePathRelative(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *pathPtr,		/* The path we have. */
    Tcl_Obj *cwdPtr)		/* Make it relative to this. */
{
    Tcl_Size cwdLen, len;
    const char *tempStr;
    Tcl_ObjInternalRep *irPtr = TclFetchInternalRep(pathPtr, &fsPathType);

    if (irPtr) {
	FsPath *fsPathPtr = PATHOBJ(pathPtr);

	if (PATHFLAGS(pathPtr) != 0 && fsPathPtr->cwdPtr == cwdPtr) {
	    return fsPathPtr->normPathPtr;
	}
    }

    /*
     * We know the cwd is a normalised object which does not end in a
     * directory delimiter, unless the cwd is the name of a volume, in which
     * case it will end in a delimiter! We handle this situation here. A
     * better test than the '!= sep' might be to simply check if 'cwd' is a
     * root volume.
     *
     * Note that if we get this wrong, we will strip off either too much or
     * too little below, leading to wrong answers returned by glob.
     */

    tempStr = TclGetStringFromObj(cwdPtr, &cwdLen);

    /*
     * Should we perhaps use 'Tcl_FSPathSeparator'? But then what about the
     * Windows special case? Perhaps we should just check if cwd is a root
     * volume.
     */

    switch (tclPlatform) {
    case TCL_PLATFORM_UNIX:
	if (tempStr[cwdLen-1] != '/') {
	    cwdLen++;
	}
	break;
    case TCL_PLATFORM_WINDOWS:
	if (tempStr[cwdLen-1] != '/' && tempStr[cwdLen-1] != '\\') {
	    cwdLen++;
	}
	break;
    }
    tempStr = TclGetStringFromObj(pathPtr, &len);

    return Tcl_NewStringObj(tempStr + cwdLen, len - cwdLen);
}

/*
 *---------------------------------------------------------------------------
 *
 * MakePathFromNormalized --
 *
 *	Like SetFsPathFromAny, but assumes the given object is an absolute
 *	normalized path. Only for internal use.
 *
 * Results:
 *	Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

static int
MakePathFromNormalized(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *pathPtr)		/* The object to convert. */
{
    FsPath *fsPathPtr;

    if (TclHasInternalRep(pathPtr, &fsPathType)) {
	return TCL_OK;
    }

    fsPathPtr = (FsPath *)Tcl_Alloc(sizeof(FsPath));

    /*
     * It's a pure normalized absolute path.
     */

    fsPathPtr->translatedPathPtr = NULL;

    Tcl_IncrRefCount(fsPathPtr->normPathPtr = Tcl_DuplicateObj(pathPtr));
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = NULL;
    fsPathPtr->fsPtr = NULL;
    /* Remember the epoch under which we decided pathPtr was normalized */
    fsPathPtr->filesystemEpoch = TclFSEpoch();

    SETPATHOBJ(pathPtr, fsPathPtr);
    PATHFLAGS(pathPtr) = 0;

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSNewNativePath --
 *
 *	Performs the something like the reverse of the usual
 *	obj->path->nativerep conversions. If some code retrieves a path in
 *	native form (from, e.g. readlink or a native dialog), and that path is
 *	to be used at the Tcl level, then calling this function is an
 *	efficient way of creating the appropriate path object type.
 *
 *	Any memory which is allocated for 'clientData' should be retained
 *	until clientData is passed to the filesystem's freeInternalRepProc
 *	when it can be freed. The built in platform-specific filesystems use
 *	'Tcl_Alloc' to allocate clientData, and Tcl_Free to free it.
 *
 * Results:
 *	NULL or a valid path object pointer, with refCount zero.
 *
 * Side effects:
 *	New memory may be allocated.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSNewNativePath(
    const Tcl_Filesystem *fromFilesystem,
    void *clientData)
{
    Tcl_Obj *pathPtr = NULL;
    FsPath *fsPathPtr;

    if (fromFilesystem->internalToNormalizedProc != NULL) {
	pathPtr = (*fromFilesystem->internalToNormalizedProc)(clientData);
    }
    if (pathPtr == NULL) {
	return NULL;
    }

    /*
     * Free old representation; shouldn't normally be any, but best to be
     * safe.
     */

    Tcl_StoreInternalRep(pathPtr, &fsPathType, NULL);
    fsPathPtr = (FsPath *)Tcl_Alloc(sizeof(FsPath));

    fsPathPtr->translatedPathPtr = NULL;

    Tcl_IncrRefCount(fsPathPtr->normPathPtr = Tcl_DuplicateObj(pathPtr));
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = clientData;
    fsPathPtr->fsPtr = fromFilesystem;
    fsPathPtr->filesystemEpoch = TclFSEpoch();

    SETPATHOBJ(pathPtr, fsPathPtr);
    PATHFLAGS(pathPtr) = 0;

    return pathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetTranslatedPath --
 *
 *	Attempts to extract the translated path from the given
 *	Tcl_Obj. If the translation succeeds (i.e. the object is a valid
 *	path), then it is returned. Otherwise NULL is returned and an
 *	error message may be left in the interpreter if it is not NULL.
 *
 * Results:
 *	A Tcl_Obj pointer or NULL.
 *
 * Side effects:
 *	pathPtr is converted to fsPathType if necessary.
 *
 *	FsPath members are modified as needed.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSGetTranslatedPath(
    Tcl_Interp *interp,
    Tcl_Obj *pathPtr)
{
    Tcl_Obj *retObj = NULL;
    FsPath *srcFsPathPtr;

    if (Tcl_FSConvertToPathType(interp, pathPtr) != TCL_OK) {
	return NULL;
    }
    srcFsPathPtr = PATHOBJ(pathPtr);
    if (srcFsPathPtr->translatedPathPtr == NULL) {
	if (PATHFLAGS(pathPtr) == 0) {
	    /*
	     * Path is already normalized
	     */
	    retObj = srcFsPathPtr->normPathPtr;
	} else {
	    /*
	     * We lack a translated path result, but we have a directory
	     * (cwdPtr) and a tail (normPathPtr), and if we join the
	     * translated version of cwdPtr to normPathPtr, we'll get the
	     * translated result we need, and can store it for future use.
	     */

	    Tcl_Obj *translatedCwdPtr = Tcl_FSGetTranslatedPath(interp,
		    srcFsPathPtr->cwdPtr);
	    Tcl_ObjInternalRep *translatedCwdIrPtr;

	    if (translatedCwdPtr == NULL) {
		return NULL;
	    }

	    retObj = Tcl_FSJoinToPath(translatedCwdPtr, 1,
		    &srcFsPathPtr->normPathPtr);
	    Tcl_IncrRefCount(srcFsPathPtr->translatedPathPtr = retObj);
	    translatedCwdIrPtr = TclFetchInternalRep(translatedCwdPtr, &fsPathType);
	    if (translatedCwdIrPtr) {
		srcFsPathPtr->filesystemEpoch
			= PATHOBJ(translatedCwdPtr)->filesystemEpoch;
	    } else {
		srcFsPathPtr->filesystemEpoch = 0;
	    }
	    Tcl_DecrRefCount(translatedCwdPtr);
	}
    } else {
	/*
	 * It is an ordinary path object.
	 */

	retObj = srcFsPathPtr->translatedPathPtr;
    }

    if (retObj != NULL) {
	Tcl_IncrRefCount(retObj);
    }
    return retObj;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetTranslatedStringPath --
 *
 *	This function attempts to extract the translated path from the given
 *	Tcl_Obj. If the translation succeeds (i.e. the object is a valid
 *	path), then the path is returned. Otherwise NULL will be returned, and
 *	an error message may be left in the interpreter (if it is non-NULL)
 *
 * Results:
 *	NULL or a valid string.
 *
 * Side effects:
 *	Only those of 'Tcl_FSConvertToPathType'
 *
 *---------------------------------------------------------------------------
 */

const char *
Tcl_FSGetTranslatedStringPath(
    Tcl_Interp *interp,
    Tcl_Obj *pathPtr)
{
    Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(interp, pathPtr);

    if (transPtr != NULL) {
	Tcl_Size len;
	const char *orig = TclGetStringFromObj(transPtr, &len);
	char *result = (char *)Tcl_Alloc(len+1);

	memcpy(result, orig, len+1);
	TclDecrRefCount(transPtr);
	return result;
    }

    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetNormalizedPath --
 *
 *	This important function attempts to extract from the given Tcl_Obj a
 *	unique normalised path representation, whose string value can be used
 *	as a unique identifier for the file.
 *
 * Results:
 *	NULL or a valid path object pointer.
 *
 * Side effects:
 *	New memory may be allocated. The Tcl 'errno' may be modified in the
 *	process of trying to examine various path possibilities.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSGetNormalizedPath(
    Tcl_Interp *interp,
    Tcl_Obj *pathPtr)
{
    FsPath *fsPathPtr;

    if (Tcl_FSConvertToPathType(interp, pathPtr) != TCL_OK) {
	return NULL;
    }
    fsPathPtr = PATHOBJ(pathPtr);

    if (PATHFLAGS(pathPtr) != 0) {
	/*
	 * This is a special path object which is the result of something like
	 * 'file join'
	 */

	Tcl_Obj *dir, *copy;
	Tcl_Size tailLen, cwdLen;
	int pathType;

	pathType = Tcl_FSGetPathType(fsPathPtr->cwdPtr);
	dir = Tcl_FSGetNormalizedPath(interp, fsPathPtr->cwdPtr);
	if (dir == NULL) {
	    return NULL;
	}
	/* TODO: Figure out why this is needed. */
	TclGetString(pathPtr);

	(void)TclGetStringFromObj(fsPathPtr->normPathPtr, &tailLen);
	if (tailLen) {
	    copy = AppendPath(dir, fsPathPtr->normPathPtr);
	} else {
	    copy = Tcl_DuplicateObj(dir);
	}
	Tcl_IncrRefCount(dir);
	Tcl_IncrRefCount(copy);

	/*
	 * We now own a reference on both 'dir' and 'copy'
	 */

	(void) TclGetStringFromObj(dir, &cwdLen);

	/* Normalize the combined string. */

	if (PATHFLAGS(pathPtr) & TCLPATH_NEEDNORM) {
	    /*
	     * If the "tail" part has components (like /../) that cause the
	     * combined path to need more complete normalizing, call on the
	     * more powerful routine to accomplish that so we avoid [Bug
	     * 2385549] ...
	     */

	    Tcl_Obj *newCopy = TclFSNormalizeAbsolutePath(interp, copy);

	    Tcl_DecrRefCount(copy);
	    copy = newCopy;
	} else {
	    /*
	     * ... but in most cases where we join a trouble free tail to a
	     * normalized head, we can more efficiently normalize the combined
	     * path by passing over only the unnormalized tail portion. When
	     * this is sufficient, prior developers claim this should be much
	     * faster. We use 'cwdLen' so that we are already pointing at
	     * the dir-separator that we know about. The normalization code
	     * will actually start off directly after that separator.
	     */

	    TclFSNormalizeToUniquePath(interp, copy, cwdLen);
	}

	/* Now we need to construct the new path object. */

	if (pathType == TCL_PATH_RELATIVE) {
	    Tcl_Obj *origDir = fsPathPtr->cwdPtr;

	    /*
	     * NOTE: here we are (dangerously?) assuming that origDir points
	     * to a Tcl_Obj with Tcl_ObjType == &fsPathType. The
	     *     pathType = Tcl_FSGetPathType(fsPathPtr->cwdPtr);
	     * above that set the pathType value should have established that,
	     * but it's far less clear on what basis we know there's been no
	     * shimmering since then.
	     */

	    FsPath *origDirFsPathPtr = PATHOBJ(origDir);

	    fsPathPtr->cwdPtr = origDirFsPathPtr->cwdPtr;
	    Tcl_IncrRefCount(fsPathPtr->cwdPtr);

	    TclDecrRefCount(fsPathPtr->normPathPtr);
	    fsPathPtr->normPathPtr = copy;

	    TclDecrRefCount(dir);
	    TclDecrRefCount(origDir);
	} else {
	    TclDecrRefCount(fsPathPtr->cwdPtr);
	    fsPathPtr->cwdPtr = NULL;
	    TclDecrRefCount(fsPathPtr->normPathPtr);
	    fsPathPtr->normPathPtr = copy;

	    TclDecrRefCount(dir);
	}
	PATHFLAGS(pathPtr) = 0;
    }

    /*
     * Ensure cwd hasn't changed.
     */

    if (fsPathPtr->cwdPtr != NULL) {
	if (!TclFSCwdPointerEquals(&fsPathPtr->cwdPtr)) {
	    TclGetString(pathPtr);
	    Tcl_StoreInternalRep(pathPtr, &fsPathType, NULL);
	    if (SetFsPathFromAny(interp, pathPtr) != TCL_OK) {
		return NULL;
	    }
	    fsPathPtr = PATHOBJ(pathPtr);
	} else if (fsPathPtr->normPathPtr == NULL) {
	    Tcl_Size cwdLen;
	    Tcl_Obj *copy;

	    copy = AppendPath(fsPathPtr->cwdPtr, pathPtr);

	    (void) TclGetStringFromObj(fsPathPtr->cwdPtr, &cwdLen);
	    cwdLen += (TclGetString(copy)[cwdLen] == '/');

	    /*
	     * Normalize the combined string, but only starting after the end
	     * of the previously normalized 'dir'. This should be much faster!
	     */

	    TclFSNormalizeToUniquePath(interp, copy, cwdLen-1);
	    fsPathPtr->normPathPtr = copy;
	    Tcl_IncrRefCount(fsPathPtr->normPathPtr);
	}
    }
    if (fsPathPtr->normPathPtr == NULL) {
	Tcl_Obj *useThisCwd = NULL;

	/*
	 * Since normPathPtr is NULL but this is a valid path object, we know
	 * that the translatedPathPtr cannot be NULL.
	 */

	Tcl_Obj *absolutePath = fsPathPtr->translatedPathPtr;
	const char *path = TclGetString(absolutePath);

	Tcl_IncrRefCount(absolutePath);

	/*
	 * We have to be a little bit careful here to avoid infinite loops
	 * we're asking Tcl_FSGetPathType to return the path's type, but that
	 * call can actually result in a lot of other filesystem action, which
	 * might loop back through here.
	 */

	if (path[0] == '\0') {
	    /*
	     * Special handling for the empty string value. This one is very
	     * weird with [file normalize {}] => {}. (The reasoning supporting
	     * this is unknown to DGP, but he fears changing it.) Attempt here
	     * to keep the expectations of other parts of Tcl_Filesystem code
	     * about state of the FsPath fields satisfied.
	     *
	     * In particular, capture the cwd value and save so it can be
	     * stored in the cwdPtr field below.
	     */

	    useThisCwd = Tcl_FSGetCwd(interp);
	} else {
	    /*
	     * We don't ask for the type of 'pathPtr' here, because that is
	     * not correct for our purposes when we have a path like '~'. Tcl
	     * has a bit of a contradiction in that '~' paths are defined as
	     * 'absolute', but in reality can be just about anything,
	     * depending on how env(HOME) is set.
	     */

	    Tcl_PathType type = Tcl_FSGetPathType(absolutePath);

	    if (type == TCL_PATH_RELATIVE) {
		useThisCwd = Tcl_FSGetCwd(interp);

		if (useThisCwd == NULL) {
		    return NULL;
		}

		Tcl_DecrRefCount(absolutePath);
		absolutePath = Tcl_FSJoinToPath(useThisCwd, 1, &absolutePath);
		Tcl_IncrRefCount(absolutePath);

		/*
		 * We have a refCount on the cwd.
		 */
#ifdef _WIN32
	    } else if (type == TCL_PATH_VOLUME_RELATIVE) {
		/*
		 * Only Windows has volume-relative paths.
		 */

		Tcl_DecrRefCount(absolutePath);
		absolutePath = TclWinVolumeRelativeNormalize(interp,
			path, &useThisCwd);
		if (absolutePath == NULL) {
		    return NULL;
		}
#endif /* _WIN32 */
	    }
	}

	/*
	 * Already has refCount incremented.
	 */

	if (fsPathPtr->normPathPtr) {
	    Tcl_DecrRefCount(fsPathPtr->normPathPtr);
	}
	fsPathPtr->normPathPtr = TclFSNormalizeAbsolutePath(interp,
		absolutePath);

	if (useThisCwd != NULL) {
	    /*
	     * We just need to free an object we allocated above for relative
	     * paths (this was returned by Tcl_FSJoinToPath above), and then
	     * of course store the cwd.
	     */

	    fsPathPtr->cwdPtr = useThisCwd;
	}
	TclDecrRefCount(absolutePath);
    }

    return fsPathPtr->normPathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetInternalRep --
 *
 *	Produces a native representation of a given path object in the given
 *	filesystem.
 *
 *	In the future it might be desirable to have separate versions
 *	of this function with different signatures, for example
 *	Tcl_FSGetNativeWinPath, Tcl_FSGetNativeUnixPath etc. Right now, since
 *	native paths are all string based, we use just one function.
 *
 * Results:
 *
 *	The native handle for the path, or NULL if the path is not handled by
 *	the given filesystem
 *
 * Side effects:
 *
 *	Tcl_FSCreateInternalRepProc if needed to produce the native
 *	handle, which is then stored in the internal representation of pathPtr.
 *
 *---------------------------------------------------------------------------
 */

void *
Tcl_FSGetInternalRep(
    Tcl_Obj *pathPtr,
    const Tcl_Filesystem *fsPtr)
{
    FsPath *srcFsPathPtr;

    if (Tcl_FSConvertToPathType(NULL, pathPtr) != TCL_OK) {
	return NULL;
    }
    srcFsPathPtr = PATHOBJ(pathPtr);

    /*
     * Currently there must be a unique bi-directional mapping between a path
     * and a filesystem, and therefore there is no way to "remap" a file, i.e.,
     * to map a file in one filesystem into another. Another way of putting
     * this is that 'stacked' filesystems are not allowed.  It could be useful
     * in the future to redesign the system to allow that.
     *
     * Even something simple like a 'pass through' filesystem which logs all
     * activity and passes the calls onto the native system would be nice, but
     * not currently easily achievable.
     */

    if (srcFsPathPtr->fsPtr == NULL) {
	Tcl_FSGetFileSystemForPath(pathPtr);

	srcFsPathPtr = PATHOBJ(pathPtr);
	if (srcFsPathPtr->fsPtr == NULL) {
	    /*
	     * The path is probably not a valid path in the filesystsem, and is
	     * most likely to be a use of the empty path "" via a direct call
	     * to one of the objectified interfaces (e.g. from the Tcl
	     * testsuite).
	     */
	    return NULL;
	}
    }

    /*
     * If the file belongs to a different filesystem, perhaps it is actually
     * linked through to a file in the given filesystem. Check this by
     * inspecting the filesystem associated with the given path.
     */

    if (fsPtr != srcFsPathPtr->fsPtr) {
	const Tcl_Filesystem *actualFs = Tcl_FSGetFileSystemForPath(pathPtr);

	if (actualFs == fsPtr) {
	    return Tcl_FSGetInternalRep(pathPtr, fsPtr);
	}
	return NULL;
    }

    if (srcFsPathPtr->nativePathPtr == NULL) {
	Tcl_FSCreateInternalRepProc *proc;
	char *nativePathPtr;

	proc = srcFsPathPtr->fsPtr->createInternalRepProc;
	if (proc == NULL) {
	    return NULL;
	}

	nativePathPtr = (char *)proc(pathPtr);
	srcFsPathPtr = PATHOBJ(pathPtr);
	srcFsPathPtr->nativePathPtr = nativePathPtr;
	srcFsPathPtr->filesystemEpoch = TclFSEpoch();
    }

    return srcFsPathPtr->nativePathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSEnsureEpochOk --
 *
 *	Ensure that the path is a valid path, and that it has a
 *	fsPathType internal representation that is not stale.
 *
 * Results:
 *	A standard Tcl return code.
 *
 * Side effects:
 *	The internal representation of fsPtrPtr is converted to fsPathType if
 *	possible.
 *
 *---------------------------------------------------------------------------
 */

int
TclFSEnsureEpochOk(
    Tcl_Obj *pathPtr,
    const Tcl_Filesystem **fsPtrPtr)
{
    FsPath *srcFsPathPtr;

    if (!TclHasInternalRep(pathPtr, &fsPathType)) {
	return TCL_OK;
    }

    srcFsPathPtr = PATHOBJ(pathPtr);

    if (!TclFSEpochOk(srcFsPathPtr->filesystemEpoch)) {
	/*
	 * The filesystem has changed in some way since the internal
	 * representation for this object was calculated. Discard the stale
	 * representation and recalculate it.
	 */

	TclGetString(pathPtr);
	Tcl_StoreInternalRep(pathPtr, &fsPathType, NULL);
	if (SetFsPathFromAny(NULL, pathPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	srcFsPathPtr = PATHOBJ(pathPtr);
    }

    if (srcFsPathPtr->fsPtr != NULL) {
	/*
	 * There is already a filesystem assigned to this path.
	 */
	*fsPtrPtr = srcFsPathPtr->fsPtr;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSSetPathDetails --
 *
 *	???
 *
 * Results:
 *	None
 *
 * Side effects:
 *	???
 *
 *---------------------------------------------------------------------------
 */

void
TclFSSetPathDetails(
    Tcl_Obj *pathPtr,
    const Tcl_Filesystem *fsPtr,
    void *clientData)
{
    FsPath *srcFsPathPtr;

    /*
     * Make sure pathPtr is of the correct type.
     */

    if (!TclHasInternalRep(pathPtr, &fsPathType)) {
	if (SetFsPathFromAny(NULL, pathPtr) != TCL_OK) {
	    return;
	}
    }

    srcFsPathPtr = PATHOBJ(pathPtr);
    srcFsPathPtr->fsPtr = fsPtr;
    srcFsPathPtr->nativePathPtr = clientData;
    srcFsPathPtr->filesystemEpoch = TclFSEpoch();
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSEqualPaths --
 *
 *	This function tests whether the two paths given are equal path
 *	objects. If either or both is NULL, 0 is always returned.
 *
 * Results:
 *	1 or 0.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSEqualPaths(
    Tcl_Obj *firstPtr,
    Tcl_Obj *secondPtr)
{
    const char *firstStr, *secondStr;
    Tcl_Size firstLen, secondLen;
    int tempErrno;

    if (firstPtr == secondPtr) {
	return 1;
    }

    if (firstPtr == NULL || secondPtr == NULL) {
	return 0;
    }
    firstStr = TclGetStringFromObj(firstPtr, &firstLen);
    secondStr = TclGetStringFromObj(secondPtr, &secondLen);
    if ((firstLen == secondLen) && !memcmp(firstStr, secondStr, firstLen)) {
	return 1;
    }

    /*
     * Try the most thorough, correct method of comparing fully normalized
     * paths.
     */

    tempErrno = Tcl_GetErrno();
    firstPtr = Tcl_FSGetNormalizedPath(NULL, firstPtr);
    secondPtr = Tcl_FSGetNormalizedPath(NULL, secondPtr);
    Tcl_SetErrno(tempErrno);

    if (firstPtr == NULL || secondPtr == NULL) {
	return 0;
    }

    firstStr = TclGetStringFromObj(firstPtr, &firstLen);
    secondStr = TclGetStringFromObj(secondPtr, &secondLen);
    return ((firstLen == secondLen) && !memcmp(firstStr, secondStr, firstLen));
}

/*
 *---------------------------------------------------------------------------
 *
 * SetFsPathFromAny --
 *
 *	Attempt to convert the internal representation of pathPtr to
 *	fsPathType.
 *
 * Results:
 *	Standard Tcl error code.
 *
 * Side effects:
 *	The old representation may be freed, and new memory allocated.
 *
 *---------------------------------------------------------------------------
 */

static int
SetFsPathFromAny(
    TCL_UNUSED(Tcl_Interp *),	/* Used for error reporting if not NULL. */
    Tcl_Obj *pathPtr)		/* The object to convert. */
{
    Tcl_Size len;
    FsPath *fsPathPtr;
    Tcl_Obj *transPtr;

    if (TclHasInternalRep(pathPtr, &fsPathType)) {
	return TCL_OK;
    }

    /*
     * First step is to translate the filename. This is similar to
     * Tcl_TranslateFilename, but shouldn't convert everything to windows
     * backslashes on that platform. The current implementation of this piece
     * is a slightly optimised version of the various Tilde/Split/Join stuff
     * to avoid multiple split/join operations.
     *
     * We remove any trailing directory separator.
     *
     * However, the split/join routines are quite complex, and one has to make
     * sure not to break anything on Unix or Win (fCmd.test, fileName.test and
     * cmdAH.test exercise most of the code).
     */

    TclGetStringFromObj(pathPtr, &len); /* TODO: Is this needed? */
    transPtr = TclJoinPath(1, &pathPtr, 1);

    /*
     * Now we have a translated filename in 'transPtr'. This will have forward
     * slashes on Windows, and will not contain any ~user sequences.
     */

    fsPathPtr = (FsPath *)Tcl_Alloc(sizeof(FsPath));

    if (transPtr == pathPtr) {
	(void) Tcl_GetString(pathPtr);
	TclFreeInternalRep(pathPtr);
	transPtr = Tcl_DuplicateObj(pathPtr);
	fsPathPtr->filesystemEpoch = 0;
    } else {
	fsPathPtr->filesystemEpoch = TclFSEpoch();
    }
    Tcl_IncrRefCount(transPtr);
    fsPathPtr->translatedPathPtr = transPtr;
    fsPathPtr->normPathPtr = NULL;
    fsPathPtr->cwdPtr = NULL;
    fsPathPtr->nativePathPtr = NULL;
    fsPathPtr->fsPtr = NULL;

    SETPATHOBJ(pathPtr, fsPathPtr);
    PATHFLAGS(pathPtr) = 0;
    return TCL_OK;
}

static void
FreeFsPathInternalRep(
    Tcl_Obj *pathPtr)		/* Path object with internal rep to free. */
{
    FsPath *fsPathPtr = PATHOBJ(pathPtr);

    if (fsPathPtr->translatedPathPtr != NULL) {
	if (fsPathPtr->translatedPathPtr != pathPtr) {
	    TclDecrRefCount(fsPathPtr->translatedPathPtr);
	}
    }
    if (fsPathPtr->normPathPtr != NULL) {
	if (fsPathPtr->normPathPtr != pathPtr) {
	    TclDecrRefCount(fsPathPtr->normPathPtr);
	}
	fsPathPtr->normPathPtr = NULL;
    }
    if (fsPathPtr->cwdPtr != NULL) {
	TclDecrRefCount(fsPathPtr->cwdPtr);
	fsPathPtr->cwdPtr = NULL;
    }
    if (fsPathPtr->nativePathPtr != NULL && fsPathPtr->fsPtr != NULL) {
	Tcl_FSFreeInternalRepProc *freeProc =
		fsPathPtr->fsPtr->freeInternalRepProc;

	if (freeProc != NULL) {
	    freeProc(fsPathPtr->nativePathPtr);
	    fsPathPtr->nativePathPtr = NULL;
	}
    }

    Tcl_Free(fsPathPtr);
}

static void
DupFsPathInternalRep(
    Tcl_Obj *srcPtr,		/* Path obj with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Path obj with internal rep to set. */
{
    FsPath *srcFsPathPtr = PATHOBJ(srcPtr);
    FsPath *copyFsPathPtr = (FsPath *)Tcl_Alloc(sizeof(FsPath));

    SETPATHOBJ(copyPtr, copyFsPathPtr);

    copyFsPathPtr->translatedPathPtr = srcFsPathPtr->translatedPathPtr;
    if (copyFsPathPtr->translatedPathPtr != NULL) {
	Tcl_IncrRefCount(copyFsPathPtr->translatedPathPtr);
    }

    copyFsPathPtr->normPathPtr = srcFsPathPtr->normPathPtr;
    if (copyFsPathPtr->normPathPtr != NULL) {
	Tcl_IncrRefCount(copyFsPathPtr->normPathPtr);
    }

    copyFsPathPtr->cwdPtr = srcFsPathPtr->cwdPtr;
    if (copyFsPathPtr->cwdPtr != NULL) {
	Tcl_IncrRefCount(copyFsPathPtr->cwdPtr);
    }

    copyFsPathPtr->flags = srcFsPathPtr->flags;

    if (srcFsPathPtr->fsPtr != NULL
	    && srcFsPathPtr->nativePathPtr != NULL) {
	Tcl_FSDupInternalRepProc *dupProc =
		srcFsPathPtr->fsPtr->dupInternalRepProc;

	if (dupProc != NULL) {
	    copyFsPathPtr->nativePathPtr =
		    dupProc(srcFsPathPtr->nativePathPtr);
	} else {
	    copyFsPathPtr->nativePathPtr = NULL;
	}
    } else {
	copyFsPathPtr->nativePathPtr = NULL;
    }
    copyFsPathPtr->fsPtr = srcFsPathPtr->fsPtr;
    copyFsPathPtr->filesystemEpoch = srcFsPathPtr->filesystemEpoch;
}

/*
 *---------------------------------------------------------------------------
 *
 * UpdateStringOfFsPath --
 *
 *	Gives an object a valid string rep.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory may be allocated.
 *
 *---------------------------------------------------------------------------
 */

static void
UpdateStringOfFsPath(
    Tcl_Obj *pathPtr)		/* path obj with string rep to update. */
{
    FsPath *fsPathPtr = PATHOBJ(pathPtr);
    Tcl_Size cwdLen;
    Tcl_Obj *copy;

    if (PATHFLAGS(pathPtr) == 0 || fsPathPtr->cwdPtr == NULL) {
	if (fsPathPtr->translatedPathPtr == NULL) {
	    Tcl_Panic("Called UpdateStringOfFsPath with invalid object");
	} else {
	    copy = Tcl_DuplicateObj(fsPathPtr->translatedPathPtr);
	}
    } else {
	    copy = AppendPath(fsPathPtr->cwdPtr, fsPathPtr->normPathPtr);
    }

    if (Tcl_IsShared(copy)) {
	copy = Tcl_DuplicateObj(copy);
    }

    Tcl_IncrRefCount(copy);
    /* Steal copy's string rep */
    pathPtr->bytes = TclGetStringFromObj(copy, &cwdLen);
    pathPtr->length = cwdLen;
    TclInitEmptyStringRep(copy);
    TclDecrRefCount(copy);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclNativePathInFilesystem --
 *
 *	Any path object is acceptable to the native filesystem, by default (we
 *	will throw errors when illegal paths are actually tried to be used).
 *
 *	However, this behavior means the native filesystem must be the last
 *	filesystem in the lookup list (otherwise it will claim all files
 *	belong to it, and other filesystems will never get a look in).
 *
 * Results:
 *	TCL_OK, to indicate 'yes', -1 to indicate no.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
TclNativePathInFilesystem(
    Tcl_Obj *pathPtr,
    TCL_UNUSED(void **))
{
    /*
     * A special case is required to handle the empty path "". This is a valid
     * path (i.e. the user should be able to do 'file exists ""' without
     * throwing an error), but equally the path doesn't exist. Those are the
     * semantics of Tcl (at present anyway), so we have to abide by them here.
     */

    if (TclHasInternalRep(pathPtr, &fsPathType)) {
	if (pathPtr->bytes != NULL && pathPtr->bytes[0] == '\0') {
	    /*
	     * We reject the empty path "".
	     */

	    return -1;
	}

	/*
	 * Otherwise there is no way this path can be empty.
	 */
    } else {
	/*
	 * It is somewhat unusual to reach this code path without the object
	 * being of fsPathType. However, we do our best to deal with the
	 * situation.
	 */

	Tcl_Size len;

	(void) TclGetStringFromObj(pathPtr, &len);
	if (len == 0) {
	    /*
	     * We reject the empty path "".
	     */

	    return -1;
	}
    }

    /*
     * Path is of correct type, or is of non-zero length, so we accept it.
     */

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeTildeRelativePath --
 *
 *      Returns a path relative to the home directory of a user.
 *      Note there is a difference between not specifying a user and
 *	explicitly specifying the current user. This mimics Tcl8's tilde
 *	expansion.
 *
 *	The subPath argument is joined to the expanded home directory
 *	as in Tcl_JoinPath. This means if it is not relative, it will
 *	returned as the result with the home directory only checked
 *	for user name validity.
 *
 * Results:
 *	Returns TCL_OK on success with home directory path in *dsPtr
 *	and TCL_ERROR on failure with error message in interp if non-NULL.
 *
 *----------------------------------------------------------------------
 */
int
MakeTildeRelativePath(
    Tcl_Interp *interp,		/* May be NULL. Only used for error messages */
    const char *user,		/* User name. NULL -> current user */
    const char *subPath,	/* Rest of path. May be NULL */
    Tcl_DString *dsPtr)		/* Output. Is initialized by the function. Must
				 * be freed on success */
{
    const char *dir;
    Tcl_DString dirString;

    Tcl_DStringInit(dsPtr);
    Tcl_DStringInit(&dirString);

    if (user == NULL || user[0] == 0) {
	/* No user name specified -> current user */

	dir = TclGetEnv("HOME", &dirString);
	if (dir == NULL) {
	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"couldn't find HOME environment variable to expand path",
			-1));
		Tcl_SetErrorCode(interp, "TCL", "VALUE", "PATH",
			"HOMELESS", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    } else {
	/* User name specified - ~user */
	dir = TclpGetUserHome(user, &dirString);
	if (dir == NULL) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"user \"%s\" doesn't exist", user));
		Tcl_SetErrorCode(interp, "TCL", "VALUE", "PATH", "NOUSER",
			(char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }
    if (subPath) {
	const char *parts[2];
	parts[0] = dir;
	parts[1] = subPath;
	Tcl_JoinPath(2, parts, dsPtr);
    } else {
	Tcl_JoinPath(1, &dir, dsPtr);
    }

    Tcl_DStringFree(&dirString);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetHomeDirObj --
 *
 *	Wrapper around MakeTildeRelativePath. See that function.
 *
 * Results:
 *      Returns a Tcl_Obj containing the home directory of a user
 *	or NULL on failure with error message in interp if non-NULL.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclGetHomeDirObj(
    Tcl_Interp *interp,		/* May be NULL. Only used for error messages */
    const char *user)		/* User name. NULL -> current user */
{
    Tcl_DString dirString;

    if (MakeTildeRelativePath(interp, user, NULL, &dirString) != TCL_OK) {
	return NULL;
    }
    return Tcl_DStringToObj(&dirString);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSTildeExpand --
 *
 *	Copies the path passed in to the output Tcl_DString dsPtr,
 *	resolving leading ~ and ~user components in the path if present.
 *      An error is returned if such a component IS present AND cannot
 *      be resolved.
 *
 *	The output dsPtr must be cleared by caller on success.
 *
 * Results:
 *      TCL_OK - path did not contain leading ~ or it was successful resolved
 *      TCL_ERROR - ~ component could not be resolved.
 *
 *----------------------------------------------------------------------
 */
int
Tcl_FSTildeExpand(
    Tcl_Interp *interp,		/* May be NULL. Only used for error messages */
    const char *path,		/* Path to resolve tilde */
    Tcl_DString *dsPtr)		/* Output DString for resolved path. */
{
    Tcl_Size split;
    int result;

    assert(path);
    assert(dsPtr);

    Tcl_DStringInit(dsPtr);
    if (path[0] != '~') {
	Tcl_DStringAppend(dsPtr, path, -1);
	return TCL_OK;
    }

    /*
     * We have multiple cases '~', '~user', '~/foo/bar...', '~user/foo...'
     * FindSplitPos returns 1 for '~/...' as well as for '~'. Note on
     * Windows FindSplitPos implicitly checks for '\' as separator
     * in addition to what is passed.
     */
    split = FindSplitPos(path, '/');

    if (split == 1) {
	/* No user name specified '~' or '~/...' -> current user */
	result = MakeTildeRelativePath(interp, NULL, path[1] ? 2 + path : NULL, dsPtr);
    } else {
	/* User name specified - ~user, ~user/... */
	const char *user;
	Tcl_DString dsUser;

	Tcl_DStringInit(&dsUser);
	Tcl_DStringAppend(&dsUser, path+1, split-1);
	user = Tcl_DStringValue(&dsUser);

	/* path[split] is / for ~user/... or \0 for ~user */
	result = MakeTildeRelativePath(interp, user,
		path[split] ? &path[split + 1] : NULL, dsPtr);
	Tcl_DStringFree(&dsUser);
    }
    if (result != TCL_OK) {
	/* Do not rely on caller to free in case of errors */
	Tcl_DStringFree(dsPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclResolveTildePath --
 *
 *	If the passed path is begins with a tilde, does tilde resolution
 *	and returns a Tcl_Obj containing the resolved path. If the tilde
 *	component cannot be resolved, returns NULL. If the path does not
 *	begin with a tilde, returns as is.
 *
 * Results:
 *      Returns a Tcl_Obj with resolved path. This may be a new Tcl_Obj
 *	with ref count 0 or that pathObj that was passed in without its
 *	ref count modified.
 *      Returns NULL if the path begins with a ~ that cannot be resolved
 *	and stores an error message in interp if non-NULL.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclResolveTildePath(
    Tcl_Interp *interp,		/* May be NULL. Only used for error messages */
    Tcl_Obj *pathObj)
{
    const char *path;
    Tcl_Size len;
    Tcl_DString resolvedPath;

    path = TclGetStringFromObj(pathObj, &len);
    /* Optimize to skip unnecessary calls below */
    if (path[0] != '~') {
	return pathObj;
    }

    if (Tcl_FSTildeExpand(interp, path, &resolvedPath) != TCL_OK) {
	return NULL;
    }

    return Tcl_DStringToObj(&resolvedPath);
}

/*
 *----------------------------------------------------------------------
 *
 * TclResolveTildePathList --
 *
 *	Given a Tcl_Obj that is a list of paths, returns a Tcl_Obj containing
 *	the paths with any ~-prefixed paths resolved.
 *
 *	Empty strings and ~-prefixed paths that cannot be resolved are
 *      removed from the returned list.
 *
 *      The trailing components of the path are returned verbatim. No
 *	processing is done on them. Moreover, no assumptions should be
 *	made about the separators in the returned path. They may be /
 *	or native. Appropriate path manipulations functions should be
 *	used by caller if desired.
 *
 * Results:
 *	Returns a Tcl_Obj with resolved paths. This may be a new Tcl_Obj with
 *	reference count 0 or the original passed-in Tcl_Obj if no paths needed
 *	resolution. A NULL is returned if the passed in value is not a list
 *	or was NULL.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *
TclResolveTildePathList(
    Tcl_Obj *pathsObj)
{
    Tcl_Obj **objv;
    Tcl_Size objc;
    Tcl_Size i;
    Tcl_Obj *resolvedPaths;
    const char *path;

    if (pathsObj == NULL) {
	return NULL;
    }
    if (Tcl_ListObjGetElements(NULL, pathsObj, &objc, &objv) != TCL_OK) {
	return NULL; /* Not a list */
    }

    /*
     * Figure out if any paths need resolving to avoid unnecessary allocations.
     */
    for (i = 0; i < objc; ++i) {
	path = Tcl_GetString(objv[i]);
	if (path[0] == '~') {
	    break; /* At least one path needs resolution */
	}
    }
    if (i == objc) {
	return pathsObj; /* No paths needed to be resolved */
    }

    resolvedPaths = Tcl_NewListObj(objc, NULL);
    for (i = 0; i < objc; ++i) {
	Tcl_Obj *resolvedPath;
	path = Tcl_GetString(objv[i]);
	if (path[0] == 0) {
	    continue; /* Skip empty strings */
	}
	resolvedPath = TclResolveTildePath(NULL, objv[i]);
	if (resolvedPath) {
	    /* Paths that cannot be resolved are skipped */
	    Tcl_ListObjAppendElement(NULL, resolvedPaths, resolvedPath);
	}
    }

    return resolvedPaths;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
