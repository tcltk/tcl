#!/usr/bin/env tclsh

if {[catch {
    package require Tcl 9.0-
} msg]} {
    puts stderr "ERROR: $msg"
    puts stderr "If running this script from 'make html', set the\
	NATIVE_TCLSH environment\nvariable to point to an installed\
	tclsh9.0 (or the equivalent tclsh90.exe\non Windows)."
    exit 1
}

# Convert Ousterhout format man pages into highly crosslinked hypertext.
#
# Along the way detect many unmatched font changes and other odd things.
#
# Note well, this program is a hack rather than a piece of software
# engineering.  In that sense it's probably a good example of things
# that a scripting language, like Tcl, can do well.  It is offered as
# an example of how someone might convert a specific set of man pages
# into hypertext, not as a general solution to the problem.  If you
# try to use this, you'll be very much on your own.
#
# Copyright © 1995-1997 Roger E. Critchlow Jr
# Copyright © 2004-2010 Donal K. Fellows

set ::Version "50/9.0"
set ::CSSFILE "docs.css"

##
## Source the utility functions that provide most of the
## implementation of the transformation from nroff to html.
##
source -encoding utf-8 [file join [file dirname [info script]] tcltk-man2html-utils.tcl]

##
## Source the static configuration information.
##
source -encoding utf-8 [file join [file dirname [info script]] tcltk-man2html-config.tcl]

##
## Parse a C header file to extract the version number from a *_VERSION macro.
## The results (if found) are returned as a two-element list.
##
proc get-version {tclh {name ""}} {
    if {[file exists $tclh]} {
	set chan [open $tclh]
	try {
	    set data [read $chan]
	} finally {
	    close $chan
	}
	if {$name eq ""} {
	    set name [string toupper [file root [file tail $tclh]]]
	}
	# backslash isn't required in front of quote, but it keeps syntax
	# highlighting straight in some editors
	if {[regexp -lineanchor \
		[string map [list @name@ $name] \
			{^#\s*define\s+@name@_VERSION\s+\"([^.])+\.([^.\"]+)}] \
		$data -> major minor]} {
	    return [list $major $minor]
	}
    }
}

##
## Look for version information within a directory structure.
##
proc find-version {top name useversion} {
    # Default search version is a glob pattern, switch it for string match:
    if { $useversion eq {{,[8-9].[0-9]{,[.ab][0-9]{,[0-9]}}}} } {
	set useversion {[8-9].[0-9]}
    }
    # Search:
    set upper [string toupper $name]
    foreach top1 [list $top $top/..] sub "{} generic" {
	foreach dirname [
	    glob -nocomplain -tails -type d -directory $top1 *] {

	    set tclh [join [list $top1 $dirname {*}$sub ${name}.h] "/"]
	    set v [get-version $tclh $upper]
	    if {[llength $v]} {
		lassign $v major minor
		# to do
		#     use glob matching instead of string matching or add
		#     brace handling to [string match]
		if {$useversion eq "" || [string match $useversion $major.$minor]} {
		    set top [file dirname [file dirname $tclh]]
		    set prefix [file dirname $top]
		    return [list $prefix [file tail $top] $major $minor]
		}
	    }
	}
    }
}

proc parse_command_line {} {
    global argv Version

    # These variables determine where the man pages come from and where
    # the converted pages go to.
    global tcltkdir tkdir tcldir webdir build_tcl build_tk verbose

    # Set defaults based on original code.
    set tcltkdir ../..
    set tkdir {}
    set tcldir {}
    set webdir ../html
    set build_tcl 0
    set opt_build_tcl 0
    set build_tk 0
    set opt_build_tk 0
    set verbose 0
    # Default search version is a glob pattern
    set useversion {{,[8-9].[0-9]{,[.ab][0-9]{,[0-9]}}}}

    # Handle arguments a la GNU:
    #   --version
    #   --useversion=<version>
    #   --help
    #   --srcdir=/path
    #   --htmldir=/path

    foreach option $argv {
	switch -glob -- $option {
	    --version {
		puts "tcltk-man-html $Version"
		exit 0
	    }

	    --help {
		puts "usage: tcltk-man-html \[OPTION\] ...\n"
		puts "  --help              print this help, then exit"
		puts "  --version           print version number, then exit"
		puts "  --srcdir=DIR        find tcl and tk source below DIR"
		puts "  --htmldir=DIR       put generated HTML in DIR"
		puts "  --tcl               build tcl help"
		puts "  --tk                build tk help"
		puts "  --useversion        version of tcl/tk to search for"
		puts "  --verbose           whether to print longer messages"
		exit 0
	    }

	    --srcdir=* {
		# length of "--srcdir=" is 9.
		set tcltkdir [string range $option 9 end]
	    }

	    --htmldir=* {
		# length of "--htmldir=" is 10
		set webdir [string range $option 10 end]
	    }

	    --useversion=* {
		# length of "--useversion=" is 13
		set useversion [string range $option 13 end]
	    }

	    --tcl {
		set build_tcl 1
		set opt_build_tcl 1
	    }

	    --tk {
		set build_tk 1
		set opt_build_tk 1
	    }

	    --verbose=* {
		set verbose [string range $option \
				 [string length --verbose=] end]
	    }
	    default {
		puts stderr "tcltk-man-html: unrecognized option -- `$option'"
		exit 1
	    }
	}
    }

    if {!$build_tcl && !$build_tk} {
	set build_tcl 1
	set build_tk 1
    }

    set major ""
    set minor ""

    if {$build_tcl} {
	# Find Tcl (firstly using glob pattern / backwards compatible way)
	set tcldir [lindex [lsort [glob -nocomplain -tails -type d \
		-directory $tcltkdir tcl$useversion]] end]
	if {$tcldir ne ""} {
	    # obtain version from generic header if we can:
	    lassign [get-version [file join $tcltkdir $tcldir generic tcl.h]] major minor
	} else {
	    lassign [find-version $tcltkdir tcl $useversion] tcltkdir tcldir major minor
	}
	if {$tcldir eq "" && $opt_build_tcl} {
	    puts stderr "tcltk-man-html: couldn't find Tcl below $tcltkdir"
	    exit 1
	}
	puts "using Tcl source directory [file join $tcltkdir $tcldir]"
    }


    if {$build_tk} {
	# Find Tk (firstly using glob pattern / backwards compatible way)
	set tkdir [lindex [lsort [glob -nocomplain -tails -type d \
		-directory $tcltkdir tk$useversion]] end]
	if {$tkdir ne ""} {
	    if {$major eq ""} {
		# obtain version from generic header if we can:
		lassign [get-version [file join $tcltkdir $tkdir generic tk.h]] major minor
	    }
	} else {
	    lassign [find-version $tcltkdir tk $useversion] tcltkdir tkdir major minor
	}
	if {$tkdir eq "" && $opt_build_tk} {
	    puts stderr "tcltk-man-html: couldn't find Tk below $tcltkdir"
	    exit 1
	}
	puts "using Tk source directory [file join $tcltkdir $tkdir]"
    }

    puts "verbose messages are [expr {$verbose ? {on} : {off}}]"

    # the title for the man pages overall
    global overall_title
    set overall_title ""
    if {$build_tcl} {
	if {$major ne ""} {
	    append overall_title "Tcl $major.$minor"
	} else {
	    append overall_title "Tcl [capitalize $tcldir]"
	}
    }
    if {$build_tcl && $build_tk} {
	append overall_title "/"
    }
    if {$build_tk} {
	append overall_title "[capitalize $tkdir]"
    }
    append overall_title " Documentation"
}

proc capitalize {string} {
    return [string toupper $string 0]
}

##
## Returns the style sheet.
##
proc css-style args {
    upvar 1 style style
    set body [uplevel 1 [list subst [lindex $args end]]]
    set tokens [join [lrange $args 0 end-1] ", "]
    append style $tokens " \{" $body "\}\n"
}

##
## Generate a specific keyword file.
##
proc write-keyword-file {html keyheader letter keys} {
    global tcltkdesc overall_title manual
    set afp [open $html/Keywords/$letter.html w]
    try {
	fconfigure $afp -translation lf -encoding utf-8
	puts $afp [htmlhead "$tcltkdesc Keywords - $letter" \
		"$tcltkdesc Keywords - $letter" \
		$overall_title "../[indexfile]"]
	puts $afp $keyheader
	puts $afp "<dl class=\"keylist\">"
	foreach k [lsort -dictionary $keys] {
	    # Remove keyword- prefix
	    set keyword [string range $k 8 end]
	    puts $afp "<dt><a name=\"[nospace-text $keyword]\" id=\"[nospace-text $keyword]\">$keyword</a></dt>"
	    set refs {}
	    foreach man $manual($k) {
		lassign $man name file
		if {[info exists manual(tooltip-$file)]} {
		    set tooltip $manual(tooltip-$file)
		    if {[string match {*[<>""]*} $tooltip]} {
			manerror "bad tooltip for $file: \"$tooltip\""
		    }
		    lappend refs "<a href=\"../$file\" title=\"$tooltip\">$name</a>"
		} else {
		    lappend refs "<a href=\"../$file\">$name</a>"
		}
	    }
	    puts $afp "<dd>[join $refs {, }]</dd>"
	}
	puts $afp "</dl>"
	# insert merged copyrights
	puts $afp [copyout $manual(merge-copyrights)]
	puts $afp "</body></html>"
    } finally {
	close $afp
    }
}

##
## foreach of the man directories specified by args
## convert manpages into hypertext in the directory
## specified by html.
##
proc make-man-pages {html args} {
    global manual overall_title tcltkdesc verbose
    global excluded_pages forced_index_pages process_first_patterns
    set letters {A B C D E F G H I J K L M N O P Q R S T U V W X Y Z}

    makedirhier $html
    set cssfd [open $html/$::CSSFILE w]
    try {
	fconfigure $cssfd -translation lf -encoding utf-8
	puts $cssfd [css-stylesheet]
    } finally {
	close $cssfd
    }
    set manual(short-toc-n) 1
    set manual(short-toc-fp) [open $html/[indexfile] w]
    fconfigure $manual(short-toc-fp) -translation lf -encoding utf-8
    puts $manual(short-toc-fp) [htmlhead $overall_title $overall_title]
    puts $manual(short-toc-fp) "<dl class=\"keylist\">"
    set manual(merge-copyrights) {}

    foreach arg $args {
	# preprocess to set up subheader for the rest of the files
	if {![llength $arg]} {
	    continue
	}
	lassign $arg -> name file
	if {[regexp {(.*)(?: Package)? Commands(?:, version .*)?} $name -> pkg]} {
	    set name "$pkg Commands"
	} elseif {[regexp {(.*)(?: Package)? C API(?:, version .*)?} $name -> pkg]} {
	    set name "$pkg C API"
	}
	lappend manual(subheader) $name $file
    }

    ##
    ## parse the manpages in a section of the docs (split by
    ## package) and construct formatted manpages
    ##
    foreach arg $args {
	if {[llength $arg]} {
	    make-manpage-section $html $arg
	}
    }

    ##
    ## build the keyword index.
    ##
    if {!$verbose} {
	puts stderr "Assembling index"
    }
    file delete -force -- $html/Keywords
    makedirhier $html/Keywords
    set keyfp [open $html/Keywords/[indexfile] w]
    try {
	fconfigure $keyfp -translation lf -encoding utf-8
	puts $keyfp [htmlhead "$tcltkdesc Keywords" "$tcltkdesc Keywords" \
		$overall_title "../[indexfile]"]
	# Create header first
	set keyheader {}
	foreach a $letters {
	    set keys [array names manual "keyword-\[[string totitle $a$a]\]*"]
	    if {[llength $keys]} {
		lappend keyheader "<a href=\"$a.html\">$a</a>"
	    } else {
		# No keywords for this letter
		lappend keyheader $a
	    }
	}
	set keyheader <h3>[join $keyheader " |\n"]</h3>
	puts $keyfp $keyheader
	foreach a $letters {
	    set keys [array names manual "keyword-\[[string totitle $a$a]\]*"]
	    if {![llength $keys]} {
		continue
	    }
	    # Per-keyword page
	    write-keyword-file $html $keyheader $a $keys
	}
	# insert merged copyrights
	puts $keyfp [copyout $manual(merge-copyrights)]
	puts $keyfp "</body></html>"
    } finally {
	close $keyfp
    }

    ##
    ## finish off short table of contents
    ##
    puts $manual(short-toc-fp) "<dt><a href=\"Keywords/[indexfile]\">Keywords</a><dd>The keywords from the $tcltkdesc man pages."
    puts $manual(short-toc-fp) "</dl>"
    # insert merged copyrights
    puts $manual(short-toc-fp) [copyout $manual(merge-copyrights)]
    puts $manual(short-toc-fp) "</body></html>"
    close $manual(short-toc-fp)

    ##
    ## output man pages
    ##
    unset manual(section)
    if {!$verbose} {
	puts stderr "Rescanning [llength $manual(all-pages)] pages to build cross links and write out"
    }
    foreach path $manual(all-pages) wing_name $manual(all-page-domains) {
	set manual(wing-file) [file dirname $path]
	set manual(tail) [file tail $path]
	set manual(name) [file root $manual(tail)]
	try {
	    set text $manual(output-$manual(wing-file)-$manual(name))
	    set ntext 0
	    foreach item $text {
		incr ntext [llength [split $item \n]]
		incr ntext
	    }
	    set toc $manual(toc-$manual(wing-file)-$manual(name))
	    set ntoc 0
	    foreach item $toc {
		incr ntoc [llength [split $item \n]]
		incr ntoc
	    }
	    if {$verbose} {
		puts stderr "rescanning page $manual(name) $ntoc/$ntext"
	    } else {
		puts -nonewline stderr .
	    }
	    set outfd [open $html/$manual(wing-file)/$manual(name).html w]
	    fconfigure $outfd -translation lf -encoding utf-8
	    puts $outfd [htmlhead "$manual($manual(wing-file)-$manual(name)-title)" \
		    $manual(name) $wing_name "[indexfile]" \
		    $overall_title "../[indexfile]"]
	    if {($ntext > 60) && ($ntoc > 32)} {
		foreach item $toc {
		    puts $outfd $item
		}
	    } elseif {$manual(name) in $forced_index_pages} {
		if {!$verbose} {puts stderr ""}
		manerror "forcing index generation"
		foreach item $toc {
		    puts $outfd $item
		}
	    }
	    foreach item $text {
		puts $outfd [insert-cross-references $item]
	    }
	    puts $outfd "</body></html>"
	} on error msg {
	    if {$verbose} {
		puts stderr $msg
	    } else {
		puts stderr "\nError when processing $manual(name): $msg"
	    }
	} finally {
	    catch {close $outfd}
	}
    }
    if {!$verbose} {
	puts stderr "\nDone"
    }
    return {}
}

##
## Helper for assembling the descriptions of base packages (i.e., Tcl and Tk).
##
proc plus-base {var root glob name dir desc} {
    global tcltkdir
    if {$var} {
	if {[file exists $tcltkdir/$root/README.md]} {
	    set f [open $tcltkdir/$root/README.md]
	    try {
		fconfigure $f -encoding utf-8
		set d [read $f]
	    } finally {
		close $f
	    }
	    if {[regexp {This is the \*\*\w+ (\S+)\*\* source distribution} $d -> version]} {
	       append name ", version $version"
	    }
	}
	set glob $root/$glob
	return [list $tcltkdir/$glob $name $dir $desc]
    }
}

##
## Helper for assembling the descriptions of contributed packages.
##
proc plus-pkgs {type args} {
    global build_tcl tcltkdir tcldir
    if {$type ni {n 3}} {
	error "unknown type \"$type\": must be 3 or n"
    }
    if {!$build_tcl} return
    set result {}
    set pkgsdir $tcltkdir/$tcldir/pkgs
    foreach {dir name version} $args {
	set globpat $pkgsdir/$dir/doc/*.$type
	if {![llength [glob -type f -nocomplain $globpat]]} {
	    # Fallback for manpages generated using doctools
	    set globpat $pkgsdir/$dir/doc/man/*.$type
	    if {![llength [glob -type f -nocomplain $globpat]]} {
		continue
	    }
	}
	set dir [string trimright $dir "0123456789-."]
	switch $type {
	    n {
		set title "$name Package Commands"
		if {$version ne ""} {
		    append title ", version $version"
		}
		set dir [string totitle $dir]Cmd
		set desc \
		    "The additional commands provided by the $name package."
	    }
	    3 {
		set title "$name Package C API"
		if {$version ne ""} {
		    append title ", version $version"
		}
		set dir [string totitle $dir]Lib
		set desc \
		    "The additional C functions provided by the $name package."
	    }
	}
	lappend result [list $globpat $title $dir $desc]
    }
    return $result
}

##
## Helper for extracting the name and version of a contributed package
## from its source tree.
##
proc extract-package-version {pkgsDir dir} {
    set description [split $dir -]
    if {2 != [llength $description]} {
	regexp {([^0-9]*)(.*)} $dir -> n v
	set description [list $n $v]
    }

    # ... but try to extract (name, version) from subdir contents
    try {
	try {
	    set f [open [file join $pkgsDir $dir configure.in]]
	} trap {POSIX ENOENT} {} {
	    set f [open [file join $pkgsDir $dir configure.ac]]
	}
	fconfigure $f -encoding utf-8
	foreach line [split [read $f] "\n"] {
	    if {[regexp {^AC_INIT\(\s*\[(.*?)\]\s*,\s*\[(.*?)\]\s*\)\s*$} $line -> n v]} {
		return [list $n $v]
	    }
	}
    } on error {} {
	puts "package folder without package ignored: $dir"
	# No can do! Empty means skip this one.
	return
    } finally {
	catch {close $f}
    }

    # Have to use the guess derived from the directory name as a fallback;
    # the configure script appears to be ill-defined.
    return $description
}

try {
    # Parse what the user told us to do
    parse_command_line

    # Some strings depend on what options are specified
    set tcltkdesc ""
    set cmdesc ""
    set appdir ""
    if {$build_tcl} {
	append tcltkdesc "Tcl"
	append cmdesc "Tcl"
	append appdir "$tcldir"
    }
    if {$build_tcl && $build_tk} {
	append tcltkdesc "/"
	append cmdesc " and "
	append appdir ","
    }
    if {$build_tk} {
	append tcltkdesc "Tk"
	append cmdesc "Tk"
	append appdir "$tkdir"
    }

    apply { {} {
    global packageBuildList tcltkdir tcldir build_tcl

    # When building docs for Tcl, try to build docs for bundled packages too
    set packageBuildList {}
    if {$build_tcl} {
	set pkgsDir [file join $tcltkdir $tcldir pkgs]
	set subdirs [glob -nocomplain -types d -tails -directory $pkgsDir *]
	foreach dir [lsort $subdirs] {
	    # Get the package name, version from the package contents.
	    set description [extract-package-version $pkgsDir $dir]

	    if {[llength $description] && [file exists [file join $pkgsDir $dir configure]]} {
		# Looks like a package, record our best extraction attempt
		# Records a triple: directory, name, version
		lappend packageBuildList $dir {*}$description
	    }
	}
    }

    # Get the list of packages to try, and what their human-readable names
    # are. Note that the package directory list should be version-less.
    try {
	set packageDirNameMap {}
	if {$build_tcl} {
	    set f [open $tcltkdir/$tcldir/pkgs/package.list.txt]
	    try {
		fconfigure $f -encoding utf-8
		foreach line [split [read $f] \n] {
		    if {[string trim $line] eq ""} continue
		    if {[string match #* $line]} continue
		    lassign $line dir name
		    lappend packageDirNameMap $dir $name
		}
	    } finally {
		close $f
	    }
	}
    } trap {POSIX ENOENT} {} {
	set packageDirNameMap $::default_package_dir_name_map
    }

    # Convert to human readable names, if applicable
    for {set idx 0} {$idx < [llength $packageBuildList]} {incr idx 3} {
	lassign [lrange $packageBuildList $idx $idx+2] d n v
	if {[dict exists $packageDirNameMap $n]} {
	    lset packageBuildList $idx+1 [dict get $packageDirNameMap $n]
	}
    }
    }}

    #
    # Invoke the scraper/converter engine.
    #
    make-man-pages $webdir \
	[list $tcltkdir/{$appdir}/doc/*.1 "$tcltkdesc Applications" UserCmd \
	     "The interpreters which implement $cmdesc."] \
	[plus-base $build_tcl $tcldir doc/*.n {Tcl Commands} TclCmd \
	     "The commands which the <b>tclsh</b> interpreter implements."] \
	[plus-base $build_tk $tkdir doc/*.n {Tk Commands} TkCmd \
	     "The additional commands which the <b>wish</b> interpreter implements."] \
	{*}[plus-pkgs n {*}$packageBuildList] \
	[plus-base $build_tcl $tcldir doc/*.3 {Tcl C API} TclLib \
	     "The C functions which a Tcl extended C program may use."] \
	[plus-base $build_tk $tkdir doc/*.3 {Tk C API} TkLib \
	     "The additional C functions which a Tk extended C program may use."] \
	{*}[plus-pkgs 3 {*}$packageBuildList]
} on error {msg opts} {
    # On failure make sure we show what went wrong. We're not supposed
    # to get here though; it represents a bug in the script.
    puts $msg\n[dict get $opts -errorinfo]
    exit 1
}

# Local-Variables:
# mode: tcl
# End:
