/*
 * tclCompile.h --
 *
 * Copyright (c) 1996-1998 Sun Microsystems, Inc.
 * Copyright (c) 1998-2000 by Scriptics Corporation.
 * Copyright (c) 2001 by Kevin B. Kenny. All rights reserved.
 * Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

struct ByteCode;		/* Forward declaration. */


typedef struct {
    const char *op;		/* Do not call it 'operator': C++ reserved */
    const char *expected;
    union {
	int numArgs;
	int identity;
    } i;
} TclOpCmdClientData;

MODULE_SCOPE int	TclSingleOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclSortingOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclVariadicOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
MODULE_SCOPE int	TclNoIdentOpCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);


MODULE_SCOPE ExecEnv *	TclCreateExecEnv(Tcl_Interp *interp, int size);
MODULE_SCOPE void	TclDeleteExecEnv(ExecEnv *eePtr);

MODULE_SCOPE struct ByteCode *	TclCompileObj(Tcl_Interp *interp, Tcl_Obj *objPtr);

MODULE_SCOPE int	TclNRExecuteByteCode(Tcl_Interp *interp,
			    struct ByteCode *codePtr);

MODULE_SCOPE Tcl_ObjCmdProc	TclNRInterpCoroutine;


#ifdef REQUIRE_BC_DEF
typedef struct _ByteCode {
    TclHandle interpHandle;	/* Handle for interpreter containing the
				 * compiled code. Commands and their compile
				 * procs are specific to an interpreter so the
				 * code emitted will depend on the
				 * interpreter. */
    Namespace *nsPtr;		/* Namespace context in which this code was
				 * compiled. If the code is executed if a
				 * different namespace, it must be
				 * recompiled. */
} _ByteCode;
#endif
