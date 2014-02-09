if {![package vsatisfies [package provide Tcl] 8.6]} {return}
package ifneeded http 2.8.8 [list tclPkgSetup $dir http 2.8.8 {{http.tcl source {::http::config ::http::formatQuery ::http::geturl ::http::reset ::http::wait ::http::register ::http::unregister ::http::mapReply}}}]
package ifneeded cookiejar 0.1 [list source [file join $dir cookiejar.tcl]]
package ifneeded tcl::idna 1.0 [list source [file join $dir idna.tcl]]
