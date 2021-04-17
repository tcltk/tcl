/*
 * tclParse.h --
 *
 *	This file contains the minimal set of shared macro definitions and
 *	declarations so that multiple source files can make use of the parsing
 *	table in tclParse.c.
 *
 * Copyright (c) 2012 by Don Porter
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

enum ParseTypes {
    TYPE_NORMAL = 0,
    TYPE_SPACE = 0x1,
    TYPE_COMMAND_END = 0x2,
    TYPE_SUBS = 0x4,
    TYPE_QUOTE = 0x8,
    TYPE_CLOSE_PAREN = 0x10,
    TYPE_CLOSE_BRACK = 0x20,
    TYPE_BRACE = 0x40
};

#define CHAR_TYPE(c)	tclCharTypeTable[(unsigned char)(c)]

MODULE_SCOPE const char tclCharTypeTable[];

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
