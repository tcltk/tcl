/*
 * color.c  --
 *
 *	Regexp package file:  colorings of characters.
 *	Note that there are some incestuous relationships between this code and
 *	NFA arc maintenance, which perhaps ought to be cleaned up sometime.
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
 * SCCS: @(#) color.c 1.10 98/02/11 17:23:09
 */

/*
 * The innards.
 */
struct colors {
	color ccolor[BYTTAB];
};
struct ptrs {
	union tree *pptr[BYTTAB];
};
union tree {
	struct colors colors;
	struct ptrs ptrs;
};
#define	tcolor	colors.ccolor
#define	tptr	ptrs.pptr
/*
 * Some of the function prototypes need this.
 ^ union tree;
 */

struct colordesc {
	uchr nchrs;		/* number of chars of this color */
	color sub;		/* open subcolor of this one, or NOSUB */
#		define	NOSUB	COLORLESS
	struct arc *arcs;	/* color chain */
#	define	UNUSEDCOLOR(cd)	((cd)->nchrs == 0 && (cd)->sub == NOSUB)
	int flags;
#		define	PSEUDO	1	/* pseudocolor, no real chars */
};

struct colormap {
	int magic;
#		define	CMMAGIC	0x876
	struct vars *v;			/* for error reporting */
	color rest;
	int filled;			/* has it been filled? */
	int ncds;			/* number of colordescs */
	struct colordesc *cd;
#	define	CDEND(cm)	(&(cm)->cd[(cm)->ncds])
#		define	NINLINECDS	10
	struct colordesc cds[NINLINECDS];
	union tree tree[NBYTS];		/* tree top, plus fill blocks */
};

#ifdef COMPILE

/*
 - newcm - get new colormap
 ^ static struct colormap *newcm(struct vars *);
 */
static struct colormap *	/* NULL for allocation failure */
newcm(v)
struct vars *v;
{
	struct colormap *cm;
	int i;
	int j;
	union tree *t;
	union tree *nextt;
	struct colordesc *cd;

	cm = (struct colormap *)ckalloc(sizeof(struct colormap));
	if (cm == NULL) {
		ERR(REG_ESPACE);
		return NULL;
	}
	cm->magic = CMMAGIC;
	cm->v = v;
	cm->rest = WHITE;
	cm->filled = 0;

	cm->ncds = NINLINECDS;
	cm->cd = cm->cds;
	for (cd = cm->cd; cd < CDEND(cm); cd++) {
		cd->nchrs = 0;
		cd->sub = NOSUB;
		cd->arcs = NULL;
		cd->flags = 0;
	}
	cm->cd[WHITE].nchrs = WCHAR_MAX - WCHAR_MIN;

	/* treetop starts as NULLs if there are lower levels */
	t = cm->tree;
	if (NBYTS > 1) {
	    for (i = BYTTAB-1; i >= 0; i--)
		    t->tptr[i] = NULL;
	}

	/* if no lower levels, treetop and last fill block are the same */

	/* fill blocks point to next fill block... */
	for (t = &cm->tree[1], j = NBYTS-2; j > 0; t = nextt, j--) {
		nextt = t + 1;
		for (i = BYTTAB-1; i >= 0; i--)
			t->tptr[i] = t + 1;
	}
	/* ...except last which is solid white */
	t = &cm->tree[NBYTS-1];
	for (i = BYTTAB-1; i >= 0; i--)
		t->tcolor[i] = WHITE;


	return cm;
}

/*
 - freecm - free a colormap
 ^ static VOID freecm(struct colormap *);
 */
static VOID
freecm(cm)
struct colormap *cm;
{
	cm->magic = 0;
	if (NBYTS > 1) {
	    cmtreefree(cm, cm->tree, 0);
	}
	if (cm->cd != cm->cds) {
		ckfree((char *)cm->cd);
	}
	ckfree((char *) cm);	/* mem leak (CCS). */
}

/*
 - cmtreefree - free a non-terminal part of a colormap tree
 ^ static VOID cmtreefree(struct colormap *, union tree *, int);
 */
static VOID
cmtreefree(cm, tree, level)
struct colormap *cm;
union tree *tree;
int level;			/* level number (top == 0) of this block */
{
	int i;
	union tree *t;
	union tree *fillt = &cm->tree[level+1];

	assert(level < NBYTS-1);	/* this level has pointers */
	for (i = BYTTAB-1; i >= 0; i--) {
		t = tree->tptr[i];
		if (t != NULL && t != fillt) {
			if ((int) level < (int) NBYTS-2) {	/* more pointer blocks below */
				cmtreefree(cm, t, level+1);
			}
			ckfree((char *) t);
		}
	}
}

/*
 - fillcm - fill in a colormap, so no NULLs remain
 * The point of this is that the tree traversal can then be a fixed set
 * of table lookups with no conditional branching.  It might be better
 * to do reallocation for a more compacted structure, on the order of
 * what's done for NFAs, but the colormap can be quite large and a total
 * rebuild of it could be costly.
 ^ static VOID fillcm(struct colormap *);
 */
static VOID
fillcm(cm)
struct colormap *cm;
{
	if (!cm->filled && NBYTS > 1)
		cmtreefill(cm, cm->tree, 0);
	cm->filled = 1;
}

/*
 - cmtreefill - fill a non-terminal part of a colormap tree
 ^ static VOID cmtreefill(struct colormap *, union tree *, int);
 */
static VOID
cmtreefill(cm, tree, level)
struct colormap *cm;
union tree *tree;
int level;			/* level number (top == 0) of this block */
{
	int i;
	union tree *t;
	union tree *fillt = &cm->tree[level+1];

	assert(level < NBYTS-1);	/* this level has pointers */
	for (i = BYTTAB-1; i >= 0; i--) {
		t = tree->tptr[i];
		if (t == fillt)			/* oops */
			{}
		else if (t == NULL) {
			tree->tptr[i] = fillt;
		}
		else if ((int) level < (int) NBYTS-2)	{/* more pointer blocks below */
			cmtreefill(cm, t, level+1);
		}
	}
}

#endif				/* ifdef COMPILE */

/*
 - getcolor - get the color of a character from a colormap
 ^ static color getcolor(struct colormap *, pchr);
 */
static color
getcolor(cm, c)
struct colormap *cm;
pchr c;
{
	uchr uc = c;
	int shift;
	int b;
	union tree *t;

	assert(cm->magic == CMMAGIC);

	t = cm->tree;
	for (shift = BYTBITS * (NBYTS - 1); t != NULL; shift -= BYTBITS) {
		b = (uc >> shift) & BYTMASK;
		if (shift == 0)		/* reached the bottom */
			return t->tcolor[b];
		t = t->tptr[b];
	}

	/* we fell off an incomplete part of the tree */
	assert(!cm->filled);
	return cm->rest;
}

#ifdef COMPILE

/*
 - setcolor - set the color of a character in a colormap
 ^ static color setcolor(struct colormap *, pchr, pcolor);
 */
static color			/* previous color */
setcolor(cm, c, co)
struct colormap *cm;
pchr c;
pcolor co;
{
	uchr uc = c;
	int shift;
	int i;
	int b;
	int bottom;
	union tree *t;
	union tree *lastt;
	color prev;

	assert(cm->magic == CMMAGIC);
	if (VISERR(cm->v) || co == COLORLESS)
		return COLORLESS;

	t = cm->tree;
	for (shift = BYTBITS * (NBYTS - 1); shift > 0; shift -= BYTBITS) {
		b = (uc >> shift) & BYTMASK;
		lastt = t;
		t = t->tptr[b];
		if (t == NULL) {	/* fell off an incomplete part */
			bottom = (shift <= BYTBITS) ? 1 : 0;
			t = (union tree *)ckalloc((bottom) ?
				sizeof(struct colors) : sizeof(struct ptrs));
			if (t == NULL) {
				VERR(cm->v, REG_ESPACE);
				return COLORLESS;
			}
			if (bottom)
				for (i = BYTTAB-1; i >= 0; i--)
					t->tcolor[i] = cm->rest;
			else
				for (i = BYTTAB-1; i >= 0; i--)
					t->tptr[i] = NULL;
			lastt->tptr[b] = t;
		}
	}
	assert(shift == 0 && t != NULL);	/* we hit bottom; it's there */

	b = uc & BYTMASK;
	prev = t->tcolor[b];
	t->tcolor[b] = (color) co;
	return prev;
}

/*
 - maxcolor - report largest color number in use
 ^ static color maxcolor(struct colormap *);
 */
static color
maxcolor(cm)
struct colormap *cm;
{
	struct colordesc *cd;
	struct colordesc *end;
	struct colordesc *lastused;

	if (VISERR(cm->v))
		return COLORLESS;

	lastused = NULL;
	end = CDEND(cm);
	for (cd = cm->cd; cd < end; cd++)
		if (!UNUSEDCOLOR(cd))
			lastused = cd;
	assert(lastused != NULL);
	return (color) (lastused - cm->cd);
}

/*
 - newcolor - find a new color (must be subject of setcolor at once)
 * Beware:  may relocate the colordescs.
 ^ static color newcolor(struct colormap *);
 */
static color			/* COLORLESS for error */
newcolor(cm)
struct colormap *cm;
{
	struct colordesc *cd;
	struct colordesc *end;
	struct colordesc *firstnew;
	int n;

	if (VISERR(cm->v))
		return COLORLESS;

	end = CDEND(cm);
	for (cd = cm->cd; cd < end; cd++)
		if (UNUSEDCOLOR(cd)) {
			assert(cd->arcs == NULL);
			return (color) (cd - cm->cd);
		}

	/* oops, must allocate more */
	n = cm->ncds * 2;
	if (cm->cd == cm->cds) {
		cd = (struct colordesc *)ckalloc(sizeof(struct colordesc) * n);
		if (cd != NULL)
			memcpy((VOID *)cd, (VOID *)cm->cds, cm->ncds *
						sizeof(struct colordesc));
	} else {
		cd = (struct colordesc *)ckrealloc((VOID *)cm->cd,
						sizeof(struct colordesc) * n);
	}
	if (cd == NULL) {
		VERR(cm->v, REG_ESPACE);
		return COLORLESS;
	}
	cm->cd = cd;
	firstnew = CDEND(cm);
	cm->ncds = n;
	end = CDEND(cm);
	for (cd = firstnew; cd < end; cd++) {
		cd->nchrs = 0;
		cd->sub = NOSUB;
		cd->arcs = NULL;
		cd->flags = 0;
	}
	assert(firstnew < CDEND(cm) && UNUSEDCOLOR(firstnew));
	return (color) (firstnew - cm->cd);
}

/*
 - pseudocolor - allocate a false color, to be managed by other means
 ^ static color pseudocolor(struct colormap *);
 */
static color
pseudocolor(cm)
struct colormap *cm;
{
	color co;

	co = newcolor(cm);
	if (VISERR(cm->v))
		return COLORLESS;
	cm->cd[co].nchrs = 1;
	cm->cd[co].flags = PSEUDO;
	return co;
}

/*
 - subcolor - allocate a new subcolor (if necessary) to this chr
 ^ static color subcolor(struct colormap *, pchr c);
 */
static color
subcolor(cm, c)
struct colormap *cm;
pchr c;
{
	color co;			/* current color of c */
	color sco;			/* new subcolor */

	co = getcolor(cm, c);
	sco = cm->cd[co].sub;
	if (sco == NOSUB) {		/* must create subcolor */
		if (cm->cd[co].nchrs == 1)	/* shortcut */
			return co;
		sco = newcolor(cm);
		if (sco == COLORLESS)
			return COLORLESS;
		cm->cd[co].sub = sco;
		cm->cd[sco].sub = sco;	/* self-referential subcolor ptr */
	}

	if (co == sco)		/* repeated character */
		return co;	/* no further action needed */
	cm->cd[co].nchrs--;
	cm->cd[sco].nchrs++;
	setcolor(cm, c, sco);
	return sco;
}

/*
 - okcolors - promote subcolors to full colors
 ^ static VOID okcolors(struct nfa *, struct colormap *);
 */
static VOID
okcolors(nfa, cm)
struct nfa *nfa;
struct colormap *cm;
{
	struct colordesc *cd;
	struct colordesc *end = CDEND(cm);
	struct colordesc *scd;
	struct arc *a;
	color co;
	color sco;

 	for (cd = cm->cd, co = 0; cd < end; cd++, co++) {
  		sco = cd->sub;
 		if (sco == NOSUB) {
 			/* has no subcolor, no further action */
 		} else if (sco == co) {
 			/* is subcolor, let parent deal with it */
  		} else if (cd->nchrs == 0) {
  			/* parent empty, its arcs change color to subcolor */
 			cd->sub = NOSUB;
  			scd = &cm->cd[sco];
  			assert(scd->nchrs > 0);
  			assert(scd->sub == sco);
 			scd->sub = NOSUB;
  			while ((a = cd->arcs) != NULL) {
  				assert(a->co == co);
  				/* uncolorchain(cm, a); */
				cd->arcs = a->colorchain;
				a->co = sco;
				/* colorchain(cm, a); */
				a->colorchain = scd->arcs;
				scd->arcs = a;
			}
		} else {
			/* parent's arcs must gain parallel subcolor arcs */
 			cd->sub = NOSUB;
 			scd = &cm->cd[sco];
 			assert(scd->nchrs > 0);
 			assert(scd->sub == sco);
 			scd->sub = NOSUB;
			for (a = cd->arcs; a != NULL; a = a->colorchain) {
				assert(a->co == co);
				newarc(nfa, a->type, sco, a->from, a->to);
			}
		}
	}
}

/*
 - colorchain - add this arc to the color chain of its color
 ^ static VOID colorchain(struct colormap *, struct arc *);
 */
static VOID
colorchain(cm, a)
struct colormap *cm;
struct arc *a;
{
	struct colordesc *cd = &cm->cd[a->co];

	a->colorchain = cd->arcs;
	cd->arcs = a;
}

/*
 - uncolorchain - delete this arc from the color chain of its color
 ^ static VOID uncolorchain(struct colormap *, struct arc *);
 */
static VOID
uncolorchain(cm, a)
struct colormap *cm;
struct arc *a;
{
	struct colordesc *cd = &cm->cd[a->co];
	struct arc *aa;

	aa = cd->arcs;
	if (aa == a)		/* easy case */
		cd->arcs = a->colorchain;
	else {
		for (; aa != NULL && aa->colorchain != a; aa = aa->colorchain)
			continue;
		assert(aa != NULL);
		aa->colorchain = a->colorchain;
	}
	a->colorchain = NULL;	/* paranoia */
}

/*
 - singleton - is this character in its own color?
 ^ static int singleton(struct colormap *, pchr c);
 */
static int			/* predicate */
singleton(cm, c)
struct colormap *cm;
pchr c;
{
	color co;			/* color of c */

	co = getcolor(cm, c);
	if (cm->cd[co].nchrs == 1 && cm->cd[co].sub == NOSUB)
		return 1;
	return 0;
}

/*
 - rainbow - add arcs of all full colors (but one) between specified states
 ^ static VOID rainbow(struct nfa *, struct colormap *, int, pcolor,
 ^ 	struct state *, struct state *);
 */
static VOID
rainbow(nfa, cm, type, exc, from, to)
struct nfa *nfa;
struct colormap *cm;
int type;
pcolor exc;			/* COLORLESS if no exceptions */
struct state *from;
struct state *to;
{
	struct colordesc *cd;
	struct colordesc *end = CDEND(cm);
	color co;

	for (cd = cm->cd, co = 0; cd < end && !VISERR(nfa->v); cd++, co++)
		if (!UNUSEDCOLOR(cd) && cd->sub != co && co != exc &&
							!(cd->flags&PSEUDO))
			newarc(nfa, type, co, from, to);
}

/*
 - colorcomplement - add arcs of complementary colors
 * The calling sequence ought to be reconciled with cloneouts().
 ^ static VOID colorcomplement(struct nfa *, struct colormap *, int,
 ^ 	struct state *, struct state *, struct state *);
 */
static VOID
colorcomplement(nfa, cm, type, of, from, to)
struct nfa *nfa;
struct colormap *cm;
int type;
struct state *of;		/* complements of this guy's PLAIN outarcs */
struct state *from;
struct state *to;
{
	struct colordesc *cd;
	struct colordesc *end = CDEND(cm);
	color co;

	assert(of != from);
	for (cd = cm->cd, co = 0; cd < end && !VISERR(nfa->v); cd++, co++)
		if (!UNUSEDCOLOR(cd) && !(cd->flags&PSEUDO))
			if (findarc(of, PLAIN, co) == NULL)
				newarc(nfa, type, co, from, to);
}

#endif				/* ifdef COMPILE */
