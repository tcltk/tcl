/* 
 * tclMacPkgConfig.c --
 *
 *	This file contains the Mac configuration information to
 *	embed into the tcl binary library.
 *
 * Copyright (c) 2002 Daniel Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclMacPkgConfig.c,v 1.2.2.1 2003/06/18 19:48:02 dgp Exp $
 */

#include "tclInt.h"

#ifdef __MWERKS__

/* define DEBUG/OPTIMIZED macros depending on the value of
 * Metrowerks specific precompiler functions.
 * (traceback is for PPC, macsbug for 68k) */
#	if __option(traceback) || __option(macsbug)
#		define TCL_CFG_DEBUG
#	else
#		define TCL_CFG_OPTIMIZED
#	endif

/* define PROFILED macros depending depending on the value of
 * Metrowerks specific precompiler functions. */
#	if __option(profile)
#		define TCL_CFG_PROFILED
#	endif

#else

#	ifdef TCL_DEBUG
#		define TCL_CFG_DEBUG
#	else
#		define TCL_CFG_OPTIMIZED
#	endif

#endif

/* the CFG_*DIR values need to be built up dynamically at runtime
 * because the name of the Macintosh Extension directory on a user's
 * system is not known at build time */
#define CFG_RUNTIME_PREFIX "${::env(EXT_FOLDER)}Tool Command Language"
#define CFG_RUNTIME_LIBDIR CFG_RUNTIME_PREFIX
#define CFG_RUNTIME_BINDIR CFG_RUNTIME_PREFIX
#define CFG_RUNTIME_SCRDIR CFG_RUNTIME_PREFIX":tcl"TCL_VERSION
#define CFG_RUNTIME_INCDIR CFG_RUNTIME_PREFIX
#define CFG_RUNTIME_DOCDIR CFG_RUNTIME_PREFIX
#define CFG_INSTALL_LIBDIR CFG_RUNTIME_LIBDIR
#define CFG_INSTALL_BINDIR CFG_RUNTIME_BINDIR
#define CFG_INSTALL_SCRDIR CFG_RUNTIME_SCRDIR
#define CFG_INSTALL_INCDIR CFG_RUNTIME_INCDIR
#define CFG_INSTALL_DOCDIR CFG_RUNTIME_DOCDIR

/* use system encoding */
#define TCL_CFGVAL_ENCODING NULL

/* We want to include tclPkgConfig.c to get the remaining CFG_* macros
 * and the cfg array declaration, but need our own definition of
 * TclInitEmbeddedConfigurationInformation, so we rename this routine
 * for the duration of the inclusion and declare it static */
static void TclUnusedInitEmbeddedConfigurationInformation
                                    _ANSI_ARGS_((Tcl_Interp *interp));
#define TclInitEmbeddedConfigurationInformation \
            TclUnusedInitEmbeddedConfigurationInformation
#include "tclPkgConfig.c"
#undef TclInitEmbeddedConfigurationInformation


void
TclInitEmbeddedConfigurationInformation (interp)
     Tcl_Interp* interp;            /* Interpreter the configuration
				     * command is registered in. */
{
    Tcl_Config		*cfgp;
	Tcl_Obj 		*valObj, *substObj;
	char			*subst;
	int				len;

	valObj = Tcl_NewObj();
	
	/* Call Tcl_SubstObj on all values in the cfg array and replace
	 * the existing value by the result if any substitution has
	 * occurred. This is needed because on the Mac the CFG_*DIR
	 * macros contain variables that are not known until runtime */
    for (cfgp = cfg;
	 (cfgp->key != (CONST char*) NULL) && (cfgp->key [0] != '\0') ;
	 cfgp++)
	{
	 	Tcl_SetStringObj(valObj, cfgp->value, -1);
	 	substObj = Tcl_SubstObj(interp, valObj, TCL_SUBST_VARIABLES);
	 	if( substObj ) {
	 		subst = Tcl_GetStringFromObj(substObj, &len);
	 		if ( strcmp(cfgp->value, subst) )
	 			cfgp->value = strcpy(ckalloc((unsigned)len+1), subst);
	 		Tcl_DecrRefCount(substObj);
	 	}
	}

	Tcl_DecrRefCount(valObj);

	Tcl_RegisterConfig (interp, "tcl", cfg, TCL_CFGVAL_ENCODING);
}
