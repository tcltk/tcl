/* 
 * tclWinFile.c --
 *
 *      This file contains temporary wrappers around UNIX file handling
 *      functions. These wrappers map the UNIX functions to Win32 HANDLE-style
 *      files, which can be manipulated through the Win32 console redirection
 *      interfaces.
 *
 * Copyright (c) 1995-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinFile.c,v 1.17.2.2 2002/06/10 05:33:19 wolfsuit Exp $
 */

#include "tclWinInt.h"
#include <sys/stat.h>
#include <shlobj.h>
#include <lmaccess.h>		/* For TclpGetUserHome(). */

static time_t		ToCTime(FILETIME fileTime);

typedef NET_API_STATUS NET_API_FUNCTION NETUSERGETINFOPROC
	(LPWSTR servername, LPWSTR username, DWORD level, LPBYTE *bufptr);

typedef NET_API_STATUS NET_API_FUNCTION NETAPIBUFFERFREEPROC
	(LPVOID Buffer);

typedef NET_API_STATUS NET_API_FUNCTION NETGETDCNAMEPROC
	(LPWSTR servername, LPWSTR domainname, LPBYTE *bufptr);

static int NativeAccess(CONST TCHAR *path, int mode);
static int NativeStat(CONST TCHAR *path, Tcl_StatBuf *statPtr);
static int NativeIsExec(CONST TCHAR *path);
static int WinIsDrive(CONST char *name, int nameLen);
static int NativeMatchType(CONST char *name, int nameLen, 
			   CONST TCHAR* nativeName, Tcl_GlobTypeData *types);


/*
 *---------------------------------------------------------------------------
 *
 * TclpFindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.
 *
 * Results:
 *	A dirty UTF string that is the path to the executable.  At this
 *	point we may not know the system encoding.  Convert the native
 *	string value to UTF using the default encoding.  The assumption
 *	is that we will still be able to parse the path given the path
 *	name contains ASCII string and '/' chars do not conflict with
 *	other UTF chars.
 *
 * Side effects:
 *	The variable tclNativeExecutableName gets filled in with the file
 *	name for the application, if we figured it out.  If we couldn't
 *	figure it out, tclNativeExecutableName is set to NULL.
 *
 *---------------------------------------------------------------------------
 */

char *
TclpFindExecutable(argv0)
    CONST char *argv0;		/* The value of the application's argv[0]
				 * (native). */
{
    Tcl_DString ds;
    WCHAR wName[MAX_PATH];

    if (argv0 == NULL) {
	return NULL;
    }
    if (tclNativeExecutableName != NULL) {
	return tclNativeExecutableName;
    }

    /*
     * Under Windows we ignore argv0, and return the path for the file used to
     * create this process.
     */

    (*tclWinProcs->getModuleFileNameProc)(NULL, wName, MAX_PATH);
    Tcl_WinTCharToUtf((CONST TCHAR *) wName, -1, &ds);

    tclNativeExecutableName = ckalloc((unsigned) (Tcl_DStringLength(&ds) + 1));
    strcpy(tclNativeExecutableName, Tcl_DStringValue(&ds));
    Tcl_DStringFree(&ds);

    TclWinNoBackslash(tclNativeExecutableName);
    return tclNativeExecutableName;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpMatchInDirectory --
 *
 *	This routine is used by the globbing code to search a
 *	directory for all files which match a given pattern.
 *
 * Results: 
 *	
 *	The return value is a standard Tcl result indicating whether an
 *	error occurred in globbing.  Errors are left in interp, good
 *	results are lappended to resultPtr (which must be a valid object)
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

int
TclpMatchInDirectory(interp, resultPtr, pathPtr, pattern, types)
    Tcl_Interp *interp;		/* Interpreter to receive errors. */
    Tcl_Obj *resultPtr;		/* List object to lappend results. */
    Tcl_Obj *pathPtr;	        /* Contains path to directory to search. */
    CONST char *pattern;	/* Pattern to match against. */
    Tcl_GlobTypeData *types;	/* Object containing list of acceptable types.
				 * May be NULL. In particular the directory
				 * flag is very important. */
{
    CONST TCHAR *nativeName;

    if (pattern == NULL || (*pattern == '\0')) {
	Tcl_Obj *norm = Tcl_FSGetNormalizedPath(NULL, pathPtr);
	if (norm != NULL) {
	    int len;
	    char *str = Tcl_GetStringFromObj(norm,&len);
	    /* Match a file directly */
	    nativeName = (CONST TCHAR*) Tcl_FSGetNativePath(pathPtr);
	    if (NativeMatchType(str, len, nativeName, types)) {
		Tcl_ListObjAppendElement(interp, resultPtr, pathPtr);
	    }
	}
	return TCL_OK;
    } else {
	char drivePat[] = "?:\\";
	const char *message;
	CONST char *dir;
	char *root;
	int dirLength;
	Tcl_DString dirString;
	DWORD attr, volFlags;
	HANDLE handle;
	WIN32_FIND_DATAT data;
	BOOL found;
	Tcl_DString ds;
	Tcl_DString dsOrig;
	Tcl_Obj *fileNamePtr;
	int matchSpecialDots;
	
	/*
	 * Convert the path to normalized form since some interfaces only
	 * accept backslashes.  Also, ensure that the directory ends with a
	 * separator character.
	 */

	fileNamePtr = Tcl_FSGetTranslatedPath(interp, pathPtr);
	if (fileNamePtr == NULL) {
	    return TCL_ERROR;
	}
	Tcl_DStringInit(&dsOrig);
	Tcl_DStringAppend(&dsOrig, Tcl_GetString(fileNamePtr), -1);

	dirLength = Tcl_DStringLength(&dsOrig);
	Tcl_DStringInit(&dirString);
	if (dirLength == 0) {
	    Tcl_DStringAppend(&dirString, ".\\", 2);
	} else {
	    char *p;

	    Tcl_DStringAppend(&dirString, Tcl_DStringValue(&dsOrig),
		    Tcl_DStringLength(&dsOrig));
	    for (p = Tcl_DStringValue(&dirString); *p != '\0'; p++) {
		if (*p == '/') {
		    *p = '\\';
		}
	    }
	    p--;
	    /* Make sure we have a trailing directory delimiter */
	    if ((*p != '\\') && (*p != ':')) {
		Tcl_DStringAppend(&dirString, "\\", 1);
		Tcl_DStringAppend(&dsOrig, "/", 1);
		dirLength++;
	    }
	}
	dir = Tcl_DStringValue(&dirString);

	/*
	 * First verify that the specified path is actually a directory.
	 */

	nativeName = Tcl_WinUtfToTChar(dir, Tcl_DStringLength(&dirString), &ds);
	attr = (*tclWinProcs->getFileAttributesProc)(nativeName);
	Tcl_DStringFree(&ds);

	if ((attr == 0xffffffff) || ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
	    Tcl_DStringFree(&dirString);
	    return TCL_OK;
	}

	/*
	 * Next check the volume information for the directory to see
	 * whether comparisons should be case sensitive or not.  If the
	 * root is null, then we use the root of the current directory.
	 * If the root is just a drive specifier, we use the root
	 * directory of the given drive.
	 */

	switch (Tcl_GetPathType(dir)) {
	    case TCL_PATH_RELATIVE:
		found = GetVolumeInformationA(NULL, NULL, 0, NULL, NULL, 
			&volFlags, NULL, 0);
		break;
	    case TCL_PATH_VOLUME_RELATIVE:
		if (dir[0] == '\\') {
		    root = NULL;
		} else {
		    root = drivePat;
		    *root = dir[0];
		}
		found = GetVolumeInformationA(root, NULL, 0, NULL, NULL, 
			&volFlags, NULL, 0);
		break;
	    case TCL_PATH_ABSOLUTE:
		if (dir[1] == ':') {
		    root = drivePat;
		    *root = dir[0];
		    found = GetVolumeInformationA(root, NULL, 0, NULL, NULL, 
			    &volFlags, NULL, 0);
		} else if (dir[1] == '\\') {
		    char *p;

		    p = strchr(dir + 2, '\\');
		    p = strchr(p + 1, '\\');
		    p++;
		    nativeName = Tcl_WinUtfToTChar(dir, p - dir, &ds);
		    found = (*tclWinProcs->getVolumeInformationProc)(nativeName, 
			    NULL, 0, NULL, NULL, &volFlags, NULL, 0);
		    Tcl_DStringFree(&ds);
		}
		break;
	}

	if (found == 0) {
	    message = "couldn't read volume information for \"";
	    goto error;
	}

	/*
	 * Check to see if the pattern should match the special
	 * . and .. names, referring to the current directory,
	 * or the directory above.  We need a special check for
	 * this because paths beginning with a dot are not considered
	 * hidden on Windows, and so otherwise a relative glob like
	 * 'glob -join * *' will actually return './. ../..' etc.
	 */

	if ((pattern[0] == '.')
		|| ((pattern[0] == '\\') && (pattern[1] == '.'))) {
	    matchSpecialDots = 1;
	} else {
	    matchSpecialDots = 0;
	}

	/*
	 * We need to check all files in the directory, so append a *.*
	 * to the path. 
	 */

	dir = Tcl_DStringAppend(&dirString, "*.*", 3);
	nativeName = Tcl_WinUtfToTChar(dir, -1, &ds);
	handle = (*tclWinProcs->findFirstFileProc)(nativeName, &data);
	Tcl_DStringFree(&ds);

	if (handle == INVALID_HANDLE_VALUE) {
	    message = "couldn't read directory \"";
	    goto error;
	}

	/*
	 * Now iterate over all of the files in the directory.
	 */

	for (found = 1; found != 0; 
		found = (*tclWinProcs->findNextFileProc)(handle, &data)) {
	    CONST TCHAR *nativeMatchResult;
	    CONST char *name, *fname;
	    
	    if (tclWinProcs->useWide) {
		nativeName = (CONST TCHAR *) data.w.cFileName;
	    } else {
		nativeName = (CONST TCHAR *) data.a.cFileName;
	    }
	    name = Tcl_WinTCharToUtf(nativeName, -1, &ds);

	    if (!matchSpecialDots) {
		/* If it is exactly '.' or '..' then we ignore it */
		if (name[0] == '.') {
		    if (name[1] == '\0' 
		      || (name[1] == '.' && name[2] == '\0')) {
			continue;
		    }
		}
	    }
	    
	    /*
	     * Check to see if the file matches the pattern.  Note that
	     * we are ignoring the case sensitivity flag because Windows
	     * doesn't honor case even if the volume is case sensitive.
	     * If the volume also doesn't preserve case, then we
	     * previously returned the lower case form of the name.  This
	     * didn't seem quite right since there are
	     * non-case-preserving volumes that actually return mixed
	     * case.  So now we are returning exactly what we get from
	     * the system.
	     */

	    nativeMatchResult = NULL;

	    if (Tcl_StringCaseMatch(name, pattern, 1) != 0) {
		nativeMatchResult = nativeName;
	    }
	    Tcl_DStringFree(&ds);

	    if (nativeMatchResult == NULL) {
		continue;
	    }

	    /*
	     * If the file matches, then we need to process the remainder
	     * of the path.
	     */

	    name = Tcl_WinTCharToUtf(nativeMatchResult, -1, &ds);
	    Tcl_DStringAppend(&dsOrig, name, -1);
	    Tcl_DStringFree(&ds);

	    fname = Tcl_DStringValue(&dsOrig);
	    nativeName = Tcl_WinUtfToTChar(fname, Tcl_DStringLength(&dsOrig), 
					   &ds);
	    
	    if (NativeMatchType(fname, Tcl_DStringLength(&dsOrig), 
				nativeName, types)) {
		Tcl_ListObjAppendElement(interp, resultPtr, 
			Tcl_NewStringObj(fname, Tcl_DStringLength(&dsOrig)));
	    }
	    /*
	     * Free ds here to ensure that nativeName is valid above.
	     */

	    Tcl_DStringFree(&ds);

	    Tcl_DStringSetLength(&dsOrig, dirLength);
	}

	FindClose(handle);
	Tcl_DStringFree(&dirString);
	Tcl_DStringFree(&dsOrig);

	return TCL_OK;
	
        error:
	Tcl_DStringFree(&dirString);
	TclWinConvertError(GetLastError());
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, message, Tcl_DStringValue(&dsOrig), "\": ", 
			 Tcl_PosixError(interp), (char *) NULL);
			 Tcl_DStringFree(&dsOrig);
	return TCL_ERROR;
    }

}

/* 
 * Does the given path represent a root volume?  We need this special
 * case because for NTFS root volumes, the getFileAttributesProc returns
 * a 'hidden' attribute when it should not.
 */
static int
WinIsDrive(
    CONST char *name,     /* Name (UTF-8) */
    int len)              /* Length of name */
{
    int remove = 0;
    while (len > 4) {
        if ((name[len-1] != '.' || name[len-2] != '.') 
	    || (name[len-3] != '/' && name[len-3] != '\\')) {
            /* We don't have '/..' at the end */
	    if (remove == 0) {
	        break;
	    }
	    remove--;
	    while (len > 0) {
		len--;
		if (name[len] == '/' || name[len] == '\\') {
		    break;
		}
	    }
	    if (len < 4) {
	        len++;
		break;
	    }
        } else {
	    /* We do have '/..' */
	    len -= 3;
	    remove++;
        }
    }
    if (len < 4) {
	if (len == 0) {
	    /* 
	     * Not sure if this is possible, but we pass it on
	     * anyway 
	     */
	} else if (len == 1 && (name[0] == '/' || name[0] == '\\')) {
	    /* Path is pointing to the root volume */
	    return 1;
	} else if ((name[1] == ':') 
		   && (len == 2 || (name[2] == '/' || name[2] == '\\'))) {
	    /* Path is of the form 'x:' or 'x:/' or 'x:\' */
	    return 1;
	}
    }
    return 0;
}
	   

/* 
 * This function needs a special case for a path which is a root
 * volume, because for NTFS root volumes, the getFileAttributesProc
 * returns a 'hidden' attribute when it should not.
 */
static int 
NativeMatchType(
    CONST char *name,         /* Name */
    int nameLen,              /* Length of name */
    CONST TCHAR* nativeName,  /* Native path to check */
    Tcl_GlobTypeData *types)  /* Type description to match against */
{
    /*
     * 'attr' represents the attributes of the file, but we only
     * want to retrieve this info if it is absolutely necessary
     * because it is an expensive call.  Unfortunately, to deal
     * with hidden files properly, we must always retrieve it.
     * There are more modern Win32 APIs available which we should
     * look into.
     */

    DWORD attr = (*tclWinProcs->getFileAttributesProc)(nativeName);
    if (attr == 0xffffffff) {
	/* File doesn't exist */
	return 0;
    }
    
    if (types == NULL) {
	/* If invisible, don't return the file */
	if (attr & FILE_ATTRIBUTE_HIDDEN && !WinIsDrive(name, nameLen)) {
	    return 0;
	}
    } else {
	if (attr & FILE_ATTRIBUTE_HIDDEN && !WinIsDrive(name, nameLen)) {
	    /* If invisible */
	    if ((types->perm == 0) || 
	      !(types->perm & TCL_GLOB_PERM_HIDDEN)) {
		return 0;
	    }
	} else {
	    /* Visible */
	    if (types->perm & TCL_GLOB_PERM_HIDDEN) {
		return 0;
	    }
	}
	
	if (types->perm != 0) {
	    if (
		((types->perm & TCL_GLOB_PERM_RONLY) &&
			!(attr & FILE_ATTRIBUTE_READONLY)) ||
		((types->perm & TCL_GLOB_PERM_R) &&
			(NativeAccess(nativeName, R_OK) != 0)) ||
		((types->perm & TCL_GLOB_PERM_W) &&
			(NativeAccess(nativeName, W_OK) != 0)) ||
		((types->perm & TCL_GLOB_PERM_X) &&
			(NativeAccess(nativeName, X_OK) != 0))
		) {
		return 0;
	    }
	}
	if (types->type != 0) {
	    Tcl_StatBuf buf;
	    
	    if (NativeStat(nativeName, &buf) != 0) {
		/* 
		 * Posix error occurred, either the file
		 * has disappeared, or there is some other
		 * strange error.  In any case we don't
		 * return this file.
		 */
		return 0;
	    }
	    /*
	     * In order bcdpfls as in 'find -t'
	     */
	    if (
		((types->type & TCL_GLOB_TYPE_BLOCK) &&
			S_ISBLK(buf.st_mode)) ||
		((types->type & TCL_GLOB_TYPE_CHAR) &&
			S_ISCHR(buf.st_mode)) ||
		((types->type & TCL_GLOB_TYPE_DIR) &&
			S_ISDIR(buf.st_mode)) ||
		((types->type & TCL_GLOB_TYPE_PIPE) &&
			S_ISFIFO(buf.st_mode)) ||
		((types->type & TCL_GLOB_TYPE_FILE) &&
			S_ISREG(buf.st_mode))
#ifdef S_ISSOCK
		|| ((types->type & TCL_GLOB_TYPE_SOCK) &&
			S_ISSOCK(buf.st_mode))
#endif
		) {
		/* Do nothing -- this file is ok */
	    } else {
#ifdef S_ISLNK
		if (types->type & TCL_GLOB_TYPE_LINK) {
		    /* 
		     * We should use 'lstat' but it is the
		     * same as 'stat' on windows.
		     */
		    if (NativeStat(nativeName, &buf) == 0) {
			if (S_ISLNK(buf.st_mode)) {
			    return 1;
			}
		    }
		}
#endif
		return 0;
	    }
	}		
    } 
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpGetUserHome --
 *
 *	This function takes the passed in user name and finds the
 *	corresponding home directory specified in the password file.
 *
 * Results:
 *	The result is a pointer to a string specifying the user's home
 *	directory, or NULL if the user's home directory could not be
 *	determined.  Storage for the result string is allocated in
 *	bufferPtr; the caller must call Tcl_DStringFree() when the result
 *	is no longer needed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclpGetUserHome(name, bufferPtr)
    CONST char *name;		/* User name for desired home directory. */
    Tcl_DString *bufferPtr;	/* Uninitialized or free DString filled
				 * with name of user's home directory. */
{
    char *result;
    HINSTANCE netapiInst;

    result = NULL;

    Tcl_DStringInit(bufferPtr);

    netapiInst = LoadLibraryA("netapi32.dll");
    if (netapiInst != NULL) {
	NETAPIBUFFERFREEPROC *netApiBufferFreeProc;
	NETGETDCNAMEPROC *netGetDCNameProc;
	NETUSERGETINFOPROC *netUserGetInfoProc;

	netApiBufferFreeProc = (NETAPIBUFFERFREEPROC *)
		GetProcAddress(netapiInst, "NetApiBufferFree");
	netGetDCNameProc = (NETGETDCNAMEPROC *) 
		GetProcAddress(netapiInst, "NetGetDCName");
	netUserGetInfoProc = (NETUSERGETINFOPROC *) 
		GetProcAddress(netapiInst, "NetUserGetInfo");
	if ((netUserGetInfoProc != NULL) && (netGetDCNameProc != NULL)
		&& (netApiBufferFreeProc != NULL)) {
	    USER_INFO_1 *uiPtr;
	    Tcl_DString ds;
	    int nameLen, badDomain;
	    char *domain;
	    WCHAR *wName, *wHomeDir, *wDomain;
	    WCHAR buf[MAX_PATH];

	    badDomain = 0;
	    nameLen = -1;
	    wDomain = NULL;
	    domain = strchr(name, '@');
	    if (domain != NULL) {
		Tcl_DStringInit(&ds);
		wName = Tcl_UtfToUniCharDString(domain + 1, -1, &ds);
		badDomain = (*netGetDCNameProc)(NULL, wName,
			(LPBYTE *) &wDomain);
		Tcl_DStringFree(&ds);
		nameLen = domain - name;
	    }
	    if (badDomain == 0) {
		Tcl_DStringInit(&ds);
		wName = Tcl_UtfToUniCharDString(name, nameLen, &ds);
		if ((*netUserGetInfoProc)(wDomain, wName, 1,
			(LPBYTE *) &uiPtr) == 0) {
		    wHomeDir = uiPtr->usri1_home_dir;
		    if ((wHomeDir != NULL) && (wHomeDir[0] != L'\0')) {
			Tcl_UniCharToUtfDString(wHomeDir, lstrlenW(wHomeDir),
				bufferPtr);
		    } else {
			/* 
			 * User exists but has no home dir.  Return
			 * "{Windows Drive}:/users/default".
			 */

			GetWindowsDirectoryW(buf, MAX_PATH);
			Tcl_UniCharToUtfDString(buf, 2, bufferPtr);
			Tcl_DStringAppend(bufferPtr, "/users/default", -1);
		    }
		    result = Tcl_DStringValue(bufferPtr);
		    (*netApiBufferFreeProc)((void *) uiPtr);
		}
		Tcl_DStringFree(&ds);
	    }
	    if (wDomain != NULL) {
		(*netApiBufferFreeProc)((void *) wDomain);
	    }
	}
	FreeLibrary(netapiInst);
    }
    if (result == NULL) {
	/*
	 * Look in the "Password Lists" section of system.ini for the 
	 * local user.  There are also entries in that section that begin 
	 * with a "*" character that are used by Windows for other 
	 * purposes; ignore user names beginning with a "*".
	 */

	char buf[MAX_PATH];

	if (name[0] != '*') {
	    if (GetPrivateProfileStringA("Password Lists", name, "", buf, 
		    MAX_PATH, "system.ini") > 0) {
		/* 
		 * User exists, but there is no such thing as a home 
		 * directory in system.ini.  Return "{Windows drive}:/".
		 */

		GetWindowsDirectoryA(buf, MAX_PATH);
		Tcl_DStringAppend(bufferPtr, buf, 3);
		result = Tcl_DStringValue(bufferPtr);
	    }
	}
    }

    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * NativeAccess --
 *
 *	This function replaces the library version of access(), fixing the
 *	following bugs:
 * 
 *	1. access() returns that all files have execute permission.
 *
 * Results:
 *	See access documentation.
 *
 * Side effects:
 *	See access documentation.
 *
 *---------------------------------------------------------------------------
 */

static int
NativeAccess(
    CONST TCHAR *nativePath,	/* Path of file to access (UTF-8). */
    int mode)			/* Permission setting. */
{
    DWORD attr;

    attr = (*tclWinProcs->getFileAttributesProc)(nativePath);

    if (attr == 0xffffffff) {
	/*
	 * File doesn't exist. 
	 */

	TclWinConvertError(GetLastError());
	return -1;
    }

    if ((mode & W_OK) && (attr & FILE_ATTRIBUTE_READONLY)) {
	/*
	 * File is not writable.
	 */

	Tcl_SetErrno(EACCES);
	return -1;
    }

    if (mode & X_OK) {
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
	    /*
	     * Directories are always executable. 
	     */
	    
	    return 0;
	}
	if (NativeIsExec(nativePath)) {
	    return 0;
	}
	Tcl_SetErrno(EACCES);
	return -1;
    }

    return 0;
}

static int
NativeIsExec(nativePath)
    CONST TCHAR *nativePath;
{
    CONST char *p, *path;
    Tcl_DString ds;
    
    /* 
     * This is really not efficient.  We should be able to examine
     * the native path directly without converting to UTF.
     */
    Tcl_DStringInit(&ds);
    path = Tcl_WinTCharToUtf(nativePath, -1, &ds);
    
    p = strrchr(path, '.');
    if (p != NULL) {
	p++;
	/* 
	 * Note: in the old code, stat considered '.pif' files as
	 * executable, whereas access did not.
	 */
	if ((stricmp(p, "exe") == 0)
		|| (stricmp(p, "com") == 0)
		|| (stricmp(p, "bat") == 0)) {
	    /*
	     * File that ends with .exe, .com, or .bat is executable.
	     */

	    Tcl_DStringFree(&ds);
	    return 1;
	}
    }
    Tcl_DStringFree(&ds);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpObjChdir --
 *
 *	This function replaces the library version of chdir().
 *
 * Results:
 *	See chdir() documentation.
 *
 * Side effects:
 *	See chdir() documentation.  
 *
 *----------------------------------------------------------------------
 */

int 
TclpObjChdir(pathPtr)
    Tcl_Obj *pathPtr; 	/* Path to new working directory. */
{
    int result;
    CONST TCHAR *nativePath;

    nativePath = (CONST TCHAR *) Tcl_FSGetNativePath(pathPtr);
    result = (*tclWinProcs->setCurrentDirectoryProc)(nativePath);

    if (result == 0) {
	TclWinConvertError(GetLastError());
	return -1;
    }
    return 0;
}

#ifdef __CYGWIN__
/*
 *---------------------------------------------------------------------------
 *
 * TclpReadlink --
 *
 *     This function replaces the library version of readlink().
 *
 * Results:
 *     The result is a pointer to a string specifying the contents
 *     of the symbolic link given by 'path', or NULL if the symbolic
 *     link could not be read.  Storage for the result string is
 *     allocated in bufferPtr; the caller must call Tcl_DStringFree()
 *     when the result is no longer needed.
 *
 * Side effects:
 *     See readlink() documentation.
 *
 *---------------------------------------------------------------------------
 */

char *
TclpReadlink(path, linkPtr)
    CONST char *path;          /* Path of file to readlink (UTF-8). */
    Tcl_DString *linkPtr;      /* Uninitialized or free DString filled
                                * with contents of link (UTF-8). */
{
    char link[MAXPATHLEN];
    int length;
    char *native;
    Tcl_DString ds;

    native = Tcl_UtfToExternalDString(NULL, path, -1, &ds);
    length = readlink(native, link, sizeof(link));     /* INTL: Native. */
    Tcl_DStringFree(&ds);
    
    if (length < 0) {
       return NULL;
    }

    Tcl_ExternalToUtfDString(NULL, link, length, linkPtr);
    return Tcl_DStringValue(linkPtr);
}
#endif /* __CYGWIN__ */

/*
 *----------------------------------------------------------------------
 *
 * TclpGetCwd --
 *
 *	This function replaces the library version of getcwd().
 *
 * Results:
 *	The result is a pointer to a string specifying the current
 *	directory, or NULL if the current directory could not be
 *	determined.  If NULL is returned, an error message is left in the
 *	interp's result.  Storage for the result string is allocated in
 *	bufferPtr; the caller must call Tcl_DStringFree() when the result
 *	is no longer needed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CONST char *
TclpGetCwd(interp, bufferPtr)
    Tcl_Interp *interp;		/* If non-NULL, used for error reporting. */
    Tcl_DString *bufferPtr;	/* Uninitialized or free DString filled
				 * with name of current directory. */
{
    WCHAR buffer[MAX_PATH];
    char *p;

    if ((*tclWinProcs->getCurrentDirectoryProc)(MAX_PATH, buffer) == 0) {
	TclWinConvertError(GetLastError());
	if (interp != NULL) {
	    Tcl_AppendResult(interp,
		    "error getting working directory name: ",
		    Tcl_PosixError(interp), (char *) NULL);
	}
	return NULL;
    }

    /*
     * Watch for the weird Windows c:\\UNC syntax.
     */

    if (tclWinProcs->useWide) {
	WCHAR *native;

	native = (WCHAR *) buffer;
	if ((native[0] != '\0') && (native[1] == ':') 
		&& (native[2] == '\\') && (native[3] == '\\')) {
	    native += 2;
	}
	Tcl_WinTCharToUtf((TCHAR *) native, -1, bufferPtr);
    } else {
	char *native;

	native = (char *) buffer;
	if ((native[0] != '\0') && (native[1] == ':') 
		&& (native[2] == '\\') && (native[3] == '\\')) {
	    native += 2;
	}
	Tcl_WinTCharToUtf((TCHAR *) native, -1, bufferPtr);
    }

    /*
     * Convert to forward slashes for easier use in scripts.
     */
	      
    for (p = Tcl_DStringValue(bufferPtr); *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    return Tcl_DStringValue(bufferPtr);
}

int 
TclpObjStat(pathPtr, statPtr)
    Tcl_Obj *pathPtr;          /* Path of file to stat */
    Tcl_StatBuf *statPtr;      /* Filled with results of stat call. */
{
#ifdef OLD_API
    Tcl_Obj *transPtr;
    /*
     * Eliminate file names containing wildcard characters, or subsequent 
     * call to FindFirstFile() will expand them, matching some other file.
     */

    transPtr = Tcl_FSGetTranslatedPath(NULL, pathPtr);
    if (transPtr == NULL || (strpbrk(Tcl_GetString(transPtr), "?*") != NULL)) {
	Tcl_SetErrno(ENOENT);
	return -1;
    }
#endif
    
    /*
     * Ensure correct file sizes by forcing the OS to write any
     * pending data to disk. This is done only for channels which are
     * dirty, i.e. have been written to since the last flush here.
     */

    TclWinFlushDirtyChannels ();

    return NativeStat((CONST TCHAR*) Tcl_FSGetNativePath(pathPtr), statPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NativeStat --
 *
 *	This function replaces the library version of stat(), fixing 
 *	the following bugs:
 *
 *	1. stat("c:") returns an error.
 *	2. Borland stat() return time in GMT instead of localtime.
 *	3. stat("\\server\mount") would return error.
 *	4. Accepts slashes or backslashes.
 *	5. st_dev and st_rdev were wrong for UNC paths.
 *
 * Results:
 *	See stat documentation.
 *
 * Side effects:
 *	See stat documentation.
 *
 *----------------------------------------------------------------------
 */

static int 
NativeStat(nativePath, statPtr)
    CONST TCHAR *nativePath;   /* Path of file to stat */
    Tcl_StatBuf *statPtr;      /* Filled with results of stat call. */
{
    Tcl_DString ds;
    DWORD attr;
    WCHAR nativeFullPath[MAX_PATH];
    TCHAR *nativePart;
    CONST char *fullPath;
    int dev, mode;
    
    if (tclWinProcs->getFileAttributesExProc == NULL) {
        /* 
         * We don't have the faster attributes proc, so we're
         * probably running on Win95
         */
	WIN32_FIND_DATAT data;
	HANDLE handle;

	handle = (*tclWinProcs->findFirstFileProc)(nativePath, &data);
	if (handle == INVALID_HANDLE_VALUE) {
	    /* 
	     * FindFirstFile() doesn't work on root directories, so call
	     * GetFileAttributes() to see if the specified file exists.
	     */

	    attr = (*tclWinProcs->getFileAttributesProc)(nativePath);
	    if (attr == 0xffffffff) {
		Tcl_SetErrno(ENOENT);
		return -1;
	    }

	    /* 
	     * Make up some fake information for this file.  It has the 
	     * correct file attributes and a time of 0.
	     */

	    memset(&data, 0, sizeof(data));
	    data.a.dwFileAttributes = attr;
	} else {
	    FindClose(handle);
	}

    
	(*tclWinProcs->getFullPathNameProc)(nativePath, MAX_PATH, nativeFullPath,
		&nativePart);

	fullPath = Tcl_WinTCharToUtf((TCHAR *) nativeFullPath, -1, &ds);

	dev = -1;
	if ((fullPath[0] == '\\') && (fullPath[1] == '\\')) {
	    CONST char *p;
	    DWORD dw;
	    CONST TCHAR *nativeVol;
	    Tcl_DString volString;

	    p = strchr(fullPath + 2, '\\');
	    p = strchr(p + 1, '\\');
	    if (p == NULL) {
		/*
		 * Add terminating backslash to fullpath or 
		 * GetVolumeInformation() won't work.
		 */

		fullPath = Tcl_DStringAppend(&ds, "\\", 1);
		p = fullPath + Tcl_DStringLength(&ds);
	    } else {
		p++;
	    }
	    nativeVol = Tcl_WinUtfToTChar(fullPath, p - fullPath, &volString);
	    dw = (DWORD) -1;
	    (*tclWinProcs->getVolumeInformationProc)(nativeVol, NULL, 0, &dw,
		    NULL, NULL, NULL, 0);
	    /*
	     * GetFullPathName() turns special devices like "NUL" into
	     * "\\.\NUL", but GetVolumeInformation() returns failure for
	     * "\\.\NUL".  This will cause "NUL" to get a drive number of
	     * -1, which makes about as much sense as anything since the
	     * special devices don't live on any drive.
	     */

	    dev = dw;
	    Tcl_DStringFree(&volString);
	} else if ((fullPath[0] != '\0') && (fullPath[1] == ':')) {
	    dev = Tcl_UniCharToLower(fullPath[0]) - 'a';
	}
	Tcl_DStringFree(&ds);
	
	attr = data.a.dwFileAttributes;

	statPtr->st_size  = ((Tcl_WideInt)data.a.nFileSizeLow) |
		(((Tcl_WideInt)data.a.nFileSizeHigh) << 32);
	statPtr->st_atime = ToCTime(data.a.ftLastAccessTime);
	statPtr->st_mtime = ToCTime(data.a.ftLastWriteTime);
	statPtr->st_ctime = ToCTime(data.a.ftCreationTime);
    } else {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if((*tclWinProcs->getFileAttributesExProc)(nativePath,
						   GetFileExInfoStandard,
						   &data) != TRUE) {
	    Tcl_SetErrno(ENOENT);
	    return -1;
	}

    
	(*tclWinProcs->getFullPathNameProc)(nativePath, MAX_PATH, 
					    nativeFullPath, &nativePart);

	fullPath = Tcl_WinTCharToUtf((TCHAR *) nativeFullPath, -1, &ds);

	dev = -1;
	if ((fullPath[0] == '\\') && (fullPath[1] == '\\')) {
	    CONST char *p;
	    DWORD dw;
	    CONST TCHAR *nativeVol;
	    Tcl_DString volString;

	    p = strchr(fullPath + 2, '\\');
	    p = strchr(p + 1, '\\');
	    if (p == NULL) {
		/*
		 * Add terminating backslash to fullpath or 
		 * GetVolumeInformation() won't work.
		 */

		fullPath = Tcl_DStringAppend(&ds, "\\", 1);
		p = fullPath + Tcl_DStringLength(&ds);
	    } else {
		p++;
	    }
	    nativeVol = Tcl_WinUtfToTChar(fullPath, p - fullPath, &volString);
	    dw = (DWORD) -1;
	    (*tclWinProcs->getVolumeInformationProc)(nativeVol, NULL, 0, &dw,
		    NULL, NULL, NULL, 0);
	    /*
	     * GetFullPathName() turns special devices like "NUL" into
	     * "\\.\NUL", but GetVolumeInformation() returns failure for
	     * "\\.\NUL".  This will cause "NUL" to get a drive number of
	     * -1, which makes about as much sense as anything since the
	     * special devices don't live on any drive.
	     */

	    dev = dw;
	    Tcl_DStringFree(&volString);
	} else if ((fullPath[0] != '\0') && (fullPath[1] == ':')) {
	    dev = Tcl_UniCharToLower(fullPath[0]) - 'a';
	}
	Tcl_DStringFree(&ds);
	
	attr = data.dwFileAttributes;
	
	statPtr->st_size  = ((Tcl_WideInt)data.nFileSizeLow) |
		(((Tcl_WideInt)data.nFileSizeHigh) << 32);
	statPtr->st_atime = ToCTime(data.ftLastAccessTime);
	statPtr->st_mtime = ToCTime(data.ftLastWriteTime);
	statPtr->st_ctime = ToCTime(data.ftCreationTime);
    }

    mode  = (attr & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR | S_IEXEC : S_IFREG;
    mode |= (attr & FILE_ATTRIBUTE_READONLY) ? S_IREAD : S_IREAD | S_IWRITE;
    if (NativeIsExec(nativePath)) {
	mode |= S_IEXEC;
    }

    /*
     * Propagate the S_IREAD, S_IWRITE, S_IEXEC bits to the group and 
     * other positions.
     */

    mode |= (mode & 0x0700) >> 3;
    mode |= (mode & 0x0700) >> 6;
    
    statPtr->st_dev	= (dev_t) dev;
    statPtr->st_ino	= 0;
    statPtr->st_mode	= (unsigned short) mode;
    statPtr->st_nlink	= 1;
    statPtr->st_uid	= 0;
    statPtr->st_gid	= 0;
    statPtr->st_rdev	= (dev_t) dev;
    return 0;
}

static time_t
ToCTime(
    FILETIME fileTime)		/* UTC Time to convert to local time_t. */
{
    FILETIME localFileTime;
    SYSTEMTIME systemTime;
    struct tm tm;

    if (FileTimeToLocalFileTime(&fileTime, &localFileTime) == 0) {
	return 0;
    }
    if (FileTimeToSystemTime(&localFileTime, &systemTime) == 0) {
	return 0;
    }
    tm.tm_sec = systemTime.wSecond;
    tm.tm_min = systemTime.wMinute;
    tm.tm_hour = systemTime.wHour;
    tm.tm_mday = systemTime.wDay;
    tm.tm_mon = systemTime.wMonth - 1;
    tm.tm_year = systemTime.wYear - 1900;
    tm.tm_wday = 0;
    tm.tm_yday = 0;
    tm.tm_isdst = -1;

    return mktime(&tm);
}

#if 0

    /*
     * Borland's stat doesn't take into account localtime.
     */

    if ((result == 0) && (buf->st_mtime != 0)) {
	TIME_ZONE_INFORMATION tz;
	int time, bias;

	time = GetTimeZoneInformation(&tz);
	bias = tz.Bias;
	if (time == TIME_ZONE_ID_DAYLIGHT) {
	    bias += tz.DaylightBias;
	}
	bias *= 60;
	buf->st_atime -= bias;
	buf->st_ctime -= bias;
	buf->st_mtime -= bias;
    }

#endif


#if 0
/*
 *-------------------------------------------------------------------------
 *
 * TclWinResolveShortcut --
 *
 *	Resolve a potential Windows shortcut to get the actual file or 
 *	directory in question.  
 *
 * Results:
 *	Returns 1 if the shortcut could be resolved, or 0 if there was
 *	an error or if the filename was not a shortcut.
 *	If bufferPtr did hold the name of a shortcut, it is modified to
 *	hold the resolved target of the shortcut instead.
 *
 * Side effects:
 *	Loads and unloads OLE package to determine if filename refers to
 *	a shortcut.
 *
 *-------------------------------------------------------------------------
 */

int
TclWinResolveShortcut(bufferPtr)
    Tcl_DString *bufferPtr;	/* Holds name of file to resolve.  On 
				 * return, holds resolved file name. */
{
    HRESULT hres; 
    IShellLink *psl; 
    IPersistFile *ppf; 
    WIN32_FIND_DATA wfd; 
    WCHAR wpath[MAX_PATH];
    char *path, *ext;
    char realFileName[MAX_PATH];

    /*
     * Windows system calls do not automatically resolve
     * shortcuts like UNIX automatically will with symbolic links.
     */

    path = Tcl_DStringValue(bufferPtr);
    ext = strrchr(path, '.');
    if ((ext == NULL) || (stricmp(ext, ".lnk") != 0)) {
	return 0;
    }

    CoInitialize(NULL);
    path = Tcl_DStringValue(bufferPtr);
    realFileName[0] = '\0';
    hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, 
	    &IID_IShellLink, &psl); 
    if (SUCCEEDED(hres)) { 
	hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, &ppf);
	if (SUCCEEDED(hres)) { 
	    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, sizeof(wpath));
	    hres = ppf->lpVtbl->Load(ppf, wpath, STGM_READ); 
	    if (SUCCEEDED(hres)) {
		hres = psl->lpVtbl->Resolve(psl, NULL, 
			SLR_ANY_MATCH | SLR_NO_UI); 
		if (SUCCEEDED(hres)) { 
		    hres = psl->lpVtbl->GetPath(psl, realFileName, MAX_PATH, 
			    &wfd, 0);
		} 
	    } 
	    ppf->lpVtbl->Release(ppf); 
	} 
	psl->lpVtbl->Release(psl); 
    } 
    CoUninitialize();

    if (realFileName[0] != '\0') {
	Tcl_DStringSetLength(bufferPtr, 0);
	Tcl_DStringAppend(bufferPtr, realFileName, -1);
	return 1;
    }
    return 0;
}
#endif

Tcl_Obj* 
TclpObjGetCwd(interp)
    Tcl_Interp *interp;
{
    Tcl_DString ds;
    if (TclpGetCwd(interp, &ds) != NULL) {
	Tcl_Obj *cwdPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	Tcl_IncrRefCount(cwdPtr);
	Tcl_DStringFree(&ds);
	return cwdPtr;
    } else {
	return NULL;
    }
}

int 
TclpObjAccess(pathPtr, mode)
    Tcl_Obj *pathPtr;
    int mode;
{
    return NativeAccess((CONST TCHAR*) Tcl_FSGetNativePath(pathPtr), mode);
}

int 
TclpObjLstat(pathPtr, buf)
    Tcl_Obj *pathPtr;
    Tcl_StatBuf *buf; 
{
    return TclpObjStat(pathPtr,buf);
}

#ifdef S_IFLNK

Tcl_Obj* 
TclpObjLink(pathPtr, toPtr)
    Tcl_Obj *pathPtr;
    Tcl_Obj *toPtr;
{
    Tcl_Obj* link = NULL;

    if (toPtr != NULL) {
	return NULL;
    } else {
	Tcl_DString ds;
	if (TclpReadlink(Tcl_FSGetTranslatedStringPath(NULL, pathPtr), &ds) 
	  != NULL) {
	    link = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1);
	    Tcl_IncrRefCount(link);
	    Tcl_DStringFree(&ds);
	}
    }
    return link;
}

#endif


/*
 *---------------------------------------------------------------------------
 *
 * TclpFilesystemPathType --
 *
 *      This function is part of the native filesystem support, and
 *      returns the path type of the given path.  Returns NTFS or FAT
 *      or whatever is returned by the 'volume information' proc.
 *
 * Results:
 *      NULL at present.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj*
TclpFilesystemPathType(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
#define VOL_BUF_SIZE 32
    int found;
    char volType[VOL_BUF_SIZE];
    char* firstSeparator;
    CONST char *path;
    
    Tcl_Obj *normPath = Tcl_FSGetNormalizedPath(NULL, pathObjPtr);
    if (normPath == NULL) return NULL;
    path = Tcl_GetString(normPath);
    if (path == NULL) return NULL;
    
    firstSeparator = strchr(path, '/');
    if (firstSeparator == NULL) {
	found = tclWinProcs->getVolumeInformationProc(
		Tcl_FSGetNativePath(pathObjPtr), NULL, 0, NULL, NULL, 
		NULL, (WCHAR *)volType, VOL_BUF_SIZE);
    } else {
	Tcl_Obj *driveName = Tcl_NewStringObj(path, firstSeparator - path+1);
	Tcl_IncrRefCount(driveName);
	found = tclWinProcs->getVolumeInformationProc(
		Tcl_FSGetNativePath(driveName), NULL, 0, NULL, NULL, 
		NULL, (WCHAR *)volType, VOL_BUF_SIZE);
	Tcl_DecrRefCount(driveName);
    }

    if (found == 0) {
	return NULL;
    } else {
	Tcl_DString ds;
	Tcl_Obj *objPtr;
	
	Tcl_WinTCharToUtf(volType, -1, &ds);
	objPtr = Tcl_NewStringObj(Tcl_DStringValue(&ds),Tcl_DStringLength(&ds));
	Tcl_DStringFree(&ds);
	return objPtr;
    }
#undef VOL_BUF_SIZE
}
