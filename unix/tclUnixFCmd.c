/*
 * tclUnixFCmd.c
 *
 *	This file implements the Unix specific portion of file manipulation
 *	subcommands of the "file" command. All filename arguments should
 *	already be translated to native format.
 *
 * Copyright © 1996-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Portions of this code were derived from NetBSD source code which has the
 * following copyright notice:
 *
 * Copyright © 1988, 1993, 1994
 *      The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "tclInt.h"
#ifndef HAVE_STRUCT_STAT_ST_BLKSIZE
#ifndef NO_FSTATFS
#include <sys/statfs.h>
#endif
#endif /* !HAVE_STRUCT_STAT_ST_BLKSIZE */
#ifdef HAVE_FTS
#include <fts.h>
#endif

/*
 * The following constants specify the type of callback when
 * TraverseUnixTree() calls the traverseProc()
 */

#define DOTREE_PRED	1	/* pre-order directory */
#define DOTREE_POSTD	2	/* post-order directory */
#define DOTREE_F	3	/* regular file */

/*
 * Fallback temporary file location the temporary file generation code. Can be
 * overridden at compile time for when it is known that temp files can't be
 * written to /tmp (hello, iOS!).
 */

#ifndef TCL_TEMPORARY_FILE_DIRECTORY
#define TCL_TEMPORARY_FILE_DIRECTORY	"/tmp"
#endif

/*
 * Callbacks for file attributes code.
 */

static int		GetGroupAttribute(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj **attributePtrPtr);
static int		GetOwnerAttribute(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj **attributePtrPtr);
static int		GetPermissionsAttribute(Tcl_Interp *interp,
			    int objIndex, Tcl_Obj *fileName,
			    Tcl_Obj **attributePtrPtr);
static int		SetGroupAttribute(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj *attributePtr);
static int		SetOwnerAttribute(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj *attributePtr);
static int		SetPermissionsAttribute(Tcl_Interp *interp,
			    int objIndex, Tcl_Obj *fileName,
			    Tcl_Obj *attributePtr);
static int		GetModeFromPermString(Tcl_Interp *interp,
			    const char *modeStringPtr, mode_t *modePtr);
#if defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE) || defined(__CYGWIN__)
static int		GetUnixFileAttributes(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj **attributePtrPtr);
static int		SetUnixFileAttributes(Tcl_Interp *interp, int objIndex,
			    Tcl_Obj *fileName, Tcl_Obj *attributePtr);
#endif

/*
 * Prototype for the TraverseUnixTree callback function.
 */

typedef int (TraversalProc)(Tcl_DString *srcPtr, Tcl_DString *dstPtr,
	const Tcl_StatBuf *statBufPtr, int type, Tcl_DString *errorPtr);

/*
 * Constants and variables necessary for file attributes subcommand.
 *
 * IMPORTANT: The permissions attribute is assumed to be the third item (i.e.
 * to be indexed with '2' in arrays) in code in tclIOUtil.c and possibly
 * elsewhere in Tcl's core.
 */

#ifndef DJGPP

enum {
#if defined(__CYGWIN__)
    UNIX_ARCHIVE_ATTRIBUTE,
#endif
    UNIX_GROUP_ATTRIBUTE,
#if defined(__CYGWIN__)
    UNIX_HIDDEN_ATTRIBUTE,
#endif
    UNIX_OWNER_ATTRIBUTE, UNIX_PERMISSIONS_ATTRIBUTE,
#if defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE) || defined(__CYGWIN__)
    UNIX_READONLY_ATTRIBUTE,
#endif
#if defined(__CYGWIN__)
    UNIX_SYSTEM_ATTRIBUTE,
#endif
#ifdef MAC_OSX_TCL
    MACOSX_CREATOR_ATTRIBUTE, MACOSX_TYPE_ATTRIBUTE, MACOSX_HIDDEN_ATTRIBUTE,
    MACOSX_RSRCLENGTH_ATTRIBUTE,
#endif
    UNIX_INVALID_ATTRIBUTE
};

const char *const tclpFileAttrStrings[] = {
#if defined(__CYGWIN__)
    "-archive",
#endif
    "-group",
#if defined(__CYGWIN__)
    "-hidden",
#endif
    "-owner", "-permissions",
#if defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE) || defined(__CYGWIN__)
    "-readonly",
#endif
#if defined(__CYGWIN__)
    "-system",
#endif
#ifdef MAC_OSX_TCL
    "-creator", "-type", "-hidden", "-rsrclength",
#endif
    NULL
};

const TclFileAttrProcs tclpFileAttrProcs[] = {
#if defined(__CYGWIN__)
    {GetUnixFileAttributes, SetUnixFileAttributes},
#endif
    {GetGroupAttribute, SetGroupAttribute},
#if defined(__CYGWIN__)
    {GetUnixFileAttributes, SetUnixFileAttributes},
#endif
    {GetOwnerAttribute, SetOwnerAttribute},
    {GetPermissionsAttribute, SetPermissionsAttribute},
#if defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE) || defined(__CYGWIN__)
    {GetUnixFileAttributes, SetUnixFileAttributes},
#endif
#if defined(__CYGWIN__)
    {GetUnixFileAttributes, SetUnixFileAttributes},
#endif
#ifdef MAC_OSX_TCL
    {TclMacOSXGetFileAttribute,	TclMacOSXSetFileAttribute},
    {TclMacOSXGetFileAttribute,	TclMacOSXSetFileAttribute},
    {TclMacOSXGetFileAttribute,	TclMacOSXSetFileAttribute},
    {TclMacOSXGetFileAttribute,	TclMacOSXSetFileAttribute},
#endif
};
#endif /* DJGPP */

/*
 * This is the maximum number of consecutive readdir/unlink calls that can be
 * made (with no intervening rewinddir or closedir/opendir) before triggering
 * a bug that makes readdir return NULL even though some directory entries
 * have not been processed. The bug afflicts SunOS's readdir when applied to
 * ufs file systems and Darwin 6.5's (and OSX v.10.3.8's) HFS+. JH found the
 * Darwin readdir to reset at 147, so 130 is chosen to be conservative. We
 * can't do a general rewind on failure as NFS can create special files that
 * recreate themselves when you try and delete them. 8.4.8 added a solution
 * that was affected by a single such NFS file, this solution should not be
 * affected by less than THRESHOLD such files. [Bug 1034337]
 */

#define MAX_READDIR_UNLINK_THRESHOLD 130

/*
 * Declarations for local procedures defined in this file:
 */

static int		CopyFileAtts(const char *src,
			    const char *dst, const Tcl_StatBuf *statBufPtr);
static const char *	DefaultTempDir(void);
static int		DoCopyFile(const char *srcPtr, const char *dstPtr,
			    const Tcl_StatBuf *statBufPtr);
static int		DoCreateDirectory(const char *pathPtr);
static int		DoRemoveDirectory(Tcl_DString *pathPtr,
			    int recursive, Tcl_DString *errorPtr);
static int		DoRenameFile(const char *src, const char *dst);
static int		TraversalCopy(Tcl_DString *srcPtr,
			    Tcl_DString *dstPtr,
			    const Tcl_StatBuf *statBufPtr, int type,
			    Tcl_DString *errorPtr);
static int		TraversalDelete(Tcl_DString *srcPtr,
			    Tcl_DString *dstPtr,
			    const Tcl_StatBuf *statBufPtr, int type,
			    Tcl_DString *errorPtr);
static int		TraverseUnixTree(TraversalProc *traversalProc,
			    Tcl_DString *sourcePtr, Tcl_DString *destPtr,
			    Tcl_DString *errorPtr, int doRewind);

#ifdef PURIFY
/*
 * realpath and purify don't mix happily. It has been noted that realpath
 * should not be used with purify because of bogus warnings, but just
 * memset'ing the resolved path will squelch those. This assumes we are
 * passing the standard MAXPATHLEN size resolved arg.
 */

static char *		Realpath(const char *path, char *resolved);

char *
Realpath(
    const char *path,
    char *resolved)
{
    memset(resolved, 0, MAXPATHLEN);
    return realpath(path, resolved);
}
#else
#   define Realpath	realpath
#endif /* PURIFY */

#ifndef NO_REALPATH
#   define haveRealpath	1
#endif /* NO_REALPATH */

#ifdef HAVE_FTS
#if defined(HAVE_STRUCT_STAT64) && !defined(__APPLE__)
/* fts doesn't do stat64 */
#   define noFtsStat	1
#else
#   define noFtsStat	0
#endif
#endif /* HAVE_FTS */

/*
 *---------------------------------------------------------------------------
 *
 * TclpObjRenameFile, DoRenameFile --
 *
 *	Changes the name of an existing file or directory, from src to dst. If
 *	src and dst refer to the same file or directory, does nothing and
 *	returns success. Otherwise if dst already exists, it will be deleted
 *	and replaced by src subject to the following conditions:
 *	    If src is a directory, dst may be an empty directory.
 *	    If src is a file, dst may be a file.
 *	In any other situation where dst already exists, the rename will fail.
 *
 * Results:
 *	If the directory was successfully created, returns TCL_OK. Otherwise
 *	the return value is TCL_ERROR and errno is set to indicate the error.
 *	Some possible values for errno are:
 *
 *	EACCES:	    src or dst parent directory can't be read and/or written.
 *	EEXIST:	    dst is a non-empty directory.
 *	EINVAL:	    src is a root directory or dst is a subdirectory of src.
 *	EISDIR:	    dst is a directory, but src is not.
 *	ENOENT:	    src doesn't exist, or src or dst is "".
 *	ENOTDIR:    src is a directory, but dst is not.
 *	EXDEV:	    src and dst are on different filesystems.
 *
 * Side effects:
 *	The implementation of rename may allow cross-filesystem renames, but
 *	the caller should be prepared to emulate it with copy and delete if
 *	errno is EXDEV.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjRenameFile(
    Tcl_Obj *srcPathPtr,
    Tcl_Obj *destPathPtr)
{
    return DoRenameFile((const char *)Tcl_FSGetNativePath(srcPathPtr),
	    (const char *)Tcl_FSGetNativePath(destPathPtr));
}

static int
DoRenameFile(
    const char *src,		/* Pathname of file or dir to be renamed
				 * (native). */
    const char *dst)		/* New pathname of file or directory
				 * (native). */
{
    if (rename(src, dst) == 0) {			/* INTL: Native. */
	return TCL_OK;
    }
    if (errno == ENOTEMPTY) {
	errno = EEXIST;
    }

    /*
     * IRIX returns EIO when you attempt to move a directory into itself. We
     * just map EIO to EINVAL get the right message on SGI. Most platforms
     * don't return EIO except in really strange cases.
     */

    if (errno == EIO) {
	errno = EINVAL;
    }

#ifndef NO_REALPATH
    /*
     * SunOS 4.1.4 reports overwriting a non-empty directory with a directory
     * as EINVAL instead of EEXIST (first rule out the correct EINVAL result
     * code for moving a directory into itself). Must be conditionally
     * compiled because realpath() not defined on all systems.
     */

    if (errno == EINVAL && haveRealpath) {
	char srcPath[MAXPATHLEN], dstPath[MAXPATHLEN];
	TclDIR *dirPtr;
	Tcl_DirEntry *dirEntPtr;

	if ((Realpath((char *) src, srcPath) != NULL)	/* INTL: Native. */
		&& (Realpath((char *) dst, dstPath) != NULL) /* INTL: Native */
		&& (strncmp(srcPath, dstPath, strlen(srcPath)) != 0)) {
	    dirPtr = TclOSopendir(dst);			/* INTL: Native. */
	    if (dirPtr != NULL) {
		while (1) {
		    dirEntPtr = TclOSreaddir(dirPtr);	/* INTL: Native. */
		    if (dirEntPtr == NULL) {
			break;
		    }
		    if ((strcmp(dirEntPtr->d_name, ".") != 0) &&
			    (strcmp(dirEntPtr->d_name, "..") != 0)) {
			errno = EEXIST;
			TclOSclosedir(dirPtr);
			return TCL_ERROR;
		    }
		}
		TclOSclosedir(dirPtr);
	    }
	}
	errno = EINVAL;
    }
#endif	/* !NO_REALPATH */

    if (strcmp(src, "/") == 0) {
	/*
	 * Alpha reports renaming / as EBUSY and Linux reports it as EACCES,
	 * instead of EINVAL.
	 */

	errno = EINVAL;
    }

    /*
     * DEC Alpha OSF1 V3.0 returns EACCES when attempting to move a file
     * across filesystems and the parent directory of that file is not
     * writable. Most other systems return EXDEV. Does nothing to correct this
     * behavior.
     */

    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpObjCopyFile, DoCopyFile --
 *
 *	Copy a single file (not a directory). If dst already exists and is not
 *	a directory, it is removed.
 *
 * Results:
 *	If the file was successfully copied, returns TCL_OK. Otherwise the
 *	return value is TCL_ERROR and errno is set to indicate the error. Some
 *	possible values for errno are:
 *
 *	EACCES:	    src or dst parent directory can't be read and/or written.
 *	EISDIR:	    src or dst is a directory.
 *	ENOENT:	    src doesn't exist. src or dst is "".
 *
 * Side effects:
 *	This procedure will also copy symbolic links, block, and character
 *	devices, and fifos. For symbolic links, the links themselves will be
 *	copied and not what they point to. For the other special file types,
 *	the directory entry will be copied and not the contents of the device
 *	that it refers to.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjCopyFile(
    Tcl_Obj *srcPathPtr,
    Tcl_Obj *destPathPtr)
{
    const char *src = (const char *)Tcl_FSGetNativePath(srcPathPtr);
    Tcl_StatBuf srcStatBuf;

    if (TclOSlstat(src, &srcStatBuf) != 0) {		/* INTL: Native. */
	return TCL_ERROR;
    }

    return DoCopyFile(src, (const char *)Tcl_FSGetNativePath(destPathPtr), &srcStatBuf);
}

static int
DoCopyFile(
    const char *src,		/* Pathname of file to be copied (native). */
    const char *dst,		/* Pathname of file to copy to (native). */
    const Tcl_StatBuf *statBufPtr)
				/* Used to determine filetype. */
{
    Tcl_StatBuf dstStatBuf;

    if (S_ISDIR(statBufPtr->st_mode)) {
	errno = EISDIR;
	return TCL_ERROR;
    }

    /*
     * Symlink, and some of the other calls will fail if the target exists, so
     * we remove it first.
     */

    if (TclOSlstat(dst, &dstStatBuf) == 0) {		/* INTL: Native. */
	if (S_ISDIR(dstStatBuf.st_mode)) {
	    errno = EISDIR;
	    return TCL_ERROR;
	}
    }
    if (unlink(dst) != 0) {				/* INTL: Native. */
	if (errno != ENOENT) {
	    return TCL_ERROR;
	}
    }

    switch ((int)(statBufPtr->st_mode & S_IFMT)) {
#ifndef DJGPP
    case S_IFLNK: {
	char linkBuf[MAXPATHLEN+1];
	ssize_t length;

	length = readlink(src, linkBuf, MAXPATHLEN);	/* INTL: Native. */
	if (length == -1) {
	    return TCL_ERROR;
	}
	linkBuf[length] = '\0';
	if (symlink(linkBuf, dst) < 0) {		/* INTL: Native. */
	    return TCL_ERROR;
	}
#ifdef MAC_OSX_TCL
	TclMacOSXCopyFileAttributes(src, dst, statBufPtr);
#endif
	break;
    }
#endif /* !DJGPP */
    case S_IFBLK:
    case S_IFCHR:
	if (mknod(dst, statBufPtr->st_mode,		/* INTL: Native. */
		statBufPtr->st_rdev) < 0) {
	    return TCL_ERROR;
	}
	return CopyFileAtts(src, dst, statBufPtr);
    case S_IFIFO:
	if (mkfifo(dst, statBufPtr->st_mode) < 0) {	/* INTL: Native. */
	    return TCL_ERROR;
	}
	return CopyFileAtts(src, dst, statBufPtr);
    default:
	return TclUnixCopyFile(src, dst, statBufPtr, 0);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUnixCopyFile -
 *
 *	Helper function for TclpCopyFile. Copies one regular file, using
 *	read() and write().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A file is copied. Dst will be overwritten if it exists.
 *
 *----------------------------------------------------------------------
 */

int
TclUnixCopyFile(
    const char *src,		/* Pathname of file to copy (native). */
    const char *dst,		/* Pathname of file to create/overwrite
				 * (native). */
    const Tcl_StatBuf *statBufPtr,
				/* Used to determine mode and blocksize. */
    int dontCopyAtts)		/* If flag set, don't copy attributes. */
{
    int srcFd, dstFd;
    size_t blockSize;		/* Optimal I/O blocksize for filesystem */
    char *buffer;		/* Data buffer for copy */
    ssize_t nread;

#ifdef DJGPP
#define BINMODE |O_BINARY
#else
#define BINMODE
#endif /* DJGPP */

#define DEFAULT_COPY_BLOCK_SIZE 4096

    if ((srcFd = TclOSopen(src, O_RDONLY BINMODE, 0)) < 0) { /* INTL: Native */
	return TCL_ERROR;
    }

    dstFd = TclOSopen(dst, O_CREAT|O_TRUNC|O_WRONLY BINMODE, /* INTL: Native */
	    statBufPtr->st_mode);
    if (dstFd < 0) {
	close(srcFd);
	return TCL_ERROR;
    }

    /*
     * Try to work out the best size of buffer to use for copying. If we
     * can't, it's no big deal as we can just use a (32-bit) page, since
     * that's likely to be fairly efficient anyway.
     */

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    blockSize = statBufPtr->st_blksize;
#elif !defined(NO_FSTATFS)
    {
	struct statfs fs;

	if (fstatfs(srcFd, &fs) == 0) {
	    blockSize = fs.f_bsize;
	} else {
	    blockSize = DEFAULT_COPY_BLOCK_SIZE;
	}
    }
#else
    blockSize = DEFAULT_COPY_BLOCK_SIZE;
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */

    /*
     * [SF Tcl Bug 1586470] Even if we HAVE_STRUCT_STAT_ST_BLKSIZE, there are
     * filesystems which report a bogus value for the blocksize. An example
     * is the Andrew Filesystem (afs), reporting a blocksize of 0. When
     * detecting such a situation we now simply fall back to a hardwired
     * default size.
     */

    if (blockSize <= 0) {
	blockSize = DEFAULT_COPY_BLOCK_SIZE;
    }
    buffer = (char *)Tcl_Alloc(blockSize);
    while (1) {
	nread = read(srcFd, buffer, blockSize);
	if ((nread == -1) || (nread == 0)) {
	    break;
	}
	if (write(dstFd, buffer, nread) != nread) {
	    nread = -1;
	    break;
	}
    }

    Tcl_Free(buffer);
    close(srcFd);
    if ((close(dstFd) != 0) || (nread == -1)) {
	unlink(dst);					/* INTL: Native. */
	return TCL_ERROR;
    }
    if (!dontCopyAtts && CopyFileAtts(src, dst, statBufPtr) == TCL_ERROR) {
	/*
	 * The copy succeeded, but setting the permissions failed, so be in a
	 * consistent state, we remove the file that was created by the copy.
	 */

	unlink(dst);					/* INTL: Native. */
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpObjDeleteFile, TclpDeleteFile --
 *
 *	Removes a single file (not a directory).
 *
 * Results:
 *	If the file was successfully deleted, returns TCL_OK. Otherwise the
 *	return value is TCL_ERROR and errno is set to indicate the error. Some
 *	possible values for errno are:
 *
 *	EACCES:	    a parent directory can't be read and/or written.
 *	EISDIR:	    path is a directory.
 *	ENOENT:	    path doesn't exist or is "".
 *
 * Side effects:
 *	The file is deleted, even if it is read-only.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjDeleteFile(
    Tcl_Obj *pathPtr)
{
    return TclpDeleteFile(Tcl_FSGetNativePath(pathPtr));
}

int
TclpDeleteFile(
    const void *path)		/* Pathname of file to be removed (native). */
{
    if (unlink((const char *)path) != 0) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpCreateDirectory, DoCreateDirectory --
 *
 *	Creates the specified directory. All parent directories of the
 *	specified directory must already exist. The directory is automatically
 *	created with permissions so that user can access the new directory and
 *	create new files or subdirectories in it.
 *
 * Results:
 *	If the directory was successfully created, returns TCL_OK. Otherwise
 *	the return value is TCL_ERROR and errno is set to indicate the error.
 *	Some possible values for errno are:
 *
 *	EACCES:	    a parent directory can't be read and/or written.
 *	EEXIST:	    path already exists.
 *	ENOENT:	    a parent directory doesn't exist.
 *
 * Side effects:
 *	A directory is created with the current umask, except that permission
 *	for u+rwx will always be added.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjCreateDirectory(
    Tcl_Obj *pathPtr)
{
    return DoCreateDirectory((const char *)Tcl_FSGetNativePath(pathPtr));
}

static int
DoCreateDirectory(
    const char *path)		/* Pathname of directory to create (native). */
{
    mode_t mode;

    mode = umask(0);
    umask(mode);

    /*
     * umask return value is actually the inverse of the permissions.
     */

    mode = (0777 & ~mode) | S_IRUSR | S_IWUSR | S_IXUSR;

    if (mkdir(path, mode) != 0) {			/* INTL: Native. */
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpObjCopyDirectory --
 *
 *	Recursively copies a directory. The target directory dst must not
 *	already exist. Note that this function does not merge two directory
 *	hierarchies, even if the target directory is an empty directory.
 *
 * Results:
 *	If the directory was successfully copied, returns TCL_OK. Otherwise
 *	the return value is TCL_ERROR, errno is set to indicate the error, and
 *	the pathname of the file that caused the error is stored in errorPtr.
 *	See TclpObjCreateDirectory and TclpObjCopyFile for a description of
 *	possible values for errno.
 *
 * Side effects:
 *	An exact copy of the directory hierarchy src will be created with the
 *	name dst. If an error occurs, the error will be returned immediately,
 *	and remaining files will not be processed.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjCopyDirectory(
    Tcl_Obj *srcPathPtr,
    Tcl_Obj *destPathPtr,
    Tcl_Obj **errorPtr)
{
    Tcl_DString ds;
    Tcl_DString srcString, dstString;
    int ret;
    Tcl_Obj *transPtr;

    transPtr = Tcl_FSGetTranslatedPath(NULL,srcPathPtr);
    ret = Tcl_UtfToExternalDStringEx(NULL, NULL,
	    (transPtr != NULL ? TclGetString(transPtr) : NULL),
	    -1, 0, &srcString, NULL);
    if (transPtr != NULL) {
	Tcl_DecrRefCount(transPtr);
    }
    if (ret != TCL_OK) {
	*errorPtr = srcPathPtr;
    } else {
	transPtr = Tcl_FSGetTranslatedPath(NULL,destPathPtr);
	ret = Tcl_UtfToExternalDStringEx(NULL, NULL,
	    (transPtr != NULL ? TclGetString(transPtr) : NULL),
	    -1, TCL_ENCODING_PROFILE_TCL8, &dstString, NULL);
	if (transPtr != NULL) {
	    Tcl_DecrRefCount(transPtr);
	}
	if (ret != TCL_OK) {
	    *errorPtr = destPathPtr;
	} else {
	    ret = TraverseUnixTree(TraversalCopy, &srcString, &dstString, &ds, 0);
	    /* Note above call only sets ds on error */
	    if (ret != TCL_OK) {
		*errorPtr = Tcl_DStringToObj(&ds);
	    }
	    Tcl_DStringFree(&dstString);
	}
	Tcl_DStringFree(&srcString);
    }
    if (ret != TCL_OK) {
	Tcl_IncrRefCount(*errorPtr);
    }
    return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpRemoveDirectory, DoRemoveDirectory --
 *
 *	Removes directory (and its contents, if the recursive flag is set).
 *
 * Results:
 *	If the directory was successfully removed, returns TCL_OK. Otherwise
 *	the return value is TCL_ERROR, errno is set to indicate the error, and
 *	the pathname of the file that caused the error is stored in errorPtr.
 *	Some possible values for errno are:
 *
 *	EACCES:	    path directory can't be read and/or written.
 *	EEXIST:	    path is a non-empty directory.
 *	EINVAL:	    path is a root directory.
 *	ENOENT:	    path doesn't exist or is "".
 *	ENOTDIR:    path is not a directory.
 *
 * Side effects:
 *	Directory removed. If an error occurs, the error will be returned
 *	immediately, and remaining files will not be deleted.
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjRemoveDirectory(
    Tcl_Obj *pathPtr,
    int recursive,
    Tcl_Obj **errorPtr)
{
    Tcl_DString ds;
    Tcl_DString pathString;
    int ret;
    Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(NULL, pathPtr);

    ret = Tcl_UtfToExternalDStringEx(NULL, NULL,
	    (transPtr != NULL ? TclGetString(transPtr) : NULL),
	    -1, TCL_ENCODING_PROFILE_TCL8, &pathString, NULL);
    if (transPtr != NULL) {
	Tcl_DecrRefCount(transPtr);
    }
    if (ret != TCL_OK) {
	*errorPtr = pathPtr;
    } else {
	ret = DoRemoveDirectory(&pathString, recursive, &ds);
	Tcl_DStringFree(&pathString);
	/* Note above call only sets ds on error */
	if (ret != TCL_OK) {
	    *errorPtr = Tcl_DStringToObj(&ds);
	}
    }

    if (ret != TCL_OK) {
	Tcl_IncrRefCount(*errorPtr);
    }
    return ret;
}

static int
DoRemoveDirectory(
    Tcl_DString *pathPtr,	/* Pathname of directory to be removed
				 * (native). */
    int recursive,		/* If non-zero, removes directories that are
				 * nonempty. Otherwise, will only remove empty
				 * directories. */
    Tcl_DString *errorPtr)	/* If non-NULL, uninitialized or free DString
				 * filled with UTF-8 name of file causing
				 * error. */
{
    const char *path;
    mode_t oldPerm = 0;
    int result;

    path = Tcl_DStringValue(pathPtr);

    if (recursive != 0) {
	/*
	 * We should try to change permissions so this can be deleted.
	 */

	Tcl_StatBuf statBuf;
	int newPerm;

	if (TclOSstat(path, &statBuf) == 0) {
	    oldPerm = (mode_t) (statBuf.st_mode & 0x00007FFF);
	}

	newPerm = oldPerm | (64+128+256);
	chmod(path, (mode_t) newPerm);
    }

    if (rmdir(path) == 0) {				/* INTL: Native. */
	return TCL_OK;
    }
    if (errno == ENOTEMPTY) {
	errno = EEXIST;
    }

    result = TCL_OK;
    if ((errno != EEXIST) || (recursive == 0)) {
	if (errorPtr != NULL) {
	    Tcl_ExternalToUtfDStringEx(NULL, NULL, path, TCL_INDEX_NONE, 0, errorPtr, NULL);
	}
	result = TCL_ERROR;
    }

    /*
     * The directory is nonempty, but the recursive flag has been specified,
     * so we recursively remove all the files in the directory.
     */

    if (result == TCL_OK) {
	result = TraverseUnixTree(TraversalDelete, pathPtr, NULL, errorPtr, 1);
    }

    if ((result != TCL_OK) && (recursive != 0)) {
	/*
	 * Try to restore permissions.
	 */

	chmod(path, oldPerm);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraverseUnixTree --
 *
 *	Traverse directory tree specified by sourcePtr, calling the function
 *	traverseProc for each file and directory encountered. If destPtr is
 *	non-null, each of name in the sourcePtr directory is appended to the
 *	directory specified by destPtr and passed as the second argument to
 *	traverseProc().
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	None caused by TraverseUnixTree, however the user specified
 *	traverseProc() may change state. If an error occurs, the error will be
 *	returned immediately, and remaining files will not be processed.
 *
 *---------------------------------------------------------------------------
 */

static int
TraverseUnixTree(
    TraversalProc *traverseProc,/* Function to call for every file and
				 * directory in source hierarchy. */
    Tcl_DString *sourcePtr,	/* Pathname of source directory to be
				 * traversed (native). */
    Tcl_DString *targetPtr,	/* Pathname of directory to traverse in
				 * parallel with source directory (native). */
    Tcl_DString *errorPtr,	/* If non-NULL, uninitialized or free DString
				 * filled with UTF-8 name of file causing
				 * error. */
    int doRewind)		/* Flag indicating that to ensure complete
				 * traversal of source hierarchy, the readdir
				 * loop should be rewound whenever
				 * traverseProc has returned TCL_OK; this is
				 * required when traverseProc modifies the
				 * source hierarchy, e.g. by deleting
				 * files. */
{
    Tcl_StatBuf statBuf;
    const char *source, *errfile;
    int result;
    size_t targetLen, sourceLen;
#ifndef HAVE_FTS
    int numProcessed = 0;
    Tcl_DirEntry *dirEntPtr;
    TclDIR *dirPtr;
#else
    const char *paths[2] = {NULL, NULL};
    FTS *fts = NULL;
    FTSENT *ent;
#endif

    errfile = NULL;
    result = TCL_OK;
    targetLen = 0;

    source = Tcl_DStringValue(sourcePtr);
    if (TclOSlstat(source, &statBuf) != 0) {		/* INTL: Native. */
	errfile = source;
	goto end;
    }
    if (!S_ISDIR(statBuf.st_mode)) {
	/*
	 * Process the regular file
	 */

	return traverseProc(sourcePtr, targetPtr, &statBuf, DOTREE_F,
		errorPtr);
    }
#ifndef HAVE_FTS
    dirPtr = TclOSopendir(source);			/* INTL: Native. */
    if (dirPtr == NULL) {
	/*
	 * Can't read directory
	 */

	errfile = source;
	goto end;
    }
    result = traverseProc(sourcePtr, targetPtr, &statBuf, DOTREE_PRED,
	    errorPtr);
    if (result != TCL_OK) {
	TclOSclosedir(dirPtr);
	return result;
    }

    TclDStringAppendLiteral(sourcePtr, "/");
    sourceLen = Tcl_DStringLength(sourcePtr);

    if (targetPtr != NULL) {
	TclDStringAppendLiteral(targetPtr, "/");
	targetLen = Tcl_DStringLength(targetPtr);
    }

    while ((dirEntPtr = TclOSreaddir(dirPtr)) != NULL) { /* INTL: Native. */
	if ((dirEntPtr->d_name[0] == '.')
		&& ((dirEntPtr->d_name[1] == '\0')
			|| (strcmp(dirEntPtr->d_name, "..") == 0))) {
	    continue;
	}

	/*
	 * Append name after slash, and recurse on the file.
	 */

	Tcl_DStringAppend(sourcePtr, dirEntPtr->d_name, TCL_INDEX_NONE);
	if (targetPtr != NULL) {
	    Tcl_DStringAppend(targetPtr, dirEntPtr->d_name, TCL_INDEX_NONE);
	}
	result = TraverseUnixTree(traverseProc, sourcePtr, targetPtr,
		errorPtr, doRewind);
	if (result != TCL_OK) {
	    break;
	} else {
	    numProcessed++;
	}

	/*
	 * Remove name after slash.
	 */

	Tcl_DStringSetLength(sourcePtr, sourceLen);
	if (targetPtr != NULL) {
	    Tcl_DStringSetLength(targetPtr, targetLen);
	}
	if (doRewind && (numProcessed > MAX_READDIR_UNLINK_THRESHOLD)) {
	    /*
	     * Call rewinddir if we've called unlink or rmdir so many times
	     * (since the opendir or the previous rewinddir), to avoid a
	     * NULL-return that may a symptom of a buggy readdir.
	     */

	    TclOSrewinddir(dirPtr);
	    numProcessed = 0;
	}
    }
    TclOSclosedir(dirPtr);

    /*
     * Strip off the trailing slash we added
     */

    Tcl_DStringSetLength(sourcePtr, sourceLen - 1);
    if (targetPtr != NULL) {
	Tcl_DStringSetLength(targetPtr, targetLen - 1);
    }

    if (result == TCL_OK) {
	/*
	 * Call traverseProc() on a directory after visiting all the files in
	 * that directory.
	 */

	result = traverseProc(sourcePtr, targetPtr, &statBuf, DOTREE_POSTD,
		errorPtr);
    }
#else /* HAVE_FTS */
    paths[0] = source;
    fts = fts_open((char **) paths, FTS_PHYSICAL | FTS_NOCHDIR |
	    (noFtsStat || doRewind ? FTS_NOSTAT : 0), NULL);
    if (fts == NULL) {
	errfile = source;
	goto end;
    }

    sourceLen = Tcl_DStringLength(sourcePtr);
    if (targetPtr != NULL) {
	targetLen = Tcl_DStringLength(targetPtr);
    }

    while ((ent = fts_read(fts)) != NULL) {
	unsigned short info = ent->fts_info;
	char *path = ent->fts_path + sourceLen;
	Tcl_Size pathlen = ent->fts_pathlen - sourceLen;
	int type;
	Tcl_StatBuf *statBufPtr = NULL;

	if (info == FTS_DNR || info == FTS_ERR || info == FTS_NS) {
	    errfile = ent->fts_path;
	    break;
	}
	Tcl_DStringAppend(sourcePtr, path, pathlen);
	if (targetPtr != NULL) {
	    Tcl_DStringAppend(targetPtr, path, pathlen);
	}
	switch (info) {
	case FTS_D:
	    type = DOTREE_PRED;
	    break;
	case FTS_DP:
	    type = DOTREE_POSTD;
	    break;
	default:
	    type = DOTREE_F;
	    break;
	}
	if (!doRewind) { /* no need to stat for delete */
	    if (noFtsStat) {
		statBufPtr = &statBuf;
		if (TclOSlstat(ent->fts_path, statBufPtr) != 0) {
		    errfile = ent->fts_path;
		    break;
		}
	    } else {
		statBufPtr = (Tcl_StatBuf *) ent->fts_statp;
	    }
	}
	result = traverseProc(sourcePtr, targetPtr, statBufPtr, type,
		errorPtr);
	if (result != TCL_OK) {
	    break;
	}
	Tcl_DStringSetLength(sourcePtr, sourceLen);
	if (targetPtr != NULL) {
	    Tcl_DStringSetLength(targetPtr, targetLen);
	}
    }
#endif /* !HAVE_FTS */

  end:
    if (errfile != NULL) {
	if (errorPtr != NULL) {
	    Tcl_ExternalToUtfDStringEx(NULL, NULL, errfile, TCL_INDEX_NONE, 0, errorPtr, NULL);
	}
	result = TCL_ERROR;
    }
#ifdef HAVE_FTS
    if (fts != NULL) {
	fts_close(fts);
    }
#endif

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TraversalCopy
 *
 *	Called from TraverseUnixTree in order to execute a recursive copy of a
 *	directory.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	The file or directory src may be copied to dst, depending on the value
 *	of type.
 *
 *----------------------------------------------------------------------
 */

static int
TraversalCopy(
    Tcl_DString *srcPtr,	/* Source pathname to copy (native). */
    Tcl_DString *dstPtr,	/* Destination pathname of copy (native). */
    const Tcl_StatBuf *statBufPtr,
				/* Stat info for file specified by srcPtr. */
    int type,			/* Reason for call - see TraverseUnixTree(). */
    Tcl_DString *errorPtr)	/* If non-NULL, uninitialized or free DString
				 * filled with UTF-8 name of file causing
				 * error. */
{
    switch (type) {
    case DOTREE_F:
	if (DoCopyFile(Tcl_DStringValue(srcPtr), Tcl_DStringValue(dstPtr),
		statBufPtr) == TCL_OK) {
	    return TCL_OK;
	}
	break;

    case DOTREE_PRED:
	if (DoCreateDirectory(Tcl_DStringValue(dstPtr)) == TCL_OK) {
	    return TCL_OK;
	}
	break;

    case DOTREE_POSTD:
	if (CopyFileAtts(Tcl_DStringValue(srcPtr),
		Tcl_DStringValue(dstPtr), statBufPtr) == TCL_OK) {
	    return TCL_OK;
	}
	break;
    }

    /*
     * There shouldn't be a problem with src, because we already checked it to
     * get here.
     */

    if (errorPtr != NULL) {
	Tcl_ExternalToUtfDStringEx(NULL, NULL, Tcl_DStringValue(dstPtr),
		Tcl_DStringLength(dstPtr), 0, errorPtr, NULL);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraversalDelete --
 *
 *	Called by procedure TraverseUnixTree for every file and directory that
 *	it encounters in a directory hierarchy. This procedure unlinks files,
 *	and removes directories after all the containing files have been
 *	processed.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Files or directory specified by src will be deleted.
 *
 *----------------------------------------------------------------------
 */

static int
TraversalDelete(
    Tcl_DString *srcPtr,	/* Source pathname (native). */
    TCL_UNUSED(Tcl_DString *),
    TCL_UNUSED(const Tcl_StatBuf *),
    int type,			/* Reason for call - see TraverseUnixTree(). */
    Tcl_DString *errorPtr)	/* If non-NULL, uninitialized or free DString
				 * filled with UTF-8 name of file causing
				 * error. */
{

    switch (type) {
    case DOTREE_F:
	if (TclpDeleteFile(Tcl_DStringValue(srcPtr)) == 0) {
	    return TCL_OK;
	}
	break;
    case DOTREE_PRED:
	return TCL_OK;
    case DOTREE_POSTD:
	if (DoRemoveDirectory(srcPtr, 0, NULL) == 0) {
	    return TCL_OK;
	}
	break;
    }
    if (errorPtr != NULL) {
	Tcl_ExternalToUtfDStringEx(NULL, NULL, Tcl_DStringValue(srcPtr),
		Tcl_DStringLength(srcPtr), 0, errorPtr, NULL);
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * CopyFileAtts --
 *
 *	Copy the file attributes such as owner, group, permissions, and
 *	modification date from one file to another.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	User id, group id, permission bits, last modification time, and last
 *	access time are updated in the new file to reflect the old file.
 *
 *---------------------------------------------------------------------------
 */

static int
CopyFileAtts(
#ifdef MAC_OSX_TCL
    const char *src,	/* Path name of source file (native). */
#else
    TCL_UNUSED(const char *) /*src*/,
#endif
    const char *dst,		/* Path name of target file (native). */
    const Tcl_StatBuf *statBufPtr)
				/* Stat info for source file */
{
    struct utimbuf tval;
    mode_t newMode;

    newMode = statBufPtr->st_mode
	    & (S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO);

    /*
     * Note that if you copy a setuid file that is owned by someone else, and
     * you are not root, then the copy will be setuid to you. The most correct
     * implementation would probably be to have the copy not setuid to anyone
     * if the original file was owned by someone else, but this corner case
     * isn't currently handled. It would require another lstat(), or getuid().
     */

    if (chmod(dst, newMode)) {				/* INTL: Native. */
	newMode &= ~(S_ISUID | S_ISGID);
	if (chmod(dst, newMode)) {			/* INTL: Native. */
	    return TCL_ERROR;
	}
    }

    tval.actime = Tcl_GetAccessTimeFromStat(statBufPtr);
    tval.modtime = Tcl_GetModificationTimeFromStat(statBufPtr);

    if (utime(dst, &tval)) {				/* INTL: Native. */
	return TCL_ERROR;
    }
#ifdef MAC_OSX_TCL
    TclMacOSXCopyFileAttributes(src, dst, statBufPtr);
#endif
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetGroupAttribute
 *
 *	Gets the group attribute of a file.
 *
 * Results:
 *	Standard TCL result. Returns a new Tcl_Obj in attributePtrPtr if there
 *	is no error.
 *
 * Side effects:
 *	A new object is allocated.
 *
 *----------------------------------------------------------------------
 */

static int
GetGroupAttribute(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr)	/* A pointer to return the object with. */
{
    Tcl_StatBuf statBuf;
    struct group *groupPtr;
    int result;

    result = TclpObjStat(fileName, &statBuf);

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not read \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }

    groupPtr = TclpGetGrGid(statBuf.st_gid);

    if (groupPtr == NULL) {
	TclNewIntObj(*attributePtrPtr, statBuf.st_gid);
    } else {
	Tcl_DString ds;
	const char *utf;

	utf = Tcl_ExternalToUtfDString(NULL, groupPtr->gr_name, TCL_INDEX_NONE, &ds);
	*attributePtrPtr = Tcl_NewStringObj(utf, TCL_INDEX_NONE);
	Tcl_DStringFree(&ds);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOwnerAttribute
 *
 *	Gets the owner attribute of a file.
 *
 * Results:
 *	Standard TCL result. Returns a new Tcl_Obj in attributePtrPtr if there
 *	is no error.
 *
 * Side effects:
 *	A new object is allocated.
 *
 *----------------------------------------------------------------------
 */

static int
GetOwnerAttribute(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr)	/* A pointer to return the object with. */
{
    Tcl_StatBuf statBuf;
    struct passwd *pwPtr;
    int result;

    result = TclpObjStat(fileName, &statBuf);

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not read \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }

    pwPtr = TclpGetPwUid(statBuf.st_uid);

    if (pwPtr == NULL) {
	TclNewIntObj(*attributePtrPtr, statBuf.st_uid);
    } else {
	Tcl_DString ds;

	(void)Tcl_ExternalToUtfDString(NULL, pwPtr->pw_name, TCL_INDEX_NONE, &ds);
	*attributePtrPtr = Tcl_DStringToObj(&ds);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetPermissionsAttribute
 *
 *	Gets the group attribute of a file.
 *
 * Results:
 *	Standard TCL result. Returns a new Tcl_Obj in attributePtrPtr if there
 *	is no error. The object will have ref count 0.
 *
 * Side effects:
 *	A new object is allocated.
 *
 *----------------------------------------------------------------------
 */

static int
GetPermissionsAttribute(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr)	/* A pointer to return the object with. */
{
    Tcl_StatBuf statBuf;
    int result;

    result = TclpObjStat(fileName, &statBuf);

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not read \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }

    *attributePtrPtr = Tcl_ObjPrintf(
	    "%0#5o", ((int)statBuf.st_mode & 0x7FFF));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetGroupAttribute --
 *
 *	Sets the group of the file to the specified group.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	As above.
 *
 *---------------------------------------------------------------------------
 */

static int
SetGroupAttribute(
    Tcl_Interp *interp,		/* The interp for error reporting. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr)	/* New group for file. */
{
    Tcl_WideInt gid;
    int result;
    const char *native;

    if (TclGetWideIntFromObj(NULL, attributePtr, &gid) != TCL_OK) {
	Tcl_DString ds;
	struct group *groupPtr = NULL;
	const char *string;
	Tcl_Size length;

	string = TclGetStringFromObj(attributePtr, &length);

	if (Tcl_UtfToExternalDStringEx(interp, NULL, string, length, 0, &ds, NULL) != TCL_OK) {
	    Tcl_DStringFree(&ds);
	    return TCL_ERROR;
	}
	native = Tcl_DStringValue(&ds);
	groupPtr = TclpGetGrNam(native); /* INTL: Native. */
	Tcl_DStringFree(&ds);

	if (groupPtr == NULL) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"could not set group for file \"%s\":"
			" group \"%s\" does not exist",
			TclGetString(fileName), string));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "SETGRP",
			"NO_GROUP", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	gid = groupPtr->gr_gid;
    }

    native = (const char *)Tcl_FSGetNativePath(fileName);
    result = chown(native, (uid_t) -1, (gid_t) gid);	/* INTL: Native. */

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not set group for file \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetOwnerAttribute --
 *
 *	Sets the owner of the file to the specified owner.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	As above.
 *
 *---------------------------------------------------------------------------
 */

static int
SetOwnerAttribute(
    Tcl_Interp *interp,		/* The interp for error reporting. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr)	/* New owner for file. */
{
    Tcl_WideInt uid;
    int result;
    const char *native;

    if (TclGetWideIntFromObj(NULL, attributePtr, &uid) != TCL_OK) {
	Tcl_DString ds;
	struct passwd *pwPtr = NULL;
	const char *string;
	Tcl_Size length;

	string = TclGetStringFromObj(attributePtr, &length);

	if (Tcl_UtfToExternalDStringEx(interp, NULL, string, length, 0, &ds, NULL) != TCL_OK) {
	    Tcl_DStringFree(&ds);
	    return TCL_ERROR;
	}
	native = Tcl_DStringValue(&ds);
	pwPtr = TclpGetPwNam(native);			/* INTL: Native. */
	Tcl_DStringFree(&ds);

	if (pwPtr == NULL) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"could not set owner for file \"%s\":"
			" user \"%s\" does not exist",
			TclGetString(fileName), string));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "SETOWN",
			"NO_USER", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
	uid = pwPtr->pw_uid;
    }

    native = (const char *)Tcl_FSGetNativePath(fileName);
    result = chown(native, (uid_t) uid, (gid_t) -1);	/* INTL: Native. */

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not set owner for file \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetPermissionsAttribute
 *
 *	Sets the file to the given permission.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	The permission of the file is changed.
 *
 *---------------------------------------------------------------------------
 */

static int
SetPermissionsAttribute(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr)	/* The attribute to set. */
{
    Tcl_WideInt mode;
    mode_t newMode;
    int result = TCL_ERROR;
    const char *native;
    const char *modeStringPtr = TclGetString(attributePtr);
    Tcl_Size scanned = TclParseAllWhiteSpace(modeStringPtr, -1);

    /*
     * First supply support for octal number format
     */

    if ((modeStringPtr[scanned] == '0')
	    && (modeStringPtr[scanned+1] >= '0')
	    && (modeStringPtr[scanned+1] <= '7')) {
	/* Leading zero - attempt octal interpretation */
	Tcl_Obj *modeObj;

	TclNewLiteralStringObj(modeObj, "0o");
	Tcl_AppendToObj(modeObj, modeStringPtr+scanned+1, TCL_INDEX_NONE);
	result = TclGetWideIntFromObj(NULL, modeObj, &mode);
	Tcl_DecrRefCount(modeObj);
    }
    if (result == TCL_OK
	    || TclGetWideIntFromObj(NULL, attributePtr, &mode) == TCL_OK) {
	newMode = (mode_t) (mode & 0x00007FFF);
    } else {
	Tcl_StatBuf buf;

	/*
	 * Try the forms "rwxrwxrwx" and "ugo=rwx"
	 *
	 * We get the current mode of the file, in order to allow for ug+-=rwx
	 * style chmod strings.
	 */

	result = TclpObjStat(fileName, &buf);
	if (result != 0) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"could not read \"%s\": %s",
			TclGetString(fileName), Tcl_PosixError(interp)));
	    }
	    return TCL_ERROR;
	}
	newMode = (mode_t) (buf.st_mode & 0x00007FFF);

	if (GetModeFromPermString(NULL, modeStringPtr, &newMode) != TCL_OK) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"unknown permission string format \"%s\"",
			modeStringPtr));
		Tcl_SetErrorCode(interp, "TCL", "VALUE", "PERMISSION", (char *)NULL);
	    }
	    return TCL_ERROR;
	}
    }

    native = (const char *)Tcl_FSGetNativePath(fileName);
    result = chmod(native, newMode);		/* INTL: Native. */
    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not set permissions for file \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

#ifndef DJGPP
/*
 *---------------------------------------------------------------------------
 *
 * TclpObjListVolumes --
 *
 *	Lists the currently mounted volumes, which on UNIX is just /.
 *
 * Results:
 *	The list of volumes.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
TclpObjListVolumes(void)
{
    Tcl_Obj *resultPtr;
    TclNewLiteralStringObj(resultPtr, "/");

    Tcl_IncrRefCount(resultPtr);
    return resultPtr;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * GetModeFromPermString --
 *
 *	This procedure is invoked to process the "file permissions" Tcl
 *	command, to check for a "rwxrwxrwx" or "ugoa+-=rwxst" string. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
GetModeFromPermString(
    TCL_UNUSED(Tcl_Interp *),
    const char *modeStringPtr,	/* Permissions string */
    mode_t *modePtr)		/* pointer to the mode value */
{
    mode_t newMode;
    mode_t oldMode;		/* Storage for the value of the old mode (that
				 * is passed in), to allow for the chmod style
				 * manipulation. */
    int i,n, who, op, what, op_found, who_found;

    /*
     * We start off checking for an "rwxrwxrwx" style permissions string
     */

    if (strlen(modeStringPtr) != 9) {
	goto chmodStyleCheck;
    }

    newMode = 0;
    for (i = 0; i < 9; i++) {
	switch (modeStringPtr[i]) {
	case 'r':
	    if ((i%3) != 0) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(8-i));
	    break;
	case 'w':
	    if ((i%3) != 1) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(8-i));
	    break;
	case 'x':
	    if ((i%3) != 2) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(8-i));
	    break;
	case 's':
	    if (((i%3) != 2) || (i > 5)) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(8-i));
	    newMode |= (1<<(11-(i/3)));
	    break;
	case 'S':
	    if (((i%3) != 2) || (i > 5)) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(11-(i/3)));
	    break;
	case 't':
	    if (i != 8) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<(8-i));
	    newMode |= (1<<9);
	    break;
	case 'T':
	    if (i != 8) {
		goto chmodStyleCheck;
	    }
	    newMode |= (1<<9);
	    break;
	case '-':
	    break;
	default:
	    /*
	     * Oops, not what we thought it was, so go on
	     */
	    goto chmodStyleCheck;
	}
    }
    *modePtr = newMode;
    return TCL_OK;

  chmodStyleCheck:
    /*
     * We now check for an "ugoa+-=rwxst" style permissions string
     */

    for (n = 0 ; modeStringPtr[n] != '\0' ; n += i) {
	oldMode = *modePtr;
	who = op = what = op_found = who_found = 0;
	for (i = 0 ; modeStringPtr[n + i] != '\0' ; i++ ) {
	    if (!who_found) {
		/* who */
		switch (modeStringPtr[n + i]) {
		case 'u':
		    who |= 0x9C0;
		    continue;
		case 'g':
		    who |= 0x438;
		    continue;
		case 'o':
		    who |= 0x207;
		    continue;
		case 'a':
		    who |= 0xFFF;
		    continue;
		}
	    }
	    who_found = 1;
	    if (who == 0) {
		who = 0xFFF;
	    }
	    if (!op_found) {
		/* op */
		switch (modeStringPtr[n + i]) {
		case '+':
		    op = 1;
		    op_found = 1;
		    continue;
		case '-':
		    op = 2;
		    op_found = 1;
		    continue;
		case '=':
		    op = 3;
		    op_found = 1;
		    continue;
		default:
		    return TCL_ERROR;
		}
	    }
	    /* what */
	    switch (modeStringPtr[n + i]) {
	    case 'r':
		what |= 0x124;
		continue;
	    case 'w':
		what |= 0x92;
		continue;
	    case 'x':
		what |= 0x49;
		continue;
	    case 's':
		what |= 0xC00;
		continue;
	    case 't':
		what |= 0x200;
		continue;
	    case ',':
		break;
	    default:
		return TCL_ERROR;
	    }
	    if (modeStringPtr[n + i] == ',') {
		i++;
		break;
	    }
	}
	switch (op) {
	case 1:
	    *modePtr = oldMode | (who & what);
	    continue;
	case 2:
	    *modePtr = oldMode & ~(who & what);
	    continue;
	case 3:
	    *modePtr = (oldMode & ~who) | (who & what);
	    continue;
	}
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpObjNormalizePath --
 *
 *	Replaces each component except that last one in a pathname that is a
 *	symbolic link with the fully resolved target of that link.
 *
 * Results:
 *	Stores the resulting path in pathPtr and returns the offset of the last
 *	byte processed to obtain the resulting path.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */

int
TclpObjNormalizePath(
    Tcl_Interp *interp,
    Tcl_Obj *pathPtr,		/* An unshared object containing the path to
				 * normalize. */
    int nextCheckpoint)		/* offset to start at in pathPtr.  Must either
				 * be 0 or the offset of a directory separator
				 * at the end of a path part that is already
				 * normalized.  I.e. this is not the index of
				 * the byte just after the separator. */
{
    const char *currentPathEndPosition;
    char cur;
    Tcl_Size pathLen;
    const char *path = TclGetStringFromObj(pathPtr, &pathLen);
    Tcl_DString ds;
    const char *nativePath;
#ifndef NO_REALPATH
    char normPath[MAXPATHLEN];
#endif

    currentPathEndPosition = path + nextCheckpoint;
    if (*currentPathEndPosition == '/') {
	currentPathEndPosition++;
    }

#ifndef NO_REALPATH
    if (nextCheckpoint == 0 && haveRealpath) {
	/*
	 * Try to get the entire path in one go
	 */

	const char *lastDir = strrchr(currentPathEndPosition, '/');

	if (lastDir != NULL) {
	    if (Tcl_UtfToExternalDStringEx(interp, NULL, path,
		    lastDir-path, 0, &ds, NULL) != TCL_OK) {
		Tcl_DStringFree(&ds);
		return -1;
	    }
	    nativePath = Tcl_DStringValue(&ds);
	    if (Realpath(nativePath, normPath) != NULL) {
		if (*nativePath != '/' && *normPath == '/') {
		    /*
		     * realpath transformed a relative path into an
		     * absolute path.  Fall back to the long way.
		     */

		    /*
		     * To do: This logic seems to be out of date.  This whole
		     * routine should be reviewed and cleaed up.
		     */
		} else {
		    nextCheckpoint = (int)(lastDir - path);
		    goto wholeStringOk;
		}
	    }
	    Tcl_DStringFree(&ds);
	}
    }

    /*
     * Else do it the slow way.
     */
#endif

    while (1) {
	cur = *currentPathEndPosition;
	if ((cur == '/') && (path != currentPathEndPosition)) {
	    /*
	     * Reached directory separator.
	     */

	    int accessOk;

	    if (Tcl_UtfToExternalDStringEx(interp, NULL, path,
		    currentPathEndPosition - path, 0, &ds, NULL) != TCL_OK) {
		Tcl_DStringFree(&ds);
		return -1;
	    }
	    nativePath = Tcl_DStringValue(&ds);
	    accessOk = access(nativePath, F_OK);
	    Tcl_DStringFree(&ds);

	    if (accessOk != 0) {
		/*
		 * File doesn't exist.
		 */

		break;
	    }

	    /*
	     * Assign the end of the current component to nextCheckpoint
	     */

	    nextCheckpoint = (int)(currentPathEndPosition - path);
	} else if (cur == 0) {
	    /*
	     * The end of the string.
	     */

	    break;
	}
	currentPathEndPosition++;
    }

    /*
     * Call 'realpath' to obtain a canonical path.
     */

#ifndef NO_REALPATH
    if (haveRealpath) {
	if (nextCheckpoint == 0) {
	    /*
	     * The path contains at most one component, e.g. '/foo' or '/',
	     * so there is nothing to resolve. Also, on some platforms
	     * 'Realpath' transforms an empty string into the normalized pwd,
	     * which is the wrong answer.
	     */

	    return 0;
	}

	if (Tcl_UtfToExternalDStringEx(interp, NULL, path,nextCheckpoint, 0, &ds, NULL)) {
	    Tcl_DStringFree(&ds);
	    return -1;
	}
	nativePath = Tcl_DStringValue(&ds);
	if (Realpath(nativePath, normPath) != NULL) {
	    Tcl_Size newNormLen;

	wholeStringOk:
	    newNormLen = strlen(normPath);
	    if ((newNormLen == Tcl_DStringLength(&ds))
		    && (strcmp(normPath, nativePath) == 0)) {
		/*
		 * The original path is unchanged.
		 */

		Tcl_DStringFree(&ds);

		/*
		 * Uncommenting this would mean that this native filesystem
		 * routine claims the path is normalized if the file exists,
		 * which would permit the caller to avoid iterating through
		 * other filesystems. Saving lots of calls is
		 * probably worth the extra access() time, but in the common
		 * case that no other filesystems are registered this is an
		 * unnecessary expense.
		 *
		if (0 == access(normPath, F_OK)) {
		    return pathLen;
		}
		 */

		return nextCheckpoint;
	    }

	    /*
	     * Free the original path and replace it with the normalized path.
	     */

	    Tcl_DStringFree(&ds);
	    Tcl_ExternalToUtfDStringEx(NULL, NULL, normPath, newNormLen, 0, &ds, NULL);

	    if (path[nextCheckpoint] != '\0') {
		/*
		 * Append the remaining path components.
		 */

		Tcl_Size normLen = Tcl_DStringLength(&ds);

		Tcl_DStringAppend(&ds, path + nextCheckpoint,
			pathLen - nextCheckpoint);

		/*
		 * characters up to and including the directory separator have
		 * been processed
		 */

		nextCheckpoint = (int)normLen + 1;
	    } else {
		/*
		 * We recognise the whole string.
		 */

		nextCheckpoint = (int)Tcl_DStringLength(&ds);
	    }

	    Tcl_SetStringObj(pathPtr, Tcl_DStringValue(&ds),
		    Tcl_DStringLength(&ds));
	}
	Tcl_DStringFree(&ds);
    }
#endif	/* !NO_REALPATH */

    return nextCheckpoint;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpOpenTemporaryFile, TclUnixOpenTemporaryFile --
 *
 *	Creates a temporary file, possibly based on the supplied bits and
 *	pieces of template supplied in the first three arguments. If the
 *	fourth argument is non-NULL, it contains a Tcl_Obj to store the name
 *	of the temporary file in (and it is caller's responsibility to clean
 *	up). If the fourth argument is NULL, try to arrange for the temporary
 *	file to go away once it is no longer needed.
 *
 * Results:
 *	A read-write Tcl Channel open on the file for TclpOpenTemporaryFile,
 *	or a file descriptor (or -1 on failure) for TclUnixOpenTemporaryFile.
 *
 * Side effects:
 *	Accesses the filesystem. Will set the contents of the Tcl_Obj fourth
 *	argument (if that is non-NULL).
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpOpenTemporaryFile(
    Tcl_Obj *dirObj,
    Tcl_Obj *basenameObj,
    Tcl_Obj *extensionObj,
    Tcl_Obj *resultingNameObj)
{
    int fd = TclUnixOpenTemporaryFile(dirObj, basenameObj, extensionObj,
	    resultingNameObj);

    if (fd == -1) {
	return NULL;
    }
    return Tcl_MakeFileChannel(INT2PTR(fd), TCL_READABLE|TCL_WRITABLE);
}

int
TclUnixOpenTemporaryFile(
    Tcl_Obj *dirObj,
    Tcl_Obj *basenameObj,
    Tcl_Obj *extensionObj,
    Tcl_Obj *resultingNameObj)
{
    Tcl_DString templ, tmp;
    const char *string;
    int fd;
    Tcl_Size length;

    /*
     * We should also check against making more than TMP_MAX of these.
     */

    if (dirObj) {
	string = TclGetStringFromObj(dirObj, &length);
	if (Tcl_UtfToExternalDStringEx(NULL, NULL, string, length, 0, &templ, NULL) != TCL_OK) {
	    return -1;
	}
    } else {
	Tcl_DStringInit(&templ);
	Tcl_DStringAppend(&templ, DefaultTempDir(), TCL_INDEX_NONE); /* INTL: native */
    }

    TclDStringAppendLiteral(&templ, "/");

    if (basenameObj) {
	string = TclGetStringFromObj(basenameObj, &length);
	if (Tcl_UtfToExternalDStringEx(NULL, NULL, string, length, 0, &tmp, NULL) != TCL_OK) {
	    Tcl_DStringFree(&tmp);
	    return -1;
	}
	TclDStringAppendDString(&templ, &tmp);
	Tcl_DStringFree(&tmp);
    } else {
	TclDStringAppendLiteral(&templ, "tcl");
    }

    TclDStringAppendLiteral(&templ, "_XXXXXX");

#ifdef HAVE_MKSTEMPS
    if (extensionObj) {
	string = TclGetStringFromObj(extensionObj, &length);
	if (Tcl_UtfToExternalDStringEx(NULL, NULL, string, length, 0, &tmp, NULL) != TCL_OK) {
	    Tcl_DStringFree(&templ);
	    return -1;
	}
	TclDStringAppendDString(&templ, &tmp);
	fd = mkstemps(Tcl_DStringValue(&templ), (int)Tcl_DStringLength(&tmp));
	Tcl_DStringFree(&tmp);
    } else
#endif
    {
	fd = mkstemp(Tcl_DStringValue(&templ));
    }

    if (fd == -1) {
	Tcl_DStringFree(&templ);
	return -1;
    }

    if (resultingNameObj) {
	if (Tcl_ExternalToUtfDStringEx(NULL, NULL, Tcl_DStringValue(&templ),
		Tcl_DStringLength(&templ), 0, &tmp, NULL) != TCL_OK) {
	    Tcl_DStringFree(&templ);
	    return -1;
	}
	Tcl_SetStringObj(resultingNameObj, Tcl_DStringValue(&tmp),
		Tcl_DStringLength(&tmp));
	Tcl_DStringFree(&tmp);
    } else {
	/*
	 * Try to delete the file immediately since we're not reporting the
	 * name to anyone. Note that we're *not* handling any errors from
	 * this!
	 */

	unlink(Tcl_DStringValue(&templ));
	errno = 0;
    }
    Tcl_DStringFree(&templ);

    return fd;
}

/*
 * Helper that does *part* of what tempnam() does.
 */

static const char *
DefaultTempDir(void)
{
    const char *dir;
    Tcl_StatBuf buf;

    dir = getenv("TMPDIR");
    if (dir && dir[0] && TclOSstat(dir, &buf) == 0 && S_ISDIR(buf.st_mode)
	    && access(dir, W_OK) == 0) {
	return dir;
    }

#ifdef P_tmpdir
    dir = P_tmpdir;
    if (TclOSstat(dir, &buf)==0 && S_ISDIR(buf.st_mode) && access(dir, W_OK)==0) {
	return dir;
    }
#endif

    /*
     * Assume that the default location ("/tmp" if not overridden) is always
     * an existing writable directory; we've no recovery mechanism if it
     * isn't.
     */

    return TCL_TEMPORARY_FILE_DIRECTORY;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateTemporaryDirectory --
 *
 *	Creates a temporary directory, possibly based on the supplied bits and
 *	pieces of template supplied in the arguments.
 *
 * Results:
 *	An object (refcount 0) containing the name of the newly-created
 *	directory, or NULL on failure.
 *
 * Side effects:
 *	Accesses the native filesystem. Makes a directory.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclpCreateTemporaryDirectory(
    Tcl_Obj *dirObj,
    Tcl_Obj *basenameObj)
{
    Tcl_DString templ, tmp;
    const char *string;

#define DEFAULT_TEMP_DIR_PREFIX	"tcl"

    /*
     * Build the template in writable memory from the user-supplied pieces and
     * some defaults.
     */

    if (dirObj) {
	string = TclGetString(dirObj);
	if (Tcl_UtfToExternalDStringEx(NULL, NULL, string, dirObj->length, 0,
		&templ, NULL) != TCL_OK) {
	    return NULL;
	}
    } else {
	Tcl_DStringInit(&templ);
	Tcl_DStringAppend(&templ, DefaultTempDir(), TCL_INDEX_NONE); /* INTL: native */
    }

    if (Tcl_DStringValue(&templ)[Tcl_DStringLength(&templ) - 1] != '/') {
	TclDStringAppendLiteral(&templ, "/");
    }

    if (basenameObj) {
	string = TclGetString(basenameObj);
	if (basenameObj->length) {
	    if (Tcl_UtfToExternalDStringEx(NULL, NULL, string, basenameObj->length,
		    0, &tmp, NULL) != TCL_OK) {
		Tcl_DStringFree(&templ);
		return NULL;
	    }
	    TclDStringAppendDString(&templ, &tmp);
	    Tcl_DStringFree(&tmp);
	} else {
	    TclDStringAppendLiteral(&templ, DEFAULT_TEMP_DIR_PREFIX);
	}
    } else {
	TclDStringAppendLiteral(&templ, DEFAULT_TEMP_DIR_PREFIX);
    }

    TclDStringAppendLiteral(&templ, "_XXXXXX");

    /*
     * Make the temporary directory.
     */

    if (mkdtemp(Tcl_DStringValue(&templ)) == NULL) {
	Tcl_DStringFree(&templ);
	return NULL;
    }

    /*
     * The template has been updated. Tell the caller what it was.
     */

    if (Tcl_ExternalToUtfDStringEx(NULL, NULL, Tcl_DStringValue(&templ),
	    Tcl_DStringLength(&templ), 0, &tmp, NULL) != TCL_OK) {
	Tcl_DStringFree(&templ);
	return NULL;
    }
    Tcl_DStringFree(&templ);
    return Tcl_DStringToObj(&tmp);
}

#if defined(__CYGWIN__)

static void
StatError(
    Tcl_Interp *interp,		/* The interp that has the error */
    Tcl_Obj *fileName)		/* The name of the file which caused the
				 * error. */
{
    Tcl_WinConvertError(GetLastError());
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("could not read \"%s\": %s",
	    TclGetString(fileName), Tcl_PosixError(interp)));
}

static WCHAR *
winPathFromObj(
    Tcl_Obj *fileName)
{
    int size;
    const char *native =  (const char *)Tcl_FSGetNativePath(fileName);
    WCHAR *winPath;

    size = cygwin_conv_path(1, native, NULL, 0);
    winPath = (WCHAR *)Tcl_Alloc(size);
    cygwin_conv_path(1, native, winPath, size);

    return winPath;
}

static const int attributeArray[] = {
    0x20, 0, 2, 0, 0, 1, 4
};

/*
 *----------------------------------------------------------------------
 *
 * GetUnixFileAttributes
 *
 *	Gets an attribute of a file.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If there is no error assigns to *attributePtrPtr the address of a new
 *	Tcl_Obj having a refCount of zero and containing the value of the
 *	specified attribute.
 *
 *
 *----------------------------------------------------------------------
 */

static int
GetUnixFileAttributes(
    Tcl_Interp *interp,		/* The interp to report errors to. */
    int objIndex,		/* The index of the attribute. */
    Tcl_Obj *fileName,		/* The pathname of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr)	/* Where to store the result. */
{
    int fileAttributes;
    WCHAR *winPath = winPathFromObj(fileName);

    fileAttributes = GetFileAttributesW(winPath);
    Tcl_Free(winPath);

    if (fileAttributes == -1) {
	StatError(interp, fileName);
	return TCL_ERROR;
    }

    TclNewIntObj(*attributePtrPtr,
	    (fileAttributes & attributeArray[objIndex]) != 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetUnixFileAttributes
 *
 *	Sets the readonly attribute of a file.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	The readonly attribute of the file is changed.
 *
 *---------------------------------------------------------------------------
 */

static int
SetUnixFileAttributes(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    int objIndex,		/* The index of the attribute. */
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr)	/* The attribute to set. */
{
    int yesNo, fileAttributes, old;
    WCHAR *winPath;

    if (Tcl_GetBooleanFromObj(interp, attributePtr, &yesNo) != TCL_OK) {
	return TCL_ERROR;
    }

    winPath = winPathFromObj(fileName);

    fileAttributes = old = GetFileAttributesW(winPath);

    if (fileAttributes == -1) {
	Tcl_Free(winPath);
	StatError(interp, fileName);
	return TCL_ERROR;
    }

    if (yesNo) {
	fileAttributes |= attributeArray[objIndex];
    } else {
	fileAttributes &= ~attributeArray[objIndex];
    }

    if ((fileAttributes != old)
	    && !SetFileAttributesW(winPath, fileAttributes)) {
	Tcl_Free(winPath);
	StatError(interp, fileName);
	return TCL_ERROR;
    }

    Tcl_Free(winPath);
    return TCL_OK;
}
#elif defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE)
/*
 *----------------------------------------------------------------------
 *
 * GetUnixFileAttributes
 *
 *	Gets the readonly attribute (user immutable flag) of a file.
 *
 * Results:
 *	Standard TCL result. Returns a new Tcl_Obj in attributePtrPtr if there
 *	is no error. The object will have ref count 0.
 *
 * Side effects:
 *	A new object is allocated.
 *
 *----------------------------------------------------------------------
 */

static int
GetUnixFileAttributes(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj **attributePtrPtr)	/* A pointer to return the object with. */
{
    Tcl_StatBuf statBuf;
    int result;

    result = TclpObjStat(fileName, &statBuf);

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not read \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }

    TclNewIntObj(*attributePtrPtr, (statBuf.st_flags & UF_IMMUTABLE) != 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetUnixFileAttributes
 *
 *	Sets the readonly attribute (user immutable flag) of a file.
 *
 * Results:
 *	Standard TCL result.
 *
 * Side effects:
 *	The readonly attribute of the file is changed.
 *
 *---------------------------------------------------------------------------
 */

static int
SetUnixFileAttributes(
    Tcl_Interp *interp,		/* The interp we are using for errors. */
    TCL_UNUSED(int) /*objIndex*/,
    Tcl_Obj *fileName,		/* The name of the file (UTF-8). */
    Tcl_Obj *attributePtr)	/* The attribute to set. */
{
    Tcl_StatBuf statBuf;
    int result, readonly;
    const char *native;

    if (Tcl_GetBooleanFromObj(interp, attributePtr, &readonly) != TCL_OK) {
	return TCL_ERROR;
    }

    result = TclpObjStat(fileName, &statBuf);

    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not read \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }

    if (readonly) {
	statBuf.st_flags |= UF_IMMUTABLE;
    } else {
	statBuf.st_flags &= ~UF_IMMUTABLE;
    }

    native = (const char *)Tcl_FSGetNativePath(fileName);
    result = chflags(native, statBuf.st_flags);		/* INTL: Native. */
    if (result != 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "could not set flags for file \"%s\": %s",
		    TclGetString(fileName), Tcl_PosixError(interp)));
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif /* defined(HAVE_CHFLAGS) && defined(UF_IMMUTABLE) */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
