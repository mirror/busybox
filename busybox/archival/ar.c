/* vi: set sw=4 ts=4: */
/*
 * Mini ar implementation for busybox 
 *
 * Copyright (C) 2000 by Glenn McGrath
 * Written by Glenn McGrath <bug1@optushome.com.au> 1 June 2000
 * 		
 * Based in part on BusyBox tar, Debian dpkg-deb and GNU ar.
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include "busybox.h"

extern int ar_main(int argc, char **argv)
{
	FILE *src_stream = NULL;
	char **extract_names = NULL;
	char ar_magic[8];
	int extract_function =  extract_unconditional;
	int opt;
	int num_of_entries = 0;
	extern off_t archive_offset;

	while ((opt = getopt(argc, argv, "ovtpx")) != -1) {
		switch (opt) {
		case 'o':
			extract_function |= extract_preserve_date;
			break;
		case 'v':
			extract_function |= extract_verbose_list;
			break;
		case 't':
			extract_function |= extract_list;
			break;
		case 'p':
			extract_function |= extract_to_stdout;
			break;
		case 'x':
			extract_function |= extract_all_to_fs;
			break;
		default:
			show_usage();
		}
	}
 
	/* check the src filename was specified */
	if (optind == argc) {
		show_usage();
	}

	src_stream = xfopen(argv[optind++], "r");

	/* check ar magic */
	fread(ar_magic, 1, 8, src_stream);
	archive_offset = 8;
	if (strncmp(ar_magic,"!<arch>",7) != 0) {
		error_msg_and_die("invalid magic");
	}

	/* Create a list of files to extract */
	while (optind < argc) {
		extract_names = xrealloc(extract_names, sizeof(char *) * (num_of_entries + 2));
		extract_names[num_of_entries] = xstrdup(argv[optind]);
		num_of_entries++;
		extract_names[num_of_entries] = NULL;
		optind++;
	}

	unarchive(src_stream, stdout, &get_header_ar, extract_function, "./", extract_names);
	return EXIT_SUCCESS;
}
