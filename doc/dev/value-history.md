# History of Tcl string values

*Speculative and likely revisionist look back at Tcl strings.*

Tcl programmers are familiar with the phrase
[Every value is a string](https://wiki.tcl-lang.org/page/everything+is+a+string).
Discussions of this aspect of Tcl are found in many places. It is harder to 
find detailed discussions of precisely what is "a string" in Tcl, and how
that precise definition has shifted over the years. This document is meant
to tackle that task, based on a review of the changing interfaces and
implementation in the Tcl source code itself.

## Prehistoric Tcl (7.6 and earlier)

**NUL** *terminated array of* **char** *(C string)*

The inspirations for the original Tcl string values were clearly the
in-memory representations of C string literal values, and the 
arguments used by C programs to receive their command line.

>	int **main**(int *argc*, char **_argv_)

Every command in Tcl 7 is implemented by a **Tcl_CmdProc** with signature

>	int **Tcl_CmdProc** (ClientData, interp, int _argc_, char *_argv_[])

Each argument value _argv_[_i_] passed to a command arrives in the
form  of a (__char__ *).
The caller is presumed to pass a non-NULL pointer to a suitably usable
contiguous chunk of memory, interpreted as a C array of type **char**. (The
caller is also required to keep this memory allocated and undisturbed while
the command procedure executes. In the days of Tcl 7, this was
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
value after evaluating a command, to read a (__char__ *) directly from 
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
and then stores a pointer to that copy in _interp_->_result_. (As an
implementation detail, there is also an internal field of static buffer
space, _interp_->_resultSpace_, that is used for copy storage when possible
as a performance benefit over always acquiring the copy storage space
via dynamic memory allocation.) A value is stored in the
field _interp_->_freeProc_ recording that this memory must be freed
whenever the fields must be made available to the next command procedure,
a task performed by **Tcl_ResetResult**.  This general solution to
passing back the result value always imposes the cost of copying it.
In some situations there are other options. 

When the result value of a command originates in a static string literal
in the source code of the command procedure, there's no need to work
with memory management and copies. A pointer to the literal in static
storage will be valid for the caller. The call

>	**Tcl_SetResult** (_interp_, **"literal"**, **TCL_STATIC**);

is appropriate for this situation. The argument **TCL_STATIC** instructs
the Tcl library to directly write the pointer to the literal into
_interp_->_result_. It is a welcome efficiency to avoid copies when
possible.

The third option is when the command procedure has used **ckalloc**
to allocate the storage for _result_ and then filled it with the proper
bytes of the result value.  Then the call

>	**Tcl_SetResult** (_interp_, _result_, **TCL_DYNAMIC**);

instructs the Tcl library not to make another copy, but just to take
ownership of the allocated storage and the duty to call **ckfree**
at the appropriate time.  It is also supported for the command procedure
to use a custom allocator and pass a function pointer for the corresponding
custom deallocator routine.

When the caller reads the result from _interp_->_result_, it is given no
supported indication which storage protocol is in use, and no supported
mechanism to claim ownership. This means if the caller has any need to keep
the result value for later use, it will need to make another copy and make
provisions for the storage of that copy. Often this takes the form of
calling **Tcl_SetVar** to store the value in a Tcl variable (which
performs the housekeeping of making the copy of its _newValue_ argument.

This system of values has several merits. It is familiar to the intended
audience of C programmers. The representation of each string element 
by a single byte is simple and direct and one-to-one. The result is
a fixed-length encoding which makes random access indexing simple and
efficient.  The overall simplicity keeps the conceptual burden low,
which enhances the utility for connecting software modules from different
authors, the use of Tcl as a so-called "glue language".

There are some deficits to contend with. The greatest complexity at work
is the need to manage memory allocation and ownership. The solutions to
these matters involve a significant amount of string copying. For any
commands that treat arguments and produce results based on interpretations
other than sequences of characters, parsing of argument strings into
other representations suitable for command operations and the generation
of a result strings from those other representations are costs that must
be repeated again and again.

Finally a large deficit of this value model of strings is they cannot
include the element **NUL** within a string, even though that string
element otherwise appears to be legal in the language. In the language
of the day, Tcl is not _binary safe_ or _8-bit clean_.

```
	% string length <\001>
	3
	% string length <\000>
	1
```


API


## Tcl 8.0

*Counted array of* **char**

## Tcl 8.1

*Representation in an internal encoding*



