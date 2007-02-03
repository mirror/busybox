/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* BB_AUDIT GNU options missing: -d, -F, -i, and -v. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/ln.html */

#include "busybox.h"

#define LN_SYMLINK          1
#define LN_FORCE            2
#define LN_NODEREFERENCE    4
#define LN_BACKUP           8
#define LN_SUFFIX           16

int ln_main(int argc, char **argv);
int ln_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int flag;
	char *last;
	char *src_name;
	char *src;
	char *suffix = (char*)"~";
	struct stat statbuf;
	int (*link_func)(const char *, const char *);

	flag = getopt32(argc, argv, "sfnbS:", &suffix);

	if (argc == optind) {
		bb_show_usage();
	}

	last = argv[argc - 1];
	argv += optind;

	if (argc == optind + 1) {
		*--argv = last;
		last = bb_get_last_path_component(xstrdup(last));
	}

	do {
		src_name = NULL;
		src = last;

		if (is_directory(src,
		                (flag & LN_NODEREFERENCE) ^ LN_NODEREFERENCE,
		            	NULL)) {
			src_name = xstrdup(*argv);
			src = concat_path_file(src, bb_get_last_path_component(src_name));
			free(src_name);
			src_name = src;
		}
		if (!(flag & LN_SYMLINK) && stat(*argv, &statbuf)) {
			// coreutils: "ln dangling_symlink new_hardlink" works
			if (lstat(*argv, &statbuf) || !S_ISLNK(statbuf.st_mode)) {
				bb_perror_msg("%s", *argv);
				status = EXIT_FAILURE;
				free(src_name);
				continue;
			}
		}

		if (flag & LN_BACKUP) {
			char *backup;
			backup = xasprintf("%s%s", src, suffix);
			if (rename(src, backup) < 0 && errno != ENOENT) {
				bb_perror_msg("%s", src);
				status = EXIT_FAILURE;
				free(backup);
				continue;
			}
			free(backup);
			/*
			 * When the source and dest are both hard links to the same
			 * inode, a rename may succeed even though nothing happened.
			 * Therefore, always unlink().
			 */
			unlink(src);
		} else if (flag & LN_FORCE) {
			unlink(src);
		}

		link_func = link;
		if (flag & LN_SYMLINK) {
			link_func = symlink;
		}

		if (link_func(*argv, src) != 0) {
			bb_perror_msg("%s", src);
			status = EXIT_FAILURE;
		}

		free(src_name);

	} while ((++argv)[1]);

	return status;
}
