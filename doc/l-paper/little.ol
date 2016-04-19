Outline
	What is it?
	Why is that interesting?
	Why not some other runtime?
	Why not just tcl?
	Language details
	Code examples
	Status or what doesn't work yet
	Future directions
	License and Availability
	What people are saying 
What is it?
	C-like syntax on top of the Tcl runtime
	Alternate compiler that generates tcl byte codes
	Does not change tcl system
	Interoperates with the tcl system
	Leverages the tcl runtime
	An example of a multi-syntax system (can have tcl & L in same file)
Why not some other runtime?
	Lotso good stuff about the tcl runtime
Why not pure tcl?
	Tcl cons from below
Why did we do it?
	Already comitted to tcl/tk
	Tcl pros
		Stable runtime
		Easy to extend in C
		Powerful, dynamic language
	Tcl cons
		Hard to read (for us)
		No structs - probably the single biggest reason
		Lots of missing syntactic sugar
			foo[3] or [lindex foo 3]
			if (buf =~ /blah.*blech/)
			or
			if {[regexp blah.*blah buf]}
			etc.
			Pass by reference with upvar is annoying
	We're a conservative development organization.	We sell
	to enterprise customers and we support releases going back
	indefinitely in some cases.  All code goes through peer review and
	is optimized for ease of reading more than for ease of writing.
	We found tcl to be suboptimal in this environment, particularly
	because of the lack of structs but also because of the lack of 
	syntactic sugar - calling procs to do array indexing is way over
	our threshold of pain for readability.
Language details
	C-like syntax compiled to tcl byte code
		L can call tcl, tcl can call L
	Additions over C
		perl like regex in statements
		associative arrays
		defined() for variables, hashes, arrays
			defined(foo)		[info exists] or winfo ???
			defined(foo{"bar"})
			defined(blech[2])
		strings are a basic type like int/float
	Additions over tcl
		structs
		type checking
		pass by reference improvements
		function fingerprints
	Types
		string (same as tcl string)
		int, float (type checked)
		var (unknown type, strongly typed on first assignment)
		poly (like tcl variable, no type checking)
		hash (associative array, currently string types for key/val)
			Implemented as dicts
			string types as key/values
			We could allow
				hash	poly foo{poly}
			if we ever want that fucked up syntax.
		structs
			like C structs
			implemented as lists
			fields are in ::L::{struct name} as a list of lists,
			each list is {type name comments}
			struct struct {
				string	type;
				string	name;
				string	comments;
			}
	Pass by reference or value?
		base types are all by value, COW, like tcl
		arrays and hashes are implicit references
			pass an array to a proc, modify array[3], caller sees
		base types and structures may be passed by reference
			pass by reference is done with &variable in the
			caller which is an alias for "variable", i.e., 
			we pass by name.
			In the callee the argument also wants a & and it means
			do an automagic upvar to get the real variable
	Returns
		All returns are by value
			all types just work as expected
				push the object onto the stack
				the other end gets the object into it's 
				variable name.
Code examples
	printenv.l
Status or what doesn't work yet
	Well there are issues
Future directions
	Scoping so you can do modules
	Precompiled modules (and byte code loader)
	Optimizations
	Debugging support
	Having an L contest for the best ($10K seem right?)
License and Availability
	Same license as Tcl
	Exported as tarballs nightly
What people are saying
	"It'll make your jaw drop"
	    -- Steve Jobs
	"It's an amazing, amazing innovation."
	    -- Steve Ballmer
	"I want to thank Larry McVoy" 
	    -- Richard Stallman
	"L == Tcl 9.0"
	    -- Jeff Hobbs
	"L?  Of course I like it.  It's named for me.  Right?"
	    -- Linus Torvalds
	"It's like perl without the nastiest bits"
	    -- Donal K Fellows
	"I think that's easier to read."
	    -- Larry Wall
	"L is the coolest thing I've ever seen"
	    -- Vadim Gelfer
