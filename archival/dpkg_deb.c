/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

extern int dpkg_deb_main(int argc, char **argv)
{
	char *argument = NULL;
	int opt = 0;
	int optflag = 0;	
	
	while ((opt = getopt(argc, argv, "ceftXxI")) != -1) {
		switch (opt) {
			case 'c':
				optflag |= extract_contents;
				break;
			case 'e':
				optflag |= extract_control;
				break;
			case 'f':
				optflag |= extract_field;
				break;
			case 't':
				optflag |= extract_fsys_tarfile;
				break;
			case 'X':
				optflag |= extract_verbose_extract;
				break;
			case 'x':
				optflag |= extract_extract;
				break;
			case 'I':
				optflag |= extract_info;
				break;
			default:
				show_usage();
		}
	}

	if (optind == argc)  {
		show_usage();
	}

	switch (optflag) {
		case (extract_control):
		case (extract_extract):
		case (extract_verbose_extract):
			/* argument is a dir name */
			if ( (optind + 1) == argc ) {
				argument = xstrdup("DEBIAN");
			} else {
				argument = xstrdup(argv[optind + 1]);
			}
			break;
		case (extract_field):
			/* argument is a control field name */
			if ((optind + 1) != argc) {
				argument = xstrdup(argv[optind + 1]);				
			}
			break;
		case (extract_info):
			/* argument is a control field name */
			if ((optind + 1) != argc) {
				argument = xstrdup(argv[optind + 1]);
				break;
			} else {
				error_msg("-I currently requires a filename to be specifies");
				return(EXIT_FAILURE);
			}
			/* argument is a filename */
		default:
	}

	deb_extract(argv[optind], optflag, argument);

	return(EXIT_SUCCESS);
}
