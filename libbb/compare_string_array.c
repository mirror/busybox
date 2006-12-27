/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* returns the array index of the string */
/* (index of first match is returned, or -1) */
int index_in_str_array(const char * const string_array[], const char *key)
{
	int i;

	for (i = 0; string_array[i] != 0; i++) {
		if (strcmp(string_array[i], key) == 0) {
			return i;
		}
	}
	return -1;
}

/* returns the array index of the string, even if it matches only a beginning */
/* (index of first match is returned, or -1) */
int index_in_substr_array(const char * const string_array[], const char *key)
{
	int i;
	int len = strlen(key);
	if (!len)
		return -1;

	for (i = 0; string_array[i] != 0; i++) {
		if (strncmp(string_array[i], key, len) == 0) {
			return i;
		}
	}
	return -1;
}
