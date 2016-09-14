###
# This file contains instructions for how to build the Odielib library
# It will produce the following files in whatever directory it was called from:
#
# * odielibc.mk - A Makefile snippet needed to compile the odielib sources
# * odielibc.c - A C file which acts as the loader for odielibc
# * logicset.c/h - A c  
# * (several .c and .h files) - C sources that are generated on the fly by automation
###
# Ad a "just in case" version or practcl we ship locally

set ::CWD [pwd]
set ::project(builddir) $::CWD
set ::project(srcdir) [file dirname [file dirname [file normalize [info script]]]]
set ::project(sandbox)  [file dirname $::project(srcdir)]
set ::project(download) [file join $::project(sandbox) download]
set ::project(teapot)   [file join $::project(sandbox) teapot]
source [file join $::CWD .. library practcl practcl.tcl]
array set ::project [::practcl::config.tcl $CWD]

set SRCPATH $::project(srcdir)
set SANDBOX $::project(sandbox)
file mkdir $CWD/build

::practcl::target implement {
  triggers {}
}
::practcl::target tcltk {
  depends deps
  triggers {script-packages script-pkgindex}
}
::practcl::target basekit {
  depends {deps tcltk}
  triggers {}
  filename [file join $CWD tclkit_bare$::project(EXEEXT)]
}
::practcl::target packages {
  depends {deps tcltk}
}
::practcl::target distclean {}
::practcl::target example {
  depends basekit
}

switch [lindex $argv 0] {
  autoconf -
  pre -
  deps {
    ::practcl::trigger implement
  }
  os {
    puts "OS: [practcl::os]"
    parray ::project
    exit 0
  }
  wrap {
    ::practcl::depends basekit
  }
  all {
    # Auto detect missing bits
    foreach {item obj} $::make_objects {
      if {[$obj check]} {
        $obj trigger
      }
    }
  }
  package {
    ::practcl::trigger packages
  }
  default {
    ::practcl::trigger {*}$argv
  }
}

parray make

set ::CWD [pwd]
::practcl::tclkit create BASEKIT {}
BASEKIT define set name tclkit
BASEKIT define set pkg_name tclkit
BASEKIT define set pkg_version 8.7.0a
BASEKIT define set localpath tclkit
BASEKIT define set profile devel
BASEKIT source [file join $::CWD packages.tcl]

if {$make(distclean)} {
  # Clean all source code back to it's pristine state from fossil
  foreach item [BASEKIT link list package] {
    $item go
    set projdir  [$item define get localsrcdir]
    if {$projdir ne {} && [file exists $projdir]} {
      fossil $projdir clean -force
    }
  }
}

file mkdir [file join $CWD build]

if {$make(tcltk)} {
  ###
  # Download our required packages
  ###
  set tcl_config_opts {}
  set tk_config_opts {}
  switch [::practcl::os] {
    windows {
      #lappend tcl_config_opts --disable-stubs      
    }
    linux {
      lappend tk_config_opts --enable-xft=no --enable-xss=no
    }
    macosx {
      lappend tcl_config_opts --enable-corefoundation=yes  --enable-framework=no
      lappend tk_config_opts --enable-aqua=yes
    }
  }
  lappend tcl_config_opts --with-tzdata --prefix [BASEKIT define get prefix]
  BASEKIT project TCLCORE define set config_opts $tcl_config_opts
  BASEKIT project TCLCORE go
  set _TclSrcDir [BASEKIT project TCLCORE define get localsrcdir]
  BASEKIT define set tclsrcdir $_TclSrcDir
  lappend tk_config_opts --with-tcl=$_TclSrcDir
  BASEKIT project TKCORE define set config_opts $tk_config_opts
  BASEKIT project TCLCORE compile
  BASEKIT project TKCORE compile
}

if {$make(basekit)} {
  BASEKIT implement $CWD
  ::practcl::build::static-tclsh $target(basekit) BASEKIT
}

if {[lindex $argv 0] eq "package"} {
  #set result {}
  foreach item [lrange $argv 1 end] {
    set obj [BASEKIT project $item]
    puts [list build $item [$obj define get static] [info object class $obj]]
    if {[string is true [$obj define get static]]} {
      $obj compile
    }
    if {[string is true [$obj define get vfsinstall]]} {
      $obj install-vfs
    }
  }
  #puts "RESULT: $result"
} elseif {$make(packages)} {
  foreach item [BASEKIT link list package] {
    if {[string is true [$item define get static]]} {
      $item compile
    }
    if {[string is true [$item define get vfsinstall]]} {
      $item install-vfs
    }
  }
}


if {$make(example)} {
  file mkdir [file join $CWD example.vfs]
  BASEKIT wrap $CWD example example.vfs
}

if {[lindex $argv 0] eq "wrap"} {
  BASEKIT wrap $CWD {*}[lrange $argv 1 end]
}
