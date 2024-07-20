#! /usr/bin/env tclsh

# You may distribute and/or modify this program under the terms of the GNU
# Affero General Public License as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# See the file "COPYING" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Thread

set ::tids [list]
for {set i 0} {$i < 4} {incr i} {
    lappend ::tids [thread::create [string map [list IVALUE $i] {
	set curdir [file dirname [info script]]
	load [file join $curdir tsdPerf[info sharedlibextension]]

	while 1 {
	    tsdPerfSet IVALUE
	}
    }]]
}

puts TIDS:$::tids

set curdir [file dirname [info script]]
load [file join $curdir tsdPerf[info sharedlibextension]]

tsdPerfSet 1234
while 1 {
    puts "TIME:[time {set value [tsdPerfGet]} 1000] VALUE:$value"
}
