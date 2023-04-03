/*
 * tclTestObj.c --
 *
 *	This file contains C command functions for the additional Tcl commands
 *	that are used for testing implementations of the Tcl object types.
 *	These commands are not normally included in Tcl applications; they're
 *	only used for testing.
 *
 * Copyright © 1995-1998 Sun Microsystems, Inc.
 * Copyright © 1999 Scriptics Corporation.
 * Copyright © 2005 Kevin B. Kenny.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#include "tclInt.h"
#ifdef TCL_WITH_EXTERNAL_TOMMATH
#   include "tommath.h"
#else
#   include "tclTomMath.h"
#endif
#include "tclStringRep.h"


/*
 * Forward declarations for functions defined later in this file:
 */

static int		CheckIfVarUnset(Tcl_Interp *interp, Tcl_Obj **varPtr, size_t varIndex);
static int		GetVariableIndex(Tcl_Interp *interp,
			    Tcl_Obj *obj, size_t *indexPtr);
static void		SetVarToObj(Tcl_Obj **varPtr, size_t varIndex, Tcl_Obj *objPtr);
static Tcl_ObjCmdProc	TestbignumobjCmd;
static Tcl_ObjCmdProc	TestbooleanobjCmd;
static Tcl_ObjCmdProc	TestdoubleobjCmd;
static Tcl_ObjCmdProc	TestindexobjCmd;
static Tcl_ObjCmdProc	TestintobjCmd;
static Tcl_ObjCmdProc	TestlistobjCmd;
static Tcl_ObjCmdProc	TestobjCmd;
static Tcl_ObjCmdProc	TeststringobjCmd;

#define VARPTR_KEY "TCLOBJTEST_VARPTR"
#define NUMBER_OF_OBJECT_VARS 20

static void VarPtrDeleteProc(void *clientData, TCL_UNUSED(Tcl_Interp *))
{
    int i;
    Tcl_Obj **varPtr = (Tcl_Obj **) clientData;
    for (i = 0;  i < NUMBER_OF_OBJECT_VARS;  i++) {
	if (varPtr[i]) Tcl_DecrRefCount(varPtr[i]);
    }
    Tcl_Free(varPtr);
}

static Tcl_Obj **GetVarPtr(Tcl_Interp *interp)
{
    Tcl_InterpDeleteProc *proc;

    return (Tcl_Obj **) Tcl_GetAssocData(interp, VARPTR_KEY, &proc);
}

/*
 *----------------------------------------------------------------------
 *
 * TclObjTest_Init --
 *
 *	This function creates additional commands that are used to test the
 *	Tcl object support.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in the interp's result if an error occurs.
 *
 * Side effects:
 *	Creates and registers several new testing commands.
 *
 *----------------------------------------------------------------------
 */

int
TclObjTest_Init(
    Tcl_Interp *interp)
{
    int i;
    /*
     * An array of Tcl_Obj pointers used in the commands that operate on or get
     * the values of Tcl object-valued variables. varPtr[i] is the i-th variable's
     * Tcl_Obj *.
     */
    Tcl_Obj **varPtr;

    varPtr = (Tcl_Obj **) Tcl_Alloc(NUMBER_OF_OBJECT_VARS *sizeof(varPtr[0]));
    if (!varPtr) {
	return TCL_ERROR;
    }
    Tcl_SetAssocData(interp, VARPTR_KEY, VarPtrDeleteProc, varPtr);
    for (i = 0;  i < NUMBER_OF_OBJECT_VARS;  i++) {
	varPtr[i] = NULL;
    }

    Tcl_CreateObjCommand(interp, "testbignumobj", TestbignumobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testbooleanobj", TestbooleanobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testdoubleobj", TestdoubleobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testintobj", TestintobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testindexobj", TestindexobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testlistobj", TestlistobjCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testobj", TestobjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "teststringobj", TeststringobjCmd,
	    NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestbignumobjCmd --
 *
 *	This function implements the "testbignumobj" command.  It is used
 *	to exercise the bignum Tcl object type implementation.
 *
 * Results:
 *	Returns a standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees bignum objects; converts objects to have bignum
 *	type.
 *
 *----------------------------------------------------------------------
 */

static int
TestbignumobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Argument count */
    Tcl_Obj *const objv[])	/* Argument vector */
{
    const char *const subcmds[] = {
	"set", "get", "mult10", "div10", "iseven", "radixsize", NULL
    };
    enum options {
	BIGNUM_SET, BIGNUM_GET, BIGNUM_MULT10, BIGNUM_DIV10, BIGNUM_ISEVEN,
	BIGNUM_RADIXSIZE
    } idx;
    int index;
    size_t varIndex;
    const char *string;
    mp_int bignumValue;
    Tcl_Obj **varPtr;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subcmds, "option", 0,
	    &idx) != TCL_OK) {
	return TCL_ERROR;
    }
    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    varPtr = GetVarPtr(interp);

    switch (idx) {
    case BIGNUM_SET:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "var value");
	    return TCL_ERROR;
	}
	string = Tcl_GetString(objv[3]);
	if (mp_init(&bignumValue) != MP_OKAY) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("error in mp_init", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	if (mp_read_radix(&bignumValue, string, 10) != MP_OKAY) {
	    mp_clear(&bignumValue);
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("error in mp_read_radix", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly.  Otherwise, if RC>1 (i.e. the object is shared),
	 * we must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBignumObj(varPtr[varIndex], &bignumValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBignumObj(&bignumValue));
	}
	break;

    case BIGNUM_GET:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	break;

    case BIGNUM_MULT10:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBignumFromObj(interp, varPtr[varIndex],
		&bignumValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (mp_mul_d(&bignumValue, 10, &bignumValue) != MP_OKAY) {
	    mp_clear(&bignumValue);
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("error in mp_mul_d", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBignumObj(varPtr[varIndex], &bignumValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBignumObj(&bignumValue));
	}
	break;

    case BIGNUM_DIV10:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBignumFromObj(interp, varPtr[varIndex],
		&bignumValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (mp_div_d(&bignumValue, 10, &bignumValue, NULL) != MP_OKAY) {
	    mp_clear(&bignumValue);
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("error in mp_div_d", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBignumObj(varPtr[varIndex], &bignumValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBignumObj(&bignumValue));
	}
	break;

    case BIGNUM_ISEVEN:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBignumFromObj(interp, varPtr[varIndex],
		&bignumValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (mp_mod_2d(&bignumValue, 1, &bignumValue) != MP_OKAY) {
	    mp_clear(&bignumValue);
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("error in mp_mod_2d", TCL_INDEX_NONE));
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBooleanObj(varPtr[varIndex], mp_iszero(&bignumValue));
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBooleanObj(mp_iszero(&bignumValue)));
	}
	mp_clear(&bignumValue);
	break;

    case BIGNUM_RADIXSIZE:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBignumFromObj(interp, varPtr[varIndex],
		&bignumValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (mp_radix_size(&bignumValue, 10, &index) != MP_OKAY) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], index);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(index));
	}
	mp_clear(&bignumValue);
	break;
    }

    Tcl_SetObjResult(interp, varPtr[varIndex]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestbooleanobjCmd --
 *
 *	This function implements the "testbooleanobj" command.  It is used to
 *	test the boolean Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees boolean objects, and also converts objects to
 *	have boolean type.
 *
 *----------------------------------------------------------------------
 */

static int
TestbooleanobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    size_t varIndex;
    int boolValue;
    const char *subCmd;
    Tcl_Obj **varPtr;

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    varPtr = GetVarPtr(interp);

    subCmd = Tcl_GetString(objv[1]);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetBooleanFromObj(interp, objv[3], &boolValue) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared),
	 * we must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBooleanObj(varPtr[varIndex], boolValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBooleanObj(boolValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "not") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetBooleanFromObj(interp, varPtr[varIndex],
				  &boolValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetBooleanObj(varPtr[varIndex], !boolValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewBooleanObj(!boolValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetString(objv[1]),
		"\": must be set, get, or not", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdoubleobjCmd --
 *
 *	This function implements the "testdoubleobj" command.  It is used to
 *	test the double-precision floating point Tcl object type
 *	implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees double objects, and also converts objects to
 *	have double type.
 *
 *----------------------------------------------------------------------
 */

static int
TestdoubleobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    size_t varIndex;
    double doubleValue;
    const char *subCmd;
    Tcl_Obj **varPtr;

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    varPtr = GetVarPtr(interp);

    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    subCmd = Tcl_GetString(objv[1]);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetDouble(interp, Tcl_GetString(objv[3]), &doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared), we
	 * must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], doubleValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewDoubleObj(doubleValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "mult10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetDoubleFromObj(interp, varPtr[varIndex],
				 &doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], doubleValue * 10.0);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewDoubleObj(doubleValue * 10.0));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "div10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetDoubleFromObj(interp, varPtr[varIndex],
		&doubleValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetDoubleObj(varPtr[varIndex], doubleValue / 10.0);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewDoubleObj(doubleValue / 10.0));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetString(objv[1]),
		"\": must be set, get, mult10, or div10", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestindexobjCmd --
 *
 *	This function implements the "testindexobj" command. It is used to
 *	test the index Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees int objects, and also converts objects to
 *	have int type.
 *
 *----------------------------------------------------------------------
 */

static int
TestindexobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int allowAbbrev, index, setError, i, result;
    Tcl_WideInt index2;
    const char **argv;
    static const char *const tablePtr[] = {"a", "b", "check", NULL};

    /*
     * Keep this structure declaration in sync with tclIndexObj.c
     */
    struct IndexRep {
	void *tablePtr;		/* Pointer to the table of strings. */
	TCL_HASH_TYPE offset;		/* Offset between table entries. */
	TCL_HASH_TYPE index;		/* Selected index into table. */
    } *indexRep;

    if ((objc == 3) && (strcmp(Tcl_GetString(objv[1]),
	    "check") == 0)) {
	/*
	 * This code checks to be sure that the results of Tcl_GetIndexFromObj
	 * are properly cached in the object and returned on subsequent
	 * lookups.
	 */

	if (Tcl_GetWideIntFromObj(interp, objv[2], &index2) != TCL_OK) {
	    return TCL_ERROR;
	}

	Tcl_GetIndexFromObj(NULL, objv[1], tablePtr, "token", 0, &index);
	indexRep = (struct IndexRep *)objv[1]->internalRep.twoPtrValue.ptr1;
	indexRep->index = index2;
	result = Tcl_GetIndexFromObj(NULL, objv[1],
		tablePtr, "token", 0, &index);
	if (result == TCL_OK) {
	    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), index);
	}
	return result;
    }

    if (objc < 5) {
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "wrong # args", TCL_INDEX_NONE);
	return TCL_ERROR;
    }

    if (Tcl_GetBooleanFromObj(interp, objv[1], &setError) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetBooleanFromObj(interp, objv[2], &allowAbbrev) != TCL_OK) {
	return TCL_ERROR;
    }

    argv = (const char **)Tcl_Alloc((objc-3) * sizeof(char *));
    for (i = 4; i < objc; i++) {
	argv[i-4] = Tcl_GetString(objv[i]);
    }
    argv[objc-4] = NULL;

    result = Tcl_GetIndexFromObj((setError? interp : NULL), objv[3],
	    argv, "token", TCL_INDEX_TEMP_TABLE|(allowAbbrev? 0 : TCL_EXACT),
	    &index);
    Tcl_Free((void *)argv);
    if (result == TCL_OK) {
	Tcl_SetWideIntObj(Tcl_GetObjResult(interp), index);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TestintobjCmd --
 *
 *	This function implements the "testintobj" command. It is used to
 *	test the int Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees int objects, and also converts objects to
 *	have int type.
 *
 *----------------------------------------------------------------------
 */

static int
TestintobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    size_t varIndex;
#if (INT_MAX != LONG_MAX)   /* int is not the same size as long */
    int i;
#endif
    Tcl_WideInt wideValue;
    const char *subCmd;
    Tcl_Obj **varPtr;

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    varPtr = GetVarPtr(interp);
    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    subCmd = Tcl_GetString(objv[1]);
    if (strcmp(subCmd, "set") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetWideIntFromObj(interp, objv[3], &wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * If the object currently bound to the variable with index varIndex
	 * has ref count 1 (i.e. the object is unshared) we can modify that
	 * object directly. Otherwise, if RC>1 (i.e. the object is shared), we
	 * must create a new object to modify/set and decrement the old
	 * formerly-shared object's ref count. This is "copy on write".
	 */

	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], wideValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(wideValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "set2") == 0) { /* doesn't set result */
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetWideIntFromObj(interp, objv[3], &wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], wideValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(wideValue));
	}
    } else if (strcmp(subCmd, "setint") == 0) {
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (Tcl_GetWideIntFromObj(interp, objv[3], &wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], wideValue);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(wideValue));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "setmax") == 0) {
	Tcl_WideInt maxWide = WIDE_MAX;
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], maxWide);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(maxWide));
	}
    } else if (strcmp(subCmd, "ismax") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetWideIntFromObj(interp, varPtr[varIndex], &wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_AppendToObj(Tcl_GetObjResult(interp),
		((wideValue == WIDE_MAX)? "1" : "0"), TCL_INDEX_NONE);
    } else if (strcmp(subCmd, "get") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "get2") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_AppendToObj(Tcl_GetObjResult(interp), Tcl_GetString(varPtr[varIndex]), TCL_INDEX_NONE);
    } else if (strcmp(subCmd, "inttoobigtest") == 0) {
	/*
	 * If long ints have more bits than ints on this platform, verify that
	 * Tcl_GetIntFromObj returns an error if the long int held in an
	 * integer object's internal representation is too large to fit in an
	 * int.
	 */

	if (objc != 3) {
	    goto wrongNumArgs;
	}
#if (INT_MAX == LONG_MAX)   /* int is same size as long int */
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "1", TCL_INDEX_NONE);
#else
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], LONG_MAX);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(LONG_MAX));
	}
	if (Tcl_GetIntFromObj(interp, varPtr[varIndex], &i) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), "1", TCL_INDEX_NONE);
	    return TCL_OK;
	}
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "0", TCL_INDEX_NONE);
#endif
    } else if (strcmp(subCmd, "mult10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetWideIntFromObj(interp, varPtr[varIndex],
		&wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], wideValue * 10);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(wideValue * 10));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else if (strcmp(subCmd, "div10") == 0) {
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_GetWideIntFromObj(interp, varPtr[varIndex],
		&wideValue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (!Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetWideIntObj(varPtr[varIndex], wideValue / 10);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewWideIntObj(wideValue / 10));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
    } else {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		"bad option \"", Tcl_GetString(objv[1]),
		"\": must be set, get, get2, mult10, or div10", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *-----------------------------------------------------------------------------
 *
 * TestlistobjCmd --
 *
 *	This function implements the 'testlistobj' command. It is used to
 *	test a few possible corner cases in list object manipulation from
 *	C code that cannot occur at the Tcl level.
 *
 *      Following new commands are added for 8.7 as regression tests for
 *      memory leaks and use-after-free. Unlike 8.6, 8.7 has multiple internal
 *      representations for lists.  It has to be ensured that corresponding
 *      implementations obey the invariants of the C list API. The script
 *      level tests do not suffice as Tcl list commands do not execute
 *      the same exact code path as the exported C API.
 *
 *      Note these new commands are only useful when Tcl is compiled with
 *      TCL_MEM_DEBUG defined.
 *
 *	indexmemcheck - loops calling Tcl_ListObjIndex on each element. This
 *	is to test that abstract lists returning elements do not depend
 *	on caller to free them. The test case should check allocated counts
 *      with the following sequence:
 *            set before <get memory counts>
 *            testobj set VARINDEX [list a b c] (or lseq etc.)
 *            testlistobj indexnoop VARINDEX
 *            testobj unset VARINDEX
 *            set after <get memory counts>
 *      after calling this command AND freeing the passed list. The targeted
 *      bug is if Tcl_LOI returns a ephemeral Tcl_Obj with no other reference
 *      resulting in a memory leak. Conversely, the command also checks
 *      that the Tcl_Obj returned by Tcl_LOI does not have a zero reference
 *      count since it is supposed to have at least one reference held
 *      by the list implementation. Returns a message in interp otherwise.
 *
 *      getelementsmemcheck - as above but for Tcl_ListObjGetElements

 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates, manipulates and frees list objects.
 *
 *-----------------------------------------------------------------------------
 */

static int
TestlistobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Tcl interpreter */
    int objc,			/* Number of arguments */
    Tcl_Obj *const objv[])	/* Argument objects */
{
    /* Subcommands supported by this command */
    const char* const subcommands[] = {
	"set",
	"get",
	"replace",
	"indexmemcheck",
	"getelementsmemcheck",
	NULL
    };
    enum listobjCmdIndex {
	LISTOBJ_SET,
	LISTOBJ_GET,
	LISTOBJ_REPLACE,
	LISTOBJ_INDEXMEMCHECK,
	LISTOBJ_GETELEMENTSMEMCHECK,
    } cmdIndex;

    size_t varIndex;		/* Variable number converted to binary */
    Tcl_WideInt first;			/* First index in the list */
    Tcl_WideInt count;			/* Count of elements in a list */
    Tcl_Obj **varPtr;
    int i, len;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg...?");
	return TCL_ERROR;
    }
    varPtr = GetVarPtr(interp);
    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subcommands, "command",
			    0, &cmdIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (cmdIndex) {
    case LISTOBJ_SET:
	if ((varPtr[varIndex] != NULL) && !Tcl_IsShared(varPtr[varIndex])) {
	    Tcl_SetListObj(varPtr[varIndex], objc-3, objv+3);
	} else {
	    SetVarToObj(varPtr, varIndex, Tcl_NewListObj(objc-3, objv+3));
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
	break;

    case LISTOBJ_GET:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr,varIndex)) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
	break;

    case LISTOBJ_REPLACE:
	if (objc < 5) {
	    Tcl_WrongNumArgs(interp, 2, objv,
			     "varIndex start count ?element...?");
	    return TCL_ERROR;
	}
	if (Tcl_GetWideIntFromObj(interp, objv[3], &first) != TCL_OK
	    || Tcl_GetWideIntFromObj(interp, objv[4], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_IsShared(varPtr[varIndex])) {
	    SetVarToObj(varPtr, varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	}
	Tcl_ResetResult(interp);
	return Tcl_ListObjReplace(interp, varPtr[varIndex], first, count,
				  objc-5, objv+5);

    case LISTOBJ_INDEXMEMCHECK:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr, varIndex)) {
	    return TCL_ERROR;
	}
	if (Tcl_ListObjLength(interp, varPtr[varIndex], &len) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (i = 0; i < len; ++i) {
	    Tcl_Obj *objP;
	    if (Tcl_ListObjIndex(interp, varPtr[varIndex], i, &objP)
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    if (objP->refCount <= 0) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"Tcl_ListObjIndex returned object with ref count <= 0",
			TCL_INDEX_NONE));
		/* Keep looping since we are also looping for leaks */
	    }
	}
	break;

    case LISTOBJ_GETELEMENTSMEMCHECK:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varIndex");
	    return TCL_ERROR;
	}
	if (CheckIfVarUnset(interp, varPtr, varIndex)) {
	    return TCL_ERROR;
	} else {
	    Tcl_Obj **elems;
	    if (Tcl_ListObjGetElements(interp, varPtr[varIndex], &len, &elems)
		!= TCL_OK) {
		return TCL_ERROR;
	    }
	    for (i = 0; i < len; ++i) {
		if (elems[i]->refCount <= 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "Tcl_ListObjGetElements element has ref count <= 0",
			    TCL_INDEX_NONE));
		    break;
		}
	    }
	}
	break;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestobjCmd --
 *
 *	This function implements the "testobj" command. It is used to test
 *	the type-independent portions of the Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees objects.
 *
 *----------------------------------------------------------------------
 */

static int
TestobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    size_t varIndex, destIndex;
    int i;
    const Tcl_ObjType *targetType;
    Tcl_Obj **varPtr;
    const char *subcommands[] = {
	"freeallvars", "bug3598580", "types",
	"objtype", "newobj", "set",
	"assign", "convert", "duplicate",
	"invalidateStringRep", "refcount", "type",
	NULL
    };
    enum testobjCmdIndex {
	TESTOBJ_FREEALLVARS, TESTOBJ_BUG3598580, TESTOBJ_TYPES,
	TESTOBJ_OBJTYPE, TESTOBJ_NEWOBJ, TESTOBJ_SET,
	TESTOBJ_ASSIGN, TESTOBJ_CONVERT, TESTOBJ_DUPLICATE,
	TESTOBJ_INVALIDATESTRINGREP, TESTOBJ_REFCOUNT, TESTOBJ_TYPE,
    } cmdIndex;

    if (objc < 2) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    varPtr = GetVarPtr(interp);
    if (Tcl_GetIndexFromObj(
	    interp, objv[1], subcommands, "command", 0, &cmdIndex)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    switch (cmdIndex) {
    case TESTOBJ_FREEALLVARS:
	if (objc != 2) {
	    goto wrongNumArgs;
	}
	for (i = 0;  i < NUMBER_OF_OBJECT_VARS;  i++) {
	    if (varPtr[i] != NULL) {
		Tcl_DecrRefCount(varPtr[i]);
		varPtr[i] = NULL;
	    }
	}
	return TCL_OK;
    case TESTOBJ_BUG3598580:
	if (objc != 2) {
	    goto wrongNumArgs;
	} else {
	    Tcl_Obj *listObjPtr, *elemObjPtr;
	    elemObjPtr = Tcl_NewWideIntObj(123);
	    listObjPtr = Tcl_NewListObj(1, &elemObjPtr);
	    /* Replace the single list element through itself, nonsense but
	     * legal. */
	    Tcl_ListObjReplace(interp, listObjPtr, 0, 1, 1, &elemObjPtr);
	    Tcl_SetObjResult(interp, listObjPtr);
	}
	return TCL_OK;
    case TESTOBJ_TYPES:
	if (objc != 2) {
	    goto wrongNumArgs;
	} else {
	    Tcl_Obj *typesObj = Tcl_NewListObj(0, NULL);
	    Tcl_AppendAllObjTypes(interp, typesObj);
	    Tcl_SetObjResult(interp, typesObj);
	}
	return TCL_OK;
    case TESTOBJ_OBJTYPE:
	/*
	 * Return an object containing the name of the argument's type of
	 * internal rep. If none exists, return "none".
	 */

	if (objc != 3) {
	    goto wrongNumArgs;
	} else {
	    const char *typeName;

	    if (objv[2]->typePtr == NULL) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj("none", TCL_INDEX_NONE));
	    } else {
		typeName = objv[2]->typePtr->name;
		if (!strcmp(typeName, "utf32string"))
		    typeName = "string";
#ifndef TCL_WIDE_INT_IS_LONG
	    else if (!strcmp(typeName, "wideInt")) typeName = "int";
#endif
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(typeName, TCL_INDEX_NONE));
	    }
	}
	return TCL_OK;
    case TESTOBJ_NEWOBJ:
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetVarToObj(varPtr, varIndex, Tcl_NewObj());
	Tcl_SetObjResult(interp, varPtr[varIndex]);
	return TCL_OK;
    case TESTOBJ_SET:
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetVarToObj(varPtr, varIndex, objv[3]);
	return TCL_OK;

    default:
	break;
    }

    /* All further commands expect an occupied varindex argument */
    if (objc < 3) {
	goto wrongNumArgs;
    }

    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    if (CheckIfVarUnset(interp, varPtr, varIndex)) {
	return TCL_ERROR;
    }

    switch (cmdIndex) {
    case TESTOBJ_ASSIGN:
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (GetVariableIndex(interp, objv[3], &destIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetVarToObj(varPtr, destIndex, varPtr[varIndex]);
	Tcl_SetObjResult(interp, varPtr[destIndex]);
	break;
    case TESTOBJ_CONVERT:
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if ((targetType = Tcl_GetObjType(Tcl_GetString(objv[3]))) == NULL) {
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
		    "no type ", Tcl_GetString(objv[3]), " found", NULL);
	    return TCL_ERROR;
	}
	if (Tcl_ConvertToType(interp, varPtr[varIndex], targetType)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, varPtr[varIndex]);
	break;
    case TESTOBJ_DUPLICATE:
	if (objc != 4) {
	    goto wrongNumArgs;
	}
	if (GetVariableIndex(interp, objv[3], &destIndex) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetVarToObj(varPtr, destIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	Tcl_SetObjResult(interp, varPtr[destIndex]);
	break;
    case TESTOBJ_INVALIDATESTRINGREP:
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	Tcl_InvalidateStringRep(varPtr[varIndex]);
	Tcl_SetObjResult(interp, varPtr[varIndex]);
	break;
    case TESTOBJ_REFCOUNT:
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(varPtr[varIndex]->refCount));
	break;
    case TESTOBJ_TYPE:
	if (objc != 3) {
	    goto wrongNumArgs;
	}
	if (varPtr[varIndex]->typePtr == NULL) { /* a string! */
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), "string", TCL_INDEX_NONE);
#ifndef TCL_WIDE_INT_IS_LONG
	} else if (!strcmp(varPtr[varIndex]->typePtr->name, "wideInt")) {
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    "int", TCL_INDEX_NONE);
#endif
	} else {
	    Tcl_AppendToObj(Tcl_GetObjResult(interp),
		    varPtr[varIndex]->typePtr->name, TCL_INDEX_NONE);
	}
	break;
    default:
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TeststringobjCmd --
 *
 *	This function implements the "teststringobj" command. It is used to
 *	test the string Tcl object type implementation.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	Creates and frees string objects, and also converts objects to
 *	have string type.
 *
 *----------------------------------------------------------------------
 */

static int
TeststringobjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_UniChar *unicode;
    size_t size, varIndex;
    int option, i;
    Tcl_WideInt length;
#define MAX_STRINGS 11
    const char *string, *strings[MAX_STRINGS+1];
    String *strPtr;
    Tcl_Obj **varPtr;
    static const char *const options[] = {
	"append", "appendstrings", "get", "get2", "length", "length2",
	"set", "set2", "setlength", "maxchars", "range", "appendself",
	"appendself2", "newunicode", NULL
    };

    if (objc < 3) {
	wrongNumArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "option arg ?arg ...?");
	return TCL_ERROR;
    }

    varPtr = GetVarPtr(interp);
    if (GetVariableIndex(interp, objv[2], &varIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0, &option)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    switch (option) {
	case 0:				/* append */
	    if (objc != 5) {
		goto wrongNumArgs;
	    }
	    if (Tcl_GetWideIntFromObj(interp, objv[4], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varPtr, varIndex, Tcl_NewObj());
	    }

	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object.
	     */

	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varPtr, varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }
	    Tcl_AppendToObj(varPtr[varIndex], Tcl_GetString(objv[3]), length);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 1:				/* appendstrings */
	    if (objc > (MAX_STRINGS+3)) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varPtr, varIndex, Tcl_NewObj());
	    }

	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object.
	     */

	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varPtr, varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }
	    for (i = 3;  i < objc;  i++) {
		strings[i-3] = Tcl_GetString(objv[i]);
	    }
	    for ( ; i < 12 + 3; i++) {
		strings[i - 3] = NULL;
	    }
	    Tcl_AppendStringsToObj(varPtr[varIndex], strings[0], strings[1],
		    strings[2], strings[3], strings[4], strings[5],
		    strings[6], strings[7], strings[8], strings[9],
		    strings[10], strings[11]);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 2:				/* get */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    if (CheckIfVarUnset(interp, varPtr,varIndex)) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 3:				/* get2 */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    if (CheckIfVarUnset(interp, varPtr, varIndex)) {
		return TCL_ERROR;
	    }
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), Tcl_GetString(varPtr[varIndex]), TCL_INDEX_NONE);
	    break;
	case 4:				/* length */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), (varPtr[varIndex] != NULL)
		    ? (Tcl_WideInt)varPtr[varIndex]->length : (Tcl_WideInt)-1);
	    break;
	case 5:				/* length2 */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] != NULL) {
		Tcl_ConvertToType(NULL, varPtr[varIndex],
			Tcl_GetObjType("string"));
		strPtr = (String *)varPtr[varIndex]->internalRep.twoPtrValue.ptr1;
		length = strPtr->allocated;
	    } else {
		length = -1;
	    }
	    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), length);
	    break;
	case 6:				/* set */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }

	    /*
	     * If the object currently bound to the variable with index
	     * varIndex has ref count 1 (i.e. the object is unshared) we can
	     * modify that object directly. Otherwise, if RC>1 (i.e. the
	     * object is shared), we must create a new object to modify/set
	     * and decrement the old formerly-shared object's ref count. This
	     * is "copy on write".
	     */

	    string = Tcl_GetStringFromObj(objv[3], &size);
	    if ((varPtr[varIndex] != NULL)
		    && !Tcl_IsShared(varPtr[varIndex])) {
		Tcl_SetStringObj(varPtr[varIndex], string, size);
	    } else {
		SetVarToObj(varPtr, varIndex, Tcl_NewStringObj(string, size));
	    }
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 7:				/* set2 */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    SetVarToObj(varPtr, varIndex, objv[3]);
	    break;
	case 8:				/* setlength */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    if (Tcl_GetWideIntFromObj(interp, objv[3], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (varPtr[varIndex] != NULL) {
		Tcl_SetObjLength(varPtr[varIndex], length);
	    }
	    break;
	case 9:				/* maxchars */
	    if (objc != 3) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] != NULL) {
		Tcl_ConvertToType(NULL, varPtr[varIndex],
			Tcl_GetObjType("string"));
		strPtr = (String *)varPtr[varIndex]->internalRep.twoPtrValue.ptr1;
		length = strPtr->maxChars;
	    } else {
		length = -1;
	    }
	    Tcl_SetWideIntObj(Tcl_GetObjResult(interp), length);
	    break;
	case 10: {				/* range */
	    Tcl_WideInt first, last;
	    if (objc != 5) {
		goto wrongNumArgs;
	    }
	    if ((Tcl_GetWideIntFromObj(interp, objv[3], &first) != TCL_OK)
		    || (Tcl_GetWideIntFromObj(interp, objv[4], &last) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, Tcl_GetRange(varPtr[varIndex], first, last));
	    break;
	}
	case 11:			/* appendself */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varPtr, varIndex, Tcl_NewObj());
	    }

	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object.
	     */

	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varPtr, varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }

	    string = Tcl_GetStringFromObj(varPtr[varIndex], &size);

	    if (Tcl_GetWideIntFromObj(interp, objv[3], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((length < 0) || ((Tcl_WideUInt)length > (Tcl_WideUInt)size)) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"index value out of range", TCL_INDEX_NONE));
		return TCL_ERROR;
	    }

	    Tcl_AppendToObj(varPtr[varIndex], string + length, size - length);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 12:			/* appendself2 */
	    if (objc != 4) {
		goto wrongNumArgs;
	    }
	    if (varPtr[varIndex] == NULL) {
		SetVarToObj(varPtr, varIndex, Tcl_NewObj());
	    }

	    /*
	     * If the object bound to variable "varIndex" is shared, we must
	     * "copy on write" and append to a copy of the object.
	     */

	    if (Tcl_IsShared(varPtr[varIndex])) {
		SetVarToObj(varPtr, varIndex, Tcl_DuplicateObj(varPtr[varIndex]));
	    }

	    unicode = Tcl_GetUnicodeFromObj(varPtr[varIndex], &size);

	    if (Tcl_GetWideIntFromObj(interp, objv[3], &length) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((length < 0) || ((Tcl_WideUInt)length > (Tcl_WideUInt)size)) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"index value out of range", TCL_INDEX_NONE));
		return TCL_ERROR;
	    }

	    Tcl_AppendUnicodeToObj(varPtr[varIndex], unicode + length, size - length);
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    break;
	case 13: /* newunicode*/
	    unicode = (Tcl_UniChar *)Tcl_Alloc((objc - 3) * sizeof(Tcl_UniChar));
	    for (i = 0; i < (objc - 3); ++i) {
		int val;
		if (Tcl_GetIntFromObj(interp, objv[i + 3], &val) != TCL_OK) {
		    break;
		}
		unicode[i] = (Tcl_UniChar)val;
	    }
	    if (i < (objc-3)) {
		Tcl_Free(unicode);
		return TCL_ERROR;
	    }
	    SetVarToObj(varPtr, varIndex, Tcl_NewUnicodeObj(unicode, objc - 3));
	    Tcl_SetObjResult(interp, varPtr[varIndex]);
	    Tcl_Free(unicode);
	    break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SetVarToObj --
 *
 *	Utility routine to assign a Tcl_Obj* to a test variable. The
 *	Tcl_Obj* can be NULL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This routine handles ref counting details for assignment: i.e. the old
 *	value's ref count must be decremented (if not NULL) and the new one
 *	incremented (also if not NULL).
 *
 *----------------------------------------------------------------------
 */

static void
SetVarToObj(
    Tcl_Obj **varPtr,
    size_t varIndex,		/* Designates the assignment variable. */
    Tcl_Obj *objPtr)		/* Points to object to assign to var. */
{
    if (varPtr[varIndex] != NULL) {
	Tcl_DecrRefCount(varPtr[varIndex]);
    }
    varPtr[varIndex] = objPtr;
    if (objPtr != NULL) {
	Tcl_IncrRefCount(objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetVariableIndex --
 *
 *	Utility routine to get a test variable index from the command line.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetVariableIndex(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    Tcl_Obj *obj,		/* The variable index
				 * specified as a nonnegative number less than
				 * NUMBER_OF_OBJECT_VARS. */
    size_t *indexPtr)		/* Place to store converted result. */
{
    Tcl_WideInt index;

    if (Tcl_GetWideIntFromObj(interp, obj, &index) != TCL_OK) {
	return TCL_ERROR;
    }
    if (index < 0 || index >= NUMBER_OF_OBJECT_VARS) {
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), "bad variable index", TCL_INDEX_NONE);
	return TCL_ERROR;
    }

    *indexPtr = index;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckIfVarUnset --
 *
 *	Utility function that checks whether a test variable is readable:
 *	i.e., that varPtr[varIndex] is non-NULL.
 *
 * Results:
 *	1 if the test variable is unset (NULL); 0 otherwise.
 *
 * Side effects:
 *	Sets the interpreter result to an error message if the variable is
 *	unset (NULL).
 *
 *----------------------------------------------------------------------
 */

static int
CheckIfVarUnset(
    Tcl_Interp *interp,		/* Interpreter for error reporting. */
    Tcl_Obj ** varPtr,
    size_t varIndex)		/* Index of the test variable to check. */
{
    if (varPtr[varIndex] == NULL) {
	char buf[32 + TCL_INTEGER_SPACE];

	snprintf(buf, sizeof(buf), "variable %" TCL_Z_MODIFIER "u is unset (NULL)", varIndex);
	Tcl_ResetResult(interp);
	Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, TCL_INDEX_NONE);
	return 1;
    }
    return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
