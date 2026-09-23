package require Tcl 9

# This script converts markdown-formatted manual pages (as produced by
# man2markdown.tcl) into nroff-formatted manual pages, using Pandoc
# and the Lua filter markdown2nroff.lua.
#
#
# When run with two different directories as arguments,
# it converts all markdown files in the first directory
# to nroff files and puts them into the second directoy
#
# ```
# tclsh markdown2nroff.tcl ../doc/markdown ../doc/nroff
# ```
#
# When called with two file names, it converts this single file:
#
# ```
# tclsh markdown2nroff.tcl ../doc/markdown/clock.md ../doc/nroff/clock.n
# ```

namespace eval ::md2nroff {
	variable scriptDir [file dirname [file normalize [info script]]]
	variable filter [file join $scriptDir markdown2nroff.lua]

	# convertFile --
	#	Convert one markdown page to nroff via Pandoc
	#
	proc convertFile {mdFile nFile} {
		variable filter
		exec pandoc \
			-f markdown-tex_math_dollars-smart \
			-t man \
			--lua-filter=$filter \
			$mdFile -o $nFile
	}

	# convertDir --
	#	Convert every *.md file in inDir into inDir/*.n files in
	#	outDir (created if it doesn't exist yet)
	#
	proc convertDir {inDir outDir} {
		if {![file isdirectory $inDir]} {
			return -code error "Error: '$inDir' is not a directory"
		}
		if {[file exists $outDir] && ![file isdirectory $outDir]} {
			return -code error "Error: '$outDir' is not a directory"
		} elseif {![file exists $outDir]} {
			file mkdir $outDir
		}
		set failed [list]
		foreach mdFile [lsort -dictionary [glob -nocomplain -directory $inDir *.md]] {
			set stem [file rootname [file tail $mdFile]]
			set nFile [file join $outDir ${stem}.n]
			puts "converting $mdFile ..."
			if {[catch {convertFile $mdFile $nFile} err]} {
				puts stderr "  FAILED: $err"
				lappend failed $mdFile
			}
		}
		if {[llength $failed]} {
			return -code error "Error: [llength $failed] file(s) failed to convert:\n[join $failed \n]"
		}
	}

	proc checkPandoc {} {
		if {[auto_execok pandoc] eq ""} {
			return -code error "Error: 'pandoc' was not found on PATH. Install it from https://pandoc.org/ first."
		}
	}

	proc main {} {
		global argv
		variable scriptDir

		checkPandoc

		if {[llength $argv] == 0} {
			set inDir [file join $scriptDir .. doc markdown]
			set outDir [file join $scriptDir .. doc nroff]
			convertDir $inDir $outDir
		} elseif {[llength $argv] == 2} {
			lassign $argv in out
			if {[file isdirectory $in]} {
				convertDir $in $out
			} else {
				# single-file conversion
				convertFile $in $out
			}
		} else {
			puts stderr "usage: tclsh markdown2nroff.tcl ?inDir outDir? | ?in.md out.n?"
			exit 2
		}
	}
}

if {[info exists argv0] && [file tail [info script]] eq [file tail $argv0]} {
	# call [main] when the script is *not* sourced by another script
	# but called from the command line
	::md2nroff::main
}
