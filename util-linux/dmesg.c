/* vi: set sw=4 ts=4: */
/* dmesg.c -- Print out the contents of the kernel ring buffer
 * Created: Sat Oct  9 16:19:47 1993
 * Revised: Thu Oct 28 21:52:17 1993 by faith@cs.unc.edu
 * Copyright 1993 Theodore Ts'o (tytso@athena.mit.edu)
 * This program comes with ABSOLUTELY NO WARRANTY.
 * Modifications by Rick Sladkey (jrs@world.std.com)
 * Larger buffersize 3 June 1998 by Nicolai Langfeldt, based on a patch
 * by Peeter Joot.  This was also suggested by John Hudson.
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@misiek.eu.org>
 * - added Native Language Support
 *
 * from util-linux -- adapted for busybox by 
 * Erik Andersen <andersee@debian.org>. I ripped out Native Language 
 * Support, replaced getopt, added some gotos for redundant stuff.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#if __GNU_LIBRARY__ < 5
# ifdef __alpha__
#   define klogctl syslog
# endif
#else
# include <sys/klog.h>
#endif

#include "busybox.h"

int dmesg_main(int argc, char **argv)
{
	char *buf;
	int c;
	int bufsize = 8196;
	int i;
	int n;
	int level = 0;
	int lastc;
	int cmd = 3;

	while ((c = getopt(argc, argv, "cn:s:")) != EOF) {
		switch (c) {
		case 'c':
			cmd = 4;
			break;
		case 'n':
			cmd = 8;
			if (optarg == NULL)
				show_usage();
			level = atoi(optarg);
			break;
		case 's':
			if (optarg == NULL)
				show_usage();
			bufsize = atoi(optarg);
			break;
		default:
			show_usage();
		}
	}			

	if (optind < argc) {
		show_usage();
	}

	if (cmd == 8) {
		if (klogctl(cmd, NULL, level) < 0)
			perror_msg_and_die("klogctl");
		return EXIT_SUCCESS;
	}

	if (bufsize < 4096)
		bufsize = 4096;
	buf = (char *) xmalloc(bufsize);
	if ((n = klogctl(cmd, buf, bufsize)) < 0)
		perror_msg_and_die("klogctl");

	lastc = '\n';
	for (i = 0; i < n; i++) {
		if (lastc == '\n' && buf[i] == '<') {
			i++;
			while (buf[i] >= '0' && buf[i] <= '9')
				i++;
			if (buf[i] == '>')
				i++;
		}
		lastc = buf[i];
		putchar(lastc);
	}
	if (lastc != '\n')
		putchar('\n');
	return EXIT_SUCCESS;
}
