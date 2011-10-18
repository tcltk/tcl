#! /bin/sh
#\
exec tclsh $0 "$@"

if {$argc!=1} {puts stderr "Usage: [file tail $::argv0] <port>";exit 1}
set port [lindex $argv 0]

proc serve {sok h p} {
    fconfigure $sok -translation binary -buffering full
    if {[catch {
	if {[gets $sok line]<0} {error "400 Empty request"}
	if {![regexp {^GET[ 	]+([^ 	]+)} $line -> path]} {error "403 Method Not Supported"}
	puts stderr "$h:$p - GET $path"
	if {[regexp {[.][.]} $path]} {error "403 Forbidden"}
	set ty text/plain
	switch -glob  [file extension $path] {
	    .html {set ty text/html}
	    .nexe - .nmf {set ty application/octet-stream}
	}
	if {[catch {set ff [open .$path r]}]} {error "404 File Not Found"}
	fconfigure $ff -translation binary
	set data [read $ff]
	close $ff
    } err]} {
	puts $sok "HTTP/1.0 $err\r\n\r"
    } else {
	puts -nonewline $sok "HTTP/1.0 200 OK\r\nContent-Type: $ty\r\nContent-Length: [string length $data]\r\n\r\n$data"
    }
    close $sok
}
socket -server serve $port
vwait forever
