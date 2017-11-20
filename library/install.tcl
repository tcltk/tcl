###
# Installer actions built into tclsh and invoked
# if the first command line argument is "install"
###
if {[llength $argv] < 2} {
  exit 0
}
switch [lindex $argv 1] {
  mkzip {
    zipfs mkzip {*}[lrange $argv 2 end]
  }
}
exit 0
