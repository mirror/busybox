/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  If you wrote this, please
 * acknowledge your work.
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
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "libbb.h"



/* From <linux/kd.h> */
static const int KDGKBTYPE = 0x4B33;  /* get keyboard type */


static int open_a_console(const char *fnam)
{
	int fd;

	/* try read-write */
	fd = open(fnam, O_RDWR);

	/* if failed, try read-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_RDONLY);

	/* if failed, try write-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_WRONLY);

	return fd;
}

/*
 * Get an fd for use with kbd/console ioctls.
 * We try several things because opening /dev/console will fail
 * if someone else used X (which does a chown on /dev/console).
 */

int get_console_fd(void)
{
	int fd;

	static const char * const choise_console_names[] = {
		CONSOLE_DEV, CURRENT_VC, CURRENT_TTY
	};

	for (fd = 2; fd >= 0; fd--) {
		int fd4name;
		int choise_fd;
		char arg;

		fd4name = open_a_console(choise_console_names[fd]);
	chk_std:
		choise_fd = fd4name >= 0 ? fd4name : fd;

		arg = 0;
		if (ioctl(choise_fd, KDGKBTYPE, &arg) == 0)
			return choise_fd;
		if(fd4name >= 0) {
			close(fd4name);
			fd4name = -1;
			goto chk_std;
		}
	}

	bb_error_msg("Couldn't get a file descriptor referring to the console");
	return fd;                      /* total failure */
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
