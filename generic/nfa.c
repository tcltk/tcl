/*
 * nfa.c --
 *
 *	Regexp package file:
 *	NFA utilities.  One or two things that technically ought to be 
 *	in here are actually in color.c, thanks to some incestuous 
 *	relationships in the color chains.
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
 * SCCS: @(#) nfa.c 1.10 98/02/17 18:42:41
 */

#define	NISERR()	VISERR(nfa->v)


/*
 - newnfa - set up an NFA
 * Caution:  colormap must be set up already.
 ^ static struct nfa *newnfa(struct vars *, struct nfa *);
 */
static struct nfa *		/* the NFA, or NULL */
newnfa(v, parent)
struct vars *v;
struct nfa *parent;		/* NULL if primary NFA */
{
	struct nfa *nfa;

	nfa = (struct nfa *)ckalloc(sizeof(struct nfa));
	if (nfa == NULL)
		return NULL;

	nfa->states = NULL;
	nfa->slast = NULL;
	nfa->free = NULL;
	nfa->nstates = 0;
	nfa->v = v;
	nfa->bos[0] = nfa->bos[1] = COLORLESS;
	nfa->eos[0] = nfa->eos[1] = COLORLESS;
	nfa->post = newfstate(nfa, '@');	/* number 0 */
	nfa->pre = newfstate(nfa, '>');		/* number 1 */
	nfa->parent = parent;

	nfa->init = newstate(nfa);		/* may become invalid later */
	nfa->final = newstate(nfa);
	if (ISERR()) {
		freenfa(nfa);
		return NULL;
	}
	rainbow(nfa, nfa->v->cm, PLAIN, COLORLESS, nfa->pre, nfa->init);
	newarc(nfa, '^', 1, nfa->pre, nfa->init);
	newarc(nfa, '^', 0, nfa->pre, nfa->init);
	rainbow(nfa, nfa->v->cm, PLAIN, COLORLESS, nfa->final, nfa->post);
	newarc(nfa, '$', 1, nfa->final, nfa->post);
	newarc(nfa, '$', 0, nfa->final, nfa->post);

	if (ISERR()) {
		freenfa(nfa);
		return NULL;
	}
	return nfa;
}

/*
 - freenfa - free an entire NFA
 ^ static VOID freenfa(struct nfa *);
 */
static VOID
freenfa(nfa)
struct nfa *nfa;
{
	struct state *s;

	while ((s = nfa->states) != NULL) {
		s->nins = s->nouts = 0;		/* don't worry about arcs */
		freestate(nfa, s);
	}
	while ((s = nfa->free) != NULL) {
		nfa->free = s->next;
		destroystate(nfa, s);
	}

	nfa->slast = NULL;
	nfa->nstates = -1;
	nfa->pre = NULL;
	nfa->post = NULL;
	ckfree((char *)nfa);
}

/*
 - newfstate - allocate an NFA state, with specified flag value
 ^ static struct state *newfstate(struct nfa *, int flag);
 */
static struct state *		/* NULL on error */
newfstate(nfa, flag)
struct nfa *nfa;
int flag;
{
	struct state *s;
	int i;

	if (nfa->free != NULL) {
		s = nfa->free;
		nfa->free = s->next;
	} else {
		s = (struct state *)ckalloc(sizeof(struct state));
		if (s == NULL) {
			VERR(nfa->v, REG_ESPACE);
			return NULL;
		}

		/* memleak (CCS). */
		
		s->oas.next = NULL;
		s->free = &s->oas.a[0];
		for (i = 0; i < ABSIZE; i++) {
			s->oas.a[i].type = 0;
			s->oas.a[i].freechain = &s->oas.a[i+1];
		}
		s->oas.a[ABSIZE-1].freechain = NULL;
	}

	assert(nfa->nstates >= 0);
	s->no = nfa->nstates++;
	s->flag = (char)flag;
	if (nfa->states == NULL)
		nfa->states = s;
	s->nins = 0;
	s->ins = NULL;
	s->nouts = 0;
	s->outs = NULL;
	s->tmp = NULL;
	s->next = NULL;
	if (nfa->slast != NULL) {
		assert(nfa->slast->next == NULL);
		nfa->slast->next = s;
	}
	s->prev = nfa->slast;
	nfa->slast = s;
	return s;
}

/*
 - newstate - allocate an ordinary NFA state
 ^ static struct state *newstate(struct nfa *);
 */
static struct state *		/* NULL on error */
newstate(nfa)
struct nfa *nfa;
{
	return newfstate(nfa, 0);
}

/*
 - dropstate - delete a state's inarcs and outarcs and free it
 ^ static VOID dropstate(struct nfa *, struct state *);
 */
static VOID
dropstate(nfa, s)
struct nfa *nfa;
struct state *s;
{
	struct arc *a;

	while ((a = s->ins) != NULL)
		freearc(nfa, a);
	while ((a = s->outs) != NULL)
		freearc(nfa, a);
	freestate(nfa, s);
}

/*
 - freestate - free a state, which has no in-arcs or out-arcs
 ^ static VOID freestate(struct nfa *, struct state *);
 */
static VOID
freestate(nfa, s)
struct nfa *nfa;
struct state *s;
{
	assert(s != NULL);
	assert(s->nins == 0 && s->nouts == 0);

	s->no = FREESTATE;
	s->flag = 0;
	if (s->next != NULL)
		s->next->prev = s->prev;
	else {
		assert(s == nfa->slast);
		nfa->slast = s->prev;
	}
	if (s->prev != NULL)
		s->prev->next = s->next;
	else {
		assert(s == nfa->states);
		nfa->states = s->next;
	}
	s->prev = NULL;
	s->next = nfa->free;	/* don't delete it, put it on the free list */
	nfa->free = s;
}

/*
 - destroystate - really get rid of an already-freed state
 ^ static VOID destroystate(struct nfa *, struct state *);
 */
static VOID
destroystate(nfa, s)
struct nfa *nfa;
struct state *s;
{
	struct arcbatch *ab;
	struct arcbatch *abnext;

	assert(s->no == FREESTATE);
	for (ab = s->oas.next; ab != NULL; ab = abnext) {
		abnext = ab->next;
		ckfree((char *)ab);
	}
	s->ins = NULL;
	s->outs = NULL;
	s->next = NULL;
	ckfree((char *)s);
}

/*
 - newarc - set up a new arc within an NFA
 ^ static VOID newarc(struct nfa *, int, pcolor, struct state *, 
 ^	struct state *);
 */
static VOID
newarc(nfa, t, co, from, to)
struct nfa *nfa;
int t;
pcolor co;
struct state *from;
struct state *to;
{
	struct arc *a;

	assert(from != NULL && to != NULL);

	/* check for duplicates */
	for (a = from->outs; a != NULL; a = a->outchain)
		if (a->type == t && a->co == co && a->to == to)
			return;

	a = allocarc(nfa, from);
	if (NISERR())
		return;
	assert(a != NULL);

	a->type = t;
	a->co = (color) co;
	a->to = to;
	a->from = from;

	/*
	 * Put the new arc on the beginning, not the end, of the chains.
	 * Not only is this easier, it has the very useful side effect that 
	 * deleting the most-recently-added arc is the cheapest case rather
	 * than the most expensive one.
	 */
	a->inchain = to->ins;
	to->ins = a;
	a->outchain = from->outs;
	from->outs = a;

	from->nouts++;
	to->nins++;

	if (COLORED(a) && nfa->parent == NULL)
		colorchain(nfa->v->cm, a);

	return;
}

/*
 - allocarc - allocate a new out-arc within a state
 ^ static struct arc *allocarc(struct nfa *, struct state *);
 */
static struct arc *		/* NULL for failure */
allocarc(nfa, s)
struct nfa *nfa;
struct state *s;
{
	struct arc *a;
	struct arcbatch *new;
	int i;

	/* if none at hand, get more */
	if (s->free == NULL) {
		new = (struct arcbatch *)ckalloc(sizeof(struct arcbatch));
		if (new == NULL) {
			VERR(nfa->v, REG_ESPACE);
			return NULL;
		}
		new->next = s->oas.next;
		s->oas.next = new;

		for (i = 0; i < ABSIZE; i++) {
			new->a[i].type = 0;
			new->a[i].freechain = &new->a[i+1];
		}
		new->a[ABSIZE-1].freechain = NULL;
		s->free = &new->a[0];
	}
	assert(s->free != NULL);

	a = s->free;
	s->free = a->freechain;
	return a;
}

/*
 - freearc - free an arc
 ^ static VOID freearc(struct nfa *, struct arc *);
 */
static VOID
freearc(nfa, victim)
struct nfa *nfa;
struct arc *victim;
{
	struct state *from = victim->from;
	struct state *to = victim->to;
	struct arc *a;

	assert(victim->type != 0);

	/* take it off color chain if necessary */
	if (COLORED(victim) && nfa->parent == NULL)
		uncolorchain(nfa->v->cm, victim);

	/* take it off source's out-chain */
	assert(from != NULL);
	assert(from->outs != NULL);
	a = from->outs;
	if (a == victim)		/* simple case:  first in chain */
		from->outs = victim->outchain;
	else {
		for (; a != NULL && a->outchain != victim; a = a->outchain)
			continue;
		assert(a != NULL);
		a->outchain = victim->outchain;
	}
	from->nouts--;

	/* take it off target's in-chain */
	assert(to != NULL);
	assert(to->ins != NULL);
	a = to->ins;
	if (a == victim)		/* simple case:  first in chain */
		to->ins = victim->inchain;
	else {
		for (; a != NULL && a->inchain != victim; a = a->inchain)
			continue;
		assert(a != NULL);
		a->inchain = victim->inchain;
	}
	to->nins--;

	/* clean up and place on free list */
	victim->type = 0;
	victim->from = NULL;		/* precautions... */
	victim->to = NULL;
	victim->inchain = NULL;
	victim->outchain = NULL;
	victim->freechain = from->free;
	from->free = victim;
}

/*
 - findarc - find arc, if any, from given source with given type and color
 * If there is more than one such arc, the result is random.
 ^ static struct arc *findarc(struct state *, int, pcolor);
 */
static struct arc *
findarc(s, type, co)
struct state *s;
int type;
pcolor co;
{
	struct arc *a;

	for (a = s->outs; a != NULL; a = a->outchain)
		if (a->type == type && a->co == co)
			return a;
	return NULL;
}

/*
 - cparc - allocate a new arc within an NFA, copying details from old one
 ^ static VOID cparc(struct nfa *, struct arc *, struct state *, 
 ^ 	struct state *);
 */
static VOID
cparc(nfa, oa, from, to)
struct nfa *nfa;
struct arc *oa;
struct state *from;
struct state *to;
{
	newarc(nfa, oa->type, oa->co, from, to);
}

/*
 - moveins - move all in arcs of a state to another state
 * You might think this could be done better by just updating the
 * existing arcs, and you would be right if it weren't for the desire
 * for duplicate suppression, which makes it easier to just make new
 * ones to exploit the suppression built into newarc.
 ^ static VOID moveins(struct nfa *, struct state *, struct state *);
 */
static VOID
moveins(nfa, old, new)
struct nfa *nfa;
struct state *old;
struct state *new;
{
	struct arc *a;

	assert(old != new);

	while ((a = old->ins) != NULL) {
		cparc(nfa, a, a->from, new);
		freearc(nfa, a);
	}
	assert(old->nins == 0);
	assert(old->ins == NULL);
}

/*
 - copyins - copy all in arcs of a state to another state
 ^ static VOID copyins(struct nfa *, struct state *, struct state *);
 */
static VOID
copyins(nfa, old, new)
struct nfa *nfa;
struct state *old;
struct state *new;
{
	struct arc *a;

	assert(old != new);

	for (a = old->ins; a != NULL; a = a->inchain)
		cparc(nfa, a, a->from, new);
}

/*
 - moveouts - move all out arcs of a state to another state
 ^ static VOID moveouts(struct nfa *, struct state *, struct state *);
 */
static VOID
moveouts(nfa, old, new)
struct nfa *nfa;
struct state *old;
struct state *new;
{
	struct arc *a;

	assert(old != new);

	while ((a = old->outs) != NULL) {
		cparc(nfa, a, new, a->to);
		freearc(nfa, a);
	}
}

/*
 - copyouts - copy all out arcs of a state to another state
 ^ static VOID copyouts(struct nfa *, struct state *, struct state *);
 */
static VOID
copyouts(nfa, old, new)
struct nfa *nfa;
struct state *old;
struct state *new;
{
	struct arc *a;

	assert(old != new);

	for (a = old->outs; a != NULL; a = a->outchain)
		cparc(nfa, a, new, a->to);
}

/*
 - cloneouts - copy out arcs of a state to another state pair, modifying type
 ^ static VOID cloneouts(struct nfa *, struct state *, struct state *,
 ^ 	struct state *, int);
 */
static VOID
cloneouts(nfa, old, from, to, type)
struct nfa *nfa;
struct state *old;
struct state *from;
struct state *to;
int type;
{
	struct arc *a;

	assert(old != from);

	for (a = old->outs; a != NULL; a = a->outchain)
		newarc(nfa, type, a->co, from, to);
}

/*
 - delsub - delete a sub-NFA, updating subre pointers if necessary
 * This uses a recursive traversal of the sub-NFA, marking already-seen
 * states using their tmp pointer.
 ^ static VOID delsub(struct nfa *, struct state *, struct state *);
 */
static VOID
delsub(nfa, lp, rp)
struct nfa *nfa;
struct state *lp;	/* the sub-NFA goes from here... */
struct state *rp;	/* ...to here, *not* inclusive */
{
	assert(lp != rp);

	rp->tmp = rp;			/* mark end */

	deltraverse(nfa, lp, lp);
	assert(lp->nouts == 0 && rp->nins == 0);	/* did the job */
	assert(lp->no != FREESTATE && rp->no != FREESTATE);	/* no more */

	rp->tmp = NULL;			/* unmark end */
	lp->tmp = NULL;			/* and begin, marked by deltraverse */
}

/*
 - deltraverse - the recursive heart of delsub
 * This routine's basic job is to destroy all out-arcs of the state.
 ^ static VOID deltraverse(struct nfa *, struct state *, struct state *);
 */
static VOID
deltraverse(nfa, leftend, s)
struct nfa *nfa;
struct state *leftend;
struct state *s;
{
	struct arc *a;
	struct state *to;

	if (s->nouts == 0)
		return;			/* nothing to do */
	if (s->tmp != NULL)
		return;			/* already in progress */

	s->tmp = s;			/* mark as in progress */

	while ((a = s->outs) != NULL) {
		to = a->to;
		deltraverse(nfa, leftend, to);
		assert(to->nouts == 0 || to->tmp != NULL);
		freearc(nfa, a);
		if (to->nins == 0 && to->tmp == NULL) {
			assert(to->nouts == 0);
			freestate(nfa, to);
		}
	}

	assert(s->no != FREESTATE);	/* we're still here */
	assert(s == leftend || s->nins != 0);	/* and still reachable */
	assert(s->nouts == 0);		/* but have no outarcs */

	s->tmp = NULL;			/* we're done here */
}

/*
 - dupnfa - duplicate sub-NFA
 * Another recursive traversal, this time using tmp to point to duplicates
 * as well as mark already-seen states.  (You knew there was a reason why
 * it's a state pointer, didn't you? :-))
 ^ static VOID dupnfa(struct nfa *, struct state *, struct state *, 
 ^ 	struct state *, struct state *);
 */
static VOID
dupnfa(nfa, start, stop, from, to)
struct nfa *nfa;
struct state *start;		/* duplicate starting here */
struct state *stop;		/* and stopping here */
struct state *from;		/* stringing duplicate from here */
struct state *to;		/* to here */
{
	if (start == stop) {
		newarc(nfa, EMPTY, 0, from, to);
		return;
	}

	stop->tmp = to;
	duptraverse(nfa, start, from);
	/* done, except for clearing out the tmp pointers */

	stop->tmp = NULL;
	cleartraverse(nfa, start);
}

/*
 - duptraverse - recursive heart of dupnfa
 ^ static VOID duptraverse(struct nfa *, struct state *, struct state *);
 */
static VOID
duptraverse(nfa, s, stmp)
struct nfa *nfa;
struct state *s;
struct state *stmp;		/* s's duplicate, or NULL */
{
	struct arc *a;

	if (s->tmp != NULL)
		return;		/* already done */

	s->tmp = (stmp == NULL) ? newstate(nfa) : stmp;
	if (s->tmp == NULL) {
		assert(NISERR());
		return;
	}

	for (a = s->outs; a != NULL && !NISERR(); a = a->outchain) {
		duptraverse(nfa, a->to, (struct state *)NULL);
		assert(a->to->tmp != NULL);
		cparc(nfa, a, s->tmp, a->to->tmp);
	}
}

/*
 - cleartraverse - recursive cleanup for algorithms that leave tmp ptrs set
 ^ static VOID cleartraverse(struct nfa *, struct state *);
 */
static VOID
cleartraverse(nfa, s)
struct nfa *nfa;
struct state *s;
{
	struct arc *a;

	if (s->tmp == NULL)
		return;
	s->tmp = NULL;

	for (a = s->outs; a != NULL; a = a->outchain)
		cleartraverse(nfa, a->to);
}

/*
 - specialcolors - fill in special colors for an NFA
 ^ static VOID specialcolors(struct nfa *);
 */
static VOID
specialcolors(nfa)
struct nfa *nfa;
{
	/* false colors for BOS, BOL, EOS, EOL */
	if (nfa->parent == NULL) {
		nfa->bos[0] = pseudocolor(nfa->v->cm);
		nfa->bos[1] = pseudocolor(nfa->v->cm);
		nfa->eos[0] = pseudocolor(nfa->v->cm);
		nfa->eos[1] = pseudocolor(nfa->v->cm);
	} else {
		assert(nfa->parent->bos[0] != COLORLESS);
		nfa->bos[0] = nfa->parent->bos[0];
		assert(nfa->parent->bos[1] != COLORLESS);
		nfa->bos[1] = nfa->parent->bos[1];
		assert(nfa->parent->eos[0] != COLORLESS);
		nfa->eos[0] = nfa->parent->eos[0];
		assert(nfa->parent->eos[1] != COLORLESS);
		nfa->eos[1] = nfa->parent->eos[1];
	}
}

/*
 - optimize - optimize an NFA
 ^ static VOID optimize(struct nfa *);
 */
static VOID
optimize(nfa)
struct nfa *nfa;
{
	int verbose = (nfa->v->cflags&REG_PROGRESS) ? 1 : 0;
	int info;

	if (verbose)
		printf("\ninitial cleanup:\n");
	cleanup(nfa);		/* may simplify situation */
	if (nfa->v->cflags&REG_PROGRESS)
		dumpnfa(nfa, stdout);
	if (verbose)
		printf("\nempties:\n");
	fixempties(nfa);	/* get rid of EMPTY arcs */
	if (verbose)
		printf("\nconstraints:\n");
	pullback(nfa);		/* pull back constraints backward */
	pushfwd(nfa);		/* push fwd constraints forward */
	if (verbose)
		printf("\nfinal cleanup:\n");
	cleanup(nfa);		/* final tidying */
	info = analyze(nfa->v, nfa);	/* and analysis */
	if (nfa->parent == NULL)
		nfa->v->re->re_info |= info;
}

/*
 - pullback - pull back constraints backward to (with luck) eliminate them
 ^ static VOID pullback(struct nfa *);
 */
static VOID
pullback(nfa)
struct nfa *nfa;
{
	struct state *s;
	struct state *nexts;
	struct arc *a;
	struct arc *nexta;
	int progress;

	/* find and pull until there are no more */
	do {
		progress = 0;
		for (s = nfa->states; s != NULL && !NISERR(); s = nexts) {
			nexts = s->next;
			for (a = s->outs; a != NULL && !NISERR(); a = nexta) {
				nexta = a->outchain;
				if (a->type == '^' || a->type == BEHIND)
					if (pull(nfa, a))
						progress = 1;
				assert(nexta == NULL || s->no != FREESTATE);
			}
		}
		if (progress && (nfa->v->cflags&REG_PROGRESS))
			dumpnfa(nfa, stdout);
	} while (progress && !NISERR());
	if (NISERR())
		return;

	for (a = nfa->pre->outs; a != NULL; a = nexta) {
		nexta = a->outchain;
		if (a->type == '^') {
			assert(a->co == 0 || a->co == 1);
			newarc(nfa, PLAIN, nfa->bos[a->co], a->from, a->to);
			freearc(nfa, a);
		}
	}
}

/*
 - pull - pull a back constraint backward past its source state
 * A significant property of this function is that it deletes at most
 * one state -- the constraint's from state -- and only if the constraint
 * was that state's last outarc.
 ^ static int pull(struct nfa *, struct arc *);
 */
static int			/* 0 couldn't, 1 could */
pull(nfa, con)
struct nfa *nfa;
struct arc *con;
{
	struct state *from = con->from;
	struct state *to = con->to;
	struct arc *a;
	struct arc *nexta;
	struct state *s;

	if (from == to) {	/* circular constraint is pointless */
		freearc(nfa, con);
		return 1;
	}
	if (from->flag)		/* can't pull back beyond start */
		return 0;
	if (from->nins == 0) {	/* unreachable */
		freearc(nfa, con);
		return 1;
	}

	/* first, clone from state if necessary to aVOID other outarcs */
	if (from->nouts > 1) {
		s = newstate(nfa);
		if (NISERR())
			return 0;
		assert(to != from);		/* con is not an inarc */
		copyins(nfa, from, s);		/* duplicate inarcs */
		cparc(nfa, con, s, to);		/* move constraint arc */
		freearc(nfa, con);
		from = s;
		con = from->outs;
	}
	assert(from->nouts == 1);

	/* propagate the constraint into the from state's inarcs */
	for (a = from->ins; a != NULL; a = nexta) {
		nexta = a->inchain;
		switch (combine(con, a)) {
		case INCOMPATIBLE:	/* destroy the arc */
			freearc(nfa, a);
			break;
		case SATISFIED:		/* no action needed */
			break;
		case COMPATIBLE:	/* swap the two arcs, more or less */
			s = newstate(nfa);
			if (NISERR())
				return 0;
			cparc(nfa, a, s, to);		/* anticipate move */
			cparc(nfa, con, a->from, s);
			if (NISERR())
				return 0;
			freearc(nfa, a);
			break;
		default:
			assert(NOTREACHED);
			break;
		}
	}

	/* remaining inarcs, if any, incorporate the constraint */
	moveins(nfa, from, to);
	dropstate(nfa, from);		/* will free the constraint */
	return 1;
}

/*
 - pushfwd - push forward constraints forward to (with luck) eliminate them
 ^ static VOID pushfwd(struct nfa *);
 */
static VOID
pushfwd(nfa)
struct nfa *nfa;
{
	struct state *s;
	struct state *nexts;
	struct arc *a;
	struct arc *nexta;
	int progress;

	/* find and push until there are no more */
	do {
		progress = 0;
		for (s = nfa->states; s != NULL && !NISERR(); s = nexts) {
			nexts = s->next;
			for (a = s->ins; a != NULL && !NISERR(); a = nexta) {
				nexta = a->inchain;
				if (a->type == '$' || a->type == AHEAD)
					if (push(nfa, a))
						progress = 1;
				assert(nexta == NULL || s->no != FREESTATE);
			}
		}
		if (progress && (nfa->v->cflags&REG_PROGRESS))
			dumpnfa(nfa, stdout);
	} while (progress && !NISERR());
	if (NISERR())
		return;

	for (a = nfa->post->ins; a != NULL; a = nexta) {
		nexta = a->inchain;
		if (a->type == '$') {
			assert(a->co == 0 || a->co == 1);
			newarc(nfa, PLAIN, nfa->eos[a->co], a->from, a->to);
			freearc(nfa, a);
		}
	}
}

/*
 - push - push a forward constraint forward past its destination state
 * A significant property of this function is that it deletes at most
 * one state -- the constraint's to state -- and only if the constraint
 * was that state's last inarc.
 ^ static int push(struct nfa *, struct arc *);
 */
static int			/* 0 couldn't, 1 could */
push(nfa, con)
struct nfa *nfa;
struct arc *con;
{
	struct state *from = con->from;
	struct state *to = con->to;
	struct arc *a;
	struct arc *nexta;
	struct state *s;

	if (to == from) {	/* circular constraint is pointless */
		freearc(nfa, con);
		return 1;
	}
	if (to->flag)		/* can't push forward beyond end */
		return 0;
	if (to->nouts == 0) {	/* dead end */
		freearc(nfa, con);
		return 1;
	}

	/* first, clone to state if necessary to aVOID other inarcs */
	if (to->nins > 1) {
		s = newstate(nfa);
		if (NISERR())
			return 0;
		copyouts(nfa, to, s);		/* duplicate outarcs */
		cparc(nfa, con, from, s);	/* move constraint */
		freearc(nfa, con);
		to = s;
		con = to->ins;
	}
	assert(to->nins == 1);

	/* propagate the constraint into the to state's outarcs */
	for (a = to->outs; a != NULL; a = nexta) {
		nexta = a->outchain;
		switch (combine(con, a)) {
		case INCOMPATIBLE:	/* destroy the arc */
			freearc(nfa, a);
			break;
		case SATISFIED:		/* no action needed */
			break;
		case COMPATIBLE:	/* swap the two arcs, more or less */
			s = newstate(nfa);
			if (NISERR())
				return 0;
			cparc(nfa, con, s, a->to);	/* anticipate move */
			cparc(nfa, a, from, s);
			if (NISERR())
				return 0;
			freearc(nfa, a);
			break;
		default:
			assert(NOTREACHED);
			break;
		}
	}

	/* remaining outarcs, if any, incorporate the constraint */
	moveouts(nfa, to, from);
	dropstate(nfa, to);		/* will free the constraint */
	return 1;
}

/*
 - combine - constraint lands on an arc, what happens?
 ^ #def	INCOMPATIBLE	1	// destroys arc
 ^ #def	SATISFIED	2	// constraint satisfied
 ^ #def	COMPATIBLE	3	// compatible but not satisfied yet
 ^ static int combine(struct arc *, struct arc *);
 */
static int
combine(con, a)
struct arc *con;
struct arc *a;
{
#	define	CA(ct,at)	(((ct)<<CHAR_BIT) | (at))

	switch (CA(con->type, a->type)) {
	case CA('^', PLAIN):		/* newlines are handled separately */
	case CA('$', PLAIN):
		return INCOMPATIBLE;
	case CA(AHEAD, PLAIN):		/* color constraints meet colors */
	case CA(BEHIND, PLAIN):
		if (con->co == a->co)
			return SATISFIED;
		return INCOMPATIBLE;
	case CA('^', '^'):		/* collision, similar constraints */
	case CA('$', '$'):
	case CA(AHEAD, AHEAD):
	case CA(BEHIND, BEHIND):
		if (con->co == a->co)		/* true duplication */
			return SATISFIED;
		return INCOMPATIBLE;
	case CA('^', BEHIND):		/* collision, dissimilar constraints */
	case CA(BEHIND, '^'):
	case CA('$', AHEAD):
	case CA(AHEAD, '$'):
		return INCOMPATIBLE;
	case CA('^', '$'):		/* constraints passing each other */
	case CA('^', AHEAD):
	case CA(BEHIND, '$'):
	case CA(BEHIND, AHEAD):
	case CA('$', '^'):
	case CA('$', BEHIND):
	case CA(AHEAD, '^'):
	case CA(AHEAD, BEHIND):
	case CA('^', LACON):
	case CA(BEHIND, LACON):
	case CA('$', LACON):
	case CA(AHEAD, LACON):
		return COMPATIBLE;
	}
	assert(NOTREACHED);
	return INCOMPATIBLE;		/* keep compiler from complaining */
}

/*
 - fixempties - get rid of EMPTY arcs
 ^ static VOID fixempties(struct nfa *);
 */
static VOID
fixempties(nfa)
struct nfa *nfa;
{
	struct state *s;
	struct state *nexts;
	struct arc *a;
	struct arc *nexta;
	int progress;

	/* find and eliminate empties until there are no more */
	do {
		progress = 0;
		for (s = nfa->states; s != NULL && !NISERR(); s = nexts) {
			nexts = s->next;
			for (a = s->outs; a != NULL && !NISERR(); a = nexta) {
				nexta = a->outchain;
				if (a->type == EMPTY && unempty(nfa, a))
					progress = 1;
				assert(nexta == NULL || s->no != FREESTATE);
			}
		}
		if (progress && (nfa->v->cflags&REG_PROGRESS))
			dumpnfa(nfa, stdout);
	} while (progress && !NISERR());
}

/*
 - unempty - optimize out an EMPTY arc, if possible
 * Actually, as it stands this function always succeeds, but the return
 * value is kept with an eye on possible future changes.
 ^ static int unempty(struct nfa *, struct arc *);
 */
static int			/* 0 couldn't, 1 could */
unempty(nfa, a)
struct nfa *nfa;
struct arc *a;
{
	struct state *from = a->from;
	struct state *to = a->to;
	int usefrom;		/* work on from, as opposed to to? */

	assert(a->type == EMPTY);
	assert(from != nfa->pre && to != nfa->post);

	if (from == to) {		/* vacuous loop */
		freearc(nfa, a);
		return 1;
	}

	/* decide which end to work on */
	usefrom = 1;			/* default:  attack from */
	if (from->nouts > to->nins)
		usefrom = 0;
	else if (from->nouts == to->nins) {
		/* decide on secondary issue:  move/copy fewest arcs */
		if (from->nins > to->nouts)
			usefrom = 0;
	}
		
	freearc(nfa, a);
	if (usefrom) {
		if (from->nouts == 0) {
			/* was the state's only outarc */
			moveins(nfa, from, to);
			freestate(nfa, from);
		} else
			copyins(nfa, from, to);
	} else {
		if (to->nins == 0) {
			/* was the state's only inarc */
			moveouts(nfa, to, from);
			freestate(nfa, to);
		} else
			copyouts(nfa, to, from);
	}

	return 1;
}

/*
 - cleanup - clean up NFA after optimizations
 ^ static VOID cleanup(struct nfa *);
 */
static VOID
cleanup(nfa)
struct nfa *nfa;
{
	struct state *s;
	struct state *nexts;
	int n;

	/* clear out unreachable or dead-end states */
	/* use pre to mark reachable, then post to mark can-reach-post */
	markreachable(nfa, nfa->pre, (struct state *)NULL, nfa->pre);
	markcanreach(nfa, nfa->post, nfa->pre, nfa->post);
	for (s = nfa->states; s != NULL; s = nexts) {
		nexts = s->next;
		if (s->tmp != nfa->post && !s->flag)
			dropstate(nfa, s);
	}
	assert(nfa->post->nins == 0 || nfa->post->tmp == nfa->post);
	cleartraverse(nfa, nfa->pre);
	assert(nfa->post->nins == 0 || nfa->post->tmp == NULL);
	/* the nins==0 (final unreachable) case will be caught later */

	/* renumber surviving states */
	n = 0;
	for (s = nfa->states; s != NULL; s = s->next)
		s->no = n++;
	nfa->nstates = n;
}

/*
 - markreachable - recursive marking of reachable states
 ^ static VOID markreachable(struct nfa *, struct state *, struct state *,
 ^ 	struct state *);
 */
static VOID
markreachable(nfa, s, okay, mark)
struct nfa *nfa;
struct state *s;
struct state *okay;		/* consider only states with this mark */
struct state *mark;		/* the value to mark with */
{
	struct arc *a;

	if (s->tmp != okay)
		return;
	s->tmp = mark;

	for (a = s->outs; a != NULL; a = a->outchain)
		markreachable(nfa, a->to, okay, mark);
}

/*
 - markcanreach - recursive marking of states which can reach here
 ^ static VOID markcanreach(struct nfa *, struct state *, struct state *,
 ^ 	struct state *);
 */
static VOID
markcanreach(nfa, s, okay, mark)
struct nfa *nfa;
struct state *s;
struct state *okay;		/* consider only states with this mark */
struct state *mark;		/* the value to mark with */
{
	struct arc *a;

	if (s->tmp != okay)
		return;
	s->tmp = mark;

	for (a = s->ins; a != NULL; a = a->inchain)
		markcanreach(nfa, a->from, okay, mark);
}

/*
 - analyze - ascertain potentially-useful facts about an optimized NFA
 ^ static int analyze(struct vars *, struct nfa *);
 */
static int			/* re_info bits to be ORed in */
analyze(v, nfa)
struct vars *v;
struct nfa *nfa;
{
	struct arc *a;
	struct arc *aa;

	/* can the NFA match the empty string? */
	for (a = nfa->pre->outs; a != NULL; a = a->outchain)
		for (aa = a->to->outs; aa != NULL; aa = aa->outchain)
			if (aa->to == nfa->post)
				return REG_UEMPTYMATCH;
	return 0;
}

/*
 - isempty - is a sub-NFA composed only of EMPTY arcs?
 * Somewhat limited implementation, makes assumptions.
 ^ static int isempty(struct state *, struct state *);
 */
static int
isempty(begin, end)
struct state *begin;
struct state *end;
{
	struct state *s;

	for (s = begin; s != end; s = s->outs->to) {
		if (s->nouts != 1)
			return 0;
		assert(s->outs != NULL);
		if (s->outs->type != EMPTY)
			return 0;
	}

	return 1;
}

/*
 - compact - compact an NFA
 ^ static VOID compact(struct vars *, struct nfa *, struct cnfa *);
 */
static VOID
compact(v, nfa, cnfa)
struct vars *v;
struct nfa *nfa;
struct cnfa *cnfa;
{
	struct state *s;
	struct arc *a;
	size_t nstates;
	size_t narcs;
	struct carc *ca;
	struct carc *first;

	assert (!ISERR());

	nstates = 0;
	narcs = 0;
	for (s = nfa->states; s != NULL; s = s->next) {
		nstates++;
		narcs += s->nouts + 1;
	}

	cnfa->states = (struct carc **)ckalloc(nstates * sizeof(struct carc *));
	cnfa->arcs = (struct carc *)ckalloc(narcs * sizeof(struct carc));
	if (cnfa->states == NULL || cnfa->arcs == NULL) {
		if (cnfa->states != NULL)
			ckfree((char *)cnfa->states);
		if (cnfa->arcs != NULL)
			ckfree((char *)cnfa->arcs);
		ERR(REG_ESPACE);
		return;
	}
	cnfa->nstates = nstates;
	cnfa->pre = nfa->pre->no;
	cnfa->post = nfa->post->no;
	cnfa->bos[0] = nfa->bos[0];
	cnfa->bos[1] = nfa->bos[1];
	cnfa->eos[0] = nfa->eos[0];
	cnfa->eos[1] = nfa->eos[1];
	cnfa->ncolors = maxcolor(v->cm) + 1;
	cnfa->haslacons = 0;
 	cnfa->leftanch = 1;		/* tentatively */

	ca = cnfa->arcs;
	for (s = nfa->states; s != NULL; s = s->next) {
		assert((size_t) s->no < nstates);
		cnfa->states[s->no] = ca;
		first = ca;
		for (a = s->outs; a != NULL; a = a->outchain)
			switch (a->type) {
			case PLAIN:
				ca->co = a->co;
				ca->to = a->to->no;
				ca++;
				break;
			case LACON:
				assert(s->no != cnfa->pre);
				ca->co = (color) (a->co + cnfa->ncolors);
				ca->to = a->to->no;
				ca++;
				cnfa->haslacons = 1;
				break;
			default:
				assert(NOTREACHED);
				break;
			}
		carcsort(first, ca-1);
		ca->co = COLORLESS;
		ca->to = 0;
		ca++;
	}
	assert(ca == &cnfa->arcs[narcs]);
	assert(cnfa->nstates != 0);

	for (a = nfa->pre->outs; a != NULL; a = a->outchain)
		if (a->type == PLAIN && a->co != nfa->bos[0] &&
			a->co != nfa->bos[1])
		    cnfa->leftanch = 0;
 }

/*
 - carcsort - sort compacted-NFA arcs by color
 * Really dumb algorithm, but if the list is long enough for that to matter,
 * you're in real trouble anyway.
 ^ static VOID carcsort(struct carc *, struct carc *);
 */
static VOID
carcsort(first, last)
struct carc *first;
struct carc *last;
{
	struct carc *p;
	struct carc *q;
	struct carc tmp;

	if (last - first <= 1)
		return;

	for (p = first; p <= last; p++)
		for (q = p; q <= last; q++)
			if (p->co > q->co ||
					(p->co == q->co && p->to > q->to)) {
				assert(p != q);
				tmp = *p;
				*p = *q;
				*q = tmp;
			}
}

/*
 - freecnfa - free a compacted NFA
 ^ static VOID freecnfa(struct cnfa *, int);
 */
static VOID
freecnfa(cnfa, dynalloc)
struct cnfa *cnfa;
int dynalloc;			/* is the cnfa struct itself dynamic? */
{
	assert(cnfa->nstates != 0);	/* not empty already */
	cnfa->nstates = 0;
	ckfree((char *)cnfa->states);
	ckfree((char *)cnfa->arcs);
	if (dynalloc)
		ckfree((char *)cnfa);
}
/*
 - dumpnfa - dump an NFA in human-readable form
 ^ static VOID dumpnfa(struct nfa *, FILE *);
 */
static VOID
dumpnfa(nfa, f)
struct nfa *nfa;
FILE *f;
{
}
/*
 - dumpcnfa - dump a compacted NFA in human-readable form
 ^ static VOID dumpcnfa(struct cnfa *, FILE *);
 */
static VOID
dumpcnfa(cnfa, f)
struct cnfa *cnfa;
FILE *f;
{
}
