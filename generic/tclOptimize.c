/*
 * tclOptimize.c --
 *
 *	This file contains the bytecode optimizer.
 *
 * Copyright (c) 2013 by Miguel Sofer.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclCompile.h"
#include <assert.h>

typedef struct optPad {
    int codeSize;
    int cache;
    int *npaths;
    int *scratch;
    int modified;
    Tcl_HashTable stack;    
    int first;
} optPad;

static void markPath(CompileEnv *envPtr, int pc, optPad *padPtr,
	              int mark);
static int  effectivePC(CompileEnv *envPtr, int pc, optPad *padPtr, int unshared);
static void optimizePush(CompileEnv *envPtr,optPad *padPtr, int pc);
static void MoveUnreachable(CompileEnv *envPtr, optPad *padPtr);
static void CompactCode(CompileEnv *envPtr, optPad *padPtr,
	              int shrinkInst);
static void Optimize(CompileEnv *envPtr, optPad *padPtr);

static void Initialize(CompileEnv *envPtr, optPad *padPtr, int move);

#define INIT_SIZE				\
    int codeSize = padPtr->codeSize

#define INIT_PATHS				\
    int *PATHS = padPtr->npaths

#define INIT_STACK				\
    Tcl_HashTable *stackPtr = &padPtr->stack

/*
 * Helper macros.
 */

#define AddrLength(address) \
    (tclInstructionTable[*(unsigned char *)(address)].numBytes)
#define InstLength(instruction) \
    (tclInstructionTable[(unsigned char)(instruction)].numBytes)
#define InstEffect(instruction) \
    (tclInstructionTable[(unsigned char)(instruction)].stackEffect)
#define Op1Type(instruction) \
    (tclInstructionTable[(unsigned char)(instruction)].opTypes[0])

/*
 * Macros used in the code compactor.
 */

#define GET_INT1_AT_PC(pc)			\
    TclGetInt1AtPtr(envPtr->codeStart + (pc))

#define GET_INT4_AT_PC(pc)			\
    TclGetInt4AtPtr(envPtr->codeStart + (pc))

#define GET_UINT1_AT_PC(pc)			\
    TclGetUInt1AtPtr(envPtr->codeStart + (pc))

#define GET_UINT4_AT_PC(pc)			\
    TclGetUInt4AtPtr(envPtr->codeStart + (pc))

#define SET_INT1_AT_PC(i, pc)				\
    TclStoreInt1AtPtr((i), envPtr->codeStart + (pc))

#define SET_INT4_AT_PC(i, pc)				\
    TclStoreInt4AtPtr((i), envPtr->codeStart + (pc))

#define INST_AT_PC(pc)				\
    (*(envPtr->codeStart + (pc)))

#define NEXT_PC(pc)				\
    pc + InstLength(INST_AT_PC(pc))

#define MARK(pc)						\
    markPath(envPtr, (pc), padPtr, /* mark */ 1)

#define UNMARK(pc)						\
    markPath(envPtr, (pc), padPtr, /* mark */ 0)

#define REPLACE(old, new)			\
    MARK(new); UNMARK(old)

#define UNSHARED(pc) (PATHS[pc] == 1)

#define MODIFIED() padPtr->modified++

/*
 * The code for following a path from a given PC.
 */

#define FOLLOW(pc, unshared)				\
    effectivePC(envPtr, (pc), padPtr, (unshared))


/*
 * ----------------------------------------------------------------------
 *
 * OptimizeBytecode --
 *
 *	An optimizer for bytecode to replace TclOptimizeBytecode.
 *
 * ----------------------------------------------------------------------
 */

void
TclOptimizeBytecode(
    void *env1Ptr)
{
    CompileEnv *envPtr = (CompileEnv *) env1Ptr;
    int codeSize = (envPtr->codeNext - envPtr->codeStart);
    int padSize = sizeof(optPad) + 4*codeSize*sizeof(int);
    optPad *padPtr;

    padPtr = (optPad *) Tcl_AttemptAlloc(padSize);
    if (!padPtr) {
	/* Not enough memory to optimize this code */
	Tcl_Panic("** Not enough mem to optimize! **");
	return;
    }
    padPtr->codeSize = codeSize;
    padPtr->cache = -1;
    padPtr->npaths  = &padPtr->first;
    padPtr->scratch = &padPtr->npaths[2*codeSize + 1];
    
    Tcl_InitHashTable(&padPtr->stack, TCL_ONE_WORD_KEYS);

    /* Simplify the code as much as possible without knowing the paths */

    /* 1. Initial path marking, move unreachable code to after INST_DONE and
     *    compress */

    Initialize(envPtr, padPtr, 1);
    CompactCode(envPtr, padPtr, 0);

    /* 2. Iterate optimizations until all done */
    /* TODO: there MUST be a more efficient approach than relaxation?
     * Possibly save the visit order on init, and process inverting that 
     * order.  */

    Optimize(envPtr, padPtr);
    
    /* 3.  Initialize again to thread jumps and detect dead code. Finally
     * remove all nops and unreachable code, reduce code size.  */

    Initialize(envPtr, padPtr, 0);
    CompactCode(envPtr, padPtr, 1);
    
    Tcl_DeleteHashTable(&padPtr->stack);
    Tcl_Free((char *) padPtr);
}

static void
Initialize(
    CompileEnv *envPtr,
    optPad *padPtr,
    int move)
{
    INIT_PATHS; INIT_SIZE;
    int i, last;
    
    /* 
     * Initialize PATHS to 0.
     */

    for (i=0; i < codeSize; i++) {
	PATHS[i] = 0;
    }

    /*
     * Compute the paths to reachable code
     */

    MARK(0);

    /*
     * Note that "jumps" from ISC (INST_START_CMD) decompilation bailouts may
     * hit targets that become otherwise unreachable after optimization. They
     * need to be marked as reachable.
     * Until we modify the implementation details of the ISC bailout, this needs
     * to happen BEFORE the unreachable code is moved.
     */

    last = -1;
    for (i = 0; i <  envPtr->numCommands; i++) {
	CmdLocation *cmdMapPtr = &envPtr->cmdMapPtr[i];
	if (cmdMapPtr->codeOffset == last) continue;
	last = cmdMapPtr->codeOffset;
	if (last + cmdMapPtr->numCodeBytes < codeSize) {
	    MARK(last + cmdMapPtr->numCodeBytes);
	} else {
	    MARK(codeSize - 1);
	}
    }
    
    /*
     * Make sure that instructions that are only reachable as loop targets of
     * reachable ranges are recognized as shared and reachable. Do not yet
     * mark catch targets. 
     */
    
    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	if (!IS_CATCH_RANGE(rangePtr) &&
		PATHS[rangePtr->codeOffset]) {
	    rangePtr->mainOffset = FOLLOW(rangePtr->mainOffset, 0);
	    MARK(rangePtr->mainOffset);	
	    if (rangePtr->continueOffset != -1) {
		rangePtr->continueOffset = FOLLOW(rangePtr->continueOffset, 0);
		MARK(rangePtr->continueOffset);
	    }
	}
    }

    if (move) {
	//MoveUnreachable(envPtr, padPtr);////
    }

    /*
     * Now insure that all remaining targets are marked as reachable, and also
     * that they are properly marked as being multiple targets
     */
    
    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	if (IS_CATCH_RANGE(rangePtr)) {
	    MARK(rangePtr->mainOffset);
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * CompactCode --
 *
 *	Remove all INST_NOPS and unreachable code. This also shrinks 4-insts
 *	to 1-insts where possible, reduces the code size, and updates all
 *	structs so that the CompileEnv remains consistent.
 *
 * ----------------------------------------------------------------------
 */

void
CompactCode(
    CompileEnv *envPtr,
    optPad *padPtr,
    int shrinkInst)
{
    int codeSize = padPtr->codeSize;
    int *PATHS   = padPtr->npaths;
    int *NEW     = padPtr->scratch;
    int pc, nops, i, nextpc;
    unsigned char inst;

    /*
     * Update range targets
     */

    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	int target;

	if (!PATHS[rangePtr->codeOffset]) continue;
	target = FOLLOW(rangePtr->mainOffset, 0);
	if (rangePtr->mainOffset != target) {
	    REPLACE(rangePtr->mainOffset, target);
	    rangePtr->mainOffset = target;
	}
	if (rangePtr->continueOffset >= 0) {
	    target = FOLLOW(rangePtr->continueOffset, 0);
	    if (rangePtr->continueOffset != target) {
		REPLACE(rangePtr->continueOffset, target);
		rangePtr->continueOffset = target;
	    }
	}
    }
    
    /*
     * First pass: compute new positions, shrink push and var access if required
     */

    restart:
    pc = 0;
    nops = 0;
    for (pc = 0; pc < codeSize; pc = nextpc) {
	int arg, resize;

	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);
	NEW[pc] = pc - nops; /* new position */

	if ((inst == INST_NOP) || !PATHS[pc]) {
	    nops += nextpc - pc;
	    continue;
	}
	if (!shrinkInst) continue;
	
	resize = 0;
	switch (inst) {
	    case INST_PUSH4:
	    case INST_LOAD_SCALAR4:
	    case INST_LOAD_ARRAY4:
	    case INST_STORE_SCALAR4:
	    case INST_STORE_ARRAY4:
		arg = GET_UINT4_AT_PC(pc+1);
		resize = (arg < 256);
		break;
		
	    case INST_JUMP4:
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4:
		arg = GET_INT4_AT_PC(pc+1);
		resize = ((arg <= 127) && (arg >= -128));
		break;
	}
	
	if (resize) {
	    /*
	     * INST_XXX1 is always one less than INST_XXX4 
	     */
	    
	    INST_AT_PC(pc) -= 1;
	    SET_INT1_AT_PC(arg, pc+1);
	    INST_AT_PC(pc+2) = INST_NOP;
	    INST_AT_PC(pc+3) = INST_NOP;
	    INST_AT_PC(pc+4) = INST_NOP;
	    nops +=3;
	}
    }
    
    if (nops == 0) {
	return;
    }
    NEW[codeSize] = codeSize - nops;
    
    /*
     * Update range targets
     */

    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	int target, after;

	if (!PATHS[rangePtr->codeOffset]) continue;
	target = rangePtr->codeOffset;
	after  = rangePtr->codeOffset + rangePtr->numCodeBytes;
	if (rangePtr->numCodeBytes < 0) {
	    /*
	     * A non-existent range; disable it completely
	     */

	    after = target;
	}
	rangePtr->codeOffset = NEW[target];
	rangePtr->numCodeBytes = NEW[after] - NEW[target];
	
	target = FOLLOW(rangePtr->mainOffset, 0);
	rangePtr->mainOffset = NEW[target];
	if (rangePtr->continueOffset >= 0) {
	    target = FOLLOW(rangePtr->continueOffset, 0);
	    rangePtr->continueOffset = NEW[target];
	}
    }

    /*
     * Update CmdLoc data
     */

    {
	CmdLocation *mapPtr = envPtr->cmdMapPtr;

	for (i=0; i < envPtr->numCommands; i++) {
	    /* After the end of this command's code there is either another
	     * instruction, or else the end of the bytecode. Notice that
	     * numCodeBytes may lie miserably: fix that!
	     */
	    
	    int start = mapPtr[i].codeOffset;
	    int next   = start + mapPtr[i].numCodeBytes;

	    if (next > codeSize) {
		next = codeSize;
	    }
	    mapPtr[i].codeOffset = NEW[start];
	    mapPtr[i].numCodeBytes = NEW[next] - NEW[start];
	}
    }

    /*
     * Second pass: move code, update jump offsets, resize the code.
     */
    
    nops = 0;
    for (pc = 0; pc < codeSize; pc = nextpc) {
	int target, i;

	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);
	if ((inst == INST_NOP) || !PATHS[pc]) {
	    continue;
	}
	
	/* update jump offsets */

	switch (inst) {
	    int offset;
	    ForeachInfo *infoPtr;
	    JumptableInfo *info2Ptr;
	    Tcl_HashEntry *hPtr;
	    Tcl_HashSearch hSearch;

	    case INST_JUMP1:
	    case INST_JUMP_TRUE1:
	    case INST_JUMP_FALSE1:
		target = pc + GET_INT1_AT_PC(pc+1);
		offset = NEW[target]-NEW[pc];
		if ((offset == 2) && (inst == INST_JUMP1)) {
		    INST_AT_PC(pc) = INST_NOP;
		    INST_AT_PC(pc+1) = INST_NOP;
		    nops += 2;
		} else {
		    SET_INT1_AT_PC(offset, pc+1);
		}
		break;

	    case INST_JUMP4:
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4:
		target = pc + GET_INT4_AT_PC(pc+1);
		offset = NEW[target]-NEW[pc];
		SET_INT4_AT_PC(offset, pc+1);
		if (offset == 5) {
		    if (inst == INST_JUMP4) {
			INST_AT_PC(pc)   = INST_NOP;
			INST_AT_PC(pc+1) = INST_NOP;
			nops += 2;
			goto push3nops;
		    }
		} else if (shrinkInst &&
			(offset < 127) && (offset > -128)) {
		    INST_AT_PC(pc) -= 1;
		    SET_INT1_AT_PC(offset, pc+1);
		    push3nops:
		    INST_AT_PC(pc+2) = INST_NOP;
		    INST_AT_PC(pc+3) = INST_NOP;
		    INST_AT_PC(pc+4) = INST_NOP;
		    nops += 3;
		}
		break;

	    case INST_FOREACH_START:
		i = GET_UINT4_AT_PC(pc+1);
		infoPtr = (ForeachInfo *) TclFetchAuxData(envPtr, i);
		target = pc + 5 - infoPtr->jumpSize;
		infoPtr->jumpSize = NEW[pc] + 5 - NEW[target];
		break;
		
	    case INST_JUMP_TABLE:
		i = GET_UINT4_AT_PC(pc+1);
		info2Ptr = (JumptableInfo *) TclFetchAuxData(envPtr, i);
		hPtr = Tcl_FirstHashEntry(&info2Ptr->hashTable, &hSearch);
		for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		    target = pc + PTR2INT(Tcl_GetHashValue(hPtr));
		    Tcl_SetHashValue(hPtr, INT2PTR(NEW[target]-NEW[pc]));
		}
		break;
	}

	/* move up opcode and operands, en block  */
	for (i=0; pc+i < nextpc; i++) {
	    int old = pc+i, new = NEW[pc] + i;
	    INST_AT_PC(new) = INST_AT_PC(old);
	    PATHS[new]     += PATHS[old];
	}
    }
    envPtr->codeNext = envPtr->codeStart + NEW[codeSize];
    padPtr->codeSize = NEW[codeSize];
    
    /*
     * Restart until all done; should be rare. Other possible criteria:
     *  - restart if (nops > x*codeSize), if new jumps, ...
     *  - use back jumps as loop indicators, restart only if some backjmp is
     *    reduced in size
     *  - don't restart, bet that there's not much more to be done
     */

    if (nops) {
	codeSize = padPtr->codeSize;
	goto restart;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * effectivePC --
 *
 *	Utility functions. Find the effective newpc that will be executed when
 *	we get at pc, by following through jumps and nops.
 *
 * ----------------------------------------------------------------------
 */

int
effectivePC(
    CompileEnv *envPtr,
    int pc,
    optPad *padPtr,
    int unshared)
{
    unsigned char inst;
    int start = pc, new;
    int *PATHS = padPtr->npaths;

    while (1) {
	if (unshared && !UNSHARED(pc)) {
	    return pc;
	}
	inst = INST_AT_PC(pc);
	switch (inst) {
	    case INST_NOP:
		new = pc + 1;
		break;
		
	    case INST_JUMP1:
		new = pc + GET_INT1_AT_PC(pc+1);
		break;

	    case INST_JUMP4:
		new = pc + GET_INT4_AT_PC(pc+1);
		break;

	    default:
		return pc;
	}
	if (new == start) {
            /* infinite loop! how do we kill it in <= 5 bytes? */
	    INST_AT_PC(new) = INST_CONTINUE;
	    return pc;
	}
	pc = new;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * markPath --
 *
 *    Count the number of paths reaching an instruction, after threading the
 *    jumps. 
 *
 * ----------------------------------------------------------------------
 */

#define PUSH(pc)				\
    iPUSH((pc), padPtr, mark)

#define POP(pc)					\
    ((pc) = iPOP(padPtr))

static void
iPUSH(
    int pc,
    optPad *padPtr,
    int mark)
{
    INIT_SIZE; INIT_PATHS; INIT_STACK;
    int tmp;
    int *cachePtr = &padPtr->cache;
    int cached = *cachePtr;

    if (pc >= codeSize) return;
    
    tmp = ((!mark && (--PATHS[pc] == 0))
	    || (mark && (++PATHS[pc] == 1)));

    if (tmp) {
	if (cached != -1) {
	    Tcl_CreateHashEntry(stackPtr, INT2PTR(cached), &tmp);
	}
	*cachePtr = pc;
    }
}

static int
iPOP(
    optPad *padPtr)
{
    INIT_STACK;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    int pc;
    int cached = padPtr->cache;

    if (cached != -1) {
	padPtr->cache = -1;
	return cached;
    }
    
    hPtr = Tcl_FirstHashEntry(stackPtr, &hSearch);
    if (!hPtr) {
	return -1;
    }
    
    pc = PTR2INT(Tcl_GetHashKey(stackPtr, hPtr));
    Tcl_DeleteHashEntry(hPtr);
    return pc;
}

void
markPath(
    CompileEnv *envPtr,
    int pc,
    optPad *padPtr,
    int mark)
{
    INIT_PATHS;
    unsigned char inst;
    int nextpc, target;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    
    /*
     * Note that each pc will be followed at most once, so that only branch
     * targets have a PATHS count > 1.
     */

    if (mark) {
	if (PATHS[pc] > 0) {
	    PATHS[pc]++;
	    return;
	}
    } else {
	if (PATHS[pc] > 1) {
	    PATHS[pc]--;
	    return;
	} else if (PATHS[pc] <= 0) {
	    PATHS[pc] = 0;
	    return;
	}
    }
    
    PUSH(pc);
    while (POP(pc) != -1) {
	if ((pc < 0) || (pc > padPtr->codeSize)) {
	    Tcl_Panic("ERR in markPath: pc out of range");
	}

	inst = INST_AT_PC(pc);
	nextpc = NEXT_PC(pc);
	mark = (PATHS[pc] > 0);
	switch(inst) {
	    case INST_DONE:
	    case INST_TAILCALL:
	    case INST_CONTINUE:
	    case INST_BREAK:
		break;

	    case INST_JUMP_TRUE1:
	    case INST_JUMP_FALSE1:
		PUSH(nextpc);
	    case INST_JUMP1:
		PUSH(pc + GET_INT1_AT_PC(pc+1));
		break;
		
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4:
		PUSH(nextpc);
	    case INST_JUMP4:
		target = pc + GET_INT4_AT_PC(pc+1);
		if (mark) {
		    target = FOLLOW(target, 0);
		    SET_INT4_AT_PC(target-pc, pc+1);
		}
		PUSH(target);
		break;

	    case INST_JUMP_TABLE:		
		hPtr = Tcl_FirstHashEntry(
		    &JUMPTABLEINFO(envPtr, envPtr->codeStart+pc+1)->hashTable,
		    &hSearch);
		for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		    target = pc + PTR2INT(Tcl_GetHashValue(hPtr));
		    if (mark) {
			target = FOLLOW(target, 0);
			Tcl_SetHashValue(hPtr, INT2PTR(target - pc));
		    }
		    PUSH(target);
		}
		PUSH(nextpc);
		break;

	    default:
		PUSH(nextpc);
		break;
	}
    }
#undef PUSH
#undef POP
}

/*
 * Move exception handling code to after INST_DONE. ASSUMES that there are no
 * FORCED 1-jumps from exception handling code to normal code.
 */

void
MoveUnreachable(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    INIT_PATHS;
    int *NEW = padPtr->scratch;
    int pc, nextpc, target, inst;
    int fixend, curr, i, imax;
    ExceptionRange *rangePtr;
    JumptableInfo *infoPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    int codeSize = padPtr->codeSize;
    
    /*
     * Move unreachable code out to after done, after checking there is enough
     * room to do it - be generous.
     */

    if ((envPtr->codeEnd - envPtr->codeStart) <
	    2 * (envPtr->codeNext - envPtr->codeStart)) {
	TclExpandCodeArray(envPtr);
    }

    for (pc = 0; pc < codeSize; pc = nextpc) {
	nextpc = NEXT_PC(pc);
	
	if (PATHS[pc] > 0) {
	    NEW[pc] = pc;
	    continue;
	}
	
	curr = (envPtr->codeNext - envPtr->codeStart);
	
	inst   = INST_AT_PC(pc);
	NEW[pc] = curr;
	INST_AT_PC(curr) = INST_AT_PC(pc);
	
	/* jump back iff next inst is reachable */
	fixend  = (nextpc >= codeSize)? 0 : PATHS[nextpc];

	switch (inst) {
	    /*
	     * There are no 1-jumps to consider.
	     */

	    case INST_JUMP4:
		fixend = 0;
	    case INST_JUMP_FALSE4:
	    case INST_JUMP_TRUE4:
		target = pc + GET_INT4_AT_PC(pc+1);
		SET_INT4_AT_PC(target, curr+1);
		break;
		
	    case INST_JUMP_TABLE:
		i = GET_UINT4_AT_PC(pc+1);
		SET_INT4_AT_PC(i, curr+1);
		infoPtr = (JumptableInfo *) TclFetchAuxData(envPtr, i);
		hPtr = Tcl_FirstHashEntry(&infoPtr->hashTable, &hSearch);
		for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		    target = pc + PTR2INT(Tcl_GetHashValue(hPtr));
		    Tcl_SetHashValue(hPtr, INT2PTR(target));
		}
		break;
		
	    case INST_DONE:
	    case INST_TAILCALL:
	    case INST_CONTINUE:
	    case INST_BREAK:
		fixend = 0;
		break;

	    default:
		for (i=1; i < nextpc-pc; i++) {
		    INST_AT_PC(curr+i) = INST_AT_PC(pc+i);
		    NEW[pc+i] = curr+i;
		}
		break;
	}
	envPtr->codeNext += (nextpc-pc);
	
	if (fixend) {
	    /* add a jump back to ok code */
	    curr = (envPtr->codeNext - envPtr->codeStart);
	    envPtr->codeNext += 5;
	    INST_AT_PC(curr) = INST_JUMP4;
	    SET_INT4_AT_PC(nextpc, curr+1);
	}
    }

    /*
     * The code has been moved, jump targets are all absolute. Pass over the
     * moved code, updating jump targets to the correct (relative) value.
     */

    for (pc = codeSize;
	     pc < envPtr->codeNext-envPtr->codeStart;
	     pc = nextpc) {
	nextpc  = NEXT_PC(pc);
	inst    = INST_AT_PC(pc);
	NEW[pc] = pc;
	PATHS[pc] = 0;
	
	switch (inst) {
	    case INST_JUMP4:
	    case INST_JUMP_FALSE4:
	    case INST_JUMP_TRUE4:
		target = NEW[GET_INT4_AT_PC(pc+1)];
		SET_INT4_AT_PC(target-pc, pc+1);
		break;
		
	    case INST_JUMP_TABLE:
		infoPtr = (JumptableInfo *) TclFetchAuxData(envPtr, GET_UINT4_AT_PC(pc+1));
		hPtr = Tcl_FirstHashEntry(&infoPtr->hashTable, &hSearch);
		for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		    target = NEW[PTR2INT(Tcl_GetHashValue(hPtr))];
		    Tcl_SetHashValue(hPtr, INT2PTR(target-pc));
		}
		break;
	}
    }

    /* Reset things to normal */

    padPtr->codeSize = envPtr->codeNext - envPtr->codeStart;

    /* Update all ranges and targets */

    imax =  envPtr->exceptArrayNext;
    for (i = 0 ; i < imax; i++) {
	rangePtr = &envPtr->exceptArrayPtr[i];
	target = FOLLOW(NEW[rangePtr->mainOffset], 0);
	rangePtr->mainOffset = target;
	if (IS_CATCH_RANGE(rangePtr)) {
	    MARK(target);
	} else if (rangePtr->continueOffset >= 0) {
	    rangePtr->continueOffset = FOLLOW(NEW[rangePtr->continueOffset], 0);
	}
	target = rangePtr->codeOffset + rangePtr->numCodeBytes;
	rangePtr->codeOffset = NEW[rangePtr->codeOffset];
	rangePtr->numCodeBytes = NEW[target]-rangePtr->codeOffset;
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * Optimize --
 *
 *	Replaces instructions and threads jumps in order to speed up the
 *	execution. It also marks unreachable code, replacing it with NOPS that
 *	can later be removed.
 *
 * ----------------------------------------------------------------------
 */

void
Optimize(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    INIT_PATHS;
    int codeSize = padPtr->codeSize;
    int pc, nextpc, tmp;
    unsigned char inst;
    int negate;
    
    restart:
    
    padPtr->modified = 0;
    for (pc = 0; pc < codeSize; pc = nextpc) {
	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);
	if ((inst == INST_NOP) || !PATHS[pc]) continue;
	
	switch(inst) {
	    case INST_PUSH4:
		optimizePush(envPtr, padPtr, pc);
		break;

	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4: {
		int t1, tgt, new;

		/*
		 * detect stupid jump-around-jump, untangle 
		 *
		 *    <-- PC:     CONDJMP->NEW       !CONDJMP->TGT
		 *    |   T1:     JMP->TGT      <=>   NOP
		 *    --> NEW:    FOO                 FOO
		 */

		new = FOLLOW(pc + GET_INT4_AT_PC(pc+1), 0);
		t1 = FOLLOW(NEXT_PC(pc), 1); 
		if (UNSHARED(t1) && (INST_AT_PC(t1) == INST_JUMP4) &&
			(new == FOLLOW(NEXT_PC(t1), 0))) {
		    /* ENTANGLED! undo ... */
		    INST_AT_PC(t1) = INST_NOP;
		    tgt = t1 + GET_INT4_AT_PC(t1+1);
		    tgt = FOLLOW(tgt, 0);
		    INST_AT_PC(pc) ^= 2;
		    SET_INT4_AT_PC(tgt-pc, pc+1);
		    MODIFIED();
		}
		break;
	    }

	    case INST_BREAK: 
	    case INST_CONTINUE: {
		int range = GET_INT4_AT_PC(pc+1);
		ExceptionRange *rangePtr = envPtr->exceptArrayPtr + range;
		
		if ((range == -1) || !rangePtr || IS_CATCH_RANGE(rangePtr)) break;
		if ((rangePtr->codeOffset == -1) || !PATHS[rangePtr->codeOffset]) break;

		if (inst == INST_BREAK) {
		    tmp = FOLLOW(rangePtr->mainOffset, 0);
		} else {
		    if (rangePtr->continueOffset < 0) {
			Tcl_Panic("Optimizer found continueOffset==%i, should not happen!", rangePtr->continueOffset);
		    }
		    tmp = FOLLOW(rangePtr->continueOffset, 0);
		}
		INST_AT_PC(pc) = INST_JUMP4;
		SET_INT4_AT_PC(tmp - pc, pc+1);
		MODIFIED();
		break;
	    }

	    case INST_LNOT:
		negate = 1;
		tmp = nextpc;
	    redoInstLNot: 
		tmp = FOLLOW(tmp, 1);
		if (!UNSHARED(tmp)) break;
		switch(INST_AT_PC(tmp)) {
		    case INST_JUMP_TRUE1:
		    case INST_JUMP_FALSE1:
		    case INST_JUMP_TRUE4:
		    case INST_JUMP_FALSE4:
			if (negate) {
			    /* Trick: this transforms to the negation! */
			    INST_AT_PC(tmp) ^= 2;
			}
			INST_AT_PC(pc) = INST_NOP;
			MODIFIED();
			break;

		    case INST_LNOT:
			negate = !negate;
			
		    case INST_UMINUS:
		    case INST_UPLUS:
		    case INST_TRY_CVT_TO_NUMERIC:
			tmp++;
			goto redoInstLNot;
		}
		break;

	    case INST_UPLUS:
	    case INST_TRY_CVT_TO_NUMERIC:
		tmp = FOLLOW(nextpc, 0);
	        switch (INST_AT_PC(tmp)) {
		    case INST_JUMP_TRUE1:
		    case INST_JUMP_TRUE4:
		    case INST_JUMP_FALSE1:
		    case INST_JUMP_FALSE4:
		    case INST_INCR_SCALAR1:
		    case INST_INCR_ARRAY1:
		    case INST_INCR_ARRAY_STK:
		    case INST_INCR_SCALAR_STK:
		    case INST_INCR_STK:
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
			INST_AT_PC(pc) = INST_NOP;
			MODIFIED();
			break;

		    default:
			break;
		}
		break;

	    case INST_INCR_SCALAR1:
	    case INST_INCR_ARRAY1:
	    case INST_INCR_ARRAY_STK:
	    case INST_INCR_SCALAR_STK:
	    case INST_INCR_STK:
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
	    case INST_BITNOT:
	    case INST_UMINUS:
		tmp = FOLLOW(nextpc, 1);
		if (!UNSHARED(tmp)) break;
		switch (INST_AT_PC(tmp)) {
		    case INST_TRY_CVT_TO_NUMERIC:
		    case INST_UPLUS:
			INST_AT_PC(tmp) = INST_NOP;
			MODIFIED();
			break;
		}
		break;
	}
    }

    if (padPtr->modified) {
	goto restart;
    }
}

void
optimizePush(
    CompileEnv *envPtr,
    optPad *padPtr,
    int pc)
{
    int inst, i, target, tmp;
    Tcl_Obj *objPtr;
    int negate = 0;
    
    if (INST_AT_PC(pc) != INST_PUSH4) return;
    
    tmp = FOLLOW(NEXT_PC(pc), 0);
    inst = INST_AT_PC(tmp);
    objPtr = envPtr->literalArrayPtr[GET_UINT4_AT_PC(pc+1)].objPtr;

    redoSwitch:
    switch (inst) {
	case INST_POP:
	    tmp = FOLLOW(tmp + 1, 0);
	    INST_AT_PC(pc) = INST_JUMP4;
	    SET_INT4_AT_PC(tmp - pc, pc+1);
	    MODIFIED();
	    return;
	    
	case INST_LNOT:
	    negate = !negate;
	    tmp = FOLLOW(tmp + 1, 0);
	    inst = INST_AT_PC(tmp);
	    goto redoSwitch;
		
	case INST_UMINUS:
	case INST_UPLUS:
	case INST_TRY_CVT_TO_NUMERIC:
	    if (Tcl_GetIntFromObj(NULL, objPtr, &i) == TCL_ERROR) {
		return;
	    }
	    tmp = FOLLOW(tmp + 1, 0);
	    inst = INST_AT_PC(tmp);
	    goto redoSwitch;
	    
	case INST_JUMP_TRUE4:
	case INST_JUMP_FALSE4:
	    if (Tcl_GetBooleanFromObj(NULL, objPtr, &i) == TCL_ERROR) {
		return;
	    }
	    /* let i indicate if we take the jump or not */
	    if ((i&&!negate) || (!i && negate)) {
		i = ((inst == INST_JUMP_TRUE1) || (inst == INST_JUMP_TRUE4));
	    } else {
		i = ((inst == INST_JUMP_FALSE1) || (inst == INST_JUMP_FALSE4));
	    }
	    if (i) {
		switch (inst) {
		    case INST_JUMP_TRUE1:
		    case INST_JUMP_FALSE1:
			target = tmp + GET_INT1_AT_PC(tmp+1);
			break;
			
		    case INST_JUMP_TRUE4:
		    case INST_JUMP_FALSE4:
			target = tmp + GET_INT4_AT_PC(tmp+1);
			break;
		}
	    } else {
		target = NEXT_PC(tmp);
	    }
	    target = FOLLOW(target, 0);
	    INST_AT_PC(pc) = INST_JUMP4;
	    SET_INT4_AT_PC(target-pc, pc+1);
	    MODIFIED();
	    return;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
