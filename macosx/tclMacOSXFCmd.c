/*
 * tclMacOSXFCmd.c
 *
 *      This file implements the MacOSX specific portion of file manipulation 
 *      subcommands of the "file" command.
 *
 * Copyright (c) 2003 Tcl Core Team.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclMacOSXFCmd.c,v 1.2 2004/04/06 22:25:56 dgp Exp $
 */

#include "tclInt.h"

#ifdef HAVE_GETATTRLIST
#include <sys/attr.h>
#include <sys/paths.h>
#endif

/*
 * Constants for file attributes subcommand.
 * Need to be kept in sync with tclUnixFCmd.c !
 */

enum {
    UNIX_GROUP_ATTRIBUTE,
    UNIX_OWNER_ATTRIBUTE,
    UNIX_PERMISSIONS_ATTRIBUTE,
#ifdef HAVE_CHFLAGS
    UNIX_READONLY_ATTRIBUTE,
#endif
#ifdef MAC_OSX_TCL
    MACOSX_CREATOR_ATTRIBUTE,
    MACOSX_TYPE_ATTRIBUTE,
    MACOSX_HIDDEN_ATTRIBUTE,
    MACOSX_RSRCLENGTH_ATTRIBUTE,
#endif
};

typedef u_int32_t OSType;

static int Tcl_GetOSTypeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, 
		OSType *osTypePtr);
static Tcl_Obj *Tcl_NewOSTypeStringObj(CONST OSType newOSType);

enum {
   kFinfoIsInvisible = 0x4000,
};

typedef struct fileinfobuf {
   u_int32_t info_length;
   union {
     struct {
       u_int32_t type;
       u_int32_t creator;
       u_int16_t fdFlags;
       u_int16_t location;
       u_int32_t padding[4];
     } finder;
     off_t rsrcForkSize;
   } data;
} fileinfobuf;

/*
 *----------------------------------------------------------------------
 *
 * TclMacOSXGetFileAttribute
 *
 *      Gets a MacOSX attribute of a file.  Which attribute is
 *      controlled by objIndex. The object will have ref count 0.
 *
 * Results:
 *      Standard TCL result. Returns a new Tcl_Obj in attributePtrPtr
 *        if there is no error.
 *
 * Side effects:
 *      A new object is allocated.
 *      
 *----------------------------------------------------------------------
 */

int
TclMacOSXGetFileAttribute(interp, objIndex, fileName, attributePtrPtr)
    Tcl_Interp *interp;          /* The interp we are using for errors. */
    int objIndex;                /* The index of the attribute. */
    Tcl_Obj *fileName;           /* The name of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr;   /* A pointer to return the object with. */
{
#ifdef HAVE_GETATTRLIST
    int result;
    Tcl_StatBuf statBuf;
    struct attrlist alist;
    fileinfobuf finfo;
    CONST char *native;
    
    result = TclpObjStat(fileName, &statBuf);
    
    if (result != 0) {
	Tcl_AppendResult(interp, "could not read \"", 
		Tcl_GetString(fileName), "\": ",
		Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    if (S_ISDIR(statBuf.st_mode) && objIndex != MACOSX_HIDDEN_ATTRIBUTE) {
        /* Directories only support attribute "-hidden" */
        errno = EISDIR;
        Tcl_AppendResult(interp, "invalid attribute: ", 
                Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }

    memset(&alist, 0, sizeof(struct attrlist));
    alist.bitmapcount = ATTR_BIT_MAP_COUNT;
    if(objIndex == MACOSX_RSRCLENGTH_ATTRIBUTE) {
        alist.fileattr = ATTR_FILE_RSRCLENGTH;
    } else {
        alist.commonattr = ATTR_CMN_FNDRINFO;    
    }
    native = Tcl_FSGetNativePath(fileName);
    result = getattrlist(native, &alist, &finfo, sizeof(fileinfobuf), 0);

    if (result != 0) {
        Tcl_AppendResult(interp, "could not read attributes of \"", 
                Tcl_GetString(fileName), "\": ",
                Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }

    switch (objIndex) {
        case MACOSX_CREATOR_ATTRIBUTE:
            *attributePtrPtr = Tcl_NewOSTypeStringObj(finfo.data.finder.creator);
            break;
        case MACOSX_TYPE_ATTRIBUTE:
            *attributePtrPtr = Tcl_NewOSTypeStringObj(finfo.data.finder.type);
            break;
        case MACOSX_HIDDEN_ATTRIBUTE:
            *attributePtrPtr = Tcl_NewBooleanObj( (finfo.data.finder.fdFlags
                & kFinfoIsInvisible) != 0);
            break;
        case MACOSX_RSRCLENGTH_ATTRIBUTE:
            *attributePtrPtr = Tcl_NewWideIntObj(finfo.data.rsrcForkSize);
            break;
    }
    return TCL_OK;
#else
    Tcl_AppendResult(interp, "Mac OS X file attributes not supported",
		(char *) NULL);
    return TCL_ERROR;
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * TclMacOSXSetFileAttribute --
 *
 *      Sets a MacOSX attribute of a file.  Which attribute is
 *      controlled by objIndex.
 *
 * Results:
 *      Standard TCL result.
 *
 * Side effects:
 *      As above.
 *      
 *---------------------------------------------------------------------------
 */

int
TclMacOSXSetFileAttribute(interp, objIndex, fileName, attributePtr)
    Tcl_Interp *interp;              /* The interp for error reporting. */
    int objIndex;                    /* The index of the attribute. */
    Tcl_Obj *fileName;               /* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr;           /* New owner for file. */
{
#ifdef HAVE_GETATTRLIST
    int result;
    Tcl_StatBuf statBuf;
    struct attrlist alist;
    fileinfobuf finfo;
    CONST char *native;
    
    result = TclpObjStat(fileName, &statBuf);
    
    if (result != 0) {
	Tcl_AppendResult(interp, "could not read \"", 
		Tcl_GetString(fileName), "\": ",
		Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    if (S_ISDIR(statBuf.st_mode) && objIndex != MACOSX_HIDDEN_ATTRIBUTE) {
        /* Directories only support attribute "-hidden" */
        errno = EISDIR;
        Tcl_AppendResult(interp, "invalid attribute: ", 
                Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }

    memset(&alist, 0, sizeof(struct attrlist));
    alist.bitmapcount = ATTR_BIT_MAP_COUNT;
    if(objIndex == MACOSX_RSRCLENGTH_ATTRIBUTE) {
        alist.fileattr = ATTR_FILE_RSRCLENGTH;
    } else {
        alist.commonattr = ATTR_CMN_FNDRINFO;    
    }
    native = Tcl_FSGetNativePath(fileName);
    result = getattrlist(native, &alist, &finfo, sizeof(fileinfobuf), 0);

    if (result != 0) {
        Tcl_AppendResult(interp, "could not read attributes of \"", 
                Tcl_GetString(fileName), "\": ",
                Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }

    if (objIndex != MACOSX_RSRCLENGTH_ATTRIBUTE) {
        switch (objIndex) {
            case MACOSX_CREATOR_ATTRIBUTE:
                if (Tcl_GetOSTypeFromObj(interp, attributePtr,
                        &finfo.data.finder.creator) != TCL_OK) {
                    return TCL_ERROR;
                }
                break;
            case MACOSX_TYPE_ATTRIBUTE:
                if (Tcl_GetOSTypeFromObj(interp, attributePtr,
                        &finfo.data.finder.type) != TCL_OK) {
                    return TCL_ERROR;
                }
                break;
            case MACOSX_HIDDEN_ATTRIBUTE:
                {
                    int hidden;
                    if (Tcl_GetBooleanFromObj(interp, attributePtr, &hidden)
                            != TCL_OK) {
                        return TCL_ERROR;
                    }
                    if (hidden) {
                        finfo.data.finder.fdFlags |= kFinfoIsInvisible;
                    } else {
                        finfo.data.finder.fdFlags &= ~kFinfoIsInvisible;
                    }
                }
                break;
        }
        result = setattrlist(native, &alist, &finfo.data, sizeof(finfo.data), 0);
    
        if (result != 0) {
            Tcl_AppendResult(interp, "could not set attributes of \"", 
                    Tcl_GetString(fileName), "\": ",
                    Tcl_PosixError(interp), (char *) NULL);
            return TCL_ERROR;
        }
    } else {
        off_t newRsrcForkSize;
        
        if (Tcl_GetWideIntFromObj(interp, attributePtr,
                &newRsrcForkSize) != TCL_OK) {
            return TCL_ERROR;
        }
        
        if(newRsrcForkSize != finfo.data.rsrcForkSize) {
            Tcl_DString ds;
            /*
             * Only setting rsrclength to 0 to strip
             * a file's resource fork is supported.
             */
            if(newRsrcForkSize != 0) {
                Tcl_AppendResult(interp,
                        "setting nonzero rsrclength not supported", 
                        (char *) NULL);
                return TCL_ERROR;
            }
            
            /* construct path to resource fork */
            Tcl_DStringInit(&ds);
            Tcl_DStringAppend(&ds, native, -1);
            Tcl_DStringAppend(&ds, _PATH_RSRCFORKSPEC, -1);

            result = truncate(Tcl_DStringValue(&ds), (off_t)0);

            Tcl_DStringFree(&ds);
    
            if (result != 0) {
                Tcl_AppendResult(interp, 
                        "could not truncate resource fork of \"",
                        Tcl_GetString(fileName), "\": ",
                        Tcl_PosixError(interp), (char *) NULL);
                return TCL_ERROR;
            }
        }
    }
    return TCL_OK;
#else
    Tcl_AppendResult(interp, "Mac OS X file attributes not supported",
		(char *) NULL);
    return TCL_ERROR;
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * TclMacOSXCopyFileAttributes --
 *
 *	Copy the MacOSX attributes and resource fork (if present) from one
 *	file to another.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	MacOSX attributes and resource fork are updated in the new file
 *	to reflect the old file.
 *
 *---------------------------------------------------------------------------
 */

int
TclMacOSXCopyFileAttributes(src, dst, statBufPtr) 
    CONST char *src;		/* Path name of source file (native). */
    CONST char *dst;		/* Path name of target file (native). */
    CONST Tcl_StatBuf *statBufPtr;
				/* Stat info for source file */
{
#ifdef HAVE_GETATTRLIST
    struct attrlist alist;
    fileinfobuf finfo;
    
    memset(&alist, 0, sizeof(struct attrlist));
    alist.bitmapcount = ATTR_BIT_MAP_COUNT;
    alist.commonattr = ATTR_CMN_FNDRINFO;    

    if (getattrlist(src, &alist, &finfo, sizeof(fileinfobuf), 0)) {
        return TCL_ERROR;
    }

    if (setattrlist(dst, &alist, &finfo.data, sizeof(finfo.data), 0)) {
        return TCL_ERROR;
    }

    if (!S_ISDIR(statBufPtr->st_mode)) {
        /* only copy non-empty resource fork */
        alist.commonattr = 0;    
        alist.fileattr = ATTR_FILE_RSRCLENGTH;
        
	if (getattrlist(src, &alist, &finfo, sizeof(fileinfobuf), 0)) {
	    return TCL_ERROR;
	}
        
        if(finfo.data.rsrcForkSize > 0) {
	    int result;
            Tcl_DString ds_src, ds_dst;
                        
            /* construct paths to resource forks */
            Tcl_DStringInit(&ds_src);
            Tcl_DStringAppend(&ds_src, src, -1);
            Tcl_DStringAppend(&ds_src, _PATH_RSRCFORKSPEC, -1);
            Tcl_DStringInit(&ds_dst);
            Tcl_DStringAppend(&ds_dst, dst, -1);
            Tcl_DStringAppend(&ds_dst, _PATH_RSRCFORKSPEC, -1);
 
            result = TclUnixCopyFile(Tcl_DStringValue(&ds_src),
                    Tcl_DStringValue(&ds_dst), statBufPtr, 1);

            Tcl_DStringFree(&ds_src);
            Tcl_DStringFree(&ds_dst);
    
	    if (result != 0) {
		return TCL_ERROR;
	    }
        }
    }
    return TCL_OK;
#else
    return TCL_ERROR;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetOSTypeFromObj --
 *
 *	Attempt to return an OSType from the Tcl object "objPtr".
 *
 * Results:
 *	Standard TCL result. If an error occurs during conversion,
 *	an error message is left in interp->objResult.
 *
 * Side effects:
 *	The string representation of objPtr will be updated if necessary.
 *
 *----------------------------------------------------------------------
 */

static int
Tcl_GetOSTypeFromObj(
    Tcl_Interp *interp,         /* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr,            /* The object from which to get an OSType. */
    OSType *osTypePtr)          /* Place to store resulting OSType. */
{
    char *string;
    int length, result = TCL_OK;
    Tcl_DString ds;
    Tcl_Encoding encoding = Tcl_GetEncoding(NULL, "macRoman");

    string = Tcl_GetStringFromObj(objPtr, &length);
    Tcl_UtfToExternalDString(encoding, string, length, &ds);

    if (Tcl_DStringLength(&ds) > sizeof(OSType)) {
	Tcl_AppendResult(interp, 
		"expected Macintosh OS type but got \"",
		string, "\": ", (char *) NULL);
        result = TCL_ERROR;
    } else {
	memset(osTypePtr, 0, sizeof(OSType));
	memcpy(osTypePtr, Tcl_DStringValue(&ds),
	        (size_t) Tcl_DStringLength(&ds));
    }
    Tcl_DStringFree(&ds);
    Tcl_FreeEncoding(encoding);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewOSTypeStringObj --
 *
 *	Create a new OSType string object.
 *
 * Results:
 *	The newly created string object is returned, it has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
Tcl_NewOSTypeStringObj(
    CONST OSType newOSType)    /* OSType used to initialize the new object. */
{
    char string[sizeof(OSType)+1];
    Tcl_Obj *resultPtr;
    Tcl_DString ds;
    Tcl_Encoding encoding = Tcl_GetEncoding(NULL, "macRoman");

    memcpy(string, &newOSType, sizeof(OSType));
    string[sizeof(OSType)] = '\0';
    Tcl_ExternalToUtfDString(encoding, string, -1, &ds);
    resultPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    Tcl_FreeEncoding(encoding);
    return resultPtr;
}
