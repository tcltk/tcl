/*
 * tclUnixPipe.c --
 *
 *	This file implements the UNIX-specific exec pipeline functions, the
 *	"pipe" channel driver, and the "pid" Tcl command.
 *
 * Copyright © 1991-1994 The Regents of the University of California.
 * Copyright © 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#ifdef HAVE_POSIX_SPAWNP
#   if defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDDUP2) \
	    && defined(HAVE_POSIX_SPAWNATTR_SETFLAGS) \
	    && !defined(HAVE_VFORK)
#	include <unistd.h>
#	include <spawn.h>
#   else
#	undef HAVE_POSIX_SPAWNP
#   endif
#endif

#ifdef HAVE_VFORK
#define fork vfork
#endif

/*
 * The following macros convert between TclFile's and fd's. The conversion
 * simple involves shifting fd's up by one to ensure that no valid fd is ever
 * the same as NULL.
 */

#define MakeFile(fd)	((TclFile)INT2PTR((fd) + 1))
#define GetFd(file)	((int)PTR2INT(file) - 1)

/*
 * This structure describes per-instance state of a pipe based channel.
 */

typedef struct {
    Tcl_Channel channel;	/* Channel associated with this file. */
    TclFile inFile;		/* Output from pipe. */
    TclFile outFile;		/* Input to pipe. */
    TclFile errorFile;		/* Error output from pipe. */
    size_t numPids;		/* How many processes are attached to this
				 * pipe? */
    Tcl_Pid *pidPtr;		/* The process IDs themselves. Allocated by
				 * the creator of the pipe. */
    int isNonBlocking;		/* Nonzero when the pipe is in nonblocking
				 * mode. Used to decide whether to wait for
				 * the children at close time. */
} PipeState;

/*
 * Declarations for local functions defined in this file:
 */

static int		PipeBlockModeProc(void *instanceData, int mode);
static int		PipeClose2Proc(void *instanceData,
			    Tcl_Interp *interp, int flags);
static int		PipeGetHandleProc(void *instanceData,
			    int direction, void **handlePtr);
static int		PipeInputProc(void *instanceData, char *buf,
			    int toRead, int *errorCode);
static int		PipeOutputProc(void *instanceData,
			    const char *buf, int toWrite, int *errorCode);
static void		PipeWatchProc(void *instanceData, int mask);
static void		RestoreSignals(void);
static int		SetupStdFile(TclFile file, int type);

/*
 * This structure describes the channel type structure for command pipe based
 * I/O:
 */

static const Tcl_ChannelType pipeChannelType = {
    "pipe",
    TCL_CHANNEL_VERSION_5,
    NULL,			/* Deprecated. */
    PipeInputProc,
    PipeOutputProc,
    NULL,			/* Deprecated. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
    PipeWatchProc,
    PipeGetHandleProc,
    PipeClose2Proc,
    PipeBlockModeProc,
    NULL,			/* Flush proc. */
    NULL,			/* Bubbled event handler proc. */
    NULL,			/* Seek proc. */
    NULL,			/* Thread action proc. */
    NULL			/* Truncation proc. */
};

/*
 *----------------------------------------------------------------------
 *
 * TclpMakeFile --
 *
 *	Make a TclFile from a channel.
 *
 * Results:
 *	Returns a new TclFile or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpMakeFile(
    Tcl_Channel channel,	/* Channel to get file from. */
    int direction)		/* Either TCL_READABLE or TCL_WRITABLE. */
{
    void *data;

    if (Tcl_GetChannelHandle(channel, direction, &data) != TCL_OK) {
	return NULL;
    }

    return MakeFile(PTR2INT(data));
}

/*
 *----------------------------------------------------------------------
 *
 * TclpOpenFile --
 *
 *	Open a file for use in a pipeline.
 *
 * Results:
 *	Returns a new TclFile handle or NULL on failure.
 *
 * Side effects:
 *	May cause a file to be created on the file system.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpOpenFile(
    const char *fname,		/* The name of the file to open. */
    int mode)			/* In what mode to open the file? */
{
    int fd;
    const char *native;
    Tcl_DString ds;

    if (Tcl_UtfToExternalDStringEx(NULL, NULL, fname, TCL_INDEX_NONE, 0, &ds, NULL) != TCL_OK) {
	Tcl_DStringFree(&ds);
	return NULL;
    }
    native = Tcl_DStringValue(&ds);
    fd = TclOSopen(native, mode, 0666);			/* INTL: Native. */
    Tcl_DStringFree(&ds);
    if (fd != -1) {
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	/*
	 * If the file is being opened for writing, seek to the end so we can
	 * append to any data already in the file.
	 */

	if ((mode & O_WRONLY) && !(mode & O_APPEND)) {
	    TclOSseek(fd, 0, SEEK_END);
	}

	/*
	 * Increment the fd so it can't be 0, which would conflict with the
	 * NULL return for errors.
	 */

	return MakeFile(fd);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateTempFile --
 *
 *	This function creates a temporary file initialized with an optional
 *	string, and returns a file handle with the file pointer at the
 *	beginning of the file.
 *
 * Results:
 *	A handle to a file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpCreateTempFile(
    const char *contents)	/* String to write into temp file, or NULL. */
{
    int fd = TclUnixOpenTemporaryFile(NULL, NULL, NULL, NULL);

    if (fd == -1) {
	return NULL;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (contents != NULL) {
	Tcl_DString dstring;
	char *native;

	if (Tcl_UtfToExternalDStringEx(NULL, NULL, contents, TCL_INDEX_NONE,
		0, &dstring, NULL) != TCL_OK) {
	    close(fd);
	    Tcl_DStringFree(&dstring);
	    return NULL;
	}
	native = Tcl_DStringValue(&dstring);
	if (write(fd, native, Tcl_DStringLength(&dstring)) == -1) {
	    close(fd);
	    Tcl_DStringFree(&dstring);
	    return NULL;
	}
	Tcl_DStringFree(&dstring);
	TclOSseek(fd, 0, SEEK_SET);
    }
    return MakeFile(fd);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpTempFileName --
 *
 *	This function returns unique filename.
 *
 * Results:
 *	Returns a valid Tcl_Obj* with refCount 0, or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
TclpTempFileName(void)
{
    Tcl_Obj *retVal, *nameObj;
    int fd;

    TclNewObj(nameObj);
    Tcl_IncrRefCount(nameObj);
    fd = TclUnixOpenTemporaryFile(NULL, NULL, NULL, nameObj);
    if (fd == -1) {
	Tcl_DecrRefCount(nameObj);
	return NULL;
    }

    fcntl(fd, F_SETFD, FD_CLOEXEC);
    TclpObjDeleteFile(nameObj);
    close(fd);
    retVal = Tcl_DuplicateObj(nameObj);
    Tcl_DecrRefCount(nameObj);
    return retVal;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclpTempFileNameForLibrary --
 *
 *	Constructs a file name in the native file system where a dynamically
 *	loaded library may be placed.
 *
 * Results:
 *	Returns the constructed file name. If an error occurs, returns NULL
 *	and leaves an error message in the interpreter result.
 *
 * On Unix, it works to load a shared object from a file of any name, so this
 * function is merely a thin wrapper around TclpTempFileName().
 *
 *----------------------------------------------------------------------------
 */

Tcl_Obj *
TclpTempFileNameForLibrary(
    Tcl_Interp *interp,		/* Tcl interpreter. */
    TCL_UNUSED(Tcl_Obj *) /*path*/)
{
    Tcl_Obj *retval = TclpTempFileName();

    if (retval == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"couldn't create temporary file: %s",
		Tcl_PosixError(interp)));
    }
    return retval;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreatePipe --
 *
 *	Creates a pipe - simply calls the pipe() function.
 *
 * Results:
 *	Returns 1 on success, 0 on failure.
 *
 * Side effects:
 *	Creates a pipe.
 *
 *----------------------------------------------------------------------
 */

int
TclpCreatePipe(
    TclFile *readPipe,		/* Location to store file handle for read side
				 * of pipe. */
    TclFile *writePipe)		/* Location to store file handle for write
				 * side of pipe. */
{
    int pipeIds[2];

    if (pipe(pipeIds) != 0) {
	return 0;
    }

    fcntl(pipeIds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeIds[1], F_SETFD, FD_CLOEXEC);

    *readPipe = MakeFile(pipeIds[0]);
    *writePipe = MakeFile(pipeIds[1]);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCloseFile --
 *
 *	Implements a mechanism to close a UNIX file.
 *
 * Results:
 *	Returns 0 on success, or -1 on error, setting errno.
 *
 * Side effects:
 *	The file is closed.
 *
 *----------------------------------------------------------------------
 */

int
TclpCloseFile(
    TclFile file)	/* The file to close. */
{
    int fd = GetFd(file);

    /*
     * Refuse to close the fds for stdin, stdout and stderr.
     */

    if ((fd == 0) || (fd == 1) || (fd == 2)) {
	return 0;
    }

    Tcl_DeleteFileHandler(fd);
    return close(fd);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpCreateProcess --
 *
 *	Create a child process that has the specified files as its standard
 *	input, output, and error. The child process runs asynchronously and
 *	runs with the same environment variables as the creating process.
 *
 *	The path is searched to find the specified executable.
 *
 * Results:
 *	The return value is TCL_ERROR and an error message is left in the
 *	interp's result if there was a problem creating the child process.
 *	Otherwise, the return value is TCL_OK and *pidPtr is filled with the
 *	process id of the child process.
 *
 * Side effects:
 *	A process is created.
 *
 *---------------------------------------------------------------------------
 */

int
TclpCreateProcess(
    Tcl_Interp *interp,		/* Interpreter in which to leave errors that
				 * occurred when creating the child process.
				 * Error messages from the child process
				 * itself are sent to errorFile. */
    size_t argc,		/* Number of arguments in following array. */
    const char **argv,		/* Array of argument strings in UTF-8.
				 * argv[0] contains the name of the executable
				 * translated using Tcl_TranslateFileName
				 * call). Additional arguments have not been
				 * converted. */
    TclFile inputFile,		/* If non-NULL, gives the file to use as input
				 * for the child process. If inputFile file is
				 * not readable or is NULL, the child will
				 * receive no standard input. */
    TclFile outputFile,		/* If non-NULL, gives the file that receives
				 * output from the child process. If
				 * outputFile file is not writable or is
				 * NULL, output from the child will be
				 * discarded. */
    TclFile errorFile,		/* If non-NULL, gives the file that receives
				 * errors from the child process. If errorFile
				 * file is not writable or is NULL, errors
				 * from the child will be discarded. errorFile
				 * may be the same as outputFile. */
    Tcl_Pid *pidPtr)		/* If this function is successful, pidPtr is
				 * filled with the process id of the child
				 * process. */
{
    TclFile errPipeIn, errPipeOut;
    ssize_t count;
    int status, fd;
    char errSpace[200 + TCL_INTEGER_SPACE];
    Tcl_DString *volatile dsArray;
    char **volatile newArgv;
    int pid;
    size_t i;
#if defined(HAVE_POSIX_SPAWNP)
    int childErrno;
    static int use_spawn = -1;
#endif

    errPipeIn = NULL;
    errPipeOut = NULL;
    pid = -1;

    /*
     * Create a pipe that the child can use to return error information if
     * anything goes wrong.
     */

    if (TclpCreatePipe(&errPipeIn, &errPipeOut) == 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"couldn't create pipe: %s", Tcl_PosixError(interp)));
	goto error;
    }

    /*
     * We need to allocate and convert this before the fork so it is properly
     * deallocated later
     */

    dsArray = (Tcl_DString *)TclStackAlloc(interp, argc * sizeof(Tcl_DString));
    newArgv = (char **)TclStackAlloc(interp, (argc+1) * sizeof(char *));
    newArgv[argc] = NULL;
    for (i = 0; i < argc; i++) {
	if (Tcl_UtfToExternalDStringEx(interp, NULL, argv[i], TCL_INDEX_NONE,
		0, &dsArray[i], NULL) != TCL_OK) {
	    while (i-- > 0) {
		Tcl_DStringFree(&dsArray[i]);
	    }
	    TclStackFree(interp, newArgv);
	    TclStackFree(interp, dsArray);
	    goto error;
	}
	newArgv[i] = Tcl_DStringValue(&dsArray[i]);
    }

#if defined(HAVE_VFORK) || defined(HAVE_POSIX_SPAWNP)
    /*
     * After vfork(), do not call code in the child that changes global state,
     * because it is using the parent's memory space at that point and writes
     * might corrupt the parent: so ensure standard channels are initialized
     * in the parent, otherwise SetupStdFile() might initialize them in the
     * child.
     */

    if (!inputFile) {
	Tcl_GetStdChannel(TCL_STDIN);
    }
    if (!outputFile) {
	Tcl_GetStdChannel(TCL_STDOUT);
    }
    if (!errorFile) {
	Tcl_GetStdChannel(TCL_STDERR);
    }
#endif

#ifdef HAVE_POSIX_SPAWNP
#ifdef _CS_GNU_LIBC_VERSION
    if (use_spawn < 0) {
	char conf[32], *p;
	int major = 0, minor = 0;

	use_spawn = 0;
	memset(conf, 0, sizeof(conf));
	confstr(_CS_GNU_LIBC_VERSION, conf, sizeof(conf));
	p = strchr(conf, ' ');	/* skip "glibc" */
	if (p != NULL) {
	    ++p;
	    if (sscanf(p, "%d.%d", &major, &minor) > 1) {
		if ((major > 2) || ((major == 2) && (minor >= 24))) {
		    use_spawn = 1;
		}
	    }
	}
    }
#endif
    status = -1;
    if (use_spawn) {
	posix_spawn_file_actions_t actions;
	posix_spawnattr_t attr;
	sigset_t sigs;

	posix_spawn_file_actions_init(&actions);
	posix_spawnattr_init(&attr);
	sigfillset(&sigs);
	sigdelset(&sigs, SIGKILL);
	sigdelset(&sigs, SIGSTOP);

	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF
#ifdef POSIX_SPAWN_USEVFORK
		| POSIX_SPAWN_USEVFORK
#endif
		);
	posix_spawnattr_setsigdefault(&attr, &sigs);

	posix_spawn_file_actions_adddup2(&actions, GetFd(inputFile), 0);
	posix_spawn_file_actions_adddup2(&actions, GetFd(outputFile), 1);
	posix_spawn_file_actions_adddup2(&actions, GetFd(errorFile), 2);

	status = posix_spawnp(&pid, newArgv[0], &actions, &attr,
		newArgv, environ);
	childErrno = errno;
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attr);

	/*
	 * Fork semantics:
	 *  - pid == 0: child process
	 *  - pid == -1: error
	 *  - pid > 0: parent process
	 *
	 * Mimic fork semantics to minimize changes below,
	 * but retry with fork() as last ressort.
	 */
    }
    if (status != 0) {
	pid = fork();
	childErrno = errno;
    }
#else
    pid = fork();
#endif
    if (pid == 0) {
	size_t len;
	int joinThisError = errorFile && (errorFile == outputFile);

	fd = GetFd(errPipeOut);

	/*
	 * Set up stdio file handles for the child process.
	 */

	if (!SetupStdFile(inputFile, TCL_STDIN)
		|| !SetupStdFile(outputFile, TCL_STDOUT)
		|| (!joinThisError && !SetupStdFile(errorFile, TCL_STDERR))
		|| (joinThisError &&
			((dup2(1,2) == -1) || (fcntl(2, F_SETFD, 0) != 0)))) {
	    snprintf(errSpace, sizeof(errSpace),
		    "%dforked process couldn't set up input/output", errno);
	    len = strlen(errSpace);
	    if (len != (size_t) write(fd, errSpace, len)) {
		    Tcl_Panic("TclpCreateProcess: unable to write to errPipeOut");
	    }
	    _exit(1);
	}

	/*
	 * Close the input side of the error pipe.
	 */

	RestoreSignals();
	execvp(newArgv[0], newArgv);			/* INTL: Native. */
	snprintf(errSpace, sizeof(errSpace), "%dcouldn't execute \"%.150s\"", errno, argv[0]);
	len = strlen(errSpace);
	if (len != (size_t) write(fd, errSpace, len)) {
	    Tcl_Panic("TclpCreateProcess: unable to write to errPipeOut");
	}
	_exit(1);
    }

    /*
     * Free the mem we used for the fork
     */

    for (i = 0; i < argc; i++) {
	Tcl_DStringFree(&dsArray[i]);
    }
    TclStackFree(interp, newArgv);
    TclStackFree(interp, dsArray);

    if (pid == -1) {
#ifdef HAVE_POSIX_SPAWNP
	errno = childErrno;
#endif
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"couldn't fork child process: %s", Tcl_PosixError(interp)));
	goto error;
    }

    /*
     * Read back from the error pipe to see if the child started up OK. The
     * info in the pipe (if any) consists of a decimal errno value followed by
     * an error message.
     */

    TclpCloseFile(errPipeOut);
    errPipeOut = NULL;

    fd = GetFd(errPipeIn);
    count = read(fd, errSpace, sizeof(errSpace) - 1);
    if (count > 0) {
	char *end;

	errSpace[count] = 0;
	errno = (int)strtol(errSpace, &end, 10);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s: %s",
		end, Tcl_PosixError(interp)));
	goto error;
    }

    TclpCloseFile(errPipeIn);
    *pidPtr = (Tcl_Pid)INT2PTR(pid);
    return TCL_OK;

  error:
    if (pid != -1) {
	/*
	 * Reap the child process now if an error occurred during its startup.
	 * We don't call this with WNOHANG because that can lead to defunct
	 * processes on an MP system. We shouldn't have to worry about hanging
	 * here, since this is the error case. [Bug: 6148]
	 */

	Tcl_WaitPid((Tcl_Pid)INT2PTR(pid), &status, 0);
    }

    if (errPipeIn) {
	TclpCloseFile(errPipeIn);
    }
    if (errPipeOut) {
	TclpCloseFile(errPipeOut);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * RestoreSignals --
 *
 *	This function is invoked in a forked child process just before
 *	exec-ing a new program to restore all signals to their default
 *	settings.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Signal settings get changed.
 *
 *----------------------------------------------------------------------
 */

static void
RestoreSignals(void)
{
#ifdef SIGABRT
    signal(SIGABRT, SIG_DFL);
#endif
#ifdef SIGALRM
    signal(SIGALRM, SIG_DFL);
#endif
#ifdef SIGFPE
    signal(SIGFPE, SIG_DFL);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_DFL);
#endif
#ifdef SIGILL
    signal(SIGILL, SIG_DFL);
#endif
#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_DFL);
#endif
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif
#ifdef SIGSEGV
    signal(SIGSEGV, SIG_DFL);
#endif
#ifdef SIGTERM
    signal(SIGTERM, SIG_DFL);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, SIG_DFL);
#endif
#ifdef SIGCHLD
    signal(SIGCHLD, SIG_DFL);
#endif
#ifdef SIGCONT
    signal(SIGCONT, SIG_DFL);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGTTIN
    signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTTOU
    signal(SIGTTOU, SIG_DFL);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * SetupStdFile --
 *
 *	Set up stdio file handles for the child process, using the current
 *	standard channels if no other files are specified. If no standard
 *	channel is defined, or if no file is associated with the channel, then
 *	the corresponding standard fd is closed.
 *
 * Results:
 *	Returns 1 on success, or 0 on failure.
 *
 * Side effects:
 *	Replaces stdio fds.
 *
 *----------------------------------------------------------------------
 */

static int
SetupStdFile(
    TclFile file,		/* File to dup, or NULL. */
    int type)			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR */
{
    Tcl_Channel channel;
    int fd;
    int targetFd = 0;		/* Initializations here needed only to */
    int direction = 0;		/* prevent warnings about using uninitialized
				 * variables. */

    switch (type) {
    case TCL_STDIN:
	targetFd = 0;
	direction = TCL_READABLE;
	break;
    case TCL_STDOUT:
	targetFd = 1;
	direction = TCL_WRITABLE;
	break;
    case TCL_STDERR:
	targetFd = 2;
	direction = TCL_WRITABLE;
	break;
    }

    if (!file) {
	channel = Tcl_GetStdChannel(type);
	if (channel) {
	    file = TclpMakeFile(channel, direction);
	}
    }
    if (file) {
	fd = GetFd(file);
	if (fd != targetFd) {
	    if (dup2(fd, targetFd) == -1) {
		return 0;
	    }

	    /*
	     * Must clear the close-on-exec flag for the target FD, since some
	     * systems (e.g. Ultrix) do not clear the CLOEXEC flag on the
	     * target FD.
	     */

	    fcntl(targetFd, F_SETFD, 0);
	} else {
	    /*
	     * Since we aren't dup'ing the file, we need to explicitly clear
	     * the close-on-exec flag.
	     */

	    fcntl(fd, F_SETFD, 0);
	}
    } else {
	close(targetFd);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateCommandChannel --
 *
 *	This function is called by the generic IO level to perform the
 *	platform specific channel initialization for a command channel.
 *
 * Results:
 *	Returns a new channel or NULL on failure.
 *
 * Side effects:
 *	Allocates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpCreateCommandChannel(
    TclFile readFile,		/* If non-null, gives the file for reading. */
    TclFile writeFile,		/* If non-null, gives the file for writing. */
    TclFile errorFile,		/* If non-null, gives the file where errors
				 * can be read. */
    size_t numPids,		/* The number of pids in the pid array. */
    Tcl_Pid *pidPtr)		/* An array of process identifiers. Allocated
				 * by the caller, freed when the channel is
				 * closed or the processes are detached (in a
				 * background exec). */
{
    char channelName[16 + TCL_INTEGER_SPACE];
    int fd;
    PipeState *statePtr = (PipeState *)Tcl_Alloc(sizeof(PipeState));
    int mode;

    statePtr->inFile = readFile;
    statePtr->outFile = writeFile;
    statePtr->errorFile = errorFile;
    statePtr->numPids = numPids;
    statePtr->pidPtr = pidPtr;
    statePtr->isNonBlocking = 0;

    mode = 0;
    if (readFile) {
	mode |= TCL_READABLE;
    }
    if (writeFile) {
	mode |= TCL_WRITABLE;
    }

    /*
     * Use one of the fds associated with the channel as the channel id.
     */

    if (readFile) {
	fd = GetFd(readFile);
    } else if (writeFile) {
	fd = GetFd(writeFile);
    } else if (errorFile) {
	fd = GetFd(errorFile);
    } else {
	fd = 0;
    }

    /*
     * For backward compatibility with previous versions of Tcl, we use
     * "file%d" as the base name for pipes even though it would be more
     * natural to use "pipe%d".
     */

    snprintf(channelName, sizeof(channelName), "file%d", fd);
    statePtr->channel = Tcl_CreateChannel(&pipeChannelType, channelName,
	    statePtr, mode);
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreatePipe --
 *
 *	System dependent interface to create a pipe for the [chan pipe]
 *	command. Stolen from TclX.
 *
 * Results:
 *	TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *	Registers two channels.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreatePipe(
    Tcl_Interp *interp,		/* Errors returned in result. */
    Tcl_Channel *rchan,		/* Returned read side. */
    Tcl_Channel *wchan,		/* Returned write side. */
    TCL_UNUSED(int) /*flags*/)	/* Reserved for future use. */
{
    int fileNums[2];

    if (pipe(fileNums) < 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("pipe creation failed: %s",
		Tcl_PosixError(interp)));
	return TCL_ERROR;
    }

    fcntl(fileNums[0], F_SETFD, FD_CLOEXEC);
    fcntl(fileNums[1], F_SETFD, FD_CLOEXEC);

    *rchan = Tcl_MakeFileChannel(INT2PTR(fileNums[0]), TCL_READABLE);
    Tcl_RegisterChannel(interp, *rchan);
    *wchan = Tcl_MakeFileChannel(INT2PTR(fileNums[1]), TCL_WRITABLE);
    Tcl_RegisterChannel(interp, *wchan);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetAndDetachPids --
 *
 *	This function is invoked in the generic implementation of a
 *	background "exec" (an exec when invoked with a terminating "&") to
 *	store a list of the PIDs for processes in a command pipeline in the
 *	interp's result and to detach the processes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the interp's result. Detaches processes.
 *
 *----------------------------------------------------------------------
 */

void
TclGetAndDetachPids(
    Tcl_Interp *interp,		/* Interpreter to append the PIDs to. */
    Tcl_Channel chan)		/* Handle for the pipeline. */
{
    PipeState *pipePtr;
    const Tcl_ChannelType *chanTypePtr;
    Tcl_Obj *pidsObj;
    size_t i;

    /*
     * Punt if the channel is not a command channel.
     */

    chanTypePtr = Tcl_GetChannelType(chan);
    if (chanTypePtr != &pipeChannelType) {
	return;
    }

    pipePtr = (PipeState *)Tcl_GetChannelInstanceData(chan);
    TclNewObj(pidsObj);
    for (i = 0; i < pipePtr->numPids; i++) {
	Tcl_ListObjAppendElement(NULL, pidsObj, Tcl_NewWideIntObj(
		PTR2INT(pipePtr->pidPtr[i])));
	Tcl_DetachPids(1, &pipePtr->pidPtr[i]);
    }
    Tcl_SetObjResult(interp, pidsObj);
    if (pipePtr->numPids > 0) {
	Tcl_Free(pipePtr->pidPtr);
	pipePtr->numPids = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeBlockModeProc --
 *
 *	Helper function to set blocking and nonblocking modes on a pipe based
 *	channel. Invoked by generic IO level code.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
PipeBlockModeProc(
    void *instanceData,		/* Pipe state. */
    int mode)			/* The mode to set. Can be one of
				 * TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    PipeState *psPtr = (PipeState *)instanceData;

    if (psPtr->inFile
	    && TclUnixSetBlockingMode(GetFd(psPtr->inFile), mode) < 0) {
	return errno;
    }
    if (psPtr->outFile
	    && TclUnixSetBlockingMode(GetFd(psPtr->outFile), mode) < 0) {
	return errno;
    }

    psPtr->isNonBlocking = (mode == TCL_MODE_NONBLOCKING);

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeClose2Proc
 *
 *	This function is invoked by the generic IO level to perform
 *	pipeline-type-specific half or full-close.
 *
 * Results:
 *	0 on success, errno otherwise.
 *
 * Side effects:
 *	Closes the command pipeline channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeClose2Proc(
    void *instanceData,		/* The pipe to close. */
    Tcl_Interp *interp,		/* For error reporting. */
    int flags)			/* Flags that indicate which side to close. */
{
    PipeState *pipePtr = (PipeState *)instanceData;
    Tcl_Channel errChan;
    int errorCode, result;

    errorCode = 0;
    result = 0;

    if (((!flags) || (flags & TCL_CLOSE_READ)) && (pipePtr->inFile != NULL)) {
	if (TclpCloseFile(pipePtr->inFile) < 0) {
	    errorCode = errno;
	} else {
	    pipePtr->inFile = NULL;
	}
    }
    if (((!flags) || (flags & TCL_CLOSE_WRITE)) && (pipePtr->outFile != NULL)
	    && (errorCode == 0)) {
	if (TclpCloseFile(pipePtr->outFile) < 0) {
	    errorCode = errno;
	} else {
	    pipePtr->outFile = NULL;
	}
    }

    /*
     * If half-closing, stop here.
     */

    if (flags) {
	return errorCode;
    }

    if (pipePtr->isNonBlocking || TclInExit()) {
	/*
	 * If the channel is non-blocking or Tcl is being cleaned up, just
	 * detach the children PIDs, reap them (important if we are in a
	 * dynamic load module), and discard the errorFile.
	 */

	Tcl_DetachPids(pipePtr->numPids, pipePtr->pidPtr);
	Tcl_ReapDetachedProcs();

	if (pipePtr->errorFile) {
	    TclpCloseFile(pipePtr->errorFile);
	}
    } else {
	/*
	 * Wrap the error file into a channel and give it to the cleanup
	 * routine.
	 */

	if (pipePtr->errorFile) {
	    errChan = Tcl_MakeFileChannel(
		    INT2PTR(GetFd(pipePtr->errorFile)),
		    TCL_READABLE);
	    /* Error channels should not raise encoding errors */
	    Tcl_SetChannelOption(NULL, errChan, "-profile", "replace");
	} else {
	    errChan = NULL;
	}
	result = TclCleanupChildren(interp, pipePtr->numPids, pipePtr->pidPtr,
		errChan);
    }

    if (pipePtr->numPids != 0) {
	Tcl_Free(pipePtr->pidPtr);
    }
    Tcl_Free(pipePtr);
    if (errorCode == 0) {
	return result;
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeInputProc --
 *
 *	This function is invoked from the generic IO level to read input from
 *	a command pipeline based channel.
 *
 * Results:
 *	The number of bytes read is returned or -1 on error. An output
 *	argument contains a POSIX error code if an error occurs, or zero.
 *
 * Side effects:
 *	Reads input from the input device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeInputProc(
    void *instanceData,		/* Pipe state. */
    char *buf,			/* Where to store data read. */
    int toRead,			/* How much space is available in the
				 * buffer? */
    int *errorCodePtr)		/* Where to store error code. */
{
    PipeState *psPtr = (PipeState *)instanceData;
    ssize_t bytesRead;		/* How many bytes were actually read from the
				 * input device? */

    *errorCodePtr = 0;

    /*
     * Assume there is always enough input available. This will block
     * appropriately, and read will unblock as soon as a short read is
     * possible, if the channel is in blocking mode. If the channel is
     * nonblocking, the read will never block. Some OSes can throw an
     * interrupt error, for which we should immediately retry. [Bug #415131]
     */

    do {
	bytesRead = read(GetFd(psPtr->inFile), buf, toRead);
    } while ((bytesRead < 0) && (errno == EINTR));

    if (bytesRead < 0) {
	*errorCodePtr = errno;
	return -1;
    }
    return (int)bytesRead;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeOutputProc--
 *
 *	This function is invoked from the generic IO level to write output to
 *	a command pipeline based channel.
 *
 * Results:
 *	The number of bytes written is returned or -1 on error. An output
 *	argument contains a POSIX error code if an error occurred, or zero.
 *
 * Side effects:
 *	Writes output on the output device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeOutputProc(
    void *instanceData,		/* Pipe state. */
    const char *buf,		/* The data buffer. */
    int toWrite,		/* How many bytes to write? */
    int *errorCodePtr)		/* Where to store error code. */
{
    PipeState *psPtr = (PipeState *)instanceData;
    ssize_t written;

    *errorCodePtr = 0;

    /*
     * Some OSes can throw an interrupt error, for which we should immediately
     * retry. [Bug #415131]
     */

    do {
	written = write(GetFd(psPtr->outFile), buf, toWrite);
    } while ((written < 0) && (errno == EINTR));

    if (written < 0) {
	*errorCodePtr = errno;
	return -1;
    }
    return (int)written;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeWatchProc --
 *
 *	Initialize the notifier to watch the fds from this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the notifier so that a future event on the channel will be
 *	seen by Tcl.
 *
 *----------------------------------------------------------------------
 */

/*
 * Bug ad5a57f2f271: Tcl_NotifyChannel is not a Tcl_FileProc,
 * so do not pass it to directly to Tcl_CreateFileHandler.
 * Instead, pass a wrapper which is a Tcl_FileProc.
 */

static void
PipeWatchNotifyChannelWrapper(
    void *clientData,
    int mask)
{
    Tcl_Channel channel = (Tcl_Channel)clientData;

    Tcl_NotifyChannel(channel, mask);
}

static void
PipeWatchProc(
    void *instanceData,		/* The pipe state. */
    int mask)			/* Events of interest; an OR-ed combination of
				 * TCL_READABLE, TCL_WRITABLE and
				 * TCL_EXCEPTION. */
{
    PipeState *psPtr = (PipeState *)instanceData;
    int newmask;

    if (psPtr->inFile) {
	newmask = mask & (TCL_READABLE | TCL_EXCEPTION);
	if (newmask) {
	    Tcl_CreateFileHandler(GetFd(psPtr->inFile), newmask,
		    PipeWatchNotifyChannelWrapper, psPtr->channel);
	} else {
	    Tcl_DeleteFileHandler(GetFd(psPtr->inFile));
	}
    }
    if (psPtr->outFile) {
	newmask = mask & (TCL_WRITABLE | TCL_EXCEPTION);
	if (newmask) {
	    Tcl_CreateFileHandler(GetFd(psPtr->outFile), newmask,
		    PipeWatchNotifyChannelWrapper, psPtr->channel);
	} else {
	    Tcl_DeleteFileHandler(GetFd(psPtr->outFile));
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from inside a
 *	command pipeline based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if there is no
 *	handle for the specified direction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PipeGetHandleProc(
    void *instanceData,		/* The pipe state. */
    int direction,		/* TCL_READABLE or TCL_WRITABLE */
    void **handlePtr)		/* Where to store the handle. */
{
    PipeState *psPtr = (PipeState *)instanceData;

    if (direction == TCL_READABLE && psPtr->inFile) {
	*handlePtr = INT2PTR(GetFd(psPtr->inFile));
	return TCL_OK;
    }
    if (direction == TCL_WRITABLE && psPtr->outFile) {
	*handlePtr = INT2PTR(GetFd(psPtr->outFile));
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Implements the waitpid system call on Unix systems.
 *
 * Results:
 *	Result of calling waitpid.
 *
 * Side effects:
 *	Waits for a process to terminate.
 *
 *----------------------------------------------------------------------
 */

Tcl_Pid
Tcl_WaitPid(
    Tcl_Pid pid,
    int *statPtr,
    int options)
{
    int result;
    pid_t real_pid = (pid_t) PTR2INT(pid);

    while (1) {
	result = (int) waitpid(real_pid, statPtr, options);
	if ((result != -1) || (errno != EINTR)) {
	    return (Tcl_Pid)INT2PTR(result);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PidObjCmd --
 *
 *	This function is invoked to process the "pid" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_PidObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Argument strings. */
{
    Tcl_Channel chan;
    PipeState *pipePtr;
    size_t i;
    Tcl_Obj *resultPtr;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?channel?");
	return TCL_ERROR;
    }

    if (objc == 1) {
	Tcl_SetObjResult(interp, Tcl_NewWideIntObj(getpid()));
    } else {
	/*
	 * Get the channel and make sure that it refers to a pipe.
	 */

	chan = Tcl_GetChannel(interp, TclGetString(objv[1]), NULL);
	if (chan == NULL) {
	    return TCL_ERROR;
	}
	if (Tcl_GetChannelType(chan) != &pipeChannelType) {
	    return TCL_OK;
	}

	/*
	 * Extract the process IDs from the pipe structure.
	 */

	pipePtr = (PipeState *)Tcl_GetChannelInstanceData(chan);
	TclNewObj(resultPtr);
	for (i = 0; i < pipePtr->numPids; i++) {
	    Tcl_ListObjAppendElement(NULL, resultPtr,
		    Tcl_NewWideIntObj(TclpGetPid(pipePtr->pidPtr[i])));
	}
	Tcl_SetObjResult(interp, resultPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizePipes --
 *
 *	Cleans up the pipe subsystem from Tcl_FinalizeThread
 *
 * Results:
 *	None.
 *
 * Notes:
 *	This function carries out no operation on Unix.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizePipes(void)
{
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
