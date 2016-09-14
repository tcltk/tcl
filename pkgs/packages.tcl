###
# This script implements a basic TclTkit with statically linked
# Tk, sqlite, threads, udp, and mmtk (which includes canvas3d and tkhtml)
###

set CWD [pwd]

my define set [array get ::project]
set os [::practcl::os]
my define set os $os
puts [list BASEKIT SANDBOX $::project(sandbox)]
my define set platform $::project(TEA_PLATFORM)
my define set prefix /zvfs
my define set sandbox    [file normalize $::project(sandbox)]
my define set installdir [file join $::project(sandbox) pkg]
my define set teapot     [file join $::project(sandbox) teapot]
my define set USEMSVC    [info exists env(VisualStudioVersion)]
my define set prefix_broken_destdir [file join $::project(sandbox) tmp]
my define set HOST $os
my define set TARGET $os
my define set tclkit_bare [file join $CWD tclkit_bare$::project(EXEEXT)]

my define set name           tclkit
my define set output_c       tclkit.c
my define set libs   	 {}

my add_project TCLCORE {
  class subproject.core
  name tcl
  tag release
  static 1
}
my project TCLCORE define set srcdir [file dirname $::project(sandbox)]

my add_project TKCORE {
  class subproject.core
  name tk
  tag release
  static 1
  autoload 0
  pkg_name Tk
  initfunc Tk_Init
}

my add_project tclconfig {
  profile {
    release: 3dfb97da548fae506374ac0015352ac0921d0cc9
    devel:   practcl  
  }
  class subproject
  preload 1
  vfsinstall 0
}

my add_project thread {
  profile {
    release: 2a36d0a6c31569bfb3562e3d58e9e8204f447a7e
    devel:   practcl  
  }
  class subproject.binary
  pkg_name Thread
  autoload 1
  initfunc Thread_Init
  static 1
}

my add_project sqlite {
  profile {
    release: 40ffdfb26af3e7443b2912e1039c06bf9ed75846
    devel:   practcl
  }
  class subproject.binary
  pkg_name sqlite3
  autoload 1
  initfunc Sqlite3_Init
  static 1
  vfsinstall 0
}

my add_project udp {
  profile {
    release: 7c396e1a767db57b07b48daa8e0cfc0ea622bbe9
    devel:   practcl
  }
  class subproject.binary
  static 1
  autoload 1
  initfunc Udp_Init
  pkg_name udp
  vfsinstall 0
}

