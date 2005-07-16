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
static const int KB_84 = 0x01;
static const int KB_101 = 0x02;    /* this is what we always answer */

int is_a_console(int fd)
{
	char arg;

	arg = 0;
	return (ioctl(fd, KDGKBTYPE, &arg) == 0
			&& ((arg == KB_101) || (arg == KB_84)));
}

static int open_a_console(char *fnam)
{
	int fd;

	/* try read-only */
	fd = open(fnam, O_RDWR);

	/* if failed, try read-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_RDONLY);

	/* if failed, try write-only */
	if (fd < 0 && errno == EACCES)
		fd = open(fnam, O_WRONLY);

	/* if failed, fail */
	if (fd < 0)
		return -1;

	/* if not a console, fail */
	if (!is_a_console(fd)) {
		close(fd);
		return -1;
	}

	/* success */
	return fd;
}

/*
 * Get an fd for use with kbd/console ioctls.
 * We try several things because opening /dev/console will fail
 * if someone else used X (which does a chown on /dev/console).
 *
 * if tty_name is non-NULL, try this one instead.
 */

int get_console_fd(void)
{
	int fd;

	if (-1 == (fd = open_a_console("/dev/console")))
		return -1;
	else
		return fd;

	fd = open_a_console(CURRENT_TTY);
	if (fd >= 0)
		return fd;

	fd = open_a_console(CURRENT_VC);
	if (fd >= 0)
		return fd;

	fd = open_a_console(CONSOLE_DEV);
	if (fd >= 0)
		return fd;

	for (fd = 0; fd < 3; fd++)
		if (is_a_console(fd))
			return fd;

	error_msg("Couldn't get a file descriptor referring to the console");
	return -1;					/* total failure */
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
