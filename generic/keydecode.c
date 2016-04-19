#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *
keydecode(char *key)
{
	unsigned int len, half, i, j;
	char	*newkey;

	if (key == NULL) {
err:		return (key);
	}
	len = strlen(key);
	half = len / 2;
	if ((len %2) != 0) goto err; /* len must be even */
	
	newkey = malloc(len);
	
	/* unpack the old bytes */
	for (j = 1, i = 0; i < half; i++, j +=2) newkey[j] = key[i];

	/* unpack the even bytes */
	for (j = 0, i = half; i < len; i++, j +=2)  newkey[j] = key[i];

	memcpy(key, newkey, len);
	free(newkey);
	return (key);
}
