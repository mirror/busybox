/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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

/* getopt not needed */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

static int file_exists(char *file)
{
	struct stat filestat;

	if (stat(file, &filestat) == 0 &&
			S_ISREG(filestat.st_mode) &&
			filestat.st_mode & S_IXUSR)
		return 1;
	else
		return 0;
}
	
extern int which_main(int argc, char **argv)
{
	char *path_list, *path_n;
	int i, count=1, found, status = EXIT_SUCCESS;

	if (argc <= 1 || **(argv + 1) == '-')
		bb_show_usage();
	argc--;

	path_list = getenv("PATH");
	if (path_list != NULL) {
		for(i=strlen(path_list); i > 0; i--)
			if (path_list[i]==':') {
				path_list[i]=0;
				count++;
			}
	} else {
		path_list = "/bin\0/sbin\0/usr/bin\0/usr/sbin\0/usr/local/bin";
		count = 5;
	}

	while(argc-- > 0) { 
		char *buf;
		path_n = path_list;
		argv++;
		found = 0;

		/*
		 * Check if we were given the full path, first.
		 * Otherwise see if the file exists in our $PATH.
		 */
		buf = *argv;
		if (file_exists(buf)) {
			puts(buf);
			found = 1;
		} else {
			for (i = 0; i < count; i++) {
				buf = concat_path_file(path_n, *argv);
				if (file_exists(buf)) {
					puts(buf);
					found = 1;
					break;
				}
				free(buf);
				path_n += (strlen(path_n) + 1);
			}
		}
		if (!found)
			status = EXIT_FAILURE;
	}
	return status;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
