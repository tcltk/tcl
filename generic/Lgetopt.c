#include "Lcompile.h"

/* for compat with code below */
#define streq(a, b)	(!strcmp(a, b))
#define strneq(a, b, n)	(!strncmp(a, b, n))
#define assert(x)

/*
 * Copyright (c) 1997 L.W.McVoy
 *
 * This version handles
 *
 *	-	(leaves it and returns)
 *	--	end of options
 *	-a
 *	-abcd
 *	-r <arg>
 *	-r<arg>
 *	-abcr <arg>
 *	-abcr<arg>
 *	-r<arg> -R<arg>, etc.
 *	--long
 *	--long:<arg>
 *	--long=<arg>
 *	--long <arg>
 *
 * Patterns in getopt string:
 *	d	boolean option		-d
 *	d:	required arg		-dARG or -d ARG
 *	d;	required arg no space   -dARG
 *	d|	optionial arg no space	-dARG or -d
 *
 * With long options:
 *	long	boolean option		--long
 *	long:	required arg		--long=ARG or --long ARG
 *	long;	required arg no space   --long=ARG
 *	long|	optionial arg no space	--long=ARG or --long
 */

int	optopt;		/* option that is in error, if we return an error */
int	optind;		/* next arg in argv we process */
char	*optarg;	/* argument to an option */
static int n;		/* current position == av[optind][n] */
static int lastn;	/* saved copy of last n */

private	int	doLong(int ac, char **av, longopt *lopts);

void
getoptReset(void)
{
	optopt = optind = 0;
	optarg = 0;
}

void
getoptConsumed(int n1)
{
	optind--;
	unless (optind) optind = 1;
	n = lastn + n1;
	// TRACE("optind = %d, n = %d, n1 = %d", optind, n, n1);
}

/*
 * Returns:
 *    - char if option found
 *    - GETOPT_EOF(-1) if end of arguments reached
 *    - GETOPT_ERR(256) if unknown option found.
 */
int
getopt(int ac, char **av, char *opts, longopt *lopts)
{
	char	*t;

	optarg = 0;	/* clear out arg from last round */
	optopt = 0;	/* clear error return */
	if (!optind) {
		optind = 1;
		lastn = n;
		n = 1;
	}
	// TRACE("GETOPT ind=%d n=%d av[%d]='%s'", optind, n, optind, av[optind]);

	if ((optind >= ac) || (av[optind][0] != '-') || !av[optind][1]) {
		return (GETOPT_EOF);
	}
	/* Stop processing options at a -- and return arguments after */
	if (streq(av[optind], "--")) {
		optind++;
		lastn = n;
		n = 1;
		return (GETOPT_EOF);
	}
	if (strneq(av[optind], "--", 2)) return (doLong(ac, av, lopts));

	assert(av[optind][n]);
	for (t = (char *)opts; *t; t++) {
		if (*t == av[optind][n]) {
			break;
		}
	}
	if (!*t) {
		optopt = av[optind][n];
		// TRACE("%s", "ran out of option letters");
		lastn = n;
		if (av[optind][n+1]) {
			n++;
		} else {
			n = 1;
			optind++;
		}
		return (GETOPT_ERR);
	}

	/* OK, we found a legit option, let's see what to do with it.
	 * If it isn't one that takes an option, just advance and return.
	 */
	if (t[1] != ':' && t[1] != '|' && t[1] != ';') {
		lastn = n;
		if (!av[optind][n+1]) {
			optind++;
			n = 1;
		} else {
			n++;
		}
		// TRACE("Legit singleton %c", *t);
		return (*t);
	}

	/* got one with an option, see if it is cozied up to the flag */
	if (av[optind][n+1]) {
		optarg = &av[optind][n+1];
		optind++;
		lastn = n;
		n = 1;
		// TRACE("%c with %s", *t, optarg);
		return (*t);
	}

	/* If it was not there, and it is optional, OK */
	if (t[1] == '|') {
		optind++;
		lastn = n;
		n = 1;
		// TRACE("%c without arg", *t);
		return (*t);
	}

	/* was it supposed to be there? */
	if (t[1] == ';') {
		optind++;
		optopt = *t;
		// TRACE("%s", "wanted another word");
		return (GETOPT_ERR);
	}

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		optopt = av[optind][n];
		// TRACE("%s", "wanted another word");
		return (GETOPT_ERR);
	}
	optarg = av[optind+1];
	optind += 2;
	lastn = n;
	n = 1;
	// TRACE("%c with arg %s", *t, optarg);
	return (*t);
}

private int
doLong(int ac, char **av, longopt *lopts)
{
	char	*s, *t;
	int	len1, len2;

	unless (lopts) {
err:		n = 1;
		optind++;
		optopt = 0;
		return (GETOPT_ERR);
	}
	/* len of option without =value part */
	s = av[optind] + 2;
	unless (t = strchr(s, '=')) t = strchr(s, ':');
	len1 = t ? (t - s) : strlen(s);
	for (; (t = lopts->name); lopts++) {
		s = av[optind] + 2;
		/* len of lopts array without suffix */
		len2 = strlen(t);
		if (strspn(t+len2-1, ":;|") == 1) --len2;

		if ((len1 == len2) && strneq(s, t, len1)) {
			s += len1;
			t += len2;
			break;	/* found a match */
		}
	}
	unless (t) goto err;

	/* OK, we found a legit option, let's see what to do with it.
	 * If it isn't one that takes an option, just advance and return.
	 */
	unless (*t) {
		/* got option anyway */
		if ((*s == '=') || (*s == ':')) goto err;
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* got one with an option, see if it is cozied up to the flag */
	if ((*s == '=') || (*s == ':')) {
		optarg = s + 1;
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* If it was not there, and it is optional, OK */
	if (*t == '|') {
		optind++;
		n = 1;
		return (lopts->ret);
	}

	/* was it supposed to be there? */
	if (*t == ';') goto err;

	/* Nope, there had better be another word. */
	if ((optind + 1 == ac) || (av[optind+1][0] == '-')) {
		goto err;
	}
	optarg = av[optind+1];
	optind += 2;
	n = 1;
	return (lopts->ret);
}
