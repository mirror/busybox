/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Based on which from debianutils
 */

#include "busybox.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


static int is_executable_file(char *a, struct stat *b)
{
	return (!access(a,X_OK) && !stat(a, b) && S_ISREG(b->st_mode));
}

int which_main(int argc, char **argv)
{
	int status;
	size_t i, count;
	char *path_list, *p;

	if (argc <= 1 || argv[1][0] == '-') {
		bb_show_usage();
	}
	argc--;

	path_list = getenv("PATH");
	if (path_list != NULL) {
		count = 1;
		p = path_list;
		while ((p = strchr(p, ':')) != NULL) {
			*p++ = 0;
			count++;
		}
	} else {
		path_list = "/bin\0/sbin\0/usr/bin\0/usr/sbin\0/usr/local/bin";
		count = 5;
	}

	status = EXIT_SUCCESS;
	while (argc-- > 0) {
		struct stat stat_b;
		char *buf;

		argv++;
		buf = argv[0];

		/* If filename is either absolute or contains slashes,
		 * stat it */
		if (strchr(buf, '/')) {
			if (is_executable_file(buf, &stat_b)) {
				puts(buf);
				goto next;
			}
		} else {
			/* File doesn't contain slashes */
			p = path_list;
			for (i = 0; i < count; i++) {
				/* Empty component in PATH is treated as . */
				buf = concat_path_file(p[0] ? p : ".", argv[0]);
				if (is_executable_file(buf, &stat_b)) {
					puts(buf);
					free(buf);
					goto next;
				}
				free(buf);
				p += strlen(p) + 1;
			}
		}
		status = EXIT_FAILURE;
 next:		/* nothing */;
	}
	bb_fflush_stdout_and_exit(status);
}
