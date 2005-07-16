/* vi: set sw=4 ts=4: */
/*
 * Mini cpio implementation for busybox
 *
 * Copyright (C) 2001 by Glenn McGrath 
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
 * Limitations:
 *		Doesn't check CRC's
 *		Only supports new ASCII and CRC formats
 *
 */
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"

extern int cpio_main(int argc, char **argv)
{
	FILE *src_stream = stdin;
	char **extract_names = NULL;
	int extract_function = 0;
	int num_of_entries = 0;
	int opt = 0;
	mode_t oldmask = 0;

	while ((opt = getopt(argc, argv, "idmuvtF:")) != -1) {
		switch (opt) {
		case 'i': // extract
			extract_function |= extract_all_to_fs;
			break;
		case 'd': // create _leading_ directories
			extract_function |= extract_create_leading_dirs;
			oldmask = umask(077); /* Make make_directory act like GNU cpio */
			break;
		case 'm': // preserve modification time
			extract_function |= extract_preserve_date;
			break;
		case 'v': // verbosly list files
			extract_function |= extract_verbose_list;
			break;
		case 'u': // unconditional
			extract_function |= extract_unconditional;
			break;
		case 't': // list files
			extract_function |= extract_list;
			break;
		case 'F':
			src_stream = xfopen(optarg, "r");
			break;
		default:
			show_usage();
		}
	}

	if ((extract_function & extract_all_to_fs) && (extract_function & extract_list)) {
		extract_function ^= extract_all_to_fs; /* If specify t, don't extract*/
	}

	if ((extract_function & extract_all_to_fs) && (extract_function & extract_verbose_list)) {
		/* The meaning of v changes on extract */
		extract_function ^= extract_verbose_list;
		extract_function |= extract_list;
	}

	while (optind < argc) {
		extract_names = xrealloc(extract_names, sizeof(char *) * (num_of_entries + 2));
		extract_names[num_of_entries] = xstrdup(argv[optind]);
		num_of_entries++;
		extract_names[num_of_entries] = NULL;
		optind++;
	}

	unarchive(src_stream, stdout, &get_header_cpio, extract_function, "./", extract_names);
	if (oldmask) {
		umask(oldmask); /* Restore umask if we changed it */
	}
	return EXIT_SUCCESS;
}

