/* vi: set sw=4 ts=4: */
/*
 * resize - set terminal width and height.
 *
 * Copyright 2006 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* no options, no getopt */
#include "busybox.h"

int resize_main(int argc, char **argv)
{
	struct termios old, new;
	struct winsize w = {0,0,0,0};
	int ret;

	tcgetattr(STDOUT_FILENO, &old); /* fiddle echo */
	new = old;
	new.c_cflag |= (CLOCAL | CREAD);
	new.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tcsetattr(STDOUT_FILENO, TCSANOW, &new);
	/* save_cursor_pos 7
	 * scroll_whole_screen [r
	 * put_cursor_waaaay_off [$x;$yH
	 * get_cursor_pos [6n
	 * restore_cursor_pos 8
	 */
	printf("\0337\033[r\033[999;999H\033[6n");
	scanf("\033[%hu;%huR", &w.ws_row, &w.ws_col);
	ret = ioctl(STDOUT_FILENO, TIOCSWINSZ, &w);
	printf("\0338");
	tcsetattr(STDOUT_FILENO, TCSANOW, &old);
	if (ENABLE_FEATURE_RESIZE_PRINT)
		printf("COLUMNS=%d;LINES=%d;export COLUMNS LINES;\n",
			w.ws_col, w.ws_row);
	return ret;
}
