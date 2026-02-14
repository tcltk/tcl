/*
 * tclWinInit.c --
 *
 *	Contains the Windows-specific interpreter initialization functions.
 *
 * Copyright © 1994-1997 Sun Microsystems, Inc.
 * Copyright © 1998-1999 Scriptics Corporation.
 * All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclWinInt.h"
#include <winnt.h>
#include <winbase.h>
#include <lmcons.h>
#if defined (__clang__) && (__clang_major__ > 20)
#pragma clang diagnostic ignored "-Wc++-keyword"
#endif

/*
 * GetUserNameW() is found in advapi32.dll
 */
#ifdef _MSC_VER
#   pragma comment(lib, "advapi32.lib")
#endif

/*
 * The following declaration is a workaround for some Microsoft brain damage.
 * The SYSTEM_INFO structure is different in various releases, even though the
 * layout is the same. So we overlay our own structure on top of it so we can
 * access the interesting slots in a uniform way.
 */

typedef struct {
    WORD wProcessorArchitecture;
    WORD wReserved;
} OemId;

/*
 * The following arrays contain the human readable strings for the
 * processor values.
 */

#define NUMPROCESSORS 15
static const char *const processors[NUMPROCESSORS] = {
    "intel", "mips", "alpha", "ppc", "shx", "arm", "ia64", "alpha64", "msil",
    "amd64", "ia32_on_win64", "neutral", "arm64", "arm32_on_win64",
    "ia32_on_arm64"
};

/*
 * Forward declarations
 */

static TclInitProcessGlobalValueProc InitializeDefaultLibraryDir;
static TclInitProcessGlobalValueProc	InitializeSourceLibraryDir;
static void		AppendEnvironment(Tcl_Obj *listPtr, const char *lib);

/*
 * The default directory in which the init.tcl file is expected to be found.
 */

static ProcessGlobalValue defaultLibraryDir =
	{0, 0, NULL, NULL, InitializeDefaultLibraryDir, NULL, NULL};
static ProcessGlobalValue sourceLibraryDir =
	{0, 0, NULL, NULL, InitializeSourceLibraryDir, NULL, NULL};


/*
 * TclGetWinInfoOnce --
 *
 *	Callback to retrieve bits of Windows platform information.
 *	To be invoked only through InitOnceExecuteOnce for thread safety.
 *
 * Results:
 *	None.
 */
static BOOL CALLBACK
TclGetWinInfoOnce(
    TCL_UNUSED(PINIT_ONCE),
    TCL_UNUSED(PVOID),
    PVOID *lpContext)
{
    static TclWinInfo tclWinInfo;
    typedef int(__stdcall getVersionProc)(void *);
    DWORD dw;

    /*
     * GetVersionExW will not return the "real" Windows version so use
     * RtlGetVersion if available and falling back.
     */
    HMODULE handle = GetModuleHandleW(L"NTDLL");
    getVersionProc *getVersion =
	(getVersionProc *)(void *)GetProcAddress(handle, "RtlGetVersion");

    tclWinInfo.osVersion.dwOSVersionInfoSize = sizeof(tclWinInfo.osVersion);
    if (getVersion == NULL || getVersion(&tclWinInfo.osVersion) != 0) {
	if (!GetVersionExW(&tclWinInfo.osVersion)) {
	    /* Should never happen but ...*/
	    return FALSE;
	}
    }

    tclWinInfo.longPathsSupported = 0;
    if (tclWinInfo.osVersion.dwMajorVersion == 10 &&
	tclWinInfo.osVersion.dwBuildNumber >= 22000) {
	tclWinInfo.osVersion.dwMajorVersion = 11;
    }
    if (tclWinInfo.osVersion.dwMajorVersion > 10 ||
	    (tclWinInfo.osVersion.dwMajorVersion == 10 &&
	    tclWinInfo.osVersion.dwBuildNumber >= 14393)) {
	dw = sizeof(tclWinInfo.longPathsSupported);
	if (RegGetValueA(HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Control\\FileSystem",
		"LongPathsEnabled", RRF_RT_REG_DWORD, NULL,
		&tclWinInfo.longPathsSupported, &dw) != ERROR_SUCCESS) {
	    tclWinInfo.longPathsSupported = 0; /* Reset in case modified */
	}
    }

    tclWinInfo.codePage[0] = 'c';
    tclWinInfo.codePage[1] = 'p';
    dw = sizeof(tclWinInfo.codePage) - 2;

    /*
     * When retrieving code page from registry,
     *  - use ANSI API's since all values will be ASCII and saves conversion
     *  - use RegGetValue, not RegQueryValueEx, since the latter does not
     *    guarantee the value is null terminated
     *  - added bonus, RegGetValue is much more convenient to use
     */
    if (RegGetValueA(HKEY_LOCAL_MACHINE,
	    "SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage",
	    "ACP", RRF_RT_REG_SZ, NULL, &tclWinInfo.codePage[2],
	    &dw) != ERROR_SUCCESS) {
	/* On failure, fallback to GetACP() */
	snprintf(tclWinInfo.codePage, sizeof(tclWinInfo.codePage), "cp%u",
	    GetACP());
    }
    if (strcmp(tclWinInfo.codePage, "cp65001") == 0) {
	strcpy(tclWinInfo.codePage, "utf-8");
    }

    *lpContext = (LPVOID)&tclWinInfo;
    return TRUE;
}

/*
 * TclGetWinInfo --
 *
 *	Returns a pointer to the TclWinInfo structure containing various bits
 *	of Windows platform information.
 *
 * Results:
 *	Pointer to TclWinInfo structure and NULL on failure. The structure
 *	is initialized only once and remains valid for the lifetime of the
 *	process.
 */
const TclWinInfo *
TclGetWinInfo(void)
{
    static INIT_ONCE winInfoOnce = INIT_ONCE_STATIC_INIT;
    TclWinInfo *winInfoPtr = NULL;
    BOOL result = InitOnceExecuteOnce(
	&winInfoOnce, TclGetWinInfoOnce, NULL, (LPVOID *)&winInfoPtr);
    return result ? winInfoPtr : NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpInitPlatform --
 *
 *	Initialize all the platform-dependent things like signals,
 *	floating-point error handling and sockets.
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
TclpInitPlatform(void)
{
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);

    tclPlatform = TCL_PLATFORM_WINDOWS;

    /*
     * Initialize the winsock library. On Windows XP and higher this
     * can never fail.
     */
    WSAStartup(wVersionRequested, &wsaData);

#ifdef STATIC_BUILD
    /*
     * If we are in a statically linked executable, then we need to explicitly
     * initialize the Windows function tables here since DllMain() will not be
     * invoked.
     */

    TclWinInit(GetModuleHandleW(NULL));
#endif

    if (TclGetWinInfo() == NULL) {
	Tcl_Panic("TclpInitPlatform: unable to get Windows information");
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * TclpInitLibraryPath --
 *
 *	This is the fallback routine that sets the library path if the
 *	application has not set one by the first time it is needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the library path to an initial value.
 *
 *-------------------------------------------------------------------------
 */

void
TclpInitLibraryPath(
    char **valuePtr,
    size_t *lengthPtr,
    Tcl_Encoding *encodingPtr)
{
#define LIBRARY_SIZE	    64
    Tcl_Obj *pathPtr;
    char installLib[LIBRARY_SIZE];
    const char *bytes;
    Tcl_Size length;

    TclNewObj(pathPtr);

    /*
     * Initialize the substring used when locating the script library. The
     * installLib variable computes the script library path relative to the
     * installed DLL.
     */

    snprintf(installLib, sizeof(installLib), "lib/tcl%s", TCL_VERSION);

    /*
     * Look for the library relative to the TCL_LIBRARY env variable. If the
     * last dirname in the TCL_LIBRARY path does not match the last dirname in
     * the installLib variable, use the last dir name of installLib in
     * addition to the original TCL_LIBRARY path.
     */

    AppendEnvironment(pathPtr, installLib);

    /*
     * Look for the library in its default location.
     */

    Tcl_ListObjAppendElement(NULL, pathPtr,
	    TclGetProcessGlobalValue(&defaultLibraryDir));

    /*
     * Look for the library in its source checkout location.
     */

    Tcl_ListObjAppendElement(NULL, pathPtr,
	    TclGetProcessGlobalValue(&sourceLibraryDir));

    *encodingPtr = NULL;
    bytes = TclGetStringFromObj(pathPtr, &length);
    *lengthPtr = length++;
    *valuePtr = (char *)Tcl_Alloc(length);
    memcpy(*valuePtr, bytes, length);
    Tcl_DecrRefCount(pathPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * AppendEnvironment --
 *
 *	Append the value of the TCL_LIBRARY environment variable onto the path
 *	pointer. If the env variable points to another version of tcl (e.g.
 *	"tcl8.6") also append the path to this version (e.g.,
 *	"tcl8.6/../tcl9.0")
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static void
AppendEnvironment(
    Tcl_Obj *pathPtr,
    const char *lib)
{
    Tcl_Size pathc;
    Tcl_Obj *objPtr;
    const char **pathv;
    char *shortlib;
    TclWinPath winPath;

    /*
     * The shortlib value needs to be the tail component of the lib path. For
     * example, "lib/tcl9.0" -> "tcl9.0" while "usr/share/tcl9.0" -> "tcl9.0".
     */

    for (shortlib = (char *) &lib[strlen(lib)-1]; shortlib>lib ; shortlib--) {
	if (*shortlib == '/') {
	    if ((size_t)(shortlib - lib) == strlen(lib) - 1) {
		Tcl_Panic("last character in lib cannot be '/'");
	    }
	    shortlib++;
	    break;
	}
    }
    if (shortlib == lib) {
	Tcl_Panic("no '/' character found in lib");
    }

    WCHAR *wEnvValue = TclWinGetEnvironmentVariable(L"TCL_LIBRARY", &winPath);
    if (wEnvValue == NULL || *wEnvValue == 0) {
	return;
    }

    Tcl_DString dsEnvValue;
    char *buf = TclWinWCharToUtfDString(wEnvValue, -1, &dsEnvValue);
    if (buf == NULL) {
	return;
    }

    if (buf[0] != '\0') {
	objPtr = Tcl_NewStringObj(buf, TCL_INDEX_NONE);
	Tcl_ListObjAppendElement(NULL, pathPtr, objPtr);

	TclWinNoBackslash(buf);
	Tcl_SplitPath(buf, &pathc, &pathv);

	/*
	 * The lstrcmpiA() will work even if pathv[pathc-1] is random UTF-8
	 * chars because I know shortlib is ascii.
	 */

	if ((pathc > 0) && (lstrcmpiA(shortlib, pathv[pathc - 1]) != 0)) {
	    /*
	     * TCL_LIBRARY is set but refers to a different tcl installation
	     * than the current version. Try fiddling with the specified
	     * directory to make it refer to this installation by removing the
	     * old "tclX.Y" and substituting the current version string.
	     */
	    Tcl_DString ds;
	    pathv[pathc - 1] = shortlib;
	    Tcl_DStringInit(&ds);
	    (void) Tcl_JoinPath(pathc, pathv, &ds);
	    objPtr = Tcl_DStringToObj(&ds);
	} else {
	    objPtr = Tcl_NewStringObj(buf, TCL_INDEX_NONE);
	}
	Tcl_ListObjAppendElement(NULL, pathPtr, objPtr);
	Tcl_Free((void *)pathv);
    }
    Tcl_DStringFree(&dsEnvValue);
}

/*
 * AllocateGrandparentSiblingPath --
 *
 *	Allocates and initializes a path corresponding to a sibling of
 *	the module's grandparent, or parent if no grandparent.
 *
 * Results:
 *	TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *
 *      The new path is allocated with Tcl_Alloc and a pointer to it is
 *	stored in *valuePtr and its length, not including the nul terminator
 *	is stored in *lengthPtr. The memory must be eventually released with
 *      with Tcl_Free.
 */
static int
AllocateGrandparentSiblingPath(
    const char *siblingPtr,	/* sub directory to append. Path separators
				 * (if any) must be forward slashes and the
				 * string must not have leading separators */
    char **valuePtr,		/* location to store pointer to path */
    size_t *lengthPtr)		/* location to store length of path */
{
    HMODULE hModule = (HMODULE)TclWinGetTclInstance();
    TclWinPath winPath;
    WCHAR *wNamePtr;
    int result = TCL_ERROR;

    wNamePtr = TclWinGetModuleFileName(hModule, &winPath);
    if (wNamePtr != NULL) {
	char *utf8Ptr;
	Tcl_DString ds;
	/* Do not use Tcl encoding API as it may not be initialized yet */
	utf8Ptr = TclWinWCharToUtfDString(wNamePtr, -1, &ds);
	if (utf8Ptr) {
	    char *end, *p;
	    TclWinNoBackslash(utf8Ptr);
	    end = strrchr(utf8Ptr, '/');
	    *end = '\0';
	    p = strrchr(utf8Ptr, '/');
	    if (p != NULL) {
		end = p;
	    }
	    *end = '/';
	    Tcl_DStringSetLength(&ds, (Tcl_Size)(end - utf8Ptr + 1));
	    Tcl_DStringAppend(&ds, siblingPtr, TCL_AUTO_LENGTH);
	    utf8Ptr = Tcl_DStringValue(&ds);
	    *lengthPtr = (size_t)Tcl_DStringLength(&ds);
	    *valuePtr = (char *)Tcl_Alloc(*lengthPtr + 1);
	    memcpy(*valuePtr, utf8Ptr, *lengthPtr + 1);
	    Tcl_DStringFree(&ds);
	    result = TCL_OK;
	}
	TclWinPathFree(&winPath);
    }
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * InitializeDefaultLibraryDir --
 *
 *	Locate the Tcl script library default location relative to the
 *	location of the Tcl DLL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static void
InitializeDefaultLibraryDir(
    char **valuePtr,
    size_t *lengthPtr,
    Tcl_Encoding *encodingPtr)
{
    *encodingPtr = NULL;
    (void) AllocateGrandparentSiblingPath("lib/tcl" TCL_VERSION,
	       valuePtr, lengthPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * InitializeSourceLibraryDir --
 *
 *	Locate the Tcl script library default location relative to the
 *	location of the Tcl DLL as it exists in the build output directory
 *	associated with the source checkout.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static void
InitializeSourceLibraryDir(
    char **valuePtr,
    size_t *lengthPtr,
    Tcl_Encoding *encodingPtr)
{
    *encodingPtr = NULL;
    (void) AllocateGrandparentSiblingPath("../library", valuePtr, lengthPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpSetInitialEncodings --
 *
 *	Based on the locale, determine the encoding of the operating system
 *	and the default encoding for newly opened files.
 *
 *	Called at process initialization time, and part way through startup,
 *	we verify that the initial encodings were correctly setup. Depending
 *	on Tcl's environment, there may not have been enough information first
 *	time through (above).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Tcl library path is converted from native encoding to UTF-8, on
 *	the first call, and the encodings may be changed on first or second
 *	call.
 *
 *---------------------------------------------------------------------------
 */

void
TclpSetInitialEncodings(void)
{
    Tcl_DString encodingName;

    Tcl_SetSystemEncoding(NULL,
	    Tcl_GetEncodingNameFromEnvironment(&encodingName));
    Tcl_DStringFree(&encodingName);
}

const char *
Tcl_GetEncodingNameForUser(
    Tcl_DString *bufPtr)
{
    Tcl_DStringInit(bufPtr);
    Tcl_DStringAppend(bufPtr, TclpGetCodePage(), -1);
    return Tcl_DStringValue(bufPtr);
}

const char *
Tcl_GetEncodingNameFromEnvironment(
    Tcl_DString *bufPtr)
{
    const OSVERSIONINFOW *osInfoPtr = TclpGetWindowsVersion();
    /*
     * TIP 716 - for Build 18362 or higher, force utf-8. Note Windows build
     * numbers always increase, so no need to check major / minor versions.
     */
    if (osInfoPtr && osInfoPtr->dwBuildNumber >= 18362) {
	Tcl_DStringInit(bufPtr);
	Tcl_DStringAppend(bufPtr, "utf-8", 5);
	return Tcl_DStringValue(bufPtr);
    } else {
	return Tcl_GetEncodingNameForUser(bufPtr);
    }
}

const char *
TclpGetUserName(
    Tcl_DString *bufferPtr)	/* Uninitialized or free DString filled with
				 * the name of user. */
{
    Tcl_DStringInit(bufferPtr);

    if (TclGetEnv("USERNAME", bufferPtr) == NULL) {
	WCHAR szUserName[UNLEN+1];
	DWORD cchUserNameLen = UNLEN;

	if (!GetUserNameW(szUserName, &cchUserNameLen)) {
	    return NULL;
	}
	cchUserNameLen--;
	Tcl_DStringInit(bufferPtr);
	Tcl_WCharToUtfDString(szUserName, cchUserNameLen, bufferPtr);
    }
    return Tcl_DStringValue(bufferPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * TclpSetVariables --
 *
 *	Performs platform-specific interpreter initialization related to the
 *	tcl_platform and env variables, and other platform-specific things.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets "tcl_platform", and "env(HOME)" Tcl variables.
 *
 *----------------------------------------------------------------------
 */

void
TclpSetVariables(
    Tcl_Interp *interp)		/* Interp to initialize. */
{
    const char *ptr;
    char buffer[TCL_INTEGER_SPACE * 2];
    union {
	SYSTEM_INFO info;
	OemId oemId;
    } sys;
    const OSVERSIONINFOW *osInfoPtr;
    Tcl_DString ds;

    Tcl_SetVar2Ex(interp, "tclDefaultLibrary", NULL,
	    TclGetProcessGlobalValue(&defaultLibraryDir), TCL_GLOBAL_ONLY);

    /*
     * Define the tcl_platform array.
     */

    Tcl_SetVar2(interp, "tcl_platform", "platform", "windows",
	    TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tcl_platform", "os", "Windows NT", TCL_GLOBAL_ONLY);
    osInfoPtr = TclpGetWindowsVersion();
    assert(osInfoPtr);
    snprintf(buffer, sizeof(buffer), "%ld.%ld", osInfoPtr->dwMajorVersion,
	osInfoPtr->dwMinorVersion);
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", buffer, TCL_GLOBAL_ONLY);

    GetSystemInfo(&sys.info);
    if (sys.oemId.wProcessorArchitecture < NUMPROCESSORS) {
	Tcl_SetVar2(interp, "tcl_platform", "machine",
		processors[sys.oemId.wProcessorArchitecture],
		TCL_GLOBAL_ONLY);
    }

    /*
     * Set up the HOME environment variable from the HOMEDRIVE & HOMEPATH
     * environment variables, if necessary.
     */

    Tcl_DStringInit(&ds);
    ptr = Tcl_GetVar2(interp, "env", "HOME", TCL_GLOBAL_ONLY);
    if (ptr == NULL) {
	ptr = Tcl_GetVar2(interp, "env", "HOMEDRIVE", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, TCL_INDEX_NONE);
	}
	ptr = Tcl_GetVar2(interp, "env", "HOMEPATH", TCL_GLOBAL_ONLY);
	if (ptr != NULL) {
	    Tcl_DStringAppend(&ds, ptr, TCL_INDEX_NONE);
	}
	if (Tcl_DStringLength(&ds) > 0) {
	    Tcl_SetVar2(interp, "env", "HOME", Tcl_DStringValue(&ds),
		    TCL_GLOBAL_ONLY);
	} else {
	    /* None of HOME, HOMEDRIVE, HOMEPATH exists. Try USERPROFILE */
	    ptr = Tcl_GetVar2(interp, "env", "USERPROFILE", TCL_GLOBAL_ONLY);
	    if (ptr != NULL && ptr[0]) {
		Tcl_SetVar2(interp, "env", "HOME", ptr, TCL_GLOBAL_ONLY);
	    } else {
		/* Last resort */
		Tcl_SetVar2(interp, "env", "HOME", "c:\\", TCL_GLOBAL_ONLY);
	    }
	}
    }

    /*
     * Initialize the user name from the environment first, since this is much
     * faster than asking the system.
     * Note: cchUserNameLen is number of characters including nul terminator.
     */

    ptr = TclpGetUserName(&ds);
    Tcl_SetVar2(interp, "tcl_platform", "user", ptr ? ptr : "",
	    TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&ds);

    /*
     * Define what the platform PATH separator is. [TIP #315]
     */

    Tcl_SetVar2(interp, "tcl_platform", "pathSeparator", ";", TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFindVariable --
 *
 *	Locate the entry in environ for a given name. On Unix this routine is
 *	case sensitive, on Windows this matches mixed case.
 *
 * Results:
 *	The return value is the index in environ of an entry with the name
 *	"name", or -1 if there is no such entry. The integer
 *	at *lengthPtr is filled in with the length of name (if a matching
 *	entry is found) or the length of the environ array (if no
 *	matching entry is found).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Size
TclpFindVariable(
    const char *name,		/* Name of desired environment variable
				 * (UTF-8). */
    Tcl_Size *lengthPtr)	/* Used to return length of name (for
				 * successful searches) or number of non-NULL
				 * entries in environ (for unsuccessful
				 * searches). */
{
    Tcl_Size i, length, result = TCL_INDEX_NONE;
    const WCHAR *env;
    const char *p1, *p2;
    char *envUpper, *nameUpper;
    Tcl_DString envString;

    /*
     * Convert the name to all upper case for the case insensitive comparison.
     */

    length = strlen(name);
    nameUpper = (char *)Tcl_Alloc(length + 1);
    memcpy(nameUpper, name, length+1);
    Tcl_UtfToUpper(nameUpper);

    Tcl_DStringInit(&envString);
    for (i = 0, env = _wenviron[i]; env != NULL; i++, env = _wenviron[i]) {
	/*
	 * Chop the env string off after the equal sign, then Convert the name
	 * to all upper case, so we do not have to convert all the characters
	 * after the equal sign.
	 */

	Tcl_DStringInit(&envString);
	envUpper = Tcl_WCharToUtfDString(env, TCL_INDEX_NONE, &envString);
	p1 = strchr(envUpper, '=');
	if (p1 == NULL) {
	    continue;
	}
	length = p1 - envUpper;
	Tcl_DStringSetLength(&envString, length+1);
	Tcl_UtfToUpper(envUpper);

	p1 = envUpper;
	p2 = nameUpper;
	for (; *p2 == *p1; p1++, p2++) {
	    /* NULL loop body. */
	}
	if ((*p1 == '=') && (*p2 == '\0')) {
	    *lengthPtr = length;
	    result = i;
	    goto done;
	}

	Tcl_DStringFree(&envString);
    }

    *lengthPtr = i;

  done:
    Tcl_DStringFree(&envString);
    Tcl_Free(nameUpper);
    return result;
}

/*
 * TclWinWCharToUtfDString --
 *
 *	Convert the passed WCHAR (UTF-16) to UTF-8 returning the result
 *	in a Tcl_DString. The primary utility of this function is to
 *	allow the conversion before Tcl encoding subsystem is initialized.
 *
 * Results:
 *	A pointer into the output Tcl_DString holding the result and NULL
 *	on failure.
 *
 * Side effects:
 *	The dsPtr Tcl_DString may allocate storage and caller should
 *	call Tcl_DStringFree on it on success.
 */
char *
TclWinWCharToUtfDString(
    const WCHAR *wsPtr,		/* string to convert */
    int numChars,		/* wsPtr character count */
    Tcl_DString *dsPtr)		/* output */
{
    int needed;

    Tcl_DStringInit(dsPtr);

    if (numChars == -1) {
	numChars = wcslen(wsPtr);
    }

    if (numChars == 0) {
	return Tcl_DStringValue(dsPtr);
    }

    needed = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, wsPtr, numChars, NULL,
		0, NULL, NULL);
    if (needed > 0) {
	/*
	 * Allocate needed + 1 so we can ensure a terminating NUL in all cases
	 * (WideCharToMultiByte writes a NUL only when source length is -1).
	 */
	int written;
	char *utf8Ptr;

	Tcl_DStringSetLength(dsPtr, (needed + 1));
	utf8Ptr = Tcl_DStringValue(dsPtr);

	written = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, wsPtr, numChars, utf8Ptr,
	    needed + 1, NULL, NULL);
	if (written > 0) {
	    Tcl_DStringSetLength(dsPtr, written);
	    return Tcl_DStringValue(dsPtr);
	}

    }
    Tcl_DStringFree(dsPtr);
    return NULL;
}

/*
 * TclWinGetEnvironmentVariable --
 *
 *      Wrapper for GetEnvironmentVariableW that automatically grows the
 *      buffer as needed.
 *
 * Results:
 *      Returns a pointer to the environment variable value. The returned
 *      pointer is valid until TclWinPathFree or TclWinPathResize is called
 *      on winPathPtr. Returns NULL on failure. An error code may be
 *      retrieved via GetLastError() as for GetEnvironmentVariableW.
 *
 * Side effects:
 *      May allocate memory that must be freed with TclWinPathFree
 *      on a non-NULL return.
 */

WCHAR *
TclWinGetEnvironmentVariable(
    const WCHAR *envName,	/* Environment variable name */
    TclWinPath *winPathPtr)	/* Buffer to receive full path. Should be
				 * uninitialized or previously reset with
				 * TclWinPathFree. */
{
    DWORD numChars;
    DWORD capacity;
    WCHAR *fullPathPtr;
    DWORD err;

    fullPathPtr = TclWinPathInit(winPathPtr, &capacity);
    numChars = GetEnvironmentVariableW(envName, fullPathPtr, capacity);

    if (numChars == 0) {
	goto errorReturn;
    }

    /*
     * numChars does not include the null terminator so even if numChars
     * equal to capacity, the buffer was too small.
     */
    if (numChars < capacity) {
	return fullPathPtr;
    }

    /*
     * Buffer too small. In this case, numChars is required space INCLUDING
     * the null terminator. Allocate a larger buffer and try again.
     */
    capacity = numChars;
    fullPathPtr = TclWinPathResize(winPathPtr, capacity);
    numChars = GetEnvironmentVariableW(envName, fullPathPtr, capacity);
    if (numChars == 0 || numChars >= capacity) {
	/* Failed or still too small (shouldn't happen). */
	goto errorReturn;
    }

    return fullPathPtr;

errorReturn:
    err = GetLastError();
    TclWinPathFree(winPathPtr);
    SetLastError(err);
    return NULL;}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
