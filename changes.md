
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.0b3 arises from the check-in with tag core-9-0-b3.

Highlighted differences between Tcl 9.0 and Tcl 8.6 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

## 64-bit capacity: Data values larger than 2Gb

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
 - Unqualified varnames resolved in current namespace, not global.
 - No --disable-threads build option.  Always thread-enabled.
 - I/O malencoding default response: raise error (-profile strict)
 - Windows platform needs Windows 7 or Windows Server 2008 R2 or later
 - Ended interpretation of ~ as home directory in pathnames
 - Removed the "identity" encoding.
 - Removed the encoding alias "binary" to "iso8859-1".
 - $::tcl_precision no longer controls string generation of doubles
 - Removed Tcl 7 legacies: [case], [puts] [read] variant syntaxes
 - Removed subcommands [trace variable|vdelete|vinfo]
 - No -eofchar option for channels anymore for writing.
 - On Windows 10+ (Version 1903 or higher), system encoding is always utf-8.
 - %b/%d/%o/%x format modifiers (without size modifier) for "format"
   and "scan" always truncate to 32-bits on all platforms.
 - %L size modifier for "scan" no longer truncates to 64-bit.
 - Removed command ::tcl::unsupported::inject.

## Incompatibilities in C public interface
 - Many arguments expanded type from int to Tcl_Size
 - Ended support for Tcl_ChannelTypeVersion less than 5
 - Introduced versioning of the Tcl_ObjType struct
 - Removed macros CONST*: Tcl 9 support means dropping Tcl 8.3 support
 - Removed routines:
>    Tcl_Backslash(), Tcl_*VA(), Tcl_*MathFunc*(), Tcl_MakeSafe(),
>    Tcl_(Save|Restore|Discard|Free)Result(), Tcl_EvalTokens(),
>    Tcl_(Get|Set)DefaultEncodingDir(),
>    Tcl_UniCharN(case)cmp(), Tcl_UniCharCaseMatch()

## New commands
 - `array default`, `array for`
 - `chan isbinary`
 - `coroinject`, `coroprobe`
 - `clock add weekdays`
 - `const`, `info const*`
 - `dict getwithdefault`
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
 - `readFile`, `writeFile`, `foreachLine`

## New command options
 - `regsub ... -command ...`
 - `lsearch ... -stride ...`
 - `clock scan ... -validate ...`
 - `socket ... -nodelay ... -keepalive ...`
 - `vwait` controlled by several new options

## Numbers
 - 0NNN format is no longer octal interpretation. Use 0oNNN.
 - 0dNNNN format to compel decimal interpretation.
 - NN_NNN_NNN, underscores in numbers for optional readability
 - Functions: isinf() isnan() isnormal() issubnormal() isunordered()
 - `fpclassify`
 - Function int() no longer truncates to word size

## tcl::oo facilities
 - private variable and methods
 - `method -export`, `method -unexport`

