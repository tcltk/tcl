# checkLibraryDoc.tcl --
#
# This script attempts to determine what APIs exist in the source base that 
# have not been documented.  By grepping through all of the doc/*.3 man 
# pages, looking for "Pkg_*" (e.g., Tcl_ or Tk_), and comparing this list
# against the list of Pkg_ APIs found in the source (e.g., tcl8.1/*/*.[ch])
# we create six lists:
#      1) APIs in Source not in Docs.
#      2) APIs in Docs not in Source.
#      3) Internal APIs and structs.
#      4) Misc APIs and structs that we are not documenting.
#      5) Command APIs (e.g., Tcl_ArrayObjCmd.)
#      6) Proc pointers (e.g., Tcl_CloseProc.)
# 
# Note: Each list is "a best guess" approximation.  If developers write
# non-standard code, this script will produce erroneous results.  Each
# list should be carefully checked for accuracy. 
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: checkLibraryDoc.tcl,v 1.1.2.1 1999/04/14 00:26:38 surles Exp $


#lappend auto_path "c:/program\ files/tclpro1.2/win32-ix86/bin"
if {[catch {package require Tclx}]} {
    puts "error: could not load TclX.  Please set TCL_LIBRARY."
    exit 1
}

# A list of structs that are known to be undocumented.

set StructList {
    Tcl_AsyncHandler \
    Tcl_CallFrame \
    Tcl_Condition \
    Tcl_Encoding \
    Tcl_EncodingState \
    Tcl_EncodingType \
    Tcl_EolTranslation \
    Tcl_HashEntry \
    Tcl_HashSearch \
    Tcl_HashTable \
    Tcl_Mutex \
    Tcl_Pid \
    Tcl_QueuePosition \
    Tcl_ResolvedVarInfo \
    Tcl_SavedResult \
    Tcl_ThreadDataKey \
    Tcl_ThreadId \
    Tcl_Time \
    Tcl_TimerToken \
    Tcl_Token \
    Tcl_Trace \
    Tcl_Value \
    Tcl_ValueType \
    Tcl_Var \
}

# Misc junk that appears in the comments of the source.  This just 
# allows us to filter comments that "fool" the script.

set CommentList {
    Tcl_Create\[Obj\]Command \
    Tcl_DecrRefCount\\n \
    Tcl_NewObj\\n \
}

# Main entry point to this script.

proc main {} {
    global argv0 
    global argv 

    set len [llength $argv]
    if {($len != 2) && ($len != 3)} {
	puts "usage: $argv0 pkgName pkgDir \[outFile\]"
	puts "   pkgName == Tcl,Tk"
	puts "   pkgDir  == /home/surles/cvs/tcl8.1"
	exit 1
    }

    set pkg [lindex $argv 0]
    set dir [lindex $argv 1]
    if {[llength $argv] == 3} {
	set file [open [lindex $argv 2] w]
    } else {
	set file stdout
    }

    foreach {c d} [compare [grepCode $dir $pkg] [grepDocs $dir $pkg]] {}
    filter $c $d $dir $pkg $file

    if {$file != "stdout"} {
	close $file
    }
    return
}
    
# Intersect the two list and write out the sets of APIs in one
# list that is not in the other.

proc compare {list1 list2} {
    set inter [intersect3 $list1 $list2]
    return [list [lindex $inter 0] [lindex $inter 2]]
}

# Filter the lists into the six lists we report on.  Then write
# the results to the file.

proc filter {code docs dir pkg {outFile stdout}} {
    # A list of Tcl command APIs.  These are not documented.
    # This list should just be verified for accuracy.

    set cmds  {}
    
    # A list of proc pointer structs.  These are not documented.
    # This list should just be verified for accuracy.

    set procs {}

    # A list of internal declarations.  These are not documented.
    # This list should just be verified for accuracy.

    set decls [grepDecl $dir $pkg]

    # A list of misc. procedure declarations that are not documented.
    # This list should just be verified for accuracy.

    set misc [grepMisc $dir $pkg]

    # A list of APIs in the source, not in the docs.
    # This list should just be verified for accuracy.

    foreach x $code {
	if {[string match *Cmd $x]} {
	    lappend cmds $x
	} elseif {[string match *Proc $x]} {
	    lappend procs $x
	} elseif {[lsearch -exact $decls $x] >= 0} {
	    # No Op.
	} elseif {[lsearch -exact $misc $x] >= 0} {
	    # No Op.
	} else {
	    lappend apis $x
	}
    }

    dump $apis  "APIs in Source not in Docs." $outFile
    dump $docs  "APIs in Docs not in Source." $outFile
    dump $decls "Internal APIs and structs."  $outFile
    dump $misc  "Misc APIs and structs that we are not documenting." $outFile
    dump $cmds  "Command APIs."  $outFile
    dump $procs "Proc pointers." $outFile
    return
}

# Print the list of APIs if the list is not null.

proc dump {list title file} {
    if {$list != {}} {
	puts $file ""
	puts $file $title
	puts $file "---------------------------------------------------------"
	foreach x $list {
	    puts $file $x
	}
    }
}

# Grep into "dir/*/*.[ch]" looking for APIs that match $pkg_*.
# (e.g., Tcl_Exit).  Return a list of APIs.

proc grepCode {dir pkg} {
    set apis [exec grep "${pkg}_\.\*" "${dir}/\*/\*\.\[ch\]" | cat]
    set pat1 ".*(Tcl_\[A-z0-9]+).*$"
 
    foreach a [split $apis "\n"] {
	if {[regexp --  $pat1 $a main n1]} {
	    set result([string trim $n1]) 1
	}
    }
    return [lsort [array names result]]
}

# Grep into "dir/doc/*.3" looking for APIs that match $pkg_*.
# (e.g., Tcl_Exit).  Return a list of APIs.

proc grepDocs {dir pkg} {
    set apis [exec grep "\\fB${pkg}_\.\*\\fR" "${dir}/doc/\*\.3" | cat]
    set pat1 ".*(Tcl_\[A-z0-9]+).*$"

    foreach a $apis {
	if {[regexp -- $pat1 $a main n1]} {
	    set result([string trim $n1]) 1
	}
    }
    return [lsort [array names result]]
}

# Grep into "generic/pkgIntDecls.h" looking for APIs that match $pkg_*.
# (e.g., Tcl_Export).  Return a list of APIs.

proc grepDecl {dir pkg} {
    set file [file join $dir generic "[string tolower $pkg]IntDecls.h"] 
    set apis [exec grep "^EXTERN.*\[ \t\]Tcl_.*" $file | cat]
    set pat1 ".*(Tcl_\[A-z0-9]+).*$"

    foreach a $apis {
	if {[regexp -- $pat1 $a main n1]} {
	    set result([string trim $n1]) 1
	}
    }
    return [lsort [array names result]]
}

# Grep into "*/*.[ch]" looking for APIs that match $pkg_Db*.
# (e.g., Tcl_DbCkalloc).  Return a list of APIs.

proc grepMisc {dir pkg} {
    global CommentList
    global StructList
    
    set apis [exec grep "^EXTERN.*\[ \t\]Tcl_Db.*" "${dir}/\*/\*\.\[ch\]" \
	    | cat]
    set pat1 ".*(Tcl_\[A-z0-9]+).*$"

    foreach a $apis {
	if {[regexp -- $pat1 $a main n1]} {
	    set dbg([string trim $n1]) 1
	}
    }

    set result {}
    eval {lappend result} $StructList
    eval {lappend result} [lsort [array names dbg]]
    eval {lappend result} $CommentList
    return $result
}

main

