# tclInt.decls --
#
#	This file contains the declarations for all unsupported
#	functions that are exported by the Tcl library.  This file
#	is used to generate the tclIntDecls.h, tclIntPlatDecls.h,
#	tclIntStub.c, tclPlatStub.c, tclCompileDecls.h and tclCompileStub.c
#	files
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: tclInt.decls,v 1.2.2.1 1999/03/05 20:18:05 stanton Exp $

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
    char * TclGetCwd(Tcl_Interp *interp)
}
declare 30 generic {
    int TclGetDate(char *p, unsigned long now, long zone, \
	    unsigned long *timePtr)
}
declare 31 generic {
    Tcl_Channel TclGetDefaultStdChannel(int type)
}
declare 32 generic {
    Tcl_Obj * TclGetElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, int leaveErrorMsg)
}
declare 33 generic {
    char * TclGetEnv(CONST char *name)
}
declare 34 generic {
    char * TclGetExtension(char *name)
}
declare 35 generic {
    int TclGetFrame(Tcl_Interp *interp, char *string, CallFrame **framePtrPtr)
}
declare 36 generic {
    TclCmdProcType TclGetInterpProc(void)
}
declare 37 generic {
    int TclGetIntForIndex(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    int endValue, int *indexPtr)
}
declare 38 generic {
    Tcl_Obj * TclGetIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    int leaveErrorMsg)
}
declare 39 generic {
    int TclGetLong(Tcl_Interp *interp, char *string, long *longPtr)
}
declare 40 generic {
    int TclGetLoadedPackages(Tcl_Interp *interp, char *targetName)
}
declare 41 generic {
    int TclGetNamespaceForQualName(Tcl_Interp *interp, char *qualName, \
	    Namespace *cxtNsPtr, int flags, Namespace **nsPtrPtr, \
	    Namespace **altNsPtrPtr, Namespace **actualCxtPtrPtr, \
	    char **simpleNamePtr)
}
declare 42 generic {
    TclObjCmdProcType TclGetObjInterpProc(void)
}
declare 43 generic {
    int TclGetOpenMode(Tcl_Interp *interp, char *string, int *seekFlagPtr)
}
declare 44 generic {
    Tcl_Command TclGetOriginalCommand(Tcl_Command command)
}
declare 45 generic {
    char * TclGetUserHome(char *name, Tcl_DString *bufferPtr)
}
declare 46 generic {
    int TclGlobalInvoke(Tcl_Interp *interp, int argc, char **argv, int flags)
}
declare 47 generic {
    int TclGuessPackageName(char *fileName, Tcl_DString *bufPtr)
}
declare 48 generic {
    int TclHideUnsafeCommands(Tcl_Interp *interp)
}
declare 49 generic {
    int TclInExit(void)
}
declare 50 generic {
    Tcl_Obj * TclIncrElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, long incrAmount)
}
declare 51 generic {
    Tcl_Obj * TclIncrIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    long incrAmount)
}
declare 52 generic {
    Tcl_Obj * TclIncrVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr, \
	    Tcl_Obj *part2Ptr, long incrAmount, int part1NotParsed)
}
declare 53 generic {
    void TclInitCompiledLocals(Tcl_Interp *interp, CallFrame *framePtr, \
	    Namespace *nsPtr)
}
declare 54 generic {
    void TclInitNamespaces(void)
}
declare 55 generic {
    int TclInterpInit(Tcl_Interp *interp)
}
declare 56 generic {
    int TclInvoke(Tcl_Interp *interp, int argc, char **argv, int flags)
}
declare 57 generic {
    int TclInvokeObjectCommand(ClientData clientData, Tcl_Interp *interp, \
	    int argc, char **argv)
}
declare 58 generic {
    int TclInvokeStringCommand(ClientData clientData, Tcl_Interp *interp, \
	    int objc, Tcl_Obj *CONST objv[])
}
declare 59 generic {
    Proc * TclIsProc(Command *cmdPtr)
}
declare 60 generic {
    int TclLoadFile(Tcl_Interp *interp, char *fileName, char *sym1, \
	    char *sym2, Tcl_PackageInitProc **proc1Ptr, \
	    Tcl_PackageInitProc **proc2Ptr)
}
declare 61 generic {
    int TclLooksLikeInt(char *p)
}
declare 62 generic {
    Var * TclLookupVar(Tcl_Interp *interp, char *part1, char *part2, \
	    int flags, char *msg, int createPart1, int createPart2, \
	    Var **arrayPtrPtr)
}
declare 63 generic {
    int TclMatchFiles(Tcl_Interp *interp, char *separators, \
	    Tcl_DString *dirPtr, char *pattern, char *tail)
}
declare 64 generic {
    int TclNeedSpace(char *start, char *end)
}
declare 65 generic {
    Tcl_Obj * TclNewProcBodyObj(Proc *procPtr)
}
declare 66 generic {
    int TclObjCommandComplete(Tcl_Obj *cmdPtr)
}
declare 67 generic {
    int TclObjInterpProc(ClientData clientData, Tcl_Interp *interp, \
	    int objc, Tcl_Obj *CONST objv[])
}
declare 68 generic {
    int TclObjInvoke(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[], \
	    int flags)
}
declare 69 generic {
    int TclObjInvokeGlobal(Tcl_Interp *interp, int objc, \
	    Tcl_Obj *CONST objv[], int flags)
}
declare 70 generic {
    int TclOpenFileChannelDeleteProc(TclOpenFileChannelProc_ *proc)
}
declare 71 generic {
    int TclOpenFileChannelInsertProc(TclOpenFileChannelProc_ *proc)
}
declare 72 generic {
    int TclpAccess(CONST char *path, int mode)
}
declare 73 generic {
    char * TclpAlloc(unsigned int size)
}
declare 74 generic {
    int TclpCopyFile(char *source, char *dest)
}
declare 75 generic {
    int TclpCopyDirectory(char *source, char *dest, Tcl_DString *errorPtr)
}
declare 76 generic {
    int TclpCreateDirectory(char *path)
}
declare 77 generic {
    int TclpDeleteFile(char *path)
}
declare 78 generic {
    void TclpFree(char *ptr)
}
declare 79 generic {
    unsigned long TclpGetClicks(void)
}
declare 80 generic {
    unsigned long TclpGetSeconds(void)
}
declare 81 generic {
    void TclpGetTime(Tcl_Time *time)
}
declare 82 generic {
    int TclpGetTimeZone(unsigned long time)
}
declare 83 generic {
    int TclpListVolumes(Tcl_Interp *interp)
}
declare 84 generic {
    Tcl_Channel TclpOpenFileChannel(Tcl_Interp *interp, char *fileName, \
	    char *modeString, int permissions)
}
declare 85 generic {
    char * TclpRealloc(char *ptr, unsigned int size)
}
declare 86 generic {
    int TclpRemoveDirectory(char *path, int recursive, Tcl_DString *errorPtr)
}
declare 87 generic {
    int TclpRenameFile(char *source, char *dest)
}
declare 88 generic {
    int TclParseBraces(Tcl_Interp *interp, char *string, char **termPtr, \
	    ParseValue *pvPtr)
}
declare 89 generic {
    int TclParseNestedCmd(Tcl_Interp *interp, char *string, int flags, \
	    char **termPtr, ParseValue *pvPtr)
}
declare 90 generic {
    int TclParseQuotes(Tcl_Interp *interp, char *string, int termChar, \
	    int flags, char **termPtr, ParseValue *pvPtr)
}
declare 91 generic {
    void TclPlatformInit(Tcl_Interp *interp)
}
declare 92 generic {
    char * TclPrecTraceProc(ClientData clientData, Tcl_Interp *interp, \
	    char *name1, char *name2, int flags)
}
declare 93 generic {
    int TclPreventAliasLoop(Tcl_Interp *interp, Tcl_Interp *cmdInterp, \
	    Tcl_Command cmd)
}
declare 94 generic {
    void TclPrintByteCodeObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 95 generic {
    void TclProcCleanupProc(Proc *procPtr)
}
declare 96 generic {
    int TclProcCompileProc(Tcl_Interp *interp, Proc *procPtr, \
	    Tcl_Obj *bodyPtr, Namespace *nsPtr, CONST char *description, \
	    CONST char *procName)
}
declare 97 generic {
    void TclProcDeleteProc(ClientData clientData)
}
declare 98 generic {
    int TclProcInterpProc(ClientData clientData, Tcl_Interp *interp, \
	    int argc, char **argv)
}
declare 99 generic {
    int TclpStat(CONST char *path, struct stat *buf)
}
declare 100 generic {
    int TclRenameCommand(Tcl_Interp *interp, char *oldName, char *newName)
}
declare 101 generic {
    void TclResetShadowedCmdRefs(Tcl_Interp *interp, Command *newCmdPtr)
}
declare 102 generic {
    int TclServiceIdle(void)
}
declare 103 generic {
    Tcl_Obj * TclSetElementOfIndexedArray(Tcl_Interp *interp, \
	    int localIndex, Tcl_Obj *elemPtr, Tcl_Obj *objPtr, int leaveErrorMsg)
}
declare 104 generic {
    Tcl_Obj * TclSetIndexedScalar(Tcl_Interp *interp, int localIndex, \
	    Tcl_Obj *objPtr, int leaveErrorMsg)
}
declare 105 generic {
    char * TclSetPreInitScript(char *string)
}
declare 106 generic {
    void TclSetupEnv(Tcl_Interp *interp)
}
declare 107 generic {
    int TclSockGetPort(Tcl_Interp *interp, char *string, char *proto, \
	    int *portPtr)
}
declare 108 generic {
    int TclSockMinimumBuffers(int sock, int size)
}
declare 109 generic {
    int TclStat(CONST char *path, TclStat_ *buf)
}
declare 110 generic {
    int TclStatDeleteProc(TclStatProc_ *proc)
}
declare 111 generic {
    int TclStatInsertProc(TclStatProc_ *proc)
}
declare 112 generic {
    void TclTeardownNamespace(Namespace *nsPtr)
}
declare 113 generic {
    int TclUpdateReturnInfo(Interp *iPtr)
}
declare 114 generic {
    char * TclWordEnd(char *start, char *lastChar, int nested, int *semiPtr)
}

# Procedures used in conjunction with Tcl namespaces. They are
# defined here instead of in tcl.decls since they are not stable yet.

declare 115 generic {
    void Tcl_AddInterpResolvers(Tcl_Interp *interp, char *name, \
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, \
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}
declare 116 generic {
    int Tcl_AppendExportList(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    Tcl_Obj *objPtr)
}
declare 117 generic {
    Tcl_Namespace * Tcl_CreateNamespace(Tcl_Interp *interp, char *name, \
	    ClientData clientData, Tcl_NamespaceDeleteProc *deleteProc)
}
declare 118 generic {
    void Tcl_DeleteNamespace(Tcl_Namespace *nsPtr)
}
declare 119 generic {
    int Tcl_Export(Tcl_Interp *interp, Tcl_Namespace *nsPtr, char *pattern, \
	    int resetListFirst)
}
declare 120 generic {
    Tcl_Command Tcl_FindCommand(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 121 generic {
    Tcl_Namespace * Tcl_FindNamespace(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 122 generic {
    int Tcl_GetInterpResolvers(Tcl_Interp *interp, char *name, \
	    Tcl_ResolverInfo *resInfo)
}
declare 123 generic {
    int Tcl_GetNamespaceResolvers(Tcl_Namespace *namespacePtr, \
	    Tcl_ResolverInfo *resInfo)
}
declare 124 generic {
    Tcl_Var Tcl_FindNamespaceVar(Tcl_Interp *interp, char *name, \
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 125 generic {
    int Tcl_ForgetImport(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    char *pattern)
}
declare 126 generic {
    Tcl_Command Tcl_GetCommandFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 127 generic {
    void Tcl_GetCommandFullName(Tcl_Interp *interp, Tcl_Command command, \
	    Tcl_Obj *objPtr)
}
declare 128 generic {
    Tcl_Namespace * Tcl_GetCurrentNamespace(Tcl_Interp *interp)
}
declare 129 generic {
    Tcl_Namespace * Tcl_GetGlobalNamespace(Tcl_Interp *interp)
}
declare 130 generic {
    void Tcl_GetVariableFullName(Tcl_Interp *interp, Tcl_Var variable, \
	    Tcl_Obj *objPtr)
}
declare 131 generic {
    int Tcl_Import(Tcl_Interp *interp, Tcl_Namespace *nsPtr, \
	    char *pattern, int allowOverwrite)
}
declare 132 generic {
    void Tcl_PopCallFrame(Tcl_Interp* interp)
}
declare 133 generic {
    int Tcl_PushCallFrame(Tcl_Interp* interp, Tcl_CallFrame *framePtr, \
	    Tcl_Namespace *nsPtr, int isProcCallFrame)
} 
declare 134 generic {
    int Tcl_RemoveInterpResolvers(Tcl_Interp *interp, char *name)
}
declare 135 generic {
    void Tcl_SetNamespaceResolvers(Tcl_Namespace *namespacePtr, \
	    Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, \
	    Tcl_ResolveCompiledVarProc *compiledVarProc)
}

# Compilation procedures for commands in the generic core:

declare 136 generic {
    int	TclCompileBreakCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 137 generic {
    int	TclCompileCatchCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 138 generic {
    int	TclCompileContinueCmd(Tcl_Interp *interp, char *string, \
	    char *lastChar, int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 139 generic {
    int	TclCompileExprCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 140 generic {
    int	TclCompileForCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 141 generic {
    int	TclCompileForeachCmd(Tcl_Interp *interp, char *string, \
	    char *lastChar, int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 142 generic {
    int	TclCompileIfCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 143 generic {
    int	TclCompileIncrCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 144 generic {
    int	TclCompileSetCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}
declare 145 generic {
    int	TclCompileWhileCmd(Tcl_Interp *interp, char *string, char *lastChar, \
	    int compileFlags, struct CompileEnv *compileEnvPtr)
}

declare 146 generic {
    int TclHasSockets(Tcl_Interp *interp)
}
declare 147 generic {
    struct tm *	TclpGetDate(TclpTime_t time, int useGMT)
}
declare 148 generic {
    size_t TclStrftime(char *s, size_t maxsize, const char *format, \
	    const struct tm *t)
}
declare 149 generic {
    int TclpCheckStackSpace(void)
}

##############################################################################

# Define the platform specific internal Tcl interface. These functions are
# only available on the designated platform.

interface tclIntPlat

########################
# Mac specific internals

declare 0 mac {
    VOID * TclpSysAlloc(long size, int isBin)
}
declare 1 mac {
    void TclpSysFree(VOID *ptr)
}
declare 2 mac {
    VOID * TclpSysRealloc(VOID *cp, unsigned int size)
}
declare 3 mac {
    void TclPlatformExit(int status)
}

# Prototypes for functions found in the tclMacUtil.c compatability library.

declare 4 mac {
    int FSpGetDefaultDir(FSSpecPtr theSpec)
}
declare 5 mac {
    int FSpSetDefaultDir(FSSpecPtr theSpec)
}
declare 6 mac {
    OSErr FSpFindFolder(short vRefNum, OSType folderType, \
	    Boolean createFolder, FSSpec *spec)
}
declare 7 mac {
    void GetGlobalMouse(Point *mouse)
}

# The following routines are utility functions in Tcl.  They are exported
# here because they are needed in Tk.  They are not officially supported,
# however.  The first set are from the MoreFiles package.

declare 8 mac {
    pascal OSErr FSpGetDirectoryID(const FSSpec *spec, long *theDirID, \
	    Boolean *isDirectory)
}
declare 9 mac {
    pascal short FSpOpenResFileCompat(const FSSpec *spec, \
	    SignedByte permission)
}
declare 10 mac {
    pascal void FSpCreateResFileCompat(const FSSpec *spec, OSType creator, \
	    OSType fileType, ScriptCode scriptTag)
}

# Like the MoreFiles routines these fix problems in the standard
# Mac calls.  These routines are from tclMacUtils.h.

declare 11 mac {
    int FSpLocationFromPath(int length, CONST char *path, FSSpecPtr theSpec)
}
declare 12 mac {
    OSErr FSpPathFromLocation(FSSpecPtr theSpec, int *length, \
	    Handle *fullPath)
}

# Prototypes of Mac only internal functions.

declare 13 mac {
    void TclMacExitHandler(void)
}
declare 14 mac {
    void TclMacInitExitToShell(int usePatch)
}
declare 15 mac {
    OSErr TclMacInstallExitToShellPatch(ExitToShellProcPtr newProc)
}
declare 16 mac {
    int TclMacOSErrorToPosixError(int error)
}
declare 17 mac {
    void TclMacRemoveTimer(void *timerToken)
}
declare 18 mac {
    void * TclMacStartTimer(long ms)
}
declare 19 mac {
    int TclMacTimerExpired(void *timerToken)
}
declare 20 mac {
    int TclMacRegisterResourceFork(short fileRef, Tcl_Obj *tokenPtr, \
	    int insert)
}	
declare 21 mac {
    short TclMacUnRegisterResourceFork(char *tokenPtr, Tcl_Obj *resultPtr)
}	
declare 22 mac {
    int TclMacCreateEnv(void)
}
declare 23 mac {
    FILE * TclMacFOpenHack(const char *path, const char *mode)
}
declare 24 mac {
    int TclMacReadlink(char *path, char *buf, int size)
}
declare 25 mac {
    int TclMacChmod(char *path, int mode)
}


############################
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
    HINSTANCE TclWinLoadLibrary(char *name)
}
declare 6 win {
    u_short TclWinNToHS(u_short ns)
}
declare 7 win {
    int TclWinSetSockOpt(SOCKET s, int level, int optname, \
	    const char FAR * optval, int optlen)
}
declare 8 win {
    unsigned long TclpGetPid(Tcl_Pid pid)
}
declare 9 win {
    void TclpFinalize(void)
}
declare 10 win {
    int TclWinGetPlatformId(void)
}
declare 11 win {
    void TclWinInit(HINSTANCE hInst)
}
declare 12 win {
    int TclWinSynchSpawn(void *args, int type, void **trans, Tcl_Pid *pidPtr)
}

# Pipe channel functions

declare 13 win {
    void TclGetAndDetachPids(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 14 win {
    int TclpCloseFile(TclFile file)
}
declare 15 win {
    Tcl_Channel TclpCreateCommandChannel(TclFile readFile, \
	    TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr)
}
declare 16 win {
    int TclpCreatePipe(TclFile *readPipe, TclFile *writePipe)
}
declare 17 win {
    int TclpCreateProcess(Tcl_Interp *interp, int argc, char **argv, \
	    TclFile inputFile, TclFile outputFile, TclFile errorFile, \
	    Tcl_Pid *pidPtr)
}
declare 18 win {
    TclFile TclpCreateTempFile(char *contents, 
    Tcl_DString *namePtr)
}
declare 19 win {
    char * TclpGetTZName(void)
}
declare 20 win {
    TclFile TclpMakeFile(Tcl_Channel channel, int direction)
}
declare 21 win {
    TclFile TclpOpenFile(char *fname, int mode)
}

#########################
# Unix specific internals

# Pipe channel functions

declare 0 unix {
    void TclGetAndDetachPids(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 1 unix {
    int TclpCloseFile(TclFile file)
}
declare 2 unix {
    Tcl_Channel TclpCreateCommandChannel(TclFile readFile, \
	    TclFile writeFile, TclFile errorFile, int numPids, Tcl_Pid *pidPtr)
}
declare 3 unix {
    int TclpCreatePipe(TclFile *readPipe, TclFile *writePipe)
}
declare 4 unix {
    int TclpCreateProcess(Tcl_Interp *interp, int argc, char **argv, \
	    TclFile inputFile, TclFile outputFile, TclFile errorFile, \
	    Tcl_Pid *pidPtr)
}
declare 5 unix {
    TclFile TclpCreateTempFile(char *contents, 
    Tcl_DString *namePtr)
}
declare 6 unix {
    TclFile TclpMakeFile(Tcl_Channel channel, int direction)
}
declare 7 unix {
    TclFile TclpOpenFile(char *fname, int mode)
}
declare 8 unix {
    int TclUnixWaitForFile(int fd, int mask, int timeout)
}

##############################################################################

# Declare the functions used by tclCompile.h.  This is an internal API and
# is subject to change.  Use at your own risk.

interface tclCompile

declare 0 generic {
    void TclCleanupByteCode(ByteCode *codePtr)
}
declare 1 generic {
    int TclCompileExpr(Tcl_Interp *interp, char *string, char *lastChar, \
	    int flags, CompileEnv *envPtr)
}
declare 2 generic {
    int TclCompileQuotes(Tcl_Interp *interp, char *string, char *lastChar, \
	    int termChar, int flags, CompileEnv *envPtr)
}
declare 3 generic {
    int TclCompileString(Tcl_Interp *interp, char *string, char *lastChar, \
	    int flags, CompileEnv *envPtr)
}
declare 4 generic {
    int TclCompileDollarVar(Tcl_Interp *interp, char *string, \
	    char *lastChar, int flags, CompileEnv *envPtr)
}
declare 5 generic {
    int TclCreateAuxData(ClientData clientData, AuxDataType *typePtr, \
	    CompileEnv *envPtr)
}
declare 6 generic {
    ExecEnv * TclCreateExecEnv(Tcl_Interp *interp)
}
declare 7 generic {
    void TclDeleteExecEnv(ExecEnv *eePtr)
}
declare 8 generic {
    void TclEmitForwardJump(CompileEnv *envPtr, TclJumpType jumpType, \
	    JumpFixup *jumpFixupPtr)
}
declare 9 generic {
    AuxDataType *TclGetAuxDataType(char *typeName)
}
declare 10 generic {
    ExceptionRange * TclGetExceptionRangeForPc(unsigned char *pc, \
	    int catchOnly, ByteCode* codePtr)
}
declare 11 generic {
    InstructionDesc * TclGetInstructionTable(void)
}
declare 12 generic {
    int TclExecuteByteCode(Tcl_Interp *interp, ByteCode *codePtr)
}
declare 13 generic {
    void TclExpandCodeArray(CompileEnv *envPtr)
}
declare 14 generic {
    void TclExpandJumpFixupArray(JumpFixupArray *fixupArrayPtr)
}
declare 15 generic {
    void TclFinalizeAuxDataTypeTable(void)
}
declare 16 generic {
    int TclFixupForwardJump(CompileEnv *envPtr, JumpFixup *jumpFixupPtr, \
	    int jumpDist, int distThreshold)
}
declare 17 generic {
    void TclFreeCompileEnv(CompileEnv *envPtr)
}
declare 18 generic {
    void TclFreeJumpFixupArray(JumpFixupArray *fixupArrayPtr)
}
declare 19 generic {
    void TclInitAuxDataTypeTable(void)
}
declare 20 generic {
    void TclInitByteCodeObj(Tcl_Obj *objPtr, CompileEnv *envPtr)
}
declare 21 generic {
    void TclInitCompileEnv(Tcl_Interp *interp, CompileEnv *envPtr, \
	    char *string)
}
declare 22 generic {
    void TclInitJumpFixupArray(JumpFixupArray *fixupArrayPtr)
}
declare 23 generic {
    int TclObjIndexForString(char *start, int length, int allocStrRep, \
	    int inHeap, CompileEnv *envPtr)
}
declare 24 generic {
    int TclPrintInstruction(ByteCode* codePtr, unsigned char *pc)
}
declare 25 generic {
    void TclPrintSource(FILE *outFile, char *string, int maxChars)
}
declare 26 generic {
    void TclRegisterAuxDataType(AuxDataType *typePtr)
}
