/* vi: set sw=4 ts=4: */
/*
 * Mini unzip implementation for busybox
 *
 * Copyright (C) 2001 by Laurence Anderson
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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "busybox.h"

extern int unzip_main(int argc, char **argv)
{
	FILE *src_stream;
	int extract_function = extract_all_to_fs | extract_create_leading_dirs;
	char **extract_names = NULL;
	char **exclude_names = NULL;
	int opt = 0;
	int num_of_entries = 0;
	int exclude = 0;
	char *outdir = "./";
	FILE *msgout = stdout;

	while ((opt = getopt(argc, argv, "lnopqxd:")) != -1) {
		switch (opt) {
			case 'l':
				extract_function |= extract_verbose_list;
				extract_function ^= extract_all_to_fs;
				break;
			case 'n':
				break;
			case 'o':
				extract_function |= extract_unconditional;
				break;
			case 'p':
				extract_function |= extract_to_stdout;
				extract_function ^= extract_all_to_fs;
				/* FALLTHROUGH */
			case 'q':
				msgout = xfopen("/dev/null", "w");
				break;
			case 'd':
				outdir = xstrdup(optarg);
				strcat(outdir, "/");
				break;
			case 'x':
				exclude = 1;
				break;
		}
	}

	if (optind == argc) {
		show_usage();
	}

	if (*argv[optind] == '-') src_stream = stdin;
	else src_stream = xfopen(argv[optind++], "r");

	while (optind < argc) {
		if (exclude) {
			exclude_names = xrealloc(exclude_names, sizeof(char *) * (num_of_entries + 2));
			exclude_names[num_of_entries] = xstrdup(argv[optind]);
		} else {
			extract_names = xrealloc(extract_names, sizeof(char *) * (num_of_entries + 2));
			extract_names[num_of_entries] = xstrdup(argv[optind]);
		}
		num_of_entries++;
		if (exclude) exclude_names[num_of_entries] = NULL;
		else extract_names[num_of_entries] = NULL;
		optind++;
	}

	unarchive(src_stream, msgout, &get_header_zip, extract_function, outdir, extract_names, exclude_names);
	return EXIT_SUCCESS;
}
