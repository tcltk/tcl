/*
 * tclStringTrim.h --
 *
 *	This file contains the definition of what characters are to be trimmed
 *	from a string by [string trim] by default. It's only needed by Tcl's
 *	implementation; it does not form a public or private API at all.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 Scriptics Corporation.
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2003-2013 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TCL_STRING_TRIM_H
#define TCL_STRING_TRIM_H

/*
 * Default set of characters to trim in [string trim] and friends. This is a
 * UTF-8 literal string containing all Unicode space characters. [TIP #413]
 */

#define DEFAULT_TRIM_SET \
	"\x09\x0a\x0b\x0c\x0d " /* ASCII */\
	"\xc0\x80" /*     nul (U+0000) */\
	"\xc2\x85" /*     next line (U+0085) */\
	"\xc2\xa0" /*     non-breaking space (U+00a0) */\
	"\xe1\x9a\x80" /* ogham space mark (U+1680) */ \
	"\xe1\xa0\x8e" /* mongolian vowel separator (U+180e) */\
	"\xe2\x80\x80" /* en quad (U+2000) */\
	"\xe2\x80\x81" /* em quad (U+2001) */\
	"\xe2\x80\x82" /* en space (U+2002) */\
	"\xe2\x80\x83" /* em space (U+2003) */\
	"\xe2\x80\x84" /* three-per-em space (U+2004) */\
	"\xe2\x80\x85" /* four-per-em space (U+2005) */\
	"\xe2\x80\x86" /* six-per-em space (U+2006) */\
	"\xe2\x80\x87" /* figure space (U+2007) */\
	"\xe2\x80\x88" /* punctuation space (U+2008) */\
	"\xe2\x80\x89" /* thin space (U+2009) */\
	"\xe2\x80\x8a" /* hair space (U+200a) */\
	"\xe2\x80\x8b" /* zero width space (U+200b) */\
	"\xe2\x80\xa8" /* line separator (U+2028) */\
	"\xe2\x80\xa9" /* paragraph separator (U+2029) */\
	"\xe2\x80\xaf" /* narrow no-break space (U+202f) */\
	"\xe2\x81\x9f" /* medium mathematical space (U+205f) */\
	"\xe2\x81\xa0" /* word joiner (U+2060) */\
	"\xe3\x80\x80" /* ideographic space (U+3000) */\
	"\xef\xbb\xbf" /* zero width no-break space (U+feff) */

/*
 * The whitespace trimming set used when [concat]enating. This is a subset of
 * the above, and deliberately so.
 */

#define CONCAT_TRIM_SET " \f\v\r\t\n"

#endif /* TCL_STRING_TRIM_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
