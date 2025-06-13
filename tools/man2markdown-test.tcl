# test script for certain procedures in man2markdown.tcl

package require tcltest
source man2markdown.tcl

tcltest::configure -verbose {pass error}


tcltest::test BIRPclean-0.0 {...} {
	::ndoc::BIRPclean {\fB::pkg::create\fR \fB\-name \fIpackageName \fB\-version \fIpackageVersion\fR ?\fB\-load \fIfilespec\fR? ... ?\fB\-source \fIfilespec\fR? ...}
} {\fB::pkg::create\fR \fB\-name\fR \fIpackageName\fR \fB\-version\fR \fIpackageVersion\fR ?\fB\-load\fR \fIfilespec\fR? ... ?\fB\-source\fR \fIfilespec\fR? ...}

tcltest::test BIRPclean-0.1 {...} {
	::ndoc::BIRPclean {\fB::msgcat::mcmax ?\fIsrc-string src-string ...\fR?}
} {\fB::msgcat::mcmax\fR ?\fIsrc-string src-string ...\fR?}

exit
foreach {num before after md ast} {
	1
	{\fBappend \fIvarName \fR?\fIvalue value value ...\fR?}
	{\fBappend\fR \fIvarName\fR ?\fIvalue value value ...\fR?}
	{**append** *varName* ?*value value value ...*?}
	{Inline {} {{Strong {} {{Text {} append}}} {Text {} { }} {Emphasis {} {{Text {} varName}}} {Text {} { ?}} {Emphasis {} {{Text {} {value value value ...}}}} {Text {} ?}}}
	
	2
	{\fBdict merge \fR?\fIdictionaryValue ...\fR?}
	{\fBdict merge\fR ?\fIdictionaryValue ...\fR?}
	{**dict merge** ?*dictionaryValue ...*?}
	{Inline {} {{Strong {} {{Text {} {dict merge}}}} {Text {} { ?}} {Emphasis {} {{Text {} {dictionaryValue ...}}}} {Text {} ?}}}
	
	3
	{\fBTk_CreateClientMessageHandler\fR arranges for \fIproc\fR}
	{\fBTk_CreateClientMessageHandler\fR arranges for \fIproc\fR}
	{**Tk_CreateClientMessageHandler** arranges for *proc*}
	{Inline {} {{Strong {} {{Text {} Tk_CreateClientMessageHandler}}} {Text {} { arranges for }} {Emphasis {} {{Text {} proc}}}}}
	
	4
	{\fBstring wordend \fIstring charIndex\fR}
	{\fBstring wordend\fR \fIstring charIndex\fR}
	{**string wordend** *string charIndex*}
	{Inline {} {{Strong {} {{Text {} {string wordend}}}} {Text {} { }} {Emphasis {} {{Text {} {string charIndex}}}}}}
	
	5
	{\fBstring compare\fR ?\fB\-nocase\fR? ?\fB\-length\fI length\fR? \fIstring1 string2\fR}
	{\fBstring compare\fR ?\fB\-nocase\fR? ?\fB\-length\fR \fIlength\fR? \fIstring1 string2\fR}
	{**string compare** ?**-nocase**? ?**-length** *length*? *string1 string2*}
	{Inline {} {{Strong {} {{Text {} {string compare}}}} {Text {} { ?}} {Strong {} {{Text {} -nocase}}} {Text {} {? ?}} {Strong {} {{Text {} -length}}} {Text {} { }} {Emphasis {} {{Text {} length}}} {Text {} {? }} {Emphasis {} {{Text {} {string1 string2}}}}}}
	
	5a
	{re_syntax \- Syntax of Tcl regular expressions}
	{re_syntax \- Syntax of Tcl regular expressions}
	{re_syntax - Syntax of Tcl regular expressions}
	{Inline {} {{Text {} {re_syntax - Syntax of Tcl regular expressions}}}}
	
	6
	{.IP \fB{\fIm\fB,\fIn\fB}\fR 6}
	{.IP \fB{\fR\fIm\fR\fB,\fR\fIn\fR\fB}\fR 6}
	{.IP **{***m***,***n***}** 6}
	{Inline {} {{Text {} {.IP }} {Strong {} {{Text {} \{}}} {Emphasis {} {{Text {} m}}} {Strong {} {{Text {} ,}}} {Emphasis {} {{Text {} n}}} {Strong {} {{Text {} \}}}} {Text {} { 6}}}}
	
	6a
	{\fB\e\fIk\fR}
	{\fB\e\fR\fIk\fR}
	{**\\**k*}
	{Inline {} {{Strong {} {{Text {} \\}}} {Emphasis {} {{Text {} k}}}}}
	
	6b
	{\fB*\fR}
	{\fB*\fR}
	{**\***}
	{Inline {} {{Strong {} {{Text {} {*}}}}}}
	
	7
	{\fB*?  +?  ??  {\fIm\fB}?  {\fIm\fB,}?  {\fIm\fB,\fIn\fB}?\fR}
	{\fB*?  +?  ??  {\fR\fIm\fR\fB}?  {\fR\fIm\fR\fB,}?  {\fR\fIm\fR\fB,\fR\fIn\fR\fB}?\fR}
	{**\*?  +?  ??  {***m***}?  {***m***,}?  {***m***,***n***}?**}
	{Inline {} {{Strong {} {{Text {} {*?  +?  ??  \{}}}} {Emphasis {} {{Text {} m}}} {Strong {} {{Text {} {\}?  \{}}}} {Emphasis {} {{Text {} m}}} {Strong {} {{Text {} {,\}?\ \ \{}}}} {Emphasis {} {{Text {} m}}} {Strong {} {{Text {} ,}}} {Emphasis {} {{Text {} n}}} {Strong {} {{Text {} \}?}}}}}
	
	8
	{The forms using \fB{\fR and \fB}\fR are known as \fIbound\fRs. The}
	{The forms using \fB{\fR and \fB}\fR are known as \fIbound\fRs. The}
	{The forms using **{** and **}** are known as *bound*s. The}
	{Inline {} {{Text {} {The forms using }} {Strong {} {{Text {} \{}}} {Text {} { and }} {Strong {} {{Text {} \}}}} {Text {} { are known as }} {Emphasis {} {{Text {} bound}}} {Text {} {s. The}}}}
	
	9
	{\fBf\fR}
	{\fBf\fR}
	{**f**}
	{Inline {} {{Strong {} {{Text {} f}}}}}
	
	10
	{\fBmax \fIarg\fB \fI...\fR}
	{\fBmax\fR \fIarg\fR \fI...\fR}
	{**max** *arg* *...*}
	{Inline {} {{Strong {} {{Text {} max}}} {Text {} { }} {Emphasis {} {{Text {} arg}}} {Text {} { }} {Emphasis {} {{Text {} ...}}}}}

	11
	{\fBthis is bold\fR}
	{\fBthis is bold\fR}
	{**this is bold**}
	{Inline {} {{Strong {} {{Text {} {this is bold}}}}}}
	
	12
	{this is a paragraph}
	{this is a paragraph}
	{this is a paragraph}
	{Inline {} {{Text {} {this is a paragraph}}}}
} {
	tcltest::test BIRPclean-$num {...} [list ::ndoc::BIRPclean $before] $after
	#tcltest::test BIR2markdown-$num {...} [list ::ndoc:::BIR2markdown $after] $md
	tcltest::test parseInline-$num {...} [list ::ndoc::parseInline Inline {} $after] $ast
	tcltest::test ast2markdown-$num {...} [list ::ndoc::AST2Markdown_Element Paragraph 0 $ast] $md
}


# note that number QW-5 below (from subst(n)) is wrong nroff
# but is parsed correctly here!
foreach {num before after} {
	QW-0
	{.QW abcd}
	{§"abcd"§}
	
	QW-1
	{.QW "up to date"}
	{§"up to date"§}
	
	QW-2
	{.QW "\fIoptions ...\fR"}
	{§"\fIoptions ...\fR"§}
	
	QW-3
	{.QW "\fIputs \N'34'hello\N'34'\fR"}
	{§"\fIputs \N'34'hello\N'34'\fR"§}
	
	QW-4
	{.QW "\fIencoding/base64\fR" .}
	{§"\fIencoding/base64\fR"§.}
	
	QW-5
	{.QW "\fBxyz {p\e} q \e{r}\fR".}
	{§"\fBxyz {p\e} q \e{r}\fR"§.}
	
	QW-6
	{.QW abcd ).}
	{§"abcd"§).}
	
	
	
	PQ-1
	{.PQ RE s ,}
	{(§"RE"§s),}
	
	PQ-2
	{.PQ "char *"}
	{(§"char *"§)}
	
	PQ-3
	{.PQ $}
	{(§"$"§)}
	
	PQ-4
	{.PQ \fB%\fR "" ,}
	{(§"\fB%\fR"§),}
	
	PQ-5
	{.PQ word extra}
	{(§"word"§extra)}
	
	
	
	QR-1
	{.QR \fB0\fR \fB9\fR ,}
	{§"\fB0\fR-\fB9\fR"§,}
} {
	tcltest::test parse-$num {...} [list ::ndoc::parseQuoting $before] $after
}


foreach {num nroff internal ast markdown} {
	
	cmd-1
	{\fBarray \fIoption arrayName\fR ?\fIarg arg ...\fR?}
	{§array= +option arrayName= ?+arg arg ...=?}
	{{Span .cmd {{Text {} array}}} {Space {} {}} {Span .arg {{Text {} option}}} {Space {} {}} {Span .arg {{Text {} arrayName}}} {Space {} {}} {Span .optdot {{Text {} {arg arg}}}}}
	{[array]{.cmd} [option]{.arg} [arrayName]{.arg} [arg arg]{.optdot}}

	cmd-2
	{\fBarray get\fI arrayName \fR?\fIpattern\fR?}
	{§array get= +arrayName= ?+pattern=?}
	{{Span .cmd {{Text {} array}}} {Space {} {}} {Span .sub {{Text {} get}}} {Space {} {}} {Span .arg {{Text {} arrayName}}} {Space {} {}} {Span .optarg {{Text {} pattern}}}}
	{[array]{.cmd} [get]{.sub} [arrayName]{.arg} [pattern]{.optarg}}
	
	cmd-3
	{\fBreturn \fR?\fB\-code \fIcode\fR? ?\fIresult\fR?}
	{§return= ?§-code= +code=? ?+result=?}
	{{Span .cmd {{Text {} return}}} {Space {} {}} {Span .optarg {{Span .lit {{Text {} -code}}} {Space {} {}} {Span .arg {{Text {} code}}}}} {Space {} {}} {Span .optarg {{Text {} result}}}}
	{[return]{cmd} [[-code]{.lit} [code]{.arg}]{.optarg} [result]{.optarg}}
	
	cmd-4
	{\fBputs \fR?\fB\-nonewline\fR? ?\fIchannel\fR? \fIstring\fR}
	{§puts= ?§-nonewline=? ?+channel=? +string=}
	{{Span .cmd {{Text {} puts}}} {Space {} {}} {Span .optlit {{Text {} -nonewline}}} {Space {} {}} {Span .optarg {{Text {} channel}}} {Space {} {}} {Span .arg {{Text {} string}}}}
	{[puts]{.cmd} [-nonewline]{.optlit} [channel]{.optarg} [string]{.arg}}
	
	cmd-5
	{\fIpathName \fBaddtag \fItag searchSpec \fR?\fIarg ...\fR?}
	{+pathName= §addtag= +tag searchSpec= ?+arg ...=?}
	{{Span .ins {{Text {} pathName}}} {Space {} {}} {Span .sub {{Text {} addtag}}} {Space {} {}} {Span .arg {{Text {} tag}}} {Space {} {}} {Span .arg {{Text {} searchSpec}}} {Space {} {}} {Span .optdot {{Text {} arg}}}}
	{[pathName]{.ins} [addtag]{.sub} [tag]{.arg} [searchSpec]{.arg} [arg]{.optdot}}
	
	cmd-6
	{\fBtrace add command\fI name ops commandPrefix\fR}
	{§trace add command= +name ops commandPrefix=}
	{{Span .cmd {{Text {} trace}}} {Space {} {}} {Span .sub {{Text {} add}}} {Space {} {}} {Span .lit {{Text {} command}}} {Space {} {}} {Span .arg {{Text {} name}}} {Space {} {}} {Span .arg {{Text {} ops}}} {Space {} {}} {Span .arg {{Text {} commandPrefix}}}}
	{[trace]{.cmd} [add]{.sub} [command]{.lit} [name]{.arg} [ops]{.arg} [commandPrefix]{.arg}}
} {
	tcltest::test parseCommandInternal-$num {...} [list ::ndoc::parseCommand -internal $nroff] $internal
	tcltest::test parseCommandAST-$num {...} [list ::ndoc::parseCommand -ast $nroff] $ast
}

# here are some tests with snipptes from man files to test the overall working, especially newlines and spaces:
tcltest::test doc1 {...} {
::ndoc::man2markdown {'\"
'\" Copyright (c) 1998-2000 Scriptics Corporation.
'\" All rights reserved.
'\"
.TH pkg::create n 8.3 Tcl "Tcl Built-In Commands"
.so man.macros
.BS
'\" Note:  do not modify the .SH NAME line immediately below!
.SH NAME
pkg::create \- Construct an appropriate 'package ifneeded' command for a given package specification
.SH SYNOPSIS
\fB::pkg::create\fR \fB\-name \fIpackageName \fB\-version \fIpackageVersion\fR ?\fB\-load \fIfilespec\fR? ... ?\fB\-source \fIfilespec\fR? ...
.BE
.SH DESCRIPTION
.PP
\fB::pkg::create\fR is a utility procedure that is part of the standard Tcl
library.  It is used to create an appropriate \fBpackage ifneeded\fR
command for a given package specification.  It can be used to construct a
\fBpkgIndex.tcl\fR file for use with the \fBpackage\fR mechanism.}
} {---
CommandName: pkg::create
ManualSection: n
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Copyright:
 - Copyright (c) 1998-2000 Scriptics Corporation.
---

# Name

pkg::create - Construct an appropriate 'package ifneeded' command for a given package specification

# Synopsis

::: {.synopsis} :::
[::pkg::create]{.cmd} [-name]{.lit} [packageName]{.arg} [-version]{.lit} [packageVersion]{.arg} [[-load]{.lit} [filespec]{.arg}]{.optarg} [[-source]{.lit} [filespec]{.arg}]{.optarg}
:::

# Description

**::pkg::create** is a utility procedure that is part of the standard Tcl library.  It is used to create an appropriate **package ifneeded** command for a given package specification.  It can be used to construct a **pkgIndex.tcl** file for use with the **package** mechanism.
}