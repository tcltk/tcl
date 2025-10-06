/*
 * tclBinary.c --
 *
 *	This file contains the implementation of the "binary" Tcl built-in
 *	command and the Tcl binary data object.
 *
 * Copyright © 1997 Sun Microsystems, Inc.
 * Copyright © 1998-1999 Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclTomMath.h"

#include <math.h>

/*
 * The following constants are used by GetFormatSpec to indicate various
 * special conditions in the parsing of a format specifier.
 */
enum GetFormatSpecSpecialCounts {
    BINARY_ALL = -1,		/* Use all elements in the argument. */
    BINARY_NOCOUNT = -2		/* No count was specified in format. */
};

/*
 * The following flags may be OR'ed together and returned by GetFormatSpec
 */
enum GetFormatSpecFlags {
    BINARY_SIGNED = 0,		/* Field to be read as signed data */
    BINARY_UNSIGNED = 1		/* Field to be read as unsigned data */
};

/*
 * The following defines the maximum number of different (integer) numbers
 * placed in the object cache by 'binary scan' before it bails out and
 * switches back to Plan A (creating a new object for each value.)
 * Theoretically, it would be possible to keep the cache about for the values
 * that are already in it, but that makes the code slower in practice when
 * overflow happens, and makes little odds the rest of the time (as measured
 * on my machine.) It is also slower (on the sample I tried at least) to grow
 * the cache to hold all items we might want to put in it; presumably the
 * extra cost of managing the memory for the enlarged table outweighs the
 * benefit from allocating fewer objects. This is probably because as the
 * number of objects increases, the likelihood of reuse of any particular one
 * drops, and there is very little gain from larger maximum cache sizes (the
 * value below is chosen to allow caching to work in full with conversion of
 * bytes.) - DKF
 */

#define BINARY_SCAN_MAX_CACHE	260

/*
 * Prototypes for local procedures defined in this file:
 */

static void		DupProperByteArrayInternalRep(Tcl_Obj *srcPtr,
			    Tcl_Obj *copyPtr);
static int		FormatNumber(Tcl_Interp *interp, int type,
			    Tcl_Obj *src, unsigned char **cursorPtr);
static void		FreeProperByteArrayInternalRep(Tcl_Obj *objPtr);
static int		GetFormatSpec(const char **formatPtr, char *cmdPtr,
			    Tcl_Size *countPtr, int *flagsPtr);
static Tcl_Obj *	ScanNumber(unsigned char *buffer, int type,
			    int flags, Tcl_HashTable **numberCachePtr);
static int		SetByteArrayFromAny(Tcl_Interp *interp, Tcl_Size limit,
			    Tcl_Obj *objPtr);
static void		UpdateStringOfByteArray(Tcl_Obj *listPtr);
static void		DeleteScanNumberCache(Tcl_HashTable *numberCachePtr);
static int		NeedReversing(int format);
static void		CopyNumber(const void *from, void *to,
			    size_t length, int type);
/* Binary ensemble commands */
static Tcl_ObjCmdProc	BinaryFormatCmd;
static Tcl_ObjCmdProc	BinaryScanCmd;
/* Binary encoding sub-ensemble commands */
static Tcl_ObjCmdProc	BinaryEncodeHex;
static Tcl_ObjCmdProc	BinaryDecodeHex;
static Tcl_ObjCmdProc	BinaryEncode64;
static Tcl_ObjCmdProc	BinaryDecode64;
static Tcl_ObjCmdProc	BinaryEncodeUu;
static Tcl_ObjCmdProc	BinaryDecodeUu;

/*
 * The following tables are used by the binary encoders
 */

static const char HexDigits[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

static const char UueDigits[65] = {
    '`', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\',']', '^', '_',
    '`'
};

static const char B64Digits[65] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
    '='
};

/*
 * How to construct the ensembles.
 */

static const EnsembleImplMap binaryMap[] = {
    { "format", BinaryFormatCmd, TclCompileBasicMin1ArgCmd, NULL, NULL, 0 },
    { "scan",   BinaryScanCmd, TclCompileBasicMin2ArgCmd, NULL, NULL, 0 },
    { "encode", NULL, NULL, NULL, NULL, 0 },
    { "decode", NULL, NULL, NULL, NULL, 0 },
    { NULL, NULL, NULL, NULL, NULL, 0 }
};
static const EnsembleImplMap encodeMap[] = {
    { "hex",      BinaryEncodeHex, TclCompileBasic1ArgCmd, NULL, NULL, 0 },
    { "uuencode", BinaryEncodeUu,  NULL, NULL, NULL, 0 },
    { "base64",   BinaryEncode64,  NULL, NULL, NULL, 0 },
    { NULL, NULL, NULL, NULL, NULL, 0 }
};
static const EnsembleImplMap decodeMap[] = {
    { "hex",      BinaryDecodeHex, TclCompileBasic1Or2ArgCmd, NULL, NULL, 0 },
    { "uuencode", BinaryDecodeUu,  TclCompileBasic1Or2ArgCmd, NULL, NULL, 0 },
    { "base64",   BinaryDecode64,  TclCompileBasic1Or2ArgCmd, NULL, NULL, 0 },
    { NULL, NULL, NULL, NULL, NULL, 0 }
};

/*
 * The following Tcl_ObjType represents an array of bytes. The intent is to
 * allow arbitrary binary data to pass through Tcl as a Tcl value without loss
 * or damage. Such values are useful for things like encoded strings or Tk
 * images to name just two.
 *
 * A bytearray is an ordered sequence of bytes. Each byte is an integer value
 * in the range [0-255].  To be a Tcl value type, we need a way to encode each
 * value in the value set as a Tcl string.  A simple encoding is to
 * represent each byte value as the same codepoint value.  A bytearray of N
 * bytes is encoded into a Tcl string of N characters where the codepoint of
 * each character is the value of corresponding byte.  This approach creates a
 * one-to-one map between all bytearray values and a subset of Tcl string
 * values.  Tcl string values outside that subset do no represent any valid
 * bytearray value.  Attempts to treat those values as bytearrays will lead
 * to errors.  See TIP 568 for how this differs from Tcl 8.
 */

static const Tcl_ObjType properByteArrayType = {
    "bytearray",
    FreeProperByteArrayInternalRep,
    DupProperByteArrayInternalRep,
    UpdateStringOfByteArray,
    NULL,
    TCL_OBJTYPE_V0
};

/*
 * The following structure is the internal rep for a ByteArray object. Keeps
 * track of how much memory has been used and how much has been allocated for
 * the byte array to enable growing and shrinking of the ByteArray object with
 * fewer mallocs.
 */

typedef struct {
    Tcl_Size used;		/* The number of bytes used in the byte
				 * array. */
    Tcl_Size allocated;		/* The amount of space actually allocated
				 * minus 1 byte. */
    unsigned char bytes[TCLFLEXARRAY];	/* The array of bytes. The actual size of this
				 * field depends on the 'allocated' field
				 * above. */
} ByteArray;

#define BYTEARRAY_MAX_LEN (TCL_SIZE_MAX - (Tcl_Size)offsetof(ByteArray, bytes))
#define BYTEARRAY_SIZE(len) \
	( (len < 0 || BYTEARRAY_MAX_LEN < (len)) \
	? (Tcl_Panic("negative length specified or max size of a Tcl value exceeded"), 0) \
	: (offsetof(ByteArray, bytes) + (len)) )
#define GET_BYTEARRAY(irPtr) ((ByteArray *) (irPtr)->twoPtrValue.ptr1)
#define SET_BYTEARRAY(irPtr, baPtr) \
		(irPtr)->twoPtrValue.ptr1 = (baPtr)

int
TclIsPureByteArray(
    Tcl_Obj * objPtr)
{
    return TclHasInternalRep(objPtr, &properByteArrayType);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_NewByteArrayObj --
 *
 *	This procedure is creates a new ByteArray object and initializes it
 *	from the given array of bytes.
 *
 * Results:
 *	The newly created object is returned. This object has no initial
 *	string representation. The returned object has a ref count of 0.
 *
 * Side effects:
 *	Memory allocated for new object and copy of byte array argument.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_NewByteArrayObj

Tcl_Obj *
Tcl_NewByteArrayObj(
    const unsigned char *bytes,	/* The array of bytes used to initialize the
				 * new object. */
    Tcl_Size numBytes)		/* Number of bytes in the array,
				 * must be >= 0. */
{
#ifdef TCL_MEM_DEBUG
    return Tcl_DbNewByteArrayObj(bytes, numBytes, "unknown", 0);
#else /* if not TCL_MEM_DEBUG */
    Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    Tcl_SetByteArrayObj(objPtr, bytes, numBytes);
    return objPtr;
#endif /* TCL_MEM_DEBUG */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbNewByteArrayObj --
 *
 *	This procedure is normally called when debugging: i.e., when
 *	TCL_MEM_DEBUG is defined. It is the same as the Tcl_NewByteArrayObj
 *	above except that it calls Tcl_DbCkalloc directly with the file name
 *	and line number from its caller. This simplifies debugging since then
 *	the [memory active] command will report the correct file name and line
 *	number when reporting objects that haven't been freed.
 *
 *	When TCL_MEM_DEBUG is not defined, this procedure just returns the
 *	result of calling Tcl_NewByteArrayObj.
 *
 * Results:
 *	The newly created object is returned. This object has no initial
 *	string representation. The returned object has a ref count of 0.
 *
 * Side effects:
 *	Memory allocated for new object and copy of byte array argument.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_MEM_DEBUG
Tcl_Obj *
Tcl_DbNewByteArrayObj(
    const unsigned char *bytes,	/* The array of bytes used to initialize the
				 * new object. */
    Tcl_Size numBytes,		/* Number of bytes in the array,
				 * must be >= 0. */
    const char *file,		/* The name of the source file calling this
				 * procedure; used for debugging. */
    int line)			/* Line number in the source file; used for
				 * debugging. */
{
    Tcl_Obj *objPtr;

    TclDbNewObj(objPtr, file, line);
    Tcl_SetByteArrayObj(objPtr, bytes, numBytes);
    return objPtr;
}
#else /* if not TCL_MEM_DEBUG */
Tcl_Obj *
Tcl_DbNewByteArrayObj(
    const unsigned char *bytes,	/* The array of bytes used to initialize the
				 * new object. */
    Tcl_Size numBytes,		/* Number of bytes in the array,
				 * must be >= 0. */
    TCL_UNUSED(const char *) /*file*/,
    TCL_UNUSED(int) /*line*/)
{
    return Tcl_NewByteArrayObj(bytes, numBytes);
}
#endif /* TCL_MEM_DEBUG */

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_SetByteArrayObj --
 *
 *	Modify an object to be a ByteArray object and to have the specified
 *	array of bytes as its value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep and internal rep is freed. Memory
 *	allocated for copy of byte array argument.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetByteArrayObj(
    Tcl_Obj *objPtr,		/* Object to initialize as a ByteArray. */
    const unsigned char *bytes,	/* The array of bytes to use as the new value.
				 * May be NULL even if numBytes > 0. */
    Tcl_Size numBytes)		/* Number of bytes in the array,
				 * must be >= 0 */
{
    ByteArray *byteArrayPtr;
    Tcl_ObjInternalRep ir;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetByteArrayObj");
    }
    TclInvalidateStringRep(objPtr);

    assert(numBytes >= 0);
    byteArrayPtr = (ByteArray *)Tcl_Alloc(BYTEARRAY_SIZE(numBytes));
    byteArrayPtr->used = numBytes;
    byteArrayPtr->allocated = numBytes;

    if ((bytes != NULL) && (numBytes > 0)) {
	memcpy(byteArrayPtr->bytes, bytes, numBytes);
    }
    SET_BYTEARRAY(&ir, byteArrayPtr);

    Tcl_StoreInternalRep(objPtr, &properByteArrayType, &ir);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetBytesFromObj --
 *
 *	Attempt to extract the value from objPtr in the representation
 *	of a byte sequence. On success return the extracted byte sequence.
 *	On failure, return NULL and record error message and code in
 *	interp (if not NULL).
 *
 * Results:
 *	NULL or pointer to array of bytes representing the ByteArray object.
 *	Writes number of bytes in array to *numBytesPtr.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_GetBytesFromObj
unsigned char *
Tcl_GetBytesFromObj(
    Tcl_Interp *interp,		/* For error reporting */
    Tcl_Obj *objPtr,		/* Value to extract from */
    Tcl_Size *numBytesPtr)	/* If non-NULL, write the number of bytes
				 * in the array here */
{
    ByteArray *baPtr;
    const Tcl_ObjInternalRep *irPtr
	    = TclFetchInternalRep(objPtr, &properByteArrayType);

    if (irPtr == NULL) {
	if (TCL_ERROR == SetByteArrayFromAny(interp, TCL_INDEX_NONE, objPtr)) {
	    return NULL;
	}
	irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    }
    baPtr = GET_BYTEARRAY(irPtr);

    if (numBytesPtr != NULL) {
	*numBytesPtr = baPtr->used;
    }
    return baPtr->bytes;
}

#ifndef TCL_NO_DEPRECATED
unsigned char *
TclGetBytesFromObj(
    Tcl_Interp *interp,		/* For error reporting */
    Tcl_Obj *objPtr,		/* Value to extract from */
    void *numBytesPtr)		/* If non-NULL, write the number of bytes
				 * in the array here */
{
    Tcl_Size numBytes = 0;
    unsigned char *bytes = Tcl_GetBytesFromObj(interp, objPtr, &numBytes);

    if (bytes && numBytesPtr) {
	if (numBytes > INT_MAX) {
	    /* Caller asked for numBytes to be written to an int, but the
	     * value is outside the int range. */

	    if (interp) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"byte sequence length exceeds INT_MAX", -1));
		Tcl_SetErrorCode(interp, "TCL", "API", "OUTDATED", (char *)NULL);
	    }
	    return NULL;
	} else {
	    *(int *)numBytesPtr = (int) numBytes;
	}
    }
    return bytes;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetByteArrayLength --
 *
 *	This procedure changes the length of the byte array for this object.
 *	Once the caller has set the length of the array, it is acceptable to
 *	directly modify the bytes in the array up until Tcl_GetStringFromObj()
 *	has been called on this object.
 *
 * Results:
 *	The new byte array of the specified length.
 *
 * Side effects:
 *	Allocates enough memory for an array of bytes of the requested size.
 *	When growing the array, the old array is copied to the new array; new
 *	bytes are undefined. When shrinking, the old array is truncated to the
 *	specified length.
 *
 *----------------------------------------------------------------------
 */

unsigned char *
Tcl_SetByteArrayLength(
    Tcl_Obj *objPtr,		/* The ByteArray object. */
    Tcl_Size numBytes)		/* Number of bytes in resized array
				 * Must be >= 0 */
{
    ByteArray *byteArrayPtr;
    Tcl_ObjInternalRep *irPtr;

    assert(numBytes >= 0);
    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object", "Tcl_SetByteArrayLength");
    }

    irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    if (irPtr == NULL) {
	if (TCL_ERROR == SetByteArrayFromAny(NULL, numBytes, objPtr)) {
	    return NULL;
	}
	irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    }

    byteArrayPtr = GET_BYTEARRAY(irPtr);
    if (numBytes > byteArrayPtr->allocated) {
	byteArrayPtr = (ByteArray *)Tcl_Realloc(byteArrayPtr,
		BYTEARRAY_SIZE(numBytes));
	byteArrayPtr->allocated = numBytes;
	SET_BYTEARRAY(irPtr, byteArrayPtr);
    }
    TclInvalidateStringRep(objPtr);
    byteArrayPtr->used = numBytes;
    return byteArrayPtr->bytes;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeByteArray --
 *
 *	Generate a ByteArray internal rep from the string rep of objPtr.
 *	The generated byte sequence may have no more than limit bytes.
 *	A negative value for limit indicates no limit imposed. If
 *	boolean argument demandProper is true, then no byte sequence should
 *	be output to the caller (write NULL instead). When no bytes sequence
 *	is output and interp is not NULL, leave an error message and error
 *	code in interp explaining why a proper byte sequence could not be
 *	made.
 *
 * Results:
 *	Returns a boolean indicating whether the bytes generated (up to
 *	limit bytes) are a proper representation of (a limited prefix of)
 *	the string. Writes a pointer to the generated ByteArray to
 *	*byteArrayPtrPtr. If not NULL it needs to be released with Tcl_Free().
 *
 *----------------------------------------------------------------------
 */

static int
MakeByteArray(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    Tcl_Size limit,
    int demandProper,
    ByteArray **byteArrayPtrPtr)
{
    Tcl_Size length;
    const char *src = TclGetStringFromObj(objPtr, &length);
    Tcl_Size numBytes = (limit >= 0 && limit < length) ? limit : length;
    ByteArray *byteArrayPtr = (ByteArray *)Tcl_Alloc(BYTEARRAY_SIZE(numBytes));
    unsigned char *dst = byteArrayPtr->bytes;
    unsigned char *dstEnd = dst + numBytes;
    const char *srcEnd = src + length;
    int proper = 1;

    for (; src < srcEnd && dst < dstEnd; ) {
	int ch;
	Tcl_Size count = TclUtfToUniChar(src, &ch);

	if (ch > 255) {
	    proper = 0;
	    if (demandProper) {
		if (interp) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "expected byte sequence but character %"
			    TCL_Z_MODIFIER "u was '%1s' (U+%06X)",
			    dst - byteArrayPtr->bytes, src, ch));
		    Tcl_SetErrorCode(interp, "TCL", "VALUE", "BYTES", (char *)NULL);
		}
		Tcl_Free(byteArrayPtr);
		*byteArrayPtrPtr = NULL;
		return proper;
	    }
	}
	src += count;
	*dst++ = UCHAR(ch);
    }
    byteArrayPtr->used = dst - byteArrayPtr->bytes;
    byteArrayPtr->allocated = numBytes;

    *byteArrayPtrPtr = byteArrayPtr;
    return proper;
}

static Tcl_Obj *
TclNarrowToBytes(
    Tcl_Obj *objPtr)
{
    if (NULL == TclFetchInternalRep(objPtr, &properByteArrayType)) {
	Tcl_ObjInternalRep ir;
	ByteArray *byteArrayPtr;

	if (0 == MakeByteArray(NULL, objPtr, TCL_INDEX_NONE, 0, &byteArrayPtr)) {
	    TclNewObj(objPtr);
	    TclInvalidateStringRep(objPtr);
	}
	SET_BYTEARRAY(&ir, byteArrayPtr);
	Tcl_StoreInternalRep(objPtr, &properByteArrayType, &ir);
    }
    Tcl_IncrRefCount(objPtr);
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SetByteArrayFromAny --
 *
 *	Generate the ByteArray internal rep from the string rep.
 *
 * Results:
 *	Tcl return code indicating OK or ERROR.
 *
 * Side effects:
 *	A ByteArray struct may be stored as the internal rep of objPtr.
 *
 *----------------------------------------------------------------------
 */

static int
SetByteArrayFromAny(
    Tcl_Interp *interp,		/* For error reporting. */
    Tcl_Size limit,		/* Create no more than this many bytes */
    Tcl_Obj *objPtr)		/* The object to convert to type ByteArray. */
{
    ByteArray *byteArrayPtr;
    Tcl_ObjInternalRep ir;

    if (0 == MakeByteArray(interp, objPtr, limit, 1, &byteArrayPtr)) {
	return TCL_ERROR;
    }

    SET_BYTEARRAY(&ir, byteArrayPtr);
    Tcl_StoreInternalRep(objPtr, &properByteArrayType, &ir);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeByteArrayInternalRep --
 *
 *	Deallocate the storage associated with a ByteArray data object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *----------------------------------------------------------------------
 */

static void
FreeProperByteArrayInternalRep(
    Tcl_Obj *objPtr)		/* Object with internal rep to free. */
{
    Tcl_Free(GET_BYTEARRAY(TclFetchInternalRep(objPtr, &properByteArrayType)));
}

/*
 *----------------------------------------------------------------------
 *
 * DupByteArrayInternalRep --
 *
 *	Initialize the internal representation of a ByteArray Tcl_Obj to a
 *	copy of the internal representation of an existing ByteArray object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory.
 *
 *----------------------------------------------------------------------
 */

static void
DupProperByteArrayInternalRep(
    Tcl_Obj *srcPtr,		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr)		/* Object with internal rep to set. */
{
    Tcl_Size length;
    ByteArray *srcArrayPtr, *copyArrayPtr;
    Tcl_ObjInternalRep ir;

    srcArrayPtr = GET_BYTEARRAY(TclFetchInternalRep(srcPtr, &properByteArrayType));
    length = srcArrayPtr->used;

    copyArrayPtr = (ByteArray *)Tcl_Alloc(BYTEARRAY_SIZE(length));
    copyArrayPtr->used = length;
    copyArrayPtr->allocated = length;
    memcpy(copyArrayPtr->bytes, srcArrayPtr->bytes, length);

    SET_BYTEARRAY(&ir, copyArrayPtr);
    Tcl_StoreInternalRep(copyPtr, &properByteArrayType, &ir);
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfByteArray --
 *
 *	Update the string representation for a ByteArray data object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from the
 *	ByteArray-to-string conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfByteArray(
    Tcl_Obj *objPtr)		/* ByteArray object whose string rep to
				 * update. */
{
    const Tcl_ObjInternalRep *irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    ByteArray *byteArrayPtr = GET_BYTEARRAY(irPtr);
    unsigned char *src = byteArrayPtr->bytes;
    Tcl_Size i, length = byteArrayPtr->used;
    Tcl_Size size = length;

    /*
     * How much space will string rep need?
     */

    for (i = 0; i < length; i++) {
	if ((src[i] == 0) || (src[i] > 127)) {
	    size++;
	}
    }

    if (size == length) {
	char *dst = Tcl_InitStringRep(objPtr, (char *)src, size);

	TclOOM(dst, size);
    } else {
	char *dst = Tcl_InitStringRep(objPtr, NULL, size);

	TclOOM(dst, size);
	for (i = 0; i < length; i++) {
	    dst += Tcl_UniCharToUtf(src[i], dst);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclAppendBytesToByteArray --
 *
 *	This function appends an array of bytes to a byte array object. Note
 *	that the object *must* be unshared, and the array of bytes *must not*
 *	refer to the object being appended to.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates enough memory for an array of bytes of the requested total
 *	size, or possibly larger. [Bug 2992970]
 *
 *----------------------------------------------------------------------
 */

void
TclAppendBytesToByteArray(
    Tcl_Obj *objPtr,
    const unsigned char *bytes,
    Tcl_Size len)
{
    ByteArray *byteArrayPtr;
    Tcl_Size needed;
    Tcl_ObjInternalRep *irPtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("%s called with shared object","TclAppendBytesToByteArray");
    }
    if (len < 0) {
	Tcl_Panic("%s must be called with definite number of bytes to append",
		"TclAppendBytesToByteArray");
    }
    if (len == 0) {
	/*
	 * Append zero bytes is a no-op.
	 */

	return;
    }

    irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    if (irPtr == NULL) {
	if (TCL_ERROR == SetByteArrayFromAny(NULL, TCL_INDEX_NONE, objPtr)) {
	    Tcl_Panic("attempt to append bytes to non-bytearray");
	}
	irPtr = TclFetchInternalRep(objPtr, &properByteArrayType);
    }
    byteArrayPtr = GET_BYTEARRAY(irPtr);

    /*
     * If we need to, resize the allocated space in the byte array.
     */

    if ((BYTEARRAY_MAX_LEN - byteArrayPtr->used) < len) {
	/* Will wrap around !! */
	Tcl_Panic("max size of a byte array exceeded");
    }
    needed = byteArrayPtr->used + len;
    if (needed > byteArrayPtr->allocated) {
	Tcl_Size newCapacity;
	byteArrayPtr = (ByteArray *)
		TclReallocElemsEx(byteArrayPtr, needed, 1,
		offsetof(ByteArray, bytes), &newCapacity);
	byteArrayPtr->allocated = newCapacity;
	SET_BYTEARRAY(irPtr, byteArrayPtr);
    }

    if (bytes) {
	memcpy(byteArrayPtr->bytes + byteArrayPtr->used, bytes, len);
    }
    byteArrayPtr->used += len;
    TclInvalidateStringRep(objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitBinaryCmd --
 *
 *	This function is called to create the "binary" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A command token for the new command.
 *
 * Side effects:
 *	Creates a new binary command as a mapped ensemble.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclInitBinaryCmd(
    Tcl_Interp *interp)
{
    Tcl_Command binaryEnsemble;

    binaryEnsemble = TclMakeEnsemble(interp, "binary", binaryMap);
    TclMakeEnsemble(interp, "binary encode", encodeMap);
    TclMakeEnsemble(interp, "binary decode", decodeMap);
    return binaryEnsemble;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryFormatCmd --
 *
 *	This procedure implements the "binary format" Tcl command.
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
BinaryFormatCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int arg;			/* Index of next argument to consume. */
    int value = 0;		/* Current integer value to be packed.
				 * Initialized to avoid compiler warning. */
    char cmd;			/* Current format character. */
    Tcl_Size count;		/* Count associated with current format
				 * character. */
    int flags;			/* Format field flags */
    const char *format;		/* Pointer to current position in format
				 * string. */
    Tcl_Obj *resultPtr = NULL;	/* Object holding result buffer. */
    unsigned char *buffer;	/* Start of result buffer. */
    unsigned char *cursor;	/* Current position within result buffer. */
    unsigned char *maxPos;	/* Greatest position within result buffer that
				 * cursor has visited.*/
    const char *errorString;
    const char *errorValue, *str;
    Tcl_Size offset, size, length;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "formatString ?arg ...?");
	return TCL_ERROR;
    }

    /*
     * To avoid copying the data, we format the string in two passes. The
     * first pass computes the size of the output buffer. The second pass
     * places the formatted data into the buffer.
     */

    format = TclGetString(objv[1]);
    arg = 2;
    offset = 0;
    length = 0;
    while (*format != '\0') {
	str = format;
	flags = 0;
	if (!GetFormatSpec(&format, &cmd, &count, &flags)) {
	    break;
	}
	switch (cmd) {
	case 'a':
	case 'A':
	case 'b':
	case 'B':
	case 'h':
	case 'H':
	    /*
	     * For string-type specifiers, the count corresponds to the number
	     * of bytes in a single argument.
	     */

	    if (arg >= objc) {
		goto badIndex;
	    }
	    if (count == BINARY_ALL) {
		if (Tcl_GetBytesFromObj(NULL, objv[arg], &count) == NULL) {
		    count = Tcl_GetCharLength(objv[arg]);
		}
	    } else if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    arg++;
	    if (cmd == 'a' || cmd == 'A') {
		offset += count;
	    } else if (cmd == 'b' || cmd == 'B') {
		offset += (count + 7) / 8;
	    } else {
		offset += (count + 1) / 2;
	    }
	    break;
	case 'c':
	    size = 1;
	    goto doNumbers;
	case 't':
	case 's':
	case 'S':
	    size = 2;
	    goto doNumbers;
	case 'n':
	case 'i':
	case 'I':
	    size = 4;
	    goto doNumbers;
	case 'm':
	case 'w':
	case 'W':
	    size = 8;
	    goto doNumbers;
	case 'r':
	case 'R':
	case 'f':
	    size = sizeof(float);
	    goto doNumbers;
	case 'q':
	case 'Q':
	case 'd':
	    size = sizeof(double);

	doNumbers:
	    if (arg >= objc) {
		goto badIndex;
	    }

	    /*
	     * For number-type specifiers, the count corresponds to the number
	     * of elements in the list stored in a single argument. If no
	     * count is specified, then the argument is taken as a single
	     * non-list value.
	     */

	    if (count == BINARY_NOCOUNT) {
		arg++;
		count = 1;
	    } else {
		Tcl_Size listc;
		Tcl_Obj **listv;

		/*
		 * The macro evals its args more than once: avoid arg++
		 */

		if (TclListObjLength(interp, objv[arg], &listc) != TCL_OK) {
		    return TCL_ERROR;
		}

		if (count == BINARY_ALL) {
		    count = listc;
		} else if (count > listc) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "number of elements in list does not match count",
			    -1));
		    return TCL_ERROR;
		}
		if (TclListObjGetElements(interp, objv[arg], &listc,
			&listv) != TCL_OK) {
		    return TCL_ERROR;
		}
		arg++;
	    }
	    offset += count*size;
	    break;

	case 'x':
	    if (count == BINARY_ALL) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"cannot use \"*\" in format string with \"x\"", -1));
		return TCL_ERROR;
	    } else if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    offset += count;
	    break;
	case 'X':
	    if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    if ((count > offset) || (count == BINARY_ALL)) {
		count = offset;
	    }
	    if (offset > length) {
		length = offset;
	    }
	    offset -= count;
	    break;
	case '@':
	    if (offset > length) {
		length = offset;
	    }
	    if (count == BINARY_ALL) {
		offset = length;
	    } else if (count == BINARY_NOCOUNT) {
		goto badCount;
	    } else {
		offset = count;
	    }
	    break;
	default:
	    errorString = str;
	    goto badField;
	}
    }
    if (offset > length) {
	length = offset;
    }
    if (length == 0) {
	return TCL_OK;
    }

    /*
     * Prepare the result object by preallocating the calculated number of
     * bytes and filling with nulls.
     */

    TclNewObj(resultPtr);
    buffer = Tcl_SetByteArrayLength(resultPtr, length);
    memset(buffer, 0, length);

    /*
     * Pack the data into the result object. Note that we can skip the error
     * checking during this pass, since we have already parsed the string
     * once.
     */

    arg = 2;
    format = TclGetString(objv[1]);
    cursor = buffer;
    maxPos = cursor;
    while (*format != 0) {
	flags = 0;
	if (!GetFormatSpec(&format, &cmd, &count, &flags)) {
	    break;
	}
	if ((count == 0) && (cmd != '@')) {
	    if (cmd != 'x') {
		arg++;
	    }
	    continue;
	}
	switch (cmd) {
	case 'a':
	case 'A': {
	    char pad = (char) (cmd == 'a' ? '\0' : ' ');
	    unsigned char *bytes;
	    Tcl_Obj *copy = TclNarrowToBytes(objv[arg++]);

	    bytes = Tcl_GetBytesFromObj(NULL, copy, &length);

	    if (count == BINARY_ALL) {
		count = length;
	    } else if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    if (length >= count) {
		memcpy(cursor, bytes, count);
	    } else {
		memcpy(cursor, bytes, length);
		memset(cursor + length, pad, count - length);
	    }
	    cursor += count;
	    Tcl_DecrRefCount(copy);
	    break;
	}
	case 'b':
	case 'B': {
	    unsigned char *last;

	    str = TclGetStringFromObj(objv[arg], &length);
	    arg++;
	    if (count == BINARY_ALL) {
		count = length;
	    } else if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    last = cursor + ((count + 7) / 8);
	    if (count > length) {
		count = length;
	    }
	    value = 0;
	    errorString = "binary";
	    if (cmd == 'B') {
		for (offset = 0; offset < count; offset++) {
		    value <<= 1;
		    if (str[offset] == '1') {
			value |= 1;
		    } else if (str[offset] != '0') {
			errorValue = str;
			Tcl_DecrRefCount(resultPtr);
			goto badValue;
		    }
		    if (((offset + 1) % 8) == 0) {
			*cursor++ = UCHAR(value);
			value = 0;
		    }
		}
	    } else {
		for (offset = 0; offset < count; offset++) {
		    value >>= 1;
		    if (str[offset] == '1') {
			value |= 128;
		    } else if (str[offset] != '0') {
			errorValue = str;
			Tcl_DecrRefCount(resultPtr);
			goto badValue;
		    }
		    if (!((offset + 1) % 8)) {
			*cursor++ = UCHAR(value);
			value = 0;
		    }
		}
	    }
	    if ((offset % 8) != 0) {
		if (cmd == 'B') {
		    value <<= 8 - (offset % 8);
		} else {
		    value >>= 8 - (offset % 8);
		}
		*cursor++ = UCHAR(value);
	    }
	    while (cursor < last) {
		*cursor++ = '\0';
	    }
	    break;
	}
	case 'h':
	case 'H': {
	    unsigned char *last;
	    int c;

	    str = TclGetStringFromObj(objv[arg], &length);
	    arg++;
	    if (count == BINARY_ALL) {
		count = length;
	    } else if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    last = cursor + ((count + 1) / 2);
	    if (count > length) {
		count = length;
	    }
	    value = 0;
	    errorString = "hexadecimal";
	    if (cmd == 'H') {
		for (offset = 0; offset < count; offset++) {
		    value <<= 4;
		    if (!isxdigit(UCHAR(str[offset]))) {     /* INTL: digit */
			errorValue = str;
			Tcl_DecrRefCount(resultPtr);
			goto badValue;
		    }
		    c = str[offset] - '0';
		    if (c > 9) {
			c += ('0' - 'A') + 10;
		    }
		    if (c > 16) {
			c += ('A' - 'a');
		    }
		    value |= (c & 0xF);
		    if (offset % 2) {
			*cursor++ = (char) value;
			value = 0;
		    }
		}
	    } else {
		for (offset = 0; offset < count; offset++) {
		    value >>= 4;

		    if (!isxdigit(UCHAR(str[offset]))) {     /* INTL: digit */
			errorValue = str;
			Tcl_DecrRefCount(resultPtr);
			goto badValue;
		    }
		    c = str[offset] - '0';
		    if (c > 9) {
			c += ('0' - 'A') + 10;
		    }
		    if (c > 16) {
			c += ('A' - 'a');
		    }
		    value |= ((c << 4) & 0xF0);
		    if (offset % 2) {
			*cursor++ = UCHAR(value & 0xFF);
			value = 0;
		    }
		}
	    }
	    if (offset % 2) {
		if (cmd == 'H') {
		    value <<= 4;
		} else {
		    value >>= 4;
		}
		*cursor++ = UCHAR(value);
	    }

	    while (cursor < last) {
		*cursor++ = '\0';
	    }
	    break;
	}
	case 'c':
	case 't':
	case 's':
	case 'S':
	case 'n':
	case 'i':
	case 'I':
	case 'm':
	case 'w':
	case 'W':
	case 'r':
	case 'R':
	case 'd':
	case 'q':
	case 'Q':
	case 'f': {
	    Tcl_Size listc, i;
	    Tcl_Obj **listv;

	    if (count == BINARY_NOCOUNT) {
		/*
		 * Note that we are casting away the const-ness of objv, but
		 * this is safe since we aren't going to modify the array.
		 */

		listv = (Tcl_Obj **) (objv + arg);
		listc = 1;
		count = 1;
	    } else {
		TclListObjGetElements(interp, objv[arg], &listc, &listv);
		if (count == BINARY_ALL) {
		    count = listc;
		}
	    }
	    arg++;
	    for (i = 0; i < count; i++) {
		if (FormatNumber(interp, cmd, listv[i], &cursor) != TCL_OK) {
		    Tcl_DecrRefCount(resultPtr);
		    return TCL_ERROR;
		}
	    }
	    break;
	}
	case 'x':
	    if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    memset(cursor, 0, count);
	    cursor += count;
	    break;
	case 'X':
	    if (cursor > maxPos) {
		maxPos = cursor;
	    }
	    if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    if ((count == BINARY_ALL) || (count > (cursor - buffer))) {
		cursor = buffer;
	    } else {
		cursor -= count;
	    }
	    break;
	case '@':
	    if (cursor > maxPos) {
		maxPos = cursor;
	    }
	    if (count == BINARY_ALL) {
		cursor = maxPos;
	    } else {
		cursor = buffer + count;
	    }
	    break;
	}
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;

  badValue:
    Tcl_ResetResult(interp);
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "expected %s string but got \"%s\" instead",
	    errorString, errorValue));
    return TCL_ERROR;

  badCount:
    errorString = "missing count for \"@\" field specifier";
    goto error;

  badIndex:
    errorString = "not enough arguments for all format specifiers";
    goto error;

  badField:
    {
	Tcl_UniChar ch = 0;
	char buf[5] = "";

	TclUtfToUniChar(errorString, &ch);
	buf[Tcl_UniCharToUtf(ch, buf)] = '\0';
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad field specifier \"%s\"", buf));
	return TCL_ERROR;
    }

  error:
    Tcl_SetObjResult(interp, Tcl_NewStringObj(errorString, -1));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryScanCmd --
 *
 *	This procedure implements the "binary scan" Tcl command.
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
BinaryScanCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int arg;			/* Index of next argument to consume. */
    int value = 0;		/* Current integer value to be packed.
				 * Initialized to avoid compiler warning. */
    char cmd;			/* Current format character. */
    Tcl_Size count;		/* Count associated with current format
				 * character. */
    int flags;			/* Format field flags */
    const char *format;		/* Pointer to current position in format
				 * string. */
    Tcl_Obj *resultPtr = NULL;	/* Object holding result buffer. */
    unsigned char *buffer;	/* Start of result buffer. */
    const char *errorString;
    const char *str;
    Tcl_Size offset, size, length = 0, i;

    Tcl_Obj *valuePtr, *elementPtr;
    Tcl_HashTable numberCacheHash;
    Tcl_HashTable *numberCachePtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"value formatString ?varName ...?");
	return TCL_ERROR;
    }
    buffer = Tcl_GetBytesFromObj(interp, objv[1], &length);
    if (buffer == NULL) {
	return TCL_ERROR;
    }
    numberCachePtr = &numberCacheHash;
    Tcl_InitHashTable(numberCachePtr, TCL_ONE_WORD_KEYS);
    format = TclGetString(objv[2]);
    arg = 3;
    offset = 0;
    while (*format != '\0') {
	str = format;
	flags = 0;
	if (!GetFormatSpec(&format, &cmd, &count, &flags)) {
	    goto done;
	}
	switch (cmd) {
	case 'a':
	case 'A':
	case 'C': {
	    unsigned char *src;

	    if (arg >= objc) {
		DeleteScanNumberCache(numberCachePtr);
		goto badIndex;
	    }
	    if (count == BINARY_ALL) {
		count = length - offset;
	    } else {
		if (count == BINARY_NOCOUNT) {
		    count = 1;
		}
		if (count > (length - offset)) {
		    goto done;
		}
	    }

	    src = buffer + offset;
	    size = count;

	    /*
	     * Apply C string semantics or trim trailing
	     * nulls and spaces, if necessary.
	     */

	    if (cmd == 'C') {
		for (i = 0; i < size; i++) {
		    if (src[i] == '\0') {
			size = i;
			break;
		    }
		}
	    } else if (cmd == 'A') {
		while (size > 0) {
		    if (src[size - 1] != '\0' && src[size - 1] != ' ') {
			break;
		    }
		    size--;
		}
	    }

	    /*
	     * Have to do this #ifdef-fery because (as part of defining
	     * Tcl_NewByteArrayObj) we removed the #def that hides this stuff
	     * normally. If this code ever gets copied to another file, it
	     * should be changed back to the simpler version.
	     */

#ifdef TCL_MEM_DEBUG
	    valuePtr = Tcl_DbNewByteArrayObj(src, size, __FILE__, __LINE__);
#else
	    valuePtr = Tcl_NewByteArrayObj(src, size);
#endif /* TCL_MEM_DEBUG */

	    resultPtr = Tcl_ObjSetVar2(interp, objv[arg], NULL, valuePtr,
		    TCL_LEAVE_ERR_MSG);
	    arg++;
	    if (resultPtr == NULL) {
		DeleteScanNumberCache(numberCachePtr);
		return TCL_ERROR;
	    }
	    offset += count;
	    break;
	}
	case 'b':
	case 'B': {
	    unsigned char *src;
	    char *dest;

	    if (arg >= objc) {
		DeleteScanNumberCache(numberCachePtr);
		goto badIndex;
	    }
	    if (count == BINARY_ALL) {
		count = (length - offset) * 8;
	    } else {
		if (count == BINARY_NOCOUNT) {
		    count = 1;
		}
		if (count > (length - offset) * 8) {
		    goto done;
		}
	    }
	    src = buffer + offset;
	    TclNewObj(valuePtr);
	    Tcl_SetObjLength(valuePtr, count);
	    dest = TclGetString(valuePtr);

	    if (cmd == 'b') {
		for (i = 0; i < count; i++) {
		    if (i % 8) {
			value >>= 1;
		    } else {
			value = *src++;
		    }
		    *dest++ = (char) ((value & 1) ? '1' : '0');
		}
	    } else {
		for (i = 0; i < count; i++) {
		    if (i % 8) {
			value <<= 1;
		    } else {
			value = *src++;
		    }
		    *dest++ = (char) ((value & 0x80) ? '1' : '0');
		}
	    }

	    resultPtr = Tcl_ObjSetVar2(interp, objv[arg], NULL, valuePtr,
		    TCL_LEAVE_ERR_MSG);
	    arg++;
	    if (resultPtr == NULL) {
		DeleteScanNumberCache(numberCachePtr);
		return TCL_ERROR;
	    }
	    offset += (count + 7) / 8;
	    break;
	}
	case 'h':
	case 'H': {
	    char *dest;
	    unsigned char *src;
	    static const char hexdigit[] = "0123456789abcdef";

	    if (arg >= objc) {
		DeleteScanNumberCache(numberCachePtr);
		goto badIndex;
	    }
	    if (count == BINARY_ALL) {
		count = (length - offset)*2;
	    } else {
		if (count == BINARY_NOCOUNT) {
		    count = 1;
		}
		if (count > (length - offset)*2) {
		    goto done;
		}
	    }
	    src = buffer + offset;
	    TclNewObj(valuePtr);
	    Tcl_SetObjLength(valuePtr, count);
	    dest = TclGetString(valuePtr);

	    if (cmd == 'h') {
		for (i = 0; i < count; i++) {
		    if (i % 2) {
			value >>= 4;
		    } else {
			value = *src++;
		    }
		    *dest++ = hexdigit[value & 0xF];
		}
	    } else {
		for (i = 0; i < count; i++) {
		    if (i % 2) {
			value <<= 4;
		    } else {
			value = *src++;
		    }
		    *dest++ = hexdigit[(value >> 4) & 0xF];
		}
	    }

	    resultPtr = Tcl_ObjSetVar2(interp, objv[arg], NULL, valuePtr,
		    TCL_LEAVE_ERR_MSG);
	    arg++;
	    if (resultPtr == NULL) {
		DeleteScanNumberCache(numberCachePtr);
		return TCL_ERROR;
	    }
	    offset += (count + 1) / 2;
	    break;
	}
	case 'c':
	    size = 1;
	    goto scanNumber;
	case 't':
	case 's':
	case 'S':
	    size = 2;
	    goto scanNumber;
	case 'n':
	case 'i':
	case 'I':
	    size = 4;
	    goto scanNumber;
	case 'm':
	case 'w':
	case 'W':
	    size = 8;
	    goto scanNumber;
	case 'r':
	case 'R':
	case 'f':
	    size = sizeof(float);
	    goto scanNumber;
	case 'q':
	case 'Q':
	case 'd': {
	    unsigned char *src;

	    size = sizeof(double);
	    /* fall through */

	scanNumber:
	    if (arg >= objc) {
		DeleteScanNumberCache(numberCachePtr);
		goto badIndex;
	    }
	    if (count == BINARY_NOCOUNT) {
		if (length < (size + offset)) {
		    goto done;
		}
		valuePtr = ScanNumber(buffer+offset, cmd, flags,
			&numberCachePtr);
		offset += size;
	    } else {
		if (count == BINARY_ALL) {
		    count = (length - offset) / size;
		}
		if ((length - offset) < (count * size)) {
		    goto done;
		}
		TclNewObj(valuePtr);
		src = buffer + offset;
		for (i = 0; i < count; i++) {
		    elementPtr = ScanNumber(src, cmd, flags, &numberCachePtr);
		    src += size;
		    Tcl_ListObjAppendElement(NULL, valuePtr, elementPtr);
		}
		offset += count * size;
	    }

	    resultPtr = Tcl_ObjSetVar2(interp, objv[arg], NULL, valuePtr,
		    TCL_LEAVE_ERR_MSG);
	    arg++;
	    if (resultPtr == NULL) {
		DeleteScanNumberCache(numberCachePtr);
		return TCL_ERROR;
	    }
	    break;
	}
	case 'x':
	    if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    if ((count == BINARY_ALL) || (count > (length - offset))) {
		offset = length;
	    } else {
		offset += count;
	    }
	    break;
	case 'X':
	    if (count == BINARY_NOCOUNT) {
		count = 1;
	    }
	    if ((count == BINARY_ALL) || (count > offset)) {
		offset = 0;
	    } else {
		offset -= count;
	    }
	    break;
	case '@':
	    if (count == BINARY_NOCOUNT) {
		DeleteScanNumberCache(numberCachePtr);
		goto badCount;
	    }
	    if ((count == BINARY_ALL) || (count > length)) {
		offset = length;
	    } else {
		offset = count;
	    }
	    break;
	default:
	    DeleteScanNumberCache(numberCachePtr);
	    errorString = str;
	    goto badField;
	}
    }

    /*
     * Set the result to the last position of the cursor.
     */

  done:
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(arg - 3));
    DeleteScanNumberCache(numberCachePtr);

    return TCL_OK;

  badCount:
    errorString = "missing count for \"@\" field specifier";
    goto error;

  badIndex:
    errorString = "not enough arguments for all format specifiers";
    goto error;

  badField:
    {
	Tcl_UniChar ch = 0;
	char buf[5] = "";

	TclUtfToUniChar(errorString, &ch);
	buf[Tcl_UniCharToUtf(ch, buf)] = '\0';
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad field specifier \"%s\"", buf));
	return TCL_ERROR;
    }

  error:
    Tcl_SetObjResult(interp, Tcl_NewStringObj(errorString, -1));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetFormatSpec --
 *
 *	This function parses the format strings used in the binary format and
 *	scan commands.
 *
 * Results:
 *	Moves the formatPtr to the start of the next command. Returns the
 *	current command character and count in cmdPtr and countPtr. The count
 *	is set to BINARY_ALL if the count character was '*' or BINARY_NOCOUNT
 *	if no count was specified. Returns 1 on success, or 0 if the string
 *	did not have a format specifier.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetFormatSpec(
    const char **formatPtr,	/* Pointer to format string. */
    char *cmdPtr,		/* Pointer to location of command char. */
    Tcl_Size *countPtr,		/* Pointer to repeat count value. */
    int *flagsPtr)		/* Pointer to field flags */
{
    /*
     * Skip any leading blanks.
     */

    while (**formatPtr == ' ') {
	(*formatPtr)++;
    }

    /*
     * The string was empty, except for whitespace, so fail.
     */

    if (!(**formatPtr)) {
	return 0;
    }

    /*
     * Extract the command character and any trailing digits or '*'.
     */

    *cmdPtr = **formatPtr;
    (*formatPtr)++;
    if (**formatPtr == 'u') {
	(*formatPtr)++;
	*flagsPtr |= BINARY_UNSIGNED;
    }
    if (**formatPtr == '*') {
	(*formatPtr)++;
	*countPtr = BINARY_ALL;
    } else if (isdigit(UCHAR(**formatPtr))) { /* INTL: digit */
	unsigned long long count;

	errno = 0;
	count = strtoull(*formatPtr, (char **) formatPtr, 10);
	if (errno || (count > TCL_SIZE_MAX)) {
	    *countPtr = TCL_SIZE_MAX;
	} else {
	    *countPtr = count;
	}
    } else {
	*countPtr = BINARY_NOCOUNT;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * NeedReversing --
 *
 *	This routine determines, if bytes of a number need to be re-ordered,
 *	and returns a numeric code indicating the re-ordering to be done.
 *	This depends on the endianness of the machine and the desired format.
 *	It is in effect a table (whose contents depend on the endianness of
 *	the system) describing whether a value needs reversing or not. Anyone
 *	porting the code to a big-endian platform should take care to make
 *	sure that they define WORDS_BIGENDIAN though this is already done by
 *	configure for the Unix build; little-endian platforms (including
 *	Windows) don't need to do anything.
 *
 * Results:
 *	0	No re-ordering needed.
 *	1	Reverse the bytes:	01234567 <-> 76543210 (little to big)
 *	2	Apply this re-ordering: 01234567 <-> 45670123 (Nokia to little)
 *	3	Apply this re-ordering: 01234567 <-> 32107654 (Nokia to big)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
NeedReversing(
    int format)
{
    switch (format) {
	/* native floats and doubles: never reverse */
    case 'd':
    case 'f':
	/* big endian ints: never reverse */
    case 'I':
    case 'S':
    case 'W':
#ifdef WORDS_BIGENDIAN
	/* native ints: reverse if we're little-endian */
    case 'n':
    case 't':
    case 'm':
	/* f: reverse if we're little-endian */
    case 'Q':
    case 'R':
#else /* !WORDS_BIGENDIAN */
	/* small endian floats: reverse if we're big-endian */
    case 'r':
#endif /* WORDS_BIGENDIAN */
	return 0;

#ifdef WORDS_BIGENDIAN
	/* small endian floats: reverse if we're big-endian */
    case 'q':
    case 'r':
#else /* !WORDS_BIGENDIAN */
	/* native ints: reverse if we're little-endian */
    case 'n':
    case 't':
    case 'm':
	/* f: reverse if we're little-endian */
    case 'R':
#endif /* WORDS_BIGENDIAN */
	/* small endian ints: always reverse */
    case 'i':
    case 's':
    case 'w':
	return 1;

#ifndef WORDS_BIGENDIAN
    /*
     * The Q and q formats need special handling to account for the unusual
     * byte ordering of 8-byte floats on Nokia 770 systems, which claim to be
     * little-endian, but also reverse word order.
     */

    case 'Q':
	if (TclNokia770Doubles()) {
	    return 3;
	}
	return 1;
    case 'q':
	if (TclNokia770Doubles()) {
	    return 2;
	}
	return 0;
#endif
    }

    Tcl_Panic("unexpected fallthrough");
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CopyNumber --
 *
 *	This routine is called by FormatNumber and ScanNumber to copy a
 *	floating-point number. If required, bytes are reversed while copying.
 *	The behaviour is only fully defined when used with IEEE float and
 *	double values (guaranteed to be 4 and 8 bytes long, respectively.)
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Copies length bytes
 *
 *----------------------------------------------------------------------
 */

static void
CopyNumber(
    const void *from,		/* source */
    void *to,			/* destination */
    size_t length,		/* Number of bytes to copy */
    int type)			/* What type of thing are we copying? */
{
    switch (NeedReversing(type)) {
    case 0:
	memcpy(to, from, length);
	break;
    case 1: {
	const unsigned char *fromPtr = (const unsigned char *)from;
	unsigned char *toPtr = (unsigned char *)to;

	switch (length) {
	case 4:
	    toPtr[0] = fromPtr[3];
	    toPtr[1] = fromPtr[2];
	    toPtr[2] = fromPtr[1];
	    toPtr[3] = fromPtr[0];
	    break;
	case 8:
	    toPtr[0] = fromPtr[7];
	    toPtr[1] = fromPtr[6];
	    toPtr[2] = fromPtr[5];
	    toPtr[3] = fromPtr[4];
	    toPtr[4] = fromPtr[3];
	    toPtr[5] = fromPtr[2];
	    toPtr[6] = fromPtr[1];
	    toPtr[7] = fromPtr[0];
	    break;
	}
	break;
    }
    case 2: {
	const unsigned char *fromPtr = (const unsigned char *)from;
	unsigned char *toPtr = (unsigned char *)to;

	toPtr[0] = fromPtr[4];
	toPtr[1] = fromPtr[5];
	toPtr[2] = fromPtr[6];
	toPtr[3] = fromPtr[7];
	toPtr[4] = fromPtr[0];
	toPtr[5] = fromPtr[1];
	toPtr[6] = fromPtr[2];
	toPtr[7] = fromPtr[3];
	break;
    }
    case 3: {
	const unsigned char *fromPtr = (const unsigned char *)from;
	unsigned char *toPtr = (unsigned char *)to;

	toPtr[0] = fromPtr[3];
	toPtr[1] = fromPtr[2];
	toPtr[2] = fromPtr[1];
	toPtr[3] = fromPtr[0];
	toPtr[4] = fromPtr[7];
	toPtr[5] = fromPtr[6];
	toPtr[6] = fromPtr[5];
	toPtr[7] = fromPtr[4];
	break;
    }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FormatNumber --
 *
 *	This routine is called by Tcl_BinaryObjCmd to format a number into a
 *	location pointed at by cursor.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Moves the cursor to the next location to be written into.
 *
 *----------------------------------------------------------------------
 */

static int
FormatNumber(
    Tcl_Interp *interp,		/* Current interpreter, used to report
				 * errors. */
    int type,			/* Type of number to format. */
    Tcl_Obj *src,		/* Number to format. */
    unsigned char **cursorPtr)	/* Pointer to index into destination buffer. */
{
    double dvalue;
    Tcl_WideInt wvalue;
    float fvalue;

    switch (type) {
    case 'd':
    case 'q':
    case 'Q':
	/*
	 * Double-precision floating point values. Tcl_GetDoubleFromObj
	 * returns TCL_ERROR for NaN, but we can check by comparing the
	 * object's type pointer.
	 */

	if (Tcl_GetDoubleFromObj(interp, src, &dvalue) != TCL_OK) {
	    const Tcl_ObjInternalRep *irPtr = TclFetchInternalRep(src, &tclDoubleType);
	    if (irPtr == NULL) {
		return TCL_ERROR;
	    }
	    dvalue = irPtr->doubleValue;
	}
	CopyNumber(&dvalue, *cursorPtr, sizeof(double), type);
	*cursorPtr += sizeof(double);
	return TCL_OK;

    case 'f':
    case 'r':
    case 'R':
	/*
	 * Single-precision floating point values. Tcl_GetDoubleFromObj
	 * returns TCL_ERROR for NaN, but we can check by comparing the
	 * object's type pointer.
	 */

	if (Tcl_GetDoubleFromObj(interp, src, &dvalue) != TCL_OK) {
	    const Tcl_ObjInternalRep *irPtr = TclFetchInternalRep(src, &tclDoubleType);

	    if (irPtr == NULL) {
		return TCL_ERROR;
	    }
	    dvalue = irPtr->doubleValue;
	}

	/*
	 * Because some compilers will generate floating point exceptions on
	 * an overflow cast (e.g. Borland), we restrict the values to the
	 * valid range for float.
	 */

	if (fabs(dvalue) > (double) FLT_MAX) {
	    if (fabs(dvalue) > (FLT_MAX + pow(2, (FLT_MAX_EXP - FLT_MANT_DIG - 1)))) {
		fvalue = (dvalue >= 0.0) ? INFINITY : -INFINITY;	// c99
	    } else {
	    fvalue = (dvalue >= 0.0) ? FLT_MAX : -FLT_MAX;
	    }
	} else {
	    fvalue = (float) dvalue;
	}
	CopyNumber(&fvalue, *cursorPtr, sizeof(float), type);
	*cursorPtr += sizeof(float);
	return TCL_OK;

	/*
	 * 64-bit integer values.
	 */
    case 'w':
    case 'W':
    case 'm':
	if (TclGetWideBitsFromObj(interp, src, &wvalue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (NeedReversing(type)) {
	    *(*cursorPtr)++ = UCHAR(wvalue);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 16);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 24);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 32);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 40);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 48);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 56);
	} else {
	    *(*cursorPtr)++ = UCHAR(wvalue >> 56);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 48);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 40);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 32);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 24);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 16);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	    *(*cursorPtr)++ = UCHAR(wvalue);
	}
	return TCL_OK;

	/*
	 * 32-bit integer values.
	 */
    case 'i':
    case 'I':
    case 'n':
	if (TclGetWideBitsFromObj(interp, src, &wvalue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (NeedReversing(type)) {
	    *(*cursorPtr)++ = UCHAR(wvalue);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 16);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 24);
	} else {
	    *(*cursorPtr)++ = UCHAR(wvalue >> 24);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 16);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	    *(*cursorPtr)++ = UCHAR(wvalue);
	}
	return TCL_OK;

	/*
	 * 16-bit integer values.
	 */
    case 's':
    case 'S':
    case 't':
	if (TclGetWideBitsFromObj(interp, src, &wvalue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (NeedReversing(type)) {
	    *(*cursorPtr)++ = UCHAR(wvalue);
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	} else {
	    *(*cursorPtr)++ = UCHAR(wvalue >> 8);
	    *(*cursorPtr)++ = UCHAR(wvalue);
	}
	return TCL_OK;

	/*
	 * 8-bit integer values.
	 */
    case 'c':
	if (TclGetWideBitsFromObj(interp, src, &wvalue) != TCL_OK) {
	    return TCL_ERROR;
	}
	*(*cursorPtr)++ = UCHAR(wvalue);
	return TCL_OK;

    default:
	Tcl_Panic("unexpected fallthrough");
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ScanNumber --
 *
 *	This routine is called by Tcl_BinaryObjCmd to scan a number out of a
 *	buffer.
 *
 * Results:
 *	Returns a newly created object containing the scanned number. This
 *	object has a ref count of zero.
 *
 * Side effects:
 *	Might reuse an object in the number cache, place a new object in the
 *	cache, or delete the cache and set the reference to it (itself passed
 *	in by reference) to NULL.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ScanNumber(
    unsigned char *buffer,	/* Buffer to scan number from. */
    int type,			/* Format character from "binary scan" */
    int flags,			/* Format field flags */
    Tcl_HashTable **numberCachePtrPtr)
				/* Place to look for cache of scanned value
				 * objects, or NULL if too many different
				 * numbers have been scanned. */
{
    long value;
    float fvalue;
    double dvalue;
    Tcl_WideUInt uwvalue;

    /*
     * We cannot rely on the compiler to properly sign extend integer values
     * when we cast from smaller values to larger values because we don't know
     * the exact size of the integer types. So, we have to handle sign
     * extension explicitly by checking the high bit and padding with 1's as
     * needed. This practice is disabled if the BINARY_UNSIGNED flag is set.
     */

    switch (type) {
    case 'c':
	/*
	 * Characters need special handling. We want to produce a signed
	 * result, but on some platforms (such as AIX) chars are unsigned. To
	 * deal with this, check for a value that should be negative but
	 * isn't.
	 */

	value = buffer[0];
	if (!(flags & BINARY_UNSIGNED)) {
	    if (value & 0x80) {
		value |= -0x100;
	    }
	}
	goto returnNumericObject;

	/*
	 * 16-bit numeric values. We need the sign extension trick (see above)
	 * here as well.
	 */

    case 's':
    case 'S':
    case 't':
	if (NeedReversing(type)) {
	    value = (long) (buffer[0] + (buffer[1] << 8));
	} else {
	    value = (long) (buffer[1] + (buffer[0] << 8));
	}
	if (!(flags & BINARY_UNSIGNED)) {
	    if (value & 0x8000) {
		value |= -0x10000;
	    }
	}
	goto returnNumericObject;

	/*
	 * 32-bit numeric values.
	 */

    case 'i':
    case 'I':
    case 'n':
	if (NeedReversing(type)) {
	    value = (long) (buffer[0]
		    + (buffer[1] << 8)
		    + (buffer[2] << 16)
		    + (((unsigned long)buffer[3]) << 24));
	} else {
	    value = (long) (buffer[3]
		    + (buffer[2] << 8)
		    + (buffer[1] << 16)
		    + (((unsigned long) buffer[0]) << 24));
	}

	/*
	 * Check to see if the value was sign extended properly on systems
	 * where an int is more than 32-bits.
	 *
	 * We avoid caching unsigned integers as we cannot distinguish between
	 * 32bit signed and unsigned in the hash (short and char are ok).
	 */

	if (flags & BINARY_UNSIGNED) {
	    return Tcl_NewWideIntObj((Tcl_WideInt)(unsigned long)value);
	}
	if ((value & (1U << 31)) && (value > 0)) {
	    value -= (1U << 31);
	    value -= (1U << 31);
	}

    returnNumericObject:
	if (*numberCachePtrPtr == NULL) {
	    return Tcl_NewWideIntObj(value);
	} else {
	    Tcl_HashTable *tablePtr = *numberCachePtrPtr;
	    Tcl_HashEntry *hPtr;
	    int isNew;

	    hPtr = Tcl_CreateHashEntry(tablePtr, INT2PTR(value), &isNew);
	    if (!isNew) {
		return (Tcl_Obj *)Tcl_GetHashValue(hPtr);
	    }
	    if (tablePtr->numEntries <= BINARY_SCAN_MAX_CACHE) {
		Tcl_Obj *objPtr;

		TclNewIntObj(objPtr, value);
		Tcl_IncrRefCount(objPtr);
		Tcl_SetHashValue(hPtr, objPtr);
		return objPtr;
	    }

	    /*
	     * We've overflowed the cache! Someone's parsing a LOT of varied
	     * binary data in a single call! Bail out by switching back to the
	     * old behaviour for the rest of the scan.
	     *
	     * Note that anyone just using the 'c' conversion (for bytes)
	     * cannot trigger this.
	     */

	    DeleteScanNumberCache(tablePtr);
	    *numberCachePtrPtr = NULL;
	    return Tcl_NewWideIntObj(value);
	}

	/*
	 * Do not cache wide (64-bit) values; they are already too large to
	 * use as keys.
	 */

    case 'w':
    case 'W':
    case 'm':
	if (NeedReversing(type)) {
	    uwvalue = ((Tcl_WideUInt) buffer[0])
		    | (((Tcl_WideUInt) buffer[1]) << 8)
		    | (((Tcl_WideUInt) buffer[2]) << 16)
		    | (((Tcl_WideUInt) buffer[3]) << 24)
		    | (((Tcl_WideUInt) buffer[4]) << 32)
		    | (((Tcl_WideUInt) buffer[5]) << 40)
		    | (((Tcl_WideUInt) buffer[6]) << 48)
		    | (((Tcl_WideUInt) buffer[7]) << 56);
	} else {
	    uwvalue = ((Tcl_WideUInt) buffer[7])
		    | (((Tcl_WideUInt) buffer[6]) << 8)
		    | (((Tcl_WideUInt) buffer[5]) << 16)
		    | (((Tcl_WideUInt) buffer[4]) << 24)
		    | (((Tcl_WideUInt) buffer[3]) << 32)
		    | (((Tcl_WideUInt) buffer[2]) << 40)
		    | (((Tcl_WideUInt) buffer[1]) << 48)
		    | (((Tcl_WideUInt) buffer[0]) << 56);
	}
	if (flags & BINARY_UNSIGNED) {
	    Tcl_Obj *bigObj = NULL;
	    mp_int big;

	    if (mp_init_u64(&big, uwvalue) == MP_OKAY) {
		bigObj = Tcl_NewBignumObj(&big);
	    }
	    return bigObj;
	}
	return Tcl_NewWideIntObj((Tcl_WideInt) uwvalue);

	/*
	 * Do not cache double values; they are already too large to use as
	 * keys and the values stored are utterly incompatible with the
	 * integer part of the cache.
	 */

	/*
	 * 32-bit IEEE single-precision floating point.
	 */

    case 'f':
    case 'R':
    case 'r':
	CopyNumber(buffer, &fvalue, sizeof(float), type);
	return Tcl_NewDoubleObj(fvalue);

	/*
	 * 64-bit IEEE double-precision floating point.
	 */

    case 'd':
    case 'Q':
    case 'q':
	CopyNumber(buffer, &dvalue, sizeof(double), type);
	return Tcl_NewDoubleObj(dvalue);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteScanNumberCache --
 *
 *	Deletes the hash table acting as a scan number cache.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Decrements the reference counts of the objects in the cache.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteScanNumberCache(
    Tcl_HashTable *numberCachePtr)
				/* Pointer to the hash table, or NULL (when
				 * the cache has already been deleted due to
				 * overflow.) */
{
    Tcl_HashEntry *hEntry;
    Tcl_HashSearch search;

    if (numberCachePtr == NULL) {
	return;
    }

    hEntry = Tcl_FirstHashEntry(numberCachePtr, &search);
    while (hEntry != NULL) {
	Tcl_Obj *value = (Tcl_Obj *)Tcl_GetHashValue(hEntry);

	if (value != NULL) {
	    Tcl_DecrRefCount(value);
	}
	hEntry = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(numberCachePtr);
}

/*
 * ----------------------------------------------------------------------
 *
 * NOTES --
 *
 *	Some measurements show that it is faster to use a table to perform
 *	uuencode and base64 value encoding than to calculate the output (at
 *	least on intel P4 arch).
 *
 *	Conversely using a lookup table for the decoding is slower than just
 *	calculating the values. We therefore use the fastest of each method.
 *
 *	Presumably this has to do with the size of the tables. The base64
 *	decode table is 255 bytes while the encode table is only 65 bytes. The
 *	choice likely depends on CPU memory cache sizes.
 */

/*
 *----------------------------------------------------------------------
 *
 * BinaryEncodeHex --
 *
 *	Implement the [binary encode hex] binary encoding. clientData must be
 *	a table to convert values to hexadecimal digits.
 *
 * Results:
 *	Interp result set to an encoded byte array object
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
BinaryEncodeHex(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj = NULL;
    unsigned char *data = NULL;
    unsigned char *cursor = NULL;
    Tcl_Size offset = 0, count = 0;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "data");
	return TCL_ERROR;
    }

    data = Tcl_GetBytesFromObj(interp, objv[1], &count);
    if (data == NULL) {
	return TCL_ERROR;
    }

    TclNewObj(resultObj);
    cursor = Tcl_SetByteArrayLength(resultObj, count * 2);
    for (offset = 0; offset < count; ++offset) {
	*cursor++ = HexDigits[(data[offset] >> 4) & 0x0F];
	*cursor++ = HexDigits[data[offset] & 0x0F];
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryDecodeHex --
 *
 *	Implement the [binary decode hex] binary encoding.
 *
 * Results:
 *	Interp result set to an decoded byte array object
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
BinaryDecodeHex(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj = NULL;
    unsigned char *data, *datastart, *dataend;
    unsigned char *begin, *cursor, c;
    int i, index, value, pure = 1, strict = 0;
    Tcl_Size size, cut = 0, count = 0;
    int ucs4;
    enum {OPT_STRICT };
    static const char *const optStrings[] = { "-strict", NULL };

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? data");
	return TCL_ERROR;
    }
    for (i = 1; i < objc - 1; ++i) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case OPT_STRICT:
	    strict = 1;
	    break;
	default:
	    TCL_UNREACHABLE();
	}
    }

    TclNewObj(resultObj);
    data = Tcl_GetBytesFromObj(NULL, objv[objc - 1], &count);
    if (data == NULL) {
	pure = 0;
	data = (unsigned char *)TclGetStringFromObj(objv[objc - 1], &count);
    }
    datastart = data;
    dataend = data + count;
    size = (count + 1) / 2;
    begin = cursor = Tcl_SetByteArrayLength(resultObj, size);
    while (data < dataend) {
	value = 0;
	for (i = 0 ; i < 2 ; i++) {
	    if (data >= dataend) {
		value <<= 4;
		break;
	    }

	    c = *data++;
	    if (!isxdigit(UCHAR(c))) {
		if (strict || !TclIsSpaceProc(c)) {
		    goto badChar;
		}
		i--;
		continue;
	    }

	    value <<= 4;
	    c -= '0';
	    if (c > 9) {
		c += ('0' - 'A') + 10;
	    }
	    if (c > 16) {
		c += ('A' - 'a');
	    }
	    value |= c & 0xF;
	}
	if (i < 2) {
	    cut++;
	}
	*cursor++ = UCHAR(value);
	value = 0;
    }
    if (cut > size) {
	cut = size;
    }
    Tcl_SetByteArrayLength(resultObj, cursor - begin - cut);
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;

  badChar:
    if (pure) {
	ucs4 = c;
    } else {
	TclUtfToUniChar((const char *)(data - 1), &ucs4);
    }
    TclDecrRefCount(resultObj);
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "invalid hexadecimal digit \"%c\" (U+%06X) at position %"
	    TCL_Z_MODIFIER "u", ucs4, ucs4, data - datastart - 1));
    Tcl_SetErrorCode(interp, "TCL", "BINARY", "DECODE", "INVALID", (char *)NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryEncode64 --
 *
 *	This procedure implements the "binary encode base64" Tcl command.
 *
 * Results:
 *	The base64 encoded value prescribed by the input arguments.
 *
 *----------------------------------------------------------------------
 */

#define OUTPUT(c) \
    do {						\
	*cursor++ = (c);				\
	outindex++;					\
	if (maxlen > 0 && cursor != limit) {		\
	    if (outindex == maxlen) {			\
		memcpy(cursor, wrapchar, wrapcharlen);	\
		cursor += wrapcharlen;			\
		outindex = 0;				\
	    }						\
	}						\
	if (cursor > limit) {				\
	    Tcl_Panic("limit hit");			\
	}						\
    } while (0)

static int
BinaryEncode64(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj;
    unsigned char *data, *limit;
    Tcl_WideInt maxlen = 0;
    const char *wrapchar = "\n";
    Tcl_Size wrapcharlen = 1;
    int index, purewrap = 1;
    Tcl_Size i, offset, size, outindex = 0, count = 0;
    enum { OPT_MAXLEN, OPT_WRAPCHAR };
    static const char *const optStrings[] = { "-maxlen", "-wrapchar", NULL };

    if (objc < 2 || objc % 2 != 0) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?-maxlen len? ?-wrapchar char? data");
	return TCL_ERROR;
    }
    for (i = 1; i < objc - 1; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case OPT_MAXLEN:
	    if (TclGetWideIntFromObj(interp, objv[i + 1], &maxlen) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (maxlen < 0) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"line length out of range", -1));
		Tcl_SetErrorCode(interp, "TCL", "BINARY", "ENCODE",
			"LINE_LENGTH", (char *)NULL);
		return TCL_ERROR;
	    }
	    break;
	case OPT_WRAPCHAR:
	    wrapchar = (const char *)Tcl_GetBytesFromObj(NULL,
		    objv[i + 1], &wrapcharlen);
	    if (wrapchar == NULL) {
		purewrap = 0;
		wrapchar = TclGetStringFromObj(objv[i + 1], &wrapcharlen);
	    }
	    break;
	default:
	    TCL_UNREACHABLE();
	}
    }
    if (wrapcharlen == 0) {
	maxlen = 0;
    }

    data = Tcl_GetBytesFromObj(interp, objv[objc - 1], &count);
    if (data == NULL) {
	return TCL_ERROR;
    }
    TclNewObj(resultObj);
    if (count > 0) {
	unsigned char *cursor = NULL;

	size = (((count * 4) / 3) + 3) & ~3;	/* ensure 4 byte chunks */
	if (maxlen > 0 && size > maxlen) {
	    Tcl_Size adjusted = size + (wrapcharlen * (size / maxlen));

	    if (size % maxlen == 0) {
		adjusted -= wrapcharlen;
	    }
	    size = adjusted;

	    if (purewrap == 0) {
		/* Wrapchar is (possibly) non-byte, so build result as
		 * general string, not bytearray */
		Tcl_SetObjLength(resultObj, size);
		cursor = (unsigned char *) TclGetString(resultObj);
	    }
	}
	if (cursor == NULL) {
	    cursor = Tcl_SetByteArrayLength(resultObj, size);
	}
	limit = cursor + size;
	for (offset = 0; offset < count; offset += 3) {
	    unsigned char d[3] = {0, 0, 0};

	    for (i = 0; i < 3 && offset + i < count; ++i) {
		d[i] = data[offset + i];
	    }
	    OUTPUT(B64Digits[d[0] >> 2]);
	    OUTPUT(B64Digits[((d[0] & 0x03) << 4) | (d[1] >> 4)]);
	    if (offset + 1 < count) {
		OUTPUT(B64Digits[((d[1] & 0x0F) << 2) | (d[2] >> 6)]);
	    } else {
		OUTPUT(B64Digits[64]);
	    }
	    if (offset+2 < count) {
		OUTPUT(B64Digits[d[2] & 0x3F]);
	    } else {
		OUTPUT(B64Digits[64]);
	    }
	}
    }
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}
#undef OUTPUT

/*
 *----------------------------------------------------------------------
 *
 * BinaryEncodeUu --
 *
 *	This implements the uuencode binary encoding. Input is broken into 6
 *	bit chunks and a lookup table is used to turn these values into output
 *	characters. This differs from the generic code above in that line
 *	lengths are also encoded.
 *
 * Results:
 *	Interp result set to an encoded byte array object
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
BinaryEncodeUu(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj;
    unsigned char *data, *start, *cursor;
    int i, bits;
    unsigned int n;
    int lineLength = 61;
    const unsigned char SingleNewline[] = { UCHAR('\n') };
    const unsigned char *wrapchar = SingleNewline;
    Tcl_Size j, rawLength, offset, count = 0, wrapcharlen = sizeof(SingleNewline);
    enum { OPT_MAXLEN, OPT_WRAPCHAR } index;
    static const char *const optStrings[] = { "-maxlen", "-wrapchar", NULL };

    if (objc < 2 || objc % 2 != 0) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"?-maxlen len? ?-wrapchar char? data");
	return TCL_ERROR;
    }
    for (i = 1; i < objc - 1; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case OPT_MAXLEN:
	    if (Tcl_GetIntFromObj(interp, objv[i + 1],
		    &lineLength) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (lineLength < 5 || lineLength > 85) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"line length out of range", -1));
		Tcl_SetErrorCode(interp, "TCL", "BINARY", "ENCODE",
			"LINE_LENGTH", (char *)NULL);
		return TCL_ERROR;
	    }
	    lineLength = ((lineLength - 1) & -4) + 1; /* 5, 9, 13 ... */
	    break;
	case OPT_WRAPCHAR:
	    wrapchar = (const unsigned char *)TclGetStringFromObj(
		    objv[i + 1], &wrapcharlen);
	    const unsigned char *p = wrapchar;
	    Tcl_Size numBytes = wrapcharlen;

	    while (numBytes) {
		switch (*p) {
		case '\t':
		case '\v':
		case '\f':
		case '\r':
		    p++;
		    numBytes--;
		    continue;
		case '\n':
		    numBytes--;
		    break;
		default:
		    goto badwrap;
		}
	    }
	    if (numBytes) {
	    badwrap:
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"invalid wrapchar; will defeat decoding", -1));
		Tcl_SetErrorCode(interp, "TCL", "BINARY",
			"ENCODE", "WRAPCHAR", (char *)NULL);
		return TCL_ERROR;
	    }
	    break;
	}
    }

    /*
     * Allocate the buffer. This is a little bit too long, but is "good
     * enough".
     */

    offset = 0;
    data = Tcl_GetBytesFromObj(interp, objv[objc - 1], &count);
    if (data == NULL) {
	return TCL_ERROR;
    }
    TclNewObj(resultObj);
    rawLength = (lineLength - 1) * 3 / 4;
    start = cursor = Tcl_SetByteArrayLength(resultObj,
	    (lineLength + wrapcharlen) *
	    ((count + (rawLength - 1)) / rawLength));
    n = bits = 0;

    /*
     * Encode the data. Each output line first has the length of raw data
     * encoded by the output line described in it by one encoded byte, then
     * the encoded data follows (encoding each 6 bits as one character).
     * Encoded lines are always terminated by a newline.
     */

    while (offset < count) {
	Tcl_Size lineLen = count - offset;

	if (lineLen > rawLength) {
	    lineLen = rawLength;
	}
	*cursor++ = UueDigits[lineLen];
	for (i = 0 ; i < lineLen ; i++) {
	    n <<= 8;
	    n |= data[offset++];
	    for (bits += 8; bits > 6 ; bits -= 6) {
		*cursor++ = UueDigits[(n >> (bits - 6)) & 0x3F];
	    }
	}
	if (bits > 0) {
	    n <<= 8;
	    *cursor++ = UueDigits[(n >> (bits + 2)) & 0x3F];
	    bits = 0;
	}
	for (j = 0 ; j < wrapcharlen ; ++j) {
	    *cursor++ = wrapchar[j];
	}
    }

    /*
     * Fix the length of the output bytearray.
     */

    Tcl_SetByteArrayLength(resultObj, cursor - start);
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryDecodeUu --
 *
 *	Decode a uuencoded string.
 *
 * Results:
 *	Interp result set to an byte array object
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
BinaryDecodeUu(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj = NULL;
    unsigned char *data, *datastart, *dataend;
    unsigned char *begin, *cursor;
    int i, index, pure = 1, strict = 0, lineLen;
    Tcl_Size size, count = 0;
    unsigned char c;
    int ucs4;
    enum { OPT_STRICT };
    static const char *const optStrings[] = { "-strict", NULL };

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? data");
	return TCL_ERROR;
    }
    for (i = 1; i < objc - 1; ++i) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case OPT_STRICT:
	    strict = 1;
	    break;
	default:
	    TCL_UNREACHABLE();
	}
    }

    TclNewObj(resultObj);
    data = Tcl_GetBytesFromObj(NULL, objv[objc - 1], &count);
    if (data == NULL) {
	pure = 0;
	data = (unsigned char *) TclGetStringFromObj(objv[objc - 1], &count);
    }
    datastart = data;
    dataend = data + count;
    size = ((count + 3) & ~3) * 3 / 4;
    begin = cursor = Tcl_SetByteArrayLength(resultObj, size);
    lineLen = -1;

    /*
     * The decoding loop. First, we get the length of line (strictly, the
     * number of data bytes we expect to generate from the line) we're
     * processing this time round if it is not already known (i.e., when the
     * lineLen variable is set to the magic value, -1).
     */

    while (data < dataend) {
	char d[4] = {0, 0, 0, 0};

	if (lineLen < 0) {
	    c = *data++;
	    if (c < 32 || c > 96) {
		if (strict || !TclIsSpaceProc(c)) {
		    goto badUu;
		}
		i--;
		continue;
	    }
	    lineLen = (c - 32) & 0x3F;
	}

	/*
	 * Now we read a four-character grouping.
	 */

	for (i = 0 ; i < 4 ; i++) {
	    if (data < dataend) {
		d[i] = c = *data++;
		if (c < 32 || c > 96) {
		    if (strict) {
			if (!TclIsSpaceProc(c)) {
			    goto badUu;
			} else if (c == '\n') {
			    goto shortUu;
			}
		    }
		    i--;
		    continue;
		}
	    }
	}

	/*
	 * Translate that grouping into (up to) three binary bytes output.
	 */

	if (lineLen > 0) {
	    *cursor++ = (unsigned char)(((d[0] - 0x20) & 0x3F) << 2)
		    | (((d[1] - 0x20) & 0x3F) >> 4);
	    if (--lineLen > 0) {
		*cursor++ = (unsigned char)(((d[1] - 0x20) & 0x3F) << 4)
			| (((d[2] - 0x20) & 0x3F) >> 2);
		if (--lineLen > 0) {
		    *cursor++ = (unsigned char)((((d[2] - 0x20) & 0x3F) << 6)
			    | (((d[3] - 0x20) & 0x3F)));
		    lineLen--;
		}
	    }
	}

	/*
	 * If we've reached the end of the line, skip until we process a
	 * newline.
	 */

	if (lineLen == 0 && data < dataend) {
	    lineLen = -1;
	    do {
		c = *data++;
		if (c == '\n') {
		    break;
		} else if (c >= 32 && c <= 96) {
		    data--;
		    break;
		} else if (strict || !TclIsSpaceProc(c)) {
		    goto badUu;
		}
	    } while (data < dataend);
	}
    }

    /*
     * Sanity check, clean up and finish.
     */

    if (lineLen > 0 && strict) {
	goto shortUu;
    }
    Tcl_SetByteArrayLength(resultObj, cursor - begin);
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;

  shortUu:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("short uuencode data"));
    Tcl_SetErrorCode(interp, "TCL", "BINARY", "DECODE", "SHORT", (char *)NULL);
    TclDecrRefCount(resultObj);
    return TCL_ERROR;

  badUu:
    if (pure) {
	ucs4 = c;
    } else {
	TclUtfToUniChar((const char *)(data - 1), &ucs4);
    }
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "invalid uuencode character \"%c\" (U+%06X) at position %"
	    TCL_Z_MODIFIER "u", ucs4, ucs4, data - datastart - 1));
    Tcl_SetErrorCode(interp, "TCL", "BINARY", "DECODE", "INVALID", (char *)NULL);
    TclDecrRefCount(resultObj);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * BinaryDecode64 --
 *
 *	Decode a base64 encoded string.
 *
 * Results:
 *	Interp result set to an byte array object
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static int
BinaryDecode64(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj = NULL;
    unsigned char *data, *datastart, *dataend, c = '\0';
    unsigned char *begin = NULL;
    unsigned char *cursor = NULL;
    int pure = 1, strict = 0;
    int i, index, cut = 0;
    Tcl_Size size, count = 0;
    int ucs4;
    enum { OPT_STRICT };
    static const char *const optStrings[] = { "-strict", NULL };

    if (objc < 2 || objc > 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? data");
	return TCL_ERROR;
    }
    for (i = 1; i < objc - 1; ++i) {
	if (Tcl_GetIndexFromObj(interp, objv[i], optStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case OPT_STRICT:
	    strict = 1;
	    break;
	default:
	    TCL_UNREACHABLE();
	}
    }

    TclNewObj(resultObj);
    data = Tcl_GetBytesFromObj(NULL, objv[objc - 1], &count);
    if (data == NULL) {
	pure = 0;
	data = (unsigned char *) TclGetStringFromObj(objv[objc - 1], &count);
    }
    datastart = data;
    dataend = data + count;
    size = ((count + 3) & ~3) * 3 / 4;
    begin = cursor = Tcl_SetByteArrayLength(resultObj, size);
    while (data < dataend) {
	unsigned long value = 0;

	/*
	 * Decode the current block. Each base64 block consists of four input
	 * characters A-Z, a-z, 0-9, +, or /. Each character supplies six bits
	 * of output data, so each block's output is 24 bits (three bytes) in
	 * length. The final block can be shorter by one or two bytes, denoted
	 * by the input ending with one or two ='s, respectively.
	 */

	for (i = 0; i < 4; i++) {
	    /*
	     * Get the next input character. At end of input, pad with at most
	     * two ='s. If more than two ='s would be needed, instead discard
	     * the block read thus far.
	     */

	    if (data < dataend) {
		c = *data++;
	    } else if (i > 1) {
		c = '=';
	    } else {
		if (strict && i <= 1) {
		    /*
		     * Single resp. unfulfilled char (each 4th next single
		     * char) is rather bad64 error case in strict mode.
		     */

		    goto bad64;
		}
		cut += 3;
		break;
	    }

	    /*
	     * Load the character into the block value. Handle ='s specially
	     * because they're only valid as the last character or two of the
	     * final block of input. Unless strict mode is enabled, skip any
	     * input whitespace characters.
	     */

	    if (cut) {
		if (c == '=' && i > 1) {
		    value <<= 6;
		    cut++;
		} else if (!strict) {
		    i--;
		} else {
		    goto bad64;
		}
	    } else if (c >= 'A' && c <= 'Z') {
		value = (value << 6) | ((c - 'A') & 0x3F);
	    } else if (c >= 'a' && c <= 'z') {
		value = (value << 6) | ((c - 'a' + 26) & 0x3F);
	    } else if (c >= '0' && c <= '9') {
		value = (value << 6) | ((c - '0' + 52) & 0x3F);
	    } else if (c == '+') {
		value = (value << 6) | 0x3E;
	    } else if (c == '/') {
		value = (value << 6) | 0x3F;
	    } else if (c == '=' && (!strict || i > 1)) {
		/*
		 * "=" and "a=" is rather bad64 error case in strict mode.
		 */

		value <<= 6;
		if (i) {
		    cut++;
		}
	    } else if (strict) {
		goto bad64;
	    } else {
		i--;
	    }
	}
	*cursor++ = UCHAR((value >> 16) & 0xFF);
	*cursor++ = UCHAR((value >> 8) & 0xFF);
	*cursor++ = UCHAR(value & 0xFF);

	/*
	 * Since = is only valid within the final block, if it was encountered
	 * but there are still more input characters, confirm that strict mode
	 * is off and all subsequent characters are whitespace.
	 */

	if (cut && data < dataend) {
	    if (strict) {
		goto bad64;
	    }
	}
    }
    Tcl_SetByteArrayLength(resultObj, cursor - begin - cut);
    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;

  bad64:
    if (pure) {
	ucs4 = c;
    } else {
	/* The decoder is byte-oriented. If we saw a byte that's not a
	 * valid member of the base64 alphabet, it could be the lead byte
	 * of a multi-byte character. */

	/* Safe because we know data is NUL-terminated */
	TclUtfToUniChar((const char *)(data - 1), &ucs4);
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "invalid base64 character \"%c\" (U+%06X) at position %"
	    TCL_Z_MODIFIER "u", ucs4, ucs4, data - datastart - 1));
    Tcl_SetErrorCode(interp, "TCL", "BINARY", "DECODE", "INVALID", (char *)NULL);
    TclDecrRefCount(resultObj);
    return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
