/* vi: set sw=4 ts=4: */
/*
 * Mini getpty implementation for busybox
 * Bjorn Wesen, Axis Communications AB (bjornw@axis.com)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define DEBUG 0

int xgetpty(char *line)
{
	int p;
#if ENABLE_FEATURE_DEVPTS
	p = open("/dev/ptmx", O_RDWR);
	if (p > 0) {
		const char *name;
		grantpt(p);
		unlockpt(p);
		name = ptsname(p);
		if (!name) {
			bb_perror_msg("ptsname error (is /dev/pts mounted?)");
			goto fail;
		}
		safe_strncpy(line, name, GETPTY_BUFSIZE);
		return p;
	}
#else
	struct stat stb;
	int i;
	int j;

	strcpy(line, "/dev/ptyXX");

	for (i = 0; i < 16; i++) {
		line[8] = "pqrstuvwxyzabcde"[i];
		line[9] = '0';
		if (stat(line, &stb) < 0) {
			continue;
		}
		for (j = 0; j < 16; j++) {
			line[9] = j < 10 ? j + '0' : j - 10 + 'a';
			if (DEBUG)
				fprintf(stderr, "Trying to open device: %s\n", line);
			p = open(line, O_RDWR | O_NOCTTY);
			if (p >= 0) {
				line[5] = 't';
				return p;
			}
		}
	}
#endif /* FEATURE_DEVPTS */
USE_FEATURE_DEVPTS( fail:)
	bb_error_msg_and_die("open pty");
	return -1; /* never get here */
}


