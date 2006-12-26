/* vi: set sw=4 ts=4: */
/*
 * lsattr.c		- List file attributes on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 * 93/11/13	- Replace stat() calls by lstat() to avoid loops
 * 94/02/27	- Integrated in Ted's distribution
 * 98/12/29	- Display version info only when -V specified (G M Sipe)
 */

#include "busybox.h"
#include "e2fs_lib.h"

enum {
	OPT_RECUR      = 0x1,
	OPT_ALL        = 0x2,
	OPT_DIRS_OPT   = 0x4,
	OPT_PF_LONG    = 0x8,
	OPT_GENERATION = 0x10,
};

static void list_attributes(const char *name)
{
	unsigned long fsflags;
	unsigned long generation;

	if (fgetflags(name, &fsflags) == -1)
		goto read_err;

	if (option_mask32 & OPT_GENERATION) {
		if (fgetversion(name, &generation) == -1)
			goto read_err;
		printf("%5lu ", generation);
	}

	if (option_mask32 & OPT_PF_LONG) {
		printf("%-28s ", name);
		print_flags(stdout, fsflags, PFOPT_LONG);
		puts("");
	} else {
		print_flags(stdout, fsflags, 0);
		printf(" %s\n", name);
	}

	return;
 read_err:
	bb_perror_msg("reading %s", name);
}

static int lsattr_dir_proc(const char *dir_name, struct dirent *de,
			   void *private)
{
	struct stat st;
	char *path;

	path = concat_path_file(dir_name, de->d_name);

	if (lstat(path, &st) == -1)
		bb_perror_msg("stat %s", path);

	else if (de->d_name[0] != '.' || (option_mask32 & OPT_ALL)) {
		list_attributes(path);
		if (S_ISDIR(st.st_mode) && (option_mask32 & OPT_RECUR)
		 && (de->d_name[0] != '.'
		     || (de->d_name[1] != '\0' && NOT_LONE_CHAR(de->d_name+1, '.')))
		) {
			printf("\n%s:\n", path);
			iterate_on_dir(path, lsattr_dir_proc, NULL);
			puts("");
		}
	}

	free(path);

	return 0;
}

static void lsattr_args(const char *name)
{
	struct stat st;

	if (lstat(name, &st) == -1) {
		bb_perror_msg("stat %s", name);
	} else if (S_ISDIR(st.st_mode) && !(option_mask32 & OPT_DIRS_OPT)) {
		iterate_on_dir(name, lsattr_dir_proc, NULL);
	} else {
		list_attributes(name);
	}
}

int lsattr_main(int argc, char **argv)
{
	getopt32(argc, argv, "Radlv");
	argv += optind;

	if (!*argv)
		lsattr_args(".");
	else {
		while (*argv)
			lsattr_args(*argv++);
	}

	return EXIT_SUCCESS;
}
