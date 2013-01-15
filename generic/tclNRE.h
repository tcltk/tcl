/* **********************************************
 * NRE internals   
 * **********************************************
 */

#ifdef TCL_NRE_DEBUG
#define NRE_STACK_DEBUG         1
#else
#define NRE_STACK_DEBUG         0
#endif
#define NRE_STACK_SIZE        100


/*
 * This is the main data struct for representing NR commands. It is designed
 * to fit in sizeof(Tcl_Obj) in order to exploit the fastest memory allocator
 * available.
 */

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

#if NRE_STACK_DEBUG

typedef struct NRE_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[4];
    struct NRE_callback *nextPtr;
} NRE_callback;

#define POP_CB(interp, cbPtr)                      \
    do {                                           \
        cbPtr = TOP_CB(interp);                    \
        TOP_CB(interp) = cbPtr->nextPtr;           \
    } while (0)

#define ALLOC_CB(interp, cbPtr)			\
    do {					\
	cbPtr = ckalloc(sizeof(NRE_callback));	\
	cbPtr->nextPtr = TOP_CB(interp);	\
	TOP_CB(interp) = cbPtr;			\
    } while (0)
    
#define FREE_CB(interp, ptr)			\
    ckfree((char *) (ptr))

#define NEXT_CB(ptr)  (ptr)->nextPtr

#else /* not debugging the NRE stack */

typedef struct NRE_callback {
    Tcl_NRPostProc *procPtr;
    ClientData data[4];
} NRE_callback;

typedef struct NRE_stack {
    struct NRE_callback items[NRE_STACK_SIZE];
    struct NRE_stack *next;
} NRE_stack;

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

#define FREE_CB(interp, cbPtr)

#define NEXT_CB(ptr) TclNextCallback(ptr)

MODULE_SCOPE NRE_callback *TclNewCallback(Tcl_Interp *interp);
MODULE_SCOPE NRE_callback *TclPopCallback(Tcl_Interp *interp);
MODULE_SCOPE NRE_callback *TclNextCallback(NRE_callback *ptr);

#endif
