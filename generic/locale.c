/*
 * locale.c --
 *
 *	Regexp package file:
 * 	collating-element handling and other locale-specific stuff
 *
 * Copyright (c) 1998 Henry Spencer.  All rights reserved.
 * 
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., and Sun Microsystems Inc., none of
 * whom are responsible for the results.  The author thanks all of them.
 * 
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications. 
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * HENRY SPENCER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) locale.c 1.11 98/01/27 20:10:09
 */

/*
 * This is largely dummy code, since it needs locale interfaces.  The
 * dummy code implements more or less the C locale.  Parts of the code
 * are marked "dummy" and "generic" in hopes of making the situation
 * clearer.
 *
 * As a hack for testing, if REG_FAKE is turned on, we add a single 
 * collating element ch between c and d, and a single equivalence class
 * containing x and y.
 *
 * The type "celt" is an entirely opaque non-array type -- it need not be 
 * an integer type, it could be (say) a pointer -- which has distinct values 
 * for all chrs and all collating elements.  The only things the outside
 * world does to celts are copying them around and comparing them for
 * equality; everything else is done in this file.  There need be no "null" 
 * value for celt.  The dummy code uses wint_t as celt, with WEOF as the
 * celt code for ch (ugh!).
 */

/*
 * dummy:
 ^ #def	MAXCE	2	// longest CE code is prepared to handle
 ^ typedef wint_t celt;	// type holding distinct codes for all chrs, all CEs
 */

/* dummy:  character-name table */
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

/* dummy:  character-class table */
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

#define	CH	WEOF		/* dummy */

/*
 - nces - how many distinct collating elements are there?
 * This is pure dummy code, although a straight "return 0" is definitely
 * what's wanted for all locales lucky enough not to have these stupid
 * things.  Case counterparts should be included.
 ^ static int nces(struct vars *);
 */
static int
nces(v)
struct vars *v;
{
	return (v->cflags&REG_FAKE) ? 1 : 0;
}

/*
 - nleaders - how many chrs can be first chrs of collating elements?
 * This is pure dummy code, although a straight "return 0" is definitely
 * what's wanted for all locales lucky enough not to have these stupid
 * things.  Case counterparts should be included.
 ^ static int nleaders(struct vars *);
 */
static int
nleaders(v)
struct vars *v;
{
	return (v->cflags&REG_FAKE) ? 1 : 0;
}

/*
 - allces - return a cvec with all the collating elements of the locale
 * This would be kind of costly if there were large numbers of them; with
 * any luck, that case does not occur in reality.  Note that case variants
 * should be included; "all" means *all*.
 * This is pure dummy code.
 ^ static struct cvec *allces(struct vars *, struct cvec *);
 */
static struct cvec *
allces(v, cv)
struct vars *v;
struct cvec *cv;		/* this is supposed to have enough room */
{
	assert(cv->cespace > 0);
	(VOID) clearcvec(cv);
	if (v->cflags&REG_FAKE)
		addce(cv, ch());
	return cv;
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
	register struct cname *cn;
	register size_t len;
	Tcl_DString ds;
	char *name;

	/* generic:  one-chr names stand for themselves */
	assert(startp < endp);
	len = endp - startp;
	if (len == 1)
		return *startp;

	NOTE(REG_ULOCALE);

	/*
	 * INTL: ISO only, search table
	 */

	Tcl_DStringInit(&ds);
	name = TclUniCharToUtfDString(startp, (int) len, &ds);

	for (cn = cnames; cn->name != NULL; cn++) {
	    if (strlen(cn->name) == len && strncmp(cn->name, name, len) == 0) {
		return UCHAR(cn->code);
	    }
	}
	Tcl_DStringFree(&ds);

	/*
	 * Special case for testing.
	 */

	if ((v->cflags&REG_FAKE) && len == 2) {
		if (*startp == 'c' && *(startp+1) == 'h')
			return (celt) CH;
	}

	/* generic:  couldn't find it */
	ERR(REG_ECOLLATE);
	return 0;
}

/*
 - range - supply cvec for a range, including legality check
 * Must include case counterparts on request.
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
	int appendch;
	struct cvec *cv;
	celt c;

	/* generic:  legality check */
	if (a != b && !before(a, b)) {
		ERR(REG_ERANGE);
		return NULL;
	}

	/* mostly dummy:  compute vector length, note presence of ch */
	appendch = 0;
	if (a == (celt) CH) {
		if (b == (celt) CH) {
			a = 'c';
			b = a - 1;	/* kludge to get no chrs */
			appendch = 1;
		} else {
			a = 'd';
			appendch = 1;
		}
	} else {
		if (b == CH) {
			appendch = 1;
			b = 'c';
		} else {
			if ((v->cflags&REG_FAKE) && a <= 'c' && b >= 'd')
				appendch = 1;
		}
	}
	nchrs = b - a + 1;
	if (cases)
		nchrs *= 2;
	cv = getcvec(v, nchrs, appendch);
	NOERRN();

	/* mostly dummy:  fill in vector */
	for (c = a; c <= b; c++) {
		addchr(cv, c);
		if (cases) {
			if (TclUniCharIsUpper((Tcl_UniChar)c))
				addchr(cv, (chr)Tcl_UniCharToLower(
				    (Tcl_UniChar)c));
			else if (TclUniCharIsLower((Tcl_UniChar)c))
				addchr(cv, (chr)Tcl_UniCharToUpper(
				    (Tcl_UniChar)c));
		}
	}
	if (appendch)
		addce(cv, ch());

	return cv;
}

/*
 - before - is celt x before celt y, for purposes of range legality?
 * This is all dummy code.
 ^ static int before(celt, celt);
 */
static int			/* predicate */
before(x, y)
celt x;
celt y;
{
	int isxch = (x == CH);
	int isych = (y == CH);

	if (!isxch && !isych && x < y)
		return 1;
	if (isxch && !isych && y >= 'd')
		return 1;
	if (!isxch && isych && x <= 'c')
		return 1;
	return 0;
}

/*
 - eclass - supply cvec for an equivalence class
 * Must include case counterparts on request.
 * This is all dummy code.
 ^ static struct cvec *eclass(struct vars *, celt, int);
 */
static struct cvec *
eclass(v, c, cases)
struct vars *v;
celt c;
int cases;			/* all cases? */
{
	struct cvec *cv;

	if (c == CH) {
		cv = getcvec(v, 0, 1);
		assert(cv != NULL);
		addce(cv, ch());
		return cv;
	}

	if ((v->cflags&REG_FAKE) && (c == 'x' || c == 'y')) {
		cv = getcvec(v, 4, 0);
		assert(cv != NULL);
		addchr(cv, (chr)'x');
		addchr(cv, (chr)'y');
		if (cases) {
			addchr(cv, (chr)'X');
			addchr(cv, (chr)'Y');
		}
		return cv;
	}

	/* no equivalence class by that name */
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
 * This is all dummy code.
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
	register char *p;
	register struct cclass *cc;
	int hasch;
	struct cvec *cv;
	Tcl_DString ds;
	char *name;

	/* check out the name */
	len = endp - startp;

	Tcl_DStringInit(&ds);
	name = TclUniCharToUtfDString(startp, (int) len, &ds);

	if (cases && len == 5 && (strncmp("lower", name, 5) == 0 ||
				strncmp("upper", name, 5) == 0))
		name = "alpha";
	for (cc = cclasses; cc->name != NULL; cc++) {
	    if (strlen(cc->name) == len && strncmp(cc->name, name, len) == 0) {
		break;
	    }
	}
	Tcl_DStringFree(&ds);

	if (cc->name == NULL) {
		ERR(REG_ECTYPE);
		return NULL;
	}

	/* set up vector */
	hasch = (v->cflags&REG_FAKE) ? cc->hasch : 0;
	cv = getcvec(v, (int) strlen(cc->chars), hasch);
	if (cv == NULL) {
		ERR(REG_ESPACE);
		return NULL;
	}

	/* fill it in */
	for (p = cc->chars; *p != '\0'; p++)
		addchr(cv, (chr)*p);
	if (hasch)
		addce(cv, ch());

	return cv;
}

/*
 - allcases - supply cvec for all case counterparts of a chr (including itself)
 * This is a shortcut, preferably an efficient one, for simple characters;
 * messy cases are done via range().
 * This is all dummy code.
 ^ static struct cvec *allcases(struct vars *, pchr);
 */
static struct cvec *
allcases(v, c)
struct vars *v;
pchr c;
{
	struct cvec *cv = getcvec(v, 2, 0);

	assert(cv != NULL);
	addchr(cv, c);
	if (TclUniCharIsUpper((Tcl_UniChar)c))
		addchr(cv, (chr)Tcl_UniCharToLower((Tcl_UniChar)c));
	else if (TclUniCharIsLower((Tcl_UniChar)c))
		addchr(cv, (chr)Tcl_UniCharToUpper((Tcl_UniChar)c));

	return cv;
}

/*
 - sncmp - case-independent chr-string compare
 * REG_ICASE backrefs need this.  It should preferably be efficient.
 * This is all dummy code.
 ^ static int sncmp(CONST chr *, CONST chr *, size_t);
 */
static int			/* -1, 0, 1 for <, =, > */
sncmp(x, y, len)
CONST chr *x;
CONST chr *y;
size_t len;			/* maximum length of comparison */
{
    int diff;
    size_t i;

    for (i = 0; i < len; i++) {
	diff = Tcl_UniCharToLower(x[i]) - Tcl_UniCharToLower(y[i]);
	if (diff) {
	    return diff;
	}
    }
    return 0;
}

/*
 * Utility functions for handling cvecs
 */

/*
 - newcvec - allocate a new cvec
 ^ static struct cvec *newcvec(int, int);
 */
static struct cvec *
newcvec(nchrs, nces)
int nchrs;			/* to hold this many chrs... */
int nces;			/* ... and this many CEs */
{
	size_t n;
	size_t nc;
	struct cvec *cv;

	nc = (size_t)nchrs + (size_t)nces*(MAXCE+1);
	n = sizeof(struct cvec) + (size_t)(nces-1)*sizeof(chr *) +
								nc*sizeof(chr);
	cv = (struct cvec *)ckalloc(n);
	if (cv == NULL)
		return NULL;
	cv->chrspace = nc;
	cv->chrs = (chr *)&cv->ces[nces];	/* chrs just after CE ptrs */
	cv->cespace = nces;
	return clearcvec(cv);
}

/*
 - clearcvec - clear a possibly-new cvec
 * Returns pointer as convenience.
 ^ static struct cvec *clearcvec(struct cvec *);
 */
static struct cvec *
clearcvec(cv)
struct cvec *cv;
{
	int i;

	assert(cv != NULL);
	cv->nchrs = 0;
	assert(cv->chrs == (chr *)&cv->ces[cv->cespace]);
	cv->nces = 0;
	cv->ncechrs = 0;
	for (i = 0; i < cv->cespace; i++)
		cv->ces[i] = NULL;

	return cv;
}

/*
 - addchr - add a chr to a cvec
 ^ static VOID addchr(struct cvec *, pchr);
 */
static VOID
addchr(cv, c)
struct cvec *cv;
pchr c;
{
	assert(cv->nchrs < cv->chrspace - cv->ncechrs);
	cv->chrs[cv->nchrs++] = (chr) c;
}

/*
 - addce - add a CE to a cvec
 ^ static VOID addce(struct cvec *, chr *);
 */
static VOID
addce(cv, startp)
struct cvec *cv;
chr *startp;			/* 0-terminated text */
{
	int n = wcslen(startp);
	int i;
	chr *s;
	chr *d;

	assert(n > 0);
	assert(cv->nchrs + n < cv->chrspace - cv->ncechrs);
	assert(cv->nces < cv->cespace);
	d = &cv->chrs[cv->chrspace - cv->ncechrs - n - 1];
	cv->ces[cv->nces++] = d;
	for (s = startp, i = n; i > 0; s++, i--)
		*d++ = *s;
	*d = 0;		/* endmarker */
	assert(d == &cv->chrs[cv->chrspace - cv->ncechrs]);
	cv->ncechrs += n + 1;
}

/*
 - haschr - does a cvec contain this chr?
 ^ static int haschr(struct cvec *, pchr);
 */
static int			/* predicate */
haschr(cv, c)
struct cvec *cv;
pchr c;
{
	int i;
	chr *p;

	for (p = cv->chrs, i = cv->nchrs; i > 0; p++, i--)
		if (*p == c)
			return 1;
	return 0;
}

/*
 - getcvec - get a cvec, remembering it as v->cv
 ^ static struct cvec *getcvec(struct vars *, int, int);
 */
static struct cvec *
getcvec(v, nchrs, nces)
struct vars *v;
int nchrs;			/* to hold this many chrs... */
int nces;			/* ... and this many CEs */
{
	if (v->cv != NULL && nchrs <= v->cv->chrspace && nces <= v->cv->cespace)
		return clearcvec(v->cv);

	if (v->cv != NULL)
		freecvec(v->cv);
	v->cv = newcvec(nchrs, nces);
	if (v->cv == NULL)
		ERR(REG_ESPACE);

	return v->cv;
}

/*
 - freecvec - free a cvec
 ^ static VOID freecvec(struct cvec *);
 */
static VOID
freecvec(cv)
struct cvec *cv;
{
	ckfree((char *)cv);
}
