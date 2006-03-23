/*
 * Monitor a pipe with a simple progress display.
 *
 * Copyright (C) 2003 by Rob Landley <rob@landley.net>, Joey Hess
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "busybox.h"

#define PIPE_PROGRESS_SIZE 4096

/*	Read a block of data from stdin, write it to stdout.
 *  Activity is indicated by a '.' to stderr
 */
extern int pipe_progress_main(int argc, char **argv)
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

#ifdef CONFIG_FEATURE_CLEAN_UP
	RELEASE_CONFIG_BUFFER(buf);
#endif
	return 0;
}
