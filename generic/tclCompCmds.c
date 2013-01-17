/*
 * tclCompCmds.c --
 *
 *	This file contains compilation procedures that compile various Tcl
 *	commands into a sequence of instructions ("bytecodes").
 *
 * Copyright (c) 1997-1998 Sun Microsystems, Inc.
 * Copyright (c) 2001 by Kevin B. Kenny.  All rights reserved.
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2004-2006 by Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompileInt.h"
#include <assert.h>

/*
 * Prototypes for procedures defined later in this file:
 */

/*
 * Macro that encapsulates an efficiency trick that avoids a function call for
 * the simplest of compiles. The ANSI C "prototype" for this macro is:
 *
 * static void		CompileWord(CompileEnv *envPtr, Tcl_Token *tokenPtr,
 *			    Tcl_Interp *interp);
 */

#define CompileWord(envPtr, tokenPtr, interp) \
    if ((tokenPtr)->type == TCL_TOKEN_SIMPLE_WORD) {			\
	TclEmitPush(TclRegisterNewLiteral((envPtr), (tokenPtr)[1].start, \
		(tokenPtr)[1].size), (envPtr));				\
    } else {								\
	TclCompileTokens((interp), (tokenPtr)+1, (tokenPtr)->numComponents, \
		(envPtr));						\
    }

/*
 * Often want to issue one of two versions of an instruction based on whether
 * the argument will fit in a single byte or not. This makes it much clearer.
 */

#define Emit14Inst(nm,idx,envPtr) \
	TclEmitInstInt4(nm##4,idx,envPtr)

/*
 * Flags bits used by PushVarName.
 */

#define TCL_NO_LARGE_INDEX 1	/* Do not return localIndex value > 255 */
#define TCL_NO_ELEMENT 2	/* Do not push the array element. */


static void
CompileReturnInternal(
    CompileEnv *envPtr,
    unsigned char op,
    int code,
    int level,
    Tcl_Obj *returnOpts)
{
    TclEmitPush(TclAddLiteralObj(envPtr, returnOpts, NULL), envPtr);
    TclEmitInstInt4(op, code, envPtr);
    TclEmitInt4(level, envPtr);
}

void
TclCompileSyntaxError(
    Tcl_Interp *interp,
    CompileEnv *envPtr)
{
    Tcl_Obj *msg = Tcl_GetObjResult(interp);
    int numBytes;
    const char *bytes = TclGetStringFromObj(msg, &numBytes);

    TclEmitPush(TclRegisterNewLiteral(envPtr, bytes, numBytes), envPtr);
    CompileReturnInternal(envPtr, INST_SYNTAX, TCL_ERROR, 0,
	    Tcl_GetReturnOptions(interp, TCL_ERROR));
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
