# Design of Tcl string values for Tcl 9

## DRAFT WORK IN PROGRESS. NOT (YET) NORMATIVE

All Tcl values are strings, but what is a string?

## Fundamentals

A string is a sequence of zero or more symbols, each symbol a member of
a symbol set known as an alphabet.  For example, the Tcl string **cat**
is the sequence of three symbols **c**, **a**, **t**.

Each symbol in the Tcl alphabet is associated with a non-negative integer.

The number of symbols in a string's symbol sequence is the length
of that string.

A symbol within a string can be identified by its place in the sequence,
counting with an index that starts at 0.  A string of length N has a
symbol at each of the index values 0 through N-1.

## Tcl alphabet versions


