/* vi: set sw=4 ts=4: */
/*
 * Mini reset implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Written by Erik Andersen and Kent Robotti <robotti@metconnect.com>
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

/* no options, no getopt */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

extern int reset_main(int argc, char **argv)
{
	if (isatty(0) || isatty(0) || isatty(0)) {
		/* See 'man 4 console_codes' for details:
		 * "ESC c"			-- Reset
		 * "ESC ( K"		-- Select user mapping
		 * "ESC [ J"		-- Erase display
		 * "ESC [ 0 m"		-- Reset all Graphics Rendition display attributes
		 * "ESC [ ? 25 h"	-- Make cursor visible.
		 */
		printf("\033c\033(K\033[J\033[0m\033[?25h");
	}
	return EXIT_SUCCESS;
}

