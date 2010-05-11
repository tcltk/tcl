/*
 * tclWinLoad.c --
 *
 *	This function provides a version of the TclLoadFile that works with
 *	the Windows "LoadLibrary" and "GetProcAddress" API for dynamic
 *	loading.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinLoad.c,v 1.30 2010/05/11 14:47:12 nijtmans Exp $
 */

#include "tclWinInt.h"

/*
 * Mutex protecting static data in this file;
 */

static Tcl_Mutex loadMutex;

/*
 * Name of the directory in the native filesystem where DLLs used in this
 * process are copied prior to loading.
 */

static WCHAR* dllDirectoryName = NULL;

/* Static functions defined within this file */

void* FindSymbol(Tcl_Interp* interp, Tcl_LoadHandle loadHandle,
		 const char* symbol);
void UnloadFile(Tcl_LoadHandle loadHandle);


/*
 *----------------------------------------------------------------------
 *
 * TclpDlopen --
 *
 *	Dynamically loads a binary code file into memory and returns a handle
 *	to the new code.
 *
 * Results:
 *	A standard Tcl completion code. If an error occurs, an error message
 *	is left in the interp's result.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclpDlopen(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Obj *pathPtr,		/* Name of the file containing the desired
				 * code (UTF-8). */
    Tcl_LoadHandle *loadHandle,	/* Filled with token for dynamically loaded
				 * file which will be passed back to
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr)
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for this
				 * file. */
{
    HINSTANCE hInstance;
    const TCHAR *nativeName;
    Tcl_LoadHandle handlePtr;

    /*
     * First try the full path the user gave us. This is particularly
     * important if the cwd is inside a vfs, and we are trying to load using a
     * relative path.
     */

    nativeName = Tcl_FSGetNativePath(pathPtr);
    hInstance = tclWinProcs->loadLibraryProc(nativeName);
    if (hInstance == NULL) {
	/*
	 * Let the OS loader examine the binary search path for whatever
	 * string the user gave us which hopefully refers to a file on the
	 * binary path.
	 */

	Tcl_DString ds;
	const char *fileName = Tcl_GetString(pathPtr);

	nativeName = tclWinProcs->utf2tchar(fileName, -1, &ds);
	hInstance = tclWinProcs->loadLibraryProc(nativeName);
	Tcl_DStringFree(&ds);
    }

    if (hInstance == NULL) {
	DWORD lastError = GetLastError();

#if 0
	/*
	 * It would be ideal if the FormatMessage stuff worked better, but
	 * unfortunately it doesn't seem to want to...
	 */

	LPTSTR lpMsgBuf;
	char *buf;
	int size;

	size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, lastError, 0,
		(LPTSTR) &lpMsgBuf, 0, NULL);
	buf = (char *) ckalloc((unsigned) TCL_INTEGER_SPACE + size + 1);
	sprintf(buf, "%d %s", lastError, (char *)lpMsgBuf);
#endif

	Tcl_AppendResult(interp, "couldn't load library \"",
		Tcl_GetString(pathPtr), "\": ", NULL);

	/*
	 * Check for possible DLL errors. This doesn't work quite right,
	 * because Windows seems to only return ERROR_MOD_NOT_FOUND for just
	 * about any problem, but it's better than nothing. It'd be even
	 * better if there was a way to get what DLLs
	 */

	switch (lastError) {
	case ERROR_MOD_NOT_FOUND:
	case ERROR_DLL_NOT_FOUND:
	    Tcl_AppendResult(interp, "this library or a dependent library"
		    " could not be found in library path", NULL);
	    break;
	case ERROR_PROC_NOT_FOUND:
	    Tcl_AppendResult(interp, "A function specified in the import"
		    " table could not be resolved by the system.  Windows"
		    " is not telling which one, I'm sorry.", NULL);
	    break;
	case ERROR_INVALID_DLL:
	    Tcl_AppendResult(interp, "this library or a dependent library"
		    " is damaged", NULL);
	    break;
	case ERROR_DLL_INIT_FAILED:
	    Tcl_AppendResult(interp, "the library initialization"
		    " routine failed", NULL);
	    break;
	default:
	    TclWinConvertError(lastError);
	    Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
	}
	return TCL_ERROR;
    } else {
	handlePtr = 
	    (Tcl_LoadHandle) ckalloc(sizeof(struct Tcl_LoadHandle_));
	handlePtr->clientData = (ClientData) hInstance;
	handlePtr->findSymbolProcPtr = &FindSymbol;
	handlePtr->unloadFileProcPtr = &UnloadFile;
	*loadHandle = handlePtr;
	*unloadProcPtr = &UnloadFile;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindSymbol --
 *
 *	Looks up a symbol, by name, through a handle associated with a
 *	previously loaded piece of code (shared library).
 *
 * Results:
 *	Returns a pointer to the function associated with 'symbol' if it is
 *	found. Otherwise returns NULL and may leave an error message in the
 *	interp's result.
 *
 *----------------------------------------------------------------------
 */

void *
FindSymbol(
    Tcl_Interp *interp,
    Tcl_LoadHandle loadHandle,
    const char *symbol)
{
    Tcl_PackageInitProc *proc = NULL;
    HINSTANCE hInstance = (HINSTANCE)(loadHandle->clientData);

    /*
     * For each symbol, check for both Symbol and _Symbol, since Borland
     * generates C symbols with a leading '_' by default.
     */

    proc = (void*) GetProcAddress(hInstance, symbol);
    if (proc == NULL) {
	Tcl_DString ds;
	const char* sym2;
	Tcl_DStringInit(&ds);
	Tcl_DStringAppend(&ds, "_", 1);
	sym2 = Tcl_DStringAppend(&ds, symbol, -1);
	proc = (Tcl_PackageInitProc *) GetProcAddress(hInstance, sym2);
	Tcl_DStringFree(&ds);
    }
    if (proc == NULL && interp != NULL) {
	Tcl_AppendResult(interp, "cannot find symbol \"", symbol, "\"", NULL);
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "LOAD_SYMBOL", symbol, NULL);
    }
    return proc;
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadFile --
 *
 *	Unloads a dynamically loaded binary code file from memory. Code
 *	pointers in the formerly loaded file are no longer valid after calling
 *	this function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Code removed from memory.
 *
 *----------------------------------------------------------------------
 */

void
UnloadFile(
    Tcl_LoadHandle loadHandle)	/* loadHandle returned by a previous call to
				 * TclpDlopen(). The loadHandle is a token
				 * that represents the loaded file. */
{
    HINSTANCE hInstance = (HINSTANCE) loadHandle->clientData;
    FreeLibrary(hInstance);
    ckfree((char*) loadHandle);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package name,
 *	this function is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a package
 *	name; generic code will then try to guess the package from the file
 *	name. A return value of 1 would have meant that we figured out the
 *	package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(
    const char *fileName,	/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr)	/* Initialized empty dstring. Append package
				 * name to this if possible. */
{
    return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * TclpTempFileNameForLibrary --
 *
 *	Constructs a temporary file name for loading a shared object (DLL).
 *
 * Results:
 *	Returns the constructed file name.
 *
 * On Windows, a DLL is identified by the final component of its path name.
 * Cross linking among DLL's (and hence, preloading) will not work unless
 * this name is preserved when copying a DLL from a VFS to a temp file for
 * preloading. For this reason, all DLLs in a given process are copied
 * to a temp directory, and their names are preserved.
 *
 *-----------------------------------------------------------------------------
 */

Tcl_Obj*
TclpTempFileNameForLibrary(Tcl_Interp* interp, /* Tcl interpreter */
			   Tcl_Obj* path)      /* Path name of the DLL in
						* the VFS */
{
    size_t nameLen;		/* Length of the temp folder name */
    WCHAR name[MAX_PATH];	/* Path name of the temp folder */
    BOOL status;		/* Status from Win32 API calls */
    Tcl_Obj* fileName;		/* Name of the temp file */
    Tcl_Obj* tail;		/* Tail of the source path */

    /*
     * Determine the name of the directory to use, and create it.
     * (Keep trying with new names until an attempt to create the directory
     * succeeds)
     */

    nameLen = 0;
    if (dllDirectoryName == NULL) {
	Tcl_MutexLock(&loadMutex);
	if (dllDirectoryName == NULL) {
	    nameLen = GetTempPathW(MAX_PATH, name);
	    if (nameLen >= MAX_PATH-12) {
		Tcl_SetErrno(ENAMETOOLONG);
		nameLen = 0;
	    } else {
		wcscpy(name+nameLen, L"TCLXXXXXXXX");
		nameLen += 11;
	    }
	    status = 1;
	    if (nameLen != 0) {
		DWORD id;
		int i = 0;
		id = GetCurrentProcessId();
		for (;;) {
		    DWORD lastError;
		    wsprintfW(name+nameLen-8, L"%08x", id);
		    status = CreateDirectoryW(name, NULL);
		    if (status) {
			break;
		    }
		    if ((lastError = GetLastError()) != ERROR_ALREADY_EXISTS) {
			TclWinConvertError(lastError);
			break;
		    } else if (++i > 256) {
			TclWinConvertError(lastError);
			break;
		    }
		    id *= 16777619;
		}
	    }
	    if (status != 0) {
		dllDirectoryName = (WCHAR*)
		    ckalloc((nameLen+1) * sizeof(WCHAR));
		wcscpy(dllDirectoryName, name);
	    }
	}
	Tcl_MutexUnlock(&loadMutex);
    }
    if (dllDirectoryName == NULL) {
	Tcl_AppendResult(interp, "couldn't create temporary directory: ",
			 Tcl_PosixError(interp), NULL);
    }
    fileName = TclpNativeToNormalized(dllDirectoryName);
    tail = TclPathPart(interp, path, TCL_PATH_TAIL);
    if (tail == NULL) {
	Tcl_DecrRefCount(fileName);
	return NULL;
    } else {
	Tcl_AppendToObj(fileName, "/", 1);
	Tcl_AppendObjToObj(fileName, tail);
	return fileName;
    }    
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
