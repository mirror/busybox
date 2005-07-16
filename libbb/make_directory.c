/* vi: set sw=4 ts=4: */
/*
 * Mini make_directory implementation for busybox
 *
 * Copyright (C) 2001  Matt Kraai.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "libbb.h"

/* Create the directory PATH with mode MODE, or the default if MODE is -1.
 * Also create parent directories as necessary if flags contains
 * FILEUTILS_RECUR.  */

int make_directory (char *path, long mode, int flags)
{
	if (!(flags & FILEUTILS_RECUR)) {
		if (mkdir (path, 0777) < 0) {
			perror_msg ("Cannot create directory `%s'", path);
			return -1;
		}

		if (mode != -1 && chmod (path, mode) < 0) {
			perror_msg ("Cannot set permissions of directory `%s'", path);
			return -1;
		}
	} else {
		struct stat st;

		if (stat (path, &st) < 0 && errno == ENOENT) {
			int status;
			char *buf, *parent;
			mode_t mask;

			mask = umask (0);
			umask (mask);

			buf = xstrdup (path);
			parent = dirname (buf);
			status = make_directory (parent, (0777 & ~mask) | 0300,
					FILEUTILS_RECUR);
			free (buf);

			if (status < 0 || make_directory (path, mode, 0) < 0)
				return -1;
		}
	}

	return 0;
}
