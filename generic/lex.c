/*
 * lex --
 *
 *	Regexp package file:  lexical analyzer - #included in other source
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
 * SCCS: @(#) lex.c 1.7 98/01/21 14:33:10
 */

/* scanning macros (know about v) */
#define	ATEOS()		(v->now >= v->stop)
#define	HAVE(n)		(v->stop - v->now >= (n))
#define	NEXT1(c)	(!ATEOS() && *v->now == CHR(c))
#define	NEXT2(a,b)	(HAVE(2) && *v->now == CHR(a) && *(v->now+1) == CHR(b))
#define	NEXT3(a,b,c)	(HAVE(3) && *v->now == CHR(a) && \
						*(v->now+1) == CHR(b) && \
						*(v->now+2) == CHR(c))
#define	SET(c)		(v->nexttype = (c))
#define	SETV(c, n)	(v->nexttype = (c), v->nextvalue = (n))
#define	RET(c)		return (SET(c), 1)
#define	RETV(c, n)	return (SETV(c, n), 1)
#define	FAILW(e)	return (ERR(e), 0)	/* ERR does SET(EOS) */
#define	LASTTYPE(t)	(v->lasttype == (t))

/* lexical contexts */
#define	L_ERE	1	/* mainline ERE/ARE */
#define	L_BRE	2	/* mainline BRE */
#define	L_Q	3	/* REG_QUOTE */
#define	L_EBND	4	/* ERE/ARE bound */
#define	L_BBND	5	/* BRE bound */
#define	L_BRACK	6	/* brackets */
#define	L_CEL	7	/* collating element */
#define	L_ECL	8	/* equivalence class */
#define	L_CCL	9	/* character class */
#define	INTO(c)		(v->lexcon = (c))
#define	_IN(con)		(v->lexcon == (con))

/*
 - lexstart - set up lexical stuff, scan leading options
 ^ static VOID lexstart(struct vars *);
 */
static VOID
lexstart(v)
register struct vars *v;
{
	prefixes(v);			/* may turn on new type bits etc. */
	NOERR();

	if (v->cflags&REG_QUOTE) {
		v->cflags &= ~(REG_EXTENDED|REG_ADVF|REG_EXPANDED);
		INTO(L_Q);
	} else if (v->cflags&REG_EXTENDED)
		INTO(L_ERE);
	else {
		v->cflags &= ~REG_ADVF;
		INTO(L_BRE);
	}

	v->nexttype = EMPTY;		/* remember we were at the start */
	next(v);			/* set up the first token */
}

/*
 - prefixes - implement various special prefixes
 ^ static VOID prefixes(struct vars *);
 */
static VOID
prefixes(v)
struct vars *v;
{
	/* literal string doesn't get any of this stuff */
	if (v->cflags&REG_QUOTE)
		return;

	/* initial "***" gets special things */	
	if (HAVE(4) && NEXT3('*', '*', '*'))
		switch (*(v->now + 3)) {
		case CHR('?'):		/* "***?" error, msg shows version */
			ERR(REG_BADPAT);
			return;		/* proceed no further */
		case CHR('='):		/* "***=" shifts to literal string */
			NOTE(REG_UNONPOSIX);
			v->cflags |= REG_QUOTE;
			v->now += 4;
			return;		/* and there can be no more prefixes */
		case CHR(':'):		/* "***:" shifts to AREs */
			NOTE(REG_UNONPOSIX);
			v->cflags |= REG_ADVANCED;
			v->now += 4;
			break;
		default:		/* otherwise *** is just an error */
			ERR(REG_BADRPT);
			return;
		}

	/* BREs and plain EREs don't get any other favors */
	if ((v->cflags&REG_ADVANCED) != REG_ADVANCED)
		return;

	/* embedded options */
	if (HAVE(3) && NEXT2('(', '?') && iswalpha(*(v->now + 2))) {
		NOTE(REG_UNONPOSIX);
		v->now += 2;
		for (; !ATEOS() && iswalpha(*v->now); v->now++)
			switch (*v->now) {
			case CHR('b'):		/* BREs (but why???) */
				v->cflags &= ~REG_EXTENDED;
				break;
			case CHR('c'):		/* case sensitive */
				v->cflags &= ~REG_ICASE;
				break;
			case CHR('e'):		/* plain EREs */
				v->cflags &= ~REG_ADVF;
				break;
			case CHR('i'):		/* case insensitive */
				v->cflags |= REG_ICASE;
				break;
			case CHR('m'):		/* Perloid synonym for n */
			case CHR('n'):		/* \n affects ^ $ . [^ */
				v->cflags |= REG_NEWLINE;
				break;
			case CHR('p'):		/* ~Perl, \n affects . [^ */
				v->cflags |= REG_NLSTOP;
				v->cflags &= ~REG_NLANCH;
				break;
			case CHR('q'):		/* literal string */
				v->cflags |= REG_QUOTE;
				break;
			case CHR('s'):		/* single line, \n ordinary */
				v->cflags &= ~REG_NEWLINE;
				break;
			case CHR('t'):		/* tight syntax */
				v->cflags &= ~REG_EXPANDED;
				break;
			case CHR('w'):		/* weird, \n affects ^ $ only */
				v->cflags &= ~REG_NLSTOP;
				v->cflags |= REG_NLANCH;
				break;
			case CHR('x'):		/* expanded syntax */
				v->cflags |= REG_EXPANDED;
				break;
			default:
				ERR(REG_BADOPT);
				return;
			}
		if (!NEXT1(')')) {
			ERR(REG_BADOPT);
			return;
		}
		v->now++;
	}
}

/*
 - lexnest - "call a subroutine", interpolating string at the lexical level
 * Note, this is not a very general facility.  There are a number of
 * implicit assumptions about what sorts of strings can be subroutines.
 ^ static VOID lexnest(struct vars *, chr *);
 */
static VOID
lexnest(v, s)
struct vars *v;
chr *s;
{
	assert(v->savenow == NULL);	/* only one level of nesting */
	v->savenow = v->now;
	v->savestop = v->stop;
	v->now = s;
	v->stop = s + wcslen(s);
}

/*
 * string CONSTants to interpolate as expansions of things like \d
 */
static chr backd[] = {		/* \d */
	CHR('['), CHR('['), CHR(':'),
	CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'),
	CHR(':'), CHR(']'), CHR(']'), CHR('\0')
};
static chr backD[] = {		/* \D */
	CHR('['), CHR('^'), CHR('['), CHR(':'),
	CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'),
	CHR(':'), CHR(']'), CHR(']'), CHR('\0')
};
static chr brbackd[] = {	/* \d within brackets */
	CHR('['), CHR(':'),
	CHR('d'), CHR('i'), CHR('g'), CHR('i'), CHR('t'),
	CHR(':'), CHR(']'), CHR('\0')
};
static chr backs[] = {		/* \s */
	CHR('['), CHR('['), CHR(':'),
	CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'),
	CHR(':'), CHR(']'), CHR(']'), CHR('\0')
};
static chr backS[] = {		/* \S */
	CHR('['), CHR('^'), CHR('['), CHR(':'),
	CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'),
	CHR(':'), CHR(']'), CHR(']'), CHR('\0')
};
static chr brbacks[] = {	/* \s within brackets */
	CHR('['), CHR(':'),
	CHR('s'), CHR('p'), CHR('a'), CHR('c'), CHR('e'),
	CHR(':'), CHR(']'), CHR('\0')
};
static chr backw[] = {		/* \w */
	CHR('['), CHR('['), CHR(':'),
	CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'),
	CHR(':'), CHR(']'), CHR('_'), CHR(']'), CHR('\0')
};
static chr backW[] = {		/* \W */
	CHR('['), CHR('^'), CHR('['), CHR(':'),
	CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'),
	CHR(':'), CHR(']'), CHR('_'), CHR(']'), CHR('\0')
};
static chr brbackw[] = {	/* \w within brackets */
	CHR('['), CHR(':'),
	CHR('a'), CHR('l'), CHR('n'), CHR('u'), CHR('m'),
	CHR(':'), CHR(']'), CHR('_'), CHR('\0')
};

/*
 - lexword - interpolate a bracket expression for word characters
 * Possibly ought to inquire whether there is a "word" character class.
 ^ static VOID lexword(struct vars *);
 */
static VOID
lexword(v)
struct vars *v;
{
	lexnest(v, backw);
}

/*
 - next - get next token
 ^ static int next(struct vars *);
 */
static int			/* 1 normal, 0 failure */
next(v)
register struct vars *v;
{
	register chr c;

	/* errors yield an infinite sequence of failures */
	if (ISERR())
		return 0;	/* the error has set nexttype to EOS */

	/* remember flavor of last token */
	v->lasttype = v->nexttype;

	/* if we're nested and we've hit end, return to outer level */
	if (v->savenow != NULL && ATEOS()) {
		v->now = v->savenow;
		v->stop = v->savestop;
		v->savenow = v->savestop = NULL;
	}

	/* skip white space etc. if appropriate (not in literal or []) */
	if (v->cflags&REG_EXPANDED)
		switch (v->lexcon) {
		case L_ERE:
		case L_BRE:
		case L_EBND:
		case L_BBND:
			skip(v);
			break;
		}

	/* handle EOS, depending on context */
	if (ATEOS()) {
		switch (v->lexcon) {
		case L_ERE:
		case L_BRE:
		case L_Q:
			RET(EOS);
		case L_EBND:
		case L_BBND:
			FAILW(REG_EBRACE);
		case L_BRACK:
		case L_CEL:
		case L_ECL:
		case L_CCL:
			FAILW(REG_EBRACK);
		}
		assert(NOTREACHED);
	}

	/* okay, time to actually get a character */
	c = *v->now++;

	/* deal with the easy contexts, punt EREs to code below */
	switch (v->lexcon) {
	case L_BRE:			/* punt BREs to separate function */
		return brenext(v, c);
	case L_ERE:			/* see below */
		break;
	case L_Q:			/* literal strings are easy */
		RETV(PLAIN, c);
	case L_BBND:			/* bounds are fairly simple */
	case L_EBND:
		switch (c) {
		case CHR('0'): case CHR('1'): case CHR('2'): case CHR('3'):
		case CHR('4'): case CHR('5'): case CHR('6'): case CHR('7'):
		case CHR('8'): case CHR('9'):
			RETV(DIGIT, (chr) DIGITVAL(c));
		case CHR(','):
			RET(',');
		case CHR('}'):		/* ERE bound ends with } */
			if (_IN(L_EBND)) {
				INTO(L_ERE);
				if ((v->cflags&REG_ADVF) && NEXT1('?')) {
					v->now++;
					NOTE(REG_UNONPOSIX);
					RETV('}', 0);
				}
				RETV('}', 1);
			} else
				FAILW(REG_BADBR);
		case CHR('\\'):		/* BRE bound ends with \} */
			if (_IN(L_BBND) && NEXT1('}')) {
				v->now++;
				INTO(L_BRE);
				RET('}');
			} else
				FAILW(REG_BADBR);
		default:
			FAILW(REG_BADBR);
		}
	case L_BRACK:			/* brackets are not too hard */
		switch (c) {
		case CHR(']'):
			if (LASTTYPE('['))
				RETV(PLAIN, c);
			else {
				INTO((v->cflags&REG_EXTENDED) ? L_ERE : L_BRE);
				RET(']');
			}
		case CHR('\\'):
			NOTE(REG_UBBS);
			if (!(v->cflags&REG_ADVF))
				RETV(PLAIN, c);
			NOTE(REG_UNONPOSIX);
			if (ATEOS())
				FAILW(REG_EESCAPE);
			(VOID) lexescape(v);
			switch (v->nexttype) {	/* not all escapes okay here */
			case PLAIN:
				return 1;
			case CCLASS:
				switch (v->nextvalue) {
				case 'd':	lexnest(v, brbackd); break;
				case 's':	lexnest(v, brbacks); break;
				case 'w':	lexnest(v, brbackw); break;
				default:
					FAILW(REG_EESCAPE);
				}
				/* lexnest done, back up and try again */
				v->nexttype = v->lasttype;
				return next(v);
			}
			/* not one of the acceptable escapes */
			FAILW(REG_EESCAPE);
		case CHR('-'):
			if (LASTTYPE('[') || NEXT1(']'))
				RETV(PLAIN, c);
			else
				RETV(RANGE, c);
		case CHR('['):
			if (ATEOS())
				FAILW(REG_EBRACK);
			switch (*v->now++) {
			case CHR('.'):
				INTO(L_CEL);
				/* might or might not be locale-specific */
				RET(COLLEL);
			case CHR('='):
				INTO(L_ECL);
				NOTE(REG_ULOCALE);
				RET(ECLASS);
			case CHR(':'):
				INTO(L_CCL);
				NOTE(REG_ULOCALE);
				RET(CCLASS);
			default:			/* oops */
				v->now--;
				RETV(PLAIN, c);
			}
		default:
			RETV(PLAIN, c);
		}
	case L_CEL:			/* collating elements are easy */
		if (c == CHR('.') && NEXT1(']')) {
			v->now++;
			INTO(L_BRACK);
			RETV(END, '.');
		} else
			RETV(PLAIN, c);
	case L_ECL:			/* ditto equivalence classes */
		if (c == CHR('=') && NEXT1(']')) {
			v->now++;
			INTO(L_BRACK);
			RETV(END, '=');
		} else
			RETV(PLAIN, c);
	case L_CCL:			/* ditto character classes */
		if (c == CHR(':') && NEXT1(']')) {
			v->now++;
			INTO(L_BRACK);
			RETV(END, ':');
		} else
			RETV(PLAIN, c);
	default:
		assert(NOTREACHED);
		break;
	}

	/* that got rid of everything except EREs */
	assert(_IN(L_ERE));

	/* deal with EREs, except for backslashes */
	switch (c) {
	case CHR('|'):
		RET('|');
	case CHR('*'):
		if ((v->cflags&REG_ADVF) && NEXT1('?')) {
			v->now++;
			NOTE(REG_UNONPOSIX);
			RETV('*', 0);
		}
		RETV('*', 1);
	case CHR('+'):
		if ((v->cflags&REG_ADVF) && NEXT1('?')) {
			v->now++;
			NOTE(REG_UNONPOSIX);
			RETV('+', 0);
		}
		RETV('+', 1);
	case CHR('?'):
		if ((v->cflags&REG_ADVF) && NEXT1('?')) {
			v->now++;
			NOTE(REG_UNONPOSIX);
			RETV('?', 0);
		}
		RETV('?', 1);
	case CHR('{'):		/* bounds start or plain character */
		if (v->cflags&REG_EXPANDED)
			skip(v);
		if (ATEOS() || !iswdigit(*v->now)) {
			NOTE(REG_UBRACES);
			NOTE(REG_UUNSPEC);
			RETV(PLAIN, c);
		} else {
			NOTE(REG_UBOUNDS);
			INTO(L_EBND);
			RET('{');
		}
	case CHR('('):		/* parenthesis, or advanced extension */
		if ((v->cflags&REG_ADVF) && NEXT1('?')) {
			NOTE(REG_UNONPOSIX);
			v->now++;
			switch (*v->now++) {
			case CHR(':'):		/* non-capturing paren */
				RETV('(', 0);
			case CHR('#'):		/* comment */
				while (!ATEOS() && *v->now != CHR(')'))
					v->now++;
				if (!ATEOS())
					v->now++;
				assert(v->nexttype == v->lasttype);
				return next(v);
			case CHR('='):		/* positive lookahead */
				NOTE(REG_ULOOKAHEAD);
				RETV(LACON, 1);
			case CHR('!'):		/* negative lookahead */
				NOTE(REG_ULOOKAHEAD);
				RETV(LACON, 0);
			case CHR('<'):		/* prefer short */
				RETV(PREFER, 0);
			case CHR('>'):		/* prefer long */
				RETV(PREFER, 1);
			default:
				FAILW(REG_BADRPT);
			}
		}
		if (v->cflags&REG_NOSUB) {
		    RETV('(', 0);		/* all parens non-capturing */
		}
		RETV('(', 1);
	case CHR(')'):
		if (LASTTYPE('('))
			NOTE(REG_UUNSPEC);
		RETV(')', c);
	case CHR('['):		/* easy except for [[:<:]] and [[:>:]] */
		if (HAVE(6) &&	*(v->now+0) == CHR('[') &&
				*(v->now+1) == CHR(':') &&
				(*(v->now+2) == CHR('<') ||
						*(v->now+2) == CHR('>')) &&
				*(v->now+3) == CHR(':') &&
				*(v->now+4) == CHR(']') &&
				*(v->now+5) == CHR(']')) {
			c = *(v->now+2);
			v->now += 6;
			NOTE(REG_UNONPOSIX);
			RET((c == CHR('<')) ? '<' : '>');
		}
		INTO(L_BRACK);
		if (NEXT1('^')) {
			v->now++;
			RETV('[', 0);
		}
		RETV('[', 1);
	case CHR('.'):
		RET('.');
	case CHR('^'):
		RET('^');
	case CHR('$'):
		RET('$');
	case CHR('\\'):		/* mostly punt backslashes to code below */
		if (ATEOS())
			FAILW(REG_EESCAPE);
		break;
	default:		/* ordinary character */
		RETV(PLAIN, c);
	}

	/* ERE backslash handling; backslash already eaten */
	assert(!ATEOS());
	if (!(v->cflags&REG_ADVF)) {	/* only AREs have non-trivial escapes */
		if (iswalnum(*v->now)) {
			NOTE(REG_UBSALNUM);
			NOTE(REG_UUNSPEC);
		}
		RETV(PLAIN, *v->now++);
	}
	(VOID) lexescape(v);
	if (ISERR())
		FAILW(REG_EESCAPE);
	if (v->nexttype == CCLASS) {	/* fudge at lexical level */
		switch (v->nextvalue) {
		case 'd':	lexnest(v, backd); break;
		case 'D':	lexnest(v, backD); break;
		case 's':	lexnest(v, backs); break;
		case 'S':	lexnest(v, backS); break;
		case 'w':	lexnest(v, backw); break;
		case 'W':	lexnest(v, backW); break;
		default:
			assert(NOTREACHED);
			FAILW(REG_ASSERT);
		}
		/* lexnest done, back up and try again */
		v->nexttype = v->lasttype;
		return next(v);
	}
	/* otherwise, lexescape has already done the work */
	return !ISERR();
}

/*
 - lexescape - parse an ARE backslash escape (backslash already eaten)
 * Note slightly nonstandard use of the CCLASS type code.
 ^ static int lexescape(struct vars *);
 */
static int			/* not actually used, but convenient for RETV */
lexescape(v)
struct vars *v;
{
	chr c;
	static chr alert[] = {
		CHR('a'), CHR('l'), CHR('e'), CHR('r'), CHR('t'), CHR('\0')
	};
	static chr esc[] = {
		CHR('E'), CHR('S'), CHR('C'), CHR('\0')
	};
	chr *save;

	assert(v->cflags&REG_ADVF);

	assert(!ATEOS());
	c = *v->now++;
	if (!iswalnum(c))
		RETV(PLAIN, c);

	NOTE(REG_UNONPOSIX);
	switch (c) {
	case CHR('a'):
		RETV(PLAIN, chrnamed(v, alert, CHR('\007')));
	case CHR('A'):
		RETV(SBEGIN, 0);
	case CHR('b'):
		RETV(PLAIN, CHR('\b'));
	case CHR('c'):
		NOTE(REG_UUNPORT);
		if (ATEOS())
			FAILW(REG_EESCAPE);
		RETV(PLAIN, (chr) (*v->now++ & 037));
	case CHR('d'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 'd');
	case CHR('D'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 'D');
	case CHR('e'):
		NOTE(REG_UUNPORT);
		RETV(PLAIN, chrnamed(v, esc, CHR('\033')));
	case CHR('E'):
		RETV(PLAIN, CHR('\\'));
	case CHR('f'):
		RETV(PLAIN, CHR('\f'));
	case CHR('n'):
		RETV(PLAIN, CHR('\n'));
	case CHR('r'):
		RETV(PLAIN, CHR('\r'));
	case CHR('s'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 's');
	case CHR('S'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 'S');
	case CHR('t'):
		RETV(PLAIN, CHR('\t'));
	case CHR('u'):
		c = lexdigits(v, 16, 4, 4);
		if (ISERR())
			FAILW(REG_EESCAPE);
		RETV(PLAIN, c);
	case CHR('U'):
		c = lexdigits(v, 16, 8, 8);
		if (ISERR())
			FAILW(REG_EESCAPE);
		RETV(PLAIN, c);
	case CHR('v'):
		RETV(PLAIN, CHR('\v'));
	case CHR('w'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 'w');
	case CHR('W'):
		NOTE(REG_ULOCALE);
		RETV(CCLASS, 'W');
	case CHR('x'):
		NOTE(REG_UUNPORT);
		c = lexdigits(v, 16, 1, 255);	/* REs >255 long outside spec */
		if (ISERR())
			FAILW(REG_EESCAPE);
		RETV(PLAIN, c);
	case CHR('y'):
		NOTE(REG_ULOCALE);
		RETV(WBDRY, 0);
	case CHR('Y'):
		NOTE(REG_ULOCALE);
		RETV(NWBDRY, 0);
	case CHR('Z'):
		RETV(SEND, 0);
	case CHR('1'): case CHR('2'): case CHR('3'): case CHR('4'):
	case CHR('5'): case CHR('6'): case CHR('7'): case CHR('8'):
	case CHR('9'):
		save = v->now;
		v->now--;	/* put first digit back */
		c = lexdigits(v, 10, 1, 255);	/* REs >255 long outside spec */
		if (ISERR())
			FAILW(REG_EESCAPE);
		/* ugly heuristic (first test is "exactly 1 digit?") */
		if (v->now - save == 0 || (int)c <= v->nsubexp) {
			NOTE(REG_UBACKREF);
			RETV(BACKREF, (chr) c);
		}
		/* oops, doesn't look like it's a backref after all... */
		v->now = save;
		/* and fall through into octal number */
	case CHR('0'):
		NOTE(REG_UUNPORT);
		v->now--;	/* put first digit back */
		c = lexdigits(v, 8, 1, 3);
		if (ISERR())
			FAILW(REG_EESCAPE);
		RETV(PLAIN, c);
	default:
		assert(iswalpha(c));
		FAILW(REG_EESCAPE);	/* unknown alphabetic escape */
	}
}

/*
 - lexdigits - slurp up digits and return chr value
 ^ static chr lexdigits(struct vars *, int, int, int);
 */
static chr			/* chr value; errors signalled via ERR */
lexdigits(v, base, minlen, maxlen)
struct vars *v;
int base;
int minlen;
int maxlen;
{
	uchr n;			/* unsigned to aVOID overflow misbehavior */
	int len;
	chr c;
	int d;
	CONST uchr ub = (uchr) base;

	n = 0;
	for (len = 0; len < maxlen && !ATEOS(); len++) {
		c = *v->now++;
		switch (c) {
		case CHR('0'): case CHR('1'): case CHR('2'): case CHR('3'):
		case CHR('4'): case CHR('5'): case CHR('6'): case CHR('7'):
		case CHR('8'): case CHR('9'):
			d = DIGITVAL(c);
			break;
		case CHR('a'): case CHR('A'): d = 10; break;
		case CHR('b'): case CHR('B'): d = 11; break;
		case CHR('c'): case CHR('C'): d = 12; break;
		case CHR('d'): case CHR('D'): d = 13; break;
		case CHR('e'): case CHR('E'): d = 14; break;
		case CHR('f'): case CHR('F'): d = 15; break;
		default:
			v->now--;	/* oops, not a digit at all */
			d = -1;
			break;
		}

		if (d >= base) {	/* not a plausible digit */
			v->now--;
			d = -1;
		}
		if (d < 0)
			break;		/* NOTE BREAK OUT */
		n = n*ub + (uchr)d;
	}
	if (len < minlen)
		ERR(REG_EESCAPE);

	return (chr)n;
}

/*
 - brenext - get next BRE token
 * This is much like EREs except for all the stupid backslashes and the
 * context-dependency of some things.
 ^ static int brenext(struct vars *, pchr);
 */
static int			/* 1 normal, 0 failure */
brenext(v, pc)
register struct vars *v;
register pchr pc;
{
	register chr c = (chr) pc;

	switch (c) {
	case CHR('*'):
		if (LASTTYPE(EMPTY) || LASTTYPE('(') || LASTTYPE('^'))
			RETV(PLAIN, c);
		RET('*');
	case CHR('['):
		if (HAVE(6) &&	*(v->now+0) == CHR('[') &&
				*(v->now+1) == CHR(':') &&
				(*(v->now+2) == CHR('<') ||
						*(v->now+2) == CHR('>')) &&
				*(v->now+3) == CHR(':') &&
				*(v->now+4) == CHR(']') &&
				*(v->now+5) == CHR(']')) {
			c = *(v->now+2);
			v->now += 6;
			NOTE(REG_UNONPOSIX);
			RET((c == CHR('<')) ? '<' : '>');
		}
		INTO(L_BRACK);
		if (NEXT1('^')) {
			v->now++;
			RETV('[', 0);
		}
		RETV('[', 1);
	case CHR('.'):
		RET('.');
	case CHR('^'):
		if (LASTTYPE(EMPTY))
			RET('^');
		if (LASTTYPE('(')) {
			NOTE(REG_UUNSPEC);
			RET('^');
		}
		RETV(PLAIN, c);
	case CHR('$'):
		if (v->cflags&REG_EXPANDED)
			skip(v);
		if (ATEOS())
			RET('$');
		if (NEXT2('\\', ')')) {
			NOTE(REG_UUNSPEC);
			RET('$');
		}
		RETV(PLAIN, c);
	case CHR('\\'):
		break;		/* see below */
	default:
		RETV(PLAIN, c);
	}

	assert(c == CHR('\\'));

	if (ATEOS())
		FAILW(REG_EESCAPE);

	c = *v->now++;
	switch (c) {
	case CHR('{'):
		INTO(L_BBND);
		NOTE(REG_UBOUNDS);
		RET('{');
	case CHR('('):
		RETV('(', 1);
	case CHR(')'):
		RETV(')', c);
	case CHR('<'):
		NOTE(REG_UNONPOSIX);
		RET('<');
	case CHR('>'):
		NOTE(REG_UNONPOSIX);
		RET('>');
	case CHR('1'): case CHR('2'): case CHR('3'): case CHR('4'):
	case CHR('5'): case CHR('6'): case CHR('7'): case CHR('8'):
	case CHR('9'):
		NOTE(REG_UBACKREF);
		RETV(BACKREF, (chr) DIGITVAL(c));
	default:
		if (iswalnum(c)) {
			NOTE(REG_UBSALNUM);
			NOTE(REG_UUNSPEC);
		}
		RETV(PLAIN, c);
	}
}

/*
 - skip - skip white space and comments in expanded form
 ^ static VOID skip(struct vars *);
 */
static VOID
skip(v)
struct vars *v;
{
	chr *start = v->now;

	assert(v->cflags&REG_EXPANDED);

	for (;;) {
		while (!ATEOS() && iswspace(*v->now))
			v->now++;
		if (ATEOS() || *v->now != CHR('#'))
			break;				/* NOTE BREAK OUT */
		assert(NEXT1('#'));
		while (!ATEOS() && *v->now != CHR('\n'))
			v->now++;
		/* leave the newline to be picked up by the iswspace loop */
	}

	if (v->now != start)
		NOTE(REG_UNONPOSIX);
}

/*
 - newline - return the chr for a newline
 * This helps confine use of CHR to this source file.
 ^ static chr newline(VOID);
 */
static chr
newline()
{
	return CHR('\n');
}

/*
 - ch - return the chr sequence for locale.c's fake collating element ch
 * This helps confine use of CHR to this source file.
 ^ static chr *ch(VOID);
 */
static chr *
ch()
{
	static chr chstr[] = { CHR('c'), CHR('h'), CHR('\0') };

	return chstr;
}

/*
 - chrnamed - return the chr known by a given (chr string) name
 * The code is a bit clumsy, but this routine gets only such specialized
 * use that it hardly matters.
 ^ static chr chrnamed(struct vars *, chr *, pchr);
 */
static chr
chrnamed(v, name, lastresort)
struct vars *v;
chr *name;
pchr lastresort;		/* what to return if name lookup fails */
{
	celt c;
	int errsave;
	int e;
	struct cvec *cv;

	errsave = v->err;
	v->err = 0;
	c = element(v, name, name+wcslen(name));
	e = v->err;
	v->err = errsave;

	if (e != 0)
		return (chr) lastresort;

	cv = range(v, c, c, 0);
	if (cv->nchrs == 0)
		return (chr) lastresort;
	return cv->chrs[0];
}
