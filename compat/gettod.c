/*
 * Copyright (c) 1995 Sun Microsystems, Inc.
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
 * gettod.c --
 *
 *	This file provides the gettimeofday function on systems
 *	that only have the System V ftime function.
 *
*/

#include "tclPort.h"
#include <sys/timeb.h>

#undef timezone

int
gettimeofday(
    struct timeval *tp,
    struct timezone *tz)
{
    struct timeb t;
    (void)tz;

    ftime(&t);
    tp->tv_sec = t.time;
    tp->tv_usec = t.millitm * 1000;
    return 0;
}

