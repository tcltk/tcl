/*
 * tclStrIdxTree.h --
 *
 *	Declarations of string index tries and other primitives currently
 *  back-ported from tclSE.
 *
 * Copyright (c) 2016 Serg G. Brester (aka sebres)
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLSTRIDXTREE_H
#define _TCLSTRIDXTREE_H


/*
 * Main structures declarations of index tree and entry
 */

typedef struct TclStrIdxTree {
    struct TclStrIdx *firstPtr;
    struct TclStrIdx *lastPtr;
} TclStrIdxTree;

typedef struct TclStrIdx {
    struct TclStrIdxTree childTree;
    struct TclStrIdx *nextPtr;
    struct TclStrIdx *prevPtr;
    Tcl_Obj	*key;
    int		 length;
    ClientData	 value;
} TclStrIdx;


/*
 *----------------------------------------------------------------------
 *
 * TclUtfFindEqual, TclUtfFindEqualNC --
 *
 *  Find largest part of string cs in string cin (case sensitive and not).
 *
 * Results:
 *  Return position of UTF character in cs after last equal character.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static inline const char *
TclUtfFindEqual(
    register const char *cs,	/* UTF string to find in cin. */
    register const char *cse,	/* End of cs */
    register const char *cin,	/* UTF string will be browsed. */
    register const char *cine)	/* End of cin */
{
    register const char *ret = cs;
    Tcl_UniChar ch1, ch2;
    do {
	cs += TclUtfToUniChar(cs, &ch1);
	cin += TclUtfToUniChar(cin, &ch2);
	if (ch1 != ch2) break;
    } while ((ret = cs) < cse && cin < cine);
    return ret;
}

static inline const char *
TclUtfFindEqualNC(
    register const char *cs,	/* UTF string to find in cin. */
    register const char *cse,	/* End of cs */
    register const char *cin,	/* UTF string will be browsed. */
    register const char *cine,	/* End of cin */
    const char	     **cinfnd)	/* Return position in cin */
{
    register const char *ret = cs;
    Tcl_UniChar ch1, ch2;
    do {
	cs += TclUtfToUniChar(cs, &ch1);
	cin += TclUtfToUniChar(cin, &ch2);
	if (ch1 != ch2) {
	    ch1 = Tcl_UniCharToLower(ch1);
	    ch2 = Tcl_UniCharToLower(ch2);
	    if (ch1 != ch2) break;
	}
	*cinfnd = cin;
    } while ((ret = cs) < cse && cin < cine);
    return ret;
}

static inline const char *
TclUtfFindEqualNCInLwr(
    register const char *cs,	/* UTF string (in anycase) to find in cin. */
    register const char *cse,	/* End of cs */
    register const char *cin,	/* UTF string (in lowercase) will be browsed. */
    register const char *cine,	/* End of cin */
    const char	     **cinfnd)	/* Return position in cin */
{
    register const char *ret = cs;
    Tcl_UniChar ch1, ch2;
    do {
	cs += TclUtfToUniChar(cs, &ch1);
	cin += TclUtfToUniChar(cin, &ch2);
	if (ch1 != ch2) {
	    ch1 = Tcl_UniCharToLower(ch1);
	    if (ch1 != ch2) break;
	}
	*cinfnd = cin;
    } while ((ret = cs) < cse && cin < cine);
    return ret;
}

static inline const char *
TclUtfNext(
    register const char *src)	/* The current location in the string. */
{
    if (((unsigned char) *(src)) < 0xC0) {
	return ++src;
    } else {
	Tcl_UniChar ch;
	return src + TclUtfToUniChar(src, &ch);
    }
}


/*
 * Primitives to safe set, reset and free references.
 */

#define Tcl_UnsetObjRef(obj) \
  if (obj != NULL) { Tcl_DecrRefCount(obj); obj = NULL; }
#define Tcl_InitObjRef(obj, val) \
  obj = val; if (obj) { Tcl_IncrRefCount(obj); }
#define Tcl_SetObjRef(obj, val) \
if (1) { \
  Tcl_Obj *nval = val; \
  if (obj != nval) { \
    Tcl_Obj *prev = obj; \
    Tcl_InitObjRef(obj, nval); \
    if (prev != NULL) { Tcl_DecrRefCount(prev); }; \
  } \
}

/*
 * Prototypes of module functions.
 */

MODULE_SCOPE const char*
		    TclStrIdxTreeSearch(TclStrIdxTree **foundParent,
			TclStrIdx **foundItem, TclStrIdxTree *tree,
			const char *start, const char *end);

MODULE_SCOPE int    TclStrIdxTreeBuildFromList(TclStrIdxTree *idxTree,
			int lstc, Tcl_Obj **lstv, ClientData *values);

MODULE_SCOPE Tcl_Obj*
		    TclStrIdxTreeNewObj();

MODULE_SCOPE TclStrIdxTree*
		    TclStrIdxTreeGetFromObj(Tcl_Obj *objPtr);

#if 1

MODULE_SCOPE int    TclStrIdxTreeTestObjCmd(ClientData, Tcl_Interp *,
			int, Tcl_Obj *const objv[]);
#endif

#endif /* _TCLSTRIDXTREE_H */
