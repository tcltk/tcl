# markdown2html.tcl --
#
# this script takes the manual pages in markdown format,
# converts them into html and builds a website structure to viwe the content
# in a web browser

# currently, this script must be executed from the tools directory!

package require Tcl 9
package require json   ;# from tcllib

# all pages that appear in the HTML outline are listed in the file markdown2html-pages.txt (next to this script).
# The file defines one outline entry per line. The same manual page file may appear on several lines,
# e.g. one entry per C API function documented on it. Each line in the file is a Tcl list with these two elements:
# - path = the position in the hierarchical outline, itself a list, e.g. {Tcl Commands after}
#          or {Tcl {C API} Tcl_Alloc}. Every element but the last is a path node of the outline hierarchy
#          (it can be opened and closed). The last element is the label of the entry linking
#          to a specific manual page. The outline is built in the order of the appearance
#          in the file. The first element is the toplevel node in the outline (Tcl, Tk, Keywords).
# - file = the file name of the markdown file without the .md extension (within the "markdown" folder defined below)
#
# For later (after TIP 700):
# Another element acting as an anchor within the page, so that several entries can lead to different places of the same page):
# - append the name of the element to pageFields (and give it a default in pageDefaults)
# - use it where the entries are turned into links (see pageHref)
#
set pagesFile [file join [file dirname [info script]] markdown2html-pages.txt]
## the elements of a line of the pages file, in this order; only the first pageFieldsRequired are mandatory:
set pageFields {path file}
set pageFieldsRequired 2
## values of the elements that are missing in a line:
set pageDefaults {}



proc isCAPI {path} {
	#
	# whether a page belongs to the C API (as opposed to script commands etc.)
	#
	# path - the outline path of the page, e.g. {Tcl {C API} Tcl_Alloc}
	#
	string match {*C API} [lindex $path 1]
}


proc pageFolder {path} {
	#
	# determines into which HTML output folder a specific manual page needs to go:
	# Tcl or Tk for commands etc., TclCAPI or TkCAPI for the C API
	#
	# path - the outline path of the page
	#
	set toplevel [lindex $path 0]
	if {$toplevel ni {Tcl Tk}} {
		return -code error "unknown toplevel '$toplevel' in path $path (the first element must be Tcl, Tk or Keywords)"
	}
	if {[isCAPI $path]} {return ${toplevel}CAPI}
	return $toplevel
}


proc pageHref {page} {
	#
	# link target of an outline entry, relative to index.html
	#
	# page - the dict of the entry (see manFiles)
	#
	set href [dict get $page folder]/[dict get $page file].html
	# further elements of the lines can be used here, e.g. [dict getdef $page anchor {}]
	return $href
}


proc parsePageLine {line} {
	#
	# turns a line of the pages file into a dict with one key per element (see pageFields)
	#
	# line - the line, a Tcl list
	#
	global pageFields pageFieldsRequired pageDefaults
	if {[llength $line] < $pageFieldsRequired || [llength $line] > [llength $pageFields]} {
		set expected [expr {$pageFieldsRequired == [llength $pageFields] ? "exactly $pageFieldsRequired" : "$pageFieldsRequired to [llength $pageFields]"}]
		return -code error "expected $expected elements ($pageFields), got [llength $line]: $line"
	}
	set page $pageDefaults
	for {set i 0} {$i < [llength $line]} {incr i} {
		dict set page [lindex $pageFields $i] [lindex $line $i]
	}
	if {[llength [dict get $page path]] < 2} {
		return -code error "the path must have at least two elements (toplevel and one node): $line"
	}
	# the output folder follows from the path:
	dict set page folder [pageFolder [dict get $page path]]
	return $page
}


proc esc {s} {
	#
	# convert specific characters to HTML equivalent
	#
	# s - the characters to convert
	#
	string map {
		&   &amp;
		<   &lt;
		>   &gt;
	} $s
}


proc attr {s} {
	#
	# escape a string for use inside of a double-quoted HTML attribute
	#
	# s - the string to escape
	#
	string map {& &amp; < &lt; > &gt; \" &quot;} $s
}


proc resolveLink {link} {
	#
	# find the page for an entry of "Links" in the YAML front matter (e.g. "concat(n)" or "Tcl_Alloc"),
	# returns {folder file}, or an empty string if nothing is found
	#
	# link - link entry
	#
	global linkTarget
	if {[regexp {^(.+)\((\w+)\)$} $link -> name section]} {
		if {$section eq "3"} {set kinds "api"} else {set kinds "cmd"}
	} else {
		set name $link
		set kinds {cmd api}
	}
	foreach kind $kinds {
		if {[dict exists $linkTarget $kind $name]} {
			return [dict get $linkTarget $kind $name]
		}
	}
	return {}
}


proc readMeta {mdFile} {
	#
	# returns the front matter (yaml) of a markdown file as a dict
	# using the template "metaTemplate"
	#
	# mdFile - a markdown-formatted manual page
	#
	global metaTemplate
	json::json2dict [exec pandoc -f markdown-tex_math_dollars-smart -t markdown \
		--template=$metaTemplate $mdFile]
}


proc seeAlsoHtml {folder myFile links} {
	#
	# HTML for the "See also" block at the bottom of a page
	#
	# folder - folder of this page
	# myFile - file name of this page
	# links  - "Links" from the YAML front matter
	#
	set items {}
	set seen {}
	foreach link $links {
		if {$link in $seen} {continue}
		lappend seen $link
		set target [resolveLink $link]
		if {$target eq ""} {
			lappend items "<li>[esc $link]</li>"
			continue
		}
		lassign $target targetFolder targetFile
		if {$targetFolder eq $folder && $targetFile eq $myFile} {continue}
		set href [expr {$targetFolder eq $folder ? "$targetFile.html" : "../$targetFolder/$targetFile.html"}]
		lappend items "<li><a href=\"[attr $href]\">[esc $link]</a></li>"
	}
	if {![llength $items]} {return {}}
	return "<footer class=\"see-also\">\n<h2>See also</h2>\n<ul>\n[join $items \n]\n</ul>\n</footer>\n"
}


proc leafHtml {label href keywords} {
	#
	# HTML of an outline entry linking to a page (the last level of the outline, without open/close triangle)
	#
	# label    - the text of the entry
	# href     - the link target
	# keywords - the keywords of the page, used by the filter field
	#
	set keywordAttr [attr [string tolower [join $keywords |]]]
	return "<li class=\"leaf\" data-keywords=\"$keywordAttr\"><a href=\"[attr $href]\" target=\"content\">[esc $label]</a></li>\n"
}


proc outlineHtml {parent} {
	#
	# HTML of the outline below a path, one nesting level of lists per level of the path:
	# sections (with children) can be opened and closed, the last element of a path is a link
	#
	# parent - the path of the section to render, {} for the whole outline
	#
	global children manFiles fileKeywords
	set html {}
	foreach name [dict get $children $parent] {
		set path [list {*}$parent $name]
		if {[dict exists $children $path]} {
			append html {<li><details><summary>} [esc $name] {</summary>} \n {<ul>} \n
			append html [outlineHtml $path]
			append html {</ul></details></li>} \n
		} else {
			set page [dict get $manFiles $path]
			set key [dict get $page folder]/[dict get $page file]
			append html [leafHtml $name [pageHref $page] [dict getdef $fileKeywords $key {}]]
		}
	}
	return $html
}



######## MAIN PROGRAM STARTS HERE ############


# manFiles: path -> dict (path file folder, plus the further elements of the line);
# "children": the outline, parent path -> the names of the entries below it (the root has the path {}):
set manFiles [dict create]
set children [dict create]
set fh [open $pagesFile]
while {[gets $fh line] >= 0} {
	if {[string trim $line] eq ""} continue
	if {[catch {parsePageLine $line} page]} {
		close $fh
		return -code error "$pagesFile: $page"
	}
	set path [dict get $page path]
	if {[dict exists $manFiles $path]} {
		close $fh
		return -code error "$pagesFile: duplicate path: $path"
	}
	dict set manFiles $path $page
	# register the path in the outline, creating missing sections on the way:
	for {set i 0} {$i < [llength $path]} {incr i} {
		set parent [lrange $path 0 $i-1]
		set name [lindex $path $i]
		if {$name ni [dict getdef $children $parent {}]} {dict lappend children $parent $name}
	}
}
close $fh
# a path cannot be a page and a section at the same time:
foreach path [dict keys $manFiles] {
	if {[dict exists $children $path]} {
		return -code error "$pagesFile: path is both a page and a section of the outline: $path"
	}
}


# compare the pages file with the markdown files that exist:
# - a listed file that does not exist is an error (collected, so that all of them are reported at once)
# - a markdown file that is not listed does not appear in the HTML at all, which is only warned about
#   (a page may be left out on purpose)
set markdownDir [file join [file dirname [info script]] .. doc markdown]
set listed [dict create]
foreach path [dict keys $manFiles] {
	dict set listed [dict get $manFiles $path folder]/[dict get $manFiles $path file] $path
}
set missing {}
foreach key [dict keys $listed] {
	if {![file exists [file join $markdownDir $key.md]]} {
		lappend missing "$key.md (outline path: [dict get $listed $key])"
	}
}
if {[llength $missing]} {
	return -code error "$pagesFile: [llength $missing] listed markdown file(s) not found in $markdownDir:\n  [join $missing "\n  "]"
}
foreach folder {Tcl TclCAPI Tk TkCAPI} {
	foreach mdFile [lsort [glob -nocomplain -directory [file join $markdownDir $folder] *.md]] {
		set key $folder/[file rootname [file tail $mdFile]]
		if {![dict exists $listed $key]} {
			puts stderr "warning: $key.md is not listed in the pages file and will not appear in the manual"
		}
	}
}


# convert the markdown-formatted manual pages
# (as produced by man2markdown.tcl) into HTML-formatted manual pages,
# using Pandoc.
# These parts are shwon in the right panel of the webpage when clicking on the respective entry in the outline of the left panel:
# - the Tcl pages go into the Tcl folder, the Tk pages into the Tk folder (as determined by the path in the pages file)
# - the Tcl C API goes into the TclCAPI folder, the Tk C API into the TkCAPI folder (see pageFolder)
file mkdir \
	../doc/html/Tcl \
	../doc/html/Tk \
	../doc/html/TclCAPI \
	../doc/html/TkCAPI


# template file for pandoc that outputs just the front matter as JSON
# (equivalent to --template=<(echo '$meta-json$') ).
# This is used to extract the YAML frontmatter from the markdown files
# and retrieve "Keywords" and "Links":
set metaTemplate {}
set fh [file tempfile metaTemplate]
puts -nonewline $fh {$meta-json$}
close $fh


set converted [dict create]


#
# lookup of the pages that the "Links" of the front matter can point to:
# linkTarget <kind> <name> -> {folder file}
# with kind = cmd (commands, applications, concepts) or api (C API);
# the page titles are tried first, the file names are the fallback
#
set linkTarget [dict create]
foreach by {title file} {
	foreach entry [dict keys $manFiles] {
		set kind [expr {[isCAPI $entry] ? "api" : "cmd"}]
		# the page title is the label of the entry, i.e. the last element of the path:
		set name [expr {$by eq "title" ? [lindex $entry end] : [dict get $manFiles $entry file]}]
		if {![dict exists $linkTarget $kind $name]} {
			dict set linkTarget $kind $name [list [dict get $manFiles $entry folder] [dict get $manFiles $entry file]]
		}
	}
}


#
# convert each individual markdown document
#
foreach entry [dict keys $manFiles] {
	set myFile [dict get $manFiles $entry file]
	set myFolder [dict get $manFiles $entry folder]
	# pages of different folders (e.g. Tcl and Tk) may have the same file name,
	# so everything that is looked up per page is keyed by folder and file:
	set pageKey $myFolder/$myFile
	if {[dict exists $converted $pageKey]} continue
	dict set converted $pageKey 1
	puts "$pageKey.md -> $pageKey.html"
	set meta [readMeta [file join .. doc markdown $myFolder $myFile.md]]
	# the keywords go into the search index (see below):
	dict set fileKeywords $pageKey [dict getdef $meta Keywords {}]
	dict set fileTitle $pageKey [dict getdef $meta title $myFile]
	dict set fileHref $pageKey $myFolder/$myFile.html
	# the links go to the bottom of the page:
	set seeAlso {}
	set seeAlsoHtml [seeAlsoHtml $myFolder $myFile [dict getdef $meta Links {}]]
	if {$seeAlsoHtml ne ""} {
		set fh [file tempfile seeAlsoFile]
		puts -nonewline $fh $seeAlsoHtml
		close $fh
		set seeAlso [list --include-after-body $seeAlsoFile]
	}
	exec pandoc -f markdown-tex_math_dollars-smart-raw_html -t html \
		--lua-filter markdown2html.lua \
		{*}$seeAlso \
		--toc --toc-depth=2 \
		-s -c [file join .. tcl-docs.css] \
		-o [file join .. doc html $myFolder $myFile.html] \
		[file join .. doc markdown $myFolder $myFile.md]
	if {$seeAlso ne ""} {file delete $seeAlsoFile}
}
file delete $metaTemplate


#
# now create the index.html file with all links to the individual pages
#
puts "\ncreate index.html\n"
# title shown in the fixed header bar and as page title:
set manualTitle {Tcl/Tk manual, version 9.1}

# the logo shown in the header bar must sit next to index.html:
file copy -force Tcl9Logo.svg [file join .. doc html Tcl9Logo.svg]

set nav {<ul class="outline">}
append nav \n

append nav [outlineHtml {}]

# the keywords of the manual pages as a group of their own after the last group:
# one entry per keyword, containing the pages that have this keyword
foreach myFile [dict keys $fileKeywords] {
	foreach keyword [dict get $fileKeywords $myFile] {
		dict lappend keywordPages $keyword [list [dict get $fileTitle $myFile] [dict get $fileHref $myFile]]
	}
}
if {[info exists keywordPages]} {
	append nav {<li><details><summary>Keywords</summary>} \n {<ul>} \n
	foreach keyword [lsort -dictionary [dict keys $keywordPages]] {
		append nav "<li><details><summary>[esc $keyword]</summary>\n<ul>\n"
		foreach item [lsort -dictionary -index 0 -unique [dict get $keywordPages $keyword]] {
			lassign $item title href
			append nav [leafHtml $title $href [list $keyword]]
		}
		append nav {</ul></details></li>} \n
	}
	append nav {</ul></details></li>} \n
}
append nav {</ul>}

set fh [open [file join .. doc html index.html] w]

puts $fh [subst -nocommands {<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<title>$manualTitle</title>
<link rel="stylesheet" href="tcl-docs.css">
<link rel="stylesheet" href="index.css">
</head><body class="manual-shell">
<header class="topbar manual-header"><img src="Tcl9Logo.svg" alt="Tcl 9 logo"><span>$manualTitle</span></header>
<div class="layout">
<nav>
<input id="filter" type="search" placeholder="Filter..." autocomplete="off">
$nav
</nav>
<iframe name="content" src="Tcl/Tcl.html" title="Manual page"></iframe>
</div>
<script>
const frame = document.querySelector('iframe[name=content]');
const links = [...document.querySelectorAll('nav a')];
function mark(a) {
  links.forEach(l => l.classList.remove('current'));
  if (!a) return;
  a.classList.add('current');
  for (let d = a.closest('details'); d; d = d.parentElement.closest('details')) d.open = true;
  a.scrollIntoView({block: 'nearest'});
}
links.forEach(a => a.addEventListener('click', () => mark(a)));
// follow links clicked inside the content frame (works where the browser allows access to the frame's location):
frame.addEventListener('load', () => {
  try {
    const p = frame.contentWindow.location.pathname;
    const cur = document.querySelector('nav a.current');
    if (cur && cur.pathname === p) return;
    const m = links.find(l => l.pathname === p);
    if (m) mark(m);
  } catch (e) {}
});
mark(links.find(a => a.getAttribute('href') === frame.getAttribute('src')));
document.getElementById('filter').addEventListener('input', e => {
  const q = e.target.value.toLowerCase();
  document.querySelectorAll('nav li > a').forEach(a => {
    const li = a.parentElement;
    li.hidden = q && !(li.textContent.toLowerCase().includes(q) || (li.dataset.keywords || '').includes(q));
  });
  // hide groups (and keywords) without any visible entry, innermost first:
  [...document.querySelectorAll('nav details')].reverse().forEach(d => {
    d.parentElement.hidden = q && !d.querySelector('li:not([hidden]) > a');
  });
  const cur = document.querySelector('nav a.current');
  document.querySelectorAll('nav details').forEach(d => d.open = !!q || d.contains(cur));
});
</script>
</body></html>}]

close $fh
