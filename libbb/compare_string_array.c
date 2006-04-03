/* vi:set ts=4:*/
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <string.h>
#include "libbb.h"

/* returns the array number of the string */
int compare_string_array(const char * const string_array[], const char *key)
{
	int i;

	for (i = 0; string_array[i] != 0; i++) {
		if (strcmp(string_array[i], key) == 0) {
			return i;
		}
	}
	return -i;
}

