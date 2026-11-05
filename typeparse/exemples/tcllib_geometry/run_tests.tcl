# run_tests.tcl -- tcllib's own geometry.test, run against the reformulated
# geometry.tcl of this directory.
#
#   tclsh run_tests.tcl ?-c? TCLLIB_SOURCES
#
# -c : geometry_c.tcl (the commands of geom.c in place of the heavy ones).
#
# TCLLIB_SOURCES: the root of tcllib's sources (with modules/math and
# modules/devtools). Nothing is changed there: both are copied
# into a temporary directory, geometry.tcl is replaced, and geometry.test is
# run with this tclsh.

set withC [expr {[lindex $argv 0] eq "-c"}]
if {$withC} {set argv [lrange $argv 1 end]}
if {[llength $argv] != 1} {
    puts stderr "usage: tclsh run_tests.tcl ?-c? TCLLIB_SOURCES"
    exit 1
}
set src  [file normalize [lindex $argv 0]]
set here [file dirname [file normalize [info script]]]
set tmp  [file tempdir]
file mkdir [file join $tmp modules]
file copy [file join $src modules math] [file join $tmp modules math]
file copy [file join $src modules devtools] [file join $tmp modules devtools]
if {$withC} {
    # geometry.tcl of the copy: the C variant, from this directory
    set f [open [file join $tmp modules math geometry.tcl] w]
    puts $f [list source [file join $here geometry_c.tcl]]
    close $f
} else {
    file copy -force [file join $here geometry.tcl] [file join $tmp modules math geometry.tcl]
}
cd [file join $tmp modules math]
set rc [catch {exec [info nameofexecutable] geometry.test 2>@1} out]
puts [join [lsearch -all -inline -not -regexp [split $out \n] {^\+\+\+\+|^$}] \n]
file delete -force $tmp
