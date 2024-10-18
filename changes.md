
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.0.1 arises from the check-in with tag `core-9-0-1`.

## Changes since Tk 9.0.0
 - [zlib-8.8, zlib-8.16 fail on Fedora 40, gcc 14.1.1](https://core.tcl-lang.org/tcl/tktview/73d5cb)

Release Tcl 9.0.0 arises from the check-in with tag `core-9-0-0`.

Highlighted differences between Tcl 9.0 and Tcl 8.6 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

# Major Features

## 64-bit capacity: Data values larger than 2Gb
 - Strings can be any length (that fits in your available memory)
 - Lists and dictionaries can have very large numbers of elements

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
 - Unqualified varnames resolved in current namespace, not global.
   Note that in almost all cases where this causes a change, the change is actually the removal of a latent bug.
 - No `--disable-threads` build option.  Always thread-enabled.
 - I/O malencoding default response: raise error (`-profile strict`)
 - Windows platform needs Windows 7 or Windows Server 2008 R2 or later
 - Ended interpretation of `~` as home directory in pathnames.
   (See `file home` and `file tildeexpand` for replacements when you need them.)
 - Removed the `identity` encoding.
   (There were only ever very few valid use cases for this; almost all uses
   were systematically wrong.)
 - Removed the encoding alias `binary` to `iso8859-1`.
 - `$::tcl_precision` no longer controls string generation of doubles.
   (If you need a particular precision, use `format`.)
 - Removed pre-Tcl 8 legacies: `case`, `puts` and `read` variant syntaxes.
 - Removed subcommands [`trace variable`|`vdelete`|`vinfo`]
 - Removed `-eofchar` option for write channels.
 - On Windows 10+ (Version 1903 or higher), system encoding is always utf-8.
 - `%b`/`%d`/`%o`/`%x` format modifiers (without size modifier) for `format`
   and `scan` always truncate to 32-bits on all platforms.
 - `%L` size modifier for `scan` no longer truncates to 64-bit.
 - Removed command `::tcl::unsupported::inject`.
   (See `coroinject` and `coroprobe` for supported commands with significantly
   more comprehensible semantics.)

## Incompatibilities in C public interface
 - Extensions built against Tcl 8.6 and before will not work with Tcl 9.0;
   ABI compatibility was a non-goal for 9.0.  In _most_ cases, rebuilding
   against Tcl 9.0 should work except when a removed API function is used.
 - Many arguments expanded type from `int` to `Tcl_Size`, a signed integer type
   large enough to support 64-bit sized memory objects.
   The constant `TCL_AUTO_LENGTH` is a value of that type that indicates that
   the length should be obtained using an appropriate function (typically `strlen()` for `char *` values).
 - Ended support for `Tcl_ChannelTypeVersion` less than 5
 - Introduced versioning of the `Tcl_ObjType` struct
 - Removed macros `CONST*`: Tcl 9 support means dropping Tcl 8.3 support.
   (Replaced with standard C `const` keyword going forward.)
 - Removed registration of several `Tcl_ObjType`s.
 - Removed API functions:

     `Tcl_Backslash()`,
     `Tcl_*VA()`,
     `Tcl_*MathFunc*()`,
     `Tcl_MakeSafe()`,
     `Tcl_(Save|Restore|Discard|Free)Result()`,
     `Tcl_EvalTokens()`,
     `Tcl_(Get|Set)DefaultEncodingDir()`,
     `Tcl_UniCharN(case)cmp()`,
     `Tcl_UniCharCaseMatch()`

 - Revised many internals; beware reliance on undocumented behaviors.

# New Features

## New commands
 - `array default` — Specify default values for arrays (note that this alters the behaviour of `append`, `incr`, `lappend`).
 - `array for` — Cheap iteration over an array's contents.
 - `chan isbinary` — Test if a channel is configured to work with binary data.
 - `coroinject`, `coroprobe` — Interact with paused coroutines.
 - `clock add weekdays` — Clock arithmetic with week days.
 - `const`, `info const*` — Commands for defining constants (variables that can't be modified).
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
 - <code>0<i>NNN</i></code> format is no longer octal interpretation. Use <code>0o<i>NNN</i></code>.
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

# Known bugs
 - [changed behaviour wrt command names, namespaces and resolution](https://core.tcl-lang.org/tcl/tktview/f14b33)
 - [windows dos device paths inconsistencies and missing functionality](https://core.tcl-lang.org/tcl/tktview/d8f121)
 - [Temporary folder with file "tcl9registry13.dll" remains after "exit"](https://core.tcl-lang.org/tcl/tktview/6ce3c0)

