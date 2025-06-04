# this script scans nroff files and builds a markdown table reporting all
# nroff macros being used in these scripts
#
# the script should be called from the 'tools' directory of the Tcl repository

package require Tcl 9.0

# macros already implemented in man2markdown.tcl:
set macrosOK {TH SH SS PP PQ QR BS BE AS RS RE TP IP VS VE}

#puts "Working nroff macros are printed in (..)"

set basedir [file dirname [file normalize [info script]]]
set topdir [file dirname $basedir]
# extract any macro invocations (.XX at start of a line) and generate a list of 
# file,macro pairs

foreach {pkg subdir} [list Tcl $topdir/doc] {
	cd $subdir
	puts "Scanning $pkg: $subdir ..."
	foreach file [lsort [glob $subdir/*]] {
		set macros [list]
		foreachLine line $file {
			if {[regexp {^\.[A-Z][A-Z]} $line]} {
				set macro [string range $line 1 2]
				lappend macros $macro
			}
		}
		set macros [lsort -unique $macros]
		puts -nonewline "[file tail $file]: "
		foreach macro $macros {
			if {$macro in $macrosOK} {
				#puts -nonewline "($macro) "
			} else {
				puts -nonewline "$macro "
			}
		}
		puts {}
	}
}

