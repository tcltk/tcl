/* 
 * tclPkgConfig.c --
 *
 *	This file contains the configuration information to
 *	embed into the tcl binary library.
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclPkgConfig.c,v 1.1.2.2 2002/02/05 20:45:51 andreas_kupries Exp $
 */

/* Note, the definitions in this module are influenced by the
 * following C preprocessor macros:
 *
 * OSCMa  = shortcut for "old style configuration macro activates"
 * NSCMdt = shortcut for "new style configuration macro declares that"
 *
 * - TCL_THREADS		OSCMa  compilation as threaded core.
 * - TCL_MEM_DEBUG		OSCMa  memory debugging.
 * - TCL_COMPILE_DEBUG		OSCMa  debugging of bytecode compiler.
 * - TCL_COMPILE_STATS		OSCMa  bytecode compiler statistics.
 *
 * - TCL_CFG_DO64BIT		NSCMdt tcl is compiled for a 64bit system.
 * - TCL_CFG_DEBUG		NSCMdt tcl is compiled with symbol info on.
 * - TCL_CFG_OPTIMIZED		NSCMdt tcl is compiled with cc optimizations on.
 * - TCL_CFG_PROFILED		NSCMdt tcl is compiled with profiling info.
 *
 * - CFG_RUNTIME_PREFIX		Path to platform independent data at runtime
 * - CFG_RUNTIME_EXEC_PREFIX	Path to platform dependent data at runtime
 * - CFG_INSTALL_PREFIX		Path to platform independent data during installation
 * - CFG_INSTALL_EXEC_PREFIX	Path to platform dependent data during installation
 *
 * - TCL_CFGVAL_ENCODING	string containing the encoding used for the
 *                              configuration values.
 */

#include "tclInt.h"



/* Use C preprocessor statements to define the various values for the
 * embedded configuration information.  */

#ifdef TCL_THREADS
#  define  CFG_THREADED "1"
#else
#  define  CFG_THREADED "0"
#endif
#ifdef TCL_MEM_DEBUG
#  define CFG_MEMDEBUG "1"
#else
#  define CFG_MEMDEBUG "0"
#endif
#ifdef TCL_COMPILE_DEBUG
#  define CFG_COMPILE_DEBUG "1"
#else
#  define CFG_COMPILE_DEBUG "0"
#endif
#ifdef TCL_COMPILE_STATS
#  define CFG_COMPILE_STATS "1"
#else
#  define CFG_COMPILE_STATS "0"
#endif
#ifdef TCL_CFG_DO64BIT
#  define CFG_64            "1"
#else
#  define CFG_64            "0"
#endif
#ifdef TCL_CFG_DEBUG
#  define CFG_DEBUG         "1"
#else
#  define CFG_DEBUG         "0"
#endif
#ifdef TCL_CFG_OPTIMIZED
#  define CFG_OPTIMIZED     "1"
#else
#  define CFG_OPTIMIZED     "0"
#endif
#ifdef TCL_CFG_PROFILED
#  define CFG_PROFILED     "1"
#else
#  define CFG_PROFILED     "0"
#endif

static Tcl_Config cfg [] = {
  {"debug",               CFG_DEBUG},
  {"threaded",            CFG_THREADED},
  {"profiled",            CFG_PROFILED},
  {"64bit",               CFG_64},
  {"optimized",           CFG_OPTIMIZED},
  {"mem_debug",           CFG_MEMDEBUG},
  {"compile_debug",       CFG_COMPILE_DEBUG},
  {"compile_stats",       CFG_COMPILE_STATS},
  {"prefix,runtime",      CFG_RUNTIME_PREFIX},
  {"exec_prefix,runtime", CFG_RUNTIME_EXEC_PREFIX},
  {"prefix,install",      CFG_INSTALL_PREFIX},
  {"exec_prefix,install", CFG_INSTALL_EXEC_PREFIX},
  /* Last entry, closes the array */
  {NULL, NULL}
};

void
TclInitEmbeddedConfigurationInformation (interp)
     Tcl_Interp* interp;            /* Interpreter the configuration
				     * command is registered in. */
{
  Tcl_RegisterConfig (interp, "tcl", cfg, TCL_CFGVAL_ENCODING);
}
