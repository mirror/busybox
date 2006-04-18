/* vi: set sw=4 ts=4: */
/*
 * wfopen_input implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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

/* A number of applets need to open a file for reading, where the filename
 * is a command line arg.  Since often that arg is '-' (meaning stdin),
 * we avoid testing everywhere by consolidating things in this routine.
 *
 * Note: We also consider "" to main stdin (for 'cmp' at least).
 */

#include <stdio.h>
#include <sys/stat.h>
#include <libbb.h>

FILE *bb_wfopen_input(const char *filename)
{
	FILE *fp = stdin;

	if ((filename != bb_msg_standard_input)
		&& filename[0] && ((filename[0] != '-') || filename[1])
	) {
		fp = bb_wfopen(filename, "r");
	}

	return fp;
}
