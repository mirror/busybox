/* vi: set sw=4 ts=4: */
/*
 * fflush_stdout_and_exit implementation for busybox
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

/* Attempt to fflush(stdout), and exit with an error code if stdout is
 * in an error state.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libbb.h>

void bb_fflush_stdout_and_exit(int retval)
{
	if (fflush(stdout)) {
		retval = bb_default_error_retval;
	}
	exit(retval);
}
