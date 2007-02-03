/* vi: set sw=4 ts=4: */
/*
 * Monitor a pipe with a simple progress display.
 *
 * Copyright (C) 2003 by Rob Landley <rob@landley.net>, Joey Hess
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define PIPE_PROGRESS_SIZE 4096

/* Read a block of data from stdin, write it to stdout.
 * Activity is indicated by a '.' to stderr
 */
int pipe_progress_main(int argc, char **argv);
int pipe_progress_main(int argc, char **argv)
{
	RESERVE_CONFIG_BUFFER(buf, PIPE_PROGRESS_SIZE);
	time_t t = time(NULL);
	size_t len;

	while ((len = fread(buf, 1, PIPE_PROGRESS_SIZE, stdin)) > 0) {
		time_t new_time = time(NULL);
		if (new_time != t) {
			t = new_time;
			fputc('.', stderr);
		}
		fwrite(buf, len, 1, stdout);
	}

	fputc('\n', stderr);

	if (ENABLE_FEATURE_CLEAN_UP)
		RELEASE_CONFIG_BUFFER(buf);

	return 0;
}
