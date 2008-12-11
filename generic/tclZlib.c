/*
 * tclZlib.c --
 *
 *	This file provides the interface to the Zlib library.
 *
 * Copyright (C) 2004, 2005 Pascal Scheffers <pascal@scheffers.net>
 * Copyright (C) 2005 Unitas Software B.V.
 * Copyright (c) 2008 Donal K. Fellows
 *
 * Parts written by Jean-Claude Wippler, as part of Tclkit, placed in the
 * public domain March 2003.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclZlib.c,v 1.2 2008/12/11 14:24:01 dkf Exp $
 */

#if 0
#include "tclInt.h"
#include <zlib.h>

typedef struct {
    z_stream stream;
    gz_header header;
    Tcl_Interp *interp;
    Tcl_Command cmd;

} StreamInfo;
typedef struct ThreadSpecificData {
    int counter;
} ThreadSpecificData;
static Tcl_ThreadDataKey tsdKey;

static void		ConvertError(Tcl_Interp *interp, int code);
static int		GenerateHeader(Tcl_Interp *interp, Tcl_Obj *dictObj,
			    gz_header *headerPtr);
static void		ExtractHeader(gz_header *headerPtr, Tcl_Obj *dictObj);
static int		ZlibStream(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static void		DeleteStream(ClientData clientData);
// TODO: Write streaming C API
// TODO: Write Tcl API

static void
ConvertError(
    Tcl_Interp *interp,
    int code)
{
    if (interp == NULL) {
	return;
    }

    if (code == Z_ERRNO) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_PosixError(interp),-1));
    } else {
	const char *codeStr;
	char codeStr2[TCL_INTEGER_SPACE];

	switch (code) {
	case Z_STREAM_ERROR:	codeStr = "STREAM";	break;
	case Z_DATA_ERROR:	codeStr = "DATA";	break;
	case Z_MEM_ERROR:	codeStr = "MEM";	break;
	case Z_BUF_ERROR:	codeStr = "BUF";	break;
	case Z_VERSION_ERROR:	codeStr = "VERSION";	break;
	default:
	    codeStr = "unknown";
	    sprintf(codeStr2, "%d", code);
	    break;
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(zError(code), -1));
	Tcl_SetErrorCode(interp, "TCL", "ZLIB", codeStr, codeStr2, NULL);
    }
}

static inline int
GetValue(
    Tcl_Interp *interp,
    Tcl_Obj *dictObj,
    const char *nameStr,
    Tcl_Obj **valuePtrPtr)
{
    Tcl_Obj *name = Tcl_NewStringObj(nameStr, -1);
    int result =  Tcl_DictObjGet(interp, dictObj, name, valuePtrPtr);

    TclDecrRefCount(name);
    return result;
}

static int
GenerateHeader(
    Tcl_Interp *interp,
    Tcl_Obj *dictObj,
    gz_header *headerPtr)
{
    Tcl_Obj *value;
    static const char *types[] = {
	"binary", "text"
    };

    if (GetValue(interp, dictObj, "comment", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL) {
	headerPtr->comment = (Bytef *) Tcl_GetString(value);
    }

    if (GetValue(interp, dictObj, "crc", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL &&
	    Tcl_GetBooleanFromObj(interp, value, &headerPtr->hcrc)) {
	return TCL_ERROR;
    }

    if (GetValue(interp, dictObj, "filename", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL) {
	headerPtr->name = (Bytef *) Tcl_GetString(value);
    }

    if (GetValue(interp, dictObj, "os", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL &&
	    Tcl_GetIntFromObj(interp, value, &headerPtr->os) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Ignore the 'size' field.
     */

    if (GetValue(interp, dictObj, "time", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL && Tcl_GetLongFromObj(interp, value,
	    (long *) &headerPtr->time) != TCL_OK) {
	return TCL_ERROR;
    }

    if (GetValue(interp, dictObj, "type", &value) != TCL_OK) {
	return TCL_ERROR;
    } else if (value != NULL && Tcl_GetIndexFromObj(interp, value, types,
	    "type", TCL_EXACT, &headerPtr->text) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}

static inline void
SetValue(
    Tcl_Obj *dictObj,
    const char *key,
    Tcl_Obj *value)
{
    Tcl_Obj *keyObj = Tcl_NewStringObj(key, -1);

    Tcl_IncrRefCount(keyObj);
    Tcl_DictObjPut(NULL, dictObj, keyObj, value);
    TclDecrRefCount(keyObj);
}

static void
ExtractHeader(
    gz_header *headerPtr,
    Tcl_Obj *dictObj)
{
    if (headerPtr->comment != Z_NULL) {
	SetValue(dictObj, "comment",
		Tcl_NewStringObj((char *) headerPtr->comment, -1));
    }
    SetValue(dictObj, "crc", Tcl_NewBooleanObj(headerPtr->hcrc));
    if (headerPtr->name != Z_NULL) {
	SetValue(dictObj, "filename",
		Tcl_NewStringObj((char *) headerPtr->name, -1));
    }
    if (headerPtr->os != 255) {
	SetValue(dictObj, "os", Tcl_NewIntObj(headerPtr->os));
    }
    if (headerPtr->time != 0 /* magic - no time */) {
	SetValue(dictObj, "time", Tcl_NewLongObj((long) headerPtr->time));
    }
    if (headerPtr->text != Z_UNKNOWN) {
	SetValue(dictObj, "type",
		Tcl_NewStringObj(headerPtr->text ? "text" : "binary", -1));
    }
}

Tcl_Obj *
Tcl_ZlibDeflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *dataObj,
    int level,
    Tcl_Obj *dictObj)
{
    int rawLength;
    unsigned char *rawBytes = Tcl_GetByteArrayFromObj(dataObj, &rawLength);
    z_stream stream;
    gz_header header, *headerPtr = NULL;
    Tcl_Obj *outObj = Tcl_NewObj();
    int code, bits;

    switch (format) {
    case TCL_ZLIB_FORMAT_RAW:
	bits = -15;
	break;
    case TCL_ZLIB_FORMAT_ZLIB:
	bits = 15;
	break;
    case TCL_ZLIB_FORMAT_GZIP:
	bits = 15 | /* gzip magic */ 16;
	if (dictObj != NULL) {
	    headerPtr = &header;
	    memset(headerPtr, 0, sizeof(gz_header));
	    if (GenerateHeader(interp, dictObj, headerPtr) != TCL_OK) {
		Tcl_DecrRefCount(outObj);
		return NULL;
	    }
	}
	break;
    default:
	Tcl_Panic("bad compression format: %d", format);
	return NULL;
    }

    stream.avail_in = (uInt) rawLength;
    stream.next_in = rawBytes;
    stream.avail_out = (uInt) rawLength + rawLength/1000 + 12;
    stream.next_out = Tcl_SetByteArrayLength(outObj, stream.avail_out);
    stream.zalloc = NULL;
    stream.zfree = NULL;
    stream.opaque = NULL;

    code = deflateInit2(&stream, level, Z_DEFLATED, bits, MAX_MEM_LEVEL,
	    Z_DEFAULT_STRATEGY);
    if (code != Z_OK) {
	goto error;
    }
    if (headerPtr != NULL) {
	deflateSetHeader(&stream, headerPtr);
	if (code != Z_OK) {
	    goto error;
	}
    }
    code = deflate(&stream, Z_FINISH);
    if (code != Z_STREAM_END) {
	deflateEnd(&stream);
	if (code == Z_OK) {
	    code = Z_BUF_ERROR;
	}
    } else {
	code = deflateEnd(&stream);
    }

    if (code == Z_OK) {
	Tcl_SetByteArrayLength(outObj, stream.total_out);
	return outObj;
    }

  error:
    Tcl_DecrRefCount(outObj);
    ConvertError(interp, code);
    return NULL;
}

Tcl_Obj *
Tcl_ZlibInflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *dataObj,
    Tcl_Obj *dictObj)
{
    int compressedLength;
    unsigned char *compressedBytes =
	    Tcl_GetByteArrayFromObj(dataObj, &compressedLength);
    z_stream stream;
    gz_header header, *headerPtr = NULL;
    Tcl_Obj *outObj = Tcl_NewObj();
    unsigned int outSize = 16 * 1024;
    int code = Z_BUF_ERROR, bits;
    char *nameBuf = NULL, *commentBuf = NULL;

    stream.avail_in = (uInt) compressedLength + 1;
    stream.next_in = compressedBytes;
    stream.opaque = NULL;

    switch (format) {
    case TCL_ZLIB_FORMAT_RAW:
	bits = -15;
	break;
    case TCL_ZLIB_FORMAT_ZLIB:
	bits = 15;
	break;
    case TCL_ZLIB_FORMAT_GZIP:
	bits = 15 | /* gzip magic */ 16;
	if (dictObj != NULL) {
	    goto allocHeader;
	}
	break;
    case TCL_ZLIB_FORMAT_AUTO:
	bits = 15 | /* auto magic */ 32;
	if (dictObj != NULL) {
	allocHeader:
	    headerPtr = &header;
	    memset(headerPtr, 0, sizeof(gz_header));
	    nameBuf = ckalloc(PATH_MAX);
	    header.name = (void *) nameBuf;
	    header.name_max = PATH_MAX;
	    commentBuf = ckalloc(256);
	    header.comment = (void *) commentBuf;
	    header.comm_max = 256;
	}
	break;
    default:
	Tcl_Panic("unrecognized format: %d", format);
	return NULL;
    }

    /*
     * Loop trying to decompress until we've got enough space. Inefficient,
     * but works.
     */

    for (; (outSize > 1024) && (code == Z_BUF_ERROR) ; outSize *= 2) {
	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.avail_out = (uInt) outSize;
	stream.next_out = Tcl_SetByteArrayLength(outObj, outSize);

	code = inflateInit2(&stream, bits);
	if (code != Z_OK) {
	    goto error;
	}

	if (headerPtr != NULL) {
	    inflateGetHeader(&stream, headerPtr);
	    if (code != Z_OK) {
		goto error;
	    }
	}

	code = inflate(&stream, Z_FINISH);

	if (code != Z_STREAM_END) {
	    inflateEnd(&stream);
	    if (code == Z_OK) {
		code = Z_BUF_ERROR;
	    }
	} else {
	    code = inflateEnd(&stream);
	}
    }

    if (code == Z_OK) {
	if (headerPtr != NULL) {
	    ExtractHeader(headerPtr, dictObj);
	    SetValue(dictObj, "size", Tcl_NewLongObj((long)stream.total_out));
	    ckfree(nameBuf);
	    ckfree(commentBuf);
	}
	Tcl_SetByteArrayLength(outObj, stream.total_out);
	return outObj;
    }

  error:
    Tcl_DecrRefCount(outObj);
    if (headerPtr != NULL) {
	ckfree(nameBuf);
	ckfree(commentBuf);
    }
    ConvertError(interp, code);
    return NULL;
}

unsigned int
Tcl_ZlibCRC32(
    const char *bytes,
    int length)
{
    unsigned int initValue = crc32(0, NULL, 0);

    return crc32(initValue, (unsigned char *) bytes, (unsigned) length);
}

unsigned int
Tcl_ZlibAdler32(
    const char *bytes,
    int length)
{
    unsigned int initValue = adler32(0, NULL, 0);

    return adler32(initValue, (unsigned char *) bytes, (unsigned) length);
}

int
Tcl_ZlibStreamInit(
    Tcl_Interp *interp,
    int mode,
    int format,
    int level,
    Tcl_Obj *dictObj,
    Tcl_ZlibStream *zshandlePtr)
{
    StreamInfo *siPtr = (StreamInfo *) ckalloc(sizeof(StreamInfo));
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&tsdKey);
    char buf[TCL_INTEGER_SPACE+8];

    memset(&siPtr->stream, 0, sizeof(z_stream));
    memset(&siPtr->header, 0, sizeof(gz_header));

    siPtr->interp = interp;
    sprintf(buf, "zstream%d", tsdPtr->counter++);
    siPtr->cmd = Tcl_CreateObjCommand(interp, buf, ZlibStream, siPtr,
	    DeleteStream);

    Tcl_Panic("unimplemented");

    *zshandlePtr = (Tcl_ZlibStream) siPtr;
    return TCL_OK;
}

Tcl_Obj *
Tcl_ZlibStreamGetCommandName(
    Tcl_ZlibStream zshandle)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    Tcl_Obj *cmdnameObj = Tcl_NewObj();

    Tcl_GetCommandFullName(siPtr->interp, siPtr->cmd, cmdnameObj);
    return cmdnameObj;
}

int
Tcl_ZlibStreamEof(
    Tcl_ZlibStream zshandle)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    Tcl_Panic("unimplemented");
    return -1;
}

int
Tcl_ZlibStreamClose(
    Tcl_ZlibStream zshandle)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    int code = -1;

    Tcl_Panic("unimplemented");

    if (siPtr->cmd) {
	/*
	 * Must be last in this function!
	 */

	register Tcl_Command cmd = siPtr->cmd;

	siPtr->cmd = NULL;
	Tcl_DeleteCommandFromToken(siPtr->interp, cmd);
    }
    return code;
}

int
Tcl_ZlibStreamAdler32(
    Tcl_ZlibStream zshandle)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    Tcl_Panic("unimplemented");
    return -1;
}

int
Tcl_ZlibStreamPut(
    Tcl_ZlibStream zshandle,
    const char *bytes,
    int length,
    int flush)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    Tcl_Panic("unimplemented");
    return -1;
}

int
Tcl_ZlibStreamGet(
    Tcl_ZlibStream zshandle,
    const char *bytes,
    int length)
{
    StreamInfo *siPtr = (StreamInfo *) zshandle;
    Tcl_Panic("unimplemented");
    return -1;
}

static void
DeleteStream(
    ClientData clientData)
{
    register StreamInfo *siPtr = clientData;

    if (siPtr->cmd) {
	siPtr->cmd = NULL;
	Tcl_ZlibStreamClose(clientData);
    }
    ckfree(clientData);
}

static int
ZlibStream(
    ClientData clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    StreamInfo *siPtr = clientData;
    static const char *subcmds[] = {
	"adler32", "close", "eof", "finalize", "flush", "fullflush", "get",
	"put", NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand ...");
	return TCL_ERROR;
    }
    Tcl_SetResult(interp, "unimplemented", TCL_STATIC);
    return TCL_ERROR;
}

#else /* !REIMPLEMENT */

#include "tcl.h"
#include <zlib.h>
#include <string.h>

/*
 * Structure used for the Tcl_ZlibStream* commands and [zlib stream ...]
 */

typedef struct {
    Tcl_Interp *interp;
    z_stream stream;
    int streamend;
    Tcl_Obj *indata, *outdata;	/* Input / output buffers (lists) */
    Tcl_Obj *current_input;	/* Pointer to what is currently being
				 * inflated. */
    int inpos, outpos;
    int mode;			/* ZLIB_DEFLATE || ZLIB_INFLATE */
    int format;			/* ZLIB_FORMAT_* */
    int level;			/* Default 5, 0-9 */
    int flush;			/* Stores the flush param for deferred the
				 * decompression. */
    int wbits;
    Tcl_Obj *cmdname;		/* Name of the associated Tcl command */
} zlibStreamHandle;


/*
 * Prototypes for private procedures defined later in this file:
 */

static int		ZlibCmd(ClientData dummy, Tcl_Interp *ip, int objc,
			    Tcl_Obj *const objv[]);
static int		ZlibStreamCmd(ClientData cd, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static void		ZlibStreamCmdDelete(ClientData cd);
static void		ZlibStreamCleanup(zlibStreamHandle *zsh);

/*
 * Prototypes for private procedures used by channel stacking:
 */

#ifdef ENABLE_CHANSTACKING
static int		ChanClose(ClientData instanceData,
			    Tcl_Interp *interp);
static int		ChanInput(ClientData instanceData, char *buf,
			    int toRead, int *errorCodePtr);
static int		ChanOutput(ClientData instanceData, const char *buf,
			    int toWrite, int*errorCodePtr);
static int		ChanSeek(ClientData instanceData, long offset,
			    int mode, int *errorCodePtr);
static int		ChanSetOption(ClientData instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    const char *value);
static int		ChanGetOption(ClientData instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    Tcl_DString *dsPtr);
static void		ChanWatch(ClientData instanceData, int mask);
static int		ChanGetHandle(ClientData instanceData, int direction,
			    ClientData *handlePtr);
static int		ChanClose2(ClientData instanceData,
			    Tcl_Interp *interp, int flags);
static int		ChanBlockMode(ClientData instanceData, int mode);
static int		ChanFlush(ClientData instanceData);
static int		ChanHandler(ClientData instanceData,
			    int interestMask);
static Tcl_WideInt	ChanWideSeek(ClientData instanceData,
			    Tcl_WideInt offset, int mode, int *errorCodePtr);

static Tcl_ChannelType zlibChannelType = {
    "zlib",
    TCL_CHANNEL_VERSION_3,
    ChanClose,
    ChanInput,
    ChanOutput,
    NULL, /* ChanSeek, */
    NULL, /* ChanSetOption, */
    NULL, /* ChanGetOption, */
    ChanWatch,
    ChanGetHandle,
    NULL, /* ChanClose2, */
    ChanBlockMode,
    ChanFlush,
    ChanHandler,
    NULL /* ChanWideSeek */
};

typedef struct {
    /* Generic channel info */
    Tcl_Channel channel;
    Tcl_TimerToken timer;
    int flags;
    int mask;

    /* Zlib specific channel state */
    int inFormat;
    int outFormat;
    z_stream instream;
    z_stream outstream;
    char *inbuffer;
    int inAllocated, inUsed, inPos;
    char *outbuffer;
    int outAllocated, outUsed, outPos;
} Zlib_ChannelData;

/* Flag values */
#define ASYNC 1
#endif /* ENABLE_CHANSTACKING */

#ifdef TCLKIT_BUILD
/*
 * Structure for the old zlib sdeflate/sdecompress commands
 * Deprecated!
 */

typedef struct {
    z_stream stream;
    Tcl_Obj *indata;
} zlibstream;

static int
zstreamincmd(
    ClientData cd,
    Tcl_Interp *ip,
    int objc,
    Tcl_Obj *const objv[])
{
    zlibstream *zp = cd;
    int count = 0;
    int e, index;
    Tcl_Obj *obj;

    static const char* cmds[] = { "fill", "drain", NULL, };

    if (Tcl_GetIndexFromObj(ip, objv[1], cmds, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case 0: /* fill ?data? */
	if (objc >= 3) {
	    Tcl_IncrRefCount(objv[2]);
	    Tcl_DecrRefCount(zp->indata);
	    zp->indata = objv[2];
	    zp->stream.next_in =
		    Tcl_GetByteArrayFromObj(zp->indata, &zp->stream.avail_in);
	}
	Tcl_SetObjResult(ip, Tcl_NewIntObj(zp->stream.avail_in));
	break;
    case 1: /* drain count */
	if (objc != 3) {
	    Tcl_WrongNumArgs(ip, 2, objv, "count");
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	obj = Tcl_GetObjResult(ip);
	Tcl_SetByteArrayLength(obj, count);
	zp->stream.next_out =
		Tcl_GetByteArrayFromObj(obj, &zp->stream.avail_out);
	e = inflate(&zp->stream, Z_NO_FLUSH);
	if (e != Z_OK && e != Z_STREAM_END) {
	    Tcl_SetResult(ip, (char *) zError(e), TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_SetByteArrayLength(obj, count - zp->stream.avail_out);
	break;
    }
    return TCL_OK;
}

static void
zstreamdelproc(
    ClientData cd)
{
    zlibstream *zp = cd;

    inflateEnd(&zp->stream);
    Tcl_DecrRefCount(zp->indata);
    ckfree((void*) zp);
}

static int
ZlibCmdO(
    ClientData dummy,
    Tcl_Interp *ip,
    int objc,
    Tcl_Obj *const objv[])
{
    int e = TCL_OK, index, dlen, wbits = -MAX_WBITS;
    unsigned flag;
    Byte *data;
    z_stream stream;
    Tcl_Obj *obj = Tcl_GetObjResult(ip);

    static const char* cmds[] = {
	"adler32", "crc32", "compress", "deflate", "decompress", "inflate",
	"sdecompress", "sinflate", NULL,
    };

    if (objc < 3 || objc > 4) {
	Tcl_WrongNumArgs(ip, 1, objv, "option data ?...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(ip, objv[1], cmds, "option", 0,
	    &index) != TCL_OK || (objc > 3 &&
	    Tcl_GetIntFromObj(ip, objv[3], &flag)) != TCL_OK) {
	return TCL_ERROR;
    }

    data = Tcl_GetByteArrayFromObj(objv[2], &dlen);

    switch (index) {
    case 0: /* adler32 str ?start? -> checksum */
	if (objc < 4) {
	    flag = adler32(0, 0, 0);
	}
	Tcl_SetIntObj(obj, adler32(flag, data, dlen));
	return TCL_OK;
    case 1: /* crc32 str ?start? -> checksum */
	if (objc < 4) {
	    flag = crc32(0, 0, 0);
	}
	Tcl_SetIntObj(obj, crc32(flag, data, dlen));
	return TCL_OK;
    case 2: /* compress data ?level? -> data */
	wbits = MAX_WBITS;
    case 3: /* deflate data ?level? -> data */
	if (objc < 4) {
	    flag = Z_DEFAULT_COMPRESSION;
	}

	stream.avail_in = (uInt) dlen;
	stream.next_in = data;

	stream.avail_out = (uInt) dlen + dlen / 1000 + 12;
	Tcl_SetByteArrayLength(obj, stream.avail_out);
	stream.next_out = Tcl_GetByteArrayFromObj(obj, NULL);

	stream.zalloc = 0;
	stream.zfree = 0;
	stream.opaque = 0;

	e = deflateInit2(&stream, flag, Z_DEFLATED, wbits,
		MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (e != Z_OK) {
	    break;
	}

	e = deflate(&stream, Z_FINISH);
	if (e != Z_STREAM_END) {
	    deflateEnd(&stream);
	    if (e == Z_OK) {
		e = Z_BUF_ERROR;
	    }
	} else {
	    e = deflateEnd(&stream);
	}
	break;
    case 4: /* decompress data ?bufsize? -> data */
	wbits = MAX_WBITS;
    case 5: /* inflate data ?bufsize? -> data */
	if (objc < 4) {
	    flag = 16 * 1024;
	}

	for (;;) {
	    stream.zalloc = 0;
	    stream.zfree = 0;

	    /*
	     * +1 because ZLIB can "over-request" input (but ignore it)
	     */

	    stream.avail_in = (uInt) dlen +  1;
	    stream.next_in = data;

	    stream.avail_out = (uInt) flag;
	    Tcl_SetByteArrayLength(obj, stream.avail_out);
	    stream.next_out = Tcl_GetByteArrayFromObj(obj, NULL);

	    /*
	     * Negative value suppresses ZLIB header
	     */

	    e = inflateInit2(&stream, wbits);
	    if (e == Z_OK) {
		e = inflate(&stream, Z_FINISH);
		if (e != Z_STREAM_END) {
		    inflateEnd(&stream);
		    if (e == Z_OK) {
			e = Z_BUF_ERROR;
		    }
		} else {
		    e = inflateEnd(&stream);
		}
	    }

	    if (e == Z_OK || e != Z_BUF_ERROR) {
		break;
	    }

	    Tcl_SetByteArrayLength(obj, 0);
	    flag *= 2;
	}
	break;
    case 6: /* sdecompress cmdname -> */
	wbits = MAX_WBITS;
    case 7: { /* sinflate cmdname -> */
	zlibstream *zp = (zlibstream *) ckalloc(sizeof(zlibstream));

	zp->indata = Tcl_NewObj();
	Tcl_IncrRefCount(zp->indata);
	zp->stream.zalloc = 0;
	zp->stream.zfree = 0;
	zp->stream.opaque = 0;
	zp->stream.next_in = 0;
	zp->stream.avail_in = 0;
	inflateInit2(&zp->stream, wbits);
	Tcl_CreateObjCommand(ip, Tcl_GetStringFromObj(objv[2], 0),
		zstreamincmd, (ClientData) zp, zstreamdelproc);
	return TCL_OK;
    }
    }

    if (e != Z_OK) {
	Tcl_SetResult(ip, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }

    Tcl_SetByteArrayLength(obj, stream.total_out);
    return TCL_OK;
}
#endif /* TCLKIT_BUILD */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamInit --
 *
 *	This command initializes a (de)compression context/handle for
 *	(de)compressing data in chunks.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	zshandle is initialised and memory allocated for internal state.
 *	Additionally, if interp is not null, a Tcl command is created and its
 *	name placed in the interp result obj.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ZlibStreamInit(
    Tcl_Interp *interp,
    int mode,			/* ZLIB_INFLATE || ZLIB_DEFLATE */
    int format,			/* ZLIB_FORMAT_* */
    int level,			/* 0-9 or ZLIB_DEFAULT_COMPRESSION */
    Tcl_Obj *dictObj,		/* Headers for gzip */
    Tcl_ZlibStream *zshandle)
{
    int wbits = 0;
    int e;
    zlibStreamHandle *zsh = NULL;
    Tcl_DString cmdname;
    Tcl_CmdInfo cmdinfo;

    if (mode == TCL_ZLIB_STREAM_DEFLATE) {
	/*
	 * Compressed format is specified by the wbits parameter. See zlib.h
	 * for details.
	 */

	if (format == TCL_ZLIB_FORMAT_RAW) {
	    wbits = -MAX_WBITS;
	} else if (format == TCL_ZLIB_FORMAT_GZIP) {
	    wbits = MAX_WBITS+16;
	} else if (format == TCL_ZLIB_FORMAT_ZLIB) {
	    wbits = MAX_WBITS;
	} else {
	    Tcl_Panic("incorrect zlib data format, must be "
		    "TCL_ZLIB_FORMAT_ZLIB, TCL_ZLIB_FORMAT_GZIP or "
		    "TCL_ZLIB_FORMAT_RAW");
	}
	if (level < -1 || level > 9) {
	    if (interp) {
		Tcl_SetResult(interp, "Compression level should be between "
			"0 (no compression) and 9 (best compression) or -1 "
			"for default compression level.", TCL_STATIC);
	    }
	    return TCL_ERROR;
	}
    } else {
	/*
	 * mode == ZLIB_INFLATE
	 * wbits are the same as DEFLATE, but FORMAT_AUTO is valid too.
	 */

	if (format == TCL_ZLIB_FORMAT_RAW) {
	    wbits = -MAX_WBITS;
	} else if (format == TCL_ZLIB_FORMAT_GZIP) {
	    wbits = MAX_WBITS+16;
	} else if (format == TCL_ZLIB_FORMAT_ZLIB) {
	    wbits = MAX_WBITS;
	} else if (format == TCL_ZLIB_FORMAT_AUTO) {
	    wbits = MAX_WBITS+32;
	} else {
	    Tcl_Panic("incorrect zlib data format, must be "
		    "TCL_ZLIB_FORMAT_ZLIB, TCL_ZLIB_FORMAT_GZIP, "
		    "TCL_ZLIB_FORMAT_RAW or TCL_ZLIB_FORMAT_AUTO");
	}
    }

    zsh = (zlibStreamHandle *) ckalloc(sizeof(zlibStreamHandle));
    zsh->interp = interp;
    zsh->mode = mode;
    zsh->format = format;
    zsh->level = level;
    zsh->wbits = wbits;
    zsh->current_input = NULL;
    zsh->streamend = 0;
    zsh->stream.avail_in = 0;
    zsh->stream.next_in = 0;
    zsh->stream.zalloc = 0;
    zsh->stream.zfree = 0;
    zsh->stream.opaque = 0;		/* Must be initialized before calling
					 * (de|in)flateInit2 */

    /*
     * No output buffer available yet
     */

    zsh->stream.avail_out = 0;
    zsh->stream.next_out = NULL;

    if (mode == TCL_ZLIB_STREAM_DEFLATE) {
	e = deflateInit2(&zsh->stream, level, Z_DEFLATED, wbits,
		MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    } else {
	e = inflateInit2(&zsh->stream, wbits);
    }

    if (e != Z_OK) {
	if (interp) {
	    Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	}
	goto error;
    }

    /*
     * I could do all this in C, but this is easier.
     */

    if (interp != NULL) {
	if (Tcl_Eval(interp, "incr ::zlib::cmdcounter") != TCL_OK) {
	    goto error;
	}
	Tcl_DStringInit(&cmdname);
	Tcl_DStringAppend(&cmdname, "::zlib::streamcmd-", -1);
	Tcl_DStringAppend(&cmdname, Tcl_GetString(Tcl_GetObjResult(interp)),
		-1);
	if (Tcl_GetCommandInfo(interp, Tcl_DStringValue(&cmdname),
		&cmdinfo) == 1 ) {
	    Tcl_SetResult(interp,
		    "BUG: Stream command name already exists", TCL_STATIC);
	    goto error;
	}
	Tcl_ResetResult(interp);

	/*
	 * Create the command.
	 */

	Tcl_CreateObjCommand(interp, Tcl_DStringValue(&cmdname),
		ZlibStreamCmd, zsh, ZlibStreamCmdDelete);

	/*
	 * Create the cmdname obj for future reference.
	 */

	zsh->cmdname = Tcl_NewStringObj(Tcl_DStringValue(&cmdname),
		Tcl_DStringLength(&cmdname));
	Tcl_IncrRefCount(zsh->cmdname);
	Tcl_DStringFree(&cmdname);
    } else {
	zsh->cmdname = NULL;
    }

    /*
     * Prepare the buffers for use.
     */

    zsh->indata = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(zsh->indata);
    zsh->outdata = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(zsh->outdata);

    zsh->inpos = 0;
    zsh->outpos = 0;

    /*
     * Now set the int pointed to by *zshandle to the pointer to the zsh
     * struct.
     */

    if (zshandle) {
	*zshandle = (Tcl_ZlibStream) zsh;
    }

    return TCL_OK;
 error:
    ckfree((char *) zsh);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ZlibStreamCmdDelete --
 *
 *	This is the delete command which Tcl invokes when a zlibstream command
 *	is deleted from the interpreter (on stream close, usually).
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Invalidates the zlib stream handle as obtained from Tcl_ZlibStreamInit
 *
 *----------------------------------------------------------------------
 */

static void
ZlibStreamCmdDelete(
    ClientData cd)
{
    zlibStreamHandle *zsh = cd;

    ZlibStreamCleanup(zsh);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamClose --
 *
 *	This procedure must be called after (de)compression is done to ensure
 *	memory is freed and the command is deleted from the interpreter (if
 *	any).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Invalidates the zlib stream handle as obtained from Tcl_ZlibStreamInit
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ZlibStreamClose(
    Tcl_ZlibStream zshandle)	/* As obtained from Tcl_ZlibStreamInit. */
{
    zlibStreamHandle *zsh = (zlibStreamHandle *) zshandle;

    /*
     * If the interp is set, deleting the command will trigger
     * ZlibStreamCleanup in ZlibStreamCmdDelete. If no interp is set, call
     * ZlibStreamCleanup directly.
     */

    if (zsh->interp && zsh->cmdname) {
	Tcl_DeleteCommand(zsh->interp,
		Tcl_GetStringFromObj(zsh->cmdname, NULL));
    } else {
	ZlibStreamCleanup(zsh);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ZlibStreamCleanup --
 *
 *	This procedure is called by either Tcl_ZlibStreamClose or
 *	ZlibStreamCmdDelete to cleanup the stream context.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Invalidates the zlib stream handle.
 *
 *----------------------------------------------------------------------
 */

void
ZlibStreamCleanup(
    zlibStreamHandle *zsh)
{
    if (!zsh->streamend) {
	if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	    deflateEnd(&zsh->stream);
	} else {
	    inflateEnd(&zsh->stream);
	}
    }

    if (zsh->indata) {
	Tcl_DecrRefCount(zsh->indata);
    }
    if (zsh->outdata) {
	Tcl_DecrRefCount(zsh->outdata);
    }
    if (zsh->cmdname) {
	Tcl_DecrRefCount(zsh->cmdname);
    }
    if (zsh->current_input) {
	Tcl_DecrRefCount(zsh->current_input);
    }

    ckfree((void *) zsh);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamReset --
 *
 *	This procedure will reinitialize an existing stream handle.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Any data left in the (de)compression buffer is lost.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ZlibStreamReset(
    Tcl_ZlibStream zshandle)	/* As obtained from Tcl_ZlibStreamInit */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;
    int e;

    if (!zsh->streamend) {
	if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	    deflateEnd(&zsh->stream);
	} else {
	    inflateEnd(&zsh->stream);
	}
    }
    Tcl_SetByteArrayLength(zsh->indata, 0);
    Tcl_SetByteArrayLength(zsh->outdata, 0);
    if (zsh->current_input) {
	Tcl_DecrRefCount(zsh->current_input);
	zsh->current_input=NULL;
    }

    zsh->inpos = 0;
    zsh->outpos = 0;
    zsh->streamend = 0;
    zsh->stream.avail_in = 0;
    zsh->stream.next_in = 0;
    zsh->stream.zalloc = 0;
    zsh->stream.zfree = 0;
    zsh->stream.opaque = 0;		/* Must be initialized before calling
					 * (de|in)flateInit2 */

    /* No output buffer available yet */
    zsh->stream.avail_out = 0;
    zsh->stream.next_out = NULL;

    if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	e = deflateInit2(&zsh->stream, zsh->level, Z_DEFLATED, zsh->wbits,
		MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    } else {
	e = inflateInit2(&zsh->stream, zsh->wbits);
    }

    if ( e != Z_OK ) {
	if (zsh->interp) {
	    Tcl_SetResult(zsh->interp, (char*) zError(e), TCL_STATIC);
	}
	/* TODOcleanup */
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamGetCommandName --
 *
 *	This procedure will return the command name associated with the
 *	stream.
 *
 * Results:
 *	A Tcl_Obj with the name of the Tcl command or NULL if no command is
 *	associated with the stream.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_ZlibStreamGetCommandName(
    Tcl_ZlibStream zshandle) /* as obtained from Tcl_ZlibStreamInit */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;

    return zsh->cmdname;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamEof --
 *
 *	This procedure This function returns 0 or 1 depending on the state of
 *	the (de)compressor. For decompression, eof is reached when the entire
 *	compressed stream has been decompressed. For compression, eof is
 *	reached when the stream has been flushed with ZLIB_FINALIZE.
 *
 * Results:
 *	Integer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ZlibStreamEof(
    Tcl_ZlibStream zshandle)	/* As obtained from Tcl_ZlibStreamInit */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;

    return zsh->streamend;
}

int
Tcl_ZlibStreamAdler32(
    Tcl_ZlibStream zshandle)	/* As obtained from Tcl_ZlibStreamInit */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;

    return zsh->stream.adler;
}

#ifdef DISABLED_CODE
int
Tcl_ZlibStreamAdd(
    Tcl_ZlibStream zshandle,	/* As obtained from Tcl_ZlibStreamInit */
    char *data,			/* Data to compress/decompress */
    int size,			/* Byte length of data */
    int flush,			/* 0, ZLIB_FLUSH, ZLIB_FULLFLUSH,
				 * ZLIB_FINALIZE */
    Tcl_Obj *outdata,		/* An object to append the compressed data
				 * to. */
    int buffersize)		/* Hint of the expected output size of
				 * inflate/deflate */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;
    char *datatmp=0, *outptr=0;
    int e, outsize;

    zsh->stream.next_in = data;
    zsh->stream.avail_in = size;

    if (zsh->mode == ZLIB_DEFLATE) {
	if (buffersize < 6) {
	    /*
	     * The 6 comes from the zlib.h description of deflate. If the
	     * suggested buffer size is below 6, use deflateBound to get the
	     * minimum number of bytes needed from zlib.
	     */

	    zsh->stream.avail_out =
		    deflateBound(&zsh->stream, zsh->stream.avail_in);
	} else {
	    zsh->stream.avail_out=buffersize;
	}
	datatmp = ckalloc(zsh->stream.avail_out);
	zsh->stream.next_out = datatmp;
	e = deflate(&zsh->stream, flush);
	while ((e==Z_OK || e==Z_BUF_ERROR) && zsh->stream.avail_out==0) {
	    /* Output buffer too small */
	    Tcl_Panic("StreamAdd/Deflate - Buffer growing not implemented yet");
	}

	/*
	 * Now append the (de)compressed data to outdata.
	 */

	Tcl_GetByteArrayFromObj(outdata, &outsize);
	outptr = Tcl_SetByteArrayLength(outdata,
		outsize + zsh->stream.total_out);
	memcpy(&outptr[outsize], datatmp, zsh->stream.total_out);
    } else {
	if (buffersize == 0) {
	    /* Start with a buffer 3 times the size of the input data */
	    /* TODO: integer bounds/overflow check */
	    buffersize = 3*zsh->stream.avail_in;
	}
	Tcl_Panic("StreamAdd/Inflate - not implemented yet");
    }
    return TCL_OK;
}
#endif /* DISABLED_CODE */

int
Tcl_ZlibStreamPut(
    Tcl_ZlibStream zshandle,	/* As obtained from Tcl_ZlibStreamInit */
    Tcl_Obj *data,		/* Data to compress/decompress */
    int flush)			/* 0, ZLIB_FLUSH, ZLIB_FULLFLUSH,
				 * ZLIB_FINALIZE */
{
    zlibStreamHandle *zsh = (zlibStreamHandle *) zshandle;
    char *dataTmp = NULL;
    int e, size, outSize;
    Tcl_Obj *obj;

    if (zsh->streamend) {
	if (zsh->interp) {
	    Tcl_SetResult(zsh->interp, "already past compressed stream end",
		    TCL_STATIC);
	}
	return TCL_ERROR;
    }

    if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	zsh->stream.next_in = Tcl_GetByteArrayFromObj(data, &size);
	zsh->stream.avail_in = size;

	/*
	 * Deflatebound doesn't seem to take various header sizes into
	 * account, so we add 100 extra bytes.
	 */

	outSize = deflateBound(&zsh->stream, zsh->stream.avail_in) + 100;
	zsh->stream.avail_out = outSize;
	dataTmp = ckalloc(zsh->stream.avail_out);
	zsh->stream.next_out = (Bytef *) dataTmp;

	e = deflate(&zsh->stream, flush);
	if ((e==Z_OK || e==Z_BUF_ERROR) && (zsh->stream.avail_out == 0)) {
	    if (outSize - zsh->stream.avail_out > 0) {
		/*
		 * Output buffer too small.
		 */

		obj = Tcl_NewByteArrayObj((unsigned char *) dataTmp,
			outSize - zsh->stream.avail_out);
		/*
		 * Now append the compressed data to the outbuffer.
		 */

		Tcl_ListObjAppendElement(zsh->interp, zsh->outdata, obj);
	    }
	    if (outSize < 0xFFFF) {
		outSize = 0xFFFF;	/* There may be *lots* of data left to
					 * output... */
		ckfree(dataTmp);
		dataTmp = ckalloc(outSize);
	    }
	    zsh->stream.avail_out = outSize;
	    zsh->stream.next_out = (Bytef *) dataTmp;

	    e = deflate(&zsh->stream, flush);
	}

	/*
	 * And append the final data block.
	 */

	if (outSize - zsh->stream.avail_out > 0) {
	    obj = Tcl_NewByteArrayObj((unsigned char *) dataTmp,
		    outSize - zsh->stream.avail_out);

	    /*
	     * Now append the compressed data to the outbuffer.
	     */

	    Tcl_ListObjAppendElement(zsh->interp, zsh->outdata, obj);
	}
    } else {
	/*
	 * This is easy. Just append to inbuffer.
	 */

	Tcl_ListObjAppendElement(zsh->interp, zsh->indata, data);

	/*
	 * and we'll need the flush parameter for the Inflate call.
	 */

	zsh->flush = flush;
    }

    return TCL_OK;
}

int
Tcl_ZlibStreamGet(
    Tcl_ZlibStream zshandle,	/* As obtained from Tcl_ZlibStreamInit */
    Tcl_Obj *data,		/* A place to put the data */
    int count)			/* Number of bytes to grab as a maximum, you
				 * may get less! */
{
    zlibStreamHandle *zsh = (zlibStreamHandle *) zshandle;
    int e, i, listLen, itemLen, dataPos = 0;
    Tcl_Obj *itemObj;
    unsigned char *dataPtr, *itemPtr;

    /*
     * Getting beyond the of stream, just return empty string.
     */

    if (zsh->streamend) {
	return TCL_OK;
    }

    if (zsh->mode == TCL_ZLIB_STREAM_INFLATE) {
	if (count == -1) {
	    /*
	     * The only safe thing to do is restict to 65k. We might cause a
	     * panic for out of memory if we just kept growing the buffer.
	     */

	    count = 65536;
	}

	/*
	 * Prepare the place to store the data.
	 */

	dataPtr = Tcl_SetByteArrayLength(data, count);

	zsh->stream.next_out = dataPtr;
	zsh->stream.avail_out = count;
	if (zsh->stream.avail_in == 0) {
	    /*
	     * zlib will probably need more data to decompress.
	     */

	    if (zsh->current_input) {
		Tcl_DecrRefCount(zsh->current_input);
		zsh->current_input=0;
	    }
	    if (Tcl_ListObjLength(zsh->interp, zsh->indata,
		    &listLen) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (listLen > 0) {
		/*
		 * There is more input available, get it from the list and
		 * give it to zlib.
		 */

		if (Tcl_ListObjIndex(zsh->interp, zsh->indata, 0,
			&itemObj) != TCL_OK) {
		    return TCL_ERROR;
		}
		itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
		Tcl_IncrRefCount(itemObj);
		zsh->current_input = itemObj;
		zsh->stream.next_in = itemPtr;
		zsh->stream.avail_in = itemLen;

		/*
		 * And remove it from the list
		 */

		Tcl_ListObjReplace(NULL, zsh->indata, 0, 1, 0, NULL);
		listLen--;
	    }
	}

	e = inflate(&zsh->stream, zsh->flush);
	if (Tcl_ListObjLength(zsh->interp, zsh->indata, &listLen) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*printf("listLen %d, e==%d, avail_out %d\n", listLen, e, zsh->stream.avail_out);*/
	while ((zsh->stream.avail_out > 0) && (e==Z_OK || e==Z_BUF_ERROR)
		&& (listLen > 0)) {
	    /*
	     * State: We have not satisfied the request yet and there may be
	     * more to inflate.
	     */

	    if (zsh->stream.avail_in > 0) {
		if (zsh->interp) {
		    Tcl_SetResult(zsh->interp,
			"Unexpected zlib internal state during decompression",
			TCL_STATIC);
		}
		return TCL_ERROR;
	    }

	    if (zsh->current_input) {
		Tcl_DecrRefCount(zsh->current_input);
		zsh->current_input = 0;
	    }

	    if (Tcl_ListObjIndex(zsh->interp, zsh->indata, 0,
		   &itemObj) != TCL_OK) {
		return TCL_ERROR;
	    }
	    itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
	    Tcl_IncrRefCount(itemObj);
	    zsh->current_input = itemObj;
	    zsh->stream.next_in = itemPtr;
	    zsh->stream.avail_in = itemLen;

	    /*
	     * And remove it from the list.
	     */

	    Tcl_ListObjReplace(NULL, zsh->indata, 0, 1, 0, NULL);
	    listLen--;

	    /*
	     * And call inflate again
	     */

	    e = inflate(&zsh->stream, zsh->flush);
	}
	if (zsh->stream.avail_out > 0) {
	    Tcl_SetByteArrayLength(data, count - zsh->stream.avail_out);
	}
	if (!(e==Z_OK || e==Z_STREAM_END || e==Z_BUF_ERROR)) {
	    if (zsh->interp) {
		Tcl_SetResult(zsh->interp, zsh->stream.msg, TCL_VOLATILE);
	    }
	    return TCL_ERROR;
	}
	if (e == Z_STREAM_END) {
	    zsh->streamend = 1;
	    if (zsh->current_input) {
		Tcl_DecrRefCount(zsh->current_input);
		zsh->current_input = 0;
	    }
	    inflateEnd(&zsh->stream);
	}
    } else {
	if (Tcl_ListObjLength(zsh->interp, zsh->outdata,
		&listLen) != TCL_OK) {
	    return TCL_ERROR;
	}

	if (count == -1) {
	    count = 0;
	    for (i=0; i<listLen; i++) {
		if (Tcl_ListObjIndex(zsh->interp, zsh->outdata, i,
			&itemObj) != TCL_OK) {
		    return TCL_ERROR;
		}
		itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
		if (i == 0) {
		    count += itemLen - zsh->outpos;
		} else {
		    count += itemLen;
		}
	    }
	}

	/*
	 * Prepare the place to store the data.
	 */

	dataPtr = Tcl_SetByteArrayLength(data, count);

	while ((count > dataPos) && (Tcl_ListObjLength(zsh->interp,
		zsh->outdata, &listLen) == TCL_OK) && (listLen > 0)) {
	    Tcl_ListObjIndex(zsh->interp, zsh->outdata, 0, &itemObj);
	    itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
	    if (itemLen-zsh->outpos >= count-dataPos) {
		unsigned len = count - dataPos;

		memcpy(dataPtr + dataPos, itemPtr + zsh->outpos, len);
		zsh->outpos += len;
		dataPos += len;
		if (zsh->outpos == itemLen) {
		    zsh->outpos = 0;
		}
	    } else {
		unsigned len = itemLen - zsh->outpos;

		memcpy(dataPtr + dataPos, itemPtr + zsh->outpos, len);
		dataPos += len;
		zsh->outpos = 0;
	    }
	    if (zsh->outpos == 0) {
		Tcl_ListObjReplace(NULL, zsh->outdata, 0, 1, 0, NULL);
		listLen--;
	    }
	}
	Tcl_SetByteArrayLength(data, dataPos);
    }
    return TCL_OK;
}

/*
 * Deflate the contents of Tcl_Obj *data with compression level in output
 * format.
 */

int
Tcl_ZlibDeflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *data,
    int level,
    Tcl_Obj *gzipHeaderDictObj)
{
    int wbits = 0, bdlen = 0, e = 0;
    Byte *bdata = 0;
    z_stream stream;
    Tcl_Obj *obj;

    /*
     * We pass the data back in the interp result obj...
     */

    if (!interp) {
	return TCL_ERROR;
    }
    obj = Tcl_GetObjResult(interp);

    /*
     * Compressed format is specified by the wbits parameter. See zlib.h for
     * details.
     */

    if (format == TCL_ZLIB_FORMAT_RAW) {
	wbits = -MAX_WBITS;
    } else if (format == TCL_ZLIB_FORMAT_GZIP) {
	wbits = MAX_WBITS + 16;
    } else if (format == TCL_ZLIB_FORMAT_ZLIB) {
	wbits = MAX_WBITS;
    } else {
	Tcl_Panic("incorrect zlib data format, must be TCL_ZLIB_FORMAT_ZLIB, "
		"TCL_ZLIB_FORMAT_GZIP or TCL_ZLIB_FORMAT_ZLIB");
    }

    if (level < -1 || level > 9) {
	Tcl_Panic("compression level should be between 0 (uncompressed) and "
		"9 (best compression) or -1 for default compression level");
    }

    /*
     * Obtain the pointer to the byte array, we'll pass this pointer straight
     * to the deflate command.
     */

    bdata = Tcl_GetByteArrayFromObj(data, &bdlen);
    stream.avail_in = (uInt) bdlen;
    stream.next_in = bdata;
    stream.zalloc = 0;
    stream.zfree = 0;
    stream.opaque = 0;			/* Must be initialized before calling
					 * deflateInit2 */

    /*
     * No output buffer available yet, will alloc after deflateInit2.
     */

    stream.avail_out = 0;
    stream.next_out = NULL;

    e = deflateInit2(&stream, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL,
	    Z_DEFAULT_STRATEGY);

    if (e != Z_OK) {
	Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Allocate the output buffer from the value of deflateBound(). This is
     * probably too much space. Before returning to the caller, we will reduce
     * it back to the actual compressed size.
     */

    stream.avail_out = deflateBound(&stream, bdlen);
    /* TODO: What happens if this next call fails? */
    Tcl_SetByteArrayLength(obj, stream.avail_out);

    /*
     * And point the output buffer to the obj buffer.
     */

    stream.next_out = Tcl_GetByteArrayFromObj(obj, NULL);

    /*
     * Perform the compression, Z_FINISH means do it in one go.
     */

    e = deflate(&stream, Z_FINISH);

    if (e != Z_STREAM_END) {
	deflateEnd(&stream);

	/*
	 * deflateEnd() returns Z_OK when there are bytes left to compress, at
	 * this point we consider that an error, although we could continue by
	 * allocating more memory and calling deflate() again.
	 */

	if (e == Z_OK) {
	    e = Z_BUF_ERROR;
	}
    } else {
	e = deflateEnd(&stream);
    }

    if (e != Z_OK) {
	Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Reduce the BA length to the actual data length produced by deflate.
     */

    Tcl_SetByteArrayLength(obj, stream.total_out);

    return TCL_OK;
}

int
Tcl_ZlibInflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *data,
    int buffersize,
    Tcl_Obj *gzipHeaderDictObj)
{
    int wbits = 0 , inlen = 0, e = 0, newbuffersize;
    Byte *indata = NULL, *outdata = NULL, *newoutdata = NULL;
    z_stream stream;
    Tcl_Obj *obj;

    /*
     * We pass the data back in the interp result obj...
     */

    if (!interp) {
	return TCL_ERROR;
    }
    obj = Tcl_GetObjResult(interp);

    /*
     * Compressed format is specified by the wbits parameter. See zlib.h for
     * details.
     */

    if (format == TCL_ZLIB_FORMAT_RAW) {
	wbits = -MAX_WBITS;
    } else if (format == TCL_ZLIB_FORMAT_GZIP) {
	wbits = MAX_WBITS+16;
    } else if (format == TCL_ZLIB_FORMAT_ZLIB) {
	wbits = MAX_WBITS;
    } else if (format == TCL_ZLIB_FORMAT_AUTO) {
	wbits = MAX_WBITS+32;
    } else {
	Tcl_Panic("incorrect zlib data format, must be TCL_ZLIB_FORMAT_ZLIB, "
	      "TCL_ZLIB_FORMAT_GZIP, TCL_ZLIB_FORMAT_RAW or ZLIB_FORMAT_AUTO");
    }

    indata = Tcl_GetByteArrayFromObj(data,&inlen);
    if (buffersize == 0) {
	/*
	 * Start with a buffer 3 times the size of the input data.
	 *
	 * TODO: integer bounds/overflow check
	 */

	buffersize = 3*inlen;
    }

    outdata = Tcl_SetByteArrayLength(obj, buffersize);
    stream.zalloc = 0;
    stream.zfree = 0;
    stream.avail_in = (uInt) inlen+1;	/* +1 because ZLIB can "over-request"
					 * input (but ignore it!) */
    stream.next_in = indata;
    stream.avail_out = buffersize;
    stream.next_out = outdata;

    /*
     * Start the decompression cycle.
     */

    e = inflateInit2(&stream, wbits);
    if (e != Z_OK) {
	Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }
    while (1) {
	e = inflate(&stream, Z_FINISH);
	if (e != Z_BUF_ERROR) {
	    break;
	}

	/*
	 * Not enough room in the output buffer. Increase it by five times the
	 * bytes still in the input buffer. (Because 3 times didn't do the
	 * trick before, 5 times is what we do next.) Further optimization
	 * should be done by the user, specify the decompressed size!
	 */

	if ((stream.avail_in == 0) && (stream.avail_out > 0)) {
	    Tcl_SetResult(interp, "decompression failed, input truncated?",
		    TCL_STATIC);
	    return TCL_ERROR;
	}
	newbuffersize = buffersize + 5 * stream.avail_in;
	if (newbuffersize == buffersize) {
	    newbuffersize = buffersize+1000;
	}
	newoutdata = Tcl_SetByteArrayLength(obj, newbuffersize);

	/*
	 * Set next out to the same offset in the new location.
	 */

	stream.next_out = newoutdata + stream.total_out;

	/*
	 * And increase avail_out with the number of new bytes allocated.
	 */

	stream.avail_out += newbuffersize - buffersize;
	outdata = newoutdata;
	buffersize = newbuffersize;
    }

    if (e != Z_STREAM_END) {
	inflateEnd(&stream);
	Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }

    e = inflateEnd(&stream);
    if (e != Z_OK) {
	Tcl_SetResult(interp, (char*) zError(e), TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Reduce the BA length to the actual data length produced by deflate.
     */

    Tcl_SetByteArrayLength(obj, stream.total_out);
    return TCL_OK;
}

unsigned int
Tcl_ZlibCRC32(
    unsigned int crc,
    const char *buf,
    int len)
{
    /* Nothing much to do, just wrap the crc32(). */
    return crc32(crc, (Bytef *) buf, (unsigned) len);
}

unsigned int
Tcl_ZlibAdler32(
    unsigned int adler,
    const char *buf,
    int len)
{
    return adler32(adler, (Bytef *) buf, (unsigned) len);
}

static int
ZlibCmd(
    ClientData notUsed,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    int command, dlen, mode, format;
#ifdef TCLKIT_BUILD
    int wbits = -MAX_WBITS;
#endif
    unsigned start, level = -1, buffersize = 0;
    Tcl_ZlibStream zh;
    Byte *data;
    Tcl_Obj *obj = Tcl_GetObjResult(interp);
    static const char *commands[] = {
	"adler32", "compress", "crc32", "decompress", "deflate", "gunzip",
	"gzip", "inflate",
#ifdef TCLKIT_BUILD
	"sdecompress", "sinflate",
#endif
	"stack", "stream", "unstack",
	NULL
    };
    enum zlibCommands {
	z_adler32, z_compress, z_crc32, z_decompress, z_deflate, z_gunzip,
	z_gzip, z_inflate,
#ifdef TCLKIT_BUILD
	z_sdecompress, z_sinflate,
#endif
	z_stack, z_stream, z_unstack
    };
    static const char *stream_formats[] = {
	"compress", "decompress", "deflate", "gunzip", "gzip", "inflate",
	NULL
    };
    enum zlibFormats {
	f_compress, f_decompress, f_deflate, f_gunzip, f_gzip, f_inflate
    };

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "command arg ?...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], commands, "command", 0,
	    &command) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum zlibCommands) command) {
    case z_adler32: /* adler32 str ?startvalue? -> checksum */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?startValue?");
	    return TCL_ERROR;
	}
	if (objc>3 && Tcl_GetIntFromObj(interp, objv[3],
		(int *) &start) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc < 4) {
	    start = Tcl_ZlibAdler32(0, 0, 0);
	}
	data = Tcl_GetByteArrayFromObj(objv[2], &dlen);
	Tcl_SetIntObj(obj, (int)
		Tcl_ZlibAdler32(start, (const char *) data, dlen));
	return TCL_OK;
    case z_crc32: /* crc32 str ?startvalue? -> checksum */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?startValue?");
	    return TCL_ERROR;
	}
	if (objc>3 && Tcl_GetIntFromObj(interp, objv[3],
		(int *) &start) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (objc < 4) {
	    start = Tcl_ZlibCRC32(0, 0, 0);
	}
	data = Tcl_GetByteArrayFromObj(objv[2],&dlen);
	Tcl_SetIntObj(obj, (int)
		Tcl_ZlibCRC32(start, (const char *) data, dlen));
	return TCL_OK;
    case z_deflate: /* deflate data ?level? -> rawCompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?level?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3], (int *)&level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_RAW, objv[2], level,
		NULL);
    case z_compress: /* compress data ?level? -> zlibCompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?level?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3], (int *)&level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_ZLIB, objv[2], level,
		NULL);
    case z_gzip: /* gzip data ?level? -> gzippedCompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?level?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3], (int *)&level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_GZIP, objv[2], level,
		NULL);
    case z_inflate:		/* inflate rawcomprdata ?bufferSize?
				 *	-> decompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?bufferSize?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3],
		    (int *) &buffersize) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (buffersize < 16 || buffersize > 65536) {
		goto badBuffer;
	    }
	}
	return Tcl_ZlibInflate(interp, TCL_ZLIB_FORMAT_RAW, objv[2],
		buffersize, NULL);
    case z_decompress:		/* decompress zlibcomprdata ?bufferSize?
				 *	-> decompressedData */
	/*
	 * We rely on TCL_ZLIB_FORMAT_AUTO to determine type.
	 */
    case z_gunzip:		/* gunzip gzippeddata ?bufferSize?
				 *	-> decompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?bufferSize?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3],
		    (int *) &buffersize) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (buffersize < 16 || buffersize > 65536) {
		goto badBuffer;
	    }
	}
	return Tcl_ZlibInflate(interp, TCL_ZLIB_FORMAT_AUTO, objv[2],
		buffersize, NULL);
    case z_stream: /* stream deflate/inflate/...gunzip ?level?*/
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "mode ?level?");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], stream_formats,
		"stream format", 0, &format) != TCL_OK) {
	    Tcl_AppendResult(interp, "format error: integer", NULL);
	    return TCL_ERROR;
	}
	mode = TCL_ZLIB_STREAM_INFLATE;
	switch ((enum zlibFormats) format) {
	case f_deflate:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	case f_inflate:
	    format = TCL_ZLIB_FORMAT_RAW;
	    break;
	case f_compress:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	case f_decompress:
	    format = TCL_ZLIB_FORMAT_ZLIB;
	    break;
	case f_gzip:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	case f_gunzip:
	    format = TCL_ZLIB_FORMAT_GZIP;
	    break;
	}
	if (objc == 4) {
	    if (Tcl_GetIntFromObj(interp, objv[3],
		    (int *) &level) != TCL_OK) {
		Tcl_AppendResult(interp, "level error: integer", NULL);
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	} else {
	    level = Z_DEFAULT_COMPRESSION;
	}
	if (Tcl_ZlibStreamInit(interp, mode, format, level, NULL,
		&zh) != TCL_OK) {
	    Tcl_AppendResult(interp, "stream init error: integer", NULL);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_ZlibStreamGetCommandName(zh));
	return TCL_OK;
    case z_stack: /* stack */
	break;
    case z_unstack: /* unstack */
	break;
#ifdef TCLKIT_BUILD
    case z_sdecompress: /* sdecompress cmdname -> */
	wbits = MAX_WBITS;
    case z_sinflate: {/* sinflate cmdname -> */
	zlibstream *zp = (zlibstream *) ckalloc(sizeof(zlibstream));

	zp->indata = Tcl_NewObj();
	Tcl_IncrRefCount(zp->indata);
	zp->stream.zalloc = 0;
	zp->stream.zfree = 0;
	zp->stream.opaque = 0;
	zp->stream.next_in = 0;
	zp->stream.avail_in = 0;
	inflateInit2(&zp->stream, wbits);
	Tcl_CreateObjCommand(interp, Tcl_GetStringFromObj(objv[2], 0),
		zstreamincmd, zp, zstreamdelproc);
	return TCL_OK;
    }
#endif /* TCLKIT_BUILD */
    };

    return TCL_ERROR;

  badLevel:
    Tcl_AppendResult(interp, "level must be 0 to 9", NULL);
    return TCL_ERROR;
  badBuffer:
    Tcl_AppendResult(interp, "buffer size must be 32 to 65536", NULL);
    return TCL_ERROR;
}

static int
ZlibStreamCmd(
    ClientData cd,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_ZlibStream zstream = cd;
    int command, index, count;
    Tcl_Obj *obj = Tcl_GetObjResult(interp);
    int buffersize;
    int flush = -1, i;
    static const char *cmds[] = {
	"add", "adler32", "close", "eof", "finalize", "flush",
	"fullflush", "get", "put", "reset",
	NULL
    };
    enum zlibStreamCommands {
	zs_add, zs_adler32, zs_close, zs_eof, zs_finalize, zs_flush,
	zs_fullflush, zs_get, zs_put, zs_reset
    };
    static const char *add_options[] = {
	"-buffer", "-finalize", "-flush", "-fullflush", NULL
    };
    enum addOptions {
	ao_buffer, ao_finalize, ao_flush, ao_fullflush
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option data ?...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], cmds, "option", 0,
	    &command) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum zlibStreamCommands) command) {
    case zs_add: /* add ?-flush|-fullflush|-finalize? /data/ */
	for (i=2; i<objc-1; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], add_options, "option", 0,
		    &index) != TCL_OK) {
		return TCL_ERROR;
	    }

	    switch ((enum addOptions) index) {
	    case ao_flush: /* -flush */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_SYNC_FLUSH;
		}
		break;
	    case ao_fullflush: /* -fullflush */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_FULL_FLUSH;
		}
		break;
	    case ao_finalize: /* -finalize */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_FINISH;
		}
		break;
	    case ao_buffer: /* -buffer */
		if (i == objc-2) {
		    Tcl_AppendResult(interp, "\"-buffer\" option must be "
			    "followed by integer decompression buffersize",
			    NULL);
		    return TCL_ERROR;
		}
		if (Tcl_GetIntFromObj(interp, objv[i+1],
			&buffersize) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }

	    if (flush == -2) {
		Tcl_AppendResult(interp, "\"-flush\", \"-fullflush\" and "
			"\"-finalize\" options are mutually exclusive", NULL);
		return TCL_ERROR;
	    }
	}
	if (flush == -1) {
	    flush = 0;
	}

	if (Tcl_ZlibStreamPut(zstream, objv[objc-1],
		flush) != TCL_OK) {
	    return TCL_ERROR;
	}
	return Tcl_ZlibStreamGet(zstream, obj, -1);

    case zs_put: /* put ?-flush|-fullflush|-finalize? /data/ */
	for (i=2; i<objc-1; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], add_options, "option", 0,
		    &index) != TCL_OK) {
		return TCL_ERROR;
	    }

	    switch ((enum addOptions) index) {
	    case ao_flush: /* -flush */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_SYNC_FLUSH;
		}
		break;
	    case ao_fullflush: /* -fullflush */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_FULL_FLUSH;
		}
		break;
	    case ao_finalize: /* -finalize */
		if (flush > -1) {
		    flush = -2;
		} else {
		    flush = Z_FINISH;
		}
		break;
	    case ao_buffer:
		Tcl_AppendResult(interp,
			"\"-buffer\" option not supported here", NULL);
		return TCL_ERROR;
	    }
	    if (flush == -2) {
		Tcl_AppendResult(interp, "\"-flush\", \"-fullflush\" and "
			"\"-finalize\" options are mutually exclusive", NULL);
		return TCL_ERROR;
	    }
	}
	if (flush == -1) {
	    flush = 0;
	}
	return Tcl_ZlibStreamPut(zstream, objv[objc-1], flush);

    case zs_get: /* get ?count? */
	count = -1;
	if (objc >= 3) {
	    if (Tcl_GetIntFromObj(interp, objv[2], &count) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	return Tcl_ZlibStreamGet(zstream, obj, count);
    case zs_flush: /* flush */
	Tcl_SetObjLength(obj, 0);
	return Tcl_ZlibStreamPut(zstream, obj, Z_SYNC_FLUSH);
    case zs_fullflush: /* fullflush */
	Tcl_SetObjLength(obj, 0);
	return Tcl_ZlibStreamPut(zstream, obj, Z_FULL_FLUSH);
    case zs_finalize: /* finalize */
	/*
	 * The flush commands slightly abuse the empty result obj as input
	 * data.
	 */

	Tcl_SetObjLength(obj, 0);
	return Tcl_ZlibStreamPut(zstream, obj, Z_FINISH);
    case zs_close: /* close */
	return Tcl_ZlibStreamClose(zstream);
    case zs_eof: /* eof */
	Tcl_SetIntObj(obj, Tcl_ZlibStreamEof(zstream));
	return TCL_OK;
    case zs_adler32: /* adler32 */
	Tcl_SetIntObj(obj, Tcl_ZlibStreamAdler32(zstream));
	return TCL_OK;
    case zs_reset: /* reset */
	return Tcl_ZlibStreamReset(zstream);
    }

    return TCL_OK;
}

#ifdef ENABLE_CHANSTACKING
 /*
  * Set of functions to support channel stacking
  */

static int
ChanClose(
    ClientData instanceData,
    Tcl_Interp *interp)
{
    Zlib_ChannelData *cd = instanceData;
    Tcl_Channel parent;
    int e;

    parent = Tcl_GetStackedChannel(cd->channel);

    if (cd->inFormat != ZLIB_PASSTHROUGH) {
	if (cd->inFormat && ZLIB_INFLATE) {
	    e = inflateEnd(&cd->instream);
	} else {
	    e = deflateEnd(&cd->instream);
	}
    }

    if (cd->outFormat != ZLIB_PASSTHROUGH) {
	if (cd->outFormat && ZLIB_INFLATE) {
	    e = inflateEnd(&cd->outstream);
	} else {
	    e = deflateEnd(&cd->outstream);
	}
    }

    if (cd->inbuffer) {
	ckfree(cd->inbuffer);
	cd->inbuffer = NULL;
    }

    if (cd->outbuffer) {
	ckfree(cd->outbuffer);
	cd->outbuffer = NULL;
    }
    return TCL_OK;
}

static int
ChanInput(
    ClientData instanceData,
    char *buf,
    int toRead,
    int *errorCodePtr)
{
    Zlib_ChannelData *cd = instanceData;

    return TCL_OK;
}

static int
ChanOutput(
    ClientData instanceData,
    const char *buf,
    int toWrite,
    int *errorCodePtr)
{
    Zlib_ChannelData *cd = instanceData;

    return TCL_OK;
}

static int
ChanSeek(
    ClientData instanceData,
    long offset,
    int mode,
    int *errorCodePtr)
{
    Zlib_ChannelData *cd = instanceData;

    return TCL_OK;
}

static int
ChanSetOption(			/* not used */
    ClientData instanceData,
    Tcl_Interp *interp,
    const char *optionName,
    const char *value)
{
    Zlib_ChannelData *cd = instanceData;
    Tcl_Channel parent = Tcl_GetStackedChannel(cd->channel);
    Tcl_DriverSetOptionProc *setOptionProc =
	    Tcl_ChannelSetOptionProc(Tcl_GetChannelType(parent));

    if (setOptionProc == NULL) {
	return TCL_ERROR;
    }

    return setOptionProc(Tcl_GetChannelInstanceData(parent), interp,
	    optionName, value);
}

static int
ChanGetOption(			/* not used */
    ClientData instanceData,
    Tcl_Interp *interp,
    const char *optionName,
    Tcl_DString *dsPtr)
{
    return TCL_OK;
}

static void
ChanWatch(
    ClientData instanceData,
    int mask)
{
    return;
}

static int
ChanGetHandle(
    ClientData instanceData,
    int direction,
    ClientData *handlePtr)
{
    /*
     * No such thing as an OS handle for Zlib.
     */

    return 0;
}

static int
ChanClose2(			/* not used */
    ClientData instanceData,
    Tcl_Interp *interp,
    int flags)
{
    return TCL_OK;
}

static int
ChanBlockMode(
    ClientData instanceData,
    int mode)
{
    Zlib_ChannelData *cd = instanceData;

    if (mode == TCL_MODE_NONBLOCKING) {
	cd->flags |= ASYNC;
    } else {
	cd->flags &= ~ASYNC;
    }
    return TCL_OK;
}

static int
ChanFlush(
    ClientData instanceData)
{
    Zlib_ChannelData *cd = instanceData;

    return TCL_OK;
}

static int
ChanHandler(
    ClientData instanceData,
    int interestMask)
{
    Zlib_ChannelData *cd = instanceData;

    return TCL_OK;
}

Tcl_WideInt
ChanWideSeek(			/* not used */
    ClientData instanceData,
    Tcl_WideInt offset,
    int mode,
    int *errorCodePtr)
{
    return TCL_OK;
}

Tcl_Channel
Tcl_ZlibStackChannel(
    Tcl_Interp *interp,
    int inFormat,
    int inLevel,
    int outFormat,
    int outLevel,
    Tcl_Channel channel)
{
    Zlib_ChannelData *cd;
    int outwbits = 0, inwbits = 0;
    int e;

    if (inFormat & ZLIB_FORMAT_RAW) {
	inwbits = -MAX_WBITS;
    } else if (inFormat & ZLIB_FORMAT_GZIP) {
	inwbits = MAX_WBITS+16;
    } else if (inFormat & ZLIB_FORMAT_ZLIB) {
	inwbits = MAX_WBITS;
    } else if ((inFormat & ZLIB_FORMAT_AUTO) && (inFormat & ZLIB_INFLATE)) {
	inwbits = MAX_WBITS+32;
    } else if (inFormat != ZLIB_PASSTHROUGH) {
	Tcl_SetResult(interp, "Incorrect zlib read/input data format, must "
		"be ZLIB_FORMAT_ZLIB, ZLIB_FORMAT_GZIP, ZLIB_FORMAT_RAW or "
		"ZLIB_FORMAT_AUTO (only for inflate).", TCL_STATIC);
	return NULL;
    }

    if (outFormat & ZLIB_FORMAT_RAW) {
	outwbits = -MAX_WBITS;
    } else if (outFormat & ZLIB_FORMAT_GZIP) {
	outwbits = MAX_WBITS+16;
    } else if (outFormat & ZLIB_FORMAT_ZLIB) {
	outwbits = MAX_WBITS;
    } else if ((outFormat & ZLIB_FORMAT_AUTO) && (outFormat & ZLIB_INFLATE)) {
	outwbits = MAX_WBITS+32;
    } else if (outFormat != ZLIB_PASSTHROUGH) {
	Tcl_SetResult(interp, "Incorrect zlib write/output data format, must "
		"be ZLIB_FORMAT_ZLIB, ZLIB_FORMAT_GZIP, ZLIB_FORMAT_RAW or "
		"ZLIB_FORMAT_AUTO (only for inflate).", TCL_STATIC);
	return NULL;
    }

    cd = (Zlib_ChannelData *) ckalloc(sizeof(Zlib_ChannelData));
    cd->inFormat = inFormat;
    cd->outFormat = outFormat;

    cd->instream.zalloc = 0;
    cd->instream.zfree = 0;
    cd->instream.opaque = 0;
    cd->instream.avail_in = 0;
    cd->instream.next_in = NULL;
    cd->instream.avail_out = 0;
    cd->instream.next_out = NULL;

    cd->outstream.zalloc = 0;
    cd->outstream.zfree = 0;
    cd->outstream.opaque = 0;
    cd->outstream.avail_in = 0;
    cd->outstream.next_in = NULL;
    cd->outstream.avail_out = 0;
    cd->outstream.next_out = NULL;

    if (inFormat != ZLIB_PASSTHROUGH) {
	if (inFormat & ZLIB_INFLATE) {
	    /* Initialize for Inflate */
	    e = inflateInit2(&cd->instream, inwbits);
	} else {
	    /* Initialize for Deflate */
	    e = deflateInit2(&cd->instream, inLevel, Z_DEFLATED, inwbits,
		    MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	}
    }

    if (outFormat != ZLIB_PASSTHROUGH) {
	if (outFormat && ZLIB_INFLATE) {
	    /* Initialize for Inflate */
	    e = inflateInit2(&cd->outstream, outwbits);
	} else {
	    /* Initialize for Deflate */
	    e = deflateInit2(&cd->outstream, outLevel, Z_DEFLATED, outwbits,
		    MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	}
    }

    cd->channel = Tcl_StackChannel(interp, &zlibChannelType, cd,
	    TCL_READABLE | TCL_WRITABLE | TCL_EXCEPTION, channel);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_GetChannelName(channel),
	    -1));
    return channel;
}

#endif /* ENABLE_CHANSTACKING */

/*
 * Finaly, the TclZlibInit function. Used to install the zlib API.
 */

int
TclZlibInit(
    Tcl_Interp *interp)
{
    Tcl_Eval(interp, "namespace eval ::zlib {variable cmdcounter 0}");
    Tcl_CreateObjCommand(interp, "zlib", ZlibCmd, 0, 0);
    return TCL_OK;
}
#endif /* REIMPLEMENT */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
