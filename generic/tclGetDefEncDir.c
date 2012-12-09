/*
 * tclGetDefEncDir.c --
 *
 *	Compatibility Tcl8 function, for inclusion in stub library
 *
 * Copyright (c) 2012 Jan Nijtmans
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#define USE_TCL_STUBS
#include "tcl.h"


/*
 *-------------------------------------------------------------------------
 *
 * Tcl_GetDefaultEncodingDir --
 *
 *	Legacy public interface to retrieve first directory in the encoding
 *	searchPath.
 *
 * Results:
 *	The directory pathname, as a string, or NULL for an empty encoding
 *	search path.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

const char *
TclGetDefaultEncodingDir(void)
{
    int numDirs;
    Tcl_Obj *first, *searchPath = Tcl_GetEncodingSearchPath();

    Tcl_ListObjLength(NULL, searchPath, &numDirs);
    if (numDirs == 0) {
	return NULL;
    }
    Tcl_ListObjIndex(NULL, searchPath, 0, &first);

    return Tcl_GetString(first);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */

