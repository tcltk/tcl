
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 8.7b1 arises from the check-in with tag core-8-7-b1 (not released yet).

Highlighted differences between Tcl 8.7 and Tcl 8.6 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

## Internationalization of text
 - Full Unicode range of codepoints
 - New encodings: `utf-16`/`utf-32`/`ucs-2`(`le`|`be`), `CESU-8`, etc.
 - `encoding` options `-profile`, `-failindex` manage encoding of I/O.
 - `msgcat` supports custom locale search list
 - `source` defaults to `-encoding utf-8`

## Zip filesystems and attached archives.
 - Packaging of the Tcl script library with the Tcl binary library,
   meaning that the `TCL_LIBRARY` environment variable is usually not required.
 - Packaging of an application into a virtual filesystem is now a supported
   core Tcl feature.

## Unix notifiers available using `epoll()` or `kqueue()`
 - This relieves limits on file descriptors imposed by legacy `select()` and fixes a performance bottleneck. 

# Incompatibilities

## Notable incompatibilities
 - No `--disable-threads` build option.  Always thread-enabled.

# New Features

## New commands
 - `array default` — Specify default values for arrays (note that this alters the behaviour of `append`, `incr`, `lappend`).
 - `array for` — Cheap iteration over an array's contents.
 - `chan isbinary` — Test if a channel is configured to work with binary data.
 - `coroinject`, `coroprobe` — Interact with paused coroutines.
 - `clock add weekdays` — Clock arithmetic with week days.
 - `dict getwithdefault` — Define a fallback value to use when `dict get` would otherwise fail.
 - `file home` — Get the user home directory.
 - `file tempdir` — Create a temporary directory.
 - `file tildeexpand` — Expand a file path containing a `~`.
 - `info commandtype` — Introspection for the kinds of commands.
 - `ledit` — Equivalent to `lreplace` but on a list in a variable.
 - `lpop` — Remove an item from a list in a variable.
 - `lremove` — Remove a sublist from a list in a variable.
 - `lseq` — Generate a list of numbers in a sequence.
 - `package files` — Describe the contents of a package.
 - `string insert` — Insert a string as a substring of another string.
 - `string is dict` — Test whether a string is a dictionary.
 - `tcl::process` — Commands for working with subprocesses.
 - `*::build-info` — Obtain information about the build of Tcl.
 - `readFile`, `writeFile`, `foreachLine` — Simple procedures for basic working with files.
 - `tcl::idna::*` — Commands for working with encoded DNS names.

## New command options
 - `chan configure ... -inputmode ...` — Support for raw terminal input and reading passwords.
 - `clock scan ... -validate ...`
 - `info loaded ... ?prefix?`
 - `lsearch ... -stride ...` — Search a list by groups of items.
 - `regsub ... -command ...` — Generate the replacement for a regular expression by calling a command.
 - `socket ... -nodelay ... -keepalive ...`
 - `vwait` controlled by several new options
 - `expr` string comparators `lt`, `gt`, `le`, `ge`
 - `expr` supports comments inside expressions

## Numbers
 - <code>0d<i>NNNN</i></code> format to compel decimal interpretation.
 - <code>NN_NNN_NNN</code>, underscores in numbers for optional readability
 - Functions: `isinf()`, `isnan()`, `isnormal()`, `issubnormal()`, `isunordered()`
 - Command: `fpclassify`
 - Function `int()` no longer truncates to word size

## TclOO facilities
 - private variables and methods
 - class variables and methods
 - abstract and singleton classes
 - configurable properties
 - `method -export`, `method -unexport`
