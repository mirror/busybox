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

#include "busybox.h"

#if ENABLE_FEATURE_HUMAN_READABLE
# if ENABLE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K
static unsigned long disp_hr = 1024;
# else
static unsigned long disp_hr = 512;
# endif
#elif ENABLE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K
static unsigned disp_k = 1;
#else
static unsigned disp_k;	/* bss inits to 0 */
#endif

static int max_print_depth = INT_MAX;
static nlink_t count_hardlinks = 1;

static int status;
static int print_files;
static int slink_depth;
static int du_depth;
static int one_file_system;
static dev_t dir_dev;


static void print(long size, const char * const filename)
{
	/* TODO - May not want to defer error checking here. */
#if ENABLE_FEATURE_HUMAN_READABLE
	printf("%s\t%s\n", make_human_readable_str(size, 512, disp_hr),
			filename);
#else
	if (disp_k) {
		size++;
		size >>= 1;
	}
	printf("%ld\t%s\n", size, filename);
#endif
}

/* tiny recursive du */
static long du(const char * const filename)
{
	struct stat statbuf;
	long sum;

	if (lstat(filename, &statbuf) != 0) {
		bb_perror_msg("%s", filename);
		status = EXIT_FAILURE;
		return 0;
	}

	if (one_file_system) {
		if (du_depth == 0) {
			dir_dev = statbuf.st_dev;
		} else if (dir_dev != statbuf.st_dev) {
			return 0;
		}
	}

	sum = statbuf.st_blocks;

	if (S_ISLNK(statbuf.st_mode)) {
		if (slink_depth > du_depth) {	/* -H or -L */
			if (stat(filename, &statbuf) != 0) {
				bb_perror_msg("%s", filename);
				status = EXIT_FAILURE;
				return 0;
			}
			sum = statbuf.st_blocks;
			if (slink_depth == 1) {
				slink_depth = INT_MAX;	/* Convert -H to -L. */
			}
		}
	}

	if (statbuf.st_nlink > count_hardlinks) {
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
			status = EXIT_FAILURE;
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
			++du_depth;
			sum += du(newfile);
			--du_depth;
			free(newfile);
		}
		closedir(dir);
	} else if (du_depth > print_files) {
		return sum;
	}
	if (du_depth <= max_print_depth) {
		print(sum, filename);
	}
	return sum;
}

int du_main(int argc, char **argv);
int du_main(int argc, char **argv)
{
	long total;
	int slink_depth_save;
	int print_final_total;
	char *smax_print_depth;
	unsigned opt;

#if ENABLE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K
	if (getenv("POSIXLY_CORRECT")) {	/* TODO - a new libbb function? */
#if ENABLE_FEATURE_HUMAN_READABLE
		disp_hr = 512;
#else
		disp_k = 0;
#endif
	}
#endif

	/* Note: SUSv3 specifies that -a and -s options cannot be used together
	 * in strictly conforming applications.  However, it also says that some
	 * du implementations may produce output when -a and -s are used together.
	 * gnu du exits with an error code in this case.  We choose to simply
	 * ignore -a.  This is consistent with -s being equivalent to -d 0.
	 */
#if ENABLE_FEATURE_HUMAN_READABLE
	opt_complementary = "h-km:k-hm:m-hk:H-L:L-H:s-d:d-s";
	opt = getopt32(argc, argv, "aHkLsx" "d:" "lc" "hm", &smax_print_depth);
	if (opt & (1 << 9)) {
		/* -h opt */
		disp_hr = 0;
	}
	if (opt & (1 << 10)) {
		/* -m opt */
		disp_hr = 1024*1024;
	}
	if (opt & (1 << 2)) {
		/* -k opt */
		disp_hr = 1024;
	}
#else
	opt_complementary = "H-L:L-H:s-d:d-s";
	opt = getopt32(argc, argv, "aHkLsx" "d:" "lc", &smax_print_depth);
#if !ENABLE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K
	if (opt & (1 << 2)) {
		/* -k opt */
		disp_k = 1;
	}
#endif
#endif
	if (opt & (1 << 0)) {
		/* -a opt */
		print_files = INT_MAX;
	}
	if (opt & (1 << 1)) {
		/* -H opt */
		slink_depth = 1;
	}
	if (opt & (1 << 3)) {
		/* -L opt */
		slink_depth = INT_MAX;
	}
	if (opt & (1 << 4)) {
		/* -s opt */
		max_print_depth = 0;
	}
	one_file_system = opt & (1 << 5); /* -x opt */
	if (opt & (1 << 6)) {
		/* -d opt */
		max_print_depth = xatoi_u(smax_print_depth);
	}
	if (opt & (1 << 7)) {
		/* -l opt */
		count_hardlinks = MAXINT(nlink_t);
	}
	print_final_total = opt & (1 << 8); /* -c opt */

	/* go through remaining args (if any) */
	argv += optind;
	if (optind >= argc) {
		*--argv = (char*)".";
		if (slink_depth == 1) {
			slink_depth = 0;
		}
	}

	slink_depth_save = slink_depth;
	total = 0;
	do {
		total += du(*argv);
		slink_depth = slink_depth_save;
	} while (*++argv);
#if ENABLE_FEATURE_CLEAN_UP
	reset_ino_dev_hashtable();
#endif

	if (print_final_total) {
		print(total, "total");
	}

	fflush_stdout_and_exit(status);
}
