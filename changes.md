
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.1.1 arises from the check-in with tag `core-9-1-1`.

Tcl patch releases have the primary purpose of delivering bug fixes
to the userbase.

# Bug fixes
 - [regexp sometimes allows an invalid backref to be matched](https://core.tcl-lang.org/tcl/tktview/75c9d9)
 - [regexp -all did not try to match at the end of the string](https://core.tcl-lang.org/tcl/tktview/b73f12)
 - [regexp/regsub -all restarted matching on the substring after the previous match](https://core.tcl-lang.org/tcl/tktview/471101)
 - [with REG_BOSONLY only the first alternative was anchored at the beginning of the string](https://core.tcl-lang.org/tcl/tktview/117854)
 - [TclUnixWaitForFile: infinite wait may return early](https://core.tcl-lang.org/tcl/tktview/49538c)
 - [lsearch -regexp released the pattern when matching failed](https://core.tcl-lang.org/tcl/tktview/359576)
 - [restore the blocking mode when "socket -async" connects at once](https://core.tcl-lang.org/tcl/tktview/d22dc7)
 - [compiled [clock clicks -mi] should give "ambiguous option" error](https://core.tcl-lang.org/tcl/tktview/1ae976)
 - [writing a file beyond the file size limit (ulimit -f) killed the process](https://core.tcl-lang.org/tcl/tktview/386563)
 - [Incorrect argument handling by [regexp]](https://core.tcl-lang.org/tcl/tktview/697b1b)
 - [compiled regexp behaves differently](https://core.tcl-lang.org/tcl/tktview/190289)
 - [a {1,1} quantifier was always dropped as vacuous](https://core.tcl-lang.org/tcl/tktview/524b35)
 - [regexp with backreferences could loop forever](https://core.tcl-lang.org/tcl/tktview/a6029e)
 - [remove empty directories from the zipfs VFS with POSIX find options](https://core.tcl-lang.org/tcl/tktview/9336f1)
 - [tailcall panicked in a command called directly by its objProc](https://core.tcl-lang.org/tcl/tktview/4955f5)
 - [compiled [self n] should be ambiguous](https://core.tcl-lang.org/tcl/tktview/8ab06e)
 - [difference between bcc'ed and eval'ed regexp](https://core.tcl-lang.org/tcl/tktview/306945)
 - [regsub always asked the regexp engine for 10 submatches](https://core.tcl-lang.org/tcl/tktview/446565)
 - [regsub -command panics with more than 9 capturing groups](https://core.tcl-lang.org/tcl/tktview/41bc9a)
 - [Problem with ::http::OpenSocket](https://core.tcl-lang.org/tcl/tktview/0c1e91)
 - [on Windows a joinable thread which exits before it is joined leaked](https://core.tcl-lang.org/tcl/tktview/338137)
 - [on Unix, Tcl_JoinThread() should not call pthread_join() for detached threads](https://core.tcl-lang.org/tcl/tktview/5974ff)
 - [zlib-8.3 and zlib-9.3 connected to localhost](https://core.tcl-lang.org/tcl/tktview/81cb2b)
 - [zipfs channels never generated file events](https://core.tcl-lang.org/tcl/tktview/0654ce)
 - [interp create resets the system encoding set by encoding system](https://core.tcl-lang.org/tcl/tktview/63941f)
 - [traceback using clock format with locale of he_IL](https://core.tcl-lang.org/tcl/tktview/301613)
 - [he locale: BCE and CE era names are swapped](https://core.tcl-lang.org/tcl/tktview/58bae8)
 - [format %A garbles negative, padded and infinite values](https://core.tcl-lang.org/tcl/tktview/2dff98)
 - [socket-2.12 failed on OpenBSD](https://core.tcl-lang.org/tcl/tktview/e99187)
 - [do not throw from [socket -async] when the connect fails at once](https://core.tcl-lang.org/tcl/tktview/59e7e7)
 - [test portability for illumos: skip socket-14.19 where SO_REUSEPORT is not supported](https://core.tcl-lang.org/tcl/tktview/dbf988)
 - [lset with several indices on an abstract list (lseq) used a freed sublist](https://core.tcl-lang.org/tcl/tktview/526b01)
 - [lset errors during appending operation left original list modified](https://core.tcl-lang.org/tcl/tktview/301934)
 - [http.test: use a port just released by the OS for badurl](https://core.tcl-lang.org/tcl/tktview/5350f3)
 - [the regexp engine could overflow the C stack on deeply nested REs](https://core.tcl-lang.org/tcl/tktview/7d47ba)
 - [[clock scan] put the half-hour time zones 30 minutes off](https://core.tcl-lang.org/tcl/tktview/e580d8)
 - [safe interp got the parent's -errorinfo and -errorstack through a failing alias](https://core.tcl-lang.org/tcl/tktview/a63f5a)
 - [threaded allocator could not be disabled with -DUSE_THREAD_ALLOC=0 or nmake OPTS=tclalloc](https://core.tcl-lang.org/tcl/tktview/31294d)
 - [win/configure: --with-system-zlib and --with-system-libtommath](https://core.tcl-lang.org/tcl/tktview/ac3ea8)
 - [Windows: make distclean left tcl.pc behind](https://core.tcl-lang.org/tcl/tktview/59a9b9)
 - [Windows: native MSYS2/Cygwin builds no longer need an installed tclsh](https://core.tcl-lang.org/tcl/tktview/2ef343)
 - [build failure on Alpine Linux (musl): no sys/queue.h](https://core.tcl-lang.org/tcl/tktview/3ff2d7)
 - [exec hangs, iff stdin & stdout both were previously closed.](https://core.tcl-lang.org/tcl/tktview/21dbc6)

# Updated bundled packages, libraries, standards, data
 - http 2.10.3
 - msgcat 1.7.2
 - tcl::idna 1.0.2
 - tzcode 2026e

# Updated bundled packages, libraries, standards, data

Release Tcl 9.1.0 arises from the check-in with tag `core-9-1-0`.

Highlighted differences between Tcl 9.1 and Tcl 9.0 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

# Incompatibilities

- [Remove `expr` behavior from `lseq`](https://core.tcl-lang.org/tips/doc/trunk/tip/746.md) *Incompatibility*
- [File paths are now treated as case-insensitive on MacOS](https://core.tcl-lang.org/tcl/tktview/e6ca0b1b) *Incompatibility*
- New command `tcl::registry` as replacement for the `registry` command without needing
the registry package to be loaded. The registry module is now part of the core
Tcl DLL in all build configurations.
- [Custom applications must call Tcl\_FindExecutable or TclZipfs_AppHook to initialize Tcl](https://core.tcl-lang.org/tips/doc/trunk/tip/732.md) *Potential incompatibility*
- [Search path for locating Tcl core script and encodings is changed](https://core.tcl-lang.org/tips/doc/trunk/tip/732.md) *Potential incompatibility*

# New commands and options

- [New options `-backslashes`, `-commands` and `-variables` for `subst` command](https://core.tcl-lang.org/tips/doc/trunk/tip/712.md)
- [New command `unicode` for Unicode normalization](https://core.tcl-lang.org/tips/doc/trunk/tip/726.md)
- [New `timer` command, switch to monotonic clock and microsecond resolution](https://core.tcl-lang.org/tips/doc/trunk/tip/723.md)
- [New `lfilter` command for selecting items from a list](https://core.tcl-lang.org/tips/doc/trunk/tip/735.md)
- [Many new functions from C99](https://core.tcl-lang.org/tips/doc/trunk/tip/745.md), specifically: `acosh()`, `asinh()`, `atanh()`, `cbrt()`, `copysign()`, `dim()`,  `erf()`, `erfc()`, `exp2()`, `expm1()`, `fma()`, `gamma()`, `ldexp()`, `lgamma()`, `log1p()`, `log2()`, `logb()`, `nextafter()`, `remainder()`, `signbit()`, and `trunc()`, and (for functions that return multiple values in their C99 API) the commands: `divmod`, `frexp`, `modf`, and `remquo`. (See [this page](https://en.cppreference.com/w/c/numeric/math.html) for more information about these functions; the Tcl functions are _intentionally_ only thin wrappers around the functions in the C99 standard.)
- [New `switch` option `-integer` to compare values as integers](https://core.tcl-lang.org/tips/doc/trunk/tip/730.md)
- [Reading and writing of child interpreter variables](https://core.tcl-lang.org/tips/doc/trunk/tip/728.md)
- [Updated Tcl Bytecode opcodes](https://core.tcl-lang.org/tips/doc/trunk/tip/720.md)
- [Add support for `\z` in regex as end-of-string anchor](https://core.tcl-lang.org/tcl/info/fbc56b259e)

- New `tcltest::configure` option `-iterations` to control number of iterations of each test.

# New public C API

- [`Tcl_IsEmpty()` &mdash; checks if the string representation of a value would be the empty string](https://core.tcl-lang.org/tips/doc/trunk/tip/711.md)
- [`Tcl_GetEncodingNameForUser()` &mdash; returns name of encoding from user settings](https://core.tcl-lang.org/tips/doc/trunk/tip/716.md)
- [`Tcl_AttemptCreateHashEntry()` &mdash; version of `Tcl_CreateHashEntry()` that returns NULL instead of panic'ing on memory allocation errors](https://core.tcl-lang.org/tips/doc/trunk/tip/717.md)
- [`Tcl_ListObjRange()`, `Tcl_ListObjRepeat()`, `Tcl_ListObjReverse()` &mdash; C API for new list operations](https://core.tcl-lang.org/tips/doc/trunk/tip/649.md)
- [`Tcl_UtfToNormalized()`, `Tcl_UtfToNormalizedDString()` &mdash; C API for Unicode normalization](https://core.tcl-lang.org/tips/doc/trunk/tip/726.md)
- [`Tcl_UtfToExternalEx()` and `Tcl_ExternalToUtfEx()` &mdash; C encoding API supporting output buffers larger than INT_MAX](https://core.tcl-lang.org/tips/doc/trunk/tip/737.md)
- [New API for monotonic clock and microseconds resolution](https://core.tcl-lang.org/tips/doc/trunk/tip/723.md)
- [Windows `auto_execok` enhancements and `exec` search reform](https://core.tcl-lang.org/tips/doc/trunk/tip/753.md)
- [New timer API using long long in stead of Tcl_Time](https://core.tcl-lang.org/tips/doc/trunk/tip/752.md)
- [`Tcl_RegisterPostInitProc` callback for post-initialization of interpreters](https://core.tcl-lang.org/tips/doc/trunk/tip/755.md)

# Performance

- [Memory efficient internal representations](https://core.tcl-lang.org/tcl/wiki?name=New+abstract+list+representations)
for list operations on large lists.
- [Continued 64-bit capacity: Command line arguments larger than 2Gb](https://core.tcl-lang.org/tips/doc/trunk/tip/626.md)
- [Support for long paths on Windows](https://core.tcl-lang.org/tips/doc/trunk/tip/744.md)
- [Faster UTF-8 encoding and I/O](https://core.tcl-lang.org/tcl/wiki?name=Faster+UTF+encoding)
- Faster interpreter creation

# Bug fixes
- [Inconsistent `glob` matching on MacOS](https://core.tcl-lang.org/tcl/tktview/e6ca0b1b)
- [`file normalize` does not uniquely identify a file on MacOS](https://core.tcl-lang.org/tcl/tktview/10890417)
- [Inconsistencies between `cd` and `file normalize` for volume-relative paths on Windows](https://core.tcl-lang.org/tcl/tktview/bca391ab)
- [Tcl cannot run from an arbitrary build directory with --disable-zipfs](https://core.tcl-lang.org/tcl/tktview/006bef5d)
- [Crash on finalization with an active thread running.](https://core.tcl-lang.org/tcl/tktview/e55a589d)
- [Crash on use-after-free in Windows socket thread.](https://core.tcl-lang.org/tcl/tktview/06f19cc4)

# Updated bundled packages, libraries, standards, data
 - Unicode 18.0.0
 - platorm 1.2.0: Add support for musl C runtime
