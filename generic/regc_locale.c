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

/* ASCII character-class table */
static struct cclass {
	char *name;
	char *chars;
	int hasch;
} cclasses[] = {
	{"alnum",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789",				1},
	{"alpha",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
					1},
	{"blank",	" \t",		0},
	{"cntrl",	"\007\b\t\n\v\f\r\1\2\3\4\5\6\16\17\20\21\22\23\24\
\25\26\27\30\31\32\33\34\35\36\37\177",	0},
	{"digit",	"0123456789",	0},
	{"graph",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
					1},
	{"lower",	"abcdefghijklmnopqrstuvwxyz",
					1},
	{"print",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ",
					1},
	{"punct",	"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
					0},
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

	nchrs = b - a + 1;
	if (cases)
		nchrs *= 2;
	cv = getcvec(v, nchrs, 0);
	NOERRN();

	for (c = a; c <= b; c++) {
		addchr(cv, c);
		if (cases) {
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
		cv = getcvec(v, 4, 0);
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
	cv = getcvec(v, 1, 0);
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
	char *p;
	struct cclass *cc;
	struct cvec *cv;
	Tcl_DString ds;
	char *np;

	/* find the name */
	len = endp - startp;
	Tcl_DStringInit(&ds);
	np = TclUniCharToUtfDString(startp, (int)len, &ds);
	if (cases && len == 5 && (strncmp("lower", np, 5) == 0 ||
					strncmp("upper", np, 5) == 0))
		np = "alpha";
	for (cc = cclasses; cc->name != NULL; cc++)
		if (strlen(cc->name) == len && strncmp(cc->name, np, len) == 0)
			break;		/* NOTE BREAK OUT */
	Tcl_DStringFree(&ds);
	if (cc->name == NULL) {
		ERR(REG_ECTYPE);
		return NULL;
	}

	/* set up vector */
	cv = getcvec(v, (int)strlen(cc->chars), 0);
	if (cv == NULL) {
		ERR(REG_ESPACE);
		return NULL;
	}

	/* fill it in */
	for (p = cc->chars; *p != '\0'; p++)
		addchr(cv, (chr)*p);

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
	struct cvec *cv = getcvec(v, 2, 0);
	chr c = (chr)pc;

	assert(cv != NULL);
	addchr(cv, c);
	if (TclUniCharIsUpper(c))
		addchr(cv, Tcl_UniCharToLower(c));
	else if (TclUniCharIsLower(c))
		addchr(cv, Tcl_UniCharToUpper(c));

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
