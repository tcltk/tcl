# Design of Tcl string values for Tcl 9

## DRAFT WORK IN PROGRESS. NOT (YET) NORMATIVE

All Tcl values are strings, but what is a string?

The aim of this document is to spell out what the answer to that question
has been, what it should be, and how we get from one to the other.

## Fundamentals

A ***string*** is a sequence of zero or more symbols, each ***symbol***
a member of a symbol set known as an ***alphabet***.  For example, the
Tcl string **cat** is the sequence of three symbols **c**, **a**, **t**.

Each symbol in the Tcl alphabet is associated with a non-negative integer,
known as the ***code*** of that symbol.  Distinct symbols have distinct
codes.  Schemes that associate symbols with code values are known
as ***character sets***.  For example, in Tcl's character set, the
symbols **c**, **a**, and **t** are associated with code
values 99, 97, and 116 respectively.

There is an ordering imposed on the alphabet by the numeric order of
the associated code values.  This ordering allows symbols to be compared
and sorted.  The natural extension of that ordering to sequences
establishes one well defined way to compare, order and sort string values.

The number of symbols in a string's symbol sequence is the ***length***
of that string.  

A symbol within a string can be identified by its place in the sequence,
counting with an ***index*** that starts at 0.  A string of length *N*
has a symbol at each of the index values 0 through *N*-1.  We can
represent the symbol found at index *i* within string *s* with the
notation *s*[*i*].

Given a string *s* of length *N*, every pair of an index
value *i*, 0 <= *i* <= *N*, and a length *L*, 0 <= *L* <= *N* - *i*,
defines a substring of *s* of length *L* built from the sequence
of symbols *s*[*i*], ..., *s*[*i* + *L* - 1]. These substrings are also
rooted to a particular location in the original string.

Given a string *s* of length *M* and a string *t* of length *N*, the
concatenation of *s* and *t* is the string *u* of length *L* = *M* + *N*,
with *u*[0] = *s*[0], ... *u*[*M* - 1] = *s*[*M* - 1],
*u*[*M*] = *t*[0], ... *u*[*L* - 1] = *t*[*N* - 1].

In all of these descriptions, the string values act in ways with a
direct analog to C arrays of integer values.  This is a familiar collection
of behaviors that a broad collection of programmers can successfully reason
about without extensive training in the complexities of character
sets and encodings.  The basics are accessible to even novice programmers.
*Make the easy things easy; make the harder things possible.*

To be able to create and store an arbitrary string, a Tcl interpreter has
the minimum need for a set of commands or substitutions with the primitive
abilities to create an empty string, to create each string of length 1
(one for each symbol in the alphabet), and the ability to concatenate
arbitrary strings.  Other important primitives are the ability to report
the length of a string, index into a string, take a substring from a
string, and compare symbols and strings.  

## Tcl strings as the universal value set

We most frequently think of strings as a data type with the purpose of
entering, creating, storing, manipulating, processing and producing
text.  In Tcl, though, string values also serve as the universal value
set.  If a value cannot be expressed as a Tcl string, it is not a Tcl value.
The representations of other value sets in Tcl is in large part an exercise
in creating schemes to encode the values of those other value sets in
the form of Tcl strings.  Because of that, we want the set of Tcl strings
to permit encodings of other value sets that are clear, simple, efficient
and convenient.  The history of growing Tcl's alphabet has been driven by
the desire to better provide for the encoding of another value set that
has not easily been accommodated by the legacy alphabet.

## Tcl alphabet versions

This section looks into Tcl's history, which can be tricky.  History gets
messy.  It is full of steps and mis-steps and it can be difficult to get
agreement looking back about which were which.

In Tcl 7, the alphabet for Tcl strings was a set of symbols associated
with code values 1 through 255.  This is exactly the set
of **NUL**-terminated C strings.  The symbols with code values 1 through 127
are defined to follow the ASCII character set.  This is important to the
definition of Tcl because all symbols with syntactic meaning in Tcl scripts
are in the ASCII set.  The symbols with code values 128 through 255 were
less stringently specified, but such symbols could nevertheless be reliably
created, stored, processed and produced by Tcl programs.  This set of
string values continues to have relevance in Tcl today, because it is
exactly this set that can be passed as arguments to Tcl commands defined
via **Tcl_CreateCommand**.

Even though no Tcl 7 value could contain a symbol with code 0, Tcl still
offered a substitution suggesting it was possible.
```
	% string length <\x01>
	3
	% string length <\x00>
	1
```
This seems to be a mis-step, where the \x00 substitution should have
raised an error.  The fact that everything created by it broke expectations
in some way supports that judgment.  But it's not beyond imagination for
someone to take another view.

The value set of arbitrary binary data, in the form of byte sequences,
is not well served by the Tcl 7 alphabet.  No encoding into Tcl 7 strings
can be both simple and efficient.  Either it must be variable-width, or
it must use a fixed width of at least two symbols per byte.  There are
certainly ways to encode arbitrary binary data in ASCII, but the commands
of Tcl 7 never chose one for the core commands of the language to use.
The problem was left unsolved so that a [**read**] from a binary channel returned a value that the rest of Tcl
simply truncated at the first **NUL**.

In Tcl 8.0, the Tcl alphabet added a symbol with code value 0.  This allowed
the simplest coding of *N*-byte sequences by a string of length *N* with
each symbol determined by the code given by the byte value.  Aribtrary
binary data could be stored in Tcl variables, and processed by any
commands created by the new **Tcl_CreateObjCommand**.  Legacy commands
still created by **Tcl_CreateCommand** remained "binary unsafe".

Note that the alphabet strictly grew.  All string values representable
in Tcl 7 remained representable in Tcl 8.0.  Internally there was reform
in representation (counted strings replaced terminated strings), but
the concept of the Tcl string value accessible to scripts changed along
an upward compatible path.

Tcl 8.0 string values suffered from two deficits.  First, The internals
still used two representations that were not reconciled to provide the
same functionality.  Second, the international character sets of 
increasing importance could not be encoded into Tcl string values in
ways that were simple, 


## Representations




