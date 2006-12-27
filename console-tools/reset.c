/* vi: set sw=4 ts=4: */
/*
 * Mini reset implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Written by Erik Andersen and Kent Robotti <robotti@metconnect.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* no options, no getopt */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

int reset_main(int argc, char **argv)
{
	if (isatty(1)) {
		/* See 'man 4 console_codes' for details:
		 * "ESC c"			-- Reset
		 * "ESC ( K"		-- Select user mapping
		 * "ESC [ J"		-- Erase display
		 * "ESC [ 0 m"		-- Reset all display attributes
		 * "ESC [ ? 25 h"	-- Make cursor visible.
		 */
		printf("\033c\033(K\033[J\033[0m\033[?25h");
	}
	return EXIT_SUCCESS;
}

