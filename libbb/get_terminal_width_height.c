/* vi: set sw=4 ts=4: */
/*
 * Determine the width and height of the terminal.
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "busybox.h"

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  It is also
 * perfectly ok to have CONFIG_FEATURE_AUTOWIDTH disabled, in 
 * which case you will always get 80x24 */
void get_terminal_width_height(int fd, int *width, int *height)
{
	struct winsize win = { 0, 0, 0, 0 };
#ifdef CONFIG_FEATURE_AUTOWIDTH
	if (ioctl(0, TIOCGWINSZ, &win) != 0) {
		win.ws_row = 24;
		win.ws_col = 80;
	}
#endif
	if (win.ws_row <= 1) {
		win.ws_row = 24;
	}
	if (win.ws_col <= 1) {
		win.ws_col = 80;
	}
	if (height) {
		*height = (int) win.ws_row;
	}
	if (width) {
		*width = (int) win.ws_col;
	}
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

