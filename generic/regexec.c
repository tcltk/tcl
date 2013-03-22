/*
 * re_*exec and friends - match REs
 *
 * Copyright (c) 1998, 1999 Henry Spencer.  All rights reserved.
 *
 * Development of this software was funded, in part, by Cray Research Inc.,
 * UUNET Communications Services Inc., Sun Microsystems Inc., and Scriptics
 * Corporation, none of whom are responsible for the results.  The author
 * thanks all of them.
 *
 * Redistribution and use in source and binary forms -- with or without
 * modification -- are permitted for any purpose, provided that
 * redistributions in source form retain this entire copyright notice and
 * indicate the origin and nature of any modifications.
 *
 * I'd appreciate being given credit for this package in the documentation of
 * software which uses it, but that is not a requirement.
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
 */

#include "regguts.h"

/*
 * Lazy-DFA representation.
 */

struct arcp {			/* "pointer" to an outarc */
    struct sset *ss;
    color co;
};

struct sset {			/* state set */
    unsigned *states;		/* pointer to bitvector */
    unsigned hash;		/* hash of bitvector */
#define	HASH(bv, nw)	(((nw) == 1) ? *(bv) : hash(bv, nw))
#define	HIT(h,bv,ss,nw)	((ss)->hash == (h) && ((nw) == 1 || \
	memcmp((void*)(bv), (void*)((ss)->states), (nw)*sizeof(unsigned)) == 0))
    int flags;
#define	STARTER		01	/* the initial state set */
#define	POSTSTATE	02	/* includes the goal state */
#define	LOCKED		04	/* locked in cache */
#define	NOPROGRESS	010	/* zero-progress state set */
    struct arcp ins;		/* chain of inarcs pointing here */
    chr *lastseen;		/* last entered on arrival here */
    struct sset **outs;		/* outarc vector indexed by color */
    struct arcp *inchain;	/* chain-pointer vector for outarcs */
};

struct dfa {
    int nssets;			/* size of cache */
    int nssused;		/* how many entries occupied yet */
    int nstates;		/* number of states */
    int ncolors;		/* length of outarc and inchain vectors */
    int wordsper;		/* length of state-set bitvectors */
    struct sset *ssets;		/* state-set cache */
    unsigned *statesarea;	/* bitvector storage */
    unsigned *work;		/* pointer to work area within statesarea */
    struct sset **outsarea;	/* outarc-vector storage */
    struct arcp *incarea;	/* inchain storage */
    struct cnfa *cnfa;
    struct colormap *cm;
    chr *lastpost;		/* location of last cache-flushed success */
    chr *lastnopr;		/* location of last cache-flushed NOPROGRESS */
    struct sset *search;	/* replacement-search-pointer memory */
    int cptsmalloced;		/* were the areas individually malloced? */
    char *mallocarea;		/* self, or master malloced area, or NULL */
};

#define	WORK	1		/* number of work bitvectors needed */

/*
 * Setup for non-malloc allocation for small cases.
 */

#define	FEWSTATES	20	/* must be less than UBITS */
#define	FEWCOLORS	15
struct smalldfa {
    struct dfa dfa;
    struct sset ssets[FEWSTATES*2];
    unsigned statesarea[FEWSTATES*2 + WORK];
    struct sset *outsarea[FEWSTATES*2 * FEWCOLORS];
    struct arcp incarea[FEWSTATES*2 * FEWCOLORS];
};
#define	DOMALLOC	((struct smalldfa *)NULL)	/* force malloc */

/*
 * Internal variables, bundled for easy passing around.
 */

struct vars {
    regex_t *re;
    struct guts *g;
    int eflags;			/* copies of arguments */
    size_t nmatch;
    regmatch_t *pmatch;
    rm_detail_t *details;
    chr *start;			/* start of string */
    chr *stop;			/* just past end of string */
    int err;			/* error code if any (0 none) */
    regoff_t *mem;		/* memory vector for backtracking */
    struct smalldfa dfa1;
    struct smalldfa dfa2;
};
#define	VISERR(vv) ((vv)->err != 0)	/* have we seen an error yet? */
#define	ISERR()	VISERR(v)
#define	VERR(vv,e) (((vv)->err) ? (vv)->err : ((vv)->err = (e)))
#define	ERR(e)	VERR(v, e)	/* record an error */
#define	NOERR()	{if (ISERR()) return v->err;}	/* if error seen, return it */
#define	OFF(p)	((p) - v->start)
#define	LOFF(p)	((long)OFF(p))

/*
 * forward declarations
 */
/* =====^!^===== begin forwards =====^!^===== */
/* automatically gathered by fwd; do not hand-edit */
/* === regexec.c === */
int exec(regex_t *, const chr *, size_t, rm_detail_t *, size_t, regmatch_t [], int);
static int simpleFind(struct vars *const, struct cnfa *const, struct colormap *const);
static int complicatedFind(struct vars *const, struct cnfa *const, struct colormap *const);
static int complicatedFindLoop(struct vars *const, struct cnfa *const, struct colormap *const, struct dfa *const, struct dfa *const, chr **const);
static void zapSubexpressions(regmatch_t *const, const size_t);
static void zapSubtree(struct vars *const, struct subre *const);
static void subset(struct vars *const, struct subre *const, chr *const, chr *const);
static int dissect(struct vars *const, struct subre *, chr *const, chr *const);
static int concatenationDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int alternationDissect(struct vars *const, struct subre *, chr *const, chr *const);
static inline int complicatedDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int complicatedCapturingDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int complicatedConcatenationDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int complicatedReversedDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int complicatedBackrefDissect(struct vars *const, struct subre *const, chr *const, chr *const);
static int complicatedAlternationDissect(struct vars *const, struct subre *, chr *const, chr *const);
/* === rege_dfa.c === */
static chr *longest(struct vars *const, struct dfa *const, chr *const, chr *const, int *const);
static chr *shortest(struct vars *const, struct dfa *const, chr *const, chr *const, chr *const, chr **const, int *const);
static chr *lastCold(struct vars *const, struct dfa *const);
static struct dfa *newDFA(struct vars *const, struct cnfa *const, struct colormap *const, struct smalldfa *);
static void freeDFA(struct dfa *const);
static unsigned hash(unsigned *const, const int);
static struct sset *initialize(struct vars *const, struct dfa *const, chr *const);
static struct sset *miss(struct vars *const, struct dfa *const, struct sset *const, const pcolor, chr *const, chr *const);
static int checkLAConstraint(struct vars *const, struct cnfa *const, chr *const, const pcolor);
static struct sset *getVacantSS(struct vars *const, struct dfa *const, chr *const, chr *const);
static struct sset *pickNextSS(struct vars *const, struct dfa *const, chr *const, chr *const);
/* automatically gathered by fwd; do not hand-edit */
/* =====^!^===== end forwards =====^!^===== */

/*
 - exec - match regular expression
 ^ int exec(regex_t *, const chr *, size_t, rm_detail_t *,
 ^					size_t, regmatch_t [], int);
 */
int
exec(
    regex_t *re,
    const chr *string,
    size_t len,
    rm_detail_t *details,
    size_t nmatch,
    regmatch_t pmatch[],
    int flags)
{
    AllocVars(v);
    int st, backref;
    size_t n;
#define	LOCALMAT	20
    regmatch_t mat[LOCALMAT];
#define	LOCALMEM	40
    regoff_t mem[LOCALMEM];

    /*
     * Sanity checks.
     */

    if (re == NULL || string == NULL || re->re_magic != REMAGIC) {
	FreeVars(v);
	return REG_INVARG;
    }
    if (re->re_csize != sizeof(chr)) {
	FreeVars(v);
	return REG_MIXED;
    }

    /*
     * Setup.
     */

    v->re = re;
    v->g = (struct guts *)re->re_guts;
    if ((v->g->cflags&REG_EXPECT) && details == NULL) {
	FreeVars(v);
	return REG_INVARG;
    }
    if (v->g->info&REG_UIMPOSSIBLE) {
	FreeVars(v);
	return REG_NOMATCH;
    }
    backref = (v->g->info&REG_UBACKREF) ? 1 : 0;
    v->eflags = flags;
    if (v->g->cflags&REG_NOSUB) {
	nmatch = 0;		/* override client */
    }
    v->nmatch = nmatch;
    if (backref) {
	/*
	 * Need work area.
	 */

	if (v->g->nsub + 1 <= LOCALMAT) {
	    v->pmatch = mat;
	} else {
	    v->pmatch = (regmatch_t *)
		    MALLOC((v->g->nsub + 1) * sizeof(regmatch_t));
	}
	if (v->pmatch == NULL) {
	    FreeVars(v);
	    return REG_ESPACE;
	}
	v->nmatch = v->g->nsub + 1;
    } else {
	v->pmatch = pmatch;
    }
    v->details = details;
    v->start = (chr *)string;
    v->stop = (chr *)string + len;
    v->err = 0;
    if (backref) {
	/*
	 * Need retry memory.
	 */

	assert(v->g->ntree >= 0);
	n = (size_t)v->g->ntree;
	if (n <= LOCALMEM) {
	    v->mem = mem;
	} else {
	    v->mem = (regoff_t *) MALLOC(n*sizeof(regoff_t));
	}
	if (v->mem == NULL) {
	    if (v->pmatch != pmatch && v->pmatch != mat) {
		FREE(v->pmatch);
	    }
	    FreeVars(v);
	    return REG_ESPACE;
	}
    } else {
	v->mem = NULL;
    }

    /*
     * Do it.
     */

    assert(v->g->tree != NULL);
    if (backref) {
	st = complicatedFind(v, &v->g->tree->cnfa, &v->g->cmap);
    } else {
	st = simpleFind(v, &v->g->tree->cnfa, &v->g->cmap);
    }

    /*
     * Copy (portion of) match vector over if necessary.
     */

    if (st == REG_OKAY && v->pmatch != pmatch && nmatch > 0) {
	zapSubexpressions(pmatch, nmatch);
	n = (nmatch < v->nmatch) ? nmatch : v->nmatch;
	memcpy((void*)(pmatch), (void*)(v->pmatch), n*sizeof(regmatch_t));
    }

    /*
     * Clean up.
     */

    if (v->pmatch != pmatch && v->pmatch != mat) {
	FREE(v->pmatch);
    }
    if (v->mem != NULL && v->mem != mem) {
	FREE(v->mem);
    }
    FreeVars(v);
    return st;
}

/*
 - simpleFind - find a match for the main NFA (no-complications case)
 ^ static int simpleFind(struct vars *, struct cnfa *, struct colormap *);
 */
static int
simpleFind(
    struct vars *const v,
    struct cnfa *const cnfa,
    struct colormap *const cm)
{
    struct dfa *s, *d;
    chr *begin, *end = NULL;
    chr *cold;
    chr *open, *close;		/* Open and close of range of possible
				 * starts */
    int hitend;
    int shorter = (v->g->tree->flags&SHORTER) ? 1 : 0;

    /*
     * First, a shot with the search RE.
     */

    s = newDFA(v, &v->g->search, cm, &v->dfa1);
    assert(!(ISERR() && s != NULL));
    NOERR();
    MDEBUG(("\nsearch at %ld\n", LOFF(v->start)));
    cold = NULL;
    close = shortest(v, s, v->start, v->start, v->stop, &cold, NULL);
    freeDFA(s);
    NOERR();
    if (v->g->cflags&REG_EXPECT) {
	assert(v->details != NULL);
	if (cold != NULL) {
	    v->details->rm_extend.rm_so = OFF(cold);
	} else {
	    v->details->rm_extend.rm_so = OFF(v->stop);
	}
	v->details->rm_extend.rm_eo = OFF(v->stop);	/* unknown */
    }
    if (close == NULL) {	/* not found */
	return REG_NOMATCH;
    }
    if (v->nmatch == 0) {	/* found, don't need exact location */
	return REG_OKAY;
    }

    /*
     * Find starting point and match.
     */

    assert(cold != NULL);
    open = cold;
    cold = NULL;
    MDEBUG(("between %ld and %ld\n", LOFF(open), LOFF(close)));
    d = newDFA(v, cnfa, cm, &v->dfa1);
    assert(!(ISERR() && d != NULL));
    NOERR();
    for (begin = open; begin <= close; begin++) {
	MDEBUG(("\nfind trying at %ld\n", LOFF(begin)));
	if (shorter) {
	    end = shortest(v, d, begin, begin, v->stop, NULL, &hitend);
	} else {
	    end = longest(v, d, begin, v->stop, &hitend);
	}
	NOERR();
	if (hitend && cold == NULL) {
	    cold = begin;
	}
	if (end != NULL) {
	    break;		/* NOTE BREAK OUT */
	}
    }
    assert(end != NULL);	/* search RE succeeded so loop should */
    freeDFA(d);

    /*
     * And pin down details.
     */

    assert(v->nmatch > 0);
    v->pmatch[0].rm_so = OFF(begin);
    v->pmatch[0].rm_eo = OFF(end);
    if (v->g->cflags&REG_EXPECT) {
	if (cold != NULL) {
	    v->details->rm_extend.rm_so = OFF(cold);
	} else {
	    v->details->rm_extend.rm_so = OFF(v->stop);
	}
	v->details->rm_extend.rm_eo = OFF(v->stop);	/* unknown */
    }
    if (v->nmatch == 1) {	/* no need for submatches */
	return REG_OKAY;
    }

    /*
     * Submatches.
     */

    zapSubexpressions(v->pmatch, v->nmatch);
    return dissect(v, v->g->tree, begin, end);
}

/*
 - complicatedFind - find a match for the main NFA (with complications)
 ^ static int complicatedFind(struct vars *, struct cnfa *, struct colormap *);
 */
static int
complicatedFind(
    struct vars *const v,
    struct cnfa *const cnfa,
    struct colormap *const cm)
{
    struct dfa *s, *d;
    chr *cold = NULL; /* silence gcc 4 warning */
    int ret;

    s = newDFA(v, &v->g->search, cm, &v->dfa1);
    NOERR();
    d = newDFA(v, cnfa, cm, &v->dfa2);
    if (ISERR()) {
	assert(d == NULL);
	freeDFA(s);
	return v->err;
    }

    ret = complicatedFindLoop(v, cnfa, cm, d, s, &cold);

    freeDFA(d);
    freeDFA(s);
    NOERR();
    if (v->g->cflags&REG_EXPECT) {
	assert(v->details != NULL);
	if (cold != NULL) {
	    v->details->rm_extend.rm_so = OFF(cold);
	} else {
	    v->details->rm_extend.rm_so = OFF(v->stop);
	}
	v->details->rm_extend.rm_eo = OFF(v->stop);	/* unknown */
    }
    return ret;
}

/*
 - complicatedFindLoop - the heart of complicatedFind
 ^ static int complicatedFindLoop(struct vars *, struct cnfa *, struct colormap *,
 ^	struct dfa *, struct dfa *, chr **);
 */
static int
complicatedFindLoop(
    struct vars *const v,
    struct cnfa *const cnfa,
    struct colormap *const cm,
    struct dfa *const d,
    struct dfa *const s,
    chr **const coldp)		/* where to put coldstart pointer */
{
    chr *begin, *end;
    chr *cold;
    chr *open, *close;		/* Open and close of range of possible
				 * starts */
    chr *estart, *estop;
    int er, hitend;
    int shorter = v->g->tree->flags&SHORTER;

    assert(d != NULL && s != NULL);
    cold = NULL;
    close = v->start;
    do {
	MDEBUG(("\ncsearch at %ld\n", LOFF(close)));
	close = shortest(v, s, close, close, v->stop, &cold, NULL);
	if (close == NULL) {
	    break;		/* NOTE BREAK */
	}
	assert(cold != NULL);
	open = cold;
	cold = NULL;
	MDEBUG(("cbetween %ld and %ld\n", LOFF(open), LOFF(close)));
	for (begin = open; begin <= close; begin++) {
	    MDEBUG(("\ncomplicatedFind trying at %ld\n", LOFF(begin)));
	    estart = begin;
	    estop = v->stop;
	    for (;;) {
		if (shorter) {
		    end = shortest(v, d, begin, estart, estop, NULL, &hitend);
		} else {
		    end = longest(v, d, begin, estop, &hitend);
		}
		if (hitend && cold == NULL) {
		    cold = begin;
		}
		if (end == NULL) {
		    break;	/* NOTE BREAK OUT */
		}

		MDEBUG(("tentative end %ld\n", LOFF(end)));
		zapSubexpressions(v->pmatch, v->nmatch);
		zapSubtree(v, v->g->tree);
		er = complicatedDissect(v, v->g->tree, begin, end);
		if (er == REG_OKAY) {
		    if (v->nmatch > 0) {
			v->pmatch[0].rm_so = OFF(begin);
			v->pmatch[0].rm_eo = OFF(end);
		    }
		    *coldp = cold;
		    return REG_OKAY;
		}
		if (er != REG_NOMATCH) {
		    ERR(er);
		    return er;
		}
		if ((shorter) ? end == estop : end == begin) {
		    /*
		     * No point in trying again.
		     */

		    *coldp = cold;
		    return REG_NOMATCH;
		}

		/*
		 * Go around and try again
		 */

		if (shorter) {
		    estart = end + 1;
		} else {
		    estop = end - 1;
		}
	    }
	}
    } while (close < v->stop);

    *coldp = cold;
    return REG_NOMATCH;
}

/*
 - zapSubexpressions - initialize the subexpression matches to "no match"
 ^ static void zapSubexpressions(regmatch_t *, size_t);
 */
static void
zapSubexpressions(
    regmatch_t *const p,
    const size_t n)
{
    size_t i;

    for (i = n-1; i > 0; i--) {
	p[i].rm_so = -1;
	p[i].rm_eo = -1;
    }
}

/*
 - zapSubtree - initialize the retry memory of a subtree to zeros
 ^ static void zapSubtree(struct vars *, struct subre *);
 */
static void
zapSubtree(
    struct vars *const v,
    struct subre *const t)
{
    if (t == NULL) {
	return;
    }

    assert(v->mem != NULL);
    v->mem[t->retry] = 0;
    if (t->op == '(') {
	assert(t->subno > 0);
	v->pmatch[t->subno].rm_so = -1;
	v->pmatch[t->subno].rm_eo = -1;
    }

    if (t->left != NULL) {
	zapSubtree(v, t->left);
    }
    if (t->right != NULL) {
	zapSubtree(v, t->right);
    }
}

/*
 - subset - set any subexpression relevant to a successful subre
 ^ static void subset(struct vars *, struct subre *, chr *, chr *);
 */
static void
subset(
    struct vars *const v,
    struct subre *const sub,
    chr *const begin,
    chr *const end)
{
    int n = sub->subno;

    assert(n > 0);
    if ((size_t)n >= v->nmatch) {
	return;
    }

    MDEBUG(("setting %d\n", n));
    v->pmatch[n].rm_so = OFF(begin);
    v->pmatch[n].rm_eo = OFF(end);
}

/*
 - dissect - determine subexpression matches (uncomplicated case)
 ^ static int dissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
dissect(
    struct vars *const v,
    struct subre *t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
#ifndef COMPILER_DOES_TAILCALL_OPTIMIZATION
  restart:
#endif
    assert(t != NULL);
    MDEBUG(("dissect %ld-%ld\n", LOFF(begin), LOFF(end)));

    switch (t->op) {
    case '=':			/* terminal node */
	assert(t->left == NULL && t->right == NULL);
	return REG_OKAY;	/* no action, parent did the work */
    case '|':			/* alternation */
	assert(t->left != NULL);
	return alternationDissect(v, t, begin, end);
    case 'b':			/* back ref -- shouldn't be calling us! */
	return REG_ASSERT;
    case '.':			/* concatenation */
	assert(t->left != NULL && t->right != NULL);
	return concatenationDissect(v, t, begin, end);
    case '(':			/* capturing */
	assert(t->left != NULL && t->right == NULL);
	assert(t->subno > 0);
	subset(v, t, begin, end);
#ifndef COMPILER_DOES_TAILCALL_OPTIMIZATION
	t = t->left;
	goto restart;
#else
	return dissect(v, t->left, begin, end);
#endif
    default:
	return REG_ASSERT;
    }
}

/*
 - concatenationDissect - determine concatenation subexpression matches
 - (uncomplicated)
 ^ static int concatenationDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
concatenationDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    struct dfa *d, *d2;
    chr *mid;
    int i;
    int shorter = (t->left->flags&SHORTER) ? 1 : 0;
    chr *stop = (shorter) ? end : begin;

    assert(t->op == '.');
    assert(t->left != NULL && t->left->cnfa.nstates > 0);
    assert(t->right != NULL && t->right->cnfa.nstates > 0);

    d = newDFA(v, &t->left->cnfa, &v->g->cmap, &v->dfa1);
    NOERR();
    d2 = newDFA(v, &t->right->cnfa, &v->g->cmap, &v->dfa2);
    if (ISERR()) {
	assert(d2 == NULL);
	freeDFA(d);
	return v->err;
    }

    /*
     * Pick a tentative midpoint.
     */

    if (shorter) {
	mid = shortest(v, d, begin, begin, end, NULL, NULL);
    } else {
	mid = longest(v, d, begin, end, NULL);
    }
    if (mid == NULL) {
	freeDFA(d);
	freeDFA(d2);
	return REG_ASSERT;
    }
    MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));

    /*
     * Iterate until satisfaction or failure.
     */

    while (longest(v, d2, mid, end, NULL) != end) {
	/*
	 * That midpoint didn't work, find a new one.
	 */

	if (mid == stop) {
	    /*
	     * All possibilities exhausted!
	     */

	    MDEBUG(("no midpoint!\n"));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_ASSERT;
	}
	if (shorter) {
	    mid = shortest(v, d, begin, mid+1, end, NULL, NULL);
	} else {
	    mid = longest(v, d, begin, mid-1, NULL);
	}
	if (mid == NULL) {
	    /*
	     * Failed to find a new one!
	     */

	    MDEBUG(("failed midpoint!\n"));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_ASSERT;
	}
	MDEBUG(("new midpoint %ld\n", LOFF(mid)));
    }

    /*
     * Satisfaction.
     */

    MDEBUG(("successful\n"));
    freeDFA(d);
    freeDFA(d2);
    i = dissect(v, t->left, begin, mid);
    if (i != REG_OKAY) {
	return i;
    }
    return dissect(v, t->right, mid, end);
}

/*
 - alternationDissect - determine alternative subexpression matches (uncomplicated)
 ^ static int alternationDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
alternationDissect(
    struct vars *const v,
    struct subre *t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    int i;

    assert(t != NULL);
    assert(t->op == '|');

    for (i = 0; t != NULL; t = t->right, i++) {
	struct dfa *d;

	MDEBUG(("trying %dth\n", i));
	assert(t->left != NULL && t->left->cnfa.nstates > 0);
	d = newDFA(v, &t->left->cnfa, &v->g->cmap, &v->dfa1);
	if (ISERR()) {
	    return v->err;
	}
	if (longest(v, d, begin, end, NULL) == end) {
	    MDEBUG(("success\n"));
	    freeDFA(d);
	    return dissect(v, t->left, begin, end);
	}
	freeDFA(d);
    }
    return REG_ASSERT;		/* none of them matched?!? */
}

/*
 - complicatedDissect - determine subexpression matches (with complications)
 * The retry memory stores the offset of the trial midpoint from begin, plus 1
 * so that 0 uniquely means "clean slate".
 ^ static int complicatedDissect(struct vars *, struct subre *, chr *, chr *);
 */
static inline int		/* regexec return code */
complicatedDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    assert(t != NULL);
    MDEBUG(("complicatedDissect %ld-%ld %c\n", LOFF(begin), LOFF(end), t->op));

    switch (t->op) {
    case '=':			/* terminal node */
	assert(t->left == NULL && t->right == NULL);
	return REG_OKAY;	/* no action, parent did the work */
    case '|':			/* alternation */
	assert(t->left != NULL);
	return complicatedAlternationDissect(v, t, begin, end);
    case 'b':			/* back ref -- shouldn't be calling us! */
	assert(t->left == NULL && t->right == NULL);
	return complicatedBackrefDissect(v, t, begin, end);
    case '.':			/* concatenation */
	assert(t->left != NULL && t->right != NULL);
	return complicatedConcatenationDissect(v, t, begin, end);
    case '(':			/* capturing */
	assert(t->left != NULL && t->right == NULL);
	assert(t->subno > 0);
	return complicatedCapturingDissect(v, t, begin, end);
    default:
	return REG_ASSERT;
    }
}

static int			/* regexec return code */
complicatedCapturingDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    int er = complicatedDissect(v, t->left, begin, end);

    if (er == REG_OKAY) {
	subset(v, t, begin, end);
    }
    return er;
}

/*
 - complicatedConcatenationDissect - concatenation subexpression matches (with complications)
 * The retry memory stores the offset of the trial midpoint from begin, plus 1
 * so that 0 uniquely means "clean slate".
 ^ static int complicatedConcatenationDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
complicatedConcatenationDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    struct dfa *d, *d2;
    chr *mid;

    assert(t->op == '.');
    assert(t->left != NULL && t->left->cnfa.nstates > 0);
    assert(t->right != NULL && t->right->cnfa.nstates > 0);

    if (t->left->flags&SHORTER) { /* reverse scan */
	return complicatedReversedDissect(v, t, begin, end);
    }

    d = newDFA(v, &t->left->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR()) {
	return v->err;
    }
    d2 = newDFA(v, &t->right->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR()) {
	freeDFA(d);
	return v->err;
    }
    MDEBUG(("cConcat %d\n", t->retry));

    /*
     * Pick a tentative midpoint.
     */

    if (v->mem[t->retry] == 0) {
	mid = longest(v, d, begin, end, NULL);
	if (mid == NULL) {
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));
	v->mem[t->retry] = (mid - begin) + 1;
    } else {
	mid = begin + (v->mem[t->retry] - 1);
	MDEBUG(("working midpoint %ld\n", LOFF(mid)));
    }

    /*
     * Iterate until satisfaction or failure.
     */

    for (;;) {
	/*
	 * Try this midpoint on for size.
	 */

	if (longest(v, d2, mid, end, NULL) == end) {
	    int er = complicatedDissect(v, t->left, begin, mid);

	    if (er == REG_OKAY) {
		er = complicatedDissect(v, t->right, mid, end);
		if (er == REG_OKAY) {
		    /*
		     * Satisfaction.
		     */

		    MDEBUG(("successful\n"));
		    freeDFA(d);
		    freeDFA(d2);
		    return REG_OKAY;
		}
	    }
	    if ((er != REG_OKAY) && (er != REG_NOMATCH)) {
		freeDFA(d);
		freeDFA(d2);
		return er;
	    }
	}

	/*
	 * That midpoint didn't work, find a new one.
	 */

	if (mid == begin) {
	    /*
	     * All possibilities exhausted.
	     */

	    MDEBUG(("%d no midpoint\n", t->retry));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	mid = longest(v, d, begin, mid-1, NULL);
	if (mid == NULL) {
	    /*
	     * Failed to find a new one.
	     */

	    MDEBUG(("%d failed midpoint\n", t->retry));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	MDEBUG(("%d: new midpoint %ld\n", t->retry, LOFF(mid)));
	v->mem[t->retry] = (mid - begin) + 1;
	zapSubtree(v, t->left);
	zapSubtree(v, t->right);
    }
}

/*
 - complicatedReversedDissect - determine backref shortest-first subexpression
 - matches
 * The retry memory stores the offset of the trial midpoint from begin, plus 1
 * so that 0 uniquely means "clean slate".
 ^ static int complicatedReversedDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
complicatedReversedDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    struct dfa *d, *d2;
    chr *mid;

    assert(t->op == '.');
    assert(t->left != NULL && t->left->cnfa.nstates > 0);
    assert(t->right != NULL && t->right->cnfa.nstates > 0);
    assert(t->left->flags&SHORTER);

    /*
     * Concatenation -- need to split the substring between parts.
     */

    d = newDFA(v, &t->left->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR()) {
	return v->err;
    }
    d2 = newDFA(v, &t->right->cnfa, &v->g->cmap, DOMALLOC);
    if (ISERR()) {
	freeDFA(d);
	return v->err;
    }
    MDEBUG(("cRev %d\n", t->retry));

    /*
     * Pick a tentative midpoint.
     */

    if (v->mem[t->retry] == 0) {
	mid = shortest(v, d, begin, begin, end, NULL, NULL);
	if (mid == NULL) {
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));
	v->mem[t->retry] = (mid - begin) + 1;
    } else {
	mid = begin + (v->mem[t->retry] - 1);
	MDEBUG(("working midpoint %ld\n", LOFF(mid)));
    }

    /*
     * Iterate until satisfaction or failure.
     */

    for (;;) {
	/*
	 * Try this midpoint on for size.
	 */

	if (longest(v, d2, mid, end, NULL) == end) {
	    int er = complicatedDissect(v, t->left, begin, mid);

	    if (er == REG_OKAY) {
		er = complicatedDissect(v, t->right, mid, end);
		if (er == REG_OKAY) {
		    /*
		     * Satisfaction.
		     */

		    MDEBUG(("successful\n"));
		    freeDFA(d);
		    freeDFA(d2);
		    return REG_OKAY;
		}
	    }
	    if (er != REG_OKAY && er != REG_NOMATCH) {
		freeDFA(d);
		freeDFA(d2);
		return er;
	    }
	}

	/*
	 * That midpoint didn't work, find a new one.
	 */

	if (mid == end) {
	    /*
	     * All possibilities exhausted.
	     */

	    MDEBUG(("%d no midpoint\n", t->retry));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	mid = shortest(v, d, begin, mid+1, end, NULL, NULL);
	if (mid == NULL) {
	    /*
	     * Failed to find a new one.
	     */

	    MDEBUG(("%d failed midpoint\n", t->retry));
	    freeDFA(d);
	    freeDFA(d2);
	    return REG_NOMATCH;
	}
	MDEBUG(("%d: new midpoint %ld\n", t->retry, LOFF(mid)));
	v->mem[t->retry] = (mid - begin) + 1;
	zapSubtree(v, t->left);
	zapSubtree(v, t->right);
    }
}

/*
 - complicatedBackrefDissect - determine backref subexpression matches
 ^ static int complicatedBackrefDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
complicatedBackrefDissect(
    struct vars *const v,
    struct subre *const t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    int i, n = t->subno, min = t->min, max = t->max;
    chr *paren, *p, *stop;
    size_t len;

    assert(t != NULL);
    assert(t->op == 'b');
    assert(n >= 0);
    assert((size_t)n < v->nmatch);

    MDEBUG(("cbackref n%d %d{%d-%d}\n", t->retry, n, min, max));

    if (v->pmatch[n].rm_so == -1) {
	return REG_NOMATCH;
    }
    paren = v->start + v->pmatch[n].rm_so;
    len = v->pmatch[n].rm_eo - v->pmatch[n].rm_so;

    /*
     * No room to maneuver -- retries are pointless.
     */

    if (v->mem[t->retry]) {
	return REG_NOMATCH;
    }
    v->mem[t->retry] = 1;

    /*
     * Special-case zero-length string.
     */

    if (len == 0) {
	if (begin == end) {
	    return REG_OKAY;
	}
	return REG_NOMATCH;
    }

    /*
     * And too-short string.
     */

    assert(end >= begin);
    if ((size_t)(end - begin) < len) {
	return REG_NOMATCH;
    }
    stop = end - len;

    /*
     * Count occurrences.
     */

    i = 0;
    for (p = begin; p <= stop && (i < max || max == INFINITY); p += len) {
	if (v->g->compare(paren, p, len) != 0) {
	    break;
	}
	i++;
    }
    MDEBUG(("cbackref found %d\n", i));

    /*
     * And sort it out.
     */

    if (p != end) {		/* didn't consume all of it */
	return REG_NOMATCH;
    }
    if (min <= i && (i <= max || max == INFINITY)) {
	return REG_OKAY;
    }
    return REG_NOMATCH;		/* out of range */
}

/*
 - complicatedAlternationDissect - determine alternative subexpression matches (w.
 - complications)
 ^ static int complicatedAlternationDissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
complicatedAlternationDissect(
    struct vars *const v,
    struct subre *t,
    chr *const begin,		/* beginning of relevant substring */
    chr *const end)		/* end of same */
{
    int er;
#define	UNTRIED	0		/* not yet tried at all */
#define	TRYING	1		/* top matched, trying submatches */
#define	TRIED	2		/* top didn't match or submatches exhausted */

#ifndef COMPILER_DOES_TAILCALL_OPTIMIZATION
    if (0) {
    doRight:
	t = t->right;
    }
#endif
    if (t == NULL) {
	return REG_NOMATCH;
    }
    assert(t->op == '|');
    if (v->mem[t->retry] == TRIED) {
	goto doRight;
    }

    MDEBUG(("cAlt n%d\n", t->retry));
    assert(t->left != NULL);

    if (v->mem[t->retry] == UNTRIED) {
	struct dfa *d = newDFA(v, &t->left->cnfa, &v->g->cmap, DOMALLOC);

	if (ISERR()) {
	    return v->err;
	}
	if (longest(v, d, begin, end, NULL) != end) {
	    freeDFA(d);
	    v->mem[t->retry] = TRIED;
	    goto doRight;
	}
	freeDFA(d);
	MDEBUG(("cAlt matched\n"));
	v->mem[t->retry] = TRYING;
    }

    er = complicatedDissect(v, t->left, begin, end);
    if (er != REG_NOMATCH) {
	return er;
    }

    v->mem[t->retry] = TRIED;
#ifndef COMPILER_DOES_TAILCALL_OPTIMIZATION
    goto doRight;
#else
  doRight:
    return complicatedAlternationDissect(v, t->right, begin, end);
#endif
}

#include "rege_dfa.c"

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
