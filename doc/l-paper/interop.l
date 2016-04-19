#lang(L)

/*
 * Demonstrate interop between L and tcl.
 * We want to show that you can pass any data structure between L and tcl.
 *
 * Base types, pass them, modify them.
 * Structures, pass them, iterate over the fields.
 * Arrays & hashes.
 * Pointers to base types.
 */
main(int ac, string av[])
{
	
}

/*
 * L calling tcl functions
 */
L_to_tcl()
{
	string	s = "Hi there mom";
	int	a = 1234;

	puts(a);			// prints 1234\n
	puts(s);			// prints Hi there mom\n
	set(&s, "Hi yourself");
	puts(s);			// prints Hi yourself\n
	puts(:nonewline, "Hi ");	// prints Hi 
	puts("there");			// prints there\n
}

set(string &out, string value)
{
	out = value;
}
