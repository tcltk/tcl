/*
 * Copyright (c) 2000 D. Richard Hipp
 * Copyright (c) 2007 PDQ Interfaces Inc.
 * Copyright (c) 2013-2014 Sean Woods
 *
 * This file is now released under the BSD style license outlined in the
 * included file license.terms.
 *
 ************************************************************************
 * A ZIP archive virtual filesystem for Tcl.
 *
 * This package of routines enables Tcl to use a Zip file as a virtual file
 * system.  Each of the content files of the Zip archive appears as a real
 * file to Tcl.
 *
 * Well, almost...  Actually, the virtual file system is limited in a number
 * of ways.  The only things you can do are "stat" and "read" file content
 * files.  You cannot use "cd".  But it turns out that "stat" and "read" are
 * sufficient for most purposes.
 *
 * This version has been modified to run under Tcl 8.6
 */
#include "tcl.h"
#include <stddef.h>
#include <ctype.h>
#include <zlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/*
 * Size of the decompression input buffer
 */
#define COMPR_BUF_SIZE		8192
/*TODO: use thread-local as appropriate*/
static int openarch = 0;	/* Set to 1 when opening archive. */

/*
 * All static variables are collected into a structure named "local".  That
 * way, it is clear in the code when we are using a static variable because
 * its name begins with "local.".
 */
static struct {
    Tcl_HashTable fileHash;	/* One entry for each file in the ZVFS. The
				 * key is the virtual filename. The data is an
				 * instance of the ZvfsFile structure. */
    Tcl_HashTable archiveHash;	/* One entry for each archive.  Key is the
				 * name. The data is the ZvfsArchive
				 * structure. */
    int isInit;			/* True after initialization */
} local;

/*
 * Each ZIP archive file that is mounted is recorded as an instance of this
 * structure
 */
typedef struct ZvfsArchive {
    char *zName;		/* Name of the archive */
    char *zMountPoint;		/* Where this archive is mounted */
    struct ZvfsFile *pFiles;	/* List of files in that archive */
} ZvfsArchive;

/*
 * Particulars about each virtual file are recorded in an instance of the
 * following structure.
 */
typedef struct ZvfsFile {
    char *zName;		/* The full pathname of the virtual file */
    ZvfsArchive *pArchive;	/* The ZIP archive holding this file data */
    int iOffset;		/* Offset into the ZIP archive of the data */
    int nByte;			/* Uncompressed size of the virtual file */
    int nByteCompr;		/* Compressed size of the virtual file */
    time_t timestamp;		/* Modification time */
    int isdir;			/* Set to 2 if directory, or 1 if mount */
    int depth;			/* Number of slashes in path. */
    int permissions;		/* File permissions. */
    struct ZvfsFile *pNext;	/* Next file in the same archive */
    struct ZvfsFile *pNextName;	/* A doubly-linked list of files with the
				 * _same_ name. Only the first is in
				 * local.fileHash */
    struct ZvfsFile *pPrevName;
} ZvfsFile;

/*
 * Information about each file within a ZIP archive is stored in an instance
 * of the following structure.  A list of these structures forms a table of
 * contents for the archive.
 */
typedef struct ZFile ZFile;
struct ZFile {
    char *zName;		/* Name of the file */
    int isSpecial;		/* Not really a file in the ZIP archive */
    int dosTime;		/* Modification time (DOS format) */
    int dosDate;		/* Modification date (DOS format) */
    int iOffset;		/* Offset into the ZIP archive of the data */
    int nByte;			/* Uncompressed size of the virtual file */
    int nByteCompr;		/* Compressed size of the virtual file */
    int nExtra;			/* Extra space in the TOC header */
    int iCRC;			/* Cyclic Redundancy Check of the data */
    int permissions;		/* File permissions. */
    int flags;			/* Deletion = bit 0. */
    ZFile *pNext;		/* Next file in the same archive */
};

EXTERN int Tcl_Zvfs_Mount(Tcl_Interp *interp,const char *zArchive,const char *zMountPoint);
EXTERN int Tcl_Zvfs_Umount(const char *zArchive);
EXTERN int TclZvfsInit(Tcl_Interp *interp);
EXTERN int Tcl_Zvfs_SafeInit(Tcl_Interp *interp);

/*
 * Macros to read 16-bit and 32-bit big-endian integers into the native format
 * of this local processor.  B is an array of characters and the integer
 * begins at the N-th character of the array.
 */
#define INT16(B, N) (B[N] + (B[N+1]<<8))
#define INT32(B, N) (INT16(B,N) + (B[N+2]<<16) + (B[N+3]<<24))

/*
 * Write a 16- or 32-bit integer as little-endian into the given buffer.
 */
static void
put16(
    char *z,
    int v)
{
    z[0] = v & 0xff;
    z[1] = (v>>8) & 0xff;
}
static void
put32(
    char *z,
    int v)
{
    z[0] = v & 0xff;
    z[1] = (v>>8) & 0xff;
    z[2] = (v>>16) & 0xff;
    z[3] = (v>>24) & 0xff;
}

/*
 * Make a new ZFile structure with space to hold a name of the number of
 * characters given.  Return a pointer to the new structure.
 */
static ZFile *
newZFile(
    int nName,
    ZFile **ppList)
{
    ZFile *pNew = (void *) Tcl_Alloc(sizeof(*pNew) + nName + 1);

    memset(pNew, 0, sizeof(*pNew));
    pNew->zName = (char*)&pNew[1];
    pNew->pNext = *ppList;
    *ppList = pNew;
    return pNew;
}

/*
 * Delete an entire list of ZFile structures
 */
static void
deleteZFileList(
    ZFile *pList)
{
    ZFile *pNext;

    while( pList ){
	pNext = pList->pNext;
	Tcl_Free((char*)pList);
	pList = pNext;
    }
}

/* Convert DOS time to unix time. */
static void
UnixTimeDate(
    struct tm *tm,
    int *dosDate,
    int *dosTime)
{
    *dosDate = ((((tm->tm_year-80)<<9)&0xfe00) | (((tm->tm_mon+1)<<5)&0x1e0)
	    | (tm->tm_mday&0x1f));
    *dosTime = (((tm->tm_hour<<11)&0xf800) | ((tm->tm_min<<5)&0x7e0)
	    | (tm->tm_sec&0x1f));
}

/* Convert DOS time to unix time. */
static time_t
DosTimeDate(
    int dosDate,
    int dosTime)
{
    time_t now;
    struct tm *tm;

    now = time(NULL);
    tm = localtime(&now);
    tm->tm_year = (((dosDate&0xfe00)>>9) + 80);
    tm->tm_mon = ((dosDate&0x1e0)>>5);
    tm->tm_mday = (dosDate & 0x1f);
    tm->tm_hour = (dosTime&0xf800)>>11;
    tm->tm_min = (dosTime&0x7e0)>>5;
    tm->tm_sec = (dosTime&0x1f);
    return mktime(tm);
}

/*
 * Translate a DOS time and date stamp into a human-readable string.
 */
static void
translateDosTimeDate(
    char *zStr,
    int dosDate,
    int dosTime){
    static char *zMonth[] = { "nil",
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
 
    sprintf(zStr, "%02d-%s-%d %02d:%02d:%02d", 
	    dosDate & 0x1f,
	    zMonth[ ((dosDate&0x1e0)>>5) ],
	    ((dosDate&0xfe00)>>9) + 1980,
	    (dosTime&0xf800)>>11,
	    (dosTime&0x7e)>>5,
	    dosTime&0x1f);
}

/* Return count of char ch in str */
int
strchrcnt(
    char *str,
    char ch)
{
    int cnt = 0;
    char *cp = str;

    while ((cp = strchr(cp,ch)) != NULL) {
	cp++;
	cnt++;
    }
    return cnt;
}

/*
 * Concatenate zTail onto zRoot to form a pathname.  zRoot will begin with
 * "/".  After concatenation, simplify the pathname be removing unnecessary
 * ".." and "." directories.  Under windows, make all characters lower case.
 *
 * Resulting pathname is returned.  Space to hold the returned path is
 * obtained form Tcl_Alloc() and should be freed by the calling function.
 */
static char *
CanonicalPath(
    const char *zRoot,
    const char *zTail)
{
    char *zPath;
    int i, j, c;

#ifdef __WIN32__
    if (isalpha(zTail[0]) && zTail[1] == ':') {
	zTail += 2;
    }
    if (zTail[0] == '\\') {
	zRoot = "";
	zTail++;
    }
#endif
    if (zTail[0] == '/') {
	zRoot = "";
	zTail++;
    }
    zPath = Tcl_Alloc(strlen(zRoot) + strlen(zTail) + 2);
    if (zTail[0]) {
	sprintf(zPath, "%s/%s", zRoot, zTail);
    } else {
	strcpy(zPath, zRoot);
    }
    for (i=j=0 ; (c = zPath[i]) != 0 ; i++) {
#ifdef __WIN32__
	if (isupper(c)) {
	    c = tolower(c);
	} else if (c == '\\') {
	    c = '/';
	}
#endif
	if (c == '/') {
	    int c2 = zPath[i+1];

	    if (c2 == '/') {
		continue;
	    }
	    if (c2 == '.') {
		int c3 = zPath[i+2];

		if (c3 == '/' || c3 == 0) {
		    i++;
		    continue;
		}
		if (c3 == '.' && (zPath[i+3] == '.' || zPath[i+3] == 0)) {
		    i += 2;
		    while (j > 0 && zPath[j-1] != '/') {
			j--;
		    }
		    continue;
		}
	    }
	}
	zPath[j++] = c;
    }
    if (j == 0) {
	zPath[j++] = '/';
    }
    /* if (j>1 && zPath[j-1] == '/') j--; */
    zPath[j] = 0;
    return zPath;
}

/*
 * Construct an absolute pathname where memory is obtained from Tcl_Alloc that
 * means the same file as the pathname given.
 */
static char *
AbsolutePath(
    const char *zRelative)
{
    Tcl_DString pwd;
    char *zResult;
    int len;

    Tcl_DStringInit(&pwd);
    if (zRelative[0] == '~' && zRelative[1] == '/') {
	/* TODO: do this for all paths??? */
	if (Tcl_TranslateFileName(0, zRelative, &pwd) != NULL) {
	    zResult = CanonicalPath("", Tcl_DStringValue(&pwd));
	    goto done;
	}
    } else if (zRelative[0] != '/') {
#ifdef __WIN32__
	if (!(zRelative[0]=='\\' || (zRelative[0] && zRelative[1] == ':'))) {
	    /*Tcl_GetCwd(0, &pwd); */
	}
#else
	Tcl_GetCwd(0, &pwd);
#endif
    }
    zResult = CanonicalPath(Tcl_DStringValue(&pwd), zRelative);
  done:
    Tcl_DStringFree(&pwd);
    len = strlen(zResult);
    if (len > 0 && zResult[len-1] == '/') {
	zResult[len-1] = 0;
    }
    return zResult;
}

int
ZvfsReadTOCStart(
    Tcl_Interp *interp,		/* Leave error messages in this interpreter */
    Tcl_Channel chan,
    ZFile **pList,
    int *iStart)
{
    int nFile;			/* Number of files in the archive */
    int iPos;			/* Current position in the archive file */
    unsigned char zBuf[100];	/* Space into which to read from the ZIP
				 * archive */
    ZFile *p;
    int zipStart;

    if (!chan) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation",
	    "binary") != TCL_OK){
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary") != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Read the "End Of Central Directory" record from the end of the ZIP
     * archive.
     */

    iPos = Tcl_Seek(chan, -22, SEEK_END);
    Tcl_Read(chan, (char *) zBuf, 22);
    if (memcmp(zBuf, "\120\113\05\06", 4)) {
	/* Tcl_AppendResult(interp, "not a ZIP archive", NULL); */
	return TCL_BREAK;
    }

    /*
     * Compute the starting location of the directory for the ZIP archive in
     * iPos then seek to that location.
     */

    zipStart = iPos;
    nFile = INT16(zBuf,8);
    iPos -= INT32(zBuf,12);
    Tcl_Seek(chan, iPos, SEEK_SET);

    while (1) {
	int lenName;		/* Length of the next filename */
	int lenExtra=0;		/* Length of "extra" data for next file */
	int iData;		/* Offset to start of file data */

	if (nFile-- <= 0) {
	    break;
	}

	/*
	 * Read the next directory entry.  Extract the size of the filename,
	 * the size of the "extra" information, and the offset into the
	 * archive file of the file data.
	 */

	Tcl_Read(chan, (char *) zBuf, 46);
	if (memcmp(zBuf, "\120\113\01\02", 4)) {
	    Tcl_AppendResult(interp, "ill-formed central directory entry",
		    NULL);
	    return TCL_ERROR;
	}
	lenName = INT16(zBuf,28);
	lenExtra = INT16(zBuf,30) + INT16(zBuf,32);
	iData = INT32(zBuf,42);
	if (iData < zipStart) {
	    zipStart = iData;
	}

	p = newZFile(lenName, pList);
	if (!p) {
	    break;
	}

	Tcl_Read(chan, p->zName, lenName);
	p->zName[lenName] = 0;
	if (lenName > 0 && p->zName[lenName-1] == '/') {
	    p->isSpecial = 1;
	}
	p->dosDate = INT16(zBuf, 14);
	p->dosTime = INT16(zBuf, 12);
	p->nByteCompr = INT32(zBuf, 20);
	p->nByte = INT32(zBuf, 24);
	p->nExtra = INT32(zBuf, 28);
	p->iCRC = INT32(zBuf, 32);

	if (nFile < 0) {
	    break;
	}

	/*
	 * Skip over the extra information so that the next read will be from
	 * the beginning of the next directory entry.
	 */

	Tcl_Seek(chan, lenExtra, SEEK_CUR);
    }
    *iStart = zipStart;
    return TCL_OK;
}

int
ZvfsReadTOC(
    Tcl_Interp *interp,		/* Leave error messages in this interpreter */
    Tcl_Channel chan,
    ZFile **pList)
{
    int iStart;

    return ZvfsReadTOCStart(interp, chan, pList, &iStart);
}

/*
 * Read a ZIP archive and make entries in the virutal file hash table for all
 * content files of that ZIP archive.  Also initialize the ZVFS if this
 * routine has not been previously called.
 */
int
Tcl_Zvfs_Mount(
    Tcl_Interp *interp,		/* Leave error messages in this interpreter */
    const char *zArchive,	/* The ZIP archive file */
    const char *zMountPoint)	/* Mount contents at this directory */
{
    Tcl_Channel chan;		/* Used for reading the ZIP archive file */
    char *zArchiveName = 0;	/* A copy of zArchive */
    char *zTrueName = 0;	/* A copy of zMountPoint */
    int nFile;			/* Number of files in the archive */
    int iPos;			/* Current position in the archive file */
    ZvfsArchive *pArchive;	/* The ZIP archive being mounted */
    Tcl_HashEntry *pEntry;	/* Hash table entry */
    int isNew;			/* Flag to tell use when a hash entry is
				 * new */
    unsigned char zBuf[100];	/* Space into which to read from the ZIP
				 * archive */
    Tcl_HashSearch zSearch;	/* Search all mount points */
    unsigned int startZip;

    if (!local.isInit) {
	return TCL_ERROR;
    }

    /*
     * If null archive name, return all current mounts.
     */

    if (!zArchive) {
	Tcl_DString dStr;

	Tcl_DStringInit(&dStr);
	pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
	while (pEntry) {
	    pArchive = Tcl_GetHashValue(pEntry);
	    if (pArchive) {
		Tcl_DStringAppendElement(&dStr, pArchive->zName);
		Tcl_DStringAppendElement(&dStr, pArchive->zMountPoint);
	    }
	    pEntry = Tcl_NextHashEntry(&zSearch);
	}
	Tcl_DStringResult(interp, &dStr);
	return TCL_OK;
    }

    /*
     * If null mount, return mount point.
     */

    /*TODO: cleanup allocations of Absolute() path.*/
    if (!zMountPoint) {
	zTrueName = AbsolutePath(zArchive);
	pEntry = Tcl_FindHashEntry(&local.archiveHash, zTrueName);
	if (pEntry) {
	    pArchive = Tcl_GetHashValue(pEntry);
	    if (pArchive && interp) {
		Tcl_AppendResult(interp, pArchive->zMountPoint, 0);
	    }
	}
	Tcl_Free(zTrueName);
	return TCL_OK;
    }
    chan = Tcl_OpenFileChannel(interp, zArchive, "r", 0);
    if (!chan) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK){
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary") != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Read the "End Of Central Directory" record from the end of the ZIP
     * archive.
     */

    iPos = Tcl_Seek(chan, -22, SEEK_END);
    Tcl_Read(chan, (char *) zBuf, 22);
    if (memcmp(zBuf, "\120\113\05\06", 4)) {
	if(interp) Tcl_AppendResult(interp, "not a ZIP archive", NULL);
	return TCL_ERROR;
    }

    /*
     * Construct the archive record.
     */

    zArchiveName = AbsolutePath(zArchive);
    pEntry = Tcl_CreateHashEntry(&local.archiveHash, zArchiveName, &isNew);
    if (!isNew) {
	pArchive = Tcl_GetHashValue(pEntry);
	if (interp) {
          Tcl_AppendResult(interp, "already mounted at ", pArchive->zMountPoint,0);
	}
	Tcl_Free(zArchiveName);
	Tcl_Close(interp, chan);
	return TCL_ERROR;
    }

    /*
     * Empty string is the special case of mounting on itself.
     */

    if (!*zMountPoint) {
	zMountPoint = zTrueName = AbsolutePath(zArchive);
    }

    pArchive = (void *) Tcl_Alloc(sizeof(*pArchive) + strlen(zMountPoint)+1);
    pArchive->zName = zArchiveName;
    pArchive->zMountPoint = (char *) &pArchive[1];
    strcpy(pArchive->zMountPoint, zMountPoint);
    pArchive->pFiles = 0;
    Tcl_SetHashValue(pEntry, pArchive);

    /*
     * Compute the starting location of the directory for the ZIP archive in
     * iPos then seek to that location.
     */

    nFile = INT16(zBuf, 8);
    iPos -= INT32(zBuf, 12);
    Tcl_Seek(chan, iPos, SEEK_SET);
    startZip = iPos;

    while (1) {
	int lenName;		/* Length of the next filename */
	int lenExtra=0;		/* Length of "extra" data for next file */
	int iData;		/* Offset to start of file data */
	int dosTime;
	int dosDate;
	int isdir;
	ZvfsFile *pZvfs;	/* A new virtual file */
	char *zFullPath;	/* Full pathname of the virtual file */
	char zName[1024];	/* Space to hold the filename */

	if (nFile-- <= 0) {
	    isdir = 1;
	    zFullPath = CanonicalPath(zMountPoint, "");
	    iData = startZip;
	    goto addentry;
	}

	/*
	 * Read the next directory entry.  Extract the size of the filename,
	 * the size of the "extra" information, and the offset into the
	 * archive file of the file data.
	 */

	Tcl_Read(chan, (char *) zBuf, 46);
	if (memcmp(zBuf, "\120\113\01\02", 4)) {
          if(interp) {
	    Tcl_AppendResult(interp, "ill-formed central directory entry",NULL);
          }
          if (zTrueName) {
              Tcl_Free(zTrueName);
          }
          return TCL_ERROR;
	}
	lenName = INT16(zBuf, 28);
	lenExtra = INT16(zBuf, 30) + INT16(zBuf, 32);
	iData = INT32(zBuf, 42);

	/*
	 * If the virtual filename is too big to fit in zName[], then skip
	 * this file
	 */

	if (lenName >= sizeof(zName)) {
	    Tcl_Seek(chan, lenName + lenExtra, SEEK_CUR);
	    continue;
	}

	/*
	 * Construct an entry in local.fileHash for this virtual file.
	 */

	Tcl_Read(chan, zName, lenName);
	isdir = 0;
	if (lenName > 0 && zName[lenName-1] == '/') {
	    lenName--;
	    isdir = 2;
	}
	zName[lenName] = 0;
	zFullPath = CanonicalPath(zMountPoint, zName);
    addentry:
	pZvfs = (void *) Tcl_Alloc(sizeof(*pZvfs));
	pZvfs->zName = zFullPath;
	pZvfs->pArchive = pArchive;
	pZvfs->isdir = isdir;
	pZvfs->depth = strchrcnt(zFullPath, '/');
	pZvfs->iOffset = iData;
	if (iData < startZip) {
	    startZip = iData;
	}
	dosDate = INT16(zBuf, 14);
	dosTime = INT16(zBuf, 12);
	pZvfs->timestamp = DosTimeDate(dosDate, dosTime);
	pZvfs->nByte = INT32(zBuf, 24);
	pZvfs->nByteCompr = INT32(zBuf, 20);
	pZvfs->pNext = pArchive->pFiles;
	pZvfs->permissions = 0xffff & (INT32(zBuf, 38) >> 16);
	pArchive->pFiles = pZvfs;
	pEntry = Tcl_CreateHashEntry(&local.fileHash, zFullPath, &isNew);
	if (isNew) {
	    pZvfs->pNextName = 0;
	} else {
	    ZvfsFile *pOld = Tcl_GetHashValue(pEntry);

	    pOld->pPrevName = pZvfs;
	    pZvfs->pNextName = pOld;
	}
	pZvfs->pPrevName = 0;
	Tcl_SetHashValue(pEntry, pZvfs);

	if (nFile < 0) {
	    break;
	}

	/*
	 * Skip over the extra information so that the next read will be from
	 * the beginning of the next directory entry.
	 */

	Tcl_Seek(chan, lenExtra, SEEK_CUR);
    }
    Tcl_Close(interp, chan);

    if (zTrueName) {
	Tcl_Free(zTrueName);
    }
    return TCL_OK;
}

/*
 * Locate the ZvfsFile structure that corresponds to the file named.  Return
 * NULL if there is no such ZvfsFile.
 */
static ZvfsFile *
ZvfsLookup(
    char *zFilename)
{
    char *zTrueName;
    Tcl_HashEntry *pEntry;
    ZvfsFile *pFile;

    if (local.isInit == 0) {
	return 0;
    }
    zTrueName = AbsolutePath(zFilename);
    pEntry = Tcl_FindHashEntry(&local.fileHash, zTrueName);
    pFile = pEntry ? Tcl_GetHashValue(pEntry) : 0;
    Tcl_Free(zTrueName);
    return pFile;
}

/*
 * Unmount all the files in the given ZIP archive.
 */
int
Tcl_Zvfs_Umount(
    const char *zArchive)
{
    char *zArchiveName;
    ZvfsArchive *pArchive;
    ZvfsFile *pFile, *pNextFile;
    Tcl_HashEntry *pEntry;

    zArchiveName = AbsolutePath(zArchive);
    pEntry = Tcl_FindHashEntry(&local.archiveHash, zArchiveName);
    Tcl_Free(zArchiveName);
    if (pEntry == 0) {
	return 0;
    }
    pArchive = Tcl_GetHashValue(pEntry);
    Tcl_DeleteHashEntry(pEntry);
    Tcl_Free(pArchive->zName);
    for(pFile=pArchive->pFiles ; pFile; pFile=pNextFile) {
	pNextFile = pFile->pNext;
	if (pFile->pNextName) {
	    pFile->pNextName->pPrevName = pFile->pPrevName;
	}
	if (pFile->pPrevName) {
	    pFile->pPrevName->pNextName = pFile->pNextName;
	} else {
	    pEntry = Tcl_FindHashEntry(&local.fileHash, pFile->zName);
	    if (pEntry == 0) {
		Tcl_Panic("This should never happen");
	    } else if (pFile->pNextName) {
		Tcl_SetHashValue(pEntry, pFile->pNextName);
	    } else {
		Tcl_DeleteHashEntry(pEntry);
	    }
	}
	Tcl_Free(pFile->zName);
	Tcl_Free((void *) pFile);
    }
    return 1;
}

/*
 * zvfs::mount  Zip-archive-name  mount-point
 *
 * Create a new mount point on the given ZIP archive. After this command
 * executes, files contained in the ZIP archive will appear to Tcl to be
 * regular files at the mount point.
 *
 * With no mount-point, return mount point for archive. With no archive,
 * return all archive/mount pairs. If mount-point is specified as an empty
 * string, mount on file path.
 */
static int
ZvfsMountObjCmd(
    ClientData clientData,	/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)		/* Values of all arguments */
{
    /*TODO: Convert to Tcl_Obj API!*/
    if (objc > 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", Tcl_GetString(objv[0]),
		" ? ZIP-FILE ? MOUNT-POINT ? ?\"", 0);
	return TCL_ERROR;
    }
    return Tcl_Zvfs_Mount(interp, objc>1?Tcl_GetString(objv[1]):NULL, objc>2?Tcl_GetString(objv[2]):NULL);
}

/*
 * zvfs::unmount  Zip-archive-name
 *
 * Undo the effects of zvfs::mount.
 */
static int
ZvfsUnmountObjCmd(
    ClientData clientData,	/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)		/* Values of all arguments */
{
    ZvfsArchive *pArchive;	/* The ZIP archive being mounted */
    Tcl_HashEntry *pEntry;	/* Hash table entry */
    Tcl_HashSearch zSearch;	/* Search all mount points */
    char *zFilename;
    if (objc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", Tcl_GetString(objv[0]),
		" ZIP-FILE\"", 0);
	return TCL_ERROR;
    }
    if (!local.isInit) {
	return TCL_ERROR;
    }
    zFilename=Tcl_GetString(objv[1]);
    if (Tcl_Zvfs_Umount(zFilename)) {
	return TCL_OK;
    }
    pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
    while (pEntry) {
	pArchive = Tcl_GetHashValue(pEntry);
	if (pArchive && pArchive->zMountPoint[0]
		&& (strcmp(pArchive->zMountPoint, zFilename) == 0)) {
	    if (Tcl_Zvfs_Umount(pArchive->zName)) {
		return TCL_OK;
	    }
	    break;
	}
	pEntry = Tcl_NextHashEntry(&zSearch);
    }

    Tcl_AppendResult(interp, "unknown zvfs mount point or file: ", zFilename,
	    NULL);
    return TCL_ERROR;
}

/*
 * zvfs::exists  filename
 *
 * Return TRUE if the given filename exists in the ZVFS and FALSE if it does
 * not.
 */
static int
ZvfsExistsObjCmd(
    ClientData clientData,	/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    char *zFilename;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "FILENAME");
	return TCL_ERROR;
    }
    zFilename = Tcl_GetString(objv[1]);
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), ZvfsLookup(zFilename)!=0);
    return TCL_OK;
}

/*
 * zvfs::info  filename
 *
 * Return information about the given file in the ZVFS.  The information
 * consists of (1) the name of the ZIP archive that contains the file, (2) the
 * size of the file after decompressions, (3) the compressed size of the file,
 * and (4) the offset of the compressed data in the archive.
 *
 * Note: querying the mount point gives the start of zip data offset in (4),
 * which can be used to truncate the zip info off an executable.
 */
static int
ZvfsInfoObjCmd(
    ClientData clientData,	/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    char *zFilename;
    ZvfsFile *pFile;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "FILENAME");
	return TCL_ERROR;
    }
    zFilename = Tcl_GetString(objv[1]);
    pFile = ZvfsLookup(zFilename);
    if (pFile) {
	Tcl_Obj *pResult = Tcl_GetObjResult(interp);

	Tcl_ListObjAppendElement(interp, pResult, 
		Tcl_NewStringObj(pFile->pArchive->zName, -1));
	Tcl_ListObjAppendElement(interp, pResult,
		Tcl_NewIntObj(pFile->nByte));
	Tcl_ListObjAppendElement(interp, pResult,
		Tcl_NewIntObj(pFile->nByteCompr));
	Tcl_ListObjAppendElement(interp, pResult,
		Tcl_NewIntObj(pFile->iOffset));
    }
    return TCL_OK;
}

/*
 * zvfs::list
 *
 * Return a list of all files in the ZVFS.  The order of the names in the list
 * is arbitrary.
 */
static int
ZvfsListObjCmd(
    ClientData clientData,	/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    char *zPattern = 0;
    Tcl_RegExp pRegexp = 0;
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch sSearch;
    Tcl_Obj *pResult = Tcl_GetObjResult(interp);

    if (objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?(-glob|-regexp)? ?PATTERN?");
	return TCL_ERROR;
    }
    if (local.isInit == 0) {
	return TCL_OK;
    }
    if (objc == 3) {
	int n;
	char *zSwitch = Tcl_GetStringFromObj(objv[1], &n);

	if (n >= 2 && strncmp(zSwitch,"-glob",n) == 0) {
	    zPattern = Tcl_GetString(objv[2]);
	} else if (n >= 2 && strncmp(zSwitch,"-regexp",n) == 0) {
	    pRegexp = Tcl_RegExpCompile(interp, Tcl_GetString(objv[2]));
	    if (pRegexp == 0) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "unknown option: ", zSwitch, 0);
	    return TCL_ERROR;
	}
    } else if (objc == 2) {
	zPattern = Tcl_GetString(objv[1]);
    }

    /*
     * Do the listing.
     */

    if (zPattern) {
	for (pEntry = Tcl_FirstHashEntry(&local.fileHash, &sSearch);
		pEntry; pEntry = Tcl_NextHashEntry(&sSearch)){
	    ZvfsFile *pFile = Tcl_GetHashValue(pEntry);
	    char *z = pFile->zName;

	    if (Tcl_StringCaseMatch(z, zPattern, 1)) {
		Tcl_ListObjAppendElement(interp, pResult,
			Tcl_NewStringObj(z, -1));
	    }
	}
    } else if (pRegexp) {
	for(pEntry = Tcl_FirstHashEntry(&local.fileHash, &sSearch);
		pEntry; pEntry = Tcl_NextHashEntry(&sSearch)){
	    ZvfsFile *pFile = Tcl_GetHashValue(pEntry);
	    char *z = pFile->zName;

	    if (Tcl_RegExpExec(interp, pRegexp, z, z)) {
		Tcl_ListObjAppendElement(interp, pResult,
			Tcl_NewStringObj(z, -1));
	    }
	}
    } else {
	for (pEntry = Tcl_FirstHashEntry(&local.fileHash, &sSearch);
		pEntry; pEntry = Tcl_NextHashEntry(&sSearch)){
	    ZvfsFile *pFile = Tcl_GetHashValue(pEntry);
	    char *z = pFile->zName;

	    Tcl_ListObjAppendElement(interp, pResult,
		    Tcl_NewStringObj(z, -1));
	}
    }
    return TCL_OK;
}

/*
 * Whenever a ZVFS file is opened, an instance of this structure is attached
 * to the open channel where it will be available to the ZVFS I/O routines
 * below.  All state information about an open ZVFS file is held in this
 * structure.
 */
typedef struct ZvfsChannelInfo {
    unsigned int nByte;		/* number of bytes of read uncompressed
				 * data */
    unsigned int nByteCompr;	/* number of bytes of unread compressed
				 * data */
    unsigned int nData;		/* total number of bytes of compressed data */
    int readSoFar;		/* Number of bytes read so far */
    long startOfData;		/* File position of start of data in ZIP
				 * archive */
    int isCompressed;		/* True data is compressed */
    Tcl_Channel chan;		/* Open to the archive file */
    unsigned char *zBuf;	/* buffer used by the decompressor */
    z_stream stream;		/* state of the decompressor */
} ZvfsChannelInfo;

/*
 * This routine is called as an exit handler.  If we do not set
 * ZvfsChannelInfo.chan to NULL, then Tcl_Close() will be called on that
 * channel twice when Tcl_Exit runs.  This will lead to a core dump.
 */
static void
vfsExit(
    void *pArg)
{
    ZvfsChannelInfo *pInfo = pArg;

    pInfo->chan = 0;
}

/*
 * This routine is called when the ZVFS channel is closed
 */
static int
vfsClose(
    ClientData instanceData,	/* A ZvfsChannelInfo structure */
    Tcl_Interp *interp)		/* The TCL interpreter */
{
    ZvfsChannelInfo* pInfo = instanceData;

    if (pInfo->zBuf) {
	Tcl_Free((void *) pInfo->zBuf);
	inflateEnd(&pInfo->stream);
    }
    if (pInfo->chan) {
	Tcl_Close(interp, pInfo->chan);
	Tcl_DeleteExitHandler(vfsExit, pInfo);
    }
    Tcl_Free((void *) pInfo);
    return TCL_OK;
}

/*
 * The TCL I/O system calls this function to actually read information from a
 * ZVFS file.
 */
static int
vfsInput(
    ClientData instanceData,	/* The channel to read from */
    char *buf,			/* Buffer to fill */
    int toRead,			/* Requested number of bytes */
    int *pErrorCode)		/* Location of error flag */
{
    ZvfsChannelInfo* pInfo = instanceData;

    if (toRead > pInfo->nByte) {
	toRead = pInfo->nByte;
    }
    if (toRead == 0) {
	return 0;
    }
    if (pInfo->isCompressed) {
	int err = Z_OK;
	z_stream *stream = &pInfo->stream;

	stream->next_out = (unsigned char *) buf;
	stream->avail_out = toRead;
	while (stream->avail_out) {
	    if (!stream->avail_in) {
		int len = pInfo->nByteCompr;

		if (len > COMPR_BUF_SIZE) {
		    len = COMPR_BUF_SIZE;
		}
		len = Tcl_Read(pInfo->chan, (char *) pInfo->zBuf, len);
		pInfo->nByteCompr -= len;
		stream->next_in = pInfo->zBuf;
		stream->avail_in = len;
	    }
	    err = inflate(stream, Z_NO_FLUSH);
	    if (err) {
		break;
	    }
	}
	if (err == Z_STREAM_END) {
	    if (stream->avail_out != 0) {
		*pErrorCode = err; /* premature end */
		return -1;
	    }
	}else if (err) {
	    *pErrorCode = err; /* some other zlib error */
	    return -1;
	}
    } else {
	toRead = Tcl_Read(pInfo->chan, buf, toRead);
    }
    pInfo->nByte -= toRead;
    pInfo->readSoFar += toRead;
    *pErrorCode = 0;
    return toRead;
}

/*
 * Write to a ZVFS file.  ZVFS files are always read-only, so this routine
 * always returns an error.
 */
static int
vfsOutput(
    ClientData instanceData,	/* The channel to write to */
    const char *buf,		/* Data to be stored. */
    int toWrite,		/* Number of bytes to write. */
    int *pErrorCode)		/* Location of error flag. */
{
    *pErrorCode = EINVAL;
    return -1;
}

/*
 * Move the file pointer so that the next byte read will be "offset".
 */
static int
vfsSeek(
    ClientData instanceData,	/* The file structure */
    long offset,		/* Offset to seek to */
    int mode,			/* One of SEEK_CUR, SEEK_SET or SEEK_END */
    int *pErrorCode)		/* Write the error code here */
{
    ZvfsChannelInfo* pInfo = instanceData;

    switch (mode) {
    case SEEK_CUR:
	offset += pInfo->readSoFar;
	break;
    case SEEK_END:
	offset += pInfo->readSoFar + pInfo->nByte;
	break;
    default:
	/* Do nothing */
	break;
    }
    if (offset < 0) {
	offset = 0;
    }
    if (!pInfo->isCompressed) {
	Tcl_Seek(pInfo->chan, offset + pInfo->startOfData, SEEK_SET);
	pInfo->nByte = pInfo->nData;
	pInfo->readSoFar = offset;
    } else {
	if (offset < pInfo->readSoFar) {
	    z_stream *stream = &pInfo->stream;

	    inflateEnd(stream);
	    stream->zalloc = (alloc_func)0;
	    stream->zfree = (free_func)0;
	    stream->opaque = (voidpf)0;
	    stream->avail_in = 2;
	    stream->next_in = pInfo->zBuf;
	    pInfo->zBuf[0] = 0x78;
	    pInfo->zBuf[1] = 0x01;
	    inflateInit(&pInfo->stream);
	    Tcl_Seek(pInfo->chan, pInfo->startOfData, SEEK_SET);
	    pInfo->nByte += pInfo->readSoFar;
	    pInfo->nByteCompr = pInfo->nData;
	    pInfo->readSoFar = 0;
	}
	while (pInfo->readSoFar < offset) {
	    int toRead, errCode;
	    char zDiscard[100];

	    toRead = offset - pInfo->readSoFar;
	    if (toRead > sizeof(zDiscard)) {
		toRead = sizeof(zDiscard);
	    }
	    vfsInput(instanceData, zDiscard, toRead, &errCode);
	}
    }
    return pInfo->readSoFar;
}

/*
 * Handle events on the channel.  ZVFS files do not generate events, so this
 * is a no-op.
 */
static void
vfsWatchChannel(
    ClientData instanceData,	/* Channel to watch */
    int mask)			/* Events of interest */
{
    return;
}

/*
 * Called to retrieve the underlying file handle for this ZVFS file.  As the
 * ZVFS file has no underlying file handle, this is a no-op.
 */
static int
vfsGetFile(
    ClientData instanceData,	/* Channel to query */
    int direction,		/* Direction of interest */
    ClientData* handlePtr)	/* Space to the handle into */
{
    return TCL_ERROR;
}

/*
 * This structure describes the channel type structure for access to the ZVFS.
 */
static Tcl_ChannelType vfsChannelType = {
    "vfs",			/* Type name. */
    NULL,			/* Set blocking/nonblocking behaviour.
				 * NULL'able */
    vfsClose,			/* Close channel, clean instance data */
    vfsInput,			/* Handle read request */
    vfsOutput,			/* Handle write request */
    vfsSeek,			/* Move location of access point. NULL'able */
    NULL,			/* Set options. NULL'able */
    NULL,			/* Get options. NULL'able */
    vfsWatchChannel,		/* Initialize notifier */
    vfsGetFile			/* Get OS handle from the channel. */
};

/*
 * This routine attempts to do an open of a file.  Check to see if the file is
 * located in the ZVFS.  If so, then open a channel for reading the file.  If
 * not, return NULL.
 */
static Tcl_Channel
ZvfsFileOpen(
    Tcl_Interp *interp,		/* The TCL interpreter doing the open */
    char *zFilename,		/* Name of the file to open */
    char *modeString,		/* Mode string for the open (ignored) */
    int permissions)		/* Permissions for a newly created file
				 * (ignored). */
{
    ZvfsFile *pFile;
    ZvfsChannelInfo *pInfo;
    Tcl_Channel chan;
    static int count = 1;
    char zName[50];
    unsigned char zBuf[50];

    pFile = ZvfsLookup(zFilename);
    if (pFile == 0) {
	return NULL;
    }
    openarch = 1;
    chan = Tcl_OpenFileChannel(interp, pFile->pArchive->zName, "r", 0);
    openarch = 0;
    if (chan == 0) {
	return 0;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")){
	/* this should never happen */
	Tcl_Close(0, chan);
	return 0;
    }
    Tcl_Seek(chan, pFile->iOffset, SEEK_SET);
    Tcl_Read(chan, (char *) zBuf, 30);
    if (memcmp(zBuf, "\120\113\03\04", 4)) {
	if (interp) {
	    Tcl_AppendResult(interp, "local header mismatch: ", NULL);
	}
	Tcl_Close(interp, chan);
	return 0;
    }
    pInfo = (void *) Tcl_Alloc(sizeof(*pInfo));
    pInfo->chan = chan;
    Tcl_CreateExitHandler(vfsExit, pInfo);
    pInfo->isCompressed = INT16(zBuf, 8);
    if (pInfo->isCompressed) {
	z_stream *stream = &pInfo->stream;

	pInfo->zBuf = (void *) Tcl_Alloc(COMPR_BUF_SIZE);
	stream->zalloc = NULL;
	stream->zfree = NULL;
	stream->opaque = NULL;
	stream->avail_in = 2;
	stream->next_in = pInfo->zBuf;
	pInfo->zBuf[0] = 0x78;
	pInfo->zBuf[1] = 0x01;
	inflateInit(&pInfo->stream);
    } else {
	pInfo->zBuf = 0;
    }
    pInfo->nByte = INT32(zBuf, 22);
    pInfo->nByteCompr = pInfo->nData = INT32(zBuf, 18);
    pInfo->readSoFar = 0;
    Tcl_Seek(chan, INT16(zBuf, 26) + INT16(zBuf, 28), SEEK_CUR);
    pInfo->startOfData = Tcl_Tell(chan);
    sprintf(zName, "zvfs_%x",count++);
    chan = Tcl_CreateChannel(&vfsChannelType, zName, pInfo, TCL_READABLE);
    return chan;
}

/*
 * This routine does a stat() system call for a ZVFS file.
 */
static int
Tobe_FSStatProc(
    Tcl_Obj *pathObj,
    Tcl_StatBuf *buf)
{
    char *path=Tcl_GetString(pathObj);
    ZvfsFile *pFile;

    pFile = ZvfsLookup(path);
    if (pFile == 0) {
	return -1;
    }
    memset(buf, 0, sizeof(*buf));
    if (pFile->isdir) {
	buf->st_mode = 040555;
    } else {
	buf->st_mode = (0100000|pFile->permissions);
    }
    buf->st_ino = 0;
    buf->st_size = pFile->nByte;
    buf->st_mtime = pFile->timestamp;
    buf->st_ctime = pFile->timestamp;
    buf->st_atime = pFile->timestamp;
    return 0;
}

/*
 * This routine does an access() system call for a ZVFS file.
 */
static int
Tobe_FSAccessProc(
    Tcl_Obj *pathPtr,
    int mode)
{
    char *path=Tcl_GetString(pathPtr);
    ZvfsFile *pFile;

    if (mode & 3) {
	return -1;
    }
    pFile = ZvfsLookup(path);
    if (pFile == 0) {
	return -1;
    }
    return 0; 
}

Tcl_Channel
Tobe_FSOpenFileChannelProc(
    Tcl_Interp *interp,
    Tcl_Obj *pathPtr,
    int mode,
    int permissions)
{
    static int inopen=0;
    Tcl_Channel chan;

    if (inopen) {
	puts("recursive zvfs open");
	return NULL;
    }
    inopen = 1;
    /* if (mode != O_RDONLY) return NULL; */
    chan = ZvfsFileOpen(interp, Tcl_GetString(pathPtr), 0, permissions);
    inopen = 0;
    return chan;
}

Tcl_Obj* Tobe_FSFilesystemSeparatorProc(Tcl_Obj *pathPtr) { 
  return Tcl_NewStringObj("/",-1);;
}

/*
 * Function to process a 'Tobe_FSMatchInDirectory()'.  If not implemented,
 * then glob and recursive copy functionality will be lacking in the
 * filesystem.
 */
int
Tobe_FSMatchInDirectoryProc(
    Tcl_Interp* interp,
    Tcl_Obj *result,
    Tcl_Obj *pathPtr,
    const char *pattern,
    Tcl_GlobTypeData * types)
{
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch sSearch;
    int scnt, len, l, dirglob, dirmnt;
    char *zPattern = NULL, *zp=Tcl_GetStringFromObj(pathPtr,&len);

    if (!zp) {
	return TCL_ERROR;
    }
    if (pattern != NULL) {
	l = strlen(pattern);
	if (!zp) {
	    zPattern = Tcl_Alloc(len + 1);
	    memcpy(zPattern, pattern, len + 1);
	} else {
	    zPattern = Tcl_Alloc(len + l + 3);
	    sprintf(zPattern, "%s%s%s", zp, zp[len-1]=='/'?"":"/", pattern);
	}
	scnt = strchrcnt(zPattern, '/');
    }
    dirglob = (types && types->type && (types->type&TCL_GLOB_TYPE_DIR));
    dirmnt = (types && types->type && (types->type&TCL_GLOB_TYPE_MOUNT));
    if (strcmp(zp, "/") == 0 && strcmp(zPattern, ".*") == 0) {
	/*TODO: What goes here?*/
    }
    for (pEntry = Tcl_FirstHashEntry(&local.fileHash, &sSearch);
	    pEntry; pEntry = Tcl_NextHashEntry(&sSearch)){
	ZvfsFile *pFile = Tcl_GetHashValue(pEntry);
	char *z = pFile->zName;

	if (zPattern != NULL) {
	    if (Tcl_StringCaseMatch(z, zPattern, 0) == 0 || 
		    (scnt != pFile->depth /* && !dirglob */)) { // TODO: ???
		continue;
	    }
	} else {
	    if (strcmp(zp, z)) {
		continue;
	    }
	}
	if (dirmnt) {
	    if (pFile->isdir != 1) {
		continue;
	    }
	} else if (dirglob) {
	    if (!pFile->isdir) {
		continue;
	    }
	} else if (types && !(types->type & TCL_GLOB_TYPE_DIR)) {
	    if (pFile->isdir) {
		continue;
	    }
	}
	Tcl_ListObjAppendElement(interp, result, Tcl_NewStringObj(z, -1));
    }
    if (zPattern) {
	Tcl_Free(zPattern);
    }
    return TCL_OK;
}

/*
 * Function to check whether a path is in this filesystem.  This is the most
 * important filesystem procedure.
 */
int
Tobe_FSPathInFilesystemProc(
    Tcl_Obj *pathPtr,
    ClientData *clientDataPtr)
{
    ZvfsFile *zFile;
    char *path = Tcl_GetString(pathPtr);
    
    if (openarch) {
	return -1;
    }
    zFile = ZvfsLookup(path);
    if (zFile != NULL && strcmp(path, zFile->pArchive->zName)) {
	return TCL_OK;
    }
    return -1;
}

Tcl_Obj *
Tobe_FSListVolumesProc(void)
{
    Tcl_HashEntry *pEntry;	/* Hash table entry */
    Tcl_HashSearch zSearch;	/* Search all mount points */
    ZvfsArchive *pArchive;	/* The ZIP archive being mounted */
    Tcl_Obj *pVols = NULL, *pVol;

    pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
    while (pEntry) {
	pArchive = Tcl_GetHashValue(pEntry);
	if (pArchive) {
	    if (!pVols) {
		pVols = Tcl_NewListObj(0, 0);
		Tcl_IncrRefCount(pVols);
	    }
	    pVol = Tcl_NewStringObj(pArchive->zMountPoint, -1);
	    Tcl_ListObjAppendElement(NULL, pVols, pVol);
	}
	pEntry = Tcl_NextHashEntry(&zSearch);
    }
    return pVols;
}

const char * const*
Tobe_FSFileAttrStringsProc(
    Tcl_Obj *pathPtr,
    Tcl_Obj** objPtrRef)
{
    char *path = Tcl_GetString(pathPtr);
#ifdef __WIN32__
    static const char *attrs[] = {
	"-archive", "-hidden", "-readonly", "-system", "-shortname", 0
    };
#else
    static const char *attrs[] = {
	"-group", "-owner", "-permissions", 0
    };
#endif
    if (ZvfsLookup(path) == 0) {
	return NULL;
    }
    return attrs;
}

int
Tobe_FSFileAttrsGetProc(
    Tcl_Interp *interp,
    int index,
    Tcl_Obj *pathPtr,
    Tcl_Obj **objPtrRef)
{
    char *path = Tcl_GetString(pathPtr);
#ifndef __WIN32__
    char buf[50];
#endif
    ZvfsFile *zFile = ZvfsLookup(path);

    if (zFile == 0) {
	return TCL_ERROR;
    }

    switch (index) {
#ifdef __WIN32__
    case 0: /* -archive */
	*objPtrRef = Tcl_NewStringObj("0", -1); break;
    case 1: /* -hidden */
	*objPtrRef = Tcl_NewStringObj("0", -1); break;
    case 2: /* -readonly */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 3: /* -system */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 4: /* -shortname */
	*objPtrRef = Tcl_NewStringObj("", -1);
#else
    case 0: /* -group */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 1: /* -owner */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 2: /* -permissions */
	sprintf(buf, "%03o", zFile->permissions);
	*objPtrRef = Tcl_NewStringObj(buf, -1); break;
#endif
    }

    return TCL_OK;
}

/****************************************************/

/*
 * Function to unload a previously successfully loaded file.  If load was
 * implemented, then this should also be implemented, if there is any cleanup
 * action required.
 */
/* We have to declare the utime structure here. */
int Tobe_FSUtimeProc(Tcl_Obj *pathPtr, struct utimbuf *tval) { return 0; }
int Tobe_FSFileAttrsSetProc(Tcl_Interp *interp, int index, Tcl_Obj *pathPtr,
			    Tcl_Obj *objPtr) { return 0; }

static Tcl_Filesystem Tobe_Filesystem = {
    "tobe",			/* The name of the filesystem. */
    sizeof(Tcl_Filesystem),	/* Length of this structure, so future binary
				 * compatibility can be assured. */
    TCL_FILESYSTEM_VERSION_1,	/* Version of the filesystem type. */
    Tobe_FSPathInFilesystemProc,/* Function to check whether a path is in this
				 * filesystem.  This is the most important
				 * filesystem procedure. */
    NULL,	/* Function to duplicate internal fs rep.  May
				 * be NULL (but then fs is less efficient). */
    NULL,	/* Function to free internal fs rep.  Must be
				 * implemented, if internal representations
				 * need freeing, otherwise it can be NULL. */
    NULL,
				/* Function to convert internal representation
				 * to a normalized path.  Only required if the
				 * fs creates pure path objects with no
				 * string/path representation. */
    NULL,
				/* Function to create a filesystem-specific
				 * internal representation.  May be NULL if
				 * paths have no internal representation, or
				 * if the Tobe_FSPathInFilesystemProc for this
				 * filesystem always immediately creates an
				 * internal representation for paths it
				 * accepts. */
    NULL,	/* Function to normalize a path.  Should be
				 * implemented for all filesystems which can
				 * have multiple string representations for
				 * the same path object. */
    NULL,
				/* Function to determine the type of a path in
				 * this filesystem.  May be NULL. */
    Tobe_FSFilesystemSeparatorProc,
				/* Function to return the separator
				 * character(s) for this filesystem.  Must be
				 * implemented. */
    Tobe_FSStatProc,		/* Function to process a 'Tobe_FSStat()' call.
				 * Must be implemented for any reasonable
				 * filesystem. */
    Tobe_FSAccessProc,		/* Function to process a 'Tobe_FSAccess()'
				 * call. Must be implemented for any
				 * reasonable filesystem. */
    Tobe_FSOpenFileChannelProc,	/* Function to process a
				 * 'Tobe_FSOpenFileChannel()' call.  Must be
				 * implemented for any reasonable
				 * filesystem. */
    Tobe_FSMatchInDirectoryProc,/* Function to process a
				 * 'Tobe_FSMatchInDirectory()'.  If not
				 * implemented, then glob and recursive copy
				 * functionality will be lacking in the
				 * filesystem. */
    Tobe_FSUtimeProc,		/* Function to process a 'Tobe_FSUtime()'
				 * call.  Required to allow setting (not
				 * reading) of times with 'file mtime', 'file
				 * atime' and the open-r/open-w/fcopy
				 * implementation of 'file copy'. */
    NULL,		/* Function to process a 'Tobe_FSLink()' call.
				 * Should be implemented only if the
				 * filesystem supports links. */
    Tobe_FSListVolumesProc,	/* Function to list any filesystem volumes
				 * added by this filesystem.  Should be
				 * implemented only if the filesystem adds
				 * volumes at the head of the filesystem. */
    Tobe_FSFileAttrStringsProc,	/* Function to list all attributes strings
				 * which are valid for this filesystem.  If
				 * not implemented the filesystem will not
				 * support the 'file attributes' command.
				 * This allows arbitrary additional
				 * information to be attached to files in the
				 * filesystem. */
    Tobe_FSFileAttrsGetProc,	/* Function to process a
				 * 'Tobe_FSFileAttrsGet()' call, used by 'file
				 * attributes'. */
    Tobe_FSFileAttrsSetProc,	/* Function to process a
				 * 'Tobe_FSFileAttrsSet()' call, used by 'file
				 * attributes'.  */
    NULL, /* Function to process a
				 * 'Tobe_FSCreateDirectory()' call. Should be
				 * implemented unless the FS is read-only. */
    NULL, /* Function to process a
				 * 'Tobe_FSRemoveDirectory()' call. Should be
				 * implemented unless the FS is read-only. */
    NULL,	/* Function to process a 'Tobe_FSDeleteFile()'
				 * call.  Should be implemented unless the FS
				 * is read-only. */
    NULL,	/* Function to process a 'Tobe_FSCopyFile()'
				 * call.  If not implemented Tcl will fall
				 * back on open-r, open-w and fcopy as a
				 * copying mechanism. */
    NULL,	/* Function to process a 'Tobe_FSRenameFile()'
				 * call.  If not implemented, Tcl will fall
				 * back on a copy and delete mechanism. */
    NULL,	/* Function to process a
				 * 'Tobe_FSCopyDirectory()' call.  If not
				 * implemented, Tcl will fall back on a
				 * recursive create-dir, file copy
				 * mechanism. */
    NULL,	/* Function to process a 'Tobe_FSLoadFile()'
				 * call. If not implemented, Tcl will fall
				 * back on a copy to native-temp followed by a
				 * Tobe_FSLoadFile on that temporary copy. */
    NULL,	/* Function to unload a previously
				 * successfully loaded file.  If load was
				 * implemented, then this should also be
				 * implemented, if there is any cleanup action
				 * required. */
    NULL,		/* Function to process a 'Tobe_FSGetCwd()'
				 * call. Most filesystems need not implement
				 * this. It will usually only be called once,
				 * if 'getcwd' is called before 'chdir'.  May
				 * be NULL. */
    NULL,		/* Function to process a 'Tobe_FSChdir()'
				 * call. If filesystems do not implement this,
				 * it will be emulated by a series of
				 * directory access checks. Otherwise, virtual
				 * filesystems which do implement it need only
				 * respond with a positive return result if
				 * the dirName is a valid directory in their
				 * filesystem. They need not remember the
				 * result, since that will be automatically
				 * remembered for use by GetCwd. Real
				 * filesystems should carry out the correct
				 * action (i.e. call the correct system
				 * 'chdir' api). If not implemented, then 'cd'
				 * and 'pwd' will fail inside the
				 * filesystem. */
};

//////////////////////////////////////////////////////////////

void (*Zvfs_PostInit)(Tcl_Interp *) = 0;
static int ZvfsAppendObjCmd(void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsAddObjCmd(void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsDumpObjCmd(void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsStartObjCmd(void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);

static int Zvfs_Common_Init(Tcl_Interp *interp) {
  if (local.isInit) return TCL_OK;
  /* One-time initialization of the ZVFS */
  if(Tcl_FSRegister(interp, &Tobe_Filesystem)) {
    return TCL_ERROR;
  }
  Tcl_InitHashTable(&local.fileHash, TCL_STRING_KEYS);
  Tcl_InitHashTable(&local.archiveHash, TCL_STRING_KEYS);
  local.isInit = 1;
  return TCL_OK;
}

/*
 * Initialize the ZVFS system.
 */
int
Zvfs_doInit(
    Tcl_Interp *interp,
    int safe)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.0", 0) == 0) {
	return TCL_ERROR;
    }
#endif
    Tcl_StaticPackage(interp, "zvfs", TclZvfsInit, Tcl_Zvfs_SafeInit);
    if (!safe) {
	Tcl_CreateObjCommand(interp, "zvfs::mount", ZvfsMountObjCmd, 0, 0);
	Tcl_CreateObjCommand(interp, "zvfs::unmount", ZvfsUnmountObjCmd, 0, 0);
	Tcl_CreateObjCommand(interp, "zvfs::append", ZvfsAppendObjCmd, 0, 0);
	Tcl_CreateObjCommand(interp, "zvfs::add", ZvfsAddObjCmd, 0, 0);
    }
    Tcl_CreateObjCommand(interp, "zvfs::exists", ZvfsExistsObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "zvfs::info", ZvfsInfoObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "zvfs::list", ZvfsListObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "zvfs::dump", ZvfsDumpObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "zvfs::start", ZvfsStartObjCmd, 0, 0);
    Tcl_SetVar(interp, "::zvfs::auto_ext",
	    ".tcl .tk .itcl .htcl .txt .c .h .tht", TCL_GLOBAL_ONLY);
    /* Tcl_CreateObjCommand(interp, "zip::open", ZipOpenObjCmd, 0, 0); */
    if(Zvfs_Common_Init(interp)) {
      return TCL_ERROR;
    }

    if (Zvfs_PostInit) {
	Zvfs_PostInit(interp);
    }
    return TCL_OK;
}

/*
** Boot a shell, mount the executable's VFS, detect main.tcl
*/
int Tcl_Zvfs_Boot(const char *archive,const char *vfsmountpoint,const char *initscript) {
  Zvfs_Common_Init(NULL);
  if(!vfsmountpoint) {
    vfsmountpoint="/zvfs";
  }
  if(!initscript) {
    initscript="main.tcl";
  }
  /* We have to initialize the virtual filesystem before calling
  ** Tcl_Init().  Otherwise, Tcl_Init() will not be able to find
  ** its startup script files.
  */
  if(!Tcl_Zvfs_Mount(NULL, archive, vfsmountpoint)) {
      Tcl_DString filepath;
      Tcl_DString preinit;

      Tcl_Obj *vfsinitscript;
      Tcl_Obj *vfstcllib;
      Tcl_Obj *vfstklib;
      Tcl_Obj *vfspreinit;
      
      Tcl_DStringInit(&filepath);
      Tcl_DStringInit(&preinit);
          
      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/",-1);
      Tcl_DStringAppend(&filepath,initscript,-1);
      vfsinitscript=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);
      
      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/tcl8.6",-1);
      vfstcllib=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);

      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/tk8.6",-1);
      vfstklib=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);

      Tcl_IncrRefCount(vfsinitscript);
      Tcl_IncrRefCount(vfstcllib);
      Tcl_IncrRefCount(vfstklib);
      
      if(Tcl_FSAccess(vfsinitscript,F_OK)==0) {
	/* Startup script should be set before calling Tcl_AppInit */
        Tcl_SetStartupScript(vfsinitscript,NULL);
      }

      if(Tcl_FSAccess(vfsinitscript,F_OK)==0) {
	/* Startup script should be set before calling Tcl_AppInit */
        Tcl_SetStartupScript(vfsinitscript,NULL);
      } else {
        Tcl_SetStartupScript(NULL,NULL);
      }
      if(Tcl_FSAccess(vfstcllib,F_OK)==0) {
        Tcl_DStringAppend(&preinit,"\nset tcl_library ",-1);
        Tcl_DStringAppendElement(&preinit,Tcl_GetString(vfstcllib));
      }
      if(Tcl_FSAccess(vfstklib,F_OK)==0) {
        Tcl_DStringAppend(&preinit,"\nset tk_library ",-1);
        Tcl_DStringAppendElement(&preinit,Tcl_GetString(vfstklib));
      }
      vfspreinit=Tcl_NewStringObj(Tcl_DStringValue(&preinit),-1);
      /* NOTE: We never decr this refcount, lest the contents of the script be deallocated */
      Tcl_IncrRefCount(vfspreinit);
      TclSetPreInitScript(Tcl_GetString(vfspreinit));

      Tcl_DecrRefCount(vfsinitscript);
      Tcl_DecrRefCount(vfstcllib);
      Tcl_DecrRefCount(vfstklib);
  }
  return TCL_OK;
}


int
TclZvfsInit(
    Tcl_Interp *interp)
{
    return Zvfs_doInit(interp, 0);
}

int
Tcl_Zvfs_SafeInit(
    Tcl_Interp *interp)
{
    return Zvfs_doInit(interp, 1);
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

/*
 * Implement the zvfs::dump command
 *
 *    zvfs::dump  ARCHIVE
 *
 * Each entry in the list returned is of the following form:
 *
 *   {FILENAME  DATE-TIME  SPECIAL-FLAG  OFFSET  SIZE  COMPRESSED-SIZE}
 */
static int
ZvfsDumpObjCmd(
    void *NotUsed,		/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    Tcl_Obj *zFilenameObj;
    Tcl_Channel chan;
    ZFile *pList;
    int rc;
    Tcl_Obj *pResult;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "FILENAME");
	return TCL_ERROR;
    }
    zFilenameObj=objv[1];
    chan = Tcl_FSOpenFileChannel(interp, zFilenameObj, "r", 0);
    if (chan == 0) {
	return TCL_ERROR;
    }
    rc = ZvfsReadTOC(interp, chan, &pList);
    if (rc == TCL_ERROR) {
	deleteZFileList(pList);
	return rc;
    }
    Tcl_Close(interp, chan);
    pResult = Tcl_GetObjResult(interp);
    while (pList) {
	Tcl_Obj *pEntry = Tcl_NewObj();
	ZFile *pNext;
	char zDateTime[100];

	Tcl_ListObjAppendElement(interp, pEntry,
		Tcl_NewStringObj(pList->zName,-1));
	translateDosTimeDate(zDateTime, pList->dosDate, pList->dosTime);
	Tcl_ListObjAppendElement(interp, pEntry,
		Tcl_NewStringObj(zDateTime, -1));
	Tcl_ListObjAppendElement(interp, pEntry,
		Tcl_NewIntObj(pList->isSpecial));
	Tcl_ListObjAppendElement(interp, pEntry,
		Tcl_NewIntObj(pList->iOffset));
	Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewIntObj(pList->nByte));
	Tcl_ListObjAppendElement(interp, pEntry,
		Tcl_NewIntObj(pList->nByteCompr));
	Tcl_ListObjAppendElement(interp, pResult, pEntry);
	pNext = pList->pNext;
	Tcl_Free((void *) pList);
	pList = pNext;
    }
    return TCL_OK;
}

/*
 * Write a file record into a ZIP archive at the current position of the write
 * cursor for channel "chan".  Add a ZFile record for the file to *ppList.  If
 * an error occurs, leave an error message on interp and return TCL_ERROR.
 * Otherwise return TCL_OK.
 */
static int
writeFile(
    Tcl_Interp *interp,		/* Leave an error message here */
    Tcl_Channel out,		/* Write the file here */
    Tcl_Channel in,		/* Read data from this file */
    Tcl_Obj *zSrcPtr,		/* Name the new ZIP file entry this */
    Tcl_Obj *zDestPtr,		/* Name the new ZIP file entry this */
    ZFile **ppList)		/* Put a ZFile struct for the new file here */
{
    char *zDest=Tcl_GetString(zDestPtr);

    z_stream stream;
    ZFile *p;
    int iEndOfData;
    int nameLen;
    int skip;
    int toOut;
    char zHdr[30];
    char zInBuf[100000];
    char zOutBuf[100000];
    struct tm *tm;
    time_t now;
    Tcl_StatBuf stat;

    /*
     * Create a new ZFile structure for this file.
     * TODO: fill in date/time etc.
     */
    nameLen = strlen(zDest);
    p = newZFile(nameLen, ppList);
    strcpy(p->zName, zDest);
    p->isSpecial = 0;
    Tcl_FSStat(zSrcPtr, &stat);
    now = stat.st_mtime;
    tm = localtime(&now);
    UnixTimeDate(tm, &p->dosDate, &p->dosTime);
    p->iOffset = Tcl_Tell(out);
    p->nByte = 0;
    p->nByteCompr = 0;
    p->nExtra = 0;
    p->iCRC = 0;
    p->permissions = stat.st_mode;

    /*
     * Fill in as much of the header as we know.
     */

    put32(&zHdr[0], 0x04034b50);
    put16(&zHdr[4], 0x0014);
    put16(&zHdr[6], 0);
    put16(&zHdr[8], 8);
    put16(&zHdr[10], p->dosTime);
    put16(&zHdr[12], p->dosDate);
    put16(&zHdr[26], nameLen);
    put16(&zHdr[28], 0);

    /*
     * Write the header and filename.
     */

    Tcl_Write(out, zHdr, 30);
    Tcl_Write(out, zDest, nameLen);

    /*
     * The first two bytes that come out of the deflate compressor are some
     * kind of header that ZIP does not use.  So skip the first two output
     * bytes.
     */

    skip = 2;

    /*
     * Write the compressed file.  Compute the CRC as we progress.
     */

    stream.zalloc = NULL;
    stream.zfree = NULL;
    stream.opaque = 0;
    stream.avail_in = 0;
    stream.next_in = (unsigned char *) zInBuf;
    stream.avail_out = sizeof(zOutBuf);
    stream.next_out = (unsigned char *) zOutBuf;
    deflateInit(&stream, 9);


    p->iCRC = crc32(0, 0, 0);
    while (!Tcl_Eof(in)) {
	if (stream.avail_in == 0) {
	    int amt = Tcl_Read(in, zInBuf, sizeof(zInBuf));

	    if (amt <= 0) {
		break;
	    }
	    p->iCRC = crc32(p->iCRC, (unsigned char *) zInBuf, amt);
	    stream.avail_in = amt;
	    stream.next_in = (unsigned char *) zInBuf;
	}
	deflate(&stream, 0);
	toOut = sizeof(zOutBuf) - stream.avail_out;
	if (toOut > skip) {
	    Tcl_Write(out, &zOutBuf[skip], toOut - skip);
	    skip = 0;
	} else {
	    skip -= toOut;
	}
	stream.avail_out = sizeof(zOutBuf);
	stream.next_out = (unsigned char *) zOutBuf;
    }

    do{
	stream.avail_out = sizeof(zOutBuf);
	stream.next_out = (unsigned char *) zOutBuf;
	deflate(&stream, Z_FINISH);
	toOut = sizeof(zOutBuf) - stream.avail_out;
	if (toOut > skip) {
	    Tcl_Write(out, &zOutBuf[skip], toOut - skip);
	    skip = 0;
	} else {
	    skip -= toOut;
	}
    } while (stream.avail_out == 0);

    p->nByte = stream.total_in;
    p->nByteCompr = stream.total_out - 2;
    deflateEnd(&stream);
    Tcl_Flush(out);

    /*
     * Remember were we are in the file.  Then go back and write the header,
     * now that we know the compressed file size.
     */

    iEndOfData = Tcl_Tell(out);
    Tcl_Seek(out, p->iOffset, SEEK_SET);
    put32(&zHdr[14], p->iCRC);
    put32(&zHdr[18], p->nByteCompr);
    put32(&zHdr[22], p->nByte);
    Tcl_Write(out, zHdr, 30);
    Tcl_Seek(out, iEndOfData, SEEK_SET);

    /*
     * Close the input file.
     */

    Tcl_Close(interp, in);
    return TCL_OK;
}

/*
 * The arguments are two lists of ZFile structures sorted by iOffset.  Either
 * or both list may be empty.  This routine merges the two lists together into
 * a single sorted list and returns a pointer to the head of the unified list.
 *
 * This is part of the merge-sort algorithm.
 */
static ZFile *
mergeZFiles(
    ZFile *pLeft,
    ZFile *pRight)
{
    ZFile fakeHead;
    ZFile *pTail;

    pTail = &fakeHead;
    while (pLeft && pRight) {
	ZFile *p;

	if (pLeft->iOffset <= pRight->iOffset) {
	    p = pLeft;
	    pLeft = p->pNext;
	} else {
	    p = pRight;
	    pRight = p->pNext;
	}
	pTail->pNext = p;
	pTail = p;
    }
    if (pLeft) {
	pTail->pNext = pLeft;
    } else if (pRight) {
	pTail->pNext = pRight;
    } else {
	pTail->pNext = 0;
    }
    return fakeHead.pNext;
}

/*
 * Sort a ZFile list so in accending order by iOffset.
 */
static ZFile *
sortZFiles(
    ZFile *pList)
{
#define NBIN 30
    int i;
    ZFile *p;
    ZFile *aBin[NBIN+1];

    for (i=0; i<=NBIN; i++) {
	aBin[i] = 0;
    }
    while (pList) {
	p = pList;
	pList = p->pNext;
	p->pNext = 0;
	for (i=0; i<NBIN && aBin[i]; i++) {
	    p = mergeZFiles(aBin[i],p);
	    aBin[i] = 0;
	}
	aBin[i] = aBin[i] ? mergeZFiles(aBin[i], p) : p;
    }
    p = 0;
    for (i=0; i<=NBIN; i++) {
	if (aBin[i] == 0) {
	    continue;
	}
	p = mergeZFiles(p, aBin[i]);
    }
    return p;
}

/*
 * Write a ZIP archive table of contents to the given channel.
 */
static void
writeTOC(
    Tcl_Channel chan,
    ZFile *pList)
{
    int iTocStart, iTocEnd;
    int nEntry = 0;
    int i;
    char zBuf[100];

    iTocStart = Tcl_Tell(chan);
    for (; pList; pList=pList->pNext) {
	if (pList->isSpecial) {
	    continue;
	}
	put32(&zBuf[0], 0x02014b50);
	put16(&zBuf[4], 0x0317);
	put16(&zBuf[6], 0x0014);
	put16(&zBuf[8], 0);
	put16(&zBuf[10], pList->nByte>pList->nByteCompr ? 0x0008 : 0x0000);
	put16(&zBuf[12], pList->dosTime);
	put16(&zBuf[14], pList->dosDate);
	put32(&zBuf[16], pList->iCRC);
	put32(&zBuf[20], pList->nByteCompr);
	put32(&zBuf[24], pList->nByte);
	put16(&zBuf[28], strlen(pList->zName));
	put16(&zBuf[30], 0);
	put16(&zBuf[32], pList->nExtra);
	put16(&zBuf[34], 1);
	put16(&zBuf[36], 0);
	put32(&zBuf[38], pList->permissions<<16);
	put32(&zBuf[42], pList->iOffset);
	Tcl_Write(chan, zBuf, 46);
	Tcl_Write(chan, pList->zName, strlen(pList->zName));
	for (i=pList->nExtra; i>0; i-=40) {
	    int toWrite = i<40 ? i : 40;

	    /* CAREFUL! String below is intentionally 40 spaces! */
	    Tcl_Write(chan,"                                             ",
		    toWrite);
	}
	nEntry++;
    }
    iTocEnd = Tcl_Tell(chan);
    put32(&zBuf[0], 0x06054b50);
    put16(&zBuf[4], 0);
    put16(&zBuf[6], 0);
    put16(&zBuf[8], nEntry);
    put16(&zBuf[10], nEntry);
    put32(&zBuf[12], iTocEnd - iTocStart);
    put32(&zBuf[16], iTocStart);
    put16(&zBuf[20], 0);
    Tcl_Write(chan, zBuf, 22);
    Tcl_Flush(chan);
}

/*
 * Implementation of the zvfs::append command.
 *
 *    zvfs::append ARCHIVE (SOURCE DESTINATION)*
 *
 * This command reads SOURCE files and appends them (using the name
 * DESTINATION) to the zip archive named ARCHIVE. A new zip archive is created
 * if it does not already exist. If ARCHIVE refers to a file which exists but
 * is not a zip archive, then this command turns ARCHIVE into a zip archive by
 * appending the necessary records and the table of contents. Treat all files
 * as binary.
 *
 * Note: No dup checking is done, so multiple occurances of the same file is
 * allowed.
 */
static int
ZvfsAppendObjCmd(
    void *NotUsed,		/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    Tcl_Obj *zArchiveObj;
    Tcl_Channel chan;
    ZFile *pList = NULL, *pToc;
    int rc = TCL_OK, i;

    /*
     * Open the archive and read the table of contents
     */

    if (objc<2 || (objc&1)!=0) {
	Tcl_WrongNumArgs(interp, 1, objv, "ARCHIVE (SRC DEST)+");
	return TCL_ERROR;
    }

    zArchiveObj=objv[1];
    chan = Tcl_FSOpenFileChannel(interp, zArchiveObj, "r+", 0644);
    if (chan == 0) {
	chan = Tcl_FSOpenFileChannel(interp, zArchiveObj, "w+", 0644);
	if (chan == 0) {
	    return TCL_ERROR;
	}
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")){
	/* this should never happen */
	Tcl_Close(0, chan);
	return TCL_ERROR;
    }

    if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
	/* Null file is ok, we're creating new one. */
    } else {
	Tcl_Seek(chan, 0, SEEK_SET);
	if (ZvfsReadTOC(interp, chan, &pList) == TCL_ERROR) {
	    deleteZFileList(pList);
	    Tcl_Close(interp, chan);
	    return TCL_ERROR;
	}
	rc = TCL_OK;
    }

    /*
     * Move the file pointer to the start of the table of contents.
     */
    for (pToc=pList; pToc; pToc=pToc->pNext) {
	if (pToc->isSpecial && strcmp(pToc->zName, "*TOC*") == 0) {
	    break;
	}
    }
    if (pToc) {
	Tcl_Seek(chan, pToc->iOffset, SEEK_SET);
    } else {
	Tcl_Seek(chan, 0, SEEK_END);
    }

    /*
     * Add new files to the end of the archive.
     */

    for (i=2; rc==TCL_OK && i<objc; i+=2) {
	Tcl_Obj *zSrcObj=objv[i];
	Tcl_Obj *zDestObj=objv[i+1];
	Tcl_Channel in;

	/*
	 * Open the file that is to be added to the ZIP archive
	 */

	in = Tcl_FSOpenFileChannel(interp, zSrcObj, "r", 0);
	if (in == 0) {
	    break;
	}
	if (Tcl_SetChannelOption(interp, in, "-translation", "binary")
		|| Tcl_SetChannelOption(interp, in, "-encoding", "binary")){
	    /* this should never happen */
	    Tcl_Close(0, in);
	    rc = TCL_ERROR;
	    break;
	}

	rc = writeFile(interp, chan, in, zSrcObj, zDestObj, &pList);
    }

    /*
     * Write the table of contents at the end of the archive.
     */

    if (rc == TCL_OK) {
	pList = sortZFiles(pList);
	writeTOC(chan, pList);
    }

    /*
     * Close the channel and exit
     */

    deleteZFileList(pList);
    Tcl_Close(interp, chan);
    return rc;
}

static const char *
GetExtension(
    const char *name)
{
    const char *p, *lastSep;

#ifdef __WIN32__
    lastSep = NULL;
    for (p = name; *p != '\0'; p++) {
	if (strchr("/\\:", *p) != NULL) {
	    lastSep = p;
	}
    }
#else
    lastSep = strrchr(name, '/');
#endif

    p = strrchr(name, '.');
    if ((p != NULL) && (lastSep != NULL) && (lastSep > p)) {
	p = NULL;
    }
    return p;
}

/*
 * Implementation of the zvfs::add command.
 *
 *    zvfs::add ?-fconfigure optpairs? ARCHIVE FILE1 FILE2 ... 
 *
 * This command is similar to append in that it adds files to the zip archive
 * named ARCHIVE, however file names are relative the current directory.  In
 * addition, fconfigure is used to apply option pairs to set upon opening of
 * each file.  Otherwise, default translation is allowed for those file
 * extensions listed in the ::zvfs::auto_ext var.  Binary translation will be
 * used for unknown extensions.
 *
 * NOTE Use '-fconfigure {}' to use auto translation for all.
 */
static int
ZvfsAddObjCmd(
    void *NotUsed,		/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    Tcl_Obj *zArchiveObj;
    Tcl_Channel chan;
    ZFile *pList = NULL, *pToc;
    int rc = TCL_OK, i, j, oLen;
    char *zOpts = NULL;
    Tcl_Obj *confOpts = NULL;
    int tobjc;
    Tcl_Obj **tobjv;
    Tcl_Obj *varObj = NULL;

    /*
     * Open the archive and read the table of contents
     */

    if (objc > 3) {
	zOpts = Tcl_GetStringFromObj(objv[1], &oLen);
	if (!strncmp("-fconfigure", zOpts, oLen)) {
	    confOpts = objv[2];
	    if (TCL_OK != Tcl_ListObjGetElements(interp, confOpts,
		    &tobjc, &tobjv) || (tobjc%2)) {
		return TCL_ERROR;
	    }
	    objc -= 2;
	    objv += 2;
	}
    }
    if (objc == 2) {
	return TCL_OK;
    }

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?-fconfigure OPTPAIRS? ARCHIVE FILE1 FILE2 ..");
	return TCL_ERROR;
    }

    zArchiveObj = objv[1];
    chan = Tcl_FSOpenFileChannel(interp, zArchiveObj, "r+", 0644);
    if (chan == 0) {
	chan = Tcl_FSOpenFileChannel(interp, zArchiveObj, "w+", 0644);
	if (chan == 0) {
	    return TCL_ERROR;
	}
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")){
	/* this should never happen */
	Tcl_Close(0, chan);
	return TCL_ERROR;
    }

    if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
	/* Null file is ok, we're creating new one. */
    } else {
	Tcl_Seek(chan, 0, SEEK_SET);
	if (ZvfsReadTOC(interp, chan, &pList) == TCL_ERROR) {
	    deleteZFileList(pList);
	    Tcl_Close(interp, chan);
	    return TCL_ERROR;
	}
	rc = TCL_OK;
    }

    /*
     * Move the file pointer to the start of the table of contents.
     */

    for (pToc=pList; pToc; pToc=pToc->pNext) {
	if (pToc->isSpecial && strcmp(pToc->zName, "*TOC*") == 0) {
	    break;
	}
    }
    if (pToc) {
	Tcl_Seek(chan, pToc->iOffset, SEEK_SET);
    } else {
	Tcl_Seek(chan, 0, SEEK_END);
    }

    /*
     * Add new files to the end of the archive.
     */

    for (i=2; rc==TCL_OK && i<objc; i++) {
	Tcl_Obj *zSrcObj=objv[i];
	char *zSrc = Tcl_GetString(zSrcObj);
	Tcl_Channel in;

	/*
	 * Open the file that is to be added to the ZIP archive
	 */

	in = Tcl_FSOpenFileChannel(interp, zSrcObj, "r", 0);
	if (in == 0) {
	    break;
	}
	if (confOpts == NULL) {
	    const char *ext = GetExtension(zSrc);

	    if (ext != NULL) {
		/* Use auto translation for known text files. */
		if (varObj == NULL) {
		    varObj = Tcl_GetVar2Ex(interp, "::zvfs::auto_ext", NULL,
			    TCL_GLOBAL_ONLY);
		}
		if (varObj && TCL_OK != Tcl_ListObjGetElements(interp, varObj,
			&tobjc, &tobjv)) {
		    for (j=0; j<tobjc; j++) {
			if (!strcmp(ext, Tcl_GetString(tobjv[j]))) {
			    break;
			}
		    }
		    if (j >= tobjc) {
			ext = NULL;
		    }
		}
	    }
	    if (ext == NULL) {
		if (Tcl_SetChannelOption(interp, in, "-translation", "binary")
			|| Tcl_SetChannelOption(interp, in, "-encoding",
				"binary")) {
		    /* this should never happen */
		    Tcl_Close(0, in);
		    rc = TCL_ERROR;
		    break;
		}
	    }
	} else {
	    for (j=0; j<tobjc; j+=2) {
		if (Tcl_SetChannelOption(interp, in, Tcl_GetString(tobjv[j]),
			Tcl_GetString(tobjv[j+1]))) {
		    Tcl_Close(0, in);
		    rc = TCL_ERROR;
		    break;
		}
	    }
	}
	if (rc == TCL_OK) {
	    rc = writeFile(interp, chan, in, zSrcObj, zSrcObj, &pList);
	}
    }

    /*
     * Write the table of contents at the end of the archive.
     */
    if (rc == TCL_OK) {
	pList = sortZFiles(pList);
	writeTOC(chan, pList);
    }

    /*
     * Close the channel and exit
     */

    deleteZFileList(pList);
    Tcl_Close(interp, chan);
    return rc;
}

/*
 * Implementation of the zvfs::start command.
 *
 *    zvfs::start ARCHIVE
 *
 * This command strips returns the offset of zip data.
 */
static int
ZvfsStartObjCmd(
    void *NotUsed,		/* Client data for this command */
    Tcl_Interp *interp,		/* The interpreter used to report errors */
    int objc,			/* Number of arguments */
    Tcl_Obj *const* objv)	/* Values of all arguments */
{
    Tcl_Obj *zArchiveObj;
    Tcl_Channel chan;
    ZFile *pList = NULL;
    int zipStart;

    /*
     * Open the archive and read the table of contents
     */

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "ARCHIVE");
	return TCL_ERROR;
    }
    zArchiveObj=objv[1];
    chan = Tcl_FSOpenFileChannel(interp, zArchiveObj, "r", 0644);
    if (chan == 0) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")){
	/* this should never happen */
	Tcl_Close(0, chan);
	return TCL_ERROR;
    }

    if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
	return TCL_OK;
    }

    Tcl_Seek(chan, 0, SEEK_SET);
    if (ZvfsReadTOCStart(interp, chan, &pList, &zipStart) != TCL_OK) {
	deleteZFileList(pList);
	Tcl_Close(interp, chan);
	Tcl_AppendResult(interp, "not an archive", 0);
	return TCL_ERROR;
    }

    /*
     * Close the channel and exit
     */

    deleteZFileList(pList);
    Tcl_Close(interp, chan);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(zipStart));
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
