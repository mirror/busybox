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
#include <dirent.h>


extern int which_main(int argc, char **argv)
{
	char *path_list, *test, *tmp;
	struct dirent *next;

	if (**(argv + 1) == '-') {
		usage("which [COMMAND ...]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
				"\nLocates a COMMAND.\n"
#endif
			 );
	}
	argc--;

	path_list = getenv("PATH");
	if (!path_list)
		path_list = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin";

	while(argc-- > 0 && *(argv++) != '\0' && strlen(*argv)) { 
		for( test=path_list; (tmp=strchr(test, ':')) && (tmp+1)!=NULL; test=++tmp) {
			DIR *dir;
			*tmp='\0';
			//printf("Checking directory '%s'\n", test);
			dir = opendir(test);
			if (!dir)
				continue;
			while ((next = readdir(dir)) != NULL) {
				//printf("Checking file '%s'\n", next->d_name);
				if ((strcmp(next->d_name, *argv) == 0)) {
					printf("%s/%s\n", test, next->d_name);
					exit(TRUE);
				}
			}
		}
	}
	exit(TRUE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
