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
# RCS: @(#) $Id: all.tcl,v 1.13 2001/09/11 17:30:44 andreas_kupries Exp $

set tcltestVersion [package require tcltest]
namespace import -force tcltest::*

tcltest::singleProcess 1
tcltest::matchFiles socket.test
tcltest::verbose {pass start}

tcltest::testsDirectory [file dir [info script]]
tcltest::runAllTests

return
