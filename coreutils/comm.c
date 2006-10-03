/* vi: set sw=4 ts=4: */
/*
 * Mini comm implementation for busybox
 *
 * Copyright (C) 2005 by Robert Sullivan <cogito.ergo.cogito@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

#define COMM_OPT_1 0x01
#define COMM_OPT_2 0x02
#define COMM_OPT_3 0x04

/* These three variables control behaviour if non-zero */

static int only_file_1;
static int only_file_2;
static int both;

/* writeline outputs the input given, appropriately aligned according to class */
static void writeline(char *line, int class)
{
	if (class == 0) {
		if (!only_file_1)
			return;
	} else if (class == 1) {
		if (!only_file_2)
			return;
		if (only_file_1)
			putchar('\t');
	}
	else /*if (class == 2)*/ {
		if (!both)
			return;
		if (only_file_1)
			putchar('\t');
		if (only_file_2)
			putchar('\t');
	}
	fputs(line, stdout);
}

/* This is the real core of the program - lines are compared here */
static void cmp_files(char **infiles)
{
#define LINE_LEN 100
#define BB_EOF_0 0x1
#define BB_EOF_1 0x2
	char thisline[2][LINE_LEN];
	FILE *streams[2];
	int i;

	for (i = 0; i < 2; ++i) {
		streams[i] = ((infiles[i][0] == '=' && infiles[i][1]) ? stdin : xfopen(infiles[i], "r"));
		fgets(thisline[i], LINE_LEN, streams[i]);
	}

	while (*thisline[0] || *thisline[1]) {
		int order = 0;

		i = 0;
		if (feof(streams[0])) i |= BB_EOF_0;
		if (feof(streams[1])) i |= BB_EOF_1;

		if (!*thisline[0])
			order = 1;
		else if (!*thisline[1])
			order = -1;
		else {
			int tl0_len, tl1_len;
			tl0_len = strlen(thisline[0]);
			tl1_len = strlen(thisline[1]);
			order = memcmp(thisline[0], thisline[1], tl0_len < tl1_len ? tl0_len : tl1_len);
			if (!order)
				order = tl0_len < tl1_len ? -1 : tl0_len != tl1_len;
		}

		if (order == 0 && !i)
			writeline(thisline[1], 2);
		else if (order > 0 && !(i & BB_EOF_1))
			writeline(thisline[1], 1);
		else if (order < 0 && !(i & BB_EOF_0))
			writeline(thisline[0], 0);

		if (i & BB_EOF_0 & BB_EOF_1) {
			break;

		} else if (i) {
			i = (i & BB_EOF_0 ? 1 : 0);
			while (!feof(streams[i])) {
				if ((order < 0 && i) || (order > 0 && !i))
					writeline(thisline[i], i);
				fgets(thisline[i], LINE_LEN, streams[i]);
			}
			break;

		} else {
			if (order >= 0)
				fgets(thisline[1], LINE_LEN, streams[1]);
			if (order <= 0)
				fgets(thisline[0], LINE_LEN, streams[0]);
		}
	}

	fclose(streams[0]);
	fclose(streams[1]);
}

int comm_main(int argc, char **argv)
{
	unsigned long flags;

	flags = getopt32(argc, argv, "123");

	if (optind + 2 != argc)
		bb_show_usage();

	only_file_1 = !(flags & COMM_OPT_1);
	only_file_2 = !(flags & COMM_OPT_2);
	both = !(flags & COMM_OPT_3);

	cmp_files(argv + optind);
	exit(EXIT_SUCCESS);
}
