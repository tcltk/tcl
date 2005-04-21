/* 
 * tclGet.c --
 *
 *	This file contains procedures to convert strings into
 *	other forms, like integers or floating-point numbers or
 *	booleans, doing syntax checking along the way.
 *
 * Copyright (c) 1990-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclGet.c,v 1.11 2005/04/21 14:23:48 dgp Exp $
 */

#include "tclInt.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetInt --
 *
 *	Given a string, produce the corresponding integer value.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *intPtr
 *	will be set to the integer value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetInt(interp, str, intPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    CONST char *str;		/* String containing a (possibly signed)
				 * integer in a form acceptable to strtoul. */
    int *intPtr;		/* Place to store converted result. */
{
    Tcl_Obj obj;
   
    obj.refCount = 1;
    obj.bytes = (char *) str;
    obj.length = strlen(str);
    obj.typePtr = NULL;

    return Tcl_GetIntFromObj(interp, &obj, intPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetLong --
 *
 *	Given a string, produce the corresponding long integer value.
 *	This routine is a version of Tcl_GetInt but returns a "long"
 *	instead of an "int".
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *longPtr
 *	will be set to the long integer value equivalent to string. If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result if interp
 *	is non-NULL. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetLong(interp, str, longPtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting
				 * if not NULL. */
    CONST char *str;		/* String containing a (possibly signed)
				 * long integer in a form acceptable to
				 * strtoul. */
    long *longPtr;		/* Place to store converted long result. */
{
    Tcl_Obj obj;

    obj.refCount = 1;
    obj.bytes = (char *) str;
    obj.length = strlen(str);
    obj.typePtr = NULL;

    return Tcl_GetLongFromObj(interp, &obj, longPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetDouble --
 *
 *	Given a string, produce the corresponding double-precision
 *	floating-point value.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *doublePtr
 *	will be set to the double-precision value equivalent to string.
 *	If string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetDouble(interp, str, doublePtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting. */
    CONST char *str;		/* String containing a floating-point number
				 * in a form acceptable to strtod. */
    double *doublePtr;		/* Place to store converted result. */
{
    Tcl_Obj obj;

    obj.refCount = 1;
    obj.bytes = (char *) str;
    obj.length = strlen(str);
    obj.typePtr = NULL;

    return Tcl_GetDoubleFromObj(interp, &obj, doublePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetBoolean --
 *
 *	Given a string, return a 0/1 boolean value corresponding
 *	to the string.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *boolPtr
 *	will be set to the 0/1 value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetBoolean(interp, str, boolPtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting. */
    CONST char *str;		/* String containing a boolean number
				 * specified either as 1/0 or true/false or
				 * yes/no. */
    int *boolPtr;		/* Place to store converted result, which
				 * will be 0 or 1. */
{
    /*
     * Can't use this (yet) due to Bug 1187123.
     *
    Tcl_Obj obj;

    obj.refCount = 1;
    obj.bytes = (char *) str;
    obj.length = strlen(str);
    obj.typePtr = NULL;

    return Tcl_GetBooleanFromObj(interp, &obj, boolPtr);
    */
    int i;
    char lowerCase[10], c;
    size_t length;

    /*
     * Convert the input string to all lower-case. 
     * INTL: This code will work on UTF strings.
     */

    for (i = 0; i < 9; i++) {
	c = str[i];
	if (c == 0) {
	    break;
	}
	if ((c >= 'A') && (c <= 'Z')) {
	    c += (char) ('a' - 'A');
	}
	lowerCase[i] = c;
    }
    lowerCase[i] = 0;

    length = strlen(lowerCase);
    c = lowerCase[0];
    if ((c == '0') && (lowerCase[1] == '\0')) {
	*boolPtr = 0;
    } else if ((c == '1') && (lowerCase[1] == '\0')) {
	*boolPtr = 1;
    } else if ((c == 'y') && (strncmp(lowerCase, "yes", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'n') && (strncmp(lowerCase, "no", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 't') && (strncmp(lowerCase, "true", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'f') && (strncmp(lowerCase, "false", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 'o') && (length >= 2)) {
	if (strncmp(lowerCase, "on", length) == 0) {
	    *boolPtr = 1;
	} else if (strncmp(lowerCase, "off", length) == 0) {
	    *boolPtr = 0;
	} else {
	    goto badBoolean;
	}
    } else {
	badBoolean:
        if (interp != (Tcl_Interp *) NULL) {
	    Tcl_Obj *msg =
		    Tcl_NewStringObj("expected boolean value but got \"", -1);
	    TclAppendLimitedToObj(msg, str, -1, 50, "");
	    Tcl_AppendToObj(msg, "\"", -1);
	    Tcl_SetObjResult(interp, msg);
        }
	return TCL_ERROR;
    }
    return TCL_OK;
}
