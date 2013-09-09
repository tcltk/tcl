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
 * The BrodnikArray data structure is adapted from Andrej Brodnik et al.,
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

#define TclBrodnikArrayDeclare(T,scope)					\
									\
typedef struct BrodnikArray_ ## T BA_ ## T;				\
									\
struct BrodnikArray_ ## T {						\
    size_t		used;						\
    size_t		avail;						\
    unsigned int	dbused;						\
    unsigned int	dbavail;					\
    T **		store;						\
};									\
									\
scope	BA_ ## T *	BA_ ## T ## _Create();				\
scope	void		BA_ ## T ## _Destroy(BA_ ## T *a);		\
scope	size_t		BA_ ## T ## _Size(BA_ ## T *a);			\
scope	BA_ ## T *	BA_ ## T ## _Grow(BA_ ## T *a);			\
scope	BA_ ## T *	BA_ ## T ## _Shrink(BA_ ## T *a);		\
scope	void		BA_ ## T ## _Copy(T *p,	BA_ ## T *a);		\
scope	BA_ ## T *	BA_ ## T ## _Append(BA_ ## T *a,T **elemPtrPtr);\
scope	BA_ ## T *	BA_ ## T ## _Detach(BA_ ## T *a,T **elemPtrPtr);\
scope	T *		BA_ ## T ## _At(BA_ ## T *a,size_t index)

									
#define TclBrodnikArray(T) 						\
									\
TclBrodnikArrayDeclare(T,static);					\
TclBrodnikArrayDefine(T,static)

#define TclBrodnikArrayDefine(T,scope)					\
scope BA_ ## T *							\
BA_ ## T ## _Create()							\
{									\
    BA_ ## T *newPtr = ckalloc(sizeof(BA_ ## T));			\
									\
    newPtr->used = 0;							\
    newPtr->avail = 1;							\
    newPtr->dbused = 1;							\
    newPtr->dbavail = 1;						\
    newPtr->store = ckalloc(sizeof(T *));				\
    newPtr->store[0] = ckalloc(sizeof(T));				\
    return newPtr;							\
}									\
									\
scope void								\
BA_ ## T ## _Destroy(							\
    BA_ ## T *a)							\
{									\
    unsigned int i = a->dbused;						\
									\
    while (i--) {							\
	ckfree(a->store[i]);						\
    }									\
    ckfree(a->store);							\
    ckfree(a);								\
}									\
									\
scope size_t								\
BA_ ## T ## _Size(							\
    BA_ ## T *a)							\
{									\
    if (a == NULL) {							\
	return 0;							\
    }									\
    return a->used;							\
}									\
									\
scope BA_ ## T *							\
BA_ ## T ## _Grow(							\
    BA_ ## T *a)							\
{									\
    unsigned int dbsize = 1 << ((TclMSB(a->avail + 1) + 1) >> 1);	\
									\
    if (a->dbused == a->dbavail) {					\
	a->dbavail *= 2;						\
	a->store = ckrealloc(a->store, a->dbavail*sizeof(T *));		\
    }									\
    a->store[a->dbused] = ckalloc(dbsize * sizeof(T));			\
    a->dbused++;							\
    a->avail += dbsize;							\
    return a;								\
}									\
									\
scope BA_ ## T *							\
BA_ ## T ## _Shrink(							\
    BA_ ## T *a)							\
{									\
    unsigned int dbsize = 1 << ((TclMSB(a->used + 1) + 1) >> 1);	\
    a->dbused--;							\
    ckfree(a->store[a->dbused]);					\
    a->avail = a->used + dbsize;					\
    if (a->dbavail / a->dbused >= 4) {					\
	a->dbavail /= 2;						\
	a->store = ckrealloc(a->store, a->dbavail*sizeof(T *));		\
    }									\
    return a;								\
}									\
									\
scope void								\
BA_ ## T ## _Copy(							\
    T *p,								\
    BA_ ## T *a)							\
{									\
    unsigned int i = 0, n = 1, m = 0, hi, lo;				\
    if (a->used == 0) {							\
	return;								\
    }									\
    TclBAConvertIndices(a->used - 1, &hi, &lo);				\
    while (i < hi) {							\
	memcpy(p, a->store[i++], n * sizeof(T));			\
	p += n;								\
	if (m == 0) {							\
	    m = n; n *= 2; m += n;					\
	}								\
	m--;								\
    }									\
    memcpy(p, a->store[hi], (lo + 1) * sizeof(T));				\
}									\
									\
scope BA_ ## T *							\
BA_ ## T ## _Append(							\
    BA_ ## T *a,							\
    T **elemPtrPtr)							\
{									\
    unsigned int hi, lo;						\
									\
    if (a->used == a->avail) {						\
	a = BA_ ## T ## _Grow(a);					\
    }									\
    TclBAConvertIndices(a->used, &hi, &lo);				\
    *elemPtrPtr = a->store[hi] + lo;					\
    a->used++;								\
    return a;								\
}									\
									\
scope BA_ ## T *							\
BA_ ## T ## _Detach(							\
    BA_ ## T *a,							\
    T **elemPtrPtr)							\
{									\
    unsigned int hi, lo;						\
									\
    if (a->used == 0) {							\
	*elemPtrPtr = NULL;						\
	return a;							\
    }									\
    a->used--;								\
    TclBAConvertIndices(a->used, &hi, &lo);				\
    *elemPtrPtr = a->store[hi] + lo;					\
    if (lo || (hi == a->dbused - 1)) {					\
	return a;							\
    }									\
    return BA_ ## T ## _Shrink(a);					\
}									\
									\
scope T *								\
BA_ ## T ## _At(							\
    BA_ ## T *a,							\
    size_t index)							\
{									\
    unsigned int hi, lo;						\
									\
    if (index >= a->used) {						\
	return NULL;							\
    }									\
    TclBAConvertIndices(index, &hi, &lo);				\
    return a->store[hi] + lo;						\
}

