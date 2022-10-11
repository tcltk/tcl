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
typedef struct ArithSeries {
    Tcl_WideInt start;
    Tcl_WideInt end;
    Tcl_WideInt step;
    Tcl_WideInt len;
    Tcl_Obj **elements;
    int isDouble;
} ArithSeries;
typedef struct ArithSeriesDbl {
    double start;
    double end;
    double step;
    Tcl_WideInt len;
    Tcl_Obj **elements;
    int isDouble;
} ArithSeriesDbl;


MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjCopy(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE int	TclArithSeriesObjStep(Tcl_Obj *arithSeriesPtr,
			    Tcl_Obj **stepObj);
MODULE_SCOPE int	TclArithSeriesObjIndex(Tcl_Obj *arithSeriesPtr,
			    Tcl_WideInt index, Tcl_Obj **elementObj);
MODULE_SCOPE Tcl_WideInt TclArithSeriesObjLength(Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjRange(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr, int fromIdx, int toIdx);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjReverse(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE int	TclArithSeriesGetElements(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, size_t *objcPtr, Tcl_Obj ***objvPtr);
MODULE_SCOPE Tcl_Obj *	TclNewArithSeriesInt(Tcl_WideInt start,
			    Tcl_WideInt end, Tcl_WideInt step,
			    Tcl_WideInt len);
MODULE_SCOPE Tcl_Obj *	TclNewArithSeriesDbl(double start, double end,
			    double step, Tcl_WideInt len);
MODULE_SCOPE int 	TclNewArithSeriesObj(Tcl_Interp *interp,
			    Tcl_Obj **arithSeriesObj, int useDoubles,
			    Tcl_Obj *startObj, Tcl_Obj *endObj,
			    Tcl_Obj *stepObj, Tcl_Obj *lenObj);
