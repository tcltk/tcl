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
 * RCS: @(#) $Id: tclLoadDyld.c,v 1.14.2.3 2005/06/04 07:05:14 das Exp $
 */

#include "tclInt.h"
#include "tclPort.h"
#include <mach-o/dyld.h>
#undef panic
#include <mach/mach.h>

typedef struct Tcl_DyldModuleHandle {
    struct Tcl_DyldModuleHandle *nextModuleHandle;
    NSModule module;
} Tcl_DyldModuleHandle;

typedef struct Tcl_DyldLoadHandle {
    CONST struct mach_header *dyld_lib;
    Tcl_DyldModuleHandle *firstModuleHandle;
} Tcl_DyldLoadHandle;

#ifdef TCL_LOAD_FROM_MEMORY
typedef struct ThreadSpecificData {
     int haveLoadMemory;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;
#endif

/*
 *----------------------------------------------------------------------
 *
 * DyldOFIErrorMsg --
 *
 *	Converts a numerical NSObjectFileImage error into an
 *	error message string.
 *
 * Results:
 *     Error message string. 
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

static CONST char* DyldOFIErrorMsg(int err) {
    CONST char *ofi_msg = NULL;
    
    if (err != NSObjectFileImageSuccess) {
        switch(err) {
        case NSObjectFileImageFailure:
            ofi_msg = "object file setup failure";
            break;
        case NSObjectFileImageInappropriateFile:
            ofi_msg = "not a Mach-O MH_BUNDLE file";
            break;
        case NSObjectFileImageArch:
            ofi_msg = "no object for this architecture";
            break;
        case NSObjectFileImageFormat:
            ofi_msg = "bad object file format";
            break;
        case NSObjectFileImageAccess:
            ofi_msg = "can't read object file";
            break;
        default:
            ofi_msg = "unknown error";
            break;
        }
    }
    return ofi_msg;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpDlopen --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	a handle to the new code.
 *
 * Results:
 *     A standard Tcl completion code.  If an error occurs, an error
 *     message is left in the interpreter's result. 
 *
 * Side effects:
 *     New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclpDlopen(interp, pathPtr, loadHandle, unloadProcPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tcl_Obj *pathPtr;		/* Name of the file containing the desired
				 * code (UTF-8). */
    Tcl_LoadHandle *loadHandle;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr;	
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for
				 * this file. */
{
    Tcl_DyldLoadHandle *dyldLoadHandle;
    CONST struct mach_header *dyld_lib;
    NSObjectFileImage dyld_ofi = NULL;
    Tcl_DyldModuleHandle *dyldModuleHandle = NULL;
    CONST char *native;

    /* 
     * First try the full path the user gave us.  This is particularly
     * important if the cwd is inside a vfs, and we are trying to load
     * using a relative path.
     */
    native = Tcl_FSGetNativePath(pathPtr);
    dyld_lib = NSAddImage(native, 
			  NSADDIMAGE_OPTION_WITH_SEARCHING | 
			  NSADDIMAGE_OPTION_RETURN_ON_ERROR);
    
    if (!dyld_lib) {
        NSLinkEditErrors editError;
        CONST char *name, *msg, *ofi_msg = NULL;
        
        NSLinkEditError(&editError, &errno, &name, &msg);
        if (editError == NSLinkEditFileAccessError) {
            /* The requested file was not found: 
             * let the OS loader examine the binary search path for
             * whatever string the user gave us which hopefully refers
             * to a file on the binary path
             */
            Tcl_DString ds;
            char *fileName = Tcl_GetString(pathPtr);
            CONST char *native = Tcl_UtfToExternalDString(NULL, fileName, -1, &ds);
            dyld_lib = NSAddImage(native, 
                                  NSADDIMAGE_OPTION_WITH_SEARCHING | 
                                  NSADDIMAGE_OPTION_RETURN_ON_ERROR);
            Tcl_DStringFree(&ds);
            if (!dyld_lib) {
                NSLinkEditError(&editError, &errno, &name, &msg);
            }
        } else if ((editError == NSLinkEditFileFormatError && errno == EBADMACHO)) {
            /* The requested file was found but was not of type MH_DYLIB, 
             * attempt to load it as a MH_BUNDLE: */
            NSObjectFileImageReturnCode err;
            err = NSCreateObjectFileImageFromFile(native, &dyld_ofi);
            ofi_msg = DyldOFIErrorMsg(err);
         }
        if (!dyld_lib && !dyld_ofi) {
            Tcl_AppendResult(interp, msg, (char *) NULL);
            if (ofi_msg) {
                Tcl_AppendResult(interp, "NSCreateObjectFileImageFromFile() error: ",
                        ofi_msg, (char *) NULL);
            }
            return TCL_ERROR;
        }
    }
    
    if (dyld_ofi) {
        NSModule module;
        module = NSLinkModule(dyld_ofi, native, NSLINKMODULE_OPTION_BINDNOW |
                                                NSLINKMODULE_OPTION_RETURN_ON_ERROR);
        NSDestroyObjectFileImage(dyld_ofi);
        if (module) {
            dyldModuleHandle = (Tcl_DyldModuleHandle *) 
                    ckalloc(sizeof(Tcl_DyldModuleHandle));
            if (!dyldModuleHandle) return TCL_ERROR;
            dyldModuleHandle->module = module;
            dyldModuleHandle->nextModuleHandle = NULL;
        } else {
            NSLinkEditErrors editError;
            CONST char *name, *msg;
            NSLinkEditError(&editError, &errno, &name, &msg);
            Tcl_AppendResult(interp, msg, (char *) NULL);
            return TCL_ERROR;
        }
    }
    dyldLoadHandle = (Tcl_DyldLoadHandle *) ckalloc(sizeof(Tcl_DyldLoadHandle));
    if (!dyldLoadHandle) return TCL_ERROR;
    dyldLoadHandle->dyld_lib = dyld_lib;
    dyldLoadHandle->firstModuleHandle = dyldModuleHandle;
    *loadHandle = (Tcl_LoadHandle) dyldLoadHandle;
    *unloadProcPtr = &TclpUnloadFile;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFindSymbol --
 *
 *	Looks up a symbol, by name, through a handle associated with
 *	a previously loaded piece of code (shared library).
 *
 * Results:
 *	Returns a pointer to the function associated with 'symbol' if
 *	it is found.  Otherwise returns NULL and may leave an error
 *	message in the interp's result.
 *
 *----------------------------------------------------------------------
 */
Tcl_PackageInitProc*
TclpFindSymbol(interp, loadHandle, symbol) 
    Tcl_Interp *interp;
    Tcl_LoadHandle loadHandle;
    CONST char *symbol;
{
    NSSymbol nsSymbol;
    CONST char *native;
    Tcl_DString newName, ds;
    Tcl_PackageInitProc* proc = NULL;
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *) loadHandle;
    /* 
     * dyld adds an underscore to the beginning of symbol names.
     */
    native = Tcl_UtfToExternalDString(NULL, symbol, -1, &ds);
    Tcl_DStringInit(&newName);
    Tcl_DStringAppend(&newName, "_", 1);
    native = Tcl_DStringAppend(&newName, native, -1);
    if (dyldLoadHandle->dyld_lib) {
        nsSymbol = NSLookupSymbolInImage(dyldLoadHandle->dyld_lib, native, 
            NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_NOW | 
            NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
        if(nsSymbol) {
            /* until dyld supports unloading of MY_DYLIB binaries, the
             * following is not needed: */
#ifdef DYLD_SUPPORTS_DYLIB_UNLOADING
            NSModule module = NSModuleForSymbol(nsSymbol);
            Tcl_DyldModuleHandle *dyldModuleHandle = dyldLoadHandle->firstModuleHandle;
            while (dyldModuleHandle) {
                if (module == dyldModuleHandle->module) break;
                dyldModuleHandle = dyldModuleHandle->nextModuleHandle;
            }
            if (!dyldModuleHandle) {
                dyldModuleHandle = (Tcl_DyldModuleHandle *)
                        ckalloc(sizeof(Tcl_DyldModuleHandle));
                if (dyldModuleHandle) {
                    dyldModuleHandle->module = module;
                    dyldModuleHandle->nextModuleHandle = 
                            dyldLoadHandle->firstModuleHandle;
                    dyldLoadHandle->firstModuleHandle = dyldModuleHandle;
                }
            }
#endif /* DYLD_SUPPORTS_DYLIB_UNLOADING */
       } else {
            NSLinkEditErrors editError;
            CONST char *name, *msg;
            NSLinkEditError(&editError, &errno, &name, &msg);
            Tcl_AppendResult(interp, msg, (char *) NULL);
        }
    } else {
        nsSymbol = NSLookupSymbolInModule(dyldLoadHandle->firstModuleHandle->module, 
                                          native);
    }
    if(nsSymbol) {
        proc = NSAddressOfSymbol(nsSymbol);
    }
    Tcl_DStringFree(&newName);
    Tcl_DStringFree(&ds);
    
    return proc;
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
 *     Note that dyld currently only supports unloading of binaries of
 *     type MH_BUNDLE loaded with NSLinkModule() in TclpDlopen() above.
 *
 *----------------------------------------------------------------------
 */

void
TclpUnloadFile(loadHandle)
    Tcl_LoadHandle loadHandle;	/* loadHandle returned by a previous call
				 * to TclpDlopen().  The loadHandle is 
				 * a token that represents the loaded 
				 * file. */
{
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *) loadHandle;
    Tcl_DyldModuleHandle *dyldModuleHandle = dyldLoadHandle->firstModuleHandle;
    void *ptr;

    while (dyldModuleHandle) {
	NSUnLinkModule(dyldModuleHandle->module, 
	               NSUNLINKMODULE_OPTION_RESET_LAZY_REFERENCES);
	ptr = dyldModuleHandle;
	dyldModuleHandle = dyldModuleHandle->nextModuleHandle;
	ckfree(ptr);
    }
    ckfree((char*) dyldLoadHandle);
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
    CONST char *fileName;      /* Name of file containing package (already
				* translated to local form if needed). */
    Tcl_DString *bufPtr;       /* Initialized empty dstring.  Append
				* package name to this if possible. */
{
    return 0;
}

#ifdef TCL_LOAD_FROM_MEMORY
/*
 *----------------------------------------------------------------------
 *
 * TclpLoadMemoryGetBuffer --
 *
 *	Allocate a buffer that can be used with TclpLoadMemory() below.
 *
 * Results:
 *     Pointer to allocated buffer or NULL if an error occurs.
 *
 * Side effects:
 *     Buffer is allocated.
 *
 *----------------------------------------------------------------------
 */

void*
TclpLoadMemoryGetBuffer(interp, size)
    Tcl_Interp *interp;		/* Used for error reporting. */
    int size;                   /* Size of desired buffer */
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    void * buffer = NULL;
    
    if (!tsdPtr->haveLoadMemory) {
        /* NSCreateObjectFileImageFromMemory is available but always 
         * fails prior to Darwin 7 */
        struct utsname name;
        if (!uname(&name)) {
            long release = strtol(name.release, NULL, 10);
            tsdPtr->haveLoadMemory = (release >= 7) ? 1 : -1;
        }
    }
    if (tsdPtr->haveLoadMemory > 0) {
        /* We must allocate the  buffer using vm_allocate, because
         * NSCreateObjectFileImageFromMemory  will dispose of it
         * using vm_deallocate.
         */
        int err = vm_allocate(mach_task_self(), 
                              (vm_address_t*)&buffer, size, 1);
        if (err) {
            buffer = NULL;
        }
    }
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpLoadMemory --
 *
 *	Dynamically loads binary code file from memory and returns
 *	a handle to the new code.
 *
 * Results:
 *     A standard Tcl completion code.  If an error occurs, an error
 *     message is left in the interpreter's result. 
 *
 * Side effects:
 *     New code is loaded from memory.
 *
 *----------------------------------------------------------------------
 */

int
TclpLoadMemory(interp, buffer, size, codeSize, loadHandle, unloadProcPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    void *buffer;		/* Buffer containing the desired code
				 * (allocated with TclpLoadMemoryGetBuffer). */
    int size;                   /* Allocation size of buffer. */
    int codeSize;               /* Size of code data read into buffer or -1 if
                                 * an error occurred and the buffer should
                                 * just be freed. */
    Tcl_LoadHandle *loadHandle;	/* Filled with token for dynamically loaded
				 * file which will be passed back to 
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr;	
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for
				 * this file. */
{
    Tcl_DyldLoadHandle *dyldLoadHandle;
    NSObjectFileImage dyld_ofi = NULL;
    Tcl_DyldModuleHandle *dyldModuleHandle;
    CONST char *ofi_msg = NULL;

    if (codeSize >= 0) {
        NSObjectFileImageReturnCode err;
        err = NSCreateObjectFileImageFromMemory(buffer, codeSize, &dyld_ofi);
        ofi_msg = DyldOFIErrorMsg(err);
    }
    if (!dyld_ofi) {
        vm_deallocate(mach_task_self(), (vm_address_t) buffer, size);
        if (ofi_msg) {
            Tcl_AppendResult(interp, "NSCreateObjectFileImageFromFile() error: ",
                    ofi_msg, (char *) NULL);
        }
        return TCL_ERROR;
    } else {
        NSModule module;
        module = NSLinkModule(dyld_ofi, "[Memory Based Bundle]", 
                NSLINKMODULE_OPTION_BINDNOW |NSLINKMODULE_OPTION_RETURN_ON_ERROR);
        NSDestroyObjectFileImage(dyld_ofi);
        if (module) {
            dyldModuleHandle = (Tcl_DyldModuleHandle *) 
                    ckalloc(sizeof(Tcl_DyldModuleHandle));
            if (!dyldModuleHandle) return TCL_ERROR;
            dyldModuleHandle->module = module;
            dyldModuleHandle->nextModuleHandle = NULL;
        } else {
            NSLinkEditErrors editError;
            CONST char *name, *msg;
            NSLinkEditError(&editError, &errno, &name, &msg);
            Tcl_AppendResult(interp, msg, (char *) NULL);
            return TCL_ERROR;
        }
    }
    dyldLoadHandle = (Tcl_DyldLoadHandle *) ckalloc(sizeof(Tcl_DyldLoadHandle));
    if (!dyldLoadHandle) return TCL_ERROR;
    dyldLoadHandle->dyld_lib = NULL;
    dyldLoadHandle->firstModuleHandle = dyldModuleHandle;
    *loadHandle = (Tcl_LoadHandle) dyldLoadHandle;
    *unloadProcPtr = &TclpUnloadFile;
    return TCL_OK;
}
#endif
