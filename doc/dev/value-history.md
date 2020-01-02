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

~~~
	int main(int argc, char **argv)
~~~

Every command in Tcl 7 is implemented by a **Tcl_CmdProc** with signature

~~~
	int (Tcl_CmdProc) (ClientData, interp, int argc, char *argv[])
~~~





## Tcl 8.0

*Counted array of* **char**

## Tcl 8.1

*Representation in an internal encoding*



