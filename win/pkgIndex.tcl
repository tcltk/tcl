# Tcl package index file, version 1.0
# This file contains package information for Windows-specific extensions.
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: pkgIndex.tcl,v 1.3 1999/04/16 00:48:07 stanton Exp $

package ifneeded registry 1.0 [list tclPkgSetup $dir registry 1.0 {{tclreg81.dll load registry}}]
package ifneeded dde 1.0 [list tclPkgSetup $dir dde 1.0 {{tcldde81.dll load dde}}]
