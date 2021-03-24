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
codes.  Said another way, two symbols associated with the same code are
equivalent.  Schemes that associate symbols with code values are known
as ***character sets***.  For example, in Tcl's character set, the
symbols **c**, **a**, and **t** are associated with code
values 99, 97, and 116 respectively.

There is an ordering imposed on the alphabet by the numeric order of
the associated code values.  This ordering allows symbols to be compared
and sorted.  The natural extension of that ordering to sequences
establishes one well-defined way to compare, order and sort string values.

The number of symbols in a string's symbol sequence is the ***length***
of that string.  

A symbol within a string can be identified by its place in the sequence,
counting with an integer ***index*** that starts at 0.  A string of length *N*
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

## Implementations

The fundamentals above describe Tcl's value strings at an abstract level,
but to make Tcl interpreters and Tcl libraries we have to create programs 
that exhibit the described abstract behviors.  We will have much more
detailed things to say about string representations later, but for now
suffice it to say that a proper implementation of the string abstraction
needs to reproduce all the fundamentals faithfully.  If a representation
is used that allows multiple representations of a single symbol, or
multiple representations of a single string, this can be a matter of
some difficulty.  Representations that include the possibility of states
that are not valid string values at all are also cases that need
careful handling.

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
This seems to be a mis-step, where the \\x00 substitution should have
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

Note that the alphabet strictly grew between Tcl 7 and Tcl 8.0.  All string
values representable in Tcl 7 remained representable in Tcl 8.0.  Internally
there was reform in representation (counted strings replaced terminated
strings), but the concept of the Tcl string value accessible to scripts
changed along an upward compatible path.

Tcl 8.0 string values suffered from two deficits.  First, The internals
still used two representations that were not reconciled to provide the
same functionality.  Second, the international character sets of 
increasing importance could not be encoded into Tcl string values in
ways that were both simple and efficient.  International character set text
became the next value set prompting an expansion of Tcl's alphabet.

In Tcl 8.1 (released April, 1999), the Tcl alphabet expanded to a set of
symbols with code values ranging from 0 to 65,535 (0x0000 to 0xFFFF).
Where a code value had an assigned symbol in Unicode 2.0, the symbol of
Tcl's alphabet agreed with the Unicode character set.  This implied
continued symbol agreement with ASCII.  All assigned codepoints of
Unicode 2.0 fit in this alphabet.  Tcl could store all Unicode text values
in a simple and efficient way in its string values.  It could perhaps be
said that Tcl 8.1 strings *were* Unicode 2.0 text values. The Tcl
documentation is certainly phrased in those terms.  In hindsight, given
the later divergence in the development of Tcl and Unicode, that's
not the most useful perspective.  It is more useful to think of the set 
of Tcl 8.1 strings as a superset of the set of Unicode 2.0 text values.
This allowed for the accommodation of continued assignment of unassigned
code values in later releases of the Unicode standard.  It also best
accounts for the reality that the Unicode standards became more clear
and explicit and restrictive in their conformance demands over time.

Tcl 8.1 added a new backslash substitution syntax, **\\u**___HHHH___,
capable of producing every symbol in the Tcl alphabet.  Built-in Tcl
commands such as **format** and **scan** were extended to produce and
accept symbols and codes in the extended alphabet.  Once again the alphabet
strictly grew, preserving upward compatibility in the set of string values
available to Tcl programs.

The representations of strings inside the Tcl 8.1 library were 
substantially reformed to support the extended alphabet.  These
representations appeared in parts of the C programming interface
of the Tcl library, so extensions written for Tcl 7 or Tcl 8 had
to be adapted to the reforms.

Tcl 8.1 string values did bring with them a new mis-step.  A new
command **encoding** provided scripts with the ability to transform
Unicode text values into a variety of encoded forms, typically to
process values near I/O operations.  This **encoding** command
included support for an encoding called **identity** that empowers
scripts to store an arbitrary byte sequence inside an internal
representation for Tcl strings.  The consequence is that scripts
can create multiple representations for the same string that are not
consistently treated as the same string, contrary to our fundamental
expectations.
```
	% info patch
	8.1.1
	% set s \u0080
	
	% set t [encoding convertfrom identity \x00]
	??
	% string length $s
	1
	% string length $t
	1
	% scan [string index $s 0] %c code; set code
	128
	% scan [string index $t 0] %c code; set code
	128
	% string equal $s $t
	0
	% string bytelength $s
	2
	% string bytelength $t
	1
```
This example also demonstrates the mis-step of **string bytelength**
that exposes differences in internal representation of values that
should be treated as equivalent.

(As of this writing, TIP 345 has purged the **identity** encoding
from the development of Tcl 8.7, but **string bytelength** is still
present in current snapshots of Tcl 9.0.)

While scripts can avoid use of the **identity** encoding (perhaps treating
any use of it as introducing *undefined* behavior), the underlying
implementation flaws that allow for its mischief are available to all
Tcl extensions, so the failure of Tcl 8.1 strings to fully conform
to fundamental expectations still lurks.  More on this when we 
examine representations below.

Tcl 8.2 (released August, 1999) kept the same string alphabet, but
revised all relevant commands to process symbols according to the
Unicode 2.1 standard.










## Representations




