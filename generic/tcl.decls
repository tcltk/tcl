# tcl.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Tcl library via the stubs table.
#	This file is used to generate the tclDecls.h, tclPlatDecls.h,
#	tclStub.c, and tclPlatStub.c files.
#	
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: tcl.decls,v 1.2.2.1 1999/03/05 20:18:02 stanton Exp $

library tcl

# Define the tcl interface with several sub interfaces:
#     tclPlat	 - platform specific public
#     tclInt	 - generic private
#     tclPlatInt - platform specific private
#     tclCompile - private compiler/execution module fucntions

interface tcl
hooks {tclPlat tclInt tclIntPlat tclCompile}

# Declare each of the functions in the public Tcl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 generic {
    void Tcl_AddErrorInfo (Tcl_Interp *interp, char *message)
}
declare 1 generic {
    void Tcl_AddObjErrorInfo(Tcl_Interp *interp, char *message, int length)
}
declare 2 generic {
    char * Tcl_Alloc(unsigned int size)
}
declare 3 generic {
    void Tcl_AllowExceptions(Tcl_Interp *interp)
}
declare 4 generic {
    int Tcl_AppendAllObjTypes(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 5 generic {
    void Tcl_AppendElement(Tcl_Interp *interp, char *string)
}
declare 6 generic {
    void Tcl_AppendResult(Tcl_Interp *interp, ...)
}
declare 7 generic {
    void Tcl_AppendResultVA(Tcl_Interp *interp, va_list argList)
}
declare 8 generic {
    void Tcl_AppendToObj(Tcl_Obj *objPtr, char *bytes, int length)
}
declare 9 generic {
    void Tcl_AppendStringsToObj(Tcl_Obj *objPtr, ...)
}
declare 10 generic {
    void Tcl_AppendStringsToObjVA(Tcl_Obj *objPtr, va_list argList)
}
declare 11 generic {
    Tcl_AsyncHandler Tcl_AsyncCreate(Tcl_AsyncProc *proc, \
	    ClientData clientData)
}
declare 12 generic {
    void Tcl_AsyncDelete(Tcl_AsyncHandler async)
}
declare 13 generic {
    int Tcl_AsyncInvoke(Tcl_Interp *interp, int code)
}
declare 14 generic {
    void Tcl_AsyncMark(Tcl_AsyncHandler async)
}
declare 15 generic {
    int Tcl_AsyncReady(void)
}
declare 16 generic {
    void Tcl_BackgroundError(Tcl_Interp *interp)
}
declare 17 generic {
    char Tcl_Backslash(CONST char *src, int *readPtr)
}
declare 18 generic {
    int Tcl_BadChannelOption(Tcl_Interp *interp, char *optionName, \
	    char *optionList)
}
declare 19 generic {
    void Tcl_CallWhenDeleted(Tcl_Interp *interp, Tcl_InterpDeleteProc *proc, \
	    ClientData clientData)
}
declare 20 generic {
    void Tcl_CancelIdleCall(Tcl_IdleProc *idleProc, ClientData clientData)
}
declare 21 generic {
    int Tcl_Close(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 22 generic {
    int Tcl_CommandComplete(char *cmd)
}
declare 23 generic {
    char * Tcl_Concat(int argc, char **argv)
}
declare 24 generic {
    Tcl_Obj * Tcl_ConcatObj(int objc, Tcl_Obj *CONST objv[])
}
declare 25 generic {
    int Tcl_ConvertCountedElement(CONST char *src, int length, char *dst, \
	    int flags)
}
declare 26 generic {
    int Tcl_ConvertElement(CONST char *src, char *dst, int flags)
}
declare 27 generic {
    int Tcl_ConvertToType(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    Tcl_ObjType *typePtr)
}
declare 28 generic {
    int Tcl_CreateAlias(Tcl_Interp *slave, char *slaveCmd, \
	    Tcl_Interp *target, char *targetCmd, int argc, char **argv)
}
declare 29 generic {
    int Tcl_CreateAliasObj(Tcl_Interp *slave, char *slaveCmd, \
	    Tcl_Interp *target, char *targetCmd, int objc, \
	    Tcl_Obj *CONST objv[])
}
declare 30 generic {
    Tcl_Channel Tcl_CreateChannel(Tcl_ChannelType *typePtr, char *chanName, \
	    ClientData instanceData, int mask)
}
declare 31 generic {
    void Tcl_CreateChannelHandler(Tcl_Channel chan, int mask, \
	    Tcl_ChannelProc *proc, ClientData clientData)
}
declare 32 generic {
    void Tcl_CreateCloseHandler(Tcl_Channel chan, Tcl_CloseProc *proc, \
	    ClientData clientData)
}
declare 33 generic {
    Tcl_Command Tcl_CreateCommand(Tcl_Interp *interp, char *cmdName, \
	    Tcl_CmdProc *proc, ClientData clientData, \
	    Tcl_CmdDeleteProc *deleteProc)
}
declare 34 generic {
    void Tcl_CreateEventSource(Tcl_EventSetupProc *setupProc, \
	    Tcl_EventCheckProc *checkProc, ClientData clientData)
}
declare 35 generic {
    void Tcl_CreateExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 36 generic {
    Tcl_Interp * Tcl_CreateInterp(void)
}
declare 37 generic {
    void Tcl_CreateMathFunc(Tcl_Interp *interp, char *name, int numArgs, \
	    Tcl_ValueType *argTypes, Tcl_MathProc *proc, ClientData clientData)
}
declare 38 generic {
    Tcl_Command Tcl_CreateObjCommand(Tcl_Interp *interp, char *cmdName, \
	    Tcl_ObjCmdProc *proc, ClientData clientData, \
	    Tcl_CmdDeleteProc *deleteProc)
}
declare 39 generic {
    Tcl_Interp * Tcl_CreateSlave(Tcl_Interp *interp, char *slaveName, \
	    int isSafe)
}
declare 40 generic {
    Tcl_TimerToken Tcl_CreateTimerHandler(int milliseconds, \
	    Tcl_TimerProc *proc, ClientData clientData)
}
declare 41 generic {
    Tcl_Trace Tcl_CreateTrace(Tcl_Interp *interp, int level, \
	    Tcl_CmdTraceProc *proc, ClientData clientData)
}
declare 42 generic {
    char * Tcl_DbCkalloc(unsigned int size, char *file, int line)
}
declare 43 generic {
    int Tcl_DbCkfree(char *ptr, char *file, int line)
}
declare 44 generic {
    char * Tcl_DbCkrealloc(char *ptr, unsigned int size, char *file, int line)
}
declare 45 generic {
    void Tcl_DbDecrRefCount(Tcl_Obj *objPtr, char *file, int line)
}
declare 46 generic {
    void Tcl_DbIncrRefCount(Tcl_Obj *objPtr, char *file, int line)
}
declare 47 generic {
    int Tcl_DbIsShared(Tcl_Obj *objPtr, char *file, int line)
}
declare 48 generic {
    Tcl_Obj * Tcl_DbNewBooleanObj(int boolValue, char *file, int line)
}
declare 49 generic {
    Tcl_Obj * Tcl_DbNewByteArrayObj(unsigned char *bytes, int length, \
	    char *file, int line)
}
declare 50 generic {
    Tcl_Obj * Tcl_DbNewDoubleObj(double doubleValue, char *file, int line)
}
declare 51 generic {
    Tcl_Obj * Tcl_DbNewListObj(int objc, Tcl_Obj *CONST objv[], char *file, \
	    int line)
}
declare 52 generic {
    Tcl_Obj * Tcl_DbNewLongObj(long longValue, char *file, int line)
}
declare 53 generic {
    Tcl_Obj * Tcl_DbNewObj(char *file, int line)
}
declare 54 generic {
    Tcl_Obj * Tcl_DbNewStringObj(char *bytes, int length, char *file, int line)
}
declare 55 generic {
    void Tcl_DeleteAssocData(Tcl_Interp *interp, char *name)
}
declare 56 generic {
    int Tcl_DeleteCommand(Tcl_Interp *interp, char *cmdName)
}
declare 57 generic {
    int Tcl_DeleteCommandFromToken(Tcl_Interp *interp, Tcl_Command command)
}
declare 58 generic {
    void Tcl_DeleteChannelHandler(Tcl_Channel chan, Tcl_ChannelProc *proc, \
	    ClientData clientData)
}
declare 59 generic {
    void Tcl_DeleteCloseHandler(Tcl_Channel chan, Tcl_CloseProc *proc, \
	    ClientData clientData)
}
declare 60 generic {
    void Tcl_DeleteEvents(Tcl_EventDeleteProc *proc, ClientData clientData)
}
declare 61 generic {
    void Tcl_DeleteEventSource(Tcl_EventSetupProc *setupProc, \
	    Tcl_EventCheckProc *checkProc, ClientData clientData)
}
declare 62 generic {
    void Tcl_DeleteExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 63 generic {
    void Tcl_DeleteHashEntry(Tcl_HashEntry *entryPtr)
}
declare 64 generic {
    void Tcl_DeleteHashTable(Tcl_HashTable *tablePtr)
}
declare 65 generic {
    void Tcl_DeleteInterp(Tcl_Interp *interp)
}
declare 66 generic {
    void Tcl_DeleteTimerHandler(Tcl_TimerToken token)
}
declare 67 generic {
    void Tcl_DeleteTrace(Tcl_Interp *interp, Tcl_Trace trace)
}
declare 68 generic {
    void Tcl_DetachPids(int numPids, Tcl_Pid *pidPtr)
}
declare 69 generic {
    void Tcl_DontCallWhenDeleted(Tcl_Interp *interp, \
	    Tcl_InterpDeleteProc *proc, ClientData clientData)
}
declare 70 generic {
    int Tcl_DoOneEvent(int flags)
}
declare 71 generic {
    void Tcl_DoWhenIdle(Tcl_IdleProc *proc, ClientData clientData)
}
declare 72 generic {
    char * Tcl_DStringAppend(Tcl_DString *dsPtr, CONST char *string, \
	    int length)
}
declare 73 generic {
    char * Tcl_DStringAppendElement(Tcl_DString *dsPtr, CONST char *string)
}
declare 74 generic {
    void Tcl_DStringEndSublist(Tcl_DString *dsPtr)
}
declare 75 generic {
    void Tcl_DStringFree(Tcl_DString *dsPtr)
}
declare 76 generic {
    void Tcl_DStringGetResult(Tcl_Interp *interp, Tcl_DString *dsPtr)
}
declare 77 generic {
    void Tcl_DStringInit(Tcl_DString *dsPtr)
}
declare 78 generic {
    void Tcl_DStringResult(Tcl_Interp *interp, Tcl_DString *dsPtr)
}
declare 79 generic {
    void Tcl_DStringSetLength(Tcl_DString *dsPtr, int length)
}
declare 80 generic {
    void Tcl_DStringStartSublist(Tcl_DString *dsPtr)
}
declare 81 generic {
    int Tcl_DumpActiveMemory(char *fileName)
}
declare 82 generic {
    Tcl_Obj * Tcl_DuplicateObj(Tcl_Obj *objPtr)
}
declare 83 generic {
    int Tcl_Eof(Tcl_Channel chan)
}
declare 84 generic {
    char * Tcl_ErrnoId(void)
}
declare 85 generic {
    char * Tcl_ErrnoMsg(int err)
}
declare 86 generic {
    int Tcl_Eval(Tcl_Interp *interp, char *string)
}
declare 87 generic {
    int Tcl_EvalFile(Tcl_Interp *interp, char *fileName)
}
declare 88 generic {
    void Tcl_EventuallyFree(ClientData clientData, Tcl_FreeProc *freeProc)
}
declare 89 generic {
    int Tcl_EvalObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 90 generic {
    void Tcl_Exit(int status)
}
declare 91 generic {
    int Tcl_ExposeCommand(Tcl_Interp *interp, char *hiddenCmdToken, \
	    char *cmdName)
}
declare 92 generic {
    int Tcl_ExprBoolean(Tcl_Interp *interp, char *string, int *ptr)
}
declare 93 generic {
    int Tcl_ExprBooleanObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *ptr)
}
declare 94 generic {
    int Tcl_ExprDouble(Tcl_Interp *interp, char *string, double *ptr)
}
declare 95 generic {
    int Tcl_ExprDoubleObj(Tcl_Interp *interp, Tcl_Obj *objPtr, double *ptr)
}
declare 96 generic {
    int Tcl_ExprLong(Tcl_Interp *interp, char *string, long *ptr)
}
declare 97 generic {
    int Tcl_ExprLongObj(Tcl_Interp *interp, Tcl_Obj *objPtr, long *ptr)
}
declare 98 generic {
    int Tcl_ExprObj(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    Tcl_Obj **resultPtrPtr)
}
declare 99 generic {
    int Tcl_ExprString(Tcl_Interp *interp, char *string)
}
declare 100 generic {
    void Tcl_Finalize(void)
}
declare 101 generic {
    void Tcl_FindExecutable(char *argv0)
}
declare 102 generic {
    Tcl_HashEntry * Tcl_FirstHashEntry(Tcl_HashTable *tablePtr, \
	    Tcl_HashSearch *searchPtr)
}
declare 103 generic {
    int Tcl_Flush(Tcl_Channel chan)
}
declare 104 generic {
    void Tcl_Free(char *ptr)
}
declare 105 generic {
    void TclFreeObj(Tcl_Obj *objPtr)
}
declare 106 generic {
    void Tcl_FreeResult(Tcl_Interp *interp)
}
declare 107 generic {
    int Tcl_GetAlias(Tcl_Interp *interp, char *slaveCmd, \
	    Tcl_Interp **targetInterpPtr, char **targetCmdPtr, int *argcPtr, char ***argvPtr)
}
declare 108 generic {
    int Tcl_GetAliasObj(Tcl_Interp *interp, char *slaveCmd, \
	    Tcl_Interp **targetInterpPtr, char **targetCmdPtr, int *objcPtr, Tcl_Obj ***objv)
}
declare 109 generic {
    ClientData Tcl_GetAssocData(Tcl_Interp *interp, char *name, \
	    Tcl_InterpDeleteProc **procPtr)
}
declare 110 generic {
    int Tcl_GetBoolean(Tcl_Interp *interp, char *string, int *boolPtr)
}
declare 111 generic {
    int Tcl_GetBooleanFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    int *boolPtr)
}
declare 112 generic {
    unsigned char * Tcl_GetByteArrayFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 113 generic {
    Tcl_Channel Tcl_GetChannel(Tcl_Interp *interp, char *chanName, \
	    int *modePtr)
}
declare 114 generic {
    int Tcl_GetChannelBufferSize(Tcl_Channel chan)
}
declare 115 generic {
    int Tcl_GetChannelHandle(Tcl_Channel chan, int direction, \
	    ClientData *handlePtr)
}
declare 116 generic {
    ClientData Tcl_GetChannelInstanceData(Tcl_Channel chan)
}
declare 117 generic {
    int Tcl_GetChannelMode(Tcl_Channel chan)
}
declare 118 generic {
    char * Tcl_GetChannelName(Tcl_Channel chan)
}
declare 119 generic {
    int Tcl_GetChannelOption(Tcl_Interp *interp, Tcl_Channel chan, \
	    char *optionName, Tcl_DString *dsPtr)
}
declare 120 generic {
    Tcl_ChannelType * Tcl_GetChannelType(Tcl_Channel chan)
}
declare 121 generic {
    int Tcl_GetCommandInfo(Tcl_Interp *interp, char *cmdName, \
	    Tcl_CmdInfo *infoPtr)
}
declare 122 generic {
    char * Tcl_GetCommandName(Tcl_Interp *interp, Tcl_Command command)
}
declare 123 generic {
    int Tcl_GetDouble(Tcl_Interp *interp, char *string, double *doublePtr)
}
declare 124 generic {
    int Tcl_GetDoubleFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    double *doublePtr)
}
declare 125 generic {
    int Tcl_GetErrno(void)
}
declare 126 generic {
    char * Tcl_GetHostName(void)
}
declare 127 generic {
    int Tcl_GetIndexFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    char **tablePtr, char *msg, int flags, int *indexPtr)
}
declare 128 generic {
    int Tcl_GetInt(Tcl_Interp *interp, char *string, int *intPtr)
}
declare 129 generic {
    int Tcl_GetInterpPath(Tcl_Interp *askInterp, Tcl_Interp *slaveInterp)
}
declare 130 generic {
    int Tcl_GetIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *intPtr)
}
declare 131 generic {
    int Tcl_GetLongFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, long *longPtr)
}
declare 132 generic {
    Tcl_Interp * Tcl_GetMaster(Tcl_Interp *interp)
}
declare 133 generic {
    CONST char * Tcl_GetNameOfExecutable(void)
}
declare 134 generic {
    Tcl_Obj * Tcl_GetObjResult(Tcl_Interp *interp)
}
declare 135 generic {
    Tcl_ObjType * Tcl_GetObjType(char *typeName)
}
declare 136 generic {
    Tcl_PathType Tcl_GetPathType(char *path)
}
declare 137 generic {
    int Tcl_Gets(Tcl_Channel chan, Tcl_DString *dsPtr)
}
declare 138 generic {
    int Tcl_GetsObj(Tcl_Channel chan, Tcl_Obj *objPtr)
}
declare 139 generic {
    int Tcl_GetServiceMode(void)
}
declare 140 generic {
    Tcl_Interp * Tcl_GetSlave(Tcl_Interp *interp, char *slaveName)
}
declare 141 generic {
    Tcl_Channel Tcl_GetStdChannel(int type)
}
declare 142 generic {
    char * Tcl_GetStringFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 143 generic {
    char * Tcl_GetStringResult(Tcl_Interp *interp)
}
declare 144 generic {
    char * Tcl_GetVar(Tcl_Interp *interp, char *varName, int flags)
}
declare 145 generic {
    char * Tcl_GetVar2(Tcl_Interp *interp, char *part1, char *part2, int flags)
}
declare 146 generic {
    int Tcl_GlobalEval(Tcl_Interp *interp, char *command)
}
declare 147 generic {
    int Tcl_GlobalEvalObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 148 generic {
    char * Tcl_HashStats(Tcl_HashTable *tablePtr)
}
declare 149 generic {
    int Tcl_HideCommand(Tcl_Interp *interp, char *cmdName, \
	    char *hiddenCmdToken)
}
declare 150 generic {
    int Tcl_Init(Tcl_Interp *interp)
}
declare 151 generic {
    void Tcl_InitHashTable(Tcl_HashTable *tablePtr, int keyType)
}
declare 152 generic {
    void Tcl_InitMemory(Tcl_Interp *interp)
}
declare 153 generic {
    int Tcl_InputBlocked(Tcl_Channel chan)
}
declare 154 generic {
    int Tcl_InputBuffered(Tcl_Channel chan)
}
declare 155 generic {
    int Tcl_InterpDeleted(Tcl_Interp *interp)
}
declare 156 generic {
    int Tcl_IsSafe(Tcl_Interp *interp)
}
declare 157 generic {
    void Tcl_InvalidateStringRep(Tcl_Obj *objPtr)
}
declare 158 generic {
    char * Tcl_JoinPath(int argc, char **argv, Tcl_DString *resultPtr)
}
declare 159 generic {
    int Tcl_LinkVar(Tcl_Interp *interp, char *varName, char *addr, int type)
}
declare 160 generic {
    int Tcl_ListObjAppendList(Tcl_Interp *interp, Tcl_Obj *listPtr, \
	    Tcl_Obj *elemListPtr)
}
declare 161 generic {
    int Tcl_ListObjAppendElement(Tcl_Interp *interp, Tcl_Obj *listPtr, \
	    Tcl_Obj *objPtr)
}
declare 162 generic {
    int Tcl_ListObjGetElements(Tcl_Interp *interp, Tcl_Obj *listPtr, \
	    int *objcPtr, Tcl_Obj ***objvPtr)
}
declare 163 generic {
    int Tcl_ListObjIndex(Tcl_Interp *interp, Tcl_Obj *listPtr, int index, \
	    Tcl_Obj **objPtrPtr)
}
declare 164 generic {
    int Tcl_ListObjLength(Tcl_Interp *interp, Tcl_Obj *listPtr, int *intPtr)
}
declare 165 generic {
    int Tcl_ListObjReplace(Tcl_Interp *interp, Tcl_Obj *listPtr, int first, \
	    int count, int objc, Tcl_Obj *CONST objv[])
}
declare 166 generic {
    Tcl_Channel Tcl_MakeFileChannel(ClientData handle, int mode)
}
declare 167 generic {
    int Tcl_MakeSafe(Tcl_Interp *interp)
}
declare 168 generic {
    Tcl_Channel Tcl_MakeTcpClientChannel(ClientData tcpSocket)
}
declare 169 generic {
    char * Tcl_Merge(int argc, char **argv)
}
declare 170 generic {
    Tcl_Obj * Tcl_NewBooleanObj(int boolValue)
}
declare 171 generic {
    Tcl_Obj * Tcl_NewByteArrayObj(unsigned char *bytes, int length)
}
declare 172 generic {
    Tcl_Obj * Tcl_NewDoubleObj(double doubleValue)
}
declare 173 generic {
    Tcl_Obj * Tcl_NewIntObj(int intValue)
}
declare 174 generic {
    Tcl_Obj * Tcl_NewListObj(int objc, Tcl_Obj *CONST objv[])
}
declare 175 generic {
    Tcl_Obj * Tcl_NewLongObj(long longValue)
}
declare 176 generic {
    Tcl_Obj * Tcl_NewObj(void)
}
declare 177 generic {
    Tcl_Obj *Tcl_NewStringObj(char *bytes, int length)
}
declare 178 generic {
    Tcl_HashEntry * Tcl_NextHashEntry(Tcl_HashSearch *searchPtr)
}
declare 179 generic {
    void Tcl_NotifyChannel(Tcl_Channel channel, int mask)
}
declare 180 generic {
    Tcl_Obj * Tcl_ObjGetVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr, \
	    Tcl_Obj *part2Ptr, int flags)
}
declare 181 generic {
    Tcl_Obj * Tcl_ObjSetVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr, \
	    Tcl_Obj *part2Ptr, Tcl_Obj *newValuePtr, int flags)
}
declare 182 generic {
    Tcl_Channel Tcl_OpenCommandChannel(Tcl_Interp *interp, int argc, \
	    char **argv, int flags)
}
declare 183 generic {
    Tcl_Channel Tcl_OpenFileChannel(Tcl_Interp *interp, char *fileName, \
	    char *modeString, int permissions)
}
declare 184 generic {
    Tcl_Channel Tcl_OpenTcpClient(Tcl_Interp *interp, int port, \
	    char *address, char *myaddr, int myport, int async)
}
declare 185 generic {
    Tcl_Channel Tcl_OpenTcpServer(Tcl_Interp *interp, int port, char *host, \
	    Tcl_TcpAcceptProc *acceptProc, ClientData callbackData)
}
declare 186 generic {
    void panic(char *format, ...)
}
declare 187 generic {
    void panicVA(char *format, va_list argList)
}
declare 188 generic {
    char * Tcl_ParseVar(Tcl_Interp *interp, char *string, char **termPtr)
}
declare 189 generic {
    char * Tcl_PkgPresent(Tcl_Interp *interp, char *name, char *version, \
	    int exact)
}
declare 190 generic {
    char * Tcl_PkgPresentEx(Tcl_Interp *interp, char *name, char *version, \
	    int exact, ClientData *clientDataPtr)
}
declare 191 generic {
    int Tcl_PkgProvide(Tcl_Interp *interp, char *name, char *version)
}
declare 192 generic {
    int Tcl_PkgProvideEx(Tcl_Interp *interp, char *name, char *version, \
	    ClientData clientData)
}
declare 193 generic {
    char * Tcl_PkgRequire(Tcl_Interp *interp, char *name, char *version, \
	    int exact)
}
declare 194 generic {
    char * Tcl_PkgRequireEx(Tcl_Interp *interp, char *name, char *version, \
	    int exact, ClientData *clientDataPtr)
}
declare 195 generic {
    char * Tcl_PosixError(Tcl_Interp *interp)
}
declare 196 generic {
    void Tcl_Preserve(ClientData data)
}
declare 197 generic {
    void Tcl_PrintDouble(Tcl_Interp *interp, double value, char *dst)
}
declare 198 generic {
    int Tcl_PutEnv(CONST char *string)
}
declare 199 generic {
    void Tcl_QueueEvent(Tcl_Event *evPtr, Tcl_QueuePosition position)
}
declare 200 generic {
    int Tcl_Read(Tcl_Channel chan, char *bufPtr, int toRead)
}
declare 201 generic {
    char * Tcl_Realloc(char *ptr, unsigned int size)
}
declare 202 generic {
    void Tcl_ReapDetachedProcs(void)
}
declare 203 generic {
    int Tcl_RecordAndEval(Tcl_Interp *interp, char *cmd, int flags)
}
declare 204 generic {
    int Tcl_RecordAndEvalObj(Tcl_Interp *interp, Tcl_Obj *cmdPtr, int flags)
}
declare 205 generic {
    Tcl_RegExp Tcl_RegExpCompile(Tcl_Interp *interp, char *string)
}
declare 206 generic {
    int Tcl_RegExpExec(Tcl_Interp *interp, Tcl_RegExp regexp, char *string, \
	    char *start)
}
declare 207 generic {
    int Tcl_RegExpMatch(Tcl_Interp *interp, char *string, char *pattern)
}
declare 208 generic {
    void Tcl_RegExpRange(Tcl_RegExp regexp, int index, char **startPtr, \
	    char **endPtr)
}
declare 209 generic {
    void Tcl_RegisterChannel(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 210 generic {
    void Tcl_RegisterObjType(Tcl_ObjType *typePtr)
}
declare 211 generic {
    void Tcl_Release(ClientData clientData)
}
declare 212 generic {
    void Tcl_ResetResult(Tcl_Interp *interp)
}
declare 213 generic {
    int Tcl_ScanCountedElement(CONST char *string, int length, int *flagPtr)
}
declare 214 generic {
    int Tcl_ScanElement(CONST char *string, int *flagPtr)
}
declare 215 generic {
    int Tcl_Seek(Tcl_Channel chan, int offset, int mode)
}
declare 216 generic {
    int Tcl_ServiceAll(void)
}
declare 217 generic {
    int Tcl_ServiceEvent(int flags)
}
declare 218 generic {
    void Tcl_SetAssocData(Tcl_Interp *interp, char *name, \
	    Tcl_InterpDeleteProc *proc, ClientData clientData)
}
declare 219 generic {
    void Tcl_SetBooleanObj(Tcl_Obj *objPtr,  int boolValue)
}
declare 220 generic {
    unsigned char * Tcl_SetByteArrayLength(Tcl_Obj *objPtr, int length)
}
declare 221 generic {
    void Tcl_SetByteArrayObj(Tcl_Obj *objPtr, unsigned char *bytes, int length)
}
declare 222 generic {
    void Tcl_SetChannelBufferSize(Tcl_Channel chan, int sz)
}
declare 223 generic {
    int Tcl_SetChannelOption(Tcl_Interp *interp, Tcl_Channel chan, \
	    char *optionName, char *newValue)
}
declare 224 generic {
    int Tcl_SetCommandInfo(Tcl_Interp *interp, char *cmdName, \
	    Tcl_CmdInfo *infoPtr)
}
declare 225 generic {
    void Tcl_SetDoubleObj(Tcl_Obj *objPtr, double doubleValue)
}
declare 226 generic {
    void Tcl_SetErrno(int err)
}
declare 227 generic {
    void Tcl_SetErrorCode(Tcl_Interp *interp, ...)
}
declare 228 generic {
    void Tcl_SetErrorCodeVA(Tcl_Interp *interp, va_list argList)
}
declare 229 generic {
    void Tcl_SetIntObj(Tcl_Obj *objPtr,  int intValue)
}
declare 230 generic {
    void Tcl_SetListObj(Tcl_Obj *objPtr,  int objc, Tcl_Obj *CONST objv[])
}
declare 231 generic {
    void Tcl_SetLongObj(Tcl_Obj *objPtr,  long longValue)
}
declare 232 generic {
    void Tcl_SetMaxBlockTime(Tcl_Time *timePtr)
}
declare 233 generic {
    void Tcl_SetObjErrorCode(Tcl_Interp *interp, Tcl_Obj *errorObjPtr)
}
declare 234 generic {
    void Tcl_SetObjLength(Tcl_Obj *objPtr, int length)
}
declare 235 generic {
    void Tcl_SetObjResult(Tcl_Interp *interp, Tcl_Obj *resultObjPtr)
}
declare 236 generic {
    void Tcl_SetPanicProc(Tcl_PanicProc *panicProc)
}
declare 237 generic {
    int Tcl_SetRecursionLimit(Tcl_Interp *interp, int depth)
}
declare 238 generic {
    void Tcl_SetResult(Tcl_Interp *interp, char *string, \
	    Tcl_FreeProc *freeProc)
}
declare 239 generic {
    int Tcl_SetServiceMode(int mode)
}
declare 240 generic {
    void Tcl_SetStdChannel(Tcl_Channel channel, int type)
}
declare 241 generic {
    void Tcl_SetStringObj(Tcl_Obj *objPtr,  char *bytes, int length)
}
declare 242 generic {
    void Tcl_SetTimer(Tcl_Time *timePtr)
}
declare 243 generic {
    char * Tcl_SetVar(Tcl_Interp *interp, char *varName, char *newValue, \
	    int flags)
}
declare 244 generic {
    char * Tcl_SetVar2(Tcl_Interp *interp, char *part1, char *part2, \
	    char *newValue, int flags)
}
declare 245 generic {
    char * Tcl_SignalId(int sig)
}
declare 246 generic {
    char * Tcl_SignalMsg(int sig)
}
declare 247 generic {
    void Tcl_Sleep(int ms)
}
declare 248 generic {
    void Tcl_SourceRCFile(Tcl_Interp *interp)
}
declare 249 generic {
    int Tcl_SplitList(Tcl_Interp *interp, char *list, int *argcPtr, \
	    char ***argvPtr)
}
declare 250 generic {
    void Tcl_SplitPath(char *path, int *argcPtr, char ***argvPtr)
}
declare 251 generic {
    void Tcl_StaticPackage(Tcl_Interp *interp, char *pkgName, \
	    Tcl_PackageInitProc *initProc, Tcl_PackageInitProc *safeInitProc)
}
declare 252 generic {
    int Tcl_StringMatch(char *string, char *pattern)
}
declare 253 generic {
    int Tcl_Tell(Tcl_Channel chan)
}
declare 254 generic {
    int Tcl_TraceVar(Tcl_Interp *interp, char *varName, int flags, \
	    Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 255 generic {
    int Tcl_TraceVar2(Tcl_Interp *interp, char *part1, char *part2, \
	    int flags, Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 256 generic {
    char * Tcl_TranslateFileName(Tcl_Interp *interp, char *name, \
	    Tcl_DString *bufferPtr)
}
declare 257 generic {
    int Tcl_Ungets(Tcl_Channel chan, char *str, int len, int atHead)
}
declare 258 generic {
    void Tcl_UnlinkVar(Tcl_Interp *interp, char *varName)
}
declare 259 generic {
    int Tcl_UnregisterChannel(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 260 generic {
    int Tcl_UnsetVar(Tcl_Interp *interp, char *varName, int flags)
}
declare 261 generic {
    int Tcl_UnsetVar2(Tcl_Interp *interp, char *part1, char *part2, int flags)
}
declare 262 generic {
    void Tcl_UntraceVar(Tcl_Interp *interp, char *varName, int flags, \
	    Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 263 generic {
    void Tcl_UntraceVar2(Tcl_Interp *interp, char *part1, char *part2, \
	    int flags, Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 264 generic {
    void Tcl_UpdateLinkedVar(Tcl_Interp *interp, char *varName)
}
declare 265 generic {
    int Tcl_UpVar(Tcl_Interp *interp, char *frameName, char *varName, \
	    char *localName, int flags)
}
declare 266 generic {
    int Tcl_UpVar2(Tcl_Interp *interp, char *frameName, char *part1, \
	    char *part2, char *localName, int flags)
}
declare 267 generic {
    void Tcl_ValidateAllMemory(char *file, int line)
}
declare 268 generic {
    int Tcl_VarEval(Tcl_Interp *interp, ...)
}
declare 269 generic {
    int  Tcl_VarEvalVA(Tcl_Interp *interp, va_list argList)
}
declare 270 generic {
    ClientData Tcl_VarTraceInfo(Tcl_Interp *interp, char *varName, \
	    int flags, Tcl_VarTraceProc *procPtr, ClientData prevClientData)
}
declare 271 generic {
    ClientData Tcl_VarTraceInfo2(Tcl_Interp *interp, char *part1, \
	    char *part2, int flags, Tcl_VarTraceProc *procPtr, \
	    ClientData prevClientData)
}
declare 272 generic {
    int Tcl_WaitForEvent(Tcl_Time *timePtr)
}
declare 273 generic {
    Tcl_Pid Tcl_WaitPid(Tcl_Pid pid, int *statPtr, int options)
}
declare 274 generic {
    int Tcl_Write(Tcl_Channel chan, char *s, int slen)
}
declare 275 generic {
    void Tcl_WrongNumArgs(Tcl_Interp *interp, int objc, \
	    Tcl_Obj *CONST objv[], char *message)
}

##############################################################################

# Define the platform specific public Tcl interface.  These functions are
# only available on the designated platform.

interface tclPlat

##################
# Mac declarations

# This is needed by the shells to handle Macintosh events.
 
declare 0 mac {
    void Tcl_MacSetEventProc(Tcl_MacConvertEventPtr procPtr)
}

# These routines are useful for handling using scripts from resources 
# in the application shell

declare 1 mac {
    char * Tcl_MacConvertTextResource(Handle resource)
}
declare 2 mac {
    int Tcl_MacEvalResource(Tcl_Interp *interp, char *resourceName, \
	    int resourceNumber, char *fileName)
}
declare 3 mac {
    Handle Tcl_MacFindResource(Tcl_Interp *interp, long resourceType, \
	    char *resourceName, int resourceNumber, char *resFileRef, \
	    int * releaseIt)
}

# These routines support the new OSType object type (i.e. the packed 4
# character type and creator codes).

declare 4 mac {
    int Tcl_GetOSTypeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, \
	    OSType *osTypePtr)
}
declare 5 mac {
    void Tcl_SetOSTypeObj(Tcl_Obj *objPtr, OSType osType)
}
declare 6 mac {
    Tcl_Obj * Tcl_NewOSTypeObj(OSType osType)
}

# These are not in MSL 2.1.2, so we need to export them from the
# Tcl shared library.  They are found in the compat directory
# except the panic routine which is found in tclMacPanic.h.
 
declare 7 mac {
    int strncasecmp(CONST char *s1, CONST char *s2, size_t n)
}
declare 8 mac {
    int strcasecmp(CONST char *s1, CONST char *s2)
}


####################
# Unix declaractions

declare 0 unix {
    void Tcl_CreateFileHandler(int fd, int mask, Tcl_FileProc *proc, \
	    ClientData clientData)
}
declare 1 unix {
    void Tcl_DeleteFileHandler(int fd)
}
declare 2 unix {
    int Tcl_GetOpenFile(Tcl_Interp *interp, char *string, int write, \
	    int checkUsage, ClientData *filePtr)
}
