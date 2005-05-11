/* vi: set sw=4 ts=4: */
/*
 * Mini comm implementation for busybox
 *
 * Copyright (C) 2005 by Robert Sullivan <cogito.ergo.cogito@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"

#define COMM_OPT_1 0x01
#define COMM_OPT_2 0x02
#define COMM_OPT_3 0x04

/* These three variables control behaviour if non-zero */

static int only_file_1;
static int only_file_2;
static int both;

/* writeline outputs the input given, appropriately aligned according to class */
static void writeline (char *line, int class) {
	switch (class) {
		case 1: if (!only_file_1)
				return;
			break;
		case 2: if (!only_file_2)
				return;
			if (only_file_1)
				putchar('\t');
			break;
		case 3: if (!both)
				return;
			if (only_file_1)
				putchar('\t');
			if (only_file_2)
				putchar('\t');
			break;
	}
	fputs(line, stdout);	
}

/* This is the real core of the program - lines are compared here */
static int cmp_files(char **infiles) {

	char thisline[2][100];
	FILE *streams[2];
	int i = 0;
	
	for (i = 0; i < 2; i++) {
		streams[i] = (strcmp(infiles[i], "=") == 0 ? stdin : fopen(infiles[i], "r"));
		fgets(thisline[i], 100, streams[i]);
	}

	while (thisline[0] || thisline[1]) {
		
		int order = 0;
		int tl0_len = strlen(thisline[0]);
		int tl1_len = strlen(thisline[1]);
		if (!thisline[0])
			order = 1;
		else if (!thisline[1]) 
			order = -1;
		else {
			order = memcmp(thisline[0], thisline[1], tl0_len < tl1_len ? tl0_len : tl1_len);
			if (!order)
				order = tl0_len < tl1_len ? -1 : tl0_len != tl1_len;
		}
		
		if ((order == 0) && (!feof(streams[0])) && (!feof(streams[1])))
			writeline(thisline[1], 3);
		else if ((order > 0) && (!feof(streams[1])))
			writeline(thisline[1], 2);
		else if ((order < 0) && (!feof(streams[0])))
			writeline(thisline[0], 1);
		
		if (feof(streams[0]) && feof(streams[1])) {
			fclose(streams[0]);
			fclose(streams[1]);
			break;
		}
		else if (feof(streams[0])) {
		
			while (!feof(streams[1])) {
				if (order < 0)
					writeline(thisline[1], 2);
				fgets(thisline[1], 100, streams[1]);
			}
			fclose(streams[0]);
			fclose(streams[1]);
			break;
		}
		else if (feof(streams[1])) {

			while (!feof(streams[0])) {
				if (order > 0)
					writeline(thisline[0], 1);
				fgets(thisline[0], 100, streams[0]);
			}
			fclose(streams[0]);
			fclose(streams[1]);
			break;
		}
		else {
			if (order >= 0)
				fgets(thisline[1], 100, streams[1]);
			if (order <= 0)
				fgets(thisline[0], 100, streams[0]);
		}
	}

	return 0;
}

int comm_main (int argc, char **argv) {
	
	unsigned long opt;
	only_file_1 = 1;
	only_file_2 = 1;
	both = 1;
	
	opt = bb_getopt_ulflags(argc, argv, "123");
	
        if ((opt & 0x80000000UL) || (optind == argc)) {
                bb_show_usage();
        }
	
	if (opt & COMM_OPT_1)
		only_file_1 = 0;
	if (opt & COMM_OPT_2)
		only_file_2 = 0;
	if (opt & COMM_OPT_3)
		both = 0;

	exit(cmp_files(argv + optind) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
