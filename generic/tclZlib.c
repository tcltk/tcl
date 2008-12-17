/*
 * tclZlib.c --
 *
 *	This file provides the interface to the Zlib library.
 *
 * Copyright (C) 2004-2005 Pascal Scheffers <pascal@scheffers.net>
 * Copyright (C) 2005 Unitas Software B.V.
 * Copyright (c) 2008 Donal K. Fellows
 *
 * Parts written by Jean-Claude Wippler, as part of Tclkit, placed in the
 * public domain March 2003.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclZlib.c,v 1.14 2008/12/17 16:47:38 nijtmans Exp $
 */

#include "tclInt.h"
#ifdef HAVE_ZLIB
#include <zlib.h>
/* #define ENABLE_CHANSTACKING */

#define GZIP_MAGIC_FLAG	16
#define AUTO_MAGIC_FLAG 32

/*
 * Structure used for handling gzip headers that are generated from a
 * dictionary.
 */

#define MAX_COMMENT_LEN		256

typedef struct {
    gz_header header;
    char nativeFilenameBuf[MAXPATHLEN];
    char nativeCommentBuf[MAX_COMMENT_LEN];
} GzipHeader;

/*
 * Structure used for the Tcl_ZlibStream* commands and [zlib stream ...]
 */

typedef struct {
    Tcl_Interp *interp;
    z_stream stream;
    int streamEnd;
    Tcl_Obj *inData, *outData;	/* Input / output buffers (lists) */
    Tcl_Obj *currentInput;	/* Pointer to what is currently being
				 * inflated. */
    int inPos, outPos;
    int mode;			/* Either TCL_ZLIB_STREAM_DEFLATE or
				 * TCL_ZLIB_STREAM_INFLATE. */
    int format;			/* Flags from the TCL_ZLIB_FORMAT_* */
    int level;			/* Default 5, 0-9 */
    int flush;			/* Stores the flush param for deferred the
				 * decompression. */
    int wbits;
    Tcl_Command cmd;		/* Token for the associated Tcl command. */
} zlibStreamHandle;

/*
 * Prototypes for private procedures defined later in this file:
 */

static void		ConvertError(Tcl_Interp *interp, int code);
static void		ExtractHeader(gz_header *headerPtr, Tcl_Obj *dictObj);
static int		GenerateHeader(Tcl_Interp *interp, Tcl_Obj *dictObj,
			    GzipHeader *headerPtr, int *extraSizePtr);
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
static int		ChanSetOption(ClientData instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    const char *value);
static int		ChanGetOption(ClientData instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    Tcl_DString *dsPtr);
static void		ChanWatch(ClientData instanceData, int mask);
static int		ChanGetHandle(ClientData instanceData, int direction,
			    ClientData *handlePtr);
static int		ChanBlockMode(ClientData instanceData, int mode);
static int		ChanFlush(ClientData instanceData);
static int		ChanHandler(ClientData instanceData,
			    int interestMask);
static Tcl_Channel	ZlibStackChannel(Tcl_Interp *interp, int mode,
			    int format, int level, Tcl_Channel channel,
			    Tcl_Obj *gzipHeaderDictPtr);

static const Tcl_ChannelType zlibChannelType = {
    "zlib",
    TCL_CHANNEL_VERSION_3,
    ChanClose,
    ChanInput,
    ChanOutput,
    NULL,			/* seekProc */
    ChanSetOption,
    ChanGetOption,
    ChanWatch,
    ChanGetHandle,
    NULL,			/* close2Proc, */
    ChanBlockMode,
    ChanFlush,
    NULL /*ChanHandler*/,
    NULL			/* wideSeekProc */
};

typedef struct {
    /* Generic channel info */
    Tcl_Channel parent;
    int flags;
    int mask;

    /* Zlib specific channel state */
    int mode;			/* Either the value TCL_ZLIB_STREAM_DEFLATE
				 * for compression on output, or
				 * TCL_ZLIB_STREAM_INFLATE for decompression
				 * on input. */
    z_stream inStream;		/* Structure used by zlib for decompression of
				 * input. */
    z_stream outStream;		/* Structure used by zlib for compression of
				 * output. */
    char *inBuffer;
    int inAllocated, inUsed, inPos;
    char *outBuffer;
    int outAllocated, outUsed, outPos;
    int flushType;

    GzipHeader inHeader;
    GzipHeader outHeader;
} ZlibChannelData;

/*
 * Value bits for the flags field. Definitions are:
 *	ASYNC -		Whether this is an asynchronous channel.
 *	IN_HEADER -	Whether the inHeader field has been registered with
 *			the input compressor.
 *	OUT_HEADER -	Whether the outputHeader field has been registered
 *			with the output decompressor.
 */

#define ASYNC		0x1
#define IN_HEADER	0x2
#define OUT_HEADER	0x4

/*
 * Size of buffers allocated by default. Should be enough...
 */

#define DEFAULT_BUFFER_SIZE	4096
#endif /* ENABLE_CHANSTACKING */

/*
 *----------------------------------------------------------------------
 *
 * ConvertError --
 *
 *	Utility function for converting a zlib error into a Tcl error.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the interpreter result and errorcode.
 *
 *----------------------------------------------------------------------
 */

static void
ConvertError(
    Tcl_Interp *interp,		/* Interpreter to store the error in. May be
				 * NULL, in which case nothing happens. */
    int code)			/* The zlib error code. */
{
    if (interp == NULL) {
	return;
    }

    if (code == Z_ERRNO) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_PosixError(interp),-1));
    } else {
	const char *codeStr, *codeStr2 = NULL;
	char codeStrBuf[TCL_INTEGER_SPACE];

	switch (code) {
	case Z_STREAM_ERROR:	codeStr = "STREAM";	break;
	case Z_DATA_ERROR:	codeStr = "DATA";	break;
	case Z_MEM_ERROR:	codeStr = "MEM";	break;
	case Z_BUF_ERROR:	codeStr = "BUF";	break;
	case Z_VERSION_ERROR:	codeStr = "VERSION";	break;
	default:
	    codeStr = "unknown";
	    codeStr2 = codeStrBuf;
	    sprintf(codeStrBuf, "%d", code);
	    break;
	}
	Tcl_SetObjResult(interp, Tcl_NewStringObj(zError(code), -1));
	Tcl_SetErrorCode(interp, "TCL", "ZLIB", codeStr, codeStr2, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateHeader --
 *
 *	Function for creating a gzip header from the contents of a dictionary
 *	(as described in the documentation). GetValue is a helper function.
 *
 * Results:
 *	A Tcl result code.
 *
 * Side effects:
 *	Updates the fields of the given gz_header structure. Adds amount of
 *	extra space required for the header to the variable referenced by the
 *	extraSizePtr argument.
 *
 *----------------------------------------------------------------------
 */

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
    Tcl_Interp *interp,		/* Where to put error messages. */
    Tcl_Obj *dictObj,		/* The dictionary whose contents are to be
				 * parsed. */
    GzipHeader *headerPtr,	/* Where to store the parsed-out values. */
    int *extraSizePtr)		/* Variable to add the length of header
				 * strings (filename, comment) to. */
{
    Tcl_Obj *value;
    int len, result = TCL_ERROR;
    char *valueStr;
    Tcl_Encoding latin1enc;
    static const char *const types[] = {
	"binary", "text"
    };

    /*
     * RFC 1952 says that header strings are in ISO 8859-1 (LATIN-1).
     */

    latin1enc = Tcl_GetEncoding(NULL, "iso8859-1");
    if (latin1enc == NULL) {
	Tcl_Panic("no latin-1 encoding");
    }

    if (GetValue(interp, dictObj, "comment", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL) {
	valueStr = Tcl_GetStringFromObj(value, &len);
	Tcl_UtfToExternal(NULL, latin1enc, valueStr, len, 0, NULL,
		headerPtr->nativeCommentBuf, MAX_COMMENT_LEN-1, NULL, &len,
		NULL);
	headerPtr->nativeCommentBuf[len] = '\0';
	headerPtr->header.comment = (Bytef *) headerPtr->nativeCommentBuf;
	*extraSizePtr += len;
    }

    if (GetValue(interp, dictObj, "crc", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL &&
	    Tcl_GetBooleanFromObj(interp, value, &headerPtr->header.hcrc)) {
	goto error;
    }

    if (GetValue(interp, dictObj, "filename", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL) {
	valueStr = Tcl_GetStringFromObj(value, &len);
	Tcl_UtfToExternal(NULL, latin1enc, valueStr, len, 0, NULL,
		headerPtr->nativeFilenameBuf, MAXPATHLEN-1, NULL, &len, NULL);
	headerPtr->nativeFilenameBuf[len] = '\0';
	headerPtr->header.name = (Bytef *) headerPtr->nativeFilenameBuf;
	*extraSizePtr += len;
    }

    if (GetValue(interp, dictObj, "os", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL && Tcl_GetIntFromObj(interp, value,
	    &headerPtr->header.os) != TCL_OK) {
	goto error;
    }

    /*
     * Ignore the 'size' field, since that is controlled by the size of the
     * input data.
     */

    if (GetValue(interp, dictObj, "time", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL && Tcl_GetLongFromObj(interp, value,
	    (long *) &headerPtr->header.time) != TCL_OK) {
	goto error;
    }

    if (GetValue(interp, dictObj, "type", &value) != TCL_OK) {
	goto error;
    } else if (value != NULL && Tcl_GetIndexFromObj(interp, value, types,
	    "type", TCL_EXACT, &headerPtr->header.text) != TCL_OK) {
	goto error;
    }

    result = TCL_OK;
  error:
    Tcl_FreeEncoding(latin1enc);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ExtractHeader --
 *
 *	Take the values out of a gzip header and store them in a dictionary.
 *	SetValue is a helper function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Updates the dictionary, which must be writable (i.e. refCount < 2).
 *
 *----------------------------------------------------------------------
 */

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
    gz_header *headerPtr,	/* The gzip header to extract from. */
    Tcl_Obj *dictObj)		/* The dictionary to store in. */
{
    Tcl_Encoding latin1enc = NULL;
    Tcl_DString tmp;

    if (headerPtr->comment != Z_NULL) {
	if (latin1enc == NULL) {
	    /*
	     * RFC 1952 says that header strings are in ISO 8859-1 (LATIN-1).
	     */

	    latin1enc = Tcl_GetEncoding(NULL, "iso8859-1");
	    if (latin1enc == NULL) {
		Tcl_Panic("no latin-1 encoding");
	    }
	}

	Tcl_ExternalToUtfDString(latin1enc, (char *) headerPtr->comment, -1,
		&tmp);
	SetValue(dictObj, "comment", Tcl_NewStringObj(Tcl_DStringValue(&tmp),
		Tcl_DStringLength(&tmp)));
	Tcl_DStringFree(&tmp);
    }
    SetValue(dictObj, "crc", Tcl_NewBooleanObj(headerPtr->hcrc));
    if (headerPtr->name != Z_NULL) {
	if (latin1enc == NULL) {
	    /*
	     * RFC 1952 says that header strings are in ISO 8859-1 (LATIN-1).
	     */

	    latin1enc = Tcl_GetEncoding(NULL, "iso8859-1");
	    if (latin1enc == NULL) {
		Tcl_Panic("no latin-1 encoding");
	    }
	}

	Tcl_ExternalToUtfDString(latin1enc, (char *) headerPtr->name, -1,
		&tmp);
	SetValue(dictObj, "filename", Tcl_NewStringObj(Tcl_DStringValue(&tmp),
		Tcl_DStringLength(&tmp)));
	Tcl_DStringFree(&tmp);
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

    if (latin1enc != NULL) {
	Tcl_FreeEncoding(latin1enc);
    }
}

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
    int mode,			/* Either TCL_ZLIB_STREAM_INFLATE or
				 * TCL_ZLIB_STREAM_DEFLATE. */
    int format,			/* Flags from the TCL_ZLIB_FORMAT_* set. */
    int level,			/* 0-9 or TCL_ZLIB_COMPRESS_DEFAULT. */
    Tcl_Obj *dictObj,		/* Dictionary containing headers for gzip. */
    Tcl_ZlibStream *zshandle)
{
    int wbits = 0;
    int e;
    zlibStreamHandle *zsh = NULL;
    Tcl_DString cmdname;
    Tcl_CmdInfo cmdinfo;

    switch (mode) {
    case TCL_ZLIB_STREAM_DEFLATE:
	/*
	 * Compressed format is specified by the wbits parameter. See zlib.h
	 * for details.
	 */

	switch (format) {
	case TCL_ZLIB_FORMAT_RAW:
	    wbits = -MAX_WBITS;
	    break;
	case TCL_ZLIB_FORMAT_GZIP:
	    wbits = MAX_WBITS | GZIP_MAGIC_FLAG;
	    break;
	case TCL_ZLIB_FORMAT_ZLIB:
	    wbits = MAX_WBITS;
	    break;
	default:
	    Tcl_Panic("incorrect zlib data format, must be "
		    "TCL_ZLIB_FORMAT_ZLIB, TCL_ZLIB_FORMAT_GZIP or "
		    "TCL_ZLIB_FORMAT_RAW");
	}
	if (level < -1 || level > 9) {
	    Tcl_Panic("compression level should be between 0 (no compression)"
		    " and 9 (best compression) or -1 for default compression "
		    "level");
	}
	break;
    case TCL_ZLIB_STREAM_INFLATE:
	/*
	 * wbits are the same as DEFLATE, but FORMAT_AUTO is valid too.
	 */

	switch (format) {
	case TCL_ZLIB_FORMAT_RAW:
	    wbits = -MAX_WBITS;
	    break;
	case TCL_ZLIB_FORMAT_GZIP:
	    wbits = MAX_WBITS | GZIP_MAGIC_FLAG;
	    break;
	case TCL_ZLIB_FORMAT_ZLIB:
	    wbits = MAX_WBITS;
	    break;
	case TCL_ZLIB_FORMAT_AUTO:
	    wbits = MAX_WBITS | AUTO_MAGIC_FLAG;
	    break;
	default:
	    Tcl_Panic("incorrect zlib data format, must be "
		    "TCL_ZLIB_FORMAT_ZLIB, TCL_ZLIB_FORMAT_GZIP, "
		    "TCL_ZLIB_FORMAT_RAW or TCL_ZLIB_FORMAT_AUTO");
	}
	break;
    default:
	Tcl_Panic("bad mode, must be TCL_ZLIB_STREAM_DEFLATE or"
		" TCL_ZLIB_STREAM_INFLATE");
    }

    zsh = (zlibStreamHandle *) ckalloc(sizeof(zlibStreamHandle));
    zsh->interp = interp;
    zsh->mode = mode;
    zsh->format = format;
    zsh->level = level;
    zsh->wbits = wbits;
    zsh->currentInput = NULL;
    zsh->streamEnd = 0;
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
	ConvertError(interp, e);
	goto error;
    }

    /*
     * I could do all this in C, but this is easier.
     */

    if (interp != NULL) {
	if (Tcl_Eval(interp, "incr ::tcl::zlib::cmdcounter") != TCL_OK) {
	    goto error;
	}
	Tcl_DStringInit(&cmdname);
	Tcl_DStringAppend(&cmdname, "::tcl::zlib::streamcmd_", -1);
	Tcl_DStringAppend(&cmdname, Tcl_GetString(Tcl_GetObjResult(interp)),
		-1);
	if (Tcl_GetCommandInfo(interp, Tcl_DStringValue(&cmdname),
		&cmdinfo) == 1) {
	    Tcl_SetResult(interp,
		    "BUG: Stream command name already exists", TCL_STATIC);
	    Tcl_DStringFree(&cmdname);
	    goto error;
	}
	Tcl_ResetResult(interp);

	/*
	 * Create the command.
	 */

	zsh->cmd = Tcl_CreateObjCommand(interp, Tcl_DStringValue(&cmdname),
		ZlibStreamCmd, zsh, ZlibStreamCmdDelete);
	Tcl_DStringFree(&cmdname);
	if (zsh->cmd == NULL) {
	    goto error;
	}
    } else {
	zsh->cmd = NULL;
    }

    /*
     * Prepare the buffers for use.
     */

    zsh->inData = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(zsh->inData);
    zsh->outData = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(zsh->outData);

    zsh->inPos = 0;
    zsh->outPos = 0;

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

    zsh->cmd = NULL;
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

    if (zsh->interp && zsh->cmd) {
	Tcl_DeleteCommandFromToken(zsh->interp, zsh->cmd);
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
    if (!zsh->streamEnd) {
	if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	    deflateEnd(&zsh->stream);
	} else {
	    inflateEnd(&zsh->stream);
	}
    }

    if (zsh->inData) {
	Tcl_DecrRefCount(zsh->inData);
    }
    if (zsh->outData) {
	Tcl_DecrRefCount(zsh->outData);
    }
    if (zsh->currentInput) {
	Tcl_DecrRefCount(zsh->currentInput);
    }

    ckfree((char *) zsh);
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

    if (!zsh->streamEnd) {
	if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	    deflateEnd(&zsh->stream);
	} else {
	    inflateEnd(&zsh->stream);
	}
    }
    Tcl_SetByteArrayLength(zsh->inData, 0);
    Tcl_SetByteArrayLength(zsh->outData, 0);
    if (zsh->currentInput) {
	Tcl_DecrRefCount(zsh->currentInput);
	zsh->currentInput = NULL;
    }

    zsh->inPos = 0;
    zsh->outPos = 0;
    zsh->streamEnd = 0;
    zsh->stream.avail_in = 0;
    zsh->stream.next_in = 0;
    zsh->stream.zalloc = 0;
    zsh->stream.zfree = 0;
    zsh->stream.opaque = 0;		/* Must be initialized before calling
					 * (de|in)flateInit2 */

    /*
     * No output buffer available yet.
     */

    zsh->stream.avail_out = 0;
    zsh->stream.next_out = NULL;

    if (zsh->mode == TCL_ZLIB_STREAM_DEFLATE) {
	e = deflateInit2(&zsh->stream, zsh->level, Z_DEFLATED, zsh->wbits,
		MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    } else {
	e = inflateInit2(&zsh->stream, zsh->wbits);
    }

    if (e != Z_OK) {
	ConvertError(zsh->interp, e);
	/* TODO:cleanup */
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
    zlibStreamHandle *zsh = (zlibStreamHandle *) zshandle;
    Tcl_Obj *objPtr = Tcl_NewObj();

    Tcl_GetCommandFullName(zsh->interp, zsh->cmd, objPtr);
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ZlibStreamEof --
 *
 *	This procedure This function returns 0 or 1 depending on the state of
 *	the (de)compressor. For decompression, eof is reached when the entire
 *	compressed stream has been decompressed. For compression, eof is
 *	reached when the stream has been flushed with TCL_ZLIB_FINALIZE.
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

    return zsh->streamEnd;
}

int
Tcl_ZlibStreamAdler32(
    Tcl_ZlibStream zshandle)	/* As obtained from Tcl_ZlibStreamInit */
{
    zlibStreamHandle *zsh = (zlibStreamHandle*) zshandle;

    return zsh->stream.adler;
}

int
Tcl_ZlibStreamPut(
    Tcl_ZlibStream zshandle,	/* As obtained from Tcl_ZlibStreamInit */
    Tcl_Obj *data,		/* Data to compress/decompress */
    int flush)			/* TCL_ZLIB_NO_FLUSH, TCL_ZLIB_FLUSH,
				 * TCL_ZLIB_FULLFLUSH, or TCL_ZLIB_FINALIZE */
{
    zlibStreamHandle *zsh = (zlibStreamHandle *) zshandle;
    char *dataTmp = NULL;
    int e, size, outSize;
    Tcl_Obj *obj;

    if (zsh->streamEnd) {
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
		 * Now append the compressed data to the outData list.
		 */

		Tcl_ListObjAppendElement(zsh->interp, zsh->outData, obj);
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
	     * Now append the compressed data to the outData list.
	     */

	    Tcl_ListObjAppendElement(zsh->interp, zsh->outData, obj);
	}
    } else {
	/*
	 * This is easy. Just append to the inData list.
	 */

	Tcl_ListObjAppendElement(zsh->interp, zsh->inData, data);

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

    if (zsh->streamEnd) {
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

	    if (zsh->currentInput) {
		Tcl_DecrRefCount(zsh->currentInput);
		zsh->currentInput = NULL;
	    }
	    if (Tcl_ListObjLength(zsh->interp, zsh->inData,
		    &listLen) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (listLen > 0) {
		/*
		 * There is more input available, get it from the list and
		 * give it to zlib.
		 */

		if (Tcl_ListObjIndex(zsh->interp, zsh->inData, 0,
			&itemObj) != TCL_OK) {
		    return TCL_ERROR;
		}
		itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
		Tcl_IncrRefCount(itemObj);
		zsh->currentInput = itemObj;
		zsh->stream.next_in = itemPtr;
		zsh->stream.avail_in = itemLen;

		/*
		 * And remove it from the list
		 */

		Tcl_ListObjReplace(NULL, zsh->inData, 0, 1, 0, NULL);
		listLen--;
	    }
	}

	e = inflate(&zsh->stream, zsh->flush);
	if (Tcl_ListObjLength(zsh->interp, zsh->inData, &listLen) != TCL_OK) {
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

	    if (zsh->currentInput) {
		Tcl_DecrRefCount(zsh->currentInput);
		zsh->currentInput = 0;
	    }

	    if (Tcl_ListObjIndex(zsh->interp, zsh->inData, 0,
		   &itemObj) != TCL_OK) {
		return TCL_ERROR;
	    }
	    itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
	    Tcl_IncrRefCount(itemObj);
	    zsh->currentInput = itemObj;
	    zsh->stream.next_in = itemPtr;
	    zsh->stream.avail_in = itemLen;

	    /*
	     * And remove it from the list.
	     */

	    Tcl_ListObjReplace(NULL, zsh->inData, 0, 1, 0, NULL);
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
	    ConvertError(zsh->interp, e);
	    return TCL_ERROR;
	}
	if (e == Z_STREAM_END) {
	    zsh->streamEnd = 1;
	    if (zsh->currentInput) {
		Tcl_DecrRefCount(zsh->currentInput);
		zsh->currentInput = 0;
	    }
	    inflateEnd(&zsh->stream);
	}
    } else {
	if (Tcl_ListObjLength(zsh->interp, zsh->outData,
		&listLen) != TCL_OK) {
	    return TCL_ERROR;
	}

	if (count == -1) {
	    count = 0;
	    for (i=0; i<listLen; i++) {
		if (Tcl_ListObjIndex(zsh->interp, zsh->outData, i,
			&itemObj) != TCL_OK) {
		    return TCL_ERROR;
		}
		itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
		if (i == 0) {
		    count += itemLen - zsh->outPos;
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
		zsh->outData, &listLen) == TCL_OK) && (listLen > 0)) {
	    Tcl_ListObjIndex(zsh->interp, zsh->outData, 0, &itemObj);
	    itemPtr = Tcl_GetByteArrayFromObj(itemObj, &itemLen);
	    if (itemLen-zsh->outPos >= count-dataPos) {
		unsigned len = count - dataPos;

		memcpy(dataPtr + dataPos, itemPtr + zsh->outPos, len);
		zsh->outPos += len;
		dataPos += len;
		if (zsh->outPos == itemLen) {
		    zsh->outPos = 0;
		}
	    } else {
		unsigned len = itemLen - zsh->outPos;

		memcpy(dataPtr + dataPos, itemPtr + zsh->outPos, len);
		dataPos += len;
		zsh->outPos = 0;
	    }
	    if (zsh->outPos == 0) {
		Tcl_ListObjReplace(NULL, zsh->outData, 0, 1, 0, NULL);
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
    int wbits = 0, inLen = 0, e = 0, extraSize = 0;
    Byte *inData = NULL;
    z_stream stream;
    GzipHeader header;
    gz_header *headerPtr = NULL;
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
	wbits = MAX_WBITS | GZIP_MAGIC_FLAG;

	/*
	 * Need to allocate extra space for the gzip header and footer. The
	 * amount of space is (a bit less than) 32 bytes, plus a byte for each
	 * byte of string that we add. Note that over-allocation is not a
	 * problem. [Bug 2419061]
	 */

	extraSize = 32;
	if (gzipHeaderDictObj) {
	    headerPtr = &header.header;
	    memset(headerPtr, 0, sizeof(gz_header));
	    if (GenerateHeader(interp, gzipHeaderDictObj, &header,
		    &extraSize) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
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

    inData = Tcl_GetByteArrayFromObj(data, &inLen);
    stream.avail_in = (uInt) inLen;
    stream.next_in = inData;
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
	goto error;
    }

    if (headerPtr != NULL) {
	e = deflateSetHeader(&stream, headerPtr);
	if (e != Z_OK) {
	    goto error;
	}
    }

    /*
     * Allocate the output buffer from the value of deflateBound(). This is
     * probably too much space. Before returning to the caller, we will reduce
     * it back to the actual compressed size.
     */

    stream.avail_out = deflateBound(&stream, inLen) + extraSize;
    stream.next_out = Tcl_SetByteArrayLength(obj, stream.avail_out);

    /*
     * Perform the compression, Z_FINISH means do it in one go.
     */

    e = deflate(&stream, Z_FINISH);

    if (e != Z_STREAM_END) {
	e = deflateEnd(&stream);

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
	goto error;
    }

    /*
     * Reduce the bytearray length to the actual data length produced by
     * deflate.
     */

    Tcl_SetByteArrayLength(obj, stream.total_out);
    return TCL_OK;

  error:
    ConvertError(interp, e);
    return TCL_ERROR;
}

int
Tcl_ZlibInflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *data,
    int bufferSize,
    Tcl_Obj *gzipHeaderDictObj)
{
    int wbits = 0, inLen = 0, e = 0, newBufferSize;
    Byte *inData = NULL, *outData = NULL, *newOutData = NULL;
    z_stream stream;
    gz_header header, *headerPtr = NULL;
    Tcl_Obj *obj;
    char *nameBuf = NULL, *commentBuf = NULL;

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

    switch (format) {
    case TCL_ZLIB_FORMAT_RAW:
	wbits = -MAX_WBITS;
	gzipHeaderDictObj = NULL;
	break;
    case TCL_ZLIB_FORMAT_ZLIB:
	wbits = MAX_WBITS;
	gzipHeaderDictObj = NULL;
	break;
    case TCL_ZLIB_FORMAT_GZIP:
	wbits = MAX_WBITS | GZIP_MAGIC_FLAG;
	break;
    case TCL_ZLIB_FORMAT_AUTO:
	wbits = MAX_WBITS | AUTO_MAGIC_FLAG;
	break;
    default:
	Tcl_Panic("incorrect zlib data format, must be TCL_ZLIB_FORMAT_ZLIB, "
		"TCL_ZLIB_FORMAT_GZIP, TCL_ZLIB_FORMAT_RAW or "
		"TCL_ZLIB_FORMAT_AUTO");
    }

    if (gzipHeaderDictObj) {
	headerPtr = &header;
	memset(headerPtr, 0, sizeof(gz_header));
	nameBuf = ckalloc(MAXPATHLEN);
	header.name = (Bytef *) nameBuf;
	header.name_max = MAXPATHLEN - 1;
	commentBuf = ckalloc(MAX_COMMENT_LEN);
	header.comment = (Bytef *) commentBuf;
	header.comm_max = MAX_COMMENT_LEN - 1;
    }

    inData = Tcl_GetByteArrayFromObj(data, &inLen);
    if (bufferSize < 1) {
	/*
	 * Start with a buffer (up to) 3 times the size of the input data.
	 */

	if (inLen < 32*1024*1024) {
	    bufferSize = 3*inLen;
	} else if (inLen < 256*1024*1024) {
	    bufferSize = 2*inLen;
	} else {
	    bufferSize = inLen;
	}
    }

    outData = Tcl_SetByteArrayLength(obj, bufferSize);
    stream.zalloc = 0;
    stream.zfree = 0;
    stream.avail_in = (uInt) inLen+1;	/* +1 because zlib can "over-request"
					 * input (but ignore it!) */
    stream.next_in = inData;
    stream.avail_out = bufferSize;
    stream.next_out = outData;

    /*
     * Initialize zlib for decompression.
     */

    e = inflateInit2(&stream, wbits);
    if (e != Z_OK) {
	goto error;
    }
    if (headerPtr) {
	e = inflateGetHeader(&stream, headerPtr);
	if (e != Z_OK) {
	    goto error;
	}
    }

    /*
     * Start the decompression cycle.
     */

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
	    e = Z_STREAM_ERROR;
	    goto error;
	}
	newBufferSize = bufferSize + 5 * stream.avail_in;
	if (newBufferSize == bufferSize) {
	    newBufferSize = bufferSize+1000;
	}
	newOutData = Tcl_SetByteArrayLength(obj, newBufferSize);

	/*
	 * Set next out to the same offset in the new location.
	 */

	stream.next_out = newOutData + stream.total_out;

	/*
	 * And increase avail_out with the number of new bytes allocated.
	 */

	stream.avail_out += newBufferSize - bufferSize;
	outData = newOutData;
	bufferSize = newBufferSize;
    }

    if (e != Z_STREAM_END) {
	inflateEnd(&stream);
	goto error;
    }

    e = inflateEnd(&stream);
    if (e != Z_OK) {
	goto error;
    }

    /*
     * Reduce the BA length to the actual data length produced by deflate.
     */

    Tcl_SetByteArrayLength(obj, stream.total_out);
    if (headerPtr != NULL) {
	ExtractHeader(&header, gzipHeaderDictObj);
	SetValue(gzipHeaderDictObj, "size",
		Tcl_NewLongObj((long) stream.total_out));
	ckfree(nameBuf);
	ckfree(commentBuf);
    }
    return TCL_OK;

  error:
    ConvertError(interp, e);
    if (nameBuf) {
	ckfree(nameBuf);
    }
    if (commentBuf) {
	ckfree(commentBuf);
    }
    return TCL_ERROR;
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
    int command, dlen, mode, format, i, option, level = -1;
    unsigned start, buffersize = 0;
    Tcl_ZlibStream zh;
    Byte *data;
    Tcl_Obj *obj = Tcl_GetObjResult(interp);
    Tcl_Obj *headerDictObj, *headerVarObj;
    const char *extraInfoStr = NULL;
    static const char *const commands[] = {
	"adler32", "compress", "crc32", "decompress", "deflate", "gunzip",
	"gzip", "inflate", "push", "stream",
	NULL
    };
    enum zlibCommands {
	z_adler32, z_compress, z_crc32, z_decompress, z_deflate, z_gunzip,
	z_gzip, z_inflate, z_push, z_stream,
    };
    static const char *const stream_formats[] = {
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
    case z_adler32:			/* adler32 str ?startvalue?
					 * -> checksum */
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
    case z_crc32:			/* crc32 str ?startvalue?
					 * -> checksum */
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
    case z_deflate:			/* deflate data ?level?
					 * -> rawCompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?level?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3], &level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_RAW, objv[2], level,
		NULL);
    case z_compress:			/* compress data ?level?
					 * -> zlibCompressedData */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?level?");
	    return TCL_ERROR;
	}
	if (objc > 3) {
	    if (Tcl_GetIntFromObj(interp, objv[3], &level) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (level < 0 || level > 9) {
		goto badLevel;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_ZLIB, objv[2], level,
		NULL);
    case z_gzip:			/* gzip data ?level?
					 * -> gzippedCompressedData */
	if (objc > 7 || ((objc & 1) == 0)) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "data ?-level level? ?-header header?");
	    return TCL_ERROR;
	}
	headerDictObj = NULL;
	for (i=3 ; i<objc ; i+=2) {
	    static const char *gzipopts[] = {
		"-header", "-level", NULL
	    };

	    if (Tcl_GetIndexFromObj(interp, objv[i], gzipopts, "option", 0,
		    &option) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch (option) {
	    case 0:
		headerDictObj = objv[i+1];
		break;
	    case 1:
		if (Tcl_GetIntFromObj(interp, objv[i+1],
			&level) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (level < 0 || level > 9) {
		    extraInfoStr = "\n    (in -level option)";
		    goto badLevel;
		}
		break;
	    }
	}
	return Tcl_ZlibDeflate(interp, TCL_ZLIB_FORMAT_GZIP, objv[2], level,
		headerDictObj);
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
	return Tcl_ZlibInflate(interp, TCL_ZLIB_FORMAT_ZLIB, objv[2],
		buffersize, NULL);
    case z_gunzip:		/* gunzip gzippeddata ?bufferSize?
				 *	-> decompressedData */
	if (objc > 5 || ((objc & 1) == 0)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?-headerVar varName?");
	    return TCL_ERROR;
	}
	headerDictObj = headerVarObj = NULL;
	for (i=3 ; i<objc ; i+=2) {
	    static const char *gunzipopts[] = {
		"-buffersize", "-headerVar", NULL
	    };

	    if (Tcl_GetIndexFromObj(interp, objv[i], gunzipopts, "option", 0,
		    &option) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch (option) {
	    case 0:
		if (Tcl_GetIntFromObj(interp, objv[i+1],
			(int *) &buffersize) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (buffersize < 16 || buffersize > 65536) {
		    goto badBuffer;
		}
		break;
	    case 1:
		headerVarObj = objv[i+1];
		headerDictObj = Tcl_NewObj();
		break;
	    }
	}
	if (Tcl_ZlibInflate(interp, TCL_ZLIB_FORMAT_GZIP, objv[2],
		buffersize, headerDictObj) != TCL_OK) {
	    if (headerDictObj) {
		TclDecrRefCount(headerDictObj);
	    }
	    return TCL_ERROR;
	}
	if (headerVarObj != NULL && Tcl_ObjSetVar2(interp, headerVarObj, NULL,
		headerDictObj, TCL_LEAVE_ERR_MSG) == NULL) {
	    if (headerDictObj) {
		TclDecrRefCount(headerDictObj);
	    }
	    return TCL_ERROR;
	}
	return TCL_OK;
    case z_stream: /* stream deflate/inflate/...gunzip ?level?*/
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "mode ?level?");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], stream_formats, "mode", 0,
		&format) != TCL_OK) {
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
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_ZlibStreamGetCommandName(zh));
	return TCL_OK;
#ifdef ENABLE_CHANSTACKING
    case z_push: {			/* push mode channel options...*/
	Tcl_Channel chan;
	int chanMode, mode;
	static const char *pushOptions[] = {
	    "-header", "-level", "-limit",
	    NULL
	};
	enum pushOptions {poHeader, poLevel, poLimit};
	Tcl_Obj *headerObj = NULL;
	int limit = 1, dummy;

	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "mode channel ?options...?");
	    return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[2], stream_formats, "mode", 0,
		&format) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum zlibFormats) format) {
	case f_deflate:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	    format = TCL_ZLIB_FORMAT_GZIP;
	    break;
	case f_inflate:
	    mode = TCL_ZLIB_STREAM_INFLATE;
	    format = TCL_ZLIB_FORMAT_RAW;
	    break;
	case f_compress:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	    format = TCL_ZLIB_FORMAT_ZLIB;
	    break;
	case f_decompress:
	    mode = TCL_ZLIB_STREAM_INFLATE;
	    format = TCL_ZLIB_FORMAT_ZLIB;
	    break;
	case f_gzip:
	    mode = TCL_ZLIB_STREAM_DEFLATE;
	    format = TCL_ZLIB_FORMAT_GZIP;
	    break;
	case f_gunzip:
	    mode = TCL_ZLIB_STREAM_INFLATE;
	    format = TCL_ZLIB_FORMAT_GZIP;
	    break;
	default:
	    Tcl_AppendResult(interp, "IMPOSSIBLE", NULL);
	    return TCL_ERROR;
	}

	if (TclGetChannelFromObj(interp, objv[3], &chan, &chanMode,
		0) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * Sanity checks.
	 */

	if (mode == TCL_ZLIB_STREAM_DEFLATE && !(chanMode & TCL_WRITABLE)) {
	    Tcl_AppendResult(interp,
		    "compression may only be applied to writable channels",
		    NULL);
	    return TCL_ERROR;
	}
	if (mode == TCL_ZLIB_STREAM_INFLATE && !(chanMode & TCL_READABLE)) {
	    Tcl_AppendResult(interp,
		    "decompression may only be applied to readable channels",
		    NULL);
	    return TCL_ERROR;
	}

	/*
	 * Parse options.
	 */

	level = Z_DEFAULT_COMPRESSION;
	for (i=4 ; i<objc ; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], pushOptions, "option", 0,
		    &option) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch ((enum pushOptions) option) {
	    case poHeader:
		if (++i > objc-1) {
		    Tcl_AppendResult(interp,
			    "value missing for -header option", NULL);
		    return TCL_ERROR;
		}
		headerObj = objv[i];
		if (Tcl_DictObjSize(interp, headerObj, &dummy) != TCL_OK) {
		    Tcl_AddErrorInfo(interp, "\n    (in -header option)");
		    return TCL_ERROR;
		}
		break;
	    case poLevel:
		if (++i > objc-1) {
		    Tcl_AppendResult(interp,
			    "value missing for -level option", NULL);
		    return TCL_ERROR;
		}
		if (Tcl_GetIntFromObj(interp, objv[i],
			(int *) &level) != TCL_OK) {
		    Tcl_AddErrorInfo(interp, "\n    (in -level option)");
		    return TCL_ERROR;
		}
		if (level < 0 || level > 9) {
		    extraInfoStr = "\n    (in -level option)";
		    goto badLevel;
		}
		break;
	    case poLimit:
		if (++i > objc-1) {
		    Tcl_AppendResult(interp,
			    "value missing for -limit option", NULL);
		    return TCL_ERROR;
		}
		if (Tcl_GetIntFromObj(interp, objv[i],
			(int *) &limit) != TCL_OK) {
		    Tcl_AddErrorInfo(interp, "\n    (in -limit option)");
		    return TCL_ERROR;
		}
		if (limit < 1) {
		    limit = 1;
		}
		break;
	    }
	}

	if (ZlibStackChannel(interp, mode, format, level, chan,
		headerObj) == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[3]);
	return TCL_OK;
    }
#else /*ENABLE_CHANSTACKING*/
    case z_push:
	Tcl_AppendResult(interp, "unimplemented", NULL);
	return TCL_ERROR;
#endif /*ENABLE_CHANSTACKING*/
    };

    return TCL_ERROR;

  badLevel:
    Tcl_AppendResult(interp, "level must be 0 to 9", NULL);
    if (extraInfoStr) {
	Tcl_AddErrorInfo(interp, extraInfoStr);
    }
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
    static const char *const cmds[] = {
	"add", "adler32", "close", "eof", "finalize", "flush",
	"fullflush", "get", "put", "reset",
	NULL
    };
    enum zlibStreamCommands {
	zs_add, zs_adler32, zs_close, zs_eof, zs_finalize, zs_flush,
	zs_fullflush, zs_get, zs_put, zs_reset
    };
    static const char *const add_options[] = {
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
 * Set of functions to support channel stacking.
 */

static int
ChanClose(
    ClientData instanceData,
    Tcl_Interp *interp)
{
    ZlibChannelData *cd = instanceData;
    int e;

    if (cd->mode == TCL_ZLIB_STREAM_DEFLATE) {
	e = deflateEnd(&cd->inStream);
    } else {
	e = inflateEnd(&cd->outStream);
    }

    if (cd->inBuffer) {
	ckfree(cd->inBuffer);
	cd->inBuffer = NULL;
    }

    if (cd->outBuffer) {
	ckfree(cd->outBuffer);
	cd->outBuffer = NULL;
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
    ZlibChannelData *cd = instanceData;
    Tcl_DriverInputProc *inProc =
	    Tcl_ChannelInputProc(Tcl_GetChannelType(cd->parent));

    if (cd->mode == TCL_ZLIB_STREAM_DEFLATE) {
	return inProc(Tcl_GetChannelInstanceData(cd->parent), buf, toRead,
		errorCodePtr);
    }

#if 0
    cd->inStream.avail_in = 0;
    do {
	cd->inStream.next_out = (Bytef *) cd->inBuffer;
	cd->inStream.avail_out = cd->inAllocated;

	if (inflate(&cd->inStream, Z_SYNC_FLUSH) != Z_OK) {
	    *errorCodePtr = EINVAL;
	    return 0;
	}
    } while (cd->inStream.avail_out > 0);
#endif
    // TODO
    *errorCodePtr = EINVAL;
    return 0;
}

static int
ChanOutput(
    ClientData instanceData,
    const char *buf,
    int toWrite,
    int *errorCodePtr)
{
    ZlibChannelData *cd = instanceData;
    Tcl_DriverOutputProc *outProc =
	    Tcl_ChannelOutputProc(Tcl_GetChannelType(cd->parent));
    int e;

    if (cd->mode == TCL_ZLIB_STREAM_INFLATE) {
	return outProc(Tcl_GetChannelInstanceData(cd->parent), buf, toWrite,
		errorCodePtr);
    }

    cd->outStream.next_in = (Bytef *) buf;
    cd->outStream.avail_in = toWrite;
    do {
	cd->outStream.next_out = (Bytef *) cd->outBuffer;
	cd->outStream.avail_out = cd->outAllocated;

	e = deflate(&cd->outStream, Z_NO_FLUSH);

	if (e == Z_OK && cd->outStream.avail_out > 0) {
	    if (Tcl_WriteRaw(cd->parent, cd->outBuffer,
		    (int) cd->outStream.avail_out) < 0) {
		*errorCodePtr = Tcl_GetErrno();
		return 0;
	    }
	}
    } while (e == Z_OK && cd->outStream.avail_in > 0);

    if (e != Z_OK) {
	*errorCodePtr = EINVAL;
	return 0;
    }
	
    return 1;
}

static int
ChanSetOption(			/* not used */
    ClientData instanceData,
    Tcl_Interp *interp,
    const char *optionName,
    const char *value)
{
    ZlibChannelData *cd = instanceData;
    Tcl_DriverSetOptionProc *setOptionProc =
	    Tcl_ChannelSetOptionProc(Tcl_GetChannelType(cd->parent));
    static const char *chanOptions = "flushmode";

    if (optionName && strcmp(optionName, "-flushmode") == 0) {
	if (value[0] == 'f' && strcmp(value, "full") == 0) {
	    cd->flushType = Z_FULL_FLUSH;
	    return TCL_OK;
	}
	if (value[0] == 's' && strcmp(value, "sync") == 0) {
	    cd->flushType = Z_SYNC_FLUSH;
	    return TCL_OK;
	}
	Tcl_AppendResult(interp, "unknown -flushmode \"", value,
		"\": must be full or sync", NULL);
	return TCL_ERROR;
    }

    if (setOptionProc == NULL) {
	return Tcl_BadChannelOption(interp, optionName, chanOptions);
    }

    return setOptionProc(Tcl_GetChannelInstanceData(cd->parent), interp,
	    optionName, value);
}

static int
ChanGetOption(
    ClientData instanceData,
    Tcl_Interp *interp,
    const char *optionName,
    Tcl_DString *dsPtr)
{
    ZlibChannelData *cd = instanceData;
    Tcl_DriverGetOptionProc *getOptionProc =
	    Tcl_ChannelGetOptionProc(Tcl_GetChannelType(cd->parent));
    static const char *chanOptions = "crc flushmode header";

    /*
     * The "crc" option reports the current CRC (calculated with the Adler32
     * or CRC32 algorithm according to the format) given the data that has
     * been processed so far.
     */

    if (optionName == NULL || strcmp(optionName, "-crc") == 0) {
	uLong crc;
	char buf[12];

	// TODO: flush?

	if (cd->mode == TCL_ZLIB_STREAM_DEFLATE) {
	    crc = cd->outStream.adler;
	} else {
	    crc = cd->inStream.adler;
	}

	sprintf(buf, "0x%lx", crc);
	if (optionName == NULL) {
	    Tcl_DStringAppendElement(dsPtr, "-crc");
	    Tcl_DStringAppendElement(dsPtr, buf);
	} else {
	    Tcl_DStringAppend(dsPtr, buf, -1);
	    return TCL_OK;
	}
    }

    /*
     * The "flushmode" option reports how the [flush] command will actually
     * effect the channel.
     */

    if (optionName == NULL || strcmp(optionName, "-flushmode") == 0) {
	char *value;

	if (cd->flushType == Z_FULL_FLUSH) {
	    value = "full";
	} else {
	    value = "sync";
	}

	if (optionName == NULL) {
	    Tcl_DStringAppendElement(dsPtr, "-flushmode");
	    Tcl_DStringAppendElement(dsPtr, value);
	} else {
	    Tcl_DStringAppend(dsPtr, value, -1);
	    return TCL_OK;
	}
    }

    /*
     * The "header" option, which is only valid on inflating gzip channels,
     * reports the header that has been read from the start of the stream.
     */

    if ((cd->flags & IN_HEADER) && ((optionName == NULL) ||
	    (strcmp(optionName, "-header") == 0))) {
	Tcl_Obj *tmpObj = Tcl_NewObj();

	ExtractHeader(&cd->inHeader.header, tmpObj);
	if (optionName == NULL) {
	    Tcl_DStringAppendElement(dsPtr, "-header");
	    Tcl_DStringAppendElement(dsPtr, Tcl_GetString(tmpObj));
	    Tcl_DecrRefCount(tmpObj);
	} else {
	    int len;
	    char *str = Tcl_GetStringFromObj(tmpObj, &len);

	    Tcl_DStringAppend(dsPtr, str, len);
	    Tcl_DecrRefCount(tmpObj);
	    return TCL_OK;
	}
    }

    /*
     * Now we do the standard processing of the stream we wrapped.
     */

    if (getOptionProc) {
	return getOptionProc(Tcl_GetChannelInstanceData(cd->parent),
		interp, optionName, dsPtr);
    }
    if (optionName == NULL) {
	return TCL_OK;
    }
    return Tcl_BadChannelOption(interp, optionName, chanOptions);
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
    ZlibChannelData *cd = instanceData;

    return Tcl_GetChannelHandle(cd->parent, direction, handlePtr);
}

static int
ChanBlockMode(
    ClientData instanceData,
    int mode)
{
    ZlibChannelData *cd = instanceData;

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
    ZlibChannelData *cd = instanceData;

    if (cd->mode == TCL_ZLIB_STREAM_DEFLATE) {
	cd->outStream.avail_in = 0;
	do {
	    cd->outStream.next_out = (Bytef *) cd->outBuffer;
	    cd->outStream.avail_out = cd->outAllocated;

	    if (deflate(&cd->outStream, cd->flushType) != Z_OK) {
		Tcl_SetErrno(EINVAL);
		return 0;
	    }

	    if (cd->outStream.avail_out > 0) {
		if (Tcl_WriteRaw(cd->parent, cd->outBuffer,
			(int) cd->outStream.next_out) < 0) {
		    return 0;
		}
	    }
	} while (cd->outStream.avail_out > 0);
    }
    return 1;
}

static int
ChanHandler(
    ClientData instanceData,
    int interestMask)
{
    /*
     * We don't handle this here. Assume it came from the underlying channel.
     */

    return interestMask;
}

static Tcl_Channel
ZlibStackChannel(
    Tcl_Interp *interp,
    int mode,
    int format,
    int level,
    Tcl_Channel channel,
    Tcl_Obj *gzipHeaderDictPtr)
{
    ZlibChannelData *cd = (ZlibChannelData *)
	    ckalloc(sizeof(ZlibChannelData));
    Tcl_Channel chan;
    int wbits = 0;
    int e;

    if (mode != TCL_ZLIB_STREAM_DEFLATE && mode != TCL_ZLIB_STREAM_INFLATE) {
	Tcl_Panic("unknown mode: %d", mode);
    }

    memset(cd, 0, sizeof(ZlibChannelData));
    cd->mode = mode;
    cd->flushType = Z_SYNC_FLUSH;

    if (format == TCL_ZLIB_FORMAT_GZIP || format == TCL_ZLIB_FORMAT_AUTO) {
	if (mode == TCL_ZLIB_STREAM_DEFLATE) {
	    int dummy = 0;

	    cd->flags |= OUT_HEADER;
	    if (GenerateHeader(interp, gzipHeaderDictPtr, &cd->outHeader,
		    &dummy) != TCL_OK) {
		goto error;
	    }
	} else {
	    cd->flags |= IN_HEADER;
	    cd->inHeader.header.name = (Bytef *)
		    &cd->inHeader.nativeFilenameBuf;
	    cd->inHeader.header.name_max = MAXPATHLEN - 1;
	    cd->inHeader.header.comment = (Bytef *)
		    &cd->inHeader.nativeCommentBuf;
	    cd->inHeader.header.comm_max = MAX_COMMENT_LEN - 1;
	}
    }

    if (format == TCL_ZLIB_FORMAT_RAW) {
	wbits = -MAX_WBITS;
    } else if (format == TCL_ZLIB_FORMAT_ZLIB) {
	wbits = MAX_WBITS;
    } else if (format == TCL_ZLIB_FORMAT_GZIP) {
	wbits = MAX_WBITS | GZIP_MAGIC_FLAG;
    } else if (format == TCL_ZLIB_FORMAT_AUTO) {
	wbits = MAX_WBITS | AUTO_MAGIC_FLAG;
    } else {
	Tcl_Panic("bad format: %d", format);
    }

    /*
     * Initialize input inflater or the output deflater.
     */

    if (mode == TCL_ZLIB_STREAM_INFLATE) {
	e = inflateInit2(&cd->inStream, wbits);
	if (e != Z_OK) {
	    goto error;
	}
	cd->inAllocated = DEFAULT_BUFFER_SIZE;
	cd->inBuffer = ckalloc(cd->inAllocated);
	if (cd->flags & IN_HEADER) {
	    e = inflateGetHeader(&cd->inStream, &cd->inHeader.header);
	    if (e != Z_OK) {
		goto error;
	    }
	}
    } else {
	e = deflateInit2(&cd->outStream, level, Z_DEFLATED, wbits,
		MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (e != Z_OK) {
	    goto error;
	}
	cd->outAllocated = DEFAULT_BUFFER_SIZE;
	cd->outBuffer = ckalloc(cd->outAllocated);
	if (cd->flags & OUT_HEADER) {
	    e = deflateSetHeader(&cd->outStream, &cd->outHeader.header);
	    if (e != Z_OK) {
		goto error;
	    }
	}
    }

    chan = Tcl_StackChannel(interp, &zlibChannelType, cd,
	    TCL_READABLE | TCL_WRITABLE, channel);
    if (chan == NULL) {
	goto error;
    }
    cd->parent = Tcl_GetStackedChannel(chan);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_GetChannelName(chan), -1));
    return chan;

  error:
    if (cd->inBuffer) {
	ckfree(cd->inBuffer);
	inflateEnd(&cd->inStream);
    }
    if (cd->outBuffer) {
	ckfree(cd->outBuffer);
	deflateEnd(&cd->outStream);
    }
    ckfree((char *) cd);
    return NULL;
}
#endif /* ENABLE_CHANSTACKING */

/*
 * Finally, the TclZlibInit function. Used to install the zlib API.
 */

int
TclZlibInit(
    Tcl_Interp *interp)
{
    Tcl_Eval(interp, "namespace eval ::tcl::zlib {variable cmdcounter 0}");
    Tcl_CreateObjCommand(interp, "zlib", ZlibCmd, 0, 0);
    return TCL_OK;
}
#else /* HAVE_ZLIB */
int
Tcl_ZlibStreamInit(
    Tcl_Interp *interp,
    int mode,
    int format,
    int level,
    Tcl_Obj *dictObj,
    Tcl_ZlibStream *zshandle)
{
    Tcl_SetResult(interp, "unimplemented", TCL_STATIC);
    return TCL_ERROR;
}

int
Tcl_ZlibStreamClose(
    Tcl_ZlibStream zshandle)
{
    return TCL_OK;
}

int
Tcl_ZlibStreamReset(
    Tcl_ZlibStream zshandle)
{
    return TCL_OK;
}

Tcl_Obj *
Tcl_ZlibStreamGetCommandName(
    Tcl_ZlibStream zshandle)
{
    return NULL;
}

int
Tcl_ZlibStreamEof(
    Tcl_ZlibStream zshandle)
{
    return 1;
}

int
Tcl_ZlibStreamAdler32(
    Tcl_ZlibStream zshandle)
{
    return 0;
}

int
Tcl_ZlibStreamPut(
    Tcl_ZlibStream zshandle,
    Tcl_Obj *data,
    int flush)
{
    return TCL_OK;
}

int
Tcl_ZlibStreamGet(
    Tcl_ZlibStream zshandle,
    Tcl_Obj *data,
    int count)
{
    return TCL_OK;
}

int
Tcl_ZlibDeflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *data,
    int level,
    Tcl_Obj *gzipHeaderDictObj)
{
    Tcl_SetResult(interp, "unimplemented", TCL_STATIC);
    return TCL_ERROR;
}

int
Tcl_ZlibInflate(
    Tcl_Interp *interp,
    int format,
    Tcl_Obj *data,
    int bufferSize,
    Tcl_Obj *gzipHeaderDictObj)
{
    Tcl_SetResult(interp, "unimplemented", TCL_STATIC);
    return TCL_ERROR;
}

unsigned int
Tcl_ZlibCRC32(
    unsigned int crc,
    const char *buf,
    int len)
{
    return 0;
}

unsigned int
Tcl_ZlibAdler32(
    unsigned int adler,
    const char *buf,
    int len)
{
    return 0;
}
#endif /* HAVE_ZLIB */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
