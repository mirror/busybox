/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2008 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "libbb.h"

/*
 * The strrstr() function finds the last occurrence of the substring needle
 * in the string haystack. The terminating nul characters are not compared.
 */
char* strrstr(const char *haystack, const char *needle)
{
	char *r = NULL;

	if (!needle[0])
			return r;
	while (1) {
			char *p = strstr(haystack, needle);
			if (!p)
					return r;
			r = p;
			haystack = p + 1;
	}
}

#ifdef __DO_STRRSTR_TEST
/* Test */
int main(int argc, char **argv)
{
	int ret = 0;
	int n;
	char *tmp;

	ret |= !(n = ((tmp = strrstr("baaabaaab", "aaa")) != NULL && strcmp(tmp, "aaab") == 0));
	printf("'baaabaaab'  vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = ((tmp = strrstr("baaabaaaab", "aaa")) != NULL && strcmp(tmp, "aaab") == 0));
	printf("'baaabaaaab' vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = ((tmp = strrstr("baaabaab", "aaa")) != NULL && strcmp(tmp, "aaabaab") == 0));
	printf("'baaabaab'   vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = (strrstr("aaa", "aaa") != NULL));
	printf("'aaa'        vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = (strrstr("aaa", "a") != NULL));
	printf("'aaa'        vs. 'a'         : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = (strrstr("aaa", "bbb") == NULL));
	printf("'aaa'        vs. 'bbb'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = (strrstr("a", "aaa") == NULL));
	printf("'a'          vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = ((tmp = strrstr("aaa", "")) != NULL && strcmp(tmp, "aaa") == 0));
	printf("'aaa'        vs. ''          : %s\n", n ? "FAILED" : "PASSED");

	ret |= !(n = (strrstr("", "aaa") == NULL));
	printf("''           vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");

	ret |= !(n = ((tmp = strrstr("", "")) != NULL && strcmp(tmp, "") == 0));
	printf("''           vs. ''          : %s\n", n ? "PASSED" : "FAILED");

	/*ret |= !(n = (strrstr(NULL, NULL) == NULL));
	printf("'NULL'       vs. 'NULL'      : %s\n", n ? "PASSED" : "FAILED");
	ret |= !(n = (strrstr("", NULL) == NULL));
	printf("''           vs. 'NULL'      : %s\n", n ? "PASSED" : "FAILED");
	ret |= !(n = (strrstr(NULL, "") == NULL));
	printf("'NULL'       vs. ''          : %s\n", n ? "PASSED" : "FAILED");
	ret |= !(n = (strrstr("aaa", NULL) == NULL));
	printf("'aaa'        vs. 'NULL'      : %s\n", n ? "PASSED" : "FAILED");
	ret |= !(n = (strrstr(NULL, "aaa") == NULL));
	printf("'NULL'       vs. 'aaa'       : %s\n", n ? "PASSED" : "FAILED");*/

	return ret;
}
#endif
