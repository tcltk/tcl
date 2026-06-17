
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.1b0 arises from the check-in with tag `core-9-1-b0`.

Highlighted differences between Tcl 9.1 and Tcl 9.0 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

# New commands and options

- [New options `-backslashes`, `-commands` and `-variables` for `subst` command](https://core.tcl-lang.org/tips/doc/trunk/tip/712.md)
- [New command `unicode` for Unicode normalization](https://core.tcl-lang.org/tips/doc/trunk/tip/726.md)
- [New `timer` command, switch to monotonic clock and microsecond resolution](https://core.tcl-lang.org/tips/doc/trunk/tip/723.md)
- [Remove `expr` behavior from `lseq`](https://core.tcl-lang.org/tips/doc/trunk/tip/746.md) *Incompatibility*
- [File paths are now treated as case-insensitive on MacOS](https://core.tcl-lang.org/tcl/tktview/e6ca0b1b) *Incompatibility*
- New command `tcl::registry` as a synonym for the `registry` command without needing
the registry package to be loaded. The registry module is now part of the core
Tcl DLL in all build configurations.
- [New `lfilter` command for selecting items from a list](https://core.tcl-lang.org/tips/doc/trunk/tip/735.md)
- [Many new functions from C99](https://core.tcl-lang.org/tips/doc/trunk/tip/745.md), specifically: `acosh()`, `asinh()`, `atanh()`, `cbrt()`, `copysign()`, `dim()`,  `erf()`, `erfc()`, `exp2()`, `expm1()`, `fma()`, `gamma()`, `ldexp()`, `lgamma()`, `log1p()`, `log2()`, `logb()`, `nextafter()`, `remainder()`, `signbit()`, and `trunc()`, and (for functions that return multiple values in their C99 API) the commands: `divmod`, `frexp`, `modf`, and `remquo`. (See [this page](https://en.cppreference.com/w/c/numeric/math.html) for more information about these functions; the Tcl functions are _intentionally_ only thin wrappers around the functions in the C99 standard.)
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

# Changes in interpreter initialization

- [Custom applications must call Tcl\_FindExecutable or TclZipfs_AppHook to initialize Tcl](https://core.tcl-lang.org/tips/doc/trunk/tip/732.md) *Potential incompatibility*
- [Search path for locating Tcl core script and encodings is changed](https://core.tcl-lang.org/tips/doc/trunk/tip/732.md) *Potential incompatibility*

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
 - Unicode 18.0.0 (draft)
