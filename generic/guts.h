/*
 * guts.h --
 *
 * 	Regexp package file:  Misc. utilities.
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
 * SCCS: @(#) guts.h 1.7 98/01/21 14:33:04
 */

#include "tclInt.h"

#define	NOTREACHED	0
#define	xxx		1

#ifndef _POSIX2_RE_DUP_MAX
#define	_POSIX2_RE_DUP_MAX	255
#endif
#define	DUPMAX	_POSIX2_RE_DUP_MAX
#define	INFINITY	(DUPMAX+1)

/* bitmap manipulation */
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
 *
 * Changes in several places are needed to handle an increase in MAXBYTS.
 * Those places check whether MAXBYTS is larger than they expect.
 */
#ifndef BYTBITS
#define	BYTBITS	8		/* bits in a byt */
#endif
#define	BYTTAB	(1<<BYTBITS)	/* size of table with one entry per byt value */
#define	BYTMASK	(BYTTAB-1)	/* bit mask for byt */
#define	NBYTS	((CHRBITS+BYTBITS-1)/BYTBITS)
#define	MAXBYTS	8		/* maximum NBYTS the code can handle */

/*
 * As soon as possible, we map chrs into equivalence classes -- "colors" --
 * which are of much more manageable number.
 */
typedef short color;		/* colors of characters */
typedef int pcolor;		/* what color promotes to */
#define	COLORLESS	(-1)	/* impossible color */
#define	WHITE		0	/* default color, parent of all others */
struct colormap;		/* forward def for master type */

/*
 * Interface definitions for locale-interface functions in locale.c
 */
struct cvec {
	int nchrs;		/* number of chrs */
	int chrspace;		/* number of chrs possible */
	chr *chrs;		/* pointer to vector of chrs */
	int nces;		/* number of multichr collating elements */
	int cespace;		/* number of CEs possible */
	int ncechrs;		/* number of chrs used for CEs */
	chr *ces[1];		/* pointers to 0-terminated CEs */
				/* and both batches of chrs are on the end */
};

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
	int haslacons;		/* does it use lookahead constraints? */
 	int leftanch;		/* is it anchored on the left? */
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
	short no;		/* node numbering */
	struct subre left;
	struct rtree *next;	/* for '|' */
	struct subre right;	/* for ',' */
};

/*
 * table of function pointers for generic manipulation functions
 * A regex_t's re_fns points to one of these.
 */
struct fns {
	VOID (*free) _ANSI_ARGS_((regex_t *));
};

/*
 * the insides of a regex_t, hidden behind a void *
 */
struct guts {
	int magic;
#		define	GUTSMAGIC	0xfed9
	int cflags;		/* copy of compile flags */
	int info;		/* copy of re_info */
	int nsub;		/* copy of re_nsub */
	struct cnfa cnfa;
	struct rtree *tree;
	int ntree;
	struct colormap *cm;
	int (*compare) _ANSI_ARGS_((CONST chr *, CONST chr *, size_t));
				/* string-compare function */
	struct subre *lacons;	/* lookahead-constraint vector */
	int nlacons;		/* size of lacons */
	int usedshorter;	/* used non-greedy quantifiers? */
};
