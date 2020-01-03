# History of Tcl string values

*Speculative and likely revisionist look back at Tcl strings.*

Tcl programmers are familiar with the phrase
[Every value is a string](https://wiki.tcl-lang.org/page/everything+is+a+string).
Discussions of this aspect of Tcl are found in many places. It is harder to 
find detailed discussions of precisely what is "a string" in Tcl, and how
that precise definition has shifted over the years. This document is meant
to tackle that task, based on a review of the changing interfaces and
implementation in the Tcl source code itself.

## Prehistoric Tcl (7.6)

**NUL** *terminated array of* **char** *(C string)*

The inspirations for the orignal Tcl string values were clearly the
in-memory representations of C string literal values, and the 
arguments used by C programs to receive their command line.

>	int **main**(int *argc*, char **_argv_)

Every command in Tcl 7 is implemented by a **Tcl_CmdProc** with signature

>	int **Tcl_CmdProc** (ClientData, interp, int _argc_, char *_argv_[])

Each argument values passed to a command arrives in the form  of a (char *).
The caller is presumed to pass a non-NULL pointer to a suitably usable
contiguous chunk of memory, interpreted as a C array of type char. The
contents of that array determine the value.  Each element
with (unsigned) char value between 1 and 255 represent an element of the
string, stored at the correponding index of that string. The first element
with value 0 (aka **NUL**) is not part of the string value, but marks its end.

From this implementation, we see that Tcl strings in release 7.6 are sequences
of zero or more char values from the range 1..255. In the C type system,
each string element is a char. It is also useful to think of each string
element as a byte. The type name **char** suggests "character", and written
works about Tcl from that time probably will use that term to refer to the
bytes in the sequence making up a string value.  In later developments these
terms have diverged in meaning.

There is a one-to-one connection between stored memory patterns and the
abstract notion of valid Tcl strings.  The Tcl string "cat" is always
represented by a 4-byte chunk of memory storing (0x63, 0x61, 0x74, 0x00).
Any different bytes in memory represent different strings. This means
byte comparisons are string comparisons and byte array length is string length.
The byte values themselves are what Tcl commands operate on. From early on,
the documentation has claimed complete freedom for each command to  interpret
the bytes that are passed as to it as arguments (or "words"):

>	The first word is used to locate a command procedure to carry out
>	the command, then all of the words of the command are passed to
>	the command procedure.  The command procedure  is  free to
>	interpret each of its words in any way it likes, such as an integer,
>	variable name, list, or Tcl script.  Different commands interpret
>	their words differently.

In practice, though...







## Tcl 8.0

*Counted array of* **char**

## Tcl 8.1

*Representation in an internal encoding*



