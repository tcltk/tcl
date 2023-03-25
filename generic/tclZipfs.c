/*
 * tclZipfs.c --
 *
 *	Implementation of the ZIP filesystem used in TIP 430
 *	Adapted from the implementation for AndroWish.
 *
 * Copyright © 2016-2017 Sean Woods <yoda@etoyoc.com>
 * Copyright © 2013-2015 Christian Werner <chw@ch-werner.de>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This file is distributed in two ways:
 *   generic/tclZipfs.c file in the TIP430-enabled Tcl cores.
 *   compat/tclZipfs.c file in the tclconfig (TEA) file system, for pre-tip430
 *	projects.
 */

#include "tclInt.h"
#include "tclFileSystem.h"

#ifndef _WIN32
#include <sys/mman.h>
#endif /* _WIN32*/

#ifndef MAP_FILE
#define MAP_FILE 0
#endif /* !MAP_FILE */
#define NOBYFOUR
#ifndef TBLS
#define TBLS 1
#endif

#if !defined(_WIN32) && !defined(NO_DLFCN_H)
#include <dlfcn.h>
#endif

/*
 * Macros to report errors only if an interp is present.
 */

#define ZIPFS_ERROR(interp,errstr) \
    do {								\
	if (interp) {							\
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(errstr, -1));	\
	}								\
    } while (0)
#define ZIPFS_MEM_ERROR(interp) \
    do {								\
	if (interp) {							\
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(			\
		    "out of memory", -1));				\
	    Tcl_SetErrorCode(interp, "TCL", "MALLOC", NULL);		\
	}								\
    } while (0)
#define ZIPFS_POSIX_ERROR(interp,errstr) \
    do {								\
	if (interp) {							\
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(			\
		    "%s: %s", errstr, Tcl_PosixError(interp)));		\
	}								\
    } while (0)
#define ZIPFS_ERROR_CODE(interp,errcode) \
    do {								\
	if (interp) {							\
	    Tcl_SetErrorCode(interp, "TCL", "ZIPFS", errcode, NULL);	\
	}								\
    } while (0)


#ifdef HAVE_ZLIB
#include "zlib.h"
#include "crypt.h"
#include "zutil.h"
#include "crc32.h"

static const z_crc_t* crc32tab;

/*
** We are compiling as part of the core.
** TIP430 style zipfs prefix
*/

#define ZIPFS_VOLUME	  "//zipfs:/"
#define ZIPFS_VOLUME_LEN  9
#define ZIPFS_APP_MOUNT	  "//zipfs:/app"
#define ZIPFS_ZIP_MOUNT	  "//zipfs:/lib/tcl"
#define ZIPFS_FALLBACK_ENCODING "cp437"

/*
 * Various constants and offsets found in ZIP archive files
 */

#define ZIP_SIG_LEN			4

/*
 * Local header of ZIP archive member (at very beginning of each member).
 */

#define ZIP_LOCAL_HEADER_SIG		0x04034b50
#define ZIP_LOCAL_HEADER_LEN		30
#define ZIP_LOCAL_SIG_OFFS		0
#define ZIP_LOCAL_VERSION_OFFS		4
#define ZIP_LOCAL_FLAGS_OFFS		6
#define ZIP_LOCAL_COMPMETH_OFFS		8
#define ZIP_LOCAL_MTIME_OFFS		10
#define ZIP_LOCAL_MDATE_OFFS		12
#define ZIP_LOCAL_CRC32_OFFS		14
#define ZIP_LOCAL_COMPLEN_OFFS		18
#define ZIP_LOCAL_UNCOMPLEN_OFFS	22
#define ZIP_LOCAL_PATHLEN_OFFS		26
#define ZIP_LOCAL_EXTRALEN_OFFS		28

/*
 * Central header of ZIP archive member at end of ZIP file.
 */

#define ZIP_CENTRAL_HEADER_SIG		0x02014b50
#define ZIP_CENTRAL_HEADER_LEN		46
#define ZIP_CENTRAL_SIG_OFFS		0
#define ZIP_CENTRAL_VERSIONMADE_OFFS	4
#define ZIP_CENTRAL_VERSION_OFFS	6
#define ZIP_CENTRAL_FLAGS_OFFS		8
#define ZIP_CENTRAL_COMPMETH_OFFS	10
#define ZIP_CENTRAL_MTIME_OFFS		12
#define ZIP_CENTRAL_MDATE_OFFS		14
#define ZIP_CENTRAL_CRC32_OFFS		16
#define ZIP_CENTRAL_COMPLEN_OFFS	20
#define ZIP_CENTRAL_UNCOMPLEN_OFFS	24
#define ZIP_CENTRAL_PATHLEN_OFFS	28
#define ZIP_CENTRAL_EXTRALEN_OFFS	30
#define ZIP_CENTRAL_FCOMMENTLEN_OFFS	32
#define ZIP_CENTRAL_DISKFILE_OFFS	34
#define ZIP_CENTRAL_IATTR_OFFS		36
#define ZIP_CENTRAL_EATTR_OFFS		38
#define ZIP_CENTRAL_LOCALHDR_OFFS	42

/*
 * Central end signature at very end of ZIP file.
 */

#define ZIP_CENTRAL_END_SIG		0x06054b50
#define ZIP_CENTRAL_END_LEN		22
#define ZIP_CENTRAL_END_SIG_OFFS	0
#define ZIP_CENTRAL_DISKNO_OFFS		4
#define ZIP_CENTRAL_DISKDIR_OFFS	6
#define ZIP_CENTRAL_ENTS_OFFS		8
#define ZIP_CENTRAL_TOTALENTS_OFFS	10
#define ZIP_CENTRAL_DIRSIZE_OFFS	12
#define ZIP_CENTRAL_DIRSTART_OFFS	16
#define ZIP_CENTRAL_COMMENTLEN_OFFS	20

#define ZIP_MIN_VERSION			20
#define ZIP_COMPMETH_STORED		0
#define ZIP_COMPMETH_DEFLATED		8

#define ZIP_PASSWORD_END_SIG		0x5a5a4b50

#define DEFAULT_WRITE_MAX_SIZE		(2 * 1024 * 1024)

/*
 * Windows drive letters.
 */

#ifdef _WIN32
static const char drvletters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
#endif /* _WIN32 */

/*
 * Mutex to protect localtime(3) when no reentrant version available.
 */

#if !defined(_WIN32) && !defined(HAVE_LOCALTIME_R) && TCL_THREADS
TCL_DECLARE_MUTEX(localtimeMutex)
#endif /* !_WIN32 && !HAVE_LOCALTIME_R && TCL_THREADS */

/*
 * Forward declaration.
 */

struct ZipEntry;

/*
 * In-core description of mounted ZIP archive file.
 */

typedef struct ZipFile {
    char *name;			/* Archive name */
    size_t nameLength;		/* Length of archive name */
    char isMemBuffer;		/* When true, not a file but a memory buffer */
    Tcl_Channel chan;		/* Channel handle or NULL */
    unsigned char *data;	/* Memory mapped or malloc'ed file */
    size_t length;		/* Length of memory mapped file */
    void *ptrToFree;		/* Non-NULL if malloc'ed file */
    size_t numFiles;		/* Number of files in archive */
    size_t baseOffset;		/* Archive start */
    size_t passOffset;		/* Password start */
    size_t directoryOffset;	/* Archive directory start */
    unsigned char passBuf[264];	/* Password buffer */
    size_t numOpen;		/* Number of open files on archive */
    struct ZipEntry *entries;	/* List of files in archive */
    struct ZipEntry *topEnts;	/* List of top-level dirs in archive */
    char *mountPoint;		/* Mount point name */
    size_t mountPointLen;	/* Length of mount point name */
#ifdef _WIN32
    HANDLE mountHandle;		/* Handle used for direct file access. */
#endif /* _WIN32 */
} ZipFile;

/*
 * In-core description of file contained in mounted ZIP archive.
 * ZIP_ATTR_
 */

typedef struct ZipEntry {
    char *name;			/* The full pathname of the virtual file */
    ZipFile *zipFilePtr;	/* The ZIP file holding this virtual file */
    size_t offset;		/* Data offset into memory mapped ZIP file */
    int numBytes;		/* Uncompressed size of the virtual file */
    int numCompressedBytes;	/* Compressed size of the virtual file */
    int compressMethod;		/* Compress method */
    int isDirectory;		/* Set to 1 if directory, or -1 if root */
    int depth;			/* Number of slashes in path. */
    int crc32;			/* CRC-32 */
    int timestamp;		/* Modification time */
    int isEncrypted;		/* True if data is encrypted */
    unsigned char *data;	/* File data if written */
    struct ZipEntry *next;	/* Next file in the same archive */
    struct ZipEntry *tnext;	/* Next top-level dir in archive */
} ZipEntry;

/*
 * File channel for file contained in mounted ZIP archive.
 */

typedef struct ZipChannel {
    ZipFile *zipFilePtr;	/* The ZIP file holding this channel */
    ZipEntry *zipEntryPtr;	/* Pointer back to virtual file */
    size_t maxWrite;		/* Maximum size for write */
    size_t numBytes;		/* Number of bytes of uncompressed data */
    size_t numRead;		/* Position of next byte to be read from the
				 * channel */
    unsigned char *ubuf;	/* Pointer to the uncompressed data */
    int iscompr;		/* True if data is compressed */
    int isDirectory;		/* Set to 1 if directory, or -1 if root */
    int isEncrypted;		/* True if data is encrypted */
    int isWriting;		/* True if open for writing */
    unsigned long keys[3];	/* Key for decryption */
} ZipChannel;

/*
 * Global variables.
 *
 * Most are kept in single ZipFS struct. When build with threading support
 * this struct is protected by the ZipFSMutex (see below).
 *
 * The "fileHash" component is the process-wide global table of all known ZIP
 * archive members in all mounted ZIP archives.
 *
 * The "zipHash" components is the process wide global table of all mounted
 * ZIP archive files.
 */

static struct {
    int initialized;		/* True when initialized */
    int lock;			/* RW lock, see below */
    int waiters;		/* RW lock, see below */
    int wrmax;			/* Maximum write size of a file; only written
				 * to from Tcl code in a trusted interpreter,
				 * so NOT protected by mutex. */
    char *fallbackEntryEncoding;/* The fallback encoding for ZIP entries when
				 * they are believed to not be UTF-8; only
				 * written to from Tcl code in a trusted
				 * interpreter, so not protected by mutex. */
    Tcl_Encoding utf8;		/* The UTF-8 encoding that we prefer to use
				 * for the strings (especially filenames)
				 * embedded in a ZIP. Other encodings are used
				 * dynamically. */
    int idCount;		/* Counter for channel names */
    Tcl_HashTable fileHash;	/* File name to ZipEntry mapping */
    Tcl_HashTable zipHash;	/* Mount to ZipFile mapping */
} ZipFS = {
    0, 0, 0, DEFAULT_WRITE_MAX_SIZE, NULL, NULL, 0,
	    {0,{0,0,0,0},0,0,0,0,0,0,0,0,0},
	    {0,{0,0,0,0},0,0,0,0,0,0,0,0,0}
};

/*
 * For password rotation.
 */

static const char pwrot[17] =
    "\x00\x80\x40\xC0\x20\xA0\x60\xE0"
    "\x10\x90\x50\xD0\x30\xB0\x70\xF0";

static const char *zipfs_literal_tcl_library = NULL;

/* Function prototypes */

static int		CopyImageFile(Tcl_Interp *interp, const char *imgName,
			    Tcl_Channel out);
static inline int	DescribeMounted(Tcl_Interp *interp,
			    const char *mountPoint);
static int		InitReadableChannel(Tcl_Interp *interp,
			    ZipChannel *info, ZipEntry *z);
static int		InitWritableChannel(Tcl_Interp *interp,
			    ZipChannel *info, ZipEntry *z, int trunc);
static inline int	ListMountPoints(Tcl_Interp *interp);
static void		SerializeCentralDirectoryEntry(
			    const unsigned char *start,
			    const unsigned char *end, unsigned char *buf,
			    ZipEntry *z, size_t nameLength);
static void		SerializeCentralDirectorySuffix(
			    const unsigned char *start,
			    const unsigned char *end, unsigned char *buf,
			    int entryCount, long long directoryStartOffset,
			    long long suffixStartOffset);
static void		SerializeLocalEntryHeader(
			    const unsigned char *start,
			    const unsigned char *end, unsigned char *buf,
			    ZipEntry *z, int nameLength, int align);
#if !defined(STATIC_BUILD)
static int		ZipfsAppHookFindTclInit(const char *archive);
#endif
static int		ZipFSPathInFilesystemProc(Tcl_Obj *pathPtr,
			    void **clientDataPtr);
static Tcl_Obj *	ZipFSFilesystemPathTypeProc(Tcl_Obj *pathPtr);
static Tcl_Obj *	ZipFSFilesystemSeparatorProc(Tcl_Obj *pathPtr);
static int		ZipFSStatProc(Tcl_Obj *pathPtr, Tcl_StatBuf *buf);
static int		ZipFSAccessProc(Tcl_Obj *pathPtr, int mode);
static Tcl_Channel	ZipFSOpenFileChannelProc(Tcl_Interp *interp,
			    Tcl_Obj *pathPtr, int mode, int permissions);
static int		ZipFSMatchInDirectoryProc(Tcl_Interp *interp,
			    Tcl_Obj *result, Tcl_Obj *pathPtr,
			    const char *pattern, Tcl_GlobTypeData *types);
static void		ZipFSMatchMountPoints(Tcl_Obj *result,
			    Tcl_Obj *normPathPtr, const char *pattern,
			    Tcl_DString *prefix);
static Tcl_Obj *	ZipFSListVolumesProc(void);
static const char *const *ZipFSFileAttrStringsProc(Tcl_Obj *pathPtr,
			    Tcl_Obj **objPtrRef);
static int		ZipFSFileAttrsGetProc(Tcl_Interp *interp, int index,
			    Tcl_Obj *pathPtr, Tcl_Obj **objPtrRef);
static int		ZipFSFileAttrsSetProc(Tcl_Interp *interp, int index,
			    Tcl_Obj *pathPtr, Tcl_Obj *objPtr);
static int		ZipFSLoadFile(Tcl_Interp *interp, Tcl_Obj *path,
			    Tcl_LoadHandle *loadHandle,
			    Tcl_FSUnloadFileProc **unloadProcPtr, int flags);
static int		ZipMapArchive(Tcl_Interp *interp, ZipFile *zf,
			    void *handle);
static void		ZipfsExitHandler(ClientData clientData);
static void		ZipfsMountExitHandler(ClientData clientData);
static void		ZipfsSetup(void);
static void		ZipfsFinalize(void);
static int		ZipChannelClose(void *instanceData,
			    Tcl_Interp *interp, int flags);
static Tcl_DriverGetHandleProc	ZipChannelGetFile;
static int		ZipChannelRead(void *instanceData, char *buf,
			    int toRead, int *errloc);
#if !defined(TCL_NO_DEPRECATED) && (TCL_MAJOR_VERSION < 9)
static int		ZipChannelSeek(void *instanceData, long offset,
			    int mode, int *errloc);
#endif
static long long	ZipChannelWideSeek(void *instanceData,
			    long long offset, int mode, int *errloc);
static void		ZipChannelWatchChannel(void *instanceData,
			    int mask);
static int		ZipChannelWrite(void *instanceData,
			    const char *buf, int toWrite, int *errloc);

/*
 * Define the ZIP filesystem dispatch table.
 */

static const Tcl_Filesystem zipfsFilesystem = {
    "zipfs",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_2,
    ZipFSPathInFilesystemProc,
    NULL, /* dupInternalRepProc */
    NULL, /* freeInternalRepProc */
    NULL, /* internalToNormalizedProc */
    NULL, /* createInternalRepProc */
    NULL, /* normalizePathProc */
    ZipFSFilesystemPathTypeProc,
    ZipFSFilesystemSeparatorProc,
    ZipFSStatProc,
    ZipFSAccessProc,
    ZipFSOpenFileChannelProc,
    ZipFSMatchInDirectoryProc,
    NULL, /* utimeProc */
    NULL, /* linkProc */
    ZipFSListVolumesProc,
    ZipFSFileAttrStringsProc,
    ZipFSFileAttrsGetProc,
    ZipFSFileAttrsSetProc,
    NULL, /* createDirectoryProc */
    NULL, /* removeDirectoryProc */
    NULL, /* deleteFileProc */
    NULL, /* copyFileProc */
    NULL, /* renameFileProc */
    NULL, /* copyDirectoryProc */
    NULL, /* lstatProc */
    (Tcl_FSLoadFileProc *) (void *) ZipFSLoadFile,
    NULL, /* getCwdProc */
    NULL, /* chdirProc */
};

/*
 * The channel type/driver definition used for ZIP archive members.
 */

static Tcl_ChannelType ZipChannelType = {
    "zip",			/* Type name. */
    TCL_CHANNEL_VERSION_5,
    TCL_CLOSE2PROC,		/* Close channel, clean instance data */
    ZipChannelRead,		/* Handle read request */
    ZipChannelWrite,		/* Handle write request */
#if !defined(TCL_NO_DEPRECATED) && (TCL_MAJOR_VERSION < 9)
    ZipChannelSeek,		/* Move location of access point, NULL'able */
#else
    NULL,			/* Move location of access point, NULL'able */
#endif
    NULL,			/* Set options, NULL'able */
    NULL,			/* Get options, NULL'able */
    ZipChannelWatchChannel,	/* Initialize notifier */
    ZipChannelGetFile,		/* Get OS handle from the channel */
    ZipChannelClose,		/* 2nd version of close channel, NULL'able */
    NULL,			/* Set blocking mode for raw channel,
				 * NULL'able */
    NULL,			/* Function to flush channel, NULL'able */
    NULL,			/* Function to handle event, NULL'able */
    ZipChannelWideSeek,		/* Wide seek function, NULL'able */
    NULL,			/* Thread action function, NULL'able */
    NULL,			/* Truncate function, NULL'able */
};

/*
 * Miscellaneous constants.
 */

#define ERROR_LENGTH	((size_t) -1)

/*
 *-------------------------------------------------------------------------
 *
 * ZipReadInt, ZipReadShort, ZipWriteInt, ZipWriteShort --
 *
 *	Inline functions to read and write little-endian 16 and 32 bit
 *	integers from/to buffers representing parts of ZIP archives.
 *
 *	These take bufferStart and bufferEnd pointers, which are used to
 *	maintain a guarantee that out-of-bounds accesses don't happen when
 *	reading or writing critical directory structures.
 *
 *-------------------------------------------------------------------------
 */

static inline unsigned int
ZipReadInt(
    const unsigned char *bufferStart,
    const unsigned char *bufferEnd,
    const unsigned char *ptr)
{
    if (ptr < bufferStart || ptr + 4 > bufferEnd) {
	Tcl_Panic("out of bounds read(4): start=%p, end=%p, ptr=%p",
		bufferStart, bufferEnd, ptr);
    }
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) |
	    ((unsigned int)ptr[3] << 24);
}

static inline unsigned short
ZipReadShort(
    const unsigned char *bufferStart,
    const unsigned char *bufferEnd,
    const unsigned char *ptr)
{
    if (ptr < bufferStart || ptr + 2 > bufferEnd) {
	Tcl_Panic("out of bounds read(2): start=%p, end=%p, ptr=%p",
		bufferStart, bufferEnd, ptr);
    }
    return ptr[0] | (ptr[1] << 8);
}

static inline void
ZipWriteInt(
    const unsigned char *bufferStart,
    const unsigned char *bufferEnd,
    unsigned char *ptr,
    unsigned int value)
{
    if (ptr < bufferStart || ptr + 4 > bufferEnd) {
	Tcl_Panic("out of bounds write(4): start=%p, end=%p, ptr=%p",
		bufferStart, bufferEnd, ptr);
    }
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
}

static inline void
ZipWriteShort(
    const unsigned char *bufferStart,
    const unsigned char *bufferEnd,
    unsigned char *ptr,
    unsigned short value)
{
    if (ptr < bufferStart || ptr + 2 > bufferEnd) {
	Tcl_Panic("out of bounds write(2): start=%p, end=%p, ptr=%p",
		bufferStart, bufferEnd, ptr);
    }
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
}

/*
 *-------------------------------------------------------------------------
 *
 * ReadLock, WriteLock, Unlock --
 *
 *	POSIX like rwlock functions to support multiple readers and single
 *	writer on internal structs.
 *
 *	Limitations:
 *	 - a read lock cannot be promoted to a write lock
 *	 - a write lock may not be nested
 *
 *-------------------------------------------------------------------------
 */

TCL_DECLARE_MUTEX(ZipFSMutex)

#if TCL_THREADS

static Tcl_Condition ZipFSCond;

static inline void
ReadLock(void)
{
    Tcl_MutexLock(&ZipFSMutex);
    while (ZipFS.lock < 0) {
	ZipFS.waiters++;
	Tcl_ConditionWait(&ZipFSCond, &ZipFSMutex, NULL);
	ZipFS.waiters--;
    }
    ZipFS.lock++;
    Tcl_MutexUnlock(&ZipFSMutex);
}

static inline void
WriteLock(void)
{
    Tcl_MutexLock(&ZipFSMutex);
    while (ZipFS.lock != 0) {
	ZipFS.waiters++;
	Tcl_ConditionWait(&ZipFSCond, &ZipFSMutex, NULL);
	ZipFS.waiters--;
    }
    ZipFS.lock = -1;
    Tcl_MutexUnlock(&ZipFSMutex);
}

static inline void
Unlock(void)
{
    Tcl_MutexLock(&ZipFSMutex);
    if (ZipFS.lock > 0) {
	--ZipFS.lock;
    } else if (ZipFS.lock < 0) {
	ZipFS.lock = 0;
    }
    if ((ZipFS.lock == 0) && (ZipFS.waiters > 0)) {
	Tcl_ConditionNotify(&ZipFSCond);
    }
    Tcl_MutexUnlock(&ZipFSMutex);
}

#else /* !TCL_THREADS */
#define ReadLock()	do {} while (0)
#define WriteLock()	do {} while (0)
#define Unlock()	do {} while (0)
#endif /* TCL_THREADS */

/*
 *-------------------------------------------------------------------------
 *
 * DosTimeDate, ToDosTime, ToDosDate --
 *
 *	Functions to perform conversions between DOS time stamps and POSIX
 *	time_t.
 *
 *-------------------------------------------------------------------------
 */

static time_t
DosTimeDate(
    int dosDate,
    int dosTime)
{
    struct tm tm;
    time_t ret;

    memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;			/* let mktime() deal with DST */
    tm.tm_year = ((dosDate & 0xfe00) >> 9) + 80;
    tm.tm_mon = ((dosDate & 0x1e0) >> 5) - 1;
    tm.tm_mday = dosDate & 0x1f;
    tm.tm_hour = (dosTime & 0xf800) >> 11;
    tm.tm_min = (dosTime & 0x7e0) >> 5;
    tm.tm_sec = (dosTime & 0x1f) << 1;
    ret = mktime(&tm);
    if (ret == (time_t) -1) {
	/* fallback to 1980-01-01T00:00:00+00:00 (DOS epoch) */
	ret = (time_t) 315532800;
    }
    return ret;
}

static int
ToDosTime(
    time_t when)
{
    struct tm *tmp, tm;

#if !TCL_THREADS || defined(_WIN32)
    /* Not threaded, or on Win32 which uses thread local storage */
    tmp = localtime(&when);
    tm = *tmp;
#elif defined(HAVE_LOCALTIME_R)
    /* Threaded, have reentrant API */
    tmp = &tm;
    localtime_r(&when, tmp);
#else /* TCL_THREADS && !_WIN32 && !HAVE_LOCALTIME_R */
    /* Only using a mutex is safe. */
    Tcl_MutexLock(&localtimeMutex);
    tmp = localtime(&when);
    tm = *tmp;
    Tcl_MutexUnlock(&localtimeMutex);
#endif
    return (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec >> 1);
}

static int
ToDosDate(
    time_t when)
{
    struct tm *tmp, tm;

#if !TCL_THREADS || defined(_WIN32)
    /* Not threaded, or on Win32 which uses thread local storage */
    tmp = localtime(&when);
    tm = *tmp;
#elif /* TCL_THREADS && !_WIN32 && */ defined(HAVE_LOCALTIME_R)
    /* Threaded, have reentrant API */
    tmp = &tm;
    localtime_r(&when, tmp);
#else /* TCL_THREADS && !_WIN32 && !HAVE_LOCALTIME_R */
    /* Only using a mutex is safe. */
    Tcl_MutexLock(&localtimeMutex);
    tmp = localtime(&when);
    tm = *tmp;
    Tcl_MutexUnlock(&localtimeMutex);
#endif
    return ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;
}

/*
 *-------------------------------------------------------------------------
 *
 * CountSlashes --
 *
 *	This function counts the number of slashes in a pathname string.
 *
 * Results:
 *	Number of slashes found in string.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static inline int
CountSlashes(
    const char *string)
{
    int count = 0;
    const char *p = string;

    while (*p != '\0') {
	if (*p == '/') {
	    count++;
	}
	p++;
    }
    return count;
}

/*
 *-------------------------------------------------------------------------
 *
 * DecodeZipEntryText --
 *
 *	Given a sequence of bytes from an entry in a ZIP central directory,
 *	convert that into a Tcl string. This is complicated because we don't
 *	actually know what encoding is in use! So we try to use UTF-8, and if
 *	that goes wrong, we fall back to a user-specified encoding, or to an
 *	encoding we specify (Windows code page 437), or to ISO 8859-1 if
 *	absolutely nothing else works.
 *
 *	During Tcl startup, we skip the user-specified encoding and cp437, as
 *	we may well not have any loadable encodings yet. Tcl's own library
 *	files ought to be using ASCII filenames.
 *
 * Results:
 *	The decoded filename; the filename is owned by the argument DString.
 *
 * Side effects:
 *	Updates dstPtr.
 *
 *-------------------------------------------------------------------------
 */

static char *
DecodeZipEntryText(
    const unsigned char *inputBytes,
    unsigned int inputLength,
    Tcl_DString *dstPtr)
{
    Tcl_Encoding encoding;
    const char *src;
    char *dst;
    int dstLen, srcLen = inputLength, flags;
    Tcl_EncodingState state;

    Tcl_DStringInit(dstPtr);
    if (inputLength < 1) {
	return Tcl_DStringValue(dstPtr);
    }

    /*
     * We can't use Tcl_ExternalToUtfDString at this point; it has no way to
     * fail. So we use this modified version of it that can report encoding
     * errors to us (so we can fall back to something else).
     *
     * The utf-8 encoding is implemented internally, and so is guaranteed to
     * be present.
     */

    src = (const char *) inputBytes;
    dst = Tcl_DStringValue(dstPtr);
    dstLen = dstPtr->spaceAvl - 1;
    flags = TCL_ENCODING_START | TCL_ENCODING_END |
	    TCL_ENCODING_STOPONERROR;	/* Special flag! */

    while (1) {
	int srcRead, dstWrote;
	int result = Tcl_ExternalToUtf(NULL, ZipFS.utf8, src, srcLen, flags,
		&state, dst, dstLen, &srcRead, &dstWrote, NULL);
	int soFar = dst + dstWrote - Tcl_DStringValue(dstPtr);

	if (result == TCL_OK) {
	    Tcl_DStringSetLength(dstPtr, soFar);
	    return Tcl_DStringValue(dstPtr);
	} else if (result != TCL_CONVERT_NOSPACE) {
	    break;
	}

	flags &= ~TCL_ENCODING_START;
	src += srcRead;
	srcLen -= srcRead;
	if (Tcl_DStringLength(dstPtr) == 0) {
	    Tcl_DStringSetLength(dstPtr, dstLen);
	}
	Tcl_DStringSetLength(dstPtr, 2 * Tcl_DStringLength(dstPtr) + 1);
	dst = Tcl_DStringValue(dstPtr) + soFar;
	dstLen = Tcl_DStringLength(dstPtr) - soFar - 1;
    }

    /*
     * Something went wrong. Fall back to another encoding. Those *can* use
     * Tcl_ExternalToUtfDString().
     */

    encoding = NULL;
    if (ZipFS.fallbackEntryEncoding) {
	encoding = Tcl_GetEncoding(NULL, ZipFS.fallbackEntryEncoding);
    }
    if (!encoding) {
	encoding = Tcl_GetEncoding(NULL, ZIPFS_FALLBACK_ENCODING);
    }
    if (!encoding) {
	/*
	 * Fallback to internal encoding that always converts all bytes.
	 * Should only happen when a filename isn't UTF-8 and we've not got
	 * our encodings initialised for some reason.
	 */

	encoding = Tcl_GetEncoding(NULL, "iso8859-1");
    }

    char *converted = Tcl_ExternalToUtfDString(encoding,
	    (const char *) inputBytes, inputLength, dstPtr);
    Tcl_FreeEncoding(encoding);
    return converted;
}

/*
 *-------------------------------------------------------------------------
 *
 * CanonicalPath --
 *
 *	This function computes the canonical path from a directory and file
 *	name components into the specified Tcl_DString.
 *
 * Results:
 *	Returns the pointer to the canonical path contained in the specified
 *	Tcl_DString.
 *
 * Side effects:
 *	Modifies the specified Tcl_DString.
 *
 *-------------------------------------------------------------------------
 */

static char *
CanonicalPath(
    const char *root,
    const char *tail,
    Tcl_DString *dsPtr,
    int inZipfs)
{
    char *path;
    int i, j, c, isUNC = 0, isVfs = 0, n = 0;
    int haveZipfsPath = 1;

#ifdef _WIN32
    if (tail[0] != '\0' && strchr(drvletters, tail[0]) && tail[1] == ':') {
	tail += 2;
	haveZipfsPath = 0;
    }
    /* UNC style path */
    if (tail[0] == '\\') {
	root = "";
	++tail;
	haveZipfsPath = 0;
    }
    if (tail[0] == '\\') {
	root = "/";
	++tail;
	haveZipfsPath = 0;
    }
#endif /* _WIN32 */

    if (haveZipfsPath) {
	/* UNC style path */
	if (root && strncmp(root, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN) == 0) {
	    isVfs = 1;
	} else if (tail &&
		strncmp(tail, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN) == 0) {
	    isVfs = 2;
	}
	if (isVfs != 1 && (root[0] == '/') && (root[1] == '/')) {
	    isUNC = 1;
	}
    }

    if (isVfs != 2) {
	if (tail[0] == '/') {
	    if (isVfs != 1) {
		root = "";
	    }
	    ++tail;
	    isUNC = 0;
	}
	if (tail[0] == '/') {
	    if (isVfs != 1) {
		root = "/";
	    }
	    ++tail;
	    isUNC = 1;
	}
    }
    i = strlen(root);
    j = strlen(tail);

    switch (isVfs) {
    case 1:
	if (i > ZIPFS_VOLUME_LEN) {
	    Tcl_DStringSetLength(dsPtr, i + j + 1);
	    path = Tcl_DStringValue(dsPtr);
	    memcpy(path, root, i);
	    path[i++] = '/';
	    memcpy(path + i, tail, j);
	} else {
	    Tcl_DStringSetLength(dsPtr, i + j);
	    path = Tcl_DStringValue(dsPtr);
	    memcpy(path, root, i);
	    memcpy(path + i, tail, j);
	}
	break;
    case 2:
	Tcl_DStringSetLength(dsPtr, j);
	path = Tcl_DStringValue(dsPtr);
	memcpy(path, tail, j);
	break;
    default:
	if (inZipfs) {
	    Tcl_DStringSetLength(dsPtr, i + j + ZIPFS_VOLUME_LEN);
	    path = Tcl_DStringValue(dsPtr);
	    memcpy(path, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN);
	    memcpy(path + ZIPFS_VOLUME_LEN + i , tail, j);
	} else {
	    Tcl_DStringSetLength(dsPtr, i + j + 1);
	    path = Tcl_DStringValue(dsPtr);
	    memcpy(path, root, i);
	    path[i++] = '/';
	    memcpy(path + i, tail, j);
	}
	break;
    }

#ifdef _WIN32
    for (i = 0; path[i] != '\0'; i++) {
	if (path[i] == '\\') {
	    path[i] = '/';
	}
    }
#endif /* _WIN32 */

    if (inZipfs) {
	n = ZIPFS_VOLUME_LEN;
    } else {
	n = 0;
    }

    for (i = j = n; (c = path[i]) != '\0'; i++) {
	if (c == '/') {
	    int c2 = path[i + 1];

	    if (c2 == '\0' || c2 == '/') {
		continue;
	    }
	    if (c2 == '.') {
		int c3 = path[i + 2];

		if ((c3 == '/') || (c3 == '\0')) {
		    i++;
		    continue;
		}
		if ((c3 == '.')
			&& ((path[i + 3] == '/') || (path[i + 3] == '\0'))) {
		    i += 2;
		    while ((j > 0) && (path[j - 1] != '/')) {
			j--;
		    }
		    if (j > isUNC) {
			--j;
			while ((j > 1 + isUNC) && (path[j - 2] == '/')) {
			    j--;
			}
		    }
		    continue;
		}
	    }
	}
	path[j++] = c;
    }
    if (j == 0) {
	path[j++] = '/';
    }
    path[j] = 0;
    Tcl_DStringSetLength(dsPtr, j);
    return Tcl_DStringValue(dsPtr);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLookup --
 *
 *	This function returns the ZIP entry struct corresponding to the ZIP
 *	archive member of the given file name. Caller must hold the right
 *	lock.
 *
 * Results:
 *	Returns the pointer to ZIP entry struct or NULL if the the given file
 *	name could not be found in the global list of ZIP archive members.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static inline ZipEntry *
ZipFSLookup(
    const char *filename)
{
    Tcl_HashEntry *hPtr;
    ZipEntry *z = NULL;

    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, filename);
    if (hPtr) {
	z = (ZipEntry *) Tcl_GetHashValue(hPtr);
    }
    return z;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLookupZip --
 *
 *	This function gets the structure for a mounted ZIP archive.
 *
 * Results:
 *	Returns a pointer to the structure, or NULL if the file is ZIP file is
 *	unknown/not mounted.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static inline ZipFile *
ZipFSLookupZip(
    const char *mountPoint)
{
    Tcl_HashEntry *hPtr;
    ZipFile *zf = NULL;

    hPtr = Tcl_FindHashEntry(&ZipFS.zipHash, mountPoint);
    if (hPtr) {
	zf = (ZipFile *) Tcl_GetHashValue(hPtr);
    }
    return zf;
}

/*
 *-------------------------------------------------------------------------
 *
 * AllocateZipFile, AllocateZipEntry, AllocateZipChannel --
 *
 *	Allocates the memory for a datastructure. Always ensures that it is
 *	zeroed out for safety.
 *
 * Returns:
 *	The allocated structure, or NULL if allocate fails.
 *
 * Side effects:
 *	The interpreter result may be written to on error. Which might fail
 *	(for ZipFile) in a low-memory situation. Always panics if ZipEntry
 *	allocation fails.
 *
 *-------------------------------------------------------------------------
 */

static inline ZipFile *
AllocateZipFile(
    Tcl_Interp *interp,
    size_t mountPointNameLength)
{
    size_t size = sizeof(ZipFile) + mountPointNameLength + 1;
    ZipFile *zf = (ZipFile *) attemptckalloc(size);

    if (!zf) {
	ZIPFS_MEM_ERROR(interp);
    } else {
	memset(zf, 0, size);
    }
    return zf;
}

static inline ZipEntry *
AllocateZipEntry(void)
{
    ZipEntry *z = (ZipEntry *) ckalloc(sizeof(ZipEntry));
    memset(z, 0, sizeof(ZipEntry));
    return z;
}

static inline ZipChannel *
AllocateZipChannel(
    Tcl_Interp *interp)
{
    ZipChannel *zc = (ZipChannel *) attemptckalloc(sizeof(ZipChannel));

    if (!zc) {
	ZIPFS_MEM_ERROR(interp);
    } else {
	memset(zc, 0, sizeof(ZipChannel));
    }
    return zc;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSCloseArchive --
 *
 *	This function closes a mounted ZIP archive file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A memory mapped ZIP archive is unmapped, allocated memory is released.
 *	The ZipFile pointer is *NOT* deallocated by this function.
 *
 *-------------------------------------------------------------------------
 */

static void
ZipFSCloseArchive(
    Tcl_Interp *interp,		/* Current interpreter. */
    ZipFile *zf)
{
    if (zf->nameLength) {
	ckfree(zf->name);
    }
    if (zf->isMemBuffer) {
	/* Pointer to memory */
	if (zf->ptrToFree) {
	    ckfree(zf->ptrToFree);
	    zf->ptrToFree = NULL;
	}
	zf->data = NULL;
	return;
    }

    /*
     * Remove the memory mapping, if we have one.
     */

#ifdef _WIN32
    if (zf->data && !zf->ptrToFree) {
	UnmapViewOfFile(zf->data);
	zf->data = NULL;
    }
    if (zf->mountHandle != INVALID_HANDLE_VALUE) {
	CloseHandle(zf->mountHandle);
    }
#else /* !_WIN32 */
    if ((zf->data != MAP_FAILED) && !zf->ptrToFree) {
	munmap(zf->data, zf->length);
	zf->data = (unsigned char *) MAP_FAILED;
    }
#endif /* _WIN32 */

    if (zf->ptrToFree) {
	ckfree(zf->ptrToFree);
	zf->ptrToFree = NULL;
    }
    if (zf->chan) {
	Tcl_Close(interp, zf->chan);
	zf->chan = NULL;
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFindTOC --
 *
 *	This function takes a memory mapped zip file and indexes the contents.
 *	When "needZip" is zero an embedded ZIP archive in an executable file
 *	is accepted. Note that we do not support ZIP64.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with an error message placed
 *	into the given "interp" if it is not NULL.
 *
 * Side effects:
 *	The given ZipFile struct is filled with information about the ZIP
 *	archive file.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSFindTOC(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    int needZip,
    ZipFile *zf)
{
    size_t i, minoff;
    const unsigned char *p, *q;
    const unsigned char *start = zf->data;
    const unsigned char *end = zf->data + zf->length;

    /*
     * Scan backwards from the end of the file for the signature. This is
     * necessary because ZIP archives aren't the only things that get tagged
     * on the end of executables; digital signatures can also go there.
     */

    p = zf->data + zf->length - ZIP_CENTRAL_END_LEN;
    while (p >= start) {
	if (*p == (ZIP_CENTRAL_END_SIG & 0xFF)) {
	    if (ZipReadInt(start, end, p) == ZIP_CENTRAL_END_SIG) {
		break;
	    }
	    p -= ZIP_SIG_LEN;
	} else {
	    --p;
	}
    }
    if (p < zf->data) {
	/*
	 * Didn't find it (or not enough space for a central directory!); not
	 * a ZIP archive. This might be OK or a problem.
	 */

	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "wrong end signature");
	ZIPFS_ERROR_CODE(interp, "END_SIG");
	goto error;
    }

    /*
     * How many files in the archive? If that's bogus, we're done here.
     */

    zf->numFiles = ZipReadShort(start, end, p + ZIP_CENTRAL_ENTS_OFFS);
    if (zf->numFiles == 0) {
	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "empty archive");
	ZIPFS_ERROR_CODE(interp, "EMPTY");
	goto error;
    }

    /*
     * Where does the central directory start?
     */

    q = zf->data + ZipReadInt(start, end, p + ZIP_CENTRAL_DIRSTART_OFFS);
    p -= ZipReadInt(start, end, p + ZIP_CENTRAL_DIRSIZE_OFFS);
    zf->baseOffset = zf->passOffset = (p>q) ? p - q : 0;
    zf->directoryOffset = q - zf->data + zf->baseOffset;
    if ((p < q) || (p < zf->data) || (p > zf->data + zf->length)
	    || (q < zf->data) || (q > zf->data + zf->length)) {
	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "archive directory not found");
	ZIPFS_ERROR_CODE(interp, "NO_DIR");
	goto error;
    }

    /*
     * Read the central directory.
     */

    q = p;
    minoff = zf->length;
    for (i = 0; i < zf->numFiles; i++) {
	int pathlen, comlen, extra;
	size_t localhdr_off = zf->length;

	if (q + ZIP_CENTRAL_HEADER_LEN > end) {
	    ZIPFS_ERROR(interp, "wrong header length");
	    ZIPFS_ERROR_CODE(interp, "HDR_LEN");
	    goto error;
	}
	if (ZipReadInt(start, end, q) != ZIP_CENTRAL_HEADER_SIG) {
	    ZIPFS_ERROR(interp, "wrong header signature");
	    ZIPFS_ERROR_CODE(interp, "HDR_SIG");
	    goto error;
	}
	pathlen = ZipReadShort(start, end, q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = ZipReadShort(start, end, q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = ZipReadShort(start, end, q + ZIP_CENTRAL_EXTRALEN_OFFS);
	localhdr_off = ZipReadInt(start, end, q + ZIP_CENTRAL_LOCALHDR_OFFS);
 	if (ZipReadInt(start, end, zf->data + zf->baseOffset + localhdr_off) != ZIP_LOCAL_HEADER_SIG) {
	    ZIPFS_ERROR(interp, "Failed to find local header");
	    ZIPFS_ERROR_CODE(interp, "LCL_HDR");
	    goto error;
	}
	if (localhdr_off < minoff) {
	    minoff = localhdr_off;
	}
	q += pathlen + comlen + extra + ZIP_CENTRAL_HEADER_LEN;
    }

    zf->passOffset = minoff + zf->baseOffset;

    /*
     * If there's also an encoded password, extract that too (but don't decode
     * yet).
     */

    q = zf->data + zf->passOffset;
    if ((zf->passOffset >= 6) && (start < q-4) &&
	    (ZipReadInt(start, end, q - 4) == ZIP_PASSWORD_END_SIG)) {
	const unsigned char *passPtr;

	i = q[-5];
	passPtr = q - 5 - i;
	if (passPtr >= start && passPtr + i < end) {
	    zf->passBuf[0] = i;
	    memcpy(zf->passBuf + 1, passPtr, i);
	    zf->passOffset -= i ? (5 + i) : 0;
	}
    }

    return TCL_OK;

  error:
    ZipFSCloseArchive(interp, zf);
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSOpenArchive --
 *
 *	This function opens a ZIP archive file for reading. An attempt is made
 *	to memory map that file. Otherwise it is read into an allocated memory
 *	buffer. The ZIP archive header is verified and must be valid for the
 *	function to succeed. When "needZip" is zero an embedded ZIP archive in
 *	an executable file is accepted.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with an error message placed
 *	into the given "interp" if it is not NULL.
 *
 * Side effects:
 *	ZIP archive is memory mapped or read into allocated memory, the given
 *	ZipFile struct is filled with information about the ZIP archive file.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSOpenArchive(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    const char *zipname,	/* Path to ZIP file to open. */
    int needZip,
    ZipFile *zf)
{
    size_t i;
    void *handle;

    zf->nameLength = 0;
    zf->isMemBuffer = 0;
#ifdef _WIN32
    zf->data = NULL;
    zf->mountHandle = INVALID_HANDLE_VALUE;
#else /* !_WIN32 */
    zf->data = (unsigned char *) MAP_FAILED;
#endif /* _WIN32 */
    zf->length = 0;
    zf->numFiles = 0;
    zf->baseOffset = zf->passOffset = 0;
    zf->ptrToFree = NULL;
    zf->passBuf[0] = 0;

    /*
     * Actually open the file.
     */

    zf->chan = Tcl_OpenFileChannel(interp, zipname, "rb", 0);
    if (!zf->chan) {
	return TCL_ERROR;
    }

    /*
     * See if we can get the OS handle. If we can, we can use that to memory
     * map the file, which is nice and efficient. However, it totally depends
     * on the filename pointing to a real regular OS file.
     *
     * Opening real filesystem entities that are not files will lead to an
     * error.
     */

    if (Tcl_GetChannelHandle(zf->chan, TCL_READABLE, &handle) == TCL_OK) {
	if (ZipMapArchive(interp, zf, handle) != TCL_OK) {
	    goto error;
	}
    } else {
	/*
	 * Not an OS file, but rather something in a Tcl VFS. Must copy into
	 * memory.
	 */

	zf->length = Tcl_Seek(zf->chan, 0, SEEK_END);
	if (zf->length == ERROR_LENGTH) {
	    ZIPFS_POSIX_ERROR(interp, "seek error");
	    goto error;
	}
	if ((zf->length - ZIP_CENTRAL_END_LEN)
		> (64 * 1024 * 1024 - ZIP_CENTRAL_END_LEN)) {
	    ZIPFS_ERROR(interp, "illegal file size");
	    ZIPFS_ERROR_CODE(interp, "FILE_SIZE");
	    goto error;
	}
	if (Tcl_Seek(zf->chan, 0, SEEK_SET) == -1) {
	    ZIPFS_POSIX_ERROR(interp, "seek error");
	    goto error;
	}
	zf->ptrToFree = zf->data = (unsigned char *) attemptckalloc(zf->length);
	if (!zf->ptrToFree) {
	    ZIPFS_MEM_ERROR(interp);
	    goto error;
	}
	i = Tcl_Read(zf->chan, (char *) zf->data, zf->length);
	if (i != zf->length) {
	    ZIPFS_POSIX_ERROR(interp, "file read error");
	    goto error;
	}
	Tcl_Close(interp, zf->chan);
	zf->chan = NULL;
    }
    return ZipFSFindTOC(interp, needZip, zf);

    /*
     * Handle errors by closing the archive. This includes closing the channel
     * handle for the archive file.
     */

  error:
    ZipFSCloseArchive(interp, zf);
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipMapArchive --
 *
 *	Wrapper around the platform-specific parts of mmap() (and Windows's
 *	equivalent) because it's not part of the standard channel API.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipMapArchive(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    ZipFile *zf,		/* The archive descriptor structure. */
    void *handle)		/* The OS handle to the open archive. */
{
#ifdef _WIN32
    HANDLE hFile = (HANDLE) handle;
    int readSuccessful;

    /*
     * Determine the file size.
     */

#   ifdef _WIN64
    readSuccessful = GetFileSizeEx(hFile, (PLARGE_INTEGER) &zf->length) != 0;
#   else /* !_WIN64 */
    zf->length = GetFileSize(hFile, 0);
    readSuccessful = (zf->length != (size_t) INVALID_FILE_SIZE);
#   endif /* _WIN64 */
    if (!readSuccessful || (zf->length < ZIP_CENTRAL_END_LEN)) {
	ZIPFS_POSIX_ERROR(interp, "invalid file size");
	return TCL_ERROR;
    }

    /*
     * Map the file.
     */

    zf->mountHandle = CreateFileMappingW(hFile, 0, PAGE_READONLY, 0,
	    zf->length, 0);
    if (zf->mountHandle == INVALID_HANDLE_VALUE) {
	ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	return TCL_ERROR;
    }
    zf->data = (unsigned char *)
	    MapViewOfFile(zf->mountHandle, FILE_MAP_READ, 0, 0, zf->length);
    if (!zf->data) {
	ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	return TCL_ERROR;
    }
#else /* !_WIN32 */
    int fd = PTR2INT(handle);

    /*
     * Determine the file size.
     */

    zf->length = lseek(fd, 0, SEEK_END);
    if (zf->length == ERROR_LENGTH || zf->length < ZIP_CENTRAL_END_LEN) {
	ZIPFS_POSIX_ERROR(interp, "invalid file size");
	return TCL_ERROR;
    }
    lseek(fd, 0, SEEK_SET);

    zf->data = (unsigned char *)
	    mmap(0, zf->length, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if (zf->data == MAP_FAILED) {
	ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	return TCL_ERROR;
    }
#endif /* _WIN32 */
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * IsPasswordValid --
 *
 *	Basic test for whether a passowrd is valid. If the test fails, sets an
 *	error message in the interpreter.
 *
 * Returns:
 *	TCL_OK if the test passes, TCL_ERROR if it fails.
 *
 *-------------------------------------------------------------------------
 */

static inline int
IsPasswordValid(
    Tcl_Interp *interp,
    const char *passwd,
    int pwlen)
{
    if ((pwlen > 255) || strchr(passwd, 0xff)) {
	ZIPFS_ERROR(interp, "illegal password");
	ZIPFS_ERROR_CODE(interp, "BAD_PASS");
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSCatalogFilesystem --
 *
 *	This function generates the root node for a ZIPFS filesystem by
 *	reading the ZIP's central directory.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with an error message placed
 *	into the given "interp" if it is not NULL.
 *
 * Side effects:
 *	Will acquire and release the write lock.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSCatalogFilesystem(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    ZipFile *zf,		/* Temporary buffer hold archive descriptors */
    const char *mountPoint,	/* Mount point path. */
    const char *passwd,		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
    const char *zipname)	/* Path to ZIP file to build a catalog of. */
{
    int pwlen, isNew;
    size_t i;
    ZipFile *zf0;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds, dsm, fpBuf;
    unsigned char *q;

    /*
     * Basic verification of the password for sanity.
     */

    pwlen = 0;
    if (passwd) {
	pwlen = strlen(passwd);
	if (IsPasswordValid(interp, passwd, pwlen) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * Validate the TOC data. If that's bad, things fall apart.
     */

    if (zf->baseOffset >= zf->length || zf->passOffset >= zf->length ||
	    zf->directoryOffset >= zf->length) {
	ZIPFS_ERROR(interp, "bad zip data");
	ZIPFS_ERROR_CODE(interp, "BAD_ZIP");
	ZipFSCloseArchive(interp, zf);
	ckfree(zf);
	return TCL_ERROR;
    }

    WriteLock();

    /*
     * Mount point sometimes is a relative or otherwise denormalized path.
     * But an absolute name is needed as mount point here.
     */

    Tcl_DStringInit(&ds);
    Tcl_DStringInit(&dsm);
    if (strcmp(mountPoint, "/") == 0) {
	mountPoint = "";
    } else {
	mountPoint = CanonicalPath("", mountPoint, &dsm, 1);
    }
    hPtr = Tcl_CreateHashEntry(&ZipFS.zipHash, mountPoint, &isNew);
    if (!isNew) {
	if (interp) {
	    zf0 = (ZipFile *) Tcl_GetHashValue(hPtr);
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "%s is already mounted on %s", zf0->name, mountPoint));
	    ZIPFS_ERROR_CODE(interp, "MOUNTED");
	}
	Unlock();
	ZipFSCloseArchive(interp, zf);
	ckfree(zf);
	return TCL_ERROR;
    }
    Unlock();

    /*
     * Convert to a real archive descriptor.
     */

    zf->mountPoint = (char *) Tcl_GetHashKey(&ZipFS.zipHash, hPtr);
    Tcl_CreateExitHandler(ZipfsMountExitHandler, zf);
    zf->mountPointLen = strlen(zf->mountPoint);

    zf->nameLength = strlen(zipname);
    zf->name = (char *) ckalloc(zf->nameLength + 1);
    memcpy(zf->name, zipname, zf->nameLength + 1);

    Tcl_SetHashValue(hPtr, zf);
    if ((zf->passBuf[0] == 0) && pwlen) {
	int k = 0;

	zf->passBuf[k++] = pwlen;
	for (i = pwlen; i-- > 0 ;) {
	    zf->passBuf[k++] = (passwd[i] & 0x0f)
		    | pwrot[(passwd[i] >> 4) & 0x0f];
	}
	zf->passBuf[k] = '\0';
    }
    if (mountPoint[0] != '\0') {
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, mountPoint, &isNew);
	if (isNew) {
	    z = AllocateZipEntry();
	    Tcl_SetHashValue(hPtr, z);

	    z->depth = CountSlashes(mountPoint);
	    z->zipFilePtr = zf;
	    z->isDirectory = (zf->baseOffset == 0) ? 1 : -1; /* root marker */
	    z->offset = zf->baseOffset;
	    z->compressMethod = ZIP_COMPMETH_STORED;
	    z->name = (char *) Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	    z->next = zf->entries;
	    zf->entries = z;
	}
    }
    q = zf->data + zf->directoryOffset;
    Tcl_DStringInit(&fpBuf);
    for (i = 0; i < zf->numFiles; i++) {
	const unsigned char *start = zf->data;
	const unsigned char *end = zf->data + zf->length;
	int extra, isdir = 0, dosTime, dosDate, nbcompr;
	size_t offs, pathlen, comlen;
	unsigned char *lq, *gq = NULL;
	char *fullpath, *path;

	pathlen = ZipReadShort(start, end, q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = ZipReadShort(start, end, q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = ZipReadShort(start, end, q + ZIP_CENTRAL_EXTRALEN_OFFS);
	path = DecodeZipEntryText(q + ZIP_CENTRAL_HEADER_LEN, pathlen, &ds);
	if ((pathlen > 0) && (path[pathlen - 1] == '/')) {
	    Tcl_DStringSetLength(&ds, pathlen - 1);
	    path = Tcl_DStringValue(&ds);
	    isdir = 1;
	}
	if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
	    goto nextent;
	}
	lq = zf->data + zf->baseOffset
		+ ZipReadInt(start, end, q + ZIP_CENTRAL_LOCALHDR_OFFS);
	if ((lq < start) || (lq + ZIP_LOCAL_HEADER_LEN > end)) {
	    goto nextent;
	}
	nbcompr = ZipReadInt(start, end, lq + ZIP_LOCAL_COMPLEN_OFFS);
	if (!isdir && (nbcompr == 0)
		&& (ZipReadInt(start, end, lq + ZIP_LOCAL_UNCOMPLEN_OFFS) == 0)
		&& (ZipReadInt(start, end, lq + ZIP_LOCAL_CRC32_OFFS) == 0)) {
	    gq = q;
	    nbcompr = ZipReadInt(start, end, gq + ZIP_CENTRAL_COMPLEN_OFFS);
	}
	offs = (lq - zf->data)
		+ ZIP_LOCAL_HEADER_LEN
		+ ZipReadShort(start, end, lq + ZIP_LOCAL_PATHLEN_OFFS)
		+ ZipReadShort(start, end, lq + ZIP_LOCAL_EXTRALEN_OFFS);
	if (offs + nbcompr > zf->length) {
	    goto nextent;
	}

	if (!isdir && (mountPoint[0] == '\0') && !CountSlashes(path)) {
#ifdef ANDROID
	    /*
	     * When mounting the ZIP archive on the root directory try to
	     * remap top level regular files of the archive to
	     * /assets/.root/... since this directory should not be in a valid
	     * APK due to the leading dot in the file name component. This
	     * trick should make the files AndroidManifest.xml,
	     * resources.arsc, and classes.dex visible to Tcl.
	     */
	    Tcl_DString ds2;

	    Tcl_DStringInit(&ds2);
	    Tcl_DStringAppend(&ds2, "assets/.root/", -1);
	    Tcl_DStringAppend(&ds2, path, -1);
	    if (ZipFSLookup(Tcl_DStringValue(&ds2))) {
		/* should not happen but skip it anyway */
		Tcl_DStringFree(&ds2);
		goto nextent;
	    }
	    Tcl_DStringSetLength(&ds, 0);
	    Tcl_DStringAppend(&ds, Tcl_DStringValue(&ds2),
		    Tcl_DStringLength(&ds2));
	    path = Tcl_DStringValue(&ds);
	    Tcl_DStringFree(&ds2);
#else /* !ANDROID */
	    /*
	     * Regular files skipped when mounting on root.
	     */
	    goto nextent;
#endif /* ANDROID */
	}

	Tcl_DStringSetLength(&fpBuf, 0);
	fullpath = CanonicalPath(mountPoint, path, &fpBuf, 1);
	z = AllocateZipEntry();
	z->depth = CountSlashes(fullpath);
	z->zipFilePtr = zf;
	z->isDirectory = isdir;
	z->isEncrypted =
		(ZipReadShort(start, end, lq + ZIP_LOCAL_FLAGS_OFFS) & 1)
		&& (nbcompr > 12);
	z->offset = offs;
	if (gq) {
	    z->crc32 = ZipReadInt(start, end, gq + ZIP_CENTRAL_CRC32_OFFS);
	    dosDate = ZipReadShort(start, end, gq + ZIP_CENTRAL_MDATE_OFFS);
	    dosTime = ZipReadShort(start, end, gq + ZIP_CENTRAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->numBytes = ZipReadInt(start, end,
		    gq + ZIP_CENTRAL_UNCOMPLEN_OFFS);
	    z->compressMethod = ZipReadShort(start, end,
		    gq + ZIP_CENTRAL_COMPMETH_OFFS);
	} else {
	    z->crc32 = ZipReadInt(start, end, lq + ZIP_LOCAL_CRC32_OFFS);
	    dosDate = ZipReadShort(start, end, lq + ZIP_LOCAL_MDATE_OFFS);
	    dosTime = ZipReadShort(start, end, lq + ZIP_LOCAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->numBytes = ZipReadInt(start, end,
		    lq + ZIP_LOCAL_UNCOMPLEN_OFFS);
	    z->compressMethod = ZipReadShort(start, end,
		    lq + ZIP_LOCAL_COMPMETH_OFFS);
	}
	z->numCompressedBytes = nbcompr;
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, fullpath, &isNew);
	if (!isNew) {
	    /* should not happen but skip it anyway */
	    ckfree(z);
	    goto nextent;
	}

	Tcl_SetHashValue(hPtr, z);
	z->name = (char *) Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	z->next = zf->entries;
	zf->entries = z;
	if (isdir && (mountPoint[0] == '\0') && (z->depth == 1)) {
	    z->tnext = zf->topEnts;
	    zf->topEnts = z;
	}

	/*
	 * Make any directory nodes we need. ZIPs are not consistent about
	 * containing directory nodes.
	 */

	if (!z->isDirectory && (z->depth > 1)) {
	    char *dir, *endPtr;
	    ZipEntry *zd;

	    Tcl_DStringSetLength(&ds, strlen(z->name) + 8);
	    Tcl_DStringSetLength(&ds, 0);
	    Tcl_DStringAppend(&ds, z->name, -1);
	    dir = Tcl_DStringValue(&ds);
	    for (endPtr = strrchr(dir, '/'); endPtr && (endPtr != dir);
		    endPtr = strrchr(dir, '/')) {
		Tcl_DStringSetLength(&ds, endPtr - dir);
		hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, dir, &isNew);
		if (!isNew) {
		    /*
		     * Already made. That's fine.
		     */
		    break;
		}

		zd = AllocateZipEntry();
		zd->depth = CountSlashes(dir);
		zd->zipFilePtr = zf;
		zd->isDirectory = 1;
		zd->offset = z->offset;
		zd->timestamp = z->timestamp;
		zd->compressMethod = ZIP_COMPMETH_STORED;
		Tcl_SetHashValue(hPtr, zd);
		zd->name = (char *) Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
		zd->next = zf->entries;
		zf->entries = zd;
		if ((mountPoint[0] == '\0') && (zd->depth == 1)) {
		    zd->tnext = zf->topEnts;
		    zf->topEnts = zd;
		}
	    }
	}
    nextent:
	q += pathlen + comlen + extra + ZIP_CENTRAL_HEADER_LEN;
    }
    Tcl_DStringFree(&fpBuf);
    Tcl_DStringFree(&ds);
    Tcl_FSMountsChanged(NULL);
    Unlock();
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipfsSetup --
 *
 *	Common initialisation code. ZipFS.initialized must *not* be set prior
 *	to the call.
 *
 *-------------------------------------------------------------------------
 */

static void
ZipfsSetup(void)
{
#if TCL_THREADS
    static const Tcl_Time t = { 0, 0 };

    /*
     * Inflate condition variable.
     */

    Tcl_MutexLock(&ZipFSMutex);
    Tcl_ConditionWait(&ZipFSCond, &ZipFSMutex, &t);
    Tcl_MutexUnlock(&ZipFSMutex);
#endif /* TCL_THREADS */

    crc32tab = get_crc_table();
    Tcl_FSRegister(NULL, &zipfsFilesystem);
    Tcl_InitHashTable(&ZipFS.fileHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&ZipFS.zipHash, TCL_STRING_KEYS);
    ZipFS.idCount = 1;
    ZipFS.wrmax = DEFAULT_WRITE_MAX_SIZE;
    ZipFS.fallbackEntryEncoding = (char *)
	    ckalloc(strlen(ZIPFS_FALLBACK_ENCODING) + 1);
    strcpy(ZipFS.fallbackEntryEncoding, ZIPFS_FALLBACK_ENCODING);
    ZipFS.utf8 = Tcl_GetEncoding(NULL, "utf-8");
    ZipFS.initialized = 1;
    Tcl_CreateExitHandler(ZipfsExitHandler, NULL);
}

/*
 *-------------------------------------------------------------------------
 *
 * ListMountPoints --
 *
 *	This procedure lists the mount points and what's mounted there, or
 *	reports whether there are any mounts (if there's no interpreter). The
 *	read lock must be held by the caller.
 *
 * Results:
 *	A standard Tcl result. TCL_OK (or TCL_BREAK if no mounts and no
 *	interpreter).
 *
 * Side effects:
 *	Interpreter result may be updated.
 *
 *-------------------------------------------------------------------------
 */

static inline int
ListMountPoints(
    Tcl_Interp *interp)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    ZipFile *zf;
    Tcl_Obj *resultList;

    if (!interp) {
	/*
	 * Are there any entries in the zipHash? Don't need to enumerate them
	 * all to know.
	 */

	return (ZipFS.zipHash.numEntries ? TCL_OK : TCL_BREAK);
    }

    resultList = Tcl_NewObj();
    for (hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search); hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	zf = (ZipFile *) Tcl_GetHashValue(hPtr);
	Tcl_ListObjAppendElement(NULL, resultList, Tcl_NewStringObj(
		zf->mountPoint, -1));
	Tcl_ListObjAppendElement(NULL, resultList, Tcl_NewStringObj(
		zf->name, -1));
    }
    Tcl_SetObjResult(interp, resultList);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * DescribeMounted --
 *
 *	This procedure describes what is mounted at the given the mount point.
 *	The interpreter result is not updated if there is nothing mounted at
 *	the given point. The read lock must be held by the caller.
 *
 * Results:
 *	A standard Tcl result. TCL_OK (or TCL_BREAK if nothing mounted there
 *	and no interpreter).
 *
 * Side effects:
 *	Interpreter result may be updated.
 *
 *-------------------------------------------------------------------------
 */

static inline int
DescribeMounted(
    Tcl_Interp *interp,
    const char *mountPoint)
{
    if (interp) {
	ZipFile *zf = ZipFSLookupZip(mountPoint);

	if (zf) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(zf->name, -1));
	    return TCL_OK;
	}
    }
    return (interp ? TCL_OK : TCL_BREAK);
}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Mount --
 *
 *	This procedure is invoked to mount a given ZIP archive file on a given
 *	mountpoint with optional ZIP password.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A ZIP archive file is read, analyzed and mounted, resources are
 *	allocated.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_Mount(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    const char *mountPoint,	/* Mount point path. */
    const char *zipname,	/* Path to ZIP file to mount; should be
				 * normalized. */
    const char *passwd)		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
{
    ZipFile *zf;

    ReadLock();
    if (!ZipFS.initialized) {
	ZipfsSetup();
    }

    /*
     * No mount point, so list all mount points and what is mounted there.
     */

    if (!mountPoint) {
	int ret = ListMountPoints(interp);
	Unlock();
	return ret;
    }

    /*
     * Mount point but no file, so describe what is mounted at that mount
     * point.
     */

    if (!zipname) {
	DescribeMounted(interp, mountPoint);
	Unlock();
	return TCL_OK;
    }
    Unlock();

    /*
     * Have both a mount point and a file (name) to mount there.
     */

    if (passwd && IsPasswordValid(interp, passwd, strlen(passwd)) != TCL_OK) {
	return TCL_ERROR;
    }
    zf = AllocateZipFile(interp, strlen(mountPoint));
    if (!zf) {
	return TCL_ERROR;
    }
    if (ZipFSOpenArchive(interp, zipname, 1, zf) != TCL_OK) {
	ckfree(zf);
	return TCL_ERROR;
    }
    if (ZipFSCatalogFilesystem(interp, zf, mountPoint, passwd, zipname)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_MountBuffer --
 *
 *	This procedure is invoked to mount a given ZIP archive file on a given
 *	mountpoint with optional ZIP password.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A ZIP archive file is read, analyzed and mounted, resources are
 *	allocated.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_MountBuffer(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    const char *mountPoint,	/* Mount point path. */
    unsigned char *data,
    size_t datalen,
    int copy)
{
    ZipFile *zf;
    int result;

    ReadLock();
    if (!ZipFS.initialized) {
	ZipfsSetup();
    }

    /*
     * No mount point, so list all mount points and what is mounted there.
     */

    if (!mountPoint) {
	int ret = ListMountPoints(interp);
	Unlock();
	return ret;
    }

    /*
     * Mount point but no data, so describe what is mounted at that mount
     * point.
     */

    if (!data) {
	DescribeMounted(interp, mountPoint);
	Unlock();
	return TCL_OK;
    }
    Unlock();

    /*
     * Have both a mount point and data to mount there.
     */

    zf = AllocateZipFile(interp, strlen(mountPoint));
    if (!zf) {
	return TCL_ERROR;
    }
    zf->isMemBuffer = 1;
    zf->length = datalen;
    if (copy) {
	zf->data = (unsigned char *) attemptckalloc(datalen);
	if (!zf->data) {
	    ZIPFS_MEM_ERROR(interp);
	    return TCL_ERROR;
	}
	memcpy(zf->data, data, datalen);
	zf->ptrToFree = zf->data;
    } else {
	zf->data = data;
	zf->ptrToFree = NULL;
    }
    if (ZipFSFindTOC(interp, 0, zf) != TCL_OK) {
	return TCL_ERROR;
    }
    result = ZipFSCatalogFilesystem(interp, zf, mountPoint, NULL,
	    "Memory Buffer");
    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Unmount --
 *
 *	This procedure is invoked to unmount a given ZIP archive.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A mounted ZIP archive file is unmounted, resources are free'd.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_Unmount(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    const char *mountPoint)	/* Mount point path. */
{
    ZipFile *zf;
    ZipEntry *z, *znext;
    Tcl_HashEntry *hPtr;
    Tcl_DString dsm;
    int ret = TCL_OK, unmounted = 0;

    WriteLock();
    if (!ZipFS.initialized) {
	goto done;
    }

    /*
     * Mount point sometimes is a relative or otherwise denormalized path.
     * But an absolute name is needed as mount point here.
     */

    Tcl_DStringInit(&dsm);
    mountPoint = CanonicalPath("", mountPoint, &dsm, 1);

    hPtr = Tcl_FindHashEntry(&ZipFS.zipHash, mountPoint);
    /* don't report no-such-mount as an error */
    if (!hPtr) {
	goto done;
    }

    zf = (ZipFile *) Tcl_GetHashValue(hPtr);
    if (zf->numOpen > 0) {
	ZIPFS_ERROR(interp, "filesystem is busy");
	ZIPFS_ERROR_CODE(interp, "BUSY");
	ret = TCL_ERROR;
	goto done;
    }
    Tcl_DeleteHashEntry(hPtr);

    /*
     * Now no longer mounted - the rest of the code won't find it - but we're
     * still cleaning things up.
     */

    for (z = zf->entries; z; z = znext) {
	znext = z->next;
	hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, z->name);
	if (hPtr) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	if (z->data) {
	    ckfree(z->data);
	}
	ckfree(z);
    }
    ZipFSCloseArchive(interp, zf);
    Tcl_DeleteExitHandler(ZipfsMountExitHandler, zf);
    ckfree(zf);
    unmounted = 1;

  done:
    Unlock();
    if (unmounted) {
	Tcl_FSMountsChanged(NULL);
    }
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMountObjCmd --
 *
 *	This procedure is invoked to process the [zipfs mount] command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A ZIP archive file is mounted, resources are allocated.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMountObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    const char *mountPoint = NULL, *zipFile = NULL, *password = NULL;
    Tcl_Obj *zipFileObj = NULL;
    int result;

    if (objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		 "?mountpoint? ?zipfile? ?password?");
	return TCL_ERROR;
    }
    if (objc > 1) {
	mountPoint = Tcl_GetString(objv[1]);
    }
    if (objc > 2) {
	zipFileObj = Tcl_FSGetNormalizedPath(interp, objv[2]);
	if (!zipFileObj) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "could not normalize zip filename", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "NORMALIZE", NULL);
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(zipFileObj);
	zipFile = Tcl_GetString(zipFileObj);
    }
    if (objc > 3) {
	password = Tcl_GetString(objv[3]);
    }

    result = TclZipfs_Mount(interp, mountPoint, zipFile, password);
    if (zipFileObj != NULL) {
	Tcl_DecrRefCount(zipFileObj);
    }
    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMountBufferObjCmd --
 *
 *	This procedure is invoked to process the [zipfs mount_data] command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A ZIP archive file is mounted, resources are allocated.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMountBufferObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    const char *mountPoint;	/* Mount point path. */
    unsigned char *data;
    int length;

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?mountpoint? ?data?");
	return TCL_ERROR;
    }
    if (objc < 2) {
	int ret;

	ReadLock();
	ret = ListMountPoints(interp);
	Unlock();
	return ret;
    }

    mountPoint = Tcl_GetString(objv[1]);
    if (objc < 3) {
	ReadLock();
	DescribeMounted(interp, mountPoint);
	Unlock();
	return TCL_OK;
    }

    data = TclGetBytesFromObj(interp, objv[2], &length);
    if (data == NULL) {
	return TCL_ERROR;
    }
    return TclZipfs_MountBuffer(interp, mountPoint, data, length, 1);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSRootObjCmd --
 *
 *	This procedure is invoked to process the [zipfs root] command. It
 *	returns the root that all zipfs file systems are mounted under.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSRootObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *)) /*objv*/
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj(ZIPFS_VOLUME, -1));
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSUnmountObjCmd --
 *
 *	This procedure is invoked to process the [zipfs unmount] command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A mounted ZIP archive file is unmounted, resources are free'd.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSUnmountObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "zipfile");
	return TCL_ERROR;
    }
    return TclZipfs_Unmount(interp, Tcl_GetString(objv[1]));
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkKeyObjCmd --
 *
 *	This procedure is invoked to process the [zipfs mkkey] command.  It
 *	produces a rotated password to be embedded into an image file.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkKeyObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int len, i = 0;
    const char *pw;
    Tcl_Obj *passObj;
    unsigned char *passBuf;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "password");
	return TCL_ERROR;
    }
    pw = Tcl_GetStringFromObj(objv[1], &len);
    if (len == 0) {
	return TCL_OK;
    }
    if (IsPasswordValid(interp, pw, len) != TCL_OK) {
	return TCL_ERROR;
    }

    passObj = Tcl_NewByteArrayObj(NULL, 264);
    passBuf = Tcl_GetByteArrayFromObj(passObj, (int *)NULL);
    while (len > 0) {
	int ch = pw[len - 1];

	passBuf[i++] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	len--;
    }
    passBuf[i] = i;
    i++;
    ZipWriteInt(passBuf, passBuf + 264, passBuf + i, ZIP_PASSWORD_END_SIG);
    Tcl_SetByteArrayLength(passObj, i + 4);
    Tcl_SetObjResult(interp, passObj);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * RandomChar --
 *
 *	Worker for ZipAddFile().  Picks a random character (range: 0..255)
 *	using Tcl's standard PRNG.
 *
 * Returns:
 *	Tcl result code. Updates chPtr with random character on success.
 *
 * Side effects:
 *	Advances the PRNG state. May reenter the Tcl interpreter if the user
 *	has replaced the PRNG.
 *
 *-------------------------------------------------------------------------
 */

static int
RandomChar(
    Tcl_Interp *interp,
    int step,
    int *chPtr)
{
    double r;
    Tcl_Obj *ret;

    if (Tcl_EvalEx(interp, "::tcl::mathfunc::rand", -1, 0) != TCL_OK) {
	goto failed;
    }
    ret = Tcl_GetObjResult(interp);
    if (Tcl_GetDoubleFromObj(interp, ret, &r) != TCL_OK) {
	goto failed;
    }
    *chPtr = (int) (r * 256);
    return TCL_OK;

  failed:
    Tcl_AppendObjToErrorInfo(interp, Tcl_ObjPrintf(
	    "\n    (evaluating PRNG step %d for password encoding)",
	    step));
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipAddFile --
 *
 *	This procedure is used by ZipFSMkZipOrImg() to add a single file to
 *	the output ZIP archive file being written. A ZipEntry struct about the
 *	input file is added to the given fileHash table for later creation of
 *	the central ZIP directory.
 *
 *	Tcl *always* encodes filenames in the ZIP as UTF-8. Similarly, it
 *	would always encode comments as UTF-8, if it supported comments.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Input file is read and (compressed and) written to the output ZIP
 *	archive file.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipAddFile(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *pathObj,		/* Actual name of the file to add. */
    const char *name,		/* Name to use in the ZIP archive, in Tcl's
				 * internal encoding. */
    Tcl_Channel out,		/* The open ZIP archive being built. */
    const char *passwd,		/* Password for encoding the file, or NULL if
				 * the file is to be unprotected. */
    char *buf,			/* Working buffer. */
    int bufsize,		/* Size of buf */
    Tcl_HashTable *fileHash)	/* Where to record ZIP entry metdata so we can
				 * built the central directory. */
{
    const unsigned char *start = (unsigned char *) buf;
    const unsigned char *end = (unsigned char *) buf + bufsize;
    Tcl_Channel in;
    Tcl_HashEntry *hPtr;
    ZipEntry *z;
    z_stream stream;
    Tcl_DString zpathDs;	/* Buffer for the encoded filename. */
    const char *zpathExt;	/* Filename in external encoding (true
				 * UTF-8). */
    const char *zpathTcl;	/* Filename in Tcl's internal encoding. */
    int crc, flush, zpathlen;
    size_t nbyte, nbytecompr, len, olen, align = 0;
    long long headerStartOffset, dataStartOffset, dataEndOffset;
    int mtime = 0, isNew, compMeth;
    unsigned long keys[3], keys0[3];
    char obuf[4096];

    /*
     * Trim leading '/' characters. If this results in an empty string, we've
     * nothing to do.
     */

    zpathTcl = name;
    while (zpathTcl && zpathTcl[0] == '/') {
	zpathTcl++;
    }
    if (!zpathTcl || (zpathTcl[0] == '\0')) {
	return TCL_OK;
    }

    /*
     * Convert to encoded form. Note that we use strlen() here; if someone's
     * crazy enough to embed NULs in filenames, they deserve what they get!
     */

    zpathExt = Tcl_UtfToExternalDString(ZipFS.utf8, zpathTcl, -1, &zpathDs);
    zpathlen = strlen(zpathExt);
    if (zpathlen + ZIP_CENTRAL_HEADER_LEN > bufsize) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"path too long for \"%s\"", Tcl_GetString(pathObj)));
	ZIPFS_ERROR_CODE(interp, "PATH_LEN");
	Tcl_DStringFree(&zpathDs);
	return TCL_ERROR;
    }
    in = Tcl_FSOpenFileChannel(interp, pathObj, "rb", 0);
    if (!in) {
	Tcl_DStringFree(&zpathDs);
#ifdef _WIN32
	/* hopefully a directory */
	if (strcmp("Permission denied", Tcl_PosixError(interp)) == 0) {
	    Tcl_Close(interp, in);
	    return TCL_OK;
	}
#endif /* _WIN32 */
	Tcl_Close(interp, in);
	return TCL_ERROR;
    } else {
	Tcl_StatBuf statBuf;

	if (Tcl_FSStat(pathObj, &statBuf) != -1) {
	    mtime = statBuf.st_mtime;
	}
    }
    Tcl_ResetResult(interp);

    /*
     * Compute the CRC.
     */

    crc = 0;
    nbyte = nbytecompr = 0;
    while (1) {
	len = Tcl_Read(in, buf, bufsize);
	if (len == ERROR_LENGTH) {
	    Tcl_DStringFree(&zpathDs);
	    if (nbyte == 0 && errno == EISDIR) {
		Tcl_Close(interp, in);
		return TCL_OK;
	    }
	readErrorWithChannelOpen:
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("read error on \"%s\": %s",
		    Tcl_GetString(pathObj), Tcl_PosixError(interp)));
	    Tcl_Close(interp, in);
	    return TCL_ERROR;
	}
	if (len == 0) {
	    break;
	}
	crc = crc32(crc, (unsigned char *) buf, len);
	nbyte += len;
    }
    if (Tcl_Seek(in, 0, SEEK_SET) == -1) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("seek error on \"%s\": %s",
		Tcl_GetString(pathObj), Tcl_PosixError(interp)));
	Tcl_Close(interp, in);
	Tcl_DStringFree(&zpathDs);
	return TCL_ERROR;
    }

    /*
     * Remember where we've got to so far so we can write the header (after
     * writing the file).
     */

    headerStartOffset = Tcl_Tell(out);

    /*
     * Reserve space for the per-file header. Includes writing the file name
     * as we already know that.
     */

    memset(buf, '\0', ZIP_LOCAL_HEADER_LEN);
    memcpy(buf + ZIP_LOCAL_HEADER_LEN, zpathExt, zpathlen);
    len = zpathlen + ZIP_LOCAL_HEADER_LEN;
    if ((size_t) Tcl_Write(out, buf, len) != len) {
    writeErrorWithChannelOpen:
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"write error on \"%s\": %s",
		Tcl_GetString(pathObj), Tcl_PosixError(interp)));
	Tcl_Close(interp, in);
	Tcl_DStringFree(&zpathDs);
	return TCL_ERROR;
    }

    /*
     * Align payload to next 4-byte boundary (if necessary) using a dummy
     * extra entry similar to the zipalign tool from Android's SDK.
     */

    if ((len + headerStartOffset) & 3) {
	unsigned char abuf[8];
	const unsigned char *astart = abuf;
	const unsigned char *aend = abuf + 8;

	align = 4 + ((len + headerStartOffset) & 3);
	ZipWriteShort(astart, aend, abuf, 0xffff);
	ZipWriteShort(astart, aend, abuf + 2, align - 4);
	ZipWriteInt(astart, aend, abuf + 4, 0x03020100);
	if ((size_t) Tcl_Write(out, (const char *) abuf, align) != align) {
	    goto writeErrorWithChannelOpen;
	}
    }

    /*
     * Set up encryption if we were asked to.
     */

    if (passwd) {
	int i, ch, tmp;
	unsigned char kvbuf[24];

	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    if (RandomChar(interp, i, &ch) != TCL_OK) {
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    kvbuf[i + 12] = UCHAR(zencode(keys, crc32tab, ch, tmp));
	}
	Tcl_ResetResult(interp);
	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    kvbuf[i] = UCHAR(zencode(keys, crc32tab, kvbuf[i + 12], tmp));
	}
	kvbuf[i++] = UCHAR(zencode(keys, crc32tab, crc >> 16, tmp));
	kvbuf[i++] = UCHAR(zencode(keys, crc32tab, crc >> 24, tmp));
	len = Tcl_Write(out, (char *) kvbuf, 12);
	memset(kvbuf, 0, 24);
	if (len != 12) {
	    goto writeErrorWithChannelOpen;
	}
	memcpy(keys0, keys, sizeof(keys0));
	nbytecompr += 12;
    }

    /*
     * Save where we've got to in case we need to just store this file.
     */

    Tcl_Flush(out);
    dataStartOffset = Tcl_Tell(out);

    /*
     * Compress the stream.
     */

    compMeth = ZIP_COMPMETH_DEFLATED;
    memset(&stream, 0, sizeof(z_stream));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if (deflateInit2(&stream, 9, Z_DEFLATED, -15, 8,
	    Z_DEFAULT_STRATEGY) != Z_OK) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"compression init error on \"%s\"", Tcl_GetString(pathObj)));
	ZIPFS_ERROR_CODE(interp, "DEFLATE_INIT");
	Tcl_Close(interp, in);
	Tcl_DStringFree(&zpathDs);
	return TCL_ERROR;
    }

    do {
	len = Tcl_Read(in, buf, bufsize);
	if (len == ERROR_LENGTH) {
	    deflateEnd(&stream);
	    goto readErrorWithChannelOpen;
	}
	stream.avail_in = len;
	stream.next_in = (unsigned char *) buf;
	flush = Tcl_Eof(in) ? Z_FINISH : Z_NO_FLUSH;
	do {
	    stream.avail_out = sizeof(obuf);
	    stream.next_out = (unsigned char *) obuf;
	    len = deflate(&stream, flush);
	    if (len == (size_t) Z_STREAM_ERROR) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"deflate error on \"%s\"", Tcl_GetString(pathObj)));
		ZIPFS_ERROR_CODE(interp, "DEFLATE");
		deflateEnd(&stream);
		Tcl_Close(interp, in);
		Tcl_DStringFree(&zpathDs);
		return TCL_ERROR;
	    }
	    olen = sizeof(obuf) - stream.avail_out;
	    if (passwd) {
		size_t i;
		int tmp;

		for (i = 0; i < olen; i++) {
		    obuf[i] = (char) zencode(keys, crc32tab, obuf[i], tmp);
		}
	    }
	    if (olen && ((size_t) Tcl_Write(out, obuf, olen) != olen)) {
		deflateEnd(&stream);
		goto writeErrorWithChannelOpen;
	    }
	    nbytecompr += olen;
	} while (stream.avail_out == 0);
    } while (flush != Z_FINISH);
    deflateEnd(&stream);

    /*
     * Work out where we've got to.
     */

    Tcl_Flush(out);
    dataEndOffset = Tcl_Tell(out);

    if (nbyte - nbytecompr <= 0) {
	/*
	 * Compressed file larger than input, write it again uncompressed.
	 */

	if (Tcl_Seek(in, 0, SEEK_SET) != 0) {
	    goto seekErr;
	}
	if (Tcl_Seek(out, dataStartOffset, SEEK_SET) != dataStartOffset) {
	seekErr:
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "seek error: %s", Tcl_PosixError(interp)));
	    Tcl_Close(interp, in);
	    Tcl_DStringFree(&zpathDs);
	    return TCL_ERROR;
	}
	nbytecompr = (passwd ? 12 : 0);
	while (1) {
	    len = Tcl_Read(in, buf, bufsize);
	    if (len == ERROR_LENGTH) {
		goto readErrorWithChannelOpen;
	    } else if (len == 0) {
		break;
	    }
	    if (passwd) {
		size_t i;
		int tmp;

		for (i = 0; i < len; i++) {
		    buf[i] = (char) zencode(keys0, crc32tab, buf[i], tmp);
		}
	    }
	    if ((size_t) Tcl_Write(out, buf, len) != len) {
		goto writeErrorWithChannelOpen;
	    }
	    nbytecompr += len;
	}
	compMeth = ZIP_COMPMETH_STORED;

	/*
	 * Chop off everything after this; it's the over-large compressed data
	 * and we don't know if it is going to get overwritten otherwise.
	 */

	Tcl_Flush(out);
	dataEndOffset = Tcl_Tell(out);
	Tcl_TruncateChannel(out, dataEndOffset);
    }
    Tcl_Close(interp, in);
    Tcl_DStringFree(&zpathDs);
    zpathExt = NULL;

    hPtr = Tcl_CreateHashEntry(fileHash, zpathTcl, &isNew);
    if (!isNew) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"non-unique path name \"%s\"", Tcl_GetString(pathObj)));
	ZIPFS_ERROR_CODE(interp, "DUPLICATE_PATH");
	return TCL_ERROR;
    }

    /*
     * Remember that we've written the file (for central directory generation)
     * and generate the local (per-file) header in the space that we reserved
     * earlier.
     */

    z = AllocateZipEntry();
    Tcl_SetHashValue(hPtr, z);
    z->isEncrypted = (passwd ? 1 : 0);
    z->offset = headerStartOffset;
    z->crc32 = crc;
    z->timestamp = mtime;
    z->numBytes = nbyte;
    z->numCompressedBytes = nbytecompr;
    z->compressMethod = compMeth;
    z->name = (char *) Tcl_GetHashKey(fileHash, hPtr);

    /*
     * Write final local header information.
     */

    SerializeLocalEntryHeader(start, end, (unsigned char *) buf, z,
	    zpathlen, align);
    if (Tcl_Seek(out, headerStartOffset, SEEK_SET) != headerStartOffset) {
	Tcl_DeleteHashEntry(hPtr);
	ckfree(z);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"seek error: %s", Tcl_PosixError(interp)));
	return TCL_ERROR;
    }
    if (Tcl_Write(out, buf, ZIP_LOCAL_HEADER_LEN) != ZIP_LOCAL_HEADER_LEN) {
	Tcl_DeleteHashEntry(hPtr);
	ckfree(z);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"write error: %s", Tcl_PosixError(interp)));
	return TCL_ERROR;
    }
    Tcl_Flush(out);
    if (Tcl_Seek(out, dataEndOffset, SEEK_SET) != dataEndOffset) {
	Tcl_DeleteHashEntry(hPtr);
	ckfree(z);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"seek error: %s", Tcl_PosixError(interp)));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFind --
 *
 *	Worker for ZipFSMkZipOrImg() that discovers the list of files to add.
 *	Simple wrapper around [zipfs find].
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
ZipFSFind(
    Tcl_Interp *interp,
    Tcl_Obj *dirRoot)
{
    Tcl_Obj *cmd[2];
    int result;

    cmd[0] = Tcl_NewStringObj("::tcl::zipfs::find", -1);
    cmd[1] = dirRoot;
    Tcl_IncrRefCount(cmd[0]);
    result = Tcl_EvalObjv(interp, 2, cmd, 0);
    Tcl_DecrRefCount(cmd[0]);
    if (result != TCL_OK) {
	return NULL;
    }
    return Tcl_GetObjResult(interp);
}

/*
 *-------------------------------------------------------------------------
 *
 * ComputeNameInArchive --
 *
 *	Helper for ZipFSMkZipOrImg() that computes what the actual name of a
 *	file in the ZIP archive should be, stripping a prefix (if appropriate)
 *	and any leading slashes. If the result is an empty string, the entry
 *	should be skipped.
 *
 * Returns:
 *	Pointer to the name (in Tcl's internal encoding), which will be in
 *	memory owned by one of the argument objects.
 *
 * Side effects:
 *	None (if Tcl_Objs have string representations)
 *
 *-------------------------------------------------------------------------
 */

static inline const char *
ComputeNameInArchive(
    Tcl_Obj *pathObj,		/* The path to the origin file */
    Tcl_Obj *directNameObj,	/* User-specified name for use in the ZIP
				 * archive */
    const char *strip,		/* A prefix to strip; may be NULL if no
				 * stripping need be done. */
    int slen)			/* The length of the prefix; must be 0 if no
				 * stripping need be done. */
{
    const char *name;
    int len;

    if (directNameObj) {
	name = Tcl_GetString(directNameObj);
    } else {
	name = Tcl_GetStringFromObj(pathObj, &len);
	if (slen > 0) {
	    if ((len <= slen) || (strncmp(strip, name, slen) != 0)) {
		/*
		 * Guaranteed to be a NUL at the end, which will make this
		 * entry be skipped.
		 */

		return name + len;
	    }
	    name += slen;
	}
    }
    while (name[0] == '/') {
	++name;
    }
    return name;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkZipOrImg --
 *
 *	This procedure is creates a new ZIP archive file or image file given
 *	output filename, input directory of files to be archived, optional
 *	password, and optional image to be prepended to the output ZIP archive
 *	file. It's the core of the implementation of [zipfs mkzip], [zipfs
 *	mkimg], [zipfs lmkzip] and [zipfs lmkimg].
 *
 *	Tcl *always* encodes filenames in the ZIP as UTF-8. Similarly, it
 *	would always encode comments as UTF-8, if it supported comments.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new ZIP archive file or image file is written.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkZipOrImg(
    Tcl_Interp *interp,		/* Current interpreter. */
    int isImg,			/* Are we making an image? */
    Tcl_Obj *targetFile,	/* What file are we making? */
    Tcl_Obj *dirRoot,		/* What directory do we take files from? Do
				 * not specify at the same time as
				 * mappingList (one must be NULL). */
    Tcl_Obj *mappingList,	/* What files are we putting in, and with what
				 * names? Do not specify at the same time as
				 * dirRoot (one must be NULL). */
    Tcl_Obj *originFile,	/* If we're making an image, what file does
				 * the non-ZIP part of the image come from? */
    Tcl_Obj *stripPrefix,	/* Are we going to strip a prefix from
				 * filenames found beneath dirRoot? If NULL,
				 * do not strip anything (except for dirRoot
				 * itself). */
    Tcl_Obj *passwordObj)	/* The password for encoding things. NULL if
				 * there's no password protection. */
{
    Tcl_Channel out;
    int pwlen = 0, slen = 0, count, ret = TCL_ERROR, lobjc;
    size_t len, i = 0;
    long long directoryStartOffset;
				/* The overall file offset of the start of the
				 * central directory. */
    long long suffixStartOffset;/* The overall file offset of the start of the
				 * suffix of the central directory (i.e.,
				 * where this data will be written). */
    Tcl_Obj **lobjv, *list = mappingList;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable fileHash;
    char *strip = NULL, *pw = NULL, passBuf[264], buf[4096];
    unsigned char *start = (unsigned char *) buf;
    unsigned char *end = start + sizeof(buf);

    /*
     * Caller has verified that the number of arguments is correct.
     */

    passBuf[0] = 0;
    if (passwordObj != NULL) {
	pw = Tcl_GetStringFromObj(passwordObj, &pwlen);
	if (IsPasswordValid(interp, pw, pwlen) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (pwlen <= 0) {
	    pw = NULL;
	    pwlen = 0;
	}
    }
    if (dirRoot != NULL) {
	list = ZipFSFind(interp, dirRoot);
	if (!list) {
	    return TCL_ERROR;
	}
    }
    Tcl_IncrRefCount(list);
    if (TclListObjLengthM(interp, list, &lobjc) != TCL_OK) {
	Tcl_DecrRefCount(list);
	return TCL_ERROR;
    }
    if (mappingList && (lobjc % 2)) {
	Tcl_DecrRefCount(list);
	ZIPFS_ERROR(interp, "need even number of elements");
	ZIPFS_ERROR_CODE(interp, "LIST_LENGTH");
	return TCL_ERROR;
    }
    if (lobjc == 0) {
	Tcl_DecrRefCount(list);
	ZIPFS_ERROR(interp, "empty archive");
	ZIPFS_ERROR_CODE(interp, "EMPTY");
	return TCL_ERROR;
    }
    if (TclListObjGetElementsM(interp, list, &lobjc, &lobjv) != TCL_OK) {
	Tcl_DecrRefCount(list);
	return TCL_ERROR;
    }
    out = Tcl_FSOpenFileChannel(interp, targetFile, "wb", 0755);
    if (out == NULL) {
	Tcl_DecrRefCount(list);
	return TCL_ERROR;
    }

    /*
     * Copy the existing contents from the image if it is an executable image.
     * Care must be taken because this might include an existing ZIP, which
     * needs to be stripped.
     */

    if (isImg) {
	ZipFile *zf, zf0;
	int isMounted = 0;
	const char *imgName;

	// TODO: normalize the origin file name
	imgName = (originFile != NULL) ? Tcl_GetString(originFile) :
		Tcl_GetNameOfExecutable();
	if (pwlen) {
	    i = 0;
	    for (len = pwlen; len-- > 0;) {
		int ch = pw[len];

		passBuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
		i++;
	    }
	    passBuf[i] = i;
	    ++i;
	    passBuf[i++] = (char) ZIP_PASSWORD_END_SIG;
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 8);
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 16);
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 24);
	    passBuf[i] = '\0';
	}

	/*
	 * Check for mounted image.
	 */

	WriteLock();
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search); hPtr;
		hPtr = Tcl_NextHashEntry(&search)) {
	    zf = (ZipFile *) Tcl_GetHashValue(hPtr);
	    if (strcmp(zf->name, imgName) == 0) {
		isMounted = 1;
		zf->numOpen++;
		break;
	    }
	}
	Unlock();

	if (!isMounted) {
	    zf = &zf0;
	    memset(&zf0, 0, sizeof(ZipFile));
	}
	if (isMounted || ZipFSOpenArchive(interp, imgName, 0, zf) == TCL_OK) {
	    /*
	     * Copy everything up to the ZIP-related suffix.
	     */

	    if ((size_t) Tcl_Write(out, (char *) zf->data,
		    zf->passOffset) != zf->passOffset) {
		memset(passBuf, 0, sizeof(passBuf));
		Tcl_DecrRefCount(list);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"write error: %s", Tcl_PosixError(interp)));
		Tcl_Close(interp, out);
		if (zf == &zf0) {
		    ZipFSCloseArchive(interp, zf);
		} else {
		    WriteLock();
		    zf->numOpen--;
		    Unlock();
		}
		return TCL_ERROR;
	    }
	    if (zf == &zf0) {
		ZipFSCloseArchive(interp, zf);
	    } else {
		WriteLock();
		zf->numOpen--;
		Unlock();
	    }
	} else {
	    /*
	     * Fall back to read it as plain file which hopefully is a static
	     * tclsh or wish binary with proper zipfs infrastructure built in.
	     */

	    if (CopyImageFile(interp, imgName, out) != TCL_OK) {
		memset(passBuf, 0, sizeof(passBuf));
		Tcl_DecrRefCount(list);
		Tcl_Close(interp, out);
		return TCL_ERROR;
	    }
	}

	/*
	 * Store the password so that the automounter can find it.
	 */

	len = strlen(passBuf);
	if (len > 0) {
	    i = Tcl_Write(out, passBuf, len);
	    if (i != len) {
		Tcl_DecrRefCount(list);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"write error: %s", Tcl_PosixError(interp)));
		Tcl_Close(interp, out);
		return TCL_ERROR;
	    }
	}
	memset(passBuf, 0, sizeof(passBuf));
	Tcl_Flush(out);
    }

    /*
     * Prepare the contents of the ZIP archive.
     */

    Tcl_InitHashTable(&fileHash, TCL_STRING_KEYS);
    if (mappingList == NULL && stripPrefix != NULL) {
	strip = Tcl_GetStringFromObj(stripPrefix, &slen);
	if (!slen) {
	    strip = NULL;
	}
    }
    for (i = 0; i < (size_t) lobjc; i += (mappingList ? 2 : 1)) {
	Tcl_Obj *pathObj = lobjv[i];
	const char *name = ComputeNameInArchive(pathObj,
		(mappingList ? lobjv[i + 1] : NULL), strip, slen);

	if (name[0] == '\0') {
	    continue;
	}
	if (ZipAddFile(interp, pathObj, name, out, pw, buf, sizeof(buf),
		&fileHash) != TCL_OK) {
	    goto done;
	}
    }

    /*
     * Construct the contents of the ZIP central directory.
     */

    directoryStartOffset = Tcl_Tell(out);
    count = 0;
    for (i = 0; i < (size_t) lobjc; i += (mappingList ? 2 : 1)) {
	const char *name = ComputeNameInArchive(lobjv[i],
		(mappingList ? lobjv[i + 1] : NULL), strip, slen);
	Tcl_DString ds;

	hPtr = Tcl_FindHashEntry(&fileHash, name);
	if (!hPtr) {
	    continue;
	}
	z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	name = Tcl_UtfToExternalDString(ZipFS.utf8, z->name, -1, &ds);
	len = Tcl_DStringLength(&ds);
	SerializeCentralDirectoryEntry(start, end, (unsigned char *) buf,
		z, len);
	if ((Tcl_Write(out, buf, ZIP_CENTRAL_HEADER_LEN)
		!= ZIP_CENTRAL_HEADER_LEN)
		|| ((size_t) Tcl_Write(out, name, len) != len)) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "write error: %s", Tcl_PosixError(interp)));
	    Tcl_DStringFree(&ds);
	    goto done;
	}
	Tcl_DStringFree(&ds);
	count++;
    }

    /*
     * Finalize the central directory.
     */

    Tcl_Flush(out);
    suffixStartOffset = Tcl_Tell(out);
    SerializeCentralDirectorySuffix(start, end, (unsigned char *) buf,
	    count, directoryStartOffset, suffixStartOffset);
    if (Tcl_Write(out, buf, ZIP_CENTRAL_END_LEN) != ZIP_CENTRAL_END_LEN) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"write error: %s", Tcl_PosixError(interp)));
	goto done;
    }
    Tcl_Flush(out);
    ret = TCL_OK;

  done:
    if (ret == TCL_OK) {
	ret = Tcl_Close(interp, out);
    } else {
	Tcl_Close(interp, out);
    }
    Tcl_DecrRefCount(list);
    for (hPtr = Tcl_FirstHashEntry(&fileHash, &search); hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	z = (ZipEntry *) Tcl_GetHashValue(hPtr);
	ckfree(z);
	Tcl_DeleteHashEntry(hPtr);
    }
    Tcl_DeleteHashTable(&fileHash);
    return ret;
}

/*
 * ---------------------------------------------------------------------
 *
 * CopyImageFile --
 *
 *	A simple file copy function that is used (by ZipFSMkZipOrImg) for
 *	anything that is not an image with a ZIP appended.
 *
 * Returns:
 *	A Tcl result code.
 *
 * Side effects:
 *	Writes to an output channel.
 *
 * ---------------------------------------------------------------------
 */

static int
CopyImageFile(
    Tcl_Interp *interp,		/* For error reporting. */
    const char *imgName,	/* Where to copy from. */
    Tcl_Channel out)		/* Where to copy to; already open for writing
				 * binary data. */
{
    size_t i, k;
    int m, n;
    Tcl_Channel in;
    char buf[4096];
    const char *errMsg;

    Tcl_ResetResult(interp);
    in = Tcl_OpenFileChannel(interp, imgName, "rb", 0644);
    if (!in) {
	return TCL_ERROR;
    }

    /*
     * Get the length of the file (and exclude non-files).
     */

    i = Tcl_Seek(in, 0, SEEK_END);
    if (i == ERROR_LENGTH) {
	errMsg = "seek error";
	goto copyError;
    }
    Tcl_Seek(in, 0, SEEK_SET);

    /*
     * Copy the whole file, 8 blocks at a time (reasonably efficient). Note
     * that this totally ignores things like Windows's Alternate File Streams.
     */

    for (k = 0; k < i; k += m) {
	m = i - k;
	if (m > (int) sizeof(buf)) {
	    m = (int) sizeof(buf);
	}
	n = Tcl_Read(in, buf, m);
	if (n == -1) {
	    errMsg = "read error";
	    goto copyError;
	} else if (n == 0) {
	    break;
	}
	m = Tcl_Write(out, buf, n);
	if (m != n) {
	    errMsg = "write error";
	    goto copyError;
	}
    }
    Tcl_Close(interp, in);
    return TCL_OK;

  copyError:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "%s: %s", errMsg, Tcl_PosixError(interp)));
    Tcl_Close(interp, in);
    return TCL_ERROR;
}

/*
 * ---------------------------------------------------------------------
 *
 * SerializeLocalEntryHeader, SerializeCentralDirectoryEntry,
 * SerializeCentralDirectorySuffix --
 *
 *	Create serialized forms of the structures that make up the ZIP
 *	metadata. Note that the both the local entry and the central directory
 *	entry need to have the name of the entry written directly afterwards.
 *
 *	We could write these as structs except we need to guarantee that we
 *	are writing these out as little-endian values.
 *
 * Side effects:
 *	Both update their buffer arguments, but otherwise change nothing.
 *
 * ---------------------------------------------------------------------
 */

static void
SerializeLocalEntryHeader(
    const unsigned char *start,	/* The start of writable memory. */
    const unsigned char *end,	/* The end of writable memory. */
    unsigned char *buf,		/* Where to serialize to */
    ZipEntry *z,		/* The description of what to serialize. */
    int nameLength,		/* The length of the name. */
    int align)			/* The number of alignment bytes. */
{
    ZipWriteInt(start, end, buf + ZIP_LOCAL_SIG_OFFS, ZIP_LOCAL_HEADER_SIG);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_VERSION_OFFS, ZIP_MIN_VERSION);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_FLAGS_OFFS, z->isEncrypted);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_COMPMETH_OFFS,
	    z->compressMethod);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_MTIME_OFFS,
	    ToDosTime(z->timestamp));
    ZipWriteShort(start, end, buf + ZIP_LOCAL_MDATE_OFFS,
	    ToDosDate(z->timestamp));
    ZipWriteInt(start, end, buf + ZIP_LOCAL_CRC32_OFFS, z->crc32);
    ZipWriteInt(start, end, buf + ZIP_LOCAL_COMPLEN_OFFS,
	    z->numCompressedBytes);
    ZipWriteInt(start, end, buf + ZIP_LOCAL_UNCOMPLEN_OFFS, z->numBytes);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_PATHLEN_OFFS, nameLength);
    ZipWriteShort(start, end, buf + ZIP_LOCAL_EXTRALEN_OFFS, align);
}

static void
SerializeCentralDirectoryEntry(
    const unsigned char *start,	/* The start of writable memory. */
    const unsigned char *end,	/* The end of writable memory. */
    unsigned char *buf,		/* Where to serialize to */
    ZipEntry *z,		/* The description of what to serialize. */
    size_t nameLength)		/* The length of the name. */
{
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_SIG_OFFS,
	    ZIP_CENTRAL_HEADER_SIG);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_VERSIONMADE_OFFS,
	    ZIP_MIN_VERSION);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_VERSION_OFFS, ZIP_MIN_VERSION);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_FLAGS_OFFS, z->isEncrypted);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_COMPMETH_OFFS,
	    z->compressMethod);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_MTIME_OFFS,
	    ToDosTime(z->timestamp));
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_MDATE_OFFS,
	    ToDosDate(z->timestamp));
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_CRC32_OFFS, z->crc32);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_COMPLEN_OFFS,
	    z->numCompressedBytes);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_UNCOMPLEN_OFFS, z->numBytes);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_PATHLEN_OFFS, nameLength);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_EXTRALEN_OFFS, 0);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_FCOMMENTLEN_OFFS, 0);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_DISKFILE_OFFS, 0);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_IATTR_OFFS, 0);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_EATTR_OFFS, 0);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_LOCALHDR_OFFS,
	    z->offset);
}

static void
SerializeCentralDirectorySuffix(
    const unsigned char *start,	/* The start of writable memory. */
    const unsigned char *end,	/* The end of writable memory. */
    unsigned char *buf,		/* Where to serialize to */
    int entryCount,		/* The number of entries in the directory */
    long long directoryStartOffset,
				/* The overall file offset of the start of the
				 * central directory. */
    long long suffixStartOffset)/* The overall file offset of the start of the
				 * suffix of the central directory (i.e.,
				 * where this data will be written). */
{
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_END_SIG_OFFS,
	    ZIP_CENTRAL_END_SIG);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_DISKNO_OFFS, 0);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_DISKDIR_OFFS, 0);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_ENTS_OFFS, entryCount);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_TOTALENTS_OFFS, entryCount);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_DIRSIZE_OFFS,
	    suffixStartOffset - directoryStartOffset);
    ZipWriteInt(start, end, buf + ZIP_CENTRAL_DIRSTART_OFFS,
	    directoryStartOffset);
    ZipWriteShort(start, end, buf + ZIP_CENTRAL_COMMENTLEN_OFFS, 0);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkZipObjCmd, ZipFSLMkZipObjCmd --
 *
 *	These procedures are invoked to process the [zipfs mkzip] and [zipfs
 *	lmkzip] commands.  See description of ZipFSMkZipOrImg().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImg().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkZipObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *stripPrefix, *password;

    if (objc < 3 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile indir ?strip? ?password?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	ZIPFS_ERROR(interp, "operation not permitted in a safe interpreter");
	ZIPFS_ERROR_CODE(interp, "SAFE_INTERP");
	return TCL_ERROR;
    }

    stripPrefix = (objc > 3 ? objv[3] : NULL);
    password = (objc > 4 ? objv[4] : NULL);
    return ZipFSMkZipOrImg(interp, 0, objv[1], objv[2], NULL, NULL,
	    stripPrefix, password);
}

static int
ZipFSLMkZipObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *password;

    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile inlist ?password?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	ZIPFS_ERROR(interp, "operation not permitted in a safe interpreter");
	ZIPFS_ERROR_CODE(interp, "SAFE_INTERP");
	return TCL_ERROR;
    }

    password = (objc > 3 ? objv[3] : NULL);
    return ZipFSMkZipOrImg(interp, 0, objv[1], NULL, objv[2], NULL,
	    NULL, password);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkImgObjCmd, ZipFSLMkImgObjCmd --
 *
 *	These procedures are invoked to process the [zipfs mkimg] and [zipfs
 *	lmkimg] commands.  See description of ZipFSMkZipOrImg().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImg().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkImgObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *originFile, *stripPrefix, *password;

    if (objc < 3 || objc > 6) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"outfile indir ?strip? ?password? ?infile?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	ZIPFS_ERROR(interp, "operation not permitted in a safe interpreter");
	ZIPFS_ERROR_CODE(interp, "SAFE_INTERP");
	return TCL_ERROR;
    }

    originFile = (objc > 5 ? objv[5] : NULL);
    stripPrefix = (objc > 3 ? objv[3] : NULL);
    password = (objc > 4 ? objv[4] : NULL);
    return ZipFSMkZipOrImg(interp, 1, objv[1], objv[2], NULL,
	    originFile, stripPrefix, password);
}

static int
ZipFSLMkImgObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *originFile, *password;

    if (objc < 3 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile inlist ?password infile?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	ZIPFS_ERROR(interp, "operation not permitted in a safe interpreter");
	ZIPFS_ERROR_CODE(interp, "SAFE_INTERP");
	return TCL_ERROR;
    }

    originFile = (objc > 4 ? objv[4] : NULL);
    password = (objc > 3 ? objv[3] : NULL);
    return ZipFSMkZipOrImg(interp, 1, objv[1], NULL, objv[2],
	    originFile, NULL, password);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSCanonicalObjCmd --
 *
 *	This procedure is invoked to process the [zipfs canonical] command.
 *	It returns the canonical name for a file within zipfs
 *
 * Results:
 *	Always TCL_OK provided the right number of arguments are supplied.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSCanonicalObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    char *mntpoint = NULL;
    char *filename = NULL;
    char *result;
    Tcl_DString dPath;

    if (objc < 2 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "?mountpoint? filename ?inZipfs?");
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dPath);
    if (objc == 2) {
	filename = Tcl_GetString(objv[1]);
	result = CanonicalPath("", filename, &dPath, 1);
    } else if (objc == 3) {
	mntpoint = Tcl_GetString(objv[1]);
	filename = Tcl_GetString(objv[2]);
	result = CanonicalPath(mntpoint, filename, &dPath, 1);
    } else {
	int zipfs = 0;

	if (Tcl_GetBooleanFromObj(interp, objv[3], &zipfs)) {
	    return TCL_ERROR;
	}
	mntpoint = Tcl_GetString(objv[1]);
	filename = Tcl_GetString(objv[2]);
	result = CanonicalPath(mntpoint, filename, &dPath, zipfs);
    }
    Tcl_SetObjResult(interp, Tcl_NewStringObj(result, -1));
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSExistsObjCmd --
 *
 *	This procedure is invoked to process the [zipfs exists] command.  It
 *	tests for the existence of a file in the ZIP filesystem and places a
 *	boolean into the interp's result.
 *
 * Results:
 *	Always TCL_OK provided the right number of arguments are supplied.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSExistsObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    char *filename;
    int exists;
    Tcl_DString ds;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }

    /*
     * Prepend ZIPFS_VOLUME to filename, eliding the final /
     */

    filename = Tcl_GetString(objv[1]);
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN - 1);
    Tcl_DStringAppend(&ds, filename, -1);
    filename = Tcl_DStringValue(&ds);

    ReadLock();
    exists = ZipFSLookup(filename) != NULL;
    Unlock();

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(exists));
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSInfoObjCmd --
 *
 *	This procedure is invoked to process the [zipfs info] command.  On
 *	success, it returns a Tcl list made up of name of ZIP archive file,
 *	size uncompressed, size compressed, and archive offset of a file in
 *	the ZIP filesystem.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSInfoObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    char *filename;
    ZipEntry *z;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }
    filename = Tcl_GetString(objv[1]);
    ReadLock();
    z = ZipFSLookup(filename);
    if (z) {
	Tcl_Obj *result = Tcl_GetObjResult(interp);

	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewStringObj(z->zipFilePtr->name, -1));
	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewWideIntObj(z->numBytes));
	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewWideIntObj(z->numCompressedBytes));
	Tcl_ListObjAppendElement(interp, result, Tcl_NewWideIntObj(z->offset));
    }
    Unlock();
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSListObjCmd --
 *
 *	This procedure is invoked to process the [zipfs list] command.	 On
 *	success, it returns a Tcl list of files of the ZIP filesystem which
 *	match a search pattern (glob or regexp).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSListObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    char *pattern = NULL;
    Tcl_RegExp regexp = NULL;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Obj *result = Tcl_GetObjResult(interp);
    const char *options[] = {"-glob", "-regexp", NULL};
    enum list_options { OPT_GLOB, OPT_REGEXP };

    /*
     * Parse arguments.
     */

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?(-glob|-regexp)? ?pattern?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	int idx;

	if (Tcl_GetIndexFromObj(interp, objv[1], options, "option",
		0, &idx) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (idx) {
	case OPT_GLOB:
	    pattern = Tcl_GetString(objv[2]);
	    break;
	case OPT_REGEXP:
	    regexp = Tcl_RegExpCompile(interp, Tcl_GetString(objv[2]));
	    if (!regexp) {
		return TCL_ERROR;
	    }
	    break;
	}
    } else if (objc == 2) {
	pattern = Tcl_GetString(objv[1]);
    }

    /*
     * Scan for matching entries.
     */

    ReadLock();
    if (pattern) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    if (Tcl_StringMatch(z->name, pattern)) {
		Tcl_ListObjAppendElement(interp, result,
			Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else if (regexp) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    if (Tcl_RegExpExec(interp, regexp, z->name, z->name)) {
		Tcl_ListObjAppendElement(interp, result,
			Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    Tcl_ListObjAppendElement(interp, result,
		    Tcl_NewStringObj(z->name, -1));
	}
    }
    Unlock();
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_TclLibrary --
 *
 *	This procedure gets (and possibly finds) the root that Tcl's library
 *	files are mounted under.
 *
 * Results:
 *	A Tcl object holding the location (with zero refcount), or NULL if no
 *	Tcl library can be found.
 *
 * Side effects:
 *	May initialise the cache of where such library files are to be found.
 *	This cache is never cleared.
 *
 *-------------------------------------------------------------------------
 */

Tcl_Obj *
TclZipfs_TclLibrary(void)
{
    Tcl_Obj *vfsInitScript;
    int found;
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(STATIC_BUILD)
#   define LIBRARY_SIZE	    64
    HMODULE hModule;
    WCHAR wName[MAX_PATH + LIBRARY_SIZE];
    char dllName[(MAX_PATH + LIBRARY_SIZE) * 3];
#endif /* _WIN32 */

    /*
     * Use the cached value if that has been set; we don't want to repeat the
     * searching and mounting.
     */

    if (zipfs_literal_tcl_library) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }

    /*
     * Look for the library file system within the executable.
     */

    vfsInitScript = Tcl_NewStringObj(ZIPFS_APP_MOUNT "/tcl_library/init.tcl",
	    -1);
    Tcl_IncrRefCount(vfsInitScript);
    found = Tcl_FSAccess(vfsInitScript, F_OK);
    Tcl_DecrRefCount(vfsInitScript);
    if (found == TCL_OK) {
	zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }

    /*
     * Look for the library file system within the DLL/shared library.  Note
     * that we must mount the zip file and dll before releasing to search.
     */

#if !defined(STATIC_BUILD)
#if defined(_WIN32) || defined(__CYGWIN__)
    hModule = (HMODULE)TclWinGetTclInstance();
    GetModuleFileNameW(hModule, wName, MAX_PATH);
#ifdef __CYGWIN__
    cygwin_conv_path(3, wName, dllName, sizeof(dllName));
#else
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, dllName, sizeof(dllName), NULL, NULL);
#endif

    if (ZipfsAppHookFindTclInit(dllName) == TCL_OK) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }
#elif !defined(NO_DLFCN_H)
    Dl_info dlinfo;
    if (dladdr((const void *)TclZipfs_TclLibrary, &dlinfo) && (dlinfo.dli_fname != NULL)
	&& (ZipfsAppHookFindTclInit(dlinfo.dli_fname) == TCL_OK)) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }
#else
    if (ZipfsAppHookFindTclInit(CFG_RUNTIME_LIBDIR "/" CFG_RUNTIME_DLLFILE) == TCL_OK) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }
#endif /* _WIN32 */
#endif /* !defined(STATIC_BUILD) */

    /*
     * If anything set the cache (but subsequently failed) go with that
     * anyway.
     */

    if (zipfs_literal_tcl_library) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    }
    return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSTclLibraryObjCmd --
 *
 *	This procedure is invoked to process the
 *	[::tcl::zipfs::tcl_library_init] command, usually called during the
 *	execution of Tcl's interpreter startup. It returns the root that Tcl's
 *	library files are mounted under.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May initialise the cache of where such library files are to be found.
 *	This cache is never cleared.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSTclLibraryObjCmd(
    TCL_UNUSED(ClientData),
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*objc*/,
    TCL_UNUSED(Tcl_Obj *const *)) /*objv*/
{
    if (!Tcl_IsSafe(interp)) {
	Tcl_Obj *pResult = TclZipfs_TclLibrary();

	if (!pResult) {
	    TclNewObj(pResult);
	}
	Tcl_SetObjResult(interp, pResult);
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelClose --
 *
 *	This function is called to close a channel.
 *
 * Results:
 *	Always TCL_OK.
 *
 * Side effects:
 *	Resources are free'd.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipChannelClose(
    void *instanceData,
    TCL_UNUSED(Tcl_Interp *),
    int flags)
{
    ZipChannel *info = (ZipChannel *) instanceData;

    if ((flags & (TCL_CLOSE_READ | TCL_CLOSE_WRITE)) != 0) {
	return EINVAL;
    }

    if (info->iscompr && info->ubuf) {
	ckfree(info->ubuf);
	info->ubuf = NULL;
    }
    if (info->isEncrypted) {
	info->isEncrypted = 0;
	memset(info->keys, 0, sizeof(info->keys));
    }
    if (info->isWriting) {
	ZipEntry *z = info->zipEntryPtr;
	unsigned char *newdata = (unsigned char *)
		attemptckrealloc(info->ubuf, info->numRead);

	if (newdata) {
	    if (z->data) {
		ckfree(z->data);
	    }
	    z->data = newdata;
	    z->numBytes = z->numCompressedBytes = info->numBytes;
	    z->compressMethod = ZIP_COMPMETH_STORED;
	    z->timestamp = time(NULL);
	    z->isDirectory = 0;
	    z->isEncrypted = 0;
	    z->offset = 0;
	    z->crc32 = 0;
	} else {
	    ckfree(info->ubuf);
	}
    }
    WriteLock();
    info->zipFilePtr->numOpen--;
    Unlock();
    ckfree(info);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelRead --
 *
 *	This function is called to read data from channel.
 *
 * Results:
 *	Number of bytes read or -1 on error with error number set.
 *
 * Side effects:
 *	Data is read and file pointer is advanced.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipChannelRead(
    void *instanceData,
    char *buf,
    int toRead,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long nextpos;

    if (info->isDirectory < 0) {
	/*
	 * Special case: when executable combined with ZIP archive file read
	 * data in front of ZIP, i.e. the executable itself.
	 */

	nextpos = info->numRead + toRead;
	if (nextpos > info->zipFilePtr->baseOffset) {
	    toRead = info->zipFilePtr->baseOffset - info->numRead;
	    nextpos = info->zipFilePtr->baseOffset;
	}
	if (toRead == 0) {
	    return 0;
	}
	memcpy(buf, info->zipFilePtr->data, toRead);
	info->numRead = nextpos;
	*errloc = 0;
	return toRead;
    }
    if (info->isDirectory) {
	*errloc = EISDIR;
	return -1;
    }
    nextpos = info->numRead + toRead;
    if (nextpos > info->numBytes) {
	toRead = info->numBytes - info->numRead;
	nextpos = info->numBytes;
    }
    if (toRead == 0) {
	return 0;
    }
    if (info->isEncrypted) {
	int i;

	for (i = 0; i < toRead; i++) {
	    int ch = info->ubuf[i + info->numRead];

	    buf[i] = zdecode(info->keys, crc32tab, ch);
	}
    } else {
	memcpy(buf, info->ubuf + info->numRead, toRead);
    }
    info->numRead = nextpos;
    *errloc = 0;
    return toRead;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelWrite --
 *
 *	This function is called to write data into channel.
 *
 * Results:
 *	Number of bytes written or -1 on error with error number set.
 *
 * Side effects:
 *	Data is written and file pointer is advanced.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipChannelWrite(
    void *instanceData,
    const char *buf,
    int toWrite,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long nextpos;

    if (!info->isWriting) {
	*errloc = EINVAL;
	return -1;
    }
    nextpos = info->numRead + toWrite;
    if (nextpos > info->maxWrite) {
	toWrite = info->maxWrite - info->numRead;
	nextpos = info->maxWrite;
    }
    if (toWrite == 0) {
	return 0;
    }
    memcpy(info->ubuf + info->numRead, buf, toWrite);
    info->numRead = nextpos;
    if (info->numRead > info->numBytes) {
	info->numBytes = info->numRead;
    }
    *errloc = 0;
    return toWrite;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelSeek/ZipChannelWideSeek --
 *
 *	This function is called to position file pointer of channel.
 *
 * Results:
 *	New file position or -1 on error with error number set.
 *
 * Side effects:
 *	File pointer is repositioned according to offset and mode.
 *
 *-------------------------------------------------------------------------
 */

static long long
ZipChannelWideSeek(
    void *instanceData,
    long long offset,
    int mode,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    size_t end;

    if (!info->isWriting && (info->isDirectory < 0)) {
	/*
	 * Special case: when executable combined with ZIP archive file, seek
	 * within front of ZIP, i.e. the executable itself.
	 */
	end = info->zipFilePtr->baseOffset;
    } else if (info->isDirectory) {
	*errloc = EINVAL;
	return -1;
    } else {
	end = info->numBytes;
    }
    switch (mode) {
    case SEEK_CUR:
	offset += info->numRead;
	break;
    case SEEK_END:
	offset += end;
	break;
    case SEEK_SET:
	break;
    default:
	*errloc = EINVAL;
	return -1;
    }
    if (offset < 0) {
	*errloc = EINVAL;
	return -1;
    }
    if (info->isWriting) {
	if ((size_t) offset > info->maxWrite) {
	    *errloc = EINVAL;
	    return -1;
	}
	if ((size_t) offset > info->numBytes) {
	    info->numBytes = offset;
	}
    } else if ((size_t) offset > end) {
	*errloc = EINVAL;
	return -1;
    }
    info->numRead = (size_t) offset;
    return info->numRead;
}

#if !defined(TCL_NO_DEPRECATED) && (TCL_MAJOR_VERSION < 9)
static int
ZipChannelSeek(
    void *instanceData,
    long offset,
    int mode,
    int *errloc)
{
    return ZipChannelWideSeek(instanceData, offset, mode, errloc);
}
#endif

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelWatchChannel --
 *
 *	This function is called for event notifications on channel. Does
 *	nothing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static void
ZipChannelWatchChannel(
    TCL_UNUSED(ClientData),
    TCL_UNUSED(int) /*mask*/)
{
    return;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelGetFile --
 *
 *	This function is called to retrieve OS handle for channel.
 *
 * Results:
 *	Always TCL_ERROR since there's never an OS handle for a file within a
 *	ZIP archive.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipChannelGetFile(
    TCL_UNUSED(ClientData),
    TCL_UNUSED(int) /*direction*/,
    TCL_UNUSED(ClientData *) /*handlePtr*/)
{
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelOpen --
 *
 *	This function opens a Tcl_Channel on a file from a mounted ZIP archive
 *	according to given open mode (already parsed by caller).
 *
 * Results:
 *	Tcl_Channel on success, or NULL on error.
 *
 * Side effects:
 *	Memory is allocated, the file from the ZIP archive is uncompressed.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Channel
ZipChannelOpen(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *filename,		/* What are we opening. */
    int wr,			/* True if we're opening in write mode. */
    int trunc)			/* True if we're opening in truncate mode. */
{
    ZipEntry *z;
    ZipChannel *info;
    int flags = 0;
    char cname[128];

    /*
     * Is the file there?
     */

    WriteLock();
    z = ZipFSLookup(filename);
    if (!z) {
	Tcl_SetErrno(ENOENT);
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file not found \"%s\": %s", filename,
		    Tcl_PosixError(interp)));
	}
	goto error;
    }

    /*
     * Do we support opening the file that way?
     */

    if (wr && z->isDirectory) {
	Tcl_SetErrno(EISDIR);
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "unsupported file type: %s",
		    Tcl_PosixError(interp)));
	}
	goto error;
    }
    if ((z->compressMethod != ZIP_COMPMETH_STORED)
	    && (z->compressMethod != ZIP_COMPMETH_DEFLATED)) {
	ZIPFS_ERROR(interp, "unsupported compression method");
	ZIPFS_ERROR_CODE(interp, "COMP_METHOD");
	goto error;
    }
    if (!trunc) {
	flags |= TCL_READABLE;
	if (z->isEncrypted && (z->zipFilePtr->passBuf[0] == 0)) {
	    ZIPFS_ERROR(interp, "decryption failed");
	    ZIPFS_ERROR_CODE(interp, "DECRYPT");
	    goto error;
	} else if (wr && !z->data && (z->numBytes > ZipFS.wrmax)) {
	    ZIPFS_ERROR(interp, "file too large");
	    ZIPFS_ERROR_CODE(interp, "FILE_SIZE");
	    goto error;
	}
    } else {
	flags = TCL_WRITABLE;
    }

    info = AllocateZipChannel(interp);
    if (!info) {
	goto error;
    }
    info->zipFilePtr = z->zipFilePtr;
    info->zipEntryPtr = z;
    if (wr) {
	/*
	 * Set up a writable channel.
	 */

	flags |= TCL_WRITABLE;
	if (InitWritableChannel(interp, info, z, trunc) == TCL_ERROR) {
	    ckfree(info);
	    goto error;
	}
    } else if (z->data) {
	/*
	 * Set up a readable channel for direct data.
	 */

	flags |= TCL_READABLE;
	info->numBytes = z->numBytes;
	info->ubuf = z->data;
    } else {
	/*
	 * Set up a readable channel.
	 */

	flags |= TCL_READABLE;
	if (InitReadableChannel(interp, info, z) == TCL_ERROR) {
	    ckfree(info);
	    goto error;
	}
    }

    /*
     * Wrap the ZipChannel into a Tcl_Channel.
     */

    sprintf(cname, "zipfs_%" TCL_Z_MODIFIER "x_%d", z->offset,
	    ZipFS.idCount++);
    z->zipFilePtr->numOpen++;
    Unlock();
    return Tcl_CreateChannel(&ZipChannelType, cname, info, flags);

  error:
    Unlock();
    return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * InitWritableChannel --
 *
 *	Assistant for ZipChannelOpen() that sets up a writable channel. It's
 *	up to the caller to actually register the channel.
 *
 * Returns:
 *	Tcl result code.
 *
 * Side effects:
 *	Allocates memory for the implementation of the channel. Writes to the
 *	interpreter's result on error.
 *
 *-------------------------------------------------------------------------
 */

static int
InitWritableChannel(
    Tcl_Interp *interp,		/* Current interpreter, or NULL (when errors
				 * will be silent). */
    ZipChannel *info,		/* The channel to set up. */
    ZipEntry *z,		/* The zipped file that the channel will write
				 * to. */
    int trunc)			/* Whether to truncate the data. */
{
    int i, ch;
    unsigned char *cbuf = NULL;

    /*
     * Set up a writable channel.
     */

    info->isWriting = 1;
    info->maxWrite = ZipFS.wrmax;

    info->ubuf = (unsigned char *) attemptckalloc(info->maxWrite);
    if (!info->ubuf) {
	goto memoryError;
    }
    memset(info->ubuf, 0, info->maxWrite);

    if (trunc) {
	/*
	 * Truncate; nothing there.
	 */

	info->numBytes = 0;
    } else if (z->data) {
	/*
	 * Already got uncompressed data.
	 */

	unsigned int j = z->numBytes;

	if (j > info->maxWrite) {
	    j = info->maxWrite;
	}
	memcpy(info->ubuf, z->data, j);
	info->numBytes = j;
    } else {
	/*
	 * Need to uncompress the existing data.
	 */

	unsigned char *zbuf = z->zipFilePtr->data + z->offset;

	if (z->isEncrypted) {
	    int len = z->zipFilePtr->passBuf[0] & 0xFF;
	    char passBuf[260];

	    for (i = 0; i < len; i++) {
		ch = z->zipFilePtr->passBuf[len - i];
		passBuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	    }
	    passBuf[i] = '\0';
	    init_keys(passBuf, info->keys, crc32tab);
	    memset(passBuf, 0, sizeof(passBuf));
	    for (i = 0; i < 12; i++) {
		ch = info->ubuf[i];
		zdecode(info->keys, crc32tab, ch);
	    }
	    zbuf += i;
	}

	if (z->compressMethod == ZIP_COMPMETH_DEFLATED) {
	    z_stream stream;
	    int err;

	    memset(&stream, 0, sizeof(z_stream));
	    stream.zalloc = Z_NULL;
	    stream.zfree = Z_NULL;
	    stream.opaque = Z_NULL;
	    stream.avail_in = z->numCompressedBytes;
	    if (z->isEncrypted) {
		unsigned int j;

		stream.avail_in -= 12;
		cbuf = (unsigned char *) attemptckalloc(stream.avail_in);
		if (!cbuf) {
		    goto memoryError;
		}
		for (j = 0; j < stream.avail_in; j++) {
		    ch = info->ubuf[j];
		    cbuf[j] = zdecode(info->keys, crc32tab, ch);
		}
		stream.next_in = cbuf;
	    } else {
		stream.next_in = zbuf;
	    }
	    stream.next_out = info->ubuf;
	    stream.avail_out = info->maxWrite;
	    if (inflateInit2(&stream, -15) != Z_OK) {
		goto corruptionError;
	    }
	    err = inflate(&stream, Z_SYNC_FLUSH);
	    inflateEnd(&stream);
	    if ((err == Z_STREAM_END)
		    || ((err == Z_OK) && (stream.avail_in == 0))) {
		if (cbuf) {
		    memset(info->keys, 0, sizeof(info->keys));
		    ckfree(cbuf);
		}
		return TCL_OK;
	    }
	    goto corruptionError;
	} else if (z->isEncrypted) {
	    /*
	     * Need to decrypt some otherwise-simple stored data.
	     */

	    for (i = 0; i < z->numBytes - 12; i++) {
		ch = zbuf[i];
		info->ubuf[i] = zdecode(info->keys, crc32tab, ch);
	    }
	} else {
	    /*
	     * Simple stored data. Copy into our working buffer.
	     */

	    memcpy(info->ubuf, zbuf, z->numBytes);
	}
	memset(info->keys, 0, sizeof(info->keys));
    }
    return TCL_OK;

  memoryError:
    if (info->ubuf) {
	ckfree(info->ubuf);
    }
    ZIPFS_MEM_ERROR(interp);
    return TCL_ERROR;

  corruptionError:
    if (cbuf) {
	memset(info->keys, 0, sizeof(info->keys));
	ckfree(cbuf);
    }
    if (info->ubuf) {
	ckfree(info->ubuf);
    }
    ZIPFS_ERROR(interp, "decompression error");
    ZIPFS_ERROR_CODE(interp, "CORRUPT");
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * InitReadableChannel --
 *
 *	Assistant for ZipChannelOpen() that sets up a readable channel. It's
 *	up to the caller to actually register the channel.
 *
 * Returns:
 *	Tcl result code.
 *
 * Side effects:
 *	Allocates memory for the implementation of the channel. Writes to the
 *	interpreter's result on error.
 *
 *-------------------------------------------------------------------------
 */

static int
InitReadableChannel(
    Tcl_Interp *interp,		/* Current interpreter, or NULL (when errors
				 * will be silent). */
    ZipChannel *info,		/* The channel to set up. */
    ZipEntry *z)		/* The zipped file that the channel will read
				 * from. */
{
    unsigned char *ubuf = NULL;
    int i, ch;

    info->iscompr = (z->compressMethod == ZIP_COMPMETH_DEFLATED);
    info->ubuf = z->zipFilePtr->data + z->offset;
    info->isDirectory = z->isDirectory;
    info->isEncrypted = z->isEncrypted;
    info->numBytes = z->numBytes;

    if (info->isEncrypted) {
	int len = z->zipFilePtr->passBuf[0] & 0xFF;
	char passBuf[260];

	for (i = 0; i < len; i++) {
	    ch = z->zipFilePtr->passBuf[len - i];
	    passBuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	}
	passBuf[i] = '\0';
	init_keys(passBuf, info->keys, crc32tab);
	memset(passBuf, 0, sizeof(passBuf));
	for (i = 0; i < 12; i++) {
	    ch = info->ubuf[i];
	    zdecode(info->keys, crc32tab, ch);
	}
	info->ubuf += i;
    }

    if (info->iscompr) {
	z_stream stream;
	int err;
	unsigned int j;

	/*
	 * Data to decode is compressed, and possibly encrpyted too.
	 */

	memset(&stream, 0, sizeof(z_stream));
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = z->numCompressedBytes;
	if (info->isEncrypted) {
	    stream.avail_in -= 12;
	    ubuf = (unsigned char *) attemptckalloc(stream.avail_in);
	    if (!ubuf) {
		info->ubuf = NULL;
		goto memoryError;
	    }

	    for (j = 0; j < stream.avail_in; j++) {
		ch = info->ubuf[j];
		ubuf[j] = zdecode(info->keys, crc32tab, ch);
	    }
	    stream.next_in = ubuf;
	} else {
	    stream.next_in = info->ubuf;
	}
	stream.next_out = info->ubuf = (unsigned char *)
		attemptckalloc(info->numBytes);
	if (!info->ubuf) {
	    goto memoryError;
	}
	stream.avail_out = info->numBytes;
	if (inflateInit2(&stream, -15) != Z_OK) {
	    goto corruptionError;
	}
	err = inflate(&stream, Z_SYNC_FLUSH);
	inflateEnd(&stream);

	/*
	 * Decompression was successful if we're either in the END state, or
	 * in the OK state with no buffered bytes.
	 */

	if ((err != Z_STREAM_END)
		&& ((err != Z_OK) || (stream.avail_in != 0))) {
	    goto corruptionError;
	}

	if (ubuf) {
	    info->isEncrypted = 0;
	    memset(info->keys, 0, sizeof(info->keys));
	    ckfree(ubuf);
	}
	return TCL_OK;
    } else if (info->isEncrypted) {
	unsigned int j, len;

	/*
	 * Decode encrypted but uncompressed file, since we support Tcl_Seek()
	 * on it, and it can be randomly accessed later.
	 */

	len = z->numCompressedBytes - 12;
	ubuf = (unsigned char *) attemptckalloc(len);
	if (ubuf == NULL) {
	    goto memoryError;
	}
	for (j = 0; j < len; j++) {
	    ch = info->ubuf[j];
	    ubuf[j] = zdecode(info->keys, crc32tab, ch);
	}
	info->ubuf = ubuf;
	info->isEncrypted = 0;
    }
    return TCL_OK;

  corruptionError:
    if (ubuf) {
	info->isEncrypted = 0;
	memset(info->keys, 0, sizeof(info->keys));
	ckfree(ubuf);
    }
    if (info->ubuf) {
	ckfree(info->ubuf);
    }
    ZIPFS_ERROR(interp, "decompression error");
    ZIPFS_ERROR_CODE(interp, "CORRUPT");
    return TCL_ERROR;

  memoryError:
    if (ubuf) {
	info->isEncrypted = 0;
	memset(info->keys, 0, sizeof(info->keys));
	ckfree(ubuf);
    }
    ZIPFS_MEM_ERROR(interp);
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipEntryStat --
 *
 *	This function implements the ZIP filesystem specific version of the
 *	library version of stat.
 *
 * Results:
 *	See stat documentation.
 *
 * Side effects:
 *	See stat documentation.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipEntryStat(
    char *path,
    Tcl_StatBuf *buf)
{
    ZipEntry *z;
    int ret = -1;

    ReadLock();
    z = ZipFSLookup(path);
    if (z) {
	memset(buf, 0, sizeof(Tcl_StatBuf));
	if (z->isDirectory) {
	    buf->st_mode = S_IFDIR | 0555;
	} else {
	    buf->st_mode = S_IFREG | 0555;
	}
	buf->st_size = z->numBytes;
	buf->st_mtime = z->timestamp;
	buf->st_ctime = z->timestamp;
	buf->st_atime = z->timestamp;
	ret = 0;
    }
    Unlock();
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipEntryAccess --
 *
 *	This function implements the ZIP filesystem specific version of the
 *	library version of access.
 *
 * Results:
 *	See access documentation.
 *
 * Side effects:
 *	See access documentation.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipEntryAccess(
    char *path,
    int mode)
{
    ZipEntry *z;

    if (mode & 3) {
	return -1;
    }
    ReadLock();
    z = ZipFSLookup(path);
    Unlock();
    return (z ? 0 : -1);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSOpenFileChannelProc --
 *
 *	Open a channel to a file in a mounted ZIP archive. Delegates to
 *	ZipChannelOpen().
 *
 * Results:
 *	Tcl_Channel on success, or NULL on error.
 *
 * Side effects:
 *	Allocates memory.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Channel
ZipFSOpenFileChannelProc(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *pathPtr,
    int mode,
    TCL_UNUSED(int) /* permissions */)
{
    int trunc = (mode & O_TRUNC) != 0;
    int wr = (mode & (O_WRONLY | O_RDWR)) != 0;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return NULL;
    }

    /*
     * Check for unsupported modes.
     */

    if ((mode & O_APPEND) || ((ZipFS.wrmax <= 0) && wr)) {
	Tcl_SetErrno(EACCES);
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "write access not supported: %s",
		    Tcl_PosixError(interp)));
	}
	return NULL;
    }

    return ZipChannelOpen(interp, Tcl_GetString(pathPtr), wr, trunc);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSStatProc --
 *
 *	This function implements the ZIP filesystem specific version of the
 *	library version of stat.
 *
 * Results:
 *	See stat documentation.
 *
 * Side effects:
 *	See stat documentation.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSStatProc(
    Tcl_Obj *pathPtr,
    Tcl_StatBuf *buf)
{
    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    return ZipEntryStat(Tcl_GetString(pathPtr), buf);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSAccessProc --
 *
 *	This function implements the ZIP filesystem specific version of the
 *	library version of access.
 *
 * Results:
 *	See access documentation.
 *
 * Side effects:
 *	See access documentation.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSAccessProc(
    Tcl_Obj *pathPtr,
    int mode)
{
    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    return ZipEntryAccess(Tcl_GetString(pathPtr), mode);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFilesystemSeparatorProc --
 *
 *	This function returns the separator to be used for a given path. The
 *	object returned should have a refCount of zero
 *
 * Results:
 *	A Tcl object, with a refCount of zero. If the caller needs to retain a
 *	reference to the object, it should call Tcl_IncrRefCount, and should
 *	otherwise free the object.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
ZipFSFilesystemSeparatorProc(
    TCL_UNUSED(Tcl_Obj *) /*pathPtr*/)
{
    return Tcl_NewStringObj("/", -1);
}

/*
 *-------------------------------------------------------------------------
 *
 * AppendWithPrefix --
 *
 *	Worker for ZipFSMatchInDirectoryProc() that is a wrapper around
 *	Tcl_ListObjAppendElement() which knows about handling prefixes.
 *
 *-------------------------------------------------------------------------
 */

static inline void
AppendWithPrefix(
    Tcl_Obj *result,		/* Where to append a list element to. */
    Tcl_DString *prefix,	/* The prefix to add to the element, or NULL
				 * for don't do that. */
    const char *name,		/* The name to append. */
    int nameLen)		/* The length of the name. May be -1 for
				 * append-up-to-NUL-byte. */
{
    if (prefix) {
	int prefixLength = Tcl_DStringLength(prefix);

	Tcl_DStringAppend(prefix, name, nameLen);
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(
		Tcl_DStringValue(prefix), Tcl_DStringLength(prefix)));
	Tcl_DStringSetLength(prefix, prefixLength);
    } else {
	Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(name, nameLen));
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMatchInDirectoryProc --
 *
 *	This routine is used by the globbing code to search a directory for
 *	all files which match a given pattern.
 *
 * Results:
 *	The return value is a standard Tcl result indicating whether an error
 *	occurred in globbing. Errors are left in interp, good results are
 *	lappend'ed to resultPtr (which must be a valid object).
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMatchInDirectoryProc(
    TCL_UNUSED(Tcl_Interp *),
    Tcl_Obj *result,		/* Where to append matched items to. */
    Tcl_Obj *pathPtr,		/* Where we are looking. */
    const char *pattern,	/* What names we are looking for. */
    Tcl_GlobTypeData *types)	/* What types we are looking for. */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Obj *normPathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    int scnt, l, dirOnly = -1, prefixLen, strip = 0, mounts = 0, len;
    char *pat, *prefix, *path;
    Tcl_DString dsPref, *prefixBuf = NULL;

    if (!normPathPtr) {
	return -1;
    }
    if (types) {
	dirOnly = (types->type & TCL_GLOB_TYPE_DIR) == TCL_GLOB_TYPE_DIR;
	mounts = (types->type == TCL_GLOB_TYPE_MOUNT);
    }

    /*
     * The prefix that gets prepended to results.
     */

    prefix = Tcl_GetStringFromObj(pathPtr, &prefixLen);

    /*
     * The (normalized) path we're searching.
     */

    path = Tcl_GetStringFromObj(normPathPtr, &len);

    Tcl_DStringInit(&dsPref);
    if (strcmp(prefix, path) == 0) {
	prefixBuf = NULL;
    } else {
	/*
	 * We need to strip the normalized prefix of the filenames and replace
	 * it with the official prefix that we were expecting to get.
	 */

	strip = len + 1;
	Tcl_DStringAppend(&dsPref, prefix, prefixLen);
	Tcl_DStringAppend(&dsPref, "/", 1);
	prefix = Tcl_DStringValue(&dsPref);
	prefixBuf = &dsPref;
    }

    ReadLock();

    /*
     * Are we globbing the mount points?
     */

    if (mounts) {
	ZipFSMatchMountPoints(result, normPathPtr, pattern, prefixBuf);
	goto end;
    }

    /*
     * Can we skip the complexity of actual globbing? Without a pattern, yes;
     * it's a directory existence test.
     */

    if (!pattern || (pattern[0] == '\0')) {
	ZipEntry *z = ZipFSLookup(path);

	if (z && ((dirOnly < 0) || (!dirOnly && !z->isDirectory)
		|| (dirOnly && z->isDirectory))) {
	    AppendWithPrefix(result, prefixBuf, z->name, -1);
	}
	goto end;
    }

    /*
     * We've got to work for our supper and do the actual globbing. And all
     * we've got really is an undifferentiated pile of all the filenames we've
     * got from all our ZIP mounts.
     */

    l = strlen(pattern);
    pat = (char *) ckalloc(len + l + 2);
    memcpy(pat, path, len);
    while ((len > 1) && (pat[len - 1] == '/')) {
	--len;
    }
    if ((len > 1) || (pat[0] != '/')) {
	pat[len] = '/';
	++len;
    }
    memcpy(pat + len, pattern, l + 1);
    scnt = CountSlashes(pat);

    for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
	    hPtr; hPtr = Tcl_NextHashEntry(&search)) {
	ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	if ((dirOnly >= 0) && ((dirOnly && !z->isDirectory)
		|| (!dirOnly && z->isDirectory))) {
	    continue;
	}
	if ((z->depth == scnt) && Tcl_StringCaseMatch(z->name, pat, 0)) {
	    AppendWithPrefix(result, prefixBuf, z->name + strip, -1);
	}
    }
    ckfree(pat);

  end:
    Unlock();
    Tcl_DStringFree(&dsPref);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMatchMountPoints --
 *
 *	This routine is a worker for ZipFSMatchInDirectoryProc, used by the
 *	globbing code to search for all mount points files which match a given
 *	pattern.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds the matching mounts to the list in result, uses prefix as working
 *	space if it is non-NULL.
 *
 *-------------------------------------------------------------------------
 */

static void
ZipFSMatchMountPoints(
    Tcl_Obj *result,		/* The list of matches being built. */
    Tcl_Obj *normPathPtr,	/* Where we're looking from. */
    const char *pattern,	/* What we're looking for. NULL for a full
				 * list. */
    Tcl_DString *prefix)	/* Workspace filled with a prefix for all the
				 * filenames, or NULL if no prefix is to be
				 * used. */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int l, normLength;
    const char *path = Tcl_GetStringFromObj(normPathPtr, &normLength);
    size_t len = (size_t) normLength;

    if (len < 1) {
	/*
	 * Shouldn't happen. But "shouldn't"...
	 */

	return;
    }
    l = CountSlashes(path);
    if (path[len - 1] == '/') {
	len--;
    } else {
	l++;
    }
    if (!pattern || (pattern[0] == '\0')) {
	pattern = "*";
    }

    for (hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search); hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	ZipFile *zf = (ZipFile *) Tcl_GetHashValue(hPtr);

	if (zf->mountPointLen == 0) {
	    ZipEntry *z;

	    /*
	     * Enumerate the contents of the ZIP; it's mounted on the root.
	     */

	    for (z = zf->topEnts; z; z = z->tnext) {
		size_t lenz = strlen(z->name);

		if ((lenz > len + 1) && (strncmp(z->name, path, len) == 0)
			&& (z->name[len] == '/')
			&& (CountSlashes(z->name) == l)
			&& Tcl_StringCaseMatch(z->name + len + 1, pattern, 0)) {
		    AppendWithPrefix(result, prefix, z->name, lenz);
		}
	    }
	} else if ((zf->mountPointLen > len + 1)
		&& (strncmp(zf->mountPoint, path, len) == 0)
		&& (zf->mountPoint[len] == '/')
		&& (CountSlashes(zf->mountPoint) == l)
		&& Tcl_StringCaseMatch(zf->mountPoint + len + 1,
			pattern, 0)) {
	    /*
	     * Standard mount; append if it matches.
	     */

	    AppendWithPrefix(result, prefix, zf->mountPoint, zf->mountPointLen);
	}
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSPathInFilesystemProc --
 *
 *	This function determines if the given path object is in the ZIP
 *	filesystem.
 *
 * Results:
 *	TCL_OK when the path object is in the ZIP filesystem, -1 otherwise.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSPathInFilesystemProc(
    Tcl_Obj *pathPtr,
    TCL_UNUSED(ClientData *))
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int ret = -1, len;
    char *path;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    path = Tcl_GetStringFromObj(pathPtr, &len);
    if (strncmp(path, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN) != 0) {
	return -1;
    }

    ReadLock();
    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, path);
    if (hPtr) {
	ret = TCL_OK;
	goto endloop;
    }

    for (hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search); hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	ZipFile *zf = (ZipFile *) Tcl_GetHashValue(hPtr);

	if (zf->mountPointLen == 0) {
	    ZipEntry *z;

	    for (z = zf->topEnts; z != NULL; z = z->tnext) {
		size_t lenz = strlen(z->name);

		if (((size_t) len >= lenz) &&
			(strncmp(path, z->name, lenz) == 0)) {
		    ret = TCL_OK;
		    goto endloop;
		}
	    }
	} else if (((size_t) len >= zf->mountPointLen) &&
		(strncmp(path, zf->mountPoint, zf->mountPointLen) == 0)) {
	    ret = TCL_OK;
	    break;
	}
    }

  endloop:
    Unlock();
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSListVolumesProc --
 *
 *	Lists the currently mounted ZIP filesystem volumes.
 *
 * Results:
 *	The list of volumes.
 *
 * Side effects:
 *	None
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
ZipFSListVolumesProc(void)
{
    return Tcl_NewStringObj(ZIPFS_VOLUME, -1);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFileAttrStringsProc --
 *
 *	This function implements the ZIP filesystem dependent 'file
 *	attributes' subcommand, for listing the set of possible attribute
 *	strings.
 *
 * Results:
 *	An array of strings
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

enum ZipFileAttrs {
    ZIP_ATTR_UNCOMPSIZE,
    ZIP_ATTR_COMPSIZE,
    ZIP_ATTR_OFFSET,
    ZIP_ATTR_MOUNT,
    ZIP_ATTR_ARCHIVE,
    ZIP_ATTR_PERMISSIONS,
    ZIP_ATTR_CRC
};

static const char *const *
ZipFSFileAttrStringsProc(
    TCL_UNUSED(Tcl_Obj *) /*pathPtr*/,
    TCL_UNUSED(Tcl_Obj **) /*objPtrRef*/)
{
    /*
     * Must match up with ZipFileAttrs enum above.
     */

    static const char *const attrs[] = {
	"-uncompsize",
	"-compsize",
	"-offset",
	"-mount",
	"-archive",
	"-permissions",
	"-crc",
	NULL,
    };

    return attrs;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFileAttrsGetProc --
 *
 *	This function implements the ZIP filesystem specific 'file attributes'
 *	subcommand, for 'get' operations.
 *
 * Results:
 *	Standard Tcl return code. The object placed in objPtrRef (if TCL_OK
 *	was returned) is likely to have a refCount of zero. Either way we must
 *	either store it somewhere (e.g. the Tcl result), or Incr/Decr its
 *	refCount to ensure it is properly freed.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSFileAttrsGetProc(
    Tcl_Interp *interp,		/* Current interpreter. */
    int index,
    Tcl_Obj *pathPtr,
    Tcl_Obj **objPtrRef)
{
    int len, ret = TCL_OK;
    char *path;
    ZipEntry *z;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    path = Tcl_GetStringFromObj(pathPtr, &len);
    ReadLock();
    z = ZipFSLookup(path);
    if (!z) {
	Tcl_SetErrno(ENOENT);
	ZIPFS_POSIX_ERROR(interp, "file not found");
	ret = TCL_ERROR;
	goto done;
    }
    switch (index) {
    case ZIP_ATTR_UNCOMPSIZE:
	TclNewIntObj(*objPtrRef, z->numBytes);
	break;
    case ZIP_ATTR_COMPSIZE:
	TclNewIntObj(*objPtrRef, z->numCompressedBytes);
	break;
    case ZIP_ATTR_OFFSET:
	TclNewIntObj(*objPtrRef, z->offset);
	break;
    case ZIP_ATTR_MOUNT:
	*objPtrRef = Tcl_NewStringObj(z->zipFilePtr->mountPoint,
		z->zipFilePtr->mountPointLen);
	break;
    case ZIP_ATTR_ARCHIVE:
	*objPtrRef = Tcl_NewStringObj(z->zipFilePtr->name, -1);
	break;
    case ZIP_ATTR_PERMISSIONS:
	*objPtrRef = Tcl_NewStringObj("0o555", -1);
	break;
    case ZIP_ATTR_CRC:
	TclNewIntObj(*objPtrRef, z->crc32);
	break;
    default:
	ZIPFS_ERROR(interp, "unknown attribute");
	ZIPFS_ERROR_CODE(interp, "FILE_ATTR");
	ret = TCL_ERROR;
    }

  done:
    Unlock();
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFileAttrsSetProc --
 *
 *	This function implements the ZIP filesystem specific 'file attributes'
 *	subcommand, for 'set' operations.
 *
 * Results:
 *	Standard Tcl return code.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSFileAttrsSetProc(
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(int) /*index*/,
    TCL_UNUSED(Tcl_Obj *) /*pathPtr*/,
    TCL_UNUSED(Tcl_Obj *) /*objPtr*/)
{
    ZIPFS_ERROR(interp, "unsupported operation");
    ZIPFS_ERROR_CODE(interp, "UNSUPPORTED_OP");
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSFilesystemPathTypeProc --
 *
 * Results:
 *
 * Side effects:
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
ZipFSFilesystemPathTypeProc(
    TCL_UNUSED(Tcl_Obj *) /*pathPtr*/)
{
    return Tcl_NewStringObj("zip", -1);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLoadFile --
 *
 *	This functions deals with loading native object code. If the given
 *	path object refers to a file within the ZIP filesystem, an approriate
 *	error code is returned to delegate loading to the caller (by copying
 *	the file to temp store and loading from there). As fallback when the
 *	file refers to the ZIP file system but is not present, it is looked up
 *	relative to the executable and loaded from there when available.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with error message left.
 *
 * Side effects:
 *	Loads native code into the process address space.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSLoadFile(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *path,
    Tcl_LoadHandle *loadHandle,
    Tcl_FSUnloadFileProc **unloadProcPtr,
    int flags)
{
    Tcl_FSLoadFileProc2 *loadFileProc;
#ifdef ANDROID
    /*
     * Force loadFileProc to native implementation since the package manager
     * already extracted the shared libraries from the APK at install time.
     */

    loadFileProc = (Tcl_FSLoadFileProc2 *) tclNativeFilesystem.loadFileProc;
    if (loadFileProc) {
	return loadFileProc(interp, path, loadHandle, unloadProcPtr, flags);
    }
    Tcl_SetErrno(ENOENT);
    ZIPFS_ERROR(interp, Tcl_PosixError(interp));
    return TCL_ERROR;
#else /* !ANDROID */
    Tcl_Obj *altPath = NULL;
    int ret = TCL_ERROR;
    Tcl_Obj *objs[2] = { NULL, NULL };

    if (Tcl_FSAccess(path, R_OK) == 0) {
	/*
	 * EXDEV should trigger loading by copying to temp store.
	 */

	Tcl_SetErrno(EXDEV);
	ZIPFS_ERROR(interp, Tcl_PosixError(interp));
	return ret;
    }

    objs[1] = TclPathPart(interp, path, TCL_PATH_DIRNAME);
    if (objs[1] && (ZipFSAccessProc(objs[1], R_OK) == 0)) {
	const char *execName = Tcl_GetNameOfExecutable();

	/*
	 * Shared object is not in ZIP but its path prefix is, thus try to
	 * load from directory where the executable came from.
	 */

	TclDecrRefCount(objs[1]);
	objs[1] = TclPathPart(interp, path, TCL_PATH_TAIL);

	/*
	 * Get directory name of executable manually to deal with cases where
	 * [file dirname [info nameofexecutable]] is equal to [info
	 * nameofexecutable] due to VFS effects.
	 */

	if (execName) {
	    const char *p = strrchr(execName, '/');

	    if (p && p > execName + 1) {
		--p;
		objs[0] = Tcl_NewStringObj(execName, p - execName);
	    }
	}
	if (!objs[0]) {
	    objs[0] = TclPathPart(interp, TclGetObjNameOfExecutable(),
		    TCL_PATH_DIRNAME);
	}
	if (objs[0]) {
	    altPath = TclJoinPath(2, objs, 0);
	    if (altPath) {
		Tcl_IncrRefCount(altPath);
		if (Tcl_FSAccess(altPath, R_OK) == 0) {
		    path = altPath;
		}
	    }
	}
    }
    if (objs[0]) {
	Tcl_DecrRefCount(objs[0]);
    }
    if (objs[1]) {
	Tcl_DecrRefCount(objs[1]);
    }

    loadFileProc = (Tcl_FSLoadFileProc2 *) (void *)
	    tclNativeFilesystem.loadFileProc;
    if (loadFileProc) {
	ret = loadFileProc(interp, path, loadHandle, unloadProcPtr, flags);
    } else {
	Tcl_SetErrno(ENOENT);
	ZIPFS_ERROR(interp, Tcl_PosixError(interp));
    }
    if (altPath) {
	Tcl_DecrRefCount(altPath);
    }
    return ret;
#endif /* ANDROID */
}

#endif /* HAVE_ZLIB */

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Init --
 *
 *	Perform per interpreter initialization of this module.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	Initializes this module if not already initialized, and adds module
 *	related commands to the given interpreter.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_Init(
    Tcl_Interp *interp)		/* Current interpreter. */
{
#ifdef HAVE_ZLIB
    static const EnsembleImplMap initMap[] = {
	{"mkimg",	ZipFSMkImgObjCmd,	NULL, NULL, NULL, 1},
	{"mkzip",	ZipFSMkZipObjCmd,	NULL, NULL, NULL, 1},
	{"lmkimg",	ZipFSLMkImgObjCmd,	NULL, NULL, NULL, 1},
	{"lmkzip",	ZipFSLMkZipObjCmd,	NULL, NULL, NULL, 1},
	/* The 4 entries above are not available in safe interpreters */
	{"mount",	ZipFSMountObjCmd,	NULL, NULL, NULL, 1},
	{"mount_data",	ZipFSMountBufferObjCmd,	NULL, NULL, NULL, 1},
	{"unmount",	ZipFSUnmountObjCmd,	NULL, NULL, NULL, 1},
	{"mkkey",	ZipFSMkKeyObjCmd,	NULL, NULL, NULL, 1},
	{"exists",	ZipFSExistsObjCmd,	NULL, NULL, NULL, 0},
	{"info",	ZipFSInfoObjCmd,	NULL, NULL, NULL, 0},
	{"list",	ZipFSListObjCmd,	NULL, NULL, NULL, 0},
	{"canonical",	ZipFSCanonicalObjCmd,	NULL, NULL, NULL, 0},
	{"root",	ZipFSRootObjCmd,	NULL, NULL, NULL, 0},
	{NULL, NULL, NULL, NULL, NULL, 0}
    };
    static const char findproc[] =
	"namespace eval ::tcl::zipfs {}\n"
	"proc ::tcl::zipfs::Find dir {\n"
	"    set result {}\n"
	"    if {[catch {glob -directory $dir -nocomplain * .*} list]} {\n"
	"        return $result\n"
	"    }\n"
	"    foreach file $list {\n"
	"        if {[file tail $file] in {. ..}} {\n"
	"            continue\n"
	"        }\n"
	"        lappend result $file {*}[Find $file]\n"
	"    }\n"
	"    return $result\n"
	"}\n"
	"proc ::tcl::zipfs::find {directoryName} {\n"
	"    return [lsort [Find $directoryName]]\n"
	"}\n";

    /*
     * One-time initialization.
     */

    WriteLock();
    if (!ZipFS.initialized) {
	ZipfsSetup();
    }
    Unlock();

    if (interp) {
	Tcl_Command ensemble;
	Tcl_Obj *mapObj;

	Tcl_EvalEx(interp, findproc, -1, TCL_EVAL_GLOBAL);
	if (!Tcl_IsSafe(interp)) {
	    Tcl_LinkVar(interp, "::tcl::zipfs::wrmax", (char *) &ZipFS.wrmax,
		    TCL_LINK_INT);
	    Tcl_LinkVar(interp, "::tcl::zipfs::fallbackEntryEncoding",
		    (char *) &ZipFS.fallbackEntryEncoding, TCL_LINK_STRING);
	}
	ensemble = TclMakeEnsemble(interp, "zipfs",
		Tcl_IsSafe(interp) ? (initMap + 4) : initMap);

	/*
	 * Add the [zipfs find] subcommand.
	 */

	Tcl_GetEnsembleMappingDict(NULL, ensemble, &mapObj);
	Tcl_DictObjPut(NULL, mapObj, Tcl_NewStringObj("find", -1),
		Tcl_NewStringObj("::tcl::zipfs::find", -1));
	Tcl_CreateObjCommand(interp, "::tcl::zipfs::tcl_library_init",
		ZipFSTclLibraryObjCmd, NULL, NULL);
	Tcl_PkgProvide(interp, "tcl::zipfs", "2.0");
    }
    return TCL_OK;
#else /* !HAVE_ZLIB */
    ZIPFS_ERROR(interp, "no zlib available");
    ZIPFS_ERROR_CODE(interp, "NO_ZLIB");
    return TCL_ERROR;
#endif /* HAVE_ZLIB */
}

#ifdef HAVE_ZLIB

#if !defined(STATIC_BUILD)
static int
ZipfsAppHookFindTclInit(
    const char *archive)
{
    Tcl_Obj *vfsInitScript;
    int found;

    if (zipfs_literal_tcl_library) {
	return TCL_ERROR;
    }
    if (TclZipfs_Mount(NULL, ZIPFS_ZIP_MOUNT, archive, NULL)) {
	/* Either the file doesn't exist or it is not a zip archive */
	return TCL_ERROR;
    }

    TclNewLiteralStringObj(vfsInitScript, ZIPFS_ZIP_MOUNT "/init.tcl");
    Tcl_IncrRefCount(vfsInitScript);
    found = Tcl_FSAccess(vfsInitScript, F_OK);
    Tcl_DecrRefCount(vfsInitScript);
    if (found == 0) {
	zipfs_literal_tcl_library = ZIPFS_ZIP_MOUNT;
	return TCL_OK;
    }

    TclNewLiteralStringObj(vfsInitScript,
	    ZIPFS_ZIP_MOUNT "/tcl_library/init.tcl");
    Tcl_IncrRefCount(vfsInitScript);
    found = Tcl_FSAccess(vfsInitScript, F_OK);
    Tcl_DecrRefCount(vfsInitScript);
    if (found == 0) {
	zipfs_literal_tcl_library = ZIPFS_ZIP_MOUNT "/tcl_library";
	return TCL_OK;
    }

    return TCL_ERROR;
}
#endif

static void
ZipfsExitHandler(
    TCL_UNUSED(ClientData)
)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    if (ZipFS.initialized != -1) {
	hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
	if (hPtr == NULL) {
	    ZipfsFinalize();
	} else {
	    /* ZipFS.fallbackEntryEncoding was already freed by
	     * ZipfsMountExitHandler
	    */
	}
    }
}

static void
ZipfsFinalize(void) {
    Tcl_FSUnregister(&zipfsFilesystem);
    Tcl_DeleteHashTable(&ZipFS.fileHash);
    ckfree(ZipFS.fallbackEntryEncoding);
    ZipFS.initialized = -1;
}

static void
ZipfsMountExitHandler(
    ClientData clientData)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    ZipFile *zf = (ZipFile *) clientData;

    if (TCL_OK != TclZipfs_Unmount(NULL, zf->mountPoint)) {
	Tcl_Panic("tried to unmount busy filesystem");
    }

    hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
    if (hPtr == NULL) {
	ZipfsFinalize();
    }

}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_AppHook --
 *
 *	Performs the argument munging for the shell
 *
 *-------------------------------------------------------------------------
 */

const char *
TclZipfs_AppHook(
#ifdef SUPPORT_BUILTIN_ZIP_INSTALL
    int *argcPtr,		/* Pointer to argc */
#else
    TCL_UNUSED(int *), /*argcPtr*/
#endif
#ifdef _WIN32
    TCL_UNUSED(WCHAR ***)) /* argvPtr */
#else /* !_WIN32 */
    char ***argvPtr)		/* Pointer to argv */
#endif /* _WIN32 */
{
    const char *archive;
    const char *version = Tcl_InitSubsystems();

#ifdef _WIN32
    Tcl_FindExecutable(NULL);
#else
    Tcl_FindExecutable((*argvPtr)[0]);
#endif
    archive = Tcl_GetNameOfExecutable();
    TclZipfs_Init(NULL);

    /*
     * Look for init.tcl in one of the locations mounted later in this
     * function.
     */

    if (!TclZipfs_Mount(NULL, ZIPFS_APP_MOUNT, archive, NULL)) {
	int found;
	Tcl_Obj *vfsInitScript;

	TclNewLiteralStringObj(vfsInitScript, ZIPFS_APP_MOUNT "/main.tcl");
	Tcl_IncrRefCount(vfsInitScript);
	if (Tcl_FSAccess(vfsInitScript, F_OK) == 0) {
	    /*
	     * Startup script should be set before calling Tcl_AppInit
	     */

	    Tcl_SetStartupScript(vfsInitScript, NULL);
	} else {
	    Tcl_DecrRefCount(vfsInitScript);
	}

	/*
	 * Set Tcl Encodings
	 */

	if (!zipfs_literal_tcl_library) {
	    TclNewLiteralStringObj(vfsInitScript,
		    ZIPFS_APP_MOUNT "/tcl_library/init.tcl");
	    Tcl_IncrRefCount(vfsInitScript);
	    found = Tcl_FSAccess(vfsInitScript, F_OK);
	    Tcl_DecrRefCount(vfsInitScript);
	    if (found == TCL_OK) {
		zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
		return version;
	    }
	}
#ifdef SUPPORT_BUILTIN_ZIP_INSTALL
    } else if (*argcPtr > 1) {
	/*
	 * If the first argument is "install", run the supplied installer
	 * script.
	 */

#ifdef _WIN32
	Tcl_DString ds;

	Tcl_DStringInit(&ds);
	archive = Tcl_WCharToUtfDString((*argvPtr)[1], -1, &ds);
#else /* !_WIN32 */
	archive = (*argvPtr)[1];
#endif /* _WIN32 */
	if (strcmp(archive, "install") == 0) {
	    Tcl_Obj *vfsInitScript;

	    /*
	     * Run this now to ensure the file is present by the time Tcl_Main
	     * wants it.
	     */

	    TclZipfs_TclLibrary();
	    TclNewLiteralStringObj(vfsInitScript,
		    ZIPFS_ZIP_MOUNT "/tcl_library/install.tcl");
	    Tcl_IncrRefCount(vfsInitScript);
	    if (Tcl_FSAccess(vfsInitScript, F_OK) == 0) {
		Tcl_SetStartupScript(vfsInitScript, NULL);
	    }
	    return version;
	} else if (!TclZipfs_Mount(NULL, ZIPFS_APP_MOUNT, archive, NULL)) {
	    int found;
	    Tcl_Obj *vfsInitScript;

	    TclNewLiteralStringObj(vfsInitScript, ZIPFS_APP_MOUNT "/main.tcl");
	    Tcl_IncrRefCount(vfsInitScript);
	    if (Tcl_FSAccess(vfsInitScript, F_OK) == 0) {
		/*
		 * Startup script should be set before calling Tcl_AppInit
		 */

		Tcl_SetStartupScript(vfsInitScript, NULL);
	    } else {
		Tcl_DecrRefCount(vfsInitScript);
	    }
	    /* Set Tcl Encodings */
	    TclNewLiteralStringObj(vfsInitScript,
		    ZIPFS_APP_MOUNT "/tcl_library/init.tcl");
	    Tcl_IncrRefCount(vfsInitScript);
	    found = Tcl_FSAccess(vfsInitScript, F_OK);
	    Tcl_DecrRefCount(vfsInitScript);
	    if (found == TCL_OK) {
		zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
		return version;
	    }
	}
#ifdef _WIN32
	Tcl_DStringFree(&ds);
#endif /* _WIN32 */
#endif /* SUPPORT_BUILTIN_ZIP_INSTALL */
    }
    return version;
}

#else /* !HAVE_ZLIB */

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Mount, TclZipfs_MountBuffer, TclZipfs_Unmount --
 *
 *	Dummy version when no ZLIB support available.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_Mount(
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(const char *),	/* Mount point path. */
    TCL_UNUSED(const char *),	/* Path to ZIP file to mount. */
    TCL_UNUSED(const char *))		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
{
    ZIPFS_ERROR(interp, "no zlib available");
    ZIPFS_ERROR_CODE(interp, "NO_ZLIB");
    return TCL_ERROR;
}

int
TclZipfs_MountBuffer(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    TCL_UNUSED(const char *),	/* Mount point path. */
    TCL_UNUSED(unsigned char *),
    TCL_UNUSED(size_t),
    TCL_UNUSED(int))
{
    ZIPFS_ERROR(interp, "no zlib available");
    ZIPFS_ERROR_CODE(interp, "NO_ZLIB");
    return TCL_ERROR;
}

int
TclZipfs_Unmount(
    Tcl_Interp *interp,		/* Current interpreter. */
    TCL_UNUSED(const char *))	/* Mount point path. */
{
    ZIPFS_ERROR(interp, "no zlib available");
    ZIPFS_ERROR_CODE(interp, "NO_ZLIB");
    return TCL_ERROR;
}

const char *
TclZipfs_AppHook(
    TCL_UNUSED(int *), /*argcPtr*/
#ifdef _WIN32
    TCL_UNUSED(WCHAR ***)) /* argvPtr */
#else /* !_WIN32 */
    TCL_UNUSED(char ***))		/* Pointer to argv */
#endif /* _WIN32 */
{
    return NULL;
}

Tcl_Obj *
TclZipfs_TclLibrary(void)
{
    return NULL;
}

#endif /* !HAVE_ZLIB */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
