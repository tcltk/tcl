/* 
 * tclConfig.c --
 *
 *	This file provides the facilities which allow Tcl and other packages
 *	to embed configuration information into their binary libraries.
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclConfig.c,v 1.1.2.1 2002/01/25 01:47:01 andreas_kupries Exp $
 */

#include "tclInt.h"



/*
 * Internal structure to hold additional information about the
 * embedded configuration. Allocated on the heap during construction
 * of the configuration command. Allocated big enough to hold
 * references to as many strings as there are entries in the
 * configuration itself. These references will refer to the
 * configuration values, converted into UTF 8. This conversion is done
 * on demand.
 */

typedef struct Tcl_ConfigMeta {
  Tcl_Config*    configuration; /* Reference to the embedded
				 * configuration. */
  Tcl_Encoding   valEncoding;   /* Token for the encoding used to
				 * represent the values in the
				 * configuration. */
  Tcl_Obj*       keylist;       /* List of the registered keys */
  int            entries;       /* Number of entries in
				 * configuration. */
  Tcl_Obj*       value [1];     /* Array of the values converted to
				 * UTF-8 */
} Tcl_ConfigMeta;

/*
 * Static functions in this file:
 */

static
int QueryConfigObjCmd _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int objc, struct Tcl_Obj * CONST * objv));

static
void QueryConfigDelete _ANSI_ARGS_((ClientData clientData));


/*
 *---------------------------------------------------------------------------
 *
 * Tcl_RegisterConfig --
 *
 *	See TIP#59 for details on what this procedure does.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates namespace and cfg query command in it as per TIP #59.
 *
 *---------------------------------------------------------------------------
 */


void
Tcl_RegisterConfig (interp, pkgName, configuration, valEncoding)
     Tcl_Interp* interp;            /* interp the configuration command is registered in */
     CONST char* pkgName;           /* Name of the package registering the
				     * embedded configuration. ASCII, thus
				     * in UTF-8 too. */
     Tcl_Config* configuration;     /* Embedded configuration */
     CONST char* valEncoding;       /* Name of the encoding used to store
				     * the configuration values, ASCII,
				     * thus UTF-8 */
{
    /* Actions:
     * - Count the entries in the configuration,
     * - Allocate a big enough wrapper (meta) and initialize it
     * - Create the configuration query command and use the wrapper
     *   as its client data.
     */

    int             n, i;
    Tcl_Config*     cfg;
    Tcl_ConfigMeta* wrap;
    Tcl_DString     cmdName;

    for (n = 0, cfg = configuration; cfg->key != (CONST char*) NULL; n++, cfg++)
        /* empty loop */
      ;

    wrap = (Tcl_ConfigMeta*) Tcl_Alloc (sizeof (Tcl_ConfigMeta) + (sizeof (char*) * n));
    wrap->configuration = configuration;
    wrap->entries       = n;
    wrap->valEncoding   = Tcl_GetEncoding (NULL, valEncoding);
    wrap->keylist       = (Tcl_Obj*) NULL;

    for (i = 0; i < n; i++) {
        wrap->value [i] = (Tcl_Obj*) NULL;
    }

    Tcl_DStringInit   (&cmdName);
    Tcl_DStringAppend (&cmdName, "::",          -1);   
    Tcl_DStringAppend (&cmdName, pkgName,       -1);   

    /* The incomplete command name is the name of the namespace to
     * place it in
     */

    if ((Tcl_Namespace*) NULL == Tcl_CreateNamespace (interp,
		      Tcl_DStringValue (&cmdName), (ClientData) NULL,
		      (Tcl_NamespaceDeleteProc *) NULL)) {
        Tcl_Panic ("Unable to create namespace for package configuration");
    }

    Tcl_DStringAppend (&cmdName, "::pkgconfig", -1);   
    if ((Tcl_Command) NULL == Tcl_CreateObjCommand (interp, Tcl_DStringValue (&cmdName),
		    QueryConfigObjCmd, (ClientData) wrap, QueryConfigDelete)) {
        Tcl_Panic ("Unable to create query command for package configuration");
    }

    Tcl_DStringFree (&cmdName);
}


/*
 *-------------------------------------------------------------------------
 *
 * QueryConfigObjCmd --
 *
 *	Implementation of "::<package>::pkgconfig", the command to
 *	query configuration information embedded into a binary library.
 *
 * Results:
 *	A standard tcl result.
 *
 * Side effects:
 *	See the manual for what this command does.
 *
 *-------------------------------------------------------------------------
 */

static
int QueryConfigObjCmd (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     struct Tcl_Obj * CONST * objv;
{
    Tcl_ConfigMeta* wrap = (Tcl_ConfigMeta*) clientData;
    int index, i;

    static CONST char *subcmdStrings[] = {
	"get", "list", NULL
    };
    enum subcmds {
      CFG_GET, CFG_LIST
    };

    if ((objc < 2) || (objc > 3)) {
        Tcl_WrongNumArgs (interp, objc-1, objv+1, "list | get key");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], subcmdStrings, "subcommand", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum subcmds) index) {
        case CFG_GET:
	    if (objc != 3) {
	        Tcl_WrongNumArgs (interp, objc-1, objv+1, "get key");
		return TCL_ERROR;
	    }
	    for (i=0; i < wrap->entries; i++) {
	        /* We can use 'strcmp' as we know that the keys are in
		 * ASCII/UTF-8
		 */
	        if (strcmp (wrap->configuration [i].key, Tcl_GetString (objv [2])) == 0) {
		    if (wrap->value [i] == (Tcl_Obj*) NULL) {
		        /* Convert the value associated with a key to
			 * UTF 8 on demand, i.e. only if requested at
			 * all and cache the result as it will never
			 * change.
			 */

		        Tcl_DString conv;
			Tcl_Obj* s = Tcl_NewStringObj (Tcl_ExternalToUtfDString (wrap->valEncoding,
					 wrap->configuration [i].value, -1, &conv), -1 );
			Tcl_DStringFree (&conv);

			if (s == (Tcl_Obj*) NULL) {
			    Tcl_SetObjResult (interp,
				      Tcl_NewStringObj ("unable to convert value to utf-8", -1));
			    return TCL_ERROR;
			}
			Tcl_IncrRefCount (s);
			wrap->value [i] = s;
		    }
		    Tcl_SetObjResult (interp, wrap->value [i]);
		    return TCL_OK;
		}
	    }
	    Tcl_SetObjResult (interp, Tcl_NewStringObj ("key not known", -1));
	    return TCL_ERROR;

        case CFG_LIST:
	    if (objc != 2) {
	        Tcl_WrongNumArgs (interp, objc-1, objv+1, "list");
		return TCL_ERROR;
	    }
	    if (wrap->keylist == (Tcl_Obj*) NULL) {
	        /* Generate the list of know keys on demand and cache
		 * it as it will never change.
		 */

	        int i;
		Tcl_Obj* l = Tcl_NewListObj (0, NULL);

		if (l == (Tcl_Obj*) NULL) {
		    return TCL_ERROR;
		}
		for (i=0; i < wrap->entries; i++) {
		    Tcl_Obj* s = Tcl_NewStringObj (wrap->configuration [i].key, -1);
		    if (s == (Tcl_Obj*) NULL) {
		        Tcl_DecrRefCount (l);
			return TCL_ERROR;
		    }
		    if (TCL_OK != Tcl_ListObjAppendElement (interp, l, s)) {
		        Tcl_DecrRefCount (l);
			return TCL_ERROR;
		    }
		}

		Tcl_IncrRefCount (l);
		wrap->keylist =   l;
	    }
	    Tcl_SetObjResult (interp, wrap->keylist);
	    return TCL_OK;
        default:
	    Tcl_Panic ("This can't happen");
	    break;
    }
    return TCL_ERROR;
}

/*
 *-------------------------------------------------------------------------
 *
 * QueryConfigDelete --
 *
 *	Command delete procedure. Cleans up after the configuration query
 *	command when it is deleted by the user or during finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates all non-transient memory allocated by Tcl_RegisterConfig.
 *
 *-------------------------------------------------------------------------
 */

static
void QueryConfigDelete (clientData)
     ClientData clientData;
{
    Tcl_ConfigMeta* wrap = (Tcl_ConfigMeta*) clientData;
    int i;

    for (i = 0; i < wrap->entries; i++) {
        if (wrap->value[i] != (Tcl_Obj*) NULL) {
	    Tcl_DecrRefCount (wrap->value [i]);
	}
    }
    if (wrap->keylist != (Tcl_Obj*) NULL) {
        Tcl_DecrRefCount (wrap->keylist);
    }
    Tcl_FreeEncoding (wrap->valEncoding);
    Tcl_Free ((char*) wrap);
}
