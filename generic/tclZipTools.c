/*
** Copyright (c) 2000 D. Richard Hipp
** Copyright (c) 2007 PDQ Interfaces Inc.
** Copyright (c) 2014 Sean Woods
**
** This file is now released under the BSD style license
** outlined in the included file license.terms.
**
*************************************************************************
**
** Utilities to Create, Modify and Introspect Zip files
** Originally distributed as part of ZipVfs.c, split into
** Their own file for clarity.
**
** This version has been modified to run under Tcl 8.6
**
*/
#include "tcl.h"
#include <ctype.h>
#include <zlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/*
** Size of the decompression input buffer
*/
#define COMPR_BUF_SIZE   8192
static int maptolower=0;
static int openarch=0;  /* Set to 1 when opening archive. */

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
  time_t timestamp;            /* Modification time */
  int isdir;		    /* Set to 2 if directory, or 1 if mount */
  int depth; 		    /* Number of slashes in path. */
  int permissions;          /* File permissions. */
  struct ZvfsFile *pNext;      /* Next file in the same archive */
  struct ZvfsFile *pNextName;  /* A doubly-linked list of files with the same */
  struct ZvfsFile *pPrevName;  /*  name.  Only the first is in local.fileHash */
} ZvfsFile;

/*
** Information about each file within a ZIP archive is stored in
** an instance of the following structure.  A list of these structures
** forms a table of contents for the archive.
*/
typedef struct ZFile ZFile;
struct ZFile {
  char *zName;         /* Name of the file */
  int isSpecial;       /* Not really a file in the ZIP archive */
  int dosTime;         /* Modification time (DOS format) */
  int dosDate;         /* Modification date (DOS format) */
  int iOffset;         /* Offset into the ZIP archive of the data */
  int nByte;           /* Uncompressed size of the virtual file */
  int nByteCompr;      /* Compressed size of the virtual file */
  int nExtra;          /* Extra space in the TOC header */
  int iCRC;            /* Cyclic Redundancy Check of the data */
  int permissions;     /* File permissions. */
  int flags;		    /* Deletion = bit 0. */
  ZFile *pNext;        /* Next file in the same archive */
};

/*
** Macros to read 16-bit and 32-bit big-endian integers into the
** native format of this local processor.  B is an array of
** characters and the integer begins at the N-th character of
** the array.
*/
#define INT16(B, N) (B[N] + (B[N+1]<<8))
#define INT32(B, N) (INT16(B,N) + (B[N+2]<<16) + (B[N+3]<<24))

static int ZvfsAppendObjCmd( void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsAddObjCmd( void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsDumpObjCmd( void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);
static int ZvfsStartObjCmd( void *NotUsed, Tcl_Interp *interp, int objc, Tcl_Obj *const* objv);

/*
** Write a 16- or 32-bit integer as little-endian into the given buffer.
*/
static void put16(char *z, int v){
  z[0] = v & 0xff;
  z[1] = (v>>8) & 0xff;
}
static void put32(char *z, int v){
  z[0] = v & 0xff;
  z[1] = (v>>8) & 0xff;
  z[2] = (v>>16) & 0xff;
  z[3] = (v>>24) & 0xff;
}

/*
** Make a new ZFile structure with space to hold a name of the number of
** characters given.  Return a pointer to the new structure.
*/
static ZFile *newZFile(int nName, ZFile **ppList){
  ZFile *pNew;

  pNew = (ZFile*)Tcl_Alloc( sizeof(*pNew) + nName + 1 );
  memset(pNew, 0, sizeof(*pNew));
  pNew->zName = (char*)&pNew[1];
  pNew->pNext = *ppList;
  *ppList = pNew;
  return pNew;
}

/*
** Delete an entire list of ZFile structures
*/
static void deleteZFileList(ZFile *pList){
  ZFile *pNext;
  while( pList ){
    pNext = pList->pNext;
    Tcl_Free((char*)pList);
    pList = pNext;
  }
}

/* Convert DOS time to unix time. */
static void UnixTimeDate(struct tm *tm, int *dosDate, int *dosTime){
  *dosDate = ((((tm->tm_year-80)<<9)&0xfe00) | (((tm->tm_mon+1)<<5)&0x1e0) | (tm->tm_mday&0x1f));
  *dosTime = (((tm->tm_hour<<11)&0xf800) | ((tm->tm_min<<5)&0x7e0) | (tm->tm_sec&0x1f));
}

/* Convert DOS time to unix time. */
static time_t DosTimeDate(int dosDate, int dosTime){
  time_t now;
  struct tm *tm;
  now=time(NULL);
  tm = localtime(&now);
  tm->tm_year=(((dosDate&0xfe00)>>9) + 80);
  tm->tm_mon=((dosDate&0x1e0)>>5);
  tm->tm_mday=(dosDate & 0x1f);
  tm->tm_hour=(dosTime&0xf800)>>11;
  tm->tm_min=(dosTime&0x7e0)>>5;
  tm->tm_sec=(dosTime&0x1f);
  return mktime(tm);
}

/*
** Translate a DOS time and date stamp into a human-readable string.
*/
static void translateDosTimeDate(char *zStr, int dosDate, int dosTime){
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
    dosTime&0x1f
  );
}

/* Return count of char ch in str */
static int strchrcnt(char *str, char ch) {
  int cnt=0;
  char *cp=str;
  while ((cp=strchr(cp,ch))) { cp++; cnt++; }
  return cnt;
}


int ZvfsReadTOCStart(
  Tcl_Interp *interp,    /* Leave error messages in this interpreter */
  Tcl_Channel chan,
  ZFile **pList,
  int *iStart
) {
  char *zArchiveName = 0;    /* A copy of zArchive */
  int nFile;                 /* Number of files in the archive */
  int iPos;                  /* Current position in the archive file */
  ZvfsArchive *pArchive;     /* The ZIP archive being mounted */
  Tcl_HashEntry *pEntry;     /* Hash table entry */
  int isNew;                 /* Flag to tell use when a hash entry is new */
  unsigned char zBuf[100];   /* Space into which to read from the ZIP archive */
  Tcl_HashSearch zSearch;   /* Search all mount points */
  ZFile *p;
  int zipStart;

  if (!chan) {
    return TCL_ERROR;
  }
  if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK){
    return TCL_ERROR;
  }
  if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary") != TCL_OK) {
    return TCL_ERROR;
  }

  /* Read the "End Of Central Directory" record from the end of the
  ** ZIP archive.
  */
  iPos = Tcl_Seek(chan, -22, SEEK_END);
  Tcl_Read(chan, zBuf, 22);
  if (memcmp(zBuf, "\120\113\05\06", 4)) {
    /* Tcl_AppendResult(interp, "not a ZIP archive", NULL); */
    return TCL_BREAK;
  }

  /* Compute the starting location of the directory for the ZIP archive
  ** in iPos then seek to that location.
  */
  zipStart = iPos;
  nFile = INT16(zBuf,8);
  iPos -= INT32(zBuf,12);
  Tcl_Seek(chan, iPos, SEEK_SET);

  while(1) {
    int lenName;            /* Length of the next filename */
    int lenExtra;           /* Length of "extra" data for next file */
    int iData;              /* Offset to start of file data */
    int dosTime;
    int dosDate;
    int isdir;
    ZvfsFile *pZvfs;        /* A new virtual file */
    char *zFullPath;        /* Full pathname of the virtual file */
    char zName[1024];       /* Space to hold the filename */

    if (nFile-- <= 0 ){
	break;
    }
    /* Read the next directory entry.  Extract the size of the filename,
    ** the size of the "extra" information, and the offset into the archive
    ** file of the file data.
    */
    Tcl_Read(chan, zBuf, 46);
    if (memcmp(zBuf, "\120\113\01\02", 4)) {
      Tcl_AppendResult(interp, "ill-formed central directory entry", NULL);
      return TCL_ERROR;
    }
    lenName = INT16(zBuf,28);
    lenExtra = INT16(zBuf,30) + INT16(zBuf,32);
    iData = INT32(zBuf,42);
    if (iData<zipStart) {
        zipStart = iData;
    }

    p = newZFile(lenName, pList);
    if (!p) break;

    Tcl_Read(chan, p->zName, lenName);
    p->zName[lenName] = 0;
    if (lenName>0 && p->zName[lenName-1] == '/') {
	p->isSpecial = 1;
    }
    p->dosDate = INT16(zBuf, 14);
    p->dosTime = INT16(zBuf, 12);
    p->nByteCompr = INT32(zBuf, 20);
    p->nByte = INT32(zBuf, 24);
    p->nExtra = INT32(zBuf, 28);
    p->iCRC = INT32(zBuf, 32);

    if (nFile < 0)
	break;

    /* Skip over the extra information so that the next read will be from
    ** the beginning of the next directory entry.
    */
    Tcl_Seek(chan, lenExtra, SEEK_CUR);
  }
  *iStart = zipStart;
  return TCL_OK;
}

int ZvfsReadTOC(
  Tcl_Interp *interp,    /* Leave error messages in this interpreter */
  Tcl_Channel chan,
  ZFile **pList
) {
    int iStart;
    return ZvfsReadTOCStart( interp, chan, pList, &iStart);
}



/************************************************************************/
/************************************************************************/
/************************************************************************/

/*
** Implement the zvfs::dump command
**
**    zvfs::dump  ARCHIVE
**
** Each entry in the list returned is of the following form:
**
**   {FILENAME  DATE-TIME  SPECIAL-FLAG  OFFSET  SIZE  COMPRESSED-SIZE}
**
*/
static int ZvfsDumpObjCmd(
  void *NotUsed,             /* Client data for this command */
  Tcl_Interp *interp,        /* The interpreter used to report errors */
  int objc,                  /* Number of arguments */
  Tcl_Obj *const* objv       /* Values of all arguments */
){
  char *zFilename;
  Tcl_Channel chan;
  ZFile *pList;
  int rc;
  Tcl_Obj *pResult;

  if( objc!=2 ){
    Tcl_WrongNumArgs(interp, 1, objv, "FILENAME");
    return TCL_ERROR;
  }
  zFilename = Tcl_GetString(objv[1]);
  chan = Tcl_OpenFileChannel(interp, zFilename, "r", 0);
  if( chan==0 ) return TCL_ERROR;
  rc = ZvfsReadTOC(interp, chan, &pList);
  if( rc==TCL_ERROR ){
    deleteZFileList(pList);
    return rc;
  }
  Tcl_Close(interp, chan);
  pResult = Tcl_GetObjResult(interp);
  while( pList ){
    Tcl_Obj *pEntry = Tcl_NewObj();
    ZFile *pNext;
    char zDateTime[100];
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewStringObj(pList->zName,-1));
    translateDosTimeDate(zDateTime, pList->dosDate, pList->dosTime);
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewStringObj(zDateTime, -1));
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewIntObj(pList->isSpecial));
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewIntObj(pList->iOffset));
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewIntObj(pList->nByte));
    Tcl_ListObjAppendElement(interp, pEntry, Tcl_NewIntObj(pList->nByteCompr));
    Tcl_ListObjAppendElement(interp, pResult, pEntry);
    pNext = pList->pNext;
    Tcl_Free((char*)pList);
    pList = pList->pNext;
  }
  return TCL_OK;
}

/*
** Write a file record into a ZIP archive at the current position of
** the write cursor for channel "chan".  Add a ZFile record for the file
** to *ppList.  If an error occurs, leave an error message on interp
** and return TCL_ERROR.  Otherwise return TCL_OK.
*/
static int writeFile(
  Tcl_Interp *interp,     /* Leave an error message here */
  Tcl_Channel out,        /* Write the file here */
  Tcl_Channel in,         /* Read data from this file */
  char *zSrc,             /* Name the new ZIP file entry this */
  char *zDest,            /* Name the new ZIP file entry this */
  ZFile **ppList          /* Put a ZFile struct for the new file here */
){
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
  struct stat stat;

  /* Create a new ZFile structure for this file.
   * TODO: fill in date/time etc.
  */
  nameLen = strlen(zDest);
  p = newZFile(nameLen, ppList);
  strcpy(p->zName, zDest);
  p->isSpecial = 0;
  Tcl_Stat(zSrc, &stat);
  now=stat.st_mtime;
  tm = localtime(&now);
  UnixTimeDate(tm, &p->dosDate, &p->dosTime);
  p->iOffset = Tcl_Tell(out);
  p->nByte = 0;
  p->nByteCompr = 0;
  p->nExtra = 0;
  p->iCRC = 0;
  p->permissions = stat.st_mode;

  /* Fill in as much of the header as we know.
  */
  put32(&zHdr[0], 0x04034b50);
  put16(&zHdr[4], 0x0014);
  put16(&zHdr[6], 0);
  put16(&zHdr[8], 8);
  put16(&zHdr[10], p->dosTime);
  put16(&zHdr[12], p->dosDate);
  put16(&zHdr[26], nameLen);
  put16(&zHdr[28], 0);

  /* Write the header and filename.
  */
  Tcl_Write(out, zHdr, 30);
  Tcl_Write(out, zDest, nameLen);

  /* The first two bytes that come out of the deflate compressor are
  ** some kind of header that ZIP does not use.  So skip the first two
  ** output bytes.
  */
  skip = 2;

  /* Write the compressed file.  Compute the CRC as we progress.
  */
  stream.zalloc = (alloc_func)0;
  stream.zfree = (free_func)0;
  stream.opaque = 0;
  stream.avail_in = 0;
  stream.next_in = zInBuf;
  stream.avail_out = sizeof(zOutBuf);
  stream.next_out = zOutBuf;
#if 1
  deflateInit(&stream, 9);
#else
  {
    int i, err, WSIZE = 0x8000, windowBits, level=6;
    for (i = ((unsigned)WSIZE), windowBits = 0; i != 1; i >>= 1, ++windowBits);
    err = deflateInit2(&stream, level, Z_DEFLATED, -windowBits, 8, 0);

  }
#endif
  p->iCRC = crc32(0, 0, 0);
  while( !Tcl_Eof(in) ){
    if( stream.avail_in==0 ){
      int amt = Tcl_Read(in, zInBuf, sizeof(zInBuf));
      if( amt<=0 ) break;
      p->iCRC = crc32(p->iCRC, zInBuf, amt);
      stream.avail_in = amt;
      stream.next_in = zInBuf;
    }
    deflate(&stream, 0);
    toOut = sizeof(zOutBuf) - stream.avail_out;
    if( toOut>skip ){
      Tcl_Write(out, &zOutBuf[skip], toOut - skip);
      skip = 0;
    }else{
      skip -= toOut;
    }
    stream.avail_out = sizeof(zOutBuf);
    stream.next_out = zOutBuf;
  }
  do{
    stream.avail_out = sizeof(zOutBuf);
    stream.next_out = zOutBuf;
    deflate(&stream, Z_FINISH);
    toOut = sizeof(zOutBuf) - stream.avail_out;
    if( toOut>skip ){
      Tcl_Write(out, &zOutBuf[skip], toOut - skip);
      skip = 0;
    }else{
      skip -= toOut;
    }
  }while( stream.avail_out==0 );
  p->nByte = stream.total_in;
  p->nByteCompr = stream.total_out - 2;
  deflateEnd(&stream);
  Tcl_Flush(out);

  /* Remember were we are in the file.  Then go back and write the
  ** header, now that we know the compressed file size.
  */
  iEndOfData = Tcl_Tell(out);
  Tcl_Seek(out, p->iOffset, SEEK_SET);
  put32(&zHdr[14], p->iCRC);
  put32(&zHdr[18], p->nByteCompr);
  put32(&zHdr[22], p->nByte);
  Tcl_Write(out, zHdr, 30);
  Tcl_Seek(out, iEndOfData, SEEK_SET);

  /* Close the input file.
  */
  Tcl_Close(interp, in);

  /* Finished!
  */
  return TCL_OK;
}

/*
** The arguments are two lists of ZFile structures sorted by iOffset.
** Either or both list may be empty.  This routine merges the two
** lists together into a single sorted list and returns a pointer
** to the head of the unified list.
**
** This is part of the merge-sort algorithm.
*/
static ZFile *mergeZFiles(ZFile *pLeft, ZFile *pRight){
  ZFile fakeHead;
  ZFile *pTail;

  pTail = &fakeHead;
  while( pLeft && pRight ){
    ZFile *p;
    if( pLeft->iOffset <= pRight->iOffset ){
      p = pLeft;
      pLeft = p->pNext;
    }else{
      p = pRight;
      pRight = p->pNext;
    }
    pTail->pNext = p;
    pTail = p;
  }
  if( pLeft ){
    pTail->pNext = pLeft;
  }else if( pRight ){
    pTail->pNext = pRight;
  }else{
    pTail->pNext = 0;
  }
  return fakeHead.pNext;
}

/*
** Sort a ZFile list so in accending order by iOffset.
*/
static ZFile *sortZFiles(ZFile *pList){
# define NBIN 30
  int i;
  ZFile *p;
  ZFile *aBin[NBIN+1];

  for(i=0; i<=NBIN; i++) aBin[i] = 0;
  while( pList ){
    p = pList;
    pList = p->pNext;
    p->pNext = 0;
    for(i=0; i<NBIN && aBin[i]; i++){
      p = mergeZFiles(aBin[i],p);
      aBin[i] = 0;
    }
    aBin[i] = aBin[i] ? mergeZFiles(aBin[i], p) : p;
  }
  p = 0;
  for(i=0; i<=NBIN; i++){
    if( aBin[i]==0 ) continue;
    p = mergeZFiles(p, aBin[i]);
  }
  return p;
}

/*
** Write a ZIP archive table of contents to the given
** channel.
*/
static void writeTOC(Tcl_Channel chan, ZFile *pList){
  int iTocStart, iTocEnd;
  int nEntry = 0;
  int i;
  char zBuf[100];

  iTocStart = Tcl_Tell(chan);
  for(; pList; pList=pList->pNext){
    if( pList->isSpecial ) continue;
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
    for(i=pList->nExtra; i>0; i-=40){
      int toWrite = i<40 ? i : 40;
      Tcl_Write(chan,"                                             ",toWrite);
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
** Implementation of the zvfs::append command.
**
**    zvfs::append ARCHIVE (SOURCE DESTINATION)*
**
** This command reads SOURCE files and appends them (using the name
** DESTINATION) to the zip archive named ARCHIVE.  A new zip archive
** is created if it does not already exist.  If ARCHIVE refers to a
** file which exists but is not a zip archive, then this command
** turns ARCHIVE into a zip archive by appending the necessary
** records and the table of contents.  Treat all files as binary.
**
** Note: No dup checking is done, so multiple occurances of the
** same file is allowed.
*/
static int ZvfsAppendObjCmd(
  void *NotUsed,             /* Client data for this command */
  Tcl_Interp *interp,        /* The interpreter used to report errors */
  int objc,                  /* Number of arguments */
  Tcl_Obj *const* objv       /* Values of all arguments */
){
  char *zArchive;
  Tcl_Channel chan;
  ZFile *pList = NULL, *pToc;
  int rc = TCL_OK, i;

  /* Open the archive and read the table of contents
  */
  if( objc<2 || (objc&1)!=0 ){
    Tcl_WrongNumArgs(interp, 1, objv, "ARCHIVE (SRC DEST)+");
    return TCL_ERROR;
  }

  zArchive = Tcl_GetString(objv[1]);
  chan = Tcl_OpenFileChannel(interp, zArchive, "r+", 0644);
  if( chan==0 ) {
    chan = Tcl_OpenFileChannel(interp, zArchive, "w+", 0644);
  }
  if( chan==0 ) return TCL_ERROR;
  if(  Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	  || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
    ){
      /* this should never happen */
      Tcl_Close(0, chan);
      return TCL_ERROR;
  }

  if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
      /* Null file is ok, we're creating new one. */
  } else {
    Tcl_Seek(chan, 0, SEEK_SET);
    rc = ZvfsReadTOC(interp, chan, &pList);
    if( rc==TCL_ERROR ){
      deleteZFileList(pList);
      Tcl_Close(interp, chan);
      return rc;
    } else rc=TCL_OK;
  }

  /* Move the file pointer to the start of
  ** the table of contents.
  */
  for(pToc=pList; pToc; pToc=pToc->pNext){
    if( pToc->isSpecial && strcmp(pToc->zName,"*TOC*")==0 ) break;
  }
  if( pToc ){
    Tcl_Seek(chan, pToc->iOffset, SEEK_SET);
  }else{
    Tcl_Seek(chan, 0, SEEK_END);
  }

  /* Add new files to the end of the archive.
  */
  for(i=2; rc==TCL_OK && i<objc; i+=2){
    char *zSrc = Tcl_GetString(objv[i]);
    char *zDest = Tcl_GetString(objv[i+1]);
    Tcl_Channel in;
    /* Open the file that is to be added to the ZIP archive
     */
    in = Tcl_OpenFileChannel(interp, zSrc, "r", 0);
    if( in==0 ){
      break;
    }
    if(  Tcl_SetChannelOption(interp, in, "-translation", "binary")
	  || Tcl_SetChannelOption(interp, in, "-encoding", "binary")
      ){
        /* this should never happen */
        Tcl_Close(0, in);
	rc = TCL_ERROR;
        break;
    }

    rc = writeFile(interp, chan, in, zSrc, zDest, &pList);
  }

  /* Write the table of contents at the end of the archive.
  */
  if (rc == TCL_OK) {
    pList = sortZFiles(pList);
    writeTOC(chan, pList);
  }

  /* Close the channel and exit */
  deleteZFileList(pList);
  Tcl_Close(interp, chan);
  return rc;
}

static CONST char *
GetExtension( CONST char *name)
{
    CONST char *p, *lastSep;
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
** Implementation of the zvfs::add command.
**
**    zvfs::add ?-fconfigure optpairs? ARCHIVE FILE1 FILE2 ... 
**
** This command is similar to append in that it adds files to the zip
** archive named ARCHIVE, however file names are relative the current directory.
** In addition, fconfigure is used to apply  option pairs to set upon opening
** of each file.  Otherwise, default translation is allowed
** for those file extensions listed in the ::zvfs::auto_ext var.
** Binary translation will be used for unknown extensions.
**
** NOTE Use '-fconfigure {}' to use auto translation for all.
*/
static int ZvfsAddObjCmd(
  void *NotUsed,             /* Client data for this command */
  Tcl_Interp *interp,        /* The interpreter used to report errors */
  int objc,                  /* Number of arguments */
  Tcl_Obj *CONST* objv       /* Values of all arguments */
){
  char *zArchive;
  Tcl_Channel chan;
  ZFile *pList = NULL, *pToc;
  int rc = TCL_OK, i, j, oLen;
  char *zOpts = NULL;
  Tcl_Obj *confOpts = NULL;
  int tobjc;
  Tcl_Obj **tobjv;
  Tcl_Obj *varObj = NULL;
  
  /* Open the archive and read the table of contents
  */
  if (objc>3) {
      zOpts =  Tcl_GetStringFromObj(objv[1], &oLen);
      if (!strncmp("-fconfigure", zOpts, oLen)) {
	  confOpts = objv[2];
          if (TCL_OK != Tcl_ListObjGetElements(interp, confOpts, &tobjc, &tobjv) || (tobjc%2)) {
	    return TCL_ERROR;
	  }
	  objc -= 2;
	  objv += 2;
      }
  }
  if( objc==2) {
      return TCL_OK;
  }

  if( objc<3) {
    Tcl_WrongNumArgs(interp, 1, objv, "?-fconfigure OPTPAIRS? ARCHIVE FILE1 FILE2 ..");
    return TCL_ERROR;
  }
  zArchive = Tcl_GetString(objv[1]);
  chan = Tcl_OpenFileChannel(interp, zArchive, "r+", 0644);
  if( chan==0 ) {
    chan = Tcl_OpenFileChannel(interp, zArchive, "w+", 0644);
  }
  if( chan==0 ) return TCL_ERROR;
  if(  Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	  || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
    ){
      /* this should never happen */
      Tcl_Close(0, chan);
      return TCL_ERROR;
  }

  if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
      /* Null file is ok, we're creating new one. */
  } else {
    Tcl_Seek(chan, 0, SEEK_SET);
    rc = ZvfsReadTOC(interp, chan, &pList);
    if( rc==TCL_ERROR ){
      deleteZFileList(pList);
      Tcl_Close(interp, chan);
      return rc;
    } else rc=TCL_OK;
  }

  /* Move the file pointer to the start of
  ** the table of contents.
  */
  for(pToc=pList; pToc; pToc=pToc->pNext){
    if( pToc->isSpecial && strcmp(pToc->zName,"*TOC*")==0 ) break;
  }
  if( pToc ){
    Tcl_Seek(chan, pToc->iOffset, SEEK_SET);
  }else{
    Tcl_Seek(chan, 0, SEEK_END);
  }

  /* Add new files to the end of the archive.
  */
  for(i=2; rc==TCL_OK && i<objc; i++){
    char *zSrc = Tcl_GetString(objv[i]);
    Tcl_Channel in;
    /* Open the file that is to be added to the ZIP archive
     */
    in = Tcl_OpenFileChannel(interp, zSrc, "r", 0);
    if( in==0 ){
      break;
    }
    if (confOpts == NULL) {
	CONST char *ext = GetExtension(zSrc);
	if (ext != NULL) {
	    /* Use auto translation for known text files. */
	    if (varObj == NULL) {
	        varObj=Tcl_GetVar2Ex(interp, "::zvfs::auto_ext", NULL, TCL_GLOBAL_ONLY);
	    }
	    if (varObj && TCL_OK != Tcl_ListObjGetElements(interp, varObj, &tobjc, &tobjv)) {
		for (j=0; j<tobjc; j++) {
		    if (!strcmp(ext, Tcl_GetString(tobjv[j]))) {
		       break;
		    }	       
		}
		if (j>=tobjc) {
		       ext = NULL;
		}

	    }
	}
	if (ext == NULL) {
	   if (( Tcl_SetChannelOption(interp, in, "-translation", "binary")
		|| Tcl_SetChannelOption(interp, in, "-encoding", "binary"))
	  ) {
	    /* this should never happen */
	    Tcl_Close(0, in);
	    rc = TCL_ERROR;
	    break;
	  }
	}
    } else {
	for (j=0; j<tobjc; j+=2) {
	  if (Tcl_SetChannelOption(interp, in, Tcl_GetString(tobjv[j]), Tcl_GetString(tobjv[j+1]))) {
	    Tcl_Close(0, in);
	    rc = TCL_ERROR;
	    break;
	  }
	}

    }
    if (rc == TCL_OK)
	rc = writeFile(interp, chan, in, zSrc, zSrc, &pList);
  }

  /* Write the table of contents at the end of the archive.
  */
  if (rc == TCL_OK) {
      pList = sortZFiles(pList);
      writeTOC(chan, pList);
  }

  /* Close the channel and exit */
  deleteZFileList(pList);
  Tcl_Close(interp, chan);
  return rc;
}

/*
** Implementation of the zvfs::start command.
**
**    zvfs::start ARCHIVE
**
** This command strips returns the offset of zip data.
**
*/
static int ZvfsStartObjCmd(
void *NotUsed,             /* Client data for this command */
Tcl_Interp *interp,        /* The interpreter used to report errors */
int objc,                  /* Number of arguments */
Tcl_Obj *CONST* objv       /* Values of all arguments */
) {
    char *zArchive;
    Tcl_Channel chan;
    ZFile *pList = NULL, *pToc;
    int rc = TCL_OK, i, j, oLen;
    char *zOpts = NULL;
    Tcl_Obj *confOpts = NULL;
    int tobjc;
    Tcl_Obj **tobjv;
    Tcl_Obj *varObj = NULL;
    int zipStart;

    /* Open the archive and read the table of contents
    */
    if( objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "ARCHIVE");
        return TCL_ERROR;
    }
    zArchive = Tcl_GetString(objv[1]);
    chan = Tcl_OpenFileChannel(interp, zArchive, "r", 0644);
    if( chan==0 ) return TCL_ERROR;
    if(  Tcl_SetChannelOption(interp, chan, "-translation", "binary")
    || Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
    ){
        /* this should never happen */
        Tcl_Close(0, chan);
        return TCL_ERROR;
    }

    if (Tcl_Seek(chan, 0, SEEK_END) == 0) {
        Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
        return TCL_OK;
    } else {
        Tcl_Seek(chan, 0, SEEK_SET);
        rc = ZvfsReadTOCStart(interp, chan, &pList, &zipStart);
        if( rc!=TCL_OK ){
            deleteZFileList(pList);
            Tcl_Close(interp, chan);
            Tcl_AppendResult(interp, "not an archive", 0);
            return TCL_ERROR;
        }
    }

    /* Close the channel and exit */
    deleteZFileList(pList);
    Tcl_Close(interp, chan);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(zipStart));
    return TCL_OK;
}


int ZvfsTools_Init(Tcl_Interp *interp){
#ifdef USE_TCL_STUBS
  if( Tcl_InitStubs(interp,"8.0",0)==0 ){
    return TCL_ERROR;
  }
#endif
  Tcl_CreateObjCommand(interp, "::zvfs::addlist", ZvfsAppendObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "::zvfs::add", ZvfsAddObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "::zvfs::dump", ZvfsDumpObjCmd, 0, 0);
  Tcl_CreateObjCommand(interp, "::zvfs::start", ZvfsStartObjCmd, 0, 0);
  Tcl_SetVar(interp, "::zvfs::auto_ext", ".tcl .tk .itcl .htcl .txt .c .h .tht", TCL_GLOBAL_ONLY);
  Tcl_PkgProvide(interp, "zvfsctools", "1.0"); 
  return TCL_OK;
}

int ZvfsTools_SafeInit(Tcl_Interp *interp){
#ifdef USE_TCL_STUBS
  if( Tcl_InitStubs(interp,"8.0",0)==0 ){
    return TCL_ERROR;
  }
#endif
  Tcl_SetVar(interp, "::zvfs::auto_ext", ".tcl .tk .itcl .htcl .txt .c .h .tht", TCL_GLOBAL_ONLY);
  Tcl_CreateObjCommand(interp, "::zvfs::dump", ZvfsDumpObjCmd, 0, 0);
  Tcl_PkgProvide(interp, "zvfsctools", "1.0");

  return TCL_OK;
}
