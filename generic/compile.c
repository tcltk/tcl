/*
 * compile.c --
 *
 *	Regexp package file:  re_*comp and friends - compile REs
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
 * SCCS: @(#) compile.c 1.12 98/02/11 17:25:30
 */

#include "tclInt.h"
#include <assert.h>
#include "tclPort.h"
#include "tclRegexp.h"
#include "chr.h"
#include "guts.h"

/*
 * forward declarations, up here so forward datatypes etc. are defined early
 */
/* =====^!^===== begin forwards =====^!^===== */
/* automatically gathered by fwd; do not hand-edit */
/* === compile.c === */
int compile _ANSI_ARGS_((regex_t *, CONST chr *, size_t, int));
static VOID moresubs _ANSI_ARGS_((struct vars *, int));
static int freev _ANSI_ARGS_((struct vars *, int));
static struct rtree *parse _ANSI_ARGS_((struct vars *, int, int, struct state *, struct state *, int));
static int scannum _ANSI_ARGS_((struct vars *));
static VOID repeat _ANSI_ARGS_((struct vars *, struct state *, struct state *, int, int));
static VOID bracket _ANSI_ARGS_((struct vars *, struct state *, struct state *));
static VOID cbracket _ANSI_ARGS_((struct vars *, struct state *, struct state *));
static VOID brackpart _ANSI_ARGS_((struct vars *, struct state *, struct state *));
static chr *scanplain _ANSI_ARGS_((struct vars *));
static VOID leaders _ANSI_ARGS_((struct vars *, struct cvec *));
static VOID onechr _ANSI_ARGS_((struct vars *, pchr, struct state *, struct state *));
static VOID dovec _ANSI_ARGS_((struct vars *, struct cvec *, struct state *, struct state *));
static color nlcolor _ANSI_ARGS_((struct vars *));
static VOID wordchrs _ANSI_ARGS_((struct vars *));
static struct subre subre _ANSI_ARGS_((struct state *, struct state *, int, int, struct rtree *));
static struct rtree *newrt _ANSI_ARGS_((struct vars *));
static VOID freert _ANSI_ARGS_((struct rtree *));
static VOID freertnode _ANSI_ARGS_((struct rtree *));
static VOID optrt _ANSI_ARGS_((struct vars *, struct rtree *));
static int numrt _ANSI_ARGS_((struct rtree *, int));
static VOID nfatree _ANSI_ARGS_((struct vars *, struct rtree *));
static VOID nfanode _ANSI_ARGS_((struct vars *, struct subre *));
static int newlacon _ANSI_ARGS_((struct vars *, struct state *, struct state *, int));
static VOID freelacons _ANSI_ARGS_((struct subre *, int));
static VOID rfree _ANSI_ARGS_((regex_t *));
static VOID dump _ANSI_ARGS_((regex_t *, FILE *));
static VOID dumprt _ANSI_ARGS_((struct rtree *, FILE *, int));
static VOID rtdump _ANSI_ARGS_((struct rtree *, FILE *, int, int));
/* === lex.c === */
static VOID lexstart _ANSI_ARGS_((struct vars *));
static VOID prefixes _ANSI_ARGS_((struct vars *));
static VOID lexnest _ANSI_ARGS_((struct vars *, chr *));
static VOID lexword _ANSI_ARGS_((struct vars *));
static int next _ANSI_ARGS_((struct vars *));
static int lexescape _ANSI_ARGS_((struct vars *));
static chr lexdigits _ANSI_ARGS_((struct vars *, int, int, int));
static int brenext _ANSI_ARGS_((struct vars *, pchr));
static VOID skip _ANSI_ARGS_((struct vars *));
static chr newline _ANSI_ARGS_((VOID));
static chr *ch _ANSI_ARGS_((VOID));
static chr chrnamed _ANSI_ARGS_((struct vars *, chr *, pchr));
/* === locale.c === */
#define	MAXCE	2	/* longest CE code is prepared to handle */
typedef wint_t celt;	/* type holding distinct codes for all chrs, all CEs */
static int nces _ANSI_ARGS_((struct vars *));
static int nleaders _ANSI_ARGS_((struct vars *));
static struct cvec *allces _ANSI_ARGS_((struct vars *, struct cvec *));
static celt element _ANSI_ARGS_((struct vars *, chr *, chr *));
static struct cvec *range _ANSI_ARGS_((struct vars *, celt, celt, int));
static int before _ANSI_ARGS_((celt, celt));
static struct cvec *eclass _ANSI_ARGS_((struct vars *, celt, int));
static struct cvec *cclass _ANSI_ARGS_((struct vars *, chr *, chr *, int));
static struct cvec *allcases _ANSI_ARGS_((struct vars *, pchr));
static int sncmp _ANSI_ARGS_((CONST chr *, CONST chr *, size_t));
static struct cvec *newcvec _ANSI_ARGS_((int, int));
static struct cvec *clearcvec _ANSI_ARGS_((struct cvec *));
static VOID addchr _ANSI_ARGS_((struct cvec *, pchr));
static VOID addce _ANSI_ARGS_((struct cvec *, chr *));
static int haschr _ANSI_ARGS_((struct cvec *, pchr));
static struct cvec *getcvec _ANSI_ARGS_((struct vars *, int, int));
static VOID freecvec _ANSI_ARGS_((struct cvec *));
/* === color.c === */
union tree;
static struct colormap *newcm _ANSI_ARGS_((struct vars *));
static VOID freecm _ANSI_ARGS_((struct colormap *));
static VOID cmtreefree _ANSI_ARGS_((struct colormap *, union tree *, int));
static VOID fillcm _ANSI_ARGS_((struct colormap *));
static VOID cmtreefill _ANSI_ARGS_((struct colormap *, union tree *, int));
static color getcolor _ANSI_ARGS_((struct colormap *, pchr));
static color setcolor _ANSI_ARGS_((struct colormap *, pchr, pcolor));
static color maxcolor _ANSI_ARGS_((struct colormap *));
static color newcolor _ANSI_ARGS_((struct colormap *));
static color pseudocolor _ANSI_ARGS_((struct colormap *));
static color subcolor _ANSI_ARGS_((struct colormap *, pchr c));
static VOID okcolors _ANSI_ARGS_((struct nfa *, struct colormap *));
static VOID colorchain _ANSI_ARGS_((struct colormap *, struct arc *));
static VOID uncolorchain _ANSI_ARGS_((struct colormap *, struct arc *));
static int singleton _ANSI_ARGS_((struct colormap *, pchr c));
static VOID rainbow _ANSI_ARGS_((struct nfa *, struct colormap *, int, pcolor, struct state *, struct state *));
static VOID colorcomplement _ANSI_ARGS_((struct nfa *, struct colormap *, int, struct state *, struct state *, struct state *));
/* === nfa.c === */
static struct nfa *newnfa _ANSI_ARGS_((struct vars *, struct nfa *));
static VOID freenfa _ANSI_ARGS_((struct nfa *));
static struct state *newfstate _ANSI_ARGS_((struct nfa *, int flag));
static struct state *newstate _ANSI_ARGS_((struct nfa *));
static VOID dropstate _ANSI_ARGS_((struct nfa *, struct state *));
static VOID freestate _ANSI_ARGS_((struct nfa *, struct state *));
static VOID destroystate _ANSI_ARGS_((struct nfa *, struct state *));
static VOID newarc _ANSI_ARGS_((struct nfa *, int, pcolor, struct state *, struct state *));
static struct arc *allocarc _ANSI_ARGS_((struct nfa *, struct state *));
static VOID freearc _ANSI_ARGS_((struct nfa *, struct arc *));
static struct arc *findarc _ANSI_ARGS_((struct state *, int, pcolor));
static VOID cparc _ANSI_ARGS_((struct nfa *, struct arc *, struct state *, struct state *));
static VOID moveins _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID copyins _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID moveouts _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID copyouts _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID cloneouts _ANSI_ARGS_((struct nfa *, struct state *, struct state *, struct state *, int));
static VOID delsub _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID deltraverse _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID dupnfa _ANSI_ARGS_((struct nfa *, struct state *, struct state *, struct state *, struct state *));
static VOID duptraverse _ANSI_ARGS_((struct nfa *, struct state *, struct state *));
static VOID cleartraverse _ANSI_ARGS_((struct nfa *, struct state *));
static VOID specialcolors _ANSI_ARGS_((struct nfa *));
static VOID optimize _ANSI_ARGS_((struct nfa *));
static VOID pullback _ANSI_ARGS_((struct nfa *));
static int pull _ANSI_ARGS_((struct nfa *, struct arc *));
static VOID pushfwd _ANSI_ARGS_((struct nfa *));
static int push _ANSI_ARGS_((struct nfa *, struct arc *));
#define	INCOMPATIBLE	1	/* destroys arc */
#define	SATISFIED	2	/* constraint satisfied */
#define	COMPATIBLE	3	/* compatible but not satisfied yet */
static int combine _ANSI_ARGS_((struct arc *, struct arc *));
static VOID fixempties _ANSI_ARGS_((struct nfa *));
static int unempty _ANSI_ARGS_((struct nfa *, struct arc *));
static VOID cleanup _ANSI_ARGS_((struct nfa *));
static VOID markreachable _ANSI_ARGS_((struct nfa *, struct state *, struct state *, struct state *));
static VOID markcanreach _ANSI_ARGS_((struct nfa *, struct state *, struct state *, struct state *));
static int analyze _ANSI_ARGS_((struct vars *, struct nfa *));
static int isempty _ANSI_ARGS_((struct state *, struct state *));
static VOID compact _ANSI_ARGS_((struct vars *, struct nfa *, struct cnfa *));
static VOID carcsort _ANSI_ARGS_((struct carc *, struct carc *));
static VOID freecnfa _ANSI_ARGS_((struct cnfa *, int));
static VOID dumpnfa _ANSI_ARGS_((struct nfa *, FILE *));
static VOID dumpcnfa _ANSI_ARGS_((struct cnfa *, FILE *));
/* automatically gathered by fwd; do not hand-edit */
/* =====^!^===== end forwards =====^!^===== */



/* internal variables, bundled for easy passing around */
struct vars {
	regex_t *re;
	chr *now;		/* scan pointer into string */
	chr *stop;		/* end of string */
	chr *savenow;		/* saved now and stop for "subroutine call" */
	chr *savestop;
	int err;		/* error code (0 if none) */
	int cflags;		/* copy of compile flags */
	int lasttype;		/* type of previous token */
	int nexttype;		/* type of next token */
	chr nextvalue;		/* value (if any) of next token */
	int lexcon;		/* lexical context type (see lex.c) */
	int nsubexp;		/* subexpression count */
	struct subre **subs;	/* subRE pointer vector */
	size_t nsubs;		/* length of vector */
	struct subre *sub10[10];	/* initial vector, enough for most */
	struct nfa *nfa;	/* the NFA */
	struct colormap *cm;	/* character color map */
	color nlcolor;		/* color of newline */
	struct state *wordchrs;	/* state in nfa holding word-char outarcs */
	struct rtree *tree;	/* subexpression tree */
	int ntree;		/* number of tree nodes */
	struct cvec *cv;	/* utility cvec */
	struct cvec *ces;	/* collating-element information */
#		define	ISCELEADER(v,c)	(v->ces != NULL && haschr(v->ces, (c)))
	struct state *cepbegin;	/* state in nfa, start of CE prototypes */
	struct state *cepend;	/* state in nfa, end of CE prototypes */
	struct subre *lacons;	/* lookahead-constraint vector */
	int nlacons;		/* size of lacons */
	int usedshorter;	/* used short-preferring quantifiers */
};

/* parsing macros; most know that `v' is the struct vars pointer */
#define	NEXT()	(next(v))		/* advance by one token */
#define	SEE(t)	(v->nexttype == (t))	/* is next token this? */
#define	EAT(t)	(SEE(t) && next(v))	/* if next is this, swallow it */
#define	VISERR(vv)	((vv)->err != 0)	/* have we seen an error yet? */
#define	ISERR()	VISERR(v)
#define	VERR(vv,e)	((vv)->nexttype = EOS, ((vv)->err) ? (vv)->err :\
							((vv)->err = (e)))
#define	ERR(e)	VERR(v, e)		/* record an error */
#define	NOERR()	{if (ISERR()) return;}	/* if error seen, return */
#define	NOERRN()	{if (ISERR()) goto end;}	/* NOERR with retval */
#define	INSIST(c, e)	((c) ? 0 : ERR(e))	/* if condition false, error */
#define	NOTE(b)	(v->re->re_info |= (b))		/* note visible condition */
#define	EMPTYARC(x, y)	newarc(v->nfa, EMPTY, 0, x, y)

/* token type codes, some also used as NFA arc types */
#define	EMPTY	'n'		/* no token present */
#define	EOS	'e'		/* end of string */
#define	PLAIN	'p'		/* ordinary character */
#define	DIGIT	'd'		/* digit (in bound) */
#define	BACKREF	'b'		/* back reference */
#define	COLLEL	'I'		/* start of [. */
#define	ECLASS	'E'		/* start of [= */
#define	CCLASS	'C'		/* start of [: */
#define	END	'X'		/* end of [. [= [: */
#define	RANGE	'R'		/* - within [] which might be range delim. */
#define	LACON	'L'		/* lookahead constraint subRE */
#define	AHEAD	'a'		/* color-lookahead arc */
#define	BEHIND	'r'		/* color-lookbehind arc */
#define	WBDRY	'w'		/* word boundary constraint */
#define	NWBDRY	'W'		/* non-word-boundary constraint */
#define	SBEGIN	'A'		/* beginning of string (even if not BOL) */
#define	SEND	'Z'		/* end of string (even if not EOL) */
#define	PREFER	'P'		/* length preference */

/* is an arc colored, and hence on a color chain? */
#define	COLORED(a)	((a)->type == PLAIN || (a)->type == AHEAD || \
							(a)->type == BEHIND)



/* static function list */
static struct fns functions = {
	rfree,			/* regfree insides */
};



/*
 - regfree - free an RE (actually, just overall coordination)
 */
VOID
regfree(re)
regex_t *re;
{
	if (re == NULL || re->re_magic != REMAGIC)
		return;		/* no way we can report it, really */

	/* free it, calling internal routine that knows details */
	(*((struct fns *)re->re_fns)->free)(re);

	re->re_magic = 0;
}

/*
 - compile - compile regular expression
 ^ int compile(regex_t *, CONST chr *, size_t, int);
 */
int
compile(re, string, len, flags)
regex_t *re;
CONST chr *string;
size_t len;
int flags;
{
	struct vars var;
	struct vars *v = &var;
	struct guts *g;
	int i;
#	define	CNOERR()	{ if (ISERR()) return freev(v, v->err); }

	if (re == NULL) {
	    return REG_INVARG;
	}
	
	/*
	 * Init re to known state, because we will try to free it if
	 * compilation fails.
	 */
	 
	re->re_magic = 0;

	/* sanity checks */
	if (string == NULL ||
		((flags&REG_EXTENDED) && (flags&REG_QUOTE)) ||
		(!(flags&REG_EXTENDED) && (flags&REG_ADVF))) {
	    return REG_INVARG;
	}

	/* initial setup (after which freev() is callable) */
	v->re = re;
	v->now = (chr *)string;
	v->stop = v->now + len;
	v->savenow = v->savestop = NULL;
	v->err = 0;
	v->cflags = flags;
	v->nsubexp = 0;
	v->subs = v->sub10;
	v->nsubs = 10;
	for (i = 0; (size_t) i < v->nsubs; i++)
		v->subs[i] = NULL;
	v->nfa = NULL;
	v->cm = NULL;
	v->nlcolor = COLORLESS;
	v->wordchrs = NULL;
	v->tree = NULL;
	v->cv = NULL;
	v->ces = NULL;
	v->lacons = NULL;
	v->nlacons = 0;
	re->re_info = 0;		/* bits get set during parse */
	re->re_guts = NULL;
	re->re_fns = NULL;

	/* more complex setup, malloced things */
	v->cm = newcm(v);		/* colormap must precede nfa... */
	CNOERR();
	v->nfa = newnfa(v, (struct nfa *)NULL);	/* ...newnfa() uses it */
	CNOERR();
	re->re_guts = ckalloc(sizeof(struct guts));
	if (re->re_guts == NULL)
		return freev(v, REG_ESPACE);
	g = (struct guts *)re->re_guts;
	ZAPCNFA(g->cnfa);
	g->tree = NULL;
	g->cm = NULL;
	g->lacons = NULL;
	g->nlacons = 0;
	v->cv = newcvec(100, 10);
	if (v->cv == NULL)
		return freev(v, REG_ESPACE);
	i = nces(v);
	if (i > 0) {
		v->ces = newcvec(nleaders(v), i);
		CNOERR();
		v->ces = allces(v, v->ces);
		leaders(v, v->ces);
	}
	CNOERR();

	/* parsing */
	lexstart(v);			/* also handles prefixes */
	if (SEE(EOS))			/* empty RE is illegal */
		return freev(v, REG_EMPTY);
	v->tree = parse(v, EOS, PLAIN, v->nfa->init, v->nfa->final, NONEYET);
	assert(SEE(EOS));		/* even if error; ISERR() => SEE(EOS) */
	CNOERR();

	/* finish setup of nfa and its subre tree */
	specialcolors(v->nfa);
	CNOERR();
	if (flags&REG_PROGRESS) {
		dumpnfa(v->nfa, stdout);
		dumprt(v->tree, stdout, 1);
	}
	v->usedshorter = 0;
	optrt(v, v->tree);
	if (v->tree != NULL)
		v->ntree = numrt(v->tree, 1);
	else
		v->ntree = 0;
	if (flags&REG_PROGRESS) {
		printf("-->\n");
		dumprt(v->tree, stdout, 1);
	}

	/* build compacted NFAs for tree, lacons, main nfa */
	nfatree(v, v->tree);
	if (flags&REG_PROGRESS) {
		printf("---->\n");
		dumprt(v->tree, stdout, 1);
	}
	CNOERR();
	assert(v->nlacons == 0 || v->lacons != NULL);
	for (i = 1; i < v->nlacons; i++)
		nfanode(v, &v->lacons[i]);
	CNOERR();
	optimize(v->nfa);		/* removes unreachable states */
	CNOERR();
	if (v->nfa->post->nins <= 0)	
		return freev(v, REG_IMPOSS);	/* end unreachable! */
	assert(v->nfa->pre->nouts > 0);
	compact(v, v->nfa, &g->cnfa);
	CNOERR();
	freenfa(v->nfa);
	v->nfa = NULL;

	/* fill color map */
	fillcm(v->cm);
	CNOERR();

	/* looks okay, package it up */
	re->re_magic = REMAGIC;
	re->re_nsub = v->nsubexp;
	/* re_info is already set */
	re->re_csize = sizeof(chr);
	re->re_guts = (VOID *)g;
	re->re_fns = (VOID *)&functions;
	v->re = NULL;
	g->magic = GUTSMAGIC;
	g->cflags = v->cflags;
	g->info = re->re_info;
	g->nsub = re->re_nsub;
	g->cm = v->cm;
	v->cm = NULL;
	g->tree = v->tree;
	v->tree = NULL;
	g->ntree = v->ntree;
	g->compare = (v->cflags&REG_ICASE) ? sncmp : wcsncmp;
	g->lacons = v->lacons;
	v->lacons = NULL;
	g->nlacons = v->nlacons;
	g->usedshorter = v->usedshorter;

	if (flags&REG_DUMP)
		dump(re, stdout);

	assert(v->err == 0);
	return freev(v, 0);
}

/*
 - moresubs - enlarge subRE vector
 ^ static VOID moresubs(struct vars *, int);
 */
static VOID
moresubs(v, wanted)
struct vars *v;
int wanted;			/* want enough room for this one */
{
	struct subre **p;
	size_t n;

	assert((size_t)wanted >= v->nsubs);
	n = (size_t)wanted * 3 / 2 + 1;
	if (v->subs == v->sub10) {
		p = (struct subre **)ckalloc(n * sizeof(struct subre *));
		if (p != NULL)
			memcpy((VOID *)p, (VOID *)v->subs,
					v->nsubs * sizeof(struct subre *));
	} else
		p = (struct subre **) ckrealloc((VOID *)v->subs,
			n * sizeof(struct subre *));
	if (p == NULL) {
		ERR(REG_ESPACE);
		return;
	}
	v->subs = p;
	for (p = &v->subs[v->nsubs]; v->nsubs < n; p++, v->nsubs++)
		*p = NULL;
	assert(v->nsubs == n);
	assert((size_t)wanted < v->nsubs);
}

/*
 - freev - free vars struct's substructures where necessary
 * Does optional error-number setting, and returns error code, to make
 * error code terser.
 ^ static int freev(struct vars *, int);
 */
static int
freev(v, err)
struct vars *v;
int err;
{
	if (v->re != NULL)
		rfree(v->re);
	if (v->subs != v->sub10)
		ckfree((char *)v->subs);
	if (v->nfa != NULL)
		freenfa(v->nfa);
	if (v->cm != NULL)
		freecm(v->cm);
	if (v->tree != NULL)
		freert(v->tree);
	if (v->cv != NULL)
		freecvec(v->cv);
	if (v->ces != NULL)
		freecvec(v->ces);
	if (v->lacons != NULL)
		freelacons(v->lacons, v->nlacons);
	ERR(err);

	return v->err;
}

/*
 - parse - parse an RE
 * Arguably this is too big and too complex and ought to be divided up.
 * However, the code is somewhat intertwined...
 ^ static struct rtree *parse(struct vars *, int, int, struct state *,
 ^ 	struct state *, int);
 */
static struct rtree *		/* NULL if no interesting substructure */
parse(v, stopper, type, init, final, pprefer)
struct vars *v;
int stopper;			/* EOS or ')' */
int type;			/* LACON (lookahead subRE) or PLAIN */
struct state *init;		/* initial state */
struct state *final;		/* final state */
int pprefer;			/* parent's short/long preference */
{
	struct state *left;	/* scaffolding for branch */
	struct state *right;
	struct state *lp;	/* scaffolding for current construct */
	struct state *rp;
	struct state *s;	/* temporaries for new states */
	struct state *s2;
#	define	ARCV(t, val)	newarc(v->nfa, t, val, lp, rp)
	int m, n;
	int emptybranch;	/* is there anything in this branch yet? */
	color co;
	struct rtree *branches;	/* top level */
	struct rtree *branch;	/* current branch */
	struct subre *now;	/* current subtree's top */
	struct subre sub;	/* communication variable */
	struct rtree *rt1;	/* temporaries */
	struct rtree *rt2;
	struct subre *t;	/* work pointer, top of interesting subtree */
	int firstbranch;	/* is this the first branch? */
	int capture;		/* any capturing parens within this? */
	int constraint;		/* is the current atom a constraint? */

	assert(stopper == ')' || stopper == EOS);

        branch = NULL;		/* lint. */
	rt1 = NULL;		/* lint. */
	
	capture = 0;
	branches = newrt(v);
	firstbranch = 1;
	NOERRN();
	do {
		/* a branch */
		emptybranch = 1;	/* tentatively */
		left = newstate(v->nfa);
		right = newstate(v->nfa);
		if (!firstbranch)
			rt1 = newrt(v);
#if 1
		if (ISERR()) {
		    freert(rt1);
		    freert(branches);	/* mem leak (CCS). */
		    return NULL;
		}
#else 
		NOERRN();
#endif
		EMPTYARC(init, left);
		EMPTYARC(right, final);
		lp = left;
		rp = right;
		if (firstbranch)
			branch = branches;
		else {
			branch->next = rt1;
			branch = rt1;
		}
		branch->op = '|';
		now = &branch->left;
		*now = subre(left, right, NONEYET, 0, (struct rtree *)NULL);
		firstbranch = 0;
		NOERRN();

		while (!SEE('|') && !SEE(stopper) && !SEE(EOS)) {
			/* initial bookkeeping */
			sub.begin = NULL;	/* no substructure seen yet */
			sub.subno = 0;
			sub.prefer = NONEYET;
			constraint = 0;
			if (emptybranch)	/* first of the branch */
				emptybranch = 0;
			else {			/* implicit concat operator */
				lp = newstate(v->nfa);
				NOERRN();
				moveins(v->nfa, rp, lp);
			}
			assert(lp->nouts == 0);	/* must string new code */
			assert(rp->nins == 0);	/*  between lp and rp */

			/* an atom... */
			switch (v->nexttype) {
			case '(':	/* value flags as capturing or non */
				m = (type == LACON) ? 0 : v->nextvalue;
				if (m) {
					v->nsubexp++;
					sub.subno = v->nsubexp;
					if ((size_t)sub.subno >= v->nsubs)
						moresubs(v, sub.subno);
					assert((size_t) sub.subno < v->nsubs);
				} else
					sub.subno = 0;
				NEXT();
				sub.begin = lp;	/* NB, substructure seen */
				sub.end = rp;
				/* use now->tree as temporary, so */
				/*  things get freed on error returns */
				assert(now->tree == NULL);
				now->tree = parse(v, ')', PLAIN, lp, rp,
								now->prefer);
				assert(SEE(')') || ISERR());
				NEXT();
				NOERRN();
				if (!m && now->tree == NULL) {
					/* actually no relevant substructure */
					sub.begin = NULL;
				}
				if (now->tree != NULL) {
					if (now->tree->op == '|')
						sub.prefer = LONGER;
					else
						sub.prefer =
							now->tree->left.prefer;
				}
				/* must postpone other processing until we */
				/*  know about any {0,0} quantifier */
				break;
			case BACKREF:	/* the Feature From The Black Lagoon */
				INSIST(type != LACON, REG_ESUBREG);
				INSIST(v->nextvalue < v->nsubs, REG_ESUBREG);
				INSIST(v->subs[v->nextvalue] != NULL,
								REG_ESUBREG);
				NOERRN();
				assert(v->nextvalue > 0);
				sub.subno = -v->nextvalue;
				sub.begin = lp;	/* NB, substructure seen */
				sub.end = rp;
				EMPTYARC(lp, rp);	/* temporarily */
				assert(now->tree == NULL);
				NEXT();
				break;
			case LACON:	/* lookahead constraint */
				m = v->nextvalue;	/* is positive? */
				NEXT();
				s = newstate(v->nfa);
				s2 = newstate(v->nfa);
				NOERRN();
				rt1 = parse(v, ')', LACON, s, s2, NONEYET);
				assert(SEE(')') || ISERR());
				NEXT();
				m = newlacon(v, s, s2, m);
				freert(rt1);
				NOERRN();
				ARCV(LACON, m);
				constraint = 1;
				break;
			case PREFER:	/* length preference */
				sub.prefer = (v->nextvalue) ? LONGER : SHORTER;
				NEXT();
				sub.begin = lp;	/* NB, substructure seen */
				sub.end = rp;
				/* use now->tree as temporary, so */
				/*  things get freed on error returns */
				assert(now->tree == NULL);
				now->tree = parse(v, ')', PLAIN, lp, rp,
								sub.prefer);
				assert(SEE(')') || ISERR());
				NEXT();
				NOERRN();
				if (now->prefer == NONEYET)
					now->prefer = sub.prefer;
				if (sub.prefer == now->prefer &&
							now->tree == NULL) {
					/* actually no relevant substructure */
					sub.begin = NULL;
				}
				break;
			case '[':
				if (v->nextvalue == 1)
					bracket(v, lp, rp);
				else
					cbracket(v, lp, rp);
				assert(SEE(']') || ISERR());
				NEXT();
				break;
			case '.':
				co = (color) ((v->cflags&REG_NLSTOP) 
					? nlcolor(v) 
					: COLORLESS);
				rainbow(v->nfa, v->cm, PLAIN, co, lp, rp);
				NEXT();
				break;
			case '^':
				ARCV('^', 1);
				if (v->cflags&REG_NLANCH)
					ARCV(BEHIND, nlcolor(v));
				NEXT();
				constraint = 1;
				break;
			case '$':
				ARCV('$', 1);
				if (v->cflags&REG_NLANCH)
					ARCV(AHEAD, nlcolor(v));
				NEXT();
				constraint = 1;
				break;
			case SBEGIN:
				ARCV('^', 1);	/* BOL */
				ARCV('^', 0);	/* or BOS */
				NEXT();
				constraint = 1;
				break;
			case SEND:
				ARCV('$', 1);	/* EOL */
				ARCV('$', 0);	/* or EOS */
				NEXT();
				constraint = 1;
				break;
			case '<':
				wordchrs(v);	/* does NEXT() */
				s = newstate(v->nfa);
				NOERRN();
				/* needs BOL, BOS, or nonword to left... */
				newarc(v->nfa, '^', 1, lp, s);
				newarc(v->nfa, '^', 0, lp, s);
				colorcomplement(v->nfa, v->cm, BEHIND,
							v->wordchrs, lp, s);
				/* ... and word to right */
				cloneouts(v->nfa, v->wordchrs, s, rp, AHEAD);
				/* (no need for special attention to \n) */
				constraint = 1;
				break;
			case '>':
				wordchrs(v);	/* does NEXT() */
				s = newstate(v->nfa);
				NOERRN();
				/* needs word to left... */
				cloneouts(v->nfa, v->wordchrs, lp, s, BEHIND);
				/* ... and EOL, EOS, or nonword to right */
				newarc(v->nfa, '$', 1, s, rp);
				newarc(v->nfa, '$', 0, s, rp);
				colorcomplement(v->nfa, v->cm, AHEAD,
							v->wordchrs, s, rp);
				/* (no need for special attention to \n) */
				constraint = 1;
				break;
			case WBDRY:
				wordchrs(v);	/* does NEXT() */
				s = newstate(v->nfa);
				NOERRN();
				/* needs BOL, BOS, or nonword to left... */
				newarc(v->nfa, '^', 1, lp, s);
				newarc(v->nfa, '^', 0, lp, s);
				colorcomplement(v->nfa, v->cm, BEHIND,
							v->wordchrs, lp, s);
				/* ... and word to right... */
				cloneouts(v->nfa, v->wordchrs, s, rp, AHEAD);
				/* ...or... */
				s = newstate(v->nfa);
				NOERRN();
				/* ...needs word to left... */
				cloneouts(v->nfa, v->wordchrs, lp, s, BEHIND);
				/* ... and EOL, EOS, or nonword to right */
				newarc(v->nfa, '$', 1, s, rp);
				newarc(v->nfa, '$', 0, s, rp);
				colorcomplement(v->nfa, v->cm, AHEAD,
							v->wordchrs, s, rp);
				/* (no need for special attention to \n) */
				constraint = 1;
				break;
			case NWBDRY:
				wordchrs(v);	/* does NEXT() */
				s = newstate(v->nfa);
				NOERRN();
				/* needs word to both left and right... */
				cloneouts(v->nfa, v->wordchrs, lp, s, BEHIND);
				cloneouts(v->nfa, v->wordchrs, s, rp, AHEAD);
				/* ...or... */
				s = newstate(v->nfa);
				NOERRN();
				/* ...BOL, BOS, or nonword to left... */
				newarc(v->nfa, '^', 1, lp, s);
				newarc(v->nfa, '^', 0, lp, s);
				colorcomplement(v->nfa, v->cm, BEHIND,
							v->wordchrs, lp, s);
				/* ... and EOL, EOS, or nonword to right */
				newarc(v->nfa, '$', 1, s, rp);
				newarc(v->nfa, '$', 0, s, rp);
				colorcomplement(v->nfa, v->cm, AHEAD,
							v->wordchrs, s, rp);
				/* (no need for special attention to \n) */
				constraint = 1;
				break;
			case ')':		/* unbalanced paren */
				if (!(v->cflags&REG_EXTENDED) ||
							(v->cflags&REG_ADVF)) {
				    ERR(REG_EPAREN);
				    goto end;
				}
				NOTE(REG_UPBOTCH);
				/* fallthrough into case PLAIN */
			case PLAIN:
				onechr(v, v->nextvalue, lp, rp);
				okcolors(v->nfa, v->cm);
				NOERRN();
				NEXT();
				break;
			case '*':
			case '+':
			case '?':
			case '{':
				ERR(REG_BADRPT);
				goto end;
			default:
				ERR(REG_ASSERT);
				goto end;
			}

			/* ...possibly followed by a quantifier */
			switch (v->nexttype) {
			case '*':
				m = 0;
				n = INFINITY;
				sub.prefer = (v->nextvalue) ? LONGER : SHORTER;
				NEXT();
				break;
			case '+':
				m = 1;
				n = INFINITY;
				sub.prefer = (v->nextvalue) ? LONGER : SHORTER;
				NEXT();
				break;
			case '?':
				m = 0;
				n = 1;
				sub.prefer = (v->nextvalue) ? LONGER : SHORTER;
				NEXT();
				break;
			case '{':
				NEXT();
				m = scannum(v);
				if (EAT(',')) {
					if (SEE(DIGIT))
						n = scannum(v);
					else
						n = INFINITY;
					if (m > n) {
						ERR(REG_BADBR);
						goto end;
					}
				} else
					n = m;
				if (!SEE('}')) {	/* gets errors too */
					ERR(REG_BADBR);
					goto end;
				}
				if (m != n)
					sub.prefer = (v->nextvalue) ? LONGER :
									SHORTER;
				NEXT();
				break;
			default:		/* no quantifier */
				m = n = 1;
				constraint = 0;
				break;
			}

			/* constraints may not be quantified */
			if (constraint) {
				ERR(REG_BADRPT);
				goto end;
			}

			/* annoying special case:  {0,0} cancels everything */
			if (m == 0 && n == 0 && sub.begin != NULL) {
				freert(now->tree);
				now->tree = NULL;
				sub.begin = NULL;	/* no substructure */
				sub.prefer = NONEYET;
				/* the repeat() below will do the rest */
			}

			/* if no substructure, aVOID hard part */
			if (now->prefer == NONEYET)
				now->prefer = sub.prefer;
			if (sub.begin == NULL && (sub.prefer == NONEYET ||
						sub.prefer == now->prefer)) {
				assert(sub.subno >= 0 || (m == 0 && n == 0));
				if (!(m == 1 && n == 1))
					repeat(v, lp, rp, m, n);
				continue;		/* NOTE CONTINUE */
			}

			/* hard part:  something messy seen */
			/* break subRE into pre, x{...}, post-to-be */
			capture = 1;		/* upper levels will care */
			rt1 = newrt(v);
			rt2 = newrt(v);
			s = newstate(v->nfa);	/* between x and post-to-be */
			NOERRN();
			moveins(v->nfa, rp, s);
			EMPTYARC(s, rp);
			rt1->op = ',';
			rt1->left = subre(now->begin, lp, now->prefer, 0,
							(struct rtree *)NULL);
			assert(now->end == rp);
			rt1->right = subre(lp, rp, sub.prefer, 0, rt2);
			rt2->op = ',';
			rt2->left = subre(lp, s, sub.prefer, 0, now->tree);
			rt2->right = subre(s, rp, NONEYET, 0,
							(struct rtree *)NULL);
			now->tree = rt1;
			now = &rt2->right;	/* future elaborations here */
			t = &rt2->left;		/* current activity here */

			/* if it's a backref, time to replicate the subNFA */
			if (sub.subno < 0) {
				assert(lp->nouts == 1);	/* just the EMPTY */
				delsub(v->nfa, lp, s);
				assert(v->subs[-sub.subno] != NULL);
				dupnfa(v->nfa, v->subs[-sub.subno]->begin,
					v->subs[-sub.subno]->end, lp, s);
				NOERRN();
			}

			/* if no/vacuous quantifier and not backref, done */
			if (m == 1 && n == 1 && sub.subno >= 0) {
				t->subno = sub.subno;
				if (sub.subno > 0)
					v->subs[sub.subno] = t;
				continue;		/* NOTE CONTINUE */
			}

			/* really sticky part, quantified capturer/backref */
			/* first, turn x{0,...} into x{1,...}| */
			if (m == 0) {
				s = newstate(v->nfa);
				s2 = newstate(v->nfa);
				rt1 = newrt(v);
				rt2 = newrt(v);
				NOERRN();
				moveouts(v->nfa, t->begin, s);
				EMPTYARC(t->begin, s);
				EMPTYARC(t->begin, s2);
				EMPTYARC(s2, t->end);
				rt1->op = rt2->op = '|';
				rt1->left = subre(s, t->end, sub.prefer, 0,
								t->tree);
				rt1->next = rt2;
				rt2->left = subre(s2, t->end, sub.prefer, 0,
							(struct rtree *)NULL);
				t->tree = rt1;
				t = &rt1->left;
				m = 1;
			}

			/* second, x{1,1} is just x */
			if (m == 1 && n == 1 && sub.subno >= 0) {
				t->subno = sub.subno;
				if (sub.subno > 0)
					v->subs[sub.subno] = t;
				continue;		/* NOTE CONTINUE */
			}

			/* backrefs get special treatment */
			if (sub.subno < 0) {
				repeat(v, t->begin, t->end, m, n);
				rt1 = newrt(v);
				NOERRN();
				assert(t->tree == NULL);
				t->tree = rt1;
				rt1->op = 'b';
				rt1->left.subno = sub.subno;
				rt1->left.min = (short) m;
				rt1->left.max = (short) n;
				rt1->left.prefer = sub.prefer;
				continue;		/* NOTE CONTINUE */
			}

			/* turn x{m,n} into x{m-1,n-1}x, with capturing */
			/*  parens in only second x */
			s = newstate(v->nfa);
			NOERRN();
			moveouts(v->nfa, t->begin, s);
			dupnfa(v->nfa, s, t->end, t->begin, s);
			assert(m >= 1 && m != INFINITY && n >= 1);
			repeat(v, t->begin, s, m-1, (n == INFINITY) ? n : n-1);
			rt1 = newrt(v);
			NOERRN();
			rt1->op = ',';
			rt1->left = subre(t->begin, s, sub.prefer, 0,
							(struct rtree *)NULL);
			/* sub.prefer not really right, but doesn't matter */
			rt1->right = subre(s, t->end, sub.prefer, sub.subno,
								t->tree);
			if (sub.subno > 0)
				v->subs[sub.subno] = &rt1->right;
			t->tree = rt1;
		}
		if (emptybranch) {
			NOTE(REG_UUNSPEC);
			EMPTYARC(lp, rp);
		}
	} while (EAT('|'));
	assert(SEE(stopper) || SEE(EOS));

	if (!SEE(stopper)) {
		assert(stopper == ')' && SEE(EOS));
		ERR(REG_EPAREN);
	}

	/* higher levels care about our preference in certain situations */
	if (branch != branches) {	/* >1 branch */
		if (pprefer != LONGER)
			capture = 1;
	} else if (branches->left.prefer != pprefer)
		capture = 1;

	/* optimize out vacuous alternation */
	if (branch == branches) {
		assert(branch->next == NULL && branch->right.begin == NULL);
		assert(branch->left.subno == 0);
		if (capture && branch->left.tree == NULL)
			branch->op = ',';
		else {
			branches = branch->left.tree;	/* might be NULL */
			freertnode(branch);
		}
	}

	if (capture)			/* actually a catchall flag */
		return branches;
	end:				/* mem leak (CCS) */
	freert(branches);
	return NULL;
}

/*
 - scannum - scan a number
 ^ static int scannum(struct vars *);
 */
static int			/* value, <= DUPMAX */
scannum(v)
struct vars *v;
{
	int n = 0;

	while (SEE(DIGIT) && n < DUPMAX) {
		n = n*10 + v->nextvalue;
		NEXT();
	}
	if (SEE(DIGIT) || n > DUPMAX) {
		ERR(REG_BADBR);
		return 0;
	}
	return n;
}

/*
 - repeat - replicate subNFA for quantifiers
 * The duplication sequences used here are chosen carefully so that any
 * pointers starting out pointing into the subexpression end up pointing into
 * the last occurrence.  (Note that it may not be strung between the same
 * left and right end states, however!)  This used to be important for the
 * subRE tree, although the important bits are now handled by the in-line
 * code in parse(), and when this is called, it doesn't matter any more.
 ^ static VOID repeat(struct vars *, struct state *, struct state *, int, int);
 */
static VOID
repeat(v, lp, rp, m, n)
struct vars *v;
struct state *lp;
struct state *rp;
int m;
int n;
{
#	define	SOME	2
#	define	INF	3
#	define	PAIR(x, y)	((x)*4 + (y))
#	define	REDUCE(x)	( ((x) == INFINITY) ? INF : (((x) > 1) ? SOME : (x)) )
	CONST int rm = REDUCE(m);
	CONST int rn = REDUCE(n);
	struct state *s;
	struct state *s2;

	switch (PAIR(rm, rn)) {
	case PAIR(0, 0):		/* empty string */
		delsub(v->nfa, lp, rp);
		EMPTYARC(lp, rp);
		break;
	case PAIR(0, 1):		/* do as x| */
		EMPTYARC(lp, rp);
		break;
	case PAIR(0, SOME):		/* do as x{1,n}| */
		repeat(v, lp, rp, 1, n);
		NOERR();
		EMPTYARC(lp, rp);
		break;
	case PAIR(0, INF):		/* loop x around */
		s = newstate(v->nfa);
		NOERR();
		moveouts(v->nfa, lp, s);
		moveins(v->nfa, rp, s);
		EMPTYARC(lp, s);
		EMPTYARC(s, rp);
		break;
	case PAIR(1, 1):		/* no action required */
		break;
	case PAIR(1, SOME):		/* do as x{0,n-1}x = (x{1,n-1}|)x */
		s = newstate(v->nfa);
		NOERR();
		moveouts(v->nfa, lp, s);
		dupnfa(v->nfa, s, rp, lp, s);
		NOERR();
		repeat(v, lp, s, 1, n-1);
		NOERR();
		EMPTYARC(lp, s);
		break;
	case PAIR(1, INF):		/* add loopback arc */
		s = newstate(v->nfa);
		s2 = newstate(v->nfa);
		NOERR();
		moveouts(v->nfa, lp, s);
		moveins(v->nfa, rp, s2);
		EMPTYARC(lp, s);
		EMPTYARC(s2, rp);
		EMPTYARC(s2, s);
		break;
	case PAIR(SOME, SOME):		/* do as x{m-1,n-1}x */
		s = newstate(v->nfa);
		NOERR();
		moveouts(v->nfa, lp, s);
		dupnfa(v->nfa, s, rp, lp, s);
		NOERR();
		repeat(v, lp, s, m-1, n-1);
		break;
	case PAIR(SOME, INF):		/* do as x{m-1,}x */
		s = newstate(v->nfa);
		NOERR();
		moveouts(v->nfa, lp, s);
		dupnfa(v->nfa, s, rp, lp, s);
		NOERR();
		repeat(v, lp, s, m-1, n);
		break;
	default:
		ERR(REG_ASSERT);
		break;
	}
}

/*
 - bracket - handle non-complemented bracket expression
 * Also called from cbracket for complemented bracket expressions.
 ^ static VOID bracket(struct vars *, struct state *, struct state *);
 */
static VOID
bracket(v, lp, rp)
struct vars *v;
struct state *lp;
struct state *rp;
{
	assert(SEE('['));
	NEXT();
	while (!SEE(']') && !SEE(EOS))
		brackpart(v, lp, rp);
	assert(SEE(']') || ISERR());
	okcolors(v->nfa, v->cm);
}

/*
 - cbracket - handle complemented bracket expression
 * We do it by calling bracket() with dummy endpoints, and then complementing
 * the result.  The alternative would be to invoke rainbow(), and then delete
 * arcs as the b.e. is seen... but that gets messy.
 ^ static VOID cbracket(struct vars *, struct state *, struct state *);
 */
static VOID
cbracket(v, lp, rp)
struct vars *v;
struct state *lp;
struct state *rp;
{
	struct state *left = newstate(v->nfa);
	struct state *right = newstate(v->nfa);
	struct state *s;
	struct arc *a;			/* arc from lp */
	struct arc *ba;			/* arc from left, from bracket() */
	struct arc *pa;			/* CE-prototype arc */
	color co;
	chr *p;
	int i;

	NOERR();
	bracket(v, left, right);
	if (v->cflags&REG_NLSTOP)
		newarc(v->nfa, PLAIN, nlcolor(v), left, right);
	NOERR();

	assert(lp->nouts == 0);		/* all outarcs will be ours */

	/* easy part of complementing */
	colorcomplement(v->nfa, v->cm, PLAIN, left, lp, rp);
	NOERR();
	if (v->ces == NULL) {		/* no CEs -- we're done */
		dropstate(v->nfa, left);
		assert(right->nins == 0);
		freestate(v->nfa, right);
		return;
	}

	/* but complementing gets messy in the presence of CEs... */
	NOTE(REG_ULOCALE);
	for (p = v->ces->chrs, i = v->ces->nchrs; i > 0; p++, i--) {
		co = getcolor(v->cm, *p);
		a = findarc(lp, PLAIN, co);
		ba = findarc(left, PLAIN, co);
		if (ba == NULL) {
			assert(a != NULL);
			freearc(v->nfa, a);
		} else {
			assert(a == NULL);
		}
		s = newstate(v->nfa);
		NOERR();
		newarc(v->nfa, PLAIN, co, lp, s);
		NOERR();
		pa = findarc(v->cepbegin, PLAIN, co);
		assert(pa != NULL);
		if (ba == NULL) {	/* easy case, need all of them */
			cloneouts(v->nfa, pa->to, s, rp, PLAIN);
			newarc(v->nfa, '$', 1, s, rp);
			newarc(v->nfa, '$', 0, s, rp);
			colorcomplement(v->nfa, v->cm, AHEAD, pa->to, s, rp);
		} else {		/* must be selective */
			if (findarc(ba->to, '$', 1) == NULL) {
				newarc(v->nfa, '$', 1, s, rp);
				newarc(v->nfa, '$', 0, s, rp);
				colorcomplement(v->nfa, v->cm, AHEAD, pa->to,
									 s, rp);
			}
			for (pa = pa->to->outs; pa != NULL; pa = pa->outchain)
				if (findarc(ba->to, PLAIN, pa->co) == NULL)
					newarc(v->nfa, PLAIN, pa->co, s, rp);
			if (s->nouts == 0)	/* limit of selectivity: none */
				dropstate(v->nfa, s);	/* frees arc too */
		}
		NOERR();
	}

	delsub(v->nfa, left, right);
	assert(left->nouts == 0);
	freestate(v->nfa, left);
	assert(right->nins == 0);
	freestate(v->nfa, right);
}
			
/*
 - brackpart - handle one item (or range) within a bracket expression
 ^ static VOID brackpart(struct vars *, struct state *, struct state *);
 */
static VOID
brackpart(v, lp, rp)
struct vars *v;
struct state *lp;
struct state *rp;
{
	celt startc;
	celt endc;
	struct cvec *cv;
	chr *startp;
	chr *endp;
	chr c[1];

	/* parse something, get rid of special cases, take shortcuts */
	switch (v->nexttype) {
	case RANGE:			/* a-b-c or other botch */
		ERR(REG_ERANGE);
		return;
	case PLAIN:
		c[0] = v->nextvalue;
		NEXT();
		/* shortcut for ordinary chr (not range, not CE leader) */
		if (!SEE(RANGE) && !ISCELEADER(v, c[0])) {
			onechr(v, c[0], lp, rp);
			return;
		}
		startc = element(v, c, c+1);
		NOERR();
		break;
	case COLLEL:
		startp = v->now;
		endp = scanplain(v);
		INSIST(startp < endp, REG_ECOLLATE);
		NOERR();
		startc = element(v, startp, endp);
		NOERR();
		break;
	case ECLASS:
		startp = v->now;
		endp = scanplain(v);
		INSIST(startp < endp, REG_ECOLLATE);
		NOERR();
		startc = element(v, startp, endp);
		NOERR();
		cv = eclass(v, startc, (v->cflags&REG_ICASE));
		NOERR();
		dovec(v, cv, lp, rp);
		return;
	case CCLASS:
		startp = v->now;
		endp = scanplain(v);
		INSIST(startp < endp, REG_ECTYPE);
		NOERR();
		cv = cclass(v, startp, endp, (v->cflags&REG_ICASE));
		NOERR();
		dovec(v, cv, lp, rp);
		return;
	default:
		ERR(REG_ASSERT);
		return;
	}

	if (SEE(RANGE)) {
		NEXT();
		switch (v->nexttype) {
		case PLAIN:
		case RANGE:
			c[0] = v->nextvalue;
			NEXT();
			endc = element(v, c, c+1);
			NOERR();
			break;
		case COLLEL:
			startp = v->now;
			endp = scanplain(v);
			INSIST(startp < endp, REG_ECOLLATE);
			NOERR();
			endc = element(v, startp, endp);
			NOERR();
			break;
		default:
			ERR(REG_ERANGE);
			return;
		}
	} else
		endc = startc;

	/*
	 * Ranges are unportable.  Actually, standard C does
	 * guarantee that digits are contiguous, but making
	 * that an exception is just too complicated.
	 */
	if (startc != endc)
		NOTE(REG_UUNPORT);
	cv = range(v, startc, endc, (v->cflags&REG_ICASE));
	NOERR();
	dovec(v, cv, lp, rp);
}

/*
 - scanplain - scan PLAIN contents of [. etc.
 * Certain bits of trickery in lex.c know that this code does not try
 * to look past the final bracket of the [. etc.
 ^ static chr *scanplain(struct vars *);
 */
static chr *			/* just after end of sequence */
scanplain(v)
struct vars *v;
{
	chr *endp;

	assert(SEE(COLLEL) || SEE(ECLASS) || SEE(CCLASS));
	NEXT();

	endp = v->now;
	while (SEE(PLAIN)) {
		endp = v->now;
		NEXT();
	}

	assert(SEE(END) || ISERR());
	NEXT();

	return endp;
}

/*
 - leaders - process a cvec of collating elements to also include leaders
 * Also gives all characters involved their own colors, which is almost
 * certainly necessary, and sets up little disconnected subNFA.
 ^ static VOID leaders(struct vars *, struct cvec *);
 */
static VOID
leaders(v, cv)
struct vars *v;
struct cvec *cv;
{
	int ce;
	chr *p;
	chr leader;
	struct state *s;
	struct arc *a;

	v->cepbegin = newstate(v->nfa);
	v->cepend = newstate(v->nfa);
	NOERR();

	for (ce = 0; ce < cv->nces; ce++) {
		p = cv->ces[ce];
		leader = *p;
		if (!haschr(cv, leader)) {
			addchr(cv, leader);
			s = newstate(v->nfa);
			newarc(v->nfa, PLAIN, subcolor(v->cm, leader),
								v->cepbegin, s);
			okcolors(v->nfa, v->cm);
		} else {
			a = findarc(v->cepbegin, PLAIN,
						getcolor(v->cm, leader));
			assert(a != NULL);
			s = a->to;
			assert(s != v->cepend);
		}
		p++;
		assert(*p != 0 && *(p+1) == 0);	/* only 2-char CEs at present */
		newarc(v->nfa, PLAIN, subcolor(v->cm, *p), s, v->cepend);
		okcolors(v->nfa, v->cm);
	}
}

/*
 - onechr - fill in arcs for a plain character, and possible case complements
 * This is mostly a shortcut for efficient handling of the common case.
 ^ static VOID onechr(struct vars *, pchr, struct state *, struct state *);
 */
static VOID
onechr(v, c, lp, rp)
struct vars *v;
pchr c;
struct state *lp;
struct state *rp;
{
	if (!(v->cflags&REG_ICASE)) {
		newarc(v->nfa, PLAIN, subcolor(v->cm, c), lp, rp);
		return;
	}

	/* rats, need general case anyway... */
	dovec(v, allcases(v, c), lp, rp);
}

/*
 - dovec - fill in arcs for each element of a cvec
 * This one has to handle the messy cases, like CEs and CE leaders.
 ^ static VOID dovec(struct vars *, struct cvec *, struct state *,
 ^ 	struct state *);
 */
static VOID
dovec(v, cv, lp, rp)
struct vars *v;
struct cvec *cv;
struct state *lp;
struct state *rp;
{
	chr *p;
	chr *np;
	int i;
	color co;
	struct arc *a;
	struct arc *pa;		/* arc in prototype */
	struct state *s;
	struct state *ps;	/* state in prototype */

	/* first, get the ordinary characters out of the way */
	np = cv->chrs;
	for (p = np, i = cv->nchrs; i > 0; p++, i--)
		if (!ISCELEADER(v, *p)) {
			newarc(v->nfa, PLAIN, subcolor(v->cm, *p), lp, rp);
			*p = 0;
		} else {
			assert(singleton(v->cm, *p));
			*np++ = *p;
		}
	cv->nchrs = np - cv->chrs;	/* only CE leaders remain */
	if (cv->nchrs == 0 && cv->nces == 0)
		return;

	/* deal with the CE leaders */
	NOTE(REG_ULOCALE);
	for (p = cv->chrs, i = cv->nchrs; i > 0; p++, i--) {
		co = getcolor(v->cm, *p);
		a = findarc(lp, PLAIN, co);
		if (a != NULL)
			s = a->to;
		else {
			s = newstate(v->nfa);
			NOERR();
			newarc(v->nfa, PLAIN, co, lp, s);
			NOERR();
		}
		pa = findarc(v->cepbegin, PLAIN, co);
		assert(pa != NULL);
		ps = pa->to;
		newarc(v->nfa, '$', 1, s, rp);
		newarc(v->nfa, '$', 0, s, rp);
		colorcomplement(v->nfa, v->cm, AHEAD, ps, s, rp);
		NOERR();
	}

	/* and the CEs */
	for (i = 0; i < cv->nces; i++) {
		p = cv->ces[i];
		assert(singleton(v->cm, *p));
		co = getcolor(v->cm, *p++);
		a = findarc(lp, PLAIN, co);
		if (a != NULL)
			s = a->to;
		else {
			s = newstate(v->nfa);
			NOERR();
			newarc(v->nfa, PLAIN, co, lp, s);
			NOERR();
		}
		assert(*p != 0);	/* at least two chars */
		assert(singleton(v->cm, *p));
		co = getcolor(v->cm, *p++);
		assert(*p == 0);	/* and only two, for now */
		newarc(v->nfa, PLAIN, co, s, rp);
		NOERR();
	}
}

/*
 - nlcolor - assign newline a unique color, if it doesn't have one already
 * Restriction:  can't be called when there are subcolors open.  (Maybe
 * this should be enforced...)
 ^ static color nlcolor(struct vars *);
 */
static color
nlcolor(v)
struct vars *v;
{
	if (v->nlcolor == COLORLESS) {
		v->nlcolor = subcolor(v->cm, newline());
		okcolors(v->nfa, v->cm);
	}
	return v->nlcolor;
}

/*
 - wordchrs - set up word-chr list for word-boundary stuff, if needed
 * The list is kept as a bunch of arcs between two dummy states; it's
 * disposed of by the unreachable-states sweep in NFA optimization.
 * Does NEXT().  Must not be called from any unusual lexical context.
 * This should be reconciled with the \w etc. handling in lex.c, and
 * should be cleaned up to reduce dependencies on input scanning.
 ^ static VOID wordchrs(struct vars *);
 */
static VOID
wordchrs(v)
struct vars *v;
{
	struct state *left;
	struct state *right;

	if (v->wordchrs != NULL) {
		NEXT();		/* for consistency */
		return;
	}

	left = newstate(v->nfa);
	right = newstate(v->nfa);
	NOERR();
	lexword(v);
	NEXT();
	assert(v->savenow != NULL && SEE('['));
	bracket(v, left, right);
	assert(((v->savenow != NULL) && SEE(']')) || ISERR());
	NEXT();
	NOERR();
	v->wordchrs = left;
}

/*
 - subre - construct a subre struct
 ^ static struct subre subre(struct state *, struct state *, int, int,
 ^ 	struct rtree *);
 */
static struct subre
subre(begin, end, prefer, subno, tree)
struct state *begin;
struct state *end;
int prefer;
int subno;
struct rtree *tree;
{
	struct subre ret;

	ret.begin = begin;
	ret.end = end;
	ret.prefer = prefer;
	ret.subno = subno;
	ret.min = ret.max = 1;
	ret.tree = tree;
	ZAPCNFA(ret.cnfa);
	return ret;
}

/*
 - newrt - allocate subRE-tree node
 ^ static struct rtree *newrt(struct vars *);
 */
static struct rtree *
newrt(v)
struct vars *v;
{
	struct rtree *rt = (struct rtree *)ckalloc(sizeof(struct rtree));

	if (rt == NULL) {
		ERR(REG_ESPACE);
		return NULL;
	}

	rt->op = '?';		/* invalid */
	rt->no = 0;
	rt->left.begin = NULL;
	rt->left.end = NULL;
	rt->left.prefer = NONEYET;
	rt->left.subno = 0;
	rt->left.min = rt->left.max = 1;
	rt->left.tree = NULL;
	ZAPCNFA(rt->left.cnfa);
	rt->right.begin = NULL;
	rt->right.end = NULL;
	rt->right.prefer = NONEYET;
	rt->right.subno = 0;
	rt->right.min = rt->right.max = 1;
	rt->right.tree = NULL;
	ZAPCNFA(rt->right.cnfa);
	rt->next = NULL;
	return rt;
}

/*
 - freert - free a subRE subtree
 ^ static VOID freert(struct rtree *);
 */
static VOID
freert(rt)
struct rtree *rt;
{
	if (rt == NULL)
		return;

	if (rt->left.tree != NULL)
		freert(rt->left.tree);
	if (rt->right.tree != NULL)
		freert(rt->right.tree);
	if (rt->next != NULL)
		freert(rt->next);

	freertnode(rt);
}

/*
 - freertnode - free one node in a subRE subtree
 ^ static VOID freertnode(struct rtree *);
 */
static VOID
freertnode(rt)
struct rtree *rt;
{
	if (rt == NULL)
		return;

	if (!NULLCNFA(rt->left.cnfa))
		freecnfa(&rt->left.cnfa, 0);
	if (!NULLCNFA(rt->right.cnfa))
		freecnfa(&rt->right.cnfa, 0);

	ckfree((char *)rt);
}

/*
 - optrt - optimize a subRE subtree
 ^ static VOID optrt(struct vars *, struct rtree *);
 */
static VOID
optrt(v, rt)
struct vars *v;
struct rtree *rt;
{
	struct rtree *t;
	int subno;

	if (rt == NULL)
		return;
	assert(rt->op != 'b');

	/* pull up subtrees if possible */
	if (rt->left.begin != NULL && rt->left.tree != NULL &&
						rt->left.tree->op != 'b') {
		t = rt->left.tree;
		optrt(v, t);
		if (t->right.begin == NULL && t->next == NULL &&
				(rt->left.prefer == NONEYET ||
					t->left.prefer == rt->left.prefer) &&
				(rt->left.subno == 0 || t->left.subno == 0)) {
			subno = rt->left.subno;
			rt->left = t->left;
			assert(NULLCNFA(t->left.cnfa));
			freertnode(t);
			if (subno != 0) {
				assert(rt->left.subno == 0 && subno > 0);
				rt->left.subno = subno;
			}
		}
	}
	if (rt->right.begin != NULL && rt->right.tree != NULL &&
						rt->right.tree->op != 'b') {
		t = rt->right.tree;
		optrt(v, t);
		if (t->right.begin == NULL && t->next == NULL &&
				(rt->right.prefer == NONEYET ||
					t->right.prefer == rt->right.prefer) &&
				(rt->right.subno == 0 || t->right.subno == 0)) {
			subno = rt->right.subno;
			rt->right = t->left;
			assert(NULLCNFA(t->right.cnfa));
			freertnode(t);
			if (subno != 0) {
				assert(rt->right.subno == 0 && subno > 0);
				rt->right.subno = subno;
			}
		}
	}

	/* simplify empties */
	if (rt->left.begin != NULL && isempty(rt->left.begin, rt->left.end))
		rt->left.end = rt->left.begin;
	if (rt->right.begin != NULL && isempty(rt->right.begin, rt->right.end))
		rt->right.end = rt->right.begin;

	/* if left subtree vacuous and right non-empty, move right over */
	if (rt->left.begin != NULL && rt->left.begin == rt->left.end &&
				rt->left.subno == 0 && rt->left.tree == NULL &&
				rt->right.begin != NULL) {
		rt->left = rt->right;
		rt->right.begin = NULL;
		rt->right.tree = NULL;
	}

	/* if right subtree vacuous, clear it out */
	if (rt->right.begin != NULL && rt->right.begin == rt->right.end &&
			rt->right.subno == 0 && rt->right.tree == NULL) {
		rt->right.begin = NULL;
		rt->right.tree = NULL;
	}

	/* preference cleanup and analysis */
	if (rt->left.prefer == NONEYET)
		rt->left.prefer = LONGER;
	if (rt->left.prefer == SHORTER)
		v->usedshorter = 1;
	if (rt->right.begin != NULL) {
		if (rt->right.prefer == NONEYET)
			rt->right.prefer = LONGER;
		if (rt->right.prefer == SHORTER)
			v->usedshorter = 1;
	}

	/* recurse through alternatives */
	if (rt->next != NULL)
		optrt(v, rt->next);
}

/*
 - numrt - number tree nodes
 ^ static int numrt(struct rtree *, int);
 */
static int			/* next number */
numrt(rt, start)
struct rtree *rt;
int start;			/* starting point for subtree numbers */
{
	int i;

	assert(rt != NULL);

	i = start;
	rt->no = (short) i++;
	if (rt->left.tree != NULL)
		i = numrt(rt->left.tree, i);
	if (rt->right.tree != NULL)
		i = numrt(rt->right.tree, i);
	if (rt->next != NULL)
		i = numrt(rt->next, i);
	return i;
}

/*
 - nfatree - turn a subRE subtree into a tree of compacted NFAs
 ^ static VOID nfatree(struct vars *, struct rtree *);
 */
static VOID
nfatree(v, rt)
struct vars *v;
struct rtree *rt;
{
	if (rt == NULL)
		return;

	if (rt->left.begin != NULL)
		nfanode(v, &rt->left);
	if (rt->left.tree != NULL)
		nfatree(v, rt->left.tree);

	if (rt->right.begin != NULL)
		nfanode(v, &rt->right);
	if (rt->right.tree != NULL)
		nfatree(v, rt->right.tree);

	if (rt->next != NULL)
		nfatree(v, rt->next);
}

/*
 - nfanode - do one NFA for nfatree
 ^ static VOID nfanode(struct vars *, struct subre *);
 */
static VOID
nfanode(v, sub)
struct vars *v;
struct subre *sub;
{
	struct nfa *nfa;

	if (sub->begin == NULL)
		return;

	nfa = newnfa(v, v->nfa);
	NOERR();
	dupnfa(nfa, sub->begin, sub->end, nfa->init, nfa->final);
	if (!ISERR()) {
		specialcolors(nfa);
		optimize(nfa);
	}
	if (!ISERR())
		compact(v, nfa, &sub->cnfa);
	freenfa(nfa);
}

/*
 - newlacon - allocate a lookahead-constraint subRE
 ^ static int newlacon(struct vars *, struct state *, struct state *, int);
 */
static int			/* lacon number */
newlacon(v, begin, end, pos)
struct vars *v;
struct state *begin;
struct state *end;
int pos;
{
	int n;
	struct subre *sub;

	if (v->nlacons == 0) {
		v->lacons = (struct subre *)ckalloc(2 * sizeof(struct subre));
		n = 1;		/* skip 0th */
		v->nlacons = 2;
	} else {
		v->lacons = (struct subre *)ckrealloc((VOID *) v->lacons,
					(v->nlacons+1)*sizeof(struct subre));
		n = v->nlacons++;
	}
	if (v->lacons == NULL) {
		ERR(REG_ESPACE);
		return 0;
	}
	sub = &v->lacons[n];
	sub->begin = begin;
	sub->end = end;
	sub->subno = pos;
	ZAPCNFA(sub->cnfa);
	return n;
}

/*
 - freelacons - free lookahead-constraint subRE vector
 ^ static VOID freelacons(struct subre *, int);
 */
static VOID
freelacons(subs, n)
struct subre *subs;
int n;
{
	struct subre *sub;
	int i;

	for (sub = subs + 1, i = n - 1; i > 0; sub++, i--)
		if (!NULLCNFA(sub->cnfa))
			freecnfa(&sub->cnfa, 0);
	ckfree((char *)subs);
}

/*
 - rfree - free a whole RE (insides of regfree)
 ^ static VOID rfree(regex_t *);
 */
static VOID
rfree(re)
regex_t *re;			/* regfree has validated it */
{
	struct guts *g = (struct guts *)re->re_guts;

	re->re_magic = 0;	/* invalidate it */
	re->re_guts = NULL;
	re->re_fns = NULL;
	g->magic = 0;
	if (!NULLCNFA(g->cnfa))
		freecnfa(&g->cnfa, 0);
	if (g->cm != NULL)
		freecm(g->cm);
	if (g->tree != NULL)
		freert(g->tree);
	if (g->lacons != NULL)
		freelacons(g->lacons, g->nlacons);
	ckfree((char *)g);
}

/*
 - dumprt - dump a subRE tree
 ^ static VOID dumprt(struct rtree *, FILE *, int);
 */
static VOID
dumprt(rt, f, nfapresent)
struct rtree *rt;
FILE *f;
int nfapresent;			/* is the original NFA still around? */
{
	if (rt == NULL)
		fprintf(f, "null tree\n");
	else
		rtdump(rt, f, nfapresent, 0);
	fflush(f);
}

/*
 - rtdump - recursive guts of dumprt
 ^ static VOID rtdump(struct rtree *, FILE *, int, int);
 */
static VOID
rtdump(rt, f, nfapresent, level)
struct rtree *rt;
FILE *f;
int nfapresent;			/* is the original NFA still around? */
int level;
{
	int i;
#	define	RTSEP	"  "

	for (i = 0; i < level; i++)
		fprintf(f, RTSEP);
	fprintf(f, "%c (n%d) {\n", rt->op, rt->no);
	if (rt->left.begin != NULL) {
		for (i = 0; i < level+1; i++)
			fprintf(f, RTSEP);
		fprintf(f, "L");
		fprintf(f, "%s", (rt->left.prefer == NONEYET) ? "-" :
				((rt->left.prefer == LONGER) ? ">" : "<"));
		if (nfapresent)
			fprintf(f, "%ld-%ld", (long)rt->left.begin->no,
							(long)rt->left.end->no);
		if (rt->left.subno > 0)
			fprintf(f, " (%d)", rt->left.subno);
		else if (rt->left.subno < 0) {
			fprintf(f, " \\%d", -rt->left.subno);
			if (rt->left.min != 1 || rt->left.max != 1) {
				fprintf(f, "{%d-", (int)rt->left.min);
				if (rt->left.max != INFINITY)
					fprintf(f, "%d", (int)rt->left.max);
				fprintf(f, "}");
			}
			if (rt->left.tree != NULL)
				fprintf(f, "(nonNULL tree!!)");
		}
		if (rt->left.tree != NULL || !NULLCNFA(rt->left.cnfa))
			fprintf(f, ":");
		fprintf(f, "\n");
		if (!NULLCNFA(rt->left.cnfa))
			dumpcnfa(&rt->left.cnfa, f);
		if (rt->left.tree != NULL)
			rtdump(rt->left.tree, f, nfapresent, level+1);
	} else if (rt->op == 'b') {
		for (i = 0; i < level+1; i++)
			fprintf(f, RTSEP);
		fprintf(f, "L");
		fprintf(f, "%s", (rt->left.prefer == NONEYET) ? "-" :
				((rt->left.prefer == LONGER) ? ">" : "<"));
		assert(rt->left.subno < 0);
		fprintf(f, " \\%d", -rt->left.subno);
		if (rt->left.min != 1 || rt->left.max != 1) {
			fprintf(f, "{%d-", (int)rt->left.min);
			if (rt->left.max != INFINITY)
				fprintf(f, "%d", (int)rt->left.max);
			fprintf(f, "}");
		}
		if (rt->left.tree != NULL)
			fprintf(f, "(nonNULL tree!!)");
		fprintf(f, "\n");
	}

	if (rt->right.begin != NULL) {
		if (rt->op != ',')
			fprintf(f, "op %c has non-NULL right tree\n", rt->op);
		for (i = 0; i < level+1; i++)
			fprintf(f, RTSEP);
		fprintf(f, "R");
		fprintf(f, "%s", (rt->right.prefer == NONEYET) ? "-" :
				((rt->right.prefer == LONGER) ? ">" : "<"));
		if (nfapresent)
			fprintf(f, "%ld-%ld", (long)rt->right.begin->no,
						(long)rt->right.end->no);
		if (rt->right.subno > 0)
			fprintf(f, " (%d)", rt->right.subno);
		else if (rt->right.subno < 0) {
			fprintf(f, " \\%d", -rt->right.subno);
			if (rt->right.min != 1 || rt->right.max != 1) {
				fprintf(f, "{%d-", (int)rt->right.min);
				if (rt->right.max != INFINITY)
					fprintf(f, "%d", (int)rt->right.max);
				fprintf(f, "}");
			}
			if (rt->right.tree != NULL)
				fprintf(f, "(nonNULL tree!!)");
		}
		if (rt->right.tree != NULL || !NULLCNFA(rt->right.cnfa))
			fprintf(f, ":");
		fprintf(f, "\n");
		if (!NULLCNFA(rt->right.cnfa))
			dumpcnfa(&rt->right.cnfa, f);
		if (rt->right.tree != NULL)
			rtdump(rt->right.tree, f, nfapresent, level+1);
	}
	for (i = 0; i < level; i++)
		fprintf(f, RTSEP);
	fprintf(f, "}\n");

	if (rt->next != NULL) {
		if (rt->op != '|')
			fprintf(f, "op %c has non-NULL next\n", rt->op);
		if (rt->next->op != rt->op)
			fprintf(f, "next op %c, expecting %c\n", rt->next->op,
									rt->op);
		rtdump(rt->next, f, nfapresent, level);
	}
}

/*
 - dump - dump an RE in human-readable form
 ^ static VOID dump(regex_t *, FILE *);
 */
static VOID
dump(re, f)
regex_t *re;
FILE *f;
{
}

#undef NOERRN
#define	NOERRN()	{if (ISERR()) return NULL;}	/* NOERR with retval */

#define	COMPILE	1
#include "lex.c"
#include "color.c"
#include "locale.c"
#include "nfa.c"
