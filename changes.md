
The source code for Tcl is managed by fossil.  Tcl developers coordinate all
changes to the Tcl source code at

> [Tcl Source Code](https://core.tcl-lang.org/tcl/timeline)

Release Tcl 9.0.2 arises from the check-in with tag `core-9-0-2`.

Tcl patch releases have the primary purpose of delivering bug fixes
to the userbase.

# Bug fixes
 - Better error-message than "interpreter uses an incompatible stubs mechanism"](https://core.tcl-lang.org/tcl/tktview/fc3509)

# Incompatibilities
 - No known incompatibilities with the Tcl 9.0.0 public interface.

# Updated bundled packages, libraries, standards, data
 - sqlite3 3.48.0

Release Tcl 9.1a0 arises from the check-in with tag core-9-1-a0.

Highlighted differences between Tcl 9.1 and Tcl 9.0 are summarized below,
with focus on changes important to programmers using the Tcl library and
writing Tcl scripts.

## Continued 64-bit capacity: Command line arguments larger than 2Gb


