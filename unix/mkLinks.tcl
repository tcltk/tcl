#!/bin/sh
# mkLinks.tcl -- 
#	This generates the mkLinks script
# \
exec tclsh "$0" ${1+"$@"}

puts stdout \
{#!/bin/sh
# This script is invoked when installing manual entries.  It generates
# additional links to manual entries, corresponding to the procedure
# and command names described by the manual entry.  For example, the
# Tcl manual entry Hash.3 describes procedures Tcl_InitHashTable,
# Tcl_CreateHashEntry, and many more.  This script will make hard
# links so that Tcl_InitHashTable.3, Tcl_CreateHashEntry.3, and so
# on all refer to Hash.3 in the installed directory.
#
# Because of the length of command and procedure names, this mechanism
# only works on machines that support file names longer than 14 characters.
# This script checks to see if long file names are supported, and it
# doesn't make any links if they are not.
#
# The script takes one argument, which is the name of the directory
# where the manual entries have been installed.

if test $# != 1; then
    echo "Usage: mkLinks dir"
    exit 1
fi

cd $1
echo foo > xyzzyTestingAVeryLongFileName.foo
x=`echo xyzzyTe*`
echo foo > xyzzyTestingaverylongfilename.foo
y=`echo xyzzyTestingav*`
rm xyzzyTe*
if test "$x" != "xyzzyTestingAVeryLongFileName.foo"; then
    exit
fi
if test "$y" != "xyzzyTestingaverylongfilename.foo"; then
    CASEINSENSITIVEFS=1
fi
}

set case_insensitive_test { if test "${CASEINSENSITIVEFS:-}" != "1"; then} 
set case_insensitive_test_fi {; fi} 

foreach file $argv {
    set in [open $file]
    set tail [file tail $file]
    set ext [file extension $file]
    set state begin
    while {[gets $in line] >= 0} {
	switch $state {
	    begin {
		if {[regexp "^.SH NAME" $line]} {
		    set state name
		}
	    }
	    name {
		regsub {\\-.*} $line {} line
		set rmOutput ""
		set lnOutput ""
		set namelist {}
		foreach name [split $line ,] {
		    regsub -all {(\\)? } $name "" name
		    if {![string match $name*$ext $tail]} {
		    	if {[string match -nocase $name*$ext $tail]} {
			   set tst $case_insensitive_test 
			   set tstfi $case_insensitive_test_fi 
		    	} else {
			   set tst ""
			   set tstfi ""
		    }
			lappend namelist $name$ext
			append rmOutput "   $tst rm -f $name$ext$tstfi\n"
			append lnOutput "   $tst ln $tail $name$ext$tstfi\n"
		}
		}
		if { [llength $namelist] } {
		    puts "if test -r $tail; then"
		    puts -nonewline $rmOutput
		    puts -nonewline $lnOutput
		    puts "fi"
		}
		set state end
	    }
	    end {
		break
	    }
	}
    }
    close $in
}
puts "exit 0"
