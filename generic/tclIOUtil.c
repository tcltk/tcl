/* 
 * tclIOUtil.c --
 *
 *	This file contains a collection of utility procedures that
 *	are shared by the platform specific IO drivers.
 *
 *	Parts of this file are based on code contributed by Karl
 *	Lehenbauer, Mark Diekhans and Peter da Silva.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclIOUtil.c 1.133 97/09/24 16:38:57
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following typedef declarations allow for hooking into the chain
 * of functions maintained for 'Tcl_Stat(...)', 'Tcl_Access(...)' &
 * 'Tcl_OpenFileChannel(...)'.  Basically for each hookable function
 * a linked list is defined.
 */

typedef struct StatProc *StatProcPtr;
typedef struct StatProc {
    TclStatProc_ *proc;		/* Function to process a 'stat()' call */
    StatProcPtr next; 		/* The next 'stat()' function to call */
} StatProc;

typedef struct AccessProc *AccessProcPtr;
typedef struct AccessProc {
    TclAccessProc_ *proc;	/* Function to process a 'access()' call */
    AccessProcPtr next; 	/* The next 'access()' function to call */
} AccessProc;

typedef struct OpenFileChannelProc *OpenFileChannelProcPtr;
typedef struct OpenFileChannelProc {
    TclOpenFileChannelProc_ *proc; /* Function to process a
				    * 'Tcl_OpenFileChannel()' call */
    OpenFileChannelProcPtr next;   /* The next 'Tcl_OpenFileChannel()'
				    * function to call */
} OpenFileChannelProc;

/*
 * For each type of hookable function, a static node is declared to
 * hold the function pointer for the "built-in" routine (e.g.
 * 'TclpStat(...)') and the respective list is initialized as a pointer
 * to that node.
 * 
 * The "delete" functions (e.g. 'TclStatDeleteProc(...)') ensure that
 * these statically declared nodes cannot be inadvertently removed.
 *
 * This method avoids the need to call any sort of "initialization"
 * function
 */

static StatProc defaultStatProc = {
    &TclpStat, NULL
};
static StatProcPtr statProcList = &defaultStatProc;

static AccessProc defaultAccessProc = {
    &TclpAccess, NULL
};
static AccessProcPtr accessProcList = &defaultAccessProc;

static OpenFileChannelProc defaultOpenFileChannelProc = {
    &TclpOpenFileChannel, NULL
};
static OpenFileChannelProcPtr openFileChannelProcList =
	&defaultOpenFileChannelProc;

/*
 *----------------------------------------------------------------------
 *
 * TclGetOpenMode --
 *
 * Description:
 *	Computes a POSIX mode mask for opening a file, from a given string,
 *	and also sets a flag to indicate whether the caller should seek to
 *	EOF after opening the file.
 *
 * Results:
 *	On success, returns mode to pass to "open". If an error occurs, the
 *	returns -1 and if interp is not NULL, sets interp->result to an
 *	error message.
 *
 * Side effects:
 *	Sets the integer referenced by seekFlagPtr to 1 to tell the caller
 *	to seek to EOF after opening the file.
 *
 * Special note:
 *	This code is based on a prototype implementation contributed
 *	by Mark Diekhans.
 *
 *----------------------------------------------------------------------
 */

int
TclGetOpenMode(interp, string, seekFlagPtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting - may be NULL. */
    char *string;			/* Mode string, e.g. "r+" or
					 * "RDONLY CREAT". */
    int *seekFlagPtr;			/* Set this to 1 if the caller
                                         * should seek to EOF during the
                                         * opening of the file. */
{
    int mode, modeArgc, c, i, gotRW;
    char **modeArgv, *flag;
#define RW_MODES (O_RDONLY|O_WRONLY|O_RDWR)

    /*
     * Check for the simpler fopen-like access modes (e.g. "r").  They
     * are distinguished from the POSIX access modes by the presence
     * of a lower-case first letter.
     */

    *seekFlagPtr = 0;
    mode = 0;
    if (islower(UCHAR(string[0]))) {
	switch (string[0]) {
	    case 'r':
		mode = O_RDONLY;
		break;
	    case 'w':
		mode = O_WRONLY|O_CREAT|O_TRUNC;
		break;
	    case 'a':
		mode = O_WRONLY|O_CREAT;
                *seekFlagPtr = 1;
		break;
	    default:
		error:
                if (interp != (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp,
                            "illegal access mode \"", string, "\"",
                            (char *) NULL);
                }
		return -1;
	}
	if (string[1] == '+') {
	    mode &= ~(O_RDONLY|O_WRONLY);
	    mode |= O_RDWR;
	    if (string[2] != 0) {
		goto error;
	    }
	} else if (string[1] != 0) {
	    goto error;
	}
        return mode;
    }

    /*
     * The access modes are specified using a list of POSIX modes
     * such as O_CREAT.
     *
     * IMPORTANT NOTE: We rely on Tcl_SplitList working correctly when
     * a NULL interpreter is passed in.
     */

    if (Tcl_SplitList(interp, string, &modeArgc, &modeArgv) != TCL_OK) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AddErrorInfo(interp,
                    "\n    while processing open access modes \"");
            Tcl_AddErrorInfo(interp, string);
            Tcl_AddErrorInfo(interp, "\"");
        }
        return -1;
    }
    
    gotRW = 0;
    for (i = 0; i < modeArgc; i++) {
	flag = modeArgv[i];
	c = flag[0];
	if ((c == 'R') && (strcmp(flag, "RDONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDONLY;
	    gotRW = 1;
	} else if ((c == 'W') && (strcmp(flag, "WRONLY") == 0)) {
	    mode = (mode & ~RW_MODES) | O_WRONLY;
	    gotRW = 1;
	} else if ((c == 'R') && (strcmp(flag, "RDWR") == 0)) {
	    mode = (mode & ~RW_MODES) | O_RDWR;
	    gotRW = 1;
	} else if ((c == 'A') && (strcmp(flag, "APPEND") == 0)) {
	    mode |= O_APPEND;
            *seekFlagPtr = 1;
	} else if ((c == 'C') && (strcmp(flag, "CREAT") == 0)) {
	    mode |= O_CREAT;
	} else if ((c == 'E') && (strcmp(flag, "EXCL") == 0)) {
	    mode |= O_EXCL;
	} else if ((c == 'N') && (strcmp(flag, "NOCTTY") == 0)) {
#ifdef O_NOCTTY
	    mode |= O_NOCTTY;
#else
	    if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'N') && (strcmp(flag, "NONBLOCK") == 0)) {
#if defined(O_NDELAY) || defined(O_NONBLOCK)
#   ifdef O_NONBLOCK
	    mode |= O_NONBLOCK;
#   else
	    mode |= O_NDELAY;
#   endif
#else
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "access mode \"", flag,
                        "\" not supported by this system", (char *) NULL);
            }
            ckfree((char *) modeArgv);
	    return -1;
#endif
	} else if ((c == 'T') && (strcmp(flag, "TRUNC") == 0)) {
	    mode |= O_TRUNC;
	} else {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "invalid access mode \"", flag,
                        "\": must be RDONLY, WRONLY, RDWR, APPEND, CREAT",
                        " EXCL, NOCTTY, NONBLOCK, or TRUNC", (char *) NULL);
            }
	    ckfree((char *) modeArgv);
	    return -1;
	}
    }
    ckfree((char *) modeArgv);
    if (!gotRW) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "access mode must include either",
                    " RDONLY, WRONLY, or RDWR", (char *) NULL);
        }
	return -1;
    }
    return mode;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EvalFile --
 *
 *	Read in a file and process the entire file as one gigantic
 *	Tcl command.
 *
 * Results:
 *	A standard Tcl result, which is either the result of executing
 *	the file or an error indicating why the file couldn't be read.
 *
 * Side effects:
 *	Depends on the commands in the file.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_EvalFile(interp, fileName)
    Tcl_Interp *interp;		/* Interpreter in which to process file. */
    char *fileName;		/* Name of file to process.  Tilde-substitution
				 * will be performed on this name. */
{
    int result;
    struct stat statBuf;
    char *cmdBuffer = (char *) NULL;
    char *oldScriptFile;
    Interp *iPtr = (Interp *) interp;
    Tcl_DString buffer;
    char *nativeName;
    Tcl_Channel chan;
    Tcl_Obj *cmdObjPtr;

    Tcl_ResetResult(interp);
    oldScriptFile = iPtr->scriptFile;
    iPtr->scriptFile = fileName;
    Tcl_DStringInit(&buffer);
    nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (nativeName == NULL) {
	goto error;
    }

    /*
     * If Tcl_TranslateFileName didn't already copy the file name, do it
     * here.  This way we don't depend on fileName staying constant
     * throughout the execution of the script (e.g., what if it happens
     * to point to a Tcl variable that the script could change?).
     */

    if (nativeName != Tcl_DStringValue(&buffer)) {
	Tcl_DStringSetLength(&buffer, 0);
	Tcl_DStringAppend(&buffer, nativeName, -1);
	nativeName = Tcl_DStringValue(&buffer);
    }
    if (TclStat(nativeName, &statBuf) == -1) {
        Tcl_SetErrno(errno);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    chan = Tcl_OpenFileChannel(interp, nativeName, "r", 0644);
    if (chan == (Tcl_Channel) NULL) {
        Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    cmdBuffer = (char *) ckalloc((unsigned) statBuf.st_size+1);
    result = Tcl_Read(chan, cmdBuffer, statBuf.st_size);
    if (result < 0) {
        Tcl_Close(interp, chan);
	Tcl_AppendResult(interp, "couldn't read file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto error;
    }
    cmdBuffer[result] = 0;
    if (Tcl_Close(interp, chan) != TCL_OK) {
        goto error;
    }

    /*
     * Transfer the buffer memory allocated above to the object system.
     * Tcl_EvalObj will own this new string object if needed,
     * so past the Tcl_EvalObj point, we must not ckfree(cmdBuffer)
     * but rather use the reference counting mechanism.
     * (Nb: and we must not thus not use goto error after this point)
     */
    cmdObjPtr = Tcl_NewObj();
    cmdObjPtr->bytes = cmdBuffer;
    cmdObjPtr->length = result;
    
    Tcl_IncrRefCount(cmdObjPtr);
    result = Tcl_EvalObj(interp, cmdObjPtr);
    Tcl_DecrRefCount(cmdObjPtr);

    if (result == TCL_RETURN) {
	result = TclUpdateReturnInfo(iPtr);
    } else if (result == TCL_ERROR) {
	char msg[200];

	/*
	 * Record information telling where the error occurred.
	 */

	sprintf(msg, "\n    (file \"%.150s\" line %d)", fileName,
		interp->errorLine);
	Tcl_AddErrorInfo(interp, msg);
    }
    iPtr->scriptFile = oldScriptFile;
    Tcl_DStringFree(&buffer);
    return result;

error:
    if (cmdBuffer != (char *) NULL) {
        ckfree(cmdBuffer);
    }
    iPtr->scriptFile = oldScriptFile;
    Tcl_DStringFree(&buffer);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetErrno --
 *
 *	Gets the current value of the Tcl error code variable. This is
 *	currently the global variable "errno" but could in the future
 *	change to something else.
 *
 * Results:
 *	The value of the Tcl error code variable.
 *
 * Side effects:
 *	None. Note that the value of the Tcl error code variable is
 *	UNDEFINED if a call to Tcl_SetErrno did not precede this call.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetErrno()
{
    return errno;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetErrno --
 *
 *	Sets the Tcl error code variable to the supplied value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the value of the Tcl error code variable.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetErrno(err)
    int err;			/* The new value. */
{
    errno = err;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PosixError --
 *
 *	This procedure is typically called after UNIX kernel calls
 *	return errors.  It stores machine-readable information about
 *	the error in $errorCode returns an information string for
 *	the caller's use.
 *
 * Results:
 *	The return value is a human-readable string describing the
 *	error.
 *
 * Side effects:
 *	The global variable $errorCode is reset.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_PosixError(interp)
    Tcl_Interp *interp;		/* Interpreter whose $errorCode variable
				 * is to be changed. */
{
    char *id, *msg;

    msg = Tcl_ErrnoMsg(errno);
    id = Tcl_ErrnoId();
    Tcl_SetErrorCode(interp, "POSIX", id, msg, (char *) NULL);
    return msg;
}

/*
 *----------------------------------------------------------------------
 *
 * TclStat --
 *
 *	This procedure replaces the library version of stat and lsat.
 *
 * Results:
 *      See stat documentation.
 *
 * Side effects:
 *      See stat documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclStat(path, buf)
    CONST char *path;		/* Path of file to stat (in current CP). */
    TclStat_ *buf;		/* Filled with results of stat call. */
{
    StatProcPtr statProcPtr = statProcList;
    int retVal = -1;

    /*
     * Call each of the "stat" function in succession.  A non-return
     * value of -1 indicates the particular function has succeeded.
     */

    while ((retVal == -1) && (statProcPtr != NULL)) {
	retVal = (*statProcPtr->proc)(path, buf);
	statProcPtr = statProcPtr->next;
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAccess --
 *
 *	This procedure replaces the library version of access.
 *
 * Results:
 *      See access documentation.
 *
 * Side effects:
 *      See access documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclAccess(path, mode)
    CONST char *path;		/* Path of file to access (in current CP). */
    int mode;                   /* Permission setting. */
{
    AccessProcPtr accessProcPtr = accessProcList;
    int retVal = -1;

    /*
     * Call each of the "access" function in succession.  A non-return
     * value of -1 indicates the particular function has succeeded.
     */

    while ((retVal == -1) && (accessProcPtr != NULL)) {
	retVal = (*accessProcPtr->proc)(path, mode);
	accessProcPtr = accessProcPtr->next;
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenFileChannel --
 *
 *      @@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */
 
Tcl_Channel
Tcl_OpenFileChannel(interp, fileName, modeString, permissions)
    Tcl_Interp *interp;                 /* Interpreter for error reporting;
                                         * can be NULL. */
    char *fileName;                     /* Name of file to open. */
    char *modeString;                   /* A list of POSIX open modes or
                                         * a string such as "rw". */
    int permissions;                    /* If the open involves creating a
                                         * file, with what modes to create
                                         * it? */
{
    OpenFileChannelProcPtr openFileChannelProcPtr = openFileChannelProcList;
    Tcl_Channel retVal = NULL;

    /*
     * Call each of the "Tcl_OpenFileChannel" function in succession.
     * A non-NULL return value indicates the particular function has
     * succeeded.
     */

    while ((retVal == NULL) && (openFileChannelProcPtr != NULL)) {
	retVal = (*openFileChannelProcPtr->proc)(interp, fileName,
		modeString, permissions);
	openFileChannelProcPtr = openFileChannelProcPtr->next;
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclStatInsertProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclStatInsertProc (proc)
    TclStatProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	StatProcPtr newStatProcPtr;

	newStatProcPtr = (StatProcPtr)Tcl_Alloc(sizeof(StatProc));;

	if (newStatProcPtr != NULL) {
	    newStatProcPtr->proc = proc;
	    newStatProcPtr->next = statProcList;
	    statProcList = newStatProcPtr;

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclStatDeleteProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclStatDeleteProc (proc)
    TclStatProc_ *proc;
{
    int retVal = TCL_ERROR;
    StatProcPtr tmpStatProcPtr = statProcList;
    StatProcPtr prevStatProcPtr = NULL;

    /*
     * Traverse the 'statProcList' looking for the particular node
     * whose 'proc' member matches 'proc' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == TCL_ERROR) && (tmpStatProcPtr != &defaultStatProc)) {
	if (tmpStatProcPtr->proc == proc) {
	    if (prevStatProcPtr == NULL) {
		statProcList = tmpStatProcPtr->next;
	    } else {
		prevStatProcPtr->next = tmpStatProcPtr->next;
	    }

	    Tcl_Free((char *)tmpStatProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevStatProcPtr = tmpStatProcPtr;
	    tmpStatProcPtr = tmpStatProcPtr->next;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAccessInsertProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclAccessInsertProc(proc)
    TclAccessProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	AccessProcPtr newAccessProcPtr;

	newAccessProcPtr = (AccessProcPtr)Tcl_Alloc(sizeof(AccessProc));;

	if (newAccessProcPtr != NULL) {
	    newAccessProcPtr->proc = proc;
	    newAccessProcPtr->next = accessProcList;
	    accessProcList = newAccessProcPtr;

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclAccessDeleteProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclAccessDeleteProc(proc)
    TclAccessProc_ *proc;
{
    int retVal = TCL_ERROR;
    AccessProcPtr tmpAccessProcPtr = accessProcList;
    AccessProcPtr prevAccessProcPtr = NULL;

    /*
     * Traverse the 'accessProcList' looking for the particular node
     * whose 'proc' member matches 'proc' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == TCL_ERROR) && (tmpAccessProcPtr != &defaultAccessProc)) {
	if (tmpAccessProcPtr->proc == proc) {
	    if (prevAccessProcPtr == NULL) {
		accessProcList = tmpAccessProcPtr->next;
	    } else {
		prevAccessProcPtr->next = tmpAccessProcPtr->next;
	    }

	    Tcl_Free((char *)tmpAccessProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevAccessProcPtr = tmpAccessProcPtr;
	    tmpAccessProcPtr = tmpAccessProcPtr->next;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFileChannelInsertProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclOpenFileChannelInsertProc(proc)
    TclOpenFileChannelProc_ *proc;
{
    int retVal = TCL_ERROR;

    if (proc != NULL) {
	OpenFileChannelProcPtr newOpenFileChannelProcPtr;

	newOpenFileChannelProcPtr =
		(OpenFileChannelProcPtr)Tcl_Alloc(sizeof(OpenFileChannelProc));;

	if (newOpenFileChannelProcPtr != NULL) {
	    newOpenFileChannelProcPtr->proc = proc;
	    newOpenFileChannelProcPtr->next = openFileChannelProcList;
	    openFileChannelProcList = newOpenFileChannelProcPtr;

	    retVal = TCL_OK;
	}
    }

    return (retVal);
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFileChannelDeleteProc --
 *
 *	@@@
 *
 * Results:
 *      @@@
 *
 * Side effects:
 *      @@@
 *
 *----------------------------------------------------------------------
 */

int
TclOpenFileChannelDeleteProc(proc)
    TclOpenFileChannelProc_ *proc;
{
    int retVal = TCL_ERROR;
    OpenFileChannelProcPtr tmpOpenFileChannelProcPtr = openFileChannelProcList;
    OpenFileChannelProcPtr prevOpenFileChannelProcPtr = NULL;

    /*
     * Traverse the 'openFileChannelProcList' looking for the particular
     * node whose 'proc' member matches 'proc' and remove that one from
     * the list.  Ensure that the "default" node cannot be removed.
     */

    while ((retVal == TCL_ERROR) &&
	    (tmpOpenFileChannelProcPtr != &defaultOpenFileChannelProc)) {
	if (tmpOpenFileChannelProcPtr->proc == proc) {
	    if (prevOpenFileChannelProcPtr == NULL) {
		openFileChannelProcList = tmpOpenFileChannelProcPtr->next;
	    } else {
		prevOpenFileChannelProcPtr->next = tmpOpenFileChannelProcPtr->next;
	    }

	    Tcl_Free((char *)tmpOpenFileChannelProcPtr);

	    retVal = TCL_OK;
	} else {
	    prevOpenFileChannelProcPtr = tmpOpenFileChannelProcPtr;
	    tmpOpenFileChannelProcPtr = tmpOpenFileChannelProcPtr->next;
	}
    }

    return (retVal);
}
