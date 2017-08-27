/*
 * tclProcess.c --
 *
 *	This file implements the "tcl::process" ensemble for subprocess 
 *      management as defined by TIP #462.
 *
 * Copyright (c) 2017 Frederic Bonnet.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * Autopurge flag. Process-global because of the way Tcl manages child 
 * processes (see tclPipe.c).
 */

static int autopurge = 1;		/* Autopurge flag. */

/*
 * Hash tables that keeps track of all child process statuses. Keys are the 
 * child process ids and resolved pids, values are (ProcessInfo *).
 */

typedef struct ProcessInfo {
    Tcl_Pid pid; /*FRED TODO*/
    int resolvedPid; /*FRED TODO*/
    int purge; /*FRED TODO*/
    TclProcessWaitStatus status;
    int code; /*FRED TODO*/
    Tcl_Obj *msg; /*FRED TODO*/
    Tcl_Obj *error; /*FRED TODO*/
} ProcessInfo;
static Tcl_HashTable infoTablePerPid;
static Tcl_HashTable infoTablePerResolvedPid;
static int infoTablesInitialized = 0;	/* 0 means not yet initialized. */
TCL_DECLARE_MUTEX(infoTablesMutex)

 /*
 * Prototypes for functions defined later in this file:
 */

static void             InitProcessInfo(ProcessInfo *info, Tcl_Pid pid,
                            int resolvedPid);
static void             FreeProcessInfo(ProcessInfo *info, int preserveObjs);
static int              RefreshProcessInfo(ProcessInfo *info, int options);
static int              WaitProcessStatus(Tcl_Pid pid, int resolvedPid, 
			    int options, int *codePtr, Tcl_Obj **msgPtr,
                            Tcl_Obj **errorObjPtr);
static Tcl_Obj *        BuildProcessStatusObj(ProcessInfo *info);
static int              ProcessListObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int              ProcessStatusObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int              ProcessPurgeObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);
static int              ProcessAutopurgeObjCmd(ClientData clientData,
                            Tcl_Interp *interp, int objc,
                            Tcl_Obj *const objv[]);

/* FRED TODO */
void
InitProcessInfo(
    ProcessInfo *info,
    Tcl_Pid pid,
    int resolvedPid)
{
    info->pid = pid;
    info->resolvedPid = resolvedPid;
    info->purge = 0;
    info->status = TCL_PROCESS_UNCHANGED;
    info->code = 0;
    info->msg = NULL;
    info->error = NULL;
}

/* FRED TODO */
void
FreeProcessInfo(
    ProcessInfo *info,
    int preserveObjs)
{
    if (!preserveObjs) {
        if (info->msg) {
            Tcl_DecrRefCount(info->msg);
        }
        if (info->error) {
            Tcl_DecrRefCount(info->error);
        }
    }
    ckfree(info);
}

/* FRED TODO */
int
RefreshProcessInfo(
    ProcessInfo *info,
    int options
)
{
    if (info->status == TCL_PROCESS_UNCHANGED) {
        /*
         * Refresh & store status.
         */

        info->status = WaitProcessStatus(info->pid, info->resolvedPid, 
                options, &info->code, &info->msg, &info->error);
        if (info->msg) Tcl_IncrRefCount(info->msg);
        if (info->error) Tcl_IncrRefCount(info->error);
        return (info->status != TCL_PROCESS_UNCHANGED);
    } else {
        return 0;
    }
}

/* FRED TODO */
int
WaitProcessStatus(
    Tcl_Pid pid,
    int resolvedPid,
    int options,
    int *codePtr, 
    Tcl_Obj **msgObjPtr,
    Tcl_Obj **errorObjPtr)
{
    int waitStatus;
    Tcl_Obj *errorStrings[5];
    const char *msg;

    pid = Tcl_WaitPid(pid, &waitStatus, options);
    if ((pid == 0)) {
        /*
         * No change.
         */
        
        return TCL_PROCESS_UNCHANGED;
    }

    /*
     * Get process status.
     */

    if (pid == (Tcl_Pid) -1) {
        /*
         * POSIX errName msg
         */

        msg = Tcl_ErrnoMsg(errno);
        if (errno == ECHILD) {
            /*
             * This changeup in message suggested by Mark Diekhans to
             * remind people that ECHILD errors can occur on some
             * systems if SIGCHLD isn't in its default state.
             */

            msg = "child process lost (is SIGCHLD ignored or trapped?)";
        }
        if (codePtr) *codePtr = errno;
        if (msgObjPtr) *msgObjPtr = Tcl_ObjPrintf(
                "error waiting for process to exit: %s", msg);
        if (errorObjPtr) {
            errorStrings[0] = Tcl_NewStringObj("POSIX", -1);
            errorStrings[1] = Tcl_NewStringObj(Tcl_ErrnoId(), -1);
            errorStrings[2] = Tcl_NewStringObj(msg, -1);
            *errorObjPtr = Tcl_NewListObj(3, errorStrings);
        }
        return TCL_PROCESS_ERROR;
    } else if (WIFEXITED(waitStatus)) {
        if (codePtr) *codePtr = WEXITSTATUS(waitStatus);
        if (!WEXITSTATUS(waitStatus)) {
            /*
             * Normal exit.
             */

            if (msgObjPtr) *msgObjPtr = NULL;
            if (errorObjPtr) *errorObjPtr = NULL;
        } else {
            /*
             * CHILDSTATUS pid code
             *
             * Child exited with a non-zero exit status.
             */

            if (msgObjPtr) *msgObjPtr = Tcl_NewStringObj(
	            "child process exited abnormally", -1);
            if (errorObjPtr) {
                errorStrings[0] = Tcl_NewStringObj("CHILDSTATUS", -1);
                errorStrings[1] = Tcl_NewIntObj(resolvedPid);
                errorStrings[2] = Tcl_NewIntObj(WEXITSTATUS(waitStatus));
                *errorObjPtr = Tcl_NewListObj(3, errorStrings);
            }
        }
        return TCL_PROCESS_EXITED;
    } else if (WIFSIGNALED(waitStatus)) {
        /*
         * CHILDKILLED pid sigName msg
         *
         * Child killed because of a signal.
         */

        msg = Tcl_SignalMsg(WTERMSIG(waitStatus));
        if (codePtr) *codePtr = WTERMSIG(waitStatus);
        if (msgObjPtr) *msgObjPtr = Tcl_ObjPrintf(
                "child killed: %s", msg);
        if (errorObjPtr) {
            errorStrings[0] = Tcl_NewStringObj("CHILDKILLED", -1);
            errorStrings[1] = Tcl_NewIntObj(resolvedPid);
            errorStrings[2] = Tcl_NewStringObj(Tcl_SignalId(WTERMSIG(waitStatus)), -1);
            errorStrings[3] = Tcl_NewStringObj(msg, -1);
            *errorObjPtr = Tcl_NewListObj(4, errorStrings);
        }
        return TCL_PROCESS_SIGNALED;
    } else if (WIFSTOPPED(waitStatus)) {
        /*
         * CHILDSUSP pid sigName msg
         *
         * Child suspended because of a signal.
         */

        msg = Tcl_SignalMsg(WSTOPSIG(waitStatus));
        if (codePtr) *codePtr = WSTOPSIG(waitStatus);
        if (msgObjPtr) *msgObjPtr = Tcl_ObjPrintf(
                "child suspended: %s", msg);
        if (errorObjPtr) {
            errorStrings[0] = Tcl_NewStringObj("CHILDSUSP", -1);
            errorStrings[1] = Tcl_NewIntObj(resolvedPid);
            errorStrings[2] = Tcl_NewStringObj(Tcl_SignalId(WSTOPSIG(waitStatus)), -1);
            errorStrings[3] = Tcl_NewStringObj(msg, -1);
            *errorObjPtr = Tcl_NewListObj(4, errorStrings);
        }
        return TCL_PROCESS_STOPPED;
    } else {
        /*
         * TCL OPERATION EXEC ODDWAITRESULT
         *
         * Child wait status didn't make sense.
         */

        if (codePtr) *codePtr = waitStatus;
        if (msgObjPtr) *msgObjPtr = Tcl_NewStringObj(
	        "child wait status didn't make sense\n", -1);
        if (errorObjPtr) {
            errorStrings[0] = Tcl_NewStringObj("TCL", -1);
            errorStrings[1] = Tcl_NewStringObj("OPERATION", -1);
            errorStrings[2] = Tcl_NewStringObj("EXEC", -1);
            errorStrings[3] = Tcl_NewStringObj("ODDWAITRESULT", -1);
            errorStrings[4] = Tcl_NewIntObj(resolvedPid);
            *errorObjPtr = Tcl_NewListObj(5, errorStrings);
        }
        return TCL_PROCESS_UNKNOWN_STATUS;
    }
}

/* FRED TODO */
Tcl_Obj *
BuildProcessStatusObj(
    ProcessInfo *info)
{
    Tcl_Obj *resultObjs[3];

    if (info->status == TCL_PROCESS_UNCHANGED) {
        /*
         * Process still running, return empty obj.
         */

        return Tcl_NewObj();
    }
    if (info->status == TCL_PROCESS_EXITED && info->code == 0) {
        /*
         * Normal exit, return TCL_OK.
         */
        
        return Tcl_NewIntObj(TCL_OK);
    }

    /*
     * Abnormal exit, return {TCL_ERROR msg error}
     */

    resultObjs[0] = Tcl_NewIntObj(TCL_ERROR);
    resultObjs[1] = info->msg;
    resultObjs[2] = info->error;
    return Tcl_NewListObj(3, resultObjs);
}

/*----------------------------------------------------------------------
 *
 * ProcessListObjCmd --
 *
 *	This function implements the 'tcl::process list' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.FRED TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessListObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *list;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    ProcessInfo *info;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, NULL);
	return TCL_ERROR;
    }

    /*
     * Return the list of all chid process ids.
     */

    list = Tcl_NewListObj(0, NULL);
    Tcl_MutexLock(&infoTablesMutex);
    for (entry = Tcl_FirstHashEntry(&infoTablePerResolvedPid, &search); 
            entry != NULL; entry = Tcl_NextHashEntry(&search)) {
        info = (ProcessInfo *) Tcl_GetHashValue(entry);
        Tcl_ListObjAppendElement(interp, list, 
                Tcl_NewIntObj(info->resolvedPid));
    }
    Tcl_MutexUnlock(&infoTablesMutex);
    Tcl_SetObjResult(interp, list);
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ProcessStatusObjCmd --
 *
 *	This function implements the 'tcl::process status' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.FRED TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessStatusObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Obj *dict;
    int index, options = WNOHANG;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    ProcessInfo *info;
    int numPids;
    Tcl_Obj **pidObjs;
    int result;
    int i;
    int pid;
    Tcl_Obj *const *savedobjv = objv;
    static const char *const switches[] = {
	"-wait", "--", NULL
    };
    enum switches {
	STATUS_WAIT, STATUS_LAST
    };
    
    while (objc > 1) {
	if (TclGetString(objv[1])[0] != '-') {
	    break;
	}
	if (Tcl_GetIndexFromObj(interp, objv[1], switches, "switches", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	++objv; --objc;
	if (STATUS_WAIT == (enum switches) index) {
	    options = 0;
	} else {
	    break;
	}
    }
    
    if (objc != 1 && objc != 2) {
	Tcl_WrongNumArgs(interp, 1, savedobjv, "?switches? ?pids?");
	return TCL_ERROR;
    }

    if (objc == 1) {
        /*
        * Return the list of all child process statuses.
        */

        dict = Tcl_NewDictObj();
        Tcl_MutexLock(&infoTablesMutex);
        for (entry = Tcl_FirstHashEntry(&infoTablePerResolvedPid, &search); 
                entry != NULL; entry = Tcl_NextHashEntry(&search)) {
            info = (ProcessInfo *) Tcl_GetHashValue(entry);
            RefreshProcessInfo(info, options);
            // TODO purge etc.

            Tcl_DictObjPut(interp, dict, Tcl_NewIntObj(info->resolvedPid), 
                    BuildProcessStatusObj(info));
        }
        Tcl_MutexUnlock(&infoTablesMutex);
    } else {
        /*
         * Only return statuses of provided processes.
         */
        
        result = Tcl_ListObjGetElements(interp, objv[1], &numPids, &pidObjs);
        if (result != TCL_OK) {
            return result;
        }
        dict = Tcl_NewDictObj();
        Tcl_MutexLock(&infoTablesMutex);
        for (i = 0; i < numPids; i++) {
            result = Tcl_GetIntFromObj(interp, pidObjs[i], (int *) &pid);
            if (result != TCL_OK) {
                Tcl_MutexUnlock(&infoTablesMutex);
                Tcl_DecrRefCount(dict);
                return result;
            }

            entry = Tcl_FindHashEntry(&infoTablePerResolvedPid, INT2PTR(pid));
            if (!entry) {
                /*
                 * Skip unknown process.
                 */
                
                continue;
            }
            
            info = (ProcessInfo *) Tcl_GetHashValue(entry);
            RefreshProcessInfo(info, options);

            // TODO purge etc.

            Tcl_DictObjPut(interp, dict, Tcl_NewIntObj(info->resolvedPid), 
                    BuildProcessStatusObj(info));
        }
        Tcl_MutexUnlock(&infoTablesMutex);
    }
    Tcl_SetObjResult(interp, dict);
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ProcessPurgeObjCmd --
 *
 *	This function implements the 'tcl::process purge' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.FRED TODO
 *
 *----------------------------------------------------------------------
 */

static int
ProcessPurgeObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    ProcessInfo *info;
    int numPids;
    Tcl_Obj **pidObjs;
    int result;
    int i;
    int pid;

    if (objc != 1 && objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?pids?");
	return TCL_ERROR;
    }

    if (objc == 1) {
        /*
         * Purge all terminated processes.
         */

        Tcl_MutexLock(&infoTablesMutex);
        for (entry = Tcl_FirstHashEntry(&infoTablePerPid, &search); entry != NULL;
                entry = Tcl_NextHashEntry(&search)) {
            info = (ProcessInfo *) Tcl_GetHashValue(entry);
            if (info->status != TCL_PROCESS_UNCHANGED) {
                Tcl_DeleteHashEntry(entry);
                FreeProcessInfo(info, 0);
            }
        }
        Tcl_MutexUnlock(&infoTablesMutex);
    } else {
        /*
         * Purge only provided processes.
         */
        
        result = Tcl_ListObjGetElements(interp, objv[1], &numPids, &pidObjs);
        if (result != TCL_OK) {
            return result;
        }
        Tcl_MutexLock(&infoTablesMutex);
        for (i = 0; i < numPids; i++) {
            result = Tcl_GetIntFromObj(interp, pidObjs[i], (int *) &pid);
            if (result != TCL_OK) {
                Tcl_MutexUnlock(&infoTablesMutex);
                return result;
            }

            entry = Tcl_FindHashEntry(&infoTablePerPid, INT2PTR(pid));
            if (entry) {
                info = (ProcessInfo *) Tcl_GetHashValue(entry);
                if (info->status != TCL_PROCESS_UNCHANGED) {
                    Tcl_DeleteHashEntry(entry);
                    FreeProcessInfo(info, 0);
                }
            }
        }
        Tcl_MutexUnlock(&infoTablesMutex);
    }
    
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * ProcessAutopurgeObjCmd --
 *
 *	This function implements the 'tcl::process autopurge' Tcl command. 
 *      Refer to the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Alters detached process handling by Tcl_ReapDetachedProcs().
 *
 *----------------------------------------------------------------------
 */

static int
ProcessAutopurgeObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc != 1 && objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?flag?");
	return TCL_ERROR;
    }

    if (objc == 2) {
        /*
         * Set given value.
         */
        
        int flag;
        int result = Tcl_GetBooleanFromObj(interp, objv[1], &flag);
        if (result != TCL_OK) {
            return result;
        }

        autopurge = !!flag;
    }

    /* 
     * Return current value. 
     */

    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(autopurge));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitProcessCmd --
 *
 *	This procedure creates the "tcl::process" Tcl command. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclInitProcessCmd(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    static const EnsembleImplMap processImplMap[] = {
        {"list", ProcessListObjCmd, TclCompileBasic0ArgCmd, NULL, NULL, 1},
        {"status", ProcessStatusObjCmd, TclCompileBasicMin0ArgCmd, NULL, NULL, 1},
        {"purge", ProcessPurgeObjCmd, TclCompileBasic0Or1ArgCmd, NULL, NULL, 1},
        {"autopurge", ProcessAutopurgeObjCmd, TclCompileBasic0Or1ArgCmd, NULL, NULL, 1},
        {NULL, NULL, NULL, NULL, NULL, 0}
    };
    Tcl_Command processCmd;

    if (infoTablesInitialized == 0) {
	Tcl_MutexLock(&infoTablesMutex);
	if (infoTablesInitialized == 0) {
	    Tcl_InitHashTable(&infoTablePerPid, TCL_ONE_WORD_KEYS);
	    Tcl_InitHashTable(&infoTablePerResolvedPid, TCL_ONE_WORD_KEYS);
	    infoTablesInitialized = 1;
	}
	Tcl_MutexUnlock(&infoTablesMutex);
    }
    
    processCmd = TclMakeEnsemble(interp, "::tcl::process", processImplMap);
    Tcl_Export(interp, Tcl_FindNamespace(interp, "::tcl", NULL, 0),
            "process", 0);
    return processCmd;
}

/* FRED TODO */
void
TclProcessCreated(
    Tcl_Pid pid)
{
    int resolvedPid;
    Tcl_HashEntry *entry;
    int isNew;
    ProcessInfo *info;

    /*
     * Get resolved pid first.
     */

    resolvedPid = TclpGetPid(pid);
     
    Tcl_MutexLock(&infoTablesMutex);

    /*
     * Create entry in pid table.
     */

    entry = Tcl_CreateHashEntry(&infoTablePerPid, pid, &isNew);
    if (!isNew) {
        /*
         * Pid was reused, free old info and reuse structure.
         */
        
        info = (ProcessInfo *) Tcl_GetHashValue(entry);
        FreeProcessInfo(info, 0);
    }

    /*
     * Allocate and initialize info structure.
     */

    info = (ProcessInfo *) ckalloc(sizeof(ProcessInfo));
    InitProcessInfo(info, pid, resolvedPid);
    
    /*
     * Add entry to tables.
     */

    Tcl_SetHashValue(entry, info);
    entry = Tcl_CreateHashEntry(&infoTablePerResolvedPid, INT2PTR(resolvedPid), &isNew);
    Tcl_SetHashValue(entry, info);
    
    Tcl_MutexUnlock(&infoTablesMutex);
}


/* FRED TODO */
TclProcessWaitStatus
TclProcessWait(
    Tcl_Pid pid,
    int options,
    int *codePtr,
    Tcl_Obj **msgObjPtr,
    Tcl_Obj **errorObjPtr)
{
    Tcl_HashEntry *entry;
    ProcessInfo *info;
    int result;

    /*
     * First search for pid in table.
     */

    entry = Tcl_FindHashEntry(&infoTablePerPid, pid);
    if (!entry) {
        /*
         * Unknown process, just call WaitProcessStatus and return.
         */
        
        return WaitProcessStatus(pid, TclpGetPid(pid), options, codePtr, 
                msgObjPtr, errorObjPtr);
    }

    info = (ProcessInfo *) Tcl_GetHashValue(entry);
    if (info->purge) {
        /*
         * Process has completed but TclProcessWait has already been called,
         * so report no change.
         */
        
        return TCL_PROCESS_UNCHANGED;
    }

    RefreshProcessInfo(info, options);
    if (info->status == TCL_PROCESS_UNCHANGED) {
        /*
         * No change, stop there.
         */
        
        return TCL_PROCESS_UNCHANGED;
    }

    /*
     * Set return values.
     */

    result = info->status;
    if (codePtr) *codePtr = info->code;
    if (msgObjPtr) *msgObjPtr = info->msg;
    if (errorObjPtr) *errorObjPtr = info->error;

    if (autopurge) {
        /*
         * Purge now.
         */

        Tcl_DeleteHashEntry(entry);
        entry = Tcl_FindHashEntry(&infoTablePerResolvedPid, 
                INT2PTR(info->resolvedPid));
        Tcl_DeleteHashEntry(entry);
        FreeProcessInfo(info, 1);
    } else {
        /*
         * Eventually purge. Subsequent calls will return
         * TCL_PROCESS_UNCHANGED.
         */

        info->purge = 1;
    }
    return result;
}
