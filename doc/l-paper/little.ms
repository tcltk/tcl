.\" The L Programming Language
.\" Copyright (c) 2006 BitMover, Inc.
.\"
.\" process with 
.\"    groff -R -ms l.ms > l.ps
.\"
.\" Mail to tcl2006@tcl.tk when done.
.\"
.\" Commands for refer
.R1
database references
accumulate
label-in-text
label A.nD.y%a
.R2
.de CS
.sp .25
.KS
.in +.5
.ta .55i 1i
.ft CW
.nf
..
.de CE
.sp .25
.in
.ft
.fi
.KE
..
.de BR
\fB\\$1\fR\\$2
..
.de LI
.br
.ne 4
.LP
.B "\\$*"
'br
..
.de BU
.IP \(bu 2
..
.\" Title, authors, etc.
.nr PO 1i
.nr LL 6.5i
.po \n[PO]u
.ll \n[LL]u
.HM .75i
.FM .75i
.TL
The L Programming Language
.br
or
.br
Tcl for C Programmers
.AU
Oscar Bonilla, Tim Daly, Jr., Larry McVoy
.AI
BitMover, Inc.
300 Orchard City Drive, Suite 132
Campbell, CA 95008
.AU
Jeffrey Hobbs
.AI
ActiveState Software Inc.
1700-409 Granville Street
Vancouver, BC, Canada
V6C 1T2 
.sp
\f(CRl@bitmover.com\fP
.\" Abstract
.AB
This paper describes a new programming language called L.  
L is a compiled-to-byte-code language with the unusual twist that it
compiles to Tcl byte codes and by doing so leverages the entire Tcl
runtime.  
L is designed to peacefully coexist with Tcl rather than replace Tcl.
L functions may call Tcl procs and vice versa.
They may also coexist in the same source file.
L is a static weakly typed language with int, float, string, struct,
array, and hash as first-class objects.
The L syntax is reminiscent of C with a tiny bit of C++ thrown in.
.PP
The implementation consists primarily of a simple compiler that Tcl
invokes whenever L source code is encountered.
The L code is parsed by a Bison-generated parser into an abstract syntax
tree (AST), which is
type-checked and then translated into Tcl byte code.
Upon its execution, L code is indistinguishable from Tcl code, which
makes for easy interoperability.
.ig
.PP
L has been discussed slightly on the #tcl IRC channel and the best quote
to date is from Donal K Fellows who said:
.I "\(lqIt's like perl without the nastiest bits.\(rq"
..
.PP
L is open source software, and it is made available under the same
license as Tcl/Tk with the hope that people will find it useful and it
may encourage more people to join the Tcl/Tk community.
.AE
.bp
.EQ
delim @@
.EN
.ce 1
.I "\(lqIt's like perl without the nastiest bits.\(rq"
.sp .5
.ce 1
-- Donal K. Fellows \s7(on the #tcl IRC channel)\s0
.ig
.sp .5
.ce 1
.I "\(lqHe shouldn't have said if he didn't mean it.\(rq"
.sp .5
.ce 1
-- Oscar Bonilla
..
.sp
.2C
.NH 1
Introduction
.LP
BitMover software is produced using a conservative development methodology.
All development goes through a stringent process that relies heavily on
peer review and extensive regression tests to ensure quality products.
.LP
Because of the stability requirements of our market,
we read code much more than we write it.
Spot checks indicate that we spend at least 10 times as much
time reading and reviewing as we do writing.
Naturally, we tend to optimize heavily for the read path rather than the
write path.
.\" Much like a file system.  H'm.  Work that into the talk?
.LP
For years we have used the Tcl/Tk system for our graphical user interfaces.
We periodically consider the alternatives and have consistently found that 
short of doing native implementations, the
Tcl/Tk system is still the best choice from a development cost point of 
view.
Our estimate is that it would cost roughly six times as much to develop
and maintain native GUIs instead of using a single Tcl source base for all 
platforms.
However, the maintenance of our Tcl source base has recently become
problematic because two things happened:
.BU
Our Tcl source base grew past a manageable size (for us).
.BU
Our peer review system could not handle Tcl code.
.LP
We have about 25,000 lines lines of Tcl, implementing about a dozen
graphical interfaces for browsing code, checking in code, viewing changes,
etc.\**
.FS
This number is artificially low because we have been holding off on a number 
of GUIs until we had a better answer.  Had we not been holding back, 100,000
lines is more likely where we would be.
.FE
Maintaining and extending the Tcl source base has become unmanageable, and
when the review process was added to the mix, the costs became
too high.
.LP
This has been a problem for us for years and we were forced to come up with 
a better answer.
We investigated the alternatives but in the end the Tcl runtime
and the Tk widgets were too compelling.
We solved our problems by marrying a language syntax we felt
was well suited for fast reviewing and understanding with what
we feel is the best GUI toolkit and runtime available today.
.LP
The rest of the paper is divided into sections that discuss the following
topics:
an overview of L,
why the L approach is interesting,
why other runtimes were not chosen,
why not pure Tcl,
why not native GUIs,
L language details such as types, calling/return conventions,
current status,
features we have not yet done but want to do,
licensing and availability,
and a summary.
There is an appendix with some small working program examples.
.NH 1
L overview
.LP
L is actually a very small addition to the Tcl system.
If we divide the Tcl system into logical parts this becomes obvious:
.TS
expand box;
l l
l c.
Subsection	Percentage of Tcl/Tk 8.5
=
Tcl parser/compiler	<= 1%
L parser/compiler	<= 1%
Tcl runtime	48%
Tk	51%
.TE
.LP
The parser and compiler are quite small when compared to the
code that implements the runtime and the libraries (in both Tcl and L it
is less than 10K lines of code).
Because the parser/compiler is such a small part of the system, it is
reasonable to add an alternative parser/compiler to the
system and let them both run side by side.
That is L in a nutshell.
It is the small amount of effort required to leverage a large amount of
value embodied in the runtime and libraries.
.LP
The L compiler creates an abstract syntax tree from L
source and compiles that to byte codes.
The byte codes generated are standard Tcl byte codes, following Tcl 
call/return conventions and using Tcl variables.
Because we are careful not to break any Tcl rules,
L functions may call Tcl procs and vice versa.
This allows L to use the extensive, mature Tcl/Tk runtime
and libraries unmodified.
.NH 1
Unique design
.LP
As we dive deeper into the L syntax and semantics it would be
easy to be drawn into a discussion of why L is better or why Tcl
is better.
To do so would be to miss an important point.
Regardless of the merits of each language, the value of L
is that it demonstrates a new way to leverage and reuse existing code.
With a relatively small amount of effort, we have leveraged over 
1.4 million lines of source making up the Tcl/Tk system plus some
extensions.
.LP
The existence of L opens the door to any number of domain-specific
languages being added to the Tcl runtime system.
.ig
If some group prefers Python syntax we see no reason they could
not take the L scanner and parser, change the syntax to Python,
and add another syntax to Tcl.
A reasonable question is \(lqwhy bother?\(rq because Python has a
runtime.
The answer isn't Python, it is domain specific languages.
Any effort that needs a specific syntax to be interpreted could
take our approach and get the job done for far less effort than
starting from scratch.
..
.LP
For example, consider the GDB debugger.
GDB lets users type C, C++, etc., at it and run the code.
Doing so means GDB has to provide an interpreter and a runtime.
Rather than building one, GDB could reuse the ideas and code
pioneered by the L effort.
Having a well maintained runtime with the option of creating an 
arbitrary syntax to use that runtime is useful for any sort of
debugger or runtime inspector.
L is just one example of a different syntax leveraging the Tcl/Tk system;
we are confident there will be others.
.NH 1
Alternative runtimes
.LP
Once the idea of adding a different parser/compiler to a scripting
language is understood, the question becomes: why Tcl rather than some
other runtime such as Perl, Python, Ruby, Java, or others?
We looked briefly at that question.
Our need was for a well supported, mature runtime that supported
scripting GUI interfaces and was extensible from C.
.LP
We dismissed Java because the runtime is too large and the GUI toolkits
are weak, both in features and in performance.
The other runtimes addressed the GUI issues mostly by providing Tk
bindings (and in some cases Qt or Gtk bindings).
Any system that is using Tk bindings is already dragging along a Tcl
interpreter to run the Tk code.
It seemed like a waste to have a different interpreter just for the GUIs.
It has also been our experience that the only way to build robust
software systems is to have the minimum number of ``moving parts.''
Having two interpreters is an unnecessary complication.
.LP
But even if there were a good runtime with a good GUI interface, there was
another requirement we felt was only well addressed by Tcl.
Tcl has been designed from the onset to be an extendable language.
The original vision was that Tcl was glue and all the heavy lifting would
be done by C extensions to the language.
The internal Tcl code is fairly small and quite pleasant to use; adding
extensions is straightforward and natural.
We needed to take advantage of this feature of the Tcl system and other
runtimes made this difficult.
.NH 1
L vs pure Tcl
.\" Brian went on and on about syntax / lint checker.
.\" Coverity example.
.LP
Many in the Tcl community may question whether there is any value in an
alternate syntax for the Tcl runtime.
After all, Tcl is a powerful, dynamic language and many significant
applications are based on Tcl.
.LP
We agree that Tcl is powerful, but that power comes at a cost.
Tcl's dynamic nature makes it impossible to detect even simple parse
errors, such as typos, without running the program.
.LP
Although there are advantages to the dynamic approach in language
design, there are also drawbacks:
.LI Data structures.
Probably the single largest problem we found with Tcl was the lack of a
C-style struct, i.e., a centralized collection of variables with
annotations indicating why they are there.
These are commonly emulated in Tcl with associative arrays.
That isn't good enough because the ``struct fields'' are 
scattered all over the source base rather than being in one place,
laid out with types and comments.
To paraphrase Fred Brooks:
.ft I
\(lqShow me your code and conceal your data structures, and I shall
continue to be mystified. Show me your data structures, and I won't
usually need your code; it'll be obvious.\(rq
.[
mythical man month
.]
.LI Lint.
It is impossible to write a syntax checker or a lint-like tool for Tcl that
works 100% of the time unless that tool is actually running the program it 
is checking.
Even an interpreter-based tool would have the problem that it is not 
practical to force the application through all possible code paths.
It is worth noting that this problem is present in all dynamic languages
and object-oriented languages have the same problem; you can't 
just look at the code and know what it is doing.
.LI Reviewing.
As mentioned previously, at BitMover we do a lot of peer review as well as
other forms of code reading.
For the same reasons that it is difficult to write a lint-like tool
for Tcl, it is difficult for a human to look at Tcl and understand what
it is doing.
The verbose style of basic operations in Tcl, e.g.,
.CS
lset fib $i \\
    [expr \\
    {[lindex $fib [expr {$i-1}]] +
     [lindex $fib [expr {$i-2}]]}]
.CE
vs 
.CS
fib[i] = fib[i-1] + fib[i-2];
.CE
tend to obscure what is actually being said in the code.
.LI Optimization.
Optimizing Tcl is more challenging than optimizing a ``weaker'' language
such as L.
Many well understood optimization techniques could be applied to the
compilation of L, resulting in a significant performance increase for
some programs.
As an example, due to the static type system of L, we believe it's
possible to make L immune to ``shimmering.''
.[
shimmering
.]
.LP
We tend to view Tcl more like assembly language on steroids.
It is a powerful tool and when that power is needed it is 
appreciated.
But most of the time we are doing fairly simplistic programming
deliberately so it is easy to read, and we find that a static language
with a static type system is much easier for us to read and easier
for a compiler to optimize and check.
.NH 1
L vs native GUIs
.LP
This question gets raised at least once a year here: why not do native
GUIs?
It is certainly possible to do so.
We have done implementations of several of our GUIs in other
toolkits.
The arguments for doing so are compelling: better look and feel, native
behavior, etc.
.LP
The reasons for staying with Tcl/Tk are simple:
.LI Cost.
The cost of creating 2-4 different implementations of each GUI interface is
probably 3 times what it took us to get where we are today.
But the cost does not end there.
The cost extends to testing the GUIs on each platform as well as putting
processes in place to make sure that the GUIs march forward in sync,
i.e., if the Java revtool gets a new feature, that same feature needs to
be added to the Linux, Windows, and Aqua GUIs.
When we add up all the costs, it looks more like 6 times the effort.
.LI Functionality.
Every time we go look at the other toolkits we find that they are not as
powerful as the Tk toolkit.
In particular, the canvas and text widgets are more useful than any others
we have found.
.sp .5
That said, a large drawback of the Tk approach is the lack of a complete
widget set in the core.
In order to get the functionality needed, a ragtag group of extensions, 
with partially overlapping features, need to be combined into a Tcl/Tk
``distribution.''
We look forward to the day that this issue is resolved.
.NH 1
L language details
.LP
In this section we cover some of the differences from C, differences
from Tcl, types, call/return conventions, expressions, and control flow.
.NH 2
Extensions to C
.LI Regex.
L uses Perl's syntax for regular expressions in statements, but it uses
Tcl's regular expression engine.
So you may say:
.CS
if (a =~ /${r}/) {...
.CE
to get the same results as Tcl's
.CS
if {[regexp $r $a]} {...
.CE
.LI Associative arrays.
We call these hashes in L to distinguish them from traditional C-style arrays.
The keys and values are strings.
.LI Arrays grow.
If you assign into an array past the last element the array grows as needed.
Many constructs that would normally use C pointers, such as linked lists
or trees, can be constructed with an array of structures linked via indices
rather than pointers.
.LI defined().
A built-in that indicates if the variable passed is defined.
The following tests for the existence of the
field in the hash, and the existence of the array element, respectively.
.CS
defined(foo{"bar"})
defined(stuff[3])
.CE
.LI Strings.
Strings are first-class objects like any other base type.
One implication of this is that unlike C strings, which are pointers,
if you want to pass a reference to the string you must do so
explicitly.
.NH 2
Unimplemented C features
.LP
L does not have bit fields, enums, unions, or C-style pointers.
L currently does not have a C-like preprocessor, though one is planned.
.NH 2
Extensions to Tcl
.LI Type checking.
L has a weak static type system, which makes it possible to do type
checking at compile time.
Note that L's type system is independent of Tcl's runtime type system,
although the two can interoperate.
Variables in L may not change types, unlike Tcl variables, which are
strings except when they're not (as with floats, ints, lists, etc.)
.LI Structs.
C-style structs are part of L.
A Tcl API is provided that supports getting and setting fields as well as
introspection.
.LI References.
Pass by reference in Tcl is possible but awkward.
Attempts have been made to improve it in Tcl
.[
pass by reference
.]
but they are unsatisfying.
We think our syntax is cleaner and easier to read.
.LI Function prototypes.
Currently these are used to get type checking when calling Tcl built-ins.
For example, we can prototype gets() as
.CS
extern int gets(FILE, string &);
.CE
to always require gets to be called with two arguments.
We could also prototype gets() as
.CS
extern string gets(FILE);
.CE
to make it return a string.
If prototypes are missing, L treats undefined functions as external Tcl
functions that return poly and take a variable number of arguments of
type poly.
.br
.ne 20
.NH 2
Types
.NH 3
Simple types
.LI int.
Integer types in L are like C integers:  they are sized to the
machine's word size (at least 32 bits and possibly 64).
Integers in L are initialized to 0, even for local variables.
.CS
int	a = 5;
int	b;         // defaults to 0
.CE
.LP
Any constant that looks like an int is typed as an int.
.LI float.
Floating-point numbers in L are at least double-precision IEEE 754.
Floats are initialized to 0.0, even for local variables.
.LP
Any constant that looks like a float is typed as a float.
Note that this means that assigning an integer to a float is only
legal because of automatic type conversion.
.CS
float	f = 1;   // converts to 1.0
float	g;       // defaults to 0.0
float	pi = 3.14159265;
.CE
.LI string.
The string type is the same as a Tcl string but different from a C string.
Strings are not null-terminated as they are in C, nor are they arrays
of bytes.
L strings are Tcl strings, which are UTF-8 encoded and have a known
length.
L strings are initialized to the empty string.
.LP
To iterate over each character in a string, use the defined() operator:
.CS
int	i;
string	s = "a string";
.sp .5
for (i = 0; defined(s[i]); i++) {
    printf("s[%d]=%s\\n", i, s[i]);
}
.CE
Note that there is no separate character type in L.
When indexing into a string, each character is merely a string of length 1.
This also means that there is no need to use special single-quoted
syntax for character literals:
.CS
str[i] = "c";
.CE
L provides a special escape sequence, ${, which allows embedding code in
strings.
All the text from ${ to the matching } is collected and evaluated.
Its value is then substituted into the string:
.CS
int i = 41;
.sp .5
printf("41 + 1 is ${i + 1}\\n"); 
.CE
prints:
.CS
41 + 1 is 42
.CE
.NH 3
Tclish types
.LI poly.
This is a generic type that is like a Tcl variable on which no type checking
is done.
Normal variables cause compile-time errors if they attempt to
change types; a poly variable suppresses the static type checking so
that a variable can switch from one type to another, e.g. float to
array or to int, etc.
The following is legal code:
.CS
poly	unchecked;
string	s;
.sp .5
unchecked = 1;
unchecked = "Hey there";
unchecked = 3.14;
// cast is required
s = (string)unchecked;
.CE
.LI var.
This is a compromise variable type.
It is type-checked but the type is not
set until the first assignment.
The type is determined from the assignment and may not change.
The following throws an error:
.CS
var	late_binding;
.sp .5
late_binding = 1;
late_binding = "Hey there";
.CE
As we noted above, constant types are intuited.
This might cause problems with @var@ variables.
For example, this throws an error:
.CS
var	f = 1;    // f is now an int
.sp .5
f = "pi";        // int/string error
.CE
but this works fine:
.CS
var	f = 1.0;
.sp .5
f += 3.14;
.CE
.NH 3
Magic
.LI :constant.
Many Tcl/Tk interfaces take key/value pairs that look like
.CS
text .t -bg white -fg black
.CE
which in L might look like
.CS
text(".t",
    "-bg", "white", "-fg", "black");
.CE
We wanted a way to make the @-whatever@ stand out from the values being passed
as an argument to @-whatever@.
We decide to do that like this:
.CS
text(".t",
    :bg, "white", :fg, "black");
.CE
When the parser sees an identifier in a function call that has a leading 
colon, L treats it as if it were a quoted string with the colon replaced
by a dash.
.NH 3
Compound types
.LI array.
Arrays are like C arrays in syntax but are implemented as Tcl lists under
the covers.
Array elements are homogeneous; all elements must share the same type.
Array assignments in declarations are supported for globals and locals:
.CS
string	foo[] = { "Hi", "there" };
int	bar[] = { 1, 2, 3, 4 };
int	i;
int	total = 0;
.sp .5
for (i = 0; defined(bar[i]); i++) {
    total += bar[i];
}
.CE
Arrays are dynamically grown and cannot be sparse.
.CS
int    a[2];
.sp .5
a[0] = 10;
a[100] = 20; // allowed
.CE
After the previous code has been executed, @a@ has 101 elements.
@a[1]@ to @a[99]@ have the value 0, which is the default initial
value for integers.
.LP
The defined operator is an easy way to check if an index is outside
the array bounds:
.CS
// prints 'no'
if (defined(a[101])) {
    printf("yes\\n");
} else {
    printf("no\\n");
}
.CE
.LI hash.
Hashes are associative arrays, indexed by strings and returning string
values.
They are implemented by Tcl dictionaries under the covers.
Hash assignments in declarations are supported for globals and locals
and follow the Perl syntax:
.CS
hash  h = { "key" => "val",
	      "key2" => "val2" };
.sp .5
h{"foo"} = "bar";
if (defined(h{"blech"})) {
   printf("blech is not a key!\\n");
}
.CE
.LP
The defined operator can also be used to check if a key is present in a
hash:
.CS
// prints no
if (defined(foo{"k"})) {
    printf("yes\\n");
} else {
    printf("no\\n");
}
.CE
.br
.ne 10
.LP
It is possible to iterate over each value in a hash using a foreach
loop:
.CS
foreach (h as k => v) {
    printf("%s => %s\\n", k, v);
}
.CE
.LI struct.
Structs are collections of typed variables, as in C.  
Declarations are the same as C declarations.
Struct assignments in declarations are supported for globals and locals:
.CS
typedef struct {
    int	a;
    float	b;
    string	c;
} eg;
.sp .5
eg	s = { 1, 3.14, "hi there" };
.CE
.LP
Structures are implemented as Tcl lists just like L arrays.
The names are translated into integer indices by the L compiler.
Since it is just a Tcl list, an L structure can be passed to any Tcl proc
that expects a list.
.LP
It is likely that we will extend the struct construct to have initializers,
e.g.,
.CS
typedef struct {
    int	a = 1;
    float	b = 3.14;
    string	c = "hi there";
} eg;
.sp .5
eg	foo;
puts(foo.a);    // prints 1
.CE
.NH 2
Passing semantics
.LP
A C programmer, looking at Tcl, would think that the Tcl model is pass by 
value.
While Tcl has no way to pass a C-style pointer to an object, it does have
a way to fake it with something called @upvar@.
L wants pass by value but it also wants to provide pass by reference.
This section describes how we used the Tcl system to provide the L passing
semantics.
It amounts to a little syntactic sugar on top of @upvar@.
.NH 3
By value
.LP
L obeys Tcl's semantics for pass by value.
Parameter passing looks like it does in C:
.CS
int	i = 1234;
.sp .5
foo(i, 0xdeadbeef, "string");
.CE
L programs typically do not pass compound types by value to other
L functions (but see the @(tcl)@ cast below for how to pass them to
Tcl procs).
.br
.ne 8
.NH 3
By reference
.LP
The Tcl system has a way of passing by reference that might appear strange
to C programmers.
.CS
proc foo {ref} {
    upvar $ref pointer

    set pointer 1
}
.CE
The @upvar@ command creates a reference to the variable in the caller's
context and places it in @pointer@.  
Assignments to @pointer@ are the same as if the assignment were done in
the caller's context (after evaluating the right-hand side).
.LP
We used this mechanism to emulate pass by reference in L.
We call it ``pass by name'' because the caller is putting the name
of the variable on the stack and the callee is doing an automatic
@upvar@ to create the reference.
The syntax looks like:
.CS
void foo(int &ref)
{
    ref = 1234;
}

int	a = 19;
.sp .5
foo(&a);
puts(a);
.CE
and that prints
.CS
1234
.CE
Arrays and hashes do not take the ampersand because they are trying to 
behave like C arrays, i.e., they are already references.
.CS
void clear(int v[])
{
    int	i;
.sp .5
    for (i = 0; defined(v[i]); i++) {
        v[i] = 0;
    }
}
.sp .5
int	junk[] = { 1, 2, 3 };
.sp .5
clear(junk);	// junk = { 0, 0, 0 }
.CE
Note that strings, unlike in C, are first-class objects and are
.B not
references.
If you want to modify a string, you must pass it by reference.
For example, to use the Tcl built-in for reading a line of input
you have to do this:
.CS
string	buf;
.sp .5
// buf is an out parameter
gets(stdin, &buf);
.CE
.NH 3
L pointers
.LP
While the @upvar@ trick works nicely for many cases, there is still a need 
for real pointers.
When creating a widget, such as an entry box, it would be natural to 
have a struct that contained all the things related to that widget
such as its path, the variable that the entry box sets, etc., like so:
.CS
widgets(entry &e)
{
    e.frame = frame(".f");
    e.entry = entry("${top}.entry"); 
    e.entry("configure",
        :textvariable, &e.textvar);
}
.CE
Our trick of making an ampersand mean ``push the variable name on the 
stack'' does not work here for multiple reasons.
First, the variable in this case is a structure field, which is an element
of a Tcl list.
There is currently no way to pass a list element as a @-variable@ argument;
Tcl does not support that.
Second, @-variable@ arguments must be accessible at the global scope.
There is no guarantee that the name passed in makes sense at the global scope.
.LP
What is needed is a way to take an L variable and turn it into something
that Tcl can find out of the event loop.
The natural answer is some kind of pointer.
.LP
We created a new Tcl object type to hold all the information related to a
pointer.
The information looks like:
.CS
struct pointer {
    int    depth;  // upvar #depth
    string name;   // var pointed to
    string index;  // optional index
};
.CE
The depth field is used to get to the call frame where the variable being
pointed at was declared. 
For GUI code like the example above, the depth is almost always 0,
indicating a global.
The string is the name of the variable to which the pointer refers.
If the underlying type of the variable is a list (remember that structs
are implemented as lists) then the index is the index into that list.
The index is a string because in the future we intend to make pointers 
into hashes work.
.br
.ne 20
.LP
There is a new Tcl command, @pointer@, which may be used to manipulate
pointers from Tcl directly.
The following code creates a pointer,
points it at the last element of the list @l@,
uses the pointer to get the value of the variable pointed at,
and uses the pointer to set the value of the variable pointed at to @foo@.
When we are done, @$l@ contains \fIa\ b\ foo\fP.
.CS
set l [list a b c]
set p [pointer create l]
pointer index $p 2
pointer get $p          # prints c
pointer set $p foo
.CE
Let's now consider the widget example above, remembering that it had a 
variable reference @&e.textvar@.
The compiler provides some magic to treat that construct as
an L pointer.
When the compiler sees a string constant of the form @-.*variable@\** and
the next token is an L variable with a leading ampersand,
the compiler automatically wraps the variable in an L pointer.
.FS
Remember that @:foo@ token is just syntactic sugar for ``@-foo@.''
.FE
.NH 3
Return values
.LP
Because returns are by value in L, and Tcl also returns by value, 
no changes were required to make returns work in L.
.LP
It is worth noting, especially for C programmers, that there is a sneaky
way to do an allocation.
When a local variable is returned, the return bumps the reference count.
Without that bump, the local variable in question would have been freed
along with any other locals that were on the callee's stack.
Tcl objects are reference counted so the variable will get freed when
the caller is finished with it.
.CS
string[]
vector(int n)
{
    string	v[];
.sp .5
    // Allocate 0..n-1
    v[n - 1] = "";
    return (v);
}

string	foo[] = v(100);
.CE
.NH 2
Casts
.LI (tcl).
There are times when we need to pass a compound object (array,
hash) as a string.
Any Tcl proc that expects to see a string on the stack will want this.
The @(tcl)@ cast is used to do this.
.CS
string	v[] = { "hi", "good day" };
.sp .5
puts((tcl)v);
.CE
prints
.CS
hi {good day}
.CE
.LI (L).
There may be times when a Tcl proc is returning a complex structure to us
and we want to cast it from the Tcl list to our structure:
.CS
#lang(tcl)
proc demo {} {
    return [list {good day} sir]
}
.sp .5
#lang(L)
v = (L)demo();
printf("%s %s\\n", v[0], v[1]);
.CE
prints
.CS
good day sir
.CE
Note: doing this sort of thing puts you at the mercy of the Tcl code
which knows nothing about the L type system.
.NH 2
Operators
.LP
L supports most of the operators in the C programming language, as well
as several of the most useful operators from Perl.
In this section we do a quick run through all of the operators in L
and discuss some of their more subtle aspects in depth.
.LP
Much of this section is cribbed from the C reference manual.
.[
C
.]
.NH 3
Arithmetic operators
.LP
The binary arithmetic operators in L are +, -, *, /, and % (modulus).
They work as in C with the C precedence rules.
.NH 3
True vs. false
.LP
All of the relational and logical operators are part of an expression and
that expression evaluates to either true or false.
.LP
In L, there is only one false value.
This is different from Tcl, which allows many false values, such as the
strings ``false'' and ``off.''
The false value in L is 0, or, equivalently, ``0''.
Any value other than 0 is considered true.
.CS
if (0) {
    printf("consequent\\n");
} else {
    printf("alternative\\n");
}
.CE
prints: \f(CWalternative\fP
.br
.ne 20
.NH 3
Numeric Comparison
.LP
These all work as in C with the C precedence rules.
.sp .25
.B "Relational operators"
.CS
@expr@ > @expr@
@expr@ >= @expr@
@expr@ < @expr@
@expr@ <= @expr@
.CE
.B "Equality operators"
.CS
@expr@ == @expr@
@expr@ != @expr@
.CE
.LI "Logical Operators"
.sp .25
The && and || operators short-circuit as in C.
.CS
@expr@ && @expr@
@expr@ || @expr@
!@expr@
.CE
.NH 3
Regular expression operators
.LP
Stolen from Perl, the first form is true if @regex@ is a regular expression
that matches @string@.  
The second form is true if @regex@ is a regular expression
that does not match @string@.  
The @//@ construct is an alias for a double quoted string,
which means that all or part
of the string may be an interpolated variable (or expression).
The @m||@ construct is also from perl; it means use the vertical bars instead
of slashes (frequently useful when dealing with path names).
.CS
@string@ =~ /@regex@/
@string@ !~ /@regex@/
@string@ =~ m|\fI${expr}\fP|
.CE
.ig
.NH 3
String Comparison
.LP
To use a numeric operator on a string is a type error in L.
Instead of extending the numeric operators to work on strings, L provides
a separate set of string operators.
.LP
**** relational operators

    gt      Greater Than 
    ge      Greater Than or Equal
    lt      Less Than
    le      Less Than or Equal

**** equality operators

    eq      Equal
    ne      Not equal
..
.NH 3
Increment and Decrement Operators
.LP
As in C, with the value returned either before or after the 
increment or decrement.
.CS
@lvalue@++
++@lvalue@
@lvalue@--
--@lvalue@
.CE
.NH 3
Bitwise Operators
.CS
@expr@ & @expr@
@expr@ | @expr@
@expr@ ^ @expr@
@expr@ << @expr@
@expr@ >> @expr@
~@expr@
.CE
.ne 10
.NH 3
Assignment Operators
.CS
@lvalue@ = @expr@
@lvalue@ += @expr@
@lvalue@ -= @expr@
@lvalue@ *= @expr@
@lvalue@ /= @expr@
@lvalue@ %= @expr@
@lvalue@ <<= @expr@
@lvalue@ >>= @expr@
@lvalue@ &= @expr@
@lvalue@ |= @expr@
@lvalue@ ^= @expr@
.CE
.NH 3
Ternary Operator
.CS
@expr@ ? @expr@ : @expr@
.CE
.NH 2
Reserved Words
.LP
These are L's reserved words:
.CS
break case continue defined do
else float for foreach if int L
poly return string struct switch
tcl typedef unless until var void
while
.CE
.NH 2
Control flow
.LI Conditional statements
.CS
if ( @expr@ ) @statement@
if ( @expr@ ) @statement@ else @statement@
unless ( @expr@ ) @statement@
.CE
In all cases @expr@ is evaluated and if it returns anything other than
zero, then the first 
.B if
statement is executed.
If it returns zero, then the 
.B else 
statement or the 
.B unless
statement is executed.
.LI While/until statements
.CS
while ( @expr@ ) @statement@
until ( @expr@ ) @statement@
.CE
The @expr@ is evaluated and @statement@ is executed repeatedly while
@expr@ is non-zero in the 
.B while 
case, or zero in the
.B until 
case.
.LI do statements
.CS
do @statement@ while ( @expr@ )
do @statement@ until ( @expr@ )
.CE
@statement@ is executed repeatedly while @expr@ is non-zero in the
.B while 
case, or until non-zero in the
.B until
case.
.br
.ne 10
.LI for statement
.CS
for ( @exp1 sub opt@; @exp2 sub opt@; @exp3 sub opt@ ) @statement@
.CE
All expressions are optional.
Other than the continue statement, which in this case executes
@exp3@, this is the same as
.CS
@exp1@;
while ( @exp2@ ) {
    @statement@
    @exp3@;
}
.CE
.LI foreach statement
.CS
foreach (@h@ as @key@ => @val@) @statement@
foreach (@p@ in @v@) @statement@
.CE
The first statement iterates over each key/value pair in the hash @h@.
The key/value pair is placed in @key@ and @val@ 
and then @statement@ is executed.
Behavior is undefined if keys are inserted or deleted in @h@ in @statement@.
The second statement sets @p@ to each element of @v@, calling @statement@
once per element.
.LI switch statement
.CS
switch ( @expr@ ) @statement@
.CE
@expr@ must evaluate to an 
.B int
or a
.BR string .
Any statement within @statement@ may contain one or more labeled statements
of the form
.CS
case @constant-expr@: @statement@
case /@constant-expr@/: @statement@
case <@constant-expr@>: @statement@
.CE
There may be at most one statement of the form:
.CS
default: @statement@
.CE
When the 
.B switch 
statement is run, @expr@ is evaluated and jumps to the 
.B case
label that matches.
Case labels may be double-quoted string constants,
integer constants (not floats),
constant regular expressions (@/.*.[ch]/@),
or constant globs (@<*.[ch]>@).
If no label matches, then if the 
.B default
label exists, a jump to the 
.B default 
label occurs.
As in C, control continues to flow past labels; see
the \(lqbreak statement\(rq for exiting from a 
.BR switch .
.LI break
.CS
break ;
.CE
causes termination of the smallest enclosing 
.BR while ,
.BR until ,
.BR do ,
.BR for ,
or
.B switch
statement.
.LI continue
.CS
continue ;
.CE
causes control to pass to the loop-continuation portion of the smallest 
enclosing
.BR while ,
.BR until ,
.BR do ,
or
.B for 
loop.
.LI return
.CS
return;
return ( @expr@ );
.CE
In the first case the return value is undefined.
In the second, the return value is @expr@.
.NH 2
Changes to Tcl
.LP
In the course of implementing L, two small but important changes were
made to Tcl that could affect all Tcl programs, although we don't
expect the effects to be visible.
.NH 3
Top-level Compilation
.LP
Top-level code in Tcl, i.e., code that isn't contained in a proc body,
is now passed to the byte-code compiler.
We require this so that the L compiler can emit byte code for top-level L
code.
It could be useful in the future for saving Tcl byte code between
invocations, similar to the TclPro compiler.
.NH 3
Changes to the Tcl Parser
.LP
The @#lang(tcl)@ string forces the language to be Tcl, the 
@#lang(L)@ forces the language to be L.
It is allowed to have snippets of both L and Tcl in the same source file.
.LP
When Tcl starts up with a file argument, if the file ends in @.l@ then
@#lang(L)@ is implicit.
The default is to start up in Tcl mode.
.LP
Tcl's @Tcl_ParseCommand@ has been modified to recognize a
comment with a special form. Whenever the parser sees @#lang(L)@ it
stops normal parsing and inserts two tokens into the token stream. The
first token is a call to the @LCompileCommand@ function and the second
is the text after the @#lang(L)@ comment up to the next @#lang(tcl)@
comment or end-of-file.
.EQ
delim ||
.EN
.NH 1
Status
.LP
The L language is under active development and the speed of development
is increasing.
Our expectation is that we will have a usable system in 1-2 months.
Our goal is to be rewriting our GUI tools in L early in 2007.
There is a mailing list, \f(CWl@bitmover.com\fP, and an IRC channel,
\f(CW##l\fP on \f(CWFreenode\fP.
People are welcome to join either.
.NH 1
Future work
.NH 2
Scoping
.LP
Like a C source file, a scope provides a container for private and/or
public variables and/or functions.
This could be used to provide a self-contained ``class.''
.NH 2
Pre-compiled modules
.LP
Imagine that each scope is a module and each module can be pre-compiled.
The on-disk format is in sections:  there is a byte-code section and a
sort of table of contents which can be thought of as a header file containing
function prototypes.
.NH 2
Optimizations
.LP
The dynamic nature of Tcl means that many traditional compiler optimization
techniques cannot be used.
L compiles the source to an abstract syntax tree and could take advantage
of a number of well known optimizations.
These include: constant subexpression elimination,
dead code removal, strength reduction, loop invariant code
motion, tail-call optimization, code hoisting, and others.
.[
optimization
.]
.LP
The lack of general C-like pointers in L greatly simplifies alias analysis
and makes it possible to be more aggressive when applying optimizations.
.[
aliasing1
.]
.[
aliasing2
.]
.NH 2
Debugging
.LP
The static nature of L code would make it possible to create a
mapping between L source code and Tcl byte codes such that traditional
debugging techniques could be used. One possible approach would be to
instrument the generated byte code to invoke a debugger every time an L
statement completes. 
.NH 1
Licensing and availability
.LP
The license is the Tcl license; L is part of Tcl as far as we are
concerned.
.LP
The source is maintained in a BitKeeper repository which is an import of
the CVS Tcl repository.
For the 3 people in the world who won't use BK, we will do nightly tarballs
and make them available on our FTP server.
.NH 1
Conclusion
.LP
This paper has described the L programming language.
The L language is unique in that it is an alternate syntax which peacefully
coexists with the Tcl/Tk system and leverages all of that system.
.LP
Over the course of the next year we expect to use L to rewrite our GUI systems.
Given that L is a young language, we expect that it will continue to evolve
as we use it.
It is likely that we will publish 
an updated version of this paper after the language stabilizes.
.NH 1
Acknowledgements
.LP
The L language draws heavily from the C language.  It's hard to imagine 
that Brian, Dennis and Ken want any more pats on the back, but here is one
more.  We are definitely C fans.
.LP
Rob Netzer, Brian Griffin, and Mark Roseman were helpful in
talking over various language problems and ideas.
.LP
John Ousterhout for Tcl/Tk, introduced in 1988 and still going strong.
.LP
Kennan Rossi for being there as always with editorial help.
.LP
This paper was typeset using groff and as always we thank Joe Ossana for
troff and James Clark for groff.
.[
$LIST$
.]
.bp
.de CS
.sp .25
.KS
.in +.5
.ft CW
.nf
.ps 9
.vs 10
..
.de CE
.sp .25
.in
.ft
.ps
.vs
.fi
.KE
..
.SH
Appendix - code samples
.SH
A simple cat
.CS
int
main(int ac, string av[])
{
    int	i;
    FILE	fd;

    if (ac == 1) {
        puts(:nonewline, read(stdin));
        return (0);
    }
    for (i = 1; defined(av[i]); i++) {
        fd = open(av[i], "r");
        puts(:nonewline, read(fd));
    }
}
.CE
.SH
A simple grep
.CS
int
main(int ac, string av[])
{
    int     i, rc;
    string  regex;
    FILE    fd;
.sp .5
    if (ac < 2) {
        // Tcl's [error]
        error("Not enough arguments.");
    }
    regex = av[1];
    ac--;
    if (ac == 1) {
        rc = grep(regex, stdin) ? 0 : 1;
        return (rc);
    } else {
	rc = 1;
        for (i = 2; i < ac; i++) {
            fd = open(av[i], "r");
            if (grep(regex, fd))) rc = 0;
            close(fd);
        }
        return (rc);
    }
    
}

int
grep(string regex, FILE in)
{
    string buf;
    int	  matches = 0;
.sp .5
    while (gets(in, &buf) >= 0) {
        if (buf =~ /${regex}/) {
            printf("%s\\n", buf);
	    matches++;
        }
    }
    return (matches);
}
.CE    
.ne 20
.SH
Fibonacci
.CS
main()
{
    int fib[] = fib(100);
.sp .5
    for (i=0; defined(fib[i]); i++) {
        printf("%d\\t%d\\n", i, fib[i]);
    }
}

int[]
fib(int n)
{
    int    fib[] = { 0, 1 };
    int    i;
.sp .5    
    for (i=2; i<n; i++) {
        fib[i] = fib[i-1] + fib[i-2];
    }
    return (fib);
}
.CE    
.SH
Quicksort
.CS
/*
 * qsort:
 * sort v[left]...v[right]
 * into increasing order.
 * From K&R C, verbatim.
 */
void qsort(int v[], int left, int right)
{
    int i, last;
.sp .5
    if (left >= right)
        return;
    swap(v, left, (left + right)/2);
    last = left;
    for (i = left+1; i<= right; i++)
        if (v[i] < v[left])
            swap(v, ++last, i);
        swap(v, left, last);
        qsort(v, left, last-1);
        qsort(v, last+1, right);
}

/* swap: interchange v[i] and v[j] */
void swap(int v[], int i, int j)
{
    int temp;
.sp .5
    temp = v[i];
    v[i] = v[j];
    v[j] = temp;
}
.CE
