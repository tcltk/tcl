/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * You may distribute and/or modify this program under the terms of the GNU
 * Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.

 * See the file "COPYING" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * cat.c --
 *
 *	Program used when testing tclWinPipe.c
 */

#ifdef TCL_BROKEN_MAINARGS
/* On mingw32 and cygwin this doesn't work */
#   undef UNICODE
#   undef _UNICODE
#endif

#include <stdio.h>
#include <io.h>
#include <string.h>
#include <tchar.h>

int
_tmain(void)
{
    char buf[1024];
    int n;
    const char *err;

    while (1) {
	n = _read(0, buf, sizeof(buf));
	if (n <= 0) {
	    break;
	}
	_write(1, buf, n);
    }
    err = (sizeof(int) == 2) ? "stderr16" : "stderr32";
    _write(2, err, (unsigned int)strlen(err));

    return 0;
}
