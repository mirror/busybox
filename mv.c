/* vi: set sw=4 ts=4: */
/*
 * Mini mv implementation for busybox
 *
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "busybox.h"

static int flags;

static int manual_rename(const char *source, const char *dest)
{
	struct stat source_stat;
	struct stat dest_stat;
	int source_exists = 1;
	int dest_exists = 1;

	if (stat(source, &source_stat) < 0) {
		if (errno != ENOENT) {
			perror_msg("unable to stat `%s'", source);
			return -1;
		}
		source_exists = 0;
	}

	if (stat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			perror_msg("unable to stat `%s'", dest);
			return -1;
		}
		dest_exists = 0;
	}

	if (dest_exists) {
		if (S_ISDIR(dest_stat.st_mode) &&
			  (!source_exists || !S_ISDIR(source_stat.st_mode))) {
			error_msg("cannot overwrite directory with non-directory");
			return -1;
		}

		if (!S_ISDIR(dest_stat.st_mode) && source_exists &&
				S_ISDIR(source_stat.st_mode)) {
			error_msg("cannot overwrite non-directory with directory");
			return -1;
		}

		if (unlink(dest) < 0) {
			perror_msg("cannot remove `%s'", dest);
			return -1;
		}
	}

	if (copy_file(source, dest, FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS |
			FILEUTILS_PRESERVE_SYMLINKS) < 0)
		return -1;

	if (remove_file(source, FILEUTILS_RECUR | FILEUTILS_FORCE) < 0)
		return -1;

	return 0;
}

static int move_file(const char *source, const char *dest)
{
	struct stat dest_stat;
	int dest_exists = 1;

	if (stat(dest, &dest_stat) < 0) {
		if (errno != ENOENT) {
			perror_msg("unable to stat `%s'", dest);
			return -1;
		}
		dest_exists = 0;
	}

	if (dest_exists && !(flags & FILEUTILS_FORCE) &&
			((access(dest, W_OK) < 0 && isatty(0)) ||
			 (flags & FILEUTILS_INTERACTIVE))) {
		fprintf(stderr, "mv: overwrite `%s'? ", dest);
		if (!ask_confirmation())
			return 0;
	}

	if (rename(source, dest) < 0) {
		if (errno == EXDEV)
			return manual_rename(source, dest);

		perror_msg("unable to rename `%s'", source);
		return -1;
	}
	
	return 0;
}

extern int mv_main(int argc, char **argv)
{
	int status = 0;
	int opt;
	int i;

	while ((opt = getopt(argc, argv, "fi")) != -1)
		switch (opt) {
		case 'f':
			flags &= ~FILEUTILS_INTERACTIVE;
			flags |= FILEUTILS_FORCE;
			break;
		case 'i':
			flags &= ~FILEUTILS_FORCE;
			flags |= FILEUTILS_INTERACTIVE;
			break;
		default:
			show_usage();
		}

	if (optind + 2 > argc)
		show_usage();

	if (optind + 2 == argc) {
		struct stat dest_stat;
		int dest_exists = 1;

		if (stat(argv[optind + 1], &dest_stat) < 0) {
			if (errno != ENOENT)
				perror_msg_and_die("unable to stat `%s'", argv[optind + 1]);
			dest_exists = 0;
		}

		if (!dest_exists || !S_ISDIR(dest_stat.st_mode)) {
			if (move_file(argv[optind], argv[optind + 1]) < 0)
				status = 1;
			return status;
		}
	}

	for (i = optind; i < argc - 1; i++) {
		char *dest = concat_path_file(argv[argc - 1],
				get_last_path_component(argv[i]));
		if (move_file(argv[i], dest) < 0)
			status = 1;
		free(dest);
	}

	return status;
}
