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
    ClientData data[4];
} NRE_callback;

typedef struct NRE_stack {
    struct NRE_callback items[NRE_STACK_SIZE];
    struct NRE_stack *next;
} NRE_stack;

/*
 * Inline versions of Tcl_NRAddCallback and friends
 */

#define TOP_CB(iPtr) (((Interp *)(iPtr))->execEnvPtr->callbackPtr)

#define TclNRAddCallback(interp,postProcPtr,data0,data1,data2,data3)	\
    do {								\
	NRE_callback *cbPtr;						\
	ALLOC_CB(interp, cbPtr);					\
	INIT_CB(cbPtr, postProcPtr,data0,data1,data2,data3);		\
    } while (0)

#define INIT_CB(cbPtr, postProcPtr,data0,data1,data2,data3)		\
    do {								\
	cbPtr->procPtr = (postProcPtr);					\
	cbPtr->data[0] = (ClientData)(data0);				\
	cbPtr->data[1] = (ClientData)(data1);				\
	cbPtr->data[2] = (ClientData)(data2);				\
	cbPtr->data[3] = (ClientData)(data3);				\
    } while (0)

#define POP_CB(interp, cbPtr)			\
    (cbPtr) = TOP_CB(interp)--

#define ALLOC_CB(interp, cbPtr)					\
    do {							\
	ExecEnv *eePtr = ((Interp *) interp)->execEnvPtr;	\
	NRE_stack *this = eePtr->NRStack;			\
								\
	if (eePtr->callbackPtr &&					\
		(eePtr->callbackPtr < &this->items[NRE_STACK_SIZE-1])) { \
	    (cbPtr) = ++eePtr->callbackPtr;				\
	} else {							\
	    (cbPtr) = TclNewCallback(interp);				\
	}								\
    } while (0)

#define NEXT_CB(ptr) TclNextCallback(ptr)

#define NRE_TRAMPOLINE 0
#if NRE_TRAMPOLINE
#define NRE_JUMP(interp,postProcPtr,data0,data1,data2,data3)		\
    TclNRAddCallback((interp),(postProcPtr),(data0),(data1),(data2),(data3)); \
    NRE_NEXT(TCL_OK)
#define NRE_NEXT(result)			\
    return (result)
#else
/* no trampoline, optimized sibcalls */
#define NRE_JUMP(interp,postProcPtr,data0,data1,data2,data3)		\
    TclNRAddCallback((interp),(postProcPtr),(data0),(data1),(data2),(data3)); \
    return TCL_OK
#define NRE_NEXT(result)					\
    do { /* optimized indirect sibling calls?! */		\
	NRE_callback *cbPtr;					\
	POP_CB(interp, cbPtr);					\
	return (cbPtr->procPtr)(cbPtr->data, interp, result);	\
    } while (0)
#endif

MODULE_SCOPE TCL_NOINLINE NRE_callback *TclNewCallback(Tcl_Interp *interp);
MODULE_SCOPE TCL_NOINLINE NRE_callback *TclPopCallback(Tcl_Interp *interp);
MODULE_SCOPE TCL_NOINLINE NRE_callback *TclNextCallback(NRE_callback *ptr);
