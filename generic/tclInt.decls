# tclInt.decls --
#
#	This file contains the declarations for all unsupported
#	functions that are exported by the Tcl library.  This file
#	is used to generate the tclIntDecls.h, tclIntPlatDecls.h,
#	tclIntStub.c, and tclPlatStub.c files.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: tclInt.decls,v 1.1 1999/03/03 00:38:40 stanton Exp $

library tcl

# Define the unsupported generic interfaces.

interface tclInt

# Declare each of the functions in the unsupported internal Tcl
# interface.  These interfaces are allowed to changed between versions.
# Use at your own risk.  Note that the position of functions should not
# be changed between versions to avoid gratuitous incompatibilities.

declare 0 generic {
    int TclAccess(CONST char *path, int mode)
}
declare 1 generic {
    int TclAccessDeleteProc(TclAccessProc_ *proc)
}
declare 2 generic {
    int TclAccessInsertProc(TclAccessProc_ *proc)
}
declare 3 generic {
    void TclAllocateFreeObjects(void)
}
declare 4 generic {
    int TclChdir(Tcl_Interp *interp, char *dirName)
}
declare 5 generic {
    int TclCleanupChildren(Tcl_Interp *interp, int numPids, Tcl_Pid *pidPtr, \
	    Tcl_Channel errorChan)
}
declare 6 generic {
    void TclCleanupCommand(Command *cmdPtr)
}
declare 7 generic {
    int TclCopyAndCollapse(int count, char *src, char *dst)
}
declare 8 generic {
    int TclCopyChannel(Tcl_Interp *interp, Tcl_Channel inChan, \
	    Tcl_Channel outChan, int toRead, Tcl_Obj *cmdPtr)
}

# TclCreatePipeline unofficially exported for use by BLT.

declare 9 generic {
    int TclCreatePipeline(Tcl_Interp *interp, int argc, char **argv, \
	    Tcl_Pid **pidArrayPtr, TclFile *inPipePtr, TclFile *outPipePtr, \
	    TclFile *errFilePtr)
}
declare 10 generic {
    int TclCreateProc(Tcl_Interp *interp, Namespace *nsPtr, char *procName, \
	    Tcl_Obj *argsPtr, Tcl_Obj *bodyPtr, Proc **procPtrPtr)
}
declare 11 generic {
    void TclDeleteCompiledLocalVars(Interp *iPtr, CallFrame *framePtr)
}
declare 12 generic {
    void TclDeleteVars(Interp *iPtr, Tcl_HashTable *tablePtr)
}
declare 13 generic {
    int TclDoGlob(Tcl_Interp *interp, char *separators, \
	    Tcl_DString *headPtr, char *tail)
}
declare 14 generic {
    void TclDumpMemoryInfo(FILE *outFile)
}
declare 15 generic {
    void TclExpandParseValue(ParseValue *pvPtr, int needed)
}
declare 16 generic {
    void TclExprFloatError(Tcl_Interp *interp, double value)
}
declare 17 generic {
    int TclFileAttrsCmd(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
}
declare 18 generic {
    int TclFileCopyCmd(Tcl_Interp *interp, int argc, char **argv)
}
declare 19 generic {
    int TclFileDeleteCmd(Tcl_Interp *interp, int argc, char **argv)
}
declare 20 generic {
    int TclFileMakeDirsCmd(Tcl_Interp *interp, int argc, char **argv)
}
declare 21 generic {
    int TclFileRenameCmd(Tcl_Interp *interp, int argc, char **argv)
}
declare 22 generic {
    void TclFinalizeCompExecEnv(void)
}
declare 23 generic {
    void TclFinalizeEnvironment(void)
}
declare 24 generic {
    void TclFinalizeExecEnv(void)
}
declare 25 generic {
    int TclFindElement(Tcl_Interp *interp, char *list, int listLength, \
	    char **elementPtr, char **nextPtr, int *sizePtr, int *bracePtr)
}
declare 26 generic {
    Proc * TclFindProc(Interp *iPtr, char *procName)
}
declare 27 generic {
    int TclFormatInt(char *buffer, long n)
}
declare 28 generic {
    void TclFreePackageInfo(Interp *iPtr)
}
declare 29 generic {
    void TclGetAndDetachPids(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 30 generic {
    char * TclGetCwd(Tcl_Interp *interp)
}
declare 31 generic {
    int TclGetDate(char *p, unsigned long now, long zone, \
	    unsigned long *timePtr)
}
declare 32 generic {
    Tcl_Channel TclGetDefaultStdChannel(int type)
}
declare 33 generic {
    Tcl_Obj * TclGetElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, int leaveErrorMsg)
}
declare 34 generic {
    char * TclGetEnv(CONST char *name)
}
declare 35 generic {
    char * TclGetExtension(char *name)
}
declare 36 generic {
    int TclGetFrame(Tcl_Interp *interp, char *string, CallFrame **framePtrPtr)
}
declare 37 generic {
    TclCmdProcType TclGetInterpProc(void)
}
declare 38 generic {
    int TclGetIntForIndex(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    int endValue, int *indexPtr)
}
declare 39 generic {
    Tcl_Obj * TclGetIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    int leaveErrorMsg)
}
declare 40 generic {
    int TclGetLong(Tcl_Interp *interp, char *string, long *longPtr)
}
declare 41 generic {
    int TclGetLoadedPackages(Tcl_Interp *interp, char *targetName)
}
declare 42 generic {
    int TclGetNamespaceForQualName(Tcl_Interp *interp, char *qualName, \
	    Namespace *cxtNsPtr, int flags, Namespace **nsPtrPtr, \
	    Namespace **altNsPtrPtr, Namespace **actualCxtPtrPtr, \
	    char **simpleNamePtr)
}
declare 43 generic {
    TclObjCmdProcType TclGetObjInterpProc(void)
}
declare 44 generic {
    int TclGetOpenMode(Tcl_Interp *interp, char *string, int *seekFlagPtr)
}
declare 45 generic {
    Tcl_Command TclGetOriginalCommand(Tcl_Command command)
}
declare 46 generic {
    char * TclpGetUserHome(char *name, Tcl_DString *bufferPtr)
}
declare 47 generic {
    int TclGlobalInvoke(Tcl_Interp *interp, int argc, char **argv, int flags)
}
declare 48 generic {
    int TclGuessPackageName(char *fileName, Tcl_DString *bufPtr)
}
declare 49 generic {
    int TclHasSockets(Tcl_Interp *interp)
}
declare 50 generic {
    int TclHideUnsafeCommands(Tcl_Interp *interp)
}
declare 51 generic {
    int TclInExit(void)
}
declare 52 generic {
    Tcl_Obj * TclIncrElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, long incrAmount)
}
declare 53 generic {
    Tcl_Obj * TclIncrIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    long incrAmount)
}
declare 54 generic {
    Tcl_Obj * TclIncrVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr, \
	    Tcl_Obj *part2Ptr, long incrAmount, int part1NotParsed)
}
declare 55 generic {
    void TclInitCompiledLocals(Tcl_Interp *interp, CallFrame *framePtr, \
	    Namespace *nsPtr)
}
declare 56 generic {
    void TclInitNamespaces(void)
}
declare 57 generic {
    int TclInterpInit(Tcl_Interp *interp)
}
declare 58 generic {
    int TclInvoke(Tcl_Interp *interp, int argc, char **argv, int flags)
}
declare 59 generic {
    int TclInvokeObjectCommand(ClientData clientData, Tcl_Interp *interp, \
	    int argc, char **argv)
}
declare 60 generic {
    int TclInvokeStringCommand(ClientData clientData, Tcl_Interp *interp, \
	    int objc, Tcl_Obj *CONST objv[])
}
declare 61 generic {
    Proc * TclIsProc(Command *cmdPtr)
}
declare 62 generic {
    int TclLoadFile(Tcl_Interp *interp, char *fileName, char *sym1, \
	    char *sym2, Tcl_PackageInitProc **proc1Ptr, \
	    Tcl_PackageInitProc **proc2Ptr)
}
declare 63 generic {
    int TclLooksLikeInt(char *p)
}
declare 64 generic {
    Var * TclLookupVar(Tcl_Interp *interp, char *part1, char *part2, \
	    int flags, char *msg, int createPart1, int createPart2, \
	    Var **arrayPtrPtr)
}
declare 65 generic {
    int TclMatchFiles(Tcl_Interp *interp, char *separators, \
	    Tcl_DString *dirPtr, char *pattern, char *tail)
}
declare 66 generic {
    int TclNeedSpace(char *start, char *end)
}
declare 67 generic {
    Tcl_Obj * TclNewProcBodyObj(Proc *procPtr)
}
declare 68 generic {
    int TclObjCommandComplete(Tcl_Obj *cmdPtr)
}
declare 69 generic {
    int TclObjInterpProc(ClientData clientData, Tcl_Interp *interp, \
	    int objc, Tcl_Obj *CONST objv[])
}
declare 70 generic {
    int TclObjInvoke(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], \
	    int flags)
}
declare 71 generic {
    int TclObjInvokeGlobal(Tcl_Interp *interp, int objc, \
	    Tcl_Obj *CONST objv[], int flags)
}
declare 72 generic {
    int TclOpenFileChannelDeleteProc(TclOpenFileChannelProc_ *proc)
}
declare 73 generic {
    int TclOpenFileChannelInsertProc(TclOpenFileChannelProc_ *proc)
}
declare 74 generic {
    char * TclpAlloc(unsigned int size)
}
declare 75 generic {
    int TclpCloseFile(TclFile file)
}
declare 76 generic {
    int TclpCopyFile(char *source, char *dest)
}
declare 77 generic {
    int TclpCopyDirectory(char *source, char *dest, Tcl_DString *errorPtr)
}
declare 78 generic {
    Tcl_Channel TclpCreateCommandChannel(TclFile readFile, \
	    TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr)
}
declare 79 generic {
    int TclpCreateDirectory(char *path)
}
declare 80 generic {
    int TclpCreatePipe(TclFile *readPipe, TclFile *writePipe)
}
declare 81 generic {
    int TclpCreateProcess(Tcl_Interp *interp, int argc, char **argv, \
	    TclFile inputFile, TclFile outputFile, TclFile errorFile, \
	    Tcl_Pid *pidPtr)
}
declare 82 generic {
    TclFile TclpCreateTempFile(char *contents, 
    Tcl_DString *namePtr)
}
declare 83 generic {
    int TclpDeleteFile(char *path)
}
declare 84 generic {
    void TclpFinalize(void)
}
declare 85 generic {
    void TclpFree(char *ptr)
}
declare 86 generic {
    unsigned long TclpGetClicks(void)
}
declare 87 generic {
    unsigned long TclpGetSeconds(void)
}
declare 88 generic {
    void TclpGetTime(Tcl_Time *time)
}
declare 89 generic {
    int TclpGetTimeZone(unsigned long time)
}
declare 90 generic {
    char * TclpGetTZName(void)
}
declare 91 generic {
    int TclpListVolumes(Tcl_Interp *interp)
}
declare 92 generic {
    TclFile TclpMakeFile(Tcl_Channel channel, int direction)
}
declare 93 generic {
    TclFile TclpOpenFile(char *fname, int mode)
}
declare 94 generic {
    Tcl_Channel TclpOpenFileChannel(Tcl_Interp *interp, char *fileName, \
	    char *modeString, int permissions)
}
declare 95 generic {
    char * TclpRealloc(char *ptr, unsigned int size)
}
declare 96 generic {
    int TclpRemoveDirectory(char *path, int recursive, Tcl_DString *errorPtr)
}
declare 97 generic {
    int TclpRenameFile(char *source, char *dest)
}
declare 98 generic {
    int TclParseBraces(Tcl_Interp *interp, char *string, char **termPtr, \
	    ParseValue *pvPtr)
}
declare 99 generic {
    int TclParseNestedCmd(Tcl_Interp *interp, char *string, int flags, \
	    char **termPtr, ParseValue *pvPtr)
}
declare 100 generic {
    int TclParseQuotes(Tcl_Interp *interp, char *string, int termChar, \
	    int flags, char **termPtr, ParseValue *pvPtr)
}
declare 101 generic {
    void TclPlatformInit(Tcl_Interp *interp)
}
declare 102 generic {
    char * TclPrecTraceProc(ClientData clientData, Tcl_Interp *interp, \
	    char *name1, char *name2, int flags)
}
declare 103 generic {
    int TclPreventAliasLoop(Tcl_Interp *interp, Tcl_Interp *cmdInterp, \
	    Tcl_Command cmd)
}
declare 104 generic {
    void TclPrintByteCodeObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 105 generic {
    void TclProcCleanupProc(Proc *procPtr)
}
declare 106 generic {
    int TclProcCompileProc(Tcl_Interp *interp, Proc *procPtr, \
	    Tcl_Obj *bodyPtr, Namespace *nsPtr, CONST char *description, \
	    CONST char *procName)
}
declare 107 generic {
    void TclProcDeleteProc(ClientData clientData)
}
declare 108 generic {
    int TclProcInterpProc(ClientData clientData, Tcl_Interp *interp, \
	    int argc, char **argv)
}
declare 109 generic {
    int TclRenameCommand(Tcl_Interp *interp, char *oldName, char *newName)
}
declare 110 generic {
    void TclResetShadowedCmdRefs(Tcl_Interp *interp, Command *newCmdPtr)
}
declare 111 generic {
    int TclServiceIdle(void)
}
declare 112 generic {
    Tcl_Obj * TclSetElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, Tcl_Obj *objPtr, int leaveErrorMsg)
}
declare 113 generic {
    Tcl_Obj * TclSetIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    Tcl_Obj *objPtr, int leaveErrorMsg)
}
declare 114 generic {
    char * TclSetPreInitScript(char *string)
}
declare 115 generic {
    void TclSetupEnv(Tcl_Interp *interp)
}
declare 116 generic {
    int TclSockGetPort(Tcl_Interp *interp, char *string, char *proto, \
	    int *portPtr)
}
declare 117 generic {
    int TclSockMinimumBuffers(int sock, int size)
}
declare 118 generic {
    int TclStat(CONST char *path, TclStat_ *buf)
}
declare 119 generic {
    int TclStatDeleteProc(TclStatProc_ *proc)
}
declare 120 generic {
    int TclStatInsertProc(TclStatProc_ *proc)
}
declare 121 generic {
    void TclTeardownNamespace(Namespace *nsPtr)
}
declare 122 generic {
    int TclUpdateReturnInfo(Interp *iPtr)
}
declare 123 generic {
    char * TclWordEnd(char *start, char *lastChar, int nested, int *semiPtr)
}

# Procedures used in conjunction with Tcl namespaces. They are
# defined here instead of in tcl.decls since they are not stable yet.

declare 124 generic {
    void Tcl_AddInterpResolvers(Tcl_Interp *interp, char *name, \
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, \
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}
declare 125 generic {
    int Tcl_AppendExportList(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    Tcl_Obj *objPtr)
}
declare 126 generic {
    Tcl_Namespace * Tcl_CreateNamespace(Tcl_Interp *interp, char *name, \
	    ClientData clientData, Tcl_NamespaceDeleteProc *deleteProc)
}
declare 127 generic {
    void Tcl_DeleteNamespace(Tcl_Namespace *nsPtr)
}
declare 128 generic {
    int Tcl_Export(Tcl_Interp *interp, Tcl_Namespace *nsPtr, char *pattern, \
	    int resetListFirst)
}
declare 129 generic {
    Tcl_Command Tcl_FindCommand(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 130 generic {
    Tcl_Namespace * Tcl_FindNamespace(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 131 generic {
    int Tcl_GetInterpResolvers(Tcl_Interp *interp, char *name, \
	    Tcl_ResolverInfo *resInfo)
}
declare 132 generic {
    int Tcl_GetNamespaceResolvers(Tcl_Namespace *namespacePtr, \
	    Tcl_ResolverInfo *resInfo)
}
declare 133 generic {
    Tcl_Var Tcl_FindNamespaceVar(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 134 generic {
    int Tcl_ForgetImport(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    char *pattern)
}
declare 135 generic {
    Tcl_Command Tcl_GetCommandFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 136 generic {
    void Tcl_GetCommandFullName(Tcl_Interp *interp, Tcl_Command command, \
	    Tcl_Obj *objPtr)
}
declare 137 generic {
    Tcl_Namespace * Tcl_GetCurrentNamespace(Tcl_Interp *interp)
}
declare 138 generic {
    Tcl_Namespace * Tcl_GetGlobalNamespace(Tcl_Interp *interp)
}
declare 139 generic {
    void Tcl_GetVariableFullName(Tcl_Interp *interp, Tcl_Var variable, \
	    Tcl_Obj *objPtr)
}
declare 140 generic {
    int Tcl_Import(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    char *pattern, int allowOverwrite)
}
declare 141 generic {
    void Tcl_PopCallFrame(Tcl_Interp* interp)
}
declare 142 generic {
    int Tcl_PushCallFrame(Tcl_Interp* interp, Tcl_CallFrame *framePtr, \
	    Tcl_Namespace *nsPtr, int isProcCallFrame)
} 
declare 143 generic {
    int Tcl_RemoveInterpResolvers(Tcl_Interp *interp, char *name)
}
declare 144 generic {
    void Tcl_SetNamespaceResolvers(Tcl_Namespace *namespacePtr, \
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, \
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}

# Compilation procedures for commands in the generic core:

declare 145 generic {
    int	TclCompileBreakCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 146 generic {
    int	TclCompileCatchCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 147 generic {
    int	TclCompileContinueCmd(Tcl_Interp *interp, char *string, \
	    char *lastChar, int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 148 generic {
    int	TclCompileExprCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 149 generic {
    int	TclCompileForCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 150 generic {
    int	TclCompileForeachCmd(Tcl_Interp *interp, char *string, \
	    char *lastChar, int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 151 generic {
    int	TclCompileIfCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 152 generic {
    int	TclCompileIncrCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 153 generic {
    int	TclCompileSetCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 154 generic {
    int	TclCompileWhileCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}


##############################################################################

# Define the platform specific internal Tcl interface. These functions are
# only available on the designated platform.

interface tclIntPlat

# Mac specific internals

declare 0 mac {
    int TclpCheckStackSpace(void)
}
declare 1 mac {
    VOID * TclpSysAlloc(long size, int isBin)
}
declare 2 mac {
    void TclpSysFree(VOID *ptr)
}
declare 3 mac {
    VOID * TclpSysRealloc(VOID *cp, unsigned int size)
}
declare 4 mac {
    void TclPlatformExit(int status)
}

# Windows specific internals

declare 0 win {
    void TclWinConvertError(DWORD errCode)
}
declare 1 win {
    void TclWinConvertWSAError(DWORD errCode)
}
declare 2 win {
    struct servent * TclWinGetServByName(const char *nm, \
	    const char *proto)
}
declare 3 win {
    int TclWinGetSockOpt(SOCKET s, int level, int optname, \
	    char FAR * optval, int FAR *optlen)
}
declare 4 win {
    HINSTANCE TclWinGetTclInstance(void)
}
declare 5 win {
    HINSTANCE	TclWinLoadLibrary(char *name)
}
declare 6 win {
    u_short TclWinNToHS(u_short ns)
}
declare 7 win {
    int TclWinSetSockOpt(SOCKET s, int level, int optname, \
	    const char FAR * optval, int optlen)
}
