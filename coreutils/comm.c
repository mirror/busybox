/* vi: set sw=4 ts=4: */
/*
 * Mini comm implementation for busybox
 *
 * Copyright (C) 2005 by Robert Sullivan <cogito.ergo.cogito@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define COMM_OPT_1 (1 << 0)
#define COMM_OPT_2 (1 << 1)
#define COMM_OPT_3 (1 << 2)

/* writeline outputs the input given, appropriately aligned according to class */
static void writeline(char *line, int class, int flags)
{
	if (class == 0) {
		if (flags & COMM_OPT_1)
			return;
	} else if (class == 1) {
		if (flags & COMM_OPT_2)
			return;
		if (!(flags & COMM_OPT_1))
			putchar('\t');
	} else /*if (class == 2)*/ {
		if (flags & COMM_OPT_3)
			return;
		if (!(flags & COMM_OPT_1))
			putchar('\t');
		if (!(flags & COMM_OPT_2))
			putchar('\t');
	}
	fputs(line, stdout);
}

int comm_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int comm_main(int argc, char **argv)
{
#define LINE_LEN 100
#define BB_EOF_0 0x1
#define BB_EOF_1 0x2
	char thisline[2][LINE_LEN];
	FILE *streams[2];
	int i;
	unsigned flags;

	opt_complementary = "=2";
	flags = getopt32(argv, "123");
	argv += optind;

	for (i = 0; i < 2; ++i) {
		streams[i] = (argv[i][0] == '-' && !argv[i][1]) ? stdin : xfopen(argv[i], "r");
		fgets(thisline[i], LINE_LEN, streams[i]);
	}

	/* This is the real core of the program - lines are compared here */

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
			writeline(thisline[1], 2, flags);
		else if (order > 0 && !(i & BB_EOF_1))
			writeline(thisline[1], 1, flags);
		else if (order < 0 && !(i & BB_EOF_0))
			writeline(thisline[0], 0, flags);

		if (i & BB_EOF_0 & BB_EOF_1) {
			break;

		} else if (i) {
			i = (i & BB_EOF_0 ? 1 : 0);
			while (!feof(streams[i])) {
				if ((order < 0 && i) || (order > 0 && !i))
					writeline(thisline[i], i, flags);
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

	if (ENABLE_FEATURE_CLEAN_UP) {
		fclose(streams[0]);
		fclose(streams[1]);
	}

	return EXIT_SUCCESS;
}
