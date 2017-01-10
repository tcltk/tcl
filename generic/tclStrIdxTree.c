/*
 * tclStrIdxTree.c --
 *
 *	Contains the routines for managing string index tries in Tcl. 
 *
 *	This code is back-ported from the tclSE engine, by Serg G. Brester.
 *
 * Copyright (c) 2016 by Sergey G. Brester aka sebres. All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * -----------------------------------------------------------------------
 *
 * String index tries are prepaired structures used for fast greedy search of the string 
 * (index) by unique string prefix as key.
 *
 * Index tree build for two lists together can be explained in the following datagram
 * 
 * Lists:
 *
 *	{Januar Februar Maerz April Mai Juni Juli August September Oktober November Dezember}
 *	{Jnr Fbr Mrz Apr Mai Jni Jli Agt Spt Okt Nvb Dzb}
 *
 * Index-Tree:
 *
 *	j		-1	 *	...
 *	 anuar		 0	 *
 *	 u		-1	 *	a		-1
 *	  ni		 5	 *	 pril		 3
 *	  li		 6	 *	 ugust		 7
 *	 n		-1	 *	 gt		 7
 *	  r		 0	 *	s		 8
 *	  i		 5	 *	 eptember	 8
 *	 li		 6	 *	 pt		 8
 *	f		 1	 *	oktober		 9
 *	 ebruar		 1	 *	n		10
 *	 br		 1	 *	 ovember	10
 *	m		-1	 *	 vb		10
 *	 a		-1	 *	d		11
 *	  erz		 2	 *	 ezember	11
 *	  i		 4	 *	 zb		11
 *	 rz		 2	 *
 *	...
 *				  
 * Thereby value -1 shows pure group items (corresponding ambigous matches).
 *
 * StrIdxTree's are very fast, so:
 *    build of above-mentioned tree takes about 10 microseconds.
 *    search of string index in this tree takes fewer as 0.1 microseconds.
 *
 */

#include "tclInt.h"
#include "tclStrIdxTree.h"


/*
 *----------------------------------------------------------------------
 *
 * TclStrIdxTreeSearch --
 *
 *  Find largest part of string "start" in indexed tree (case sensitive).
 *
 *  Also used for building of string index tree.
 *
 * Results:
 *  Return position of UTF character in start after last equal character
 *  and found item (with parent).
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE const char*
TclStrIdxTreeSearch(
    TclStrIdxTree **foundParent, /* Return value of found sub tree (used for tree build) */
    TclStrIdx	  **foundItem,	 /* Return value of found item */
    TclStrIdxTree  *tree,	 /* Index tree will be browsed */
    const char	*start,		 /* UTF string to find in tree */
    const char	*end)		 /* End of string */
{
    TclStrIdxTree *parent = tree, *prevParent = tree;
    TclStrIdx  *item = tree->firstPtr, *prevItem = NULL;
    const char *s = start, *f, *cin, *cinf, *prevf;
    int offs = 0;

    if (item == NULL) {
	goto done;
    }

    /* search in tree */
    do {
	cin = TclGetString(item->key) + offs;
	f = TclUtfFindEqualNCInLwr(s, end, cin, cin + item->length, &cinf);
	/* if something was found */
	if (f > s) {
	    /* if whole string was found */
	    if (f >= end) {
		start = f;
		goto done;
	    };
	    /* set new offset and shift start string */
	    offs += cinf - cin;
	    s = f;
	    /* if match item, go deeper as long as possible */
	    if (offs >= item->length && item->childTree.firstPtr) {
		/* save previuosly found item (if not ambigous) for 
		 * possible fallback (few greedy match) */
		if (item->value != -1) {
		    prevf = f;
		    prevItem = item;
		    prevParent = parent;
		}
		parent = &item->childTree;
		item = item->childTree.firstPtr;
		continue;
	    }
	    /* no children - return this item and current chars found */
	    start = f;
	    goto done;
	}

	item = item->nextPtr;

    } while (item != NULL);

    /* fallback (few greedy match) not ambigous (has a value) */
    if (prevItem != NULL) {
	item = prevItem;
	parent = prevParent;
	start = prevf;
    }

done:

    if (foundParent)
	*foundParent = parent;
    if (foundItem)
	*foundItem = item;
    return start;
}

MODULE_SCOPE void 
TclStrIdxTreeFree(
    TclStrIdx *tree)
{
    while (tree != NULL) {
	TclStrIdx *t;
	Tcl_DecrRefCount(tree->key);
	if (tree->childTree.firstPtr != NULL) {
	    TclStrIdxTreeFree(tree->childTree.firstPtr);
	}
	t = tree, tree = tree->nextPtr;
	ckfree(t);
    }	
}

/*
 * Several bidirectional list primitives
 */
inline void 
TclStrIdxTreeInsertBranch(
    TclStrIdxTree *parent,
    register TclStrIdx *item,
    register TclStrIdx *child)
{
    if (parent->firstPtr == child)
	parent->firstPtr = item;
    if (parent->lastPtr == child)
	parent->lastPtr = item;
    if (item->nextPtr = child->nextPtr) {
	item->nextPtr->prevPtr = item;
	child->nextPtr = NULL;
    }
    if (item->prevPtr = child->prevPtr) {
	item->prevPtr->nextPtr = item;
	child->prevPtr = NULL;
    }
    item->childTree.firstPtr = child;
    item->childTree.lastPtr = child;
}

inline void
TclStrIdxTreeAppend(
    register TclStrIdxTree *parent,
    register TclStrIdx	   *item)
{
    if (parent->lastPtr != NULL) {
	parent->lastPtr->nextPtr = item;
    }
    item->prevPtr = parent->lastPtr;
    item->nextPtr = NULL;
    parent->lastPtr = item;
    if (parent->firstPtr == NULL) {
	parent->firstPtr = item;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * TclStrIdxTreeBuildFromList --
 *
 * Build or extend string indexed tree from tcl list.
 * 
 * Important: by multiple lists, optimal tree can be created only if list with
 * larger strings used firstly.
 *
 * Results:
 *  Returns a standard Tcl result.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TclStrIdxTreeBuildFromList(
    TclStrIdxTree *idxTree,
    int	       lstc,
    Tcl_Obj  **lstv)
{
    Tcl_Obj  **lwrv;
    int i, ret = TCL_ERROR;
    const char *s, *e, *f;
    TclStrIdx	*item;

    /* create lowercase reflection of the list keys */

    lwrv = ckalloc(sizeof(Tcl_Obj*) * lstc);
    if (lwrv == NULL) {
	return TCL_ERROR;
    }
    for (i = 0; i < lstc; i++) {
	lwrv[i] = Tcl_DuplicateObj(lstv[i]);
	if (lwrv[i] == NULL) {
	    return TCL_ERROR;
	}
	Tcl_IncrRefCount(lwrv[i]);
	lwrv[i]->length = Tcl_UtfToLower(TclGetString(lwrv[i]));
    }

    /* build index tree of the list keys */
    for (i = 0; i < lstc; i++) {
	TclStrIdxTree *foundParent = idxTree;
	e = s = TclGetString(lwrv[i]);
	e += lwrv[i]->length;

	/* ignore empty values (impossible to index it) */
	if (lwrv[i]->length == 0) continue;

	item = NULL;
	if (idxTree->firstPtr != NULL) {
	    TclStrIdx  *foundItem;
	    f = TclStrIdxTreeSearch(&foundParent, &foundItem,
		idxTree, s, e);
	    /* if common prefix was found */
	    if (f > s) {
		/* ignore element if fulfilled or ambigous */
		if (f == e) {
		    continue;
		}
		/* if shortest key was found with the same value,
		 * just replace its current key with longest key */
		if ( foundItem->value == i 
		  && foundItem->length < lwrv[i]->length
		  && foundItem->childTree.firstPtr == NULL
		) {
		    Tcl_SetObjRef(foundItem->key, lwrv[i]);
		    foundItem->length = lwrv[i]->length;
		    continue;
		}
		/* split tree (e. g. j->(jan,jun) + jul == j->(jan,ju->(jun,jul)) ) 
		 * but don't split by fulfilled child of found item ( ii->iii->iiii ) */
		if (foundItem->length != (f - s)) {
		    /* first split found item (insert one between parent and found + new one) */
		    item = ckalloc(sizeof(*item));
		    if (item == NULL) {
			goto done;
		    }
		    Tcl_InitObjRef(item->key, foundItem->key);
		    item->length = f - s;
		    /* set value or mark as ambigous if not the same value of both */
		    item->value = (foundItem->value == i) ? i : -1;
		    /* insert group item between foundParent and foundItem */
		    TclStrIdxTreeInsertBranch(foundParent, item, foundItem);
		    foundParent = &item->childTree;
		} else {
		    /* the new item should be added as child of found item */
		    foundParent = &foundItem->childTree;
		}
	    }
	}
	/* append item at end of found parent */
	item = ckalloc(sizeof(*item));
	if (item == NULL) {
	    goto done;
	}
	item->childTree.lastPtr = item->childTree.firstPtr = NULL;
	Tcl_InitObjRef(item->key, lwrv[i]);
	item->length = lwrv[i]->length;
	item->value = i;
	TclStrIdxTreeAppend(foundParent, item);
    };

    ret = TCL_OK;

done:

    if (lwrv != NULL) {
	for (i = 0; i < lstc; i++) {
	    Tcl_DecrRefCount(lwrv[i]);
	}
	ckfree(lwrv);
    }

    if (ret != TCL_OK) {
	if (idxTree->firstPtr != NULL) {
	    TclStrIdxTreeFree(idxTree->firstPtr);
	}
    }

    return ret;
}


static void
StrIdxTreeObj_DupIntRepProc(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void
StrIdxTreeObj_FreeIntRepProc(Tcl_Obj *objPtr);
static void
StrIdxTreeObj_UpdateStringProc(Tcl_Obj *objPtr);

Tcl_ObjType StrIdxTreeObjType = {
    "str-idx-tree",		    /* name */
    StrIdxTreeObj_FreeIntRepProc,   /* freeIntRepProc */
    StrIdxTreeObj_DupIntRepProc,    /* dupIntRepProc */
    StrIdxTreeObj_UpdateStringProc, /* updateStringProc */
    NULL			    /* setFromAnyProc */
};

MODULE_SCOPE Tcl_Obj* 
TclStrIdxTreeNewObj()
{
    Tcl_Obj *objPtr = Tcl_NewObj();
    objPtr->internalRep.twoPtrValue.ptr1 = NULL;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    objPtr->typePtr = &StrIdxTreeObjType;
    /* return tree root in internal representation */
    return objPtr;
}

static void
StrIdxTreeObj_DupIntRepProc(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr)
{
    /* follow links (smart pointers) */
    if ( srcPtr->internalRep.twoPtrValue.ptr1 != NULL
      && srcPtr->internalRep.twoPtrValue.ptr2 == NULL
    ) {
	srcPtr = (Tcl_Obj*)srcPtr->internalRep.twoPtrValue.ptr1;
    }
    /* create smart pointer to it (ptr1 != NULL, ptr2 = NULL) */
    Tcl_InitObjRef(((Tcl_Obj *)copyPtr->internalRep.twoPtrValue.ptr1), 
	srcPtr);
    copyPtr->internalRep.twoPtrValue.ptr2 = NULL;
    copyPtr->typePtr = &StrIdxTreeObjType;
}

static void
StrIdxTreeObj_FreeIntRepProc(Tcl_Obj *objPtr)
{
    /* follow links (smart pointers) */
    if ( objPtr->internalRep.twoPtrValue.ptr1 != NULL
      && objPtr->internalRep.twoPtrValue.ptr2 == NULL
    ) {
	/* is a link */
	Tcl_UnsetObjRef(((Tcl_Obj *)objPtr->internalRep.twoPtrValue.ptr1));
    } else {
	/* is a tree */
	TclStrIdxTree *tree = (TclStrIdxTree*)&objPtr->internalRep.twoPtrValue.ptr1;
	if (tree->firstPtr != NULL) {
	    TclStrIdxTreeFree(tree->firstPtr);
	}
	objPtr->internalRep.twoPtrValue.ptr1 = NULL;
	objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    }
    objPtr->typePtr = NULL;
};

static void
StrIdxTreeObj_UpdateStringProc(Tcl_Obj *objPtr)
{
    /* currently only dummy empty string possible */
    objPtr->length = 0;
    objPtr->bytes = tclEmptyStringRep;
};

MODULE_SCOPE TclStrIdxTree *
TclStrIdxTreeGetFromObj(Tcl_Obj *objPtr) {
    /* follow links (smart pointers) */
    if (objPtr->typePtr != &StrIdxTreeObjType) {
	return NULL;
    }
    if ( objPtr->internalRep.twoPtrValue.ptr1 != NULL
      && objPtr->internalRep.twoPtrValue.ptr2 == NULL
    ) {
	objPtr = (Tcl_Obj*)objPtr->internalRep.twoPtrValue.ptr1;
    }
    /* return tree root in internal representation */
    return (TclStrIdxTree*)&objPtr->internalRep.twoPtrValue.ptr1;
}

/*
 * Several debug primitives
 */
#if 1

void 
TclStrIdxTreePrint(
    Tcl_Interp *interp,
    TclStrIdx  *tree,
    int offs)
{
    Tcl_Obj *obj[2];
    const char *s;
    Tcl_InitObjRef(obj[0], Tcl_NewStringObj("::puts", -1));
    while (tree != NULL) {
	s = TclGetString(tree->key) + offs;
	Tcl_InitObjRef(obj[1], Tcl_ObjPrintf("%*s%.*s\t:%d", 
		offs, "", tree->length - offs, s, tree->value));
	Tcl_PutsObjCmd(NULL, interp, 2, obj);
	Tcl_UnsetObjRef(obj[1]);
	if (tree->childTree.firstPtr != NULL) {
	    TclStrIdxTreePrint(interp, tree->childTree.firstPtr, tree->length);
	}
	tree = tree->nextPtr;
    }
    Tcl_UnsetObjRef(obj[0]);
}


MODULE_SCOPE int
TclStrIdxTreeTestObjCmd(
    ClientData clientData, Tcl_Interp *interp,
    int objc, Tcl_Obj *const objv[])
{
    const char *cs, *cin, *ret;

    static const char *const options[] = {
	"index", "puts-index", "findequal",
	NULL
    };
    enum optionInd {
	O_INDEX,  O_PUTS_INDEX, O_FINDEQUAL
    };
    int optionIndex;

    if (objc < 2) {
	Tcl_SetResult(interp, "wrong # args", TCL_STATIC);
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], options, 
	"option", 0, &optionIndex) != TCL_OK) {
	Tcl_SetErrorCode(interp, "CLOCK", "badOption",
		Tcl_GetString(objv[1]), NULL);
	return TCL_ERROR;
    }
    switch (optionIndex) {
    case O_FINDEQUAL:
	if (objc < 4) {
	    Tcl_SetResult(interp, "wrong # args", TCL_STATIC);
	    return TCL_ERROR;
	}
	cs = TclGetString(objv[2]);
	cin = TclGetString(objv[3]);
	ret = TclUtfFindEqual(
	    cs, cs + objv[1]->length, cin, cin + objv[2]->length);
	Tcl_SetObjResult(interp, Tcl_NewIntObj(ret - cs));
    break;
    case O_INDEX:
    case O_PUTS_INDEX:

    if (1) {
	Tcl_Obj **lstv;
	int i, lstc;
	TclStrIdxTree idxTree = {NULL, NULL};
	i = 1;
	while (++i < objc) {
	    if (TclListObjGetElements(interp, objv[i], 
		    &lstc, &lstv) != TCL_OK) {
		return TCL_ERROR;
	    };
	    TclStrIdxTreeBuildFromList(&idxTree, lstc, lstv);
	}
	if (optionIndex == O_PUTS_INDEX) {
	    TclStrIdxTreePrint(interp, idxTree.firstPtr, 0);
	}
	TclStrIdxTreeFree(idxTree.firstPtr);
    }
    break;
    }

    return TCL_OK;
}

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
