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
#include <string.h>
#include <getopt.h>
#include "busybox.h"

extern int dpkg_deb_main(int argc, char **argv)
{
	char *prefix = NULL;
	char *filename = NULL;
	char *output_buffer = NULL;
	int opt = 0;
	int arg_type = 0;
	int deb_extract_funct = extract_create_leading_dirs | extract_unconditional;	
	
	const int arg_type_prefix = 1;
	const int arg_type_field = 2;
	const int arg_type_filename = 4;
//	const int arg_type_un_ar_gz = 8;

	while ((opt = getopt(argc, argv, "ceftXxI")) != -1) {
		switch (opt) {
			case 'c':
				deb_extract_funct |= extract_data_tar_gz;
				deb_extract_funct |= extract_verbose_list;
				break;
			case 'e':
				arg_type = arg_type_prefix;
				deb_extract_funct |= extract_control_tar_gz;
				deb_extract_funct |= extract_all_to_fs;
				break;
			case 'f':
				arg_type = arg_type_field;
				deb_extract_funct |= extract_control_tar_gz;
				deb_extract_funct |= extract_one_to_buffer;
				filename = xstrdup("./control");
				break;
			case 't': /* --fsys-tarfile, i just made up this short name */
				/* Integrate the functionality needed with some code from ar.c */
				error_msg_and_die("Option disabled");
//				arg_type = arg_type_un_ar_gz;
				break;
			case 'X':
				arg_type = arg_type_prefix;
				deb_extract_funct |= extract_data_tar_gz;
				deb_extract_funct |= extract_all_to_fs;
				deb_extract_funct |= extract_list;
			case 'x':
				arg_type = arg_type_prefix;
				deb_extract_funct |= extract_data_tar_gz;
				deb_extract_funct |= extract_all_to_fs;
				break;
			case 'I':
				arg_type = arg_type_filename;
				deb_extract_funct |= extract_control_tar_gz;
				deb_extract_funct |= extract_one_to_buffer;
				break;
			default:
				show_usage();
		}
	}

	if (optind == argc)  {
		show_usage();
	}

	/* Workout where to extract the files */
	if (arg_type == arg_type_prefix) {
		/* argument is a dir name */
		if ((optind + 1) == argc ) {
			prefix = xstrdup("./DEBIAN/");
		} else {
			prefix = (char *) xmalloc(strlen(argv[optind + 1]) + 2);
			strcpy(prefix, argv[optind + 1]);
			/* Make sure the directory has a trailing '/' */
			if (last_char_is(prefix, '/') == NULL) {
				strcat(prefix, "/");
			}
		}
		mkdir(prefix, 0777);
	}

	if (arg_type == arg_type_filename) {
		if ((optind + 1) != argc) {
			filename = xstrdup(argv[optind + 1]);
		} else {
			error_msg_and_die("-I currently requires a filename to be specified");
		}
	}

	output_buffer = deb_extract(argv[optind], stdout, deb_extract_funct, prefix, filename);

	if ((arg_type == arg_type_filename) && (output_buffer != NULL)) {
		puts(output_buffer);
	}
	else if (arg_type == arg_type_field) {
		char *field = NULL;
		char *name;
		char *value;
		int field_start = 0;

		while (1) {
			field_start += read_package_field(&output_buffer[field_start], &name, &value);
			if (name == NULL) {
				break;
			}
			if (strcmp(name, argv[optind + 1]) == 0) {
				puts(value);
			}
			free(field);
		}
	}

	return(EXIT_SUCCESS);
}
