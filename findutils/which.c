/* vi: set sw=4 ts=4: */
/*
 * Which implementation for busybox
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <sys/param.h>

extern int which_main(int argc, char **argv)
{
	char *path_list, *test, *tmp, *path_parsed;
	char buf[PATH_MAX];
	struct stat filestat;
	int count = 0;

	if (argc <= 1 || **(argv + 1) == '-')
		usage(which_usage);
	argc--;

	path_list = getenv("PATH");
	if (!path_list)
		path_list = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin";

	path_parsed = xmalloc (strlen(path_list) + 1);
	strcpy (path_parsed, path_list);

	/* Replace colons with zeros in path_parsed and count them */
	count = 1;
	test = path_parsed;
	while (1) {
		tmp = strchr(test, ':');
		if (tmp == NULL)
			break;
		*tmp = 0;
		test = tmp + 1;
		count++;
	}


	while(argc-- > 0) { 
		int i;
		int found = FALSE;
		test = path_parsed;
		argv++;
		for (i = 0; i < count; i++) {
			strcpy (buf, test);
			strcat (buf, "/");
			strcat (buf, *argv);
			if (stat (buf, &filestat) == 0
			    && filestat.st_mode & S_IXUSR)
			{
				found = TRUE;
				break;
			}
			test += (strlen(test) + 1);
		}
		if (found == TRUE)
			printf ("%s\n", buf);
		else
		{
			printf ("which: no %s in (%s)\n", *argv, path_list);
			exit (FALSE);
		}
	}
	return(TRUE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
