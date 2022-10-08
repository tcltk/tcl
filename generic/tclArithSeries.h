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
 * The structure used for the AirthSeries internal representation.
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

#define ArithSeriesIndexM(arithSeriesRepPtr, index) \
    ((arithSeriesRepPtr)->isDouble ?					\
     (((ArithSeriesDbl*)(arithSeriesRepPtr))->start+((index) * ((ArithSeriesDbl*)(arithSeriesRepPtr))->step)) \
     :									\
     ((arithSeriesRepPtr)->start+((index) * arithSeriesRepPtr->step)))

#define ArithSeriesStepM(arithSeriesRepPtr) \
    ((arithSeriesRepPtr)->isDouble ?					\
     ((ArithSeriesDbl*)(arithSeriesRepPtr))->step			\
     :									\
     (arithSeriesRepPtr)->step)


MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjCopy(Tcl_Interp *interp,
			    Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE int	TclArithSeriesObjStep(Tcl_Obj *arithSeriesPtr, Tcl_Obj **stepObj);
MODULE_SCOPE int	TclArithSeriesObjIndex(Tcl_Obj *arithSeriesPtr,
                            Tcl_WideInt index, Tcl_Obj **elemObj);
MODULE_SCOPE Tcl_WideInt TclArithSeriesObjLength(Tcl_Obj *arithSeriesObj);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjRange(Tcl_Obj *arithSeriesPtr,
			    Tcl_WideInt fromIdx, Tcl_WideInt toIdx);
MODULE_SCOPE Tcl_Obj *	TclArithSeriesObjReverse(Tcl_Obj *arithSeriesPtr);
MODULE_SCOPE int	TclArithSeriesGetElements(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, int *objcPtr, Tcl_Obj ***objvPtr);
MODULE_SCOPE Tcl_Obj *	TclNewArithSeriesInt(Tcl_WideInt start,
			    Tcl_WideInt end, Tcl_WideInt step,
			    Tcl_WideInt len);
MODULE_SCOPE Tcl_Obj *	TclNewArithSeriesDbl(double start, double end,
			    double step, Tcl_WideInt len);
MODULE_SCOPE int	TclNewArithSeriesObj(Tcl_Interp *interp, Tcl_Obj **arithSeriesPtr,
                            int useDoubles, Tcl_Obj *startObj, Tcl_Obj *endObj,
                            Tcl_Obj *stepObj, Tcl_Obj *lenObj);

MODULE_SCOPE Tcl_Obj *  Tcl_NewArithSeriesObj(int objc, Tcl_Obj *objv[]);


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
