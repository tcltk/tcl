
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.1a1 arises from the check-in with tag `core-9-1-a1`.

Highlighted differences between Tcl 9.1 and Tcl 9.0 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

# New commands and options

- [New options -backslashes, -commands and -variables for subst command](https://core.tcl-lang.org/tips/doc/trunk/tip/712.md)
- [New command unicode for Unicode normalization](https://core.tcl-lang.org/tips/doc/trunk/tip/726.md)

# New public C API

- [Tcl\_IsEmpty checks if the string representation of a value would be the empty string](https://core.tcl-lang.org/tips/doc/trunk/tip/711.md)
- [Tcl\_GetEncodingNameForUser returns name of encoding from user settings](https://core.tcl-lang.org/tips/doc/trunk/tip/716.md)
- [Tcl\_AttemptCreateHashEntry - version of Tcl\_CreateHashEntry that returns NULL instead of panic'ing on memory allocation errors](https://core.tcl-lang.org/tips/doc/trunk/tip/717.md)
- [Tcl\_ListObjRange, Tcl\_ListObjRepeat, Tcl\_TclListObjReverse - C API for new list operations](https://core.tcl-lang.org/tips/doc/trunk/tip/649.md)
- [Tcl\_UtfToNormalized, Tcl\_UtfToNormalizedDString - C API for Unicode normalization](https://core.tcl-lang.org/tips/doc/trunk/tip/726.md)
- [Tcl\_UtfToExternalEx and Tcl\_ExternalToUtfEx - C encoding API supporting output buffers larger than INT_MAX](https://core.tcl-lang.org/tips/doc/trunk/tip/737.md)

# Performance

- [Memory efficient internal representations](https://core.tcl-lang.org/tcl/wiki?name=New+abstract+list+representations)
for list operations on large lists.
- [Continued 64-bit capacity: Command line arguments larger than 2Gb](https://core.tcl-lang.org/tips/doc/trunk/tip/626.md)

# Bug fixes
 - [tclEpollNotfy PlatformEventsControl panics if websocket disconnected](https://core.tcl-lang.org/tcl/tktview/010d8f38)

