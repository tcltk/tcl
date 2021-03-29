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
careful consideration.

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
certainly ways to encode arbitrary binary data using only the Tcl 7 alphabet,
but the commands of Tcl 7 never chose one for the core commands of the
language to use.  The problem was left unsolved so that a [**read**] from a
binary channel returned a value that the rest of Tcl simply truncated at
the first **NUL**.

In Tcl 8.0, the Tcl alphabet added a symbol with code value 0.  This allowed
the direct encoding of a byte sequences of length *N* by a string of
length *N* with each symbol determined by the code given by the byte value.
Aribtrary binary data could be stored in Tcl variables, and processed by any
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
This set of string values came to be known as UCS-2 to distinguish it
from later versions of Unicode.

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
Unicode text values into a variety of encoded forms.  This **encoding**
command included support for an encoding called **identity** that
empowers scripts to store an arbitrary byte sequence inside an internal
representation for Tcl strings.  The consequence is that scripts
can create multiple representations for the same string that are not
consistently treated as the same string, contrary to our fundamental
expectations.
```
	% info patch
	8.1.1
	% set s \u0080
	
	% set t [encoding convertfrom identity \x80]
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
Unicode 2.1 standard.  Notably this included the assignment of
code point **U+20AC** to the symbol EURO SIGN.  The Tcl 8.3.* series
of releases (February 2000 - October 2002) continued that same standard
of Unicode support.

From Chapter 1 of *The Unicode Standard, Version 2.0*,
"**The Unicode Standard is a fixed-width, uniform encoding scheme for written characters and text.**"
Chapter 2 presents a set of desgin principles.  The first principle states,
in part,
"**Plain Unicode text consists of pure 16-bit Unicode character sequences.**"
The second principle states 
"**The full 16-bit codespace... is available to represent characters.**"
A plain reading of those designs and assurances suggests that sequences
of symbols from a 16-bit alphabet will be a suitable fixed-width
representation for Unicode text.  The second design principle, though, goes
on to describe a "*surrogate extension mechanism*" which uses a pair of
16-bit code values to represent a single character.  This mechanism, of
course, makes the specified Unicode definition a variable-width encoding
of abstract characters, not a fixed-width encoding at all.

That said, Unicode 2.0, does specify surrogate pairs as the way to
represent a full collection of 1,114,112 distinguishable symbols
in the potential Unicode collection.  It also reserves the surrogate
code points to be used properly in Unicode text only in the formation
of such pairs.  It did not assign any characters to those pairs, instead
describing the mechanism as one "for encoding extremely rare characters".
Tcl string values processed symbols corresponding to Unicode surrogates
no differently from any other symbols in the Tcl alphabet.  In this state
of things, it is clear that the set of Tcl strings is strictly a superset
of the set of well-formed Unicode text.  This is not only because of
unassigned codepoints awaiting their symbols, but includes an ability
to store symbol sequences in Tcl string values that can never become
well-formed Unicode text at any point in the future.

Unicode 3.1.0 was released March 2001.  This was the first version
of the Unicode Standard that assigned characters to surrogate code
pairs in the 16-bit encoding, which in Unicode 3 came to have the 
name UTF-16.  In Unicode 3 it was acknowledged that UTF-16 is a
variable-length encoding.  Tcl's source code was updated to support
this specification of Unicode in May 2001, but support for surrogate
pairs to represent Unicode characters with codes greater than **U+FFFF**
was not implemented.  This was when Tcl Unicode support broke away
from conformance to the Unicode Standard.  Many programming assumptions
rooted in the existence of a fixed-width, 16-bit encoding for every
Tcl string value had become deeply embedded in both Tcl's implementation
and in some of its interfaces.  The assigned Unicode characters to be gained
outside the Basic Multilingual Place remained those
of "extremely rare" interest.  This level of partial Unicode 3.1.0
support was first released in Tcl 8.4.0 in September 2002.

And then Tcl's Unicode support fell into a deep sleep.

While Tcl's support of Unicode slept, Unicode itself evolved and
had the language of its conformance standards tightened and refined.
The conception of Unicode defined as seqeunces of 16-bit code units
faded away, and the 16-bit system, UTF-16, became just one of several
encodings to be used.  Fundamentally each distinguishable Unicode text
that exists *or that ever will exist under future revisions of Unicode*
became defined as a sequence of zero or more symbols from the
alphabet of ***unicode scalar values***.  A unicode scalar value is
associated in the Unicode character set with a code value that can
be expressed as a 21-bit integer, but also constrained to exclude
the values associate with the surrogate extension mechanism.  The
code values of unicode scalar values are in the integer ranges
0 to 55,295 (0x0000 to 0xD7FF) and 57,344 to 1,114,111 (0xE000 to 0x10FFFF).
From this perspective, the Tcl 8.1 string values were no longer seen as
proper Unicode, but as a legacy system called UCS-2 which was flaws in
two ways.  It lacked support for the supplemntary planes of the full
Unicode character set beyond **U+FFFF**.  It also allowed for the
presence of symbols with codes between 55,296 and 57,343 (0xD800 to 0xDFFF).
Neither system could encode the other in direct, simple, efficient
representations.

Unicode also came to define a collection of encodings.  The simplest to
understand is UTF-32, where each unicode scalar value in a Unicode sequence
is directly represented by a 4-octet (32-bit) value matching the code.
UTF-32 offers simplicity and fixed-width representation for efficient
random access indexing.  It also offers considerable waste of storage
and transmission.  Properly defined, UTF-32 does not include representations
of symbols that are not unicode scalar values.  However, the extension
to include them is not difficult to imagine.  The name UCS-4 comes down
from the ISO 10646 effort, and has evolved to mean the same thing as UTF-32.

The UTF-16 encoding uses 2-octet (16-bit) code units in variable length
patterns to represent each unicode scalar value.  The Unicode scalar
values from ranges 0x0000 - 0xD7FF and 0xE000 - 0xFFFF are represented by
themselves in a single code unit.  The Unicode scalar values from the
range 0x10000 - 0x10FFFF are each represented by a pair of 16-bit code
units, the first from the range 0xD800 - 0xDBFF and the second from the
range 0xDC00 - 0xDFFF.  Every 2-octet value may appear somewhere in the
proper UTF-16 encoding of some Unicode text.  There are no code unit
values that are forbidden.  There are UCS-2 sequences that are not
valid UTF-16, precisely those UCS-2 sequences that include surrogates
not arranged in properly formed pairs.  

Note the difficulties if we try to design an encoding with 2-octet code
units to encode the union of UCS-2 and Unicode.  This can certainly
be done, but the result will bear little resemblence to UTF-16.  All
valid UTF-16 sequences are used up representing valid Unicode.  To also
represent the strings of UCS-2 that are not valid Unicode, we would need to
use 2-octet sequences that are not valid UTF-16.  Any such scheme will
have some point of discontinuity with the legacy UCS-2 system.  Likewise,
since every 2-octet code unit sequence is used in UCS-2 to represent
itself, there is no room to create representation for supplemental
planes of unicode in a 2-octet encoding without introducing a discontinuity.
There will have to be some 2-octet sequence that used to mean one thing
and now means another thing at the transition, or there will have to
be some approach of preserving both systems and taking care to distinguish
at each point which is in use.  This sticking point is a major difficulty
in managing a migration in Tcl's Unicode support, since public interfaces
exist that transfer 2-octet encoded data.

Unicode also defines the UTF-8 encoding of unicode scalar values into
single-octet (8-bit) (byte-oriented) code units in variable length
patterns.  The Unicode scalar values from the range 0x0000 - 0x007F
are represented by themselves in a single code unit. Each value from the
range 0x0080 - 0x07FF is represented by a two-byte sequence of a leading
byte from the range 0xC2 - 0xDF and a trailing byte from the
range 0x80 - 0xBF.  Each value from the ranges 0x0800 - 0xD7FF and
0xE000 - 0xFFFF is represented by a three-byte sequence starting
with a leading byte from the range 0xE0 - 0xEF and two trailing bytes
as before. (Some such three-byte sequences would encode surrogates,
and are therefore not valid UTF-8 sequences).  Each value from the
range 0x10000 - 0x10FFFF is represented by a four-byte sequence
starting with a leading byte from the range 0xF0 - 0xF4 and three trailing
bytes as before. (Some such four-byte sequences would decode to a
codepoint greater than 0x10FFF, and are therefore not valid UTF-8 sequences).
Note the implication that the bytes 0xC0, 0xC1, and 0xF5 - 0xFF can never
appear in a proper UTF-8 byte sequence.  Besides those forbidden bytes,
there are many sequences also forbidden, including any trailing byte
where one does not belong, any leading byte where one does not belong,
or any multi-byte sequence that when decoded would produce a value
outside the range for that sequence length (for example, the four-byte
sequence 0xF0 0x80 0x80 0x80 that would appear to encode U+0000, which
can only properly be encoded by a single-byte).  The last of these
constraints was formally imposed by Unicode 3.1.0.  Earlier versions
of Unicode explictly approved of decoders that accepted overlong
byte sequences in UTF-8, and even included such decoders in their sample
implementations.

Here the news is better when it comes to thinking about representations
of the union of UCS-2 and Unicode.  The three-byte sequences that might
encode surrogates that are forbidden in UTF-8 are available to encode
those values of UCS-2 without interference with UTF-8 encoding of everything
else.  The WTF-8 variation of UTF-8 is one approach in this area, though
the details require careful examination.  The byte
sequence 0xF0 0x90 0x80 0x80 (representing U+10000) and the
byte sequence 0xED 0xA0 0x80 0xED 0xB0 0x80 (representing U+D8000 U+DC00,
which in turn is the surrogate pair representation of U+10000)
are distinct, allowing the required distinction.  WTF-8 is defined to
disallow this, however, because of the lack of a continuing ability to
distinguish after conversion to UTF-16.  That remains the critical sticking
point.

Tcl 8.1 documentation began the claim that Tcl used UTF-8 to store
its string values and as the encoding with which to pass byte-oriented
string values through interfaces, but that was never strictly true.
More on that below.

Unicode 6.0.0 (October 2010) was notable as the first version to include
assignments for emoji symbols.  Their popularity on mobile devices would end
the days when interest in Unicode characters above **U+FFFF** could be
said to be rare.  

Also in October 2010 Tcl was awakened out of its Unicode support slumber,
still offering only partial Unicode 3.1.0 support.  At that point, Tcl
was brought up to date with Unicode 6.0.0, but Tcl remained without
surrogate pair support.  Since that implied Tcl was also without emoji
symbol support, this increasingly became a mark against Tcl as a langauge
to use for good Unicode programming.  The partial support of Unicode 6 was
first released in Tcl 8.5.10 in June 2011.  Since then it has been customary
to update Tcl's partial support for the new versions of the Unicode Standard
as they are released.  Tcl 8.5.19 was released February 2016 with partial
support of Unicode 8.0.  Tcl 8.6.0 was released December 2012 with partial
support of Unicode 6.2.  Tcl 8.6.6 was released July 2016 with partial
support of Unicode 9.0.

In 2017, the trunk of Tcl development was turned over to work on the
Tcl 9.0 release.  This focused attention again on how best to revise
the string representation for that new milestone.  The history of Tcl
string values in releases 8.6.7 and later felt the influence of that 
focus, and are best discussed after some attention to Tcl's string
representations.

## Representations

Tcl 7 strings are represented directly as C strings.  The representation is
one-to-one and complete.  Every Tcl 7 string has a representation by exactly
one C string, and every C string represents exactly one proper Tcl 7 string.
There is no C string that can be rejected as not representing a Tcl 7 string.
This is very simple.  Also, any interface using the passing of C string values
to implement the conceptual passing of Tcl 7 string values can be designed
with this knowledge.  There is no need to provide for error handling when
the possibility of error is defined out of existence.  Because of this, many
of Tcl's interface routines that date back the longest offer no capability
to report errors in their string arguments.  The C string representation is
also a fixed width encoding of Tcl 7 strings.  This allows for efficient
indexing.

Tcl 8.0 strings are represented directly as the pair of a byte array and
a length stored as a C **signed int**.  No Tcl 8.0 string with length greater
than **INT\_MAX** can be accommodated by this representation.  Other than
that limitation, the same direct, complete, and one-to-one nature of
representation is present as for Tcl 7 strings.  A string too long is the
only error condition that needs to be considered, and that error can be
prevented by constraining the arguments by type.  Again, many interfaces
offer no detection or handling mechanisms to deal with an error in string
value arguments.  The Tcl 8.0 representations continue to offer efficient
indexing via fixed-width storage.

Tcl 8.0 strings are a superset of Tcl 7 strings.  When a Tcl 7 string value
is represented in the Tcl 8.0 manner, the byte array has the same contents
as the C string representation from Tcl 7.  The only difference is whether
there must be a terminating **NUL** byte.  In many places in the Tcl 8.0
representation, such a terminating **NUL** byte was used to allow easier
interoperability with legacy routines written to Tcl 7 expectations.  Most
notably the *bytes* and *length* fields of a **Tcl\_Obj** struct implement
the Tcl 8.0 string representation and require the terminating **NULL**
to be present at *bytes*[*length*].




