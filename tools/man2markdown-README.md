# Usage of man2markdown script to convert nroff to markdown

This script parses an nroff formatted Tcl/Tk manual page, converts it into some AST (Abstract Syntax Tree) and then writes it out again as a markdown file.

When this script is called from another Tcl script (i.e. using the [source] command), you can call the procedure `::ndoc::man2markdown` with a the content of a man file to be converted as an argument (the whole script and its procedures live in the '::ndoc::' namespace). If you just have the man file, you can use: `::ndoc::man2markdown [::ndoc::readFile /path/to/my/file.n]`.

This procedure will return the resulting markdown.

When this script is called directly from the command line using tclsh, specifying an nroff file as an argument will call the `[::ndoc::man2markdown]` procedure on the file content and return the markdown on stdout.

When not specifying an argument, it runs in test mode, converting a specific manual page from within the repository structure which this script is a part of. The result is written to stdout.

When run with two directory names, it will convert all nroff files in the (existing) first directory and put the result as markdown files into the second directory (which will be created if it does not exist). This should be used to produce the current status of afairs into doc/markdown/:

```
# (from the tools directory)
tclsh man2markdown.tcl ../doc ../markdown
```

The last invocation can be used to check what changed with respect to the last `fossil commit`. Any change reported by fossil (before committing) is a change that might have issues compared to teh previous version, so it should be checked before the commit and amended before the commit!

# Usage of Pandoc to convert generated markdown to nroff

...

# Usage of Pandoc to convert generated markdown to HTML

....

# Status of conversion process

The tcl/doc directory contains 248 manual pages which are divided as follows:


section | number of documents | initial conversion | final conversion
--------|---------------------|--------------------|-----------------
1       | 1                   | 1                  | 0
3       | 108                 | 25                 | 0
n       | 139                 | 139                | 0


The tk/doc directory comes next, after finishing Tcl.

Note, that only pages counted under "final conversion" are considered
final markdown versions of the nroff originals. Currently, many of the
markdown pages are still missing some markup or still contains
nroff markdown that is not yet handled. 


# Open issues
This section lists the shortcoming which are still present in the pages that already have an initial markdown version.

## General
- handling of '\fB\e\fIk\fR' (see test script) not correct yet
- not handling of ".AS" and ".AP" macros yet

## Specific pages
- oo::... pages: no handling of the class hierarchy subsection yet
- configurable.n: no way to represent nested '?' in synopsis, attempt made to split syntax definition into pieces


# Implemented and missing features in conversion script

List of nroff commands/macros currently implemented (partly not a complete implementation):

- .\" (comments)
- '\" (comments)
- backslash escapes "e" and "c"
- . (single dot as placeholder)
- \\fB (bold text)
- \\fI (italic text)
- \\fR (reset font info)
- \\fP (treated the same as \\fB)
- .QW (text in quotes)
- .PQ (text in quotes + paranthesis)
- .QR (text in quotes + dash between elements)
- .so (include files)
- .BS .BE (start and end of box enclosure)
- .AS (maximum sizes of arguments for setting tab stops)
- .ta (set tab stops)
- .RS .RE (relative inset, i.e. indentation)
- .nf .fi (turn off/on filling of lines)
- .TH (title heading)
- .TP (tagged paragraph)
- .IP (indented paragaph)
- .PP (paragraph)
- .SH (section header)
- .SS (subsection header)
- .VS .VE (start and end of vertical line)
- .CS .CE (start and end of code block)



AST element "Paragraph" is not yet treated properly (the content field of the AST is not yet a list of Inline AST elements).