/*
 * You may distribute and/or modify this program under the terms of the GNU
 * Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.

 * See the file "COPYING" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
*/

/*
 * Minimal set of shared flag definitions and declarations so that multiple
 * source files can make use of the parsing table in tclParse.c
 */

enum ParseTypeFlags {
    TYPE_NORMAL = 0,
    TYPE_SPACE = 0x1,
    TYPE_COMMAND_END = 0x2,
    TYPE_SUBS = 0x4,
    TYPE_QUOTE = 0x8,
    TYPE_CLOSE_PAREN = 0x10,
    TYPE_CLOSE_BRACK = 0x20,
    TYPE_BRACE = 0x40,
    TYPE_OPEN_PAREN = 0x80,
    TYPE_BAD_ARRAY_INDEX = (
	TYPE_OPEN_PAREN | TYPE_CLOSE_PAREN | TYPE_QUOTE | TYPE_BRACE)
};

#define CHAR_TYPE(c) tclCharTypeTable[(unsigned char)(c)]

MODULE_SCOPE const unsigned char tclCharTypeTable[];
