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
1       | 1                   | 0                  | 0
3       | 108                 | 0                  | 0
n       | 139                 | 21                 | 0


The tk/doc directory comes next, after finishing Tcl.


# Open issues


## General
- handling of '\fB\e\fIk\fR' (see test script)

## oo::... pages
- No handling of the class hierarchy subsection