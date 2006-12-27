/* vi: set sw=4 ts=4: */
/*
 * parse_mode implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Mar 5, 2003    Manuel Novoa III
 *
 * This is the main work function for the 'mkdir' applet.  As such, it
 * strives to be SUSv3 compliant in it's behaviour when recursively
 * making missing parent dirs, and in it's mode setting of the final
 * directory 'path'.
 *
 * To recursively build all missing intermediate directories, make
 * sure that (flags & FILEUTILS_RECUR) is non-zero.  Newly created
 * intermediate directories will have at least u+wx perms.
 *
 * To set specific permissions on 'path', pass the appropriate 'mode'
 * val.  Otherwise, pass -1 to get default permissions.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libbb.h"

int bb_make_directory (char *path, long mode, int flags)
{
	mode_t mask;
	const char *fail_msg;
	char *s = path;
	char c;
	struct stat st;

	mask = umask(0);
	if (mode == -1) {
		umask(mask);
		mode = (S_IXUSR | S_IXGRP | S_IXOTH |
				S_IWUSR | S_IWGRP | S_IWOTH |
				S_IRUSR | S_IRGRP | S_IROTH) & ~mask;
	} else {
		umask(mask & ~0300);
	}

	do {
		c = 0;

		if (flags & FILEUTILS_RECUR) {	/* Get the parent. */
			/* Bypass leading non-'/'s and then subsequent '/'s. */
			while (*s) {
				if (*s == '/') {
					do {
						++s;
					} while (*s == '/');
					c = *s;		/* Save the current char */
					*s = 0;		/* and replace it with nul. */
					break;
				}
				++s;
			}
		}

		if (mkdir(path, 0777) < 0) {
			/* If we failed for any other reason than the directory
			 * already exists, output a diagnostic and return -1.*/
			if (errno != EEXIST
				|| !(flags & FILEUTILS_RECUR)
				|| (stat(path, &st) < 0 || !S_ISDIR(st.st_mode))) {
				fail_msg = "create";
				umask(mask);
				break;
			}
			/* Since the directory exists, don't attempt to change
			 * permissions if it was the full target.  Note that
			 * this is not an error conditon. */
			if (!c) {
				umask(mask);
				return 0;
			}
		}

		if (!c) {
			/* Done.  If necessary, updated perms on the newly
			 * created directory.  Failure to update here _is_
			 * an error.*/
			umask(mask);
			if ((mode != -1) && (chmod(path, mode) < 0)){
				fail_msg = "set permissions of";
				break;
			}
			return 0;
		}

		/* Remove any inserted nul from the path (recursive mode). */
		*s = c;

	} while (1);

	bb_perror_msg ("cannot %s directory '%s'", fail_msg, path);
	return -1;
}
