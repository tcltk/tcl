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

static TclInitProcessGlobalValueProc	InitializeDefaultLibraryDir;
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
 * TclpGetWindowsVersionOnce --
 *
 *	Callback to retrieve Windows version information. To be invoked only
 *	through InitOnceExecuteOnce for thread safety.
 *
 * Results:
 *	None.
 */
static BOOL CALLBACK TclpGetWindowsVersionOnce(
    TCL_UNUSED(PINIT_ONCE),
    TCL_UNUSED(PVOID),
    PVOID *lpContext)
{
    typedef int(__stdcall getVersionProc)(void *);
    static OSVERSIONINFOW osInfo;

    /*
     * GetVersionExW will not return the "real" Windows version so use
     * RtlGetVersion if available and falling back.
     */
    HMODULE handle = GetModuleHandleW(L"NTDLL");
    getVersionProc *getVersion =
	(getVersionProc *)(void *)GetProcAddress(handle, "RtlGetVersion");

    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    if (getVersion == NULL || getVersion(&osInfo)) {
	if (!GetVersionExW(&osInfo)) {
	    /* Should never happen but ...*/
	    return FALSE;
	}
    }
    *lpContext = (LPVOID)&osInfo;
    return TRUE;
}

/*
 * TclpGetWindowsVersion --
 *
 *	Returns a pointer to the OSVERSIONINFOW structure containing the
 *	version information for the current Windows version.
 *
 * Results:
 *	Pointer to OSVERSIONINFOW structure.
 */
static const OSVERSIONINFOW *TclpGetWindowsVersion(void)
{
    static INIT_ONCE osInfoOnce = INIT_ONCE_STATIC_INIT;
    OSVERSIONINFOW *osInfoPtr = NULL;
    BOOL result = InitOnceExecuteOnce(
	&osInfoOnce, TclpGetWindowsVersionOnce, NULL, (LPVOID *)&osInfoPtr);
    return result ? osInfoPtr : NULL;
}

/*
 * TclpGetCodePageOnce --
 *
 *	Callback to retrieve user code page. To be invoked only
 *	through InitOnceExecuteOnce for thread safety.
 *
 * Results:
 *	None.
 */
static BOOL CALLBACK
TclpGetCodePageOnce(
    TCL_UNUSED(PINIT_ONCE),
    TCL_UNUSED(PVOID),
    PVOID *lpContext)
{
    static char codePage[20];
    codePage[0] = 'c';
    codePage[1] = 'p';
    DWORD size = sizeof(codePage) - 2;

    /*
     * When retrieving code page from registry,
     *  - use ANSI API's since all values will be ASCII and saves conversion
     *  - use RegGetValue, not RegQueryValueEx, since the latter does not
     *    guarantee the value is null terminated
     *  - added bonus, RegGetValue is much more convenient to use
     */
    if (RegGetValueA(HKEY_LOCAL_MACHINE,
	    "SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage",
	    "ACP", RRF_RT_REG_SZ, NULL, codePage+2,
	    &size) != ERROR_SUCCESS) {
	/* On failure, fallback to GetACP() */
	UINT acp = GetACP();
	snprintf(codePage, sizeof(codePage), "cp%u", acp);
    }
    if (strcmp(codePage, "cp65001") == 0) {
	strcpy(codePage, "utf-8");
    }
    *lpContext = (LPVOID)&codePage[0];
    return TRUE;
}

/*
 * TclpGetCodePage --
 *
 *  Returns a pointer to the string identifying the user code page.
 *
 *  For consistency with Windows, which caches the code page at program
 *  startup, the code page is not updated even if the value in the registry
 *  changes. (This is similar to environment variables.)
 */
static const char *
TclpGetCodePage(void)
{
    static INIT_ONCE codePageOnce = INIT_ONCE_STATIC_INIT;
    const char *codePagePtr = NULL;
    BOOL result = InitOnceExecuteOnce(
	&codePageOnce, TclpGetCodePageOnce, NULL, (LPVOID *)&codePagePtr);
#ifdef NDEBUG
    (void) result; /* Keep gcc unused variable quiet */
#else
    assert(result == TRUE);
#endif
    assert(codePagePtr != NULL);
    return codePagePtr;
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

    /* Initialize code page once at startup, will not be updated */
    (void)TclpGetCodePage();
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
    WCHAR wBuf[MAX_PATH];
    DWORD dw;
    char buf[MAX_PATH * 3];
    Tcl_Obj *objPtr;
    Tcl_DString ds;
    const char **pathv;
    char *shortlib;

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

    dw = GetEnvironmentVariableW(L"TCL_LIBRARY", wBuf, MAX_PATH);
    if (dw <= 0 || dw >= MAX_PATH) {
	return;
    }
    if (WideCharToMultiByte(
	    CP_UTF8, 0, wBuf, -1, buf, MAX_PATH * 3, NULL, NULL) == 0) {
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
    HMODULE hModule = (HMODULE)TclWinGetTclInstance();
    WCHAR wName[MAX_PATH + LIBRARY_SIZE];
    char name[(MAX_PATH + LIBRARY_SIZE) * 3];
    char *end, *p;

    GetModuleFileNameW(hModule, wName, sizeof(wName)/sizeof(WCHAR));
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, sizeof(name), NULL, NULL);

    end = strrchr(name, '\\');
    *end = '\0';
    p = strrchr(name, '\\');
    if (p != NULL) {
	end = p;
    }
    *end = '\\';

    TclWinNoBackslash(name);
    snprintf(end + 1, LIBRARY_SIZE, "lib/tcl%s", TCL_VERSION);
    *lengthPtr = strlen(name);
    *valuePtr = (char *)Tcl_Alloc(*lengthPtr + 1);
    *encodingPtr = NULL;
    memcpy(*valuePtr, name, *lengthPtr + 1);
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
    HMODULE hModule = (HMODULE)TclWinGetTclInstance();
    WCHAR wName[MAX_PATH + LIBRARY_SIZE];
    char name[(MAX_PATH + LIBRARY_SIZE) * 3];
    char *end, *p;

    GetModuleFileNameW(hModule, wName, sizeof(wName)/sizeof(WCHAR));
    WideCharToMultiByte(CP_UTF8, 0, wName, -1, name, sizeof(name), NULL, NULL);

    end = strrchr(name, '\\');
    *end = '\0';
    p = strrchr(name, '\\');
    if (p != NULL) {
	end = p;
    }
    *end = '\\';

    TclWinNoBackslash(name);
    snprintf(end + 1, LIBRARY_SIZE, "../library");
    *lengthPtr = strlen(name);
    *valuePtr = (char *)Tcl_Alloc(*lengthPtr + 1);
    *encodingPtr = NULL;
    memcpy(*valuePtr, name, *lengthPtr + 1);
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
    typedef int(__stdcall getVersionProc)(void *);
    const char *ptr;
    char buffer[TCL_INTEGER_SPACE * 2];
    union {
	SYSTEM_INFO info;
	OemId oemId;
    } sys;
    static OSVERSIONINFOW osInfo;
    static int osInfoInitialized = 0;
    Tcl_DString ds;

    Tcl_SetVar2Ex(interp, "tclDefaultLibrary", NULL,
	    TclGetProcessGlobalValue(&defaultLibraryDir), TCL_GLOBAL_ONLY);

    if (!osInfoInitialized) {
	HMODULE handle = GetModuleHandleW(L"NTDLL");
	getVersionProc *getVersion = (getVersionProc *) (void *)
		GetProcAddress(handle, "RtlGetVersion");

	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
	if (!getVersion || getVersion(&osInfo)) {
	    GetVersionExW(&osInfo);
	}
	osInfoInitialized = 1;
    }
    GetSystemInfo(&sys.info);

    /*
     * Define the tcl_platform array.
     */

    Tcl_SetVar2(interp, "tcl_platform", "platform", "windows",
	    TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tcl_platform", "os", "Windows NT", TCL_GLOBAL_ONLY);
    if (osInfo.dwMajorVersion == 10 && osInfo.dwBuildNumber >= 22000) {
	osInfo.dwMajorVersion = 11;
    }
    snprintf(buffer, sizeof(buffer), "%ld.%ld",
	    osInfo.dwMajorVersion, osInfo.dwMinorVersion);
    Tcl_SetVar2(interp, "tcl_platform", "osVersion", buffer, TCL_GLOBAL_ONLY);
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
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
