/* vi: set sw=4 ts=4: */
/*
 * Mini update implementation for busybox; much pasted from update-2.11
 *
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (c) 1996, 1997, 1999 Torsten Poulin.
 * Copyright (c) 2000 by Karl M. Hegbloom <karlheg@debian.org>
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

#include "internal.h"
#include <sys/param.h>
#include <sys/syslog.h>


#if defined(__GLIBC__)
#include <sys/kdaemon.h>
#else
static _syscall2(int, bdflush, int, func, int, data);
#endif							/* __GLIBC__ */

static unsigned int sync_duration = 30;
static unsigned int flush_duration = 5;
static int use_sync = 0;

extern int update_main(int argc, char **argv)
{
	int pid;

	argc--;
	argv++;
	while (argc>0 && **argv == '-') {
		while (*++(*argv)) {
			switch (**argv) {
			case 'S':
				use_sync = 1;
				break;
			case 's':
				if (--argc < 1) usage(update_usage);
				sync_duration = atoi(*(++argv));
				break;
			case 'f':
				if (--argc < 1) usage(update_usage);
				flush_duration = atoi(*(++argv));
				break;
			default:
				usage(update_usage);
			}
		}
		argc--;
		argv++;
	}

	pid = fork();
	if (pid < 0)
		exit(FALSE);
	else if (pid == 0) {
		/* Become a proper daemon */
		setsid();
		chdir("/");
		for (pid = 0; pid < OPEN_MAX; pid++) close(pid);

		/*
		 * This is no longer necessary since 1.3.5x, but it will harmlessly
		 * exit if that is the case.
		 */
		argv[0] = "bdflush (update)";
		argv[1] = NULL;
		argv[2] = NULL;
		for (;;) {
			if (use_sync) {
				sleep(sync_duration);
				sync();
			} else {
				sleep(flush_duration);
				if (bdflush(1, 0) < 0) {
					openlog("update", LOG_CONS, LOG_DAEMON);
					syslog(LOG_INFO,
						   "This kernel does not need update(8). Exiting.");
					closelog();
					exit(TRUE);
				}
			}
		}
	}
	return( TRUE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
