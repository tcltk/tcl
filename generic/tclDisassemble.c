/*
 * tclDisassemble.c --
 *
 *	This file contains procedures that disassemble bytecode into either
 *	human-readable or Tcl-processable forms.
 *
 * Copyright © 1996-1998 Sun Microsystems, Inc.
 * Copyright © 2001 Kevin B. Kenny. All rights reserved.
 * Copyright © 2013-2016 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"
#include "tclOOInt.h"
#include <assert.h>

/*
 * Prototypes for procedures defined later in this file:
 */

static Tcl_Obj *	DisassembleByteCodeAsDicts(Tcl_Obj *objPtr);
static Tcl_Obj *	DisassembleByteCodeObj(Tcl_Obj *objPtr);
static int		FormatInstruction(ByteCode *codePtr,
			    const unsigned char *pc, Tcl_Obj *bufferObj);
static void		GetLocationInformation(Proc *procPtr,
			    Tcl_Obj **fileObjPtr, int *linePtr);
static void		PrintSourceToObj(Tcl_Obj *appendObj,
			    const char *stringPtr, Tcl_Size maxChars);
static void		UpdateStringOfInstName(Tcl_Obj *objPtr);

/*
 * The structure below defines an instruction name Tcl object to allow
 * reporting of inner contexts in errorstack without string allocation.
 */

static const Tcl_ObjType instNameType = {
    "instname",			/* name */
    NULL,			/* freeIntRepProc */
    NULL,			/* dupIntRepProc */
    UpdateStringOfInstName,	/* updateStringProc */
    NULL,			/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

#define InstNameSetInternalRep(objPtr, inst) \
    do {								\
	Tcl_ObjInternalRep ir;						\
	ir.wideValue = (inst);						\
	Tcl_StoreInternalRep((objPtr), &instNameType, &ir);		\
    } while (0)

#define InstNameGetInternalRep(objPtr, inst) \
    do {								\
	const Tcl_ObjInternalRep *irPtr;				\
	irPtr = TclFetchInternalRep((objPtr), &instNameType);		\
	assert(irPtr != NULL);						\
	(inst) = irPtr->wideValue;					\
    } while (0)

/*
 *----------------------------------------------------------------------
 *
 * GetLocationInformation --
 *
 *	This procedure looks up the information about where a procedure was
 *	originally declared.
 *
 * Results:
 *	Writes to the variables pointed at by fileObjPtr and linePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
GetLocationInformation(
    Proc *procPtr,		/* What to look up the information for. */
    Tcl_Obj **fileObjPtr,	/* Where to write the information about what
				 * file the code came from. Will be written
				 * to, either with the object (assume shared!)
				 * that describes what the file was, or with
				 * NULL if the information is not
				 * available. */
    int *linePtr)		/* Where to write the information about what
				 * line number represented the start of the
				 * code in question. Will be written to,
				 * either with the line number or with -1 if
				 * the information is not available. */
{
    CmdFrame *cfPtr = TclGetCmdFrameForProcedure(procPtr);

    *fileObjPtr = NULL;
    *linePtr = -1;
    if (cfPtr == NULL) {
	return;
    }

    /*
     * Get the source location data out of the CmdFrame.
     */

    *linePtr = cfPtr->line[0];
    if (cfPtr->type == TCL_LOCATION_SOURCE) {
	*fileObjPtr = cfPtr->data.eval.path;
    }
}

#ifdef TCL_COMPILE_DEBUG
/*
 *----------------------------------------------------------------------
 *
 * TclDebugPrintByteCodeObj --
 *
 *	This procedure prints ("disassembles") the instructions of a bytecode
 *	object to stdout.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclDebugPrintByteCodeObj(
    Tcl_Obj *objPtr)		/* The bytecode object to disassemble. */
{
    if (tclTraceCompile >= 2) {
	Tcl_Obj *bufPtr = DisassembleByteCodeObj(objPtr);

	fprintf(stdout, "\n%s", TclGetString(bufPtr));
	Tcl_DecrRefCount(bufPtr);
	fflush(stdout);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrintInstruction --
 *
 *	This procedure prints ("disassembles") one instruction from a bytecode
 *	object to stdout.
 *
 * Results:
 *	Returns the length in bytes of the current instruiction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclPrintInstruction(
    ByteCode *codePtr,		/* Bytecode containing the instruction. */
    const unsigned char *pc)	/* Points to first byte of instruction. */
{
    Tcl_Obj *bufferObj;
    int numBytes;

    TclNewObj(bufferObj);
    numBytes = FormatInstruction(codePtr, pc, bufferObj);
    fprintf(stdout, "%s", TclGetString(bufferObj));
    Tcl_DecrRefCount(bufferObj);
    return numBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrintObject --
 *
 *	This procedure prints up to a specified number of characters from the
 *	argument Tcl object's string representation to a specified file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Outputs characters to the specified file.
 *
 *----------------------------------------------------------------------
 */

void
TclPrintObject(
    FILE *outFile,		/* The file to print the source to. */
    Tcl_Obj *objPtr,		/* Points to the Tcl object whose string
				 * representation should be printed. */
    Tcl_Size maxChars)		/* Maximum number of chars to print. */
{
    char *bytes;
    Tcl_Size length;

    bytes = TclGetStringFromObj(objPtr, &length);
    TclPrintSource(outFile, bytes, TclMin(length, maxChars));
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrintSource --
 *
 *	This procedure prints up to a specified number of characters from the
 *	argument string to a specified file. It tries to produce legible
 *	output by adding backslashes as necessary.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Outputs characters to the specified file.
 *
 *----------------------------------------------------------------------
 */

void
TclPrintSource(
    FILE *outFile,		/* The file to print the source to. */
    const char *stringPtr,	/* The string to print. */
    Tcl_Size maxChars)		/* Maximum number of chars to print. */
{
    Tcl_Obj *bufferObj;

    TclNewObj(bufferObj);
    PrintSourceToObj(bufferObj, stringPtr, maxChars);
    fprintf(outFile, "%s", TclGetString(bufferObj));
    Tcl_DecrRefCount(bufferObj);
}
#endif /* TCL_COMPILE_DEBUG */

/*
 *----------------------------------------------------------------------
 *
 * DisassembleByteCodeObj --
 *
 *	Given an object which is of bytecode type, return a disassembled
 *	version of the bytecode (in a new refcount 0 object). No guarantees
 *	are made about the details of the contents of the result.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
DisassembleByteCodeObj(
    Tcl_Obj *objPtr)		/* The bytecode object to disassemble. */
{
    ByteCode *codePtr;
    unsigned char *codeStart, *codeLimit, *pc;
    unsigned char *codeDeltaNext, *codeLengthNext;
    unsigned char *srcDeltaNext, *srcLengthNext;
    int codeOffset, codeLen, srcOffset, srcLen, numCmds, delta, line;
    Tcl_Size i;
    Interp *iPtr;
    Tcl_Obj *bufferObj, *fileObj;

    ByteCodeGetInternalRep(objPtr, &tclByteCodeType, codePtr);

    iPtr = (Interp *) *codePtr->interpHandle;

    TclNewObj(bufferObj);
    if (!codePtr->refCount) {
	return bufferObj;	/* Already freed. */
    }

    codeStart = codePtr->codeStart;
    codeLimit = codeStart + codePtr->numCodeBytes;
    numCmds = codePtr->numCommands;

    /*
     * Print header lines describing the ByteCode.
     */

    Tcl_AppendPrintfToObj(bufferObj,
	    "ByteCode %p, refCt %" TCL_SIZE_MODIFIER "d, epoch %" TCL_SIZE_MODIFIER "d, interp %p (epoch %" TCL_SIZE_MODIFIER "d)\n",
	    codePtr, codePtr->refCount, codePtr->compileEpoch, iPtr, iPtr->compileEpoch);
    Tcl_AppendToObj(bufferObj, "  Source ", -1);
    PrintSourceToObj(bufferObj, codePtr->source,
	    TclMin(codePtr->numSrcBytes, 55));
    GetLocationInformation(codePtr->procPtr, &fileObj, &line);
    if (line >= 0 && fileObj != NULL) {
	Tcl_AppendPrintfToObj(bufferObj, "\n  File \"%s\" Line %d",
		TclGetString(fileObj), line);
    }
    Tcl_AppendPrintfToObj(bufferObj,
	    "\n  Cmds %d, src %" TCL_SIZE_MODIFIER "d, inst %" TCL_SIZE_MODIFIER "d, litObjs %" TCL_SIZE_MODIFIER "d, aux %" TCL_SIZE_MODIFIER "d, stkDepth %" TCL_SIZE_MODIFIER "d, code/src %.2f\n",
	    numCmds, codePtr->numSrcBytes, codePtr->numCodeBytes,
	    codePtr->numLitObjects, codePtr->numAuxDataItems,
	    codePtr->maxStackDepth,
#ifdef TCL_COMPILE_STATS
	    codePtr->numSrcBytes?
		    codePtr->structureSize/(float)codePtr->numSrcBytes :
#endif
	    0.0);

#ifdef TCL_COMPILE_STATS
    Tcl_AppendPrintfToObj(bufferObj,
	    "  Code %" TCL_Z_MODIFIER "u = header %" TCL_Z_MODIFIER "u+inst %" TCL_SIZE_MODIFIER "d+litObj %"
	    TCL_Z_MODIFIER "u+exc %" TCL_Z_MODIFIER "u+aux %" TCL_Z_MODIFIER "u+cmdMap %" TCL_SIZE_MODIFIER "d\n",
	    codePtr->structureSize,
	    offsetof(ByteCode, localCachePtr),
	    codePtr->numCodeBytes,
	    codePtr->numLitObjects * sizeof(Tcl_Obj *),
	    codePtr->numExceptRanges*sizeof(ExceptionRange),
	    codePtr->numAuxDataItems * sizeof(AuxData),
	    codePtr->numCmdLocBytes);
#endif /* TCL_COMPILE_STATS */

    /*
     * If the ByteCode is the compiled body of a Tcl procedure, print
     * information about that procedure. Note that we don't know the
     * procedure's name since ByteCode's can be shared among procedures.
     */

    if (codePtr->procPtr != NULL) {
	Proc *procPtr = codePtr->procPtr;
	Tcl_Size numCompiledLocals = procPtr->numCompiledLocals;

	Tcl_AppendPrintfToObj(bufferObj,
		"  Proc %p, refCt %" TCL_SIZE_MODIFIER "d, args %" TCL_SIZE_MODIFIER "d, compiled locals %" TCL_SIZE_MODIFIER "d\n",
		procPtr, procPtr->refCount, procPtr->numArgs,
		numCompiledLocals);
	if (numCompiledLocals > 0) {
	    CompiledLocal *localPtr = procPtr->firstLocalPtr;

	    for (i = 0;  i < numCompiledLocals;  i++) {
		Tcl_AppendPrintfToObj(bufferObj,
			"      slot %" TCL_SIZE_MODIFIER "d%s%s%s%s%s%s", i,
			(localPtr->flags & (VAR_ARRAY|VAR_LINK)) ? "" : ", scalar",
			(localPtr->flags & VAR_ARRAY) ? ", array" : "",
			(localPtr->flags & VAR_LINK) ? ", link" : "",
			(localPtr->flags & VAR_ARGUMENT) ? ", arg" : "",
			(localPtr->flags & VAR_TEMPORARY) ? ", temp" : "",
			(localPtr->flags & VAR_RESOLVED) ? ", resolved" : "");
		if (TclIsVarTemporary(localPtr)) {
		    Tcl_AppendToObj(bufferObj, "\n", -1);
		} else {
		    Tcl_AppendPrintfToObj(bufferObj, ", \"%s\"\n",
			    localPtr->name);
		}
		localPtr = localPtr->nextPtr;
	    }
	}
    }

    /*
     * Print the ExceptionRange array.
     */

    if ((int)codePtr->numExceptRanges > 0) {
	Tcl_AppendPrintfToObj(bufferObj, "  Exception ranges %" TCL_SIZE_MODIFIER "d, depth %" TCL_SIZE_MODIFIER "d:\n",
		codePtr->numExceptRanges, codePtr->maxExceptDepth);
	for (i = 0;  i < (int)codePtr->numExceptRanges;  i++) {
	    ExceptionRange *rangePtr = &codePtr->exceptArrayPtr[i];

	    Tcl_AppendPrintfToObj(bufferObj,
		    "      %" TCL_SIZE_MODIFIER "d: level %" TCL_SIZE_MODIFIER "d, %s, pc %" TCL_SIZE_MODIFIER "d-%" TCL_SIZE_MODIFIER "d, ",
		    i, rangePtr->nestingLevel,
		    (rangePtr->type==LOOP_EXCEPTION_RANGE ? "loop" : "catch"),
		    rangePtr->codeOffset,
		    (rangePtr->codeOffset + rangePtr->numCodeBytes - 1));
	    switch (rangePtr->type) {
	    case LOOP_EXCEPTION_RANGE:
		Tcl_AppendPrintfToObj(bufferObj, "continue %" TCL_SIZE_MODIFIER "d, break %" TCL_SIZE_MODIFIER "d\n",
			rangePtr->continueOffset, rangePtr->breakOffset);
		break;
	    case CATCH_EXCEPTION_RANGE:
		Tcl_AppendPrintfToObj(bufferObj, "catch %" TCL_SIZE_MODIFIER "d\n",
			rangePtr->catchOffset);
		break;
	    default:
		Tcl_Panic("DisassembleByteCodeObj: bad ExceptionRange type %d",
			rangePtr->type);
	    }
	}
    }

    /*
     * If there were no commands (e.g., an expression or an empty string was
     * compiled), just print all instructions and return.
     */

    if (numCmds == 0) {
	pc = codeStart;
	while (pc < codeLimit) {
	    Tcl_AppendToObj(bufferObj, "    ", -1);
	    pc += FormatInstruction(codePtr, pc, bufferObj);
	}
	return bufferObj;
    }

    /*
     * Print table showing the code offset, source offset, and source length
     * for each command. These are encoded as a sequence of bytes.
     */

    Tcl_AppendPrintfToObj(bufferObj, "  Commands %d:", numCmds);
    codeDeltaNext = codePtr->codeDeltaStart;
    codeLengthNext = codePtr->codeLengthStart;
    srcDeltaNext = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    for (i = 0;  i < numCmds;  i++) {
	if (*codeDeltaNext == 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if (*codeLengthNext == 0xFF) {
	    codeLengthNext++;
	    codeLen = TclGetInt4AtPtr(codeLengthNext);
	    codeLengthNext += 4;
	} else {
	    codeLen = TclGetInt1AtPtr(codeLengthNext);
	    codeLengthNext++;
	}

	if (*srcDeltaNext == 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if (*srcLengthNext == 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}

	Tcl_AppendPrintfToObj(bufferObj, "%s%4" TCL_SIZE_MODIFIER "d: pc %d-%d, src %d-%d",
		((i % 2)? "     " : "\n   "),
		(i+1), codeOffset, (codeOffset + codeLen - 1),
		srcOffset, (srcOffset + srcLen - 1));
    }
    if (numCmds > 0) {
	Tcl_AppendToObj(bufferObj, "\n", -1);
    }

    /*
     * Print each instruction. If the instruction corresponds to the start of
     * a command, print the command's source. Note that we don't need the code
     * length here.
     */

    codeDeltaNext = codePtr->codeDeltaStart;
    srcDeltaNext = codePtr->srcDeltaStart;
    srcLengthNext = codePtr->srcLengthStart;
    codeOffset = srcOffset = 0;
    pc = codeStart;
    for (i = 0;  i < numCmds;  i++) {
	if (*codeDeltaNext == 0xFF) {
	    codeDeltaNext++;
	    delta = TclGetInt4AtPtr(codeDeltaNext);
	    codeDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(codeDeltaNext);
	    codeDeltaNext++;
	}
	codeOffset += delta;

	if (*srcDeltaNext == 0xFF) {
	    srcDeltaNext++;
	    delta = TclGetInt4AtPtr(srcDeltaNext);
	    srcDeltaNext += 4;
	} else {
	    delta = TclGetInt1AtPtr(srcDeltaNext);
	    srcDeltaNext++;
	}
	srcOffset += delta;

	if (*srcLengthNext == 0xFF) {
	    srcLengthNext++;
	    srcLen = TclGetInt4AtPtr(srcLengthNext);
	    srcLengthNext += 4;
	} else {
	    srcLen = TclGetInt1AtPtr(srcLengthNext);
	    srcLengthNext++;
	}

	/*
	 * Print instructions before command i.
	 */

	while ((pc-codeStart) < codeOffset) {
	    Tcl_AppendToObj(bufferObj, "    ", -1);
	    pc += FormatInstruction(codePtr, pc, bufferObj);
	}

	Tcl_AppendPrintfToObj(bufferObj, "  Command %" TCL_SIZE_MODIFIER "d: ", i+1);
	PrintSourceToObj(bufferObj, (codePtr->source + srcOffset),
		TclMin(srcLen, 55));
	Tcl_AppendToObj(bufferObj, "\n", -1);
    }
    if (pc < codeLimit) {
	/*
	 * Print instructions after the last command.
	 */

	while (pc < codeLimit) {
	    Tcl_AppendToObj(bufferObj, "    ", -1);
	    pc += FormatInstruction(codePtr, pc, bufferObj);
	}
    }
    return bufferObj;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatInstruction --
 *
 *	Appends a representation of a bytecode instruction to a Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static int
FormatInstruction(
    ByteCode *codePtr,		/* Bytecode containing the instruction. */
    const unsigned char *pc,	/* Points to first byte of instruction. */
    Tcl_Obj *bufferObj)		/* Object to append instruction info to. */
{
    Proc *procPtr = codePtr->procPtr;
    unsigned char opCode = *pc;
    const InstructionDesc *instDesc = &tclInstructionTable[opCode];
    unsigned char *codeStart = codePtr->codeStart;
    unsigned pcOffset = pc - codeStart;
    int opnd = 0, i, j, numBytes = 1;
    Tcl_Size localCt = procPtr ? procPtr->numCompiledLocals : 0;
    CompiledLocal *localPtr = procPtr ? procPtr->firstLocalPtr : NULL;
    char suffixBuffer[128];	/* Additional info to print after main opcode
				 * and immediates. */
    char *suffixSrc = NULL;
    Tcl_Obj *suffixObj = NULL;
    AuxData *auxPtr = NULL;

    suffixBuffer[0] = '\0';
    Tcl_AppendPrintfToObj(bufferObj, "(%u) %s ", pcOffset, instDesc->name);
    for (i = 0;  i < instDesc->numOperands;  i++) {
	switch (instDesc->opTypes[i]) {
	case OPERAND_INT1:
	    opnd = TclGetInt1AtPtr(pc+numBytes); numBytes++;
	    Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
	    break;
	case OPERAND_INT4:
	    opnd = TclGetInt4AtPtr(pc+numBytes); numBytes += 4;
	    Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
	    break;
	case OPERAND_UINT1:
	    opnd = TclGetUInt1AtPtr(pc+numBytes); numBytes++;
	    Tcl_AppendPrintfToObj(bufferObj, "%u ", opnd);
	    break;
	case OPERAND_UINT4:
	    opnd = TclGetUInt4AtPtr(pc+numBytes); numBytes += 4;
	    if (opCode == INST_START_CMD) {
		snprintf(suffixBuffer+strlen(suffixBuffer), sizeof(suffixBuffer) - strlen(suffixBuffer),
			", %u cmds start here", opnd);
	    }
	    Tcl_AppendPrintfToObj(bufferObj, "%u ", opnd);
	    break;
	case OPERAND_OFFSET1:
	    opnd = TclGetInt1AtPtr(pc+numBytes); numBytes++;
	    snprintf(suffixBuffer, sizeof(suffixBuffer), "pc %u", pcOffset+opnd);
	    Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
	    break;
	case OPERAND_OFFSET4:
	    opnd = TclGetInt4AtPtr(pc+numBytes); numBytes += 4;
	    if (opCode == INST_START_CMD) {
		snprintf(suffixBuffer, sizeof(suffixBuffer), "next cmd at pc %u", pcOffset+opnd);
	    } else {
		snprintf(suffixBuffer, sizeof(suffixBuffer), "pc %u", pcOffset+opnd);
	    }
	    Tcl_AppendPrintfToObj(bufferObj, "%+d ", opnd);
	    break;
	case OPERAND_LIT1:
	    opnd = TclGetUInt1AtPtr(pc+numBytes); numBytes++;
	    suffixObj = codePtr->objArrayPtr[opnd];
	    Tcl_AppendPrintfToObj(bufferObj, "%u ", opnd);
	    break;
	case OPERAND_LIT4:
	    opnd = TclGetUInt4AtPtr(pc+numBytes); numBytes += 4;
	    suffixObj = codePtr->objArrayPtr[opnd];
	    Tcl_AppendPrintfToObj(bufferObj, "%u ", opnd);
	    break;
	case OPERAND_AUX4:
	    opnd = TclGetUInt4AtPtr(pc+numBytes); numBytes += 4;
	    Tcl_AppendPrintfToObj(bufferObj, "%u ", opnd);
	    auxPtr = &codePtr->auxDataArrayPtr[opnd];
	    break;
	case OPERAND_IDX4:
	    opnd = TclGetInt4AtPtr(pc+numBytes); numBytes += 4;
	    if (opnd >= -1) {
		Tcl_AppendPrintfToObj(bufferObj, "%d ", opnd);
	    } else if (opnd == -2) {
		Tcl_AppendPrintfToObj(bufferObj, "end ");
	    } else {
		Tcl_AppendPrintfToObj(bufferObj, "end-%d ", -2-opnd);
	    }
	    break;
	case OPERAND_LVT1:
	    opnd = TclGetUInt1AtPtr(pc+numBytes);
	    numBytes++;
	    goto printLVTindex;
	case OPERAND_LVT4:
	    opnd = TclGetUInt4AtPtr(pc+numBytes);
	    numBytes += 4;
	printLVTindex:
	    if (localPtr != NULL) {
		if (opnd >= localCt) {
		    Tcl_Panic("FormatInstruction: bad local var index %u (%" TCL_SIZE_MODIFIER "d locals)",
			    opnd, localCt);
		}
		for (j = 0;  j < opnd;  j++) {
		    localPtr = localPtr->nextPtr;
		}
		if (TclIsVarTemporary(localPtr)) {
		    snprintf(suffixBuffer, sizeof(suffixBuffer), "temp var %u", opnd);
		} else {
		    snprintf(suffixBuffer, sizeof(suffixBuffer), "var ");
		    suffixSrc = localPtr->name;
		}
	    }
	    Tcl_AppendPrintfToObj(bufferObj, "%%v%u ", opnd);
	    break;
	case OPERAND_SCLS1:
	    opnd = TclGetUInt1AtPtr(pc+numBytes); numBytes++;
	    Tcl_AppendPrintfToObj(bufferObj, "%s ",
		    tclStringClassTable[opnd].name);
	    break;
	case OPERAND_NONE:
	default:
	    break;
	}
    }
    if (suffixObj) {
	const char *bytes;
	Tcl_Size length;

	Tcl_AppendToObj(bufferObj, "\t# ", -1);
	bytes = TclGetStringFromObj(codePtr->objArrayPtr[opnd], &length);
	PrintSourceToObj(bufferObj, bytes, TclMin(length, 40));
    } else if (suffixBuffer[0]) {
	Tcl_AppendPrintfToObj(bufferObj, "\t# %s", suffixBuffer);
	if (suffixSrc) {
	    PrintSourceToObj(bufferObj, suffixSrc, 40);
	}
    }
    Tcl_AppendToObj(bufferObj, "\n", -1);
    if (auxPtr && auxPtr->type->printProc) {
	Tcl_AppendToObj(bufferObj, "\t\t[", -1);
	auxPtr->type->printProc(auxPtr->clientData, bufferObj, codePtr,
		pcOffset);
	Tcl_AppendToObj(bufferObj, "]\n", -1);
    }
    return numBytes;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetInnerContext --
 *
 *	If possible, returns a list capturing the inner context. Otherwise
 *	return NULL.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclGetInnerContext(
    Tcl_Interp *interp,
    const unsigned char *pc,
    Tcl_Obj **tosPtr)
{
    Tcl_Size objc = 0;
    Tcl_Obj *result;
    Interp *iPtr = (Interp *) interp;

    switch (*pc) {
    case INST_STR_LEN:
    case INST_LNOT:
    case INST_BITNOT:
    case INST_UMINUS:
    case INST_UPLUS:
    case INST_TRY_CVT_TO_NUMERIC:
    case INST_EXPAND_STKTOP:
    case INST_EXPR_STK:
	objc = 1;
	break;

    case INST_LIST_IN:
    case INST_LIST_NOT_IN:	/* Basic list containment operators. */
    case INST_STR_EQ:
    case INST_STR_NEQ:		/* String (in)equality check */
    case INST_STR_CMP:		/* String compare. */
    case INST_STR_INDEX:
    case INST_STR_MATCH:
    case INST_REGEXP:
    case INST_EQ:
    case INST_NEQ:
    case INST_LT:
    case INST_GT:
    case INST_LE:
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
	objc = 2;
	break;

    case INST_RETURN_STK:
	/* early pop. TODO: dig out opt dict too :/ */
	objc = 1;
	break;

    case INST_SYNTAX:
    case INST_RETURN_IMM:
	objc = 2;
	break;

    case INST_INVOKE_STK4:
	objc = TclGetUInt4AtPtr(pc+1);
	break;

    case INST_INVOKE_STK1:
	objc = TclGetUInt1AtPtr(pc+1);
	break;
    }

    result = iPtr->innerContext;
    if (Tcl_IsShared(result)) {
	Tcl_DecrRefCount(result);
	iPtr->innerContext = result = Tcl_NewListObj(objc + 1, NULL);
	Tcl_IncrRefCount(result);
    } else {
	Tcl_Size len;

	/*
	 * Reset while keeping the list internalrep as much as possible.
	 */

	TclListObjLength(interp, result, &len);
	Tcl_ListObjReplace(interp, result, 0, len, 0, NULL);
    }
    Tcl_ListObjAppendElement(NULL, result, TclNewInstNameObj(*pc));

    for (; objc>0 ; objc--) {
	Tcl_Obj *objPtr;

	objPtr = tosPtr[1 - objc];
	if (!objPtr) {
	    Tcl_Panic("InnerContext: bad tos -- appending null object");
	}
	if ((objPtr->refCount <= 0)
#ifdef TCL_MEM_DEBUG
		|| (objPtr->refCount == 0x61616161)
#endif
	) {
	    Tcl_Panic("InnerContext: bad tos -- appending freed object %p",
		    objPtr);
	}
	Tcl_ListObjAppendElement(NULL, result, objPtr);
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNewInstNameObj --
 *
 *	Creates a new InstName Tcl_Obj based on the given instruction
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclNewInstNameObj(
    unsigned char inst)
{
    Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    TclInvalidateStringRep(objPtr);
    InstNameSetInternalRep(objPtr, inst);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfInstName --
 *
 *	Update the string representation for an instruction name object.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfInstName(
    Tcl_Obj *objPtr)
{
    size_t inst;	/* NOTE: We know this is really an unsigned char */
    char *dst;

    InstNameGetInternalRep(objPtr, inst);

    if (inst >= LAST_INST_OPCODE) {
	dst = Tcl_InitStringRep(objPtr, NULL, TCL_INTEGER_SPACE + 5);
	TclOOM(dst, TCL_INTEGER_SPACE + 5);
	snprintf(dst, TCL_INTEGER_SPACE + 5, "inst_%" TCL_Z_MODIFIER "u", inst);
	(void) Tcl_InitStringRep(objPtr, NULL, strlen(dst));
    } else {
	const char *s = tclInstructionTable[inst].name;
	size_t len = strlen(s);
	dst = Tcl_InitStringRep(objPtr, s, len);
	TclOOM(dst, len);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PrintSourceToObj --
 *
 *	Appends a quoted representation of a string to a Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static void
PrintSourceToObj(
    Tcl_Obj *appendObj,		/* The object to print the source to. */
    const char *stringPtr,	/* The string to print. */
    Tcl_Size maxChars)		/* Maximum number of chars to print. */
{
    const char *p;
    Tcl_Size i = 0, len;

    if (stringPtr == NULL) {
	Tcl_AppendToObj(appendObj, "\"\"", -1);
	return;
    }

    Tcl_AppendToObj(appendObj, "\"", -1);
    p = stringPtr;
    for (;  (*p != '\0') && (i < maxChars);  p+=len) {
	int ucs4;

	len = TclUtfToUniChar(p, &ucs4);
	switch (ucs4) {
	case '"':
	    Tcl_AppendToObj(appendObj, "\\\"", -1);
	    i += 2;
	    continue;
	case '\f':
	    Tcl_AppendToObj(appendObj, "\\f", -1);
	    i += 2;
	    continue;
	case '\n':
	    Tcl_AppendToObj(appendObj, "\\n", -1);
	    i += 2;
	    continue;
	case '\r':
	    Tcl_AppendToObj(appendObj, "\\r", -1);
	    i += 2;
	    continue;
	case '\t':
	    Tcl_AppendToObj(appendObj, "\\t", -1);
	    i += 2;
	    continue;
	case '\v':
	    Tcl_AppendToObj(appendObj, "\\v", -1);
	    i += 2;
	    continue;
	default:
	    if (ucs4 > 0xFFFF) {
		Tcl_AppendPrintfToObj(appendObj, "\\U%08x", ucs4);
		i += 10;
	    } else if (ucs4 < 0x20 || ucs4 >= 0x7F) {
		Tcl_AppendPrintfToObj(appendObj, "\\u%04x", ucs4);
		i += 6;
	    } else {
		Tcl_AppendPrintfToObj(appendObj, "%c", ucs4);
		i++;
	    }
	    continue;
	}
    }
    if (*p != '\0') {
	Tcl_AppendToObj(appendObj, "...", -1);
    }
    Tcl_AppendToObj(appendObj, "\"", -1);
}

/*
 *----------------------------------------------------------------------
 *
 * DisassembleByteCodeAsDicts --
 *
 *	Given an object which is of bytecode type, return a disassembled
 *	version of the bytecode (in a new refcount 0 object) in a dictionary.
 *	No guarantees are made about the details of the contents of the
 *	result, but it is intended to be more readable than the old output
 *	format.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
DisassembleByteCodeAsDicts(
    Tcl_Obj *objPtr)		/* The bytecode-holding value to take apart */
{
    ByteCode *codePtr;
    Tcl_Obj *description, *literals, *variables, *instructions, *inst;
    Tcl_Obj *aux, *exn, *commands, *file;
    unsigned char *pc, *opnd, *codeOffPtr, *codeLenPtr, *srcOffPtr, *srcLenPtr;
    int codeOffset, codeLength, sourceOffset, sourceLength, val, line;
    Tcl_Size i;

    ByteCodeGetInternalRep(objPtr, &tclByteCodeType, codePtr);

    /*
     * Get the literals from the bytecode.
     */

    TclNewObj(literals);
    for (i=0 ; i<codePtr->numLitObjects ; i++) {
	Tcl_ListObjAppendElement(NULL, literals, codePtr->objArrayPtr[i]);
    }

    /*
     * Get the variables from the bytecode.
     */

    TclNewObj(variables);
    if (codePtr->procPtr) {
	Tcl_Size localCount = codePtr->procPtr->numCompiledLocals;
	CompiledLocal *localPtr = codePtr->procPtr->firstLocalPtr;

	for (i=0 ; i<localCount ; i++,localPtr=localPtr->nextPtr) {
	    Tcl_Obj *descriptor[2];

	    TclNewObj(descriptor[0]);
	    if (!(localPtr->flags & (VAR_ARRAY|VAR_LINK))) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("scalar", -1));
	    }
	    if (localPtr->flags & VAR_ARRAY) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("array", -1));
	    }
	    if (localPtr->flags & VAR_LINK) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("link", -1));
	    }
	    if (localPtr->flags & VAR_ARGUMENT) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("arg", -1));
	    }
	    if (localPtr->flags & VAR_TEMPORARY) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("temp", -1));
	    }
	    if (localPtr->flags & VAR_RESOLVED) {
		Tcl_ListObjAppendElement(NULL, descriptor[0],
			Tcl_NewStringObj("resolved", -1));
	    }
	    if (localPtr->flags & VAR_TEMPORARY) {
		Tcl_ListObjAppendElement(NULL, variables,
			Tcl_NewListObj(1, descriptor));
	    } else {
		descriptor[1] = Tcl_NewStringObj(localPtr->name, -1);
		Tcl_ListObjAppendElement(NULL, variables,
			Tcl_NewListObj(2, descriptor));
	    }
	}
    }

    /*
     * Get the instructions from the bytecode.
     */

    TclNewObj(instructions);
    for (pc=codePtr->codeStart; pc<codePtr->codeStart+codePtr->numCodeBytes;){
	const InstructionDesc *instDesc = &tclInstructionTable[*pc];
	int address = pc - codePtr->codeStart;

	TclNewObj(inst);
	Tcl_ListObjAppendElement(NULL, inst, Tcl_NewStringObj(
		instDesc->name, -1));
	opnd = pc + 1;
	for (i=0 ; i<instDesc->numOperands ; i++) {
	    switch (instDesc->opTypes[i]) {
	    case OPERAND_INT1:
		val = TclGetInt1AtPtr(opnd);
		opnd += 1;
		goto formatNumber;
	    case OPERAND_UINT1:
		val = TclGetUInt1AtPtr(opnd);
		opnd += 1;
		goto formatNumber;
	    case OPERAND_INT4:
		val = TclGetInt4AtPtr(opnd);
		opnd += 4;
		goto formatNumber;
	    case OPERAND_UINT4:
		val = TclGetUInt4AtPtr(opnd);
		opnd += 4;
	    formatNumber:
		Tcl_ListObjAppendElement(NULL, inst, Tcl_NewWideIntObj(val));
		break;

	    case OPERAND_OFFSET1:
		val = TclGetInt1AtPtr(opnd);
		opnd += 1;
		goto formatAddress;
	    case OPERAND_OFFSET4:
		val = TclGetInt4AtPtr(opnd);
		opnd += 4;
	    formatAddress:
		Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			"pc %d", address + val));
		break;

	    case OPERAND_LIT1:
		val = TclGetUInt1AtPtr(opnd);
		opnd += 1;
		goto formatLiteral;
	    case OPERAND_LIT4:
		val = TclGetUInt4AtPtr(opnd);
		opnd += 4;
	    formatLiteral:
		Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			"@%d", val));
		break;

	    case OPERAND_LVT1:
		val = TclGetUInt1AtPtr(opnd);
		opnd += 1;
		goto formatVariable;
	    case OPERAND_LVT4:
		val = TclGetUInt4AtPtr(opnd);
		opnd += 4;
	    formatVariable:
		Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			"%%%d", val));
		break;
	    case OPERAND_IDX4:
		val = TclGetInt4AtPtr(opnd);
		opnd += 4;
		if (val >= -1) {
		    Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			    ".%d", val));
		} else if (val == -2) {
		    Tcl_ListObjAppendElement(NULL, inst, Tcl_NewStringObj(
			    ".end", -1));
		} else {
		    Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			    ".end-%d", -2-val));
		}
		break;
	    case OPERAND_AUX4:
		val = TclGetInt4AtPtr(opnd);
		opnd += 4;
		Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			"?%d", val));
		break;
	    case OPERAND_SCLS1:
		val = TclGetUInt1AtPtr(opnd);
		opnd++;
		Tcl_ListObjAppendElement(NULL, inst, Tcl_ObjPrintf(
			"=%s", tclStringClassTable[val].name));
		break;
	    case OPERAND_NONE:
		Tcl_Panic("opcode %d with more than zero 'no' operands", *pc);
	    }
	}
	Tcl_DictObjPut(NULL, instructions, Tcl_NewWideIntObj(address), inst);
	pc += instDesc->numBytes;
    }

    /*
     * Get the auxiliary data from the bytecode.
     */

    TclNewObj(aux);
    for (i=0 ; i<(int)codePtr->numAuxDataItems ; i++) {
	AuxData *auxData = &codePtr->auxDataArrayPtr[i];
	Tcl_Obj *auxDesc = Tcl_NewStringObj(auxData->type->name, -1);

	if (auxData->type->disassembleProc) {
	    Tcl_Obj *desc;

	    TclNewObj(desc);
	    TclDictPut(NULL, desc, "name", auxDesc);
	    auxDesc = desc;
	    auxData->type->disassembleProc(auxData->clientData, auxDesc,
		    codePtr, 0);
	} else if (auxData->type->printProc) {
	    Tcl_Obj *desc;

	    TclNewObj(desc);
	    auxData->type->printProc(auxData->clientData, desc, codePtr, 0);
	    Tcl_ListObjAppendElement(NULL, auxDesc, desc);
	}
	Tcl_ListObjAppendElement(NULL, aux, auxDesc);
    }

    /*
     * Get the exception ranges from the bytecode.
     */

    TclNewObj(exn);
    for (i=0 ; i<(int)codePtr->numExceptRanges ; i++) {
	ExceptionRange *rangePtr = &codePtr->exceptArrayPtr[i];

	switch (rangePtr->type) {
	case LOOP_EXCEPTION_RANGE:
	    Tcl_ListObjAppendElement(NULL, exn, Tcl_ObjPrintf(
		    "type %s level %" TCL_SIZE_MODIFIER "d from %" TCL_SIZE_MODIFIER "d to %" TCL_SIZE_MODIFIER "d break %" TCL_SIZE_MODIFIER "d continue %" TCL_SIZE_MODIFIER "d",
		    "loop", rangePtr->nestingLevel, rangePtr->codeOffset,
		    rangePtr->codeOffset + rangePtr->numCodeBytes - 1,
		    rangePtr->breakOffset, rangePtr->continueOffset));
	    break;
	case CATCH_EXCEPTION_RANGE:
	    Tcl_ListObjAppendElement(NULL, exn, Tcl_ObjPrintf(
		    "type %s level %" TCL_SIZE_MODIFIER "d from %" TCL_SIZE_MODIFIER "d to %" TCL_SIZE_MODIFIER "d catch %" TCL_SIZE_MODIFIER "d",
		    "catch", rangePtr->nestingLevel, rangePtr->codeOffset,
		    rangePtr->codeOffset + rangePtr->numCodeBytes - 1,
		    rangePtr->catchOffset));
	    break;
	}
    }

    /*
     * Get the command information from the bytecode.
     *
     * The way these are encoded in the bytecode is non-trivial; the Decode
     * macro (which updates its argument and returns the next decoded value)
     * handles this so that the rest of the code does not.
     */

#define Decode(ptr) \
    ((TclGetUInt1AtPtr(ptr) == 0xFF)			\
	? ((ptr)+=5 , TclGetInt4AtPtr((ptr)-4))		\
	: ((ptr)+=1 , TclGetInt1AtPtr((ptr)-1)))

    TclNewObj(commands);
    codeOffPtr = codePtr->codeDeltaStart;
    codeLenPtr = codePtr->codeLengthStart;
    srcOffPtr = codePtr->srcDeltaStart;
    srcLenPtr = codePtr->srcLengthStart;
    codeOffset = sourceOffset = 0;
    for (i=0 ; i<(int)codePtr->numCommands ; i++) {
	Tcl_Obj *cmd;

	codeOffset += Decode(codeOffPtr);
	codeLength = Decode(codeLenPtr);
	sourceOffset += Decode(srcOffPtr);
	sourceLength = Decode(srcLenPtr);
	TclNewObj(cmd);
	TclDictPut(NULL, cmd, "codefrom", Tcl_NewWideIntObj(codeOffset));
	TclDictPut(NULL, cmd, "codeto", Tcl_NewWideIntObj(
		codeOffset + codeLength - 1));

	/*
	 * Convert byte offsets to character offsets; important if multibyte
	 * characters are present in the source!
	 */

	TclDictPut(NULL, cmd, "scriptfrom", Tcl_NewWideIntObj(
		Tcl_NumUtfChars(codePtr->source, sourceOffset)));
	TclDictPut(NULL, cmd, "scriptto", Tcl_NewWideIntObj(
		Tcl_NumUtfChars(codePtr->source, sourceOffset + sourceLength - 1)));
	TclDictPut(NULL, cmd, "script",
		Tcl_NewStringObj(codePtr->source+sourceOffset, sourceLength));
	Tcl_ListObjAppendElement(NULL, commands, cmd);
    }

#undef Decode

    /*
     * Get the source file and line number information from the CmdFrame
     * system if it is available.
     */

    GetLocationInformation(codePtr->procPtr, &file, &line);

    /*
     * Build the overall result.
     */

    TclNewObj(description);
    TclDictPut(NULL, description, "literals", literals);
    TclDictPut(NULL, description, "variables", variables);
    TclDictPut(NULL, description, "exception", exn);
    TclDictPut(NULL, description, "instructions", instructions);
    TclDictPut(NULL, description, "auxiliary", aux);
    TclDictPut(NULL, description, "commands", commands);
    TclDictPut(NULL, description, "script",
	    Tcl_NewStringObj(codePtr->source, codePtr->numSrcBytes));
    TclDictPut(NULL, description, "namespace",
	    TclNewNamespaceObj((Tcl_Namespace *) codePtr->nsPtr));
    TclDictPut(NULL, description, "stackdepth",
	    Tcl_NewWideIntObj(codePtr->maxStackDepth));
    TclDictPut(NULL, description, "exceptdepth",
	    Tcl_NewWideIntObj(codePtr->maxExceptDepth));
    if (line >= 0) {
	TclDictPut(NULL, description, "initiallinenumber",
		Tcl_NewWideIntObj(line));
    }
    if (file) {
	TclDictPut(NULL, description, "sourcefile", file);
    }
    return description;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DisassembleObjCmd --
 *
 *	Implementation of the "::tcl::unsupported::disassemble" command. This
 *	command is not documented, but will disassemble procedures, lambda
 *	terms and general scripts. Note that will compile terms if necessary
 *	in order to disassemble them.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DisassembleObjCmd(
    void *clientData,		/* What type of operation. */
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *const types[] = {
	"constructor", "destructor",
	"lambda", "method", "objmethod", "proc", "script", NULL
    };
    enum Types {
	DISAS_CLASS_CONSTRUCTOR, DISAS_CLASS_DESTRUCTOR,
	DISAS_LAMBDA, DISAS_CLASS_METHOD, DISAS_OBJECT_METHOD, DISAS_PROC,
	DISAS_SCRIPT
    } idx;
    int result;
    Tcl_Obj *codeObjPtr = NULL;
    Proc *procPtr = NULL;
    Tcl_HashEntry *hPtr;
    Object *oPtr;
    ByteCode *codePtr;
    Method *methodPtr;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "type ...");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], types, "type", 0, &idx)!=TCL_OK){
	return TCL_ERROR;
    }

    switch (idx) {
    case DISAS_LAMBDA: {
	Command cmd;
	Tcl_Obj *nsObjPtr;
	Tcl_Namespace *nsPtr;

	/*
	 * Compile (if uncompiled) and disassemble a lambda term.
	 */

	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "lambdaTerm");
	    return TCL_ERROR;
	}

	procPtr = TclGetLambdaFromObj(interp, objv[2], &nsObjPtr);
	if (procPtr == NULL) {
	    return TCL_ERROR;
	}

	memset(&cmd, 0, sizeof(Command));
	result = TclGetNamespaceFromObj(interp, nsObjPtr, &nsPtr);
	if (result != TCL_OK) {
	    return result;
	}
	cmd.nsPtr = (Namespace *) nsPtr;
	procPtr->cmdPtr = &cmd;
	result = TclPushProcCallFrame(procPtr, interp, objc, objv, 1);
	if (result != TCL_OK) {
	    return result;
	}
	TclPopStackFrame(interp);
	codeObjPtr = procPtr->bodyPtr;
	break;
    }
    case DISAS_PROC:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "procName");
	    return TCL_ERROR;
	}

	procPtr = TclFindProc((Interp *) interp, TclGetString(objv[2]));
	if (procPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" isn't a procedure", TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "PROC",
		    TclGetString(objv[2]), (char *)NULL);
	    return TCL_ERROR;
	}

	/*
	 * Compile (if uncompiled) and disassemble a procedure.
	 */

	result = TclPushProcCallFrame(procPtr, interp, 2, objv+1, 1);
	if (result != TCL_OK) {
	    return result;
	}
	TclPopStackFrame(interp);
	codeObjPtr = procPtr->bodyPtr;
	break;
    case DISAS_SCRIPT:
	/*
	 * Compile and disassemble a script.
	 */

	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "script");
	    return TCL_ERROR;
	}

	if (!TclHasInternalRep(objv[2], &tclByteCodeType) && (TCL_OK
		!= TclSetByteCodeFromAny(interp, objv[2], NULL, NULL))) {
	    return TCL_ERROR;
	}
	codeObjPtr = objv[2];
	break;

    case DISAS_CLASS_CONSTRUCTOR:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "className");
	    return TCL_ERROR;
	}

	/*
	 * Look up the body of a constructor.
	 */

	oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (oPtr == NULL) {
	    return TCL_ERROR;
	}
	if (oPtr->classPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" is not a class", TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		    TclGetString(objv[2]), (char *)NULL);
	    return TCL_ERROR;
	}

	methodPtr = oPtr->classPtr->constructorPtr;
	if (methodPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" has no defined constructor",
		    TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		    "CONSRUCTOR", (char *)NULL);
	    return TCL_ERROR;
	}
	procPtr = TclOOGetProcFromMethod(methodPtr);
	if (procPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "body not available for this kind of constructor", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		    "METHODTYPE", (char *)NULL);
	    return TCL_ERROR;
	}

	/*
	 * Compile if necessary.
	 */

	if (!TclHasInternalRep(procPtr->bodyPtr, &tclByteCodeType)) {
	    Command cmd;

	    /*
	     * Yes, this is ugly, but we need to pass the namespace in to the
	     * compiler in two places.
	     */

	    cmd.nsPtr = (Namespace *) oPtr->namespacePtr;
	    procPtr->cmdPtr = &cmd;
	    result = TclProcCompileProc(interp, procPtr, procPtr->bodyPtr,
		    (Namespace *) oPtr->namespacePtr, "body of constructor",
		    TclGetString(objv[2]));
	    procPtr->cmdPtr = NULL;
	    if (result != TCL_OK) {
		return result;
	    }
	}
	codeObjPtr = procPtr->bodyPtr;
	break;

    case DISAS_CLASS_DESTRUCTOR:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "className");
	    return TCL_ERROR;
	}

	/*
	 * Look up the body of a destructor.
	 */

	oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (oPtr == NULL) {
	    return TCL_ERROR;
	}
	if (oPtr->classPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" is not a class", TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		    TclGetString(objv[2]), (char *)NULL);
	    return TCL_ERROR;
	}

	methodPtr = oPtr->classPtr->destructorPtr;
	if (methodPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" has no defined destructor",
		    TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		    "DESRUCTOR", (char *)NULL);
	    return TCL_ERROR;
	}
	procPtr = TclOOGetProcFromMethod(methodPtr);
	if (procPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "body not available for this kind of destructor", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		    "METHODTYPE", (char *)NULL);
	    return TCL_ERROR;
	}

	/*
	 * Compile if necessary.
	 */

	if (!TclHasInternalRep(procPtr->bodyPtr, &tclByteCodeType)) {
	    Command cmd;

	    /*
	     * Yes, this is ugly, but we need to pass the namespace in to the
	     * compiler in two places.
	     */

	    cmd.nsPtr = (Namespace *) oPtr->namespacePtr;
	    procPtr->cmdPtr = &cmd;
	    result = TclProcCompileProc(interp, procPtr, procPtr->bodyPtr,
		    (Namespace *) oPtr->namespacePtr, "body of destructor",
		    TclGetString(objv[2]));
	    procPtr->cmdPtr = NULL;
	    if (result != TCL_OK) {
		return result;
	    }
	}
	codeObjPtr = procPtr->bodyPtr;
	break;

    case DISAS_CLASS_METHOD:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "className methodName");
	    return TCL_ERROR;
	}

	/*
	 * Look up the body of a class method.
	 */

	oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (oPtr == NULL) {
	    return TCL_ERROR;
	}
	if (oPtr->classPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "\"%s\" is not a class", TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "CLASS",
		    TclGetString(objv[2]), (char *)NULL);
	    return TCL_ERROR;
	}
	hPtr = Tcl_FindHashEntry(&oPtr->classPtr->classMethods,
		objv[3]);
	goto methodBody;
    case DISAS_OBJECT_METHOD:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "objectName methodName");
	    return TCL_ERROR;
	}

	/*
	 * Look up the body of an instance method.
	 */

	oPtr = (Object *) Tcl_GetObjectFromObj(interp, objv[2]);
	if (oPtr == NULL) {
	    return TCL_ERROR;
	}
	if (oPtr->methodsPtr == NULL) {
	    goto unknownMethod;
	}
	hPtr = Tcl_FindHashEntry(oPtr->methodsPtr, objv[3]);

	/*
	 * Compile (if necessary) and disassemble a method body.
	 */

    methodBody:
	if (hPtr == NULL) {
	unknownMethod:
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "unknown method \"%s\"", TclGetString(objv[3])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "METHOD",
		    TclGetString(objv[3]), (char *)NULL);
	    return TCL_ERROR;
	}
	procPtr = TclOOGetProcFromMethod((Method *)Tcl_GetHashValue(hPtr));
	if (procPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "body not available for this kind of method", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		    "METHODTYPE", (char *)NULL);
	    return TCL_ERROR;
	}
	if (!TclHasInternalRep(procPtr->bodyPtr, &tclByteCodeType)) {
	    Command cmd;

	    /*
	     * Yes, this is ugly, but we need to pass the namespace in to the
	     * compiler in two places.
	     */

	    cmd.nsPtr = (Namespace *) oPtr->namespacePtr;
	    procPtr->cmdPtr = &cmd;
	    result = TclProcCompileProc(interp, procPtr, procPtr->bodyPtr,
		    (Namespace *) oPtr->namespacePtr, "body of method",
		    TclGetString(objv[3]));
	    procPtr->cmdPtr = NULL;
	    if (result != TCL_OK) {
		return result;
	    }
	}
	codeObjPtr = procPtr->bodyPtr;
	break;
    default:
	CLANG_ASSERT(0);
    }

    /*
     * Do the actual disassembly.
     */

    ByteCodeGetInternalRep(codeObjPtr, &tclByteCodeType, codePtr);

    if (codePtr->flags & TCL_BYTECODE_PRECOMPILED) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"may not disassemble prebuilt bytecode", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "DISASSEMBLE",
		"BYTECODE", (char *)NULL);
	return TCL_ERROR;
    }
    if (clientData) {
	Tcl_SetObjResult(interp,
		DisassembleByteCodeAsDicts(codeObjPtr));
    } else {
	Tcl_SetObjResult(interp,
		DisassembleByteCodeObj(codeObjPtr));
    }
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
