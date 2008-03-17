/* vi: set sw=4 ts=4: */
/*
 * Mini du implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2002  Edward Betts <edward@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant (unless default blocksize set to 1k) */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/du.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Mostly rewritten for SUSv3 compliance and to fix bugs/defects.
 * 1) Added support for SUSv3 -a, -H, -L, gnu -c, and (busybox) -d options.
 *    The -d option allows setting of max depth (similar to gnu --max-depth).
 * 2) Fixed incorrect size calculations for links and directories, especially
 *    when errors occurred.  Calculates sizes should now match gnu du output.
 * 3) Added error checking of output.
 * 4) Fixed busybox bug #1284 involving long overflow with human_readable.
 */

#include "libbb.h"

struct globals {
#if ENABLE_FEATURE_HUMAN_READABLE
	unsigned long disp_hr;
#else
	unsigned disp_k;
#endif

	int max_print_depth;
	nlink_t count_hardlinks;

	bool status;
	bool one_file_system;
	int print_files;
	int slink_depth;
	int du_depth;
	dev_t dir_dev;
};
#define G (*(struct globals*)&bb_common_bufsiz1)


static void print(unsigned long size, const char *filename)
{
	/* TODO - May not want to defer error checking here. */
#if ENABLE_FEATURE_HUMAN_READABLE
	printf("%s\t%s\n", make_human_readable_str(size, 512, G.disp_hr),
			filename);
#else
	if (G.disp_k) {
		size++;
		size >>= 1;
	}
	printf("%ld\t%s\n", size, filename);
#endif
}

/* tiny recursive du */
static unsigned long du(const char *filename)
{
	struct stat statbuf;
	unsigned long sum;

	if (lstat(filename, &statbuf) != 0) {
		bb_simple_perror_msg(filename);
		G.status = EXIT_FAILURE;
		return 0;
	}

	if (G.one_file_system) {
		if (G.du_depth == 0) {
			G.dir_dev = statbuf.st_dev;
		} else if (G.dir_dev != statbuf.st_dev) {
			return 0;
		}
	}

	sum = statbuf.st_blocks;

	if (S_ISLNK(statbuf.st_mode)) {
		if (G.slink_depth > G.du_depth) {	/* -H or -L */
			if (stat(filename, &statbuf) != 0) {
				bb_simple_perror_msg(filename);
				G.status = EXIT_FAILURE;
				return 0;
			}
			sum = statbuf.st_blocks;
			if (G.slink_depth == 1) {
				G.slink_depth = INT_MAX;	/* Convert -H to -L. */
			}
		}
	}

	if (statbuf.st_nlink > G.count_hardlinks) {
		/* Add files/directories with links only once */
		if (is_in_ino_dev_hashtable(&statbuf)) {
			return 0;
		}
		add_to_ino_dev_hashtable(&statbuf, NULL);
	}

	if (S_ISDIR(statbuf.st_mode)) {
		DIR *dir;
		struct dirent *entry;
		char *newfile;

		dir = warn_opendir(filename);
		if (!dir) {
			G.status = EXIT_FAILURE;
			return sum;
		}

		newfile = last_char_is(filename, '/');
		if (newfile)
			*newfile = '\0';

		while ((entry = readdir(dir))) {
			char *name = entry->d_name;

			newfile = concat_subpath_file(filename, name);
			if (newfile == NULL)
				continue;
			++G.du_depth;
			sum += du(newfile);
			--G.du_depth;
			free(newfile);
		}
		closedir(dir);
	} else if (G.du_depth > G.print_files) {
		return sum;
	}
	if (G.du_depth <= G.max_print_depth) {
		print(sum, filename);
	}
	return sum;
}

int du_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int du_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	unsigned long total;
	int slink_depth_save;
	bool print_final_total;
	unsigned opt;

#if ENABLE_FEATURE_HUMAN_READABLE
	USE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K(G.disp_hr = 1024;)
	SKIP_FEATURE_DU_DEFAULT_BLOCKSIZE_1K(G.disp_hr = 512;)
	if (getenv("POSIXLY_CORRECT"))  /* TODO - a new libbb function? */
		G.disp_hr = 512;
#else
	USE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K(G.disp_k = 1;)
	/* SKIP_FEATURE_DU_DEFAULT_BLOCKSIZE_1K(G.disp_k = 0;) - G is pre-zeroed */
#endif
	G.max_print_depth = INT_MAX;
	G.count_hardlinks = 1;

	/* Note: SUSv3 specifies that -a and -s options cannot be used together
	 * in strictly conforming applications.  However, it also says that some
	 * du implementations may produce output when -a and -s are used together.
	 * gnu du exits with an error code in this case.  We choose to simply
	 * ignore -a.  This is consistent with -s being equivalent to -d 0.
	 */
#if ENABLE_FEATURE_HUMAN_READABLE
	opt_complementary = "h-km:k-hm:m-hk:H-L:L-H:s-d:d-s:d+";
	opt = getopt32(argv, "aHkLsx" "d:" "lc" "hm", &G.max_print_depth);
	argv += optind;
	if (opt & (1 << 9)) {
		/* -h opt */
		G.disp_hr = 0;
	}
	if (opt & (1 << 10)) {
		/* -m opt */
		G.disp_hr = 1024*1024;
	}
	if (opt & (1 << 2)) {
		/* -k opt */
		G.disp_hr = 1024;
	}
#else
	opt_complementary = "H-L:L-H:s-d:d-s:d+";
	opt = getopt32(argv, "aHkLsx" "d:" "lc", &G.max_print_depth);
	argv += optind;
#if !ENABLE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K
	if (opt & (1 << 2)) {
		/* -k opt */
		G.disp_k = 1;
	}
#endif
#endif
	if (opt & (1 << 0)) {
		/* -a opt */
		G.print_files = INT_MAX;
	}
	if (opt & (1 << 1)) {
		/* -H opt */
		G.slink_depth = 1;
	}
	if (opt & (1 << 3)) {
		/* -L opt */
		G.slink_depth = INT_MAX;
	}
	if (opt & (1 << 4)) {
		/* -s opt */
		G.max_print_depth = 0;
	}
	G.one_file_system = opt & (1 << 5); /* -x opt */
	if (opt & (1 << 7)) {
		/* -l opt */
		G.count_hardlinks = MAXINT(nlink_t);
	}
	print_final_total = opt & (1 << 8); /* -c opt */

	/* go through remaining args (if any) */
	if (!*argv) {
		*--argv = (char*)".";
		if (G.slink_depth == 1) {
			G.slink_depth = 0;
		}
	}

	slink_depth_save = G.slink_depth;
	total = 0;
	do {
		total += du(*argv);
		G.slink_depth = slink_depth_save;
	} while (*++argv);

	if (ENABLE_FEATURE_CLEAN_UP)
		reset_ino_dev_hashtable();
	if (print_final_total)
		print(total, "total");

	fflush_stdout_and_exit(G.status);
}
