/* vi: set sw=4 ts=4: */
/*
 * Mini watch implementation for busybox
 *
 * Copyright (C) 2001 by Michael Habermann <mhabermann@gmx.de>
 * Copyrigjt (C) Mar 16, 2003 Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A */
/* BB_AUDIT GNU defects -- only option -n is supported. */

#include "busybox.h"

// procps 2.0.18:
// watch [-d] [-n seconds]
//   [--differences[=cumulative]] [--interval=seconds] command
//
// procps-3.2.3:
// watch [-dt] [-n seconds]
//   [--differences[=cumulative]] [--interval=seconds] [--no-title] command
//
// (procps 3.x and procps 2.x are forks, not newer/older versions of the same)

int watch_main(int argc, char **argv);
int watch_main(int argc, char **argv)
{
	unsigned opt;
	unsigned period = 2;
	unsigned cmdlen = 1; // 1 for terminal NUL
	char *header = NULL;
	char *cmd;
	char *tmp;
	char **p;

	opt_complementary = "-1"; // at least one param please
	opt = getopt32(argc, argv, "+dtn:", &tmp);
	//if (opt & 0x1) // -d (ignore)
	//if (opt & 0x2) // -t
	if (opt & 0x4) period = xatou(tmp);
	argv += optind;

	p = argv;
	while (*p)
		cmdlen += strlen(*p++) + 1;
	tmp = cmd = xmalloc(cmdlen);
	while (*argv) {
		tmp += sprintf(tmp, " %s", *argv);
		argv++;
	}
	cmd++; // skip initial space

	while (1) {
		printf("\033[H\033[J");
		if (!(opt & 0x2)) { // no -t
			int width, len;
			char *thyme;
			time_t t;

			get_terminal_width_height(STDOUT_FILENO, &width, 0);
			header = xrealloc(header, width--);
			// '%-*s' pads header with spaces to the full width
			snprintf(header, width, "Every %ds: %-*s", period, width, cmd);
			time(&t);
			thyme = ctime(&t);
			len = strlen(thyme);
			if (len < width)
				strcpy(header + width - len, thyme);
			puts(header);
		}
		fflush(stdout);
		// TODO: 'real' watch pipes cmd's output to itself
		// and does not allow it to overflow the screen
		// (taking into account linewrap!)
		system(cmd);
		sleep(period);
	}
	return 0; // gcc thinks we can reach this :)
}
