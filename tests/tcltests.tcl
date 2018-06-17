#! /usr/bin/env tclsh

# Some tests require the "exec" command.
# Skip them if exec is not defined.
testConstraint exec [llength [info commands exec]]

testConstraint notValgrind [expr {![testConstraint valgrind]}]
