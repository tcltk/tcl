/* 
 * tclWinInit.c --
 *
 *	Contains the Windows-specific interpreter initialization functions.
 *
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinInit.c,v 1.1.2.4 1998/11/11 04:08:39 stanton Exp $
 */

#include "tclWinInt.h"
#include <winreg.h>
#include <winnt.h>
#include <winbase.h>

/*
 * The following macro can be defined at compile time to specify
 * the root of the Tcl registry keys.
 */
 
#ifndef TCL_REGISTRY_KEY
#define TCL_REGISTRY_KEY "Software\\Scriptics\\Tcl\\" TCL_VERSION
#endif

/*
 * The following declaration is a workaround for some Microsoft brain damage.
 * The SYSTEM_INFO structure is different in various releases, even though the
 * layout is the same.  So we overlay our own structure on top of it so we
 * can access the interesting slots in a uniform way.
 */

typedef struct {
    WORD wProcessorArchitecture;
    WORD wReserved;
} OemId;

/*
 * The following macros are missing from some versions of winnt.h.
 */

#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL 0
#endif
#ifndef PROCESSOR_ARCHITECTURE_MIPS
#define PROCESSOR_ARCHITECTURE_MIPS  1
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA
#define PROCESSOR_ARCHITECTURE_ALPHA 2
#endif
#ifndef PROCESSOR_ARCHITECTURE_PPC
#define PROCESSOR_ARCHITECTURE_PPC   3
#endif
#ifndef PROCESSOR_ARCHITECTURE_UNKNOWN
#define PROCESSOR_ARCHITECTURE_UNKNOWN 0xFFFF
#endif

/*
 * The following arrays contain the human readable strings for the Windows
 * platform and processor values.
 */


#define NUMPLATFORMS 3
static char* platforms[NUMPLATFORMS] = {
    "Win32s", "Windows 95", "Windows NT"
};

#define NUMPROCESSORS 4
static char* processors[NUMPROCESSORS] = {
    "intel", "mips", "alpha", "ppc"
};

/*
 * The Init script (common to Windows and Unix platforms) is
 * defined in tkInitScript.h
 */

#include "tclInitScript.h"

static void		AppendEnvironment(Tcl_Obj *listPtr, CONST char *lib);
static void		AppendPath(Tcl_Obj *listPtr, HMODULE hModule,
			    CONST char *lib);
static void		AppendRegistry(Tcl_Obj *listPtr, CONST char *lib);
static int		ToUtf(CONST WCHAR *wSrc, char *dst);



/*
 *---------------------------------------------------------------------------
 *
 * TclpInitPlatform --
 *
 *	Initialize all the platform-dependant things like signals and
 *	floating-point error handling.
 *
 *	Called at process initialization time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclpInitPlatform()
{
    tclPlatform = TCL_PLATFORM_WINDOWS;

    /*
     * The following code stops Windows 3.X and Windows NT 3.51 from 
     * automatically putting up Sharing Violation dialogs, e.g, when 
     * someone tries to access a file that is locked or a drive with no 
     * disk in it.  Tcl already returns the appropriate error to the 
     * caller, and they can decide to put up their own dialog in response 
     * to that failure.  
     *
     * Under 95 and NT 4.0, this is a NOOP because the system doesn't 
     * automatically put up dialogs when the above operations fail.
     */

    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpInitLibraryPath --
 *
 *	Initialize the library path at startup.  
 *
 *	This call sets the library path to strings in UTF-8. Any 
 *	pre-existing library path information is assumed to have been 
 *	in the native multibyte encoding.
 *
 *	Called at process initialization time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclpInitLibraryPath(argv0)
    CONST char *argv0;		/* Name of executable from argv[0] to main().
				 * Not used because we can determine the name
				 * by querying the module handle. */
{
#define LIBRARY_SIZE	    32
    Tcl_Obj *pathPtr, *objPtr;
    char installLib[LIBRARY_SIZE], developLib[LIBRARY_SIZE];

    pathPtr = Tcl_NewObj();

    /*
     * set installLib lib/tcl[info tclversion]
     *
     * if {[string match {*[ab]*} [info patchlevel]} {
     *	   set developLib ../tcl[info patchlevel]/library
     * } else {
     *     set developLib ../tcl[info tclversion]/library
     * }
     */
     
    sprintf(installLib, "lib/tcl%s", TCL_VERSION);
    sprintf(developLib, "../tcl%s/library",
	    ((TCL_RELEASE_LEVEL < 2) ? TCL_PATCH_LEVEL : TCL_VERSION));

    /*
     * if {[info exists $env(TCL_LIBRARY)]} {
     *     lappend dirs $env(TCL_LIBRARY)
     *     set split [file split $TCL_LIBRARY]
     *	   set tail [lindex [file split $installLib] end]
     *     if {[string tolower [lindex $split end]] != $tail} {
     *         set split [lreplace $split end end $tail]
     *         lappend dirs [eval file join $split]
     *     }
     * }
     */

    AppendEnvironment(pathPtr, installLib);

    /*
     * if {[info exists $auto_path]} {
     *     eval lappend dirs $auto_path
     * }
     */

    objPtr = TclGetLibraryPath();
    if (objPtr != NULL) {
	int objc;
	Tcl_Obj **objv;
	int i, length;
	char *str;
	char tmp[MAX_PATH * TCL_UTF_MAX];
	WCHAR wBuf[MAX_PATH];

	objc = 0;
	Tcl_ListObjGetElements(NULL, pathPtr, &objc, &objv);
	for (i = 0; i < objc; i++) {
	    str = Tcl_GetStringFromObj(objv[i], &length);
	    length = MultiByteToWideChar(CP_ACP, 0, str, length, wBuf, 
		    MAX_PATH);
	    Tcl_SetStringObj(objv[i], tmp, ToUtf(wBuf, tmp));
	}
	Tcl_ListObjAppendList(NULL, pathPtr, objPtr);
    }

    /*
     * if {[info nameofexecutable] != ""} {
     *     set prefix [file dirname [file dirname [info nameofexecutable]]]
     *     lappend dirs $prefix/$installLib
     *     lappend dirs $prefix/$developLib
     * }
     */

    AppendPath(pathPtr, NULL, installLib);
    AppendPath(pathPtr, NULL, developLib);
    AppendPath(pathPtr, NULL, NULL);
     
    /*
     * if {[info nameoflibrary] != ""} {
     *     lappend dirs [file dirname [info nameoflibrary]]/$installLib
     * }
     */

    AppendPath(pathPtr, TclWinGetTclInstance(), installLib);
    AppendPath(pathPtr, TclWinGetTclInstance(), NULL);

    AppendRegistry(pathPtr, installLib);
    TclSetLibraryPath(pathPtr);
}

static void
AppendEnvironment(
    Tcl_Obj *listPtr,
    CONST char *lib)
{
    int pathc;
    WCHAR wBuf[MAX_PATH];
    char buf[MAX_PATH * TCL_UTF_MAX];
    Tcl_Obj *objPtr;
    char *str;
    Tcl_DString ds;
    char **pathv;

    if (GetEnvironmentVariableW(L"TCL_LIBRARY", wBuf, MAX_PATH) == 0) {
        buf[0] = '\0';
	GetEnvironmentVariableA("TCL_LIBRARY", buf, MAX_PATH);
    } else {
	ToUtf(wBuf, buf);
    }

    if (buf[0] != '\0') {
	objPtr = Tcl_NewStringObj(buf, -1);
	Tcl_ListObjAppendElement(NULL, listPtr, objPtr);

	TclWinNoBackslash(buf);
	Tcl_SplitPath(buf, &pathc, &pathv);

	/* 
	 * The lstrcmpi() will work even if pathv[pathc - 1] is random
	 * UTF-8 chars because I know lib is ascii.
	 */

	if ((pathc > 0) && (lstrcmpiA(lib + 4, pathv[pathc - 1]) != 0)) {
	    /*
	     * TCL_LIBRARY is set but refers to a different tcl
	     * installation than the current version.  Try fiddling with the
	     * specified directory to make it refer to this installation by
	     * removing the old "tclX.Y" and substituting the current
	     * version string.
	     */
	    
	    pathv[pathc - 1] = (char *) (lib + 4);
	    Tcl_DStringInit(&ds);
	    str = Tcl_JoinPath(pathc, pathv, &ds);
	    objPtr = Tcl_NewStringObj(str, Tcl_DStringLength(&ds));
	    Tcl_DStringFree(&ds);
	} else {
	    objPtr = Tcl_NewStringObj(buf, -1);
	}
	Tcl_ListObjAppendElement(NULL, listPtr, objPtr);
	ckfree((char *) pathv);
    }
}

static void 
AppendPath(
    Tcl_Obj *listPtr, 
    HMODULE hModule,
    CONST char *lib)
{
    WCHAR wName[MAX_PATH + LIBRARY_SIZE];
    char name[(MAX_PATH + LIBRARY_SIZE) * TCL_UTF_MAX];

    if (GetModuleFileNameW(hModule, wName, MAX_PATH) == 0) {
	GetModuleFileNameA(hModule, name, MAX_PATH);
    } else {
	ToUtf(wName, name);
    }
    if (lib != NULL) {
	char *end, *p;

	end = strrchr(name, '\\');
	*end = '\0';
	p = strrchr(name, '\\');
	if (p != NULL) {
	    end = p;
	}
	*end = '\\';
	strcpy(end + 1, lib);
    }
    TclWinNoBackslash(name);
    Tcl_ListObjAppendElement(NULL, listPtr, Tcl_NewStringObj(name, -1));
}

static void
AppendRegistry(
    Tcl_Obj *listPtr,
    CONST char *lib)
{
    HKEY key;
    LONG result;
    WCHAR wBuf[MAX_PATH + 64];
    char buf[(MAX_PATH + LIBRARY_SIZE) * TCL_UTF_MAX];
    DWORD len;

    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32s) {
	key = HKEY_CLASSES_ROOT;
    } else {
	key = HKEY_LOCAL_MACHINE;
    }
    result = RegOpenKeyExA(key, TCL_REGISTRY_KEY, 0, KEY_QUERY_VALUE, &key);
    if (result != ERROR_SUCCESS) {
	return;
    }

    /*
     * Can't just call RegQueryValueExW() and then if that fails (on 95)
     * call RegQueryValueExA() because RegQueryValueExW() always seems to
     * return ERROR_SUCCESS on Windows 95 even though it doesn't exist and
     * doesn't do anything.
     */

    len = MAX_PATH;
    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32_NT) {
	MultiByteToWideChar(CP_ACP, 0, "", -1, wBuf, MAX_PATH);
        result = RegQueryValueExW(key, wBuf, NULL, NULL, (LPBYTE) wBuf, &len);
	if (result == ERROR_SUCCESS) {
	    len = ToUtf(wBuf, buf);
	}
    } else {
	result = RegQueryValueExA(key, "", NULL, NULL, (LPBYTE) buf, &len);
    }
    if (result == ERROR_SUCCESS) {
	if (buf[len - 1] != '\\') {
	    buf[len] = '\\';
	    len++;
	}
	strcpy(buf + len, lib);
	TclWinNoBackslash(buf);
	Tcl_ListObjAppendElement(NULL, listPtr, Tcl_NewStringObj(buf, -1));
    }
    RegCloseKey(key);
}

static int
ToUtf(
    CONST WCHAR *wSrc,
    char *dst)
{
    char *start;

    start = dst;
    while (*wSrc != '\0') {
	dst += Tcl_UniCharToUtf(*wSrc, dst);
	wSrc++;
    }
    *dst = '\0';
    return dst - start;
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpSetInitialEncodings --
 *
 *	Based on the locale, determine the encoding of the operating
 *	system and the default encoding for newly opened files.
 *
 *	Called at process initialization time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Tcl library path is converted from native encoding to UTF-8.
 *
 *---------------------------------------------------------------------------
 */

void
TclpSetInitialEncodings()
{
    CONST char *encoding;
    char buf[4 + TCL_INTEGER_SPACE];
    int platformId;
    Tcl_Obj *pathPtr;

    platformId = TclWinGetPlatformId();

    TclWinSetInterfaces(platformId == VER_PLATFORM_WIN32_NT);

    wsprintfA(buf, "cp%d", GetACP());
    Tcl_SetSystemEncoding(NULL, buf);

    if (platformId != VER_PLATFORM_WIN32_NT) {
	pathPtr = TclGetLibraryPath();
	if (pathPtr != NULL) {
	    int i, objc;
	    Tcl_Obj **objv;
	    
	    objc = 0;
	    Tcl_ListObjGetElements(NULL, pathPtr, &objc, &objv);
	    for (i = 0; i < objc; i++) {
		int length;
		char *string;
		Tcl_DString ds;

		string = Tcl_GetStringFromObj(objv[i], &length);
		Tcl_ExternalToUtfDString(NULL, string, length, &ds);
		Tcl_SetStringObj(objv[i], Tcl_DStringValue(&ds), 
			Tcl_DStringLength(&ds));
		Tcl_DStringFree(&ds);
	    }
	}
    }

    /*
     * Keep this encoding preloaded.  The IO package uses it for gets on a
     * binary channel.  
     */

    encoding = "iso8859-1";
    Tcl_GetEncoding(NULL, encoding);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpSetVariables --
 *
 *	Performs platform-specific interpreter initialization related to
 *	the tcl_library and tcl_platform variables, and other platform-
 *	specific things.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_library", "tcl_platform", and "env(HOME)" Tcl variables.
 *
 *----------------------------------------------------------------------
 */

void
TclpSetVariables(interp)
    Tcl_Interp *interp;		/* Interp to initialize. */	
{	    
    char *ptr;
    char buffer[TCL_INTEGER_SPACE * 2];
    SYSTEM_INFO sysInfo;
    OemId *oemId;
    OSVERSIONINFOA osInfo;

    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    GetVersionExA(&osInfo);

    oemId = (OemId *) &sysInfo;
    if (osInfo.dwPlatformId == VER_PLATFORM_WIN32s) {
	/*
	 * Since Win32s doesn't support GetSystemInfo, we use a default value.
	 */

	oemId->wProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;
    } else {
	GetSystemInfo(&sysInfo);
    }

    /*
     * Define the tcl_platform array.
     */

    Tcl_SetVar2(interp, "tcl_platform", "platform", "windows",
	    TCL_GLOBAL_ONLY);
    if (osInfo.dwPlatformId < NUMPLATFORMS) {
	Tcl_SetVar2(interp, "tcl_platform", "os",
		platforms[osInfo.dwPlatformId], TCL_GLOBAL_ONLY);
    }
    wsprintfA(buffer, "%d.%d", osInfo.dwMajorVersion, osInfo.dwMinorVersion);
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", buffer, TCL_GLOBAL_ONLY);
    if (oemId->wProcessorArchitecture < NUMPROCESSORS) {
	Tcl_SetVar2(interp, "tcl_platform", "machine",
		processors[oemId->wProcessorArchitecture],
		TCL_GLOBAL_ONLY);
    }

#ifdef _DEBUG
    /*
     * The existence of the "debug" element of the tcl_platform array indicates
     * that this particular Tcl shell has been compiled with debug information.
     * Using "info exists tcl_platform(debug)" a Tcl script can direct the 
     * interpreter to load debug versions of DLLs with the load command.
     */

    Tcl_SetVar2(interp, "tcl_platform", "debug", "1",
		TCL_GLOBAL_ONLY);
#endif

    /*
     * Set up the HOME environment variable from the HOMEDRIVE & HOMEPATH
     * environment variables, if necessary.
     */

    ptr = Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY);
    if (ptr == NULL) {
	Tcl_DString ds;

        Tcl_DStringInit(&ds);
	ptr = Tcl_GetVar2(interp, "env", "HOMEDRIVE", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	ptr = Tcl_GetVar2(interp, "env", "HOMEPATH", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, -1);
	}
	if (Tcl_DStringLength(&ds) > 0) {
	    Tcl_SetVar2(interp, "env", "HOME", Tcl_DStringValue(&ds),
		    TCL_GLOBAL_ONLY);
	} else {
	    Tcl_SetVar2(interp, "env", "HOME", "c:\\", TCL_GLOBAL_ONLY);
	}
	Tcl_DStringFree(&ds);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Init --
 *
 *	This procedure is typically invoked by Tcl_AppInit procedures
 *	to perform additional initialization for a Tcl interpreter,
 *	such as sourcing the "init.tcl" script.
 *
 * Results:
 *	Returns a standard Tcl completion code and sets the interp's
 *	result if there is an error.
 *
 * Side effects:
 *	Depends on what's in the init.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    Tcl_Obj *pathPtr;

    pathPtr = TclGetLibraryPath();
    if (pathPtr == NULL) {
	pathPtr = Tcl_NewObj();
    }
    Tcl_SetObjVar2(interp, "tcl_libPath", NULL, pathPtr, TCL_GLOBAL_ONLY);
    return Tcl_Eval(interp, initScript);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SourceRCFile --
 *
 *	This procedure is typically invoked by Tcl_Main of Tk_Main
 *	procedure to source an application specific rc file into the
 *	interpreter at startup time.
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
Tcl_SourceRCFile(interp)
    Tcl_Interp *interp;		/* Interpreter to source rc file into. */
{
    Tcl_DString temp;
    char *fileName;
    Tcl_Channel errChannel;

    fileName = Tcl_GetVar(interp, "tcl_rcFileName", TCL_GLOBAL_ONLY);

    if (fileName != NULL) {
        Tcl_Channel c;
	char *fullName;

        Tcl_DStringInit(&temp);
	fullName = Tcl_TranslateFileName(interp, fileName, &temp);
	if (fullName == NULL) {
	    /*
	     * Couldn't translate the file name (e.g. it referred to a
	     * bogus user or there was no HOME environment variable).
	     * Just do nothing.
	     */
	} else {

	    /*
	     * Test for the existence of the rc file before trying to read it.
	     */

            c = Tcl_OpenFileChannel(NULL, fullName, "r", 0);
            if (c != (Tcl_Channel) NULL) {
                Tcl_Close(NULL, c);
		if (Tcl_EvalFile(interp, fullName) != TCL_OK) {
		    errChannel = Tcl_GetStdChannel(TCL_STDERR);
		    if (errChannel) {
			Tcl_WriteObj(errChannel, Tcl_GetObjResult(interp));
			Tcl_WriteChars(errChannel, "\n", 1);
		    }
		}
	    }
	}
        Tcl_DStringFree(&temp);
    }
}
