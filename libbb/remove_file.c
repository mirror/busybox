/* vi: set sw=4 ts=4: */
/*
 * Mini remove_file implementation for busybox
 *
 * Copyright (C) 2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
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
 */

#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "libbb.h"

extern int remove_file(const char *path, int flags)
{
	struct stat path_stat;
	int path_exists = 1;

	if (lstat(path, &path_stat) < 0) {
		if (errno != ENOENT) {
			perror_msg("unable to stat `%s'", path);
			return -1;
		}

		path_exists = 0;
	}

	if (!path_exists) {
		if (!(flags & FILEUTILS_FORCE)) {
			perror_msg("cannot remove `%s'", path);
			return -1;
		}
		return 0;
	}

	if (S_ISDIR(path_stat.st_mode)) {
		DIR *dp;
		struct dirent *d;
		int status = 0;

		if (!(flags & FILEUTILS_RECUR)) {
			error_msg("%s: is a directory", path);
			return -1;
		}

		if ((!(flags & FILEUTILS_FORCE) && access(path, W_OK) < 0 &&
					isatty(0)) ||
				(flags & FILEUTILS_INTERACTIVE)) {
			fprintf(stderr, "%s: descend into directory `%s'? ", applet_name,
					path);
			if (!ask_confirmation())
				return 0;
		}

		if ((dp = opendir(path)) == NULL) {
			perror_msg("unable to open `%s'", path);
			return -1;
		}

		while ((d = readdir(dp)) != NULL) {
			char *new_path;

			if (strcmp(d->d_name, ".") == 0 ||
					strcmp(d->d_name, "..") == 0)
				continue;

			new_path = concat_path_file(path, d->d_name);
			if (remove_file(new_path, flags) < 0)
				status = -1;
			free(new_path);
		}

		if (closedir(dp) < 0) {
			perror_msg("unable to close `%s'", path);
			return -1;
		}

		if (flags & FILEUTILS_INTERACTIVE) {
			fprintf(stderr, "%s: remove directory `%s'? ", applet_name, path);
			if (!ask_confirmation())
				return status;
		}

		if (rmdir(path) < 0) {
			perror_msg("unable to remove `%s'", path);
			return -1;
		}

		return status;
	} else {
		if ((!(flags & FILEUTILS_FORCE) && access(path, W_OK) < 0 &&
					!S_ISLNK(path_stat.st_mode) &&
					isatty(0)) ||
				(flags & FILEUTILS_INTERACTIVE)) {
			fprintf(stderr, "%s: remove `%s'? ", applet_name, path);
			if (!ask_confirmation())
				return 0;
		}

		if (unlink(path) < 0) {
			perror_msg("unable to remove `%s'", path);
			return -1;
		}

		return 0;
	}
}
