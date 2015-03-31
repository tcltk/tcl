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
 * Opaque pointers to define the protoypes.
 */

typedef struct HAMT *TclHAMT;

typedef		size_t (TclHashProc) (ClientData key);
typedef		int (TclIsEqualProc) (ClientData x, ClientData y);
typedef		ClientData (TclMakeRefProc) (ClientData value);
typedef		void (TclDropRefProc) (ClientData value);

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
