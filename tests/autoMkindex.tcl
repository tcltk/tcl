# Test file for:
#   auto_mkindex
#
# This file provides example cases for testing the Tcl autoloading
# facility.  Things are much more complicated with namespaces and classes.
# The "auto_mkindex" facility can no longer be built on top of a simple
# regular expression parser.  It must recognize constructs like this:
#
#   namespace eval foo {
#       proc test {x y} { ... }
#       namespace eval bar {
#           proc another {args} { ... }
#       }
#   }
#
# Note that procedures and itcl class definitions can be nested inside
# of namespaces.
#
# Copyright (c) 1993-1998  Lucent Technologies, Inc.

# This shouldn't cause any problems
namespace import -force blt::*

# Should be able to handle "proc" definitions, even if they are
# preceded by white space.

proc normal {x y} {return [expr $x+$y]}
  proc indented {x y} {return [expr $x+$y]}

#
# Should be able to handle proc declarations within namespaces,
# even if they have explicit namespace paths.
#
namespace eval buried {
    proc inside {args} {return "inside: $args"}

    namespace export pub_*
    proc pub_one {args} {return "one: $args"}
    proc pub_two {args} {return "two: $args"}
}
proc buried::within {args} {return "within: $args"}

namespace eval buried {
    namespace eval under {
        proc neath {args} {return "neath: $args"}
    }
    namespace eval ::buried {
        proc relative {args} {return "relative: $args"}
        proc ::top {args} {return "top: $args"}
        proc ::buried::explicit {args} {return "explicit: $args"}
    }
}
