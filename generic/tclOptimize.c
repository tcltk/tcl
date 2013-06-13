/*
 * tclOptimize.c --
 *
 *	This file contains the bytecode optimizer.
 *
 * Copyright (c) 2013 by Donal Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"
#include <assert.h>

#define DefineTargetAddress(tablePtr, address) \
    ((void) Tcl_CreateHashEntry((tablePtr), (void *) (address), &isNew))
#define IsTargetAddress(tablePtr, address) \
    (Tcl_FindHashEntry((tablePtr), (void *) (address)) != NULL)

static void
LocateTargetAddresses(
    CompileEnv *envPtr,
    Tcl_HashTable *tablePtr)
{
    unsigned char *pc, *target;
    int size, isNew, i;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;

    Tcl_InitHashTable(tablePtr, TCL_ONE_WORD_KEYS);

    /*
     * The starts of commands represent target addresses.
     */

    for (i=0 ; i<envPtr->numCommands ; i++) {
	DefineTargetAddress(tablePtr, TclCmdStartAddress(envPtr, i));
    }

    /*
     * Find places where we should be careful about replacing instructions
     * because they are the targets of various types of jumps.
     */

    for (pc = envPtr->codeStart ; pc < envPtr->codeNext ; pc += size) {
	size = tclInstructionTable[*pc].numBytes;
	switch (*pc) {
	case INST_JUMP1:
	case INST_JUMP_TRUE1:
	case INST_JUMP_FALSE1:
	    target = pc + TclGetInt1AtPtr(pc+1);
	    goto storeTarget;
	case INST_JUMP4:
	case INST_JUMP_TRUE4:
	case INST_JUMP_FALSE4:
	    target = pc + TclGetInt4AtPtr(pc+1);
	    goto storeTarget;
	case INST_BEGIN_CATCH4:
	    target = envPtr->codeStart + envPtr->exceptArrayPtr[
		    TclGetUInt4AtPtr(pc+1)].codeOffset;
	storeTarget:
	    DefineTargetAddress(tablePtr, target);
	    break;
	case INST_JUMP_TABLE:
	    hPtr = Tcl_FirstHashEntry(
		    &JUMPTABLEINFO(envPtr, pc+1)->hashTable, &hSearch);
	    for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		target = pc + PTR2INT(Tcl_GetHashValue(hPtr));
		DefineTargetAddress(tablePtr, target);
	    }
	    break;
	case INST_RETURN_CODE_BRANCH:
	    for (i=TCL_ERROR ; i<TCL_CONTINUE+1 ; i++) {
		DefineTargetAddress(tablePtr, pc + 2*i - 1);
	    }
	    break;
	case INST_START_CMD:
	    assert (envPtr->atCmdStart < 2);
	}
    }

    /*
     * Add a marker *after* the last bytecode instruction. WARNING: points to
     * one past the end!
     */

    DefineTargetAddress(tablePtr, pc);

    /*
     * Enter in the targets of exception ranges.
     */

    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];

	if (rangePtr->type == CATCH_EXCEPTION_RANGE) {
	    target = envPtr->codeStart + rangePtr->catchOffset;
	    DefineTargetAddress(tablePtr, target);
	} else {
	    target = envPtr->codeStart + rangePtr->breakOffset;
	    DefineTargetAddress(tablePtr, target);
	    if (rangePtr->continueOffset >= 0) {
		target = envPtr->codeStart + rangePtr->continueOffset;
		DefineTargetAddress(tablePtr, target);
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * TclOptimizeBytecode --
 *
 *	A very simple peephole optimizer for bytecode.
 *
 * ----------------------------------------------------------------------
 */

void
TclOptimizeBytecode(
    CompileEnv *envPtr)
{
    unsigned char *pc;
    int size;
    Tcl_HashTable targets;

    /*
     * Replace PUSH/POP sequences (when non-hazardous) with NOPs. Also replace
     * PUSH empty/CONCAT and TRY_CVT_NUMERIC (when followed by an operation
     * that guarantees the check for arithmeticity) and eliminate LNOT when we
     * can invert the following JUMP condition.
     */

    LocateTargetAddresses(envPtr, &targets);
    for (pc = envPtr->codeStart ; pc < envPtr->codeNext ; pc += size) {
	int blank = 0, i, inst;

	size = tclInstructionTable[*pc].numBytes;
	while (*(pc+size) == INST_NOP) {
	    if (IsTargetAddress(&targets, pc + size)) {
		break;
	    }
	    size += tclInstructionTable[INST_NOP].numBytes;
	}
	if (IsTargetAddress(&targets, pc + size)) {
	    continue;
	}
	inst = *(pc + size);
	switch (*pc) {
	case INST_PUSH1:
	    if (inst == INST_POP) {
		blank = size + tclInstructionTable[inst].numBytes;
	    } else if (inst == INST_CONCAT1
		    && TclGetUInt1AtPtr(pc + size + 1) == 2) {
		Tcl_Obj *litPtr = TclFetchLiteral(envPtr,
			TclGetUInt1AtPtr(pc + 1));
		int numBytes;

		(void) Tcl_GetStringFromObj(litPtr, &numBytes);
		if (numBytes == 0) {
		    blank = size + tclInstructionTable[inst].numBytes;
		}
	    }
	    break;
	case INST_PUSH4:
	    if (inst == INST_POP) {
		blank = size + 1;
	    } else if (inst == INST_CONCAT1
		    && TclGetUInt1AtPtr(pc + size + 1) == 2) {
		Tcl_Obj *litPtr = TclFetchLiteral(envPtr,
			TclGetUInt4AtPtr(pc + 1));
		int numBytes;

		(void) Tcl_GetStringFromObj(litPtr, &numBytes);
		if (numBytes == 0) {
		    blank = size + tclInstructionTable[inst].numBytes;
		}
	    }
	    break;
	case INST_LNOT:
	    switch (inst) {
	    case INST_JUMP_TRUE1:
		blank = size;
		*(pc + size) = INST_JUMP_FALSE1;
		break;
	    case INST_JUMP_FALSE1:
		blank = size;
		*(pc + size) = INST_JUMP_TRUE1;
		break;
	    case INST_JUMP_TRUE4:
		blank = size;
		*(pc + size) = INST_JUMP_FALSE4;
		break;
	    case INST_JUMP_FALSE4:
		blank = size;
		*(pc + size) = INST_JUMP_TRUE4;
		break;
	    }
	    break;
	case INST_TRY_CVT_TO_NUMERIC:
	    switch (inst) {
	    case INST_JUMP_TRUE1:
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE1:
	    case INST_JUMP_FALSE4:
	    case INST_INCR_SCALAR1:
	    case INST_INCR_ARRAY1:
	    case INST_INCR_ARRAY_STK:
	    case INST_INCR_SCALAR_STK:
	    case INST_INCR_STK:
	    case INST_LOR:
	    case INST_LAND:
	    case INST_EQ:
	    case INST_NEQ:
	    case INST_LT:
	    case INST_LE:
	    case INST_GT:
	    case INST_GE:
	    case INST_MOD:
	    case INST_LSHIFT:
	    case INST_RSHIFT:
	    case INST_BITOR:
	    case INST_BITXOR:
	    case INST_BITAND:
	    case INST_EXPON:
	    case INST_ADD:
	    case INST_SUB:
	    case INST_DIV:
	    case INST_MULT:
	    case INST_LNOT:
	    case INST_BITNOT:
	    case INST_UMINUS:
	    case INST_UPLUS:
	    case INST_TRY_CVT_TO_NUMERIC:
		blank = size;
		break;
	    }
	    break;
	}
	if (blank > 0) {
	    for (i=0 ; i<blank ; i++) {
		*(pc + i) = INST_NOP;
	    }
	    size = blank;
	}
    }
    Tcl_DeleteHashTable(&targets);

    /*
     * Trim unreachable instructions after a DONE.
     */

    LocateTargetAddresses(envPtr, &targets);
    for (pc = envPtr->codeStart ; pc < envPtr->codeNext-1 ; pc += size) {
	int clear = 0;

	size = tclInstructionTable[*pc].numBytes;
	if (*pc != INST_DONE) {
	    continue;
	}
	assert (size == 1);
	while (!IsTargetAddress(&targets, pc + 1 + clear)) {
	    clear += tclInstructionTable[*(pc + 1 + clear)].numBytes;
	}
	if (pc + 1 + clear == envPtr->codeNext) {
	    envPtr->codeNext -= clear;
	} else {
	    while (clear --> 0) {
		*(pc + 1 + clear) = INST_NOP;
	    }
	}
    }

    Tcl_DeleteHashTable(&targets);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
