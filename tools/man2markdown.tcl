# This script parses an nroff formatted Tcl/Tk manual page,
# converts it into some AST (Abstract Syntax Tree)
# and then writes it out again as a markdown file.
#
#
# When this script is called from another Tcl script (i.e. using the [source] command),
# you can call the procedure `::ndoc::man2markdown` with a the content of a man file
# to be converted as an argument
# (the whole script and its procedures live in the '::ndoc::' namespace).
# If you just have the man file, you can use: `::ndoc::man2markdown [::ndoc::readFile /path/to/my/file.n]`
#
#
# This procedure will return the resulting markdown.
#
# When this script is called directly from the command line using tclsh,
# specifying an nroff file as an argument will call the [::ndoc::man2markdown] procedure
# on the file content and return the markdown on stdout.
#
# When not specifying an argument,
# it runs in test mode, converting a specific manual page from within the
# repository structure which this script is a part of. The result is written to stdout.
#
# When run with two directory names, it will convert all nroff files in the (existing) first directory
# and put the result as markdown files into the second directory (which will be created if it does not exist).
# This should be used to produce the current status of afairs into doc/markdown/:
#
# ```
# # (from the tools directory)
# tclsh man2markdown.tcl ../doc ../markdown
# ```
#
# The last invocation can be used to check what changed with respect to the last `fossil commit`.
# Any change reported by fossil (before committing) is a change that might have issues
# compared to teh previous version, so it should be checked before the commit
# and amended before the commit!
#
# This script is part of the implementation of TIP 700 <https://core.tcl-lang.org/tips/doc/trunk/tip/700.md>
#

namespace eval ::ndoc {
	
	# internal documentation of the used AST
	set markdownText {
		
		A typical Tcl/Tk nroff manual document consists of block elements and inline elements
		and conforms to the following structure (only loosely used BNF notation here ...):
		
		``` {.BNF}
		Block  = Paragraph | Code | Header | List | Dlist | Div | Syntax
		Inline = Space | Text | Emphasis | Strong | Quoted | Link | Span
		
		Document  = Block +
		Paragraph = Inline +
		Header    = Text
		List      = ListItem +
		ListItem  = Block +  [but without 'Header']
		Dlist     = DlistItem +
		DlistItem = Block +  [but without 'Header']
		Emphasis  = Inline +
		Strong    = Inline +
		Quoted    = Inline +
		Link      = Inline +
		Syntax    = Span + | Space +
		Div       = Div + | Span + | Inline +
		Span      = Span | Text | Space +  [currently a Span can not hold any other inline elements except itself (but only once) or Text/Space]
		
		Text = <a string of visible characters including spaces>
		Code = <a string of visible characters including spaces and newlines>
		Space = <a space character>
		```
		
		We don't mind (for now) to be lazy, so the spaces are not separate all the time. They can be
		present stand-alone or as part of a 'Text' element. For each newline between two input lines
		we add a space.
		
		The AST corresponding to a document is internally represented as a nested Tcl list.
		The list has three elements where the last element may contain further lists of the same structure.
		The first element of that list is always the "Document" item. Within that item the other
		structural items are nested according to the above BNF structure.
		
		Each item of the AST consists of three elements:
		
		    <keyword> <options> <content>
	   
		1. the <keyword> (e.g. "Document" or "Text")
		2. a set of <options> with values (e.g. -level foo -class bar)
		3. the <content> of the item which is itself either another structural item or a terminal element
		
		All structural items with a '+' above have one or more nested AST items as their <content> elements.
		This means, the content is itself a list with AST elements, each of these elements being a 3-element list as described above.
		The others (the terminals: Text, Code, Space) only have a simple string of characters in the <content> element.
		They do not have nested elements. Note, that the content element of Space is an empty string.
		For most items, the options are currently empty (i.e. the empty list). These are the
		possible structural items of the AST in the Tcl representation:
		
		| item keyword | options       | description                                         |
		|--------------|---------------|-----------------------------------------------------|
		| Document     |               |
		| Paragraph    | -vs           | the value is the value of the .VS macro showing information on when this content was added
		| Header       | -level        | a section heading of a specific level (-level 1|2|3)
		|              | -class        | defines that the header belongs to a specific class (can be used for CSS)
		| List         | -type         | a bullet (-type bullet) or numbered (-type numbered) list
		| ListItem     |               |
		| Dlist        |               | a definition list
		| DlistItem    | -definition   | the definition text making up the 'label' of the item (this is an Inline AST element!)
		|              | -vs           | value of a .VS macro for that DlistItem > not possible to use in markdown
		| Emphasis     |               |
		| Strong       |               | text with strong emphasis, typically rendered as bold |
		| Quoted       | -type         | text enclosed in quotes of a specific type (-type single|double)
		| Link         | -target       | an internal link, within the document (-target #<anchor>) or to another manual page (-target <page>)
		| Span         | <class>  -vs  | a span element with a custom class attribute such as '.cmd', '.sub' etc. used for Tcl command syntax elements
		| Div          | <class>       | a div element with a custom class attribute (currently only '.synopsis') used for grouping lines of Tcl command syntax
	
	   
		Here is example 1 of a complete AST:
		
		``` {.nroff}
		this is \fIitalic "and quoted" text\fR, right?
		```
	
		Example 1 AST as nested Tcl list:
		
		``` {.tcl}
		Document {}	{
			{Paragraph {} {
		      {Text {} "this is "}
		      {Emphasis {} {
					{Text {} "italic "}
		         {Quoted {} {
		            {Text {} "and quoted"}
		         }}
		         {Text {} "text"}
		      }}
		      {Text {} ", right?"}
			}}
		}
		```
		
		Here's another example 2 where the nesting is slightly different:
		
		``` {.nroff}
		this is "*italic and quoted*"
		```
	
		Example 2 AST as nested Tcl list:
		
		``` {.tcl}
		Document {} {
		   {Paragraph {} {
		      {Text {} "this is "}
		      {Quoted {} {
		         {Emphasis {} {
		            {Text {} "italic and quoted"}
		         }}
		      }}
			}}
		}
		```
		
		Example 3 contains some more block elements:
		
		```{.nroff}
		.SH My header
		.PP
		Some text here with code:
		.CS
		set myVar theValue
		.CE
		.TP 13
		A definition label
		.
		Some definition text
		.RS
		.PP
		.CS
		some code
		.CE
		.RE
		somemore text
		.TP 13
		Another definition
		.
		Text for second item
		.SH Next header
		```
		
		This is the corresponding Tcl list representation:
		
		```{.tcl}
		Document {} {
			{Header {-level 1} {
				{Text {} "My header"}
			}}
			{Paragraph {} {
				{Text {} {Some text here with code:}}
			}}
			{Code {} {set myVar theValue}}
			{Dlist {} {
				{DlistItem {-definition "A definition label"} {
					{Paragraph {} {
						{Text {} {Some definition text}}
					}}
					{Code {} {puts "some code"}}
					{Paragraph {} {
						{Text {} {some more text}}
					}}
				}}
				{DlistItem {-definition "Another definition"} {
					{Text {} "Text for second item"}
				}}
			}}
			{Header {-level 1} {Next header}}
			...
		}
		```
	}
	
	# give diagnostic output:
	set verbose 0
	# the dictionary into which the manual page is put
	set manual [dict create]
	# whether to try and convert command names in the text to real links:
	set convertCmdToLink 1
}


proc ::ndoc::BIRPstrip {text} {
	#
	# removes any \fB, \fI, \fP and \fR nroff markup from a text
	#
	return [string map {{\fB} {} {\fI} {} {\fR} {} {\fP} {}} $text]
}


proc ::ndoc::BIRPclean {text} {
	#
	# cleanup \fB, \fI, \fP and \fR nroff markup to be more regular
	#
	# text - raw nroff text to clean up the BIR markup in
	#
	# Returns the cleaned nroff
	#
	# We want to clean a bit up here so that spaces do not become part of
	# Emphasis or Strong (typically found in the 'Synopsis' section):
	#
	# "\fBappend \fIvarName \fR?\fIvalue value value ...\fR?" 
	# and "\fBdict merge \fR?\fIdictionaryValue ...\fR?"
	#
	# should be
	#
	# "\fBappend\fR \fIvarName\fR ?\fIvalue value value ...\fR?"
	# and "\fBdict merge\fR ?\fIdictionaryValue ...\fR?"
	# 
	#
	# This hould NOT apply to: "\fBTk_CreateClientMessageHandler\fR arranges for \fIproc\fR",
	# i.e. when \fR is in between ...
	# so for simplicity we do not allow a backslash in between
	#
	# Note: the procedure does not yet catch all instances that could need cleanup
	#
	# Note that \fB followed by \fI will not give us bold+italic
	# but rather switch from bold only to italic only
	#
	# Note: we here treat \fP as if it is \fR since the Tk sources do not
	# seem to use \fP in other cases. Thus, any \fP is changed to \fR
	#
	set state N
	set out {}
	# some initial cleaning, changing bold and italic whitespace to normal whitespace:
	set text [string map {{\fB \fI} {\fR \fI}} $text]
	set text [string map {{\fI \fB} {\fR \fB}} $text]
	for {set i 0} {$i < [string length $text]} {incr i} {
		set char [string index $text $i]
		set lookahead [string range $text [expr {$i+1}] [expr {$i+2}]]
		set lookfurther [string index $text [expr {$i+3}]]
		set lookback [string index $text [expr {$i-1}]]
		set lookback2 [string range $text [expr {$i-2}] [expr {$i-1}]]
		if {$char ne "\\"} {
			append out $char
		} else {
			# command found with backslash as first character:
			if {$lookahead ni {fB fI fR fP}} {
				# no BIRP, keep backslash:
				append out $char
			} else {
				# BIRP found (current char is the backslash in front):
				switch $state {
					N - R - P {
						switch $lookahead {
							fB {append out \\fB; set state B}
							fI {append out \\fI; set state I}
							fR {return -code error "fR without previous fI/fB in: $text"}
							fP {return -code error "fP without previous fI/fB in: $text"}
						}
					}
					B {
						switch $lookahead {
							fB {return -code error "dirty BIRP: fB following fB in: $text"}
							fR - fP {
								if {$lookback eq { }} {
									set out [string range $out 0 end-1]
									append out \\fR { }
								} else {
									append out \\fR
								}
								set state N
							}
							fI {
								if {$lookback eq { }} {
									set out [string range $out 0 end-1]
									append out \\fR { }
								} elseif {$lookback2 eq " ?"} {
									set out [string range $out 0 end-2]
									append out \\fR { ?}
								} else {
									append out \\fR
								}
								if {$lookfurther eq { }} {append out { }; incr i}
								append out \\fI
								set state I
							}
						}
					}
					I {
						switch $lookahead {
							fB {
								if {$lookback eq { }} {
									set out [string range $out 0 end-1]
									append out \\fR { }
								} else {
									append out \\fR
								}
								if {$lookfurther eq { }} {append out { }; incr i}
								append out \\fB
								set state B
							}
							fR - fP {
								if {$lookback eq { }} {
									set out [string range $out 0 end-1]
									append out \\fR { }
								} else {
									append out \\fR
								}
								set state N
							}
							fI {return -code error "dirty BIR: fI following fI in: $text"}
						}
					}
				}
				incr i 2
			}
		}
	}
	return $out
}


proc ::ndoc::BIR2markdown {text} {
	#
	# takes cleaned BIR nroff and translates it into markdown
	#
	# - this proc is currently not used anywhere -
	#
	set state N
	set md {}
	for {set i 0} {$i < [string length $text]} {incr i} {
		set char [string index $text $i]
		set lookahead [string range $text [expr {$i+1}] [expr {$i+2}]]
		# we need to always escape asterisks as they have special meaning in markdown:
		if {$char eq "*"} {append md \\}
		if {$char ne "\\"} {
			append md $char
		} elseif {$char eq "\\" && [string index $lookahead 0] eq "e"} {
			append md "\\\\"
			incr i
		} else {
			# ignore the backslash:
			incr i
			if {$lookahead ni {fB fI fR}} {
				append md [string index $text $i]
			} else {
				switch $state {
					N - R {
						switch $lookahead {
							fB {append md **; set state B}
							fI {append md *; set state I}
							fR {return -code error "fR without previous fI/fB in: $text"}
						}
					}
					B {
						switch $lookahead {
							fB {append md {** **}}
							fR {append md **; set state N}
							fI {append md {** *}; set state I}
						}
					}
					I {
						switch $lookahead {
							fB {append md {* **}; set state B}
							fR {append md *; set state N}
							fI {append md {* *}}
						}
					}
				}
				incr i
			}
		}
	}
	return $md
}


proc ::ndoc::parseArgs {line} {
	#
	# Takes a line with an nroff macro call and returns a list of the arguments
	#
	# line - a line of nroff with a macro call and its arguments
	#
	set res [list]
	set stopHere 0
	set startIndex 4
	while {! $stopHere} {
		if {[string index $line $startIndex] eq "\""} {
			# text in quotes
			incr startIndex
			set endIndex [string first \" $line $startIndex]
			if {$endIndex == -1} {return -code error "no matching quote found"}
			if {$endIndex == [string length $line] - 1} {
				set stopHere 1
				set endIndex end-1
			} else {
				incr endIndex -1
				set nextIndex [expr {$endIndex + 3}]
			}
		} else {
			# no quotes
			set endIndex [string first { } $line $startIndex]
			if {$endIndex == -1} {
				set endIndex end
				set stopHere 1
			} else {
				incr endIndex -1
				set nextIndex [expr {$endIndex + 2}]
			}
		}
		lappend res [string range $line $startIndex $endIndex]
		if {! $stopHere} {set startIndex $nextIndex}
	}
	return $res
}


proc ::ndoc::parseQuoting {line} {
	#
	# parse a .QW, .PQ or .QR nroff macro
	# that is formatted as a space-separated two or more element list
	# with possible embedded spaces in quoted text
	#
	# line - a line of nroff with a .QW or .PQ or .QR tag
	#
	# Returns raw text with normal text quotation marks
	# surrounded by an extra § character.
	#
	# .PQ adds parenthesis arount the quotes.
	# .QR adds a dash between the first two elements
	#
	# The § are embedded so the nroff inline parser can transform this to a "Quoted" AST element
	# instead of treating the quote as a literal character
	#
	# Example: '.QW "the end" .' gives us '§"the end"§.'
	#
	set stop 0
	set type [string range $line 1 2]
	if {$type eq "PQ"} {set quotedText "("}
	# split text into the two or three elements:
	lassign [parseArgs $line] argText(1) argText(2) argText(3)
	append quotedText §\" $argText(1)
	# make the broken ctags happy ...: "
	if {$type eq "QR"} {
		append quotedText - $argText(2) \"§
	} else {
		append quotedText \"§ $argText(2)
	}
	if {$type eq "PQ"} {append quotedText ")"}
	append quotedText $argText(3)
	return $quotedText
}


proc ::ndoc::parseBackslash {text} {
	#
	# parses an nroff text string and handles escaping of characters
	# (both removing or adding backslashes)
	#
	# Note: this procedure does not handle the \f... sequences
	# (\fB, \fI and \fR are handled by BIRPclean, BIRPstrip and parseInline)
	#
	# Returns the transformed text
	#
	set text [string map {
		\u005ce \u005c
		\u005c- -
	} $text]
	return $text
}


proc ::ndoc::man2AST {manContent} {
	#
	# parses the data in manContent
	# interpreting it as nroff format inlcuding Tcl macros,
	# and converting it into an abstract syntax tree (AST)
	#
	# manContent - the content of a file containing an nroff formatted manual page
	#
	# The procedure fills the global dict 'manual' with four elements:
	#  - copyright = a list of copyright entries
	#  - meta = a list of some metadata elements and their values (see below in the .TH branch)
	#  - content = the AST
	#  - name = the name of the manual page
	#
	variable manual
	variable verbose
	dict set manual copyright [list]
	dict set manual meta [list]
	dict set manual content [list]
	
	set manContent [split $manContent \n]
	
	if {$verbose} {puts "++++ processing blocks ++++"}
	set blockAST [parseBlock Document $manContent]
#foreach element $blockAST {puts $element\n}; puts +++++++++++++++++++++++; exit

	if {$verbose} {puts "++++ expanding blocks ++++"}
	# this also parses the Inline elements:
	set expandedBlocks [blocksExpand $blockAST]
	dict set manual content [list Document {} $expandedBlocks]

	if {$verbose} {puts "++++++++ end ++++++++++++++"}
#foreach element $expandedBlocks {puts $element\n}; exit
}


proc ::ndoc::blocksExpand {blockList} {
	#
	# takes a hierarchical list of blocks and processes them in breadth-first order
	# to parse the inline content and convert it into proper inline AST elements
	#
	# blockList - a list of AST block elements with unparsed inline content
	#
	# Returns a modified AST with the parsed inline elements
	#
	variable manual
	set newList [list]
	foreach block $blockList {
		lassign $block blockType blockAttributes blockContent
		switch $blockType {
			List - Dlist {
				lappend newList [list $blockType $blockAttributes [blocksExpand $blockContent]]
			}
			ListItem - DlistItem {
				if {$blockType eq "DlistItem"} {
					dict set blockAttributes -definition [list [parseInline Inline {} [dict get $blockAttributes -definition]]]
				}
				lappend newList [list $blockType $blockAttributes [blocksExpand $blockContent]]
			}
			Span - Div {
				# Spans and Divs already comes fully expanded, so just add them as they are:
				lappend newList $block
			}
			default {
				lappend newList [parseInline $blockType $blockAttributes $blockContent]
			}
		}
	}
	return $newList
}


proc ::ndoc::parseBlock {parent manContent} {
	#
	# parses an nroff document and splits it into its block elements
	#
	# parent     - the surrounding AST block element, i.e. the block in which we are now operating
	# manContent - a list of nroff lines to parse
	#
	# This procedure just splits the whole document into blocks, it does
	# not look into the block content and processes the Inlines.
	#
	# Returns a replacement AST (a list of block elements) for the blocks specified in manContent
	#
	# Note: this procedure also extracts the 'KEYWORDS' and 'SEE ALSO' sections
	# and moves their content into the metadata. This enables
	# putting that information anywhere during output
	#
	#
	variable manual
	variable verbose
	# the accumulative list of blocks so far:
	set blockList [list]
	# which blockType this one is (inside of $parent):
	set blockType {}
	# the content of the current block:
	set blockContent {}
	# a set of attributes for the block (options of an AST element)
	set blockAttributes [list]
	# a flag, whether a new block starts, so we can finalise the current one.
	# when this flag is set, the currently read line is kept and a block is finalised
	# before digesting the current line (in a repeated run of the while loop)
	set doCloseBlock 0
	while {[llength $manContent] > 0} {
		if {$doCloseBlock} {
			if {$blockContent ne ""} {
				# sometimes .PP is directly followed by .CS, producing empty paragraphs
				# which we want to ignore
				lappend blockList [list $blockType $blockAttributes $blockContent]
				if {$verbose} {puts "... closed block: [lindex $blockList end]"}
			}
			set doCloseBlock 0
			set blockType {}
			set blockContent {}
			set blockAttributes [list]
		}
		set line [lindex $manContent 0]
		if {$verbose} {puts "\nLINE = ->$line<-"}
		set markup [string range $line 0 2]
		if {$verbose} {puts "MARKUP = ->$markup<-"}
		switch -- $markup {
			{.\"} - {'\"} {
				# a comment, we only collect copyright info here for now
				# and put it into the metadata
				if $verbose {puts COMMENT}
				if {[lindex $line 1] eq "Copyright"} {
					if $verbose {puts COPYRIGHT}
					dict lappend manual copyright [lrange $line 1 end]
				}
				set manContent [lrange $manContent 1 end]
			}
			.so - .BS - .BE - .AS - .ta - .RS - .RE - .nf - .fi {
				# can be ignored
				# .so = include files
				# .BS .BE = start and end of box enclosure
				# .AS = maximum sizes of arguments for setting tab stops
				# .ta = set tab stops
				# .RS .RE = relative inset, i.e. indentation
				# .nf .fi = turn off/on filling of lines
				if $verbose {puts "IGNORED ($markup)"}
				set manContent [lrange $manContent 1 end]
			}
			.TH {
				# title heading
				if $verbose {puts TH}
				lassign $line - commandName manualSection version tclPart tclDescription
				if {$version eq ""} {set version unknown}
				dict lappend manual meta \
					CommandName $commandName \
					ManualSection $manualSection \
					Version $version \
					TclPart $tclPart \
					TclDescription $tclDescription
				set manContent [lrange $manContent 1 end]
			}
			.TP - .IP {
#set verbose 1
### ... what about a .TP inside a .RS/.RE pair inside a .TP ... as in file(n)?
				# .TP = tagged paragraph, used as a delimiter between e.g. subcommands
				# .IP = indented paragraph, used for short lists where the label and the description are often in one line
				#
				# We interpret these lists as definition list or simple list depending on the context.
				# .TP is always a definition list
				# .IP is a bullet list when "\(bu 3" is used, a numbered list when "[1]" or "(1)" or "1)" is used
				#     and a definition list when other text is used
				# Once an .IP list start, it cannot change to .TP in the middle
				#
				# Todo: StdChannels.3 uses
				#     ../nroff/tcl9.0/doc/StdChannels.3:.IP 1)
				#  	../nroff/tcl9.0/doc/StdChannels.3:.IP 2)
				#     ../nroff/tcl9.0/doc/StdChannels.3:.IP (a)
				#     ../nroff/tcl9.0/doc/StdChannels.3:.IP (b)
				#     ../nroff/tcl9.0/doc/StdChannels.3:.IP 3)
				#
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				set blockType Dlist
				lassign [parseArgs $line] number
				set subType [string range $markup 1 end]
				set blockAttributes [list]
				if {$subType eq "IP"} {
					# do we have a simple bullet/numbered list here?
					if {[string match {\[[0-9]\]} $number]} {set blockType List; set blockAttributes [list -type numbered]}
					if {[string match {([0-9])} $number]} {set blockType List; set blockAttributes [list -type numbered]}
					if {[string match {[0-9])} $number]} {set blockType List; set blockAttributes [list -type numbered]}
					if {[string match {\\(bu} $number]} {set blockType List; set blockAttributes [list -type bullet]}
				}
				if $verbose {puts "$blockType (subtype $subType)"}
				# collect all material until the end of the definition list
				# - if a .TP item has more than one paragraph or a code block, then it is wrapped into a pair of .RS/.RE
				# - the list ends at the next unwrapped .PP or at .SH/.SS
				set itemTitle {}
				set itemContent {}
				set itemVS [list]
				set dlistContent [list]
				set finishDlist 0
				# the following tells us whether we are in a normal paragraph of an item (dlistState = empty string)
				# or in an .RS/.RE pair of a .TP list (dlistState = inset)
				set dlistState {}				
				while {! $finishDlist} {
					if $verbose {puts "---\nline: $line"}
					switch -- $markup {
						.RS {
							# more than just a simple paragraph (but only for .TP)
							if $verbose {puts "inset starts"}
							set dlistState inset
						}
						.RE {
							# next item comes or end of list
							if $verbose {puts "inset ends"}
							set dlistState {}
						}
						.PP {
							if {$dlistState eq "inset"} {
								if $verbose {puts "new paragraph in inset"}
								# - inside .RS/.RE we are still within one item
								lappend itemContent $line
							} else {
								# - outside, the list ends
								if $verbose {puts "finish dlist"}
								set finishDlist 1
							}
						}
						.SH - .SS {
							if $verbose {puts "SH/SS - finish list"}
							set finishDlist 1
						}
						.TP - .IP {
							if $verbose {puts "TP/IP found"}
							if {$dlistState eq "inset"} {
								# a list nested within a list item
								lappend itemContent $line
							} else {
								# new item starts, so finish the current one, if any
								if {$itemTitle ne ""} {
									if {$itemContent eq ""} {set itemContent "...see next..."}
									if $verbose {puts "finish ${blockType} item:\n+++\n$itemContent\n+++\n"}
									if {$blockType eq "Dlist"} {set itemAttr [list -definition $itemTitle]} else {set itemAttr [list]}
									if {[llength $itemVS]} {lappend itemAttr {*}$itemVS}
									set itemVS [list]
									lappend dlistContent [list ${blockType}Item $itemAttr [parseBlock ${blockType}Item $itemContent]]
									if $verbose {puts "dlistContent: $dlistContent"}
								}
								# !! subType can change in the middle of a .TP list !!
								set subType [string range $markup 1 end]
								# start new item
								if $verbose {puts "start new dlist item"}
								if {$subType eq "IP"} {
									# itemTitle is on the same line
									lassign [parseArgs $line] itemTitle
									set itemTitle [parseBackslash [BIRPclean $itemTitle]]
								} else {
									# itemTitle is on the next line
									set manContent [lrange $manContent 1 end]
									set itemTitle [parseBackslash [BIRPclean [lindex $manContent 0]]]
								}
								set itemContent {}
							}
						}
						. - .\\\" {
							# make broken ctags happy: "
							if $verbose {puts "ignore"}
							# ignore
						}
						.VS {
							lassign $line - info
							set itemVS [list -vs $info]
						}
						.VE {
							# we assume that VS does not span more than one TP/IP
							# so we automatically reset this info when the next TP/IP starts
						}
						default {
							# just collect item material
							if $verbose {puts "collect dlist item ($line)"}
							lappend itemContent $line
						}
					}
					if {! $finishDlist} {
						# prepare next line
						set manContent [lrange $manContent 1 end]
						if {[llength $manContent] == 0} {
							# emergency stop:
							break
						}
						set line [lindex $manContent 0]
						set markup [string range $line 0 2]
					}
				}
				# the list is over, finish it and look for the next block
				if $verbose {puts "finish dlist"}
				if {$itemContent eq ""} {set itemContent "...see next..."}
				# as the content of the DlistItem block is itself some blocks, we need to parse it again here:
				if $verbose {puts "finish last dlist item:\n+++\n$itemContent\n+++\n"}
				if {$blockType eq "Dlist"} {set itemAttr [list -definition $itemTitle]} else {set itemAttr [list]}
				if {[llength $itemVS]} {lappend itemAttr {*}$itemVS}
				lappend dlistContent [list ${blockType}Item $itemAttr [parseBlock ${blockType}Item $itemContent]]
				if $verbose {puts "dlistContent: $dlistContent"}
				set blockContent $dlistContent
				set doCloseBlock 1
			}
			.SH {
				# section heading (assumption: everything is on one single line)
				# this will typically also be one ending a block of "AP" elements
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				if $verbose {puts Section}
				set SHcontent [string totitle [string trim [string range $line 3 end] \"\ ]]
				switch $SHcontent {
					"Keywords" -  "See also" {
						# the content of this section is just a list of keywords
						# -> read them and transfer to metadata:
						array set SHType {Keywords Keywords "See also" Links}
						set markup {}
						set keywords [list]
						while {1} {
							set manContent [lrange $manContent 1 end]
							set line [lindex $manContent 0]
							set markup [string index $line 0]
							if {$markup ni {. '} && [llength $manContent] > 0} {
								lappend keywords {*}[split [string trim $line {, }] ,]
							} else {
								set keywords [lmap el $keywords {string trim $el}]
								dict lappend manual meta $SHType($SHcontent) $keywords
								break
							}
						}
					}
					"Synopsis" {
						# this is the place where the commands are listed with their syntax:
						lappend blockList [list Header {-level 1} $SHcontent]
						# go to first line of section content:
						set manContent [lrange $manContent 1 end]
						set line [lindex $manContent 0]
						# the Tcl.n page is special (has no command syntax):
						if {[dict get $manual name] eq "Tcl"} {
							lappend blockList [list Paragraph {} $line]
							set manContent [lrange $manContent 1 end]
							continue
						}
						set cmd [string range $line 0 2]
						# the content of this section is some command syntax
						# with one command per line
						set contentList [list]
						set lineCount 0
						while {$cmd ne ".SH"} {
							if {! [string match ".*" $cmd]} {
								# in the synopsis of section 3 pages there are lines of their own with
								# the return type of the Tcl API function. We want to get them onto the same
								# line as the command for processing:
								if {[dict get $manual meta ManualSection] == 3 && [string range $line 0 2] ne "\\fB"} {
									set line "#$line "
									incr lineCount
									append line [lindex $manContent $lineCount]
								}
								lappend contentList [list Syntax {} [parseCommand -ast $line]]
							}
							# go to next line:
							incr lineCount
							set line [lindex $manContent $lineCount]
							set cmd [string range $line 0 2]
						}
						# add to AST wrapped in a fenced div:
						lappend blockList [list Div .synopsis $contentList]
						# remove eaten lines from manContent:
						set manContent [lrange $manContent $lineCount end]
					}
					default {
						# add section to AST
						lappend blockList [list Header {-level 1} $SHcontent]
						set manContent [lrange $manContent 1 end]
					}				
				}
			}
			.SS {
				# subsection heading (assumption: everything is on one single line)
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				if $verbose {puts Subsection}
				set SScontent [string totitle [string trim [string range $line 3 end] \"\ ]]
				#return [list [list Header {-level 2} $SScontent] {*}[parseBlock Header [lrange $manContent 1 end]]]
				lappend blockList [list Header {-level 2} $SScontent]
				set manContent [lrange $manContent 1 end]
			}
			.PP - . {
				# beginning of new paragrah
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				if $verbose {puts PP}
				set blockType Paragraph
				set manContent [lrange $manContent 1 end]
			}
			.CS {
				# code start
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				if $verbose {puts "CODE"}
				set blockType Code
				set manContent [lrange $manContent 1 end]
			} 
			.CE - .VE {
				# .CE = code end
				# .VE = end of sidebar
				set doCloseBlock 1
				set manContent [lrange $manContent 1 end]
				continue
			}
			.QW - .PQ - .QR {
				# this is not a block element of the AST but it is a single line in nroff so we must handle it here.
				# We convert this to Inline quotes and append to the current content
				# with a special syntax starting with "§" and ending with "§"
				# (this enables us to distinguish between a quoted text and a literal quote character)
				# ToDo: this is often used to write inline Tcl code -> try to detect automatically!
				if {$blockType eq ""} {set blockType Paragraph}
				if {$verbose} {puts "Quote"}
				set myText [parseQuoting $line]
				if {$blockContent eq ""} {append blockContent $myText} else {append blockContent { } $myText}
				set manContent [lrange $manContent 1 end]
			}
			.VS {
				# sidebar for marking newly-changed parts of manual pages.
				# We interpret this as a new paragraph (for now):
				if {$blockType ne ""} {set doCloseBlock 1; continue}
				set blockType Paragraph
				lassign $line - info
				set blockAttributes [list version $info]
				set manContent [lrange $manContent 1 end]
			}
			default {
				# if nothing else, just collect content:
				if {$blockType eq ""} {set blockType Paragraph}
				if {$verbose} {puts "CONTENT of $blockType"}
				if {$blockType eq "Code"} {set separator \n} else {set separator { }}
				if {$blockContent eq ""} {append blockContent $line} else {append blockContent $separator $line}
				set manContent [lrange $manContent 1 end]
			}
		}
	}
	# also process the last still open block:
	if {$blockContent ne ""} {lappend blockList [list $blockType $blockAttributes $blockContent]}
	if {$verbose} {puts "... produced [lindex $blockList end]"}
	return $blockList
}


proc ::ndoc::parseCommand {mode line} {
	#
	# reads a line of the nroff containing a Tcl syntax (from the SYNOPSIS etc.)
	# and returns a list of AST Span elements representing this syntax
	#
	# mode - the operation mode; '-ast' returns the AST block list, '-internal' returns the internal representation for easier parsing
	# line - the complete Tcl command in nroff syntax 
	#
	# Note: the Span elements are alread fully expanded with appropriate AST "Text" elements
	#
	set DEBUG 0
	set line [BIRPclean $line]
	# make life a bit easier for parsing by replacing nroff syntax with something simpler,
	# so this is the internal representation:
	# § is bold
	# + is italic
	# = is reset
	set line [string map {{\fB} § {\fI} + {\fR} = {\fP} =} $line]
	set line [string map {\\ {}} $line]
	if {$mode eq "-internal"} {return $line}
	set spanList [list]
	set state =
	set chunk {}
	# some more processing is needed for section 3 pages:
	if {[regexp {^.+\(.*\)$} $line]} {
		if {$DEBUG == 3} {puts "===\nline API=$line"}
		set l0 {}
		set startIndex 0
		if {[string index $line 0] eq "#"} {
			# return code at the beginning, split into own word:
			set startIndex [string first "§" $line]
			set l0 [string trim [string range $line 0 [expr {$startIndex - 1}]]]
		}
		set openParens [string first ( $line $startIndex]
		# the API function name:
		set l1 [string range $line $startIndex [expr {$openParens - 1}]]
		# the arguments to the function:
		set l2 [string range $line [expr {$openParens + 1}] end-1]
		if {$DEBUG == 3} {puts L0=$l0; puts L1=$l1; puts L2=$l2}
		if {$l0 eq ""} {set line [list $l1 $l2]} else {set line [list $l0 $l1 $l2]}
	} elseif {[string index $line 0] eq "§" && [string index $line 1] eq "#"} {
		# an '#include' line in section 3:
		if {$DEBUG == 3} {puts "line # = $line"; puts L=#}
		return [list [list Strong {} [list [list Text {} [string range $line 1 end-1]]]]]
	}
	# [apply] code to expand the content of a Span (except Space) to contain a correct AST Text element:
	set expandSpan	[list spanList {
		lmap el $spanList {
			lassign $el sName sAttr sContent
			if {$sName eq "Space"} {
				set el
			} else {
				set el [list $sName $sAttr [list [list Text {} $sContent]]]
			}
		}
	}]
	# translate line to Span elements with Space elements in between:
	if $DEBUG {puts "-------\nparseCommand: $line"}
	set cmdType {}
	# treat the line as a list so that we easily can detect the individual elements:
	for {set i 0} {$i < [llength $line]} {incr i} {
		set word [lindex $line $i]
		if {$i == 0} {
			# the first word
			if $DEBUG {puts "1st-word: $word"}
			switch -regexp $word {
				{^§.+=$} {
					# only one cmd without subcommand:
					if {[string range $word 1 4] eq "Tcl_" || [string range $word 1 9] eq "TclZipfs_"} {
						# API command:
						set cmdType ccmd
					} else {
						set cmdType cmd
					}
					lappend spanList [list Span .$cmdType [string range $word 1 end-1]]
					if $DEBUG {puts "single .cmd: $spanList"}
				}
				{^§.+[^=]$} {
					# a cmd with a subcommand and possibly more elements:
					lappend spanList [list Span .cmd [string range $word 1 end]]
					# there must be a second word = subcommand,
					# sometimes also more words and these are then literals
					while 1 {
						incr i
						set word [lindex $line $i]
						if {$i == 1} {set type .sub} else {set type .lit}
						if {[string index $word end] eq "="} {
							lappend spanList [list Space {} {}] [list Span $type [string range $word 0 end-1]]
							break
						} else {
							lappend spanList [list Space {} {}] [list Span $type [string range $word 0 end]]
						}
						if {$i > [llength $line]} {
							puts "emergencyStop 1 in parseCommand"
							puts "Line: $line"
							exit
						}
					}
					if $DEBUG {puts ".cmd with subcommand: $spanList"}
				}
				{^\+.+=$} {
					# an instance as a command name:
					# (this one is always followd by a subcommand)
					incr i
					set sub [lindex $line $i]
					lappend spanList [list Span .ins [string range $word 1 end-1]] \
						[list Space {} {}] \
						[list Span .sub [string range $sub 1 end-1]]
					if $DEBUG {puts "instance command .ins: $spanList"}
				}
				{^#.+$} {
					# the return type of a Tcl C API function
					# (the '#' was prefixed in parseBlock so we can detect it here ...)
					lappend spanList [list Span .ret [string range $word 1 end]] [list Space {} {}]
					if $DEBUG {puts "section 3 return code .ret: $spanList"}
					# remove the first word from the line and go again for the first word (avoid code duplication):
					set line [lrange $line 1 end]
					incr i -1
					continue
				}
				default {
					puts "emergencyStop 0 in parseCommand"
							puts "Line: $line"
							exit
				}
			}
			set spanList [apply $expandSpan $spanList]
		} else {
			# remaining words of the command:
			set sublist [list]
			if $DEBUG {puts "word [expr {$i + 1}]: $word"}
			if {$cmdType eq "ccmd"} {
				if $DEBUG {puts "API arguments: [string range $word 1 end-1]"}
				lappend spanList [list Span .cargs [list [list Text {} [string range $word 1 end-1]]]]
				continue
			}
			switch -regexp $word {
				{^\?\+.+ \.\.\.\=\?$} {
					# possibly multiple optional args
					# a word with trailing "..." and a space between
					lappend sublist [list Span .optdot [string range $word 2 end-6]]
					lappend spanList [list Space {} {}] {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "(multiple) optional .optdot: $spanList"}
				}
				{^\+.+=$} {
					# single mandatory arg: .arg
					# + followed by some word followed by =
					lappend sublist [list Span .arg [string range $word 1 end-1]]
					lappend spanList [list Space {} {}] {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "single mandatory .arg: $spanList"}
				}
				{^\+.+[^=]?$} {
					# multiple mandatory args: .arg
					lappend sublist [list Space {} {}] [list Span .arg [string range $word 1 end]]
					# also get all other members of this group:
					while 1 {
						incr i
						set word [lindex $line $i]
						if {[string index $word end] eq "="} {
							lappend sublist [list Space {} {}] [list Span .arg [string range $word 0 end-1]]
							break
						} else {
							lappend sublist [list Space {} {}] [list Span .arg $word]
						}
						if {$i > [llength $line]} {
							puts "emergencyStop 2 in parseCommand"
							puts "Line: $line"
							exit
						}
					}
					lappend spanList {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "multiple mandartoy .arg: $spanList"}
				}
				{^\?\+.+=\?$} {
					# single optional arg = .optarg
					# ? followed by + followed by a word followed by = followed by ?
					lappend sublist [list Span .optarg [string range $word 2 end-2]]
					lappend spanList [list Space {} {}] {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "single optional arg .optarg: $spanList"}
				}
				{^§.+=$} {
					# single mandatory literal = .lit
					# (can also start with a dash and is then called a 'required option' in Tcl jargon):
					lappend sublist [list Span .lit [string range $word 1 end-1]]
					lappend spanList [list Space {} {}] {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "single mandatory literal .lit: $spanList"}
				}
				{^\?§.+=\?$} {
					# single optional literal = .optlit:
					lappend sublist [list Span .optlit [string range $word 2 end-2]]
					lappend spanList [list Space {} {}] {*}[apply $expandSpan $sublist]
					if $DEBUG {puts "single optional literal .optlit: $spanList"}
				}
				{^\?§.+=$} {
					# multiple optional arg group (first arg is mandatory literal):
					lappend sublist [list Span .lit [string range $word 2 end-1]]
					set grouptype .optarg
					# also get all other members of this group and put them into the first span
					while 1 {
						incr i
						set word [lindex $line $i]
						# next word is typically optional
						if {[string index $word 0] eq "+"} {
							if {[string index $word end] eq "?"} {
								# end of group
								lappend sublist [list Space {} {}] [list Span .arg [string range $word 1 end-2]]
								break
							} else {
								# middle of group

								if {[lindex $word end] eq "="} {
									lappend sublist [list Space {} {}] [list Span .arg [string range $word 1 end-1]]
								} else {
									lappend sublist [list Space {} {}] [list Span .arg [string range $word 1 end]]
								}
							}
						} elseif {[string range $word 0 2] eq "..."} {
							set grouptype .optdot
							break
						} else {
							return -code error "parseCommand: detected an arg in an optional argument group that not yet a handled case."
						}
						if {$i > [llength $line]} {
							puts "emergencyStop 3 in parseCommand"
							puts "Line: $line"
							exit
						}
					}
					set sublist [apply $expandSpan $sublist]
					lappend spanList [list Space {} {}] [list Span $grouptype $sublist]
					if $DEBUG {puts "multiple optional arg group: $spanList"}
				}
				{^\?\+.+[^=][^\?]} {
					# multiple optional arguments
					set content [string range $word 2 end]
					while 1 {
						incr i
						set word [lindex $line $i]
						if {[string range $word end-1 end] eq "=?"} {
							set word [string range $word 0 end-2]
							if {$word eq "..."} {
								set type .optdot
							} else {
								set type .optarg
								lappend content [string range $word 0 end]
							}
							break
						} else {
							lappend content $word
						}
						if {$i > [llength $line]} {
							puts "emergencyStop 4 in parseCommand"
							puts "Line: $line"
							exit
						}
					}
					lappend spanList [list Space {} {}] {*}[apply $expandSpan [list [list Span $type [join $content " "]]]]
					if $DEBUG {puts "multiple optional arguments: $spanList"}
				}
				default {
					puts "parseCommand error: unknown syntax pattern"
					puts "Line: $line"
					exit
				}
			}
		}
	}
	if $DEBUG {puts "RESULT = $spanList"}
	return $spanList
}


proc ::ndoc::parseInline {keyword attributes content} {
	#
	# parses an inline element of the (raw) nroff document
	#
	# keyword - an AST block keyword in which the inline is located
	# attributes - AST element options of the AST block element
	# content - a string of raw nroff characters representing the inline content
	#
	# The inline content is processed and a replacement AST is returned for the content
	#
	# Returns a replacement AST where the content is replaced with a list of (possibly nested) Inline AST elements
	#
	variable verbose
	variable convertCmdToLink
	set inlineAST [list]
	if {$verbose} {puts CONTENT=$content}
	if {$keyword eq "Code"} {
		# special case for 'Code' where we do not want to have anything expanded
		# apart from nroff backslash "formatting"
		set content [BIRPstrip $content]
		set content [string map {{\e} "\\"} $content]
		return [list $keyword $attributes $content]
	}
	set content [BIRPclean $content]

	while {[string length $content]} {
		set char [string index $content 0]
		set lookahead [string range $content 1 2]
		if {$verbose} {puts AST=$inlineAST}
		if {$verbose} {puts CHAR=$char}
		if {$char eq "\\" && $lookahead in {fB fI fR fP}} {
			if {$verbose} {puts STRONG/EMPHASIS}
			if {[string index $lookahead end] in {R P}} {
				return -code error "found '\\f[string index $lookahead end]' ... this should never happen ..."
			}
			set Keyword [string map {B Strong I Emphasis} [string index $lookahead end]]
			set found [regexp -indices -start 3 {(\\f)[BIRP]} $content endMark]
			if {$found} {
				set endMark [lindex $endMark 0]
				if {$verbose} {puts endMark=$endMark}
			} else {
				return -code error "unbalanced \\f markup"
			}
			lappend inlineAST [parseInline $Keyword {} [string range $content 3 $endMark-1]]
			# we here assume that \fP is used exactly like \fR as this seems the case in Tk man pages
			# but strictly the two are not the same!!
			if {[string index $content $endMark+2] in {R P}} {
				# the current String/Emphasis just ended, so remove the end marker
				set content [string range $content $endMark+3 end]
			} else {
				# we go directly from bold to italic or vice versa, so keep the start marker:
				set content [string range $content $endMark end]
			}
		} elseif {$char eq "§" && [string index $content 1] eq "\""} {
			# quoted text (starts with §" and ends with "§)
			if {$verbose} {puts QUOTED}
			set endMark [string first \"§ $content 2]
			# "
			if {$endMark == -1} {return -code error "parseInline: unbalanced quote (no end marker found)"}
			lappend inlineAST [parseInline Quoted {} [string range $content 2 $endMark-1]]
			set content [string range $content $endMark+2 end]
		} else {
			# normal text, digest it until 
			# - a known backslash sequence comes (\fI, \fB, ...)
			# - a quoted text comes
			# - or until the end of the content if nothing of the above is found
			if {$verbose} {puts DEFAULT}
			# put the first index where an '\f' or a quote was found into 'endMark'
			# (note that 'endMark' is a list of two indices)
			set found [regexp -indices {(\\f)[BIRP]|(§\")} $content endMark]
			# 
			if {$found} {
				set endMark [lindex $endMark 0]
			} else {
				set endMark [string length $content]
			}
			set textContent [string range $content 0 $endMark-1]
			# handle escaping of characters:
			set textContent [parseBackslash $textContent]
			lappend inlineAST [list Text {} $textContent]
			set content [string range $content $endMark end]
		}
	}
	return [list $keyword $attributes $inlineAST]
}


proc ::ndoc::AST2Markdown {} {
	#
	# converts an AST into markdown
	#
	variable manual
	# start with YAML metadata block:
	set output "---\n"
	foreach {key value} [dict get $manual meta] {
		switch $key {
			Keywords - Links {
				append output $key: \n
				foreach el $value {append output { - } $el \n}
			}
			default {
				append output $key : { } $value \n
			}
		}
	}
	append output Copyright: \n
	foreach el [dict get $manual copyright] {
		append output { - } $el \n
	}
	append output "---\n"
	# then continue with the actual content:
	set content [dict get $manual content]
	lassign $content documentType documentAttributes documentContent
	if {$documentType ne "Document"} {
		return -code error "AST2Markdown: 'Document' element missing"
	}
	foreach item $documentContent {
		append output [AST2Markdown_Element $documentType 0 $item]
	}
	return $output
}


proc ::ndoc::AST2Markdown_Element {parentType indent ASTelement} {
	#
	# convert a (possibly nested) single AST element into Markdown
	# and return the conversion result
	#
	# parentType - the AST type of the parent element (needed in some context to modify the formatting of the child element)
	# indent     - nmber of spaces the content must be indented
	# ASTelement - the AST element (3 element list) to be converted to markdown
	#
	variable listCounter
	variable listType
	# type       - the AST type of the element to convert
	# attributes - the list with option-value-pairs of AST attributes
	# content    - the actual content of the AST element, may be another nested AST element
	lassign $ASTelement type attributes content
	set output {}
	### prefix: ###
	switch $type {
		Paragraph {
			switch $parentType {
				List - Dlist {
					append output [string repeat { } $indent]
				}
				ListItem - DlistItem {
					append output [string repeat { } $indent]
				}
				default {
					append output \n
					if {[dict size $attributes]} {
						# put attributes into fenced div:
						append output ::: { } \{ .info
						dict for {k v} $attributes {append output { } $k=\"$v\"}
						append output \} \n
					}
				}
			}
		}
		Code      {append output \n [string repeat { } $indent] ``` \n}
		Header    {append output \n [string repeat # [dict get $attributes -level]] { }}
		List      {
			set listCounter 0
			set listType [dict get $attributes -type]
			append output \n
		}
		ListItem  {
			switch $listType {
				bullet {
					append output {- }
				}
				numbered {
					incr listCounter
					append output $listCounter {. }
				}
				default {
					return -code error "no such List type '$listType'"
				}
			}
		}
		Dlist     {append output \n}
		DlistItem {
			foreach item [dict get $attributes -definition] {
				append output [AST2Markdown_Element DlistItem 0 $item]
			}
			append output \n {: }
		}
		Span      {}
		Div       {
			append output \n ::: " {$attributes} " ::: \n
		}
		Syntax {}
		Space     {}
		Text      {}
		Emphasis  {append output *}
		Strong    {append output **}
		Quoted    {append output \"}
		Link      {}
		Inline    {}
		default {return -code error "no such AST element type '$type'"}
	}
	### content: ###
	switch $type {
		Text {
			append output $content
		}
		Code {
			foreach codeLine [split $content \n] {append output [string repeat { } $indent] $codeLine \n}
		}
		Space {
			append output { }
		}
		DlistItem {
			append output [AST2Markdown_Element $type 0 [lindex $content 0]]
			foreach item [lrange $content 1 end] {append output [AST2Markdown_Element $type 4 $item]}
		}
		Span {
			append output \[
			foreach item $content {append output [AST2Markdown_Element $type 0 $item]}
			append output \]
			append output \{ $attributes \}
		}
		Div {
			foreach item $content {append output [AST2Markdown_Element $type $indent $item]\n}
		}
		Syntax {
			foreach item $content {append output [AST2Markdown_Element $type $indent $item]}
		}
		default {
			foreach item $content {append output [AST2Markdown_Element $type $indent $item]}
		}
	}
	### suffix: ###
	switch $type {
		Paragraph {
			append output \n
			if {[dict size $attributes]} {
				append output ::: \n
			}
		}
		Header    {append output \n}
		Code      {append output [string repeat { } $indent] ``` \n}
		List      {}
		Dlist     {}
		ListItem  {append output \n}
		DlistItem {append output \n}
		Span      {}
		Div       {append output ::: \n}
		Syntax    {}
		Space     {}
		Text      {}
		Emphasis  {append output *}
		Strong    {append output **}
		Quoted    {append output \"}
		Link      {}
		default   {}
	}
	return $output
}


proc ::ndoc::man2markdown {manContent} {
	#
	# convert the nroff from a Tcl/Tk manual page into markdown
	#
	# manContent - the content of the nroff file to be converted
	#
	# Returns the markdown
	#
	man2AST $manContent
	set md [AST2Markdown]
	return [mdExceptions $md]
}


proc ::ndoc::mdExceptions {md} {
	#
	# deal with stuff we cannot easily do automatically,
	# i.e. all sorts of things that fall through the normal handling
	# or are being done over-agressively
	#
	variable manual
	set myFile [dict get $manual meta CommandName]
	switch $myFile {
		re_syntax {
			set md [string map {{Different flavors of res} {Different flavours of REs}} $md] 
		}
	}
	return $md
}


proc ::ndoc::readFile {filename} {
	#
	# read a text file and returns its content as a list split at \n
	#
	# filename - name (including path) of the file to be read
	#
	# Side effect: sets the 'name' property in the variable 'manual'
	#
	variable manual
	dict set manual name [file root [file tail $filename]]
	if {![file exists $filename]} {
		return -code error "readFile: File '$filename' does not exist." 
	}
	set fh [open $filename r]
	set manContent [read $fh]
	close $fh
	return $manContent
}


proc ::ndoc::main {} { 
	#
	# main entry point to the script
	# when called directly from the command line
	# (i.e. not [source]d)
	#
	global argv
	variable manual
	variable verbose
	if {[lindex $argv 0] eq ""} {
		#### main testing code follows (prints to stdout) ####
		set myDir [file dirname [info script]]
		set tclManDir [file join $myDir .. doc]
		set tkManDir [file join $myDir .. doc]
	
		set myFile [file join $tclManDir string.n]
		#set myFile [file join $tclManDir clock.n]
		#set myFile [file join $tclManDir Dstring.3]
		#set myFile [file join $tclManDir exec.n]
		#set myFile [file join $tclManDir filename.n]
		#set myFile [file join $tclManDir SetResult.3]
		#set myFile [file join $tclManDir SetVar.3]
		#set myFile [file join $tclManDir subst.n]
		#set myFile [file join $tkManDir bind.n]
		#set myFile [file join $tclManDir filename.n]
		#set myFile [file join $tkManDir scrollbar.n]
		puts [man2markdown [readFile $myFile]]
	} elseif {[llength $argv] == 2} {
		# convert whole directory:
		lassign $argv inDir outDir
		if {! [file isdirectory $inDir]} {
			return -code error "Error: first argument is not a directory"
		}
		if {[file exists $outDir] && ! [file isdirectory $outDir]} {
			return -code error "Error: second argument is not a directory"
		} elseif {! [file exists $outDir]} {
			file mkdir $outDir
		}
		# first the files in section "n", then in section "1", finally in section "3":
		foreach section {n 1 3} {
			foreach file [glob [file join $inDir *.$section]] {
				puts "converting $file ..."
				set md [man2markdown [readFile $file]]
				set stem [file rootname [file tail $file]]
				# make sure not to overwrite files in the n section with
				# files from the 3 section having the same name
				# (was no problem previously as they had different
				# file extensions):
				if {$section ne "n" && $stem in {Class Concat Encoding Eval Exit Load Namespace Object RegExp UpVar zipfs}} {append stem $section}
				set fh [open [file join $outDir ${stem}.md] w]
				puts $fh $md
				close $fh
			}
		}
	} else {
		# convert single file (to stdout):
		puts [man2markdown [readFile [lindex $argv 0]]]
	}
}


if {[info exists argv0] && [file tail [info script]] eq [file tail $argv0]} {
	# call [main] when the script is *not* sourced by another script
	# but called from the command line
	::ndoc::main
}