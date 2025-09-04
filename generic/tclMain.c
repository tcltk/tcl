/*
 * tclMain.c --
 *
 *	Main program for Tcl shells and other Tcl-based applications.
 *	This file contains a generic main program for Tcl shells and other
 *	Tcl-based applications. It can be used as-is for many applications,
 *	just by supplying a different appInitProc function for each specific
 *	application. Or, it can be used as a template for creating new main
 *	programs for Tcl applications.
 *
 * Copyright © 1988-1994 The Regents of the University of California.
 * Copyright © 1994-1997 Sun Microsystems, Inc.
 * Copyright © 2000 Ajuba Solutions.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * On Windows, this file needs to be compiled twice, once with UNICODE and
 * _UNICODE defined. This way both Tcl_Main and Tcl_MainExW can be
 * implemented, sharing the same source code.
 */

#include "tclInt.h"

/*
 * The default prompt used when the user has not overridden it.
 */

static const char DEFAULT_PRIMARY_PROMPT[] = "% ";
static const char ENCODING_ERROR[] = "\n\t(encoding error in stderr)";

/*
 * This file can be compiled on Windows in UNICODE mode, as well as on all
 * other platforms using the native encoding. This is done by using the normal
 * Windows functions like _tcscmp, but on platforms which don't have <tchar.h>
 * we have to translate that to strcmp here.
 */

#ifndef _WIN32
#   define TCHAR char
#   define TEXT(arg) arg
#   define _tcscmp strcmp
#endif

static inline Tcl_Obj *
NewNativeObj(
    TCHAR *string)
{
    Tcl_DString ds;

#ifdef UNICODE
    Tcl_DStringInit(&ds);
    Tcl_WCharToUtfDString(string, -1, &ds);
#else
    (void)Tcl_ExternalToUtfDString(NULL, (char *)string, -1, &ds);
#endif
    return Tcl_DStringToObj(&ds);
}

/*
 * Declarations for various library functions and variables (don't want to
 * include tclPort.h here, because people might copy this file out of the Tcl
 * source directory to make their own modified versions).
 */

/*
 * The thread-local variables for this file's functions.
 */

typedef struct {
    Tcl_Obj *path;		/* The filename of the script for *_Main()
				 * routines to [source] as a startup script,
				 * or NULL for none set, meaning enter
				 * interactive mode. */
    Tcl_Obj *encoding;		/* The encoding of the startup script file. */
    Tcl_MainLoopProc *mainLoopProc;
				/* Any installed main loop handler. The main
				 * extension that installs these is Tk. */
} ThreadSpecificData;

/*
 * Structure definition for information used to keep the state of an
 * interactive command processor that reads lines from standard input and
 * writes prompts and results to standard output.
 */

typedef enum {
    PROMPT_NONE,		/* Print no prompt */
    PROMPT_START,		/* Print prompt for command start */
    PROMPT_CONTINUE		/* Print prompt for command continuation */
} PromptType;

typedef struct {
    Tcl_Channel input;		/* The standard input channel from which lines
				 * are read. */
    int tty;			/* Non-zero means standard input is a
				 * terminal-like device. Zero means it's a
				 * file. */
    Tcl_Obj *commandPtr;	/* Used to assemble lines of input into Tcl
				 * commands. */
    PromptType prompt;		/* Next prompt to print */
    Tcl_Interp *interp;		/* Interpreter that evaluates interactive
				 * commands. */
} InteractiveState;

/*
 * Forward declarations for functions defined later in this file.
 */

MODULE_SCOPE Tcl_MainLoopProc *TclGetMainLoop(void);
static void		Prompt(Tcl_Interp *interp, InteractiveState *isPtr);
static void		StdinProc(void *clientData, int mask);
static void		FreeMainInterp(void *clientData);

#if !defined(_WIN32) || defined(UNICODE) && !defined(TCL_ASCII_MAIN)
static Tcl_ThreadDataKey dataKey;

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetStartupScript --
 *
 *	Sets the path and encoding of the startup script to be evaluated by
 *	Tcl_Main, used to override the command line processing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetStartupScript(
    Tcl_Obj *path,		/* Filesystem path of startup script file */
    const char *encodingName)	/* Encoding of the data in that file */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    Tcl_Obj *encodingObj = NULL;

    if (encodingName != NULL) {
	encodingObj = Tcl_NewStringObj(encodingName, -1);
	Tcl_IncrRefCount(encodingObj);
    }

    if (path != NULL) {
	Tcl_IncrRefCount(path);
    }
    if (tsdPtr->path != NULL) {
	Tcl_DecrRefCount(tsdPtr->path);
    }
    tsdPtr->path = path;

    if (tsdPtr->encoding != NULL) {
	Tcl_DecrRefCount(tsdPtr->encoding);
    }
    tsdPtr->encoding = encodingObj;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStartupScript --
 *
 *	Gets the path and encoding of the startup script to be evaluated by
 *	Tcl_Main.
 *
 * Results:
 *	The path of the startup script; NULL if none has been set.
 *
 * Side effects:
 *	If encodingPtr is not NULL, stores a (const char *) in it pointing to
 *	the encoding name registered for the startup script. Tcl retains
 *	ownership of the string, and may free it. Caller should make a copy
 *	for long-term use.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_GetStartupScript(
    const char **encodingPtr)	/* When not NULL, points to storage for the
				 * (const char *) that points to the
				 * registered encoding name for the startup
				 * script. */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (encodingPtr != NULL) {
	if (tsdPtr->encoding != NULL) {
	    *encodingPtr = Tcl_GetString(tsdPtr->encoding);
	} else {
	    *encodingPtr = NULL;
	}
    }
    return tsdPtr->path;
}

/*----------------------------------------------------------------------
 *
 * Tcl_SourceRCFile --
 *
 *	This function is typically invoked by Tcl_Main of Tk_Main function to
 *	source an application specific rc file into the interpreter at startup
 *	time. If the filename cannot be translated (e.g. it referred to a bogus
 *	user or there was no HOME environment variable). Just do nothing.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what's in the rc script.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SourceRCFile(
    Tcl_Interp *interp)		/* Interpreter to source rc file into. */
{
    Tcl_DString temp;
    const char *fileName;
    Tcl_Channel chan;

    fileName = Tcl_GetVar2(interp, "tcl_rcFileName", NULL, TCL_GLOBAL_ONLY);
    if (fileName != NULL) {
	Tcl_Channel c;
	const char *fullName;

	Tcl_DStringInit(&temp);
	fullName = Tcl_TranslateFileName(interp, fileName, &temp);
	if (fullName != NULL) {
	    /*
	     * Test for the existence of the rc file before trying to read it.
	     */

	    c = Tcl_OpenFileChannel(NULL, fullName, "r", 0);
	    if (c != NULL) {
		Tcl_CloseEx(NULL, c, 0);
		if (Tcl_EvalFile(interp, fullName) != TCL_OK) {
		    chan = Tcl_GetStdChannel(TCL_STDERR);
		    if (chan) {
			if (Tcl_WriteObj(chan, Tcl_GetObjResult(interp)) < 0) {
			    Tcl_WriteChars(chan, ENCODING_ERROR, -1);
			}
			Tcl_WriteChars(chan, "\n", 1);
		    }
		}
	    }
	}
	Tcl_DStringFree(&temp);
    }
}
#endif /* !UNICODE */

/*----------------------------------------------------------------------
 *
 * Tcl_MainEx --
 *
 *	Main program for tclsh and most other Tcl-based applications.
 *
 * Results:
 *	None. This function never returns (it exits the process when it's
 *	done).
 *
 * Side effects:
 *	This function initializes the Tcl world and then starts interpreting
 *	commands; almost anything could happen, depending on the script being
 *	interpreted.
 *
 *----------------------------------------------------------------------
 */

TCL_NORETURN void
Tcl_MainEx(
    Tcl_Size argc,		/* Number of arguments. */
    TCHAR **argv,		/* Array of argument strings. */
    Tcl_AppInitProc *appInitProc,
				/* Application-specific initialization
				 * function to call after most initialization
				 * but before starting to execute commands. */
    Tcl_Interp *interp)
{
    Tcl_Size i=0;		/* argv[i] index */
    Tcl_Obj *path, *resultPtr, *argvPtr, *appName;
    const char *encodingName = NULL;
    int code, exitCode = 0;
    Tcl_MainLoopProc *mainLoopProc;
    Tcl_Channel chan;
    InteractiveState is;

    TclpSetInitialEncodings();
    if (argc > 0) {
	--argc;			/* consume argv[0] */
	++i;
    }
    TclpFindExecutable((const char *)argv[0]);	/* nb: this could be NULL
						 * w/ (eg) an empty argv supplied to execve() */

    Tcl_InitMemory(interp);

    is.interp = interp;
    is.prompt = PROMPT_START;
    TclNewObj(is.commandPtr);

    /*
     * If the application has not already set a startup script, parse the
     * first few command line arguments to determine the script path and
     * encoding.
     */

    if (NULL == Tcl_GetStartupScript(NULL)) {
	/*
	 * Check whether first 3 args (argv[1] - argv[3]) look like
	 *  -encoding ENCODING FILENAME
	 * or like
	 *  FILENAME
	 */

	/* mind argc is being adjusted as we proceed */
	if ((argc >= 3) && (0 == _tcscmp(TEXT("-encoding"), argv[1]))
		&& ('-' != argv[3][0])) {
	    Tcl_Obj *value = NewNativeObj(argv[2]);
	    Tcl_SetStartupScript(NewNativeObj(argv[3]),
		    Tcl_GetString(value));
	    Tcl_DecrRefCount(value);
	    argc -= 3;
	    i += 3;
	} else if ((argc >= 1) && ('-' != argv[1][0])) {
	    Tcl_SetStartupScript(NewNativeObj(argv[1]), NULL);
	    argc--;
	    i++;
	}
    }

    path = Tcl_GetStartupScript(&encodingName);
    if (path != NULL) {
	appName = path;
    } else if (argv[0]) {
	appName = NewNativeObj(argv[0]);
    } else {
	appName = Tcl_NewStringObj("tclsh", -1);
    }
    Tcl_SetVar2Ex(interp, "argv0", NULL, appName, TCL_GLOBAL_ONLY);

    Tcl_SetVar2Ex(interp, "argc", NULL, Tcl_NewWideIntObj(argc), TCL_GLOBAL_ONLY);

    argvPtr = Tcl_NewListObj(0, NULL);
    while (argc--) {
	Tcl_ListObjAppendElement(NULL, argvPtr, NewNativeObj(argv[i++]));
    }
    Tcl_SetVar2Ex(interp, "argv", NULL, argvPtr, TCL_GLOBAL_ONLY);

    /*
     * Set the "tcl_interactive" variable.
     */

    is.tty = isatty(0);
    Tcl_SetVar2Ex(interp, "tcl_interactive", NULL,
	    Tcl_NewBooleanObj(!path && is.tty), TCL_GLOBAL_ONLY);

    /*
     * Invoke application-specific initialization.
     */

    Tcl_Preserve(interp);
    if (appInitProc(interp) != TCL_OK) {
	chan = Tcl_GetStdChannel(TCL_STDERR);
	if (chan) {
	    Tcl_WriteChars(chan,
		    "application-specific initialization failed: ", -1);
	    if (Tcl_WriteObj(chan, Tcl_GetObjResult(interp)) < 0) {
		Tcl_WriteChars(chan, ENCODING_ERROR, -1);
	    }
	    Tcl_WriteChars(chan, "\n", 1);
	}
    }
    if (Tcl_InterpDeleted(interp)) {
	goto done;
    }
    if (Tcl_LimitExceeded(interp)) {
	goto done;
    }
    if (TclFullFinalizationRequested()) {
	/*
	 * Arrange for final deletion of the main interp
	 */

	/* ARGH Munchhausen effect */
	Tcl_CreateExitHandler(FreeMainInterp, interp);
    }

    /*
     * Invoke the script specified on the command line, if any. Must fetch it
     * again, as the appInitProc might have reset it.
     */

    path = Tcl_GetStartupScript(&encodingName);
    if (path != NULL) {
	Tcl_ResetResult(interp);
	code = Tcl_FSEvalFileEx(interp, path, encodingName);
	if (code != TCL_OK) {
	    chan = Tcl_GetStdChannel(TCL_STDERR);
	    if (chan) {
		Tcl_Obj *options = Tcl_GetReturnOptions(interp, code);
		Tcl_Obj *valuePtr = NULL;

		TclDictGet(NULL, options, "-errorinfo", &valuePtr);
		if (valuePtr) {
		    if (Tcl_WriteObj(chan, valuePtr) < 0) {
			Tcl_WriteChars(chan, ENCODING_ERROR, -1);
		    }
		}
		Tcl_WriteChars(chan, "\n", 1);
		Tcl_DecrRefCount(options);
	    }
	    exitCode = 1;
	}
	goto done;
    }

    /*
     * We're running interactively. Source a user-specific startup file if the
     * application specified one and if the file exists.
     */

    Tcl_SourceRCFile(interp);
    if (Tcl_LimitExceeded(interp)) {
	goto done;
    }

    /*
     * Process commands from stdin until there's an end-of-file. Note that we
     * need to fetch the standard channels again after every eval, since they
     * may have been changed.
     */

    Tcl_IncrRefCount(is.commandPtr);

    /*
     * Get a new value for tty if anyone writes to ::tcl_interactive
     */

    Tcl_LinkVar(interp, "tcl_interactive", &is.tty, TCL_LINK_BOOLEAN);
    is.input = Tcl_GetStdChannel(TCL_STDIN);
    while ((is.input != NULL) && !Tcl_InterpDeleted(interp)) {
	mainLoopProc = TclGetMainLoop();
	if (mainLoopProc == NULL) {
	    Tcl_Size length;

	    if (is.tty) {
		Prompt(interp, &is);
		if (Tcl_InterpDeleted(interp)) {
		    break;
		}
		if (Tcl_LimitExceeded(interp)) {
		    break;
		}
		is.input = Tcl_GetStdChannel(TCL_STDIN);
		if (is.input == NULL) {
		    break;
		}
	    }
	    if (Tcl_IsShared(is.commandPtr)) {
		Tcl_DecrRefCount(is.commandPtr);
		is.commandPtr = Tcl_DuplicateObj(is.commandPtr);
		Tcl_IncrRefCount(is.commandPtr);
	    }
	    length = Tcl_GetsObj(is.input, is.commandPtr);
	    if (length < 0) {
		if (Tcl_InputBlocked(is.input)) {
		    /*
		     * This can only happen if stdin has been set to
		     * non-blocking. In that case cycle back and try again.
		     * This sets up a tight polling loop (since we have no
		     * event loop running). If this causes bad CPU hogging, we
		     * might try toggling the blocking on stdin instead.
		     */

		    continue;
		}

		/*
		 * Either EOF, or an error on stdin; we're done
		 */

		break;
	    }

	    /*
	     * Add the newline removed by Tcl_GetsObj back to the string. Have
	     * to add it back before testing completeness, because it can make
	     * a difference. [Bug 1775878]
	     */

	    if (Tcl_IsShared(is.commandPtr)) {
		Tcl_DecrRefCount(is.commandPtr);
		is.commandPtr = Tcl_DuplicateObj(is.commandPtr);
		Tcl_IncrRefCount(is.commandPtr);
	    }
	    Tcl_AppendToObj(is.commandPtr, "\n", 1);
	    if (!TclObjCommandComplete(is.commandPtr)) {
		is.prompt = PROMPT_CONTINUE;
		continue;
	    }

	    is.prompt = PROMPT_START;

	    /*
	     * The final newline is syntactically redundant, and causes some
	     * error messages troubles deeper in, so lop it back off.
	     */

	    (void)Tcl_GetStringFromObj(is.commandPtr, &length);
	    Tcl_SetObjLength(is.commandPtr, --length);
	    code = Tcl_RecordAndEvalObj(interp, is.commandPtr,
		    TCL_EVAL_GLOBAL);
	    is.input = Tcl_GetStdChannel(TCL_STDIN);
	    Tcl_DecrRefCount(is.commandPtr);
	    TclNewObj(is.commandPtr);
	    Tcl_IncrRefCount(is.commandPtr);
	    if (code != TCL_OK) {
		chan = Tcl_GetStdChannel(TCL_STDERR);
		if (chan) {
			if (Tcl_WriteObj(chan, Tcl_GetObjResult(interp)) < 0) {
			    Tcl_WriteChars(chan, ENCODING_ERROR, -1);
			}
		    Tcl_WriteChars(chan, "\n", 1);
		}
	    } else if (is.tty) {
		resultPtr = Tcl_GetObjResult(interp);
		Tcl_IncrRefCount(resultPtr);
		(void)Tcl_AttemptGetStringFromObj(resultPtr, &length);
		if (!resultPtr->bytes) {
		    chan = Tcl_GetStdChannel(TCL_STDERR);
		    if (chan) {
			Tcl_WriteChars(chan, "Memory allocation error\n", -1);
		    }
		} else {
		    chan = Tcl_GetStdChannel(TCL_STDOUT);
		    if ((length > 0) && chan) {
			if (Tcl_WriteObj(chan, resultPtr) < 0) {
			    Tcl_WriteChars(chan, ENCODING_ERROR, -1);
			}
			Tcl_WriteChars(chan, "\n", 1);
		    }
		}
		Tcl_DecrRefCount(resultPtr);
	    }
	} else {	/* (mainLoopProc != NULL) */
	    /*
	     * If a main loop has been defined while running interactively, we
	     * want to start a fileevent based prompt by establishing a
	     * channel handler for stdin.
	     */

	    if (is.input) {
		if (is.tty) {
		    Prompt(interp, &is);
		}

		Tcl_CreateChannelHandler(is.input, TCL_READABLE,
			StdinProc, &is);
	    }

	    mainLoopProc();
	    Tcl_SetMainLoop(NULL);

	    if (is.input) {
		Tcl_DeleteChannelHandler(is.input, StdinProc, &is);
	    }
	    is.input = Tcl_GetStdChannel(TCL_STDIN);
	}

	/*
	 * This code here only for the (unsupported and deprecated) [checkmem]
	 * command.
	 */

#ifdef TCL_MEM_DEBUG
	if (tclMemDumpFileName != NULL) {
	    Tcl_SetMainLoop(NULL);
	    Tcl_DeleteInterp(interp);
	}
#endif /* TCL_MEM_DEBUG */
    }

  done:
    mainLoopProc = TclGetMainLoop();
    if ((exitCode == 0) && mainLoopProc && !Tcl_LimitExceeded(interp)) {
	/*
	 * If everything has gone OK so far, call the main loop proc, if it
	 * exists. Packages (like Tk) can set it to start processing events at
	 * this point.
	 */

	mainLoopProc();
	Tcl_SetMainLoop(NULL);
    }
    if (is.commandPtr != NULL) {
	Tcl_DecrRefCount(is.commandPtr);
    }

    /*
     * Rather than calling exit, invoke the "exit" command so that users can
     * replace "exit" with some other command to do additional cleanup on
     * exit. The Tcl_EvalObjEx call should never return.
     */

    if (!Tcl_InterpDeleted(interp) && !Tcl_LimitExceeded(interp)) {
	Tcl_Obj *cmd = Tcl_ObjPrintf("exit %d", exitCode);

	Tcl_IncrRefCount(cmd);
	Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(cmd);
    }

    /*
     * If Tcl_EvalObjEx returns, trying to eval [exit], something unusual is
     * happening. Maybe interp has been deleted; maybe [exit] was redefined,
     * maybe we've blown up because of an exceeded limit. We still want to
     * cleanup and exit.
     */

    Tcl_Exit(exitCode);
}

#if !defined(_WIN32) || defined(UNICODE)

/*
 *---------------------------------------------------------------
 *
 * Tcl_SetMainLoop --
 *
 *	Sets an alternative main loop function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This function will be called before Tcl exits, allowing for the
 *	creation of an event loop.
 *
 *---------------------------------------------------------------
 */

void
Tcl_SetMainLoop(
    Tcl_MainLoopProc *proc)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    tsdPtr->mainLoopProc = proc;
}

/*
 *---------------------------------------------------------------
 *
 * TclGetMainLoop --
 *
 *	Returns the current alternative main loop function.
 *
 * Results:
 *	Returns the previously defined main loop function, or NULL to indicate
 *	that no such function has been installed and standard tclsh behaviour
 *	(i.e., exit once the script is evaluated if not interactive) is
 *	requested..
 *
 * Side effects:
 *	None (other than possible creation of this file's TSD block).
 *
 *---------------------------------------------------------------
 */

Tcl_MainLoopProc *
TclGetMainLoop(void)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    return tsdPtr->mainLoopProc;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFullFinalizationRequested --
 *
 *	This function returns true when either -DPURIFY is specified, or the
 *	environment variable TCL_FINALIZE_ON_EXIT is set and not "0". This
 *	predicate is called at places affecting the exit sequence, so that the
 *	default behavior is a fast and deadlock-free exit, and the modified
 *	behavior is a more thorough finalization for debugging purposes (leak
 *	hunting etc).
 *
 * Results:
 *	A boolean.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TclFullFinalizationRequested(void)
{
#ifdef PURIFY
    return 1;
#else
    const char *fin;
    Tcl_DString ds;
    int finalize = 0;

    fin = TclGetEnv("TCL_FINALIZE_ON_EXIT", &ds);
    finalize = ((fin != NULL) && strcmp(fin, "0"));
    if (fin != NULL) {
	Tcl_DStringFree(&ds);
    }
    return finalize;
#endif /* PURIFY */
}
#endif /* UNICODE */

/*
 *----------------------------------------------------------------------
 *
 * StdinProc --
 *
 *	This function is invoked by the event dispatcher whenever standard
 *	input becomes readable. It grabs the next line of input characters,
 *	adds them to a command being assembled, and executes the command if
 *	it's complete.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Could be almost arbitrary, depending on the command that's typed.
 *
 *----------------------------------------------------------------------
 */

static void
StdinProc(
    void *clientData,		/* The state of interactive cmd line */
    TCL_UNUSED(int) /*mask*/)
{
    int code;
    Tcl_Size length;
    InteractiveState *isPtr = (InteractiveState *)clientData;
    Tcl_Channel chan = isPtr->input;
    Tcl_Obj *commandPtr = isPtr->commandPtr;
    Tcl_Interp *interp = isPtr->interp;

    if (Tcl_IsShared(commandPtr)) {
	Tcl_DecrRefCount(commandPtr);
	commandPtr = Tcl_DuplicateObj(commandPtr);
	Tcl_IncrRefCount(commandPtr);
    }
    length = Tcl_GetsObj(chan, commandPtr);
    if (length < 0) {
	if (Tcl_InputBlocked(chan)) {
	    return;
	}
	if (isPtr->tty) {
	    /*
	     * Would be better to find a way to exit the mainLoop? Or perhaps
	     * evaluate [exit]? Leaving as is for now due to compatibility
	     * concerns.
	     */

	    Tcl_Exit(0);
	}
	Tcl_DeleteChannelHandler(chan, StdinProc, isPtr);
	return;
    }

    if (Tcl_IsShared(commandPtr)) {
	Tcl_DecrRefCount(commandPtr);
	commandPtr = Tcl_DuplicateObj(commandPtr);
	Tcl_IncrRefCount(commandPtr);
    }
    Tcl_AppendToObj(commandPtr, "\n", 1);
    if (!TclObjCommandComplete(commandPtr)) {
	isPtr->prompt = PROMPT_CONTINUE;
	goto prompt;
    }
    isPtr->prompt = PROMPT_START;
    (void)Tcl_GetStringFromObj(commandPtr, &length);
    Tcl_SetObjLength(commandPtr, --length);

    /*
     * Disable the stdin channel handler while evaluating the command;
     * otherwise if the command re-enters the event loop we might process
     * commands from stdin before the current command is finished. Among other
     * things, this will trash the text of the command being evaluated.
     */

    Tcl_CreateChannelHandler(chan, 0, StdinProc, isPtr);
    code = Tcl_RecordAndEvalObj(interp, commandPtr, TCL_EVAL_GLOBAL);
    isPtr->input = chan = Tcl_GetStdChannel(TCL_STDIN);
    Tcl_DecrRefCount(commandPtr);
    TclNewObj(commandPtr);
    isPtr->commandPtr = commandPtr;
    Tcl_IncrRefCount(commandPtr);
    if (chan != NULL) {
	Tcl_CreateChannelHandler(chan, TCL_READABLE, StdinProc, isPtr);
    }
    if (code != TCL_OK) {
	chan = Tcl_GetStdChannel(TCL_STDERR);

	if (chan != NULL) {
	    if (Tcl_WriteObj(chan, Tcl_GetObjResult(interp)) < 0) {
		Tcl_WriteChars(chan, ENCODING_ERROR, -1);
	    }
	    Tcl_WriteChars(chan, "\n", 1);
	}
    } else if (isPtr->tty) {
	Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
	chan = Tcl_GetStdChannel(TCL_STDOUT);

	Tcl_IncrRefCount(resultPtr);
	(void)Tcl_AttemptGetStringFromObj(resultPtr, &length);
	if (!resultPtr->bytes) {
	    chan = Tcl_GetStdChannel(TCL_STDERR);
	    if (chan) {
		Tcl_WriteChars(chan, "Memory allocation error\n", -1);
	    }
	} else {
	    if ((length > 0) && (chan != NULL)) {
		if (Tcl_WriteObj(chan, resultPtr) < 0) {
		    Tcl_WriteChars(chan, ENCODING_ERROR, -1);
		}
		Tcl_WriteChars(chan, "\n", 1);
	    }
	}
	Tcl_DecrRefCount(resultPtr);
    }

    /*
     * If a tty stdin is still around, output a prompt.
     */

  prompt:
    if (isPtr->tty && (isPtr->input != NULL)) {
	Prompt(interp, isPtr);
	isPtr->input = Tcl_GetStdChannel(TCL_STDIN);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Prompt --
 *
 *	Issue a prompt on standard output, or invoke a script to issue the
 *	prompt.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A prompt gets output, and a Tcl script may be evaluated in interp.
 *
 *----------------------------------------------------------------------
 */

static void
Prompt(
    Tcl_Interp *interp,		/* Interpreter to use for prompting. */
    InteractiveState *isPtr)	/* InteractiveState. Filled with PROMPT_NONE
				 * after a prompt is printed. */
{
    Tcl_Obj *promptCmdPtr;
    int code;
    Tcl_Channel chan;

    if (isPtr->prompt == PROMPT_NONE) {
	return;
    }

    promptCmdPtr = Tcl_GetVar2Ex(interp,
	    (isPtr->prompt==PROMPT_CONTINUE ? "tcl_prompt2" : "tcl_prompt1"),
	    NULL, TCL_GLOBAL_ONLY);

    if (Tcl_InterpDeleted(interp)) {
	return;
    }
    if (promptCmdPtr == NULL) {
    defaultPrompt:
	if (isPtr->prompt == PROMPT_START) {
	    chan = Tcl_GetStdChannel(TCL_STDOUT);
	    if (chan != NULL) {
		Tcl_WriteChars(chan, DEFAULT_PRIMARY_PROMPT,
			sizeof(DEFAULT_PRIMARY_PROMPT) - 1);
	    }
	}
    } else {
	code = Tcl_EvalObjEx(interp, promptCmdPtr, TCL_EVAL_GLOBAL);
	if (code != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		    "\n    (script that generates prompt)");
	    chan = Tcl_GetStdChannel(TCL_STDERR);
	    if (chan != NULL) {
		if (Tcl_WriteObj(chan, Tcl_GetObjResult(interp)) < 0) {
		    Tcl_WriteChars(chan, ENCODING_ERROR, -1);
		}
		Tcl_WriteChars(chan, "\n", 1);
	    }
	    goto defaultPrompt;
	}
    }

    chan = Tcl_GetStdChannel(TCL_STDOUT);
    if (chan != NULL) {
	Tcl_Flush(chan);
    }
    isPtr->prompt = PROMPT_NONE;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeMainInterp --
 *
 *	Exit handler used to cleanup the main interpreter and ancillary
 *	startup script storage at exit.
 *
 *----------------------------------------------------------------------
 */

static void
FreeMainInterp(
    void *clientData)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;

    /*if (TclInExit()) return;*/

    if (!Tcl_InterpDeleted(interp)) {
	Tcl_DeleteInterp(interp);
    }
    Tcl_SetStartupScript(NULL, NULL);
    Tcl_Release(interp);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
