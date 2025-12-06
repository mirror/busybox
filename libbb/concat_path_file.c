/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* Concatenate path and filename to new allocated buffer.
 * Add '/' only as needed (no duplicate // are produced).
 * If path is NULL, it is assumed to be "/".
 * filename should not be NULL.
 */

char* FAST_FUNC concat_path_file(const char *path, const char *filename)
{
#if 0
	char *lc;

	if (!path)
		path = "";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;
	return xasprintf("%s%s%s", path, (lc==NULL ? "/" : ""), filename);
#else
/* ^^^^^^^^^^^ timing of xasprintf-based code above:
 * real 7.074s
 * user 0.156s <<<
 * sys  6.394s
 *	"rm -rf" of a Linux kernel tree from tmpfs (run time still dominated by in-kernel work, though)
 * real 6.989s
 * user 0.055s <<< 3 times less CPU used
 * sys  6.450s
 * vvvvvvvvvvv timing of open-coded malloc+memcpy code below (+59 bytes):
 */
	char *buf, *p;
	size_t n1, n2, n3;

	while (*filename == '/')
		filename++;

	if (!path || !path[0])
		return xstrdup(filename);

	n1 = strlen(path);
	n2 = (path[n1 - 1] != '/'); /* 1: "path has no trailing slash" */
	n3 = strlen(filename) + 1;

	buf = xmalloc(n1 + n2 + n3);
	p = mempcpy(buf, path, n1);
	if (n2)
		*p++ = '/';
	memcpy(p, filename, n3);
	return buf;
#endif
}

/* If second component comes from struct dirent,
 * it's possible to eliminate one strlen() by using name length
 * provided by kernel in struct dirent. See below.
 * However, the win seems to be insignificant.
 */

#if 0

/* Extract d_namlen from struct dirent */
static size_t get_d_namlen(const struct dirent *de)
{
#if defined(_DIRENT_HAVE_D_NAMLEN)
	return de->d_namlen;
#elif defined(_DIRENT_HAVE_D_RECLEN)
	const size_t prefix_sz = offsetof(struct dirent, d_name);
	return de->d_reclen - prefix_sz;
#else
	return strlen(de->d_name);
#endif
}

char* FAST_FUNC concat_path_dirent(const char *path, const struct dirent *de)
{
	char *buf, *p;
	size_t n1, n2, n3;

	if (!path || !path[0])
		return xstrdup(de->d_name);

	n1 = strlen(path);
	n2 = (path[n1 - 1] != '/');
	n3 = get_d_namlen(de) + 1;

	buf = xmalloc(n1 + n2 + n3);
	p = mempcpy(buf, path, n1);
	if (n2)
		*p++ = '/';
	memcpy(p, de->d_name, n3);
	return buf;
}

#endif
