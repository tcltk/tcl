# markdown2html.tcl --
#
# this script takes the manual pages in markdown format,
# converts them into html and builds a website structure to viwe the content
# in a web browser

# currently, this script must be executed from the tools directory!

package require Tcl 9
package require json   ;# from tcllib


# all pages with metadata determining how they will occur in the HTML outline
# are listed in the separate file markdown2html-pages.txt (next to this script).
# The file describes one manual page per line, each line being a Tcl list with four elements:
# - file   = the file name of the markdown file without the .md extension
# - title  = the page title for the outline item linking to the page
# - group1 = the initial level of the hierarchical outline: the book, either Tcl or Tk
#            (this also selects the output folder, see pageFolder)
# - group2 = the second level of the hierarchical ouline: the section within the book,
#            e.g. Commands or C API (a label ending in "C API" selects the C API folder).
#            Only the combination of group1 and group2 identifies a section, so different
#            books can use the same group2 labels (e.g. Tcl|Commands and Tk|Commands).
# (a page may appear several times, e.g. one entry per C API function documented on it)
#
set pagesFile [file join [file dirname [info script]] markdown2html-pages.txt]
set fh [open $pagesFile]
while {[gets $fh line] >= 0} {
	if {[string trim $line] eq ""} continue
	if {[llength $line] != 4} {
		close $fh
		return -code error "$pagesFile: expected 4 elements (file title group1 group2), got [llength $line]: $line"
	}
	lassign $line file title group1 group2
	# code here to build the left panel of the webpage for navigation:
	dict set manFiles $group1/$group2/$title [list file $file title $title group1 $group1 group2 $group2]
}
close $fh


## ordering of the books (group1) in the outline:
set groupOrder {
	Tcl
	Tk
}

## ordering of the sections (group2) within each book:
set pageOrder {
	{Syntax}
	{Applications}
	{Commands}
	{TclOO commands}
	{Variables}
	{Math functions}
	{Math operators}
	{Filename conventions}
	{Regular expression syntax}
	{Library procedures}
	{C API}
	{TclOO C API}
}


# convert the markdown-formatted manual pages
# (as produced by man2markdown.tcl) into HTML-formatted manual pages,
# using Pandoc.
# These parts are shwon in the right panel of the webpage when clicking on the respective entry in the outline of the left panel:
# - the Tcl pages go into the Tcl folder, the Tk pages into the Tk folder (as determined by "group1" in the pages table)
# - the Tcl C API goes into the TclCAPI folder, the Tk C API into the TkCAPI folder (as determined by "group1" and "group2" in the pages table)
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


proc isCAPI {group2} {
	#
	# tells whether a section holds C API pages (as opposed to commands etc.)
	#
	# group2 - the section label from the pages table, e.g. "C API" or "TclOO C API"
	#
	string match {*C API} $group2
}


proc pageFolder {group1 group2} {
	#
	# determines into which folder a specific manual page needs to go:
	# Tcl or Tk for commands etc., TclCAPI or TkCAPI for the C API
	#
	# group1 - the book of the page (Tcl or Tk)
	# group2 - the section within the book (see the manFiles dict)
	#
	if {$group1 ni {Tcl Tk}} {
		return -code error "unknown book '$group1' (group1 must be Tcl or Tk)"
	}
	if {[isCAPI $group2]} {return ${group1}CAPI}
	return $group1
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
	# using the template "metaTemplate" written above
	#
	# mdFile - a markdown-formatted manual page
	#
	global metaTemplate
	json::json2dict [exec pandoc -f markdown-tex_math_dollars-smart -t markdown \
		--template=$metaTemplate $mdFile]
}


proc seeAlsoHtml {folder myFile links} {
	#
	# html for the "See also" block at the bottom of a page
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


#
# lookup of the pages that the "Links" of the front matter can point to:
# linkTarget <kind> <name> -> {folder file}
# with kind = cmd (commands, applications, concepts) or api (C API);
# the page titles are tried first, the file names are the fallback
#
set linkTarget [dict create]
foreach by {title file} {
	foreach entry [dict keys $manFiles] {
		set group1 [dict get $manFiles $entry group1]
		set group2 [dict get $manFiles $entry group2]
		set kind [expr {[isCAPI $group2] ? "api" : "cmd"}]
		set name [dict get $manFiles $entry $by]
		if {![dict exists $linkTarget $kind $name]} {
			dict set linkTarget $kind $name [list [pageFolder $group1 $group2] [dict get $manFiles $entry file]]
		}
	}
}


#
# convert each individual markdown document
#
foreach entry [dict keys $manFiles] {
	set myFile [dict get $manFiles $entry file]
	set myFolder [pageFolder [dict get $manFiles $entry group1] [dict get $manFiles $entry group2]]
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
foreach entry [dict keys $manFiles] {
	dict with manFiles $entry {
		set folder [pageFolder $group1 $group2]
		# the section is identified by book and section label together:
		dict lappend tree [list $group1 $group2] [list $title $folder/$file.html [dict getdef $fileKeywords $folder/$file {}]]
	}
}


# title shown in the fixed header bar and as page title:
set manualTitle {Tcl/Tk manual, version 9.1}

# the logo shown in the header bar must sit next to index.html:
file copy -force Tcl9Logo.svg [file join .. doc html Tcl9Logo.svg]

set nav {<ul class="outline">}
append nav \n

foreach g1 $groupOrder {
	# first level: the book (Tcl, Tk); skipped if it has no pages (yet)
	set sections [list]
	foreach g2 $pageOrder {
		if {[dict exists $tree [list $g1 $g2]]} {lappend sections $g2}
	}
	if {![llength $sections]} continue
	append nav {<li><details><summary>} [esc $g1] {</summary>} \n {<ul>} \n
	foreach g2 $sections {
		# second level: the section of the book
		append nav {<li><details><summary>} [esc $g2] {</summary>} \n {<ul>} \n
		foreach item [dict get $tree [list $g1 $g2]] {
			lassign $item title href keywords
			set keywordAttr [attr [string tolower [join $keywords |]]]
			append nav "<li data-keywords=\"$keywordAttr\"><a href=\"$href\" target=\"content\">[esc $title]</a></li>\n"
		}
		append nav {</ul></details></li>} \n
	}
	append nav {</ul></details></li>} \n
}

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
		set keywordAttr [attr [string tolower $keyword]]
		foreach item [lsort -dictionary -index 0 -unique [dict get $keywordPages $keyword]] {
			lassign $item title href
			append nav "<li data-keywords=\"$keywordAttr\"><a href=\"$href\" target=\"content\">[esc $title]</a></li>\n"
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
