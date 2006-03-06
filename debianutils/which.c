/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 *
 * Based on which from debianutils
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "busybox.h"

int which_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	size_t i, count;
	char *path_list;

	if (argc <= 1 || **(argv + 1) == '-') {
		bb_show_usage();
	}
	argc--;

	path_list = getenv("PATH");
	if (path_list != NULL) {
		size_t path_len = bb_strlen(path_list);
		char *new_list = NULL;
		count = 1;

		for (i = 0; i <= path_len; i++) {
			char *this_i = &path_list[i];
			if (*this_i == ':') {
				/* ^::[^:] == \.: */
				if (!i && (*(this_i + 1) == ':')) {
					*this_i = '.';
					continue;
				}
				*this_i = 0;
				count++;
				/* ^:[^:] == \.0 and [^:]::[^:] == 0\.0 and [^:]:$ == 0\.0 */
				if (!i || (*(this_i + 1) == ':') || (i == path_len-1)) {
					new_list = xrealloc(new_list, path_len += 1);
					if (i) {
						memmove(&new_list[i+2], &path_list[i+1], path_len-i);
						new_list[i+1] = '.';
						memmove(new_list, path_list, i);
					} else {
						memmove(&new_list[i+1], &path_list[i], path_len-i);
						new_list[i] = '.';
					}
					path_list = new_list;
				}
			}
		}
	} else {
		path_list = "/bin\0/sbin\0/usr/bin\0/usr/sbin\0/usr/local/bin";
		count = 5;
	}

	while (argc-- > 0) {
		struct stat stat_b;
		char *buf;
		char *path_n;
		char found = 0;
#define is_executable_file(a, b) (!access(a,X_OK) && !stat(a, &b) && \
		S_ISREG(b.st_mode))

		argv++;
		path_n = path_list;
		buf = *argv;

		/* if filename is either absolute or contains slashes,
		 * stat it */
		if (strchr(buf, '/') != NULL && is_executable_file(buf, stat_b)) {
			found = 1;
		} else {
			/* Couldn't access file and file doesn't contain slashes */
			for (i = 0; i < count; i++) {
				buf = concat_path_file(path_n, *argv);
				if (is_executable_file(buf, stat_b)) {
					found = 1;
					break;
				}
				free(buf);
				path_n += (bb_strlen(path_n) + 1);
			}
		}
		if (found) {
			puts(buf);
		} else {
			status = EXIT_FAILURE;
		}
	}
	bb_fflush_stdout_and_exit(status);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
