/*
 * Minimal set of shared macro definitions and declarations so that multiple
 * source files can make use of the parsing table in tclParse.c
 */

#define TYPE_NORMAL		0
#define TYPE_SPACE		0x1
#define TYPE_COMMAND_END	0x2
#define TYPE_SUBS		0x4
#define TYPE_QUOTE		0x8
#define TYPE_CLOSE_PAREN	0x10
#define TYPE_CLOSE_BRACK	0x20
#define TYPE_BRACE		0x40
#define TYPE_OPEN_PAREN		0x80
#define TYPE_BAD_ARRAY_INDEX	(TYPE_OPEN_PAREN|TYPE_CLOSE_PAREN|TYPE_QUOTE|TYPE_BRACE)

#define CHAR_TYPE(c) tclCharTypeTable[(unsigned char)(c)]

MODULE_SCOPE const unsigned char tclCharTypeTable[];
