/* **********************************************
 * NRE internals   
 * **********************************************
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

