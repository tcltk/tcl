/*
 * tclUtf.c --
 *
 *	Routines for manipulating UTF-8 strings.
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUtf.c 1.25 98/01/28 18:02:43
 */

#include "tclInt.h"

/*
 * Unicode characters less than this value are represented by themselves 
 * in UTF-8 strings. 
 */

#define UNICODE_SELF	0x80

/*
 * The following structures are used when mapping between Unicode (UCS-2)
 * and UTF-8.
 */
 
CONST unsigned char totalBytes[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
#if TCL_UTF_MAX > 3
    4,4,4,4,4,4,4,4,
#else
    1,1,1,1,1,1,1,1,
#endif
#if TCL_UTF_MAX > 4
    5,5,5,5,
#else
    1,1,1,1,
#endif
#if TCL_UTF_MAX > 5
    6,6,6,6
#else
    1,1,1,1
#endif
};


/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UniCharToUtf --
 *
 *	Store the given Tcl_UniChar as a sequence of UTF-8 bytes in the
 *	provided buffer.  Equivalent to Plan 9 runetochar().
 *
 * Results:
 *	The return values is the number of bytes in the buffer that
 *	were consumed.  
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
INLINE int
Tcl_UniCharToUtf(ch, str)
    int ch;			/* The Tcl_UniChar to be stored in the
				 * buffer. */
    char *str;			/* Buffer in which the UTF-8 representation
				 * of the Tcl_UniChar is stored.  Buffer must
				 * be large enough to hold the UTF-8 character
				 * (at most TCL_UTF_MAX bytes). */
{
    if ((ch > 0) && (ch < UNICODE_SELF)) {
	str[0] = (char) ch;
	return 1;
    }
    if (ch <= 0x7FF) {
	str[1] = (char) ((ch | 0x80) & 0xBF);
	str[0] = (char) ((ch >> 6) | 0xC0);
	return 2;
    }
    if (ch <= 0xFFFF) {
	three:
	str[2] = (char) ((ch | 0x80) & 0xBF);
	str[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
	str[0] = (char) ((ch >> 12) | 0xE0);
	return 3;
    }

#if TCL_UTF_MAX > 3
    if (ch <= 0x1FFFFF) {
	str[3] = (char) ((ch | 0x80) & 0xBF);
	str[2] = (char) (((ch >> 6) | 0x80) & 0xBF);
	str[1] = (char) (((ch >> 12) | 0x80) & 0xBF);
	str[0] = (char) ((ch >> 18) | 0xF0);
	return 4;
    }
    if (ch <= 0x3FFFFFF) {
	str[4] = (char) ((ch | 0x80) & 0xBF);
	str[3] = (char) (((ch >> 6) | 0x80) & 0xBF);
	str[2] = (char) (((ch >> 12) | 0x80) & 0xBF);
	str[1] = (char) (((ch >> 18) | 0x80) & 0xBF);
	str[0] = (char) ((ch >> 24) | 0xF8);
	return 5;
    }
    if (ch <= 0x7FFFFFFF) {
	str[5] = (char) ((ch | 0x80) & 0xBF);
	str[4] = (char) (((ch >> 6) | 0x80) & 0xBF);
	str[3] = (char) (((ch >> 12) | 0x80) & 0xBF);
	str[2] = (char) (((ch >> 18) | 0x80) & 0xBF);
	str[1] = (char) (((ch >> 24) | 0x80) & 0xBF);
	str[0] = (char) ((ch >> 30) | 0xFC);
	return 6;
    }
#endif

    ch = 0xFFFD;
    goto three;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclUniCharToUtfDString --
 *
 *	Convert the given Unicode string to UTF-8.
 *
 * Results:
 *	The return value is a pointer to the UTF-8 representation of the
 *	Unicode string.  Storage for the return value is appended to the
 *	end of dsPtr.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
char *
TclUniCharToUtfDString(wString, numChars, dsPtr)
    CONST Tcl_UniChar *wString;	/* Unicode string to convert to UTF-8. */
    int numChars;		/* Length of Unicode string in Tcl_UniChars
				 * (must be >= 0). */
    Tcl_DString *dsPtr;		/* UTF-8 representation of string is
				 * appended to this previously initialized
				 * DString. */
{
    CONST Tcl_UniChar *w, *wEnd;
    char *p, *string;
    int oldLength;

    /*
     * UTF-8 string length in bytes will be <= Unicode string length *
     * TCL_UTF_MAX.
     */

    oldLength = Tcl_DStringLength(dsPtr);
    Tcl_DStringSetLength(dsPtr, (oldLength + numChars + 1) * TCL_UTF_MAX);
    string = Tcl_DStringValue(dsPtr) + oldLength;

    p = string;
    wEnd = wString + numChars;
    for (w = wString; w < wEnd; ) {
	p += Tcl_UniCharToUtf(*w, p);
	w++;
    }
    Tcl_DStringSetLength(dsPtr, oldLength + (p - string));

    return string;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfToUniChar --
 *
 *	Extract the Tcl_UniChar represented by the UTF-8 string.  Bad
 *	UTF-8 sequences are converted to valid Tcl_UniChars and processing
 *	continues.  Equivalent to Plan 9 chartorune().
 *
 *	The caller must ensure that the source buffer is long enough that
 *	this routine does not run off the end and dereference non-existent
 *	memory looking for trail bytes.  If the source buffer is known to
 *	be '\0' terminated, this cannot happen.  Otherwise, the caller
 *	should call Tcl_UtfCharComplete() before calling this routine to
 *	ensure that enough bytes remain in the string.
 *
 * Results:
 *	*chPtr is filled with the Tcl_UniChar, and the return value is the
 *	number of bytes from the UTF-8 string that were consumed.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
int
Tcl_UtfToUniChar(str, chPtr)
    register CONST char *str;	 /* The UTF-8 string. */
    register Tcl_UniChar *chPtr; /* Filled with the Tcl_UniChar represented
				  * by the UTF-8 string. */
{
    register int byte;
    
    /*
     * Unroll 1 to 3 byte UTF-8 sequences, use loop to handle longer ones.
     */

    byte = *((unsigned char *) str);
    if (byte < 0xC0) {
	/*
	 * Handles properly formed UTF-8 characters between 0x01 and 0x7F.
	 * Also treats \0 and naked trail bytes 0x80 to 0xBF as valid
	 * characters representing themselves.
	 */
	 
	*chPtr = (Tcl_UniChar) byte;
	return 1;
    } else if (byte < 0xE0) {
	if ((str[1] & 0xC0) == 0x80) {
	    /*
	     * Two-byte-character lead-byte followed by a trail-byte.
	     */
	     
	    *chPtr = (Tcl_UniChar) (((byte & 0x1F) << 6) | (str[1] & 0x3F));
	    return 2;
	}
	/*
	 * A two-byte-character lead-byte not followed by trail-byte
	 * represents itself.
	 */
	 
	*chPtr = (Tcl_UniChar) byte;
	return 1;
    } else if (byte < 0xF0) {
	if (((str[1] & 0xC0) == 0x80) && ((str[2] & 0xC0) == 0x80)) {
	    /*
	     * Three-byte-character lead byte followed by two trail bytes.
	     */

	    *chPtr = (Tcl_UniChar) (((byte & 0x0F) << 12) 
		    | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F));
	    return 3;
	}
	/*
	 * A three-byte-character lead-byte not followed by two trail-bytes
	 * represents itself.
	 */

	*chPtr = (Tcl_UniChar) byte;
	return 1;
    }
#if TCL_UTF_MAX > 3
    else {
	int ch, total, trail;

	total = totalBytes[byte];
	trail = total - 1;
	if (trail > 0) {
	    ch = byte & (0x3F >> trail);
	    do {
		str++;
		if ((*str & 0xC0) != 0x80) {
		    *chPtr = byte;
		    return 1;
		}
		ch <<= 6;
		ch |= (*str & 0x3F);
		trail--;
	    } while (trail > 0);
	    *chPtr = ch;
	    return total;
	}
    }
#endif

    *chPtr = (Tcl_UniChar) byte;
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclUtfToUniCharDString --
 *
 *	Convert the UTF-8 string to Unicode.
 *
 * Results:
 *	The return value is a pointer to the Unicode representation of the
 *	UTF-8 string.  Storage for the return value is appended to the
 *	end of dsPtr.  The Unicode string is terminated with a Unicode
 *	NULL character.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

Tcl_UniChar *
TclUtfToUniCharDString(string, length, dsPtr)
    CONST char *string;		/* UTF-8 string to convert to Unicode. */
    int length;			/* Length of UTF-8 string in bytes, or -1
				 * for strlen(). */
    Tcl_DString *dsPtr;		/* Unicode representation of string is
				 * appended to this previously initialized
				 * DString. */
{
    Tcl_UniChar *w, *wString;
    CONST char *p, *end;
    int oldLength;

    if (length < 0) {
	length = strlen(string);
    }

    /*
     * Unicode string length in Tcl_UniChars will be <= UTF-8 string length
     * in bytes.
     */

    oldLength = Tcl_DStringLength(dsPtr);
    Tcl_DStringSetLength(dsPtr,
	    (int) ((oldLength + length + 1) * sizeof(Tcl_UniChar)));
    wString = (Tcl_UniChar *) (Tcl_DStringValue(dsPtr) + oldLength);

    w = wString;
    end = string + length;
    for (p = string; p < end; ) {
	p += Tcl_UtfToUniChar(p, w);
	w++;
    }
    *w = '\0';
    Tcl_DStringSetLength(dsPtr,
	    (oldLength + ((char *) w - (char *) wString)));

    return wString;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfCharComplete --
 *
 *	Determine if the UTF-8 string of the given length is long enough
 *	to be decoded by Tcl_UtfToUniChar().  This does not ensure that the
 *	UTF-8 string is properly formed.  Equivalent to Plan 9 fullrune().
 *
 * Results:
 *	The return value is 0 if the string is not long enough, non-zero
 *	otherwise.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UtfCharComplete(str, len)
    CONST char *str;		/* String to check if first few bytes
				 * contain a complete UTF-8 character. */
    int len;			/* Length of above string in bytes. */
{
    int ch;

    ch = *((unsigned char *) str);
    return len >= totalBytes[ch];
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_NumUtfChars --
 *
 *	Returns the number of characters (not bytes) in the UTF-8 string,
 *	not including the terminating NULL byte.  This is equivalent to
 *	Plan 9 utflen() and utfnlen().
 *
 * Results:
 *	As above.  
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
int 
Tcl_NumUtfChars(str, len)
    register CONST char *str;	/* The UTF-8 string to measure. */
    int len;			/* The length of the string in bytes, or -1
				 * for strlen(string). */
{
    Tcl_UniChar ch;
    register Tcl_UniChar *chPtr = &ch;
    register int n;
    int i;

    /*
     * The separate implementations are faster.
     */
     
    i = 0;
    if (len < 0) {
	while (1) {
	    str += Tcl_UtfToUniChar(str, chPtr);
	    if (ch == '\0') {
		break;
	    }
	    i++;
	}
    } else {
	while (len > 0) {
	    n = Tcl_UtfToUniChar(str, chPtr);
	    len -= n;
	    str += n;
	    i++;
	}
    }
    return i;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfFindFirst --
 *
 *	Returns a pointer to the first occurance of the given Tcl_UniChar
 *	in the NULL-terminated UTF-8 string.  The NULL terminator is
 *	considered part of the UTF-8 string.  Equivalent to Plan 9
 *	utfrune().
 *
 * Results:
 *	As above.  If the Tcl_UniChar does not exist in the given string,
 *	the return value is NULL.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
char *
Tcl_UtfFindFirst(string, ch)
    CONST char *string;		/* The UTF-8 string to be searched. */
    int ch;			/* The Tcl_UniChar to search for. */
{
    int len;
    Tcl_UniChar find;
    
    while (1) {
	len = Tcl_UtfToUniChar(string, &find);
	if (find == ch) {
	    return (char *) string;
	}
	if (*string == '\0') {
	    return NULL;
	}
	string += len;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfFindLast --
 *
 *	Returns a pointer to the last occurance of the given Tcl_UniChar
 *	in the NULL-terminated UTF-8 string.  The NULL terminator is
 *	considered part of the UTF-8 string.  Equivalent to Plan 9
 *	utfrrune().
 *
 * Results:
 *	As above.  If the Tcl_UniChar does not exist in the given string,
 *	the return value is NULL.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

char *
Tcl_UtfFindLast(string, ch)
    CONST char *string;		/* The UTF-8 string to be searched. */
    int ch;			/* The Tcl_UniChar to search for. */
{
    int len;
    Tcl_UniChar find;
    CONST char *last;
	
    last = NULL;
    while (1) {
	len = Tcl_UtfToUniChar(string, &find);
	if (find == ch) {
	    last = string;
	}
	if (*string == '\0') {
	    break;
	}
	string += len;
    }
    return (char *) last;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfNext --
 *
 *	Given a pointer to some current location in a UTF-8 string,
 *	move forward one character.  The caller must ensure that they
 *	are not asking for the next character after the last character
 *	in the string.
 *
 * Results:
 *	The return value is the pointer to the next character in
 *	the UTF-8 string.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
char *
Tcl_UtfNext(str) 
    CONST char *str;		    /* The current location in the string. */
{
    Tcl_UniChar ch;

    return (char *) str + Tcl_UtfToUniChar(str, &ch);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfPrev --
 *
 *	Given a pointer to some current location in a UTF-8 string,
 *	move backwards one character.
 *
 * Results:
 *	The return value is a pointer to the previous character in the
 *	UTF-8 string.  If the current location was already at the
 *	beginning of the string, the return value will also be a
 *	pointer to the beginning of the string.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

char *
Tcl_UtfPrev(str, start)
    CONST char *str;		    /* The current location in the string. */
    CONST char *start;		    /* Pointer to the beginning of the
				     * string, to avoid going backwards too
				     * far. */
{
    CONST char *look;
    int i, byte;
    
    str--;
    look = str;
    for (i = 0; i < TCL_UTF_MAX; i++) {
	if (look < start) {
	    if (str < start) {
		str = start;
	    }
	    break;
	}
	byte = *((unsigned char *) look);
	if (byte < 0x80) {
	    break;
	} 
	if (byte >= 0xC0) {
	    if (totalBytes[byte] != i + 1) {
		break;
	    }
	    return (char *) look;
	}
	look--;
    }
    return (char *) str;
}
	
/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UniCharAtIndex --
 *
 *	Returns the Unicode character represented at the specified
 *	character (not byte) position in the UTF-8 string.
 *
 * Results:
 *	As above.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
 
Tcl_UniChar
Tcl_UniCharAtIndex(src, index)
    register CONST char *src;	/* The UTF-8 string to dereference. */
    register int index;		/* The position of the desired character. */
{
    Tcl_UniChar ch;

    while (index >= 0) {
	index--;
	src += Tcl_UtfToUniChar(src, &ch);
    }
    return ch;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfAtIndex --
 *
 *	Returns a pointer to the specified character (not byte) position
 *	in the UTF-8 string.
 *
 * Results:
 *	As above.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

char *
Tcl_UtfAtIndex(src, index)
    register CONST char *src;	/* The UTF-8 string. */
    register int index;		/* The position of the desired character. */
{
    Tcl_UniChar ch;
    
    while (index > 0) {
	index--;
	src += Tcl_UtfToUniChar(src, &ch);
    }
    return (char *) src;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tcl_UtfBackslash --
 *
 *	Figure out how to handle a backslash sequence.
 *
 * Results:
 *	Stores the bytes represented by the backslash sequence in dst and
 *	returns the number of bytes written to dst.  At most TCL_UTF_MAX
 *	bytes are written to dst; dst must have been large enough to accept
 *	those bytes.  If readPtr isn't NULL then it is filled in with a
 *	count of the number of bytes in the backslash sequence.  
 *
 * Side effects:
 *	The maximum number of bytes it takes to represent a Unicode
 *	character in UTF-8 is guaranteed to be less than the number of
 *	bytes used to express the backslash sequence that represents
 *	that Unicode character.  If the target buffer into which the
 *	caller is going to store the bytes that represent the Unicode
 *	character is at least as large as the source buffer from which
 *	the backslashed sequence was extracted, no buffer overruns should
 *	occur.
 *
 *---------------------------------------------------------------------------
 */

int
Tcl_UtfBackslash(src, readPtr, dst)
    CONST char *src;		/* Points to the backslash character of
				 * a backslash sequence. */
    int *readPtr;		/* Fill in with number of characters read
				 * from src, unless NULL. */
    char *dst;			/* Filled with the bytes represented by the
				 * backslash sequence. */
{
    register CONST char *p = src+1;
    int result, count, n;
    char buf[TCL_UTF_MAX];

    if (dst == NULL) {
	dst = buf;
    }

    count = 2;
    switch (*p) {
	/*
         * Note: in the conversions below, use absolute values (e.g.,
         * 0xa) rather than symbolic values (e.g. \n) that get converted
         * by the compiler.  It's possible that compilers on some
         * platforms will do the symbolic conversions differently, which
         * could result in non-portable Tcl scripts.
         */

        case 'a':
            result = 0x7;
            break;
        case 'b':
            result = 0x8;
            break;
        case 'f':
            result = 0xc;
            break;
        case 'n':
            result = 0xa;
            break;
        case 'r':
            result = 0xd;
            break;
        case 't':
            result = 0x9;
            break;
        case 'v':
            result = 0xb;
            break;
        case 'x':
            if (isxdigit(UCHAR(p[1]))) { /* INTL: digit */
                char *end;

                result = (unsigned char) strtoul(p+1, &end, 16);
                count = end - src;
            } else {
                count = 2;
                result = 'x';
            }
            break;
	case 'u':
	    result = 0;
	    for (count = 0; count < 4; count++) {
		p++;
		if (!isxdigit(UCHAR(*p))) { /* INTL: digit */
		    break;
		}
		n = *p - '0';
		if (n > 9) {
		    n = n + '0' + 10 - 'A';
		}
		if (n > 16) {
		    n = n + 'A' - 'a';
		}
		result = (result << 4) + n;
	    }
	    if (count == 0) {
		result = 'u';
	    }
	    count += 2;
	    break;
		    
        case '\n':
            do {
                p++;
            } while ((*p == ' ') || (*p == '\t'));
            result = ' ';
            count = p - src;
            break;
        case 0:
            result = '\\';
            count = 1;
            break;
	default:
	    if (isdigit(UCHAR(*p))) { /* INTL: digit */
		result = (unsigned char)(*p - '0');
		p++;
		if (!isdigit(UCHAR(*p))) { /* INTL: digit */
		    break;
		}
		count = 3;
		result = (unsigned char)((result << 3) + (*p - '0'));
		p++;
		if (!isdigit(UCHAR(*p))) { /* INTL: digit */
		    break;
		}
		count = 4;
		result = (unsigned char)((result << 3) + (*p - '0'));
		break;
	    }
	    result = *p;
	    count = 2;
	    break;
    }

    if (readPtr != NULL) {
	*readPtr = count;
    }
    return Tcl_UniCharToUtf(result, dst);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToUpper --
 *
 *	Convert lowercase characters to uppercase characters in a UTF
 *	string in place.  The conversion may shrink the UTF string.
 *
 *	INTL: This implementation only handles iso8859-1 characters
 *	in the current locale.  This should be changed to use the
 *	Unicode character tables.
 *
 * Results:
 *	Returns the number of bytes in the resulting string
 *	excluding the trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToUpper(str)
    char *str;			/* String to convert in place. */
{
    Tcl_UniChar ch;
    char *src, *dst;
    
    /*
     * Iterate over the string until we hit the terminating null.
     */

    src = dst = str;
    while (*src) {
	src += Tcl_UtfToUniChar(src, &ch);

	/*
	 * INTL: This conversion should be replaced with a table lookup for the
	 * full Unicode translation.
	 */

	if ((ch < 0x100) && islower(ch)) { /* INTL: ISO only */
	    ch = (Tcl_UniChar) UCHAR(toupper(ch)); /* INTL: ISO only */
	}
	dst += Tcl_UniCharToUtf(ch, dst);
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToLower --
 *
 *	Convert uppercase characters to lowercase characters in a UTF
 *	string in place.  The conversion may shrink the UTF string.
 *
 *	INTL: This implementation only handles iso8859-1 characters
 *	in the current locale.  This should be changed to use the
 *	Unicode character tables.
 *
 * Results:
 *	Returns the number of bytes in the resulting string
 *	excluding the trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToLower(str)
    char *str;			/* String to convert in place. */
{
    Tcl_UniChar ch;
    char *src, *dst;
    
    /*
     * Iterate over the string until we hit the terminating null.
     */

    src = dst = str;
    while (*src) {
	src += Tcl_UtfToUniChar(src, &ch);

	/*
	 * INTL: This conversion should be replaced with a table lookup for the
	 * full Unicode translation.
	 */

	if ((ch < 0x100) && isupper(ch)) { /* INTL: ISO only */
	    ch = (Tcl_UniChar) UCHAR(tolower(ch)); /* INTL: ISO only */
	}
	dst += Tcl_UniCharToUtf(ch, dst);
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UtfToTitle --
 *
 *	Changes the first character of a UTF string to title case or
 *	uppercase and the rest of the string to lowercase.  The
 *	conversion happens in place and may shrink the UTF string.
 *
 *	INTL: This implementation only handles iso8859-1 characters
 *	in the current locale.  This should be changed to use the
 *	Unicode character tables.
 *
 * Results:
 *	Returns the number of bytes in the resulting string
 *	excluding the trailing null.
 *
 * Side effects:
 *	Writes a terminating null after the last converted character.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_UtfToTitle(str)
    char *str;			/* String to convert in place. */
{
    Tcl_UniChar ch;
    char *src, *dst;
    
    /*
     * Capitalize the first character and then lowercase the rest of the
     * characters until we get to a null.
     */

    src = dst = str;

    if (*src) {
	src += Tcl_UtfToUniChar(src, &ch);
	if ((ch < 0x100) && islower(ch)) { /* INTL: ISO only */
	    ch = (Tcl_UniChar) UCHAR(toupper(ch)); /* INTL: ISO only */
	}
	dst += Tcl_UniCharToUtf(ch, dst);
    }
    while (*src) {
	src += Tcl_UtfToUniChar(src, &ch);
	if ((ch < 0x100) && isupper(ch)) { /* INTL: ISO only */
	    ch = (Tcl_UniChar) UCHAR(tolower(ch)); /* INTL: ISO only */
	}
	dst += Tcl_UniCharToUtf(ch, dst);
    }
    *dst = '\0';
    return (dst - str);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToUpper --
 *
 *	Compute the uppercase equivalent of the given Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns the uppercase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar
Tcl_UniCharToUpper(ch)
    int ch;			/* Unicode character to convert. */
{
    return (Tcl_UniChar) ((ch < 0x100)
	    ? UCHAR(toupper(ch))	/* INTL: ISO only */
	    : ch);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToLower --
 *
 *	Compute the lowercase equivalent of the given Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns the lowercase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar
Tcl_UniCharToLower(ch)
    int ch;			/* Unicode character to convert. */
{
    return (Tcl_UniChar) ((ch < 0x100)
	    ? UCHAR(tolower(ch))	/* INTL: ISO only */
	    : ch);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_UniCharToTitle --
 *
 *	Compute the titlecase equivalent of the given Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns the titlecase Unicode character.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_UniChar
Tcl_UniCharToTitle(ch)
    int ch;			/* Unicode character to convert. */
{
    return (Tcl_UniChar) ((ch < 0x100)
	    ? UCHAR(toupper(ch))	/* INTL: ISO only */
	    : ch);
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharLen --
 *
 *	Find the length of a UniChar string.  The str input must be null
 *	terminated.
 *
 * Results:
 *	Returns the length of str in UniChars (not bytes).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharLen(str)
    Tcl_UniChar *str;		/* Unicode string to find length of. */
{
    int len = 0;
    
    while (*str != '\0') {
	len++;
	str++;
    }
    return len;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharNcmp --
 *
 *	Compare at most n unichars of string cs to string ct.  Both cs
 *	and ct are assumed to be at least n unichars long.
 *
 * Results:
 *	Return <0 if cs < ct, 0 if cs == ct, or >0 if cs > ct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharNcmp(cs, ct, n)
    CONST Tcl_UniChar *cs;		/* Unicode string to compare to ct. */
    CONST Tcl_UniChar *ct;		/* Unicode string cs is compared to. */
    size_t n;				/* Number of unichars to compare. */
{
    for ( ; n != 0; n--, cs++, ct++) {
	if (*cs != *ct) {
	    return *cs - *ct;
	}
	if (*cs == '\0') {
	    break;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsAlnum --
 *
 *	Test if a character is an alphanumeric Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is alphanumeric.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsAlnum(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? isalnum(ch) : 0); /* INTL: ISO only */
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsAlpha --
 *
 *	Test if a character is an alphabetic Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is alphabetic.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsAlpha(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? isalpha(ch) : 0); /* INTL: ISO only */
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsDigit --
 *
 *	Test if a character is a numeric Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is a digit.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsDigit(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? isdigit(ch) : 0); /* INTL: ISO only */
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsLower --
 *
 *	Test if a character is a lowercase Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is lowercase.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsLower(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? islower(ch) : 0); /* INTL: ISO only */
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsSpace --
 *
 *	Test if a character is a whitespace Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is a space.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsSpace(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? isspace(ch) : 0); /* INTL: ISO only */
}

/*
 *----------------------------------------------------------------------
 *
 * TclUniCharIsUpper --
 *
 *	Test if a character is a uppercase Unicode character.
 *
 *	INTL: this implementation only works on ISO characters.
 *
 * Results:
 *	Returns 1 if character is uppercase.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclUniCharIsUpper(ch)
    int ch;			/* Unicode character to test. */
{
    return ((ch < 0x100) ? isupper(ch) : 0); /* INTL: ISO only */
}
