/* **********************************************
 * NRE internals   
 * **********************************************
 */

#define NRE_STACK_SIZE        100

/*
 * This is the main data struct for representing NR commands. It was
 * originally designed to fit in sizeof(Tcl_Obj) in order to exploit the
 * fastest memory allocator available. The current version completely changed
 * the memory management approach (stack vs linked list), but the struct was
 * kept as it proved to be a "good" fit.
 */

typedef struct NRE_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[3];
} NRE_callback;

/*
 * - NOTES ON ALIGNMENT -
 *
 * . Cache lines are 64B long; callbacks are 32B on x86_64 and 16B on x32. In 
 *   order to insure that no callback ever strides cache line boundaries, we
 *   require the first callback to be aligned to 32b (on x32, 16B would be
 *   enough).
 * . we use a gcc'ism to insure alignment, portability not being the priority
 *   right now.
 */

typedef struct NRE_stack {
    struct NRE_callback items[NRE_STACK_SIZE];
    struct NRE_stack *next;
} NRE_stack  __attribute__ ((aligned (32)));

#define NRE_newExtra(ptr)			\
    TclSmallAlloc(5*sizeof(ClientData), ptr)
#define NRE_freeExtra(ptr)  TclSmallFree(ptr)

/*
 * Inline versions of Tcl_NRAddCallback and friends
 */

#define TOP_CB(iPtr) (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

#define TclNRAddCallback(interp,postProcPtr,data0,data1,data2)	\
    do {								\
	NRE_callback *cbPtr;						\
	ALLOC_CB(interp, cbPtr);					\
	INIT_CB(cbPtr, postProcPtr,data0,data1,data2);		\
    } while (0)

#define INIT_CB(cbPtr, postProcPtr,data0,data1,data2)		\
    do {								\
	cbPtr->procPtr = (postProcPtr);					\
	cbPtr->data[0] = (ClientData)(data0);				\
	cbPtr->data[1] = (ClientData)(data1);				\
	cbPtr->data[2] = (ClientData)(data2);				\
    } while (0)

#define POP_CB(interp, cbPtr)			\
    (cbPtr) = TOP_CB(interp)++

#define ALLOC_CB(interp, cbPtr)					\
    do {							\
	ExecEnv *eePtr = ((Interp *) interp)->execEnvPtr;	\
	NRE_stack *this = eePtr->NRStack;			\
								\
	if (eePtr->callbackPtr &&					\
		(eePtr->callbackPtr > &this->items[0])) { \
	    (cbPtr) = --eePtr->callbackPtr;				\
	} else {							\
	    (cbPtr) = TclNewCallback(interp);				\
	}								\
    } while (0)

#define NEXT_CB(ptr) TclNextCallback(ptr)

#define NRE_TRAMPOLINE 0
#if NRE_TRAMPOLINE
#define NRE_JUMP(interp,postProcPtr,data0,data1,data2)		\
    TclNRAddCallback((interp),(postProcPtr),(data0),(data1),(data2)); \
    NRE_NEXT(TCL_OK)
#define NRE_NEXT(result)			\
    return (result)
#else
/* no trampoline, optimized sibcalls */
#define NRE_JUMP(interp,postProcPtr,data0,data1,data2)		\
    TclNRAddCallback((interp),(postProcPtr),(data0),(data1),(data2)); \
    NRE_NEXT(TCL_OK)
#define NRE_NEXT(result)					\
    do { /* optimized indirect sibling calls?! */		\
	NRE_callback *cbPtr;					\
	POP_CB(interp, cbPtr);					\
	return (cbPtr->procPtr)(cbPtr->data, interp, (result));	\
    } while (0)
#endif

MODULE_SCOPE TCL_NOINLINE NRE_callback *TclNewCallback(Tcl_Interp *interp);
MODULE_SCOPE TCL_NOINLINE NRE_callback *TclPopCallback(Tcl_Interp *interp);
MODULE_SCOPE TCL_NOINLINE NRE_callback *TclNextCallback(NRE_callback *ptr);
