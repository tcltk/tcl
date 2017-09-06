/*
 * tclHAMT.h --
 *
 *	This file contains the declarations of the types and routines
 *	of the hash array map trie .
 *
 * Contributions from Don Porter, NIST, 2015. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TCL_HAMT_H
#define TCL_HAMT_H

#include "tclInt.h"

/*
 * The point of this data structure is to store a map from keys to
 * values.  So let's first establish what keys and values are. We
 * want to be usable by creators with different perspectives on that,
 * so we build in some configurability by letting a creator describe
 * what we need to know about keys and values via defining their "type".
 *
 * We will store values (received as ClientData) and some kinds of
 * values will want to be notified when we start holding on to them
 * and conversely when we declare we will not be using them any more.
 * They can define routines of the following types to be called back
 * at those times.
 *
 * We will also store keys (also received as ClientData), so they also
 * have the opporunity to define these callbacks.
 *
 * If the creator has no need of these notifications, the type or its
 * fields can be left NULL.
 */

typedef		ClientData (TclMakeRefProc) (ClientData value);
typedef		void (TclDropRefProc) (ClientData value);

/*
 * We also need to operate on keys. First we need to be able to check
 * when two key values are the same according to the logic of our
 * creator.  We make the assumption that identical key values are seen
 * as the same key by any plausible creator logic.  But when key values
 * are distinct, they may still represent "the same" key when interpreted
 * as the creator intends. If so, the creator must give us a callback
 * routine we can use to check that.
 */

typedef		int (TclIsEqualProc) (ClientData x, ClientData y);



typedef		size_t (TclHashProc) (ClientData key);

typedef struct {
    TclHashProc		*hashProc;
    TclIsEqualProc	*isEqualProc;
    TclMakeRefProc	*makeRefProc;
    TclDropRefProc	*dropRefProc;
} TclHAMTKeyType;

typedef struct {
    TclMakeRefProc	*makeRefProc;
    TclDropRefProc	*dropRefProc;
} TclHAMTValType;

/*
 * Opaque pointers to define the protoypes.
 */

typedef struct HAMT *TclHAMT;

/*
 * Interface procedure declarations.
 */

MODULE_SCOPE TclHAMT		TclHAMTCreate(const TclHAMTKeyType *ktPtr,
				    const TclHAMTValType *vtPtr);
MODULE_SCOPE void		TclHAMTDelete(TclHAMT hamt);
MODULE_SCOPE TclHAMT		TclHAMTInsert(TclHAMT hamt, ClientData key,
				    ClientData value, ClientData *valuePtr);
MODULE_SCOPE TclHAMT		TclHAMTRemove(TclHAMT hamt, ClientData key,
				    ClientData *valuePtr);
MODULE_SCOPE ClientData		TclHAMTFetch(TclHAMT hamt, ClientData key);

#endif /* TCL_HAMT_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
