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
	const int dpkg_deb_contents = 1;
	const int dpkg_deb_control = 2;
//	const int dpkg_deb_info = 4;
	const int dpkg_deb_extract = 8;
	const int dpkg_deb_verbose_extract = 16;
	const int dpkg_deb_list = 32;
	char *target_dir = NULL;
	int opt = 0;
	int optflag = 0;	
	
	while ((opt = getopt(argc, argv, "cexXl")) != -1) {
		switch (opt) {
			case 'c':
				optflag |= dpkg_deb_contents;
				break;
			case 'e':
				optflag |= dpkg_deb_control;
				break;
			case 'X':
				optflag |= dpkg_deb_verbose_extract;
				break;
			case 'x':
				optflag |= dpkg_deb_extract;
				break;
			case 'l':
				optflag |= dpkg_deb_list;
				break;
/*			case 'I':
				optflag |= dpkg_deb_info;
				break;
*/
			default:
				show_usage();
		}
	}

	if (((optind + 1 ) > argc) || (optflag == 0))  {
		show_usage();
	}
	if ((optflag & dpkg_deb_control) || (optflag & dpkg_deb_extract) || (optflag & dpkg_deb_verbose_extract)) {
		if ( (optind + 1) == argc ) {
			target_dir = (char *) xmalloc(7);
			strcpy(target_dir, "DEBIAN");
		} else {
			target_dir = (char *) xmalloc(strlen(argv[optind + 1]));
			strcpy(target_dir, argv[optind + 1]);
		}
	}
	deb_extract(argv[optind], optflag, target_dir);
/*	else if (optflag & dpkg_deb_info) {
		extract_flag = TRUE;
		extract_to_stdout = TRUE;
		strcpy(ar_filename, "control.tar.gz");
		extract_list = argv+optind+1;
		printf("list one is [%s]\n",extract_list[0]);
	}
*/
	return(EXIT_SUCCESS);
}
