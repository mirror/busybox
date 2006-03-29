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
 * Erik Andersen <andersen@codepoet.org>. I ripped out Native Language
 * Support, replaced getopt, added some gotos for redundant stuff.
 *
 * Audited and cleaned up on 7 March 2003 to reduce size of
 * check error handling by Erik Andersen <andersen@codepoet.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/klog.h>

#include "busybox.h"

int dmesg_main(int argc, char **argv)
{
	char *buf, *tmp;
	int bufsize = 8196;
	int i, n = 0;
	int c = 3;

	i = bb_getopt_ulflags(argc, argv, "cn:s:", &buf, &tmp);
	if (i & 1)
				c = 4;
	if (i & 2) {
				c = 8;
				n = bb_xgetlarg(buf, 10, 0, 10);
	}
	if (i & 4)
				/* I think a 512k max kernel ring buffer is big enough for
				 * anybody, as the default is 16k...  Could be wrong though.
				 * If so I'm sure I'll hear about it by the enraged masses*/
				bufsize = bb_xgetlarg(tmp, 10, 4096, 512*1024);

	if (c == 8) {
		if (klogctl(c, NULL, n) < 0)
			goto die_the_death;
		goto all_done;
	}

	buf = xmalloc(bufsize);
	if ((n = klogctl(c, buf, bufsize)) < 0)
		goto die_the_death;

	c = '\n';
	for (i = 0; i < n; i++) {
		if (c == '\n' && buf[i] == '<') {
			i++;
			while (buf[i] >= '0' && buf[i] <= '9')
				i++;
			if (buf[i] == '>')
				i++;
		}
		c = buf[i];
		putchar(c);
	}
	if (c != '\n')
		putchar('\n');
all_done:
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(buf);
	}

	return EXIT_SUCCESS;
die_the_death:
	bb_perror_nomsg_and_die();
}
