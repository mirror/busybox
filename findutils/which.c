/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

extern int which_main(int argc, char **argv)
{
	char *path_list, *path_n;
	struct stat filestat;
	int i, count=1, found, status = EXIT_SUCCESS;

	if (argc <= 1 || **(argv + 1) == '-')
		show_usage();
	argc--;

	path_list = getenv("PATH");
	if (!path_list)
		path_list = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin";

	/* Replace colons with zeros in path_parsed and count them */
	for(i=strlen(path_list); i > 0; i--) 
		if (path_list[i]==':') {
			path_list[i]=0;
			count++;
		}

	while(argc-- > 0) { 
		path_n = path_list;
		argv++;
		found = 0;
		for (i = 0; i < count; i++) {
			char *buf;
			buf = concat_path_file(path_n, *argv);
			if (stat (buf, &filestat) == 0
			    && filestat.st_mode & S_IXUSR)
			{
				puts(buf);
				found = 1;
				break;
			}
			free(buf);
			path_n += (strlen(path_n) + 1);
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
