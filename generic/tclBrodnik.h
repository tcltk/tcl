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
typedef struct BrodnikPointer_ ## T BP_ ## T;				\
									\
struct BrodnikPointer_ ## T {						\
    T *			ptr;						\
    BA_ ## T *		array;						\
    unsigned int	hi;						\
    unsigned int	lo;						\
    unsigned int	dbsize;						\
    unsigned int	count;						\
};									\
									\
struct BrodnikArray_ ## T {						\
    unsigned int	hi;						\
    unsigned int	lo;						\
    unsigned int	dbsize;						\
    unsigned int	count;						\
    unsigned int	dbused;						\
    unsigned int	dbavail;					\
    T **		store;						\
};									\
									\
scope	BA_ ## T *	BA_ ## T ## _Create();				\
scope	void		BA_ ## T ## _Destroy(BA_ ## T *a);		\
scope	size_t		BA_ ## T ## _Size(BA_ ## T *a);			\
scope	void		BA_ ## T ## _Copy(T *p,	BA_ ## T *a);		\
scope	T *		BA_ ## T ## _Append(BA_ ## T *a);		\
scope	T *		BA_ ## T ## _Detach(BA_ ## T *a);		\
scope	T *		BA_ ## T ## _Get(BA_ ## T *a, size_t index,	\
			    BP_ ## T *p);				\
scope	T *		BA_ ## T ## _At(BA_ ## T *a, size_t index);	\
									\
scope	T *		BA_ ## T ## _First(BA_ ## T *a, BP_ ## T *p);	\
scope	T *		BP_ ## T ## _Next(BP_ ## T *p)

									
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
    newPtr->hi = 0;							\
    newPtr->lo = 0;							\
    newPtr->dbsize = 1;							\
    newPtr->count = 0;							\
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
    return TclBAInvertIndices(a->hi, a->lo);				\
}									\
									\
scope void								\
BA_ ## T ## _Grow(							\
    BA_ ## T *a)							\
{									\
    if (a->dbused == a->dbavail) {					\
	a->dbavail *= 2;						\
	a->store = ckrealloc(a->store, a->dbavail*sizeof(T *));		\
    }									\
    a->store[a->dbused] = ckalloc(a->dbsize * sizeof(T));		\
    a->dbused++;							\
}									\
									\
scope void								\
BA_ ## T ## _Shrink(							\
    BA_ ## T *a)							\
{									\
    a->dbused--;							\
    ckfree(a->store[a->dbused]);					\
    if (a->dbavail / a->dbused >= 4) {					\
	a->dbavail /= 2;						\
	a->store = ckrealloc(a->store, a->dbavail*sizeof(T *));		\
    }									\
}									\
									\
scope void								\
BA_ ## T ## _Copy(							\
    T *p,								\
    BA_ ## T *a)							\
{									\
    unsigned int i = 0, n = 1, m = 0;					\
    if (a->hi == 0) {							\
	return;								\
    }									\
    while (i < a->hi) {							\
	memcpy(p, a->store[i++], n * sizeof(T));			\
	p += n;								\
	if (m == 0) {							\
	    m = n; n *= 2; m += n;					\
	}								\
	m--;								\
    }									\
    if (a->lo) {							\
	memcpy(p, a->store[a->hi], a->lo * sizeof(T));			\
    }									\
}									\
									\
scope T *								\
BA_ ## T ## _Append(							\
    BA_ ## T *a)							\
{									\
    T *elemPtr;								\
    if (a->hi == a->dbused) {						\
	BA_ ## T ## _Grow(a);						\
    }									\
    elemPtr = a->store[a->hi] + a->lo;					\
    a->lo++;								\
    if (a->lo == a->dbsize) {						\
	a->lo = 0;							\
	a->hi++;							\
	if (a->count == 0) {						\
	    a->count = a->dbsize;					\
	    a->dbsize *= 2;						\
	    a->count += a->dbsize;					\
	}								\
	a->count--;							\
    }									\
    return elemPtr;							\
}									\
									\
scope T *								\
BA_ ## T ## _Detach(							\
    BA_ ## T *a)							\
{									\
    T *elemPtr;								\
    if (a->hi == 0) {							\
	return NULL;							\
    }									\
    if (a->lo) {							\
	a->lo--;							\
    } else {								\
	a->hi--;							\
	a->count++;							\
	if (a->count == 3 * (a->dbsize / 2)) {				\
	    a->count = 0;						\
	    a->dbsize /= 2;						\
	}								\
	a->lo = a->dbsize - 1;						\
    }									\
    elemPtr = a->store[a->hi] + a->lo;					\
    if (a->lo == 0 && (a->hi != a->dbused - 1)) {			\
	BA_ ## T ## _Shrink(a);						\
    }									\
    return elemPtr;							\
}									\
									\
scope T *								\
BA_ ## T ## _Get(							\
    BA_ ## T *a,							\
    size_t index,							\
    BP_ ## T *p)							\
{									\
    unsigned int hi, lo;						\
									\
    TclBAConvertIndices(index, &hi, &lo);				\
    if (hi > a->hi || (hi == a->hi && lo >= a->lo)) {			\
	if (p) {p->ptr = NULL;}						\
	return NULL;							\
    }									\
    if (p) {								\
	size_t plus2 = hi + 2;						\
	int n = TclMSB(plus2) - 1;					\
	p->array = a;							\
	p->hi = hi;							\
	p->lo = lo;							\
	p->dbsize = 1 << (n + ((plus2 >> n) & 1));			\
	p->count = 3 * p->dbsize - 3 - hi;				\
	p->ptr = a->store[hi] + lo;					\
    }									\
    return a->store[hi] + lo;						\
}									\
									\
scope T *								\
BA_ ## T ## _At(							\
    BA_ ## T *a,							\
    size_t index)							\
{									\
    return BA_ ## T ## _Get(a, index, NULL);				\
}									\
									\
scope T *								\
BA_ ## T ## _First(							\
    BA_ ## T *a,							\
    BP_ ## T *p)							\
{									\
    p->array = a;							\
    p->hi = 0;								\
    p->lo = 0;								\
    p->dbsize = 1;							\
    p->count = 0;							\
    p->ptr = (a->hi) ? a->store[0] : NULL;				\
    return p->ptr;							\
}									\
									\
scope T *								\
BP_ ## T ## _Next(							\
    BP_ ## T *p)							\
{									\
    if (p->ptr) {							\
	p->lo++;							\
	if (p->lo < p->dbsize) {					\
	    p->ptr++;							\
	} else {							\
	    p->lo = 0;							\
	    p->hi++;							\
	    p->ptr = p->array->store[p->hi];				\
	    if (p->count == 0) {					\
		p->count = p->dbsize;					\
		p->dbsize *= 2;						\
		p->count += p->dbsize;					\
	    }								\
	    p->count--;							\
	}								\
	if (p->hi > p->array->hi 					\
		|| (p->hi == p->array->hi && p->lo >= p->array->lo)) {	\
	    p->ptr = NULL;						\
	}								\
    }									\
    return p->ptr;							\
}

