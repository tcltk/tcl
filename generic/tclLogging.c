/*
 * tclLogging.c --
 *
 *	Contains the logging facilities for TCL.
 *
 * Copyright (c) 2019 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

static const char *levelNames[] = {
    "DEV",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARNING",
    "ERROR",
    "FATAL",
    "BUG"
};

Tcl_LogLevel
Tcl_GetLogLevel(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    return iPtr->log.level;
}

void
Tcl_SetLogLevel(
    Tcl_Interp *interp,
    Tcl_LogLevel level)
{
    Interp *iPtr = (Interp *) interp;

    if (TCL_LOG_DEV > (int) level || TCL_LOG_BUG < (int) level) {
	Tcl_Panic("bad log level: %d", level);
    }
    iPtr->log.level = level;
    if (iPtr->log.handler && iPtr->log.handler->setLevelProc) {
	iPtr->log.handler->setLevelProc(iPtr->log.clientData, interp, level);
    }
}

int
Tcl_LogLevelEnabled(
    Tcl_Interp *interp,
    Tcl_LogLevel level)
{
    Interp *iPtr = (Interp *) interp;
    return level >= iPtr->log.level;
}

void
Tcl_Log(
    Tcl_Interp *interp,
    Tcl_LogLevel level,
    const char *message,
    ...)
{
    Interp *iPtr = (Interp *) interp;
    va_list argList;
    Tcl_Obj *objPtr;

    /*
     * Check the logging level is legal.
     */

    if (TCL_LOG_DEV > (int) level || TCL_LOG_BUG < (int) level) {
	Tcl_Panic("bad log level: %d", level);
    }

    /*
     * Are we allowed to log this at the moment?
     */

    if (level < iPtr->log.level) {
	return;
    }

    /*
     * Is there a handler registered?
     */

    if (!iPtr->log.handler || !iPtr->log.handler->logProc) {
	return;
    }

    /*
     * Format the log detail message. The log handler itself may do more.
     */

    objPtr = Tcl_NewObj();
    va_start(argList, message);
    TclAppendPrintfToObjVA(objPtr, message, argList);
    va_end(argList);

    /*
     * Dispatch to the log handler. Note that logging does not report errors
     * directly.
     */

    Tcl_IncrRefCount(objPtr);
    iPtr->log.handler->logProc(iPtr->log.clientData,
	    interp, level, objPtr);
    Tcl_DecrRefCount(objPtr);
}

void
TclDoLog(
    Tcl_Interp *interp,
    Tcl_LogLevel level,
    const char *message,
    ...)
{
    Interp *iPtr = (Interp *) interp;
    va_list argList;
    Tcl_Obj *objPtr;

    /*
     * Are we allowed to log this at the moment?
     */

    if (level < iPtr->log.level) {
	return;
    }

    /*
     * Is there a handler registered?
     */

    if (!iPtr->log.handler || !iPtr->log.handler->logProc) {
	return;
    }

    /*
     * Format the log detail message. The log handler itself may do more.
     */

    objPtr = Tcl_NewObj();
    va_start(argList, message);
    TclAppendPrintfToObjVA(objPtr, message, argList);
    va_end(argList);

    /*
     * Dispatch to the log handler. Note that logging does not report errors
     * directly.
     */

    Tcl_IncrRefCount(objPtr);
    iPtr->log.handler->logProc(iPtr->log.clientData,
	    interp, level, objPtr);
    Tcl_DecrRefCount(objPtr);
}

void
Tcl_SetLogHandler(
    Tcl_Interp *interp,
    const Tcl_LogHandler *logHandler,
    ClientData clientData)
{
    Interp *iPtr = (Interp *) interp;

    /*
     * Free any old handler.
     */

    if (iPtr->log.handler && iPtr->log.handler->freeProc) {
	iPtr->log.handler->freeProc(iPtr->log.clientData);
    }

    /*
     * Install the new log handler.
     */

    iPtr->log.handler = logHandler;
    iPtr->log.clientData = clientData;

    /*
     * Inform the log handler what the current logging level is.
     */

    if (iPtr->log.handler->setLevelProc) {
	iPtr->log.handler->setLevelProc(iPtr->log.clientData,
		interp, iPtr->log.level);
    }
}

static Tcl_LogProc LogToStdout;
static Tcl_SetLogLevelProc StdoutLogLevelSet;
static Tcl_FreeLogHandlerProc StdoutLogFree;

static const Tcl_LogHandler simpleStdoutLog = {
    LogToStdout,
    StdoutLogLevelSet,
    StdoutLogFree
};

struct StdoutLog {
    Tcl_LogLevel level;
};

void
TclInstallStdoutLogger(
    Tcl_Interp *interp)
{
    struct StdoutLog *levelStore = ckalloc(sizeof(struct StdoutLog));
    Tcl_SetLogHandler(interp, &simpleStdoutLog, levelStore);
}

static void
StdoutLogLevelSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_LogLevel level)
{
    struct StdoutLog *levelStore = clientData;
    levelStore->level = level;
}

static void
StdoutLogFree(
    ClientData clientData)
{
    struct StdoutLog *levelStore = clientData;
    ckfree(levelStore);
}

static int
LogToStdout(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_LogLevel level,
    Tcl_Obj *message)
{
    fprintf(stdout, "[%s] %s\n", levelNames[level], TclGetString(message));
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
