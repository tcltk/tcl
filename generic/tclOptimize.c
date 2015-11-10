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
    Tcl_HashTable stack;    
    int first;
} optPad;

static void markPath(CompileEnv *envPtr, int pc, optPad *padPtr,
	              int mark);
static int  effectivePCInternal(CompileEnv *envPtr, int pc,
	              optPad *padPtr, int start);
static void ThreadJumps(CompileEnv *envPtr, optPad *padPtr);
static int  UpdateJump(CompileEnv *envPtr,optPad *padPtr, int pc);
static int  optimizePush_0(CompileEnv *envPtr,optPad *padPtr, int pc);
static int  optimizePush_1(CompileEnv *envPtr,optPad *padPtr, int pc);
static void MoveUnreachable(CompileEnv *envPtr, optPad *padPtr);
static void CompactCode(CompileEnv *envPtr, optPad *padPtr,
	              int shrinkInst);
static void Optimize_0(CompileEnv *envPtr, optPad *padPtr);
static void Optimize_1(CompileEnv *envPtr, optPad *padPtr);

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

#define UPDATE_JUMP(pc)				\
    UpdateJump(envPtr, padPtr, (pc))

#define UNSHARED(pc) (PATHS[pc] == 1)


/*
 * The code for following a path from a given PC.
 */

#define FOLLOW(pc)				\
    effectivePC(envPtr, (pc), padPtr)

static inline int
effectivePC(
    CompileEnv *envPtr,
    int pc,
    optPad *padPtr)
{
    unsigned char inst = INST_AT_PC(pc);
    
    if ((inst == INST_NOP)
	    || (inst == INST_JUMP1)
	    || (inst == INST_JUMP4)) {
	/* do update the whole path forward */
	pc = effectivePCInternal(envPtr, pc, padPtr, pc);
    }
    return pc;
}


static void       Initialize(CompileEnv *envPtr, optPad *padPtr);


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

    /* 1. Path-independent opts: push elimination, jump threading, etc. We
     * could advance some jumps (to cond-jump, to done or return) */

    Optimize_0(envPtr, padPtr);
    
    /* 2. Initial path marking, move unreachable code to after INST_DONE and
     *    compress */

    Initialize(envPtr, padPtr);

    /* 3. Iterate path-dependent optimizations until all done; this can
     * conceivably enable new path-independent opts, so try again. Note that
     * paths need to be kept up-to-date for this to work properly - careful
     * with backwards jumps (? think properly). Maybe separate jump-threading
     * from path computations. */
    /* TODO: there MUST be a more efficient approach than relaxation */
    
    Optimize_1(envPtr, padPtr);
    
    /* Finally remove all nops and unreachable code, reduce code size */
    CompactCode(envPtr, padPtr, 1);

    Tcl_DeleteHashTable(&padPtr->stack);
    Tcl_Free((char *) padPtr);
}

static void
Initialize(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    INIT_PATHS; INIT_SIZE;
    int i;
    
    /*
     * Initialize NEXT to identity, PATHS to 0.
     */

    for (i=0; i < codeSize; i++) {
	PATHS[i] = 0;
    }

    /*
     * Compute the paths to reachable code
     */

    MARK(0);

    /*
     * Make sure that instructions that are only reachable as loop targets of
     * reachable ranges are recognized as shared and reachable. Do not yet
     * mark catch targets. 
     */
    
    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	if ((rangePtr->type == LOOP_EXCEPTION_RANGE) &&
		PATHS[rangePtr->codeOffset]) {
	    MARK(rangePtr->breakOffset);	
	    if (rangePtr->continueOffset != -1) {
		MARK(rangePtr->continueOffset);
	    }
	}
    }
    
    MoveUnreachable(envPtr, padPtr);

    /*
     * Now insure that all remaining targets are marked as reachable, and also
     * thet they are properly marked as being multiple targets
     */
    
    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	if (rangePtr->type == CATCH_EXCEPTION_RANGE) {
	    MARK(rangePtr->catchOffset);
	}
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

#define MODIFIED					\
    goto restart

void
Optimize_0(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    int codeSize = padPtr->codeSize;
    int pc, nextpc;
    unsigned char inst;

    for (pc = 0; pc < codeSize; pc = nextpc) {
	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);
	
	switch(inst) {
	    case INST_RETURN_CODE_BRANCH:
		nextpc += 10;
		continue;

	    case INST_PUSH4:
		optimizePush_0(envPtr, padPtr, pc);

	    case INST_JUMP1:
	    case INST_JUMP_TRUE1:
	    case INST_JUMP_FALSE1:
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4:
	    case INST_JUMP4:
	    case INST_JUMP_TABLE:
		UPDATE_JUMP(pc);
		break;
    
	}
    }
}

void
Optimize_1(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    INIT_PATHS;
    int codeSize = padPtr->codeSize;
    int pc, nextpc, tmp;
    unsigned char inst;

    restart:
    ThreadJumps(envPtr, padPtr);
    for (pc = 0; pc < codeSize; pc = nextpc) {
	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);
	if ((inst == INST_NOP) || !PATHS[pc]) continue;
	
	switch(inst) {
	    case INST_RETURN_CODE_BRANCH:
		nextpc += 10;
		continue;

	    case INST_JUMP1:
	    case INST_JUMP_TRUE1:
	    case INST_JUMP_FALSE1:
		//Tcl_Panic("found a 1-jump!");
		break;
		
	    case INST_PUSH4:
		if (optimizePush_1(envPtr, padPtr, pc)) {
		    if (!UNSHARED(pc)) MODIFIED;
		}
		break;

	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4: {
		int t1, t, tgt, new;

		/*
		 * detect stupid jump-around-jump, untangle 
		 *
		 *    <-- PC:     CONDJMP->OLD       !CONDJMP->TGT
		 *    |   T1:     JMP->TGT      <=>   NOP
		 *    --> OLD:    FOO                 FOO
		 */

		if (UPDATE_JUMP(pc)) MODIFIED;
		new = pc + GET_INT4_AT_PC(pc+1);
		for (t1 = NEXT_PC(pc);
		     (INST_AT_PC(t1) == INST_NOP) && UNSHARED(t1);
		     t1++) {};
		if ((INST_AT_PC(t1) == INST_JUMP4) && UNSHARED(t1)) {
		    if (new == FOLLOW(NEXT_PC(t1))) {
			/* ENTANGLED! undo ... */
			tgt = t1 + GET_INT4_AT_PC(t1+1);
			for (t = t1; t < t1 + 5; t++) {
			    INST_AT_PC(t) = INST_NOP;
			    PATHS[t] = 0;
			}
			t = pc + GET_INT4_AT_PC(pc+1);
			REPLACE(t, t1);
			
			t = FOLLOW(tgt);
			REPLACE(tgt, t);
			INST_AT_PC(pc) ^= 2;
			SET_INT4_AT_PC(t-pc, pc+1);
			if (!UNSHARED(pc)) MODIFIED;
		    }
		}
		break;
	    }

	    case INST_JUMP4:
		if (UPDATE_JUMP(pc)) MODIFIED;
		break;
    
	    case INST_LNOT:
		tmp = FOLLOW(nextpc);
		if (UNSHARED(tmp)) {
		    inst = INST_AT_PC(tmp);
		    switch(inst) {
			case INST_JUMP_TRUE1:
			case INST_JUMP_FALSE1:
			case INST_JUMP_TRUE4:
			case INST_JUMP_FALSE4:
			    /* Trick: this transforms to the negation! */
			    INST_AT_PC(tmp) ^= 2;
			    INST_AT_PC(pc) = INST_NOP;
			    if (!UNSHARED(pc)) MODIFIED;
		    }
		}
		break;
	}
    }
}
#undef MODIFIED

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
     * First pass: shrink jumps and push, compute new positions.
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

	resize = 0;
	switch (inst) {
	    case INST_RETURN_CODE_BRANCH:
		/* do not count NOPS in the special jump targets: skip them
		 * altogether */

		for (i = 0; i < 10; i++) {
		    NEW[nextpc] = nextpc - nops;
		    if (PATHS[nextpc] == 0) {
			PATHS[nextpc] = 1;
		    }
		    nextpc++;
		}
		break;

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
		resize = ((arg < 127) && (arg > -128));
		break;
	}

	if (shrinkInst && resize) {
	    /*
	     * INST_XXX1 is always one less than INST_XXX4 
	     */
	    
	    INST_AT_PC(pc) -= 1;
	    SET_INT1_AT_PC(arg, pc+1);
	    INST_AT_PC(pc+2) = INST_NOP;
	    INST_AT_PC(pc+3) = INST_NOP;
	    INST_AT_PC(pc+4) = INST_NOP;
	    nextpc = pc+2; /* get them counted */
	}
    }

    if (nops == 0) {
	return;
    }
    NEW[codeSize] = codeSize - nops;
    
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
	    case INST_START_CMD:
		target = pc + GET_INT4_AT_PC(pc+1);
		offset = NEW[target]-NEW[pc];
		SET_INT4_AT_PC(offset, pc+1);
		if (inst != INST_START_CMD) {
		    if (offset == 5) {
			if (inst == INST_JUMP4) {
			    INST_AT_PC(pc)   = INST_NOP;
			    INST_AT_PC(pc+1) = INST_NOP;
			    nops += 2;
			    goto push3nops;
			} else if (shrinkInst) {
			    INST_AT_PC(pc) -= 1;
			    SET_INT1_AT_PC(2, pc+1);
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
		}
		break;

	    case INST_FOREACH_START:
		i = GET_UINT4_AT_PC(pc+1);
		infoPtr = (ForeachInfo *) TclFetchAuxData(envPtr, i);
		target = pc + 5 - infoPtr->loopCtTemp;
		infoPtr->loopCtTemp = NEW[pc] + 5 - NEW[target];
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
	    PATHS[new]      = PATHS[old];
	}
    }
    envPtr->codeNext = envPtr->codeStart + NEW[codeSize];
    padPtr->codeSize = NEW[codeSize];
    
    /*
     * Update range targets
     */

    for (i=0 ; i<envPtr->exceptArrayNext ; i++) {
	ExceptionRange *rangePtr = &envPtr->exceptArrayPtr[i];
	int target, after;

	target = rangePtr->codeOffset;
	after  = rangePtr->codeOffset + rangePtr->numCodeBytes;
	rangePtr->codeOffset = NEW[target];
	rangePtr->numCodeBytes = NEW[after] - NEW[target];
	
	if (rangePtr->type == CATCH_EXCEPTION_RANGE) {
	    target = rangePtr->catchOffset;
	    rangePtr->catchOffset = NEW[target];
	} else {
	    target = rangePtr->breakOffset;
	    rangePtr->breakOffset = NEW[target];
	    if (rangePtr->continueOffset >= 0) {
		target = rangePtr->continueOffset;
		rangePtr->continueOffset = NEW[target];
	    }
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
 * effectivePC, effectivePCinternal --
 *
 *	Utility functions. Find the effective newpc that will be executed when
 *	we get at pc, by following through jumps and nops. The results are
 *	cached in the NEXT array.
 *
 *      Side effects: may update the PATHS array.
 *
 *      Remark: PATHS needs to be initialized to 0 before the first call (as
 *              this may call MARK/UNMARK which requires that).
 * ----------------------------------------------------------------------
 */

int
effectivePCInternal(
    CompileEnv *envPtr,
    int pc,
    optPad *padPtr,
    int start)
{
    int old, new;
    unsigned char inst;

    /* recurse so that the whole path forward is updated and cached */

    inst = INST_AT_PC(pc);
    switch (inst) {
	case INST_NOP:
	    old = pc + 1;
	    break;
	    
	case INST_JUMP1:
	    old = pc + GET_INT1_AT_PC(pc+1);
	    break;

	case INST_JUMP4:
	    old = pc + GET_INT4_AT_PC(pc+1);
	    break;

	default:
	    return pc;
    }

    if (start == old) {
	/*
	 * INFINITE LOOP! TODO
	 * Eventually insert error generating code, possibly after INST_DONE?
	 */
	return pc;
    }
    
    new  = FOLLOW(old);
    return new;
}

int
UpdateJump(
    CompileEnv *envPtr,
    optPad *padPtr,
    int pc)
{
    int old, new, inst;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSearch;
    Tcl_HashTable *tPtr;
    int result = 0;
    
    inst = INST_AT_PC(pc);

    switch(inst) {
	default:
	    return 0;

	case INST_JUMP_TABLE:
	    tPtr = &JUMPTABLEINFO(envPtr, envPtr->codeStart+pc+1)->hashTable;
	    
	    hPtr = Tcl_FirstHashEntry(tPtr, &hSearch);
	    for (; hPtr ; hPtr = Tcl_NextHashEntry(&hSearch)) {
		old = pc + PTR2INT(Tcl_GetHashValue(hPtr));
		new = FOLLOW(old);
		if (new != old) {
		    //REPLACE(old, new);
		    Tcl_SetHashValue(hPtr, INT2PTR(new - pc));
		    result = 1;
		}
	    }
	    return result;

	case INST_JUMP_TRUE4:
	case INST_JUMP_FALSE4:
	case INST_JUMP4:
	    old = pc + GET_INT4_AT_PC(pc+1);
	    break;
    }

    new = FOLLOW(old);
    if (new == old) {
	return 0;
    }
    SET_INT4_AT_PC(new - pc, pc+1);
    //REPLACE(old, new);
    return 1;
}

void
ThreadJumps(
    CompileEnv *envPtr,
    optPad *padPtr)
{
    INIT_SIZE;
    int pc, nextpc;
    unsigned char inst;
    
    for (pc = 0; pc < codeSize; pc = nextpc) {
	nextpc = NEXT_PC(pc);
	inst = INST_AT_PC(pc);

	switch(inst) {
	    /* TODO:
	     * - condJmp around a non-target jmp -> invert the condition,
	     *   remove the jump!
	     * - advance jump to unshared cond-jmp? error reporting problems,
	     *   same as advancing done (when caught ... can it be?)
	     */
	    
	    case INST_JUMP_TRUE4:
	    case INST_JUMP_FALSE4:
	    case INST_JUMP4:
	    case INST_JUMP_TABLE:
		UPDATE_JUMP(pc);
		break;
	}
    }
}

/*
 * ----------------------------------------------------------------------
 *
 * markPath --
 *
 *    Count the number of paths reaching an instruction, following the jumps
 *    as possible.
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
    int nextpc, i, target;
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
	} else if (PATHS[pc] <=0) {
	    PATHS[pc] = 0;
	    return;
	}
    }
    
    PUSH(pc);
    while (POP(pc) != -1) {
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
		    target = FOLLOW(target);
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
			target = FOLLOW(target);
			Tcl_SetHashValue(hPtr, INT2PTR(target - pc));
		    }
		    PUSH(target);
		}
		PUSH(nextpc);
		break;

	    case INST_RETURN_CODE_BRANCH:
		/*
		 * uncommon jumps: handle all here. Convert possible unreachable
		 * NOP to DONE, marked as reachable so that they are not
		 * eliminated. This is followed by 2-bytes each for codes: error,
		 * return, break, continue, other. After that, the code for ok.
		 */

		for (i = 0; i < 5; i++, nextpc += 2) {
		    PUSH(nextpc);
		    if (InstLength(INST_AT_PC(nextpc)) == 1) {
			PUSH(nextpc + 1);
		    }
		}
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
 * FORCED 1-jumps (as after INST_RETURN_CODE_BRANCH) from exception handling
 * code to normal code.
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

	    case INST_RETURN_CODE_BRANCH:
		/* let's hope there was no 1-jump to OK code! */
		nextpc += 10;
		
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
	if (rangePtr->type == CATCH_EXCEPTION_RANGE) {
	    target = FOLLOW(NEW[rangePtr->catchOffset]);
	    rangePtr->catchOffset = target;
	    MARK(target);
	} else {
	    rangePtr->breakOffset = FOLLOW(NEW[rangePtr->breakOffset]);
	    if (rangePtr->continueOffset >= 0) {
		rangePtr->continueOffset = FOLLOW(NEW[rangePtr->continueOffset]);
	    }
	}
	target = rangePtr->codeOffset + rangePtr->numCodeBytes;
	rangePtr->codeOffset = NEW[rangePtr->codeOffset];
	rangePtr->numCodeBytes = NEW[target]-rangePtr->codeOffset;
    }
}

int
optimizePush_0(
    CompileEnv *envPtr,
    optPad *padPtr,
    int pc)
{
    int inst, nextpc, i, target, tmp;
    Tcl_Obj *objPtr;

    nextpc = NEXT_PC(pc);
    
    tmp = FOLLOW(nextpc);
    inst = INST_AT_PC(tmp);
    objPtr = envPtr->literalArrayPtr[GET_UINT4_AT_PC(pc+1)].objPtr;

    switch (inst) {
	case INST_POP:
	    tmp = FOLLOW(tmp + 1);
	    INST_AT_PC(pc) = INST_JUMP4;
	    SET_INT4_AT_PC(tmp - pc, pc+1);
	    //REPLACE(nextpc, tmp);
	    return 1;
	    
	case INST_JUMP_TRUE1:
	case INST_JUMP_FALSE1:
	case INST_JUMP_TRUE4:
	case INST_JUMP_FALSE4:
	    if (Tcl_GetBooleanFromObj(NULL, objPtr, &i) == TCL_ERROR) {
		return 0;
	    }
	    /* let i indicate if we take the jump or not */
	    if (i) {
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
	    target = FOLLOW(target);
	    //REPLACE(nextpc, target);
	    INST_AT_PC(pc) = INST_JUMP4;
	    SET_INT4_AT_PC(target-pc, pc+1);
	    return 1;
	    
	case INST_LNOT:
	    break;
	    
	    /* FIXME: TODO, eval and jump past it? No room for the
	     * jump. If unshared target, change the literal if it
	     * is a number. */
	    
	case INST_UMINUS:
	case INST_UPLUS:
	case INST_TRY_CVT_TO_NUMERIC:
	case INST_LSHIFT:
	case INST_RSHIFT:
	case INST_BITNOT:
	    break;
    }
    return 0;
}

static inline int
optPush1(
    CompileEnv *envPtr,
    optPad *padPtr,
    int pc)
{
    INIT_PATHS;
    int inst, nextpc, i, tmp;
    Tcl_Obj *objPtr;

    nextpc = NEXT_PC(pc);
    
    redoPush:
    tmp = FOLLOW(nextpc);
    if (!UNSHARED(tmp)) {
	return 0;
    }
    
    inst = INST_AT_PC(tmp);
    objPtr = envPtr->literalArrayPtr[GET_UINT4_AT_PC(pc+1)].objPtr;
    switch (inst) {
	case INST_LNOT:
	    if (Tcl_GetBooleanFromObj(NULL, objPtr, &i) != TCL_ERROR) {
		INST_AT_PC(tmp) = INST_NOP;
		i = TclRegisterNewLiteral(envPtr, i? "0" : "1", 1);
		SET_INT4_AT_PC(i, pc+1);
		goto redoPush;
	    }
	    break;
	    
	    /* FIXME: TODO, eval and jump past it? No room for the
	     * jump. If unshared target, change the literal if it
	     * is a number. */
	    
	case INST_UMINUS:
	case INST_UPLUS:
	case INST_TRY_CVT_TO_NUMERIC:
	case INST_LSHIFT:
	case INST_RSHIFT:
	case INST_BITNOT:
	    break;
    }
    return 0;
}


int
optimizePush_1(
    CompileEnv *envPtr,
    optPad *padPtr,
    int pc)
{
    int res;

    if (INST_AT_PC(pc) != INST_PUSH4) {
	return 0;
    }

    res = optimizePush_0(envPtr, padPtr, pc);
    if (!res) {
	res = optPush1(envPtr, padPtr, pc);
    }
    return res;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
