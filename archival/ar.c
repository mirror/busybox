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
#include <getopt.h>
#include <unistd.h>
#include "busybox.h"

extern int ar_main(int argc, char **argv)
{
	FILE *src_stream = NULL;
	int extract_function = 0, opt = 0;
	file_headers_t *head;
	file_headers_t *ar_extract_list = NULL;

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
	head = get_ar_headers(src_stream);

	/* find files to extract or display */
	/* search through argv and build extract list */
	ar_extract_list = (file_headers_t *) xcalloc(1, sizeof(file_headers_t));
	if (optind < argc) {
		while (optind < argc) {
			ar_extract_list = add_from_archive_list(head, ar_extract_list, argv[optind]);
			optind++;
		}
	} else {
		ar_extract_list = head;
	}

	/* If there isnt even one possible entry then abort */
	if (ar_extract_list->name == NULL) {
		error_msg_and_die("No files to extract");
	}	

	fseek(src_stream, 0, SEEK_SET);
	extract_archive(src_stream, stdout, ar_extract_list, extract_function, "./");

	return EXIT_SUCCESS;
}
