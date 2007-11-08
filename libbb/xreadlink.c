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
char *xmalloc_readlink(const char *path)
{
	enum { GROWBY = 80 }; /* how large we will grow strings by */

	char *buf = NULL;
	int bufsize = 0, readsize = 0;

	do {
		bufsize += GROWBY;
		buf = xrealloc(buf, bufsize);
		readsize = readlink(path, buf, bufsize);
		if (readsize == -1) {
			free(buf);
			return NULL;
		}
	} while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}

/*
 * this routine is not the same as realpath(), which canonicalizes
 * the given path completely.  this routine only follows trailing
 * symlinks until a real file is reached, and returns its name. 
 * intermediate symlinks are not expanded.  as above, a malloced
 * char* is returned, which must be freed.
 */
char *xmalloc_readlink_follow(const char *path)
{
	char *buf;
	char *lpc;
	char *linkpath;
	int bufsize;
	int looping = MAXSYMLINKS + 1;

	linkpath = xstrdup(path);
	goto jump_in;

	while (1) {
		if (!--looping) {
			free(linkpath);
			free(buf);
			return NULL;
		}
		linkpath = xmalloc_readlink(buf);
		if (!linkpath) {
			if (errno == EINVAL) /* not a symlink */
				return buf;
			free(buf);
			return NULL;
		} 
		if (linkpath[0] != '/') {
			bufsize += strlen(linkpath);
			buf = xrealloc(buf, bufsize);
			lpc = bb_get_last_path_component_strip(buf);
			strcpy(lpc, linkpath);
			free(linkpath);
		} else {
			free(buf);
 jump_in:
			buf = linkpath;
			bufsize = strlen(buf) + 1;
		}
	}
}

char *xmalloc_readlink_or_warn(const char *path)
{
	char *buf = xmalloc_readlink(path);
	if (!buf) {
		/* EINVAL => "file: Invalid argument" => puzzled user */
		bb_error_msg("%s: cannot read link (not a symlink?)", path);
	}
	return buf;
}

/* UNUSED */
#if 0
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
#endif
