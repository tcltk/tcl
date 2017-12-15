/*
 * tclWinPanic.c --
 *
 *	Contains the Windows-specific command-line panic proc.
 *
 * Copyright (c) 2013 by Jan Nijtmans.
 * All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConsolePanic --
 *
 *	Display a message. If a debugger is present, present it directly to
 *	the debugger, otherwise send it to stderr.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ConsolePanic(
    const char *format, ...)
{
#define TCL_MAX_WARN_LEN 26000
    va_list argList;
    WCHAR msgString[TCL_MAX_WARN_LEN];
    char buf[TCL_MAX_WARN_LEN * TCL_UTF_MAX];
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    DWORD dummy;

    va_start(argList, format);
    vsnprintf(buf+3, sizeof(buf)-3, format, argList);
    buf[sizeof(buf)-1] = 0;
    msgString[TCL_MAX_WARN_LEN-1] = L'\0';
    MultiByteToWideChar(CP_UTF8, 0, buf+3, -1, msgString, TCL_MAX_WARN_LEN);

    /*
     * Truncate MessageBox string if it is too long to not overflow the buffer.
     */

    if (msgString[TCL_MAX_WARN_LEN-1] != L'\0') {
	memcpy(msgString + (TCL_MAX_WARN_LEN - 5), L" ...", 5 * sizeof(WCHAR));
    }

    if (IsDebuggerPresent()) {
	OutputDebugStringW(msgString);
    } else if (_isatty(2)) {
	WriteConsoleW(handle, msgString, wcslen(msgString), &dummy, 0);
    } else {
	buf[0] = 0xEF; buf[1] = 0xBB; buf[2] = 0xBF; /* UTF-8 bom */
	WriteFile(handle, buf, 3, &dummy, 0);
	FlushFileBuffers(handle);
    }
#   if defined(__GNUC__)
	__builtin_trap();
#   elif defined(_WIN64)
	__debugbreak();
#   elif defined(_MSC_VER)
	_asm {int 3}
#   else
	DebugBreak();
#   endif
#if defined(_WIN32)
	ExitProcess(1);
#else
	abort();
#endif
}
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * tab-width: 8
 * End:
 */
