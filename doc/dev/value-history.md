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

>	int **main**(int *argc*, char **_argv_)

Every command in Tcl 7 is implemented by a **Tcl_CmdProc** with signature

>	int **Tcl_CmdProc** (ClientData, interp, int _argc_, char *_argv_[])

Each argument value _argv_[_i_] passed to a command arrives in the
form  of a (__char__ *).
The caller is presumed to pass a non-NULL pointer to a suitably usable
contiguous chunk of memory, interpreted as a C array of type **char**. This
includes a duty of the caller to keep this memory allocated and undisturbed
while the command procedure executes. (In the days of Tcl 7, this was
trivially achieved with an assumption that all use of the Tcl library was
single-threaded.) The contents of that array determine the string value.
Each element with (unsigned) **char** value between 1 and 255 represent an
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
represented by a 4-byte chunk of memory storing (0x63, 0x61, 0x74, 0x00).
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
the byte value 0x0A . No command purporting to accept and evaluate
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
support in many image formats for Tk command __image create photo -data__.
Tk 4 never had any built-in image format that supported this function.

Since Tcl I/O could read in and write out binary data, but that data could
not pass between commands, this forced the design of many commands to accept
filename arguments to do their own I/O rather than compose with general
language-wide I/O facilities. As channels expanded to include sockets and
pipes the corresponding expansion of utility could not occur in commands
coded in this way without further revisions.

The public C API for Tcl 7.6 includes 22 routines that return
a (__char__ *) which is a Tcl string value. It includes 73 routines
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
taken as the string value.  This implementation is often called a
counted string to distinguish it from a **NUL**-terminated string. In the
new representation, the set of valid Tcl strings is expanded. 
In Tcl 8.0, a Tcl string is a sequence of zero up to **INT_MAX** __char__
values from the range 0..255.

Expanded values from expanded implementation.
Encoding still simple.
Size limit reasonable at the time.


## Tcl 8.1

*Representation in an internal encoding*



