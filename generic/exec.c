/*
 * exec.c --
 *
 *	Regexp package file:  re_*exec and friends - match REs
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
 * SCCS: @(#) exec.c 1.10 98/01/21 14:32:57
 */

#include "tclInt.h"
#include <assert.h>
#include "tclRegexp.h"
#include "chr.h"
#include "guts.h"


/* internal variables, bundled for easy passing around */
struct vars {
	regex_t *re;
	struct guts *g;
	int eflags;		/* copies of arguments */
	size_t nmatch;
	regmatch_t *pmatch;
	chr *start;		/* start of string */
	chr *stop;		/* just past end of string */
	int err;		/* error code if any (0 none) */
	regoff_t *mem;		/* memory vector for backtracking */
	regoff_t *mem1;		/* localizer vector */
	regoff_t *mem2;		/* dissector vector */
};
#define	VISERR(vv)	((vv)->err != 0)	/* have we seen an error yet? */
#define	ISERR()	VISERR(v)
#define	VERR(vv,e)	(((vv)->err) ? (vv)->err : ((vv)->err = (e)))
#define	ERR(e)	VERR(v, e)		/* record an error */
#define	NOERR()	{if (ISERR()) return;}	/* if error seen, return */
#define	OFF(p)	((p) - v->start)



/* lazy-DFA representation */
struct arcp {			/* "pointer" to an outarc */
	struct sset *ss;
	color co;
};

struct sset {			/* state set */
	unsigned *states;	/* pointer to bitvector */
	unsigned hash;		/* xor of bitvector */
	int flags;
#		define	STARTER		01	/* the initial state set */
#		define	POSTSTATE	02	/* includes the goal state */
	struct arcp ins;	/* chain of inarcs pointing here */
	chr *lastseen;		/* last entered on arrival here */
	struct sset **outs;	/* outarc vector indexed by color */
	struct arcp *inchain;	/* chain-pointer vector for outarcs */
};

struct dfa {
	int nssets;		/* size of cache */
	int nssused;		/* how many entries occupied yet */
	int nstates;		/* number of states */
	int ncolors;		/* length of outarc and inchain vectors */
	int wordsper;		/* length of state-set bitvectors */
	struct sset *ssets;	/* state-set cache */
	unsigned *statesarea;	/* bitvector storage */
	unsigned *work;		/* pointer to work area within statesarea */
	struct sset **outsarea;	/* outarc-vector storage */
	struct arcp *incarea;	/* inchain storage */
	struct cnfa *cnfa;
	struct colormap *cm;
	chr *lastpost;		/* location of last cache-flushed success */
};

#define	CACHE	200
#define	WORK	1		/* number of work bitvectors needed */



/*
 * forward declarations
 */
/* =====^!^===== begin forwards =====^!^===== */
/* automatically gathered by fwd; do not hand-edit */
/* === exec.c === */
int exec _ANSI_ARGS_((regex_t *, CONST chr *, size_t, size_t, regmatch_t [], int));
static int find _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *));
static int cfind _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *));
static VOID zapmatches _ANSI_ARGS_((regmatch_t *, size_t));
static VOID zapmem _ANSI_ARGS_((struct vars *, struct rtree *));
static VOID subset _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int dissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int altdissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int cdissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int crevdissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int csindissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int cbrdissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static int caltdissect _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static chr *dismatch _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static chr *dismrev _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static chr *dismsin _ANSI_ARGS_((struct vars *, struct rtree *, chr *, chr *));
static chr *longest _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *));
static chr *shortest _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *, chr *));
static struct dfa *newdfa _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *));
static VOID freedfa _ANSI_ARGS_((struct dfa *));
static unsigned hash _ANSI_ARGS_((unsigned *, int));
static struct sset *initialize _ANSI_ARGS_((struct vars *, struct dfa *, chr *));
static struct sset *miss _ANSI_ARGS_((struct vars *, struct dfa *, struct sset *, pcolor, chr *));
static int lacon _ANSI_ARGS_((struct vars *, struct cnfa *, chr *, pcolor));
static struct sset *getvacant _ANSI_ARGS_((struct vars *, struct dfa *));
static struct sset *pickss _ANSI_ARGS_((struct vars *, struct dfa *));
/* === color.c === */
union tree;
static color getcolor _ANSI_ARGS_((struct colormap *, pchr));
/* automatically gathered by fwd; do not hand-edit */
/* =====^!^===== end forwards =====^!^===== */



/*
 - exec - match regular expression
 ^ int exec(regex_t *, CONST chr *, size_t, size_t, regmatch_t [], int);
 */
int
exec(re, string, len, nmatch, pmatch, flags)
regex_t *re;
CONST chr *string;
size_t len;
size_t nmatch;
regmatch_t pmatch[];
int flags;
{
	struct vars var;
	register struct vars *v = &var;
	int st;
	size_t n;
	int complications;

	/* sanity checks */
	if (re == NULL || string == NULL || re->re_magic != REMAGIC)
		return REG_INVARG;
	if (re->re_csize != sizeof(chr))
		return REG_MIXED;

	/* setup */
	v->re = re;
	v->g = (struct guts *)re->re_guts;
	complications = (v->g->info&REG_UBACKREF) ? 1 : 0;
	if (v->g->usedshorter)
		complications = 1;
	v->eflags = flags;
	if (v->g->cflags&REG_NOSUB)
		nmatch = 0;		/* override client */
	v->nmatch = nmatch;
	if (complications && v->nmatch < (size_t)(v->g->nsub + 1)) {
		/* need work area bigger than what user gave us */
		v->pmatch = (regmatch_t *)ckalloc((v->g->nsub + 1) *
							sizeof(regmatch_t));
		if (v->pmatch == NULL)
			return REG_ESPACE;
		v->nmatch = v->g->nsub + 1;
	} else
		v->pmatch = pmatch;
	v->start = (chr *)string;
	v->stop = (chr *)string + len;
	v->err = 0;
	if (complications) {
		v->mem1 = (regoff_t *)ckalloc(2*v->g->ntree*sizeof(regoff_t));
		if (v->mem1 == NULL) {
			if (v->pmatch != pmatch)
				ckfree((char *)v->pmatch);
			return REG_ESPACE;
		}
		v->mem2 = v->mem1 + v->g->ntree;
	} else
		v->mem1 = NULL;

	/* do it */
	if (complications)
		st = cfind(v, &v->g->cnfa, v->g->cm);
	else
		st = find(v, &v->g->cnfa, v->g->cm);
	if (st == REG_OKAY && v->pmatch != pmatch && nmatch > 0) {
		zapmatches(pmatch, nmatch);
		n = (nmatch < v->nmatch) ? nmatch : v->nmatch;
		memcpy((VOID *)pmatch, (VOID *)v->pmatch, n*sizeof(regmatch_t));
	}
	if (v->pmatch != pmatch)
		ckfree((char *)v->pmatch);
	if (v->mem1 != NULL)
		ckfree((char *)v->mem1);
	return st;
}

/*
 - find - find a match for the main NFA (no-complications case)
 ^ static int find(struct vars *, struct cnfa *, struct colormap *);
 */
static int
find(v, cnfa, cm)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
{
	struct dfa *d = newdfa(v, cnfa, cm);
	chr *begin;
	chr *end;
 	chr *stop = (cnfa->leftanch) ? v->start : v->stop;

	if (d == NULL)
		return v->err;

	for (begin = v->start; begin <= stop; begin++) {
		if (v->eflags&REG_MTRACE)
			printf("\ntrying at %ld\n", (long)OFF(begin));
 		end = longest(v, d, begin, v->stop);
		if (end != NULL) {
			if (v->nmatch > 0) {
				v->pmatch[0].rm_so = OFF(begin);
				v->pmatch[0].rm_eo = OFF(end);
			}
			freedfa(d);
			if (v->nmatch > 1) {
				zapmatches(v->pmatch, v->nmatch);
				return dissect(v, v->g->tree, begin, end);
			}
			return REG_OKAY;
		}
	}

	freedfa(d);
	return REG_NOMATCH;
}

/*
 - cfind - find a match for the main NFA (with complications)
 ^ static int cfind(struct vars *, struct cnfa *, struct colormap *);
 */
static int
cfind(v, cnfa, cm)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
{
	struct dfa *d = newdfa(v, cnfa, cm);
	chr *begin;
	chr *end;
 	chr *stop = (cnfa->leftanch) ? v->start : v->stop;
	chr *estop;
	int er;
	int usedis = (v->g->tree == NULL || v->g->tree->op == '|') ? 0 : 1;

	if (d == NULL)
		return v->err;

	if (!v->g->usedshorter)
		usedis = 0;
	for (begin = v->start; begin <= stop; begin++) {
		if (v->eflags&REG_MTRACE)
			printf("\ntrying at %ld\n", (long)OFF(begin));
		if (usedis) {
			v->mem = v->mem1;
			zapmem(v, v->g->tree);
 		}
		estop = v->stop;
		for (;;) {
			if (usedis) {
				v->mem = v->mem1;
				end = dismatch(v, v->g->tree, begin, v->stop);
			} else
				end = longest(v, d, begin, estop);
			if (end == NULL)
				break;		/* NOTE BREAK OUT */
			if (v->eflags&REG_MTRACE)
				printf("tentative end %ld\n", (long)OFF(end));
			zapmatches(v->pmatch, v->nmatch);
			v->mem = v->mem2;
			zapmem(v, v->g->tree);
			er = cdissect(v, v->g->tree, begin, end);
			switch (er) {
			case REG_OKAY:
				if (v->nmatch > 0) {
					v->pmatch[0].rm_so = OFF(begin);
					v->pmatch[0].rm_eo = OFF(end);
				}
				freedfa(d);
				return REG_OKAY;
			case REG_NOMATCH:
				/* go around and try again */
				if (!usedis) {
					if (end == begin) {
						/* no point in trying again */
						freedfa(d);
						return REG_NOMATCH;
					}
					estop = end - 1;
				}
				break;
			default:
				freedfa(d);
				return er;
			}
		}
	}

	freedfa(d);
	return REG_NOMATCH;
}

/*
 - zapmatches - initialize the subexpression matches to "no match"
 ^ static VOID zapmatches(regmatch_t *, size_t);
 */
static VOID
zapmatches(p, n)
regmatch_t *p;
size_t n;
{
	size_t i;

	for (i = 1; i < n; i++) {
		p[i].rm_so = -1;
		p[i].rm_eo = -1;
	}
}

/*
 - zapmem - initialize the retry memory of a subtree to zeros
 ^ static VOID zapmem(struct vars *, struct rtree *);
 */
static VOID
zapmem(v, rt)
struct vars *v;
struct rtree *rt;
{
	if (rt == NULL)
		return;

	assert(v->mem != NULL);
	v->mem[rt->no] = 0;

	if (rt->left.tree != NULL)
		zapmem(v, rt->left.tree);
	if (rt->left.subno > 0) {
		v->pmatch[rt->left.subno].rm_so = -1;
		v->pmatch[rt->left.subno].rm_eo = -1;
	}
	if (rt->right.tree != NULL)
		zapmem(v, rt->right.tree);
	if (rt->right.subno > 0) {
		v->pmatch[rt->right.subno].rm_so = -1;
		v->pmatch[rt->right.subno].rm_eo = -1;
	}
	if (rt->next != NULL)
		zapmem(v, rt->next);
}

/*
 - subset - set any subexpression relevant to a successful subre
 ^ static VOID subset(struct vars *, struct subre *, chr *, chr *);
 */
static VOID
subset(v, sub, begin, end)
struct vars *v;
struct subre *sub;
chr *begin;
chr *end;
{
	int n = sub->subno;

	if (n == 0)
		return;
	assert(n > 0);
	if ((size_t)n >= v->nmatch)
		return;

	if (v->eflags&REG_MTRACE)
		printf("setting %d\n", n);
	v->pmatch[n].rm_so = OFF(begin);
	v->pmatch[n].rm_eo = OFF(end);
}

/*
 - dissect - determine subexpression matches (uncomplicated case)
 ^ static int dissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
dissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	struct dfa *d2;
	chr *mid;
	int i;

	if (rt == NULL)
		return REG_OKAY;
	if (v->eflags&REG_MTRACE)
		printf("substring %ld-%ld\n", (long)OFF(begin), (long)OFF(end));

	/* alternatives -- punt to auxiliary */
	if (rt->op == '|')
		return altdissect(v, rt, begin, end);

	/* concatenation -- need to split the substring between parts */
	assert(rt->op == ',');
	assert(rt->left.cnfa.nstates > 0);
	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (ISERR())
		return v->err;

	/* in some cases, there may be no right side... */
	if (rt->right.cnfa.nstates == 0) {
		if (v->eflags&REG_MTRACE)
			printf("singleton\n");
		if (longest(v, d, begin, end) != end) {
			freedfa(d);
			return REG_ASSERT;
		}
		freedfa(d);
		assert(rt->left.subno >= 0);
		subset(v, &rt->left, begin, end);
		return dissect(v, rt->left.tree, begin, end);
	}

	/* general case */
	assert(rt->right.cnfa.nstates > 0);
	d2 = newdfa(v, &rt->right.cnfa, v->g->cm);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}

	/* pick a tentative midpoint */
	mid = longest(v, d, begin, end);
	if (mid == NULL) {
		freedfa(d);
		freedfa(d2);
		return REG_ASSERT;
	}
	if (v->eflags&REG_MTRACE)
		printf("tentative midpoint %ld\n", (long)OFF(mid));

	/* iterate until satisfaction or failure */
	while (longest(v, d2, mid, end) != end) {
		/* that midpoint didn't work, find a new one */
		if (mid == begin) {
			/* all possibilities exhausted! */
			if (v->eflags&REG_MTRACE)
				printf("no midpoint!\n");
			freedfa(d);
			freedfa(d2);
			return REG_ASSERT;
		}
		mid = longest(v, d, begin, mid-1);
		if (mid == NULL) {
			/* failed to find a new one! */
			if (v->eflags&REG_MTRACE)
				printf("failed midpoint!\n");
			freedfa(d);
			freedfa(d2);
			return REG_ASSERT;
		}
		if (v->eflags&REG_MTRACE)
			printf("new midpoint %ld\n", (long)OFF(mid));
	}

	/* satisfaction */
	if (v->eflags&REG_MTRACE)
		printf("successful\n");
	freedfa(d);
	freedfa(d2);
	assert(rt->left.subno >= 0);
	subset(v, &rt->left, begin, mid);
	assert(rt->right.subno >= 0);
	subset(v, &rt->right, mid, end);
	i = dissect(v, rt->left.tree, begin, mid);
	if (i != REG_OKAY)
		return i;
	return dissect(v, rt->right.tree, mid, end);
}

/*
 - altdissect - determine alternative subexpression matches (uncomplicated)
 ^ static int altdissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
altdissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	int i;

	assert(rt != NULL);
	assert(rt->op == '|');

	for (i = 0; rt != NULL; rt = rt->next, i++) {
		if (v->eflags&REG_MTRACE)
			printf("trying %dth\n", i);
		assert(rt->left.begin != NULL);
		d = newdfa(v, &rt->left.cnfa, v->g->cm);
		if (ISERR())
			return v->err;
		if (longest(v, d, begin, end) == end) {
			if (v->eflags&REG_MTRACE)
				printf("success\n");
			freedfa(d);
			assert(rt->left.subno >= 0);
			subset(v, &rt->left, begin, end);
			return dissect(v, rt->left.tree, begin, end);
		}
		freedfa(d);
	}
	return REG_ASSERT;	/* none of them matched?!? */
}

/*
 - cdissect - determine subexpression matches (with complications)
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static int cdissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
cdissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	struct dfa *d2;
	chr *mid;
	int er;

	if (rt == NULL)
		return REG_OKAY;
	if (v->eflags&REG_MTRACE)
		printf("csubstr %ld-%ld\n", (long)OFF(begin), (long)OFF(end));

	/* punt various cases to auxiliaries */
	if (rt->op == '|')			/* alternatives */
		return caltdissect(v, rt, begin, end);
	if (rt->op == 'b')			/* backref */
		return cbrdissect(v, rt, begin, end);
	if (rt->right.cnfa.nstates == 0)	/* no RHS */
		return csindissect(v, rt, begin, end);
	if (rt->left.prefer == SHORTER)		/* reverse scan */
		return crevdissect(v, rt, begin, end);

	/* concatenation -- need to split the substring between parts */
	assert(rt->op == ',');
	assert(rt->left.cnfa.nstates > 0);
	assert(rt->right.cnfa.nstates > 0);
	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (ISERR())
		return v->err;
	d2 = newdfa(v, &rt->right.cnfa, v->g->cm);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}
	if (v->eflags&REG_MTRACE)
		printf("cconcat %d\n", rt->no);

	/* pick a tentative midpoint */
	if (v->mem[rt->no] == 0) {
		mid = longest(v, d, begin, end);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		if (v->eflags&REG_MTRACE)
			printf("tentative midpoint %ld\n", (long)OFF(mid));
		subset(v, &rt->left, begin, mid);
		v->mem[rt->no] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[rt->no] - 1);
		if (v->eflags&REG_MTRACE)
			printf("working midpoint %ld\n", (long)OFF(mid));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		er = cdissect(v, rt->left.tree, begin, mid);
		if (er == REG_OKAY && longest(v, d2, mid, end) == end &&
				(er = cdissect(v, rt->right.tree, mid, end)) == 
								REG_OKAY)
			break;			/* NOTE BREAK OUT */
		if (er != REG_OKAY && er != REG_NOMATCH) {
			freedfa(d);
			freedfa(d2);
			return er;
		}

		/* that midpoint didn't work, find a new one */
		if (mid == begin) {
			/* all possibilities exhausted */
			if (v->eflags&REG_MTRACE)
				printf("%d no midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		mid = longest(v, d, begin, mid-1);
		if (mid == NULL) {
			/* failed to find a new one */
			if (v->eflags&REG_MTRACE)
				printf("%d failed midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		if (v->eflags&REG_MTRACE)
			printf("%d: new midpoint %ld\n", rt->no,
								(long)OFF(mid));
		subset(v, &rt->left, begin, mid);
		v->mem[rt->no] = (mid - begin) + 1;
		zapmem(v, rt->left.tree);
		zapmem(v, rt->right.tree);
	}

	/* satisfaction */
	if (v->eflags&REG_MTRACE)
		printf("successful\n");
	freedfa(d);
	freedfa(d2);
	subset(v, &rt->right, mid, end);
	return REG_OKAY;
}

/*
 - crevdissect - determine shortest-first subexpression matches
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static int crevdissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
crevdissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	struct dfa *d2;
	chr *mid;
	int er;

	if (rt == NULL)
		return REG_OKAY;
	assert(rt->op == ',' && rt->left.prefer == SHORTER);

	/* concatenation -- need to split the substring between parts */
	assert(rt->left.cnfa.nstates > 0);
	assert(rt->right.cnfa.nstates > 0);
	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (ISERR())
		return v->err;
	d2 = newdfa(v, &rt->right.cnfa, v->g->cm);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}
	if (v->eflags&REG_MTRACE)
		printf("crev %d\n", rt->no);

	/* pick a tentative midpoint */
	if (v->mem[rt->no] == 0) {
		mid = shortest(v, d, begin, begin, end);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		if (v->eflags&REG_MTRACE)
			printf("tentative midpoint %ld\n", (long)OFF(mid));
		subset(v, &rt->left, begin, mid);
		v->mem[rt->no] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[rt->no] - 1);
		if (v->eflags&REG_MTRACE)
			printf("working midpoint %ld\n", (long)OFF(mid));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		er = cdissect(v, rt->left.tree, begin, mid);
		if (er == REG_OKAY && longest(v, d2, mid, end) == end &&
				(er = cdissect(v, rt->right.tree, mid, end)) == 
								REG_OKAY)
			break;			/* NOTE BREAK OUT */
		if (er != REG_OKAY && er != REG_NOMATCH) {
			freedfa(d);
			freedfa(d2);
			return er;
		}

		/* that midpoint didn't work, find a new one */
		if (mid == end) {
			/* all possibilities exhausted */
			if (v->eflags&REG_MTRACE)
				printf("%d no midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		mid = shortest(v, d, begin, mid+1, end);
		if (mid == NULL) {
			/* failed to find a new one */
			if (v->eflags&REG_MTRACE)
				printf("%d failed midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		if (v->eflags&REG_MTRACE)
			printf("%d: new midpoint %ld\n", rt->no,
								(long)OFF(mid));
		subset(v, &rt->left, begin, mid);
		v->mem[rt->no] = (mid - begin) + 1;
		zapmem(v, rt->left.tree);
		zapmem(v, rt->right.tree);
	}

	/* satisfaction */
	if (v->eflags&REG_MTRACE)
		printf("successful\n");
	freedfa(d);
	freedfa(d2);
	subset(v, &rt->right, mid, end);
	return REG_OKAY;
}

/*
 - csindissect - determine singleton subexpression matches (with complications)
 ^ static int csindissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
csindissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	int er;

	assert(rt != NULL);
	assert(rt->op == ',');
	assert(rt->right.cnfa.nstates == 0);
	if (v->eflags&REG_MTRACE)
		printf("csingleton %d\n", rt->no);

	assert(rt->left.cnfa.nstates > 0);

	/* exploit memory only to suppress repeated work in retries */
	if (!v->mem[rt->no]) {
		d = newdfa(v, &rt->left.cnfa, v->g->cm);
		if (longest(v, d, begin, end) != end) {
			freedfa(d);
			return REG_NOMATCH;
		}
		freedfa(d);
		v->mem[rt->no] = 1;
		if (v->eflags&REG_MTRACE)
			printf("csingleton matched\n");
	}

	er = cdissect(v, rt->left.tree, begin, end);
	if (er != REG_OKAY)
		return er;
	subset(v, &rt->left, begin, end);
	return REG_OKAY;
}

/*
 - cbrdissect - determine backref subexpression matches
 ^ static int cbrdissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
cbrdissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	int i;
	int n = -rt->left.subno;
	size_t len;
	chr *paren;
	chr *p;
	chr *stop;
	int min = rt->left.min;
	int max = rt->left.max;

	assert(rt != NULL);
	assert(rt->op == 'b');
	assert(rt->right.cnfa.nstates == 0);
	assert((size_t)n < v->nmatch);

	if (v->eflags&REG_MTRACE)
		printf("cbackref n%d %d{%d-%d}\n", rt->no, n, min, max);

	if (v->pmatch[n].rm_so == -1)
		return REG_NOMATCH;
	paren = v->start + v->pmatch[n].rm_so;
	len = v->pmatch[n].rm_eo - v->pmatch[n].rm_so;

	/* no room to maneuver -- retries are pointless */
	if (v->mem[rt->no])
		return REG_NOMATCH;
	v->mem[rt->no] = 1;

	/* special-case zero-length string */
	if (len == 0) {
		if (begin == end)
			return REG_OKAY;
		return REG_NOMATCH;
	}

	/* and too-short string */
	if ((size_t)(end - begin) < len)
		return REG_NOMATCH;
	stop = end - len;

	/* count occurrences */
	i = 0;
	for (p = begin; p <= stop && (i < max || max == INFINITY); p += len) {
		if ((*v->g->compare)(paren, p, len) != 0)
				break;
		i++;
	}
	if (v->eflags&REG_MTRACE)
		printf("cbackref found %d\n", i);

	/* and sort it out */
	if (p != end)			/* didn't consume all of it */
		return REG_NOMATCH;
	if (min <= i && (i <= max || max == INFINITY))
		return REG_OKAY;
	return REG_NOMATCH;		/* out of range */
}

/*
 - caltdissect - determine alternative subexpression matches (w. complications)
 ^ static int caltdissect(struct vars *, struct rtree *, chr *, chr *);
 */
static int			/* regexec return code */
caltdissect(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	int er;
#	define	UNTRIED	0	/* not yet tried at all */
#	define	TRYING	1	/* top matched, trying submatches */
#	define	TRIED	2	/* top didn't match or submatches exhausted */

	if (rt == NULL)
		return REG_NOMATCH;
	assert(rt->op == '|');
	if (v->mem[rt->no] == TRIED)
		return caltdissect(v, rt->next, begin, end);

	if (v->eflags&REG_MTRACE)
		printf("calt n%d\n", rt->no);
	assert(rt->left.begin != NULL);

	if (v->mem[rt->no] == UNTRIED) {
		d = newdfa(v, &rt->left.cnfa, v->g->cm);
		if (ISERR())
			return v->err;
		if (longest(v, d, begin, end) != end) {
			freedfa(d);
			v->mem[rt->no] = TRIED;
			return caltdissect(v, rt->next, begin, end);
		}
		freedfa(d);
		if (v->eflags&REG_MTRACE)
			printf("calt matched\n");
		v->mem[rt->no] = TRYING;
	}

	er = cdissect(v, rt->left.tree, begin, end);
	if (er == REG_OKAY) {
		subset(v, &rt->left, begin, end);
		return REG_OKAY;
	}
	if (er != REG_NOMATCH)
		return er;

	v->mem[rt->no] = TRIED;
	return caltdissect(v, rt->next, begin, end);
}

/*
 - dismatch - determine overall match using top-level dissection
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static chr *dismatch(struct vars *, struct rtree *, chr *, chr *);
 */
static chr *			/* endpoint, or NULL */
dismatch(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	struct dfa *d2;
	chr *mid;
	chr *ret;

	if (rt == NULL)
		return begin;
	if (v->eflags&REG_MTRACE)
		printf("dsubstr %ld-%ld\n", (long)OFF(begin), (long)OFF(end));

	/* punt various cases to auxiliaries */
	if (rt->right.cnfa.nstates == 0)	/* no RHS */
		return dismsin(v, rt, begin, end);
	if (rt->left.prefer == SHORTER)		/* reverse scan */
		return dismrev(v, rt, begin, end);

	/* concatenation -- need to split the substring between parts */
	assert(rt->op == ',');
	assert(rt->left.cnfa.nstates > 0);
	assert(rt->right.cnfa.nstates > 0);
	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (ISERR())
		return NULL;
	d2 = newdfa(v, &rt->right.cnfa, v->g->cm);
	if (ISERR()) {
		freedfa(d);
		return NULL;
	}
	if (v->eflags&REG_MTRACE)
		printf("dconcat %d\n", rt->no);

	/* pick a tentative midpoint */
	if (v->mem[rt->no] == 0) {
		mid = longest(v, d, begin, end);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		if (v->eflags&REG_MTRACE)
			printf("tentative midpoint %ld\n", (long)OFF(mid));
		v->mem[rt->no] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[rt->no] - 1);
		if (v->eflags&REG_MTRACE)
			printf("working midpoint %ld\n", (long)OFF(mid));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		if (rt->right.tree == NULL || rt->right.tree->op == 'b') {
			if (rt->right.prefer == LONGER)
				ret = longest(v, d2, mid, end);
			else
				ret = shortest(v, d2, mid, mid, end);
		} else {
			if (longest(v, d2, mid, end) != NULL)
				ret = dismatch(v, rt->right.tree, mid, end);
			else
				ret = NULL;
		}
		if (ret != NULL)
			break;			/* NOTE BREAK OUT */

		/* that midpoint didn't work, find a new one */
		if (mid == begin) {
			/* all possibilities exhausted */
			if (v->eflags&REG_MTRACE)
				printf("%d no midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		mid = longest(v, d, begin, mid-1);
		if (mid == NULL) {
			/* failed to find a new one */
			if (v->eflags&REG_MTRACE)
				printf("%d failed midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		if (v->eflags&REG_MTRACE)
			printf("%d: new midpoint %ld\n", rt->no,
								(long)OFF(mid));
		v->mem[rt->no] = (mid - begin) + 1;
		zapmem(v, rt->right.tree);
	}

	/* satisfaction */
	if (v->eflags&REG_MTRACE)
		printf("successful\n");
	freedfa(d);
	freedfa(d2);
	return ret;
}

/*
 - dismrev - determine overall match using top-level dissection
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static chr *dismrev(struct vars *, struct rtree *, chr *, chr *);
 */
static chr *			/* endpoint, or NULL */
dismrev(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	struct dfa *d2;
	chr *mid;
	chr *ret;

	if (rt == NULL)
		return begin;
	if (v->eflags&REG_MTRACE)
		printf("rsubstr %ld-%ld\n", (long)OFF(begin), (long)OFF(end));

	/* concatenation -- need to split the substring between parts */
	assert(rt->op == ',');
	assert(rt->left.cnfa.nstates > 0);
	assert(rt->right.cnfa.nstates > 0);
	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (ISERR())
		return NULL;
	d2 = newdfa(v, &rt->right.cnfa, v->g->cm);
	if (ISERR()) {
		freedfa(d);
		return NULL;
	}
	if (v->eflags&REG_MTRACE)
		printf("dconcat %d\n", rt->no);

	/* pick a tentative midpoint */
	if (v->mem[rt->no] == 0) {
		mid = shortest(v, d, begin, begin, end);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		if (v->eflags&REG_MTRACE)
			printf("tentative midpoint %ld\n", (long)OFF(mid));
		v->mem[rt->no] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[rt->no] - 1);
		if (v->eflags&REG_MTRACE)
			printf("working midpoint %ld\n", (long)OFF(mid));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		if (rt->right.tree == NULL || rt->right.tree->op == 'b') {
			if (rt->right.prefer == LONGER)
				ret = longest(v, d2, mid, end);
			else
				ret = shortest(v, d2, mid, mid, end);
		} else {
			if (longest(v, d2, mid, end) != NULL)
				ret = dismatch(v, rt->right.tree, mid, end);
			else
				ret = NULL;
		}
		if (ret != NULL)
			break;			/* NOTE BREAK OUT */

		/* that midpoint didn't work, find a new one */
		if (mid == end) {
			/* all possibilities exhausted */
			if (v->eflags&REG_MTRACE)
				printf("%d no midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		mid = shortest(v, d, begin, mid+1, end);
		if (mid == NULL) {
			/* failed to find a new one */
			if (v->eflags&REG_MTRACE)
				printf("%d failed midpoint\n", rt->no);
			freedfa(d);
			freedfa(d2);
			return NULL;
		}
		if (v->eflags&REG_MTRACE)
			printf("%d: new midpoint %ld\n", rt->no,
								(long)OFF(mid));
		v->mem[rt->no] = (mid - begin) + 1;
		zapmem(v, rt->right.tree);
	}

	/* satisfaction */
	if (v->eflags&REG_MTRACE)
		printf("successful\n");
	freedfa(d);
	freedfa(d2);
	return ret;
}

/*
 - dismsin - determine singleton subexpression matches (with complications)
 ^ static chr *dismsin(struct vars *, struct rtree *, chr *, chr *);
 */
static chr *
dismsin(v, rt, begin, end)
struct vars *v;
struct rtree *rt;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct dfa *d;
	chr *ret;

	assert(rt != NULL);
	assert(rt->op == ',');
	assert(rt->right.cnfa.nstates == 0);
	if (v->eflags&REG_MTRACE)
		printf("dsingleton %d\n", rt->no);

	assert(rt->left.cnfa.nstates > 0);

	/* retries are pointless */
	if (v->mem[rt->no])
		return NULL;
	v->mem[rt->no] = 1;

	d = newdfa(v, &rt->left.cnfa, v->g->cm);
	if (d == NULL)
		return NULL;
	if (rt->left.prefer == LONGER)
		ret = longest(v, d, begin, end);
	else
		ret = shortest(v, d, begin, begin, end);
	freedfa(d);
	if (ret != NULL && (v->eflags&REG_MTRACE))
		printf("dsingleton matched\n");
	return ret;
}

/*
 - longest - longest-preferred matching engine
 ^ static chr *longest(struct vars *, struct dfa *, chr *, chr *);
 */
static chr *			/* endpoint, or NULL */
longest(v, d, start, stop)
struct vars *v;			/* used only for debug and exec flags */
struct dfa *d;
chr *start;			/* where the match should start */
chr *stop;			/* match must end at or before here */
{
	chr *cp;
	chr *realstop = (stop == v->stop) ? stop : stop + 1;
	color co;
	struct sset *css;
	struct sset *ss;
	chr *post;
	int i;
	struct colormap *cm = d->cm;

	/* initialize */
	css = initialize(v, d, start);
	cp = start;

	/* startup */
	if (v->eflags&REG_FTRACE)
		printf("+++ startup +++\n");
	if (cp == v->start) {
		co = d->cnfa->bos[(v->eflags&REG_NOTBOL) ? 0 : 1];
		if (v->eflags&REG_FTRACE)
			printf("color %ld\n", (long)co);
	} else {
		co = getcolor(cm, *(cp - 1));
		if (v->eflags&REG_FTRACE)
			printf("char %c, color %ld\n", (char)*(cp-1), (long)co);
	}
	css = miss(v, d, css, co, cp);
	if (css == NULL)
		return NULL;
	css->lastseen = cp;

	/* main loop */
	if (v->eflags&REG_FTRACE)
		while (cp < realstop) {
			printf("+++ at c%d +++\n", css - d->ssets);
			co = getcolor(cm, *cp);
			printf("char %c, color %ld\n", (char)*cp, (long)co);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
		}
	else
		while (cp < realstop) {
			co = getcolor(cm, *cp);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
		}

	/* shutdown */
	if (v->eflags&REG_FTRACE)
		printf("+++ shutdown at c%d +++\n", css - d->ssets);
	if (cp == v->stop && stop == v->stop) {
		co = d->cnfa->eos[(v->eflags&REG_NOTEOL) ? 0 : 1];
		if (v->eflags&REG_FTRACE)
			printf("color %ld\n", (long)co);
		ss = miss(v, d, css, co, cp);
		/* special case:  match ended at eol? */
		if (ss != NULL && (ss->flags&POSTSTATE))
			return cp;
		else if (ss != NULL)
			ss->lastseen = cp;	/* to be tidy */
	}

	/* find last match, if any */
	post = d->lastpost;
	for (ss = d->ssets, i = 0; i < d->nssused; ss++, i++)
		if ((ss->flags&POSTSTATE) && post != ss->lastseen &&
					(post == NULL || post < ss->lastseen))
			post = ss->lastseen;
	if (post != NULL)		/* found one */
		return post - 1;

	return NULL;
}

/*
 - shortest - shortest-preferred matching engine
 ^ static chr *shortest(struct vars *, struct dfa *, chr *, chr *, chr *);
 */
static chr *			/* endpoint, or NULL */
shortest(v, d, start, min, max)
struct vars *v;			/* used only for debug and exec flags */
struct dfa *d;
chr *start;			/* where the match should start */
chr *min;			/* match must end at or after here */
chr *max;			/* match must end at or before here */
{
	chr *cp;
	chr *realmin = (min == v->stop) ? min : min + 1;
	chr *realmax = (max == v->stop) ? max : max + 1;
	color co;
	struct sset *css;
	struct sset *ss = NULL;
	struct colormap *cm = d->cm;

	/* initialize */
	css = initialize(v, d, start);
	cp = start;

	/* startup */
	if (v->eflags&REG_FTRACE)
		printf("--- startup ---\n");
	if (cp == v->start) {
		co = d->cnfa->bos[(v->eflags&REG_NOTBOL) ? 0 : 1];
		if (v->eflags&REG_FTRACE)
			printf("color %ld\n", (long)co);
	} else {
		co = getcolor(cm, *(cp - 1));
		if (v->eflags&REG_FTRACE)
			printf("char %c, color %ld\n", (char)*(cp-1), (long)co);
	}
	css = miss(v, d, css, co, cp);
	if (css == NULL)
		return NULL;
	css->lastseen = cp;

	/* main loop */
	if (v->eflags&REG_FTRACE)
		while (cp < realmax) {
			printf("--- at c%d ---\n", css - d->ssets);
			co = getcolor(cm, *cp);
			printf("char %c, color %ld\n", (char)*cp, (long)co);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
			if ((ss->flags&POSTSTATE) && cp >= realmin)
				break;		/* NOTE BREAK OUT */
		}
	else
		while (cp < realmax) {
			co = getcolor(cm, *cp);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
			if ((ss->flags&POSTSTATE) && cp >= realmin)
				break;		/* NOTE BREAK OUT */
		}

	if (ss == NULL)
		return NULL;
	if (ss->flags&POSTSTATE) {
		assert(cp >= realmin);
		return cp - 1;
	}

	/* shutdown */
	if (v->eflags&REG_FTRACE)
		printf("--- shutdown at c%d ---\n", css - d->ssets);
	if (cp == v->stop && max == v->stop) {
		co = d->cnfa->eos[(v->eflags&REG_NOTEOL) ? 0 : 1];
		if (v->eflags&REG_FTRACE)
			printf("color %ld\n", (long)co);
		ss = miss(v, d, css, co, cp);
		/* special case:  match ended at eol? */
		if (ss != NULL && (ss->flags&POSTSTATE))
			return cp;
	}

	return NULL;
}

/*
 - newdfa - set up a fresh DFA
 ^ static struct dfa *newdfa(struct vars *, struct cnfa *,
 ^ 	struct colormap *);
 */
static struct dfa *
newdfa(v, cnfa, cm)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
{
	struct dfa *d = (struct dfa *)ckalloc(sizeof(struct dfa));
	int wordsper = (cnfa->nstates + UBITS - 1) / UBITS;
	struct sset *ss;
	int i;

	assert(cnfa != NULL && cnfa->nstates != 0);
	if (d == NULL) {
		ERR(REG_ESPACE);
		return NULL;
	}

	d->ssets = (struct sset *)ckalloc(CACHE * sizeof(struct sset));
	d->statesarea = (unsigned *)ckalloc((CACHE+WORK) * wordsper *
							sizeof(unsigned));
	d->work = &d->statesarea[CACHE * wordsper];
	d->outsarea = (struct sset **)ckalloc(CACHE * cnfa->ncolors *
							sizeof(struct sset *));
	d->incarea = (struct arcp *)ckalloc(CACHE * cnfa->ncolors *
							sizeof(struct arcp));
	if (d->ssets == NULL || d->statesarea == NULL || d->outsarea == NULL ||
							d->incarea == NULL) {
		freedfa(d);
		ERR(REG_ESPACE);
		return NULL;
	}

	d->nssets = (v->eflags&REG_SMALL) ? 5 : CACHE;
	d->nssused = 0;
	d->nstates = cnfa->nstates;
	d->ncolors = cnfa->ncolors;
	d->wordsper = wordsper;
	d->cnfa = cnfa;
	d->cm = cm;
	d->lastpost = NULL;

	for (ss = d->ssets, i = 0; i < d->nssets; ss++, i++) {
		/* initialization of most fields is done as needed */
		ss->states = &d->statesarea[i * d->wordsper];
		ss->outs = &d->outsarea[i * d->ncolors];
		ss->inchain = &d->incarea[i * d->ncolors];
	}

	return d;
}

/*
 - freedfa - free a DFA
 ^ static VOID freedfa(struct dfa *);
 */
static VOID
freedfa(d)
struct dfa *d;
{
	if (d->ssets != NULL)
		ckfree((char *)d->ssets);
	if (d->statesarea != NULL)
		ckfree((char *)d->statesarea);
	if (d->outsarea != NULL)
		ckfree((char *)d->outsarea);
	if (d->incarea != NULL)
		ckfree((char *)d->incarea);
	ckfree((char *)d);
}

/*
 - hash - construct a hash code for a bitvector
 * There are probably better ways, but they're more expensive.
 ^ static unsigned hash(unsigned *, int);
 */
static unsigned
hash(uv, n)
unsigned *uv;
int n;
{
	int i;
	unsigned h;

	h = 0;
	for (i = 0; i < n; i++)
		h ^= uv[i];
	return h;
}

/*
 - initialize - hand-craft a cache entry for startup, otherwise get ready
 ^ static struct sset *initialize(struct vars *, struct dfa *, chr *);
 */
static struct sset *
initialize(v, d, start)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
chr *start;
{
	struct sset *ss;
	int i;

	/* is previous one still there? */
	if (d->nssused > 0 && (d->ssets[0].flags&STARTER))
		ss = &d->ssets[0];
	else {				/* no, must (re)build it */
		ss = getvacant(v, d);
		for (i = 0; i < d->wordsper; i++)
			ss->states[i] = 0;
		BSET(ss->states, d->cnfa->pre);
		ss->hash = hash(ss->states, d->wordsper);
		assert(d->cnfa->pre != d->cnfa->post);
		ss->flags = STARTER;
		/* lastseen dealt with below */
	}

	for (i = 0; i < d->nssused; i++)
		d->ssets[i].lastseen = NULL;
	ss->lastseen = start;		/* maybe untrue, but harmless */
	d->lastpost = NULL;
	return ss;
}

/*
 - miss - handle a cache miss
 ^ static struct sset *miss(struct vars *, struct dfa *, struct sset *,
 ^ 	pcolor, chr *);
 */
static struct sset *		/* NULL if goes to empty set */
miss(v, d, css, co, cp)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
struct sset *css;
pcolor co;
chr *cp;			/* next chr */
{
	struct cnfa *cnfa = d->cnfa;
	int i;
	unsigned h;
	struct carc *ca;
	struct sset *p;
	int ispost;
	int gotstate;
	int dolacons;
	int didlacons;

	/* for convenience, we can be called even if it might not be a miss */
	if (css->outs[co] != NULL) {
		if (v->eflags&REG_FTRACE)
			printf("hit\n");
		return css->outs[co];
	}
	if (v->eflags&REG_FTRACE)
		printf("miss\n");

	/* first, what set of states would we end up in? */
	for (i = 0; i < d->wordsper; i++)
		d->work[i] = 0;
	ispost = 0;
	gotstate = 0;
	for (i = 0; i < d->nstates; i++)
		if (ISBSET(css->states, i))
			for (ca = cnfa->states[i]; ca->co != COLORLESS; ca++)
				if (ca->co == co) {
					BSET(d->work, ca->to);
					gotstate = 1;
					if (ca->to == cnfa->post)
						ispost = 1;
					if (v->eflags&REG_FTRACE)
						printf("%d -> %d\n", i, ca->to);
				}
	dolacons = (gotstate) ? cnfa->haslacons : 0;
	didlacons = 0;
	while (dolacons) {		/* transitive closure */
		dolacons = 0;
		for (i = 0; i < d->nstates; i++)
			if (ISBSET(d->work, i))
				for (ca = cnfa->states[i]; ca->co != COLORLESS;
									ca++)
					if (ca->co > cnfa->ncolors &&
						!ISBSET(d->work, ca->to) &&
							lacon(v, cnfa, cp,
								ca->co)) {
						BSET(d->work, ca->to);
						dolacons = 1;
						didlacons = 1;
						if (ca->to == cnfa->post)
							ispost = 1;
						if (v->eflags&REG_FTRACE)
							printf("%d :-> %d\n",
								i, ca->to);
					}
	}
	if (!gotstate)
		return NULL;
	h = hash(d->work, d->wordsper);

	/* next, is that in the cache? */
	for (p = d->ssets, i = d->nssused; i > 0; p++, i--)
		if (p->hash == h && memcmp((VOID *)d->work, (VOID *)p->states,
					d->wordsper*sizeof(unsigned)) == 0) {
			if (v->eflags&REG_FTRACE)
				printf("cached c%d\n", p - d->ssets);
			break;			/* NOTE BREAK OUT */
		}
	if (i == 0) {		/* nope, need a new cache entry */
		p = getvacant(v, d);
		assert(p != css);
		for (i = 0; i < d->wordsper; i++)
			p->states[i] = d->work[i];
		p->hash = h;
		p->flags = (ispost) ? POSTSTATE : 0;
		/* lastseen to be dealt with by caller */
	}

	if (!didlacons) {		/* lookahead conds. always cache miss */
		css->outs[co] = p;
		css->inchain[co] = p->ins;
		p->ins.ss = css;
		p->ins.co = (color) co;
	}
	return p;
}

/*
 - lacon - lookahead-constraint checker for miss()
 ^ static int lacon(struct vars *, struct cnfa *, chr *, pcolor);
 */
static int			/* predicate:  constraint satisfied? */
lacon(v, pcnfa, precp, co)
struct vars *v;
struct cnfa *pcnfa;		/* parent cnfa */
chr *precp;			/* points to previous chr */
pcolor co;			/* "color" of the lookahead constraint */
{
	int n;
	struct subre *sub;
	struct dfa *d;
	chr *end;

	n = co - pcnfa->ncolors;
	assert(n < v->g->nlacons && v->g->lacons != NULL);
	if (v->eflags&REG_FTRACE)
		printf("=== testing lacon %d\n", n);
	sub = &v->g->lacons[n];
	d = newdfa(v, &sub->cnfa, v->g->cm);
	if (d == NULL) {
		ERR(REG_ESPACE);
		return 0;
	}
	end = longest(v, d, precp, v->stop);
	freedfa(d);
	if (v->eflags&REG_FTRACE)
		printf("=== lacon %d match %d\n", n, (end != NULL));
	return (sub->subno) ? (end != NULL) : (end == NULL);
}

/*
 - getvacant - get a vacant state set
 * This routine clears out the inarcs and outarcs, but does not otherwise
 * clear the innards of the state set -- that's up to the caller.
 ^ static struct sset *getvacant(struct vars *, struct dfa *);
 */
static struct sset *
getvacant(v, d)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
{
	int i;
	struct sset *ss;
	struct sset *p;
	struct arcp ap;
	struct arcp lastap;
	color co;

	ss = pickss(v, d);

	/* clear out its inarcs, including self-referential ones */
	ap = ss->ins;
	while ((p = ap.ss) != NULL) {
		co = ap.co;
		if (v->eflags&REG_FTRACE)
			printf("zapping c%d's %ld outarc\n", p - d->ssets,
								(long)co);
		p->outs[co] = NULL;
		ap = p->inchain[co];
		p->inchain[co].ss = NULL;	/* paranoia */
	}
	ss->ins.ss = NULL;

	/* take it off the inarc chains of the ssets reached by its outarcs */
	for (i = 0; i < d->ncolors; i++) {
		p = ss->outs[i];
		assert(p != ss);		/* not self-referential */
		if (p == NULL)
			continue;		/* NOTE CONTINUE */
		if (v->eflags&REG_FTRACE)
			printf("deleting outarc %d from c%d's inarc chain\n",
							i, p - d->ssets);
		if (p->ins.ss == ss && p->ins.co == i)
			p->ins = ss->inchain[i];
		else {
			assert(p->ins.ss != NULL);
			for (ap = p->ins; ap.ss != NULL &&
						!(ap.ss == ss && ap.co == i);
						ap = ap.ss->inchain[ap.co])
				lastap = ap;
			assert(ap.ss != NULL);
			lastap.ss->inchain[lastap.co] = ss->inchain[i];
		}
		ss->outs[i] = NULL;
		ss->inchain[i].ss = NULL;
	}

	/* if ss was a success state, may need to remember location */
	if ((ss->flags&POSTSTATE) && ss->lastseen != d->lastpost &&
			(d->lastpost == NULL || d->lastpost < ss->lastseen))
		d->lastpost = ss->lastseen;

	return ss;
}

/*
 - pickss - pick the next stateset to be used
 ^ static struct sset *pickss(struct vars *, struct dfa *);
 */
static struct sset *
pickss(v, d)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
{
	int i;
	struct sset *ss;
	struct sset *oldest;

	/* shortcut for cases where cache isn't full */
	if (d->nssused < d->nssets) {
		ss = &d->ssets[d->nssused];
		d->nssused++;
		if (v->eflags&REG_FTRACE)
			printf("new c%d\n", ss - d->ssets);
		/* must make innards consistent */
		ss->ins.ss = NULL;
		for (i = 0; i < d->ncolors; i++) {
			ss->outs[i] = NULL;
			ss->inchain[i].ss = NULL;
		}
		ss->flags = 0;
		return ss;
	}

	/* look for oldest */
	oldest = d->ssets;
	for (ss = d->ssets, i = d->nssets; i > 0; ss++, i--) {
		if (ss->lastseen != oldest->lastseen && (ss->lastseen == NULL ||
					ss->lastseen < oldest->lastseen))
			oldest = ss;
	}
	if (v->eflags&REG_FTRACE)
		printf("replacing c%d\n", oldest - d->ssets);
	return oldest;
}

#define	EXEC	1
#include "color.c"
