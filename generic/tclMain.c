/* 
 * tclMain.c --
 *
 *	Main program for Tcl shells and other Tcl-based applications.
 *
 * Copyright (c) 1988-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Ajuba Solutions.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclMain.c,v 1.7.2.2.2.2 2002/11/26 20:05:51 hobbs Exp $
 */

#include "tcl.h"
#include "tclInt.h"

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT

/*
 * The following code ensures that tclLink.c is linked whenever
 * Tcl is linked.  Without this code there's no reference to the
 * code in that file from anywhere in Tcl, so it may not be
 * linked into the application.
 */

EXTERN int Tcl_LinkVar();
int (*tclDummyLinkVarPtr)() = Tcl_LinkVar;

/*
 * Declarations for various library procedures and variables (don't want
 * to include tclPort.h here, because people might copy this file out of
 * the Tcl source directory to make their own modified versions).
 * Note:  "exit" should really be declared here, but there's no way to
 * declare it without causing conflicts with other definitions elsewher
 * on some systems, so it's better just to leave it out.
 */

#if !defined(MAC_TCL)
extern int		isatty _ANSI_ARGS_((int fd));
#else
#include <unistd.h>
#endif
extern char *		strcpy _ANSI_ARGS_((char *dst, CONST char *src));

static char *tclStartupScriptFileName = NULL;

static Tcl_MainLoopProc *mainLoopProc = NULL;

typedef struct ThreadSpecificData {
    Tcl_Interp *interp;         /* Interpreter for this thread. */
    Tcl_DString command;        /* Used to assemble lines of terminal input
				 * into Tcl commands. */
    Tcl_DString line;           /* Used to read the next line from the
				 * terminal input. */
    int tty;                    /* Non-zero means standard input is a 
				 * terminal-like device.  Zero means it's
				 * a file. */
} ThreadSpecificData;
Tcl_ThreadDataKey dataKey;

/*
 * Forward declarations for procedures defined later in this file.
 */

static void		Prompt _ANSI_ARGS_((Tcl_Interp *interp, int partial));
#ifndef TCL_NO_FILEEVENTS
static void		StdinProc _ANSI_ARGS_((ClientData clientData,
			    int mask));
#endif


/*
 *----------------------------------------------------------------------
 *
 * TclSetStartupScriptFileName --
 *
 *	Primes the startup script file name, used to override the
 *      command line processing.
 *
 * Results:
 *	None. 
 *
 * Side effects:
 *	This procedure initializes the file name of the Tcl script to
 *      run at startup.
 *
 *----------------------------------------------------------------------
 */
void TclSetStartupScriptFileName(fileName)
    char *fileName;
{
    tclStartupScriptFileName = fileName;
}


/*
 *----------------------------------------------------------------------
 *
 * TclGetStartupScriptFileName --
 *
 *	Gets the startup script file name, used to override the
 *      command line processing.
 *
 * Results:
 *	The startup script file name, NULL if none has been set.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
char *TclGetStartupScriptFileName()
{
    return tclStartupScriptFileName;
}



/*
 *----------------------------------------------------------------------
 *
 * Tcl_Main --
 *
 *	Main program for tclsh and most other Tcl-based applications.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done).
 *
 * Side effects:
 *	This procedure initializes the Tcl world and then starts
 *	interpreting commands;  almost anything could happen, depending
 *	on the script being interpreted.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Main(argc, argv, appInitProc)
    int argc;			/* Number of arguments. */
    char **argv;		/* Array of argument strings. */
    Tcl_AppInitProc *appInitProc;
				/* Application-specific initialization
				 * procedure to call after most
				 * initialization but before starting to
				 * execute commands. */
{
    Tcl_Obj *resultPtr;
    Tcl_Obj *commandPtr = NULL;
    char buffer[1000], *args;
    int code, gotPartial, length;
    int exitCode = 0;
    Tcl_Channel inChannel, outChannel, errChannel;
    Tcl_Interp *interp;
    Tcl_DString argString;
    ThreadSpecificData *tsdPtr;

    Tcl_FindExecutable(argv[0]);

    tsdPtr = (ThreadSpecificData *) 
	Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    tsdPtr->interp = interp = Tcl_CreateInterp();
#ifdef TCL_MEM_DEBUG
    Tcl_InitMemory(interp);
#endif

    /*
     * Make command-line arguments available in the Tcl variables "argc"
     * and "argv".  If the first argument doesn't start with a "-" then
     * strip it off and use it as the name of a script file to process.
     */

    if (tclStartupScriptFileName == NULL) {
	if ((argc > 1) && (argv[1][0] != '-')) {
	    tclStartupScriptFileName = argv[1];
	    argc--;
	    argv++;
	}
    }
    args = Tcl_Merge(argc-1, argv+1);
    Tcl_ExternalToUtfDString(NULL, args, -1, &argString);
    Tcl_SetVar(interp, "argv", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&argString);
    ckfree(args);

    if (tclStartupScriptFileName == NULL) {
	Tcl_ExternalToUtfDString(NULL, argv[0], -1, &argString);
    } else {
	tclStartupScriptFileName = Tcl_ExternalToUtfDString(NULL,
		tclStartupScriptFileName, -1, &argString);
    }

    TclFormatInt(buffer, argc-1);
    Tcl_SetVar(interp, "argc", buffer, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", Tcl_DStringValue(&argString), TCL_GLOBAL_ONLY);

    /*
     * Set the "tcl_interactive" variable.
     */

    tsdPtr->tty = isatty(0);
    Tcl_SetVar(interp, "tcl_interactive",
	    ((tclStartupScriptFileName == NULL) && tsdPtr->tty) ? "1" : "0",
	    TCL_GLOBAL_ONLY);
    
    /*
     * Invoke application-specific initialization.
     */

    if ((*appInitProc)(interp) != TCL_OK) {
	errChannel = Tcl_GetStdChannel(TCL_STDERR);
	if (errChannel) {
	    Tcl_WriteChars(errChannel,
		    "application-specific initialization failed: ", -1);
	    Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
	    Tcl_WriteChars(errChannel, "\n", 1);
	}
    }

#ifndef TCL_NO_FILESYSTEM
#ifndef TCL_NO_NONSTDCHAN
    /* IOS FIXME : See Tcl_EvalFile */
    /*
     * If a script file was specified then just source that file
     * and quit.
     */

    if (tclStartupScriptFileName != NULL) {
	code = Tcl_EvalFile(interp, tclStartupScriptFileName);
	if (code != TCL_OK) {
	    errChannel = Tcl_GetStdChannel(TCL_STDERR);
	    if (errChannel) {
		/*
		 * The following statement guarantees that the errorInfo
		 * variable is set properly.
		 */

		Tcl_AddErrorInfo(interp, "");
		Tcl_WriteObj(errChannel, Tcl_GetVar2Ex(interp, "errorInfo",
			NULL, TCL_GLOBAL_ONLY));
		Tcl_WriteChars(errChannel, "\n", 1);
	    }
	    exitCode = 1;
	}
	goto done;
    }
#endif
#endif
    Tcl_DStringFree(&argString);

    /*
     * We're running interactively.  Source a user-specific startup
     * file if the application specified one and if the file exists.
     */

    Tcl_SourceRCFile(interp);

    /*
     * Process commands from stdin until there's an end-of-file.  Note
     * that we need to fetch the standard channels again after every
     * eval, since they may have been changed.
     */

    commandPtr = Tcl_NewObj();
    Tcl_IncrRefCount(commandPtr);

    inChannel = Tcl_GetStdChannel(TCL_STDIN);
    outChannel = Tcl_GetStdChannel(TCL_STDOUT);
    gotPartial = 0;
    while (1) {
	if (tsdPtr->tty) {
	    Tcl_Obj *promptCmdPtr;

	    promptCmdPtr = Tcl_GetVar2Ex(interp,
		    (gotPartial ? "tcl_prompt2" : "tcl_prompt1"),
		    NULL, TCL_GLOBAL_ONLY);
	    if (promptCmdPtr == NULL) {
                defaultPrompt:
		if (!gotPartial && outChannel) {
		    Tcl_WriteChars(outChannel, "% ", 2);
		}
	    } else {
		code = Tcl_EvalObjEx(interp, promptCmdPtr, 0);
		inChannel = Tcl_GetStdChannel(TCL_STDIN);
		outChannel = Tcl_GetStdChannel(TCL_STDOUT);
		errChannel = Tcl_GetStdChannel(TCL_STDERR);
		if (code != TCL_OK) {
		    if (errChannel) {
			Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
			Tcl_WriteChars(errChannel, "\n", 1);
		    }
		    Tcl_AddErrorInfo(interp,
			    "\n    (script that generates prompt)");
		    goto defaultPrompt;
		}
	    }
	    if (outChannel) {
		Tcl_Flush(outChannel);
	    }
	}
	if (!inChannel) {
	    goto done;
	}
        length = Tcl_GetsObj(inChannel, commandPtr);
	if (length < 0) {
	    goto done;
	}
	if ((length == 0) && Tcl_Eof(inChannel) && (!gotPartial)) {
	    goto done;
	}

        /*
         * Add the newline removed by Tcl_GetsObj back to the string.
         */

	Tcl_AppendToObj(commandPtr, "\n", 1);
	if (!TclObjCommandComplete(commandPtr)) {
	    gotPartial = 1;
	    continue;
	}

	gotPartial = 0;
	code = Tcl_RecordAndEvalObj(interp, commandPtr, 0);
	inChannel = Tcl_GetStdChannel(TCL_STDIN);
	outChannel = Tcl_GetStdChannel(TCL_STDOUT);
	errChannel = Tcl_GetStdChannel(TCL_STDERR);
	Tcl_DecrRefCount(commandPtr);
	commandPtr = Tcl_NewObj();
	Tcl_IncrRefCount(commandPtr);
	if (code != TCL_OK) {
	    if (errChannel) {
		Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
		Tcl_WriteChars(errChannel, "\n", 1);
	    }
	} else if (tsdPtr->tty) {
	    resultPtr = Tcl_GetObjResult(interp);
	    Tcl_GetStringFromObj(resultPtr, &length);
	    if ((length > 0) && outChannel) {
		Tcl_WriteObj(outChannel, resultPtr);
		Tcl_WriteChars(outChannel, "\n", 1);
	    }
	}
	if (mainLoopProc != NULL) {
	    /*
	     * If a main loop has been defined while running interactively,
	     * we want to start a fileevent based prompt by establishing a
	     * channel handler for stdin.
	     */

#ifndef TCL_NO_FILEEVENTS
	    if (inChannel) {
		Tcl_CreateChannelHandler(inChannel, TCL_READABLE, StdinProc,
			(ClientData) inChannel);
	    }
#endif
	    if (tsdPtr->tty) {
		Prompt(interp, 0);
	    }
	    Tcl_DStringInit(&tsdPtr->command);
	    Tcl_DStringInit(&tsdPtr->line);

	    (*mainLoopProc)();
	    mainLoopProc = NULL;
	    break;
	}
#ifdef TCL_MEM_DEBUG
	if (tclMemDumpFileName != NULL) {
	    Tcl_DecrRefCount(commandPtr);
	    Tcl_DeleteInterp(interp);
	    Tcl_Exit(0);
	}
#endif
    }

    /*
     * Rather than calling exit, invoke the "exit" command so that
     * users can replace "exit" with some other command to do additional
     * cleanup on exit.  The Tcl_Eval call should never return.
     */

    done:
    if ((exitCode == 0) && (mainLoopProc != NULL)) {
	/*
	 * If everything has gone OK so far, call the main loop proc,
	 * if it exists.  Packages (like Tk) can set it to start processing
	 * events at this point.
	 */

	(*mainLoopProc)();
	mainLoopProc = NULL;
    }
    if (commandPtr != NULL) {
	Tcl_DecrRefCount(commandPtr);
    }
    sprintf(buffer, "exit %d", exitCode);
    Tcl_Eval(interp, buffer);
#ifdef PURIFY
    /*
     * Not reached unless exit is redefined.
     */
    Tcl_DeleteInterp(interp);
#endif
}

/*
 *---------------------------------------------------------------
 *
 * Tcl_SetMainLoop --
 *
 *	Sets an alternative main loop procedure.
 *
 * Results:
 *	Returns the previously defined main loop procedure.
 *
 * Side effects:
 *	This procedure will be called before Tcl exits, allowing for
 *	the creation of an event loop.
 *
 *---------------------------------------------------------------
 */

void
Tcl_SetMainLoop(proc)
    Tcl_MainLoopProc *proc;
{
    mainLoopProc = proc;
}

/*
 *----------------------------------------------------------------------
 *
 * StdinProc --
 *
 *	This procedure is invoked by the event dispatcher whenever
 *	standard input becomes readable.  It grabs the next line of
 *	input characters, adds them to a command being assembled, and
 *	executes the command if it's complete.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Could be almost arbitrary, depending on the command that's
 *	typed.
 *
 *----------------------------------------------------------------------
 */

#ifndef TCL_NO_FILEEVENTS
    /* ARGSUSED */
static void
StdinProc(clientData, mask)
    ClientData clientData;		/* Not used. */
    int mask;				/* Not used. */
{
    static int gotPartial = 0;
    char *cmd;
    int code, count;
    Tcl_Channel chan = (Tcl_Channel) clientData;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *) 
            Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    Tcl_Interp *interp = tsdPtr->interp;

    count = Tcl_Gets(chan, &tsdPtr->line);

    if (count < 0) {
	if (!gotPartial) {
	    if (tsdPtr->tty) {
		Tcl_Exit(0);
	    } else {
		Tcl_DeleteChannelHandler(chan, StdinProc, (ClientData) chan);
	    }
	    return;
	} 
    }

    (void) Tcl_DStringAppend(&tsdPtr->command, Tcl_DStringValue(
            &tsdPtr->line), -1);
    cmd = Tcl_DStringAppend(&tsdPtr->command, "\n", -1);
    Tcl_DStringFree(&tsdPtr->line);
    if (!Tcl_CommandComplete(cmd)) {
        gotPartial = 1;
        goto prompt;
    }
    gotPartial = 0;

    /*
     * Disable the stdin channel handler while evaluating the command;
     * otherwise if the command re-enters the event loop we might
     * process commands from stdin before the current command is
     * finished.  Among other things, this will trash the text of the
     * command being evaluated.
     */

    Tcl_CreateChannelHandler(chan, 0, StdinProc, (ClientData) chan);
    code = Tcl_RecordAndEval(interp, cmd, TCL_EVAL_GLOBAL);
    
    chan = Tcl_GetStdChannel(TCL_STDIN);
    if (chan) {
	Tcl_CreateChannelHandler(chan, TCL_READABLE, StdinProc,
		(ClientData) chan);
    }
    Tcl_DStringFree(&tsdPtr->command);
    if (Tcl_GetStringResult(interp)[0] != '\0') {
	if ((code != TCL_OK) || (tsdPtr->tty)) {
	    chan = Tcl_GetStdChannel(TCL_STDOUT);
	    if (chan) {
		Tcl_WriteObj(chan, Tcl_GetObjResult(interp));
		Tcl_WriteChars(chan, "\n", 1);
	    }
	}
    }

    /*
     * Output a prompt.
     */

    prompt:
    if (tsdPtr->tty) {
	Prompt(interp, gotPartial);
    }
    Tcl_ResetResult(interp);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Prompt --
 *
 *	Issue a prompt on standard output, or invoke a script
 *	to issue the prompt.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A prompt gets output, and a Tcl script may be evaluated
 *	in interp.
 *
 *----------------------------------------------------------------------
 */

static void
Prompt(interp, partial)
    Tcl_Interp *interp;			/* Interpreter to use for prompting. */
    int partial;			/* Non-zero means there already
					 * exists a partial command, so use
					 * the secondary prompt. */
{
    char *promptCmd;
    int code;
    Tcl_Channel outChannel, errChannel;

    promptCmd = Tcl_GetVar(interp,
	partial ? "tcl_prompt2" : "tcl_prompt1", TCL_GLOBAL_ONLY);
    if (promptCmd == NULL) {
defaultPrompt:
	if (!partial) {

            /*
             * We must check that outChannel is a real channel - it
             * is possible that someone has transferred stdout out of
             * this interpreter with "interp transfer".
             */

	    outChannel = Tcl_GetChannel(interp, "stdout", NULL);
            if (outChannel != (Tcl_Channel) NULL) {
                Tcl_WriteChars(outChannel, "% ", 2);
            }
	}
    } else {
	code = Tcl_Eval(interp, promptCmd);
	if (code != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		    "\n    (script that generates prompt)");
            /*
             * We must check that errChannel is a real channel - it
             * is possible that someone has transferred stderr out of
             * this interpreter with "interp transfer".
             */
            
	    errChannel = Tcl_GetChannel(interp, "stderr", NULL);
            if (errChannel != (Tcl_Channel) NULL) {
                Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
                Tcl_WriteChars(errChannel, "\n", 1);
            }
	    goto defaultPrompt;
	}
    }
    outChannel = Tcl_GetChannel(interp, "stdout", NULL);
    if (outChannel != (Tcl_Channel) NULL) {
	Tcl_Flush(outChannel);
    }
}
