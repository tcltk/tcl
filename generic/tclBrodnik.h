/*
 * tclBrodnik.h --
 *
 *	This file contains the implementation of a BrodnikArray.
 *
 * Contributions from Don Porter, NIST, 2013. (not subject to US copyright)
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * The routines that follow implement a BrodnikArray of T elements, where
 * T is the type of the elements to be stored, which is the argument of
 * the macro.  
 *
 * The BroknikArray data structure is adapted from Andrej Brodnik et al.,
 * "Resizable Arrays in Optimal Time and Space", from the Proceedings of
 * the 1999 Workshop on Algorithms and Data Structures, Lecture Notes
 * in Computer Science (LNCS) vol. 1663, pp. 37-48.
 *
 * The BrodnikArray is an indexed sequence of values.  The routines
 * Append() and Detach() permit inserting and deleting an element at
 * the end of the sequence, with allocated memory growing and shrinking
 * as needed.  The At(index) routine returns a (T *) pointing to the
 * index'th element in the sequence.  Indices are size_t values and
 * array elements have indices starting with 0.  Through the pointer
 * elements may be read or (over)written.  These routines provide
 * stack-like access as well as random access to the elements stored
 * in the array.
 *
 * The memory allocation management is such that for N elements stored
 * in the array, the amount of memory allocated, but not used
 * is O(sqrt(N)).  This is more efficient than the traditional Tcl
 * array growth algorithm that has O(N) memory waste.  The longest
 * contiguous span of allocated memory is also O(sqrt(N)) so longer
 * sequences should be possible without failure due to lack of a
 * long enough contiguous span of memory.  The main drawback of the
 * BrodnikArray is that it is a two-level storage structure, so two
 * pointer derefences are required to get an element, when a plain
 * array gets the job done in only one.  The other potential trouble
 * is the need for frequent reallocs as small arrays grow, though that's
 * not hard to remedy if it's a problem in practice.
 */

#define TclBrodnikArray(T)						\
									\
typedef struct BrodnikArray_ ## T BA_ ## T;				\
									\
struct BrodnikArray_ ## T {						\
    size_t	used;							\
    size_t	avail;							\
    size_t	dbused;							\
    size_t	dbavail;						\
    T *		store[1];						\
};									\
									\
static BA_ ## T *							\
BA_ ## T ## _Create()							\
{									\
    BA_ ## T *newPtr = ckalloc(sizeof(BA_ ## T));			\
									\
    newPtr->used = 0;							\
    newPtr->avail = 1;							\
    newPtr->dbused = 1;							\
    newPtr->dbavail = 1;						\
    newPtr->store[0] = ckalloc(sizeof(T));				\
    return newPtr;							\
}									\
									\
static void								\
BA_ ## T ## _Destroy(							\
    BA_ ## T *a)							\
{									\
    size_t i = a->dbused;						\
									\
    while (i--) {							\
	ckfree(a->store[i]);						\
    }									\
    ckfree(a);								\
}									\
									\
static BA_ ## T *							\
BA_ ## T ## _Grow(							\
    BA_ ## T *a)							\
{									\
    size_t dbsize = 1 << ((TclMSB(a->avail + 1) + 1) >> 1);		\
									\
    if (a->dbused == a->dbavail) {					\
	a->dbavail *= 2;						\
	a = ckrealloc(a, sizeof(BA_ ## T)+(a->dbavail-1)*sizeof(T *));	\
    }									\
    a->store[a->dbused] = ckalloc(dbsize * sizeof(T));			\
    a->dbused++;							\
    a->avail += dbsize;							\
    return a;								\
}									\
									\
static BA_ ## T *							\
BA_ ## T ## _Shrink(							\
    BA_ ## T *a)							\
{									\
    a->dbused--;							\
    ckfree(a->store[a->dbused]);					\
    if (a->dbavail / a->dbused >= 4) {					\
	a->dbavail /= 2;						\
	a = ckrealloc(a, sizeof(BA_ ## T)+(a->dbavail-1)*sizeof(T *));	\
    }									\
    return a;								\
}									\
									\
static BA_ ## T *							\
BA_ ## T ## _Append(							\
    BA_ ## T *a,							\
    T *elemPtr)								\
{									\
    size_t hi, lo;							\
									\
    if (a->used == a->avail) {						\
	a = BA_ ## T ## _Grow(a);					\
    }									\
    TclBAConvertIndices(a->used, &hi, &lo);				\
    a->store[hi][lo] = *elemPtr;					\
    a->used++;								\
    return a;								\
}									\
									\
static BA_ ## T *							\
BA_ ## T ## _Detach(							\
    BA_ ## T *a,							\
    T *elemPtr)								\
{									\
    size_t hi, lo;							\
									\
    if (a->used == 0) {							\
	return a;							\
    }									\
    a->used--;								\
    TclBAConvertIndices(a->used, &hi, &lo);				\
    *elemPtr = a->store[hi][lo];					\
    if (lo || (hi == a->dbused - 1)) {					\
	return a;							\
    }									\
    return BA_ ## T ## _Shrink(a);					\
}									\
									\
static T *								\
BA_ ## T ## _At(							\
    BA_ ## T *a,							\
    size_t index)							\
{									\
    size_t hi, lo;							\
									\
    if (index >= a->used) {						\
	return NULL;							\
    }									\
    TclBAConvertIndices(index, &hi, &lo);				\
    return a->store[hi] + lo;						\
}

