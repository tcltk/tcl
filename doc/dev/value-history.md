# History of Tcl string values

*Speculative and likely revisionist look back at Tcl strings.*

Tcl programmers are familiar with the phrase
[Every value is a string](https://wiki.tcl-lang.org/page/everything+is+a+string).
Discussions of this aspect of Tcl are found in many places. It is harder to 
find detailed discussions of precisely what is "a string" in Tcl, and how
that precise definition has shifted over the years. This document is meant
to tackle that task, based on a review of the changing interfaces and
implementation in the Tcl source code itself.

## Prehistoric Tcl (7.6 and earlier, pre-1997)

**NUL** *terminated array of* **char** *(C string)*

The inspirations for the original Tcl string values were clearly the
in-memory representations of C string literals, and the 
arguments used by C programs to receive their command line.

>	**int** **main** ( **int** *argc*, **char** **_argv_)

Every command in Tcl 7 is implemented by a **Tcl_CmdProc** with signature

>	**int** **Tcl_CmdProc** ( **ClientData**, _interp_, **int** _argc_, **char** *_argv_[])

Each argument value _argv_[_i_] passed to a command arrives in the
form  of a (__char__ *).
The caller is presumed to pass a non-NULL pointer to a suitably usable
contiguous chunk of memory, interpreted as a C array of type **char**. This
includes a duty of the caller to keep this memory allocated and undisturbed
while the command procedure executes. (In the days of Tcl 7, this was
trivially achieved with an assumption that all use of the Tcl library was
single-threaded.) The elements of that array determine the string value.
Each element with (unsigned) **char** value between 1 and 255 represents an
element of the string, stored at the correponding index of that string.
The first element with value 0 (aka **NUL**) is not part of the string value,
but marks its end.

From this implementation, we see that a Tcl string in release 7.6 is a
sequence of zero or more __char__ values from the range 1..255.
In the C type system, each string element is a __char__.
It is also useful to think of each string
element as a byte. The type name **char** suggests "character", and written
works about Tcl from that time probably refer to a Tcl string as
a "sequence of characters".  In later developments the terms "byte"
and "character" have diverged in meaning. There is no defined limit on the
length of this string representation; the only limit is available memory.

There is a one-to-one connection between stored memory patterns and the
abstract notion of valid Tcl strings.  The Tcl string "cat" is always
represented by a 4-byte chunk of memory storing
(**0x63**, **0x61**, **0x74**, **0x00**).
Any different bytes in memory represent different strings. This means
byte comparisons are string comparisons and byte array size is string length.
The byte values themselves are what Tcl commands operate on. From early on,
the documentation has claimed complete freedom for each command to  interpret
the bytes that are passed to it as arguments (or "words"):

>	The first word is used to locate a command procedure to carry out
>	the command, then all of the words of the command are passed to
>	the command procedure.  The command procedure  is  free to
>	interpret each of its words in any way it likes, such as an integer,
>	variable name, list, or Tcl script.  Different commands interpret
>	their words differently.

In practice though, it has been expected that where interpretation of a
string element value as a member of a charset matters, the ASCII encoding
is presumed for the byte values 1..127. This is in agreement with the
representation of C string literals in all C compilers, and it anchors the
character definitions that are important to the syntax of Tcl itself. For
instance, the newline character that terminates a command in a script is
the byte value **0x0A** . No command purporting to accept and evaluate
an argument as a Tcl script would be free to choose something else.  The
handling of byte values 128..255 showed more variation among commands that
took any particular note of them.  Tcl provided built-in commands
__format__ and __scan__, as well as the backslash encoding forms of
__\\xHH__ and __\\ooo__ to manage the transformation between string element
values and the corresponding numeric values.

Each Tcl command also leaves a result value in the interpreter of evaluation,
whether that is some result defined by the proper functioning of the
command, or a readable message describing an error condition. In Tcl 7,
a caller of **Tcl_Eval** has only one mechanism to retrieve that result
value after evaluating a command: read a (__char__ *) directly from 
the _result_ field of the same **Tcl_Interp** struct passed to **Tcl_Eval**.
The pointer found there points to the same kind of 
**NUL**-terminated array of __char__ used to pass in the argument words.
The set of possible return values from a command is the same as the set of
possible arguments (and set of possible values stored in a variable),
a sequence of zero or more __char__ values from the range 1..255.

It must be arranged that the pointer in *interp*->*result* points to
valid memory readable by the caller even after the **Tcl_CmdProc** is
fully returned. The routine **Tcl_SetResult** offers several options.
If a command procedure already has the result value as
a (__char__ *) _result_, it will always work to call

>	**Tcl_SetResult** (_interp_, _result_, **TCL_VOLATILE**);

The argument **TCL_VOLATILE** instructs the Tcl library to allocate
a large enough block of memory to copy _result_ into, makes that copy,
and then stores a pointer to that copy in _interp_->_result_. When the
string to be copied has length no greater than __TCL\_RESULT\_SIZE__,
this "allocation" takes the form of using an internal _interp_ field of
static buffer space, avoiding dynamic memory allocation for short-enough
results.  A record is kept in other fields of _interp_ recording
that this memory must be freed whenever the result machinery must be
re-initialized for the use of the next command procedure,
a task performed by **Tcl_ResetResult**.  This general solution to
passing back the result value using **TCL_VOLATILE** always imposes the
cost of copying it.  In some situations there are other options. 

When the result value of a command originates in a static string literal
in the source code of the command procedure, there is no need to work
with memory management and copies. A pointer to the literal in static
storage will be valid for the caller. The call

>	**Tcl_SetResult** (_interp_, **"literal"**, **TCL_STATIC**);

is appropriate for this situation. The argument **TCL_STATIC** instructs
the Tcl library to directly write the pointer to the literal into
_interp_->_result_. It is a welcome efficiency to avoid copies when
possible.

A third option is when the command procedure has used **ckalloc**
to allocate the storage for _result_ and then filled it with the proper
bytes of the result value.  Then the call

>	**Tcl_SetResult** (_interp_, _result_, **TCL_DYNAMIC**);

instructs the Tcl library not to make another copy, but to take
ownership of the allocated storage and the duty to call **ckfree**
at the appropriate time.  It is also supported for the command procedure
to use a custom allocator and pass a function pointer for the corresponding
custom deallocator routine.

Besides **Tcl_SetResult**, a command procedure might also build up the
result in place inside the interp result machinery by use of the
routines **Tcl_AppendResult** or **Tcl_AppendElement**, which places more
of the housekeeping burdens in the Tcl library. You may also see examples
like

>	**sprintf** (_interp_->_result_, "%d", _n_);

where the command procedure can be confident about not overflowing
the **TCL\_RESULT\_SIZE** bytes of static buffer space.

When the caller reads the result from _interp_->_result_, it is given no
supported indication which storage protocol is in use, and no supported
mechanism to claim ownership. This means if the caller has any need to keep
the result value for later use, it will need to make another copy and make
provisions for the storage of that copy. Often this takes the form of
calling **Tcl_SetVar** to store the value in a Tcl variable (which
performs the housekeeping of making the copy of its _newValue_ argument).

This system of values has several merits. It is familiar to the intended
audience of C programmers. The representation of each string element 
by a single byte is simple and direct and one-to-one. The result is
a fixed-length encoding which makes random access indexing simple and
efficient.  The processing of string values takes the form of iterating
a pointer through an array, a familiar technique that compilers and hardware
have strong history of making efficient. The overall simplicity keeps the
conceptual burden low, which enhances the utility for connecting software
modules from different origins, the use of Tcl as a so-called "glue language".

There are some deficits to contend with. The greatest complexity at work
is the need to manage memory allocation and ownership. The solutions to
these matters involve a significant amount of string copying. For any
commands that treat arguments and produce results based on interpretations
other than sequences of characters, parsing of argument strings into
other representations suitable for command operations and the generation
of a result strings from those other representations are costs that must
be repeated again and again, since only the string values themselves carry
information into and out of command procedures.

Finally a large deficit of this value model of strings is they cannot
include the element **NUL** within a string, even though that string
element otherwise appears to be legal in the language. In the language
of the day, Tcl 7 is not _binary safe_ or _8-bit clean_.

<pre>
```
	% string length <\x01>
	3
	% string length <\x00>
	1
```
</pre>

Note here that Tcl backslash substitution raises no error when asked to
generate the **NUL** byte, even though Tcl version 7.6 has no way at
all to do anything with that byte. At some point it gets treated as
a string terminator.

Since Tcl 7 strings are C strings, C programmers of the time were familiar
with the issue of binary safety, and several systems of the day made use of
solutions. Electronic mail systems had to deal with the ability to pass
arbitrary binary data through systems where binary safety of all the components
could not be assumed. The solution was to define a number of encoding schemes.
Tcl 7 did not take this approach. Any encoding defined to support
the **NUL** byte would have to borrow from some byte sequence already
representing itself. It must have been judged that the utility of the **NUL**
was less than the disutility of complicating the value encoding that all
commands would need to contend with, at least for general values.

As Tcl gained cross-platform channel support, the inabilty to store in a
Tcl value the arbitrary data read in from a channel started to cause more
and more pain. In particular it was not possible to safely copy a file,
or transmit its contents to an output channel (as one might seek to do
when programming a web server) with code like

<pre>
```
	fconfigure $in -translation binary
	fconfigure $out -translation binary
	puts -nonewline $out [read $in]
```
</pre>

The issue was so compelling that Tcl gained a semi-secret unsupported
command __unsupported0__ for just this purpose with functionality
much like the command __fcopy__ that would come later.

It is more speculative, but it appears the inability to pass arbitrary
binary data through Tcl contributed to the late development of full
support of many image formats for Tk command __image create photo -data__.
Tk 4 never had any built-in image format that supported this function.

Since Tcl I/O could read in and write out binary data, but that data could
not pass between commands, this forced the design of many commands to accept
filename arguments to do their own I/O rather than compose with general
language-wide I/O facilities. As channels expanded to include sockets and
pipes the corresponding expansion of utility could not occur in commands
coded in this way without further revisions.

The public C API for Tcl 7.6 includes 22 routines that return
a (__char__ *) which is a Tcl string value. It includes 74 routines
that accept at least one (__char__ *) argument that is intended to
be a Tcl string value. It includes 6 callback signature typedefs that
specify callback interfaces which must accept at least one (__char__ *)
argument that is a Tcl string value. It includes one callback signature
typedef, **Tcl_VarTraceProc**, that specfies a callback interface that
must return a (__char__ *) that is a Tcl string value.  All of these
components of the public API are points of potential compatibility
concern as the conception of Tcl strings shifts over time.

## Tcl 8.0 (Development begun 1996, Official release 1997-1999)

*Counted array of* **char**

In order to remedy the deficits of the Tcl 7 value set, the implementation
of Tcl strings had to be modified. The modification put in place by
Tcl 8.0 was to stop treating every **NUL** byte as a marker of the end of
a Tcl string value. Tcl would begin to permit a **NUL** byte in the internal
portion of the __char__ arrays holding Tcl strings. Since the presence of
a **NUL** byte would no longer (always) terminate a string, a (__char__ *)
alone could no longer convey an arbitrary string value. New interfaces were
defined where a (__char__ *) argument was accompanied by an __int__ argument
that would specify how many bytes of memory at the pointer should be
taken as the string value.  

As an illustration, consider the Tcl 7 public routine

>	**int** **Tcl_ScanElement** (const **char** * _string_, **int** * _flagPtr_ );

It is a utility routine used in the task of generating the string value
of a list out of the string values of its elements. The argument _string_
is an element of the list passed in as a **NUL**-terminated string. It
is not possible to successfully use this interface to pass in the string
value of a list element that includes the **NUL** character. By expanding
the Tcl value set, Tcl left many public interfaces incapable of dealing with
the entire value set. These routines continue to do what they always did,
and for some purposes that is good enough, but they are unsuitable for
uses that must accommodate the new values supported in Tcl 8.0. This leads
to the creation of an additional public routine

>	**int** **Tcl_ScanCountedElement** (const **char** * _string_, **int** _length_, **int** * _flagPtr_ );

The additional argument _length_ combines with the argument _string_ to
transmit any Tcl 8.0 string value into the routine. In most interfaces
expanded in this way, it is conventional for the _length_ argument to
accept the value **-1** (or sometimes any negative value) as a signal
that _string_ is **NUL**-terminated. Another advantage of this interface
is that it is not necessary for _string_ [ _length_ ] to contain the
byte value **NUL** . This makes it easier to pass substrings of larger
strings without the need to copy or overwrite.

This implementation is often called a
counted string to distinguish it from a **NUL**-terminated string.
In Tcl 8.0, every valid Tcl string is a sequence of zero up
to **INT_MAX** __char__ values from the range 0..255.
In the new representation, the set of valid Tcl strings is both expanded
and contracted.  The alphabet is expanded to include **NUL**, while a
defined limit is imposed on the length of the string sequence of elements
from that alphabet for the first time.

In the context of available memory in most computing systems
of the day, the limitation on string length did not seem unreasonable.
The new implementation solves the problem of storing in a Tcl value
arbitrary binary data up to 2Gb in size.  The encoding of string elements
as __char__ array elements is still direct, simple, one-to-one, and every
__char__ array that was a valid representation of a value in Tcl 7 remains
valid and continues to represent the same value in Tcl 8.0, which offers a
high degree of accommodation of client code written for Tcl 7. Every
collection of bytes in the __char__ array represents a valid string, so
there is no need to consider how any routine ought to respond to an invalid
input.

The public C API for Tcl 8.0 includes only 7 new routines that either
accept or return explicitly a pair of values of type (__char__ *)
and __int__ representing together a counted string value. A second solution
is to include these two values as fields of a new struct, **Tcl_Obj**, and
define new interface routines that make use of that struct.
The **Tcl_Obj** struct introduced in Tcl 8.0 includes a (__char__ *) 
field _bytes_ and an __int__ field _length_.  Taken together
these two fields are a Tcl value represented as a counted string.  The
routine

>	**Tcl_Obj** * **Tcl_NewStringObj** (__char__ * _bytes_, __int__ _length_);

is used to create such a struct out of the pair of arguments defining a
counted string. The routine

>	__char__ * **Tcl_GetStringFromObj** ( **Tcl_Obj** * _objPtr_, __int__ * _lengthPtr_ );

provides the reverse functionality, returning a (__char__ *) value and
writing to *_lengthPtr_ an __int__ value making up the counted string
retrieved from _objPtr_. These routines support the transmission of
the complete binary-safe set of Tcl 8.0 values.

It is mostly outside the scope of this document, but additional fields
of the **Tcl_Obj** struct enable value sharing, and caching of conversions
to other value representations. These other functions solve many of the
defects of the Tcl 7 value representation other than the constrained
universe of supported values itself. Notably these other features enable
substantial performance gains. Much of the published advice to adopt
interfaces using the **Tcl_Obj** struct centers on the performance rewards
rather than the expanded capability to include **NUL** bytes.

The storage of a __char__ array by a **Tcl_Obj** has some parallels with
the storage of a __char__ array as the result of a command evaluated
in a **Tcl_Interp**.  However, the same functionality of **Tcl_SetResult**
was not made available. To do so, something like a _freeProc_ field
would need to be included in the **Tcl_Obj** struct, and concerns about
excessive memory requirements to store every value in Tcl ruled the day.
As a consequence, it is an implicit requirement that _objPtr_->_bytes_
normally points to memory allocated by **ckalloc** and when sharing
demands are all released it will be freed by a call to **ckfree**.
The routine **Tcl_NewStringObj** makes such an allocation and copies
the __char__ array in its _bytes_ argument into it. In contrast,
the (__char__ *) value returned by **Tcl_GetStringFromObj** is a pointer
to the same __char__ array that is stored by _objPtr_. The caller of
**Tcl_GetStringFromObj** can sustain the validity of that memory by
claiming a share on _objPtr_ with a call to **Tcl_IncrRefCount** and
then later releasing that claim with a call to **Tcl_DecrRefCount**.
One complete copy of the __char__ array contents cannot be avoided by this
value model, but value sharing permits most subsequent copying to be avoided.
This design also imposes consequential costs on the "Copy on Write" strategy
when over-writing a **Tcl_Obj** struct. No alternative to complete
allocation and copy is available, with performance consequences for
large values that have visible impact on scripts.

With a **Tcl_Obj** struct defined as a container for any Tcl 8.0 value,
a new signature for command procedures was defined,

>	**int** **Tcl_ObjCmdProc** ( **ClientData**, _interp_, **int** _objc_, **Tcl_Obj** * const _objv_[]);

and a new public routine **Tcl_CreateObjCommand** to create commands 
implemented using the new command procedure signature. Commands using the
new signature have the ability to receive the complete argument set of
Tcl 8.0 as arguments.  An arbitrary Tcl 8.0 value can be returned 
using a call to **Tcl_SetObjResult**.  One of these commands can be
evaluated by use of the old routine **Tcl_Eval** or the new routine
**Tcl_EvalObj** when the script to be evaluated itself might include
the __NUL__ byte. The caller then retrieves the result of the evaluated
command with a call to **Tcl_GetObjResult**.  A new routine
**Tcl_GetStringResult** allows the interpreter result to be retrieved
as a **NUL**-terminated string, with the corresponding truncation at
any **NUL** byte that might be in it. This is not a recommended routine,
but it is an improvement over direct access to the _interp_->_result_
field, which Tcl 8.0 declares deprecated.  All Tcl operations that need
to support the complete new Tcl value set had to be revised to make use
of new facilities and interfaces.

Continuing earlier illustrations, we can see that the **string** command
was adapted in Tcl 8.0

<pre>
```
	% string length <\x00>
	3
```
</pre>

Likewise the **puts** and **read** commands are fully binary safe, so
that 

<pre>
```
	fconfigure $in -translation binary
	fconfigure $out -translation binary
	puts -nonewline $out [read $in]
```
</pre>

now faithfully copies all bytes from one channel to another. Tcl 8.0
also introduces new commands **binary format** and **binary scan**
that Tcl programs can use to process binary values. In Tcl 8.0, these
commands operate directly on the counted string in _objPtr_->_bytes_.

A deployed collection of extension commands written for Tcl 7 could
still be used by Tcl 8.0.  Since these commands were implemented by
command procedures using the **Tcl_CmdProc** signature, they could
only accept **NUL**-terminated strings as arguments. Attempts to pass
any argument value to them that included a **NUL** byte would only
pass the truncated value. Note that this remains within the very
free requirements of an arbitrary Tcl command. Each command is free to
interpret its arguments as it chooses, including the choice to
truncate them at the first **NUL**.  A Tcl 7 command would produce a
result within the constrained value set, but that is acceptable.
If the command records that result via direct write to _interp_->_result_
that is deprecated and formally unsupported, but in practice tolerated
and functional. Such an approach allowed for a gentle, even lazy, approach
to migration to new capabilities.  All of the public Tcl 7 routines that
accept or return **NUL**-terminated strings noted in the earlier section
continue to be supported public routines in Tcl 8.0, doing what they do,
with all the implicit limitations.  The model in place was clearly an
expectation that all commands would migrate in time, but there was not
a big hurry.  Several routines incapable of supporting the expanded
Tcl value model were declared deprecated as a prod toward eventual migration.

Tcl 8.0 itself did not fully convert all of its built-in commands. At least
23 **Tcl_CmdProc** command procedures persist within it. Some can be
demonstrated.

<pre>
```
	% regsub -all y zyz|zyz x foo
	2
	% set foo
	zxz|zxz
	% regsub -all y zyz\x00zyz x foo
	1
	% set foo
	zxz
```
</pre>

Others are largely masked by the use of the bytecode compiler and 
execution engine, also new in Tcl 8.0.
In other built-in commands, the use of routines with these interface
limitations leads to similar failures.

<pre>
```
	% set foo <\x00>
	<>
	% subst {$foo}
	<
```
</pre>

Several routines, both public and private, and a number of established
data structures continue to make use of **NUL**-terminated strings. Because
of this, although the fully general set of Tcl values includes all
binary sequences, several components of Tcl remain limited in release 8.0,
and may not include the **NUL** byte. Examples include command names,
namespace names, channel names, channel type names, **Tcl_ObjType** names,
and likely more.

## Tcl 8.1 (Development begun 1997, Official release 1999-)

*Strings encoded in UTF-8, as modified in Java 1.1*

A major re-thinking of Tcl values began in 1997 as work toward Tcl 8.1
began. The reconciliation of Unicode 1.1 and ISO-10646 into agreement
showed a clear path forward for software to manage text encodings under
one organized plan.  With this new future vision of text management in
software, the notion that programmers could benefit from being shielded
from the need to deal with encodings rapidly vanished.  In contrast, the
value of joining in support of a developing standard was apparent.  

The Tcl _changes_ file records an entry (in part):

<pre>
```
6/18/97 (new feature) Tcl now supports international character sets:
    - All C APIs now accept UTF-8 strings instead of iso8859-1 strings,
      wherever you see "char *", unless explicitly noted otherwise.
    - All Tcl strings represented in UTF-8, which is a convenient
      multi-byte encoding of Unicode.  Variable names, procedure names,
      and all other values in Tcl may include arbitrary Unicode characters.
      For example, the Tcl command "string length" returns how many
      Unicode characters are in the argument string.
    - For Java compatibility, embedded null bytes in C strings are
      represented as \xC080 in UTF-8 strings, but the null byte at the end
      of a UTF-8 string remains \0.  Thus Tcl strings once again do not
      contain null bytes, except for termination bytes.
    - For Java compatibility, "\uXXXX" is used in Tcl to enter a Unicode
      character.  "\u0000" through "\uffff" are acceptable Unicode
      characters.
```
</pre>

The corresponding development patch is no longer available.  The relevant
code development fossil record for Tcl starts on September 21, 1998,
with the source code of something close to the Tcl 8.1a2 release
(February 24, 1998). We also have the Tcl 8.1a1 release (January 23, 1998).
The absence of relevant entries in the _changes_ file
suggests the  "international character set" support in the Tcl 8.1a1
release is what was committed in June, 1997. We can only speculate what
development effort, what judgments and what referrals to what sources
and standards played out in the development of that first implementation
patch. Our speculation is informed, though, by what we know and can reasonably
infer from the technical environment where this work took place.

The claim that Tcl 8.1 strings are stored in UTF-8 is false, strictly
speaking. Neither the encoding generated nor the encoding accepted by Tcl 8.1
follow the precise specification of UTF-8. Not the specification in place
today, nor any of the specifications in place near the time of this work.
As the _changes_ entry indicates, a number of implementation choices are
claimed to serve "Java compatibility". Tcl development at this time took
place at Sun Microsystems, which was also the home of Java. It appears that
"Java compatibility" is not a statement about any sort of interoperability,
but about following the same conventions for the sake of programmer
familiarity.  Tcl 8.1 added the **\\u_HHHH_** syntax as a way for
scripts to specify Unicode characters using only the ASCII characters.
This is the same syntax Java uses for the same thing. The "compatibility"
at work is a reduction of cognitive burden by using the same solution to
the same problem in two languages.  Tcl copied some other deviations
from standard UTF-8 from Java as well.

To appreciate the choices made by Tcl in how to implement support for the
"international character set", it is helpful to review some of the basics
of encoding, and the players and offered standards of the time.  Two efforts
at defining a single Universal Coded Character Set (UCS) were active 
during the 1990s, ISO-10646 and Unicode.  The common task was to define
a mapping from each integer index value (codepoint) to a character assigned
to the location in the character set table identified by the codepoint. The
ISO-10646 effort conceived of a table with 4-byte (32-bit) codepoints, with
space for up to 2^32 characters.  They gave the name UCS-4 to both this
conceptual character set and the trivial encoding of its codepoints
as 4-byte integer values.  In contrast, the Unicode effort proposed
a character set table with up to 2^16 characters, indexed by 2-byte (16 bit)
indices.  This (potentially) smaller character set was formally known
as UCS-2, The trivial encoding of its codepoints as 2-byte integer values
was also called UCS-2. Less formally, but very commonly the character
set UCS-2 was called "Unicode", and two-byte codepoint values were called
"Unicode characters", emphasizing the much smaller table, and smaller
codepoints of the Unicode project. The notation **U+_HHHH_** for codepoint
values was also in use. In the mid-1990s, no codepoints outside UCS-2
were assigned by either UCS project.

At the time, a migration away from single-byte encodings like ASCII
or ISO-8859-1 to either UCS-2 or UCS-4 was a tough sell. A doubling
or quadrupling of memory and network traffic requirements for the sake
of additional characters that many applications had no need to handle
was not a welcome proposition. It was clear that a variable-length encoding
that better managed the costs involved would be needed. Appendix F of The
Unicode Standard, Version 1.1 (1993) contains a description of
what was called the File System Safe UCS Transformation Format (FSS-UTF).
FSS-UTF is a variable-length byte-oriented encoding of UCS codepoints.
The implementation code in Tcl 8.1 more closely matches this specification
than anything bearing the name UTF-8.  The encoding rules of FSS-UTF are
summarized in the table

<pre>
```
	Codepoints		Encoding
	----------		--------
	U+0000 - U+007F		0bbbbbbb
	U+0080 - U+07FF		110bbbbb 10bbbbbb
	U+0800 - U+FFFF		1110bbbb (10bbbbbb)2

	U+00010000 - U+001FFFFF	11110bbb (10bbbbbb)3
	U+00200000 - U+03FFFFFF	111110bb (10bbbbbb)4
	U+04000000 - U+7FFFFFFF	1111110b (10bbbbbb)5
```
</pre>

FSS-UTF encodes each codepoint in the ASCII range as a single-byte,
the same byte already used by ASCII for that character.  Longer byte seqeunces
encode higher codepoints that were less frequently used.  A footnote
in Appendix F makes the observation that encoded sequences of three bytes
are sufficient to cover all 16-bit codepoints of UCS-2 (Unicode). The
longer encodings of four through six bytes prepared the way to cover
codepoints in UCS-4 up to 31 bits.

(Side observation: it would be
straightforward to continue the table of encoding rules by one more
line, using the lead byte **0xFE** to introduce a sequence of six trail
bytes. This would be a seven byte encoding that could encode up to 36 bits,
easily covering the remainder of the 32-bit range of all of UCS-4.  It 
seems the value of keeping byte **0xFE** out of encoded streams outweighed
the failure to encode the upper half of UCS-4, which had no forseeable use.
The bytes **0xFE** and **0xFF** together make up a Byte Order Mark that
is used to determine endianness of UCS-2 data. When neither byte appears
in UTF-8 or FSS-UTF the confidence of that interpretation is boldened.)

In Tcl 8.1, encoding of Unicode codepoints is accomplished by the routine

>	**int** **Tcl_UniCharToUtf** ( **int** *ch*, **char** *_str_);

The routine is (nearly) a direct translation of the table of encoding rules
of FSS-UTF.  However it is clear that a decision was made that Tcl would
support "Unicode" which meant UCS-2 which meant 16-bit codepoints as
"Unicode characters".  Unicode characters require a maximum of
three encoded bytes, and Tcl 8.1 source code uses **TCL\_UTF\_MAX**
to represent that limit. It is useful in sizing buffers where encoded
strings are to be written.  The public header for Tcl 8.1 includes

>	\#define **TCL\_UTF\_MAX**	3

and

>	typedef **unsigned short Tcl_UniChar**;

However, the body of **Tcl_UniCharToUtf** includes code to
encode up to six-byte sequences as prescribed by FSS-UTF representing
up to 31-bit codepoints, protected by a conditional compiler directive

>	\#if **TCL\_UTF\_MAX** > 3 .

With the default setting, Tcl is able to encode a total of 65,536
distinct values of the *ch* codepoint argument. These are all the
codepoints of UCS-2, the complete capacity of Unicode 1.1. When
**Tcl_UniCharToUtf** is passed a *ch* argument outside that supported
range, the codepoint **U+FFFD** is encoded in its place. Unicode 1.1
assigns this codepoint the name **REPLACEMENT CHARACTER**. Unicode
prescribes the use of **U+FFFD** to replace an incoming character
whose value is unrepresentable.  In a configuration of Tcl 8.1 with
**TCL\_UTF\_MAX** set to 6, the **REPLACEMENT CHARACTER** would
replace only the *ch* values in the upper half of UCS-4.  Note that
Tcl 8.1 has made a design choice here to handle unsupported inputs
via replacement, and not via raising an error.

It is clear that the Unicode support provided by Tcl 8.1 was intentionally
limited to UCS-2, while conventions and migration supports were put in place
so that a future (binary incompatible) version of Tcl would expand its support
to UCS-4 with FSS-UTF encoding sequences of up to six bytes, at some point
where the considerable cost in memory and processing burden traded off
against the benefits of support for whatever future codepoint assignments
outside of UCS-2 compelled support.

The capacity of UCS-2 is a table of 65,536 codepoints. However, Unicode 1.1
assigns characters to only 40,635 rows in that table. If we generate all
possible sequences of those 40,635 assigned codepoints, and then encode
each sequence via the FSS-UTF rules, we generate a set of byte sequences
far smaller and more constrained that the general set of all byte sequences.
When we claim that Tcl 8.1 strings are kept in the UTF-8 encoding, we
imply that Tcl 8.1 strings are constrained to a much smaller set of byte
sequences than were permitted for Tcl 8.0 strings.  This raises questions
about both compatibility and what a decoder should do with a non-conformant
byte sequence.

It is an expected feature of the Unicode character set that more characters
are to be added over time.  The number of assigned codepoints grows as new
versions of the Unicode standard are codified and published.  Because
support for standards always lags their publication, software written in
conformance to one version of Unicode is likely to encounter data produced
in conformance to a later version.  In light of this, the best practice is
to accommodate and preserve unassigned codepoints as much as possible.
Software written to support Unicode 1.1 can then accept Unicode 2 data streams,pass them through and output them again undamaged. In this way a middleware
written to an obsolete Unicode standard can still support providers and
clients that seek to use characters assigned only in a later standard.
The middleware would fail to properly answer any questions about the
later-assigned characters, such as a **string is alpha** test, but could
still not be immediately useless.
Unicode 1.1 left open the possibility that any codepoint in UCS-2 might
one day be assigned. Tcl 8.1 imposes no conditions on the encoding of any
**Tcl_UniChar** value at all.

The standards specifying text encodings published in the mid-1990s were quite
clear and explicit about the right way to do things. They were often less
demanding and specific about how to respond in the presence of errors. The
spirit of the Postel Robustness Principle

>	*Be liberal in what you accept, and conservative in what you send.*

held considerable influence at the time. Many implementations chose to
accommodate input errors, especially when that was the natural result
of laziness. For example, the specification of FSS-UTF and all specifications
for UTF-8 are completely clear and explicit that the proper encoding
of the codepoint **U+0000** is the byte value **0x00**, (our old friend
the **NUL** byte!). However, inspection of the FSS-UTF encoding rules
quickly reveals that the multi-byte encoding strategies have room to
encode all the codepoints that already have a proper encoding as a single
byte.  Unless we take active steps to prevent it, a decoding procedure
will happily decode the byte sequences (**0xC0**, **0x80**) or
(**0xE0**, **0x80**, **0x80**) as the codepoint **U+0000**.  Both
Unicode 1.1 and Unicode 2.0 include sample decoder routines that do
exactly that. The text of Appendix A.2 of The Unicode Standard 2.0
(which contains one definition of UTF-8 from 1996) explicitly states,

>	When converting from UTF-8 to Unicode values, however,
>	implementations do not need to check that the shortest encoding
>	is being used, which simplifies the conversion algorithm.

Whether or not the Tcl developers were explicitly following this advice,
the routine in Tcl 8.1 for decoding Unicode codepoints from the
encoded byte sequences,

>	**int** **Tcl_UtfToUniChar** ( **const char** *_str_, **Tcl_UniChar** *_chPtr_),

implements the same forgiving support for overlong byte sequences.

In the context of the still incomplete transition from the Tcl 7 to
the Tcl 8.0 conventions for string values, and the lingering difficulties
in handling the **NUL** byte, the forgiving nature of the UTF-8 decoders
presented a clear opportunity.  If the decoder is equally happy to receive
the byte sequence (**0xC0**, **0x80**) as a representation of **U+0000**,
why not have the encoder encode it that way in the first place?  With
this modification, encoded byte sequences return to not having the **NUL**
byte in them at all, and we sweep away the remnants of a difficult
transition (while admittedly adding a new one).  Tcl chose to adopt
this modification of the encoding rules.  Java 1.1, another language
developed by Sun Microsystems at the same time, made the same modification.

The modification is not without its problems.  One feature of the proper
UTF-8 encoding scheme is that a sorting of encoded byte sequences matches
the sorting of the UCS-2 sequences they encode.  Not so when custom
modifications are in place.  Likewise, byte-for-byte comparisons of
byte sequences in a proper UTF-8 encoding reliably transfer to
character-for-character comparisons of the UCS-2 strings they encode.
This is especially important when testing for equality as is vital
in searches. When any character has multiple acceptable encodings,
the fully correct implementation of these matters becomes much more
complex.  It is also a complication that when a custom encoding
does not match any standard specification, it becomes impossible to document
what encoding is supported in short and precise ways. Either a long
document spelling out the rules that are in place must be written, or
else false claims are made about using "UTF-8" supplemented by haphazard
revelations to the curious questioners of all the exceptions
and transgressions that are acutally in place.

That is not the end. Even when all overlong encodings of all codepoints
of UCS-2 are accepted, there are still many, many more byte sequences
to consider.  A scheme demanding UTF-8 encoding would be expected to
reject these other sequences, either by raising an error, or by inserting
the **REPLACEMENT CHARACTER** in suitable places to mark the non-conformant
input.  This is not what **Tcl_UtfToUniChar** does.  Instead when any
byte is encountered that is invalid by the modified rules of UTF-8 already
described, that byte is taken as an encoding of the code-point with the
same value as the byte value.  That is, when the modified UTF-8 encoding
fails to be satisfied, the ISO-8859-1 encoding of Tcl 8.0 is assumed instead.
Furthermore, this is not just a quiet accommodation, or perhaps an error
in implementation. It is the explict documented behavior of
**Tcl_UtfToUniChar** in Tcl 8.1.

>	If the input is not in proper UTF‐8 format, Tcl\_UtfToUniChar will
>	store the first byte of src in *chPtr  as a Tcl\_UniChar between
>	0x0000 and 0x00ff and return 1.

The reasoning behind this encoding choice is not explicitly recorded
in any known place.  This does not appear to be a choice copied from
Java.  It seems a reasonable guess to presume this is intended to offer
some measure of compatibility to deployed callers of Tcl 7 and Tcl 8.0,
though the effectiveness of that strategy is questionable to say the least.
A Tcl 8.1 that continues to treat the byte sequence (**0xC0**, **0x20**)
as the same two bytes as Tcl 8.0, but begins to treat the byte
sequence (**0xC0**, **0x80**) as a representation of **NUL** is not
much of a compatibility accommodation to rely upon.

many-to-one, interpretive freedom, (parse of \n)
variable length

Compat with 8.0
ByteArrays

UCS-2 sequences
bytes v chars

Two-tier encoding. 

Threading & memory validity

API Counts?

NUL naming limitations

UTF-16 and surrogate pairs




During the development of Tcl 8.1, the prevailing Unicode standards were
Unicode 2.0 and 2.1. In these definitions of Unicode, all assigned codepoints
were in the set U+0000 through U+FFFF, the set of characters within
UCS-2, also called the Basic Multilingual Plane.

The publication of RFC 2044 describing a UTF-8
encoding for UCS-2 in October 1996 was also a key event.

Note the end of overlong byte sequence interpretation (other than NUL)
as bug fix 
https://core.tcl-lang.org/tcl/info/67aa9a2070
This didn't happen at a clean minor version bump.



MILESTONE: Bug 67aa9a2070 was fixed in June 2017, first released in Tcl 8.6.7
in August 2017.  This is when Tcl stopped accepting overlong byte sequences
in its internal encoding as valid alternative ways to encode characters.


