/*
 * regfree - free an RE
 *
 * You might think that this could be incorporated into regcomp.c, and
 * that would be a reasonable idea... except that this is a generic
 * function (with a generic name), applicable to all compiled REs
 * regardless of the size of their characters, whereas the stuff in
 * regcomp.c gets compiled once per character size.
 */

#include "regguts.h"

/*
 - regfree - free an RE (generic function, punts to RE-specific function)
 *
 * Ignoring invocation with NULL is a convenience.
 */
VOID
regfree(re)
regex_t *re;
{
	if (re == NULL)
		return;
	(*((struct fns *)re->re_fns)->free)(re);
}
