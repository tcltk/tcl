#! /usr/bin/env tclsh

testConstraint notValgrind [expr {![testConstraint valgrind]}]
