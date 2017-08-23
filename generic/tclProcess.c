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
 * Hash table that keeps track of all child process statuses. Keys are the 
 * child process ids, values are (ProcessInfo *).
 */

typedef struct ProcessInfo {
    Tcl_Pid pid; /*FRED TODO*/
    int resolvedPid; /*FRED TODO unused?*/
    Tcl_Obj *status; /*FRED TODO*/

} ProcessInfo;
static Tcl_HashTable statusTable;
static int statusTableInitialized = 0;	/* 0 means not yet initialized. */
TCL_DECLARE_MUTEX(statusMutex)

 /*
 * Prototypes for functions defined later in this file:
 */

static int              GetProcessStatus(Tcl_Pid pid, int resolvedPid, 
                            int options, Tcl_Obj **statusPtr);
static int              PurgeProcessStatus(Tcl_HashEntry *entry);
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
    Tcl_MutexLock(&statusMutex);
    for (entry = Tcl_FirstHashEntry(&statusTable, &search); entry != NULL;
	    entry = Tcl_NextHashEntry(&search)) {
        info = (ProcessInfo *) Tcl_GetHashValue(entry);
        Tcl_ListObjAppendElement(interp, list, 
                Tcl_NewIntObj(info->resolvedPid));
    }
    Tcl_MutexUnlock(&statusMutex);
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
        Tcl_MutexLock(&statusMutex);
        for (entry = Tcl_FirstHashEntry(&statusTable, &search); entry != NULL;
                entry = Tcl_NextHashEntry(&search)) {
            info = (ProcessInfo *) Tcl_GetHashValue(entry);
            if (autopurge) {
                if (GetProcessStatus(info->pid, info->resolvedPid, options, 
                        NULL)) {
                   /*
                    * Purge.
                    */

                    PurgeProcessStatus(entry);
                    Tcl_DeleteHashEntry(entry);
                    continue;
                }
            } else if (!info->status) {
                /*
                * Update status.
                */

                if (GetProcessStatus(info->pid, info->resolvedPid, options, 
                        &info->status)) {
                    Tcl_IncrRefCount(info->status);
                }
           }
           Tcl_DictObjPut(interp, dict, Tcl_NewIntObj(info->resolvedPid), 
                    info->status ? info->status : Tcl_NewObj());
        }
        Tcl_MutexUnlock(&statusMutex);
    } else {
        /*
         * Only return statuses of provided processes.
         */
        
        result = Tcl_ListObjGetElements(interp, objv[1], &numPids, &pidObjs);
        if (result != TCL_OK) {
            return result;
        }
        dict = Tcl_NewDictObj();
        Tcl_MutexLock(&statusMutex);
        for (i = 0; i < numPids; i++) {
            result = Tcl_GetIntFromObj(interp, pidObjs[i], (int *) &pid);
            if (result != TCL_OK) {
                Tcl_MutexUnlock(&statusMutex);
                Tcl_DecrRefCount(dict);
                return result;
            }

            entry = Tcl_FindHashEntry(&statusTable, INT2PTR(pid));
            if (!entry) {
                /*
                 * Skip unknown process.
                 */
                
                continue;
            }
            
            info = (ProcessInfo *) Tcl_GetHashValue(entry);
            if (autopurge) {
                if (GetProcessStatus(info->pid, info->resolvedPid, options, 
                        NULL)) {
                   /*
                    * Purge.
                    */

                    PurgeProcessStatus(entry);
                    Tcl_DeleteHashEntry(entry);
                    continue;
                }
            } else if (!info->status) {
                /*
                * Update status.
                */

                if (GetProcessStatus(info->pid, info->resolvedPid, options, 
                        &info->status)) {
                    Tcl_IncrRefCount(info->status);
                }
           }
           Tcl_DictObjPut(interp, dict, Tcl_NewIntObj(info->resolvedPid), 
                    info->status ? info->status : Tcl_NewObj());
        }
        Tcl_MutexUnlock(&statusMutex);
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

        Tcl_MutexLock(&statusMutex);
        for (entry = Tcl_FirstHashEntry(&statusTable, &search); entry != NULL;
                entry = Tcl_NextHashEntry(&search)) {
            if (PurgeProcessStatus(entry)) {
                Tcl_DeleteHashEntry(entry);
            }
        }
        Tcl_MutexUnlock(&statusMutex);
    } else {
        /*
         * Purge only provided processes.
         */
        
        result = Tcl_ListObjGetElements(interp, objv[1], &numPids, &pidObjs);
        if (result != TCL_OK) {
            return result;
        }
        Tcl_MutexLock(&statusMutex);
        for (i = 0; i < numPids; i++) {
            result = Tcl_GetIntFromObj(interp, pidObjs[i], (int *) &pid);
            if (result != TCL_OK) {
                Tcl_MutexUnlock(&statusMutex);
                return result;
            }

            entry = Tcl_FindHashEntry(&statusTable, INT2PTR(pid));
            if (entry && PurgeProcessStatus(entry)) {
                Tcl_DeleteHashEntry(entry);
            }
        }
        Tcl_MutexUnlock(&statusMutex);
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

    if (statusTableInitialized == 0) {
	Tcl_MutexLock(&statusMutex);
	if (statusTableInitialized == 0) {
	    Tcl_InitHashTable(&statusTable, TCL_ONE_WORD_KEYS);
	    statusTableInitialized = 1;
	}
	Tcl_MutexUnlock(&statusMutex);
    }
    
    processCmd = TclMakeEnsemble(interp, "::tcl::process", processImplMap);
    Tcl_Export(interp, Tcl_FindNamespace(interp, "::tcl", NULL, 0),
            "process", 0);
    return processCmd;
}

/* FRED TODO */
void
TclProcessDetach(
    Tcl_Pid pid)
{
    int resolvedPid;
    Tcl_HashEntry *entry;
    int isNew;
    ProcessInfo *info;

    resolvedPid = TclpGetPid(pid);
    Tcl_MutexLock(&statusMutex);
    entry = Tcl_CreateHashEntry(&statusTable, INT2PTR(resolvedPid), &isNew);
    if (!isNew) {
        /*
         * Pid was reused, free old status and reuse structure.
         */
        
        info = (ProcessInfo *) Tcl_GetHashValue(entry);
        if (info->status) {
            Tcl_DecrRefCount(info->status);
        }
    } else {
        /*
         * Allocate new info structure.
         */

        info = (ProcessInfo *) ckalloc(sizeof(ProcessInfo));
        Tcl_SetHashValue(entry, info);
    }
    
    /*
     * Initialize with an empty status.
     */

    info->pid = pid;
    info->resolvedPid = resolvedPid;
    info->status = NULL;

    Tcl_MutexUnlock(&statusMutex);
}

/* FRED TODO */
int
TclProcessStatus(
    Tcl_Pid pid,
    int options)
{
    int resolvedPid;
    Tcl_HashEntry *entry;
    ProcessInfo *info;
    Tcl_Obj *status;
    int isNew;
    
    /*
     * We need to get the resolved pid before we wait on it as the windows
     * implementation of Tcl_WaitPid deletes the information such that any
     * following calls to TclpGetPid fail.
     */

    resolvedPid = TclpGetPid(pid);

    if (!GetProcessStatus(pid, resolvedPid, options, 
            (autopurge ? NULL /* unused */: &status))) {
        /*
         * Process still alive, or non child-related error.
         */
        
        return 0;
    }
    
    if (autopurge) {
        /*
        * Child terminated, purge.
        */

        Tcl_MutexLock(&statusMutex);
        entry = Tcl_FindHashEntry(&statusTable, INT2PTR(resolvedPid));
        if (entry) {
            PurgeProcessStatus(entry);
            Tcl_DeleteHashEntry(entry);
        }
        Tcl_MutexUnlock(&statusMutex);

        return 1;
    }

    /*
     * Store process status.
     */

    Tcl_MutexLock(&statusMutex);
    entry = Tcl_CreateHashEntry(&statusTable, INT2PTR(resolvedPid), &isNew);
    if (!isNew) {
        info = (ProcessInfo *) Tcl_GetHashValue(entry);
        if (info->status) {
            /*
             * Free old status object.
             */
        
            Tcl_DecrRefCount(info->status);
        }
    } else {
        /*
         * Allocate new info structure.
         */

        info = (ProcessInfo *) ckalloc(sizeof(ProcessInfo));
        info->pid = pid;
        info->resolvedPid = resolvedPid;
        Tcl_SetHashValue(entry, info);
    }

    info->status = status;
    Tcl_IncrRefCount(status);
    Tcl_MutexUnlock(&statusMutex);
    
    return 1;
}

/* FRED TODO */
int
GetProcessStatus(
    Tcl_Pid pid,
    int resolvedPid,
    int options,
    Tcl_Obj **statusPtr)
{
    int waitStatus;
    Tcl_Obj *statusCodes[5];
    const char *msg;

    pid = Tcl_WaitPid(pid, &waitStatus, options);
    if ((pid == 0) || ((pid == (Tcl_Pid) -1) && (errno != ECHILD))) {
        /*
         * Process still alive, or non child-related error.
         */
        
        return 0;
    }

    if (!statusPtr) {
        return 1;
    }

    /*
     * Get process status.
     */

    if (pid == (Tcl_Pid) -1) {
        /*
         * POSIX errName msg
         */

        statusCodes[0] = Tcl_NewStringObj("POSIX", -1);
        statusCodes[1] = Tcl_NewStringObj(Tcl_ErrnoId(), -1);
        msg = Tcl_ErrnoMsg(errno);
        if (errno == ECHILD) {
            /*
             * This changeup in message suggested by Mark Diekhans to
             * remind people that ECHILD errors can occur on some
             * systems if SIGCHLD isn't in its default state.
             */

            msg = "child process lost (is SIGCHLD ignored or trapped?)";
        }
        statusCodes[2] = Tcl_NewStringObj(msg, -1);
        *statusPtr = Tcl_NewListObj(3, statusCodes);
    } else if (WIFEXITED(waitStatus)) {
        /*
         * CHILDSTATUS pid code
         *
         * Child exited with a non-zero exit status.
         */

        statusCodes[0] = Tcl_NewStringObj("CHILDSTATUS", -1);
        statusCodes[1] = Tcl_NewIntObj(resolvedPid);
        statusCodes[2] = Tcl_NewIntObj(WEXITSTATUS(waitStatus));
        *statusPtr = Tcl_NewListObj(3, statusCodes);
    } else if (WIFSIGNALED(waitStatus)) {
        /*
         * CHILDKILLED pid sigName msg
         *
         * Child killed because of a signal
         */

        statusCodes[0] = Tcl_NewStringObj("CHILDKILLED", -1);
        statusCodes[1] = Tcl_NewIntObj(resolvedPid);
        statusCodes[2] = Tcl_NewStringObj(Tcl_SignalId(WTERMSIG(waitStatus)), -1);
        statusCodes[3] = Tcl_NewStringObj(Tcl_SignalMsg(WTERMSIG(waitStatus)), -1);
        *statusPtr = Tcl_NewListObj(4, statusCodes);
    } else if (WIFSTOPPED(waitStatus)) {
        /*
         * CHILDSUSP pid sigName msg
         *
         * Child suspended because of a signal
         */

        statusCodes[0] = Tcl_NewStringObj("CHILDSUSP", -1);
        statusCodes[1] = Tcl_NewIntObj(resolvedPid);
        statusCodes[2] = Tcl_NewStringObj(Tcl_SignalId(WSTOPSIG(waitStatus)), -1);
        statusCodes[3] = Tcl_NewStringObj(Tcl_SignalMsg(WSTOPSIG(waitStatus)), -1);
        *statusPtr = Tcl_NewListObj(4, statusCodes);
    } else {
        /*
         * TCL OPERATION EXEC ODDWAITRESULT
         *
         * Child wait status didn't make sense.
         */

        statusCodes[0] = Tcl_NewStringObj("TCL", -1);
        statusCodes[1] = Tcl_NewStringObj("OPERATION", -1);
        statusCodes[2] = Tcl_NewStringObj("EXEC", -1);
        statusCodes[3] = Tcl_NewStringObj("ODDWAITRESULT", -1);
        statusCodes[4] = Tcl_NewIntObj(resolvedPid);
        *statusPtr = Tcl_NewListObj(5, statusCodes);
    }

    return 1;
}

/* FRED TODO */
int
PurgeProcessStatus(
    Tcl_HashEntry *entry)
{
    ProcessInfo *info;

    info = (ProcessInfo *) Tcl_GetHashValue(entry);
    if (info->status) {
        /*
         * Process has ended, purge.
         */

        Tcl_DecrRefCount(info->status);
        return 1;
    }

    return 0;
}
