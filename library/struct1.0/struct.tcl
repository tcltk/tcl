package provide struct 1.0
source [file join [file dirname [info script]] stack.tcl]
source [file join [file dirname [info script]] queue.tcl]
namespace eval struct {
	namespace export *
	namespace import stack::*
	namespace import queue::*
}
