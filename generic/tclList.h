/*
 * tclList.h --
 *
 *	Declarations needed by Tcl internals that operate on lists.
 *
 * Contributions from Don Porter, NIST, 2013. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
 
#ifndef _TCLLIST
#define _TCLLIST

#include "tcl.h"

typedef struct Span Span;
typedef struct TclList TclList;
typedef struct TclListIndex TclListIndex;

struct Span {
    unsigned short int refCount;/* Number of users of the Span */
    unsigned short int first;	/* Index of objv for first element */
    unsigned short int last;	/* Index of objv after last element */
    unsigned short int size;	/* Number of elements allocated for objv */
    Tcl_Obj *objv[];		/* Storage for element refs */
};

struct TclList {
    unsigned short int refCount;/* Number of users of the TclList */
    unsigned short int first;	/* Index of first used Span ref */
    unsigned short int last;	/* Index after last used Span ref */
    unsigned short int size;	/* Number of Span refs allocated */
    size_t length;		/* Number of elements in whole list */
    Span *span[];		/* Storage for Span refs */
};

struct TclListIndex {
    TclList *listPtr;		/* The list in which this points */
    size_t index;		/* The overall index value into the list */
    unsigned short int span;	/* The Span ref we point into */
    unsigned short int elem;	/* The objv element we point to */
};

#undef LIST_MAX
#define LIST_MAX ((size_t)USHRT_MAX*(size_t)USHRT_MAX)

#undef LIST_SIZE
#define LIST_SIZE(numSpans) \
	(sizeof(TclList) + ((numSpans) - 1) * sizeof(Span *))

MODULE_SCOPE TclList *	TclListAllocate(Tcl_Interp *interp, size_t numElements);
MODULE_SCOPE int	TclListAppend(Tcl_Interp *interp, TclList **listPtrPtr,
				Tcl_Obj *objPtr);
MODULE_SCOPE TclList *	TclListCopy(TclList *listPtr);
MODULE_SCOPE size_t	TclListLength(TclList *listPtr);
MODULE_SCOPE void	TclListRelease(TclList *listPtr);


MODULE_SCOPE TclListIndex *	TclListIndexCreate(TclList *listPtr,
					size_t index);
MODULE_SCOPE Tcl_Obj *		TclListIndexGetValue(TclListIndex *indexPtr);
MODULE_SCOPE void		TclListIndexIncrement(TclListIndex *indexPtr);
MODULE_SCOPE void		TclListIndexRelease(TclListIndex *indexPtr);

#endif
