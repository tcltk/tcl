/*
 * regcomp and regexec - front ends to re_ routines
 *
 * Mostly for implementation of backward-compatibility kludges.  Note
 * that these routines exist ONLY in char versions.
 */

#include "regguts.h"

/*
 - regcomp - compile regular expression
 */
int
regcomp(re, str, flags)
regex_t *re;
CONST char *str;
int flags;
{
	size_t len;
	int f = flags;

	if (f&REG_PEND) {
		len = re->re_endp - str;
		f &= ~REG_PEND;
	} else
		len = strlen(str);

	return re_comp(re, str, len, f);
}

/*
 - regexec - execute regular expression
 */
int
regexec(re, str, nmatch, pmatch, flags)
regex_t *re;
CONST char *str;
size_t nmatch;
regmatch_t pmatch[];
int flags;
{
	CONST char *start;
	size_t len;
	int f = flags;

	if (f&REG_STARTEND) {
		start = str + pmatch[0].rm_so;
		len = pmatch[0].rm_eo - pmatch[0].rm_so;
		f &= ~REG_STARTEND;
	} else {
		start = str;
		len = strlen(str);
	}

	return re_exec(re, start, len, nmatch, pmatch, f);
}
