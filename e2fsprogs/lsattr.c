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
//config:config LSATTR
//config:	bool "lsattr (5.5 kb)"
//config:	default y
//config:	help
//config:	lsattr lists the file attributes on a second extended file system.

//applet:IF_LSATTR(APPLET_NOEXEC(lsattr, lsattr, BB_DIR_BIN, BB_SUID_DROP, lsattr))
/* ls is NOEXEC, so we should be too! ;) */

//kbuild:lib-$(CONFIG_LSATTR) += lsattr.o e2fs_lib.o

//usage:#define lsattr_trivial_usage
//usage:       "[-Radlpv] [FILE]..."
//usage:#define lsattr_full_usage "\n\n"
//usage:       "List ext2 file attributes\n"
//usage:     "\n	-R	Recurse"
//usage:     "\n	-a	Include names starting with ."
//usage:     "\n	-d	List directory names, not contents"
// -a,-d text should match ls --help
//usage:     "\n	-l	List long flag names"
//usage:     "\n	-p	List project ID"
//usage:     "\n	-v	List version/generation number"

#include "libbb.h"
#include "e2fs_lib.h"

enum {
	OPT_RECUR      = 1 << 0,
	OPT_ALL        = 1 << 1,
	OPT_DIRS_OPT   = 1 << 2,
	OPT_PF_LONG    = 1 << 3,
	OPT_GENERATION = 1 << 4,
	OPT_PROJID     = 1 << 5,
};

static void list_attributes(const char *name)
{
	unsigned long fsflags;

	if (fgetflags(name, &fsflags) != 0)
		goto read_err;

	if (option_mask32 & OPT_PROJID) {
		uint32_t p;
		if (fgetprojid(name, &p) != 0)
			goto read_err;
		printf("%5lu ", (unsigned long)p);
	}

	if (option_mask32 & OPT_GENERATION) {
		unsigned long generation;
		if (fgetversion(name, &generation) != 0)
			goto read_err;
		printf("%-10lu ", generation);
	}

	if (option_mask32 & OPT_PF_LONG) {
		printf("%-28s ", name);
		print_e2flags(stdout, fsflags, PFOPT_LONG);
		bb_putchar('\n');
	} else {
		print_e2flags(stdout, fsflags, 0);
		printf(" %s\n", name);
	}

	return;
 read_err:
	bb_perror_msg("reading %s", name);
}

static int FAST_FUNC lsattr_dir_proc(const char *dir_name,
		struct dirent *de,
		void *private UNUSED_PARAM)
{
	struct stat st;
	char *path;

	path = concat_path_file(dir_name, de->d_name);

	if (lstat(path, &st) != 0)
		bb_perror_msg("stat %s", path);
	else if (de->d_name[0] != '.' || (option_mask32 & OPT_ALL)) {
		list_attributes(path);
		if (S_ISDIR(st.st_mode) && (option_mask32 & OPT_RECUR)
		 && !DOT_OR_DOTDOT(de->d_name)
		) {
			printf("\n%s:\n", path);
			iterate_on_dir(path, lsattr_dir_proc, NULL);
			bb_putchar('\n');
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

int lsattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int lsattr_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "Radlvp");
	argv += optind;

	if (!*argv)
		*--argv = (char*)".";
	do lsattr_args(*argv++); while (*argv);

	return EXIT_SUCCESS;
}
