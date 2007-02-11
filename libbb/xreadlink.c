/* vi: set sw=4 ts=4: */
/*
 *  xreadlink.c - safe implementation of readlink.
 *  Returns a NULL on failure...
 */

#include "libbb.h"

/*
 * NOTE: This function returns a malloced char* that you will have to free
 * yourself. You have been warned.
 */

char *xmalloc_readlink_or_warn(const char *path)
{
	enum { GROWBY = 80 }; /* how large we will grow strings by */

	char *buf = NULL;
	int bufsize = 0, readsize = 0;

	do {
		buf = xrealloc(buf, bufsize += GROWBY);
		readsize = readlink(path, buf, bufsize); /* 1st try */
		if (readsize == -1) {
			bb_perror_msg("%s", path);
			free(buf);
			return NULL;
		}
	}
	while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}

char *xmalloc_realpath(const char *path)
{
#if defined(__GLIBC__) && !defined(__UCLIBC__)
	/* glibc provides a non-standard extension */
	return realpath(path, NULL);
#else
	char buf[PATH_MAX+1];

	/* on error returns NULL (xstrdup(NULL) ==NULL) */
	return xstrdup(realpath(path, buf));
#endif
}
