/*
 * locale-specific stuff, including MCCE handling
 * This file is #included by regcomp.c.
 *
 * No MCCEs for Tcl.  The handling of character names and classes is
 * still ASCII-centric, and needs to be extended to handle full Unicode.
 */

/* ASCII character-name table */

static struct cname {
	char *name;
	char code;
} cnames[] = {
	{"NUL",	'\0'},
	{"SOH",	'\001'},
	{"STX",	'\002'},
	{"ETX",	'\003'},
	{"EOT",	'\004'},
	{"ENQ",	'\005'},
	{"ACK",	'\006'},
	{"BEL",	'\007'},
	{"alert",	'\007'},
	{"BS",		'\010'},
	{"backspace",	'\b'},
	{"HT",		'\011'},
	{"tab",		'\t'},
	{"LF",		'\012'},
	{"newline",	'\n'},
	{"VT",		'\013'},
	{"vertical-tab",	'\v'},
	{"FF",		'\014'},
	{"form-feed",	'\f'},
	{"CR",		'\015'},
	{"carriage-return",	'\r'},
	{"SO",	'\016'},
	{"SI",	'\017'},
	{"DLE",	'\020'},
	{"DC1",	'\021'},
	{"DC2",	'\022'},
	{"DC3",	'\023'},
	{"DC4",	'\024'},
	{"NAK",	'\025'},
	{"SYN",	'\026'},
	{"ETB",	'\027'},
	{"CAN",	'\030'},
	{"EM",	'\031'},
	{"SUB",	'\032'},
	{"ESC",	'\033'},
	{"IS4",	'\034'},
	{"FS",	'\034'},
	{"IS3",	'\035'},
	{"GS",	'\035'},
	{"IS2",	'\036'},
	{"RS",	'\036'},
	{"IS1",	'\037'},
	{"US",	'\037'},
	{"space",		' '},
	{"exclamation-mark",	'!'},
	{"quotation-mark",	'"'},
	{"number-sign",		'#'},
	{"dollar-sign",		'$'},
	{"percent-sign",		'%'},
	{"ampersand",		'&'},
	{"apostrophe",		'\''},
	{"left-parenthesis",	'('},
	{"right-parenthesis",	')'},
	{"asterisk",	'*'},
	{"plus-sign",	'+'},
	{"comma",	','},
	{"hyphen",	'-'},
	{"hyphen-minus",	'-'},
	{"period",	'.'},
	{"full-stop",	'.'},
	{"slash",	'/'},
	{"solidus",	'/'},
	{"zero",		'0'},
	{"one",		'1'},
	{"two",		'2'},
	{"three",	'3'},
	{"four",		'4'},
	{"five",		'5'},
	{"six",		'6'},
	{"seven",	'7'},
	{"eight",	'8'},
	{"nine",		'9'},
	{"colon",	':'},
	{"semicolon",	';'},
	{"less-than-sign",	'<'},
	{"equals-sign",		'='},
	{"greater-than-sign",	'>'},
	{"question-mark",	'?'},
	{"commercial-at",	'@'},
	{"left-square-bracket",	'['},
	{"backslash",		'\\'},
	{"reverse-solidus",	'\\'},
	{"right-square-bracket",	']'},
	{"circumflex",		'^'},
	{"circumflex-accent",	'^'},
	{"underscore",		'_'},
	{"low-line",		'_'},
	{"grave-accent",		'`'},
	{"left-brace",		'{'},
	{"left-curly-bracket",	'{'},
	{"vertical-line",	'|'},
	{"right-brace",		'}'},
	{"right-curly-bracket",	'}'},
	{"tilde",		'~'},
	{"DEL",	'\177'},
	{NULL,	0}
};

/* Unicode character-class tables */

typedef struct crange {
    chr start;
    chr end;
} crange;

static crange alphaTable[] = {
    {0X0041, 0X005A}, {0X0061, 0X007A}, {0X00AA, 0X00AA}, {0X00B5, 0X00B5}, 
    {0X00BA, 0X00BA}, {0X00C0, 0X00D6}, {0X00D8, 0X00F6}, {0X00F8, 0X01F5},
    {0X01FA, 0X0217}, {0X0250, 0X02A8}, {0X02B0, 0X02B8}, {0X02BB, 0X02C1},
    {0X02E0, 0X02E4}, {0X037A, 0X037A}, {0x0386, 0x0386}, {0X0388, 0X038A},
    {0X038C, 0X038C}, {0X038E, 0X03A1}, {0X03A3, 0X03CE}, {0X03D0, 0X03D6},
    {0X03DA, 0X03DA}, {0X03DC, 0X03DC}, {0X03DE, 0X03DE}, {0X03E0, 0X03E0}, 
    {0X03E2, 0X03F3}, {0X0401, 0X040C}, {0X040E, 0X044F}, {0X0451, 0X045C},
    {0X045E, 0X0481}, {0X0490, 0X04C4}, {0X04C7, 0X04C8}, {0X04CB, 0X04CC},
    {0X04D0, 0X04EB}, {0X04EE, 0X04F5}, {0X04F8, 0X04F9}, {0x0531, 0x0556},
    {0x0559, 0x0559}, {0x0561, 0x0587}, {0X05D0, 0X05EA}, {0X05F0, 0X05F2},
    {0X0621, 0X063A}, {0x0641, 0x0652}, {0X0670, 0X06B7}, {0X06BA, 0X06BE},
    {0X06C0, 0X06CE}, {0X06D0, 0X06D3}, {0X06D5, 0X06DC}, {0X06E1, 0X06E8},
    {0X06ED, 0X06ED}, {0x0901, 0x0903}, {0x0905, 0x0939}, {0X093D, 0X094C},
    {0x0958, 0x0963}, {0x0981, 0x0983}, {0X0985, 0X098C}, {0X098F, 0X0990},
    {0X0993, 0X09A8}, {0X09AA, 0X09B0}, {0X09B2, 0X09B2}, {0X09B6, 0X09B9},
    {0X09BE, 0X09C4}, {0X09C7, 0X09C8}, {0X09CB, 0X09CC}, {0X09D7, 0X09D7}, 
    {0X09DC, 0X09DD}, {0X09DF, 0X09E3}, {0X09F0, 0X09F1}, {0X0A02, 0X0A02}, 
    {0X0A05, 0X0A0A}, {0X0A0F, 0X0A10}, {0X0A13, 0X0A28}, {0X0A2A, 0X0A30},
    {0X0A32, 0X0A33}, {0X0A35, 0X0A36}, {0X0A38, 0X0A39}, {0X0A3E, 0X0A42},
    {0X0A47, 0X0A48}, {0X0A4B, 0X0A4C}, {0X0A59, 0X0A5C}, {0X0A5E, 0X0A5E}, 
    {0X0A70, 0X0A74}, {0X0A81, 0X0A83}, {0X0A85, 0X0A8B}, {0X0A8D, 0X0A8D}, 
    {0X0A8F, 0X0A91}, {0X0A93, 0X0AA8}, {0X0AAA, 0X0AB0}, {0X0AB2, 0X0AB3},
    {0X0AB5, 0X0AB9}, {0X0ABD, 0X0AC5}, {0X0AC7, 0X0AC9}, {0X0ACB, 0X0ACC},
    {0X0AE0, 0X0AE0}, {0X0B01, 0X0B03}, {0X0B05, 0X0B0C}, {0X0B0F, 0X0B10},
    {0X0B13, 0X0B28}, {0X0B2A, 0X0B30}, {0X0B32, 0X0B33}, {0X0B36, 0X0B39},
    {0X0B3D, 0X0B43}, {0X0B47, 0X0B48}, {0X0B4B, 0X0B4C}, {0X0B56, 0X0B57},
    {0X0B5C, 0X0B5D}, {0X0B5F, 0X0B61}, {0X0B82, 0X0B83}, {0X0B85, 0X0B8A},
    {0X0B8E, 0X0B90}, {0X0B92, 0X0B95}, {0X0B99, 0X0B9A}, {0X0B9C, 0X0B9C}, 
    {0X0B9E, 0X0B9F}, {0X0BA3, 0X0BA4}, {0X0BA8, 0X0BAA}, {0X0BAE, 0X0BB5},
    {0X0BB7, 0X0BB9}, {0X0BBE, 0X0BC2}, {0X0BC6, 0X0BC8}, {0X0BCA, 0X0BCC},
    {0X0BD7, 0X0BD7}, {0X0C01, 0X0C03}, {0X0C05, 0X0C0C}, {0X0C0E, 0X0C10},
    {0X0C12, 0X0C28}, {0X0C2A, 0X0C33}, {0X0C35, 0X0C39}, {0X0C3E, 0X0C44},
    {0X0C46, 0X0C48}, {0X0C4A, 0X0C4C}, {0X0C55, 0X0C56}, {0X0C60, 0X0C61},
    {0X0C82, 0X0C83}, {0X0C85, 0X0C8C}, {0X0C8E, 0X0C90}, {0X0C92, 0X0CA8},
    {0X0CAA, 0X0CB3}, {0X0CB5, 0X0CB9}, {0X0CBE, 0X0CC4}, {0X0CC6, 0X0CC8},
    {0X0CCA, 0X0CCC}, {0X0CD5, 0X0CD6}, {0X0CDE, 0X0CDE}, {0X0CE0, 0X0CE1},
    {0X0D02, 0X0D03}, {0X0D05, 0X0D0C}, {0X0D0E, 0X0D10}, {0X0D12, 0X0D28},
    {0X0D2A, 0X0D39}, {0X0D3E, 0X0D43}, {0X0D46, 0X0D48}, {0X0D4A, 0X0D4C},
    {0X0D57, 0X0D57}, {0X0D60, 0X0D61}, {0X0E01, 0X0E2E}, {0X0E30, 0X0E3A},
    {0X0E40, 0X0E45}, {0X0E47, 0X0E47}, {0X0E4D, 0X0E4D}, {0X0E81, 0X0E82},
    {0X0E84, 0X0E84}, {0X0E87, 0X0E88}, {0X0E8A, 0X0E8A}, {0X0E8D, 0X0E8D}, 
    {0X0E94, 0X0E97}, {0X0E99, 0X0E9F}, {0X0EA1, 0X0EA3}, {0X0EA5, 0X0EA5}, 
    {0X0EA7, 0X0EA7}, {0X0EAA, 0X0EAB}, {0X0EAD, 0X0EAE}, {0X0EB0, 0X0EB9},
    {0X0EBB, 0X0EBD}, {0X0EC0, 0X0EC4}, {0X0ECD, 0X0ECD}, {0X0EDC, 0X0EDD},
    {0X0F40, 0X0F47}, {0X0F49, 0X0F69}, {0X0F71, 0X0F81}, {0X0F90, 0X0F95},
    {0X0F97, 0X0F97}, {0X0F99, 0X0FAD}, {0X0FB1, 0X0FB7}, {0X0FB9, 0X0FB9}, 
    {0X10A0, 0X10C5}, {0X10D0, 0X10F6}, {0x1100, 0x1159}, {0X115F, 0X11A2},
    {0X11A8, 0X11F9}, {0X1E00, 0X1E9B}, {0X1EA0, 0X1EF9}, {0X1F00, 0X1F15},
    {0X1F18, 0X1F1D}, {0X1F20, 0X1F45}, {0X1F48, 0X1F4D}, {0X1F50, 0X1F57},
    {0X1F59, 0X1F59}, {0X1F5B, 0X1F5B}, {0X1F5D, 0X1F5D}, {0X1F5F, 0X1F7D},
    {0X1F80, 0X1FB4}, {0X1FB6, 0X1FBC}, {0X1FBE, 0X1FBE}, {0X1FC2, 0X1FC4},
    {0X1FC6, 0X1FCC}, {0X1FD0, 0X1FD3}, {0X1FD6, 0X1FDB}, {0X1FE0, 0X1FEC},
    {0X1FF2, 0X1FF4}, {0X1FF6, 0X1FFC}, {0X207F, 0X207F}, {0x2102, 0x2102}, 
    {0x2107, 0x2107}, {0X210A, 0X2113}, {0x2115, 0x2115}, {0X2118, 0X211D},
    {0x2124, 0x2124}, {0x2126, 0x2126}, {0x2128, 0x2128}, {0X212A, 0X2131},
    {0x2133, 0x2138}, {0x2160, 0x2182}, {0x3041, 0x3094}, {0X30A1, 0X30FA},
    {0X3105, 0X312C}, {0X3131, 0X318E}, {0XAC00, 0XD7A3}, {0XFB00, 0XFB06},
    {0XFB13, 0XFB17}, {0XFB1F, 0XFB28}, {0XFB2A, 0XFB36}, {0XFB38, 0XFB3C},
    {0XFB3E, 0XFB3E}, {0XFB40, 0XFB41}, {0XFB43, 0XFB44}, {0XFB46, 0XFBB1},
    {0XFBD3, 0XFD3D}, {0XFD50, 0XFD8F}, {0XFD92, 0XFDC7}, {0XFDF0, 0XFDFB},
    {0XFE70, 0XFE72}, {0XFE74, 0XFE74}, {0XFE76, 0XFEFC}, {0XFF21, 0XFF3A},
    {0XFF41, 0XFF5A}, {0XFF66, 0XFF6F}, {0XFF71, 0XFF9D}, {0XFFA0, 0XFFBE},
    {0XFFC2, 0XFFC7}, {0XFFCA, 0XFFCF}, {0XFFD2, 0XFFD7}, {0XFFDA, 0XFFDC}
};

#define NUM_ALPHA (sizeof(alphaTable)/sizeof(crange))

static crange digitTable[] = {
    {0x0030, 0x0039}
};

#define NUM_DIGIT (sizeof(digitTable)/sizeof(crange))

static crange punctTable[] = {
    {0x0021, 0x0023}, {0X0025, 0X002A}, {0X002C, 0X002F}, {0X003A, 0X003B},
    {0X003F, 0X0040}, {0X005B, 0X005D}, {0X005F, 0X005F}, {0X007B, 0X007B},
    {0X007D, 0X007D}, {0X00A1, 0X00A1}, {0X00AB, 0X00AB}, {0X00AD, 0X00AD},
    {0X00BB, 0X00BB}, {0X00BF, 0X00BF}, {0X02BC, 0X02BC}, {0x0374, 0x0375},
    {0X037E, 0X037E}, {0x0387, 0x0387}, {0X055A, 0X055F}, {0x0589, 0x0589},
    {0X05BE, 0X05BE}, {0X05C0, 0X05C0}, {0X05C3, 0X05C3}, {0X05F3, 0X05F4},
    {0X060C, 0X060C}, {0X061B, 0X061B}, {0X061F, 0X061F}, {0X066A, 0X066D},
    {0X06D4, 0X06D4}, {0x0964, 0x0965}, {0x0970, 0x0970}, {0X0E2F, 0X0E2F},
    {0X0E5A, 0X0E5B}, {0X0EAF, 0X0EAF}, {0X0F04, 0X0F12}, {0X0F3A, 0X0F3F},
    {0X0F85, 0X0F85}, {0X10FB, 0X10FB}, {0x2010, 0x2027}, {0x2030, 0x2043},
    {0x2045, 0x2046}, {0X207D, 0X207E}, {0X208D, 0X208E}, {0X2329, 0X232A},
    {0x3001, 0x3003}, {0x3006, 0x3006}, {0x3008, 0x3011}, {0X3014, 0X301F},
    {0x3030, 0x3030}, {0X30FB, 0X30FB}, {0XFD3E, 0XFD3F}, {0XFE30, 0XFE44},
    {0XFE49, 0XFE52}, {0XFE54, 0XFE61}, {0XFE63, 0XFE63}, {0XFE68, 0XFE68},
    {0XFE6A, 0XFE6B}, {0XFF01, 0XFF03}, {0XFF05, 0XFF0A}, {0XFF0C, 0XFF0F},
    {0XFF1A, 0XFF1B}, {0XFF1F, 0XFF20}, {0XFF3B, 0XFF3D}, {0XFF3F, 0XFF3F},
    {0XFF5B, 0XFF5B}, {0XFF5D, 0XFF5D}, {0XFF61, 0XFF65}
};

#define NUM_PUNCT (sizeof(punctTable)/sizeof(crange))

static crange spaceTable[] = {
    {0x0000, 0x0000}, {0x0009, 0x000D}, {0x0020, 0x0020}, {0x00A0, 0x00A0},
    {0x2000, 0x200F}, {0x2028, 0x202E}, {0X206A, 0X206F}, {0x3000, 0x3000},
    {0xFEFF, 0xFEFF}
};  

#define NUM_SPACE (sizeof(spaceTable)/sizeof(crange))

static crange upperRangeTable[] = {
    {0x0041, 0x005a}, {0x00c0, 0x00d6}, {0x00d8, 0x00de}, {0x0189, 0x018b},
    {0x018e, 0x0191}, {0x0388, 0x038a}, {0x0391, 0x03a1}, {0x03a3, 0x03ab},
    {0x03d2, 0x03d4}, {0x0401, 0x040c}, {0x040e, 0x042f}, {0x0531, 0x0556},
    {0x10a0, 0x10c5}, {0x1f08, 0x1f0f}, {0x1f18, 0x1f1d}, {0x1f28, 0x1f2f},
    {0x1f38, 0x1f3f}, {0x1f48, 0x1f4d}, {0x1f68, 0x1f6f}, {0x1f88, 0x1f8f},
    {0x1f98, 0x1f9f}, {0x1fa8, 0x1faf}, {0x1fb8, 0x1fbc}, {0x1fc8, 0x1fcc},
    {0x1fd8, 0x1fdb}, {0x1fe8, 0x1fec}, {0x1ff8, 0x1ffc}, {0x210b, 0x210d},
    {0x2110, 0x2112}, {0x2118, 0x211d}, {0x212a, 0x212d}, {0x2130, 0x2131},
    {0xff21, 0xff3a}
};

#define NUM_UPPER_RANGE (sizeof(upperRangeTable)/sizeof(crange))

static chr upperCharTable[] = {
    0x0100, 0x0102, 0x0104, 0x0106, 0x0108, 0x010a, 0x010c, 0x010e, 0x0110,
    0x0112, 0x0114, 0x0116, 0x0118, 0x011a, 0x011c, 0x011e, 0x0120, 0x0122,
    0x0124, 0x0126, 0x0128, 0x012a, 0x012c, 0x012e, 0x0130, 0x0132, 0x0134,
    0x0136, 0x0139, 0x013b, 0x013d, 0x013f, 0x0141, 0x0143, 0x0145, 0x0147,
    0x014a, 0x014c, 0x014e, 0x0150, 0x0152, 0x0154, 0x0156, 0x0158, 0x015a,
    0x015c, 0x015e, 0x0160, 0x0162, 0x0164, 0x0166, 0x0168, 0x016a, 0x016c,
    0x016e, 0x0170, 0x0172, 0x0174, 0x0176, 0x0178, 0x0179, 0x017b, 0x017d,
    0x0181, 0x0182, 0x0184, 0x0186, 0x0187, 0x0193, 0x0194, 0x0196, 0x0197,
    0x0198, 0x019c, 0x019d, 0x019f, 0x01a0, 0x01a2, 0x01a4, 0x01a6, 0x01a7,
    0x01a9, 0x01ac, 0x01ae, 0x01af, 0x01b1, 0x01b2, 0x01b3, 0x01b5, 0x01b7,
    0x01b8, 0x01bc, 0x01c4, 0x01c7, 0x01ca, 0x01cd, 0x01cf, 0x01d1, 0x01d3,
    0x01d5, 0x01d7, 0x01d9, 0x01db, 0x01de, 0x01e0, 0x01e2, 0x01e4, 0x01e6,
    0x01e8, 0x01ea, 0x01ec, 0x01ee, 0x01f1, 0x01f4, 0x01fa, 0x01fc, 0x01fe,
    0x0200, 0x0202, 0x0204, 0x0206, 0x0208, 0x020a, 0x020c, 0x020e, 0x0210,
    0x0212, 0x0214, 0x0216, 0x0386, 0x038c, 0x038e, 0x038f, 0x03da, 0x03dc,
    0x03de, 0x03e0, 0x03e2, 0x03e4, 0x03e6, 0x03e8, 0x03ea, 0x03ec, 0x03ee,
    0x0460, 0x0462, 0x0464, 0x0466, 0x0468, 0x046a, 0x046c, 0x046e, 0x0470,
    0x0472, 0x0474, 0x0476, 0x0478, 0x047a, 0x047c, 0x047e, 0x0480, 0x0490,
    0x0492, 0x0494, 0x0496, 0x0498, 0x049a, 0x049c, 0x049e, 0x04a0, 0x04a2,
    0x04a4, 0x04a6, 0x04a8, 0x04aa, 0x04ac, 0x04ae, 0x04b0, 0x04b2, 0x04b4,
    0x04b6, 0x04b8, 0x04ba, 0x04bc, 0x04be, 0x04c1, 0x04c3, 0x04c7, 0x04cb,
    0x04d0, 0x04d2, 0x04d4, 0x04d6, 0x04d8, 0x04da, 0x04dc, 0x04de, 0x04e0,
    0x04e2, 0x04e4, 0x04e6, 0x04e8, 0x04ea, 0x04ee, 0x04f0, 0x04f2, 0x04f4,
    0x04f8, 0x1e00, 0x1e02, 0x1e04, 0x1e06, 0x1e08, 0x1e0a, 0x1e0c, 0x1e0e,
    0x1e10, 0x1e12, 0x1e14, 0x1e16, 0x1e18, 0x1e1a, 0x1e1c, 0x1e1e, 0x1e20,
    0x1e22, 0x1e24, 0x1e26, 0x1e28, 0x1e2a, 0x1e2c, 0x1e2e, 0x1e30, 0x1e32,
    0x1e34, 0x1e36, 0x1e38, 0x1e3a, 0x1e3c, 0x1e3e, 0x1e40, 0x1e42, 0x1e44,
    0x1e46, 0x1e48, 0x1e4a, 0x1e4c, 0x1e4e, 0x1e50, 0x1e52, 0x1e54, 0x1e56,
    0x1e58, 0x1e5a, 0x1e5c, 0x1e5e, 0x1e60, 0x1e62, 0x1e64, 0x1e66, 0x1e68,
    0x1e6a, 0x1e6c, 0x1e6e, 0x1e70, 0x1e72, 0x1e74, 0x1e76, 0x1e78, 0x1e7a,
    0x1e7c, 0x1e7e, 0x1e80, 0x1e82, 0x1e84, 0x1e86, 0x1e88, 0x1e8a, 0x1e8c,
    0x1e8e, 0x1e90, 0x1e92, 0x1e94, 0x1ea0, 0x1ea2, 0x1ea4, 0x1ea6, 0x1ea8,
    0x1eaa, 0x1eac, 0x1eae, 0x1eb0, 0x1eb2, 0x1eb4, 0x1eb6, 0x1eb8, 0x1eba,
    0x1ebc, 0x1ebe, 0x1ec0, 0x1ec2, 0x1ec4, 0x1ec6, 0x1ec8, 0x1eca, 0x1ecc,
    0x1ece, 0x1ed0, 0x1ed2, 0x1ed4, 0x1ed6, 0x1ed8, 0x1eda, 0x1edc, 0x1ede,
    0x1ee0, 0x1ee2, 0x1ee4, 0x1ee6, 0x1ee8, 0x1eea, 0x1eec, 0x1eee, 0x1ef0,
    0x1ef2, 0x1ef4, 0x1ef6, 0x1ef8, 0x1f59, 0x1f5b, 0x1f5d, 0x1f5f, 0x1fbe,
    0x2102, 0x2107, 0x2115, 0x2124, 0x2126, 0x2128, 0x2133
};

#define NUM_UPPER_CHAR (sizeof(upperCharTable)/sizeof(chr))

static crange lowerRangeTable[] = {
    {0x0061, 0x007a}, {0x00df, 0x00f6}, {0x00f8, 0x00ff}, {0x0199, 0x019b},
    {0x0250, 0x02a8}, {0x03ac, 0x03ce}, {0x03ef, 0x03f2}, {0x0430, 0x044f},
    {0x0451, 0x045c}, {0x0561, 0x0587}, {0x10d0, 0x10f6}, {0x1e95, 0x1e9b},
    {0x1f00, 0x1f07}, {0x1f10, 0x1f15}, {0x1f20, 0x1f27}, {0x1f30, 0x1f37},
    {0x1f40, 0x1f45}, {0x1f50, 0x1f57}, {0x1f60, 0x1f67}, {0x1f70, 0x1f7d},
    {0x1f80, 0x1f87}, {0x1f90, 0x1f97}, {0x1fa0, 0x1fa7}, {0x1fb0, 0x1fb4},
    {0x1fd0, 0x1fd3}, {0x1fe0, 0x1fe7}, {0x1ff2, 0x1ff4}, {0xfb00, 0xfb06},
    {0xfb13, 0xfb17}, {0xff41, 0xff5a}
};

#define NUM_LOWER_RANGE (sizeof(lowerRangeTable)/sizeof(crange))

static chr lowerCharTable[] = {
    0x00aa, 0x00b5, 0x00ba, 0x0101, 0x0103, 0x0105, 0x0107, 0x0109, 0x010b,
    0x010d, 0x010f, 0x0111, 0x0113, 0x0115, 0x0117, 0x0119, 0x011b, 0x011d,
    0x011f, 0x0121, 0x0123, 0x0125, 0x0127, 0x0129, 0x012b, 0x012d, 0x012f,
    0x0131, 0x0133, 0x0135, 0x0138, 0x013a, 0x013c, 0x013e, 0x0140, 0x0142,
    0x0144, 0x0146, 0x0149, 0x014b, 0x014d, 0x014f, 0x0151, 0x0153, 0x0155,
    0x0157, 0x0159, 0x015b, 0x015d, 0x015f, 0x0161, 0x0163, 0x0165, 0x0167,
    0x0169, 0x016b, 0x016d, 0x016f, 0x0171, 0x0173, 0x0175, 0x0177, 0x017a,
    0x017c, 0x017e, 0x017f, 0x0180, 0x0183, 0x0185, 0x0188, 0x018c, 0x018d,
    0x0192, 0x0195, 0x019e, 0x01a1, 0x01a3, 0x01a5, 0x01a8, 0x01ab, 0x01ad,
    0x01b0, 0x01b4, 0x01b6, 0x01b9, 0x01ba, 0x01bd, 0x01c6, 0x01c9, 0x01cc,
    0x01ce, 0x01d0, 0x01d2, 0x01d4, 0x01d6, 0x01d8, 0x01da, 0x01dd, 0x01df,
    0x01e1, 0x01e3, 0x01e5, 0x01e7, 0x01e9, 0x01eb, 0x01ed, 0x01f0, 0x01f3,
    0x01f5, 0x01fb, 0x01fd, 0x01ff, 0x0201, 0x0203, 0x0205, 0x0207, 0x0209,
    0x020b, 0x020d, 0x020f, 0x0211, 0x0213, 0x0215, 0x0217, 0x0390, 0x03d0,
    0x03d1, 0x03d5, 0x03d6, 0x03e3, 0x03e5, 0x03e7, 0x03e9, 0x03eb, 0x03ed,
    0x045e, 0x045f, 0x0461, 0x0463, 0x0465, 0x0467, 0x0469, 0x046b, 0x046d,
    0x046f, 0x0471, 0x0473, 0x0475, 0x0477, 0x0479, 0x047b, 0x047d, 0x047f,
    0x0481, 0x0491, 0x0493, 0x0495, 0x0497, 0x0499, 0x049b, 0x049d, 0x049f,
    0x04a1, 0x04a3, 0x04a5, 0x04a7, 0x04a9, 0x04ab, 0x04ad, 0x04af, 0x04b1,
    0x04b3, 0x04b5, 0x04b7, 0x04b9, 0x04bb, 0x04bd, 0x04bf, 0x04c2, 0x04c4,
    0x04c8, 0x04cc, 0x04d1, 0x04d3, 0x04d5, 0x04d7, 0x04d9, 0x04db, 0x04dd,
    0x04df, 0x04e1, 0x04e3, 0x04e5, 0x04e7, 0x04e9, 0x04eb, 0x04ef, 0x04f1,
    0x04f3, 0x04f5, 0x04f9, 0x1e01, 0x1e03, 0x1e05, 0x1e07, 0x1e09, 0x1e0b,
    0x1e0d, 0x1e0f, 0x1e11, 0x1e13, 0x1e15, 0x1e17, 0x1e19, 0x1e1b, 0x1e1d,
    0x1e1f, 0x1e21, 0x1e23, 0x1e25, 0x1e27, 0x1e29, 0x1e2b, 0x1e2d, 0x1e2f,
    0x1e31, 0x1e33, 0x1e35, 0x1e37, 0x1e39, 0x1e3b, 0x1e3d, 0x1e3f, 0x1e41,
    0x1e43, 0x1e45, 0x1e47, 0x1e49, 0x1e4b, 0x1e4d, 0x1e4f, 0x1e51, 0x1e53,
    0x1e55, 0x1e57, 0x1e59, 0x1e5b, 0x1e5d, 0x1e5f, 0x1e61, 0x1e63, 0x1e65,
    0x1e67, 0x1e69, 0x1e6b, 0x1e6d, 0x1e6f, 0x1e71, 0x1e73, 0x1e75, 0x1e77,
    0x1e79, 0x1e7b, 0x1e7d, 0x1e7f, 0x1e81, 0x1e83, 0x1e85, 0x1e87, 0x1e89,
    0x1e8b, 0x1e8d, 0x1e8f, 0x1e91, 0x1e93, 0x1ea1, 0x1ea3, 0x1ea5, 0x1ea7,
    0x1ea9, 0x1eab, 0x1ead, 0x1eaf, 0x1eb1, 0x1eb3, 0x1eb5, 0x1eb7, 0x1eb9,
    0x1ebb, 0x1ebd, 0x1ebf, 0x1ec1, 0x1ec3, 0x1ec5, 0x1ec7, 0x1ec9, 0x1ecb,
    0x1ecd, 0x1ecf, 0x1ed1, 0x1ed3, 0x1ed5, 0x1ed7, 0x1ed9, 0x1edb, 0x1edd,
    0x1edf, 0x1ee1, 0x1ee3, 0x1ee5, 0x1ee7, 0x1ee9, 0x1eeb, 0x1eed, 0x1eef,
    0x1ef1, 0x1ef3, 0x1ef5, 0x1ef7, 0x1ef9, 0x1fb6, 0x1fb7, 0x1fc2, 0x1fc3,
    0x1fc4, 0x1fc6, 0x1fc7, 0x1fd6, 0x1fd7, 0x1ff6, 0x1ff7, 0x207f, 0x210a,
    0x210e, 0x210f, 0x2113, 0x212e, 0x212f, 0x2134
};

#define NUM_LOWER_CHAR (sizeof(lowerCharTable)/sizeof(chr))

/*
 * The graph table includes the set of characters that are neither ISO control
 * characters nor in the space table.
 */

static crange graphTable[] = {
    {0x0021, 0x007e}, {0x00a1, 0x1fff}, {0x2010, 0x2027}, {0x202f, 0x2069},
    {0x2070, 0x2fff}, {0x3001, 0xfefe}, {0xff00, 0xffff}
};

#define NUM_GRAPH (sizeof(graphTable)/sizeof(crange))


/* ASCII character-class table */
static struct cclass {
	char *name;
	char *chars;
	int hasch;
} cclasses[] = {
	{"blank",	" \t",		0},
	{"cntrl",	"\007\b\t\n\v\f\r\1\2\3\4\5\6\16\17\20\21\22\23\24\
\25\26\27\30\31\32\33\34\35\36\37\177",	0},
	{"graph",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
					1},
	{"lower",	"abcdefghijklmnopqrstuvwxyz",
					1},
	{"print",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ",
					1},
	{"space",	"\t\n\v\f\r ",	0},
	{"upper",	"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
					0},
	{"xdigit",	"0123456789ABCDEFabcdef",
					0},
	{NULL,		0,		0}
};

#define	CH	NOCELT

/*
 - nmcces - how many distinct MCCEs are there?
 ^ static int nmcces(struct vars *);
 */
static int
nmcces(v)
struct vars *v;
{
	return 0;
}

/*
 - nleaders - how many chrs can be first chrs of MCCEs?
 ^ static int nleaders(struct vars *);
 */
static int
nleaders(v)
struct vars *v;
{
	return 0;
}

/*
 - allmcces - return a cvec with all the MCCEs of the locale
 ^ static struct cvec *allmcces(struct vars *, struct cvec *);
 */
static struct cvec *
allmcces(v, cv)
struct vars *v;
struct cvec *cv;		/* this is supposed to have enough room */
{
	return clearcvec(cv);
}

/*
 - element - map collating-element name to celt
 ^ static celt element(struct vars *, chr *, chr *);
 */
static celt
element(v, startp, endp)
struct vars *v;
chr *startp;			/* points to start of name */
chr *endp;			/* points just past end of name */
{
	struct cname *cn;
	size_t len;
	Tcl_DString ds;
	char *np;

	/* generic:  one-chr names stand for themselves */
	assert(startp < endp);
	len = endp - startp;
	if (len == 1)
		return *startp;

	NOTE(REG_ULOCALE);

	/* search table */
	Tcl_DStringInit(&ds);
	np = TclUniCharToUtfDString(startp, (int)len, &ds);
	for (cn = cnames; cn->name != NULL; cn++)
		if (strlen(cn->name) == len && strncmp(cn->name, np, len) == 0)
			break;		/* NOTE BREAK OUT */
	Tcl_DStringFree(&ds);
	if (cn->name != NULL)
		return CHR(cn->code);

	/* couldn't find it */
	ERR(REG_ECOLLATE);
	return 0;
}

/*
 - range - supply cvec for a range, including legality check
 ^ static struct cvec *range(struct vars *, celt, celt, int);
 */
static struct cvec *
range(v, a, b, cases)
struct vars *v;
celt a;
celt b;				/* might equal a */
int cases;			/* case-independent? */
{
	int nchrs;
	struct cvec *cv;
	celt c, lc, uc, tc;

	if (a != b && !before(a, b)) {
		ERR(REG_ERANGE);
		return NULL;
	}

	if (!cases) {		/* easy version */
		cv = getcvec(v, 0, 1, 0);
		NOERRN();
		addrange(cv, a, b);
		return cv;
	}

	/*
	 * When case-independent, it's hard to decide when cvec ranges are
	 * usable, so for now at least, we won't try.  We allocate enough
	 * space for two case variants plus a little extra for the two
	 * title case variants.
	 */

	nchrs = (b - a + 1)*2 + 4;

	cv = getcvec(v, nchrs, 0, 0);
	NOERRN();

	for (c = a; c <= b; c++) {
		addchr(cv, c);
		lc = Tcl_UniCharToLower((chr)c);
		uc = Tcl_UniCharToUpper((chr)c);
		tc = Tcl_UniCharToTitle((chr)c);
		if (c != lc) {
			addchr(cv, lc);
		}
		if (c != uc) {
			addchr(cv, uc);
		}
		if (c != tc && tc != uc) {
			addchr(cv, tc);
		}
	}

	return cv;
}

/*
 - before - is celt x before celt y, for purposes of range legality?
 ^ static int before(celt, celt);
 */
static int			/* predicate */
before(x, y)
celt x;
celt y;
{
	/* trivial because no MCCEs */
	if (x < y)
		return 1;
	return 0;
}

/*
 - eclass - supply cvec for an equivalence class
 * Must include case counterparts on request.
 ^ static struct cvec *eclass(struct vars *, celt, int);
 */
static struct cvec *
eclass(v, c, cases)
struct vars *v;
celt c;
int cases;			/* all cases? */
{
	struct cvec *cv;

	/* crude fake equivalence class for testing */
	if ((v->cflags&REG_FAKEEC) && c == 'x') {
		cv = getcvec(v, 4, 0, 0);
		addchr(cv, (chr)'x');
		addchr(cv, (chr)'y');
		if (cases) {
			addchr(cv, (chr)'X');
			addchr(cv, (chr)'Y');
		}
		return cv;
	}

	/* otherwise, none */
	if (cases)
		return allcases(v, c);
	cv = getcvec(v, 1, 0, 0);
	assert(cv != NULL);
	addchr(cv, (chr)c);
	return cv;
}

/*
 - cclass - supply cvec for a character class
 * Must include case counterparts on request.
 ^ static struct cvec *cclass(struct vars *, chr *, chr *, int);
 */
static struct cvec *
cclass(v, startp, endp, cases)
struct vars *v;
chr *startp;			/* where the name starts */
chr *endp;			/* just past the end of the name */
int cases;			/* case-independent? */
{
    size_t len;
    struct cvec *cv;
    Tcl_DString ds;
    char *np, **namePtr;
    int i, index;

    /*
     * The following arrays define the valid character class names.
     */

    static char *classNames[] = {
	"alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower",
	"print", "punct", "space", "upper", "xdigit", NULL
    };

    enum classes {
	CC_ALNUM, CC_ALPHA, CC_BLANK, CC_CNTRL, CC_DIGIT, CC_GRAPH, CC_LOWER,
	CC_PRINT, CC_PUNCT, CC_SPACE, CC_UPPER, CC_XDIGIT
    };
    

    /*
     * Extract the class name
     */

    len = endp - startp;
    Tcl_DStringInit(&ds);
    np = TclUniCharToUtfDString(startp, (int)len, &ds);

    /*
     * Remap lower and upper to alpha if the match is case insensitive.
     */

    if (cases && len == 5 && (strncmp("lower", np, 5) == 0
	    || strncmp("upper", np, 5) == 0)) {
	np = "alpha";
    }

    /*
     * Map the name to the corresponding enumerated value.
     */

    index = -1;
    for (namePtr = classNames, i = 0; *namePtr != NULL; namePtr++, i++) {
	if ((strlen(*namePtr) == len) && (strncmp(*namePtr, np, len) == 0)) {
	    index = i;
	    break;
	}
    }
    Tcl_DStringInit(&ds);
    if (index == -1) {
	ERR(REG_ECTYPE);
	return NULL;
    }
    
    /*
     * Now compute the character class contents.
     */

    switch((enum classes) index) {
	case CC_PRINT:
	case CC_ALNUM:
	    cv = getcvec(v, 0, NUM_DIGIT + NUM_ALPHA, 0);
	    if (cv) {	
		for (i = 0; i < NUM_ALPHA; i++) {
		    addrange(cv, alphaTable[i].start, alphaTable[i].end);
		}
		for (i = 0; i < NUM_DIGIT; i++) {
		    addrange(cv, digitTable[i].start, digitTable[i].end);
		}
	    }
	    break;
	case CC_ALPHA:
	    cv = getcvec(v, 0, NUM_ALPHA, 0);
	    if (cv) {
		for (i = 0; i < NUM_ALPHA; i++) {
		    addrange(cv, alphaTable[i].start, alphaTable[i].end);
		}
	    }
	    break;
	case CC_BLANK:
	    cv = getcvec(v, 2, 0, 0);
	    addchr(cv, '\t');
	    addchr(cv, ' ');
	    break;
	case CC_CNTRL:
	    cv = getcvec(v, 0, 2, 0);
	    addrange(cv, 0x0, 0x1f);
	    addrange(cv, 0x7f, 0x9f);
	    break;
	case CC_DIGIT:
	    cv = getcvec(v, 0, NUM_DIGIT, 0);
	    if (cv) {	
		for (i = 0; i < NUM_DIGIT; i++) {
		    addrange(cv, digitTable[i].start, digitTable[i].end);
		}
	    }
	    break;
	case CC_PUNCT:
	    cv = getcvec(v, 0, NUM_PUNCT, 0);
	    if (cv) {	
		for (i = 0; i < NUM_PUNCT; i++) {
		    addrange(cv, punctTable[i].start, punctTable[i].end);
		}
	    }
	    break;
	case CC_XDIGIT:
	    cv = getcvec(v, 0, NUM_DIGIT+2, 0);
	    if (cv) {	
		for (i = 0; i < NUM_DIGIT; i++) {
		    addrange(cv, digitTable[i].start, digitTable[i].end);
		}
		addrange(cv, 'a', 'f');
		addrange(cv, 'A', 'F');
	    }
	    break;
	case CC_SPACE:
	    cv = getcvec(v, 0, NUM_SPACE, 0);
	    if (cv) {	
		for (i = 0; i < NUM_SPACE; i++) {
		    addrange(cv, spaceTable[i].start, spaceTable[i].end);
		}
	    }
	    break;
	case CC_LOWER:
	    cv  = getcvec(v, NUM_LOWER_CHAR, NUM_LOWER_RANGE, 0);
	    if (cv) {
		for (i = 0; i < NUM_LOWER_RANGE; i++) {
		    addrange(cv, lowerRangeTable[i].start,
			     lowerRangeTable[i].end);
		}
		for (i = 0; i < NUM_LOWER_CHAR; i++) {
		    addchr(cv, lowerCharTable[i]);
		}
	    }
	    break;
	case CC_UPPER:
	    cv  = getcvec(v, NUM_UPPER_CHAR, NUM_UPPER_RANGE, 0);
	    if (cv) {
		for (i = 0; i < NUM_UPPER_RANGE; i++) {
		    addrange(cv, upperRangeTable[i].start,
			     upperRangeTable[i].end);
		}
		for (i = 0; i < NUM_UPPER_CHAR; i++) {
		    addchr(cv, upperCharTable[i]);
		}
	    }
	    break;
	case CC_GRAPH:
	    cv = getcvec(v, 0, NUM_GRAPH, 0);
	    if (cv) {	
		for (i = 0; i < NUM_GRAPH; i++) {
		    addrange(cv, graphTable[i].start, graphTable[i].end);
		}
	    }
	    break;
    }
    if (cv == NULL) {
	ERR(REG_ESPACE);
    }
    return cv;
}

/*
 - allcases - supply cvec for all case counterparts of a chr (including itself)
 * This is a shortcut, preferably an efficient one, for simple characters;
 * messy cases are done via range().
 ^ static struct cvec *allcases(struct vars *, pchr);
 */
static struct cvec *
allcases(v, pc)
struct vars *v;
pchr pc;
{
	struct cvec *cv;
	chr c = (chr)pc;
	chr lc, uc, tc;

	lc = Tcl_UniCharToLower((chr)c);
	uc = Tcl_UniCharToUpper((chr)c);
	tc = Tcl_UniCharToTitle((chr)c);

	if (tc != uc) {
	    cv = getcvec(v, 3, 0, 0);
	    addchr(cv, tc);
	} else {
	    cv = getcvec(v, 2, 0, 0);
	}
	addchr(cv, lc);
	if (lc != uc) {
	    addchr(cv, uc);
	}
	return cv;
}

/*
 - cmp - chr-substring compare
 * Backrefs need this.  It should preferably be efficient.
 * Note that it does not need to report anything except equal/unequal.
 * Note also that the length is exact, and the comparison should not
 * stop at embedded NULs!
 ^ static int cmp(CONST chr *, CONST chr *, size_t);
 */
static int			/* 0 for equal, nonzero for unequal */
cmp(x, y, len)
CONST chr *x;
CONST chr *y;
size_t len;			/* exact length of comparison */
{
	return memcmp(VS(x), VS(y), len*sizeof(chr));
}

/*
 - casecmp - case-independent chr-substring compare
 * REG_ICASE backrefs need this.  It should preferably be efficient.
 * Note that it does not need to report anything except equal/unequal.
 * Note also that the length is exact, and the comparison should not
 * stop at embedded NULs!
 ^ static int casecmp(CONST chr *, CONST chr *, size_t);
 */
static int			/* 0 for equal, nonzero for unequal */
casecmp(x, y, len)
CONST chr *x;
CONST chr *y;
size_t len;			/* exact length of comparison */
{
	size_t i;
	CONST chr *xp;
	CONST chr *yp;

	for (xp = x, yp = y, i = len; i > 0; i--)
		if (Tcl_UniCharToLower(*xp++) != Tcl_UniCharToLower(*yp++))
			return 1;
	return 0;
}
