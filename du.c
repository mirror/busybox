/* vi: set sw=4 ts=4: */
/*
 * Mini du implementation for busybox
 *
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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

#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_name_too_long
#include "messages.c"


#ifdef BB_FEATURE_HUMAN_READABLE
static unsigned long disp_hr = KILOBYTE;
#endif

typedef void (Display) (long, char *);

static int du_depth = 0;
static int count_hardlinks = 0;

static Display *print;

static void print_normal(long size, char *filename)
{
	unsigned long base;
#ifdef BB_FEATURE_HUMAN_READABLE
	switch (disp_hr) {
		case MEGABYTE:
			base = KILOBYTE;
			break;
		case KILOBYTE:
			base = 1;
			break;
		default:
			base = 0;
	}
	printf("%s\t%s\n", make_human_readable_str(size, base), filename);
#else
	printf("%ld\t%s\n", size, filename);
#endif
}

static void print_summary(long size, char *filename)
{
	if (du_depth == 1) {
		printf("summary\n");
		print_normal(size, filename);
	}
}

/* tiny recursive du */
static long du(char *filename)
{
	struct stat statbuf;
	long sum;
	int len;

	if ((lstat(filename, &statbuf)) != 0) {
		perror_msg("%s", filename);
		return 0;
	}

	du_depth++;
	sum = (statbuf.st_blocks >> 1);

	/* Don't add in stuff pointed to by symbolic links */
	if (S_ISLNK(statbuf.st_mode)) {
		sum = 0L;
		if (du_depth == 1)
			print(sum, filename);
	}
	if (S_ISDIR(statbuf.st_mode)) {
		DIR *dir;
		struct dirent *entry;

		dir = opendir(filename);
		if (!dir) {
			du_depth--;
			return 0;
		}

		len = strlen(filename);
		if (filename[len - 1] == '/')
			filename[--len] = '\0';

		while ((entry = readdir(dir))) {
			char *newfile;
			char *name = entry->d_name;

			if ((strcmp(name, "..") == 0)
				|| (strcmp(name, ".") == 0)) {
				continue;
			}
			newfile = concat_path_file(filename, name);
			sum += du(newfile);
			free(newfile);
		}
		closedir(dir);
		print(sum, filename);
	}
	else if (statbuf.st_nlink > 1 && !count_hardlinks) {
		/* Add files with hard links only once */
		if (is_in_ino_dev_hashtable(&statbuf, NULL)) {
			sum = 0L;
			if (du_depth == 1)
				print(sum, filename);
		}
		else {
			add_to_ino_dev_hashtable(&statbuf, NULL);
		}
	}
	du_depth--;
	return sum;
}

int du_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int i;
	int c;

	/* default behaviour */
	print = print_normal;

	/* parse argv[] */
	while ((c = getopt(argc, argv, "sl"
#ifdef BB_FEATURE_HUMAN_READABLE
"hm"
#endif
"k")) != EOF) {
			switch (c) {
			case 's':
					print = print_summary;
					break;
			case 'l':
					count_hardlinks = 1;
					break;
#ifdef BB_FEATURE_HUMAN_READABLE
			case 'h': disp_hr = 0;        break;
			case 'm': disp_hr = MEGABYTE; break;
#endif
			case 'k': break;
			default:
					show_usage();
			}
	}

	/* go through remaining args (if any) */
	if (optind >= argc) {
		if (du(".") == 0)
			status = EXIT_FAILURE;
	} else {
		long sum;

		for (i=optind; i < argc; i++) {
			if ((sum = du(argv[i])) == 0)
				status = EXIT_FAILURE;
			if(is_directory(argv[i], FALSE, NULL)==FALSE) {
				print_normal(sum, argv[i]);
			}
			reset_ino_dev_hashtable();
		}
	}

	return status;
}

/* $Id: du.c,v 1.44 2001/04/09 22:48:11 andersen Exp $ */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
