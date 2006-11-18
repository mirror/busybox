/* vi: set sw=4 ts=4: */
/* pidfile.c
 *
 * Functions to assist in the writing and removing of pidfiles.
 *
 * Russ Dill <Russ.Dill@asu.edu> September 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "common.h"


static const char *saved_pidfile;

static void pidfile_delete(void)
{
	if (saved_pidfile) unlink(saved_pidfile);
}


int pidfile_acquire(const char *pidfile)
{
	int pid_fd;
	if (!pidfile) return -1;

	pid_fd = open(pidfile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (pid_fd < 0) {
		bb_perror_msg("cannot open pidfile %s", pidfile);
	} else {
		lockf(pid_fd, F_LOCK, 0);
		if (!saved_pidfile)
			atexit(pidfile_delete);
		saved_pidfile = pidfile;
	}

	return pid_fd;
}


void pidfile_write_release(int pid_fd)
{
	FILE *out;

	if (pid_fd < 0) return;

	out = fdopen(pid_fd, "w");
	if (out) {
		fprintf(out, "%d\n", getpid());
		fclose(out);
	}
	lockf(pid_fd, F_UNLCK, 0);
	close(pid_fd);
}
