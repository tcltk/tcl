/*
 * tclZipfs.c --
 *
 *	Implementation of the ZIP filesystem used in TIP 430
 *	Adapted from the implentation for AndroWish.
 *
 * Copyright (c) 2016-2017 Sean Woods <yoda@etoyoc.com>
 * Copyright (c) 2013-2015 Christian Werner <chw@ch-werner.de>
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

#ifdef _WIN32
#include <winbase.h>
#else
#include <sys/mman.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifdef HAVE_ZLIB
#include "zlib.h"
#include "crypt.h"

#ifdef CFG_RUNTIME_DLLFILE
/*
** We are compiling as part of the core.
** TIP430 style zipfs prefix
*/
#define ZIPFS_VOLUME	  "//zipfs:/"
#define ZIPFS_VOLUME_LEN  9
#define ZIPFS_APP_MOUNT	  "//zipfs:/app"
#define ZIPFS_ZIP_MOUNT	  "//zipfs:/lib/tcl"
#else
/*
** We are compiling from the /compat folder of tclconfig
** Pre TIP430 style zipfs prefix
** //zipfs:/ doesn't work straight out of the box on either windows or Unix
** without other changes made to tip 430
*/
#define ZIPFS_VOLUME	  "zipfs:/"
#define ZIPFS_VOLUME_LEN  7
#define ZIPFS_APP_MOUNT	  "zipfs:/app"
#define ZIPFS_ZIP_MOUNT	  "zipfs:/lib/tcl"
#endif
/*
 * Various constants and offsets found in ZIP archive files
 */

#define ZIP_SIG_LEN			4

/* Local header of ZIP archive member (at very beginning of each member). */
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

/* Central header of ZIP archive member at end of ZIP file. */
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

/* Central end signature at very end of ZIP file. */
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

/* Macros to report errors only if an interp is present */
#define ZIPFS_ERROR(interp,errstr) \
    do {								\
	if (interp != NULL) {						\
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(errstr, -1));	\
	}								\
    } while (0)
#define ZIPFS_POSIX_ERROR(interp,errstr) \
    do {								\
	if (interp != NULL) {						\
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(			\
		    "%s: %s", errstr, Tcl_PosixError(interp)));		\
	}								\
    } while (0)

/*
 * Macros to read and write 16 and 32 bit integers from/to ZIP archives.
 */

#define ZipReadInt(p) \
    ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
#define ZipReadShort(p) \
    ((p)[0] | ((p)[1] << 8))

#define ZipWriteInt(p, v) \
    do {			     \
	(p)[0] = (v) & 0xff;	     \
	(p)[1] = ((v) >> 8) & 0xff;  \
	(p)[2] = ((v) >> 16) & 0xff; \
	(p)[3] = ((v) >> 24) & 0xff; \
    } while (0)
#define ZipWriteShort(p, v) \
    do {			    \
	(p)[0] = (v) & 0xff;	    \
	(p)[1] = ((v) >> 8) & 0xff; \
    } while (0)

/*
 * Windows drive letters.
 */

#ifdef _WIN32
static const char drvletters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
#endif

/*
 * Mutex to protect localtime(3) when no reentrant version available.
 */

#if !defined(_WIN32) && !defined(HAVE_LOCALTIME_R) && defined(TCL_THREADS)
TCL_DECLARE_MUTEX(localtimeMutex)
#endif

/*
 * In-core description of mounted ZIP archive file.
 */

typedef struct ZipFile {
    char *name;			/* Archive name */
    size_t nameLength;
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
    char *mountPoint;		/* Mount point */
    size_t mountPointLen;
#ifdef _WIN32
    HANDLE mountHandle;
#endif
} ZipFile;

/*
 * In-core description of file contained in mounted ZIP archive.
 */

typedef struct ZipEntry {
    char *name;			/* The full pathname of the virtual file */
    ZipFile *zipfile;		/* The ZIP file holding this virtual file */
    Tcl_WideInt offset;		/* Data offset into memory mapped ZIP file */
    int nbyte;			/* Uncompressed size of the virtual file */
    int nbytecompr;		/* Compressed size of the virtual file */
    int cmeth;			/* Compress method */
    int isdir;			/* Set to 1 if directory, or -1 if root */
    int depth;			/* Number of slashes in path. */
    int crc32;			/* CRC-32 */
    int timestamp;		/* Modification time */
    int isenc;			/* True if data is encrypted */
    unsigned char *data;	/* File data if written */
    struct ZipEntry *next;	/* Next file in the same archive */
    struct ZipEntry *tnext;	/* Next top-level dir in archive */
} ZipEntry;

/*
 * File channel for file contained in mounted ZIP archive.
 */

typedef struct ZipChannel {
    ZipFile *zipfile;		/* The ZIP file holding this channel */
    ZipEntry *zipentry;		/* Pointer back to virtual file */
    size_t nmax;		/* Maximum size for write */
    size_t nbyte;		/* Number of bytes of uncompressed data */
    size_t nread;		/* Position of next byte to be read from the
				 * channel */
    unsigned char *ubuf;	/* Pointer to the uncompressed data */
    int iscompr;		/* True if data is compressed */
    int isdir;			/* Set to 1 if directory, or -1 if root */
    int isenc;			/* True if data is encrypted */
    int iswr;			/* True if open for writing */
    unsigned long keys[3];	/* Key for decryption */
} ZipChannel;

/*
 * Global variables.
 *
 * Most are kept in single ZipFS struct. When build with threading support
 * this struct is protected by the ZipFSMutex (see below).
 *
 * The "fileHash" component is the process wide global table of all known ZIP
 * archive members in all mounted ZIP archives.
 *
 * The "zipHash" components is the process wide global table of all mounted
 * ZIP archive files.
 */

static struct {
    int initialized;		/* True when initialized */
    int lock;			/* RW lock, see below */
    int waiters;		/* RW lock, see below */
    int wrmax;			/* Maximum write size of a file */
    int idCount;		/* Counter for channel names */
    Tcl_HashTable fileHash;	/* File name to ZipEntry mapping */
    Tcl_HashTable zipHash;	/* Mount to ZipFile mapping */
} ZipFS = {
    0, 0, 0, 0, 0,
};

/*
 * For password rotation.
 */

static const char pwrot[16] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0
};

/*
 * Table to compute CRC32.
 */

static const z_crc_t crc32tab[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
    0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
    0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
    0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
    0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
    0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
    0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
    0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
    0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
    0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
    0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
    0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
    0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
    0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
    0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
    0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
    0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
    0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
    0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
    0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
    0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
    0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
    0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
    0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
    0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
    0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
    0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
    0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
    0x2d02ef8d,
};

const char *zipfs_literal_tcl_library = NULL;

/* Function prototypes */

int			TclZipfs_Mount(Tcl_Interp *interp,
			    const char *mountPoint, const char *zipname,
			    const char *passwd);
int			TclZipfs_Mount_Buffer(Tcl_Interp *interp,
			    const char *mountPoint, unsigned char *data,
			    size_t datalen, int copy);
static int		ZipfsAppHookFindTclInit(const char *archive);
static int		ZipFSPathInFilesystemProc(Tcl_Obj *pathPtr,
			    ClientData *clientDataPtr);
static Tcl_Obj *	ZipFSFilesystemPathTypeProc(Tcl_Obj *pathPtr);
static Tcl_Obj *	ZipFSFilesystemSeparatorProc(Tcl_Obj *pathPtr);
static int		ZipFSStatProc(Tcl_Obj *pathPtr, Tcl_StatBuf *buf);
static int		ZipFSAccessProc(Tcl_Obj *pathPtr, int mode);
static Tcl_Channel	ZipFSOpenFileChannelProc(Tcl_Interp *interp,
			    Tcl_Obj *pathPtr, int mode, int permissions);
static int		ZipFSMatchInDirectoryProc(Tcl_Interp *interp,
			    Tcl_Obj *result, Tcl_Obj *pathPtr,
			    const char *pattern, Tcl_GlobTypeData *types);
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
static void		TclZipfs_C_Init(void);

/*
 * Define the ZIP filesystem dispatch table.
 */

MODULE_SCOPE const Tcl_Filesystem zipfsFilesystem;

const Tcl_Filesystem zipfsFilesystem = {
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
    (Tcl_FSLoadFileProc *) ZipFSLoadFile,
    NULL, /* getCwdProc */
    NULL, /* chdirProc */
};

/*
 * Miscellaneous constants.
 */

#define ERROR_LENGTH	((size_t) -1)

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

#ifdef TCL_THREADS

static Tcl_Condition ZipFSCond;

static void
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

static void
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

static void
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

    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = ((dosDate & 0xfe00) >> 9) + 80;
    tm.tm_mon = ((dosDate & 0x1e0) >> 5) - 1;
    tm.tm_mday = dosDate & 0x1f;
    tm.tm_hour = (dosTime & 0xf800) >> 11;
    tm.tm_min = (dosTime & 0x7e) >> 5;
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

#if !defined(TCL_THREADS)
    /* Not threaded */
    tmp = localtime(&when);
    tm = *tmp;
#elif defined(_WIN32)
    /* Win32 uses thread local storage */
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

#if !defined(TCL_THREADS)
    /* Not threaded */
    tmp = localtime(&when);
    tm = *tmp;
#elif /* TCL_THREADS && */ defined(_WIN32)
    /* Win32 uses thread local storage */
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

static int
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
    int ZIPFSPATH)
{
    char *path;
    int i, j, c, isUNC = 0, isVfs = 0, n = 0;
    int haveZipfsPath = 1;

#ifdef _WIN32
    if ((tail[0] != '\0') && (strchr(drvletters, tail[0]) != NULL)
	    && (tail[1] == ':')) {
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
	if (ZIPFSPATH) {
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
#endif

    if (ZIPFSPATH) {
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
 *	archive member of the given file name.
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

static ZipEntry *
ZipFSLookup(
    char *filename)
{
    Tcl_HashEntry *hPtr;
    ZipEntry *z = NULL;

    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, filename);
    if (hPtr != NULL) {
	z = Tcl_GetHashValue(hPtr);
    }
    return z;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLookupMount --
 *
 *	This function returns an indication if the given file name corresponds
 *	to a mounted ZIP archive file.
 *
 * Results:
 *	Returns true, if the given file name is a mounted ZIP archive file.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

#ifdef NEVER_USED
static int
ZipFSLookupMount(
    char *filename)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;

    hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
    while (hPtr != NULL) {
	ZipFile *zf = Tcl_GetHashValue(hPtr);

	if (strcmp(zf->mountPoint, filename) == 0) {
	    return 1;
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
    return 0;
}
#endif /* NEVER_USED */

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
	if (zf->ptrToFree != NULL) {
	    ckfree(zf->ptrToFree);
	    zf->ptrToFree = NULL;
	}
	zf->data = NULL;
	return;
    }

#ifdef _WIN32
    if ((zf->data != NULL) && (zf->ptrToFree == NULL)) {
	UnmapViewOfFile(zf->data);
	zf->data = NULL;
    }
    if (zf->mountHandle != INVALID_HANDLE_VALUE) {
	CloseHandle(zf->mountHandle);
    }
#else /* !_WIN32 */
    if ((zf->data != MAP_FAILED) && (zf->ptrToFree == NULL)) {
	munmap(zf->data, zf->length);
	zf->data = MAP_FAILED;
    }
#endif /* _WIN32 */

    if (zf->ptrToFree != NULL) {
	ckfree(zf->ptrToFree);
	zf->ptrToFree = NULL;
    }
    if (zf->chan != NULL) {
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
 *	is accepted.
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
    size_t i;
    unsigned char *p, *q;

    p = zf->data + zf->length - ZIP_CENTRAL_END_LEN;
    while (p >= zf->data) {
	if (*p == (ZIP_CENTRAL_END_SIG & 0xFF)) {
	    if (ZipReadInt(p) == ZIP_CENTRAL_END_SIG) {
		break;
	    }
	    p -= ZIP_SIG_LEN;
	} else {
	    --p;
	}
    }
    if (p < zf->data) {
	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "wrong end signature");
	goto error;
    }
    zf->numFiles = ZipReadShort(p + ZIP_CENTRAL_ENTS_OFFS);
    if (zf->numFiles == 0) {
	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "empty archive");
	goto error;
    }
    q = zf->data + ZipReadInt(p + ZIP_CENTRAL_DIRSTART_OFFS);
    p -= ZipReadInt(p + ZIP_CENTRAL_DIRSIZE_OFFS);
    if ((p < zf->data) || (p > zf->data + zf->length)
	    || (q < zf->data) || (q > zf->data + zf->length)) {
	if (!needZip) {
	    zf->baseOffset = zf->passOffset = zf->length;
	    return TCL_OK;
	}
	ZIPFS_ERROR(interp, "archive directory not found");
	goto error;
    }
    zf->baseOffset = zf->passOffset = p - q;
    zf->directoryOffset = p - zf->data;
    q = p;
    for (i = 0; i < zf->numFiles; i++) {
	int pathlen, comlen, extra;

	if (q + ZIP_CENTRAL_HEADER_LEN > zf->data + zf->length) {
	    ZIPFS_ERROR(interp, "wrong header length");
	    goto error;
	}
	if (ZipReadInt(q) != ZIP_CENTRAL_HEADER_SIG) {
	    ZIPFS_ERROR(interp, "wrong header signature");
	    goto error;
	}
	pathlen = ZipReadShort(q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = ZipReadShort(q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = ZipReadShort(q + ZIP_CENTRAL_EXTRALEN_OFFS);
	q += pathlen + comlen + extra + ZIP_CENTRAL_HEADER_LEN;
    }
    q = zf->data + zf->baseOffset;
    if ((zf->baseOffset >= 6) && (ZipReadInt(q - 4) == ZIP_PASSWORD_END_SIG)) {
	i = q[-5];
	if (q - 5 - i > zf->data) {
	    zf->passBuf[0] = i;
	    memcpy(zf->passBuf + 1, q - 5 - i, i);
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
    ClientData handle;

    zf->nameLength = 0;
    zf->isMemBuffer = 0;
#ifdef _WIN32
    zf->data = NULL;
    zf->mountHandle = INVALID_HANDLE_VALUE;
#else /* !_WIN32 */
    zf->data = MAP_FAILED;
#endif /* _WIN32 */
    zf->length = 0;
    zf->numFiles = 0;
    zf->baseOffset = zf->passOffset = 0;
    zf->ptrToFree = NULL;
    zf->passBuf[0] = 0;
    zf->chan = Tcl_OpenFileChannel(interp, zipname, "rb", 0);
    if (zf->chan == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetChannelHandle(zf->chan, TCL_READABLE, &handle) != TCL_OK) {
	zf->length = Tcl_Seek(zf->chan, 0, SEEK_END);
	if (zf->length == ERROR_LENGTH) {
	    ZIPFS_POSIX_ERROR(interp, "seek error");
	    goto error;
	}
	if ((zf->length - ZIP_CENTRAL_END_LEN)
		> (64 * 1024 * 1024 - ZIP_CENTRAL_END_LEN)) {
	    ZIPFS_ERROR(interp, "illegal file size");
	    goto error;
	}
	if (Tcl_Seek(zf->chan, 0, SEEK_SET) == -1) {
	    ZIPFS_POSIX_ERROR(interp, "seek error");
	    goto error;
	}
	zf->ptrToFree = zf->data = attemptckalloc(zf->length);
	if (zf->ptrToFree == NULL) {
	    ZIPFS_ERROR(interp, "out of memory");
	    goto error;
	}
	i = Tcl_Read(zf->chan, (char *) zf->data, zf->length);
	if (i != zf->length) {
	    ZIPFS_POSIX_ERROR(interp, "file read error");
	    goto error;
	}
	Tcl_Close(interp, zf->chan);
	zf->chan = NULL;
    } else {
#ifdef _WIN32
	int readSuccessful;
#   ifdef _WIN64
	i = GetFileSizeEx((HANDLE) handle, (PLARGE_INTEGER) &zf->length);
	readSuccessful = (i != 0);
#   else /* !_WIN64 */
	zf->length = GetFileSize((HANDLE) handle, 0);
	readSuccessful = (zf->length != (size_t) INVALID_FILE_SIZE);
#   endif /* _WIN64 */
	if (!readSuccessful || (zf->length < ZIP_CENTRAL_END_LEN)) {
	    ZIPFS_POSIX_ERROR(interp, "invalid file size");
	    goto error;
	}
	zf->mountHandle = CreateFileMapping((HANDLE) handle, 0, PAGE_READONLY,
		0, zf->length, 0);
	if (zf->mountHandle == INVALID_HANDLE_VALUE) {
	    ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	    goto error;
	}
	zf->data = MapViewOfFile(zf->mountHandle, FILE_MAP_READ, 0, 0,
		zf->length);
	if (zf->data == NULL) {
	    ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	    goto error;
	}
#else /* !_WIN32 */
	zf->length = lseek(PTR2INT(handle), 0, SEEK_END);
	if (zf->length == ERROR_LENGTH || zf->length < ZIP_CENTRAL_END_LEN) {
	    ZIPFS_POSIX_ERROR(interp, "invalid file size");
	    goto error;
	}
	lseek(PTR2INT(handle), 0, SEEK_SET);
	zf->data = (unsigned char *) mmap(0, zf->length, PROT_READ,
		MAP_FILE | MAP_PRIVATE, PTR2INT(handle), 0);
	if (zf->data == MAP_FAILED) {
	    ZIPFS_POSIX_ERROR(interp, "file mapping failed");
	    goto error;
	}
#endif /* _WIN32 */
    }
    return ZipFSFindTOC(interp, needZip, zf);

  error:
    ZipFSCloseArchive(interp, zf);
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSRootNode --
 *
 *	This function generates the root node for a ZIPFS filesystem.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with an error message placed
 *	into the given "interp" if it is not NULL.
 *
 * Side effects:
 *	...
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSCatalogFilesystem(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    ZipFile *zf0,
    const char *mountPoint,	/* Mount point path. */
    const char *passwd,		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
    const char *zipname)	/* Path to ZIP file to build a catalog of. */
{
    int pwlen, isNew;
    size_t i;
    ZipFile *zf;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds, dsm, fpBuf;
    unsigned char *q;

    /*
     * Basic verification of the password for sanity.
     */

    pwlen = 0;
    if (passwd != NULL) {
	pwlen = strlen(passwd);
	if ((pwlen > 255) || (strchr(passwd, 0xff) != NULL)) {
	    if (interp) {
		Tcl_SetObjResult(interp,
			Tcl_NewStringObj("illegal password", -1));
	    }
	    return TCL_ERROR;
	}
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
	if (interp != NULL) {
	    zf = Tcl_GetHashValue(hPtr);
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "%s is already mounted on %s", zf->name, mountPoint));
	}
	Unlock();
	ZipFSCloseArchive(interp, zf0);
	return TCL_ERROR;
    }
    zf = attemptckalloc(sizeof(ZipFile) + strlen(mountPoint) + 1);
    if (zf == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "out of memory", (char *) NULL);
	}
	Unlock();
	ZipFSCloseArchive(interp, zf0);
	return TCL_ERROR;
    }
    Unlock();

    *zf = *zf0;
    zf->mountPoint = Tcl_GetHashKey(&ZipFS.zipHash, hPtr);
    zf->mountPointLen = strlen(zf->mountPoint);
    zf->nameLength = strlen(zipname);
    zf->name = ckalloc(zf->nameLength + 1);
    memcpy(zf->name, zipname, zf->nameLength + 1);
    zf->entries = NULL;
    zf->topEnts = NULL;
    zf->numOpen = 0;
    Tcl_SetHashValue(hPtr, zf);
    if ((zf->passBuf[0] == 0) && pwlen) {
	int k = 0;

	i = pwlen;
	zf->passBuf[k++] = i;
	while (i > 0) {
	    zf->passBuf[k] = (passwd[i - 1] & 0x0f)
		    | pwrot[(passwd[i - 1] >> 4) & 0x0f];
	    k++;
	    i--;
	}
	zf->passBuf[k] = '\0';
    }
    if (mountPoint[0] != '\0') {
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, mountPoint, &isNew);
	if (isNew) {
	    z = ckalloc(sizeof(ZipEntry));
	    Tcl_SetHashValue(hPtr, z);

	    z->tnext = NULL;
	    z->depth = CountSlashes(mountPoint);
	    z->zipfile = zf;
	    z->isdir = (zf->baseOffset == 0) ? 1 : -1; /* root marker */
	    z->isenc = 0;
	    z->offset = zf->baseOffset;
	    z->crc32 = 0;
	    z->timestamp = 0;
	    z->nbyte = z->nbytecompr = 0;
	    z->cmeth = ZIP_COMPMETH_STORED;
	    z->data = NULL;
	    z->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	    z->next = zf->entries;
	    zf->entries = z;
	}
    }
    q = zf->data + zf->directoryOffset;
    Tcl_DStringInit(&fpBuf);
    for (i = 0; i < zf->numFiles; i++) {
	int extra, isdir = 0, dosTime, dosDate, nbcompr;
	size_t offs, pathlen, comlen;
	unsigned char *lq, *gq = NULL;
	char *fullpath, *path;

	pathlen = ZipReadShort(q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = ZipReadShort(q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = ZipReadShort(q + ZIP_CENTRAL_EXTRALEN_OFFS);
	Tcl_DStringSetLength(&ds, 0);
	Tcl_DStringAppend(&ds, (char *) q + ZIP_CENTRAL_HEADER_LEN, pathlen);
	path = Tcl_DStringValue(&ds);
	if ((pathlen > 0) && (path[pathlen - 1] == '/')) {
	    Tcl_DStringSetLength(&ds, pathlen - 1);
	    path = Tcl_DStringValue(&ds);
	    isdir = 1;
	}
	if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
	    goto nextent;
	}
	lq = zf->data + zf->baseOffset
		+ ZipReadInt(q + ZIP_CENTRAL_LOCALHDR_OFFS);
	if ((lq < zf->data) || (lq > zf->data + zf->length)) {
	    goto nextent;
	}
	nbcompr = ZipReadInt(lq + ZIP_LOCAL_COMPLEN_OFFS);
	if (!isdir && (nbcompr == 0)
		&& (ZipReadInt(lq + ZIP_LOCAL_UNCOMPLEN_OFFS) == 0)
		&& (ZipReadInt(lq + ZIP_LOCAL_CRC32_OFFS) == 0)) {
	    gq = q;
	    nbcompr = ZipReadInt(gq + ZIP_CENTRAL_COMPLEN_OFFS);
	}
	offs = (lq - zf->data)
		+ ZIP_LOCAL_HEADER_LEN
		+ ZipReadShort(lq + ZIP_LOCAL_PATHLEN_OFFS)
		+ ZipReadShort(lq + ZIP_LOCAL_EXTRALEN_OFFS);
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
	    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, Tcl_DStringValue(&ds2));
	    if (hPtr != NULL) {
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
	z = ckalloc(sizeof(ZipEntry));
	z->name = NULL;
	z->tnext = NULL;
	z->depth = CountSlashes(fullpath);
	z->zipfile = zf;
	z->isdir = isdir;
	z->isenc = (ZipReadShort(lq + ZIP_LOCAL_FLAGS_OFFS) & 1)
		&& (nbcompr > 12);
	z->offset = offs;
	if (gq != NULL) {
	    z->crc32 = ZipReadInt(gq + ZIP_CENTRAL_CRC32_OFFS);
	    dosDate = ZipReadShort(gq + ZIP_CENTRAL_MDATE_OFFS);
	    dosTime = ZipReadShort(gq + ZIP_CENTRAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->nbyte = ZipReadInt(gq + ZIP_CENTRAL_UNCOMPLEN_OFFS);
	    z->cmeth = ZipReadShort(gq + ZIP_CENTRAL_COMPMETH_OFFS);
	} else {
	    z->crc32 = ZipReadInt(lq + ZIP_LOCAL_CRC32_OFFS);
	    dosDate = ZipReadShort(lq + ZIP_LOCAL_MDATE_OFFS);
	    dosTime = ZipReadShort(lq + ZIP_LOCAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->nbyte = ZipReadInt(lq + ZIP_LOCAL_UNCOMPLEN_OFFS);
	    z->cmeth = ZipReadShort(lq + ZIP_LOCAL_COMPMETH_OFFS);
	}
	z->nbytecompr = nbcompr;
	z->data = NULL;
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, fullpath, &isNew);
	if (!isNew) {
	    /* should not happen but skip it anyway */
	    ckfree(z);
	} else {
	    Tcl_SetHashValue(hPtr, z);
	    z->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	    z->next = zf->entries;
	    zf->entries = z;
	    if (isdir && (mountPoint[0] == '\0') && (z->depth == 1)) {
		z->tnext = zf->topEnts;
		zf->topEnts = z;
	    }
	    if (!z->isdir && (z->depth > 1)) {
		char *dir, *end;
		ZipEntry *zd;

		Tcl_DStringSetLength(&ds, strlen(z->name) + 8);
		Tcl_DStringSetLength(&ds, 0);
		Tcl_DStringAppend(&ds, z->name, -1);
		dir = Tcl_DStringValue(&ds);
		end = strrchr(dir, '/');
		while ((end != NULL) && (end != dir)) {
		    Tcl_DStringSetLength(&ds, end - dir);
		    hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, dir, &isNew);
		    if (!isNew) {
			break;
		    }
		    zd = ckalloc(sizeof(ZipEntry));
		    zd->name = NULL;
		    zd->tnext = NULL;
		    zd->depth = CountSlashes(dir);
		    zd->zipfile = zf;
		    zd->isdir = 1;
		    zd->isenc = 0;
		    zd->offset = z->offset;
		    zd->crc32 = 0;
		    zd->timestamp = z->timestamp;
		    zd->nbyte = zd->nbytecompr = 0;
		    zd->cmeth = ZIP_COMPMETH_STORED;
		    zd->data = NULL;
		    Tcl_SetHashValue(hPtr, zd);
		    zd->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
		    zd->next = zf->entries;
		    zf->entries = zd;
		    if ((mountPoint[0] == '\0') && (zd->depth == 1)) {
			zd->tnext = zf->topEnts;
			zf->topEnts = zd;
		    }
		    end = strrchr(dir, '/');
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
 * TclZipfs_C_Init --
 *
 *	Common initialisation code. ZipFS.initialized must *not* be set prior
 *	to the call.
 *
 *-------------------------------------------------------------------------
 */

static void
TclZipfs_C_Init(void)
{
#ifdef TCL_THREADS
    static const Tcl_Time t = { 0, 0 };

    /*
     * Inflate condition variable.
     */

    Tcl_MutexLock(&ZipFSMutex);
    Tcl_ConditionWait(&ZipFSCond, &ZipFSMutex, &t);
    Tcl_MutexUnlock(&ZipFSMutex);
#endif /* TCL_THREADS */

    Tcl_FSRegister(NULL, &zipfsFilesystem);
    Tcl_InitHashTable(&ZipFS.fileHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&ZipFS.zipHash, TCL_STRING_KEYS);
    ZipFS.idCount = 1;
    ZipFS.initialized = 1;
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
    const char *zipname,	/* Path to ZIP file to mount. */
    const char *passwd)		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
{
    int i, pwlen;
    ZipFile *zf;

    ReadLock();
    if (!ZipFS.initialized) {
	TclZipfs_C_Init();
    }
    if (mountPoint == NULL) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int ret = TCL_OK;

	i = 0;
	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    zf = Tcl_GetHashValue(hPtr);
	    if (interp != NULL) {
		Tcl_AppendElement(interp, zf->mountPoint);
		Tcl_AppendElement(interp, zf->name);
	    }
	    ++i;
	    hPtr = Tcl_NextHashEntry(&search);
	}
	if (interp == NULL) {
	    ret = (i > 0) ? TCL_OK : TCL_BREAK;
	}
	Unlock();
	return ret;
    }

    if (zipname == NULL) {
	if (interp != NULL) {
	    Tcl_HashEntry *hPtr =
		    Tcl_FindHashEntry(&ZipFS.zipHash, mountPoint);

	    if (hPtr != NULL) {
		zf = Tcl_GetHashValue(hPtr);
		Tcl_SetObjResult(interp, Tcl_NewStringObj(zf->name, -1));
	    }
	}
	Unlock();
	return TCL_OK;
    }
    Unlock();
    pwlen = 0;
    if (passwd != NULL) {
	pwlen = strlen(passwd);
	if ((pwlen > 255) || (strchr(passwd, 0xff) != NULL)) {
	    if (interp) {
		Tcl_SetObjResult(interp,
			Tcl_NewStringObj("illegal password", -1));
	    }
	    return TCL_ERROR;
	}
    }
    zf = attemptckalloc(sizeof(ZipFile) + strlen(mountPoint) + 1);
    if (zf == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "out of memory", (char *) NULL);
	}
	return TCL_ERROR;
    }
    if (ZipFSOpenArchive(interp, zipname, 1, zf) != TCL_OK) {
	return TCL_ERROR;
    }
    return ZipFSCatalogFilesystem(interp, zf, mountPoint, passwd, zipname);
}

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Mount_Buffer --
 *
 *	This procedure is invoked to mount a given ZIP archive file on
 *	a given mountpoint with optional ZIP password.
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
TclZipfs_Mount_Buffer(
    Tcl_Interp *interp,		/* Current interpreter. NULLable. */
    const char *mountPoint,	/* Mount point path. */
    unsigned char *data,
    size_t datalen,
    int copy)
{
    ZipFile *zf;

    ReadLock();
    if (!ZipFS.initialized) {
	TclZipfs_C_Init();
    }
    if (mountPoint == NULL) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int ret = (interp ? TCL_BREAK : TCL_OK);

	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    zf = Tcl_GetHashValue(hPtr);
	    if (interp != NULL) {
		Tcl_AppendElement(interp, zf->mountPoint);
		Tcl_AppendElement(interp, zf->name);
	    } else {
		ret = TCL_OK;
		/*
		 * Stop searching here if we're not returning a list.
		 */
		break;
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	Unlock();
	return ret;
    }

    if (data == NULL) {
	if (interp != NULL) {
	    Tcl_HashEntry *hPtr =
		    Tcl_FindHashEntry(&ZipFS.zipHash, mountPoint);

	    if (hPtr != NULL) {
		zf = Tcl_GetHashValue(hPtr);
		Tcl_SetObjResult(interp, Tcl_NewStringObj(zf->name, -1));
	    }
	}
	Unlock();
	return TCL_OK;
    }
    Unlock();

    zf = attemptckalloc(sizeof(ZipFile) + strlen(mountPoint) + 1);
    if (zf == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "out of memory", (char *) NULL);
	}
	return TCL_ERROR;
    }
    zf->isMemBuffer = 1;
    zf->length = datalen;
    if (copy) {
	zf->data = attemptckalloc(datalen);
	if (zf->data == NULL) {
	    if (interp != NULL) {
		Tcl_AppendResult(interp, "out of memory", (char *) NULL);
	    }
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
    return ZipFSCatalogFilesystem(interp, zf, mountPoint, NULL,
	    "Memory Buffer");
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
    if (hPtr == NULL) {
	goto done;
    }

    zf = Tcl_GetHashValue(hPtr);
    if (zf->numOpen > 0) {
	ZIPFS_ERROR(interp, "filesystem is busy");
	ret = TCL_ERROR;
	goto done;
    }
    Tcl_DeleteHashEntry(hPtr);
    for (z = zf->entries; z; z = znext) {
	znext = z->next;
	hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, z->name);
	if (hPtr) {
	    Tcl_DeleteHashEntry(hPtr);
	}
	if (z->data != NULL) {
	    ckfree(z->data);
	}
	ckfree(z);
    }
    ZipFSCloseArchive(interp, zf);
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
 *	This procedure is invoked to process the "zipfs::mount" command.
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv,
		 "?mountpoint? ?zipfile? ?password?");
	return TCL_ERROR;
    }

    return TclZipfs_Mount(interp, (objc > 1) ? Tcl_GetString(objv[1]) : NULL,
	    (objc > 2) ? Tcl_GetString(objv[2]) : NULL,
	    (objc > 3) ? Tcl_GetString(objv[3]) : NULL);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMountObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::mount" command.
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    const char *mountPoint;	/* Mount point path. */
    unsigned char *data;
    int length;

    if (objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "?mountpoint? ?data?");
	return TCL_ERROR;
    }
    if (objc < 2) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	int ret = TCL_OK;

	ReadLock();
	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    ZipFile *zf = Tcl_GetHashValue(hPtr);

	    Tcl_AppendElement(interp, zf->mountPoint);
	    Tcl_AppendElement(interp, zf->name);
	    hPtr = Tcl_NextHashEntry(&search);
	}
	Unlock();
	return ret;
    }

    mountPoint = Tcl_GetString(objv[1]);
    if (objc < 3) {
	Tcl_HashEntry *hPtr;

	ReadLock();
	hPtr = Tcl_FindHashEntry(&ZipFS.zipHash, mountPoint);
	if (hPtr != NULL) {
	    ZipFile *zf = Tcl_GetHashValue(hPtr);

	    Tcl_SetObjResult(interp, Tcl_NewStringObj(zf->name, -1));
	}
	Unlock();
	return TCL_OK;
    }

    data = Tcl_GetByteArrayFromObj(objv[2], &length);
    return TclZipfs_Mount_Buffer(interp, mountPoint, data, length, 1);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSRootObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::root" command. It
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj(ZIPFS_VOLUME, -1));
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSUnmountObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::unmount" command.
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
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
 *	This procedure is invoked to process the "zipfs::mkkey" command.  It
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    int len, i = 0;
    char *pw, passBuf[264];

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "password");
	return TCL_ERROR;
    }
    pw = Tcl_GetString(objv[1]);
    len = strlen(pw);
    if (len == 0) {
	return TCL_OK;
    }
    if ((len > 255) || (strchr(pw, 0xff) != NULL)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("illegal password", -1));
	return TCL_ERROR;
    }
    while (len > 0) {
	int ch = pw[len - 1];

	passBuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	i++;
	len--;
    }
    passBuf[i] = i;
    ++i;
    passBuf[i++] = (char) ZIP_PASSWORD_END_SIG;
    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 8);
    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 16);
    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 24);
    passBuf[i] = '\0';
    Tcl_AppendResult(interp, passBuf, (char *) NULL);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipAddFile --
 *
 *	This procedure is used by ZipFSMkZipOrImgCmd() to add a single file to
 *	the output ZIP archive file being written. A ZipEntry struct about the
 *	input file is added to the given fileHash table for later creation of
 *	the central ZIP directory.
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
    const char *path,
    const char *name,
    Tcl_Channel out,
    const char *passwd,		/* Password for encoding the file, or NULL if
				 * the file is to be unprotected. */
    char *buf,
    int bufsize,
    Tcl_HashTable *fileHash)
{
    Tcl_Channel in;
    Tcl_HashEntry *hPtr;
    ZipEntry *z;
    z_stream stream;
    const char *zpath;
    int crc, flush, zpathlen;
    size_t nbyte, nbytecompr, len, olen, align = 0;
    Tcl_WideInt pos[3];
    int mtime = 0, isNew, cmeth;
    unsigned long keys[3], keys0[3];
    char obuf[4096];

    zpath = name;
    while (zpath != NULL && zpath[0] == '/') {
	zpath++;
    }
    if ((zpath == NULL) || (zpath[0] == '\0')) {
	return TCL_OK;
    }
    zpathlen = strlen(zpath);
    if (zpathlen + ZIP_CENTRAL_HEADER_LEN > bufsize) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"path too long for \"%s\"", path));
	return TCL_ERROR;
    }
    in = Tcl_OpenFileChannel(interp, path, "rb", 0);
    if (in == NULL) {
#ifdef _WIN32
	/* hopefully a directory */
	if (strcmp("permission denied", Tcl_PosixError(interp)) == 0) {
	    Tcl_Close(interp, in);
	    return TCL_OK;
	}
#endif /* _WIN32 */
	Tcl_Close(interp, in);
	return TCL_ERROR;
    } else {
	Tcl_Obj *pathObj = Tcl_NewStringObj(path, -1);
	Tcl_StatBuf statBuf;

	Tcl_IncrRefCount(pathObj);
	if (Tcl_FSStat(pathObj, &statBuf) != -1) {
	    mtime = statBuf.st_mtime;
	}
	Tcl_DecrRefCount(pathObj);
    }
    Tcl_ResetResult(interp);
    crc = 0;
    nbyte = nbytecompr = 0;
    while ((len = Tcl_Read(in, buf, bufsize)) + 1 > 1) {
	crc = crc32(crc, (unsigned char *) buf, len);
	nbyte += len;
    }
    if (len == ERROR_LENGTH) {
	if (nbyte == 0 && errno == EISDIR) {
	    Tcl_Close(interp, in);
	    return TCL_OK;
	}
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("read error on \"%s\": %s",
		path, Tcl_PosixError(interp)));
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    if (Tcl_Seek(in, 0, SEEK_SET) == -1) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("seek error on \"%s\": %s",
		path, Tcl_PosixError(interp)));
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    pos[0] = Tcl_Tell(out);
    memset(buf, '\0', ZIP_LOCAL_HEADER_LEN);
    memcpy(buf + ZIP_LOCAL_HEADER_LEN, zpath, zpathlen);
    len = zpathlen + ZIP_LOCAL_HEADER_LEN;
    if ((size_t) Tcl_Write(out, buf, len) != len) {
    wrerr:
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"write error on %s: %s", path, Tcl_PosixError(interp)));
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    if ((len + pos[0]) & 3) {
	unsigned char abuf[8];

	/*
	 * Align payload to next 4-byte boundary using a dummy extra entry
	 * similar to the zipalign tool from Android's SDK.
	 */

	align = 4 + ((len + pos[0]) & 3);
	ZipWriteShort(abuf, 0xffff);
	ZipWriteShort(abuf + 2, align - 4);
	ZipWriteInt(abuf + 4, 0x03020100);
	if ((size_t) Tcl_Write(out, (const char *) abuf, align) != align) {
	    goto wrerr;
	}
    }
    if (passwd != NULL) {
	int i, ch, tmp;
	unsigned char kvbuf[24];
	Tcl_Obj *ret;

	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    double r;

	    if (Tcl_EvalEx(interp, "::tcl::mathfunc::rand", -1, 0) != TCL_OK) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("PRNG error: %s",
			Tcl_GetString(Tcl_GetObjResult(interp))));
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    ret = Tcl_GetObjResult(interp);
	    if (Tcl_GetDoubleFromObj(interp, ret, &r) != TCL_OK) {
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    ch = (int) (r * 256);
	    kvbuf[i + 12] = (unsigned char) zencode(keys, crc32tab, ch, tmp);
	}
	Tcl_ResetResult(interp);
	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    kvbuf[i] = (unsigned char)
		    zencode(keys, crc32tab, kvbuf[i + 12], tmp);
	}
	kvbuf[i++] = (unsigned char) zencode(keys, crc32tab, crc >> 16, tmp);
	kvbuf[i++] = (unsigned char) zencode(keys, crc32tab, crc >> 24, tmp);
	len = Tcl_Write(out, (char *) kvbuf, 12);
	memset(kvbuf, 0, 24);
	if (len != 12) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "write error on %s: %s", path, Tcl_PosixError(interp)));
	    Tcl_Close(interp, in);
	    return TCL_ERROR;
	}
	memcpy(keys0, keys, sizeof(keys0));
	nbytecompr += 12;
    }
    Tcl_Flush(out);
    pos[2] = Tcl_Tell(out);
    cmeth = ZIP_COMPMETH_DEFLATED;
    memset(&stream, 0, sizeof(z_stream));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if (deflateInit2(&stream, 9, Z_DEFLATED, -15, 8,
	    Z_DEFAULT_STRATEGY) != Z_OK) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"compression init error on \"%s\"", path));
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    do {
	len = Tcl_Read(in, buf, bufsize);
	if (len == ERROR_LENGTH) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "read error on %s: %s", path, Tcl_PosixError(interp)));
	    deflateEnd(&stream);
	    Tcl_Close(interp, in);
	    return TCL_ERROR;
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
			"deflate error on %s", path));
		deflateEnd(&stream);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    olen = sizeof(obuf) - stream.avail_out;
	    if (passwd != NULL) {
		size_t i;
		int tmp;

		for (i = 0; i < olen; i++) {
		    obuf[i] = (char) zencode(keys, crc32tab, obuf[i], tmp);
		}
	    }
	    if (olen && ((size_t) Tcl_Write(out, obuf, olen) != olen)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"write error: %s", Tcl_PosixError(interp)));
		deflateEnd(&stream);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    nbytecompr += olen;
	} while (stream.avail_out == 0);
    } while (flush != Z_FINISH);
    deflateEnd(&stream);
    Tcl_Flush(out);
    pos[1] = Tcl_Tell(out);
    if (nbyte - nbytecompr <= 0) {
	/*
	 * Compressed file larger than input, write it again uncompressed.
	 */
	if (Tcl_Seek(in, 0, SEEK_SET) != 0) {
	    goto seekErr;
	}
	if (Tcl_Seek(out, pos[2], SEEK_SET) != pos[2]) {
	seekErr:
	    Tcl_Close(interp, in);
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "seek error: %s", Tcl_PosixError(interp)));
	    return TCL_ERROR;
	}
	nbytecompr = (passwd != NULL) ? 12 : 0;
	while (1) {
	    len = Tcl_Read(in, buf, bufsize);
	    if (len == ERROR_LENGTH) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"read error on \"%s\": %s",
			path, Tcl_PosixError(interp)));
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    } else if (len == 0) {
		break;
	    }
	    if (passwd != NULL) {
		size_t i;
		int tmp;

		for (i = 0; i < len; i++) {
		    buf[i] = (char) zencode(keys0, crc32tab, buf[i], tmp);
		}
	    }
	    if ((size_t) Tcl_Write(out, buf, len) != len) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"write error: %s", Tcl_PosixError(interp)));
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    nbytecompr += len;
	}
	cmeth = ZIP_COMPMETH_STORED;
	Tcl_Flush(out);
	pos[1] = Tcl_Tell(out);
	Tcl_TruncateChannel(out, pos[1]);
    }
    Tcl_Close(interp, in);

    z = ckalloc(sizeof(ZipEntry));
    z->name = NULL;
    z->tnext = NULL;
    z->depth = 0;
    z->zipfile = NULL;
    z->isdir = 0;
    z->isenc = (passwd != NULL) ? 1 : 0;
    z->offset = pos[0];
    z->crc32 = crc;
    z->timestamp = mtime;
    z->nbyte = nbyte;
    z->nbytecompr = nbytecompr;
    z->cmeth = cmeth;
    z->data = NULL;
    hPtr = Tcl_CreateHashEntry(fileHash, zpath, &isNew);
    if (!isNew) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"non-unique path name \"%s\"", path));
	ckfree(z);
	return TCL_ERROR;
    } else {
	Tcl_SetHashValue(hPtr, z);
	z->name = Tcl_GetHashKey(fileHash, hPtr);
	z->next = NULL;
    }

    /*
     * Write final local header information.
     */
    ZipWriteInt(buf + ZIP_LOCAL_SIG_OFFS, ZIP_LOCAL_HEADER_SIG);
    ZipWriteShort(buf + ZIP_LOCAL_VERSION_OFFS, ZIP_MIN_VERSION);
    ZipWriteShort(buf + ZIP_LOCAL_FLAGS_OFFS, z->isenc);
    ZipWriteShort(buf + ZIP_LOCAL_COMPMETH_OFFS, z->cmeth);
    ZipWriteShort(buf + ZIP_LOCAL_MTIME_OFFS, ToDosTime(z->timestamp));
    ZipWriteShort(buf + ZIP_LOCAL_MDATE_OFFS, ToDosDate(z->timestamp));
    ZipWriteInt(buf + ZIP_LOCAL_CRC32_OFFS, z->crc32);
    ZipWriteInt(buf + ZIP_LOCAL_COMPLEN_OFFS, z->nbytecompr);
    ZipWriteInt(buf + ZIP_LOCAL_UNCOMPLEN_OFFS, z->nbyte);
    ZipWriteShort(buf + ZIP_LOCAL_PATHLEN_OFFS, zpathlen);
    ZipWriteShort(buf + ZIP_LOCAL_EXTRALEN_OFFS, align);
    if (Tcl_Seek(out, pos[0], SEEK_SET) != pos[0]) {
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
    if (Tcl_Seek(out, pos[1], SEEK_SET) != pos[1]) {
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
 * ZipFSMkZipOrImgObjCmd --
 *
 *	This procedure is creates a new ZIP archive file or image file given
 *	output filename, input directory of files to be archived, optional
 *	password, and optional image to be prepended to the output ZIP archive
 *	file.
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
ZipFSMkZipOrImgObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int isImg,
    int isList,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Channel out;
    int pwlen = 0, count, ret = TCL_ERROR, lobjc;
    size_t len, slen = 0, i = 0;
    Tcl_WideInt pos[3];
    Tcl_Obj **lobjv, *list = NULL;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable fileHash;
    char *strip = NULL, *pw = NULL, passBuf[264], buf[4096];

    /*
     * Caller has verified that the number of arguments is correct.
     */

    passBuf[0] = 0;
    if (objc > (isList ? 3 : 4)) {
	pw = Tcl_GetString(objv[isList ? 3 : 4]);
	pwlen = strlen(pw);
	if ((pwlen > 255) || (strchr(pw, 0xff) != NULL)) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("illegal password", -1));
	    return TCL_ERROR;
	}
    }
    if (isList) {
	list = objv[2];
	Tcl_IncrRefCount(list);
    } else {
	Tcl_Obj *cmd[3];

	cmd[1] = Tcl_NewStringObj("::tcl::zipfs::find", -1);
	cmd[2] = objv[2];
	cmd[0] = Tcl_NewListObj(2, cmd + 1);
	Tcl_IncrRefCount(cmd[0]);
	if (Tcl_EvalObjEx(interp, cmd[0], TCL_EVAL_DIRECT) != TCL_OK) {
	    Tcl_DecrRefCount(cmd[0]);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(cmd[0]);
	list = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(list);
    }
    if (Tcl_ListObjGetElements(interp, list, &lobjc, &lobjv) != TCL_OK) {
	Tcl_DecrRefCount(list);
	return TCL_ERROR;
    }
    if (isList && (lobjc % 2)) {
	Tcl_DecrRefCount(list);
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("need even number of elements", -1));
	return TCL_ERROR;
    }
    if (lobjc == 0) {
	Tcl_DecrRefCount(list);
	Tcl_SetObjResult(interp, Tcl_NewStringObj("empty archive", -1));
	return TCL_ERROR;
    }
    out = Tcl_OpenFileChannel(interp, Tcl_GetString(objv[1]), "wb", 0755);
    if (out == NULL) {
	Tcl_DecrRefCount(list);
	return TCL_ERROR;
    }
    if (pwlen <= 0) {
	pw = NULL;
	pwlen = 0;
    }
    if (isImg) {
	ZipFile *zf, zf0;
	int isMounted = 0;
	const char *imgName;

	if (isList) {
	    imgName = (objc > 4) ? Tcl_GetString(objv[4]) :
		    Tcl_GetNameOfExecutable();
	} else {
	    imgName = (objc > 5) ? Tcl_GetString(objv[5]) :
		    Tcl_GetNameOfExecutable();
	}
	if (pwlen) {
	    i = 0;
	    len = pwlen;
	    while (len > 0) {
		int ch = pw[len - 1];

		passBuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
		i++;
		len--;
	    }
	    passBuf[i] = i;
	    ++i;
	    passBuf[i++] = (char) ZIP_PASSWORD_END_SIG;
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 8);
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 16);
	    passBuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 24);
	    passBuf[i] = '\0';
	}
	/* Check for mounted image */
	WriteLock();
	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    zf = Tcl_GetHashValue(hPtr);
	    if (strcmp(zf->name, imgName) == 0) {
		isMounted = 1;
		zf->numOpen++;
		break;
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	Unlock();
	if (!isMounted) {
	    zf = &zf0;
	}
	if (isMounted || ZipFSOpenArchive(interp, imgName, 0, zf) == TCL_OK) {
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
	    size_t k;
	    int m, n;
	    Tcl_Channel in;
	    const char *errMsg = "seek error";

	    /*
	     * Fall back to read it as plain file which hopefully is a static
	     * tclsh or wish binary with proper zipfs infrastructure built in.
	     */

	    Tcl_ResetResult(interp);
	    in = Tcl_OpenFileChannel(interp, imgName, "rb", 0644);
	    if (in == NULL) {
		memset(passBuf, 0, sizeof(passBuf));
		Tcl_DecrRefCount(list);
		Tcl_Close(interp, out);
		return TCL_ERROR;
	    }
	    i = Tcl_Seek(in, 0, SEEK_END);
	    if (i == ERROR_LENGTH) {
	    cperr:
		memset(passBuf, 0, sizeof(passBuf));
		Tcl_DecrRefCount(list);
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"%s: %s", errMsg, Tcl_PosixError(interp)));
		Tcl_Close(interp, out);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    Tcl_Seek(in, 0, SEEK_SET);
	    k = 0;
	    while (k < i) {
		m = i - k;
		if (m > (int) sizeof(buf)) {
		    m = (int) sizeof(buf);
		}
		n = Tcl_Read(in, buf, m);
		if (n == -1) {
		    errMsg = "read error";
		    goto cperr;
		} else if (n == 0) {
		    break;
		}
		m = Tcl_Write(out, buf, n);
		if (m != n) {
		    errMsg = "write error";
		    goto cperr;
		}
		k += m;
	    }
	    Tcl_Close(interp, in);
	}
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
    Tcl_InitHashTable(&fileHash, TCL_STRING_KEYS);
    pos[0] = Tcl_Tell(out);
    if (!isList && (objc > 3)) {
	strip = Tcl_GetString(objv[3]);
	slen = strlen(strip);
    }
    for (i = 0; i < (size_t) lobjc; i += (isList ? 2 : 1)) {
	const char *path, *name;

	path = Tcl_GetString(lobjv[i]);
	if (isList) {
	    name = Tcl_GetString(lobjv[i + 1]);
	} else {
	    name = path;
	    if (slen > 0) {
		len = strlen(name);
		if ((len <= slen) || (strncmp(strip, name, slen) != 0)) {
		    continue;
		}
		name += slen;
	    }
	}
	while (name[0] == '/') {
	    ++name;
	}
	if (name[0] == '\0') {
	    continue;
	}
	if (ZipAddFile(interp, path, name, out, pw, buf, sizeof(buf),
		&fileHash) != TCL_OK) {
	    goto done;
	}
    }
    pos[1] = Tcl_Tell(out);
    count = 0;
    for (i = 0; i < (size_t) lobjc; i += (isList ? 2 : 1)) {
	const char *path, *name;

	path = Tcl_GetString(lobjv[i]);
	if (isList) {
	    name = Tcl_GetString(lobjv[i + 1]);
	} else {
	    name = path;
	    if (slen > 0) {
		len = strlen(name);
		if ((len <= slen) || (strncmp(strip, name, slen) != 0)) {
		    continue;
		}
		name += slen;
	    }
	}
	while (name[0] == '/') {
	    ++name;
	}
	if (name[0] == '\0') {
	    continue;
	}
	hPtr = Tcl_FindHashEntry(&fileHash, name);
	if (hPtr == NULL) {
	    continue;
	}
	z = Tcl_GetHashValue(hPtr);
	len = strlen(z->name);
	ZipWriteInt(buf + ZIP_CENTRAL_SIG_OFFS, ZIP_CENTRAL_HEADER_SIG);
	ZipWriteShort(buf + ZIP_CENTRAL_VERSIONMADE_OFFS, ZIP_MIN_VERSION);
	ZipWriteShort(buf + ZIP_CENTRAL_VERSION_OFFS, ZIP_MIN_VERSION);
	ZipWriteShort(buf + ZIP_CENTRAL_FLAGS_OFFS, z->isenc ? 1 : 0);
	ZipWriteShort(buf + ZIP_CENTRAL_COMPMETH_OFFS, z->cmeth);
	ZipWriteShort(buf + ZIP_CENTRAL_MTIME_OFFS, ToDosTime(z->timestamp));
	ZipWriteShort(buf + ZIP_CENTRAL_MDATE_OFFS, ToDosDate(z->timestamp));
	ZipWriteInt(buf + ZIP_CENTRAL_CRC32_OFFS, z->crc32);
	ZipWriteInt(buf + ZIP_CENTRAL_COMPLEN_OFFS, z->nbytecompr);
	ZipWriteInt(buf + ZIP_CENTRAL_UNCOMPLEN_OFFS, z->nbyte);
	ZipWriteShort(buf + ZIP_CENTRAL_PATHLEN_OFFS, len);
	ZipWriteShort(buf + ZIP_CENTRAL_EXTRALEN_OFFS, 0);
	ZipWriteShort(buf + ZIP_CENTRAL_FCOMMENTLEN_OFFS, 0);
	ZipWriteShort(buf + ZIP_CENTRAL_DISKFILE_OFFS, 0);
	ZipWriteShort(buf + ZIP_CENTRAL_IATTR_OFFS, 0);
	ZipWriteInt(buf + ZIP_CENTRAL_EATTR_OFFS, 0);
	ZipWriteInt(buf + ZIP_CENTRAL_LOCALHDR_OFFS, z->offset - pos[0]);
	if ((Tcl_Write(out, buf,
		    ZIP_CENTRAL_HEADER_LEN) != ZIP_CENTRAL_HEADER_LEN)
		|| ((size_t) Tcl_Write(out, z->name, len) != len)) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "write error: %s", Tcl_PosixError(interp)));
	    goto done;
	}
	count++;
    }
    Tcl_Flush(out);
    pos[2] = Tcl_Tell(out);
    ZipWriteInt(buf + ZIP_CENTRAL_END_SIG_OFFS, ZIP_CENTRAL_END_SIG);
    ZipWriteShort(buf + ZIP_CENTRAL_DISKNO_OFFS, 0);
    ZipWriteShort(buf + ZIP_CENTRAL_DISKDIR_OFFS, 0);
    ZipWriteShort(buf + ZIP_CENTRAL_ENTS_OFFS, count);
    ZipWriteShort(buf + ZIP_CENTRAL_TOTALENTS_OFFS, count);
    ZipWriteInt(buf + ZIP_CENTRAL_DIRSIZE_OFFS, pos[2] - pos[1]);
    ZipWriteInt(buf + ZIP_CENTRAL_DIRSTART_OFFS, pos[1] - pos[0]);
    ZipWriteShort(buf + ZIP_CENTRAL_COMMENTLEN_OFFS, 0);
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
    hPtr = Tcl_FirstHashEntry(&fileHash, &search);
    while (hPtr != NULL) {
	z = Tcl_GetHashValue(hPtr);
	ckfree(z);
	Tcl_DeleteHashEntry(hPtr);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&fileHash);
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkZipObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::mkzip" command.  See
 *	description of ZipFSMkZipOrImgCmd().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImgCmd().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkZipObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 3 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile indir ?strip? ?password?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"operation not permitted in a safe interpreter", -1));
	return TCL_ERROR;
    }
    return ZipFSMkZipOrImgObjCmd(clientData, interp, 0, 0, objc, objv);
}

static int
ZipFSLMkZipObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile inlist ?password?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"operation not permitted in a safe interpreter", -1));
	return TCL_ERROR;
    }
    return ZipFSMkZipOrImgObjCmd(clientData, interp, 0, 1, objc, objv);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSZipFSOpenArchiveObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::mkimg" command.  See
 *	description of ZipFSMkZipOrImgCmd().
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImgCmd().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkImgObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 3 || objc > 6) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"outfile indir ?strip? ?password? ?infile?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"operation not permitted in a safe interpreter", -1));
	return TCL_ERROR;
    }
    return ZipFSMkZipOrImgObjCmd(clientData, interp, 1, 0, objc, objv);
}

static int
ZipFSLMkImgObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    if (objc < 3 || objc > 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "outfile inlist ?password infile?");
	return TCL_ERROR;
    }
    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"operation not permitted in a safe interpreter", -1));
	return TCL_ERROR;
    }
    return ZipFSMkZipOrImgObjCmd(clientData, interp, 1, 1, objc, objv);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSCanonicalObjCmd --
 *
 *	This procedure is invoked to process the "zipfs::canonical" command.
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    char *mntpoint = NULL;
    char *filename = NULL;
    char *result;
    Tcl_DString dPath;

    if (objc < 2 || objc > 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "?mntpnt? filename ?ZIPFS?");
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
 *	This procedure is invoked to process the "zipfs::exists" command.  It
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
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

    filename = Tcl_GetStringFromObj(objv[1], 0);
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
 *	This procedure is invoked to process the "zipfs::info" command.	 On
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    char *filename;
    ZipEntry *z;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }
    filename = Tcl_GetStringFromObj(objv[1], 0);
    ReadLock();
    z = ZipFSLookup(filename);
    if (z != NULL) {
	Tcl_Obj *result = Tcl_GetObjResult(interp);

	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewStringObj(z->zipfile->name, -1));
	Tcl_ListObjAppendElement(interp, result, Tcl_NewWideIntObj(z->nbyte));
	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewWideIntObj(z->nbytecompr));
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
 *	This procedure is invoked to process the "zipfs::list" command.	 On
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
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    char *pattern = NULL;
    Tcl_RegExp regexp = NULL;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Obj *result = Tcl_GetObjResult(interp);

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?(-glob|-regexp)? ?pattern?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	int n;
	char *what = Tcl_GetStringFromObj(objv[1], &n);

	if ((n >= 2) && (strncmp(what, "-glob", n) == 0)) {
	    pattern = Tcl_GetString(objv[2]);
	} else if ((n >= 2) && (strncmp(what, "-regexp", n) == 0)) {
	    regexp = Tcl_RegExpCompile(interp, Tcl_GetString(objv[2]));
	    if (regexp == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "unknown option \"%s\"", what));
	    return TCL_ERROR;
	}
    } else if (objc == 2) {
	pattern = Tcl_GetStringFromObj(objv[1], 0);
    }
    ReadLock();
    if (pattern != NULL) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = Tcl_GetHashValue(hPtr);

	    if (Tcl_StringMatch(z->name, pattern)) {
		Tcl_ListObjAppendElement(interp, result,
			Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else if (regexp != NULL) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = Tcl_GetHashValue(hPtr);

	    if (Tcl_RegExpExec(interp, regexp, z->name, z->name)) {
		Tcl_ListObjAppendElement(interp, result,
			Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = Tcl_GetHashValue(hPtr);

	    Tcl_ListObjAppendElement(interp, result,
		    Tcl_NewStringObj(z->name, -1));
	}
    }
    Unlock();
    return TCL_OK;
}

#ifdef _WIN32
#define LIBRARY_SIZE	    64

static int
ToUtf(
    const WCHAR *wSrc,
    char *dst)
{
    char *start;

    start = dst;
    while (*wSrc != '\0') {
	dst += Tcl_UniCharToUtf(*wSrc, dst);
	wSrc++;
    }
    *dst = '\0';
    return (int) (dst - start);
}
#endif /* _WIN32 */

Tcl_Obj *
TclZipfs_TclLibrary(void)
{
    if (zipfs_literal_tcl_library) {
	return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
    } else {
	Tcl_Obj *vfsinitscript;
	int found = 0;
#ifdef _WIN32
	HMODULE hModule = TclWinGetTclInstance();
	WCHAR wName[MAX_PATH + LIBRARY_SIZE];
	char dllname[(MAX_PATH + LIBRARY_SIZE) * TCL_UTF_MAX];
#endif /* _WIN32 */
	/*
	 * Look for the library file system within the executable.
	 */

	vfsinitscript = Tcl_NewStringObj(
		ZIPFS_APP_MOUNT "/tcl_library/init.tcl", -1);
	Tcl_IncrRefCount(vfsinitscript);
	found = Tcl_FSAccess(vfsinitscript, F_OK);
	Tcl_DecrRefCount(vfsinitscript);
	if (found == TCL_OK) {
	    zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}

#if defined(_WIN32)
	if (GetModuleFileNameW(hModule, wName, MAX_PATH) == 0) {
	    GetModuleFileNameA(hModule, dllname, MAX_PATH);
	} else {
	    ToUtf(wName, dllname);
	}

	/*
	 * Mount zip file and dll before releasing to search.
	 */

	if (ZipfsAppHookFindTclInit(dllname) == TCL_OK) {
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}
#elif /* !_WIN32 && */ defined(CFG_RUNTIME_DLLFILE)
	/*
	 * Mount zip file and dll before releasing to search.
	 */
	if (ZipfsAppHookFindTclInit(
		CFG_RUNTIME_LIBDIR "/" CFG_RUNTIME_DLLFILE) == TCL_OK) {
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}
#endif /* _WIN32 || CFG_RUNTIME_DLLFILE */

#ifdef CFG_RUNTIME_ZIPFILE
	if (ZipfsAppHookFindTclInit(
		CFG_RUNTIME_LIBDIR "/" CFG_RUNTIME_ZIPFILE) == TCL_OK) {
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}
	if (ZipfsAppHookFindTclInit(
		CFG_RUNTIME_SCRDIR "/" CFG_RUNTIME_ZIPFILE) == TCL_OK) {
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}
	if (ZipfsAppHookFindTclInit(CFG_RUNTIME_ZIPFILE) == TCL_OK) {
	    return Tcl_NewStringObj(zipfs_literal_tcl_library, -1);
	}
#endif /* CFG_RUNTIME_ZIPFILE */
    }
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
 *	This procedure is invoked to process the "zipfs::root" command. It
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
ZipFSTclLibraryObjCmd(
    ClientData clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *pResult = TclZipfs_TclLibrary();

    if (!pResult) {
	pResult = Tcl_NewObj();
    }
    Tcl_SetObjResult(interp, pResult);
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
    ClientData instanceData,
    Tcl_Interp *interp)		/* Current interpreter. */
{
    ZipChannel *info = instanceData;

    if (info->iscompr && (info->ubuf != NULL)) {
	ckfree(info->ubuf);
	info->ubuf = NULL;
    }
    if (info->isenc) {
	info->isenc = 0;
	memset(info->keys, 0, sizeof(info->keys));
    }
    if (info->iswr) {
	ZipEntry *z = info->zipentry;
	unsigned char *newdata = attemptckrealloc(info->ubuf, info->nread);

	if (newdata != NULL) {
	    if (z->data != NULL) {
		ckfree(z->data);
	    }
	    z->data = newdata;
	    z->nbyte = z->nbytecompr = info->nbyte;
	    z->cmeth = ZIP_COMPMETH_STORED;
	    z->timestamp = time(NULL);
	    z->isdir = 0;
	    z->isenc = 0;
	    z->offset = 0;
	    z->crc32 = 0;
	} else {
	    ckfree(info->ubuf);
	}
    }
    WriteLock();
    info->zipfile->numOpen--;
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
    ClientData instanceData,
    char *buf,
    int toRead,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long nextpos;

    if (info->isdir < 0) {
	/*
	 * Special case: when executable combined with ZIP archive file read
	 * data in front of ZIP, i.e. the executable itself.
	 */

	nextpos = info->nread + toRead;
	if (nextpos > info->zipfile->baseOffset) {
	    toRead = info->zipfile->baseOffset - info->nread;
	    nextpos = info->zipfile->baseOffset;
	}
	if (toRead == 0) {
	    return 0;
	}
	memcpy(buf, info->zipfile->data, toRead);
	info->nread = nextpos;
	*errloc = 0;
	return toRead;
    }
    if (info->isdir) {
	*errloc = EISDIR;
	return -1;
    }
    nextpos = info->nread + toRead;
    if (nextpos > info->nbyte) {
	toRead = info->nbyte - info->nread;
	nextpos = info->nbyte;
    }
    if (toRead == 0) {
	return 0;
    }
    if (info->isenc) {
	int i;

	for (i = 0; i < toRead; i++) {
	    int ch = info->ubuf[i + info->nread];

	    buf[i] = zdecode(info->keys, crc32tab, ch);
	}
    } else {
	memcpy(buf, info->ubuf + info->nread, toRead);
    }
    info->nread = nextpos;
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
    ClientData instanceData,
    const char *buf,
    int toWrite,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long nextpos;

    if (!info->iswr) {
	*errloc = EINVAL;
	return -1;
    }
    nextpos = info->nread + toWrite;
    if (nextpos > info->nmax) {
	toWrite = info->nmax - info->nread;
	nextpos = info->nmax;
    }
    if (toWrite == 0) {
	return 0;
    }
    memcpy(info->ubuf + info->nread, buf, toWrite);
    info->nread = nextpos;
    if (info->nread > info->nbyte) {
	info->nbyte = info->nread;
    }
    *errloc = 0;
    return toWrite;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelSeek --
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

static int
ZipChannelSeek(
    ClientData instanceData,
    long offset,
    int mode,
    int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long end;

    if (!info->iswr && (info->isdir < 0)) {
	/*
	 * Special case: when executable combined with ZIP archive file, seek
	 * within front of ZIP, i.e. the executable itself.
	 */
	end = info->zipfile->baseOffset;
    } else if (info->isdir) {
	*errloc = EINVAL;
	return -1;
    } else {
	end = info->nbyte;
    }
    switch (mode) {
    case SEEK_CUR:
	offset += info->nread;
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
    if (info->iswr) {
	if ((unsigned long) offset > info->nmax) {
	    *errloc = EINVAL;
	    return -1;
	}
	if ((unsigned long) offset > info->nbyte) {
	    info->nbyte = offset;
	}
    } else if ((unsigned long) offset > end) {
	*errloc = EINVAL;
	return -1;
    }
    info->nread = (unsigned long) offset;
    return info->nread;
}

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
    ClientData instanceData,
    int mask)
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
    ClientData instanceData,
    int direction,
    ClientData *handlePtr)
{
    return TCL_ERROR;
}

/*
 * The channel type/driver definition used for ZIP archive members.
 */

static Tcl_ChannelType ZipChannelType = {
    "zip",		    /* Type name. */
    TCL_CHANNEL_VERSION_5,
    ZipChannelClose,	    /* Close channel, clean instance data */
    ZipChannelRead,	    /* Handle read request */
    ZipChannelWrite,	    /* Handle write request */
    ZipChannelSeek,	    /* Move location of access point, NULL'able */
    NULL,		    /* Set options, NULL'able */
    NULL,		    /* Get options, NULL'able */
    ZipChannelWatchChannel, /* Initialize notifier */
    ZipChannelGetFile,	    /* Get OS handle from the channel */
    NULL,		    /* 2nd version of close channel, NULL'able */
    NULL,		    /* Set blocking mode for raw channel, NULL'able */
    NULL,		    /* Function to flush channel, NULL'able */
    NULL,		    /* Function to handle event, NULL'able */
    NULL,		    /* Wide seek function, NULL'able */
    NULL,		    /* Thread action function, NULL'able */
    NULL,		    /* Truncate function, NULL'able */
};

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelOpen --
 *
 *	This function opens a Tcl_Channel on a file from a mounted ZIP archive
 *	according to given open mode.
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
    char *filename,
    int mode,
    int permissions)
{
    ZipEntry *z;
    ZipChannel *info;
    int i, ch, trunc, wr, flags = 0;
    char cname[128];

    if ((mode & O_APPEND)
	    || ((ZipFS.wrmax <= 0) && (mode & (O_WRONLY | O_RDWR)))) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("unsupported open mode", -1));
	}
	return NULL;
    }
    WriteLock();
    z = ZipFSLookup(filename);
    if (z == NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "file not found \"%s\"", filename));
	}
	goto error;
    }
    trunc = (mode & O_TRUNC) != 0;
    wr = (mode & (O_WRONLY | O_RDWR)) != 0;
    if ((z->cmeth != ZIP_COMPMETH_STORED)
	    && (z->cmeth != ZIP_COMPMETH_DEFLATED)) {
	ZIPFS_ERROR(interp, "unsupported compression method");
	goto error;
    }
    if (wr && z->isdir) {
	ZIPFS_ERROR(interp, "unsupported file type");
	goto error;
    }
    if (!trunc) {
	flags |= TCL_READABLE;
	if (z->isenc && (z->zipfile->passBuf[0] == 0)) {
	    ZIPFS_ERROR(interp, "decryption failed");
	    goto error;
	} else if (wr && (z->data == NULL) && (z->nbyte > ZipFS.wrmax)) {
	    ZIPFS_ERROR(interp, "file too large");
	    goto error;
	}
    } else {
	flags = TCL_WRITABLE;
    }
    info = attemptckalloc(sizeof(ZipChannel));
    if (info == NULL) {
	ZIPFS_ERROR(interp, "out of memory");
	goto error;
    }
    info->zipfile = z->zipfile;
    info->zipentry = z;
    info->nread = 0;
    if (wr) {
	flags |= TCL_WRITABLE;
	info->iswr = 1;
	info->isdir = 0;
	info->nmax = ZipFS.wrmax;
	info->iscompr = 0;
	info->isenc = 0;
	info->ubuf = attemptckalloc(info->nmax);
	if (info->ubuf == NULL) {
	merror0:
	    if (info->ubuf != NULL) {
		ckfree(info->ubuf);
	    }
	    ckfree(info);
	    ZIPFS_ERROR(interp, "out of memory");
	    goto error;
	}
	memset(info->ubuf, 0, info->nmax);
	if (trunc) {
	    info->nbyte = 0;
	} else if (z->data != NULL) {
	    unsigned int j = z->nbyte;

	    if (j > info->nmax) {
		j = info->nmax;
	    }
	    memcpy(info->ubuf, z->data, j);
	    info->nbyte = j;
	} else {
	    unsigned char *zbuf = z->zipfile->data + z->offset;

	    if (z->isenc) {
		int len = z->zipfile->passBuf[0];
		char passBuf[260];

		for (i = 0; i < len; i++) {
		    ch = z->zipfile->passBuf[len - i];
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
	    if (z->cmeth == ZIP_COMPMETH_DEFLATED) {
		z_stream stream;
		int err;
		unsigned char *cbuf = NULL;

		memset(&stream, 0, sizeof(z_stream));
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = z->nbytecompr;
		if (z->isenc) {
		    unsigned int j;

		    stream.avail_in -= 12;
		    cbuf = attemptckalloc(stream.avail_in);
		    if (cbuf == NULL) {
			goto merror0;
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
		stream.avail_out = info->nmax;
		if (inflateInit2(&stream, -15) != Z_OK) {
		    goto cerror0;
		}
		err = inflate(&stream, Z_SYNC_FLUSH);
		inflateEnd(&stream);
		if ((err == Z_STREAM_END)
			|| ((err == Z_OK) && (stream.avail_in == 0))) {
		    if (cbuf != NULL) {
			memset(info->keys, 0, sizeof(info->keys));
			ckfree(cbuf);
		    }
		    goto wrapchan;
		}
	    cerror0:
		if (cbuf != NULL) {
		    memset(info->keys, 0, sizeof(info->keys));
		    ckfree(cbuf);
		}
		if (info->ubuf != NULL) {
		    ckfree(info->ubuf);
		}
		ckfree(info);
		ZIPFS_ERROR(interp, "decompression error");
		goto error;
	    } else if (z->isenc) {
		for (i = 0; i < z->nbyte - 12; i++) {
		    ch = zbuf[i];
		    info->ubuf[i] = zdecode(info->keys, crc32tab, ch);
		}
	    } else {
		memcpy(info->ubuf, zbuf, z->nbyte);
	    }
	    memset(info->keys, 0, sizeof(info->keys));
	    goto wrapchan;
	}
    } else if (z->data != NULL) {
	flags |= TCL_READABLE;
	info->iswr = 0;
	info->iscompr = 0;
	info->isdir = 0;
	info->isenc = 0;
	info->nbyte = z->nbyte;
	info->nmax = 0;
	info->ubuf = z->data;
    } else {
	flags |= TCL_READABLE;
	info->iswr = 0;
	info->iscompr = z->cmeth == ZIP_COMPMETH_DEFLATED;
	info->ubuf = z->zipfile->data + z->offset;
	info->isdir = z->isdir;
	info->isenc = z->isenc;
	info->nbyte = z->nbyte;
	info->nmax = 0;
	if (info->isenc) {
	    int len = z->zipfile->passBuf[0];
	    char passBuf[260];

	    for (i = 0; i < len; i++) {
		ch = z->zipfile->passBuf[len - i];
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
	    unsigned char *ubuf = NULL;
	    unsigned int j;

	    memset(&stream, 0, sizeof(z_stream));
	    stream.zalloc = Z_NULL;
	    stream.zfree = Z_NULL;
	    stream.opaque = Z_NULL;
	    stream.avail_in = z->nbytecompr;
	    if (info->isenc) {
		stream.avail_in -= 12;
		ubuf = attemptckalloc(stream.avail_in);
		if (ubuf == NULL) {
		    info->ubuf = NULL;
		    goto merror;
		}
		for (j = 0; j < stream.avail_in; j++) {
		    ch = info->ubuf[j];
		    ubuf[j] = zdecode(info->keys, crc32tab, ch);
		}
		stream.next_in = ubuf;
	    } else {
		stream.next_in = info->ubuf;
	    }
	    stream.next_out = info->ubuf = attemptckalloc(info->nbyte);
	    if (info->ubuf == NULL) {
	    merror:
		if (ubuf != NULL) {
		    info->isenc = 0;
		    memset(info->keys, 0, sizeof(info->keys));
		    ckfree(ubuf);
		}
		ckfree(info);
		if (interp != NULL) {
		    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj("out of memory", -1));
		}
		goto error;
	    }
	    stream.avail_out = info->nbyte;
	    if (inflateInit2(&stream, -15) != Z_OK) {
		goto cerror;
	    }
	    err = inflate(&stream, Z_SYNC_FLUSH);
	    inflateEnd(&stream);
	    if ((err == Z_STREAM_END)
		    || ((err == Z_OK) && (stream.avail_in == 0))) {
		if (ubuf != NULL) {
		    info->isenc = 0;
		    memset(info->keys, 0, sizeof(info->keys));
		    ckfree(ubuf);
		}
		goto wrapchan;
	    }
	cerror:
	    if (ubuf != NULL) {
		info->isenc = 0;
		memset(info->keys, 0, sizeof(info->keys));
		ckfree(ubuf);
	    }
	    if (info->ubuf != NULL) {
		ckfree(info->ubuf);
	    }
	    ckfree(info);
	    ZIPFS_ERROR(interp, "decompression error");
	    goto error;
	}
    }

  wrapchan:
    sprintf(cname, "zipfs_%" TCL_LL_MODIFIER "x_%d", z->offset,
	    ZipFS.idCount++);
    z->zipfile->numOpen++;
    Unlock();
    return Tcl_CreateChannel(&ZipChannelType, cname, (ClientData) info, flags);

  error:
    Unlock();
    return NULL;
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
    if (z != NULL) {
	memset(buf, 0, sizeof(Tcl_StatBuf));
	if (z->isdir) {
	    buf->st_mode = S_IFDIR | 0555;
	} else {
	    buf->st_mode = S_IFREG | 0555;
	}
	buf->st_size = z->nbyte;
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
    return (z != NULL) ? 0 : -1;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSOpenFileChannelProc --
 *
 * Results:
 *
 * Side effects:
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Channel
ZipFSOpenFileChannelProc(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *pathPtr,
    int mode,
    int permissions)
{
    int len;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return NULL;
    }
    return ZipChannelOpen(interp, Tcl_GetStringFromObj(pathPtr, &len), mode,
	    permissions);
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
    int len;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    return ZipEntryStat(Tcl_GetStringFromObj(pathPtr, &len), buf);
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
    int len;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }
    return ZipEntryAccess(Tcl_GetStringFromObj(pathPtr, &len), mode);
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
    Tcl_Obj *pathPtr)
{
    return Tcl_NewStringObj("/", -1);
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
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *result,
    Tcl_Obj *pathPtr,
    const char *pattern,
    Tcl_GlobTypeData *types)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_Obj *normPathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    int scnt, l, dirOnly = -1, prefixLen, strip = 0;
    size_t len;
    char *pat, *prefix, *path;
    Tcl_DString dsPref;

    if (!normPathPtr) {
	return -1;
    }
    if (types != NULL) {
	dirOnly = (types->type & TCL_GLOB_TYPE_DIR) == TCL_GLOB_TYPE_DIR;
    }

    /*
     * The prefix that gets prepended to results.
     */

    prefix = Tcl_GetStringFromObj(pathPtr, &prefixLen);

    /*
     * The (normalized) path we're searching.
     */

    path = Tcl_GetString(normPathPtr);
    len = normPathPtr->length;

    Tcl_DStringInit(&dsPref);
    Tcl_DStringAppend(&dsPref, prefix, prefixLen);

    if (strcmp(prefix, path) == 0) {
	prefix = NULL;
    } else {
	strip = len + 1;
    }
    if (prefix != NULL) {
	Tcl_DStringAppend(&dsPref, "/", 1);
	prefixLen++;
	prefix = Tcl_DStringValue(&dsPref);
    }
    ReadLock();
    if ((types != NULL) && (types->type == TCL_GLOB_TYPE_MOUNT)) {
	l = CountSlashes(path);
	if (path[len - 1] == '/') {
	    len--;
	} else {
	    l++;
	}
	if ((pattern == NULL) || (pattern[0] == '\0')) {
	    pattern = "*";
	}
	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    ZipFile *zf = Tcl_GetHashValue(hPtr);

	    if (zf->mountPointLen == 0) {
		ZipEntry *z;

		for (z = zf->topEnts; z != NULL; z = z->tnext) {
		    size_t lenz = strlen(z->name);

		    if ((lenz > len + 1) && (strncmp(z->name, path, len) == 0)
			    && (z->name[len] == '/')
			    && (CountSlashes(z->name) == l)
			    && Tcl_StringCaseMatch(z->name + len + 1, pattern,
				    0)) {
			if (prefix != NULL) {
			    Tcl_DStringAppend(&dsPref, z->name, lenz);
			    Tcl_ListObjAppendElement(NULL, result,
				    Tcl_NewStringObj(Tcl_DStringValue(&dsPref),
					    Tcl_DStringLength(&dsPref)));
			    Tcl_DStringSetLength(&dsPref, prefixLen);
			} else {
			    Tcl_ListObjAppendElement(NULL, result,
				    Tcl_NewStringObj(z->name, lenz));
			}
		    }
		}
	    } else if ((zf->mountPointLen > len + 1)
		    && (strncmp(zf->mountPoint, path, len) == 0)
		    && (zf->mountPoint[len] == '/')
		    && (CountSlashes(zf->mountPoint) == l)
		    && Tcl_StringCaseMatch(zf->mountPoint + len + 1,
			    pattern, 0)) {
		if (prefix != NULL) {
		    Tcl_DStringAppend(&dsPref, zf->mountPoint,
			    zf->mountPointLen);
		    Tcl_ListObjAppendElement(NULL, result,
			    Tcl_NewStringObj(Tcl_DStringValue(&dsPref),
				    Tcl_DStringLength(&dsPref)));
		    Tcl_DStringSetLength(&dsPref, prefixLen);
		} else {
		    Tcl_ListObjAppendElement(NULL, result,
			    Tcl_NewStringObj(zf->mountPoint,
				    zf->mountPointLen));
		}
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	goto end;
    }

    if ((pattern == NULL) || (pattern[0] == '\0')) {
	hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, path);
	if (hPtr != NULL) {
	    ZipEntry *z = Tcl_GetHashValue(hPtr);

	    if ((dirOnly < 0) || (!dirOnly && !z->isdir)
		    || (dirOnly && z->isdir)) {
		if (prefix != NULL) {
		    Tcl_DStringAppend(&dsPref, z->name, -1);
		    Tcl_ListObjAppendElement(NULL, result,
			    Tcl_NewStringObj(Tcl_DStringValue(&dsPref),
				    Tcl_DStringLength(&dsPref)));
		    Tcl_DStringSetLength(&dsPref, prefixLen);
		} else {
		    Tcl_ListObjAppendElement(NULL, result,
			    Tcl_NewStringObj(z->name, -1));
		}
	    }
	}
	goto end;
    }

    l = strlen(pattern);
    pat = ckalloc(len + l + 2);
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
	    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	ZipEntry *z = Tcl_GetHashValue(hPtr);

	if ((dirOnly >= 0) && ((dirOnly && !z->isdir)
		|| (!dirOnly && z->isdir))) {
	    continue;
	}
	if ((z->depth == scnt) && Tcl_StringCaseMatch(z->name, pat, 0)) {
	    if (prefix != NULL) {
		Tcl_DStringAppend(&dsPref, z->name + strip, -1);
		Tcl_ListObjAppendElement(NULL, result,
			Tcl_NewStringObj(Tcl_DStringValue(&dsPref),
				Tcl_DStringLength(&dsPref)));
		Tcl_DStringSetLength(&dsPref, prefixLen);
	    } else {
		Tcl_ListObjAppendElement(NULL, result,
			Tcl_NewStringObj(z->name + strip, -1));
	    }
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
    ClientData *clientDataPtr)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    ZipFile *zf;
    int ret = -1;
    size_t len;
    char *path;

    pathPtr = Tcl_FSGetNormalizedPath(NULL, pathPtr);
    if (!pathPtr) {
	return -1;
    }

    path = Tcl_GetString(pathPtr);
    if (strncmp(path, ZIPFS_VOLUME, ZIPFS_VOLUME_LEN) != 0) {
	return -1;
    }

    len = pathPtr->length;

    ReadLock();
    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, path);
    if (hPtr != NULL) {
	ret = TCL_OK;
	goto endloop;
    }

    hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
    while (hPtr != NULL) {
	zf = Tcl_GetHashValue(hPtr);
	if (zf->mountPointLen == 0) {
	    ZipEntry *z;

	    for (z = zf->topEnts; z != NULL; z = z->tnext) {
		size_t lenz = strlen(z->name);

		if ((len >= lenz) && (strncmp(path, z->name, lenz) == 0)) {
		    ret = TCL_OK;
		    goto endloop;
		}
	    }
	} else if ((len >= zf->mountPointLen) &&
		(strncmp(path, zf->mountPoint, zf->mountPointLen) == 0)) {
	    ret = TCL_OK;
	    goto endloop;
	}
	hPtr = Tcl_NextHashEntry(&search);
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

static const char *const *
ZipFSFileAttrStringsProc(
    Tcl_Obj *pathPtr,
    Tcl_Obj **objPtrRef)
{
    static const char *const attrs[] = {
	"-uncompsize",
	"-compsize",
	"-offset",
	"-mount",
	"-archive",
	"-permissions",
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
    if (z == NULL) {
	ZIPFS_ERROR(interp, "file not found");
	ret = TCL_ERROR;
	goto done;
    }
    switch (index) {
    case 0:
	*objPtrRef = Tcl_NewWideIntObj(z->nbyte);
	break;
    case 1:
	*objPtrRef = Tcl_NewWideIntObj(z->nbytecompr);
	break;
    case 2:
	*objPtrRef = Tcl_NewWideIntObj(z->offset);
	break;
    case 3:
	*objPtrRef = Tcl_NewStringObj(z->zipfile->mountPoint,
		z->zipfile->mountPointLen);
	break;
    case 4:
	*objPtrRef = Tcl_NewStringObj(z->zipfile->name, -1);
	break;
    case 5:
	*objPtrRef = Tcl_NewStringObj("0555", -1);
	break;
    default:
	ZIPFS_ERROR(interp, "unknown attribute");
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
    int index,
    Tcl_Obj *pathPtr,
    Tcl_Obj *objPtr)
{
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("unsupported operation", -1));
    }
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
    Tcl_Obj *pathPtr)
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
    if (loadFileProc != NULL) {
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
    if ((objs[1] != NULL) && (ZipFSAccessProc(objs[1], R_OK) == 0)) {
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

	if (execName != NULL) {
	    const char *p = strrchr(execName, '/');

	    if (p > execName + 1) {
		--p;
		objs[0] = Tcl_NewStringObj(execName, p - execName);
	    }
	}
	if (objs[0] == NULL) {
	    objs[0] = TclPathPart(interp, TclGetObjNameOfExecutable(),
		    TCL_PATH_DIRNAME);
	}
	if (objs[0] != NULL) {
	    altPath = TclJoinPath(2, objs);
	    if (altPath != NULL) {
		Tcl_IncrRefCount(altPath);
		if (Tcl_FSAccess(altPath, R_OK) == 0) {
		    path = altPath;
		}
	    }
	}
    }
    if (objs[0] != NULL) {
	Tcl_DecrRefCount(objs[0]);
    }
    if (objs[1] != NULL) {
	Tcl_DecrRefCount(objs[1]);
    }

    loadFileProc = (Tcl_FSLoadFileProc2 *) tclNativeFilesystem.loadFileProc;
    if (loadFileProc != NULL) {
	ret = loadFileProc(interp, path, loadHandle, unloadProcPtr, flags);
    } else {
	Tcl_SetErrno(ENOENT);
	ZIPFS_ERROR(interp, Tcl_PosixError(interp));
    }
    if (altPath != NULL) {
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

MODULE_SCOPE int
TclZipfs_Init(
    Tcl_Interp *interp)		/* Current interpreter. */
{
#ifdef HAVE_ZLIB
    static const EnsembleImplMap initMap[] = {
	{"mkimg",	ZipFSMkImgObjCmd,	NULL, NULL, NULL, 0},
	{"mkzip",	ZipFSMkZipObjCmd,	NULL, NULL, NULL, 0},
	{"lmkimg",	ZipFSLMkImgObjCmd,	NULL, NULL, NULL, 0},
	{"lmkzip",	ZipFSLMkZipObjCmd,	NULL, NULL, NULL, 0},
	/* The 4 entries above are not available in safe interpreters */
	{"mount",	ZipFSMountObjCmd,	NULL, NULL, NULL, 0},
	{"mount_data",	ZipFSMountBufferObjCmd,	NULL, NULL, NULL, 0},
	{"unmount",	ZipFSUnmountObjCmd,	NULL, NULL, NULL, 0},
	{"mkkey",	ZipFSMkKeyObjCmd,	NULL, NULL, NULL, 0},
	{"exists",	ZipFSExistsObjCmd,	NULL, NULL, NULL, 1},
	{"info",	ZipFSInfoObjCmd,	NULL, NULL, NULL, 1},
	{"list",	ZipFSListObjCmd,	NULL, NULL, NULL, 1},
	{"canonical",	ZipFSCanonicalObjCmd,	NULL, NULL, NULL, 1},
	{"root",	ZipFSRootObjCmd,	NULL, NULL, NULL, 1},
	{"tcl_library",	ZipFSTclLibraryObjCmd,	NULL, NULL, NULL, 0},
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
	"proc ::tcl::zipfs::find dir {\n"
	"    return [lsort [Find $dir]]\n"
	"}\n";

    /*
     * One-time initialization.
     */

    WriteLock();
    /* Tcl_StaticPackage(interp, "zipfs", TclZipfs_Init, TclZipfs_Init); */
    if (!ZipFS.initialized) {
	TclZipfs_C_Init();
    }
    Unlock();

    if (interp != NULL) {
	Tcl_EvalEx(interp, findproc, -1, TCL_EVAL_GLOBAL);
	Tcl_LinkVar(interp, "::tcl::zipfs::wrmax", (char *) &ZipFS.wrmax,
		TCL_LINK_INT);
	TclMakeEnsemble(interp, "zipfs",
		Tcl_IsSafe(interp) ? (initMap + 4) : initMap);
	Tcl_PkgProvide(interp, "zipfs", "2.0");
    }
    return TCL_OK;
#else
    ZIPFS_ERROR(interp, "no zlib available");
    return TCL_ERROR;
#endif
}

static int
ZipfsAppHookFindTclInit(
    const char *archive)
{
    Tcl_Obj *vfsinitscript;
    int found;

    if (zipfs_literal_tcl_library) {
	return TCL_ERROR;
    }
    if (TclZipfs_Mount(NULL, ZIPFS_ZIP_MOUNT, archive, NULL)) {
	/* Either the file doesn't exist or it is not a zip archive */
	return TCL_ERROR;
    }

    vfsinitscript = Tcl_NewStringObj(ZIPFS_ZIP_MOUNT "/init.tcl", -1);
    Tcl_IncrRefCount(vfsinitscript);
    found = Tcl_FSAccess(vfsinitscript, F_OK);
    Tcl_DecrRefCount(vfsinitscript);
    if (found == 0) {
	zipfs_literal_tcl_library = ZIPFS_ZIP_MOUNT;
	return TCL_OK;
    }

    vfsinitscript = Tcl_NewStringObj(
	    ZIPFS_ZIP_MOUNT "/tcl_library/init.tcl", -1);
    Tcl_IncrRefCount(vfsinitscript);
    found = Tcl_FSAccess(vfsinitscript, F_OK);
    Tcl_DecrRefCount(vfsinitscript);
    if (found == 0) {
	zipfs_literal_tcl_library = ZIPFS_ZIP_MOUNT "/tcl_library";
	return TCL_OK;
    }

    return TCL_ERROR;
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

int
TclZipfs_AppHook(
    int *argcPtr,		/* Pointer to argc */
#ifdef _WIN32
    TCHAR
#else /* !_WIN32 */
    char
#endif /* _WIN32 */
    ***argvPtr)			/* Pointer to argv */
{
#ifdef _WIN32
    Tcl_DString ds;
#endif /* _WIN32 */
    char *archive;

    Tcl_FindExecutable((*argvPtr)[0]);
    archive = (char *) Tcl_GetNameOfExecutable();
    TclZipfs_Init(NULL);

    /*
     * Look for init.tcl in one of the locations mounted later in this
     * function.
     */

    if (!TclZipfs_Mount(NULL, ZIPFS_APP_MOUNT, archive, NULL)) {
	int found;
	Tcl_Obj *vfsInitScript;

	vfsInitScript = Tcl_NewStringObj(ZIPFS_APP_MOUNT "/main.tcl", -1);
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
	    vfsInitScript = Tcl_NewStringObj(
		    ZIPFS_APP_MOUNT "/tcl_library/init.tcl", -1);
	    Tcl_IncrRefCount(vfsInitScript);
	    found = Tcl_FSAccess(vfsInitScript, F_OK);
	    Tcl_DecrRefCount(vfsInitScript);
	    if (found == TCL_OK) {
		zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
		return TCL_OK;
	    }
	}
#ifdef SUPPORT_BUILTIN_ZIP_INSTALL
    } else if (*argcPtr > 1) {
	/*
	 * If the first argument is "install", run the supplied installer
	 * script.
	 */

#ifdef _WIN32
	archive = Tcl_WinTCharToUtf((*argvPtr)[1], -1, &ds);
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
	    vfsInitScript = Tcl_NewStringObj(
		    ZIPFS_ZIP_MOUNT "/tcl_library/install.tcl", -1);
	    Tcl_IncrRefCount(vfsInitScript);
	    if (Tcl_FSAccess(vfsInitScript, F_OK) == 0) {
		Tcl_SetStartupScript(vfsInitScript, NULL);
	    }
	    return TCL_OK;
	} else if (!TclZipfs_Mount(NULL, ZIPFS_APP_MOUNT, archive, NULL)) {
	    int found;
	    Tcl_Obj *vfsInitScript;

	    vfsInitScript = Tcl_NewStringObj(ZIPFS_APP_MOUNT "/main.tcl", -1);
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
	    vfsInitScript = Tcl_NewStringObj(
		    ZIPFS_APP_MOUNT "/tcl_library/init.tcl", -1);
	    Tcl_IncrRefCount(vfsInitScript);
	    found = Tcl_FSAccess(vfsInitScript, F_OK);
	    Tcl_DecrRefCount(vfsInitScript);
	    if (found == TCL_OK) {
		zipfs_literal_tcl_library = ZIPFS_APP_MOUNT "/tcl_library";
		return TCL_OK;
	    }
	}
#ifdef _WIN32
	Tcl_DStringFree(&ds);
#endif /* _WIN32 */
#endif /* SUPPORT_BUILTIN_ZIP_INSTALL */
    }
    return TCL_OK;
}

#ifndef HAVE_ZLIB

/*
 *-------------------------------------------------------------------------
 *
 * TclZipfs_Mount, TclZipfs_Unmount --
 *
 *	Dummy version when no ZLIB support available.
 *
 *-------------------------------------------------------------------------
 */

int
TclZipfs_Mount(
    Tcl_Interp *interp,		/* Current interpreter. */
    const char *mountPoint,	/* Mount point path. */
    const char *zipname,	/* Path to ZIP file to mount. */
    const char *passwd)		/* Password for opening the ZIP, or NULL if
				 * the ZIP is unprotected. */
{
    return TclZipfs_Init(interp, 1);
}

int
TclZipfs_Unmount(
    Tcl_Interp *interp,		/* Current interpreter. */
    const char *mountPoint)	/* Mount point path. */
{
    return TclZipfs_Init(interp, 1);
}
#endif /* !HAVE_ZLIB */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
