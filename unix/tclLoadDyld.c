/* 
 * tclLoadDyld.c --
 *
 *     This procedure provides a version of the TclLoadFile that
 *     works with NeXT/Apple's dyld dynamic loading.  This file
 *     provided by Wilfredo Sanchez (wsanchez@apple.com).
 *     The works on Mac OS X and Mac OS X Server.
 *     It should work with OpenStep, but it's not been tried.
 *
 * Copyright (c) 1995 Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclLoadDyld.c,v 1.5.2.1 2001/10/15 09:13:49 wolfsuit Exp $
 */

#include "tclInt.h"
#include <mach-o/dyld.h>

/*
 *----------------------------------------------------------------------
 *
 * TclpLoadFile --
 *
 *     Dynamically loads a binary code file into memory and returns
 *     the addresses of two procedures within that file, if they
 *     are defined.
 *
 * Results:
 *     A standard Tcl completion code.  If an error occurs, an error
 *     message is left in the interpreter's result.  *proc1Ptr and *proc2Ptr
 *     are filled in with the addresses of the symbols given by
 *     *sym1 and *sym2, or NULL if those symbols can't be found.
 *
 * Side effects:
 *     New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclpLoadFile(interp, pathPtr, sym1, sym2, proc1Ptr, proc2Ptr, 
	     clientDataPtr, unloadProcPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *pathPtr;		/* Name of the file containing the desired
				 * code. */
    CONST char *sym1, *sym2;	/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
    ClientData *clientDataPtr;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr;	
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for
				 * this file. */
{
    NSSymbol symbol;
    enum DYLD_BOOL dyld_return;
    CONST char *native;
    char *name;

    native = Tcl_FSGetNativePath(pathPtr);

    dyld_return = NSAddLibrary(native);
    
    if (dyld_return !=  TRUE) {
      Tcl_SetResult (interp, "dyld: Couldn't add library", TCL_STATIC);
      return TCL_ERROR;
    }

    name = (char*)malloc(sizeof(char)*(strlen(sym1)+2));
    sprintf(name, "_%s", sym1);
    symbol = NSLookupAndBindSymbol(name);
    free(name);
    *proc1Ptr = NSAddressOfSymbol(symbol);

    name = (char*)malloc(sizeof(char)*(strlen(sym2)+2));
    sprintf(name, "_%s", sym2);
    symbol = NSLookupAndBindSymbol(name);
    free(name);
    *proc2Ptr = NSAddressOfSymbol(symbol);

    *clientDataPtr = NULL;
    *unloadProcPtr = &TclpUnloadFile;
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpUnloadFile --
 *
 *     Unloads a dynamically loaded binary code file from memory.
 *     Code pointers in the formerly loaded file are no longer valid
 *     after calling this function.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Code dissapears from memory.
 *     Note that this is a no-op on older (OpenStep) versions of dyld.
 *
 *----------------------------------------------------------------------
 */

void
TclpUnloadFile(clientData)
    ClientData clientData;	/* ClientData returned by a previous call
				 * to TclpLoadFile().  The clientData is 
				 * a token that represents the loaded 
				 * file. */
{
  /* Unloading Libraries is not currently supported by dyld */
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *     If the "load" command is invoked without providing a package
 *     name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *     Always returns 0 to indicate that we couldn't figure out a
 *     package name;  generic code will then try to guess the package
 *     from the file name.  A return value of 1 would have meant that
 *     we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;	       /* Name of file containing package (already
				* translated to local form if needed). */
    Tcl_DString *bufPtr;       /* Initialized empty dstring.  Append
				* package name to this if possible. */
{
    return 0;
}
