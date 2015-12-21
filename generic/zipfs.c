/*
 * zipfs.c --
 *
 *	Implementation of the ZIP filesystem used in AndroWish.
 *
 * Copyright (c) 2013-2015 Christian Werner <chw@ch-werner.de>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclFileSystem.h"
#include "zipfs.h"
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/mman.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef HAVE_ZLIB
#include "zlib.h"
#include "zcrypt.h"

/*
 * Various constants and offsets found in ZIP archive files.
 */

#define ZIP_SIG_LEN                     4

/* Local header of ZIP archive member (at very beginning of each member). */
#define ZIP_LOCAL_HEADER_SIG            0x04034b50
#define ZIP_LOCAL_HEADER_LEN            30
#define ZIP_LOCAL_SIG_OFFS              0
#define ZIP_LOCAL_VERSION_OFFS          4
#define ZIP_LOCAL_FLAGS_OFFS            6
#define ZIP_LOCAL_COMPMETH_OFFS         8
#define ZIP_LOCAL_MTIME_OFFS            10
#define ZIP_LOCAL_MDATE_OFFS            12
#define ZIP_LOCAL_CRC32_OFFS            14
#define ZIP_LOCAL_COMPLEN_OFFS          18
#define ZIP_LOCAL_UNCOMPLEN_OFFS        22
#define ZIP_LOCAL_PATHLEN_OFFS          26
#define ZIP_LOCAL_EXTRALEN_OFFS         28

/* Central header of ZIP archive member at end of ZIP file. */
#define ZIP_CENTRAL_HEADER_SIG          0x02014b50
#define ZIP_CENTRAL_HEADER_LEN          46
#define ZIP_CENTRAL_SIG_OFFS            0
#define ZIP_CENTRAL_VERSIONMADE_OFFS    4
#define ZIP_CENTRAL_VERSION_OFFS        6
#define ZIP_CENTRAL_FLAGS_OFFS          8
#define ZIP_CENTRAL_COMPMETH_OFFS       10
#define ZIP_CENTRAL_MTIME_OFFS          12
#define ZIP_CENTRAL_MDATE_OFFS          14
#define ZIP_CENTRAL_CRC32_OFFS          16
#define ZIP_CENTRAL_COMPLEN_OFFS        20
#define ZIP_CENTRAL_UNCOMPLEN_OFFS      24
#define ZIP_CENTRAL_PATHLEN_OFFS        28
#define ZIP_CENTRAL_EXTRALEN_OFFS       30
#define ZIP_CENTRAL_FCOMMENTLEN_OFFS    32
#define ZIP_CENTRAL_DISKFILE_OFFS       34
#define ZIP_CENTRAL_IATTR_OFFS          36
#define ZIP_CENTRAL_EATTR_OFFS          38
#define ZIP_CENTRAL_LOCALHDR_OFFS       42

/* Central end signature at very end of ZIP file. */
#define ZIP_CENTRAL_END_SIG             0x06054b50
#define ZIP_CENTRAL_END_LEN             22
#define ZIP_CENTRAL_END_SIG_OFFS        0
#define ZIP_CENTRAL_DISKNO_OFFS         4
#define ZIP_CENTRAL_DISKDIR_OFFS        6
#define ZIP_CENTRAL_ENTS_OFFS           8
#define ZIP_CENTRAL_TOTALENTS_OFFS      10
#define ZIP_CENTRAL_DIRSIZE_OFFS        12
#define ZIP_CENTRAL_DIRSTART_OFFS       16
#define ZIP_CENTRAL_COMMENTLEN_OFFS     20

#define ZIP_MIN_VERSION                 20
#define ZIP_COMPMETH_STORED             0
#define ZIP_COMPMETH_DEFLATED           8

#define ZIP_PASSWORD_END_SIG            0x5a5a4b50

/*
 * Macros to read and write 16 and 32 bit integers from/to ZIP archives.
 */

#define zip_read_int(p)         \
    ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
#define zip_read_short(p)       \
    ((p)[0] | ((p)[1] << 8))

#define zip_write_int(p, v)	\
    (p)[0] = (v) & 0xff; (p)[1] = ((v) >> 8) & 0xff; \
    (p)[2] = ((v) >> 16) & 0xff; (p)[3] = ((v) >> 24) & 0xff;
#define zip_write_short(p, v)	\
    (p)[0] = (v) & 0xff; (p)[1] = ((v) >> 8) & 0xff;

/*
 * Windows drive letters.
 */

#if defined(_WIN32) || defined(_WIN64)
#define HAS_DRIVES 1
static const char drvletters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
#else
#define HAS_DRIVES 0
#endif

/*
 * Mutex to protect localtime(3) when no reentrant version available.
 */

#if !defined(_WIN32) && !defined(_WIN64)
#ifndef HAVE_LOCALTIME_R
#ifdef TCL_THREADS
TCL_DECLARE_MUTEX(localtimeMutex)
#endif
#endif
#endif

/*
 * In-core description of mounted ZIP archive file.
 */

typedef struct ZipFile {
    char *name;               /* Archive name */
    Tcl_Channel chan;         /* Channel handle or NULL */
    unsigned char *data;      /* Memory mapped or malloc'ed file */
    long length;              /* Length of memory mapped file */
    unsigned char *tofree;    /* Non-NULL if malloc'ed file */
    int nfiles;               /* Number of files in archive */
    int baseoffs;             /* Archive start */
    int baseoffsp;            /* Password start */
    int centoffs;             /* Archive directory start */
    char pwbuf[264];          /* Password buffer */
#if defined(_WIN32) || defined(_WIN64)
    HANDLE mh;
#endif
    int nopen;                /* Number of open files on archive */
    struct ZipEntry *entries; /* List of files in archive */
    struct ZipEntry *topents; /* List of top-level dirs in archive */
#if HAS_DRIVES
    int mntdrv;	              /* Drive letter of mount point */
#endif
    int mntptlen;             /* Length of mount point */
    char mntpt[1];            /* Mount point */
} ZipFile;

/*
 * In-core description of file contained in mounted ZIP archive.
 */

typedef struct ZipEntry {
    char *name;               /* The full pathname of the virtual file */
    ZipFile *zipfile;         /* The ZIP file holding this virtual file */
    long offset;              /* Data offset into memory mapped ZIP file */
    int nbyte;                /* Uncompressed size of the virtual file */
    int nbytecompr;           /* Compressed size of the virtual file */
    int cmeth;                /* Compress method */
    int isdir;	              /* Set to 1 if directory */
    int depth; 	              /* Number of slashes in path. */
    int crc32;                /* CRC-32 */
    int timestamp;            /* Modification time */
    int isenc;                /* True if data is encrypted */
    unsigned char *data;      /* File data if written */
    struct ZipEntry *next;    /* Next file in the same archive */
    struct ZipEntry *tnext;   /* Next top-level dir in archive */
} ZipEntry;

/*
 * File channel for file contained in mounted ZIP archive.
 */

typedef struct ZipChannel {
    ZipFile *zipfile;         /* The ZIP file holding this channel */
    ZipEntry *zipentry;       /* Pointer back to virtual file */
    unsigned long nmax;       /* Max. size for write */
    unsigned long nbyte;      /* Number of bytes of uncompressed data */
    unsigned long nread;      /* Pos of next byte to be read from the channel */
    unsigned char *ubuf;      /* Pointer to the uncompressed data */
    int iscompr;              /* True if data is compressed */
    int isdir;	              /* Set to 1 if directory */
    int isenc;                /* True if data is encrypted */
    int iswr;                 /* True if open for writing */
    unsigned long keys[3];    /* Key for decryption */
} ZipChannel;

/*
 * Global variables.
 *
 * Most are kept in single ZipFS struct. When build with threading
 * support this struct is protected by the ZipFSMutex (see below).
 *
 * The "fileHash" component is the process wide global table of all known
 * ZIP archive members in all mounted ZIP archives.
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

static const unsigned int crc32tab[256] = {
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

/*
 *-------------------------------------------------------------------------
 *
 * ReadLock, WriteLock, Unlock --
 *
 *	POSIX like rwlock functions to support multiple readers
 *	and single writer on internal structs.
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

#else

#define ReadLock()	do {} while (0)
#define WriteLock()	do {} while (0)
#define Unlock()	do {} while (0)

#endif

/*
 *-------------------------------------------------------------------------
 *
 * DosTimeDate, ToDosTime, ToDosDate --
 *
 *	Functions to perform conversions between DOS time stamps
 *	and POSIX time_t.
 *
 *-------------------------------------------------------------------------
 */

static time_t
DosTimeDate(int dosDate, int dosTime)
{
    struct tm tm;
    time_t ret;

    memset(&tm, 0, sizeof (tm));
    tm.tm_year = (((dosDate & 0xfe00) >> 9) + 80);
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
ToDosTime(time_t when)
{
    struct tm *tmp, tm;

#ifdef TCL_THREADS
#if defined(_WIN32) || defined(_WIN64)
    /* Win32 uses thread local storage */
    tmp = localtime(&when);
    tm = *tmp;
#else
#ifdef HAVE_LOCALTIME_R
    tmp = &tm;
    localtime_r(&when, tmp);
#else
    Tcl_MutexLock(&localtimeMutex);
    tmp = localtime(&when);
    tm = *tmp;
    Tcl_MutexUnlock(&localtimeMutex);
#endif
#endif
#else
    tmp = localtime(&when);
    tm = *tmp;
#endif
    return (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec >> 1);
}

static int
ToDosDate(time_t when)
{
    struct tm *tmp, tm;

#ifdef TCL_THREADS
#if defined(_WIN32) || defined(_WIN64)
    /* Win32 uses thread local storage */
    tmp = localtime(&when);
    tm = *tmp;
#else
#ifdef HAVE_LOCALTIME_R
    tmp = &tm;
    localtime_r(&when, tmp);
#else
    Tcl_MutexLock(&localtimeMutex);
    tmp = localtime(&when);
    tm = *tmp;
    Tcl_MutexUnlock(&localtimeMutex);
#endif
#endif
#else
    tmp = localtime(&when);
    tm = *tmp;
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
CountSlashes(const char *string)
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
 *	This function computes the canonical path from a directory
 *	and file name components into the specified Tcl_DString.
 *
 * Results:
 *	Returns the pointer to the canonical path contained in the
 *	specified Tcl_DString.
 *
 * Side effects:
 *	Modifies the specified Tcl_DString.
 *
 *-------------------------------------------------------------------------
 */

static char *
CanonicalPath(const char *root, const char *tail, Tcl_DString *dsPtr)
{
    char *path;
    int i, j, c, isunc = 0;

#if HAS_DRIVES
    if ((tail[0] != '\0') && (strchr(drvletters, tail[0]) != NULL) &&
	(tail[1] == ':')) {
	tail += 2;
    }
    /* UNC style path */
    if (tail[0] == '\\') {
	root = "";
	++tail;
    }
    if (tail[0] == '\\') {
	root = "/";
	++tail;
    }
#endif
    /* UNC style path */
    if ((root[0] == '/') && (root[1] == '/')) {
	isunc = 1;
    }
    if (tail[0] == '/') {
	root = "";
	++tail;
	isunc = 0;
    }
    if (tail[0] == '/') {
	root = "/";
	++tail;
	isunc = 1;
    }
    i = strlen(root);
    j = strlen(tail);
    Tcl_DStringSetLength(dsPtr, i + j + 1);
    path = Tcl_DStringValue(dsPtr);
    memcpy(path, root, i);
    path[i++] = '/';
    memcpy(path + i, tail, j);
#if HAS_DRIVES
    for (i = 0; path[i] != '\0'; i++) {
	if (path[i] == '\\') {
	    path[i] = '/';
	}
    }
#endif
    for (i = j = 0; (c = path[i]) != '\0'; i++) {
	if (c == '/') {
	    int c2 = path[i + 1];

	    if (c2 == '/') {
		continue;
	    }
	    if (c2 == '.') {
		int c3 = path[i + 2];

		if ((c3 == '/') || (c3 == '\0')) {
		    i++;
		    continue;
		}
		if ((c3 == '.') &&
		    ((path[i + 3] == '/') || (path [i + 3] == '\0'))) {
		    i += 2;
		    while ((j > 0) && (path[j - 1] != '/')) {
			j--;
		    }
		    if (j > isunc) {
			--j;
			while ((j > 1 + isunc) && (path[j - 2] == '/')) {
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
 * AbsolutePath --
 *
 *	This function computes the absolute path from a given
 *	(relative) path name into the specified Tcl_DString.
 *
 * Results:
 *	Returns the pointer to the absolute path contained in the
 *	specified Tcl_DString.
 *
 * Side effects:
 *	Modifies the specified Tcl_DString.
 *
 *-------------------------------------------------------------------------
 */

static char *
AbsolutePath(const char *path,
#if HAS_DRIVES
	     int *drvPtr,
#endif
	     Tcl_DString *dsPtr)
{
    char *result;

#if HAS_DRIVES
    if (drvPtr != NULL) {
	*drvPtr = 0;
    }
#endif
    if (*path == '~') {
	Tcl_DStringAppend(dsPtr, path, -1);
	return Tcl_DStringValue(dsPtr);
    }
    if ((*path != '/')
#if HAS_DRIVES
	&& (*path != '\\') &&
	(((*path != '\0') && (strchr(drvletters, *path) == NULL)) ||
	 (path[1] != ':'))
#endif
	) {
	Tcl_DString pwd;

	/* relative path */
	Tcl_DStringInit(&pwd);
	Tcl_GetCwd(NULL, &pwd);
	result = Tcl_DStringValue(&pwd);
#if HAS_DRIVES
	if ((result[0] != '\0') && (strchr(drvletters, result[0]) != NULL) &&
	    (result[1] == ':')) {
	    if (drvPtr != NULL) {
		drvPtr[0] = result[0];
		if ((drvPtr[0] >= 'a') && (drvPtr[0] <= 'z')) {
		    drvPtr[0] -= 'a' - 'A';
		}
	    }
	    result += 2;
	}
#endif
	result = CanonicalPath(result, path, dsPtr);
	Tcl_DStringFree(&pwd);
    } else {
	/* absolute path */
#if HAS_DRIVES
	if ((path[0] != '\0') && (strchr(drvletters, path[0]) != NULL) &&
	    (path[1] == ':')) {
	    if (drvPtr != NULL) {
		drvPtr[0] = path[0];
		if ((drvPtr[0] >= 'a') && (drvPtr[0] <= 'z')) {
		    drvPtr[0] -= 'a' - 'A';
		}
	    }
	}
#endif
	result = CanonicalPath("", path, dsPtr);
    }
    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLookup --
 *
 *	This function returns the ZIP entry struct corresponding to
 *	the ZIP archive member of the given file name.
 *
 * Results:
 *	Returns the pointer to ZIP entry struct or NULL if the
 *	the given file name could not be found in the global list
 *	of ZIP archive members.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static ZipEntry *
ZipFSLookup(char *filename)
{
    char *realname;
    Tcl_HashEntry *hPtr;
    ZipEntry *z;
    Tcl_DString ds;
#if HAS_DRIVES
    int drive = 0;
#endif

    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    realname = AbsolutePath(filename, &drive, &ds);
#else
    realname = AbsolutePath(filename, &ds);
#endif
    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, realname);
    z = hPtr ? (ZipEntry *) Tcl_GetHashValue(hPtr) : NULL;
    Tcl_DStringFree(&ds);
#if HAS_DRIVES
    if ((z != NULL) && (drive != z->zipfile->mntdrv)) {
	z = NULL;
    }
#endif
    return z;
}

#ifdef NEVER_USED

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSLookupMount --
 *
 *	This function returns an indication if the given file name
 *	corresponds to a mounted ZIP archive file.
 *
 * Results:
 *	Returns true, if the given file name is a mounted ZIP archive file.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSLookupMount(char *filename)
{
    char *realname;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    ZipFile *zf;
    Tcl_DString ds;
    int match = 0;
#if HAS_DRIVES
    int drive = 0;
#endif

    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    realname = AbsolutePath(filename, &drive, &ds);
#else
    realname = AbsolutePath(filename, &ds);
#endif
    hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
    while (hPtr != NULL) {
	if ((zf = (ZipFile *) Tcl_GetHashValue(hPtr)) != NULL) {
#if HAS_DRIVES
	    if (drive != zf->mntdrv) {
		hPtr = Tcl_NextHashEntry(&search);
		continue;
	    }
#endif
	    if (strcmp(zf->mntpt, realname) == 0) {
		match = 1;
		break;
	    }
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DStringFree(&ds);
    return match;
}
#endif

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
 *	A memory mapped ZIP archive is unmapped, allocated memory is
 *	released.
 *
 *-------------------------------------------------------------------------
 */

static void
ZipFSCloseArchive(Tcl_Interp *interp, ZipFile *zf)
{
#if defined(_WIN32) || defined(_WIN64)
    if ((zf->data != NULL) && (zf->tofree == NULL)) {
	UnmapViewOfFile(zf->data);
	zf->data = NULL;
    }
    if (zf->mh != INVALID_HANDLE_VALUE) {
	CloseHandle(zf->mh);
    }
#else
    if ((zf->data != MAP_FAILED) && (zf->tofree == NULL)) {
	munmap(zf->data, zf->length);
	zf->data = MAP_FAILED;
    }
#endif
    if (zf->tofree != NULL) {
	Tcl_Free((char *) zf->tofree);
	zf->tofree = NULL;
    }
    Tcl_Close(interp, zf->chan);
    zf->chan = NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSOpenArchive --
 *
 *	This function opens a ZIP archive file for reading. An attempt
 *	is made to memory map that file. Otherwise it is read into
 *	an allocated memory buffer. The ZIP archive header is verified
 *	and must be valid for the function to succeed. When "needZip"
 *	is zero an embedded ZIP archive in an executable file is accepted.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR otherwise with an error message
 *	placed into the given "interp" if it is not NULL.
 *
 * Side effects:
 *	ZIP archive is memory mapped or read into allocated memory,
 *	the given ZipFile struct is filled with information about
 *	the ZIP archive file.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSOpenArchive(Tcl_Interp *interp, const char *zipname, int needZip,
		 ZipFile *zf)
{
    int i;
    ClientData handle;
    unsigned char *p, *q;

#if defined(_WIN32) || defined(_WIN64)
    zf->data = NULL;
    zf->mh = INVALID_HANDLE_VALUE;
#else
    zf->data = MAP_FAILED;
#endif
    zf->length = 0;
    zf->nfiles = 0;
    zf->baseoffs = zf->baseoffsp = 0;
    zf->tofree = NULL;
    zf->pwbuf[0] = 0;
    zf->chan = Tcl_OpenFileChannel(interp, zipname, "r", 0);
    if (zf->chan == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_GetChannelHandle(zf->chan, TCL_READABLE, &handle) != TCL_OK) {
	if (Tcl_SetChannelOption(interp, zf->chan, "-translation", "binary")
	    != TCL_OK) {
	    goto error;
	}
	if (Tcl_SetChannelOption(interp, zf->chan, "-encoding", "binary")
	    != TCL_OK) {
	    goto error;
	}
	zf->length = Tcl_Seek(zf->chan, 0, SEEK_END);
	if ((zf->length <= 0) || (zf->length > 64 * 1024 * 1024)) {
	    if (interp) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("illegal file size", -1));
	    }
	    goto error;
	}
	Tcl_Seek(zf->chan, 0, SEEK_SET);
	zf->tofree = zf->data = (unsigned char *) Tcl_AttemptAlloc(zf->length);
	if (zf->tofree == NULL) {
	    if (interp) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("out of memory", -1));
	    }
	    goto error;
	}
	i = Tcl_Read(zf->chan, (char *) zf->data, zf->length);
	if (i != zf->length) {
	    if (interp) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("file read error", -1));
	    }
	    goto error;
	}
	Tcl_Close(interp, zf->chan);
	zf->chan = NULL;
    } else {
#if defined(_WIN32) || defined(_WIN64)
	zf->length = GetFileSize((HANDLE) handle, 0);
	if ((zf->length == INVALID_FILE_SIZE) ||
	    (zf->length < ZIP_CENTRAL_END_LEN)) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("invalid file size", -1));
	    }
	    goto error;
	}
	zf->mh = CreateFileMapping((HANDLE) handle, 0, PAGE_READONLY, 0,
				   zf->length, 0);
	if (zf->mh == INVALID_HANDLE_VALUE) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("file mapping failed", -1));
	    }
	    goto error;
	}
	zf->data = MapViewOfFile(zf->mh, FILE_MAP_READ, 0, 0, zf->length);
	if (zf->data == NULL) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("file mapping failed", -1));
	    }
	    goto error;
	}
#else
	zf->length = lseek((int) (long) handle, 0, SEEK_END);
	if ((zf->length == -1) || (zf->length < ZIP_CENTRAL_END_LEN)) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("invalid file size", -1));
	    }
	    goto error;
	}
	lseek((int) (long) handle, 0, SEEK_SET);
	zf->data = (unsigned char *) mmap(0, zf->length, PROT_READ,
					  MAP_FILE | MAP_PRIVATE,
					  (int) (long) handle, 0);
	if (zf->data == MAP_FAILED) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
				 Tcl_NewStringObj("file mapping failed", -1));
	    }
	    goto error;
	}
#endif
    }
    p = zf->data + zf->length - ZIP_CENTRAL_END_LEN;
    while (p >= zf->data) {
	if (*p == (ZIP_CENTRAL_END_SIG & 0xFF)) {
	    if (zip_read_int(p) == ZIP_CENTRAL_END_SIG) {
		break;
	    }
	    p -= ZIP_SIG_LEN;
	} else {
	    --p;
	}
    }
    if (p < zf->data) {
	if (!needZip) {
	    zf->baseoffs = zf->baseoffsp = zf->length;
	    return TCL_OK;
	}
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("wrong end signature", -1));
	}
	goto error;
    }
    zf->nfiles = zip_read_short(p + ZIP_CENTRAL_ENTS_OFFS);
    if (zf->nfiles == 0) {
	if (!needZip) {
	    zf->baseoffs = zf->baseoffsp = zf->length;
	    return TCL_OK;
	}
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("empty archive", -1));
	}
	goto error;
    }
    q = zf->data + zip_read_int(p + ZIP_CENTRAL_DIRSTART_OFFS);
    p -= zip_read_int(p + ZIP_CENTRAL_DIRSIZE_OFFS);
    if ((p < zf->data) || (p > (zf->data + zf->length)) ||
	(q < zf->data) || (q > (zf->data + zf->length))) {
	if (!needZip) {
	    zf->baseoffs = zf->baseoffsp = zf->length;
	    return TCL_OK;
	}
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		Tcl_NewStringObj("archive directory not found", -1));
	}
	goto error;
    }
    zf->baseoffs = zf->baseoffsp = p - q;
    zf->centoffs = p - zf->data;
    q = p;
    for (i = 0; i < zf->nfiles; i++) {
	int pathlen, comlen, extra;

	if ((q + ZIP_CENTRAL_HEADER_LEN) > (zf->data + zf->length)) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("wrong header length", -1));
	    }
	    goto error;
	}
	if (zip_read_int(q) != ZIP_CENTRAL_HEADER_SIG) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("wrong header signature", -1));
	    }
	    goto error;
	}
	pathlen = zip_read_short(q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = zip_read_short(q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = zip_read_short(q + ZIP_CENTRAL_EXTRALEN_OFFS);
	q += pathlen + comlen + extra + ZIP_CENTRAL_HEADER_LEN;
    }
    q = zf->data + zf->baseoffs;
    if ((zf->baseoffs >= 6) &&
	(zip_read_int(q - 4) == ZIP_PASSWORD_END_SIG)) {
	i = q[-5];
	if (q - 5 - i > zf->data) {
	    zf->pwbuf[0] = i;
	    memcpy(zf->pwbuf + 1, q - 5 - i, i);
	    zf->baseoffsp -= i ? (5 + i) : 0;
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
 * Zipfs_Mount --
 *
 *      This procedure is invoked to mount a given ZIP archive file on
 *	a given mountpoint with optional ZIP password.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      A ZIP archive file is read, analyzed and mounted, resources are
 *	allocated.
 *
 *-------------------------------------------------------------------------
 */

int
Zipfs_Mount(Tcl_Interp *interp, const char *zipname, const char *mntpt,
	    const char *passwd)
{
    char *realname, *p;
    int i, pwlen, isNew;
    ZipFile *zf, zf0;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds, fpBuf;
    unsigned char *q;
#if HAS_DRIVES
    int drive = 0;
#endif

    ReadLock();
    if (!ZipFS.initialized) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("not initialized", -1));
	}
	Unlock();
	return TCL_ERROR;
    }
    if (zipname == NULL) {
	Tcl_HashSearch search;
	int ret = TCL_OK;

	i = 0;
	hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
	while (hPtr != NULL) {
	    if ((zf = (ZipFile *) Tcl_GetHashValue(hPtr)) != NULL) {
		if (interp != NULL) {
		    Tcl_AppendElement(interp, zf->mntpt);
		    Tcl_AppendElement(interp, zf->name);
		}
		++i;
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	if (interp == NULL) {
	    ret = (i > 0) ? TCL_OK : TCL_BREAK;
	}
	Unlock();
	return ret;
    }
    if (mntpt == NULL) {
	if (interp == NULL) {
	    Unlock();
	    return TCL_OK;
	}
	Tcl_DStringInit(&ds);
#if HAS_DRIVES
	p = AbsolutePath(zipname, &drive, &ds);
#else
	p = AbsolutePath(zipname, &ds);
#endif
	hPtr = Tcl_FindHashEntry(&ZipFS.zipHash, p);
	if (hPtr != NULL) {
	    if ((zf = Tcl_GetHashValue(hPtr)) != NULL) {
#if HAS_DRIVES
		if (drive == zf->mntdrv)
#endif
		{
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(zf->mntpt, -1));
		}
	    }
	}
	Unlock();
	Tcl_DStringFree(&ds);
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
    if (ZipFSOpenArchive(interp, zipname, 1, &zf0) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    realname = AbsolutePath(zipname, NULL, &ds);
#else
    realname = AbsolutePath(zipname, &ds);
#endif
    WriteLock();
    hPtr = Tcl_CreateHashEntry(&ZipFS.zipHash, realname, &isNew);
    Tcl_DStringSetLength(&ds, 0);
    if (!isNew) {
	zf = (ZipFile *) Tcl_GetHashValue(hPtr);
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "already mounted on \"", zf->mntptlen ?
			     zf->mntpt : "/", "\"", (char *) NULL);
	}
	Unlock();
	Tcl_DStringFree(&ds);
	ZipFSCloseArchive(interp, &zf0);
	return TCL_ERROR;
    }
#if HAS_DRIVES
    if ((mntpt[0] != '\0') && (mntpt[1] == ':') &&
	(strchr(drvletters, mntpt[0]) != NULL)) {
	drive = mntpt[0];
	if ((drive >= 'a') && (drive <= 'z')) {
	    drive -= 'a' - 'A';
	}
	mntpt += 2;
    }
#endif
    if (strcmp(mntpt, "/") == 0) {
	mntpt = "";
    }
    zf = (ZipFile *) Tcl_AttemptAlloc(sizeof (*zf) + strlen(mntpt) + 1);
    if (zf == NULL) {
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "out of memory", (char *) NULL);
	}
	Unlock();
	Tcl_DStringFree(&ds);
	ZipFSCloseArchive(interp, &zf0);
	return TCL_ERROR;
    }
    *zf = zf0;
    zf->name = Tcl_GetHashKey(&ZipFS.zipHash, hPtr);
    strcpy(zf->mntpt, mntpt);
    zf->mntptlen = strlen(zf->mntpt);
#if HAS_DRIVES
    for (p = zf->mntpt; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    zf->mntdrv = drive;
#endif
    zf->entries = NULL;
    zf->topents = NULL;
    zf->nopen = 0;
    Tcl_SetHashValue(hPtr, (ClientData) zf);
    if ((zf->pwbuf[0] == 0) && pwlen) {
	int k = 0;

	i = pwlen;
	zf->pwbuf[k++] = i;
	while (i > 0) {
	    zf->pwbuf[k] = (passwd[i - 1] & 0x0f) |
		pwrot[(passwd[i - 1] >> 4) & 0x0f];
	    k++;
	    i--;
	}
	zf->pwbuf[k] = '\0';
    }
    if (mntpt[0] != '\0') {
	z = (ZipEntry *) Tcl_Alloc(sizeof (*z));
	z->name = NULL;
	z->tnext = NULL;
	z->depth = CountSlashes(mntpt);
	z->zipfile = zf;
	z->isdir = 1;
	z->isenc = 0;
	z->offset = zf->baseoffs;
	z->crc32 = 0;
	z->timestamp = 0;
	z->nbyte = z->nbytecompr = 0;
	z->cmeth = ZIP_COMPMETH_STORED;
	z->data = NULL;
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, mntpt, &isNew);
	if (!isNew) {
	    /* skip it */
	    Tcl_Free((char *) z);
	} else {
	    Tcl_SetHashValue(hPtr, (ClientData) z);
	    z->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	    z->next = zf->entries;
	    zf->entries = z;
	}
    }
    q = zf->data + zf->centoffs;
    Tcl_DStringInit(&fpBuf);
    for (i = 0; i < zf->nfiles; i++) {
	int pathlen, comlen, extra, isdir = 0, dosTime, dosDate, nbcompr, offs;
	unsigned char *lq, *gq = NULL;
	char *fullpath, *path;

	pathlen = zip_read_short(q + ZIP_CENTRAL_PATHLEN_OFFS);
	comlen = zip_read_short(q + ZIP_CENTRAL_FCOMMENTLEN_OFFS);
	extra = zip_read_short(q + ZIP_CENTRAL_EXTRALEN_OFFS);
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
	lq = zf->data + zf->baseoffs +
	     zip_read_int(q + ZIP_CENTRAL_LOCALHDR_OFFS);
	if ((lq < zf->data) || (lq > (zf->data + zf->length))) {
	    goto nextent;
	}
	nbcompr = zip_read_int(lq + ZIP_LOCAL_COMPLEN_OFFS);
	if (!isdir && (nbcompr == 0) &&
	    (zip_read_int(lq + ZIP_LOCAL_UNCOMPLEN_OFFS) == 0) &&
	    (zip_read_int(lq + ZIP_LOCAL_CRC32_OFFS) == 0)) {
	    gq = q;
	    nbcompr = zip_read_int(gq + ZIP_CENTRAL_COMPLEN_OFFS);
	}
	offs = (lq - zf->data)
	     + ZIP_LOCAL_HEADER_LEN
	     + zip_read_short(lq + ZIP_LOCAL_PATHLEN_OFFS)
	     + zip_read_short(lq + ZIP_LOCAL_EXTRALEN_OFFS);
	if ((offs + nbcompr) > zf->length) {
	    goto nextent;
	}
	if (!isdir && (mntpt[0] == '\0') && !CountSlashes(path)) {
#ifdef ANDROID
	    /*
	     * When mounting the ZIP archive on the root directory try
	     * to remap top level regular files of the archive to
	     * /assets/.root/... since this directory should not be
	     * in a valid APK due to the leading dot in the file name
	     * component. This trick should make the files
	     * AndroidManifest.xml, resources.arsc, and classes.dex
	     * visible to Tcl.
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
#else
	    /*
	     * Regular files skipped when mounting on root.
	     */
	    goto nextent;
#endif
	}
	Tcl_DStringSetLength(&fpBuf, 0);
	fullpath = CanonicalPath(mntpt, path, &fpBuf);
	z = (ZipEntry *) Tcl_Alloc(sizeof (*z));
	z->name = NULL;
	z->tnext = NULL;
	z->depth = CountSlashes(fullpath);
	z->zipfile = zf;
	z->isdir = isdir;
	z->isenc = (zip_read_short(lq + ZIP_LOCAL_FLAGS_OFFS) & 1)
	    && (nbcompr > 12);
	z->offset = offs;
	if (gq != NULL) {
	    z->crc32 = zip_read_int(gq + ZIP_CENTRAL_CRC32_OFFS);
	    dosDate = zip_read_short(gq + ZIP_CENTRAL_MDATE_OFFS);
	    dosTime = zip_read_short(gq + ZIP_CENTRAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->nbyte = zip_read_int(gq + ZIP_CENTRAL_UNCOMPLEN_OFFS);
	    z->cmeth = zip_read_short(gq + ZIP_CENTRAL_COMPMETH_OFFS);
	} else {
	    z->crc32 = zip_read_int(lq + ZIP_LOCAL_CRC32_OFFS);
	    dosDate = zip_read_short(lq + ZIP_LOCAL_MDATE_OFFS);
	    dosTime = zip_read_short(lq + ZIP_LOCAL_MTIME_OFFS);
	    z->timestamp = DosTimeDate(dosDate, dosTime);
	    z->nbyte = zip_read_int(lq + ZIP_LOCAL_UNCOMPLEN_OFFS);
	    z->cmeth = zip_read_short(lq + ZIP_LOCAL_COMPMETH_OFFS);
	}
	z->nbytecompr = nbcompr;
	z->data = NULL;
	hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, fullpath, &isNew);
	if (!isNew) {
	    /* should not happen but skip it anyway */
	    Tcl_Free((char *) z);
	} else {
	    Tcl_SetHashValue(hPtr, (ClientData) z);
	    z->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
	    z->next = zf->entries;
	    zf->entries = z;
	    if (isdir && (mntpt[0] == '\0') && (z->depth == 1)) {
		z->tnext = zf->topents;
		zf->topents = z;
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
		    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, dir);
		    if (hPtr != NULL) {
			break;
		    }
		    zd = (ZipEntry *) Tcl_Alloc(sizeof (*zd));
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
		    hPtr = Tcl_CreateHashEntry(&ZipFS.fileHash, dir, &isNew);
		    if (!isNew) {
			/* should not happen but skip it anyway */
			Tcl_Free((char *) zd);
		    } else {
			Tcl_SetHashValue(hPtr, (ClientData) zd);
			zd->name = Tcl_GetHashKey(&ZipFS.fileHash, hPtr);
			zd->next = zf->entries;
			zf->entries = zd;
			if ((mntpt[0] == '\0') && (zd->depth == 1)) {
			    zd->tnext = zf->topents;
			    zf->topents = zd;
			}
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
    Unlock();
    Tcl_FSMountsChanged(NULL);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zipfs_Unmount --
 *
 *      This procedure is invoked to unmount a given ZIP archive.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      A mounted ZIP archive file is unmounted, resources are free'd.
 *
 *-------------------------------------------------------------------------
 */

int
Zipfs_Unmount(Tcl_Interp *interp, const char *zipname)
{
    char *realname;
    ZipFile *zf;
    ZipEntry *z, *znext;
    Tcl_HashEntry *hPtr;
    Tcl_DString ds;
    int ret = TCL_OK, unmounted = 0;
#if HAS_DRIVES
    int drive = 0;
#endif

    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    realname = AbsolutePath(zipname, &drive, &ds);
#else
    realname = AbsolutePath(zipname, &ds);
#endif
    WriteLock();
    if (!ZipFS.initialized) {
	goto done;
    }
    hPtr = Tcl_FindHashEntry(&ZipFS.zipHash, realname);
    if (hPtr == NULL) {
	/* don't report error */
	goto done;
    }
    zf = (ZipFile *) Tcl_GetHashValue(hPtr);
#if HAS_DRIVES
    if (drive != zf->mntdrv) {
	/* don't report error */
	goto done;
    }
#endif
    if (zf->nopen > 0) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
			     Tcl_NewStringObj("filesystem is busy", -1));
	}
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
	    Tcl_Free((char *) z->data);
	}
	Tcl_Free((char *) z);
    }
    ZipFSCloseArchive(interp, zf);
    Tcl_Free((char *) zf);
    unmounted = 1;
done:
    Unlock();
    Tcl_DStringFree(&ds);
    if (unmounted) {
	Tcl_FSMountsChanged(NULL);
    }
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMountCmd --
 *
 *      This procedure is invoked to process the "zipfs::mount" command.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      A ZIP archive file is mounted, resources are allocated.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMountCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, const char **argv)
{
    if (argc > 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " ?zipfile ?mountpoint? ?password???\"", 0);
	return TCL_ERROR;
    }
    return Zipfs_Mount(interp, (argc > 1) ? argv[1] : NULL,
		       (argc > 2) ? argv[2] : NULL,
		       (argc > 3) ? argv[3] : NULL);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSUnmountCmd --
 *
 *      This procedure is invoked to process the "zipfs::unmount" command.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      A mounted ZIP archive file is unmounted, resources are free'd.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSUnmountCmd(ClientData clientData, Tcl_Interp *interp,
		int argc, const char **argv)
{
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " zipfile\"", (char *) NULL);
	return TCL_ERROR;
    }
    return Zipfs_Unmount(interp, argv[1]);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMountCmd --
 *
 *      This procedure is invoked to process the "zipfs::mkkey" command.
 *	It produces a rotated password to be embedded into an image file.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkKeyCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, const char **argv)
{
    int len, i = 0;
    char pwbuf[264];

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " password\"", (char *) NULL);
	return TCL_ERROR;
    }
    len = strlen(argv[1]);
    if (len == 0) {
	return TCL_OK;
    }
    if ((len > 255) || (strchr(argv[1], 0xff) != NULL)) {
	Tcl_SetObjResult(interp,
			 Tcl_NewStringObj("illegal password", -1));
	return TCL_ERROR;
    }
    while (len > 0) {
	int ch = argv[1][len - 1];

	pwbuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	i++;
	len--;
    }
    pwbuf[i] = i;
    ++i;
    pwbuf[i++] = (char) ZIP_PASSWORD_END_SIG;
    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 8);
    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 16);
    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 24);
    pwbuf[i] = '\0';
    Tcl_AppendResult(interp, pwbuf, (char *) NULL);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipAddFile --
 *
 *      This procedure is used by ZipFSMkZipOrImgCmd() to add a single
 *	file to the output ZIP archive file being written. A ZipEntry
 *	struct about the input file is added to the given fileHash table
 *	for later creation of the central ZIP directory.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	Input file is read and (compressed and) written to the output
 *	ZIP archive file.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipAddFile(Tcl_Interp *interp, const char *path, const char *name,
	   Tcl_Channel out, const char *passwd,
	   char *buf, int bufsize, Tcl_HashTable *fileHash)
{
    Tcl_Channel in;
    Tcl_HashEntry *hPtr;
    ZipEntry *z;
    z_stream stream;
    const char *zpath;
    int nbyte, nbytecompr, len, crc, flush, pos[3], zpathlen, olen;
    int mtime = 0, isNew, align = 0, cmeth;
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
	Tcl_AppendResult(interp, "path too long for \"", path, "\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    in = Tcl_OpenFileChannel(interp, path, "r", 0);
    if ((in == NULL) ||
	(Tcl_SetChannelOption(interp, in, "-translation", "binary")
	 != TCL_OK) ||
	(Tcl_SetChannelOption(interp, in, "-encoding", "binary")
	 != TCL_OK)) {
#if defined(_WIN32) || defined(_WIN64)
	 /* hopefully a directory */
	 if (strcmp("permission denied", Tcl_PosixError(interp)) == 0) {
	    Tcl_Close(interp, in);
	    return TCL_OK;
	}
#endif
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
    while ((len = Tcl_Read(in, buf, bufsize)) > 0) {
	crc = crc32(crc, (unsigned char *) buf, len);
	nbyte += len;
    }
    if (len == -1) {
	if (nbyte == 0) {
	    if (strcmp("illegal operation on a directory",
		       Tcl_PosixError(interp)) == 0) {
		Tcl_Close(interp, in);
		return TCL_OK;
	    }
	}
	Tcl_AppendResult(interp, "read error on \"", path, "\"",
			 (char *) NULL);
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    if (Tcl_Seek(in, 0, SEEK_SET) == -1) {
	Tcl_AppendResult(interp, "seek error on \"", path, "\"",
			 (char *) NULL);
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    pos[0] = Tcl_Tell(out);
    memset(buf, '\0', ZIP_LOCAL_HEADER_LEN);
    memcpy(buf + ZIP_LOCAL_HEADER_LEN, zpath, zpathlen);
    len = zpathlen + ZIP_LOCAL_HEADER_LEN;
    if (Tcl_Write(out, buf, len) != len) {
wrerr:
	Tcl_AppendResult(interp, "write error", (char *) NULL);
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    if ((len + pos[0]) & 3) {
	char abuf[8];

	/*
	 * Align payload to next 4-byte boundary using a dummy extra
	 * entry similar to the zipalign tool from Android's SDK.
	 */
	align = 4 + ((len + pos[0]) & 3);
	zip_write_short(abuf, 0xffff);
	zip_write_short(abuf + 2, align - 4);
	zip_write_int(abuf + 4, 0x03020100);
	if (Tcl_Write(out, abuf, align) != align) {
	    goto wrerr;
	}
    }
    if (passwd != NULL) {
	int i, ch, tmp;
	unsigned char kvbuf[24];
	Tcl_Obj *ret;

	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    if (Tcl_Eval(interp, "expr int(rand() * 256) % 256") != TCL_OK) {
		Tcl_AppendResult(interp, "PRNG error", (char *) NULL);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    ret = Tcl_GetObjResult(interp);
	    if (Tcl_GetIntFromObj(interp, ret, &ch) != TCL_OK) {
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    kvbuf[i + 12] = (unsigned char) zencode(keys, crc32tab, ch, tmp);
	}
	Tcl_ResetResult(interp);
	init_keys(passwd, keys, crc32tab);
	for (i = 0; i < 12 - 2; i++) {
	    kvbuf[i] = (unsigned char) zencode(keys, crc32tab,
					       kvbuf[i + 12], tmp);
	}
	kvbuf[i++] = (unsigned char) zencode(keys, crc32tab, crc >> 16, tmp);
	kvbuf[i++] = (unsigned char) zencode(keys, crc32tab, crc >> 24, tmp);
	len = Tcl_Write(out, (char *) kvbuf, 12);
	memset(kvbuf, 0, 24);
	if (len != 12) {
	    Tcl_AppendResult(interp, "write error", (char *) NULL);
	    Tcl_Close(interp, in);
	    return TCL_ERROR;
	}
	memcpy(keys0, keys, sizeof (keys0));
	nbytecompr += 12;
    }
    Tcl_Flush(out);
    pos[2] = Tcl_Tell(out);
    cmeth = ZIP_COMPMETH_DEFLATED;
    memset(&stream, 0, sizeof (stream));
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if (deflateInit2(&stream, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)
	!= Z_OK) {
	Tcl_AppendResult(interp, "compression init error on \"", path, "\"",
			 (char *) NULL);
	Tcl_Close(interp, in);
	return TCL_ERROR;
    }
    do {
	len = Tcl_Read(in, buf, bufsize);
	if (len == -1) {
	    Tcl_AppendResult(interp, "read error on \"", path, "\"",
			     (char *) NULL);
	    deflateEnd(&stream);
	    Tcl_Close(interp, in);
	    return TCL_ERROR;
	}
	stream.avail_in = len;
	stream.next_in = (unsigned char *) buf;
	flush = Tcl_Eof(in) ? Z_FINISH : Z_NO_FLUSH;
	do {
	    stream.avail_out = sizeof (obuf);
	    stream.next_out = (unsigned char *) obuf;
	    len = deflate(&stream, flush);
	    if (len == Z_STREAM_ERROR) {
		Tcl_AppendResult(interp, "deflate error on \"", path, "\"",
				 (char *) NULL);
		deflateEnd(&stream);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    }
	    olen = sizeof (obuf) - stream.avail_out;
	    if (passwd != NULL) {
		int i, tmp;

		for (i = 0; i < olen; i++) {
		    obuf[i] = (char) zencode(keys, crc32tab, obuf[i], tmp);
		}
	    }
	    if (olen && (Tcl_Write(out, obuf, olen) != olen)) {
		Tcl_AppendResult(interp, "write error", (char *) NULL);
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
  	 * Compressed file larger than input,
  	 * write it again uncompressed.
  	 */
	if ((int) Tcl_Seek(in, 0, SEEK_SET) != 0) {
	    goto seekErr;
	}
	if ((int) Tcl_Seek(out, pos[2], SEEK_SET) != pos[2]) {
seekErr:
	    Tcl_Close(interp, in);
	    Tcl_AppendResult(interp, "seek error", (char *) NULL);
	    return TCL_ERROR;
	}
	nbytecompr = (passwd != NULL) ? 12 : 0;
	while (1) {
	    len = Tcl_Read(in, buf, bufsize);
	    if (len == -1) {
		Tcl_AppendResult(interp, "read error on \"", path, "\"",
				 (char *) NULL);
		Tcl_Close(interp, in);
		return TCL_ERROR;
	    } else if (len == 0) {
		break;
	    }
	    if (passwd != NULL) {
		int i, tmp;

		for (i = 0; i < len; i++) {
		    buf[i] = (char) zencode(keys0, crc32tab, buf[i], tmp);
		}
	    }
	    if (Tcl_Write(out, buf, len) != len) {
		Tcl_AppendResult(interp, "write error", (char *) NULL);
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

    z = (ZipEntry *) Tcl_Alloc(sizeof (*z));
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
	Tcl_AppendResult(interp, "non-unique path name \"", path, "\"",
			 (char *) NULL);
	Tcl_Free((char *) z);
	return TCL_ERROR;
    } else {
	Tcl_SetHashValue(hPtr, (ClientData) z);
	z->name = Tcl_GetHashKey(fileHash, hPtr);
	z->next = NULL;
    }

    /*
     * Write final local header information.
     */
    zip_write_int(buf + ZIP_LOCAL_SIG_OFFS, ZIP_LOCAL_HEADER_SIG);
    zip_write_short(buf + ZIP_LOCAL_VERSION_OFFS, ZIP_MIN_VERSION);
    zip_write_short(buf + ZIP_LOCAL_FLAGS_OFFS, z->isenc);
    zip_write_short(buf + ZIP_LOCAL_COMPMETH_OFFS, z->cmeth);
    zip_write_short(buf + ZIP_LOCAL_MTIME_OFFS, ToDosTime(z->timestamp));
    zip_write_short(buf + ZIP_LOCAL_MDATE_OFFS, ToDosDate(z->timestamp));
    zip_write_int(buf + ZIP_LOCAL_CRC32_OFFS, z->crc32);
    zip_write_int(buf + ZIP_LOCAL_COMPLEN_OFFS, z->nbytecompr);
    zip_write_int(buf + ZIP_LOCAL_UNCOMPLEN_OFFS, z->nbyte);
    zip_write_short(buf + ZIP_LOCAL_PATHLEN_OFFS, zpathlen);
    zip_write_short(buf + ZIP_LOCAL_EXTRALEN_OFFS, align);
    if ((int) Tcl_Seek(out, pos[0], SEEK_SET) != pos[0]) {
	Tcl_DeleteHashEntry(hPtr);
	Tcl_Free((char *) z);
	Tcl_AppendResult(interp, "seek error", (char *) NULL);
	return TCL_ERROR;
    }
    if (Tcl_Write(out, buf, ZIP_LOCAL_HEADER_LEN) != ZIP_LOCAL_HEADER_LEN) {
	Tcl_DeleteHashEntry(hPtr);
	Tcl_Free((char *) z);
	Tcl_AppendResult(interp, "write error", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Flush(out);
    if ((int) Tcl_Seek(out, pos[1], SEEK_SET) != pos[1]) {
	Tcl_DeleteHashEntry(hPtr);
	Tcl_Free((char *) z);
	Tcl_AppendResult(interp, "seek error", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkZipOrImgCmd --
 *
 *      This procedure is creates a new ZIP archive file or image file
 *	given output filename, input directory of files to be archived,
 *	optional password, and optional image to be prepended to the
 *	output ZIP archive file.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	A new ZIP archive file or image file is written.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkZipOrImgCmd(ClientData clientData, Tcl_Interp *interp,
		   int isImg, int argc, const char **argv)
{
    Tcl_Channel out;
    int len = 0, pwlen = 0, slen = 0, i, count, ret = TCL_ERROR, largc, pos[3];
    const char **largv;
    Tcl_DString ds;
    ZipEntry *z;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable fileHash;
    char pwbuf[264], buf[4096];

    if ((argc < 3) || (argc > (isImg ? 6 : 5))) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " outfile indir ?strip? ?password?",
			 isImg ? " ?infile?\"" : "\"", (char *) NULL);
	return TCL_ERROR;
    }
    pwbuf[0] = 0;
    if (argc > 4) {
	pwlen = strlen(argv[4]);
	if ((pwlen > 255) || (strchr(argv[4], 0xff) != NULL)) {
	    Tcl_AppendResult(interp, "illegal password", (char *) NULL);
	    return TCL_ERROR;
	}
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringAppendElement(&ds, "::zipfs::find");
    Tcl_DStringAppendElement(&ds, argv[2]);
    if (Tcl_Eval(interp, Tcl_DStringValue(&ds)) != TCL_OK) {
	Tcl_DStringFree(&ds);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&ds);
    if (Tcl_SplitList(interp, Tcl_GetStringResult(interp), &largc, &largv)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_ResetResult(interp);
    if (largc == 0) {
	Tcl_Free((char *) largv);
	Tcl_AppendResult(interp, "empty archive", (char *) NULL);
	return TCL_ERROR;
    }
    out = Tcl_OpenFileChannel(interp, argv[1], "w", 0755);
    if ((out == NULL) ||
	(Tcl_SetChannelOption(interp, out, "-translation", "binary")
	 != TCL_OK) ||
	(Tcl_SetChannelOption(interp, out, "-encoding", "binary")
	 != TCL_OK)) {
	Tcl_Close(interp, out);
	Tcl_Free((char *) largv);
	return TCL_ERROR;
    }
    if (isImg) {
	ZipFile zf0;

	if (ZipFSOpenArchive(interp, (argc > 5) ? argv[5] :
			     Tcl_GetNameOfExecutable(), 0, &zf0) != TCL_OK) {
	    Tcl_Close(interp, out);
	    Tcl_Free((char *) largv);
	    return TCL_ERROR;
	}
	if (pwlen && (argc > 4)) {
	    i = 0;
	    len = pwlen;
	    while (len > 0) {
		int ch = argv[4][len - 1];

		pwbuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
		i++;
		len--;
	    }
	    pwbuf[i] = i;
	    ++i;
	    pwbuf[i++] = (char) ZIP_PASSWORD_END_SIG;
	    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 8);
	    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 16);
	    pwbuf[i++] = (char) (ZIP_PASSWORD_END_SIG >> 24);
	    pwbuf[i] = '\0';
	}
	i = Tcl_Write(out, (char *) zf0.data, zf0.baseoffsp);
	if (i != zf0.baseoffsp) {
	    Tcl_AppendResult(interp, "write error", (char *) NULL);
	    Tcl_Close(interp, out);
	    Tcl_Free((char *) largv);
	    ZipFSCloseArchive(interp, &zf0);
	    return TCL_ERROR;
	}
	ZipFSCloseArchive(interp, &zf0);
	len = strlen(pwbuf);
	if (len > 0) {
	    i = Tcl_Write(out, pwbuf, len);
	    if (i != len) {
		Tcl_AppendResult(interp, "write error", (char *) NULL);
		Tcl_Close(interp, out);
		Tcl_Free((char *) largv);
		return TCL_ERROR;
	    }
	}
	memset(pwbuf, 0, sizeof (pwbuf));
	Tcl_Flush(out);
    }
    Tcl_InitHashTable(&fileHash, TCL_STRING_KEYS);
    pos[0] = Tcl_Tell(out);
    if (argc > 3) {
	slen = strlen(argv[3]);
    }
    for (i = 0; i < largc; i++) {
	const char *name = largv[i];

	if (slen > 0) {
	    len = strlen(name);
	    if ((len <= slen) || (strncmp(argv[3], name, slen) != 0)) {
		continue;
	    }
	    name += slen;
	}
	while (name[0] == '/') {
	    ++name;
	}
	if (name[0] == '\0') {
	    continue;
	}
	if (ZipAddFile(interp, largv[i], name, out,
		(pwlen > 0) ? argv[4] : NULL, buf, sizeof (buf), &fileHash)
	    != TCL_OK) {
	    goto done;
	}
    }
    pos[1] = Tcl_Tell(out);
    count = 0;
    for (i = 0; i < largc; i++) {
	const char *name = largv[i];

	if (slen > 0) {
	    len = strlen(name);
	    if ((len <= slen) || (strncmp(argv[3], name, slen) != 0)) {
		continue;
	    }
	    name += slen;
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
	z = (ZipEntry *) Tcl_GetHashValue(hPtr);
	len = strlen(z->name);
	zip_write_int(buf + ZIP_CENTRAL_SIG_OFFS, ZIP_CENTRAL_HEADER_SIG);
	zip_write_short(buf + ZIP_CENTRAL_VERSIONMADE_OFFS, ZIP_MIN_VERSION);
	zip_write_short(buf + ZIP_CENTRAL_VERSION_OFFS, ZIP_MIN_VERSION);
	zip_write_short(buf + ZIP_CENTRAL_FLAGS_OFFS, z->isenc ? 1 : 0);
	zip_write_short(buf + ZIP_CENTRAL_COMPMETH_OFFS, z->cmeth);
	zip_write_short(buf + ZIP_CENTRAL_MTIME_OFFS, ToDosTime(z->timestamp));
	zip_write_short(buf + ZIP_CENTRAL_MDATE_OFFS, ToDosDate(z->timestamp));
	zip_write_int(buf + ZIP_CENTRAL_CRC32_OFFS, z->crc32);
	zip_write_int(buf + ZIP_CENTRAL_COMPLEN_OFFS, z->nbytecompr);
	zip_write_int(buf + ZIP_CENTRAL_UNCOMPLEN_OFFS, z->nbyte);
	zip_write_short(buf + ZIP_CENTRAL_PATHLEN_OFFS, len);
	zip_write_short(buf + ZIP_CENTRAL_EXTRALEN_OFFS, 0);
	zip_write_short(buf + ZIP_CENTRAL_FCOMMENTLEN_OFFS, 0);
	zip_write_short(buf + ZIP_CENTRAL_DISKFILE_OFFS, 0);
	zip_write_short(buf + ZIP_CENTRAL_IATTR_OFFS, 0);
	zip_write_int(buf + ZIP_CENTRAL_EATTR_OFFS, 0);
	zip_write_int(buf + ZIP_CENTRAL_LOCALHDR_OFFS, z->offset - pos[0]);
	memcpy(buf + ZIP_CENTRAL_HEADER_LEN, z->name, len);
	len += ZIP_CENTRAL_HEADER_LEN;
	if (Tcl_Write(out, buf, len) != len) {
	    Tcl_AppendResult(interp, "write error", (char *) NULL);
	    goto done;
	}
	count++;
    }
    Tcl_Flush(out);
    pos[2] = Tcl_Tell(out);
    zip_write_int(buf + ZIP_CENTRAL_END_SIG_OFFS, ZIP_CENTRAL_END_SIG);
    zip_write_short(buf + ZIP_CENTRAL_DISKNO_OFFS, 0);
    zip_write_short(buf + ZIP_CENTRAL_DISKDIR_OFFS, 0);
    zip_write_short(buf + ZIP_CENTRAL_ENTS_OFFS, count);
    zip_write_short(buf + ZIP_CENTRAL_TOTALENTS_OFFS, count);
    zip_write_int(buf + ZIP_CENTRAL_DIRSIZE_OFFS, pos[2] - pos[1]);
    zip_write_int(buf + ZIP_CENTRAL_DIRSTART_OFFS, pos[1] - pos[0]);
    zip_write_short(buf + ZIP_CENTRAL_COMMENTLEN_OFFS, 0);
    if (Tcl_Write(out, buf, ZIP_CENTRAL_END_LEN) != ZIP_CENTRAL_END_LEN) {
	Tcl_AppendResult(interp, "write error", (char *) NULL);
	goto done;
    }
    Tcl_Flush(out);
    ret = TCL_OK;
done:
    Tcl_Free((char *) largv);
    Tcl_Close(interp, out);
    hPtr = Tcl_FirstHashEntry(&fileHash, &search);
    while (hPtr != NULL) {
	z = (ZipEntry *) Tcl_GetHashValue(hPtr);
	Tcl_Free((char *) z);
	Tcl_DeleteHashEntry(hPtr);
	hPtr = Tcl_FirstHashEntry(&fileHash, &search);
    }
    Tcl_DeleteHashTable(&fileHash);
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkZipCmd --
 *
 *      This procedure is invoked to process the "zipfs::mkzip" command.
 *	See description of ZipFSMkZipOrImgCmd().
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImgCmd().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkZipCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, const char **argv)
{
    return ZipFSMkZipOrImgCmd(clientData, interp, 0, argc, argv);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSMkImgCmd --
 *
 *      This procedure is invoked to process the "zipfs::mkimg" command.
 *	See description of ZipFSMkZipOrImgCmd().
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	See description of ZipFSMkZipOrImgCmd().
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSMkImgCmd(ClientData clientData, Tcl_Interp *interp,
	      int argc, const char **argv)
{
    return ZipFSMkZipOrImgCmd(clientData, interp, 1, argc, argv);
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSExistsObjCmd --
 *
 *      This procedure is invoked to process the "zipfs::exists" command.
 *	It tests for the existence of a file in the ZIP filesystem and
 *	places a boolean into the interp's result.
 *
 * Results:
 *      Always TCL_OK.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSExistsObjCmd(ClientData clientData, Tcl_Interp *interp,
		  int objc, Tcl_Obj *const objv[])
{
    char *filename;
    int exists;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }
    filename = Tcl_GetStringFromObj(objv[1], 0);
    ReadLock();
    exists = ZipFSLookup(filename) != NULL;
    Unlock();
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), exists);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSInfoObjCmd --
 *
 *      This procedure is invoked to process the "zipfs::info" command.
 *	On success, it returns a Tcl list made up of name of ZIP archive
 *	file, size uncompressed, size compressed, and archive offset of
 *	a file in the ZIP filesystem.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSInfoObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *const objv[])
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
	Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(z->nbyte));
	Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(z->nbytecompr));
	Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(z->offset));
    }
    Unlock();
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipFSListObjCmd --
 *
 *      This procedure is invoked to process the "zipfs::list" command.
 *	On success, it returns a Tcl list of files of the ZIP filesystem
 *	which match a search pattern (glob or regexp).
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipFSListObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *const objv[])
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
	    Tcl_AppendResult(interp, "unknown option \"", what,
			     "\"", (char *) NULL);
	    return TCL_ERROR;
	}
    } else if (objc == 2) {
	pattern = Tcl_GetStringFromObj(objv[1], 0);
    }
    ReadLock();
    if (pattern != NULL) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
	     hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    if (Tcl_StringMatch(z->name, pattern)) {
		Tcl_ListObjAppendElement(interp, result,
					 Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else if (regexp != NULL) {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
	     hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    if (Tcl_RegExpExec(interp, regexp, z->name, z->name)) {
		Tcl_ListObjAppendElement(interp, result,
					 Tcl_NewStringObj(z->name, -1));
	    }
	}
    } else {
	for (hPtr = Tcl_FirstHashEntry(&ZipFS.fileHash, &search);
	     hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
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
ZipChannelClose(ClientData instanceData, Tcl_Interp *interp)
{
    ZipChannel *info = (ZipChannel *) instanceData;

    if (info->iscompr && (info->ubuf != NULL)) {
	Tcl_Free((char *) info->ubuf);
	info->ubuf = NULL;
    }
    if (info->isenc) {
	info->isenc = 0;
	memset(info->keys, 0, sizeof (info->keys));
    }
    if (info->iswr) {
	ZipEntry *z = info->zipentry;
	unsigned char *newdata;

	newdata = (unsigned char *)
	    Tcl_AttemptRealloc((char *) info->ubuf, info->nread);
	if (newdata != NULL) {
	    if (z->data != NULL) {
		Tcl_Free((char *) z->data);
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
	    Tcl_Free((char *) info->ubuf);
	}
    }
    WriteLock();
    info->zipfile->nopen--;
    Unlock();
    Tcl_Free((char *) info);
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
ZipChannelRead(ClientData instanceData, char *buf, int toRead, int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;
    unsigned long nextpos;

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
	int i, ch;

	for (i = 0; i < toRead; i++) {
	    ch = info->ubuf[i + info->nread];
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
ZipChannelWrite(ClientData instanceData, const char *buf,
		int toWrite, int *errloc)
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
ZipChannelSeek(ClientData instanceData, long offset, int mode, int *errloc)
{
    ZipChannel *info = (ZipChannel *) instanceData;

    if (info->isdir) {
	*errloc = EINVAL;
	return -1;
    }
    switch (mode) {
    case SEEK_CUR:
	offset += info->nread;
	break;
    case SEEK_END:
	offset += info->nbyte;
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
    } else if ((unsigned long) offset > info->nbyte) {
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
 *	This function is called for event notifications on channel.
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
ZipChannelWatchChannel(ClientData instanceData, int mask)
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
 *	Always TCL_ERROR since there's never an OS handle for a
 *	file within a ZIP archive.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
ZipChannelGetFile(ClientData instanceData, int direction,
		  ClientData *handlePtr)
{
    return TCL_ERROR;
}

/*
 * The channel type/driver definition used for ZIP archive members.
 */

static Tcl_ChannelType ZipChannelType = {
    "zip",                  /* Type name. */
#ifdef TCL_CHANNEL_VERSION_4
    TCL_CHANNEL_VERSION_4,
    ZipChannelClose,        /* Close channel, clean instance data */
    ZipChannelRead,         /* Handle read request */
    ZipChannelWrite,        /* Handle write request */
    ZipChannelSeek,         /* Move location of access point, NULL'able */
    NULL,                   /* Set options, NULL'able */
    NULL,                   /* Get options, NULL'able */
    ZipChannelWatchChannel, /* Initialize notifier */
    ZipChannelGetFile,      /* Get OS handle from the channel */
    NULL,                   /* 2nd version of close channel, NULL'able */
    NULL,                   /* Set blocking mode for raw channel, NULL'able */
    NULL,                   /* Function to flush channel, NULL'able */
    NULL,                   /* Function to handle event, NULL'able */
    NULL,                   /* Wide seek function, NULL'able */
    NULL,                   /* Thread action function, NULL'able */
#else
    NULL,                   /* Set blocking/nonblocking behaviour, NULL'able */
    ZipChannelClose,        /* Close channel, clean instance data */
    ZipChannelRead,         /* Handle read request */
    ZipChannelWrite,        /* Handle write request */
    ZipChannelSeek,         /* Move location of access point, NULL'able */
    NULL,                   /* Set options, NULL'able */
    NULL,                   /* Get options, NULL'able */
    ZipChannelWatchChannel, /* Initialize notifier */
    ZipChannelGetFile,      /* Get OS handle from the channel */
#endif
};

/*
 *-------------------------------------------------------------------------
 *
 * ZipChannelOpen --
 *
 *	This function opens a Tcl_Channel on a file from a mounted ZIP
 *	archive according to given open mode.
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
ZipChannelOpen(Tcl_Interp *interp, char *filename, int mode, int permissions)
{
    ZipEntry *z;
    ZipChannel *info;
    int i, ch, trunc, wr, flags = 0;
    char cname[128];

    if ((mode & O_APPEND) ||
	((ZipFS.wrmax <= 0) && (mode & (O_WRONLY | O_RDWR)))) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("unsupported open mode", -1));
	}
	return NULL;
    }
    WriteLock();
    z = ZipFSLookup(filename);
    if (z == NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("file not found", -1));
	}
	goto error;
    }
    trunc = (mode & O_TRUNC) != 0;
    wr = (mode & (O_WRONLY | O_RDWR)) != 0;
    if ((z->cmeth != ZIP_COMPMETH_STORED) &&
	(z->cmeth != ZIP_COMPMETH_DEFLATED)) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		Tcl_NewStringObj("unsupported compression method", -1));
	}
	goto error;
    }
    if (wr && z->isdir) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp,
		Tcl_NewStringObj("unsupported file type", -1));
	}
	goto error;
    }
    if (!trunc) {
	flags |= TCL_READABLE;
	if (z->isenc && (z->zipfile->pwbuf[0] == 0)) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("decryption failed", -1));
	    }
	    goto error;
	} else if (wr && (z->data == NULL) && (z->nbyte > ZipFS.wrmax)) {
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("file too large", -1));
	    }
	    goto error;
	}
    } else {
	flags = TCL_WRITABLE;
    }
    info = (ZipChannel *) Tcl_AttemptAlloc(sizeof (*info));
    if (info == NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("out of memory", -1));
	}
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
	info->ubuf = (unsigned char *) Tcl_AttemptAlloc(info->nmax);
	if (info->ubuf == NULL) {
merror0:
	    if (info->ubuf != NULL) {
		Tcl_Free((char *) info->ubuf);
	    }
	    Tcl_Free((char *) info);
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("out of memory", -1));
	    }
	    goto error;
	}
	memset(info->ubuf, 0, info->nmax);
	if (trunc) {
	    info->nbyte = 0;
	} else {
	    if (z->data != NULL) {
		unsigned int j = z->nbyte;
		if (j > info->nmax) {
		    j = info->nmax;
		}
		memcpy(info->ubuf, z->data, j);
		info->nbyte = j;
	    } else {
		unsigned char *zbuf = z->zipfile->data + z->offset;

		if (z->isenc) {
		    int len = z->zipfile->pwbuf[0];
		    char pwbuf[260];

		    for (i = 0; i < len; i++) {
			ch = z->zipfile->pwbuf[len - i];
			pwbuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
		    }
		    pwbuf[i] = '\0';
		    init_keys(pwbuf, info->keys, crc32tab);
		    memset(pwbuf, 0, sizeof (pwbuf));
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

		    memset(&stream, 0, sizeof (stream));
		    stream.zalloc = Z_NULL;
		    stream.zfree = Z_NULL;
		    stream.opaque = Z_NULL;
		    stream.avail_in = z->nbytecompr;
		    if (z->isenc) {
			unsigned int j;
			stream.avail_in -= 12;
			cbuf = (unsigned char *)
			    Tcl_AttemptAlloc(stream.avail_in);
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
		    if ((err == Z_STREAM_END) ||
			((err == Z_OK) && (stream.avail_in == 0))) {
			if (cbuf != NULL) {
			    memset(info->keys, 0, sizeof (info->keys));
			    Tcl_Free((char *) cbuf);
			}
			goto wrapchan;
		    }
cerror0:
		    if (cbuf != NULL) {
			memset(info->keys, 0, sizeof (info->keys));
			Tcl_Free((char *) cbuf);
		    }
		    if (info->ubuf != NULL) {
			Tcl_Free((char *) info->ubuf);
		    }
		    Tcl_Free((char *) info);
		    if (interp != NULL) {
			Tcl_SetObjResult(interp,
			    Tcl_NewStringObj("decompression error", -1));
		    }
		    goto error;
		} else if (z->isenc) {
		    for (i = 0; i < z->nbyte - 12; i++) {
			ch = zbuf[i];
			info->ubuf[i] = zdecode(info->keys, crc32tab, ch);
		    }
		} else {
		    memcpy(info->ubuf, zbuf, z->nbyte);
		}
		memset(info->keys, 0, sizeof (info->keys));
		goto wrapchan;
	    }
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
	    int len = z->zipfile->pwbuf[0];
	    char pwbuf[260];

	    for (i = 0; i < len; i++) {
		ch = z->zipfile->pwbuf[len - i];
		pwbuf[i] = (ch & 0x0f) | pwrot[(ch >> 4) & 0x0f];
	    }
	    pwbuf[i] = '\0';
	    init_keys(pwbuf, info->keys, crc32tab);
	    memset(pwbuf, 0, sizeof (pwbuf));
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

	    memset(&stream, 0, sizeof (stream));
	    stream.zalloc = Z_NULL;
	    stream.zfree = Z_NULL;
	    stream.opaque = Z_NULL;
	    stream.avail_in = z->nbytecompr;
	    if (info->isenc) {
		stream.avail_in -= 12;
		ubuf = (unsigned char *) Tcl_AttemptAlloc(stream.avail_in);
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
	    stream.next_out = info->ubuf =
		(unsigned char *) Tcl_AttemptAlloc(info->nbyte);
	    if (info->ubuf == NULL) {
merror:
		if (ubuf != NULL) {
		    info->isenc = 0;
		    memset(info->keys, 0, sizeof (info->keys));
		    Tcl_Free((char *) ubuf);
		}
		Tcl_Free((char *) info);
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
	    if ((err == Z_STREAM_END) ||
		((err == Z_OK) && (stream.avail_in == 0))) {
		if (ubuf != NULL) {
		    info->isenc = 0;
		    memset(info->keys, 0, sizeof (info->keys));
		    Tcl_Free((char *) ubuf);
		}
		goto wrapchan;
	    }
cerror:
	    if (ubuf != NULL) {
		info->isenc = 0;
		memset(info->keys, 0, sizeof (info->keys));
		Tcl_Free((char *) ubuf);
	    }
	    if (info->ubuf != NULL) {
		Tcl_Free((char *) info->ubuf);
	    }
	    Tcl_Free((char *) info);
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("decompression error", -1));
	    }
	    goto error;
	}
    }
wrapchan:
    sprintf(cname, "zipfs_%lx_%d", (unsigned long) z->offset, ZipFS.idCount++);
    z->zipfile->nopen++;
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
 *	This function implements the ZIP filesystem specific version
 *	of the library version of stat.
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
ZipEntryStat(char *path, Tcl_StatBuf *buf)
{
    ZipEntry *z;
    int ret = -1;

    ReadLock();
    z = ZipFSLookup(path);
    if (z == NULL) {
	goto done;
    }
    memset(buf, 0, sizeof (Tcl_StatBuf));
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
done:
    Unlock();
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * ZipEntryAccess --
 *
 *	This function implements the ZIP filesystem specific version
 *	of the library version of access.
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
ZipEntryAccess(char *path, int mode)
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
 * Zip_FSOpenFileChannelProc --
 *
 * Results:
 *
 * Side effects:
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Channel
Zip_FSOpenFileChannelProc(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			  int mode, int permissions)
{
    int len;

    return ZipChannelOpen(interp, Tcl_GetStringFromObj(pathPtr, &len),
			  mode, permissions);
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSStatProc --
 *
 *	This function implements the ZIP filesystem specific version
 *	of the library version of stat.
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
Zip_FSStatProc(Tcl_Obj *pathPtr, Tcl_StatBuf *buf)
{
    int len;

    return ZipEntryStat(Tcl_GetStringFromObj(pathPtr, &len), buf);
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSAccessProc --
 *
 *	This function implements the ZIP filesystem specific version
 *	of the library version of access.
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
Zip_FSAccessProc(Tcl_Obj *pathPtr, int mode)
{
    int len;

    return ZipEntryAccess(Tcl_GetStringFromObj(pathPtr, &len), mode);
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSFilesystemSeparatorProc --
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
Zip_FSFilesystemSeparatorProc(Tcl_Obj *pathPtr)
{
    return Tcl_NewStringObj("/", -1);
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSMatchInDirectoryProc --
 *
 *	This routine is used by the globbing code to search a directory for
 *	all files which match a given pattern.
 *
 * Results:
 *	The return value is a standard Tcl result indicating whether an
 *	error occurred in globbing. Errors are left in interp, good
 *	results are lappend'ed to resultPtr (which must be a valid object).
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static int
Zip_FSMatchInDirectoryProc(Tcl_Interp* interp, Tcl_Obj *result,
			   Tcl_Obj *pathPtr, const char *pattern,
			   Tcl_GlobTypeData *types)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    int scnt, len, l, dirOnly = -1, prefixLen, strip = 0;
    char *pat, *prefix, *path;
#if HAS_DRIVES
    char drivePrefix[3];
#endif
    Tcl_DString ds, dsPref;

#if HAS_DRIVES
    if ((pattern != NULL) && (pattern[0] != '\0') &&
	(strchr(drvletters, pattern[0]) != NULL) && (pattern[1] == ':')) {
	pattern += 2;
    }
#endif
    if (types != NULL) {
	dirOnly = (types->type & TCL_GLOB_TYPE_DIR) == TCL_GLOB_TYPE_DIR;
    }
    Tcl_DStringInit(&ds);
    Tcl_DStringInit(&dsPref);
    prefix = Tcl_GetStringFromObj(pathPtr, &prefixLen);
    Tcl_DStringAppend(&dsPref, prefix, prefixLen);
    prefix = Tcl_DStringValue(&dsPref);
#if HAS_DRIVES
    path = AbsolutePath(prefix, NULL, &ds);
#else
    path = AbsolutePath(prefix, &ds);
#endif
    len = Tcl_DStringLength(&ds);
    if (strcmp(prefix, path) == 0) {
	prefix = NULL;
    } else {
#if HAS_DRIVES
	if ((strchr(drvletters, prefix[0]) != NULL) && (prefix[1] == ':')) {
	    if (strcmp(prefix + 2, path) == 0) {
		strncpy(drivePrefix, prefix, 3);
		drivePrefix[2] = '\0';
		prefix = drivePrefix;
	    }
	} else {
	    strip = len + 1;
	}
#else
	strip = len + 1;
#endif
    }
    if (prefix != NULL) {
#if HAS_DRIVES
	if (prefix == drivePrefix) {
	    Tcl_DStringSetLength(&dsPref, 0);
	    Tcl_DStringAppend(&dsPref, drivePrefix, -1);
	    prefixLen = Tcl_DStringLength(&dsPref);
	} else {
	    Tcl_DStringAppend(&dsPref, "/", 1);
	    prefixLen++;
	}
	prefix = Tcl_DStringValue(&dsPref);
#else
	Tcl_DStringAppend(&dsPref, "/", 1);
	prefixLen++;
	prefix = Tcl_DStringValue(&dsPref);
#endif
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
	    ZipFile *zf = (ZipFile *) Tcl_GetHashValue(hPtr);

	    if (zf->mntptlen == 0) {
		ZipEntry *z = zf->topents;

		while (z != NULL) {
		    int lenz = strlen(z->name);

		    if ((lenz > len + 1) &&
			(strncmp(z->name, path, len) == 0) &&
			(z->name[len] == '/') &&
			(CountSlashes(z->name) == l) &&
			Tcl_StringCaseMatch(z->name + len + 1, pattern, 0)) {
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
		    z = z->tnext;
		}
	    } else if ((zf->mntptlen > len + 1) &&
		       (strncmp(zf->mntpt, path, len) == 0) &&
		       (zf->mntpt[len] == '/') &&
		       (CountSlashes(zf->mntpt) == l) &&
		       Tcl_StringCaseMatch(zf->mntpt + len + 1, pattern, 0)) {
		if (prefix != NULL) {
		    Tcl_DStringAppend(&dsPref, zf->mntpt, zf->mntptlen);
		    Tcl_ListObjAppendElement(NULL, result,
			Tcl_NewStringObj(Tcl_DStringValue(&dsPref),
			    Tcl_DStringLength(&dsPref)));
		    Tcl_DStringSetLength(&dsPref, prefixLen);
		} else {
		    Tcl_ListObjAppendElement(NULL, result,
			Tcl_NewStringObj(zf->mntpt, zf->mntptlen));
		}
	    }
	    hPtr = Tcl_NextHashEntry(&search);
	}
	goto end;
    }
    if ((pattern == NULL) || (pattern[0] == '\0')) {
	hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, path);
	if (hPtr != NULL) {
	    ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	    if ((dirOnly < 0) ||
		(!dirOnly && !z->isdir) ||
		(dirOnly && z->isdir)) {
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
    pat = Tcl_Alloc(len + l + 2);
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
	ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);

	if ((dirOnly >= 0) &&
	    ((dirOnly && !z->isdir) || (!dirOnly && z->isdir))) {
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
    Tcl_Free(pat);
end:
    Unlock();
    Tcl_DStringFree(&dsPref);
    Tcl_DStringFree(&ds);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSNormalizePathProc --
 *
 *	Function to normalize given path object.
 *
 * Results:
 *	Length of final absolute path name.
 *
 * Side effects:
 *	Path object gets converted to an absolute path.
 *
 *-------------------------------------------------------------------------
 */

static int
Zip_FSNormalizePathProc(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			int nextCheckpoint)
{
    char *path;
    Tcl_DString ds;
    int len;

    path = Tcl_GetStringFromObj(pathPtr, &len);
    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    path = AbsolutePath(path, NULL, &ds);
#else
    path = AbsolutePath(path, &ds);
#endif
    nextCheckpoint = Tcl_DStringLength(&ds);
    Tcl_SetStringObj(pathPtr, Tcl_DStringValue(&ds),
		     Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    return nextCheckpoint;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSPathInFilesystemProc --
 *
 *	This function determines if the given path object is in the
 *	ZIP filesystem.
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
Zip_FSPathInFilesystemProc(Tcl_Obj *pathPtr, ClientData *clientDataPtr)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    ZipFile *zf;
    int ret = -1, len;
    char *path;
    Tcl_DString ds;
#if HAS_DRIVES
    int drive = 0;
#endif

    path = Tcl_GetStringFromObj(pathPtr, &len);
    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    path = AbsolutePath(path, &drive, &ds);
#else
    path = AbsolutePath(path, &ds);
#endif
    len = Tcl_DStringLength(&ds);
#if HAS_DRIVES
    if (len && (strchr(drvletters, path[0]) != NULL) && (path[1] == ':')) {
	path += 2;
	len -= 2;
    }
#endif
    ReadLock();
    hPtr = Tcl_FindHashEntry(&ZipFS.fileHash, path);
    if (hPtr != NULL) {
#if HAS_DRIVES
	ZipEntry *z = (ZipEntry *) Tcl_GetHashValue(hPtr);
	if (drive == z->zipfile->mntdrv) {
	    ret = TCL_OK;
	    goto endloop;
	}
#else
	ret = TCL_OK;
	goto endloop;
#endif
    }
    hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
    while (hPtr != NULL) {
	zf = (ZipFile *) Tcl_GetHashValue(hPtr);
#if HAS_DRIVES
	if (drive != zf->mntdrv) {
	    hPtr = Tcl_NextHashEntry(&search);
	    continue;
	}
#endif
	if (zf->mntptlen == 0) {
	    ZipEntry *z = zf->topents;

	    while (z != NULL) {
		int lenz = strlen(z->name);

		if ((len >= lenz) &&
		    (strncmp(path, z->name, lenz) == 0)) {
		    ret = TCL_OK;
		    goto endloop;
		}
		z = z->tnext;
	    }
	} else if ((len >= zf->mntptlen) &&
		   (strncmp(path, zf->mntpt, zf->mntptlen) == 0)) {
	    ret = TCL_OK;
	    goto endloop;
	}
	hPtr = Tcl_NextHashEntry(&search);
    }
endloop:
    Unlock();
    Tcl_DStringFree(&ds);
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSListVolumesProc --
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
Zip_FSListVolumesProc(void)
{
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    ZipFile *zf;
    Tcl_Obj *vols = Tcl_NewObj(), *vol;

    ReadLock();
    hPtr = Tcl_FirstHashEntry(&ZipFS.zipHash, &search);
    while (hPtr != NULL) {
	zf = (ZipFile *) Tcl_GetHashValue(hPtr);
#if HAS_DRIVES
	vol = Tcl_ObjPrintf("%c:%s", zf->mntdrv, zf->mntpt);
#else
	vol = Tcl_NewStringObj(zf->mntpt, zf->mntptlen);
#endif
	Tcl_ListObjAppendList(NULL, vols, vol);
	Tcl_DecrRefCount(vol);
	hPtr = Tcl_NextHashEntry(&search);
    }
    Unlock();
    return vols;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSChdirProc --
 *
 *	If the path object refers to a directory within the ZIP
 *	filesystem the current directory is set to this directory.
 *
 * Results:
 *	TCL_OK on success, -1 on error with error number set.
 *
 * Side effects:
 *	The global cwdPathPtr may change value.
 *
 *-------------------------------------------------------------------------
 */

static int
Zip_FSChdirProc(Tcl_Obj *pathPtr)
{
    int len;
    char *path;
    Tcl_DString ds;
    ZipEntry *z;
    int ret = TCL_OK;
#if HAS_DRIVES
    int drive = 0;
#endif

    path = Tcl_GetStringFromObj(pathPtr, &len);
    Tcl_DStringInit(&ds);
#if HAS_DRIVES
    path = AbsolutePath(path, &drive, &ds);
#else
    path = AbsolutePath(path, &ds);
#endif
    ReadLock();
    z = ZipFSLookup(path);
    if ((z == NULL) || !z->isdir) {
	Tcl_SetErrno(ENOENT);
	ret = -1;
    }
#if HAS_DRIVES
    if ((z != NULL) && (drive != z->zipfile->mntdrv)) {
	Tcl_SetErrno(ENOENT);
	ret = -1;
    }
#endif
    Unlock();
    Tcl_DStringFree(&ds);
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSFileAttrStringsProc --
 *
 *	This function implements the ZIP filesystem dependent 'file attributes'
 *	subcommand, for listing the set of possible attribute strings.
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
Zip_FSFileAttrStringsProc(Tcl_Obj *pathPtr, Tcl_Obj** objPtrRef)
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
 * Zip_FSFileAttrsGetProc --
 *
 *	This function implements the ZIP filesystem specific
 *	'file attributes' subcommand, for 'get' operations.
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
Zip_FSFileAttrsGetProc(Tcl_Interp *interp, int index, Tcl_Obj *pathPtr,
		       Tcl_Obj **objPtrRef)
{
    int len, ret = TCL_OK;
    char *path;
    ZipEntry *z;

    path = Tcl_GetStringFromObj(pathPtr, &len);
    ReadLock();
    z = ZipFSLookup(path);
    if (z == NULL) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("file not found", -1));
	}
	ret = TCL_ERROR;
	goto done;
    }
    switch (index) {
    case 0:
	*objPtrRef = Tcl_NewIntObj(z->nbyte);
	goto done;
    case 1:
	*objPtrRef= Tcl_NewIntObj(z->nbytecompr);
	goto done;
    case 2:
	*objPtrRef= Tcl_NewLongObj(z->offset);
	goto done;
    case 3:
	*objPtrRef= Tcl_NewStringObj(z->zipfile->mntpt, -1);
	goto done;
    case 4:
	*objPtrRef= Tcl_NewStringObj(z->zipfile->name, -1);
	goto done;
    case 5:
	*objPtrRef= Tcl_NewStringObj("0555", -1);
	goto done;
    }
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("unknown attribute", -1));
    }
    ret = TCL_ERROR;
done:
    Unlock();
    return ret;
}

/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSFileAttrsSetProc --
 *
 *	This function implements the ZIP filesystem specific
 *	'file attributes' subcommand, for 'set' operations.
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
Zip_FSFileAttrsSetProc(Tcl_Interp *interp, int index, Tcl_Obj *pathPtr,
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
 * Zip_FSFilesystemPathTypeProc --
 *
 * Results:
 *
 * Side effects:
 *
 *-------------------------------------------------------------------------
 */

static Tcl_Obj *
Zip_FSFilesystemPathTypeProc(Tcl_Obj *pathPtr)
{
    return Tcl_NewStringObj("zip", -1);
}


/*
 *-------------------------------------------------------------------------
 *
 * Zip_FSLoadFile --
 *
 *	This functions deals with loading native object code. If
 *	the given path object refers to a file within the ZIP
 *	filesystem, an approriate error code is returned to delegate
 *	loading to the caller (by copying the file to temp store
 *	and loading from there). As fallback when the file refers
 *	to the ZIP file system but is not present, it is looked up
 *	relative to the executable and loaded from there when available.
 *
 * Results:
 *	TCL_OK on success, -1 otherwise with error number set.
 *
 * Side effects:
 *	Loads native code into the process address space.
 *
 *-------------------------------------------------------------------------
 */

static int
Zip_FSLoadFile(Tcl_Interp *interp, Tcl_Obj *path, Tcl_LoadHandle *loadHandle,
	       Tcl_FSUnloadFileProc **unloadProcPtr, int flags)
{
#ifdef ANDROID
	/*
	 * Force loadFileProc to native implementation since the
	 * package manger already extracted the shared libraries
	 * from the APK at install time.
	 */
    return tclNativeFilesystem.loadFileProc(interp, path, loadHandle, unloadProcPtr, flags);
#else
    Tcl_FSLoadFileProc2 *loadFileProc;
    Tcl_Obj *altPath = NULL;
    int ret = -1;

    if (Tcl_FSAccess(path, R_OK) == 0) {
	/*
	 * EXDEV should trigger loading by copying to temp store.
	 */
	Tcl_SetErrno(EXDEV);
	return ret;
    } else {
	Tcl_Obj *objs[2] = { NULL, NULL };

	objs[1] = TclPathPart(interp, path, TCL_PATH_DIRNAME);
	if ((objs[1] != NULL) && (Zip_FSAccessProc(objs[1], R_OK) == 0)) {
	    /*
	     * Shared object is not in ZIP but its path prefix is,
	     * thus try to load from directory where the executable
	     * came from.
	     */
	    TclDecrRefCount(objs[1]);
	    objs[1] = TclPathPart(interp, path, TCL_PATH_TAIL);
	    objs[0] = TclPathPart(interp, TclGetObjNameOfExecutable(),
				  TCL_PATH_DIRNAME);
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
    }
    loadFileProc = (Tcl_FSLoadFileProc2 *) tclNativeFilesystem.loadFileProc;
    if (loadFileProc != NULL) {
	ret = loadFileProc(interp, path, loadHandle, unloadProcPtr, flags);
    } else {
	Tcl_SetErrno(ENOENT);
    }
    if (altPath != NULL) {
	Tcl_DecrRefCount(altPath);
    }
    return ret;
#endif
}


/*
 * Define the ZIP filesystem dispatch table.
 */

MODULE_SCOPE const Tcl_Filesystem zipfsFilesystem;

const Tcl_Filesystem zipfsFilesystem = {
    "zipfs",
    sizeof (Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_2,
    Zip_FSPathInFilesystemProc,
    NULL, /* dupInternalRepProc */
    NULL, /* freeInternalRepProc */
    NULL, /* internalToNormalizedProc */
    NULL, /* createInternalRepProc */
    Zip_FSNormalizePathProc,
    Zip_FSFilesystemPathTypeProc,
    Zip_FSFilesystemSeparatorProc,
    Zip_FSStatProc,
    Zip_FSAccessProc,
    Zip_FSOpenFileChannelProc,
    Zip_FSMatchInDirectoryProc,
    NULL, /* utimeProc */
    NULL, /* linkProc */
    Zip_FSListVolumesProc,
    Zip_FSFileAttrStringsProc,
    Zip_FSFileAttrsGetProc,
    Zip_FSFileAttrsSetProc,
    NULL, /* createDirectoryProc */
    NULL, /* removeDirectoryProc */
    NULL, /* deleteFileProc */
    NULL, /* copyFileProc */
    NULL, /* renameFileProc */
    NULL, /* copyDirectoryProc */
    NULL, /* lstatProc */
    (Tcl_FSLoadFileProc *) Zip_FSLoadFile,
    NULL, /* getCwdProc */
    Zip_FSChdirProc,
};

#endif /* HAVE_ZLIB */


/*
 *-------------------------------------------------------------------------
 *
 * Zipfs_doInit --
 *
 *	Perform per interpreter initialization of this module.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	Initializes this module if not already initialized, and adds
 *	module related commands to the given interpreter.
 *
 *-------------------------------------------------------------------------
 */

static int
Zipfs_doInit(Tcl_Interp *interp, int safe)
{
#ifdef HAVE_ZLIB
    static const char findproc[] =
	"proc ::zipfs::find dir {\n"
	" set result {}\n"
	" if {[catch {glob -directory $dir -tails -nocomplain * .*} list]} {\n"
	"  return $result\n"
	" }\n"
	" foreach file $list {\n"
	"  if {$file eq \".\" || $file eq \"..\"} {\n"
	"   continue\n"
	"  }\n"
	"  set file [file join $dir $file]\n"
	"  lappend result $file\n"
	"  foreach file [::zipfs::find $file] {\n"
	"   lappend result $file\n"
	"  }\n"
	" }\n"
	" return [lsort $result]\n"
	"}\n";

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.0", 0) == NULL) {
	return TCL_ERROR;
    }
#endif
    /* one-time initialization */
    WriteLock();
    if (!ZipFS.initialized) {
#ifdef TCL_THREADS
	static const Tcl_Time t = { 0, 0 };

	/*
	 * Inflate condition variable.
	 */
	Tcl_MutexLock(&ZipFSMutex);
	Tcl_ConditionWait(&ZipFSCond, &ZipFSMutex, &t);
	Tcl_MutexUnlock(&ZipFSMutex);
#endif
	Tcl_FSRegister(NULL, &zipfsFilesystem);
	Tcl_InitHashTable(&ZipFS.fileHash, TCL_STRING_KEYS);
	Tcl_InitHashTable(&ZipFS.zipHash, TCL_STRING_KEYS);
	ZipFS.initialized = ZipFS.idCount = 1;
#if defined(ZIPFS_IN_TCL) || defined(ZIPFS_IN_TK)
	Tcl_StaticPackage(interp, "zipfs", Zipfs_Init, Zipfs_SafeInit);
#endif
    }
    Unlock();
#if !defined(ZIPFS_IN_TCL) && !defined(ZIPFS_IN_TK)
    Tcl_PkgProvide(interp, "zipfs", "1.0");
#endif
    if (!safe) {
	Tcl_CreateCommand(interp, "::zipfs::mount", ZipFSMountCmd, 0, 0);
	Tcl_CreateCommand(interp, "::zipfs::unmount", ZipFSUnmountCmd, 0, 0);
	Tcl_CreateCommand(interp, "::zipfs::mkkey", ZipFSMkKeyCmd, 0, 0);
	Tcl_CreateCommand(interp, "::zipfs::mkimg", ZipFSMkImgCmd, 0, 0);
	Tcl_CreateCommand(interp, "::zipfs::mkzip", ZipFSMkZipCmd, 0, 0);
	Tcl_GlobalEval(interp, findproc);
    }
    Tcl_CreateObjCommand(interp, "::zipfs::exists", ZipFSExistsObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::zipfs::info", ZipFSInfoObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::zipfs::list", ZipFSListObjCmd, 0, 0);
    if (!safe) {
	Tcl_LinkVar(interp, "::zipfs::wrmax", (char *) &ZipFS.wrmax,
		    TCL_LINK_INT);
    }
    return TCL_OK;
#else
    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("no zlib available", -1));
    }
    return TCL_ERROR;
#endif
}

/*
 *-------------------------------------------------------------------------
 *
 * Zipfs_Init, Zipfs_SafeInit --
 *
 *	These functions are invoked to perform per interpreter initialization
 *	of this module.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side effects:
 *	Initializes this module if not already initialized, and adds
 *	module related commands to the given interpreter.
 *
 *-------------------------------------------------------------------------
 */

int
Zipfs_Init(Tcl_Interp *interp)
{
    return Zipfs_doInit(interp, 0);
}

int
Zipfs_SafeInit(Tcl_Interp *interp)
{
    return Zipfs_doInit(interp, 1);
}

#ifndef HAVE_ZLIB

/*
 *-------------------------------------------------------------------------
 *
 * Zipfs_Mount, Zipfs_Unmount --
 *
 *	Dummy version when no ZLIB support available.
 *
 *-------------------------------------------------------------------------
 */

int
Zipfs_Mount(Tcl_Interp *interp, const char *zipname, const char *mntpt,
	    const char *passwd)
{
    return Zipfs_doInit(interp, 1);
}

int
Zipfs_Unmount(Tcl_Interp *interp, const char *zipname)
{
    return Zipfs_doInit(interp, 1);
}

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
