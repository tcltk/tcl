#! /usr/bin/env tclsh

package ifneeded tcltests 0.1 {
	source [file dirname [file dirname [file normalize [info script]/...]]]/tcltests.tcl
	package provide tcltests 0.1
}
