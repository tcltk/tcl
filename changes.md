
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 8.7b1 arises from the check-in with tag core-8.7-b1.

Highlighted differences between Tcl 8.7 and Tcl 8.6 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

## Internationalization of text
 - Full Unicode range of codepoints
 - New encodings: utf-16/utf-32/ucs-2(le|be), CESU-8, etc.
 - `encoding` options -profile, -failindex manage encoding of I/O.
 - `msgcat` supports custom locale search list
 - `source` defaults to -encoding utf-8

## Zip filesystems and attached archives.

## Unix notifiers available using epoll() or kqueue()
 - relieves limits on file descriptors imposed by legacy select()

## Notable incompatibilities
 - No --disable-threads build option.  Always thread-enabled.
 - Windows platform needs Windows 7 or Windows Server 2008 R2 or later

## New commands
 - `array default`, `array for`
 - `coroinject`, `coroprobe`
 - `clock add weekdays`
 - `dict getdefault`
 - `file tempdir`, `file home`, `file tildeexpand`
 - `info commandtype`
 - `ledit`
 - `lpop`
 - `lremove`
 - `lseq`
 - `package files`
 - `string insert`, `string is dict`
 - `tcl::process`
 - `*::build-info`

## New command options
 - `regsub ... -command ...`
 - `lsearch ... -stride ...`
 - `clock scan ... -validate ...`
 - `socket ... -nodelay ... -keepalive ...`
 - `vwait` controlled by several new options

## Numbers
 - 0dNNNN format to compel decimal interpretation.
 - NN_NNN_NNN, underscores in numbers for optional readability
 - Functions: isinf() isnan() isnormal() issubnormal() isunordered()
 - `fpclassify`
 - Function int() no longer truncates to word size

## tcl::oo facilities
 - private variable and methods
 - `method -export`, `method -unexport`

