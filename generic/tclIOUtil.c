/* 
 * tclIOUtil.c --
 *
 *	This file contains the implementation of Tcl's generic
 *	filesystem code, which supports a pluggable filesystem
 *	architecture allowing both platform specific filesystems and
 *	'virtual filesystems'.  All filesystem access should go through
 *	the functions defined in this file.  Most of this code was
 *	contributed by Vince Darley.
 *
 *	Parts of this file are based on code contributed by Karl
 *	Lehenbauer, Mark Diekhans and Peter da Silva.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclIOUtil.c,v 1.81.2.1 2003/05/22 19:12:06 dgp Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
#ifdef MAC_TCL
#include "tclMacInt.h"
#endif
#ifdef __WIN32__
/* for tclWinProcs->useWide */
#include "tclWinInt.h"
#endif
#include "tclFileSystem.h"

/*
 * Prototypes for procedures defined later in this file.
 */

static FilesystemRecord* FsGetIterator(void);
static void FsReleaseIterator(void);
static void FsThrExitProc(ClientData cd);

/* 
 * These form part of the native filesystem support.  They are needed
 * here because we have a few native filesystem functions (which are
 * the same for mac/win/unix) in this file.  There is no need to place
 * them in tclInt.h, because they are not (and should not be) used
 * anywhere else.
 */
extern CONST char *		tclpFileAttrStrings[];
extern CONST TclFileAttrProcs	tclpFileAttrProcs[];

/* 
 * The following functions are obsolete string based APIs, and should
 * be removed in a future release (Tcl 9 would be a good time).
 */

/* Obsolete */
int
Tcl_Stat(path, oldStyleBuf)
    CONST char *path;		/* Path of file to stat (in current CP). */
    struct stat *oldStyleBuf;	/* Filled with results of stat call. */
{
    int ret;
    Tcl_StatBuf buf;
    Tcl_Obj *pathPtr = Tcl_NewStringObj(path,-1);

    Tcl_IncrRefCount(pathPtr);
    ret = Tcl_FSStat(pathPtr, &buf);
    Tcl_DecrRefCount(pathPtr);
    if (ret != -1) {
#ifndef TCL_WIDE_INT_IS_LONG
#   define OUT_OF_RANGE(x) \
	(((Tcl_WideInt)(x)) < Tcl_LongAsWide(LONG_MIN) || \
	 ((Tcl_WideInt)(x)) > Tcl_LongAsWide(LONG_MAX))
#   define OUT_OF_URANGE(x) \
	(((Tcl_WideUInt)(x)) > (Tcl_WideUInt)ULONG_MAX)

	/*
	 * Perform the result-buffer overflow check manually.
	 *
	 * Note that ino_t/ino64_t is unsigned...
	 */

        if (OUT_OF_URANGE(buf.st_ino) || OUT_OF_RANGE(buf.st_size)
#ifdef HAVE_ST_BLOCKS
		|| OUT_OF_RANGE(buf.st_blocks)
#endif
	    ) {
#ifdef EFBIG
	    errno = EFBIG;
#else
#  ifdef EOVERFLOW
	    errno = EOVERFLOW;
#  else
#    error  "What status should be returned for file size out of range?"
#  endif
#endif
	    return -1;
	}

#   undef OUT_OF_RANGE
#   undef OUT_OF_URANGE
#endif /* !TCL_WIDE_INT_IS_LONG */

	/*
	 * Copy across all supported fields, with possible type
	 * coercions on those fields that change between the normal
	 * and lf64 versions of the stat structure (on Solaris at
	 * least.)  This is slow when the structure sizes coincide,
	 * but that's what you get for using an obsolete interface.
	 */

	oldStyleBuf->st_mode    = buf.st_mode;
	oldStyleBuf->st_ino     = (ino_t) buf.st_ino;
	oldStyleBuf->st_dev     = buf.st_dev;
	oldStyleBuf->st_rdev    = buf.st_rdev;
	oldStyleBuf->st_nlink   = buf.st_nlink;
	oldStyleBuf->st_uid     = buf.st_uid;
	oldStyleBuf->st_gid     = buf.st_gid;
	oldStyleBuf->st_size    = (off_t) buf.st_size;
	oldStyleBuf->st_atime   = buf.st_atime;
	oldStyleBuf->st_mtime   = buf.st_mtime;
	oldStyleBuf->st_ctime   = buf.st_ctime;
#ifdef HAVE_ST_BLOCKS
	oldStyleBuf->st_blksize = buf.st_blksize;
	oldStyleBuf->st_blocks  = (blkcnt_t) buf.st_blocks;
#endif
    }
    return ret;
}

/* Obsolete */
int
Tcl_Access(path, mode)
    CONST char *path;		/* Path of file to access (in current CP). */
    int mode;                   /* Permission setting. */
{
    int ret;
    Tcl_Obj *pathPtr = Tcl_NewStringObj(path,-1);
    Tcl_IncrRefCount(pathPtr);
    ret = Tcl_FSAccess(pathPtr,mode);
    Tcl_DecrRefCount(pathPtr);
    return ret;
}

/* Obsolete */
Tcl_Channel
Tcl_OpenFileChannel(interp, path, modeString, permissions)
    Tcl_Interp *interp;                 /* Interpreter for error reporting;
					 * can be NULL. */
    CONST char *path;                   /* Name of file to open. */
    CONST char *modeString;             /* A list of POSIX open modes or
					 * a string such as "rw". */
    int permissions;                    /* If the open involves creating a
					 * file, with what modes to create
					 * it? */
{
    Tcl_Channel ret;
    Tcl_Obj *pathPtr = Tcl_NewStringObj(path,-1);
    Tcl_IncrRefCount(pathPtr);
    ret = Tcl_FSOpenFileChannel(interp, pathPtr, modeString, permissions);
    Tcl_DecrRefCount(pathPtr);
    return ret;

}

/* Obsolete */
int
Tcl_Chdir(dirName)
    CONST char *dirName;
{
    int ret;
    Tcl_Obj *pathPtr = Tcl_NewStringObj(dirName,-1);
    Tcl_IncrRefCount(pathPtr);
    ret = Tcl_FSChdir(pathPtr);
    Tcl_DecrRefCount(pathPtr);
    return ret;
}

/* Obsolete */
char *
Tcl_GetCwd(interp, cwdPtr)
    Tcl_Interp *interp;
    Tcl_DString *cwdPtr;
{
    Tcl_Obj *cwd;
    cwd = Tcl_FSGetCwd(interp);
    if (cwd == NULL) {
	return NULL;
    } else {
	Tcl_DStringInit(cwdPtr);
	Tcl_DStringAppend(cwdPtr, Tcl_GetString(cwd), -1);
	Tcl_DecrRefCount(cwd);
	return Tcl_DStringValue(cwdPtr);
    }
}

/* Obsolete */
int
Tcl_EvalFile(interp, fileName)
    Tcl_Interp *interp;		/* Interpreter in which to process file. */
    CONST char *fileName;	/* Name of file to process.  Tilde-substitution
				 * will be performed on this name. */
{
    int ret;
    Tcl_Obj *pathPtr = Tcl_NewStringObj(fileName,-1);
    Tcl_IncrRefCount(pathPtr);
    ret = Tcl_FSEvalFile(interp, pathPtr);
    Tcl_DecrRefCount(pathPtr);
    return ret;
}


/* 
 * The 3 hooks for Stat, Access and OpenFileChannel are obsolete.  The
 * complete, general hooked filesystem APIs should be used instead.
 * This define decides whether to include the obsolete hooks and
 * related code.  If these are removed, we'll also want to remove them
 * from stubs/tclInt.  The only known users of these APIs are prowrap
 * and mktclapp.  New code/extensions should not use them, since they
 * do not provide as full support as the full filesystem API.
 * 
 * As soon as prowrap and mktclapp are updated to use the full
 * filesystem support, I suggest all these hooks are removed.
 */
#define USE_OBSOLETE_FS_HOOKS


#ifdef USE_OBSOLETE_FS_HOOKS
/*
 * The following typedef declarations allow for hooking into the chain
 * of functions maintained for 'Tcl_Stat(...)', 'Tcl_Access(...)' &
 * 'Tcl_OpenFileChannel(...)'.  Basically for each hookable function
 * a linked list is defined.
 */

typedef struct StatProc {
    TclStatProc_ *proc;		 /* Function to process a 'stat()' call */
    struct StatProc *nextPtr;    /* The next 'stat()' function to call */
} StatProc;

typedef struct AccessProc {
    TclAccessProc_ *proc;	 /* Function to process a 'access()' call */
    struct AccessProc *nextPtr;  /* The next 'access()' function to call */
} AccessProc;

typedef struct OpenFileChannelProc {
    TclOpenFileChannelProc_ *proc;  /* Function to process a
				     * 'Tcl_OpenFileChannel()' call */
    struct OpenFileChannelProc *nextPtr;
				    /* The next 'Tcl_OpenFileChannel()'
				     * function to call */
} OpenFileChannelProc;

/*
 * For each type of (obsolete) hookable function, a static node is
 * declared to hold the function pointer for the "built-in" routine
 * (e.g. 'TclpStat(...)') and the respective list is initialized as a
 * pointer to that node.
 * 
 * The "delete" functions (e.g. 'TclStatDeleteProc(...)') ensure that
 * these statically declared list entry cannot be inadvertently removed.
 *
 * This method avoids the need to call any sort of "initialization"
 * function.
 *
 * All three lists are protected by a global obsoleteFsHookMutex.
 */

static StatProc *statProcList = NULL;
static AccessProc *accessProcList = NULL;
static OpenFileChannelProc *openFileChannelProcList = NULL;

TCL_DECLARE_MUTEX(obsoleteFsHookMutex)

#endif /* USE_OBSOLETE_FS_HOOKS */

/* 
 * Declare the native filesystem support.  These functions should
 * be considered private to Tcl, and should really not be called
 * directly by any code other than this file (i.e. neither by
 * Tcl's core nor by extensions).  Similarly, the old string-based
 * Tclp... native filesystem functions should not be called.
 * 
 * The correct API to use now is the Tcl_FS... set of functions,
 * which ensure correct and complete virtual filesystem support.
 * 
 * We cannot make all of these static, since some of them
 * are implemented in the platform-specific directories.
 */
static Tcl_FSFilesystemSeparatorProc NativeFilesystemSeparator;
static Tcl_FSFreeInternalRepProc NativeFreeInternalRep;
Tcl_FSDupInternalRepProc NativeDupInternalRep;
static Tcl_FSCreateInternalRepProc NativeCreateNativeRep;
static Tcl_FSFileAttrStringsProc NativeFileAttrStrings;
static Tcl_FSFileAttrsGetProc NativeFileAttrsGet;
static Tcl_FSFileAttrsSetProc NativeFileAttrsSet;

/* 
 * The only reason these functions are not static is that they
 * are either called by code in the native (win/unix/mac) directories
 * or they are actually implemented in those directories.  They
 * should simply not be called by code outside Tcl's native
 * filesystem core.  i.e. they should be considered 'static' to
 * Tcl's filesystem code (if we ever built the native filesystem
 * support into a separate code library, this could actually be
 * enforced).
 */
Tcl_FSFilesystemPathTypeProc TclpFilesystemPathType;
Tcl_FSInternalToNormalizedProc TclpNativeToNormalized;
Tcl_FSStatProc TclpObjStat;
Tcl_FSAccessProc TclpObjAccess;	    
Tcl_FSMatchInDirectoryProc TclpMatchInDirectory;  
Tcl_FSGetCwdProc TclpObjGetCwd;     
Tcl_FSChdirProc TclpObjChdir;	    
Tcl_FSLstatProc TclpObjLstat;	    
Tcl_FSCopyFileProc TclpObjCopyFile; 
Tcl_FSDeleteFileProc TclpObjDeleteFile;	    
Tcl_FSRenameFileProc TclpObjRenameFile;	    
Tcl_FSCreateDirectoryProc TclpObjCreateDirectory;	    
Tcl_FSCopyDirectoryProc TclpObjCopyDirectory;	    
Tcl_FSRemoveDirectoryProc TclpObjRemoveDirectory;	    
Tcl_FSUnloadFileProc TclpUnloadFile;	    
Tcl_FSLinkProc TclpObjLink; 
Tcl_FSListVolumesProc TclpObjListVolumes;	    

/* 
 * Define the native filesystem dispatch table.  If necessary, it
 * is ok to make this non-static, but it should only be accessed
 * by the functions actually listed within it (or perhaps other
 * helper functions of them).  Anything which is not part of this
 * 'native filesystem implementation' should not be delving inside
 * here!
 */
Tcl_Filesystem tclNativeFilesystem = {
    "native",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    &NativePathInFilesystem,
    &NativeDupInternalRep,
    &NativeFreeInternalRep,
    &TclpNativeToNormalized,
    &NativeCreateNativeRep,
    &TclpObjNormalizePath,
    &TclpFilesystemPathType,
    &NativeFilesystemSeparator,
    &TclpObjStat,
    &TclpObjAccess,
    &TclpOpenFileChannel,
    &TclpMatchInDirectory,
    &TclpUtime,
#ifndef S_IFLNK
    NULL,
#else
    &TclpObjLink,
#endif /* S_IFLNK */
    &TclpObjListVolumes,
    &NativeFileAttrStrings,
    &NativeFileAttrsGet,
    &NativeFileAttrsSet,
    &TclpObjCreateDirectory,
    &TclpObjRemoveDirectory, 
    &TclpObjDeleteFile,
    &TclpObjCopyFile,
    &TclpObjRenameFile,
    &TclpObjCopyDirectory, 
    &TclpObjLstat,
    &TclpDlopen,
    &TclpObjGetCwd,
    &TclpObjChdir
};

/* 
 * Define the tail of the linked list.  Note that for unconventional
 * uses of Tcl without a native filesystem, we may in the future wish
 * to modify the current approach of hard-coding the native filesystem
 * in the lookup list 'filesystemList' below.
 * 
 * We initialize the record so that it thinks one file uses it.  This
 * means it will never be freed.
 */
static FilesystemRecord nativeFilesystemRecord = {
    NULL,
    &tclNativeFilesystem,
    1,
    NULL
};

/* 
 * The following few variables are protected by the 
 * filesystemMutex just below.
 */

/* 
 * This is incremented each time we modify the linked list of
 * filesystems.  Any time it changes, all cached filesystem
 * representations are suspect and must be freed.
 */
int theFilesystemEpoch = 0;

/*
 * Stores the linked list of filesystems.
 */
static FilesystemRecord *filesystemList = &nativeFilesystemRecord;

/* 
 * The number of loops which are currently iterating over the linked
 * list.  If this is greater than zero, we can't modify the list.
 */
static int filesystemIteratorsInProgress = 0;

/*
 * Someone wants to modify the list of filesystems if this is set.
 */
static int filesystemWantToModify = 0;

#ifdef TCL_THREADS
static Tcl_Condition filesystemOkToModify = NULL;
#endif

TCL_DECLARE_MUTEX(filesystemMutex)

/* 
 * Used to implement Tcl_FSGetCwd in a file-system independent way.
 * This is protected by the cwdMutex below.
 */
static Tcl_Obj* cwdPathPtr = NULL;
static int cwdPathEpoch = 0;
TCL_DECLARE_MUTEX(cwdMutex)

/*
 * This structure holds per-thread private copy of the
 * current directory maintained by the global cwdPathPtr.
 */
typedef struct ThreadSpecificData {
    int initialized;
    int cwdPathEpoch;
    Tcl_Obj *cwdPathPtr;
} ThreadSpecificData;

Tcl_ThreadDataKey dataKey;

/* 
 * Declare fallback support function and 
 * information for Tcl_FSLoadFile 
 */
static Tcl_FSUnloadFileProc FSUnloadTempFile;

/*
 * One of these structures is used each time we successfully load a
 * file from a file system by way of making a temporary copy of the
 * file on the native filesystem.  We need to store both the actual
 * unloadProc/clientData combination which was used, and the original
 * and modified filenames, so that we can correctly undo the entire
 * operation when we want to unload the code.
 */
typedef struct FsDivertLoad {
    Tcl_LoadHandle loadHandle;
    Tcl_FSUnloadFileProc *unloadProcPtr;	
    Tcl_Obj *divertedFile;
    Tcl_Filesystem *divertedFilesystem;
    ClientData divertedFileNativeRep;
} FsDivertLoad;

/* Now move on to the basic filesystem implementation */

static void
FsThrExitProc(cd)
    ClientData cd;
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData*)cd;
    if (tsdPtr->cwdPathPtr != NULL) {
	Tcl_DecrRefCount(tsdPtr->cwdPathPtr);
    }
}

int 
TclFSCwdPointerEquals(objPtr)
    Tcl_Obj* objPtr;
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_MutexLock(&cwdMutex);
    if (tsdPtr->initialized == 0) {
	Tcl_CreateThreadExitHandler(FsThrExitProc, (ClientData)tsdPtr);
	tsdPtr->initialized = 1;
    }
    if (tsdPtr->cwdPathPtr == NULL) {
        if (cwdPathPtr == NULL) {
    	    tsdPtr->cwdPathPtr = NULL;
        } else {
    	    tsdPtr->cwdPathPtr = Tcl_DuplicateObj(cwdPathPtr);
    	    Tcl_IncrRefCount(tsdPtr->cwdPathPtr);
        }
	tsdPtr->cwdPathEpoch = cwdPathEpoch;
    } else if (tsdPtr->cwdPathEpoch != cwdPathEpoch) { 
        Tcl_DecrRefCount(tsdPtr->cwdPathPtr);
        if (cwdPathPtr == NULL) {
    	    tsdPtr->cwdPathPtr = NULL;
        } else {
    	    tsdPtr->cwdPathPtr = Tcl_DuplicateObj(cwdPathPtr);
    	    Tcl_IncrRefCount(tsdPtr->cwdPathPtr);
        }
    }
    Tcl_MutexUnlock(&cwdMutex);

    return (tsdPtr->cwdPathPtr == objPtr); 
}

static FilesystemRecord* 
FsGetIterator(void) {
    Tcl_MutexLock(&filesystemMutex);
    filesystemIteratorsInProgress++;
    Tcl_MutexUnlock(&filesystemMutex);
    /* Now we know the list of filesystems cannot be modified */
    return filesystemList;
}

static void 
FsReleaseIterator(void) {
    Tcl_MutexLock(&filesystemMutex);
    filesystemIteratorsInProgress--;
    if (filesystemIteratorsInProgress == 0) {
        /* Notify any waiting threads that things are ok now */
	if (filesystemWantToModify > 0) {
	    Tcl_ConditionNotify(&filesystemOkToModify);
	}
    }
    Tcl_MutexUnlock(&filesystemMutex);
}

static void
FsUpdateCwd(cwdObj)
    Tcl_Obj *cwdObj;
{
    int len;
    char *str = NULL;
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (cwdObj != NULL) {
	str = Tcl_GetStringFromObj(cwdObj, &len);
    }

    Tcl_MutexLock(&cwdMutex);
    if (cwdPathPtr != NULL) {
        Tcl_DecrRefCount(cwdPathPtr);
    }
    if (cwdObj == NULL) {
	cwdPathPtr = NULL;
    } else {
	/* This must be stored as string obj! */
	cwdPathPtr = Tcl_NewStringObj(str, len); 
    	Tcl_IncrRefCount(cwdPathPtr);
    }
    cwdPathEpoch++;
    tsdPtr->cwdPathEpoch = cwdPathEpoch;
    Tcl_MutexUnlock(&cwdMutex);

    if (tsdPtr->cwdPathPtr) {
        Tcl_DecrRefCount(tsdPtr->cwdPathPtr);
    }
    if (cwdObj == NULL) {
	tsdPtr->cwdPathPtr = NULL;
    } else {
	tsdPtr->cwdPathPtr = Tcl_NewStringObj(str, len); 
	Tcl_IncrRefCount(tsdPtr->cwdPathPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclFinalizeFilesystem --
 *
 *	Clean up the filesystem.  After this, calls to all Tcl_FS...
 *	functions will fail.
 *	
 *	We will later call TclResetFilesystem to restore the FS
 *	to a pristine state.
 *	
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees any memory allocated by the filesystem.
 *
 *----------------------------------------------------------------------
 */

void
TclFinalizeFilesystem()
{
    /* 
     * Assumption that only one thread is active now.  Otherwise
     * we would need to put various mutexes around this code.
     */
    
    if (cwdPathPtr != NULL) {
	Tcl_DecrRefCount(cwdPathPtr);
	cwdPathPtr = NULL;
        cwdPathEpoch = 0;
    }

    /* 
     * Remove all filesystems, freeing any allocated memory
     * that is no longer needed
     */
    while (filesystemList != NULL) {
	FilesystemRecord *tmpFsRecPtr = filesystemList->nextPtr;
	if (filesystemList->fileRefCount > 0) {
	    /* 
	     * This filesystem must have some path objects still
	     * around which will be freed later (e.g. when unloading
	     * any shared libraries).  If not, then someone is
	     * causing us to leak memory.
	     */
	} else {
	    /* The native filesystem is static, so we don't free it */
	    if (filesystemList != &nativeFilesystemRecord) {
		ckfree((char *)filesystemList);
	    }
	}
	filesystemList = tmpFsRecPtr;
    }
    /*
     * Now filesystemList is NULL.  This means that any attempt
     * to use the filesystem is likely to fail.
     */
    statProcList = NULL;
    accessProcList = NULL;
    openFileChannelProcList = NULL;
#ifdef __WIN32__
    TclWinEncodingsCleanup();
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TclResetFilesystem --
 *
 *	Restore the filesystem to a pristine state.
 *	
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclResetFilesystem()
{
    filesystemList = &nativeFilesystemRecord;
    /* 
     * Note, at this point, I believe nativeFilesystemRecord ->
     * fileRefCount should equal 1 and if not, we should try to track
     * down the cause.
     */
    
    filesystemIteratorsInProgress = 0;
    filesystemWantToModify = 0;
#ifdef TCL_THREADS
    filesystemOkToModify = NULL;
#endif
#ifdef __WIN32__
    /* 
     * Cleans up the win32 API filesystem proc lookup table. This must
     * happen very late in finalization so that deleting of copied
     * dlls can occur.
     */
    TclWinResetInterfaces();
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSRegister --
 *
 *    Insert the filesystem function table at the head of the list of
 *    functions which are used during calls to all file-system
 *    operations.  The filesystem will be added even if it is 
 *    already in the list.  (You can use Tcl_FSData to
 *    check if it is in the list, provided the ClientData used was
 *    not NULL).
 *    
 *    Note that the filesystem handling is head-to-tail of the list.
 *    Each filesystem is asked in turn whether it can handle a
 *    particular request, _until_ one of them says 'yes'. At that
 *    point no further filesystems are asked.
 *    
 *    In particular this means if you want to add a diagnostic
 *    filesystem (which simply reports all fs activity), it must be 
 *    at the head of the list: i.e. it must be the last registered.
 *
 * Results:
 *    Normally TCL_OK; TCL_ERROR if memory for a new node in the list
 *    could not be allocated.
 *
 * Side effects:
 *    Memory allocated and modifies the link list for filesystems.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSRegister(clientData, fsPtr)
    ClientData clientData;    /* Client specific data for this fs */
    Tcl_Filesystem  *fsPtr;   /* The filesystem record for the new fs. */
{
    FilesystemRecord *newFilesystemPtr;

    if (fsPtr == NULL) {
	return TCL_ERROR;
    }

    newFilesystemPtr = (FilesystemRecord *) ckalloc(sizeof(FilesystemRecord));

    newFilesystemPtr->clientData = clientData;
    newFilesystemPtr->fsPtr = fsPtr;
    /* 
     * We start with a refCount of 1.  If this drops to zero, then
     * anyone is welcome to ckfree us.
     */
    newFilesystemPtr->fileRefCount = 1;

    /* 
     * Is this lock and wait strictly speaking necessary?  Since any
     * iterators out there will have grabbed a copy of the head of
     * the list and be iterating away from that, if we add a new
     * element to the head of the list, it can't possibly have any
     * effect on any of their loops.  In fact it could be better not
     * to wait, since we are adjusting the filesystem epoch, any
     * cached representations calculated by existing iterators are
     * going to have to be thrown away anyway.
     * 
     * However, since registering and unregistering filesystems is
     * a very rare action, this is not a very important point.
     */
    Tcl_MutexLock(&filesystemMutex);
    if (filesystemIteratorsInProgress) {
	filesystemWantToModify++;
	Tcl_ConditionWait(&filesystemOkToModify, &filesystemMutex, NULL);
	filesystemWantToModify--;
    }

    newFilesystemPtr->nextPtr = filesystemList;
    filesystemList = newFilesystemPtr;
    /* 
     * Increment the filesystem epoch counter, since existing paths
     * might conceivably now belong to different filesystems.
     */
    theFilesystemEpoch++;
    Tcl_MutexUnlock(&filesystemMutex);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSUnregister --
 *
 *    Remove the passed filesystem from the list of filesystem
 *    function tables.  It also ensures that the built-in
 *    (native) filesystem is not removable, although we may wish
 *    to change that decision in the future to allow a smaller
 *    Tcl core, in which the native filesystem is not used at
 *    all (we could, say, initialise Tcl completely over a network
 *    connection).
 *
 * Results:
 *    TCL_OK if the procedure pointer was successfully removed,
 *    TCL_ERROR otherwise.
 *
 * Side effects:
 *    Memory may be deallocated (or will be later, once no "path" 
 *    objects refer to this filesystem), but the list of registered
 *    filesystems is updated immediately.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSUnregister(fsPtr)
    Tcl_Filesystem  *fsPtr;   /* The filesystem record to remove. */
{
    int retVal = TCL_ERROR;
    FilesystemRecord *tmpFsRecPtr;
    FilesystemRecord *prevFsRecPtr = NULL;

    Tcl_MutexLock(&filesystemMutex);
    if (filesystemIteratorsInProgress) {
	filesystemWantToModify++;
	Tcl_ConditionWait(&filesystemOkToModify, &filesystemMutex, NULL);
	filesystemWantToModify--;
    }
    tmpFsRecPtr = filesystemList;
    /*
     * Traverse the 'filesystemList' looking for the particular node
     * whose 'fsPtr' member matches 'fsPtr' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == TCL_ERROR) && (tmpFsRecPtr != &nativeFilesystemRecord)) {
	if (tmpFsRecPtr->fsPtr == fsPtr) {
	    if (prevFsRecPtr == NULL) {
		filesystemList = filesystemList->nextPtr;
	    } else {
		prevFsRecPtr->nextPtr = tmpFsRecPtr->nextPtr;
	    }
	    /* 
	     * Increment the filesystem epoch counter, since existing
	     * paths might conceivably now belong to different
	     * filesystems.  This should also ensure that paths which
	     * have cached the filesystem which is about to be deleted
	     * do not reference that filesystem (which would of course
	     * lead to memory exceptions).
	     */
	    theFilesystemEpoch++;
	    
	    tmpFsRecPtr->fileRefCount--;
	    if (tmpFsRecPtr->fileRefCount <= 0) {
	        ckfree((char *)tmpFsRecPtr);
	    }

	    retVal = TCL_OK;
	} else {
	    prevFsRecPtr = tmpFsRecPtr;
	    tmpFsRecPtr = tmpFsRecPtr->nextPtr;
	}
    }

    Tcl_MutexUnlock(&filesystemMutex);
    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSMatchInDirectory --
 *
 *	This routine is used by the globbing code to search a directory
 *	for all files which match a given pattern.  The appropriate
 *	function for the filesystem to which pathPtr belongs will be
 *	called.  If pathPtr does not belong to any filesystem and if it
 *	is NULL or the empty string, then we assume the pattern is to be
 *	matched in the current working directory.  To avoid each
 *	filesystem's Tcl_FSMatchInDirectoryProc having to deal with this
 *	issue, we create a pathPtr on the fly (equal to the cwd), and
 *	then remove it from the results returned.  This makes filesystems
 *	easy to write, since they can assume the pathPtr passed to them
 *	is an ordinary path.  In fact this means we could remove such
 *	special case handling from Tcl's native filesystems.
 *	
 *	If 'pattern' is NULL, then pathPtr is assumed to be a fully
 *	specified path of a single file/directory which must be
 *	checked for existence and correct type.
 *
 * Results: 
 *	
 *	The return value is a standard Tcl result indicating whether an
 *	error occurred in globbing.  Error messages are placed in
 *	interp, but good results are placed in the resultPtr given.
 *	
 *	Recursive searches, e.g.
 *	
 *	   glob -dir $dir -join * pkgIndex.tcl
 *	   
 *	which must recurse through each directory matching '*' are
 *	handled internally by Tcl, by passing specific flags in a 
 *	modified 'types' parameter.  This means the actual filesystem
 *	only ever sees patterns which match in a single directory.
 *
 * Side effects:
 *	The interpreter may have an error message inserted into it.
 *
 *---------------------------------------------------------------------- 
 */

int
Tcl_FSMatchInDirectory(interp, result, pathPtr, pattern, types)
    Tcl_Interp *interp;		/* Interpreter to receive error messages. */
    Tcl_Obj *result;		/* List object to receive results. */
    Tcl_Obj *pathPtr;	        /* Contains path to directory to search. */
    CONST char *pattern;	/* Pattern to match against. */
    Tcl_GlobTypeData *types;	/* Object containing list of acceptable types.
				 * May be NULL. In particular the directory
				 * flag is very important. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSMatchInDirectoryProc *proc = fsPtr->matchInDirectoryProc;
	if (proc != NULL) {
	    return (*proc)(interp, result, pathPtr, pattern, types);
	}
    } else {
	Tcl_Obj* cwd;
	int ret = -1;
	if (pathPtr != NULL) {
	    int len;
	    Tcl_GetStringFromObj(pathPtr,&len);
	    if (len != 0) {
		/* 
		 * We have no idea how to match files in a directory
		 * which belongs to no known filesystem
		 */
		Tcl_SetErrno(ENOENT);
		return -1;
	    }
	}
	/* 
	 * We have an empty or NULL path.  This is defined to mean we
	 * must search for files within the current 'cwd'.  We
	 * therefore use that, but then since the proc we call will
	 * return results which include the cwd we must then trim it
	 * off the front of each path in the result.  We choose to deal
	 * with this here (in the generic code), since if we don't,
	 * every single filesystem's implementation of
	 * Tcl_FSMatchInDirectory will have to deal with it for us.
	 */
	cwd = Tcl_FSGetCwd(NULL);
	if (cwd == NULL) {
	    if (interp != NULL) {
		Tcl_SetResult(interp, "glob couldn't determine "
			  "the current working directory", TCL_STATIC);
	    }
	    return TCL_ERROR;
	}
	fsPtr = Tcl_FSGetFileSystemForPath(cwd);
	if (fsPtr != NULL) {
	    Tcl_FSMatchInDirectoryProc *proc = fsPtr->matchInDirectoryProc;
	    if (proc != NULL) {
		Tcl_Obj* tmpResultPtr = Tcl_NewListObj(0, NULL);
		Tcl_IncrRefCount(tmpResultPtr);
		ret = (*proc)(interp, tmpResultPtr, cwd, pattern, types);
		if (ret == TCL_OK) {
		    int resLength;

		    ret = Tcl_ListObjLength(interp, tmpResultPtr, &resLength);
		    if (ret == TCL_OK) {
			int i;

			for (i = 0; i < resLength; i++) {
			    Tcl_Obj *elt;
			    
			    Tcl_ListObjIndex(interp, tmpResultPtr, i, &elt);
			    Tcl_ListObjAppendElement(interp, result, 
				TclFSMakePathRelative(interp, elt, cwd));
			}
		    }
		}
		Tcl_DecrRefCount(tmpResultPtr);
	    }
	}
	Tcl_DecrRefCount(cwd);
	return ret;
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSMountsChanged --
 *
 *    Notify the filesystem that the available mounted filesystems
 *    (or within any one filesystem type, the number or location of
 *    mount points) have changed.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The global filesystem variable 'theFilesystemEpoch' is
 *    incremented.  The effect of this is to make all cached
 *    path representations invalid.  Clearly it should only therefore
 *    be called when it is really required!  There are a few 
 *    circumstances when it should be called:
 *    
 *    (1) when a new filesystem is registered or unregistered.  
 *    Strictly speaking this is only necessary if the new filesystem
 *    accepts file paths as is (normally the filesystem itself is
 *    really a shell which hasn't yet had any mount points established
 *    and so its 'pathInFilesystem' proc will always fail).  However,
 *    for safety, Tcl always calls this for you in these circumstances.
 * 
 *    (2) when additional mount points are established inside any
 *    existing filesystem (except the native fs)
 *    
 *    (3) when any filesystem (except the native fs) changes the list
 *    of available volumes.
 *    
 *    (4) when the mapping from a string representation of a file to
 *    a full, normalized path changes.  For example, if 'env(HOME)' 
 *    is modified, then any path containing '~' will map to a different
 *    filesystem location.  Therefore all such paths need to have
 *    their internal representation invalidated.
 *    
 *    Tcl has no control over (2) and (3), so any registered filesystem
 *    must make sure it calls this function when those situations
 *    occur.
 *    
 *    (Note: the reason for the exception in 2,3 for the native
 *    filesystem is that the native filesystem by default claims all
 *    unknown files even if it really doesn't understand them or if
 *    they don't exist).
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FSMountsChanged(fsPtr)
    Tcl_Filesystem *fsPtr;
{
    /* 
     * We currently don't do anything with this parameter.  We
     * could in the future only invalidate files for this filesystem
     * or otherwise take more advanced action.
     */
    (void)fsPtr;
    /* 
     * Increment the filesystem epoch counter, since existing paths
     * might now belong to different filesystems.
     */
    Tcl_MutexLock(&filesystemMutex);
    theFilesystemEpoch++;
    Tcl_MutexUnlock(&filesystemMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSData --
 *
 *    Retrieve the clientData field for the filesystem given,
 *    or NULL if that filesystem is not registered.
 *
 * Results:
 *    A clientData value, or NULL.  Note that if the filesystem
 *    was registered with a NULL clientData field, this function
 *    will return that NULL value.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

ClientData
Tcl_FSData(fsPtr)
    Tcl_Filesystem  *fsPtr;   /* The filesystem record to query. */
{
    ClientData retVal = NULL;
    FilesystemRecord *tmpFsRecPtr;

    tmpFsRecPtr = FsGetIterator();
    /*
     * Traverse the 'filesystemList' looking for the particular node
     * whose 'fsPtr' member matches 'fsPtr' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == NULL) && (tmpFsRecPtr != NULL)) {
	if (tmpFsRecPtr->fsPtr == fsPtr) {
	    retVal = tmpFsRecPtr->clientData;
	}
	tmpFsRecPtr = tmpFsRecPtr->nextPtr;
    }

    FsReleaseIterator();
    return (retVal);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclFSNormalizeToUniquePath --
 *
 * Description:
 *	Takes a path specification containing no ../, ./ sequences,
 *	and converts it into a unique path for the given platform.
 *      On MacOS, Unix, this means the path must be free of
 *	symbolic links/aliases, and on Windows it means we want the
 *	long form, with that long form's case-dependence (which gives
 *	us a unique, case-dependent path).
 *
 * Results:
 *	The pathPtr is modified in place.  The return value is
 *	the last byte offset which was recognised in the path
 *	string.
 *
 * Side effects:
 *	None (beyond the memory allocation for the result).
 *
 * Special notes:
 *	If the filesystem-specific normalizePathProcs can re-introduce
 *	../, ./ sequences into the path, then this function will
 *	not return the correct result.  This may be possible with
 *	symbolic links on unix/macos.
 *
 *      Important assumption: if startAt is non-zero, it must point
 *      to a directory separator that we know exists and is already
 *      normalized (so it is important not to point to the char just
 *      after the separator).
 *---------------------------------------------------------------------------
 */
int
TclFSNormalizeToUniquePath(interp, pathPtr, startAt, clientDataPtr)
    Tcl_Interp *interp;
    Tcl_Obj *pathPtr;
    int startAt;
    ClientData *clientDataPtr;
{
    FilesystemRecord *fsRecPtr;
    /* Ignore this variable */
    (void)clientDataPtr;
    
    /*
     * Call each of the "normalise path" functions in succession. This is
     * a special case, in which if we have a native filesystem handler,
     * we call it first.  This is because the root of Tcl's filesystem
     * is always a native filesystem (i.e. '/' on unix is native).
     */

    fsRecPtr = FsGetIterator();
    while (fsRecPtr != NULL) {
        if (fsRecPtr == &nativeFilesystemRecord) {
	    Tcl_FSNormalizePathProc *proc = fsRecPtr->fsPtr->normalizePathProc;
	    if (proc != NULL) {
		startAt = (*proc)(interp, pathPtr, startAt);
	    }
	    break;
        }
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();
    
    fsRecPtr = FsGetIterator();
    while (fsRecPtr != NULL) {
	/* Skip the native system next time through */
	if (fsRecPtr != &nativeFilesystemRecord) {
	    Tcl_FSNormalizePathProc *proc = fsRecPtr->fsPtr->normalizePathProc;
	    if (proc != NULL) {
		startAt = (*proc)(interp, pathPtr, startAt);
	    }
	    /* 
	     * We could add an efficiency check like this:
	     * 
	     *   if (retVal == length-of(pathPtr)) {break;}
	     * 
	     * but there's not much benefit.
	     */
	}
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();

    return (startAt);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclGetOpenMode --
 *
 * Description:
 *	Computes a POSIX mode mask for opening a file, from a given string,
 *	and also sets a flag to indicate whether the caller should seek to
 *	EOF after opening the file.
 *
 * Results:
 *	On success, returns mode to pass to "open". If an error occurs, the
 *	return value is -1 and if interp is not NULL, sets interp's result
 *	object to an error message.
 *
 * Side effects:
 *	Sets the integer referenced by seekFlagPtr to 1 to tell the caller
 *	to seek to EOF after opening the file.
 *
 * Special note:
 *	This code is based on a prototype implementation contributed
 *	by Mark Diekhans.
 *
 *---------------------------------------------------------------------------
 */

int
TclGetOpenMode(interp, string, seekFlagPtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting - may be NULL. */
    CONST char *string;			/* Mode string, e.g. "r+" or
					 * "RDONLY CREAT". */
    int *seekFlagPtr;			/* Set this to 1 if the caller
                                         * should seek to EOF during the
                                         * opening of the file. */
{
    int mode, modeArgc, c, i, gotRW;
    CONST char **modeArgv, *flag;
#define RW_MODES (O_RDONLY|O_WRONLY|O_RDWR)

    /*
     * Check for the simpler fopen-like access modes (e.g. "r").  They
     * are distinguished from the POSIX access modes by the presence
     * of a lower-case first letter.
     */

    *seekFlagPtr = 0;
    mode = 0;

    /*
     * Guard against international characters before using byte oriented
     * routines.
     */

    if (!(string[0] & 0x80)
	    && islower(UCHAR(string[0]))) { /* INTL: ISO only. */
	switch (string[0]) {
	    case 'r':
		mode = O_RDONLY;
		break;
	    case 'w':
		mode = O_WRONLY|O_CREAT|O_TRUNC;
		break;
	    case 'a':
		mode = O_WRONLY|O_CREAT;
                *seekFlagPtr = 1;
		break;
	    default:
		error:
                if (interp != (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp,
                            "illegal access mode \"", string, "\"",
                            (char *) NULL);
                }
		return -1;
	}
	if (string[1] == '+') {
	    mode &= ~(O_RDONLY|O_WRONLY);
	    mode |= O_RDWR;
	    if (string[2] != 0) {
		goto error;
	    }
	} else if (string[1] != 0) {
	    goto error;
	}
        return mode;
    }

    /*
     * The access modes are specified using a list of POSIX modes
     * such as O_CREAT.
     *
     * IMPORTANT NOTE: We rely on Tcl_SplitList working correctly when
     * a NULL interpreter is passed in.
     */

    if (Tcl_SplitList(interp, string, &modeArgc, &modeArgv) != TCL_OK) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AddErrorInfo(interp,
                    "\n    while processing open access modes \"");
            Tcl_AddErrorInfo(interp, string);
            Tcl_AddErrorInfo(interp, "\"");
        }
        return -1;
    }
    
    gotRW = 0;
    for (i = 0; i < modeArgc; i++) {
	flag = modeArgv[i];
	c = flag[0];
	if ((c == 'R') && (strcmp(flag, "RDONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDONLY;
	    gotRW = 1;
	} else if ((c == 'W') && (strcmp(flag, "WRONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_WRONLY;
	    gotRW = 1;
	} else if ((c == 'R') && (strcmp(flag, "RDWR") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDWR;
	    gotRW = 1;
	} else if ((c == 'A') && (strcmp(flag, "APPEND") == 0)) {
	    mode |= O_APPEND;
            *seekFlagPtr = 1;
	} else if ((c == 'C') && (strcmp(flag, "CREAT") == 0)) {
	    mode |= O_CREAT;
	} else if ((c == 'E') && (strcmp(flag, "EXCL") == 0)) {
	    mode |= O_EXCL;
	} else if ((c == 'N') && (strcmp(flag, "NOCTTY") == 0)) {
#ifdef O_NOCTTY
	    mode |= O_NOCTTY;
#else
	    if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'N') && (strcmp(flag, "NONBLOCK") == 0)) {
#if defined(O_NDELAY) || defined(O_NONBLOCK)
#   ifdef O_NONBLOCK
	    mode |= O_NONBLOCK;
#   else
	    mode |= O_NDELAY;
#   endif
#else
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'T') && (strcmp(flag, "TRUNC") == 0)) {
	    mode |= O_TRUNC;
	} else {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "invalid access mode \"", flag,
                        "\": must be RDONLY, WRONLY, RDWR, APPEND, CREAT",
                        " EXCL, NOCTTY, NONBLOCK, or TRUNC", (char *) NULL);
            }
	    ckfree((char *) modeArgv);
	    return -1;
	}
    }
    ckfree((char *) modeArgv);
    if (!gotRW) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "access mode must include either",
                    " RDONLY, WRONLY, or RDWR", (char *) NULL);
        }
	return -1;
    }
    return mode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSEvalFile --
 *
 *	Read in a file and process the entire file as one gigantic
 *	Tcl command.
 *
 * Results:
 *	A standard Tcl result, which is either the result of executing
 *	the file or an error indicating why the file couldn't be read.
 *
 * Side effects:
 *	Depends on the commands in the file.  During the evaluation
 *	of the contents of the file, iPtr->scriptFile is made to
 *	point to pathPtr (the old value is cached and replaced when
 *	this function returns).
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSEvalFile(interp, pathPtr)
    Tcl_Interp *interp;		/* Interpreter in which to process file. */
    Tcl_Obj *pathPtr;		/* Path of file to process.  Tilde-substitution
				 * will be performed on this name. */
{
    int result;
    Tcl_StatBuf statBuf;
    Tcl_Obj *oldScriptFile;
    Interp *iPtr;
    Tcl_Channel chan;
    Tcl_Obj *objPtr;

    if (Tcl_FSGetNormalizedPath(interp, pathPtr) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_FSStat(pathPtr, &statBuf) == -1) {
        Tcl_SetErrno(errno);
	Tcl_AppendResult(interp, "couldn't read file \"", 
		Tcl_GetString(pathPtr),
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    chan = Tcl_FSOpenFileChannel(interp, pathPtr, "r", 0644);
    if (chan == (Tcl_Channel) NULL) {
        Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "couldn't read file \"", 
		Tcl_GetString(pathPtr),
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    result = TCL_ERROR;
    objPtr = Tcl_NewObj();
    Tcl_IncrRefCount(objPtr);
    /*
     * The eofchar is \32 (^Z).  This is the usual on Windows, but we
     * effect this cross-platform to allow for scripted documents.
     * [Bug: 2040]
     */

    Tcl_SetChannelOption(interp, chan, "-eofchar", "\32");
    if (Tcl_ReadChars(chan, objPtr, -1, 0) < 0) {
        Tcl_Close(interp, chan);
	Tcl_AppendResult(interp, "couldn't read file \"", 
		Tcl_GetString(pathPtr),
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto end;
    }
    if (Tcl_Close(interp, chan) != TCL_OK) {
        goto end;
    }

    iPtr = (Interp *) interp;
    oldScriptFile = iPtr->scriptFile;
    iPtr->scriptFile = pathPtr;
    Tcl_IncrRefCount(iPtr->scriptFile);

    result = Tcl_EvalObjEx(interp, objPtr, TCL_EVAL_DIRECT);

    /* 
     * Now we have to be careful; the script may have changed the
     * iPtr->scriptFile value, so we must reset it without
     * assuming it still points to 'pathPtr'.
     */
    if (iPtr->scriptFile != NULL) {
	Tcl_DecrRefCount(iPtr->scriptFile);
    }
    iPtr->scriptFile = oldScriptFile;

    if (result == TCL_RETURN) {
	result = TclUpdateReturnInfo(iPtr);
    } else if (result == TCL_ERROR) {
	char msg[200 + TCL_INTEGER_SPACE];

	/*
	 * Record information telling where the error occurred.
	 */

	sprintf(msg, "\n    (file \"%.150s\" line %d)", Tcl_GetString(pathPtr),
		interp->errorLine);
	Tcl_AddErrorInfo(interp, msg);
    }

    end:
    Tcl_DecrRefCount(objPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetErrno --
 *
 *	Gets the current value of the Tcl error code variable. This is
 *	currently the global variable "errno" but could in the future
 *	change to something else.
 *
 * Results:
 *	The value of the Tcl error code variable.
 *
 * Side effects:
 *	None. Note that the value of the Tcl error code variable is
 *	UNDEFINED if a call to Tcl_SetErrno did not precede this call.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetErrno()
{
    return errno;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetErrno --
 *
 *	Sets the Tcl error code variable to the supplied value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the value of the Tcl error code variable.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetErrno(err)
    int err;			/* The new value. */
{
    errno = err;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PosixError --
 *
 *	This procedure is typically called after UNIX kernel calls
 *	return errors.  It stores machine-readable information about
 *	the error in $errorCode returns an information string for
 *	the caller's use.
 *
 * Results:
 *	The return value is a human-readable string describing the
 *	error.
 *
 * Side effects:
 *	The global variable $errorCode is reset.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Tcl_PosixError(interp)
    Tcl_Interp *interp;		/* Interpreter whose $errorCode variable
				 * is to be changed. */
{
    CONST char *id, *msg;

    msg = Tcl_ErrnoMsg(errno);
    id = Tcl_ErrnoId();
    Tcl_SetErrorCode(interp, "POSIX", id, msg, (char *) NULL);
    return msg;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSStat --
 *
 *	This procedure replaces the library version of stat and lsat.
 *	
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      See stat documentation.
 *
 * Side effects:
 *      See stat documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSStat(pathPtr, buf)
    Tcl_Obj *pathPtr;		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *buf;		/* Filled with results of stat call. */
{
    Tcl_Filesystem *fsPtr;
#ifdef USE_OBSOLETE_FS_HOOKS
    struct stat oldStyleStatBuffer;
    int retVal = -1;

    /*
     * Call each of the "stat" function in succession.  A non-return
     * value of -1 indicates the particular function has succeeded.
     */

    Tcl_MutexLock(&obsoleteFsHookMutex);
    
    if (statProcList != NULL) {
	StatProc *statProcPtr;
	char *path;
	Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(NULL, pathPtr);
	if (transPtr == NULL) {
	    path = NULL;
	} else {
	    path = Tcl_GetString(transPtr);
	}

	statProcPtr = statProcList;
	while ((retVal == -1) && (statProcPtr != NULL)) {
	    retVal = (*statProcPtr->proc)(path, &oldStyleStatBuffer);
	    statProcPtr = statProcPtr->nextPtr;
	}
    }
    
    Tcl_MutexUnlock(&obsoleteFsHookMutex);
    if (retVal != -1) {
	/*
	 * Note that EOVERFLOW is not a problem here, and these
	 * assignments should all be widening (if not identity.)
	 */
	buf->st_mode = oldStyleStatBuffer.st_mode;
	buf->st_ino = oldStyleStatBuffer.st_ino;
	buf->st_dev = oldStyleStatBuffer.st_dev;
	buf->st_rdev = oldStyleStatBuffer.st_rdev;
	buf->st_nlink = oldStyleStatBuffer.st_nlink;
	buf->st_uid = oldStyleStatBuffer.st_uid;
	buf->st_gid = oldStyleStatBuffer.st_gid;
	buf->st_size = Tcl_LongAsWide(oldStyleStatBuffer.st_size);
	buf->st_atime = oldStyleStatBuffer.st_atime;
	buf->st_mtime = oldStyleStatBuffer.st_mtime;
	buf->st_ctime = oldStyleStatBuffer.st_ctime;
#ifdef HAVE_ST_BLOCKS
	buf->st_blksize = oldStyleStatBuffer.st_blksize;
	buf->st_blocks = Tcl_LongAsWide(oldStyleStatBuffer.st_blocks);
#endif
        return retVal;
    }
#endif /* USE_OBSOLETE_FS_HOOKS */
    fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSStatProc *proc = fsPtr->statProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, buf);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSLstat --
 *
 *	This procedure replaces the library version of lstat.
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.  If no 'lstat' function is listed,
 *	but a 'stat' function is, then Tcl will fall back on the
 *	stat function.
 *
 * Results:
 *      See lstat documentation.
 *
 * Side effects:
 *      See lstat documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSLstat(pathPtr, buf)
    Tcl_Obj *pathPtr;		/* Path of file to stat (in current CP). */
    Tcl_StatBuf *buf;		/* Filled with results of stat call. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSLstatProc *proc = fsPtr->lstatProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, buf);
	} else {
	    Tcl_FSStatProc *sproc = fsPtr->statProc;
	    if (sproc != NULL) {
		return (*sproc)(pathPtr, buf);
	    }
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSAccess --
 *
 *	This procedure replaces the library version of access.
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      See access documentation.
 *
 * Side effects:
 *      See access documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSAccess(pathPtr, mode)
    Tcl_Obj *pathPtr;		/* Path of file to access (in current CP). */
    int mode;                   /* Permission setting. */
{
    Tcl_Filesystem *fsPtr;
#ifdef USE_OBSOLETE_FS_HOOKS
    int retVal = -1;

    /*
     * Call each of the "access" function in succession.  A non-return
     * value of -1 indicates the particular function has succeeded.
     */

    Tcl_MutexLock(&obsoleteFsHookMutex);

    if (accessProcList != NULL) {
	AccessProc *accessProcPtr;
	char *path;
	Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(NULL, pathPtr);
	if (transPtr == NULL) {
	    path = NULL;
	} else {
	    path = Tcl_GetString(transPtr);
	}

	accessProcPtr = accessProcList;
	while ((retVal == -1) && (accessProcPtr != NULL)) {
	    retVal = (*accessProcPtr->proc)(path, mode);
	    accessProcPtr = accessProcPtr->nextPtr;
	}
    }
    
    Tcl_MutexUnlock(&obsoleteFsHookMutex);
    if (retVal != -1) {
	return retVal;
    }
#endif /* USE_OBSOLETE_FS_HOOKS */
    fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSAccessProc *proc = fsPtr->accessProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, mode);
	}
    }

    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSOpenFileChannel --
 *
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *	The new channel or NULL, if the named file could not be opened.
 *
 * Side effects:
 *	May open the channel and may cause creation of a file on the
 *	file system.
 *
 *----------------------------------------------------------------------
 */
 
Tcl_Channel
Tcl_FSOpenFileChannel(interp, pathPtr, modeString, permissions)
    Tcl_Interp *interp;                 /* Interpreter for error reporting;
                                         * can be NULL. */
    Tcl_Obj *pathPtr;                   /* Name of file to open. */
    CONST char *modeString;             /* A list of POSIX open modes or
                                         * a string such as "rw". */
    int permissions;                    /* If the open involves creating a
                                         * file, with what modes to create
                                         * it? */
{
    Tcl_Filesystem *fsPtr;
#ifdef USE_OBSOLETE_FS_HOOKS
    Tcl_Channel retVal = NULL;

    /*
     * Call each of the "Tcl_OpenFileChannel" functions in succession.
     * A non-NULL return value indicates the particular function has
     * succeeded.
     */

    Tcl_MutexLock(&obsoleteFsHookMutex);
    if (openFileChannelProcList != NULL) {
	OpenFileChannelProc *openFileChannelProcPtr;
	char *path;
	Tcl_Obj *transPtr = Tcl_FSGetTranslatedPath(interp, pathPtr);
	
	if (transPtr == NULL) {
	    path = NULL;
	} else {
	    path = Tcl_GetString(transPtr);
	}

	openFileChannelProcPtr = openFileChannelProcList;
	
	while ((retVal == NULL) && (openFileChannelProcPtr != NULL)) {
	    retVal = (*openFileChannelProcPtr->proc)(interp, path,
						     modeString, permissions);
	    openFileChannelProcPtr = openFileChannelProcPtr->nextPtr;
	}
    }
    Tcl_MutexUnlock(&obsoleteFsHookMutex);
    if (retVal != NULL) {
	return retVal;
    }
#endif /* USE_OBSOLETE_FS_HOOKS */
    
    /* 
     * We need this just to ensure we return the correct error messages
     * under some circumstances.
     */
    if (Tcl_FSGetNormalizedPath(interp, pathPtr) == NULL) {
        return NULL;
    }
    
    fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSOpenFileChannelProc *proc = fsPtr->openFileChannelProc;
	if (proc != NULL) {
	    int mode, seekFlag;
	    mode = TclGetOpenMode(interp, modeString, &seekFlag);
	    if (mode == -1) {
	        return NULL;
	    }
	    retVal = (*proc)(interp, pathPtr, mode, permissions);
	    if (retVal != NULL) {
		if (seekFlag) {
		    if (Tcl_Seek(retVal, (Tcl_WideInt)0, 
				 SEEK_END) < (Tcl_WideInt)0) {
			if (interp != (Tcl_Interp *) NULL) {
			    Tcl_AppendResult(interp,
			      "could not seek to end of file while opening \"",
			      Tcl_GetString(pathPtr), "\": ", 
			      Tcl_PosixError(interp), (char *) NULL);
			}
			Tcl_Close(NULL, retVal);
			return NULL;
		    }
		}
	    }
	    return retVal;
	}
    }
    /* File doesn't belong to any filesystem that can open it */
    Tcl_SetErrno(ENOENT);
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't open \"", 
			 Tcl_GetString(pathPtr), "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSUtime --
 *
 *	This procedure replaces the library version of utime.
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      See utime documentation.
 *
 * Side effects:
 *      See utime documentation.
 *
 *----------------------------------------------------------------------
 */

int 
Tcl_FSUtime (pathPtr, tval)
    Tcl_Obj *pathPtr;       /* File to change access/modification times */
    struct utimbuf *tval;   /* Structure containing access/modification 
                             * times to use.  Should not be modified. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSUtimeProc *proc = fsPtr->utimeProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, tval);
	}
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * NativeFileAttrStrings --
 *
 *	This procedure implements the platform dependent 'file
 *	attributes' subcommand, for the native filesystem, for listing
 *	the set of possible attribute strings.  This function is part
 *	of Tcl's native filesystem support, and is placed here because
 *	it is shared by Unix, MacOS and Windows code.
 *
 * Results:
 *      An array of strings
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static CONST char**
NativeFileAttrStrings(pathPtr, objPtrRef)
    Tcl_Obj *pathPtr;
    Tcl_Obj** objPtrRef;
{
    return tclpFileAttrStrings;
}

/*
 *----------------------------------------------------------------------
 *
 * NativeFileAttrsGet --
 *
 *	This procedure implements the platform dependent
 *	'file attributes' subcommand, for the native
 *	filesystem, for 'get' operations.  This function is part
 *	of Tcl's native filesystem support, and is placed here
 *	because it is shared by Unix, MacOS and Windows code.
 *
 * Results:
 *      Standard Tcl return code.  The object placed in objPtrRef
 *      (if TCL_OK was returned) is likely to have a refCount of zero.
 *      Either way we must either store it somewhere (e.g. the Tcl 
 *      result), or Incr/Decr its refCount to ensure it is properly
 *      freed.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
NativeFileAttrsGet(interp, index, pathPtr, objPtrRef)
    Tcl_Interp *interp;		/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* path of file we are operating on. */
    Tcl_Obj **objPtrRef;	/* for output. */
{
    return (*tclpFileAttrProcs[index].getProc)(interp, index, 
					       pathPtr, objPtrRef);
}

/*
 *----------------------------------------------------------------------
 *
 * NativeFileAttrsSet --
 *
 *	This procedure implements the platform dependent
 *	'file attributes' subcommand, for the native
 *	filesystem, for 'set' operations. This function is part
 *	of Tcl's native filesystem support, and is placed here
 *	because it is shared by Unix, MacOS and Windows code.
 *
 * Results:
 *      Standard Tcl return code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
NativeFileAttrsSet(interp, index, pathPtr, objPtr)
    Tcl_Interp *interp;		/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* path of file we are operating on. */
    Tcl_Obj *objPtr;		/* set to this value. */
{
    return (*tclpFileAttrProcs[index].setProc)(interp, index,
					       pathPtr, objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSFileAttrStrings --
 *
 *	This procedure implements part of the hookable 'file
 *	attributes' subcommand.  The appropriate function for the
 *	filesystem to which pathPtr belongs will be called.
 *
 * Results:
 *      The called procedure may either return an array of strings,
 *      or may instead return NULL and place a Tcl list into the 
 *      given objPtrRef.  Tcl will take that list and first increment
 *      its refCount before using it.  On completion of that use, Tcl
 *      will decrement its refCount.  Hence if the list should be
 *      disposed of by Tcl when done, it should have a refCount of zero,
 *      and if the list should not be disposed of, the filesystem
 *      should ensure it retains a refCount on the object.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

CONST char **
Tcl_FSFileAttrStrings(pathPtr, objPtrRef)
    Tcl_Obj* pathPtr;
    Tcl_Obj** objPtrRef;
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSFileAttrStringsProc *proc = fsPtr->fileAttrStringsProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, objPtrRef);
	}
    }
    Tcl_SetErrno(ENOENT);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSFileAttrsGet --
 *
 *	This procedure implements read access for the hookable 'file
 *	attributes' subcommand.  The appropriate function for the
 *	filesystem to which pathPtr belongs will be called.
 *
 * Results:
 *      Standard Tcl return code.  The object placed in objPtrRef
 *      (if TCL_OK was returned) is likely to have a refCount of zero.
 *      Either way we must either store it somewhere (e.g. the Tcl 
 *      result), or Incr/Decr its refCount to ensure it is properly
 *      freed.

 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSFileAttrsGet(interp, index, pathPtr, objPtrRef)
    Tcl_Interp *interp;		/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* filename we are operating on. */
    Tcl_Obj **objPtrRef;	/* for output. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSFileAttrsGetProc *proc = fsPtr->fileAttrsGetProc;
	if (proc != NULL) {
	    return (*proc)(interp, index, pathPtr, objPtrRef);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSFileAttrsSet --
 *
 *	This procedure implements write access for the hookable 'file
 *	attributes' subcommand.  The appropriate function for the
 *	filesystem to which pathPtr belongs will be called.
 *
 * Results:
 *      Standard Tcl return code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSFileAttrsSet(interp, index, pathPtr, objPtr)
    Tcl_Interp *interp;		/* The interpreter for error reporting. */
    int index;			/* index of the attribute command. */
    Tcl_Obj *pathPtr;		/* filename we are operating on. */
    Tcl_Obj *objPtr;		/* Input value. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSFileAttrsSetProc *proc = fsPtr->fileAttrsSetProc;
	if (proc != NULL) {
	    return (*proc)(interp, index, pathPtr, objPtr);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSGetCwd --
 *
 *	This function replaces the library version of getcwd().
 *	
 *	Most VFS's will *not* implement a 'cwdProc'.  Tcl now maintains
 *	its own record (in a Tcl_Obj) of the cwd, and an attempt
 *	is made to synchronise this with the cwd's containing filesystem,
 *	if that filesystem provides a cwdProc (e.g. the native filesystem).
 *	
 *	Note that if Tcl's cwd is not in the native filesystem, then of
 *	course Tcl's cwd and the native cwd are different: extensions
 *	should therefore ensure they only access the cwd through this
 *	function to avoid confusion.
 *	
 *	If a global cwdPathPtr already exists, it is cached in the thread's
 *	private data structures and reference to the cached copy is returned,
 *	subject to a synchronisation attempt in that cwdPathPtr's fs.
 *	
 *	Otherwise, the chain of functions that have been "inserted"
 *	into the filesystem will be called in succession until either a
 *	value other than NULL is returned, or the entire list is
 *	visited.
 *
 * Results:
 *	The result is a pointer to a Tcl_Obj specifying the current
 *	directory, or NULL if the current directory could not be
 *	determined.  If NULL is returned, an error message is left in the
 *	interp's result.  
 *	
 *	The result already has its refCount incremented for the caller.
 *	When it is no longer needed, that refCount should be decremented.
 *
 * Side effects:
 *	Various objects may be freed and allocated.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
Tcl_FSGetCwd(interp)
    Tcl_Interp *interp;
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    
    if (TclFSCwdPointerEquals(NULL)) {
	FilesystemRecord *fsRecPtr;
	Tcl_Obj *retVal = NULL;

	/* 
	 * We've never been called before, try to find a cwd.  Call
	 * each of the "Tcl_GetCwd" function in succession.  A non-NULL
	 * return value indicates the particular function has
	 * succeeded.
	 */

	fsRecPtr = FsGetIterator();
	while ((retVal == NULL) && (fsRecPtr != NULL)) {
	    Tcl_FSGetCwdProc *proc = fsRecPtr->fsPtr->getCwdProc;
	    if (proc != NULL) {
		retVal = (*proc)(interp);
	    }
	    fsRecPtr = fsRecPtr->nextPtr;
	}
	FsReleaseIterator();
	/* 
	 * Now the 'cwd' may NOT be normalized, at least on some
	 * platforms.  For the sake of efficiency, we want a completely
	 * normalized cwd at all times.
	 * 
	 * Finally, if retVal is NULL, we do not have a cwd, which
	 * could be problematic.
	 */
	if (retVal != NULL) {
	    Tcl_Obj *norm = TclFSNormalizeAbsolutePath(interp, retVal, NULL);
	    if (norm != NULL) {
		/* 
		 * We found a cwd, which is now in our global storage.
		 * We must make a copy.  Norm already has a refCount of
		 * 1.
		 * 
		 * Threading issue: note that multiple threads at system
		 * startup could in principle call this procedure 
		 * simultaneously.  They will therefore each set the
		 * cwdPathPtr independently.  That behaviour is a bit
		 * peculiar, but should be fine.  Once we have a cwd,
		 * we'll always be in the 'else' branch below which
		 * is simpler.
		 */
                FsUpdateCwd(norm);
	    }
	    Tcl_DecrRefCount(retVal);
	}
    } else {
	/* 
	 * We already have a cwd cached, but we want to give the
	 * filesystem it is in a chance to check whether that cwd
	 * has changed, or is perhaps no longer accessible.  This
	 * allows an error to be thrown if, say, the permissions on
	 * that directory have changed.
	 */
	Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(tsdPtr->cwdPathPtr);
	/* 
	 * If the filesystem couldn't be found, or if no cwd function
	 * exists for this filesystem, then we simply assume the cached
	 * cwd is ok.  If we do call a cwd, we must watch for errors
	 * (if the cwd returns NULL).  This ensures that, say, on Unix
	 * if the permissions of the cwd change, 'pwd' does actually
	 * throw the correct error in Tcl.  (This is tested for in the
	 * test suite on unix).
	 */
	if (fsPtr != NULL) {
	    Tcl_FSGetCwdProc *proc = fsPtr->getCwdProc;
	    if (proc != NULL) {
		Tcl_Obj *retVal = (*proc)(interp);
		if (retVal != NULL) {
		    Tcl_Obj *norm = TclFSNormalizeAbsolutePath(interp, retVal, NULL);
		    /* 
		     * Check whether cwd has changed from the value
		     * previously stored in cwdPathPtr.  Really 'norm'
		     * shouldn't be null, but we are careful.
		     */
		    if (norm == NULL) {
			/* Do nothing */
		    } else if (Tcl_FSEqualPaths(tsdPtr->cwdPathPtr, norm)) {
			/* 
			 * If the paths were equal, we can be more
			 * efficient and retain the old path object
			 * which will probably already be shared.  In
			 * this case we can simply free the normalized
			 * path we just calculated.
			 */
			Tcl_DecrRefCount(norm);
		    } else {
			FsUpdateCwd(norm);
		    }
		    Tcl_DecrRefCount(retVal);
		} else {
		    /* The 'cwd' function returned an error; reset the cwd */
		    FsUpdateCwd(NULL);
		}
	    }
	}
    }
    
    if (tsdPtr->cwdPathPtr != NULL) {
	Tcl_IncrRefCount(tsdPtr->cwdPathPtr);
    }
    
    return tsdPtr->cwdPathPtr; 
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSChdir --
 *
 *	This function replaces the library version of chdir().
 *	
 *	The path is normalized and then passed to the filesystem
 *	which claims it.
 *
 * Results:
 *	See chdir() documentation.  If successful, we keep a 
 *	record of the successful path in cwdPathPtr for subsequent 
 *	calls to getcwd.
 *
 * Side effects:
 *	See chdir() documentation.  The global cwdPathPtr may 
 *	change value.
 *
 *----------------------------------------------------------------------
 */
int
Tcl_FSChdir(pathPtr)
    Tcl_Obj *pathPtr;
{
    Tcl_Filesystem *fsPtr;
    int retVal = -1;
    
    if (Tcl_FSGetNormalizedPath(NULL, pathPtr) == NULL) {
        return TCL_ERROR;
    }
    
    fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSChdirProc *proc = fsPtr->chdirProc;
	if (proc != NULL) {
	    retVal = (*proc)(pathPtr);
	} else {
	    /* Fallback on stat-based implementation */
	    Tcl_StatBuf buf;
	    /* If the file can be stat'ed and is a directory and
	     * is readable, then we can chdir. */
	    if ((Tcl_FSStat(pathPtr, &buf) == 0) 
	      && (S_ISDIR(buf.st_mode))
	      && (Tcl_FSAccess(pathPtr, R_OK) == 0)) {
		/* We allow the chdir */
		retVal = 0;
	    }
	}
    }

    if (retVal != -1) {
	/* 
	 * The cwd changed, or an error was thrown.  If an error was
	 * thrown, we can just continue (and that will report the error
	 * to the user).  If there was no error we must assume that the
	 * cwd was actually changed to the normalized value we
	 * calculated above, and we must therefore cache that
	 * information.
	 */
	if (retVal == TCL_OK) {
	    /* 
	     * Note that this normalized path may be different to what
	     * we found above (or at least a different object), if the
	     * filesystem epoch changed recently.  This can actually
	     * happen with scripted documents very easily.  Therefore
	     * we ask for the normalized path again (the correct value
	     * will have been cached as a result of the
	     * Tcl_FSGetFileSystemForPath call above anyway).
	     */
	    Tcl_Obj *normDirName = Tcl_FSGetNormalizedPath(NULL, pathPtr);
	    if (normDirName == NULL) {
	        return TCL_ERROR;
	    }
	    FsUpdateCwd(normDirName);
	}
    } else {
	Tcl_SetErrno(ENOENT);
    }
    
    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FSLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they are
 *	defined.  The appropriate function for the filesystem to which
 *	pathPtr belongs will be called.
 *	
 *	Note that the native filesystem doesn't actually assume
 *	'pathPtr' is a path.  Rather it assumes filename is either
 *	a path or just the name of a file which can be found somewhere
 *	in the environment's loadable path.  This behaviour is not
 *	very compatible with virtual filesystems (and has other problems
 *	documented in the load man-page), so it is advised that full
 *	paths are always used.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	New code suddenly appears in memory.  This may later be
 *	unloaded by passing the clientData to the unloadProc.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FSLoadFile(interp, pathPtr, sym1, sym2, proc1Ptr, proc2Ptr, 
	       handlePtr, unloadProcPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *pathPtr;		/* Name of the file containing the desired
				 * code. */
    CONST char *sym1, *sym2;	/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
    Tcl_LoadHandle *handlePtr;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr;	
                                /* Filled with address of Tcl_FSUnloadFileProc
                                 * function which should be used for
                                 * this file. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSLoadFileProc *proc = fsPtr->loadFileProc;
	if (proc != NULL) {
	    int retVal = (*proc)(interp, pathPtr, handlePtr, unloadProcPtr);
	    if (retVal != TCL_OK) {
		return retVal;
	    }
	    if (*handlePtr == NULL) {
		return TCL_ERROR;
	    }
	    if (sym1 != NULL) {
	        *proc1Ptr = TclpFindSymbol(interp, *handlePtr, sym1);
	    }
	    if (sym2 != NULL) {
	        *proc2Ptr = TclpFindSymbol(interp, *handlePtr, sym2);
	    }
	    return retVal;
	} else {
	    Tcl_Filesystem *copyFsPtr;
	    Tcl_Obj *copyToPtr;
	    
	    /* First check if it is readable -- and exists! */
	    if (Tcl_FSAccess(pathPtr, R_OK) != 0) {
		Tcl_AppendResult(interp, "couldn't load library \"",
				 Tcl_GetString(pathPtr), "\": ", 
				 Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    }
	    
	    /* 
	     * Get a temporary filename to use, first to
	     * copy the file into, and then to load. 
	     */
	    copyToPtr = TclpTempFileName();
	    if (copyToPtr == NULL) {
	        return -1;
	    }
	    Tcl_IncrRefCount(copyToPtr);
	    
	    copyFsPtr = Tcl_FSGetFileSystemForPath(copyToPtr);
	    if ((copyFsPtr == NULL) || (copyFsPtr == fsPtr)) {
		/* 
		 * We already know we can't use Tcl_FSLoadFile from 
		 * this filesystem, and we must avoid a possible
		 * infinite loop.  Try to delete the file we
		 * probably created, and then exit.
		 */
		Tcl_FSDeleteFile(copyToPtr);
		Tcl_DecrRefCount(copyToPtr);
		return -1;
	    }
	    
	    if (TclCrossFilesystemCopy(interp, pathPtr, 
				       copyToPtr) == TCL_OK) {
		Tcl_LoadHandle newLoadHandle = NULL;
		Tcl_FSUnloadFileProc *newUnloadProcPtr = NULL;
		FsDivertLoad *tvdlPtr;
		int retVal;

#if !defined(__WIN32__) && !defined(MAC_TCL)
		/* 
		 * Do we need to set appropriate permissions 
		 * on the file?  This may be required on some
		 * systems.  On Unix we could loop over
		 * the file attributes, and set any that are
		 * called "-permissions" to 0700.  However,
		 * we just do this directly, like this:
		 */
		
		Tcl_Obj* perm = Tcl_NewStringObj("0700",-1);
		Tcl_IncrRefCount(perm);
		Tcl_FSFileAttrsSet(NULL, 2, copyToPtr, perm);
		Tcl_DecrRefCount(perm);
#endif
		
		/* 
		 * We need to reset the result now, because the cross-
		 * filesystem copy may have stored the number of bytes
		 * in the result
		 */
		Tcl_ResetResult(interp);
		
		retVal = Tcl_FSLoadFile(interp, copyToPtr, sym1, sym2,
					proc1Ptr, proc2Ptr, 
					&newLoadHandle,
					&newUnloadProcPtr);
	        if (retVal != TCL_OK) {
		    /* The file didn't load successfully */
		    Tcl_FSDeleteFile(copyToPtr);
		    Tcl_DecrRefCount(copyToPtr);
		    return retVal;
		}
		/* 
		 * Try to delete the file immediately -- this is
		 * possible in some OSes, and avoids any worries
		 * about leaving the copy laying around on exit. 
		 */
		if (Tcl_FSDeleteFile(copyToPtr) == TCL_OK) {
		    Tcl_DecrRefCount(copyToPtr);
		    /* 
		     * We tell our caller about the real shared
		     * library which was loaded.  Note that this
		     * does mean that the package list maintained
		     * by 'load' will store the original (vfs)
		     * path alongside the temporary load handle
		     * and unload proc ptr.
		     */
		    (*handlePtr) = newLoadHandle;
		    (*unloadProcPtr) = newUnloadProcPtr;
		    return TCL_OK;
		}
		/* 
		 * When we unload this file, we need to divert the 
		 * unloading so we can unload and cleanup the 
		 * temporary file correctly.
		 */
		tvdlPtr = (FsDivertLoad*) ckalloc(sizeof(FsDivertLoad));

		/* 
		 * Remember three pieces of information.  This allows
		 * us to cleanup the diverted load completely, on
		 * platforms which allow proper unloading of code.
		 */
		tvdlPtr->loadHandle = newLoadHandle;
		tvdlPtr->unloadProcPtr = newUnloadProcPtr;

		if (copyFsPtr != &tclNativeFilesystem) {
		    /* copyToPtr is already incremented for this reference */
		    tvdlPtr->divertedFile = copyToPtr;

		    /* 
		     * This is the filesystem we loaded it into.  Since
		     * we have a reference to 'copyToPtr', we already
		     * have a refCount on this filesystem, so we don't
		     * need to worry about it disappearing on us.
		     */
		    tvdlPtr->divertedFilesystem = copyFsPtr;
		    tvdlPtr->divertedFileNativeRep = NULL;
		} else {
		    /* We need the native rep */
		    tvdlPtr->divertedFileNativeRep = 
		      NativeDupInternalRep(Tcl_FSGetInternalRep(copyToPtr, 
								copyFsPtr));
		    /* 
		     * We don't need or want references to the copied
		     * Tcl_Obj or the filesystem if it is the native
		     * one.
		     */
		    tvdlPtr->divertedFile = NULL;
		    tvdlPtr->divertedFilesystem = NULL;
		    Tcl_DecrRefCount(copyToPtr);
		}

		copyToPtr = NULL;
		(*handlePtr) = (Tcl_LoadHandle) tvdlPtr;
		(*unloadProcPtr) = &FSUnloadTempFile;
		return retVal;
	    } else {
		/* Cross-platform copy failed */
		Tcl_FSDeleteFile(copyToPtr);
		Tcl_DecrRefCount(copyToPtr);
		return TCL_ERROR;
	    }
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}
/* 
 * This function used to be in the platform specific directories, but it
 * has now been made to work cross-platform
 */
int
TclpLoadFile(interp, pathPtr, sym1, sym2, proc1Ptr, proc2Ptr, 
	     clientDataPtr, unloadProcPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *pathPtr;		/* Name of the file containing the desired
				 * code (UTF-8). */
    CONST char *sym1, *sym2;	/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
    ClientData *clientDataPtr;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr;	
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for
				 * this file. */
{
    Tcl_LoadHandle handle = NULL;
    int res;
    
    res = TclpDlopen(interp, pathPtr, &handle, unloadProcPtr);
    
    if (res != TCL_OK) {
        return res;
    }

    if (handle == NULL) {
	return TCL_ERROR;
    }
    
    *clientDataPtr = (ClientData)handle;
    
    *proc1Ptr = TclpFindSymbol(interp, handle, sym1);
    *proc2Ptr = TclpFindSymbol(interp, handle, sym2);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FSUnloadTempFile --
 *
 *	This function is called when we loaded a library of code via
 *	an intermediate temporary file.  This function ensures
 *	the library is correctly unloaded and the temporary file
 *	is correctly deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The effects of the 'unload' function called, and of course
 *	the temporary file will be deleted.
 *
 *---------------------------------------------------------------------------
 */
static void 
FSUnloadTempFile(loadHandle)
    Tcl_LoadHandle loadHandle; /* loadHandle returned by a previous call
			       * to Tcl_FSLoadFile().  The loadHandle is 
			       * a token that represents the loaded 
			       * file. */
{
    FsDivertLoad *tvdlPtr = (FsDivertLoad*)loadHandle;
    /* 
     * This test should never trigger, since we give
     * the client data in the function above.
     */
    if (tvdlPtr == NULL) { return; }
    
    /* 
     * Call the real 'unloadfile' proc we actually used. It is very
     * important that we call this first, so that the shared library
     * is actually unloaded by the OS.  Otherwise, the following
     * 'delete' may well fail because the shared library is still in
     * use.
     */
    if (tvdlPtr->unloadProcPtr != NULL) {
	(*tvdlPtr->unloadProcPtr)(tvdlPtr->loadHandle);
    }
    
    if (tvdlPtr->divertedFilesystem == NULL) {
	/* 
	 * It was the native filesystem, and we have a special
	 * function available just for this purpose, which we 
	 * know works even at this late stage.
	 */
	TclpDeleteFile(tvdlPtr->divertedFileNativeRep);
	NativeFreeInternalRep(tvdlPtr->divertedFileNativeRep);
    } else {
	/* 
	 * Remove the temporary file we created.  Note, we may crash
	 * here because encodings have been taken down already.
	 */
	if (tvdlPtr->divertedFilesystem->deleteFileProc(tvdlPtr->divertedFile)
	    != TCL_OK) {
	    /* 
	     * The above may have failed because the filesystem, or something
	     * it depends upon (e.g. encodings) have been taken down because
	     * Tcl is exiting.
	     * 
	     * We may need to work out how to delete this file more
	     * robustly (or give the filesystem the information it needs
	     * to delete the file more robustly).
	     * 
	     * In particular, one problem might be that the filesystem
	     * cannot extract the information it needs from the above
	     * path object because Tcl's entire filesystem apparatus
	     * (the code in this file) has been finalized, and it
	     * refuses to pass the internal representation to the
	     * filesystem.
	     */
	}
	
	/* 
	 * And free up the allocations.  This will also of course remove
	 * a refCount from the Tcl_Filesystem to which this file belongs,
	 * which could then free up the filesystem if we are exiting.
	 */
	Tcl_DecrRefCount(tvdlPtr->divertedFile);
    }

    ckfree((char*)tvdlPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSLink --
 *
 *	This function replaces the library version of readlink() and
 *	can also be used to make links.  The appropriate function for
 *	the filesystem to which pathPtr belongs will be called.
 *
 * Results:
 *      If toPtr is NULL, then the result is a Tcl_Obj specifying the 
 *      contents of the symbolic link given by 'pathPtr', or NULL if
 *      the symbolic link could not be read.  The result is owned by
 *      the caller, which should call Tcl_DecrRefCount when the result
 *      is no longer needed.
 *      
 *      If toPtr is non-NULL, then the result is toPtr if the link action
 *      was successful, or NULL if not.  In this case the result has no
 *      additional reference count, and need not be freed.  The actual
 *      action to perform is given by the 'linkAction' flags, which is
 *      an or'd combination of:
 *      
 *        TCL_CREATE_SYMBOLIC_LINK
 *        TCL_CREATE_HARD_LINK
 *      
 *      Note that most filesystems will not support linking across
 *      to different filesystems, so this function will usually
 *      fail unless toPtr is in the same FS as pathPtr.
 *      
 * Side effects:
 *	See readlink() documentation.  A new filesystem link 
 *	object may appear
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_FSLink(pathPtr, toPtr, linkAction)
    Tcl_Obj *pathPtr;		/* Path of file to readlink or link */
    Tcl_Obj *toPtr;		/* NULL or path to be linked to */
    int linkAction;             /* Action to perform */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSLinkProc *proc = fsPtr->linkProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr, toPtr, linkAction);
	}
    }
    /*
     * If S_IFLNK isn't defined it means that the machine doesn't
     * support symbolic links, so the file can't possibly be a
     * symbolic link.  Generate an EINVAL error, which is what
     * happens on machines that do support symbolic links when
     * you invoke readlink on a file that isn't a symbolic link.
     */
#ifndef S_IFLNK
    errno = EINVAL;
#else
    Tcl_SetErrno(ENOENT);
#endif /* S_IFLNK */
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSListVolumes --
 *
 *	Lists the currently mounted volumes.  The chain of functions
 *	that have been "inserted" into the filesystem will be called in
 *	succession; each may return a list of volumes, all of which are
 *	added to the result until all mounted file systems are listed.
 *	
 *	Notice that we assume the lists returned by each filesystem
 *	(if non NULL) have been given a refCount for us already.
 *	However, we are NOT allowed to hang on to the list itself
 *	(it belongs to the filesystem we called).  Therefore we
 *	quite naturally add its contents to the result we are
 *	building, and then decrement the refCount.
 *
 * Results:
 *	The list of volumes, in an object which has refCount 0.
 *
 * Side effects:
 *	None
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj*
Tcl_FSListVolumes(void)
{
    FilesystemRecord *fsRecPtr;
    Tcl_Obj *resultPtr = Tcl_NewObj();
    
    /*
     * Call each of the "listVolumes" function in succession.
     * A non-NULL return value indicates the particular function has
     * succeeded.  We call all the functions registered, since we want
     * a list of all drives from all filesystems.
     */

    fsRecPtr = FsGetIterator();
    while (fsRecPtr != NULL) {
	Tcl_FSListVolumesProc *proc = fsRecPtr->fsPtr->listVolumesProc;
	if (proc != NULL) {
	    Tcl_Obj *thisFsVolumes = (*proc)();
	    if (thisFsVolumes != NULL) {
		Tcl_ListObjAppendList(NULL, resultPtr, thisFsVolumes);
		Tcl_DecrRefCount(thisFsVolumes);
	    }
	}
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();
    
    return resultPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSSplitPath --
 *
 *      This function takes the given Tcl_Obj, which should be a valid
 *      path, and returns a Tcl List object containing each segment of
 *      that path as an element.
 *
 * Results:
 *      Returns list object with refCount of zero.  If the passed in
 *      lenPtr is non-NULL, we use it to return the number of elements
 *      in the returned list.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Obj* 
Tcl_FSSplitPath(pathPtr, lenPtr)
    Tcl_Obj *pathPtr;		/* Path to split. */
    int *lenPtr;		/* int to store number of path elements. */
{
    Tcl_Obj *result = NULL;  /* Needed only to prevent gcc warnings. */
    Tcl_Filesystem *fsPtr;
    char separator = '/';
    int driveNameLength;
    char *p;
    
    /*
     * Perform platform specific splitting. 
     */

    if (FSGetPathType(pathPtr, &fsPtr, &driveNameLength) 
	== TCL_PATH_ABSOLUTE) {
	if (fsPtr == &tclNativeFilesystem) {
	    return TclpNativeSplitPath(pathPtr, lenPtr);
	}
    } else {
	return TclpNativeSplitPath(pathPtr, lenPtr);
    }

    /* We assume separators are single characters */
    if (fsPtr->filesystemSeparatorProc != NULL) {
	Tcl_Obj *sep = (*fsPtr->filesystemSeparatorProc)(pathPtr);
	if (sep != NULL) {
	    separator = Tcl_GetString(sep)[0];
	}
    }
    
    /* 
     * Place the drive name as first element of the
     * result list.  The drive name may contain strange
     * characters, like colons and multiple forward slashes
     * (for example 'ftp://' is a valid vfs drive name)
     */
    result = Tcl_NewObj();
    p = Tcl_GetString(pathPtr);
    Tcl_ListObjAppendElement(NULL, result, 
			     Tcl_NewStringObj(p, driveNameLength));
    p+= driveNameLength;
    			
    /* Add the remaining path elements to the list */
    for (;;) {
	char *elementStart = p;
	int length;
	while ((*p != '\0') && (*p != separator)) {
	    p++;
	}
	length = p - elementStart;
	if (length > 0) {
	    Tcl_Obj *nextElt;
	    if (elementStart[0] == '~') {
		nextElt = Tcl_NewStringObj("./",2);
		Tcl_AppendToObj(nextElt, elementStart, length);
	    } else {
		nextElt = Tcl_NewStringObj(elementStart, length);
	    }
	    Tcl_ListObjAppendElement(NULL, result, nextElt);
	}
	if (*p++ == '\0') {
	    break;
	}
    }
			     
    /*
     * Compute the number of elements in the result.
     */

    if (lenPtr != NULL) {
	Tcl_ListObjLength(NULL, result, lenPtr);
    }
    return result;
}

/* Simple helper function */
Tcl_Obj* 
TclFSInternalToNormalized(fromFilesystem, clientData, fsRecPtrPtr, epochPtr)
    Tcl_Filesystem *fromFilesystem;
    ClientData clientData;
    FilesystemRecord **fsRecPtrPtr;
    int *epochPtr;
{
    FilesystemRecord *fsRecPtr = FsGetIterator();
    while (fsRecPtr != NULL) {
	if (fsRecPtr->fsPtr == fromFilesystem) {
	    *epochPtr = theFilesystemEpoch;
	    *fsRecPtrPtr = fsRecPtr;
	    break;
	}
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();
    
    if ((fsRecPtr != NULL) 
      && (fromFilesystem->internalToNormalizedProc != NULL)) {
	return (*fromFilesystem->internalToNormalizedProc)(clientData);
    } else {
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetPathType --
 *
 *	Helper function used by FSGetPathType.
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
GetPathType(pathObjPtr, filesystemPtrPtr, driveNameLengthPtr, driveNameRef)
    Tcl_Obj *pathObjPtr;
    Tcl_Filesystem **filesystemPtrPtr;
    int *driveNameLengthPtr;
    Tcl_Obj **driveNameRef;
{
    FilesystemRecord *fsRecPtr;
    int pathLen;
    char *path;
    Tcl_PathType type = TCL_PATH_RELATIVE;
    
    path = Tcl_GetStringFromObj(pathObjPtr, &pathLen);

    /*
     * Call each of the "listVolumes" function in succession, checking
     * whether the given path is an absolute path on any of the volumes
     * returned (this is done by checking whether the path's prefix
     * matches).
     */

    fsRecPtr = FsGetIterator();
    while (fsRecPtr != NULL) {
	Tcl_FSListVolumesProc *proc = fsRecPtr->fsPtr->listVolumesProc;
	/* 
	 * We want to skip the native filesystem in this loop because
	 * otherwise we won't necessarily pass all the Tcl testsuite --
	 * this is because some of the tests artificially change the
	 * current platform (between mac, win, unix) but the list
	 * of volumes we get by calling (*proc) will reflect the current
	 * (real) platform only and this may cause some tests to fail.
	 * In particular, on unix '/' will match the beginning of 
	 * certain absolute Windows paths starting '//' and those tests
	 * will go wrong.
	 * 
	 * Besides these test-suite issues, there is one other reason
	 * to skip the native filesystem --- since the tclFilename.c
	 * code has nice fast 'absolute path' checkers, we don't want
	 * to waste time repeating that effort here, and this 
	 * function is actually called quite often, so if we can
	 * save the overhead of the native filesystem returning us
	 * a list of volumes all the time, it is better.
	 */
	if ((fsRecPtr->fsPtr != &tclNativeFilesystem) && (proc != NULL)) {
	    int numVolumes;
	    Tcl_Obj *thisFsVolumes = (*proc)();
	    if (thisFsVolumes != NULL) {
		if (Tcl_ListObjLength(NULL, thisFsVolumes, 
				      &numVolumes) != TCL_OK) {
		    /* 
		     * This is VERY bad; the Tcl_FSListVolumesProc
		     * didn't return a valid list.  Set numVolumes to
		     * -1 so that we skip the while loop below and just
		     * return with the current value of 'type'.
		     * 
		     * It would be better if we could signal an error
		     * here (but panic seems a bit excessive).
		     */
		    numVolumes = -1;
		}
		while (numVolumes > 0) {
		    Tcl_Obj *vol;
		    int len;
		    char *strVol;

		    numVolumes--;
		    Tcl_ListObjIndex(NULL, thisFsVolumes, numVolumes, &vol);
		    strVol = Tcl_GetStringFromObj(vol,&len);
		    if (pathLen < len) {
			continue;
		    }
		    if (strncmp(strVol, path, (size_t) len) == 0) {
			type = TCL_PATH_ABSOLUTE;
			if (filesystemPtrPtr != NULL) {
			    *filesystemPtrPtr = fsRecPtr->fsPtr;
			}
			if (driveNameLengthPtr != NULL) {
			    *driveNameLengthPtr = len;
			}
			if (driveNameRef != NULL) {
			    *driveNameRef = vol;
			    Tcl_IncrRefCount(vol);
			}
			break;
		    }
		}
		Tcl_DecrRefCount(thisFsVolumes);
		if (type == TCL_PATH_ABSOLUTE) {
		    /* We don't need to examine any more filesystems */
		    break;
		}
	    }
	}
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();
    
    if (type != TCL_PATH_ABSOLUTE) {
	type = TclpGetNativePathType(pathObjPtr, driveNameLengthPtr, 
				     driveNameRef);
	if ((type == TCL_PATH_ABSOLUTE) && (filesystemPtrPtr != NULL)) {
	    *filesystemPtrPtr = &tclNativeFilesystem;
	}
    }
    return type;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSRenameFile --
 *
 *	If the two paths given belong to the same filesystem, we call
 *	that filesystems rename function.  Otherwise we simply
 *	return the posix error 'EXDEV', and -1.
 *
 * Results:
 *      Standard Tcl error code if a function was called.
 *
 * Side effects:
 *	A file may be renamed.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSRenameFile(srcPathPtr, destPathPtr)
    Tcl_Obj* srcPathPtr;	/* Pathname of file or dir to be renamed
				 * (UTF-8). */
    Tcl_Obj *destPathPtr;	/* New pathname of file or directory
				 * (UTF-8). */
{
    int retVal = -1;
    Tcl_Filesystem *fsPtr, *fsPtr2;
    fsPtr = Tcl_FSGetFileSystemForPath(srcPathPtr);
    fsPtr2 = Tcl_FSGetFileSystemForPath(destPathPtr);

    if (fsPtr == fsPtr2 && fsPtr != NULL) {
	Tcl_FSRenameFileProc *proc = fsPtr->renameFileProc;
	if (proc != NULL) {
	    retVal =  (*proc)(srcPathPtr, destPathPtr);
	}
    }
    if (retVal == -1) {
	Tcl_SetErrno(EXDEV);
    }
    return retVal;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSCopyFile --
 *
 *	If the two paths given belong to the same filesystem, we call
 *	that filesystem's copy function.  Otherwise we simply
 *	return the posix error 'EXDEV', and -1.
 *	
 *	Note that in the native filesystems, 'copyFileProc' is defined
 *	to copy soft links (i.e. it copies the links themselves, not
 *	the things they point to).
 *
 * Results:
 *      Standard Tcl error code if a function was called.
 *
 * Side effects:
 *	A file may be copied.
 *
 *---------------------------------------------------------------------------
 */

int 
Tcl_FSCopyFile(srcPathPtr, destPathPtr)
    Tcl_Obj* srcPathPtr;	/* Pathname of file to be copied (UTF-8). */
    Tcl_Obj *destPathPtr;	/* Pathname of file to copy to (UTF-8). */
{
    int retVal = -1;
    Tcl_Filesystem *fsPtr, *fsPtr2;
    fsPtr = Tcl_FSGetFileSystemForPath(srcPathPtr);
    fsPtr2 = Tcl_FSGetFileSystemForPath(destPathPtr);

    if (fsPtr == fsPtr2 && fsPtr != NULL) {
	Tcl_FSCopyFileProc *proc = fsPtr->copyFileProc;
	if (proc != NULL) {
	    retVal = (*proc)(srcPathPtr, destPathPtr);
	}
    }
    if (retVal == -1) {
	Tcl_SetErrno(EXDEV);
    }
    return retVal;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclCrossFilesystemCopy --
 *
 *	Helper for above function, and for Tcl_FSLoadFile, to copy
 *	files from one filesystem to another.  This function will
 *	overwrite the target file if it already exists.
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	A file may be created.
 *
 *---------------------------------------------------------------------------
 */
int 
TclCrossFilesystemCopy(interp, source, target) 
    Tcl_Interp *interp; /* For error messages */
    Tcl_Obj *source;	/* Pathname of file to be copied (UTF-8). */
    Tcl_Obj *target;	/* Pathname of file to copy to (UTF-8). */
{
    int result = TCL_ERROR;
    int prot = 0666;
    
    Tcl_Channel out = Tcl_FSOpenFileChannel(interp, target, "w", prot);
    if (out != NULL) {
	/* It looks like we can copy it over */
	Tcl_Channel in = Tcl_FSOpenFileChannel(interp, source, 
					       "r", prot);
	if (in == NULL) {
	    /* This is very strange, we checked this above */
	    Tcl_Close(interp, out);
	} else {
	    Tcl_StatBuf sourceStatBuf;
	    struct utimbuf tval;
	    /* 
	     * Copy it synchronously.  We might wish to add an
	     * asynchronous option to support vfs's which are
	     * slow (e.g. network sockets).
	     */
	    Tcl_SetChannelOption(interp, in, "-translation", "binary");
	    Tcl_SetChannelOption(interp, out, "-translation", "binary");
	    
	    if (TclCopyChannel(interp, in, out, -1, NULL) == TCL_OK) {
		result = TCL_OK;
	    }
	    /* 
	     * If the copy failed, assume that copy channel left
	     * a good error message.
	     */
	    Tcl_Close(interp, in);
	    Tcl_Close(interp, out);
	    
	    /* Set modification date of copied file */
	    if (Tcl_FSLstat(source, &sourceStatBuf) == 0) {
		tval.actime = sourceStatBuf.st_atime;
		tval.modtime = sourceStatBuf.st_mtime;
		Tcl_FSUtime(target, &tval);
	    }
	}
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSDeleteFile --
 *
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	A file may be deleted.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSDeleteFile(pathPtr)
    Tcl_Obj *pathPtr;		/* Pathname of file to be removed (UTF-8). */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSDeleteFileProc *proc = fsPtr->deleteFileProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSCreateDirectory --
 *
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	A directory may be created.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSCreateDirectory(pathPtr)
    Tcl_Obj *pathPtr;		/* Pathname of directory to create (UTF-8). */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSCreateDirectoryProc *proc = fsPtr->createDirectoryProc;
	if (proc != NULL) {
	    return (*proc)(pathPtr);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSCopyDirectory --
 *
 *	If the two paths given belong to the same filesystem, we call
 *	that filesystems copy-directory function.  Otherwise we simply
 *	return the posix error 'EXDEV', and -1.
 *
 * Results:
 *      Standard Tcl error code if a function was called.
 *
 * Side effects:
 *	A directory may be copied.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSCopyDirectory(srcPathPtr, destPathPtr, errorPtr)
    Tcl_Obj* srcPathPtr;	/* Pathname of directory to be copied
				 * (UTF-8). */
    Tcl_Obj *destPathPtr;	/* Pathname of target directory (UTF-8). */
    Tcl_Obj **errorPtr;	        /* If non-NULL, then will be set to a
                       	         * new object containing name of file
                       	         * causing error, with refCount 1. */
{
    int retVal = -1;
    Tcl_Filesystem *fsPtr, *fsPtr2;
    fsPtr = Tcl_FSGetFileSystemForPath(srcPathPtr);
    fsPtr2 = Tcl_FSGetFileSystemForPath(destPathPtr);

    if (fsPtr == fsPtr2 && fsPtr != NULL) {
	Tcl_FSCopyDirectoryProc *proc = fsPtr->copyDirectoryProc;
	if (proc != NULL) {
	    retVal = (*proc)(srcPathPtr, destPathPtr, errorPtr);
	}
    }
    if (retVal == -1) {
	Tcl_SetErrno(EXDEV);
    }
    return retVal;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSRemoveDirectory --
 *
 *	The appropriate function for the filesystem to which pathPtr
 *	belongs will be called.
 *
 * Results:
 *      Standard Tcl error code.
 *
 * Side effects:
 *	A directory may be deleted.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_FSRemoveDirectory(pathPtr, recursive, errorPtr)
    Tcl_Obj *pathPtr;		/* Pathname of directory to be removed
				 * (UTF-8). */
    int recursive;		/* If non-zero, removes directories that
				 * are nonempty.  Otherwise, will only remove
				 * empty directories. */
    Tcl_Obj **errorPtr;	        /* If non-NULL, then will be set to a
				 * new object containing name of file
				 * causing error, with refCount 1. */
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathPtr);
    if (fsPtr != NULL) {
	Tcl_FSRemoveDirectoryProc *proc = fsPtr->removeDirectoryProc;
	if (proc != NULL) {
	    if (recursive) {
	        /* 
	         * We check whether the cwd lies inside this directory
	         * and move it if it does.
	         */
		Tcl_Obj *cwdPtr = Tcl_FSGetCwd(NULL);
		if (cwdPtr != NULL) {
		    char *cwdStr, *normPathStr;
		    int cwdLen, normLen;
		    Tcl_Obj *normPath = Tcl_FSGetNormalizedPath(NULL, pathPtr);
		    if (normPath != NULL) {
		        normPathStr = Tcl_GetStringFromObj(normPath, &normLen);
			cwdStr = Tcl_GetStringFromObj(cwdPtr, &cwdLen);
			if ((cwdLen >= normLen) && (strncmp(normPathStr, 
					cwdStr, (size_t) normLen) == 0)) {
			    /* 
			     * the cwd is inside the directory, so we
			     * perform a 'cd [file dirname $path]'
			     */
			    Tcl_Obj *dirPtr = TclFileDirname(NULL, pathPtr);
			    Tcl_FSChdir(dirPtr);
			    Tcl_DecrRefCount(dirPtr);
			}
		    }
		    Tcl_DecrRefCount(cwdPtr);
		}
	    }
	    return (*proc)(pathPtr, recursive, errorPtr);
	}
    }
    Tcl_SetErrno(ENOENT);
    return -1;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetFileSystemForPath --
 *
 *      This function determines which filesystem to use for a
 *      particular path object, and returns the filesystem which
 *      accepts this file.  If no filesystem will accept this object
 *      as a valid file path, then NULL is returned.
 *
 * Results:
.*      NULL or a filesystem which will accept this path.
 *
 * Side effects:
 *	The object may be converted to a path type.
 *
 *---------------------------------------------------------------------------
 */

Tcl_Filesystem*
Tcl_FSGetFileSystemForPath(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
    FilesystemRecord *fsRecPtr;
    Tcl_Filesystem* retVal = NULL;
    
    /* 
     * If the object has a refCount of zero, we reject it.  This
     * is to avoid possible segfaults or nondeterministic memory
     * leaks (i.e. the user doesn't know if they should decrement
     * the ref count on return or not).
     */
    
    if (pathObjPtr->refCount == 0) {
	panic("Tcl_FSGetFileSystemForPath called with object with refCount == 0");
	return NULL;
    }
    
    /* 
     * Check if the filesystem has changed in some way since
     * this object's internal representation was calculated.
     */
    if (TclFSEnsureEpochOk(pathObjPtr, theFilesystemEpoch, 
			   &retVal) != TCL_OK) {
	return NULL;
    }

    /*
     * Call each of the "pathInFilesystem" functions in succession.  A
     * non-return value of -1 indicates the particular function has
     * succeeded.
     */

    fsRecPtr = FsGetIterator();
    while ((retVal == NULL) && (fsRecPtr != NULL)) {
	Tcl_FSPathInFilesystemProc *proc = fsRecPtr->fsPtr->pathInFilesystemProc;
	if (proc != NULL) {
	    ClientData clientData = NULL;
	    int ret = (*proc)(pathObjPtr, &clientData);
	    if (ret != -1) {
		/* 
		 * We assume the type of pathObjPtr hasn't been changed 
		 * by the above call to the pathInFilesystemProc.
		 */
		TclFSSetPathDetails(pathObjPtr, fsRecPtr, clientData, 
			       theFilesystemEpoch);
		retVal = fsRecPtr->fsPtr;
	    }
	}
	fsRecPtr = fsRecPtr->nextPtr;
    }
    FsReleaseIterator();

    return retVal;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSGetNativePath --
 *
 *      This function is for use by the Win/Unix/MacOS native filesystems,
 *      so that they can easily retrieve the native (char* or TCHAR*)
 *      representation of a path.  Other filesystems will probably
 *      want to implement similar functions.  They basically act as a 
 *      safety net around Tcl_FSGetInternalRep.  Normally your file-
 *      system procedures will always be called with path objects
 *      already converted to the correct filesystem, but if for 
 *      some reason they are called directly (i.e. by procedures 
 *      not in this file), then one cannot necessarily guarantee that
 *      the path object pointer is from the correct filesystem.
 *      
 *      Note: in the future it might be desireable to have separate
 *      versions of this function with different signatures, for
 *      example Tcl_FSGetNativeMacPath, Tcl_FSGetNativeUnixPath etc.
 *      Right now, since native paths are all string based, we use just
 *      one function.  On MacOS we could possibly use an FSSpec or
 *      FSRef as the native representation.
 *
 * Results:
 *      NULL or a valid native path.
 *
 * Side effects:
 *	See Tcl_FSGetInternalRep.
 *
 *---------------------------------------------------------------------------
 */

CONST char *
Tcl_FSGetNativePath(pathObjPtr)
    Tcl_Obj *pathObjPtr;
{
    return (CONST char *)Tcl_FSGetInternalRep(pathObjPtr, &tclNativeFilesystem);
}

/*
 *---------------------------------------------------------------------------
 *
 * NativeCreateNativeRep --
 *
 *      Create a native representation for the given path.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static ClientData 
NativeCreateNativeRep(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
    char *nativePathPtr;
    Tcl_DString ds;
    Tcl_Obj* validPathObjPtr;
    int len;
    char *str;

    /* Make sure the normalized path is set */
    validPathObjPtr = Tcl_FSGetNormalizedPath(NULL, pathObjPtr);

    str = Tcl_GetStringFromObj(validPathObjPtr, &len);
#ifdef __WIN32__
    Tcl_WinUtfToTChar(str, len, &ds);
    if (tclWinProcs->useWide) {
	len = Tcl_DStringLength(&ds) + sizeof(WCHAR);
    } else {
	len = Tcl_DStringLength(&ds) + sizeof(char);
    }
#else
    Tcl_UtfToExternalDString(NULL, str, len, &ds);
    len = Tcl_DStringLength(&ds) + sizeof(char);
#endif
    nativePathPtr = ckalloc((unsigned) len);
    memcpy((VOID*)nativePathPtr, (VOID*)Tcl_DStringValue(&ds), (size_t) len);
	  
    Tcl_DStringFree(&ds);
    return (ClientData)nativePathPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpNativeToNormalized --
 *
 *      Convert native format to a normalized path object, with refCount
 *      of zero.
 *
 * Results:
 *      A valid normalized path.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj* 
TclpNativeToNormalized(clientData)
    ClientData clientData;
{
    Tcl_DString ds;
    Tcl_Obj *objPtr;
    CONST char *copy;
    int len;
    
#ifdef __WIN32__
    Tcl_WinTCharToUtf((CONST char*)clientData, -1, &ds);
#else
    Tcl_ExternalToUtfDString(NULL, (CONST char*)clientData, -1, &ds);
#endif
    
    copy = Tcl_DStringValue(&ds);
    len = Tcl_DStringLength(&ds);

#ifdef __WIN32__
    /* 
     * Certain native path representations on Windows have this special
     * prefix to indicate that they are to be treated specially.  For
     * example extremely long paths, or symlinks 
     */
    if (*copy == '\\') {
        if (0 == strncmp(copy,"\\??\\",4)) {
	    copy += 4;
	    len -= 4;
	} else if (0 == strncmp(copy,"\\\\?\\",4)) {
	    copy += 4;
	    len -= 4;
	}
    }
#endif

    objPtr = Tcl_NewStringObj(copy,len);
    Tcl_DStringFree(&ds);
    
    return objPtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * NativeDupInternalRep --
 *
 *      Duplicate the native representation.
 *
 * Results:
 *      The copied native representation, or NULL if it is not possible
 *      to copy the representation.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
ClientData 
NativeDupInternalRep(clientData)
    ClientData clientData;
{
    ClientData copy;
    size_t len;

    if (clientData == NULL) {
	return NULL;
    }

#ifdef __WIN32__
    if (tclWinProcs->useWide) {
	/* unicode representation when running on NT/2K/XP */
	len = sizeof(WCHAR) + (wcslen((CONST WCHAR*)clientData) * sizeof(WCHAR));
    } else {
	/* ansi representation when running on 95/98/ME */
	len = sizeof(char) + (strlen((CONST char*)clientData) * sizeof(char));
    }
#else
    /* ansi representation when running on Unix/MacOS */
    len = sizeof(char) + (strlen((CONST char*)clientData) * sizeof(char));
#endif
    
    copy = (ClientData) ckalloc(len);
    memcpy((VOID*)copy, (VOID*)clientData, len);
    return copy;
}

/*
 *---------------------------------------------------------------------------
 *
 * NativeFreeInternalRep --
 *
 *      Free a native internal representation, which will be non-NULL.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Memory is released.
 *
 *---------------------------------------------------------------------------
 */
static void 
NativeFreeInternalRep(clientData)
    ClientData clientData;
{
    ckfree((char*)clientData);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSFileSystemInfo --
 *
 *      This function returns a list of two elements.  The first
 *      element is the name of the filesystem (e.g. "native" or "vfs"),
 *      and the second is the particular type of the given path within
 *      that filesystem.
 *
 * Results:
 *      A list of two elements.
 *
 * Side effects:
 *	The object may be converted to a path type.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj*
Tcl_FSFileSystemInfo(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
    Tcl_Obj *resPtr;
    Tcl_FSFilesystemPathTypeProc *proc;
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathObjPtr);
    
    if (fsPtr == NULL) {
	return NULL;
    }
    
    resPtr = Tcl_NewListObj(0,NULL);
    
    Tcl_ListObjAppendElement(NULL, resPtr, 
			     Tcl_NewStringObj(fsPtr->typeName,-1));

    proc = fsPtr->filesystemPathTypeProc;
    if (proc != NULL) {
	Tcl_Obj *typePtr = (*proc)(pathObjPtr);
	if (typePtr != NULL) {
	    Tcl_ListObjAppendElement(NULL, resPtr, typePtr);
	}
    }
    
    return resPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_FSPathSeparator --
 *
 *      This function returns the separator to be used for a given
 *      path.  The object returned should have a refCount of zero
 *
 * Results:
 *      A Tcl object, with a refCount of zero.  If the caller
 *      needs to retain a reference to the object, it should
 *      call Tcl_IncrRefCount.
 *
 * Side effects:
 *	The path object may be converted to a path type.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj*
Tcl_FSPathSeparator(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
    Tcl_Filesystem *fsPtr = Tcl_FSGetFileSystemForPath(pathObjPtr);
    
    if (fsPtr == NULL) {
	return NULL;
    }
    if (fsPtr->filesystemSeparatorProc != NULL) {
	return (*fsPtr->filesystemSeparatorProc)(pathObjPtr);
    }
    
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * NativeFilesystemSeparator --
 *
 *      This function is part of the native filesystem support, and
 *      returns the separator for the given path.
 *
 * Results:
 *      String object containing the separator character.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj*
NativeFilesystemSeparator(pathObjPtr)
    Tcl_Obj* pathObjPtr;
{
    char *separator = NULL; /* lint */
    switch (tclPlatform) {
	case TCL_PLATFORM_UNIX:
	    separator = "/";
	    break;
	case TCL_PLATFORM_WINDOWS:
	    separator = "\\";
	    break;
	case TCL_PLATFORM_MAC:
	    separator = ":";
	    break;
    }
    return Tcl_NewStringObj(separator,1);
}

/* Everything from here on is contained in this obsolete ifdef */
#ifdef USE_OBSOLETE_FS_HOOKS

/*
 *----------------------------------------------------------------------
 *
 * TclStatInsertProc --
 *
 *	Insert the passed procedure pointer at the head of the list of
 *	functions which are used during a call to 'TclStat(...)'. The
 *	passed function should behave exactly like 'TclStat' when called
 *	during that time (see 'TclStat(...)' for more information).
 *	The function will be added even if it already in the list.
 *
 * Results:
 *      Normally TCL_OK; TCL_ERROR if memory for a new node in the list
 *	could not be allocated.
 *
 * Side effects:
 *      Memory allocated and modifies the link list for 'TclStat'
 *	functions.
 *
 *----------------------------------------------------------------------
 */

int
TclStatInsertProc (proc)
    TclStatProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	StatProc *newStatProcPtr;

	newStatProcPtr = (StatProc *)ckalloc(sizeof(StatProc));

	if (newStatProcPtr != NULL) {
	    newStatProcPtr->proc = proc;
	    Tcl_MutexLock(&obsoleteFsHookMutex);
	    newStatProcPtr->nextPtr = statProcList;
	    statProcList = newStatProcPtr;
	    Tcl_MutexUnlock(&obsoleteFsHookMutex);

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclStatDeleteProc --
 *
 *	Removed the passed function pointer from the list of 'TclStat'
 *	functions.  Ensures that the built-in stat function is not
 *	removvable.
 *
 * Results:
 *      TCL_OK if the procedure pointer was successfully removed,
 *	TCL_ERROR otherwise.
 *
 * Side effects:
 *      Memory is deallocated and the respective list updated.
 *
 *----------------------------------------------------------------------
 */

int
TclStatDeleteProc (proc)
    TclStatProc_ *proc;
{
    int retVal = TCL_ERROR;
    StatProc *tmpStatProcPtr;
    StatProc *prevStatProcPtr = NULL;

    Tcl_MutexLock(&obsoleteFsHookMutex);
    tmpStatProcPtr = statProcList;
    /*
     * Traverse the 'statProcList' looking for the particular node
     * whose 'proc' member matches 'proc' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == TCL_ERROR) && (tmpStatProcPtr != NULL)) {
	if (tmpStatProcPtr->proc == proc) {
	    if (prevStatProcPtr == NULL) {
		statProcList = tmpStatProcPtr->nextPtr;
	    } else {
		prevStatProcPtr->nextPtr = tmpStatProcPtr->nextPtr;
	    }

	    ckfree((char *)tmpStatProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevStatProcPtr = tmpStatProcPtr;
	    tmpStatProcPtr = tmpStatProcPtr->nextPtr;
	}
    }

    Tcl_MutexUnlock(&obsoleteFsHookMutex);
    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAccessInsertProc --
 *
 *	Insert the passed procedure pointer at the head of the list of
 *	functions which are used during a call to 'TclAccess(...)'.
 *	The passed function should behave exactly like 'TclAccess' when
 *	called during that time (see 'TclAccess(...)' for more
 *	information).  The function will be added even if it already in
 *	the list.
 *
 * Results:
 *      Normally TCL_OK; TCL_ERROR if memory for a new node in the list
 *	could not be allocated.
 *
 * Side effects:
 *      Memory allocated and modifies the link list for 'TclAccess'
 *	functions.
 *
 *----------------------------------------------------------------------
 */

int
TclAccessInsertProc(proc)
    TclAccessProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	AccessProc *newAccessProcPtr;

	newAccessProcPtr = (AccessProc *)ckalloc(sizeof(AccessProc));

	if (newAccessProcPtr != NULL) {
	    newAccessProcPtr->proc = proc;
	    Tcl_MutexLock(&obsoleteFsHookMutex);
	    newAccessProcPtr->nextPtr = accessProcList;
	    accessProcList = newAccessProcPtr;
	    Tcl_MutexUnlock(&obsoleteFsHookMutex);

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAccessDeleteProc --
 *
 *	Removed the passed function pointer from the list of 'TclAccess'
 *	functions.  Ensures that the built-in access function is not
 *	removvable.
 *
 * Results:
 *      TCL_OK if the procedure pointer was successfully removed,
 *	TCL_ERROR otherwise.
 *
 * Side effects:
 *      Memory is deallocated and the respective list updated.
 *
 *----------------------------------------------------------------------
 */

int
TclAccessDeleteProc(proc)
    TclAccessProc_ *proc;
{
    int retVal = TCL_ERROR;
    AccessProc *tmpAccessProcPtr;
    AccessProc *prevAccessProcPtr = NULL;

    /*
     * Traverse the 'accessProcList' looking for the particular node
     * whose 'proc' member matches 'proc' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    Tcl_MutexLock(&obsoleteFsHookMutex);
    tmpAccessProcPtr = accessProcList;
    while ((retVal == TCL_ERROR) && (tmpAccessProcPtr != NULL)) {
	if (tmpAccessProcPtr->proc == proc) {
	    if (prevAccessProcPtr == NULL) {
		accessProcList = tmpAccessProcPtr->nextPtr;
	    } else {
		prevAccessProcPtr->nextPtr = tmpAccessProcPtr->nextPtr;
	    }

	    ckfree((char *)tmpAccessProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevAccessProcPtr = tmpAccessProcPtr;
	    tmpAccessProcPtr = tmpAccessProcPtr->nextPtr;
	}
    }
    Tcl_MutexUnlock(&obsoleteFsHookMutex);

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFileChannelInsertProc --
 *
 *	Insert the passed procedure pointer at the head of the list of
 *	functions which are used during a call to
 *	'Tcl_OpenFileChannel(...)'. The passed function should behave
 *	exactly like 'Tcl_OpenFileChannel' when called during that time
 *	(see 'Tcl_OpenFileChannel(...)' for more information). The
 *	function will be added even if it already in the list.
 *
 * Results:
 *      Normally TCL_OK; TCL_ERROR if memory for a new node in the list
 *	could not be allocated.
 *
 * Side effects:
 *      Memory allocated and modifies the link list for
 *	'Tcl_OpenFileChannel' functions.
 *
 *----------------------------------------------------------------------
 */

int
TclOpenFileChannelInsertProc(proc)
    TclOpenFileChannelProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	OpenFileChannelProc *newOpenFileChannelProcPtr;

	newOpenFileChannelProcPtr =
		(OpenFileChannelProc *)ckalloc(sizeof(OpenFileChannelProc));

	if (newOpenFileChannelProcPtr != NULL) {
	    newOpenFileChannelProcPtr->proc = proc;
	    Tcl_MutexLock(&obsoleteFsHookMutex);
	    newOpenFileChannelProcPtr->nextPtr = openFileChannelProcList;
	    openFileChannelProcList = newOpenFileChannelProcPtr;
	    Tcl_MutexUnlock(&obsoleteFsHookMutex);

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFileChannelDeleteProc --
 *
 *	Removed the passed function pointer from the list of
 *	'Tcl_OpenFileChannel' functions.  Ensures that the built-in
 *	open file channel function is not removable.
 *
 * Results:
 *      TCL_OK if the procedure pointer was successfully removed,
 *	TCL_ERROR otherwise.
 *
 * Side effects:
 *      Memory is deallocated and the respective list updated.
 *
 *----------------------------------------------------------------------
 */

int
TclOpenFileChannelDeleteProc(proc)
    TclOpenFileChannelProc_ *proc;
{
    int retVal = TCL_ERROR;
    OpenFileChannelProc *tmpOpenFileChannelProcPtr = openFileChannelProcList;
    OpenFileChannelProc *prevOpenFileChannelProcPtr = NULL;

    /*
     * Traverse the 'openFileChannelProcList' looking for the particular
     * node whose 'proc' member matches 'proc' and remove that one from
     * the list.  
     */

    Tcl_MutexLock(&obsoleteFsHookMutex);
    tmpOpenFileChannelProcPtr = openFileChannelProcList;
    while ((retVal == TCL_ERROR) &&
	    (tmpOpenFileChannelProcPtr != NULL)) {
	if (tmpOpenFileChannelProcPtr->proc == proc) {
	    if (prevOpenFileChannelProcPtr == NULL) {
		openFileChannelProcList = tmpOpenFileChannelProcPtr->nextPtr;
	    } else {
		prevOpenFileChannelProcPtr->nextPtr =
			tmpOpenFileChannelProcPtr->nextPtr;
	    }

	    ckfree((char *)tmpOpenFileChannelProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevOpenFileChannelProcPtr = tmpOpenFileChannelProcPtr;
	    tmpOpenFileChannelProcPtr = tmpOpenFileChannelProcPtr->nextPtr;
	}
    }
    Tcl_MutexUnlock(&obsoleteFsHookMutex);

    return (retVal);
}
#endif /* USE_OBSOLETE_FS_HOOKS */
