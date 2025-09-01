#include <tcl.h>

extern DLLEXPORT Tcl_LibraryInitProc Tsdperf_Init;

static Tcl_ThreadDataKey key;

typedef struct {
    Tcl_WideInt value;
} TsdPerf;


static int
tsdPerfSetObjCmd(void *cdata, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));
    Tcl_WideInt i;

    if (2 != objc) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetWideIntFromObj(interp, objv[1], &i)) {
	return TCL_ERROR;
    }

    perf->value = i;

    return TCL_OK;
}

static int
tsdPerfGetObjCmd(void *cdata, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));


    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(perf->value));

    return TCL_OK;
}

int
Tsdperf_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.7-", 0) == NULL) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand2(interp, "tsdPerfSet", tsdPerfSetObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "tsdPerfGet", tsdPerfGetObjCmd, NULL, NULL);

    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
