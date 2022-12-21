/*
 * tclArithSeries.h --
 *
 *     This file contains the ArithSeries concrete abstract list
 *     implementation. It implements the inner workings of the lseq command.
 *
 * Copyright © 2022 Brian S. Griffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * The structure used for the ArithSeries internal representation.
 * Note that the len can in theory be always computed by start,end,step
 * but it's faster to cache it inside the internal representation.
 */
typedef struct {
    Tcl_Size len;
    Tcl_Obj **elements;
    int isDouble;
    Tcl_WideInt start;
    Tcl_WideInt end;
    Tcl_WideInt step;
} ArithSeries;
typedef struct {
    Tcl_Size len;
    Tcl_Obj **elements;
    int isDouble;
    double start;
    double end;
    double step;
} ArithSeriesDbl;


MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjCopy(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE  Tcl_Obj *TclArithSeriesObjIndex(Tcl_Interp *, Tcl_Obj *,
			    Tcl_WideInt index);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjRange(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr, Tcl_Size fromIdx, Tcl_Size toIdx);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjReverse(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE int	TclArithSeriesGetElements(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_Size *objcPtr, Tcl_Obj ***objvPtr);
MODULE_SCOPE int 	TclNewArithSeriesObj(Tcl_Interp *interp,
			    Tcl_Obj **arithSeriesObj, int useDoubles,
			    Tcl_Obj *startObj, Tcl_Obj *endObj,
			    Tcl_Obj *stepObj, Tcl_Obj *lenObj);

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
