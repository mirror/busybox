/*
 *  minit version 0.9.1 by Felix von Leitner
 *  ported to busybox by Glenn McGrath <bug1@optushome.com.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#include "busybox.h"
/* purpose: argv[1] is the full path to a PID file,
 *          argv+2 is the daemon to run.
 * the daemon is expected to fork in the background and write its PID in
 * the pid file.
 */

extern int pidfilehack_main(int argc, char **argv)
{
	int count = 0;

	if (argc < 3) {
		bb_show_usage();

	}
	if (unlink(argv[2]) && (errno != ENOENT)) {
		bb_perror_msg("could not remove pid file");
	}
	switch (fork()) {
	case -1:
		bb_perror_msg("could not fork");
		return 2;
	case 0:	/* child */
		execv(argv[3], argv + 3);
		bb_perror_msg("execvp failed");
		return 3;
	}
	do {
		int fd = open(argv[2], O_RDONLY);

		if (fd >= 0) {
			static char buf[100] = "-P";
			int len = read(fd, buf + 2, 100);

			close(fd);
			if (len > 0) {
				char *_argv[] = { "msvc", 0, 0, 0 };
				if (buf[len + 1] == '\n') {
					buf[len + 1] = 0;
				} else {
					buf[len + 2] = 0;
				}
				_argv[1] = buf;
				_argv[2] = argv[1];
				execvp(_argv[0], _argv);
				bb_perror_msg("execvp failed");
				return 0;
			}
		}
		sleep(1);
	} while (++count < 30);

	return(0);
}
