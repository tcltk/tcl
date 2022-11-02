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
    Tcl_Size len;
    Tcl_Obj **elements;
    int isDouble;
} ArithSeries;

typedef struct ArithSeriesDbl {
    double start;
    double end;
    double step;
    Tcl_Size len;
    Tcl_Obj **elements;
    int isDouble;
} ArithSeriesDbl;

MODULE_SCOPE int	TclNewArithSeriesObj(Tcl_Interp *interp, Tcl_Obj **arithSeriesPtr,
                            int useDoubles, Tcl_Obj *startObj, Tcl_Obj *endObj,
                            Tcl_Obj *stepObj, Tcl_Obj *lenObj);

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
