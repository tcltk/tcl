# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.test" when running tcltest
# in this directory.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2000 by Ajuba Solutions
# All rights reserved.
# 
# RCS: @(#) $Id: all.tcl,v 1.14.4.1 2002/02/05 02:22:03 wolfsuit Exp $

set tcltestVersion [package require tcltest]
namespace import -force tcltest::*

if {$tcl_platform(platform) == "macintosh"} {
	tcltest::singleProcess 1
}

tcltest::testsDirectory [file dir [info script]]
tcltest::runAllTests

return
