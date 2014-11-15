/*
** Copyright (c) 2000 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** You should have received a copy of the GNU General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*************************************************************************
** A ZIP archive virtual filesystem for Tcl.
**
** This package of routines enables Tcl to use a Zip file as
** a virtual file system.  Each of the content files of the Zip
** archive appears as a real file to Tcl.
**
** Modified to use Tcl VFS hooks by Peter MacDonald
**   peter@pdqi.com
**   http://pdqi.com
**
** @(#) $Id: zvfs.c,v 1.1.1.1 2002/01/27 17:44:02 cvs Exp $
** 
**   Revison  Date           Author             Description
**   -------  -------------  -----------------  ----------------------------------------------
**            Jan  8, 2006   Dennis R. LaBelle  Modified to support encrypted files
**
**            Dec 16, 2009   Dennis R. LaBelle  Corrected Tobe_FSMatchInDirectoryProc() for
**                                              proper operation of glob command on ZVFS files
**                                              under TCL 8.5.
**	      Oct 19, 2014   Sean D. Woods      Corrected Tobe_FSMatchInDirectoryProc() to work around
**						issues resolving global file paths under Windows.
**						Wrapped FreeWrap specific calls inside of macros.
**						Wrapped calls that implement encryption inside of macros. (The supporting
**						library for this is part of Zip, and not distributed with Tcl.)
**						Reconciled this edition of Zvfs with parallel work on the Odie project.
*/

#include <ctype.h>
#include <zlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include "tcl.h"

TCL_DECLARE_MUTEX(ArchiveFileAccess)

#undef ZIPVFSCRYPT
#ifdef ZIPVFSCRYPT
/* Some modifications to support encrypted files */
#define update_keys    zp_update_keys
#define init_keys      zp_init_keys
#define decrypt_byte   zp_decrypt_byte

/* some prototype definitions */
extern void init_keys(char *pwd);
extern int update_keys(int c);
extern unsigned char decrypt_byte();
extern char *getPwdKey(char *keybuf);
extern const unsigned long *crc_32_tab;
#endif

/* End of modifications to support encrypted files. */

/*
** Size of the decompression input buffer
*/
#define COMPR_BUF_SIZE   32768
static int openarch = 0;	/* Set to 1 when opening archive. */
#ifdef __WIN32__
static int maptolower=0;
#endif
/*
** All static variables are collected into a structure named "local".
** That way, it is clear in the code when we are using a static
** variable because its name begins with "local.".
*/
static struct {
  Tcl_HashTable fileHash;     /* One entry for each file in the ZVFS.  The
                              ** The key is the virtual filename.  The data
                              ** an an instance of the ZvfsFile structure. */
  Tcl_HashTable archiveHash;  /* One entry for each archive.  Key is the name. 
                              ** data is the ZvfsArchive structure */
  int isInit;                 /* True after initialization */
  char *firstMount;		/* The path to to the first mounted file. */
} local;

/*
** Each ZIP archive file that is mounted is recorded as an instance
** of this structure
*/
typedef struct ZvfsArchive {
  char *zName;              /* Name of the archive */
  char *zMountPoint;        /* Where this archive is mounted */
  struct ZvfsFile *pFiles;  /* List of files in that archive */
} ZvfsArchive;

/*
** Particulars about each virtual file are recorded in an instance
** of the following structure.
*/
typedef struct ZvfsFile {
  char *zName;              /* The full pathname of the virtual file */
  ZvfsArchive *pArchive;    /* The ZIP archive holding this file data */
  int iOffset;              /* Offset into the ZIP archive of the data */
  int nByte;                /* Uncompressed size of the virtual file */
  int nByteCompr;           /* Compressed size of the virtual file */
  int isdir;		    /* Set to 1 if directory */
  int depth; 		    /* Number of slashes in path. */
  int timestamp;            /* Modification time */
  int permissions;		/* File permissions. */
  struct ZvfsFile *pNext;      /* Next file in the same archive */
  struct ZvfsFile *pNextName;  /* A doubly-linked list of files with the same */
  struct ZvfsFile *pPrevName;  /*  name.  Only the first is in local.fileHash */
  /* The following would be used for writable zips.                           */
  int nExtra;               /* Extra space in the TOC header */
  int isSpecial;            /* Not really a file in the ZIP archive */
  int dosTime;              /* Modification time (DOS format) */
  int dosDate;              /* Modification date (DOS format) */
  int iCRC;                 /* Cyclic Redundancy Check of the data */
} ZvfsFile;

/*
** Macros to read 16-bit and 32-bit big-endian integers into the
** native format of this local processor.  B is an array of
** characters and the integer begins at the N-th character of
** the array.
*/
#define INT16(B, N) (B[N] + (B[N+1]<<8))
#define INT32(B, N) (INT16(B,N) + (B[N+2]<<16) + (B[N+3]<<24))


/* Convert DOS time to unix time. */
static time_t DosTimeDate(int dosDate, int dosTime){
  time_t now;
  struct tm *tm;
  now=time(NULL);
  tm = localtime(&now);
  tm->tm_year=(((dosDate&0xfe00)>>9) + 80);
  tm->tm_mon=((dosDate&0x1e0)>>5)-1;
  tm->tm_mday=(dosDate & 0x1f);
  tm->tm_hour=(dosTime&0xf800)>>11;
  tm->tm_min=(dosTime&0x7e)>>5;
  tm->tm_sec=(dosTime&0x1f);
  return mktime(tm);
}

/* Return count of char ch in str */
static int strchrcnt(char *str, char ch) {
  int cnt=0;
  char *cp=str;
  while ((cp=strchr(cp,ch))) { cp++; cnt++; }
  return cnt;
}

/*
** Concatenate zTail onto zRoot to form a pathname.  zRoot will begin
** with "/".  After concatenation, simplify the pathname be removing
** unnecessary ".." and "." directories.  Under windows, make all
** characters lower case.
**
** Resulting pathname is returned.  Space to hold the returned path is
** obtained from Tcl_Alloc() and should be freed by the calling function.
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

/*
** Read a ZIP archive and make entries in the virutal file hash table for all
** content files of that ZIP archive.  Also initialize the ZVFS if this
** routine has not been previously called.
*/
int Zvfs_Mount(
  Tcl_Interp *interp,    /* Leave error messages in this interpreter */
  const char *zArchive,        /* The ZIP archive file */
  const char *zMountPoint      /* Mount contents at this directory */
) {
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
	Tcl_Free((char *)zTrueName);
	return TCL_OK;
    }
    Tcl_MutexLock(&ArchiveFileAccess);
    chan = Tcl_OpenFileChannel(interp, zArchive, "r", 0);
    if (!chan) {
        Tcl_MutexUnlock(&ArchiveFileAccess);
        return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK){
	goto closeReleaseDie;
    }
    if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary") != TCL_OK) {
	goto closeReleaseDie;
    }

    /*
     * Read the "End Of Central Directory" record from the end of the ZIP
     * archive.
     */

    iPos = Tcl_Seek(chan, -22, SEEK_END);
    Tcl_Read(chan, (char *) zBuf, 22);
    if (memcmp(zBuf, "\120\113\05\06", 4)) {
	if(interp) Tcl_AppendResult(interp, "not a ZIP archive", NULL);
        goto closeReleaseDie;
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
        goto closeReleaseDie;
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
    Tcl_MutexUnlock(&ArchiveFileAccess);
    return TCL_OK;

closeReleaseDie:
  Tcl_Close(interp, chan);
  Tcl_MutexUnlock(&ArchiveFileAccess);
  return TCL_ERROR;
}

/*
** Locate the ZvfsFile structure that corresponds to the file named.
** Return NULL if there is no such ZvfsFile.
*/
static ZvfsFile *ZvfsLookup(char *zFilename){
  char *zTrueName;
  Tcl_HashEntry *pEntry;
  ZvfsFile *pFile;

  if( local.isInit==0 ) return 0;
  zTrueName = AbsolutePath(zFilename);
  pEntry = Tcl_FindHashEntry(&local.fileHash, zTrueName);
  pFile = pEntry ? Tcl_GetHashValue(pEntry) : 0;
  Tcl_Free(zTrueName);
  return pFile;
}

/*
** Unmount all the files in the given ZIP archive.
*/
static int Zvfs_Unmount(
    const char *zArchive
) {
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
    Tcl_MutexLock(&ArchiveFileAccess);
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
    Tcl_MutexUnlock(&ArchiveFileAccess);
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
    return Zvfs_Mount(interp, objc>1?Tcl_GetString(objv[1]):NULL, objc>2?Tcl_GetString(objv[2]):NULL);
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
    if (Zvfs_Unmount(zFilename)) {
	return TCL_OK;
    }
    pEntry = Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
    while (pEntry) {
	pArchive = Tcl_GetHashValue(pEntry);
	if (pArchive && pArchive->zMountPoint[0]
		&& (strcmp(pArchive->zMountPoint, zFilename) == 0)) {
	    if (Zvfs_Unmount(pArchive->zName)) {
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
** Whenever a ZVFS file is opened, an instance of this structure is
** attached to the open channel where it will be available to the
** ZVFS I/O routines below.  All state information about an open
** ZVFS file is held in this structure.
*/
typedef struct ZvfsChannelInfo {
  unsigned long nByte;      /* number of bytes of uncompressed data */
  unsigned long nByteCompr; /* number of bytes of unread compressed data */
  unsigned long nData;      /* total number of bytes of compressed data */
  unsigned long readSoFar;  /* position of next byte to be read from the channel */
  long startOfData;         /* File position of start of data in ZIP archive */
  Tcl_Channel chan;         /* Open file handle to the archive file */
  unsigned char *zBuf;      /* buffer used by the decompressor */
  unsigned char *uBuf;	    /* pointer to the uncompressed, unencrypted data */
  z_stream stream;          /* state of the decompressor */
  int isEncrypted;          /* file is encrypted */
  int isCompressed;         /* True data is compressed */
} ZvfsChannelInfo;

/*
** This routine is called as an exit handler.  If we do not set
** ZvfsChannelInfo.chan to NULL, then Tcl_Close() will be called on
** that channel a second time when Tcl_Exit runs.  This will lead to a 
** core dump.
*/
static void vfsExit(void *pArg){
  ZvfsChannelInfo *pInfo = (ZvfsChannelInfo*)pArg;
  pInfo->chan = 0;
}

/*
** This routine is called when the ZVFS channel is closed
*/
static int vfsClose(
  ClientData  instanceData,    /* A ZvfsChannelInfo structure */
  Tcl_Interp *interp           /* The TCL interpreter */
){
  ZvfsChannelInfo* pInfo = (ZvfsChannelInfo*)instanceData;

  if( pInfo->zBuf ){
    Tcl_Free((char *)pInfo->zBuf);
    Tcl_Free((char *)pInfo->uBuf);
    inflateEnd(&pInfo->stream);
  }
  if( pInfo->chan ){
    Tcl_Close(interp, pInfo->chan);
    Tcl_DeleteExitHandler(vfsExit, pInfo);
  }
  Tcl_MutexUnlock(&ArchiveFileAccess);
  Tcl_Free((char*)pInfo);
  return TCL_OK;
}

static int vfsInput (
  ClientData instanceData, /* The channel to read from */
  char *buf,               /* Buffer to fill */
  int toRead,              /* Requested number of bytes */
  int *pErrorCode          /* Location of error flag */
){ /* The TCL I/O system calls this function to actually read information
    * from a ZVFS file.
    */
  ZvfsChannelInfo* pInfo = (ZvfsChannelInfo*) instanceData;
  unsigned long nextpos;

  nextpos = pInfo->readSoFar + toRead;
  if (nextpos > pInfo->nByte) {
	  toRead = pInfo->nByte - pInfo->readSoFar;
	  nextpos = pInfo->nByte;
  }
  if( toRead == 0 )
    return 0;

  memcpy(buf, pInfo->uBuf + pInfo->readSoFar, toRead);

  pInfo->readSoFar = nextpos;
  *pErrorCode = 0;
  
  return toRead;
}


static int vfsRead (
  ClientData instanceData, /* The channel to read from */
  char *buf,               /* Buffer to fill */
  int toRead,              /* Requested number of bytes */
  int *pErrorCode          /* Location of error flag */
  ){ /* Read and decompress all data for the associated file into the specified buffer */
  ZvfsChannelInfo* pInfo = (ZvfsChannelInfo*) instanceData;
  int len;
#ifdef ZIPVFSCRYPT
  unsigned char encryptHdr[12];
  int C;
  int temp;
  int i;
  char pwdbuf[20];
#endif

  if( (unsigned long)toRead > pInfo->nByte ){
    toRead = pInfo->nByte;
  }
  if( toRead == 0 ){
    return 0;
  }
  if (pInfo->isEncrypted) {
#ifdef ZIPVFSCRYPT

      /* Make preparations to decrypt the data. */

	  /* Read and decrypt the encryption header. */
      crc_32_tab = get_crc_table();
	  init_keys(getPwdKey(pwdbuf));
      len = Tcl_Read(pInfo->chan, encryptHdr, sizeof(encryptHdr));
      if (len == sizeof(encryptHdr)) {
		  for (i = 0; i < sizeof(encryptHdr); ++i) {
				C = encryptHdr[i] ^ decrypt_byte();
				update_keys(C);
		  }

	  }
#endif

	}
  if( pInfo->isCompressed ){
    int err = Z_OK;
    z_stream *stream = &pInfo->stream;
    stream->next_out = (unsigned char *)buf;
    stream->avail_out = toRead;
    while (stream->avail_out) {
      if (!stream->avail_in) {
        len = pInfo->nByteCompr;
        if (len > COMPR_BUF_SIZE) {
          len = COMPR_BUF_SIZE;
        }
        len = Tcl_Read(pInfo->chan, (char *)pInfo->zBuf, len);
#ifdef ZIPVFSCRYPT

		if (pInfo->isEncrypted) {
			/* Decrypt the bytes we have just read. */
			for (i = 0; i < len; ++i) {
				C = pInfo->zBuf[i];
			    temp = C ^ decrypt_byte();
				update_keys(temp);
				pInfo->zBuf[i] = temp;
			}
		}
#endif
        pInfo->nByteCompr -= len;
        stream->next_in = pInfo->zBuf;
        stream->avail_in = len;
      }
      err = inflate(stream, Z_NO_FLUSH);
      if (err) break;
    }
    if (err == Z_STREAM_END) {
      if ((stream->avail_out != 0)) {
        *pErrorCode = err; /* premature end */
        return -1;
      }
    }else if( err ){
      *pErrorCode = err; /* some other zlib error */
      return -1;
    }
  }else{
    toRead = Tcl_Read(pInfo->chan, buf, toRead);
#ifdef ZIPVFSCRYPT
	if (pInfo->isEncrypted) {
		/* Decrypt the bytes we have just read. */
		for (i = 0; i < toRead; ++i) {
			C = buf[i];
		    temp = C ^ decrypt_byte();
			update_keys(temp);
			buf[i] = temp;
		}
	}
#endif
  }
  pInfo->nByte = toRead;
  pInfo->readSoFar = 0;
  *pErrorCode = 0;
  return toRead;
}

/*
** Write to a ZVFS file.  ZVFS files are always read-only, so this routine
** always returns an error.
*/
static int vfsOutput(
  ClientData instanceData,   /* The channel to write to */
  CONST char *buf,                 /* Data to be stored. */
  int toWrite,               /* Number of bytes to write. */
  int *pErrorCode            /* Location of error flag. */
){
  *pErrorCode = EINVAL;
  return -1;
}

static int vfsSeek(
  ClientData instanceData,    /* The file structure */
  long offset,                /* Offset to seek to */
  int mode,                   /* One of SEEK_CUR, SEEK_SET or SEEK_END */
  int *pErrorCode             /* Write the error code here */
){ /* Move the file pointer so that the next byte read will be "offset". */

  ZvfsChannelInfo* pInfo = (ZvfsChannelInfo*) instanceData;

  switch( mode ){
    case SEEK_CUR: {
      offset += pInfo->readSoFar;
      break;
    }
    case SEEK_END: {
      offset += pInfo->nByte - 1;
      break;
    }
    default: {
      /* Do nothing */
      break;
    }
  }
/* Don't seek past end of data */
if (pInfo->nByte < (unsigned long)offset)
    return -1;

/* Don't seek before the start of data */
if (offset < 0)
    return -1;

pInfo->readSoFar = (unsigned long)offset;
return pInfo->readSoFar;
}

/*
** Handle events on the channel.  ZVFS files do not generate events,
** so this is a no-op.
*/
static void vfsWatchChannel(
  ClientData instanceData,   /* Channel to watch */
  int mask                   /* Events of interest */
){
  return;
}

/*
** Called to retrieve the underlying file handle for this ZVFS file.
** As the ZVFS file has no underlying file handle, this is a no-op.
*/
static int vfsGetFile(
  ClientData  instanceData,   /* Channel to query */
  int direction,              /* Direction of interest */
  ClientData* handlePtr       /* Space to the handle into */
){
  return TCL_ERROR;
}

/*
** This structure describes the channel type structure for 
** access to the ZVFS.
*/
static Tcl_ChannelType vfsChannelType = {
  "vfs",		/* Type name.                                    */
  NULL,			/* Set blocking/nonblocking behaviour. NULL'able */
  vfsClose,		/* Close channel, clean instance data            */
  vfsInput,		/* Handle read request                           */
  vfsOutput,		/* Handle write request                          */
  vfsSeek,		/* Move location of access point.      NULL'able */
  NULL,			/* Set options.                        NULL'able */
  NULL,			/* Get options.                        NULL'able */
  vfsWatchChannel,	/* Initialize notifier                           */
  vfsGetFile		/* Get OS handle from the channel.               */
};

/*
** This routine attempts to do an open of a file.  Check to see
** if the file is located in the ZVFS.  If so, then open a channel
** for reading the file.  If not, return NULL.
*/
static Tcl_Channel ZvfsFileOpen(
  Tcl_Interp *interp,     /* The TCL interpreter doing the open */
  char *zFilename,        /* Name of the file to open */
  char *modeString,       /* Mode string for the open (ignored) */
  int permissions         /* Permissions for a newly created file (ignored) */
){
  ZvfsFile *pFile;
  ZvfsChannelInfo *pInfo;
  Tcl_Channel chan;
  static int count = 1;
  char zName[50];
  unsigned char zBuf[50];
  int errCode;

  pFile = ZvfsLookup(zFilename);
  if( pFile==0 ) {
    return NULL;
  }
  openarch = 1;
  
  Tcl_MutexLock(&ArchiveFileAccess);
  chan = Tcl_OpenFileChannel(interp, pFile->pArchive->zName, "r", 0);
  openarch = 0;
  
  if (local.firstMount == NULL) {
	local.firstMount = pFile->pArchive->zName;
  }
  if( chan==0 ){
    Tcl_MutexUnlock(&ArchiveFileAccess);
    return 0;
  }
  if(  Tcl_SetChannelOption(interp, chan, "-translation", "binary")
    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
  ){
    /* this should never happen */
    goto closeReleaseDie;
  }
  Tcl_Seek(chan, pFile->iOffset, SEEK_SET);
  Tcl_Read(chan, (char *)zBuf, 30);
  if( memcmp(zBuf, "\120\113\03\04", 4) ){
    if( interp ){
      Tcl_AppendResult(interp, "local header mismatch: ", NULL);
    }
    goto closeReleaseDie;
  }
  pInfo = (ZvfsChannelInfo*)Tcl_Alloc( sizeof(*pInfo) );
  pInfo->chan = chan;
  Tcl_CreateExitHandler(vfsExit, pInfo);
#ifdef ZIPVFSCRYPT
  pInfo->isEncrypted = zBuf[6] & 1;
  if (pFile->pArchive->zName == local.firstMount) {
	/* FreeWrap specific.
	   We are opening a file from the executable.
	   All such files must be encrypted.
      */
	if (!pInfo->isEncrypted) {
	    /* The file is not encrypted.
		 Someone must have tampered with the application.
		 Let's exit the program.
	    */
		 printf("This application has an unauthorized modification. Exiting immediately\n");
		 exit(-10);
	   }
     }
#endif
  pInfo->isCompressed = INT16(zBuf, 8);
  if (pInfo->isCompressed ){
      z_stream *stream = &pInfo->stream;
      pInfo->zBuf = (void *)Tcl_Alloc(COMPR_BUF_SIZE);
      stream->zalloc = (alloc_func)0;
      stream->zfree = (free_func)0;
      stream->opaque = (voidpf)0;
      stream->avail_in = 2;
      stream->next_in = pInfo->zBuf;
      pInfo->zBuf[0] = 0x78;
      pInfo->zBuf[1] = 0x01;
      inflateInit(&pInfo->stream);
  } else {
        pInfo->zBuf = 0;
  }
  pInfo->nByte = INT32(zBuf,22);
  pInfo->nByteCompr = pInfo->nData = INT32(zBuf,18);
  pInfo->readSoFar = 0;
  Tcl_Seek(chan, INT16(zBuf,26)+INT16(zBuf,28), SEEK_CUR);
  pInfo->startOfData = Tcl_Tell(chan);
  sprintf(zName,"vfs_%x_%x",((int)pFile)>>12,count++);
  chan = Tcl_CreateChannel(&vfsChannelType, zName, 
                           (ClientData)pInfo, TCL_READABLE);

  pInfo->uBuf = (void *)Tcl_Alloc(pInfo->nByte);
  /* Read and decompress the file contents */
  if (pInfo->uBuf) {
    pInfo->uBuf[0] = 0;
    vfsRead(pInfo, (char *)pInfo->uBuf, pInfo->nByte, &errCode);
    pInfo->readSoFar = 0;
  }

  return chan;

closeReleaseDie:
  Tcl_Close(interp, chan);
  Tcl_MutexUnlock(&ArchiveFileAccess);
  return NULL;

}

Tcl_Channel Tobe_FSOpenFileChannelProc (
  Tcl_Interp *interp, 
  Tcl_Obj *pathPtr,
  int mode, 
  int permissions
) {
  int len;
  /* if (mode != O_RDONLY) return NULL; */
  return ZvfsFileOpen(interp, Tcl_GetStringFromObj(pathPtr,&len), 0,
    permissions);
}

/*
** This routine does a stat() system call for a ZVFS file.
*/
int Tobe_FSStatProc (
   Tcl_Obj *pathPtr, 
   Tcl_StatBuf *buf
) {
    char *path=Tcl_GetString(pathPtr);
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
** This routine does an access() system call for a ZVFS file.
*/
int Tobe_FSAccessProc (Tcl_Obj *pathPtr, int mode) {
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

Tcl_Obj* Tobe_FSFilesystemSeparatorProc(Tcl_Obj *pathPtr) { 
  return Tcl_NewStringObj("/",-1);;
}

/* Function to process a 
* 'Tobe_FSMatchInDirectory()'.  If not
* implemented, then glob and recursive
* copy functionality will be lacking in
* the filesystem. */
int Tobe_FSMatchInDirectoryProc (
    Tcl_Interp* interp,
    Tcl_Obj *result,
    Tcl_Obj *pathPtr,
    const char *pattern,
    Tcl_GlobTypeData * types
) {
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch sSearch;
    int scnt=0, pathlen=0, patternlen=0, dirglob=0, fileglob=0, mntglob=0;
    int nullpattern=(pattern == NULL || (*pattern=='\0'));
    char *zPattern = NULL;
    char *zp=NULL;
    
    if(types && types->type) {
      dirglob = (types->type&TCL_GLOB_TYPE_DIR);
      fileglob = (types->type & TCL_GLOB_TYPE_FILE);
      mntglob = (types->type&TCL_GLOB_TYPE_MOUNT);
    }
    if(!nullpattern) {
      patternlen=strlen(pattern);
    }
    if(pathPtr) {
      zp=Tcl_GetStringFromObj(pathPtr,&pathlen);
#ifdef __WIN32__
      if (isalpha(zp[0]) && zp[1]==':') {
	zp+=2;
      }
#endif
    }
    if(mntglob) { return TCL_OK; }
    if (mntglob) {
      /* Look for a directory mount */
      Tcl_HashEntry *pEntry;     /* Hash table entry */
      Tcl_HashSearch zSearch;   /* Search all mount points */
      ZvfsArchive *pArchive;     /* The ZIP archive being mounted */
      char mountpt[200];
      int i=1;
      mountpt[0]='/';
      if(pathPtr) {
        for(i=1;i<pathlen && i<200;i++) {
          if(zp[i]=='/' || zp[i]=='\0') break;
          mountpt[i]=zp[i];
        }
      }
      mountpt[i]='\0';
      for(
        pEntry=Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
        pEntry;
        pEntry=Tcl_NextHashEntry(&zSearch)
      ) {
        pArchive = Tcl_GetHashValue(pEntry);
        if (pArchive) {
          char *z = pArchive->zMountPoint;
          Tcl_Obj *pTail=NULL;
          int match=0;
          match=(strcmp(mountpt, z)==0);
          if(!match) continue;
          pTail=Tcl_NewStringObj(z, -1);
          Tcl_ListObjAppendElement(interp, result, pTail);
          //Tcl_DecrRefCount(pTail);
        }
      }
    } else {
      int idx=0;
      if(!zp && nullpattern) {
        zPattern=NULL;
      } else {
	Tcl_DString dTempPath;
	Tcl_DStringInit(&dTempPath);
        zPattern=(char*)Tcl_Alloc(pathlen+patternlen+3);
        memset(zPattern,0,pathlen+patternlen+3);
        if(zp) {
	  Tcl_DStringAppend(&dTempPath,zp,-1); 
        }
        if (zp && !nullpattern) {
          if (pathlen > 1 || zPattern[0] != '/') {
	    Tcl_DStringAppend(&dTempPath,"/",-1); 
            idx++;
          }
        }
        if (!nullpattern) {
	  Tcl_DStringAppend(&dTempPath,pattern,patternlen); 
        }
	zPattern=strdup(Tcl_DStringValue(&dTempPath));
        Tcl_DStringFree(&dTempPath);
        scnt = strchrcnt(zPattern, '/');
      }      
      for (
        pEntry = Tcl_FirstHashEntry(&local.fileHash, &sSearch);
        pEntry;
        pEntry = Tcl_NextHashEntry(&sSearch)
      ){
          ZvfsFile *pFile = Tcl_GetHashValue(pEntry);
          if(pFile) {
            char *z = pFile->zName;
            Tcl_Obj *pTail=NULL;
            int match=0;
            if (dirglob && !pFile->isdir) continue;
            if (fileglob && pFile->isdir) continue;
            if(scnt != pFile->depth) continue;
            match=Tcl_StringCaseMatch(z, zPattern, 0);
            if(!match) continue;
            pTail=Tcl_NewStringObj(z, -1);
            Tcl_ListObjAppendElement(interp, result, pTail);
          }
      }
    }

    if (zPattern) {
	free(zPattern);
    }
    return TCL_OK;
}

/* Function to check whether a path is in 
* this filesystem.  This is the most
* important filesystem procedure. */
int Tobe_FSPathInFilesystemProc (
    Tcl_Obj *pathPtr,
    ClientData *clientDataPtr
) {
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

Tcl_Obj *Tobe_FSListVolumesProc (void) {
    Tcl_HashEntry *pEntry;     /* Hash table entry */
    Tcl_HashSearch zSearch;   /* Search all mount points */
    ZvfsArchive *pArchive;     /* The ZIP archive being mounted */
    Tcl_Obj *pVols=0, *pVol;
    char mountpt[200];
    
    pEntry=Tcl_FirstHashEntry(&local.archiveHash,&zSearch);
    while (pEntry) {
	pArchive = Tcl_GetHashValue(pEntry);
	if (pArchive) {
	    if (!pVols) {
		pVols=Tcl_NewListObj(0,0);
		Tcl_IncrRefCount(pVols);
	    }
	    sprintf(mountpt, "zvfs:%s", pArchive->zMountPoint);
	    pVol=Tcl_NewStringObj(mountpt,-1);
	    Tcl_IncrRefCount(pVol);
	    Tcl_ListObjAppendElement(NULL, pVols,pVol);
	    /* Tcl_AppendResult(interp,pArchive->zMountPoint," ",pArchive->zName," ",0);*/
	}
	pEntry=Tcl_NextHashEntry(&zSearch);
    }
    return pVols;
}

int Tobe_FSChdirProc (Tcl_Obj *pathPtr) {
   /* Someday, we should actually check if this is a valid path. */
   return TCL_OK;
}

const char * const*
Tobe_FSFileAttrStringsProc(
    Tcl_Obj *pathPtr,
    Tcl_Obj** objPtrRef)
{
    char *path = Tcl_GetString(pathPtr);
#ifdef __WIN32__
    static const char *attrs[] = {
	"uncompsize", "compsize", "offset", "mount", "archive","-archive", "-hidden", "-readonly", "-system", "-shortname", 0
    };
#else
    static const char *attrs[] = {
	"uncompsize", "compsize", "offset", "mount", "archive","-group", "-owner", "-permissions", 0
    };
#endif
    if (ZvfsLookup(path) == 0) {
	return NULL;
    }
    return attrs;
}

int Tobe_FSFileAttrsGetProc (
  Tcl_Interp *interp,
  int index, 
  Tcl_Obj *pathPtr,
  Tcl_Obj **objPtrRef
) {
  char *zFilename;
  ZvfsFile *pFile;
  zFilename = Tcl_GetString(pathPtr);
  pFile = ZvfsLookup(zFilename);
  if(!pFile)
	return TCL_ERROR;
  switch (index) {
     case 0:
	*objPtrRef=Tcl_NewIntObj(pFile->nByteCompr);
	return TCL_OK;
     case 1:
	*objPtrRef= Tcl_NewIntObj(pFile->nByte);
	return TCL_OK;
     case 2:
	*objPtrRef= Tcl_NewIntObj(pFile->nByte);
	return TCL_OK;
     case 3:
	*objPtrRef= Tcl_NewStringObj(pFile->pArchive->zMountPoint,-1);
	return TCL_OK;
     case 4:
	*objPtrRef= Tcl_NewStringObj(pFile->pArchive->zName,-1);
	return TCL_OK;
#ifdef __WIN32__
    case 5: /* -archive */
	*objPtrRef = Tcl_NewStringObj("0", -1); break;
    case 6: /* -hidden */
	*objPtrRef = Tcl_NewStringObj("0", -1); break;
    case 7: /* -readonly */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 8: /* -system */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 9: /* -shortname */
	*objPtrRef = Tcl_NewStringObj("", -1);
#else
    case 5: /* -group */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 6: /* -owner */
	*objPtrRef = Tcl_NewStringObj("", -1); break;
    case 7: /* -permissions */ {
      char buf[32];
      sprintf(buf, "%03o", pFile->permissions);
      *objPtrRef = Tcl_NewStringObj(buf, -1); break;
    }
#endif
     default:
        return TCL_ERROR;
  }
  return TCL_OK;
}
int Tobe_FSFileAttrsSetProc (
  Tcl_Interp *interp,
  int index,
  Tcl_Obj *pathPtr,
  Tcl_Obj *objPtr) { return TCL_ERROR; }

Tcl_Obj* Tobe_FSFilesystemPathTypeProc (Tcl_Obj *pathPtr) {
    return Tcl_NewStringObj("zip",-1);
}

/****************************************************/

static Tcl_Filesystem Tobe_Filesystem = {
    "zvfs",		     /* The name of the filesystem. */
    sizeof(Tcl_Filesystem),  /* Length of this structure, so future
                             * binary compatibility can be assured. */
    TCL_FILESYSTEM_VERSION_1,
                            /* Version of the filesystem type. */
    Tobe_FSPathInFilesystemProc,
                            /* Function to check whether a path is in 
                             * this filesystem.  This is the most
                             * important filesystem procedure. */
    NULL,
                            /* Function to duplicate internal fs rep.  May
                             * be NULL (but then fs is less efficient). */
    NULL,
                            /* Function to free internal fs rep.  Must
                             * be implemented, if internal representations
                             * need freeing, otherwise it can be NULL. */
    NULL,
                            /* Function to convert internal representation
                             * to a normalized path.  Only required if
                             * the fs creates pure path objects with no
                             * string/path representation. */
    NULL,
                            /* Function to create a filesystem-specific
                             * internal representation.  May be NULL
                             * if paths have no internal representation, 
                             * or if the Tobe_FSPathInFilesystemProc
                             * for this filesystem always immediately 
                             * creates an internal representation for 
                             * paths it accepts. */
    NULL,
                            /* Tobe_FSNormalizePathProc (Not needed)
			     * Function to normalize a path.  Should
                             * be implemented for all filesystems
                             * which can have multiple string 
                             * representations for the same path 
                             * object. */
    Tobe_FSFilesystemPathTypeProc,
                            /* Function to determine the type of a 
                             * path in this filesystem.  May be NULL. */
    Tobe_FSFilesystemSeparatorProc,
                            /* Function to return the separator 
                             * character(s) for this filesystem.  Must
                             * be implemented. */
    Tobe_FSStatProc,
                            /* 
                             * Function to process a 'Tobe_FSStat()'
                             * call.  Must be implemented for any
                             * reasonable filesystem.
                             */
    Tobe_FSAccessProc,
                            /* 
                             * Function to process a 'Tobe_FSAccess()'
                             * call.  Must be implemented for any
                             * reasonable filesystem.
                             */
    Tobe_FSOpenFileChannelProc,
                            /* 
                             * Function to process a
                             * 'Tobe_FSOpenFileChannel()' call.  Must be
                             * implemented for any reasonable
                             * filesystem.
                             */
    Tobe_FSMatchInDirectoryProc,
                            /* Function to process a 
                             * 'Tobe_FSMatchInDirectory()'.  If not
                             * implemented, then glob and recursive
                             * copy functionality will be lacking in
                             * the filesystem. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSUtime()' call.  Required to
                             * allow setting (not reading) of times 
                             * with 'file mtime', 'file atime' and
                             * the open-r/open-w/fcopy implementation
                             * of 'file copy'. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSLink()' call.  Should be
                             * implemented only if the filesystem supports
                             * links. */
    Tobe_FSListVolumesProc,
                            /* Function to list any filesystem volumes 
                             * added by this filesystem.  Should be
                             * implemented only if the filesystem adds
                             * volumes at the head of the filesystem. */
    Tobe_FSFileAttrStringsProc,
                            /* Function to list all attributes strings 
                             * which are valid for this filesystem.  
                             * If not implemented the filesystem will
                             * not support the 'file attributes' command.
                             * This allows arbitrary additional information
                             * to be attached to files in the filesystem. */
    Tobe_FSFileAttrsGetProc,
                            /* Function to process a 
                             * 'Tobe_FSFileAttrsGet()' call, used by
                             * 'file attributes'. */
    Tobe_FSFileAttrsSetProc,
                            /* Function to process a 
                             * 'Tobe_FSFileAttrsSet()' call, used by
                             * 'file attributes'.  */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSCreateDirectory()' call. Should
                             * be implemented unless the FS is
                             * read-only. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSRemoveDirectory()' call. Should
                             * be implemented unless the FS is
                             * read-only. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSDeleteFile()' call.  Should
                             * be implemented unless the FS is
                             * read-only. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSCopyFile()' call.  If not
                             * implemented Tcl will fall back
                             * on open-r, open-w and fcopy as
                             * a copying mechanism. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSRenameFile()' call.  If not
                             * implemented, Tcl will fall back on
                             * a copy and delete mechanism. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSCopyDirectory()' call.  If
                             * not implemented, Tcl will fall back
                             * on a recursive create-dir, file copy
                             * mechanism. */
    NULL,
                            /* Function to process a 
                             * 'Tobe_FSLoadFile()' call.  If not
                             * implemented, Tcl will fall back on
                             * a copy to native-temp followed by a 
                             * Tobe_FSLoadFile on that temporary copy. */
    NULL,
                            /* Function to unload a previously 
                             * successfully loaded file.  If load was
                             * implemented, then this should also be
                             * implemented, if there is any cleanup
                             * action required. */
    NULL,
                            /* 
                             * Function to process a 'Tobe_FSGetCwd()'
                             * call.  Most filesystems need not
                             * implement this.  It will usually only be
                             * called once, if 'getcwd' is called
                             * before 'chdir'.  May be NULL.
                             */
    NULL,
                            /* 
                             * Function to process a 'Tobe_FSChdir()'
                             * call.  If filesystems do not implement
                             * this, it will be emulated by a series of
                             * directory access checks.  Otherwise,
                             * virtual filesystems which do implement
                             * it need only respond with a positive
                             * return result if the dirName is a valid
                             * directory in their filesystem.  They
                             * need not remember the result, since that
                             * will be automatically remembered for use
                             * by GetCwd.  Real filesystems should
                             * carry out the correct action (i.e. call
                             * the correct system 'chdir' api).  If not
                             * implemented, then 'cd' and 'pwd' will
                             * fail inside the filesystem.
                             */
};

void (*Zvfs_PostInit)(Tcl_Interp *)=0;

int Zvfs_Common_Init(Tcl_Interp *interp) {
  if( !local.isInit ){
    /* One-time initialization of the ZVFS */
    Tcl_FSRegister(0, &Tobe_Filesystem);
    Tcl_InitHashTable(&local.fileHash, TCL_STRING_KEYS);
    Tcl_InitHashTable(&local.archiveHash, TCL_STRING_KEYS);
    local.isInit = 1;
  }
  return TCL_OK;
}

int Zvfs_Init(Tcl_Interp *interp){
#ifdef USE_TCL_STUBS
  if( Tcl_InitStubs(interp,"8.0",0)==0 ){
    return TCL_ERROR;
  }
#endif
  Tcl_PkgProvide(interp, "zvfs", "1.0");
  Zvfs_Common_Init(interp);
  Tcl_CreateObjCommand(interp, "zvfs::mount", ZvfsMountObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::unmount", ZvfsUnmountObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::exists", ZvfsExistsObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::info", ZvfsInfoObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::list", ZvfsListObjCmd, 0, 0);
  if (Zvfs_PostInit) Zvfs_PostInit(interp);
  return TCL_OK;
}

int Zvfs_SafeInit(Tcl_Interp *interp){
#ifdef USE_TCL_STUBS
  if( Tcl_InitStubs(interp,"8.0",0)==0 ){
    return TCL_ERROR;
  }
#endif
  Zvfs_Common_Init(interp);
  Tcl_PkgProvide(interp, "zvfs", "1.0");
  Tcl_CreateObjCommand(interp, "zvfs::exists", ZvfsExistsObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::info", ZvfsInfoObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "zvfs::list", ZvfsListObjCmd, 0, 0);
  if (Zvfs_PostInit) Zvfs_PostInit(interp);
  return TCL_OK;
}

