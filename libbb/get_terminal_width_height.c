/* vi: set sw=4 ts=4: */
/*
 * Determine the width and height of the terminal.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "libbb.h"

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  */
int get_terminal_width_height(int fd, int *width, int *height)
{
	struct winsize win = { 0, 0, 0, 0 };
	int ret = ioctl(fd, TIOCGWINSZ, &win);
	if (win.ws_row <= 1) win.ws_row = 24;
	if (win.ws_col <= 1) win.ws_col = 80;
	if (height) *height = (int) win.ws_row;
	if (width) *width = (int) win.ws_col;

	return ret;
}
