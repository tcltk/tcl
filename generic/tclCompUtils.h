/*
 * tclCompUtils.h --
 *
 *	This file contains utility macros for generating Tcl bytecode.
 *
 * Copyright (c) 2025 Donal K. Fellows <dkf@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TCLCOMPUTILS
#define _TCLCOMPUTILS 1
 
#include "tclCompile.h"

/*
 * Shorthand macros for instruction issuing.
 */

#define OP(name)	TclEmitOpcode(INST_##name, envPtr)
#define OP1(name,val)	TclEmitInstInt1(INST_##name,(val),envPtr)
#define OP4(name,val)	TclEmitInstInt4(INST_##name,(val),envPtr)

#define OP14(name,val1,val2) \
    TclEmitInstInt14(INST_##name,(val1),(val2),envPtr)
#define OP44(name,val1,val2) \
    TclEmitInstInt44(INST_##name,(val1),(val2),envPtr)
#define OP41(name,val1,val2) \
    TclEmitInstInt41(INST_##name,(val1),(val2),envPtr)

#define PUSH(str) \
    PushStringLiteral(envPtr, str)
#define BACKLABEL(var) \
    (var)=CurrentOffset(envPtr)
#define BACKJUMP(name, var) \
    TclEmitInstInt4(INST_##name,(var)-CurrentOffset(envPtr),envPtr)
#define FWDJUMP(name,var) \
    (var)=CurrentOffset(envPtr);TclEmitInstInt4(INST_##name,0,envPtr)
#define FWDLABEL(var) \
    TclStoreInt4AtPtr(CurrentOffset(envPtr)-(var),envPtr->codeStart+(var)+1)
#define INVOKE(name) \
    TclEmitInvoke(envPtr,INST_##name)

#define CATCH_RANGE(range) \
    for(int tcl__range=(ExceptionRangeStarts(envPtr,(range)),0);	\
	    !tcl__range;						\
	    tcl__range=(ExceptionRangeEnds(envPtr,(range)),1))

#define STKDELTA(delta) \
    TclAdjustStackDepth((delta), envPtr)

#endif

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
