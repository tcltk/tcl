/* 
 * tclLoadDyld.c --
 *
 *     This procedure provides a version of the TclLoadFile that
 *     works with Apple's dyld dynamic loading.  This file
 *     provided by Wilfredo Sanchez (wsanchez@apple.com).
 *     This works on Mac OS X.
 *
 * Copyright (c) 1995 Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclLoadDyld.c,v 1.2.2.3 2002/02/25 15:20:36 das Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
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
TclpLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr, clientDataPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
    ClientData *clientDataPtr;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * TclpUnloadFile() to unload the file. */
{
    NSSymbol symbol;
    const struct mach_header *dyld_lib;
    Tcl_DString newName, ds;
    char *native;

    native = Tcl_UtfToExternalDString(NULL, fileName, -1, &ds);
    dyld_lib = NSAddImage(native, 
        NSADDIMAGE_OPTION_WITH_SEARCHING | 
        NSADDIMAGE_OPTION_RETURN_ON_ERROR);
    Tcl_DStringFree(&ds);
    
    if (!dyld_lib) {
        NSLinkEditErrors editError;
        char *name, *msg;
        NSLinkEditError(&editError, &errno, &name, &msg);
        Tcl_AppendResult(interp, msg, (char *) NULL);
        return TCL_ERROR;
    }

    /* 
     * dyld adds an underscore to the beginning of symbol names.
     */

    native = Tcl_UtfToExternalDString(NULL, sym1, -1, &ds);
    Tcl_DStringInit(&newName);
    Tcl_DStringAppend(&newName, "_", 1);
    native = Tcl_DStringAppend(&newName, native, -1);
    symbol = NSLookupSymbolInImage(dyld_lib, native, 
        NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_NOW | 
        NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if(symbol) {
        *proc1Ptr = NSAddressOfSymbol(symbol);
        *clientDataPtr = NSModuleForSymbol(symbol);
    } else {
        *proc1Ptr=NULL;
        *clientDataPtr=NULL;
    }
    Tcl_DStringFree(&newName);
    Tcl_DStringFree(&ds);

    native = Tcl_UtfToExternalDString(NULL, sym2, -1, &ds);
    Tcl_DStringInit(&newName);
    Tcl_DStringAppend(&newName, "_", 1);
    native = Tcl_DStringAppend(&newName, native, -1);
    symbol = NSLookupSymbolInImage(dyld_lib, native, 
        NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_NOW | 
        NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if(symbol) {
        *proc2Ptr = NSAddressOfSymbol(symbol);
    } else {
        *proc2Ptr=NULL;
    }
    Tcl_DStringFree(&newName);
    Tcl_DStringFree(&ds);
    
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
    NSUnLinkModule(clientData, FALSE);
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
