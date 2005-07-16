/* vi: set sw=4 ts=4: */
/*
 * Mini tee implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

#include "busybox.h"
#include <getopt.h>
#include <stdio.h>

int
tee_main(int argc, char **argv)
{
	char *mode = "w";
	int c, i, status = 0, nfiles = 0;
	FILE **files;

	while ((c = getopt(argc, argv, "a")) != EOF) {
		switch (c) {
		case 'a': 
			mode = "a";
			break;
		default:
			show_usage();
		}
	}

	files = (FILE **)xmalloc(sizeof(FILE *) * (argc - optind + 1));
	files[nfiles++] = stdout;
	while (optind < argc) {
		if ((files[nfiles++] = fopen(argv[optind++], mode)) == NULL) {
			nfiles--;
			perror_msg("%s", argv[optind-1]);
			status = 1;
		}
	}

	while ((c = getchar()) != EOF)
		for (i = 0; i < nfiles; i++)
			putc(c, files[i]);

	return status;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
