/*
 * Internal interface definitions, etc., for the regex package
 */



/*
 * Environmental customization.  It should not (I hope) be necessary to
 * alter the file you are now reading -- regcustom.h should handle it all,
 * given care here and elsewhere.
 */
#include "regcustom.h"



/*
 * Things that regcustom.h might override.
 */

/* standard header files (NULL is a reasonable indicator for them) */
#ifndef NULL
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#endif

/* assertions */
#ifndef assert
#include <assert.h>
#endif

/* voids */
#ifndef VOID
#define	VOID	void			/* for function return values */
#endif
#ifndef DISCARD
#define	DISCARD	VOID			/* for throwing values away */
#endif
#ifndef PVOID
#define	PVOID	VOID *			/* generic pointer */
#endif
#ifndef VS
#define	VS(x)	((PVOID)(x))		/* cast something to generic ptr */
#endif
#ifndef NOPARMS
#define	NOPARMS	VOID			/* for empty parm lists */
#endif

/* function-pointer declarator */
#ifndef FUNCPTR
#if __STDC__ >= 1
#define	FUNCPTR(name, args)	(*name)args
#else
#define	FUNCPTR(name, args)	(*name)()
#endif
#endif

/* memory allocation */
#ifndef MALLOC
#define	MALLOC(n)	malloc(n)
#endif
#ifndef REALLOC
#define	REALLOC(p, n)	realloc(VS(p), n)
#endif
#ifndef FREE
#define	FREE(p)		free(VS(p))
#endif

/* want size of a char in bits, and max value in bounded quantifiers */
#ifndef CHAR_BIT
#include <limits.h>
#endif
#ifndef _POSIX2_RE_DUP_MAX
#define	_POSIX2_RE_DUP_MAX	255	/* normally from <limits.h> */
#endif



/*
 * misc
 */

#define	NOTREACHED	0
#define	xxx		1

#define	DUPMAX	_POSIX2_RE_DUP_MAX
#define	INFINITY	(DUPMAX+1)

#define	REMAGIC	0xfed7		/* magic number for main struct */



/*
 * debugging facilities
 */
#ifdef REG_DEBUG
#define	FDEBUG(arglist)	{ if (v->eflags&REG_FTRACE) printf arglist; }
#define	MDEBUG(arglist)	{ if (v->eflags&REG_MTRACE) printf arglist; }
#else
#define	FDEBUG(arglist)	{}
#define	MDEBUG(arglist)	{}
#endif



/*
 * bitmap manipulation
 */
#define	UBITS	(CHAR_BIT * sizeof(unsigned))
#define	BSET(uv, sn)	((uv)[(sn)/UBITS] |= (unsigned)1 << ((sn)%UBITS))
#define	ISBSET(uv, sn)	((uv)[(sn)/UBITS] & ((unsigned)1 << ((sn)%UBITS)))



/*
 * Map a truth value into -1 for false, 1 for true.  This is so it is
 * possible to write compile-time assertions by declaring a dummy array
 * of this size.  (Why not #if?  Because sizeof is not available there.)
 */
#define	NEGIFNOT(x)	(2*!!(x) - 1)		/* !! ensures 0 or 1 */



/*
 * We dissect a chr into byts for colormap table indexing.  Here we define
 * a byt, which will be the same as a byte on most machines...  The exact
 * size of a byt is not critical, but about 8 bits is good, and extraction
 * of 8-bit chunks is sometimes especially fast.
 */
#ifndef BYTBITS
#define	BYTBITS	8		/* bits in a byt */
#endif
#define	BYTTAB	(1<<BYTBITS)	/* size of table with one entry per byt value */
#define	BYTMASK	(BYTTAB-1)	/* bit mask for byt */
#define	NBYTS	((CHRBITS+BYTBITS-1)/BYTBITS)
/* the definition of GETCOLOR(), below, assumes NBYTS <= 4 */



/*
 * As soon as possible, we map chrs into equivalence classes -- "colors" --
 * which are of much more manageable number.
 */
typedef short color;		/* colors of characters */
typedef int pcolor;		/* what color promotes to */
#define	COLORLESS	(-1)	/* impossible color */
#define	WHITE		0	/* default color, parent of all others */



/*
 * A colormap is a tree -- more precisely, a DAG -- indexed at each level
 * by a byt of the chr, to map the chr to a color efficiently.  Because
 * lower sections of the tree can be shared, it can exploit the usual
 * sparseness of such a mapping table.  The final tree is always NBYTS
 * levels deep (at present it may be shallower during construction, but
 * it is always "filled" to full depth at the end of that, using pointers
 * to "fill blocks" which are entirely WHITE in color).
 */

/* the tree itself */
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

/* internal per-color structure for the color machinery */
struct colordesc {
	uchr nchrs;		/* number of chars of this color */
	color sub;		/* open subcolor of this one, or NOSUB */
#		define	NOSUB	COLORLESS
	struct arc *arcs;	/* color chain */
#	define	UNUSEDCOLOR(cd)	((cd)->nchrs == 0 && (cd)->sub == NOSUB)
	int flags;
#		define	PSEUDO	1	/* pseudocolor, no real chars */
};

/* the color map itself */
struct colormap {
	int magic;
#		define	CMMAGIC	0x876
	struct vars *v;			/* for compile error reporting */
	color rest;
	int filled;			/* has it been filled? */
	size_t ncds;			/* number of colordescs */
	struct colordesc *cd;
#	define	CDEND(cm)	(&(cm)->cd[(cm)->ncds])
#		define	NINLINECDS	((size_t)10)
	struct colordesc cds[NINLINECDS];
	union tree tree[NBYTS];		/* tree top, plus fill blocks */
};

/* optimization magic to do fast chr->color mapping */
#define	B0(c)	((c) & BYTMASK)
#define	B1(c)	(((c)>>BYTBITS) & BYTMASK)
#define	B2(c)	(((c)>>(2*BYTBITS)) & BYTMASK)
#define	B3(c)	(((c)>>(3*BYTBITS)) & BYTMASK)
#if NBYTS == 1
#define	GETCOLOR(cm, c)	((cm)->tree->tcolor[B0(c)])
#endif
#if NBYTS == 2
#define	GETCOLOR(cm, c)	((cm)->tree->tptr[B1(c)]->tcolor[B0(c)])
#endif
#if NBYTS == 4
#define	GETCOLOR(cm, c)	((cm)->tree->tptr[B3(c)]->tptr[B2(c)]->tptr[B1(c)]->tcolor[B0(c)])
#endif



/*
 * Interface definitions for locale-interface functions in locale.c.
 * Multi-character collating elements (MCCEs) cause most of the trouble.
 */
struct cvec {
	int nchrs;		/* number of chrs */
	int chrspace;		/* number of chrs possible */
	chr *chrs;		/* pointer to vector of chrs */
	int nmcces;		/* number of MCCEs */
	int mccespace;		/* number of MCCEs possible */
	int nmccechrs;		/* number of chrs used for MCCEs */
	chr *mcces[1];		/* pointers to 0-terminated MCCEs */
				/* and both batches of chrs are on the end */
};

/* caution:  this value cannot be changed easily */
#define	MAXMCCE	2		/* length of longest MCCE */



/*
 * definitions for NFA internal representation
 *
 * Having a "from" pointer within each arc may seem redundant, but it
 * saves a lot of hassle.
 */
struct state;

struct arc {
	int type;
#		define	ARCFREE	'\0'
	color co;
	struct state *from;	/* where it's from (and contained within) */
	struct state *to;	/* where it's to */
	struct arc *outchain;	/* *from's outs chain or free chain */
#	define	freechain	outchain
	struct arc *inchain;	/* *to's ins chain */
	struct arc *colorchain;	/* color's arc chain */
};

struct arcbatch {		/* for bulk allocation of arcs */
	struct arcbatch *next;
#	define	ABSIZE	10
	struct arc a[ABSIZE];
};

struct state {
	int no;
#		define	FREESTATE	(-1)
	char flag;		/* marks special states */
	int nins;		/* number of inarcs */
	struct arc *ins;	/* chain of inarcs */
	int nouts;		/* number of outarcs */
	struct arc *outs;	/* chain of outarcs */
	struct arc *free;	/* chain of free arcs */
	struct state *tmp;	/* temporary for traversal algorithms */
	struct state *next;	/* chain for traversing all */
	struct state *prev;	/* back chain */
	struct arcbatch oas;	/* first arcbatch, avoid malloc in easy case */
};

struct nfa {
	struct state *pre;	/* pre-initial state */
	struct state *init;	/* initial state */
	struct state *final;	/* final state */
	struct state *post;	/* post-final state */
	int nstates;		/* for numbering states */
	struct state *states;	/* state-chain header */
	struct state *slast;	/* tail of the chain */
	struct state *free;	/* free list */
	struct colormap *cm;	/* the color map */
	color bos[2];		/* colors, if any, assigned to BOS and BOL */
	color eos[2];		/* colors, if any, assigned to EOS and EOL */
	struct vars *v;		/* simplifies compile error reporting */
	struct nfa *parent;	/* parent NFA, if any */
};



/*
 * definitions for compacted NFA
 */
struct carc {
	color co;		/* COLORLESS is list terminator */
	int to;			/* state number */
};

struct cnfa {
	int nstates;		/* number of states */
	int ncolors;		/* number of colors */
	int flags;
#		define	HASLACONS	01	/* uses lookahead constraints */
#		define	LEFTANCH	02	/* anchored on left */
	int pre;		/* setup state number */
	int post;		/* teardown state number */
	color bos[2];		/* colors, if any, assigned to BOS and BOL */
	color eos[2];		/* colors, if any, assigned to EOS and EOL */
	struct carc **states;	/* vector of pointers to outarc lists */
	struct carc *arcs;	/* the area for the lists */
};
#define	ZAPCNFA(cnfa)	((cnfa).nstates = 0)
#define	NULLCNFA(cnfa)	((cnfa).nstates == 0)



/*
 * definitions for subexpression tree
 * The intrepid code-reader is hereby warned that the subexpression tree
 * is kludge piled upon kludge, and is badly in need of rethinking.  Do
 * not expect it to look clean and sensible.
 */
struct subre {
	struct state *begin;	/* outarcs from here... */
	struct state *end;	/* ...ending in inarcs here */
	int prefer;		/* match preference */
#		define	NONEYET		00
#		define	LONGER		01
#		define	SHORTER		02
	int subno;		/* subexpression number (0 none, <0 backref) */
	short min;		/* min repetitions, for backref only */
	short max;		/* max repetitions, for backref only */
	struct rtree *tree;	/* substructure, if any */
	struct cnfa cnfa;	/* compacted NFA, if any */
};

struct rtree {
	char op;		/* operator:  '|', ',' */
	char flags;
#		define	INUSE	01	/* in use in the tree */
	short no;		/* index into retry memory */
	struct subre left;
	struct rtree *next;	/* for '|' */
	struct subre right;	/* for ',' */
	struct rtree *chain;	/* for bookkeeping and error cleanup */
};



/*
 * table of function pointers for generic manipulation functions
 * A regex_t's re_fns points to one of these.
 */
struct fns {
	VOID FUNCPTR(free, (regex_t *));
};



/*
 * the insides of a regex_t, hidden behind a void *
 */
struct guts {
	int magic;
#		define	GUTSMAGIC	0xfed9
	int cflags;		/* copy of compile flags */
	int info;		/* copy of re_info */
	size_t nsub;		/* copy of re_nsub */
	struct cnfa cnfa;
	struct rtree *tree;
	int ntree;
	struct colormap *cm;
	int FUNCPTR(compare, (CONST chr *, CONST chr *, size_t));
	struct subre *lacons;	/* lookahead-constraint vector */
	int nlacons;		/* size of lacons */
	int usedshorter;	/* used non-greedy quantifiers? */
};
